/* JACK MIDI LOOPER
Copyright (C) 2014  Joshua Otto

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA. */

/* Some of the JACK MIDI plumbing here comes from jack-keyboard-2.5
   by Edward Tomasz Napierała, FreeBSD license, jack-keyboard.sourceforge.net */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>

#include <pthread.h>

#include <jack/jack.h>
#include <jack/midiport.h>
#include <jack/ringbuffer.h>

#include <glib.h>

#include <lo/lo.h>

#include "../config.h"

#include "midi_message.h"
#include "loop.h"
#include "control_action_table.h"

#define NOTE_OFF 0x80
#define NOTE_ON 0x90
#define CONTROL_CHANGE 0xB0

#ifdef DEBUGGING_OUTPUT
#define DEBUGGING_MESSAGE( format_data... ) fprintf( stderr, format_data )
#else
#define DEBUGGING_MESSAGE( format_data... ) 
#endif

/* ----------------------------------------------------   
   Plumbing
   ---------------------------------------------------- */

char *homebrew_strdup( const char *in )
{   
    char *duplicate = malloc( ( strlen( in ) + 1 ) * sizeof( char ) );
    if( duplicate ) {   
        strcpy( duplicate, in );
    }
    return duplicate;
}

void str_hash_destroy_key( gpointer str )
{
    free( str );
}

/* ----------------------------------------------------   
   JACK
   ---------------------------------------------------- */

// Data.
jack_client_t *jack_client;
jack_port_t *control_input;

jack_nframes_t sample_rate;
int rate_flag = 0;

pthread_mutex_t loop_table_lock;
pthread_mutex_t action_table_lock;
GHashTable *loop_table;
ControlActionTable action_table;

int sample_rate_change( jack_nframes_t nframes, void *notUsed )
{
    if( !rate_flag ) {
        rate_flag = 1;
        return 0;
    }

    printf( "Sample rate has changed! Exiting...\n" );
    exit( -1 );
}

void process_for_each_loop( gpointer key, gpointer value, gpointer data )
{
    Loop loop = value;
    jack_nframes_t *nframes = data;
    loop_process_callback( loop, *nframes );
}

void process_control_input( jack_nframes_t nframes )
{
    int events;
    void *control_port_buffer;

    control_port_buffer = jack_port_get_buffer( control_input, nframes );
    if ( control_port_buffer == NULL) {
        fprintf( stderr, "Failed to get control input port buffer.\n" );
        return;
    }

    events = jack_midi_get_event_count( control_port_buffer );

    if(
        pthread_mutex_trylock( &loop_table_lock ) == 0
        && pthread_mutex_trylock( &action_table_lock ) == 0
    ) {
        int i;
        for ( i = 0; i < events; i++ ) {
            struct MidiMessage rev;

            if( midi_message_from_port_buffer( &rev, control_port_buffer, i ) != 0 ) {
                fprintf( stderr, "TROUBLE\n" );
            }
            unsigned char midi_channel, midi_value;
            enum MidiControlType midi_type;
            unsigned char message_type = rev.data[0] & 0xf0;
            if(
                message_type == NOTE_ON
                || message_type == NOTE_OFF
                || message_type == CONTROL_CHANGE
            ) {
                midi_value = rev.data[1];
                midi_channel = rev.data[0] & 0xf;

                switch( message_type ) {
                    case NOTE_ON: midi_type = TYPE_NOTE_ON; break;
                    case NOTE_OFF: midi_type = TYPE_NOTE_OFF; break;
                    case CONTROL_CHANGE: midi_type = rev.data[2] > 63 ? TYPE_CC_ON : TYPE_CC_OFF; break;
                    default:
                        // shut gcc up
                        midi_type = TYPE_NOTE_ON;
                        assert( 0 );
                        break;
                }

                control_action_table_invoke(
                    action_table,
                    midi_channel,
                    midi_type,
                    midi_value,
                    rev.time
                );
            }
        }

        g_hash_table_foreach( loop_table, process_for_each_loop, &nframes );
        pthread_mutex_unlock( &loop_table_lock );
        pthread_mutex_unlock( &action_table_lock );
    } else {
        DEBUGGING_MESSAGE( "Tables locked, no output.\n" );
    }

}

int process( jack_nframes_t frames, void *notUsed )
{
    process_control_input( frames );
    return 0;
}

