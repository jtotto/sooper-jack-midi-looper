#include "midi_message.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

#include <jack/midiport.h>
#include <jack/ringbuffer.h>

static void midi_message_from_midi_event( struct MidiMessage *message, jack_midi_event_t event );

int midi_message_from_port_buffer(
        struct MidiMessage *message,
        void *port_buffer,
        int event_index
    ) {

    jack_midi_event_t event;

    int read = jack_midi_event_get( &event, port_buffer, event_index );
    if( read )
    {
        fprintf( stderr, "Control JACK MIDI event get failed.\n" );
        return -10;
    }

    if( event.size > 3 )
    {
        fprintf( stderr, "Ignoring MIDI message longer than three bytes, probably a SysEx.\n" );
        return 10; // This is probably not an error from the caller's perspective.
    }

    midi_message_from_midi_event( message, event );

    return 0;
}

static void midi_message_from_midi_event( struct MidiMessage *message, jack_midi_event_t event )
{
    assert( event.size >= 1 && event.size <= 3 );

    message->len = event.size;
    message->time = event.time;

    memcpy( message->data, event.buffer, message->len );
}

int queue_midi_message( jack_ringbuffer_t *ringbuffer, struct MidiMessage *ev )
{
    int written;

    if( jack_ringbuffer_write_space( ringbuffer ) < sizeof( *ev ) )
    {
        fprintf( stderr, "Not enough space to queue midi message, NOTE LOST\n" );
        return -10;
    }

    written = jack_ringbuffer_write( ringbuffer, (char *)ev, sizeof( *ev ) );

    if( written != sizeof( *ev ) )
    {
        fprintf( stderr, "jack_ringbuffer_write failed, NOTE LOST.\n" );
        return -20;
    }

    return 0;
}

