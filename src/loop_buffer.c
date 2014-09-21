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

#include "loop_buffer.h"

#include "midi_message.h"

#include <stdlib.h>

struct loop_buffer_type {
    struct MidiMessage *buffer;
    struct MidiMessage *buffer_end;
    struct MidiMessage *read_pointer;
    struct MidiMessage *write_pointer;
};

struct loop_buffer_type *loop_buffer_init( size_t capacity ) 
{
    struct loop_buffer_type *buffer_struct = malloc(
        sizeof( struct loop_buffer_type )
    );

    if( buffer_struct == NULL ) {
        return NULL;
    }

    buffer_struct->buffer = malloc(
        sizeof( struct MidiMessage ) * capacity
    );

    if( buffer_struct->buffer == NULL ) {
        free( buffer_struct );
        return NULL;
    }

    loop_buffer_reset_write( buffer_struct );
    buffer_struct->buffer_end = buffer_struct->buffer + capacity;

    return buffer_struct;
}

int loop_buffer_is_valid( struct loop_buffer_type *loop_buffer ) 
{

    if( loop_buffer == NULL || loop_buffer->buffer == NULL ) {
        return 0;
    }

    return 1;
}

void loop_buffer_reset_read( struct loop_buffer_type *loop_buffer )
{
    loop_buffer->read_pointer = loop_buffer->buffer;
}

void loop_buffer_reset_write( struct loop_buffer_type *loop_buffer )
{
    loop_buffer->read_pointer = NULL;
    loop_buffer->write_pointer = loop_buffer->buffer;
}

void loop_buffer_free( struct loop_buffer_type *loop_buffer )
{
    if( loop_buffer ) {
        free( loop_buffer->buffer );
        free( loop_buffer );
    }
}

int loop_buffer_push( struct loop_buffer_type *loop_buffer, struct MidiMessage *message )
{

    if( loop_buffer->write_pointer == loop_buffer->buffer_end ) {
        return -10;
    }

    if( loop_buffer->write_pointer == loop_buffer->buffer ) {
        loop_buffer->read_pointer = loop_buffer->buffer;
    }

    *( loop_buffer->write_pointer ) = *message;
    loop_buffer->write_pointer++;

    return 0;
}

struct MidiMessage *loop_buffer_peek( struct loop_buffer_type *loop_buffer )
{
    return loop_buffer->read_pointer;
}

int loop_buffer_read_advance( struct loop_buffer_type *loop_buffer )
{
    if( loop_buffer->read_pointer != NULL ) {
        loop_buffer->read_pointer++;
        if( loop_buffer->read_pointer < loop_buffer->write_pointer ) {
            return 0;
        } else {
            loop_buffer->read_pointer = loop_buffer->buffer;
        }
    }
    
    return 1;
}