void init_jack( void )
{

    jack_client = jack_client_open ( "midi_looper", JackNullOption, NULL );
    if( jack_client == 0 ) {
        fprintf ( stderr, "jack server not running?\n" );
        exit( -1 );
    }

    jack_set_sample_rate_callback ( jack_client, sample_rate_change, NULL );

    jack_set_process_callback( jack_client, process, NULL );

    sample_rate = jack_get_sample_rate( jack_client );

    control_input = jack_port_register(
        jack_client,
        "control input",
        JACK_DEFAULT_MIDI_TYPE,
        JackPortIsInput,
        0
    );

    if( control_input == NULL ) {
        fprintf( stderr, "Could not register JACK control input port.\n" );
        exit( -1 );
    }

    if( jack_activate( jack_client ) ) {
        fprintf( stderr, "Could not activate JACK.\n" );
        exit( -1 );
    }

}

void close_jack( void ) 
{
    jack_client_close( jack_client );
}

/* ----------------------------------------------------   
   Loops/Mappings
   ---------------------------------------------------- */

void loop_free_wrapper( gpointer data )
{
    Loop loop = data;
    loop_free( loop );
}

void init_loops( void )
{
    if( pthread_mutex_init( &loop_table_lock, NULL ) != 0 )
    {
        fprintf( stderr, "Loop table mutex init failed.\n" );
        return;
    }

    loop_table = g_hash_table_new_full(
        g_str_hash,
        g_str_equal,
        str_hash_destroy_key,
        loop_free_wrapper
    );
}

void close_loops( void )
{
    pthread_mutex_destroy( &loop_table_lock );
    g_hash_table_destroy( loop_table );
}

/* ----------------------------------------------------   
   OSC/Application Logic
   ---------------------------------------------------- */

/* NOTE ABOUT ALL OF THE YUCKY CONST CASTING:
   GLib doesn't use const, as a matter of philosopy.  See mailing list
   discussions for confirmation.  Using it with const-qualified types, as I
   often do here, means that a whole lot of unsavoury looking const casting
   happens.  Feels bad, but there's nothing to be done about it. */

// Data.
GHashTable *update_table; // This table does NOT own its keys.

pthread_mutex_t done_lock;
int done = 0;

lo_server_thread server_thread;
GHashTable *lo_address_table;

// Ported from equivalent logic in sooperlooper.
lo_address find_or_cache_addr( const char *returl )
{
    lo_address addr = g_hash_table_lookup( lo_address_table, returl );
    if( !addr ) {
        addr = lo_address_new_from_url( returl );
        if (lo_address_errno( addr ) < 0 ) {
            fprintf(
                stderr,
                "addr error %d: %s\n",
                lo_address_errno( addr ),
                lo_address_errstr( addr )
            );
        }
        g_hash_table_insert( lo_address_table, homebrew_strdup( returl ), addr );
    }

    return addr;
}

// For the interface methods that specify their loop in the OSC path.
char *extract_loop_name_from_path( char *in )
{
    strtok( in, "/" ); // empty
    strtok( NULL, "/" ); // jml
    return strtok( NULL, "/" ); // should be the name
}

struct where_to {
    lo_address addr;
    const char *retpath;
};

void send_update_data(
        const char *action,
        const char *data,
        struct where_to *where
    ) {
    
    int send = lo_send(
        where->addr,
        where->retpath,
        "ss",
        action,
        data
    );

    if ( send < 0) {
        fprintf(
            stderr,
            "OSC error %d: %s\n",
            lo_address_errno( where->addr ),
            lo_address_errstr( where->addr )
        );
    }
}

gint compare_auto_update( gconstpointer a, gconstpointer b )
{
    const struct where_to *x = a, *y = b;
    return x->addr == y->addr && strcmp( x->retpath, y->retpath );
}

void free_update_item( gpointer data )
{
    struct where_to *update_data = data;
    free( (char * )update_data->retpath );
    free( update_data );
}
void free_update_list( gpointer key, gpointer value, gpointer data )
{
    GList *list = value;
    g_list_free_full( list, free_update_item );
}

