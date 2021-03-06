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

#include "loop.h"

#include <stdio.h>
#include <string.h>

#include <jack/jack.h>
#include <jack/ringbuffer.h>

#include "debug.h"
#include "loop_buffer.h"
#include "midi_message.h"

// Are these reasonable?
#define MIDI_IO_BUFFER_SIZE     (1024*sizeof( struct MidiMessage ))
#define MIDI_LOOP_BUFFER_SIZE    2048

typedef enum {
    STATE_RECORDING = 0,
    STATE_PLAYBACK = 1,
    STATE_IDLE = 2
} LoopState;

#ifdef DEBUGGING_OUTPUT
static const char *STATE_STRINGS[3] = {
    "STATE_RECORDING",
    "STATE_PLAYBACK",
    "STATE_IDLE"
};
#endif

struct StateSchedule {
    LoopState state;
    jack_nframes_t time;
};

// I'm really not anticipating more than one or two state changes per process cycle.
#define STATE_BUFFER_SIZE     32*sizeof( struct StateSchedule )

struct loop_type {

    // Injected.
    const char *name;
    int midi_through;
    int playback_after_recording;

    // Internal.
    jack_client_t *jack_client;

    jack_port_t *loop_input;
    char *loop_input_name;

    jack_port_t *loop_output;
    char *loop_output_name;

    jack_ringbuffer_t *midi_io_buffer;
    jack_ringbuffer_t *state_buffer;

    struct StateSchedule current_state;
    jack_nframes_t last_playback_start; // Will be absolute.

    jack_nframes_t recording_start;
    jack_nframes_t recording_end;
    jack_nframes_t recording_length; // Saves recomputing it once per callback invocation.
    LoopBuffer midi_loop_buffer;
};

static void schedule_state_change(
    Loop this,
    LoopState state,
    jack_nframes_t time
);

static int process_state_midi_input(
    Loop this,
    void *input_port_buffer,
    int events,
    int *event_index,
    jack_nframes_t end_of_state,
    jack_nframes_t last_frame_time
);

static int process_state_midi_playback(
    Loop this,
    jack_nframes_t end_of_state,
    jack_nframes_t last_frame_time
);

static int process_midi_output( Loop this, jack_nframes_t nframes, jack_nframes_t last_frame_time );

#undef BAIL
#define BAIL( message, code ) \
    fprintf( stderr, message, name );\
    loop_free( this );\
    *loop_pointer = NULL;\
    return code;

