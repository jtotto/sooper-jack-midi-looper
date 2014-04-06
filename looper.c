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

// Some of the JACK MIDI plumbing here comes from jack-keyboard-2.5
// by Edward Tomasz Napierała, FreeBSD license, jack-keyboard.sourceforge.net

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <jack/jack.h>
#include <jack/midiport.h>
#include <jack/ringbuffer.h>

#include "midi_message.h"

jack_client_t *jack_client;
jack_port_t *control_input;

jack_nframes_t sampleRate;
int rateFlag = 0;

int sample_rate_change( jack_nframes_t nframes, void *notUsed )
{
    if( !rateFlag )
    {
        rateFlag = 1;
        return 0;
    }
    printf( "Sample rate has changed! Exiting...\n" );
    exit( -1 );
}

void process_midi_input( jack_nframes_t nframes )
{
    int events;
    void *control_port_buffer;

    control_port_buffer = jack_port_get_buffer( control_input, nframes );
    if ( control_port_buffer == NULL)
    {
        fprintf( stderr, "Failed to get control input port buffer.\n" );
        return;
    }

    events = jack_midi_get_event_count( control_port_buffer );

    int i;
    for ( i = 0; i < events; i++ )
    {
        struct MidiMessage rev;

        if( midi_message_from_port_buffer( &rev, control_port_buffer, i ) != 0 )
        {
            fprintf( stderr, "TROUBLE\n" );
        }

        printf( "%d - %d\n", nframes, rev.time );
        jack_nframes_t last_frame_time = jack_last_frame_time( jack_client );
        printf( "LAST %d\n", last_frame_time );

    }

}

int process( jack_nframes_t frames, void *notUsed )
{
    process_midi_input( frames );

    return 0;
}

void init_jack( void ) {

    jack_client = jack_client_open ( "midi_looper", JackNullOption, NULL );
    if( jack_client == 0 )
    {
        fprintf ( stderr, "jack server not running?\n" );
        exit( -1 );
    }

    jack_set_sample_rate_callback ( jack_client, sample_rate_change, NULL );

    jack_set_process_callback( jack_client, process, NULL );

    sampleRate = jack_get_sample_rate( jack_client );

    control_input = jack_port_register(
        jack_client,
        "control input",
        JACK_DEFAULT_MIDI_TYPE,
        JackPortIsInput,
        0
    );

    if( control_input == NULL )
    {
        fprintf( stderr, "Could not register JACK control input port.\n" );
        exit( -1 );
    }

    if( jack_activate( jack_client ) )
    {
        fprintf( stderr, "Could not activate JACK.\n" );
        exit( -1 );
    }

}
int main(int argc, char *argv[])
{
    // Command line options.

    // Fire up JACK.
    init_jack();

    // Sleep in the main thread (all the interesting stuff is happening in the RT JACK thread).
    sleep( -1 );

    return 0;
}