void generic_register_auto_update(
        const char *key,
        const char *returl,
        const char *retpath
    ) {

    gpointer value;
    gboolean valid_update
        = g_hash_table_lookup_extended( update_table, key, NULL, &value );

    if( valid_update ) {
        GList *list = value;
        struct where_to *new_subscriber;
        new_subscriber = malloc( sizeof( *new_subscriber ) );
        new_subscriber->addr = find_or_cache_addr( returl );
        new_subscriber->retpath = homebrew_strdup( retpath );
        list = g_list_prepend( list, new_subscriber );
        g_hash_table_insert( update_table, (gpointer)key, list );
    }
}

void generic_unregister_auto_update(
        const char *key,
        const char *returl,
        const char *retpath
    ) {
    
    gpointer value;
    gboolean valid_update
        = g_hash_table_lookup_extended( update_table, key, NULL, &value );

    if( valid_update ) {
        GList *list = value;
        struct where_to ref = {
            .addr = find_or_cache_addr( returl ),
            .retpath = retpath
        };
        GList *subscriber;
        while( ( subscriber = g_list_find_custom( list, &ref, compare_auto_update ) ) ) {
            free_update_item( subscriber->data );
            list = g_list_remove_link( list, subscriber );
        }
        g_hash_table_insert( update_table, (gpointer)key, list );
    }
}

int global_register_auto_update_handler(
        const char *path,
        const char *types,
        lo_arg **argv,
        int argc,
        void *data,
        void *user_data
    ) {

    const char *type = &argv[0]->s, *returl = &argv[1]->s, *retpath = &argv[2]->s;
    DEBUGGING_MESSAGE( "global_register_auto_update_handler %s %s %s\n", type, returl, retpath );
    generic_register_auto_update( type, returl, retpath );
    return 0;
}

int global_unregister_auto_update_handler(
        const char *path,
        const char *types,
        lo_arg **argv,
        int argc,
        void *data,
        void *user_data
    ) {

    const char *type = &argv[0]->s, *returl = &argv[1]->s, *retpath = &argv[2]->s;
    DEBUGGING_MESSAGE( "global_unregister_auto_update_handler %s %s %s\n", type, returl, retpath );
    generic_unregister_auto_update( type, returl, retpath );
    return 0;
}

void auto_update( const char *type, const char *change, const char *data )
{
    DEBUGGING_MESSAGE( "auto_update %s %s %s\n", type, change, data );
    gpointer value;
    gboolean valid_update
        = g_hash_table_lookup_extended( update_table, type, NULL, &value );

    if( valid_update ) {
        GList *subscribers = value;
        while( subscribers != NULL ) {
            struct where_to *update_data = subscribers->data;
            DEBUGGING_MESSAGE( "    %p %s\n", update_data->addr, update_data->retpath );
            send_update_data( change, data, update_data );

            subscribers = subscribers->next;
        }
    } else {
        DEBUGGING_MESSAGE( "INVALID UPDATE %s\n", type );
    }
}

int loop_register_auto_update_handler(
        const char *path,
        const char *types,
        lo_arg **argv,
        int argc,
        void *data,
        void *user_data
    ) {

    const char *returl = &argv[0]->s, *retpath = &argv[1]->s;
    char pathtemp[100];
    strcpy( pathtemp, path );
    char *name = extract_loop_name_from_path( pathtemp );
    DEBUGGING_MESSAGE( "loop_register_auto_update_handler %s %s %s\n", name, returl, retpath );
    generic_register_auto_update( name, returl, retpath );

    return 0;
}

int loop_unregister_auto_update_handler(
        const char *path,
        const char *types,
        lo_arg **argv,
        int argc,
        void *data,
        void *user_data
    ) {

    const char *returl = &argv[0]->s, *retpath = &argv[1]->s;
    char pathtemp[100];
    strcpy( pathtemp, path );
    char *name = extract_loop_name_from_path( pathtemp );
    DEBUGGING_MESSAGE( "loop_unregister_auto_update_handler %s %s %s\n", name, returl, retpath );
    generic_unregister_auto_update( name, returl, retpath );

    return 0;
}

int quit_handler(
        const char *path,
        const char *types,
        lo_arg **argv,
        int argc,
        void *data,
        void *user_data
    ) {

    DEBUGGING_MESSAGE( "quit_handler\n" );
    pthread_mutex_lock( &done_lock );
    done = 1;
    pthread_mutex_unlock( &done_lock );

    return 0;
}