int loop_new(
        struct loop_type **loop_pointer, // The client owns this pointer - would simply return this, but wanted to return integer status.
        jack_client_t *jack_client,
        const char *name,
        int midi_through,
        int playback_after_recording
    ) {

    *loop_pointer = malloc( sizeof( struct loop_type ) );
    struct loop_type *this = *loop_pointer;
    // Null-initialize all dynamically allocated members.
    this->loop_input = NULL;
    this->loop_output = NULL;
    this->loop_input_name = NULL;
    this->loop_output_name = NULL;
    this->midi_io_buffer = NULL;
    this->state_buffer = NULL;

    // Main loop buffer.
    this->midi_loop_buffer = loop_buffer_init( MIDI_LOOP_BUFFER_SIZE ); 
    if( !loop_buffer_is_valid( this->midi_loop_buffer ) ) {
        BAIL( "Cannot create loop buffer for %s.\n", -1 );
    }

    // I/O ringbuffer.
    this->midi_io_buffer = jack_ringbuffer_create( MIDI_IO_BUFFER_SIZE );

    if ( this->midi_io_buffer == NULL) {
        BAIL( "Cannot create JACK MIDI ringbuffer for %s.\n", -10 );
    }

    jack_ringbuffer_mlock( this->midi_io_buffer ); // Prevent paging of the I/O buffer.

    // State change ringbuffer.
    this->state_buffer = jack_ringbuffer_create( STATE_BUFFER_SIZE );

    if( this->state_buffer == NULL ) {
        BAIL( "Cannot create state change ringbuffer for %s.\n", -11 );
    }

    jack_ringbuffer_mlock( this->state_buffer );

    // Loop ports.
    size_t length_of_name = strlen( name );

    this->loop_output_name = malloc( length_of_name + 13 );
    if( sprintf( this->loop_output_name, "loop_%s_output", name ) < 0 ) {
        BAIL( "Error building output port name string for %s.\n", -20 );
    }

    this->loop_output = jack_port_register(
        jack_client,
        this->loop_output_name,
        JACK_DEFAULT_MIDI_TYPE,
        JackPortIsOutput,
        0
    );

    if( this->loop_output == NULL ) {
        BAIL( "Could not register JACK output port for %s.\n", -30 );
    }

    this->loop_input_name = malloc( length_of_name + 12 );
    if( sprintf( this->loop_input_name, "loop_%s_input", name ) < 0 ) {
        BAIL( "Error building input port name string for %s.\n", -40 );
    }

    this->loop_input = jack_port_register(
        jack_client,
        this->loop_input_name,
        JACK_DEFAULT_MIDI_TYPE,
        JackPortIsInput,
        0
    );

    if( this->loop_input == NULL ) {
        BAIL( "Could not register JACK loop input port for %s.\n.", -50 );
    }

    // Set remaining members.
    this->name = name;
    this->midi_through = midi_through;
    this->playback_after_recording = playback_after_recording;

    this->jack_client = jack_client;

    this->current_state.time = jack_last_frame_time( jack_client );
    this->current_state.state = STATE_IDLE;

    return 0;
}

void loop_free( struct loop_type *this )
{
    if( this ) {
        // JACK ringbuffers.
        if( this->midi_io_buffer ) {
            jack_ringbuffer_free( this->midi_io_buffer );
        }

        if( this->state_buffer ) {
            jack_ringbuffer_free( this->state_buffer );
        }

        // Ports and their names.
        if( this->loop_output_name ) {
            free( this->loop_output_name );
        }

        if( this->loop_output ) {
            jack_port_unregister( this->jack_client, this->loop_output );
        }

        if( this->loop_input_name ) {
            free( this->loop_input_name );
        }

        if( this->loop_input ) {
            jack_port_unregister( this->jack_client, this->loop_input );
        }

        // Our custom loop buffer.
        loop_buffer_free( this->midi_loop_buffer );

        // Ourself.
        free( this );
    }
}

const char *loop_get_name( Loop this )
{
    return this->name;
}

void loop_set_midi_through( Loop this, int set )
{
    this->midi_through = set;
}

int loop_get_midi_through( Loop this )
{
    return this->midi_through;
}

void loop_set_playback_after_recording( Loop this, int set )
{
    this->playback_after_recording = set;
}

int loop_get_playback_after_recording( Loop this )
{
    return this->playback_after_recording;
}

void loop_toggle_playback( Loop this, jack_nframes_t time )
{
    DEBUGGING_MESSAGE( "loop_toggle_playback %s", loop_get_name( this ) );
    if( this->current_state.state == STATE_PLAYBACK ) {
        schedule_state_change( this, STATE_IDLE, time );
    } else {
        schedule_state_change( this, STATE_PLAYBACK, time );
    }
}

void loop_toggle_recording( Loop this, jack_nframes_t time )
{
    DEBUGGING_MESSAGE( "loop_toggle_recording %s", loop_get_name( this ) );
    if( this->current_state.state == STATE_RECORDING ) {
        schedule_state_change( this, STATE_PLAYBACK, time );
    } else {
        schedule_state_change( this, STATE_RECORDING, time );
    }
}

