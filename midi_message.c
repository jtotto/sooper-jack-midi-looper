/* JACK MIDI LOOPER
  2 Copyright (C) 2014  Joshua Otto
  3 
  4 This program is free software; you can redistribute it and/or
  5 modify it under the terms of the GNU General Public License
  6 as published by the Free Software Foundation; either version 2
  7 of the License, or any later version.
  8 
  9 This program is distributed in the hope that it will be useful,
 10 but WITHOUT ANY WARRANTY; without even the implied warranty of
 11 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 12 GNU General Public License for more details.
 13 
 14 You should have received a copy of the GNU General Public License
 15 along with this program; if not, write to the Free Software
 16 Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA. */

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
    if( read ) {
        fprintf( stderr, "Control JACK MIDI event get failed.\n" );
        return -10;
    }

    if( event.size > 3 ) {
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

    if( jack_ringbuffer_write_space( ringbuffer ) < sizeof( *ev ) ) {
        fprintf( stderr, "Not enough space to queue midi message, NOTE LOST\n" );
        return -10;
    }

    written = jack_ringbuffer_write( ringbuffer, (char *)ev, sizeof( *ev ) );

    if( written != sizeof( *ev ) ) {
        fprintf( stderr, "jack_ringbuffer_write failed, NOTE LOST.\n" );
        return -20;
    }

    return 0;
}