int ping_handler(
        const char *path,
        const char *types,
        lo_arg **argv,
        int argc,
        void *data,
        void *user_data
    ) {

    const char *returl = &argv[0]->s, *retpath = &argv[1]->s;
    DEBUGGING_MESSAGE( "ping_handler %s %s\n", returl, retpath );
    lo_address addr = find_or_cache_addr( returl );
    if( addr ) {
        char *server_url;
        int send = lo_send(
            addr,
            retpath,
            "ssi",
            ( server_url = lo_server_thread_get_url( server_thread ) ),
            PACKAGE_VERSION,
            g_hash_table_size( loop_table )
        );
        free( server_url );

        if ( send < 0) {
            fprintf(
                stderr,
                "OSC error %d: %s\n",
                lo_address_errno( addr ),
                lo_address_errstr( addr )
            );
        }
    }

    return 0;
}

void send_loop( gpointer key, gpointer value, gpointer data )
{
    struct where_to *send_data = data;
    const char *loop_name = key;

    send_update_data( "add", loop_name, send_data );
}

int loop_list_handler(
        const char *path,
        const char *types,
        lo_arg **argv,
        int argc,
        void *data,
        void *user_data
    ) {

    const char *returl = &argv[0]->s, *retpath = &argv[1]->s;
    DEBUGGING_MESSAGE( "loop_list_handler %s %s\n", returl, retpath );
    lo_address addr = find_or_cache_addr( returl );
    if( addr ) {
        struct where_to send_data = {
            .addr = addr,
            .retpath = retpath
        };
        g_hash_table_foreach( loop_table, send_loop, &send_data );
    }

    return 0;
}

void serialize_loop_controls( char *out, Loop loop )
{
    sprintf(
        out,
        "%d %d",
        loop_get_midi_through( loop ),
        loop_get_playback_after_recording( loop )
    );
}

int loop_get_controls_handler(
        const char *path,
        const char *types,
        lo_arg **argv,
        int argc,
        void *data,
        void *user_data
    ) {

    const char *returl = &argv[0]->s, *retpath = &argv[1]->s;

    char pathtemp[100];
    strcpy( pathtemp, path );
    char *name = extract_loop_name_from_path( pathtemp ); 
    DEBUGGING_MESSAGE( "loop_get_controls_handler %s %s %s\n", name, returl, retpath );
    Loop loop = g_hash_table_lookup( loop_table, name );
    if( loop ) {
        char serialization[100];
        serialize_loop_controls( serialization, loop );

        struct where_to return_address = {
            .addr = find_or_cache_addr( returl ),
            .retpath = retpath
        };
        send_update_data( "controls", serialization, &return_address );
    }

    return 0;
}

int loop_set_controls_handler(
        const char *path,
        const char *types,
        lo_arg **argv,
        int argc,
        void *data,
        void *user_data
    ) {

    const char *new_controls = &argv[0]->s;

    char pathtemp[100];
    strcpy( pathtemp, path );
    char *name = extract_loop_name_from_path( pathtemp ); 
    DEBUGGING_MESSAGE( "loop_set_controls_handler %s %s\n", name, new_controls );
    Loop loop = g_hash_table_lookup( loop_table, name );
    if( loop ) {
        char controltemp[100];
        strcpy( controltemp, new_controls );

        // Declare an array of loop control setters.
        void (*loop_set_functions[2])( Loop, int ) = {
            loop_set_midi_through,
            loop_set_playback_after_recording
        };

        char *control = strtok( controltemp, " " );
        for( int i = 0; control != NULL; i++ ) { // The weirdness is deliberate.
            if( strcmp( control, "same" ) ) {
                int new_control_value = atoi( control );
                loop_set_functions[i]( loop, new_control_value );
            }
            control = strtok( NULL, " " );
        }

        // Update subscribers.
        char serialization[100];
        serialize_loop_controls( serialization, loop );
        auto_update( name, "controls", serialization );
    }

    return 0;
}