// May be invoked from the process callback (ie. it must be RT)
static void schedule_state_change(
        Loop this,
        LoopState state,
        jack_nframes_t time
    ) {

    struct StateSchedule change = {
        .state = state,
        .time = time
    };

    if( jack_ringbuffer_write_space( this->state_buffer ) < sizeof( change ) ) {
        fprintf( stderr, "Not enough space in the %s state buffer, CHANGE LOST.\n", this->name );
        return;
    }

    size_t written = jack_ringbuffer_write( this->state_buffer, (char *) &change, sizeof( change ) );

    if( written != sizeof( change ) ) {
        fprintf( stderr, "jack_ringubffer_write failed for the %s state buffer, CHANGE LOST.\n", this->name );
    }
}

// Also in the process callback => also RT
int loop_process_callback( Loop this, jack_nframes_t nframes )
{
    struct StateSchedule previous_state = this->current_state;

    void *input_port_buffer = jack_port_get_buffer( this->loop_input, nframes );
    if ( input_port_buffer == NULL ) {
        fprintf( stderr, "jack_port_get_buffer failed for %s, cannot receive anything.\n", this->name );
        return -10;
    }

    jack_nframes_t last_frame_time = jack_last_frame_time( this->jack_client );

    int event_index = 0;
    int events = jack_midi_get_event_count( input_port_buffer );

    int read_next_state;
    do {
        //DEBUGGING_MESSAGE( "%s: state %s\n", loop_get_name( this ), STATE_STRINGS[this->current_state.state] );
        struct StateSchedule next;
        read_next_state = jack_ringbuffer_read( this->state_buffer, (char *) &next, sizeof( next ) );
        if( read_next_state == 0 ) {
            next.time = nframes;
            next.state = this->current_state.state;
        } else if( read_next_state != sizeof( next ) ) {
            fprintf( stderr, "invalid state buffer read in loop %s, can't continue processing\n", this->name );
            return -20;
        }

        int input_result = process_state_midi_input(
            this,
            input_port_buffer,
            events,
            &event_index,
            next.time,
            last_frame_time
        );

        if( input_result != 0 ) {
            return -30;
        }

        switch( this->current_state.state ) {
            case STATE_PLAYBACK:
                {
                    if( previous_state.state != STATE_PLAYBACK ) {
                        this->last_playback_start = this->current_state.time + last_frame_time;
                    }

                    int playback_result = process_state_midi_playback( this, next.time, last_frame_time );
                    if( playback_result != 0 ) {
                        return -40;
                    }
                }
                break;

            case STATE_RECORDING:
                {
                    if( previous_state.state != STATE_RECORDING ) {
                        this->recording_start = this->current_state.time + last_frame_time;
                        loop_buffer_reset_write( this->midi_loop_buffer );
                    }
                }
                break;

            case STATE_IDLE:
                // No action.
                break;
        }

        // Transition states.
        if( next.state == STATE_PLAYBACK && this->current_state.state != STATE_PLAYBACK ) {
            loop_buffer_reset_read( this->midi_loop_buffer );
        }
        
        if( this->current_state.state == STATE_RECORDING && next.state != STATE_RECORDING ) {
            this->recording_end = next.time + last_frame_time;
            this->recording_length = this->recording_end - this->recording_start;
            DEBUGGING_MESSAGE( "end recording end start %d %d\n",
            this->recording_end, this->recording_start );
        }

        previous_state = this->current_state;
        this->current_state = next;

    } while( read_next_state );

    int output_result = process_midi_output( this, nframes, last_frame_time );
    if( output_result != 0 ) {
        return -50;
    }

    return 0;
}

/* Called once per STATE - figures out what to do with the MIDI input received
   received while the loop is in the given state. */
