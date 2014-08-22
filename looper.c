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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>

#include <jack/jack.h>
#include <jack/midiport.h>
#include <jack/ringbuffer.h>

#include <gtk/gtk.h>
#include <glib.h>

#include "midi_message.h"
#include "loop.h"
#include "control_action_table.h"

#define NOTE_OFF 0x80
#define NOTE_ON 0x90
#define CONTROL_CHANGE 0xB0

jack_client_t *jack_client;
jack_port_t *control_input;

jack_nframes_t sample_rate;
int rate_flag = 0;

GHashTable *loop_table; // Enforces uniqueness of loop names.
GHashTable *mapping_table;

struct ui_loop_type {
    int row_index;
    Loop loop;
};

struct ui_mapping_type {
    int row_index;
    midi_hash_key_type midi_mapping;
    struct ControlAction action_data;
};

GtkGrid *loop_grid;
gint current_low_row_index = 0;
void ui_remove_loop( GtkWidget *widget, gpointer data );

/* ----------------------------------------------------   
   JACK
   ---------------------------------------------------- */

int sample_rate_change( jack_nframes_t nframes, void *notUsed )
{
    if( !rate_flag ) {
        rate_flag = 1;
        return 0;
    }

    printf( "Sample rate has changed! Exiting...\n" );
    exit( -1 );
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

    int i;
    for ( i = 0; i < events; i++ ) {
        struct MidiMessage rev;

        if( midi_message_from_port_buffer( &rev, control_port_buffer, i ) != 0 ) {
            fprintf( stderr, "TROUBLE\n" );
        }

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
   Loops
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

void destroy_ui_loop( gpointer ui_loop_pointer )
{
    struct ui_loop_type *ui_loop = (struct ui_loop_type *) ui_loop_pointer;
    loop_free( ui_loop->loop );
    free( ui_loop );
}

/* Pulled this logic from a forum post almost verbatim - it strikes
   me as odd that this much plumbing is need for such a simple UI
   interaction. */
char *loop_entry_dialog()
{
    GtkWidget *dialog;
    GtkWidget *entry;
    GtkWidget *content_area;

    dialog = gtk_dialog_new();
    gtk_dialog_add_button( GTK_DIALOG( dialog ), "Create", 0 );

    content_area = gtk_dialog_get_content_area( GTK_DIALOG( dialog ) );
    entry = gtk_entry_new();
    gtk_container_add( GTK_CONTAINER( content_area ), entry );

    gtk_widget_show_all( dialog );
    gint result = gtk_dialog_run( GTK_DIALOG( dialog ) );

    const gchar *entry_line;
    char *input_name = NULL;

    if( result == 0 ) {
        entry_line = gtk_entry_get_text(GTK_ENTRY(entry));

        if( strlen( entry_line ) ) {
            input_name = homebrew_strdup( entry_line );
        }
    }

    gtk_widget_destroy(dialog);

    return input_name;
}

void ui_add_loop( GtkWidget *widget, gpointer data )
{
    char *loop_name = loop_entry_dialog();

    if( !loop_name || g_hash_table_lookup( loop_table, loop_name ) ) {
        free( loop_name );
        return;
    }

    Loop new_loop = NULL; // Is initialized by the init function.

    // TODO: allow user specification of play-through and playback behaviour settings.
    int success = loop_new(
        &new_loop,
        jack_client,
        loop_name,
        1,
        1
    );

    if( success != 0 ) {
        fprintf( stderr, "Couldn't allocate a new loop, bailing on UI steps.\n" );
        free( loop_name );
        return;
    }

    struct ui_loop_type *ui_loop = malloc( sizeof( struct ui_loop_type ) );
    ui_loop->row_index = current_low_row_index;
    ui_loop->loop = new_loop;

    g_hash_table_insert(
        loop_table,
        loop_name,
        ui_loop
    );

    // Update the UI.
    gtk_grid_insert_row( loop_grid, current_low_row_index );

    GtkWidget *label = gtk_label_new( loop_name );
    gtk_grid_attach( loop_grid, label, 0, current_low_row_index, 1, 1 );

    GtkWidget *remove_row_button = gtk_button_new_with_label( "Remove Loop" );
    gtk_grid_attach( loop_grid, remove_row_button, 1, current_low_row_index, 1, 1 );
    g_signal_connect( remove_row_button, "clicked", G_CALLBACK( ui_remove_loop ), loop_name );

    gtk_widget_show( label );
    gtk_widget_show( remove_row_button );

    current_low_row_index++;  // Update for next use.
}

void decrement_loop_indices( gpointer key, gpointer value, gpointer data )
{
    int *removed_index = data;
    struct ui_loop_type *ui_loop = value;

    if( ui_loop->row_index > *removed_index ) {
        ui_loop->row_index--;
    }
}

void ui_remove_loop( GtkWidget *widget, gpointer data )
{
    gchar *loop_name = data;
    struct ui_loop_type *ui_loop = g_hash_table_lookup( loop_table, loop_name );

    int removed_row_index = ui_loop->row_index;
    gtk_grid_remove_row( loop_grid, ui_loop->row_index );
    g_hash_table_remove( loop_table, loop_name );
    g_hash_table_foreach( loop_table, decrement_loop_indices, &removed_row_index ); // This does NOT scale - but shouldn't ever have to.
    current_low_row_index--;
}

/* ----------------------------------------------------   
   MIDI Mappings
   ---------------------------------------------------- */

void destroy_control_mapping( gpointer ui_mapping_pointer )
{
    struct ui_loop_type *ui_loop = (struct ui_mapping_type *) ui_mapping_pointer;
    //TODO: remove the 
}

/* ----------------------------------------------------   
   GUI Top Level
   ---------------------------------------------------- */

gboolean delete_event( GtkWidget *widget, GdkEvent *event, gpointer data )
{
    gtk_main_quit();
    return FALSE;
}

void init_gui()
{
    GtkWidget *window;
    GtkWidget *label;

    window = gtk_window_new( GTK_WINDOW_TOPLEVEL );

    gtk_window_set_title( GTK_WINDOW(window), "MIDI Looper" );

    g_signal_connect( window, "delete-event", G_CALLBACK( delete_event ), NULL );
    gtk_container_set_border_width( GTK_CONTAINER( window ), 10 );

    GtkWidget *notebook = gtk_notebook_new();
    gtk_container_add( GTK_CONTAINER( window ), notebook ); 

    label = gtk_label_new( "Loops" );
    gtk_widget_show( label );

    loop_grid = (GtkGrid *) gtk_grid_new();
    gtk_grid_set_row_spacing( loop_grid, 5 );
    gtk_notebook_append_page( (GtkNotebook *)notebook, (GtkWidget *) loop_grid, label );

    GtkWidget *add_row_button = gtk_button_new_with_label( "Add Loop" );
    gtk_widget_show( add_row_button );
    gtk_grid_attach( loop_grid, add_row_button, 1, 0, 1, 1);
    g_signal_connect( add_row_button, "clicked", G_CALLBACK( ui_add_loop ), NULL);

    gtk_widget_show( (GtkWidget *)loop_grid );

    label = gtk_label_new( "MIDI Mappings" );
    GtkWidget *temp = gtk_label_new( "Temp" );
    gtk_widget_show( label );
    gtk_widget_show( temp );
    gtk_notebook_append_page( (GtkNotebook *)notebook, temp, label );
 
    gtk_widget_show( notebook );
    gtk_widget_show( window );
}

int main(int argc, char *argv[])
{
    // Create the central loop collection.
    loop_table = g_hash_table_new_full(
        g_str_hash,
        g_str_equal,
        str_hash_destroy_key,
        destroy_ui_loop
    );

    mapping_table = g_hash_table_new_full(
        g_direct_hash,
        g_direct_equal,
        NULL,
        destroy_control_mapping
    );

    // Fire up JACK.
    init_jack();

    // GTK next.
    gtk_init(&argc, &argv);
    init_gui();
    gtk_main();

    // At this point, the program has been terminated.
    g_hash_table_destroy( loop_table );
    close_jack();

    return 0;
}