int loop_add_handler(
        const char *path,
        const char *types,
        lo_arg **argv,
        int argc,
        void *data,
        void *user_data
    ) {

    const char *name = &argv[0]->s;
    DEBUGGING_MESSAGE( "loop_add_handler %s\n", name );

    pthread_mutex_lock( &loop_table_lock );

    // Checks the update table to prohibit the use of special names.
    if(
        strstr( name, "/" ) == NULL
        && strstr( name, " " ) == NULL
        && strlen( name ) < 50
        && !g_hash_table_contains( update_table, name )
    ) {
        Loop new_loop;
        char *dup_name = homebrew_strdup( name );
        loop_new( &new_loop, jack_client, dup_name, 1, 1 ); // Discards status code.
        g_hash_table_insert( loop_table, dup_name, new_loop );
        auto_update( "loops", "add", dup_name );

        char ctrl_get_url[100];
        char ctrl_set_url[100];
        char register_url[100];
        char unregister_url[100];
        sprintf( ctrl_get_url, "/jml/%s/get", name );
        sprintf( ctrl_set_url, "/jml/%s/set", name );
        sprintf( register_url, "/jml/%s/register_auto_update", name );
        sprintf( unregister_url, "/jml/%s/register_auto_update", name );
        lo_server_thread_add_method(
            server_thread,
            ctrl_get_url,
            "ss",
            loop_get_controls_handler,
            NULL
        );
        lo_server_thread_add_method(
            server_thread,
            ctrl_set_url,
            "s",
            loop_set_controls_handler,
            NULL
        );
        lo_server_thread_add_method(
            server_thread,
            register_url,
            "ss",
            loop_register_auto_update_handler,
            NULL
        );
        lo_server_thread_add_method(
            server_thread,
            unregister_url,
            "ss",
            loop_unregister_auto_update_handler,
            NULL
        );
        // Creates an entry in the subscriptions table for the loop.
        g_hash_table_insert( update_table, dup_name, NULL );
    }

    pthread_mutex_unlock( &loop_table_lock );
    return 0;
}

int loop_del_handler(
        const char *path,
        const char *types,
        lo_arg **argv,
        int argc,
        void *data,
        void *user_data
    ) {

    const char *name = &argv[0]->s;
    DEBUGGING_MESSAGE( "loop_del_handler %s\n", name );

    pthread_mutex_lock( &loop_table_lock );

    Loop to_be_removed = g_hash_table_lookup( loop_table, name );
    if( to_be_removed ) {
        g_hash_table_remove( loop_table, name );
        auto_update( "loops", "remove", name );

        char ctrl_get_url[100];
        char ctrl_set_url[100];
        char register_url[100];
        char unregister_url[100];
        sprintf( ctrl_get_url, "/jml/%s/get", name );
        sprintf( ctrl_set_url, "/jml/%s/set", name );
        sprintf( register_url, "/jml/%s/register_auto_update", name );
        sprintf( unregister_url, "/jml/%s/register_auto_update", name );
        lo_server_thread_del_method( server_thread, ctrl_get_url, "ss" );
        lo_server_thread_del_method( server_thread, ctrl_set_url, "ss" );
        lo_server_thread_del_method( server_thread, register_url, "ss" );
        lo_server_thread_del_method( server_thread, unregister_url, "ss" );
        g_hash_table_remove( update_table, name );
    }

    pthread_mutex_unlock( &loop_table_lock );
    return 0;
}

void serialize_mapping( 
        char *out,
        unsigned char midi_channel,
        enum MidiControlType midi_type,
        unsigned char midi_value,
        Loop loop,
        LoopControlFunc control_func
    ) {
    static const char *type_serializations[4] = { "on", "off", "cc_on", "cc_off" };
    sprintf(
        out,
        "%u %s %u %s %s",
        midi_channel,
        type_serializations[midi_type],
        midi_value,
        control_func == LOOP_CONTROL_FUNC_TOGGLE_PLAYBACK
            ? "toggle_playback"
            : "toggle_recording",
        loop_get_name( loop )
    );
}

enum MidiControlType control_type_from_string( const char *in )
{
    if( !strcmp( in, "on" ) ) {
        return TYPE_NOTE_ON;
    } else if( !strcmp( in, "off" ) ) {
        return TYPE_NOTE_OFF;
    } else if( !strcmp( in, "cc_on" ) ) {
        return TYPE_CC_ON;
    } else if( !strcmp( in, "cc_off" ) ) {
        return TYPE_CC_OFF;
    }
    assert( 0 );
}

LoopControlFunc control_func_from_string( const char *in )
{
    if( !strcmp( in, "toggle_playback" ) ) {
        return LOOP_CONTROL_FUNC_TOGGLE_PLAYBACK;
    } else if( !strcmp( in, "toggle_recording" ) ) {
        return LOOP_CONTROL_FUNC_TOGGLE_RECORDING;
    }
    assert( 0 );
}