static int process_state_midi_input(
        Loop this,
        void *input_port_buffer,
        int events,
        int *event_index,
        jack_nframes_t end_of_state,
        jack_nframes_t last_frame_time
    ) {

    for( ; *event_index < events; ( *event_index )++ ) {
        struct MidiMessage input_message;
        int read_message_result = midi_message_from_port_buffer( &input_message, input_port_buffer, *event_index );

        if( read_message_result != 0 ) {
            continue; // Error was already reported at this point.
        }

        if( input_message.time >= end_of_state ) {
            break;
        }

        if( this->midi_through ) {
            // DEBUGGING_MESSAGE( "queuing midi through\n" );
            int queue_result = queue_midi_message( this->midi_io_buffer, &input_message );

            if( queue_result != 0 ) {
                return -10;
            }
        }

        if( this->current_state.state == STATE_RECORDING ) {
            input_message.time = ( last_frame_time + input_message.time ) - this->recording_start;
            int pushed = loop_buffer_push( this->midi_loop_buffer, &input_message );

            if( pushed < 0 ) {
                fprintf( stderr, "loop buffer full in loop %s, can't continue processing\n", this->name );
                return -20;
            }
        }
    }

    return 0;
}

// Called once per PLAYBACK STATE - determines which recorded events are up for playback.
static int process_state_midi_playback(
        Loop this,
        jack_nframes_t end_of_state,
        jack_nframes_t last_frame_time
    ) {

    int wrapped;
    struct MidiMessage *recorded;

    /* Only returns NULL if the loop is invalid to begin with.
     * Peek is constant time, so the readability gain seems worth it. */
    while( ( recorded = loop_buffer_peek( this->midi_loop_buffer ) ) ) {

        jack_nframes_t playback_time = recorded->time + this->last_playback_start;

        /* DEBUGGING_MESSAGE(
            "recorded playback %d %d %d %d %d\n",
            recorded->time,
            this->last_playback_start,
            playback_time,
            end_of_state,
            last_frame_time
        ); */

        if( playback_time < end_of_state + last_frame_time ) {
            struct MidiMessage adjusted = *recorded;
            adjusted.time = playback_time - last_frame_time;

            queue_midi_message( this->midi_io_buffer, &adjusted );
            wrapped = loop_buffer_read_advance( this->midi_loop_buffer );
            if( wrapped ) {
                this->last_playback_start += this->recording_length;
            }
        } else {
            break; // The loop will almost certainly exit this way.
        }
    }

    return 0;
}

// Called once per PROCESS CYCLE - gives all of the MIDI output for this cycle to JACK.
static int process_midi_output( Loop this, jack_nframes_t nframes, jack_nframes_t last_frame_time )
{
	int read;
	unsigned char *buffer;
	void *port_buffer;
	struct MidiMessage ev;

	port_buffer = jack_port_get_buffer( this->loop_output, nframes );
	if( port_buffer == NULL ) {
		fprintf( stderr, "Couldn't get output buffer for loop %s\n", this->name );
		return -10;
	}

	jack_midi_clear_buffer( port_buffer );

	while( jack_ringbuffer_read_space( this->midi_io_buffer ) ) {

		read = jack_ringbuffer_peek( this->midi_io_buffer, (char *)&ev, sizeof( ev ) );

		if( read != sizeof( ev ) ) {
			fprintf( stderr, "Short read from the %s output ringbuffer, possible note loss.\n", this->name );
			jack_ringbuffer_read_advance( this->midi_io_buffer, read );
			continue;
		}

		/* If the event time is too much into the future, we'll need
		   to send it later. */
		if( ev.time >= (int)nframes )
			break;

		/* If the event time is < 0, we missed a cycle because of xrun. */
		if( ev.time < 0 )
			ev.time = 0;

		jack_ringbuffer_read_advance( this->midi_io_buffer, sizeof( ev ) );

		buffer = jack_midi_event_reserve( port_buffer, ev.time, ev.len );

		if( buffer == NULL ) {
			fprintf( stderr, "Couldn't reserve space on the output buffer for %s\n", this->name );
			return -20;
		}

		memcpy( buffer, ev.data, ev.len );
	}

    return 0;
}
