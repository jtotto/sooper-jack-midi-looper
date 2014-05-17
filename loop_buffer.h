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

#ifndef LOOP_BUFFER_H
#define LOOP_BUFFER_H

#include "midi_message.h"

typedef struct loop_buffer_type *LoopBuffer;

LoopBuffer loop_buffer_init( size_t capacity );

// Hides the implementation of LoopBuffer as a struct pointer.
int loop_buffer_is_valid( LoopBuffer buffer );

void loop_buffer_reset_read( LoopBuffer buffer );
void loop_buffer_reset_write( LoopBuffer buffer );
void loop_buffer_free( LoopBuffer buffer );

int loop_buffer_push( LoopBuffer buffer, struct MidiMessage *message );
struct MidiMessage *loop_buffer_peek( LoopBuffer buffer, int *wrapped );
void loop_buffer_read_advance( LoopBuffer buffer );

#endif