// GROSS
void deserialize_mapping(
        const char *in,
        unsigned char *midi_channel,
        enum MidiControlType *midi_type,
        unsigned char *midi_value,
        Loop *loop,
        LoopControlFunc *control_func
    ) {

    char mappingtemp[100];
    strcpy( mappingtemp, in );
    char *current = strtok( mappingtemp, " " );
    *midi_channel = (unsigned char)atoi( current );
    current = strtok( NULL, " " );
    *midi_type = control_type_from_string( current );
    current = strtok( NULL, " " );
    *midi_value = (unsigned char)atoi( current );
    current = strtok( NULL, " " );
    *control_func = control_func_from_string( current );
    current = strtok( NULL, " " );
    *loop = g_hash_table_lookup( loop_table, current );

    assert( *midi_channel >= 0 && *midi_channel < 16 );
    assert( *midi_value >= 0 && *midi_value < 128 );
    assert( loop );
}

void mapping_table_change_handler( 
        enum TableChange change,
        unsigned char midi_channel,
        enum MidiControlType midi_type,
        unsigned char midi_value,
        Loop loop,
        LoopControlFunc control_func
    ) {
    char serialization[100];
    serialize_mapping(
        serialization,
        midi_channel,
        midi_type,
        midi_value,
        loop,
        control_func
    );

    char *change_string = change == ACTION_ADD ? "add" : "remove";
    auto_update( "mappings", change_string, serialization );
}

void send_mapping(
        unsigned char midi_channel,
        enum MidiControlType midi_type,
        unsigned char midi_value,
        Loop loop,
        LoopControlFunc control_func,
        void *user_data
    ) {

    struct where_to *return_address = user_data;
    char serialization[100];
    serialize_mapping(
        serialization,
        midi_channel,
        midi_type,
        midi_value,
        loop,
        control_func
    );
    send_update_data( "add", serialization, return_address );
}

int midi_binding_list_handler(
        const char *path,
        const char *types,
        lo_arg **argv,
        int argc,
        void *data,
        void *user_data
    ) {

    const char *returl = &argv[0]->s, *retpath = &argv[1]->s;
    DEBUGGING_MESSAGE( "midi_binding_list_handler %s %s\n", returl, retpath );
    lo_address addr = find_or_cache_addr( returl );
    if( addr ) {
        struct where_to send_data = {
            .addr = addr,
            .retpath = retpath
        };
        control_action_table_foreach_mapping( action_table, send_mapping, &send_data );
    }

    return 0;
}

int clear_midi_bindings_handler(
        const char *path,
        const char *types,
        lo_arg **argv,
        int argc,
        void *data,
        void *user_data
    ) {

    DEBUGGING_MESSAGE( "clear_midi_bindings_handler\n" );
    control_action_table_clear_mappings( action_table );
    return 0;
}

int add_mapping_handler(
        const char *path,
        const char *types,
        lo_arg **argv,
        int argc,
        void *data,
        void *user_data
    ) {

    const char *serialization = &argv[0]->s;
    DEBUGGING_MESSAGE( "add_mapping_handler with %s\n", serialization );
    unsigned char midi_channel, midi_value;
    enum MidiControlType midi_type;
    Loop loop;
    LoopControlFunc control_func;

    deserialize_mapping(
        serialization,
        &midi_channel,
        &midi_type,
        &midi_value,
        &loop,
        &control_func
    );

    pthread_mutex_lock( &loop_table_lock );
    pthread_mutex_lock( &action_table_lock );

    control_action_table_insert(
        action_table,
        midi_channel,
        midi_type,
        midi_value,
        loop,
        control_func
    );

    pthread_mutex_unlock( &action_table_lock );
    pthread_mutex_unlock( &loop_table_lock );

    return 0;
}

int remove_mapping_handler(
        const char *path,
        const char *types,
        lo_arg **argv,
        int argc,
        void *data,
        void *user_data
    ) {

    const char *serialization = &argv[0]->s;
    DEBUGGING_MESSAGE( "remove_mapping_handler with %s\n", serialization );
    unsigned char midi_channel, midi_value;
    enum MidiControlType midi_type;
    Loop loop;
    LoopControlFunc control_func;

    deserialize_mapping(
        serialization,
        &midi_channel,
        &midi_type,
        &midi_value,
        &loop,
        &control_func
    );

    pthread_mutex_lock( &loop_table_lock );
    pthread_mutex_lock( &action_table_lock );

    control_action_table_remove(
        action_table,
        midi_channel,
        midi_type,
        midi_value,
        loop,
        control_func
    );

    auto_update( "mappings", "remove", serialization );

    pthread_mutex_unlock( &action_table_lock );
    pthread_mutex_unlock( &loop_table_lock );

    return 0;
}

void osc_error( int num, const char *msg, const char *path )
{
    fprintf( stderr, "liblo server error %d in path %s: %s\n", num, path, msg );
    fflush( stderr );
}

void init_liblo( const char *osc_port )
{
    if( pthread_mutex_init( &done_lock, NULL ) != 0 )
    {
        fprintf( stderr, "Done flag mutex init failed.\n" );
        exit( -1 );
    }

    lo_address_table = g_hash_table_new_full(
        g_str_hash,
        g_str_equal,
        str_hash_destroy_key,
        lo_address_free
    );

    server_thread = lo_server_thread_new( osc_port, osc_error );
    fprintf(
        stderr,
        "Looper engine serving via OSC on port %d\n",
        lo_server_thread_get_port( server_thread )
    );

    lo_server_thread_add_method( server_thread, "/quit", "", quit_handler, NULL );
    lo_server_thread_add_method( server_thread, "/ping", "ss", ping_handler, NULL );
    lo_server_thread_add_method( server_thread, "/loop_list", "ss", loop_list_handler, NULL );
    lo_server_thread_add_method( server_thread, "/loop_add", "s", loop_add_handler, NULL );
    lo_server_thread_add_method( server_thread, "/loop_del", "s", loop_del_handler, NULL );

    update_table = g_hash_table_new( g_str_hash, g_str_equal );
    g_hash_table_insert( update_table, "loops", NULL /* the empty GList */ );
    g_hash_table_insert( update_table, "mappings", NULL /* the empty GList */ );
    g_hash_table_insert( update_table, "errors", NULL /* the empty GList */ );

    lo_server_thread_add_method( server_thread, "/register_auto_update", "sss", global_register_auto_update_handler, NULL );
    lo_server_thread_add_method( server_thread, "/unregister_auto_update", "sss", global_unregister_auto_update_handler, NULL );
    lo_server_thread_add_method( server_thread, "/midi_binding_list",  "ss", midi_binding_list_handler, NULL );
    lo_server_thread_add_method( server_thread, "/clear_midi_bindings", "", clear_midi_bindings_handler, NULL );
    lo_server_thread_add_method( server_thread, "/add_midi_binding", "s", add_mapping_handler, NULL );
    lo_server_thread_add_method( server_thread, "/remove_midi_binding", "s", remove_mapping_handler, NULL );
    
    lo_server_thread_start( server_thread );
}

void close_liblo( void )
{
    pthread_mutex_destroy( &done_lock );    
    g_hash_table_destroy( lo_address_table );
    g_hash_table_foreach( update_table, free_update_list, NULL );
    g_hash_table_destroy( update_table );
    lo_server_thread_free( server_thread );
}

/* ----------------------------------------------------   
   Main
   ---------------------------------------------------- */

int main( int argc, char *argv[] )
{
    fprintf( stdout, "%s\n", PACKAGE_STRING );
    DEBUGGING_MESSAGE( "Compiled with debugging messages.\n" );

    int opt;
    const char *osc_port = NULL;

    while( ( opt = getopt( argc, argv, "p:" ) ) != -1 ) {
        switch( opt ) {
            case 'p': osc_port = optarg; break;
        }
    }

    init_loops();

    // Fire up JACK.
    init_jack();

    // OSC next.
    init_liblo( osc_port );

    action_table = control_action_table_new( mapping_table_change_handler );
    if( pthread_mutex_init( &action_table_lock, NULL ) != 0 )
    {
        fprintf( stderr, "Action table mutex init failed.\n" );
        return 1;
    }

    int quit = 0;
    while( !quit ) {
        sleep( 1 );
        pthread_mutex_lock( &done_lock );
        quit = done;
        pthread_mutex_unlock( &done_lock );
    }

    // At this point, the engine has been terminated.
    close_jack();
    close_liblo();
    close_loops();
    control_action_table_free( action_table );
    pthread_mutex_destroy( &action_table_lock );

    return 0;
}
