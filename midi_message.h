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

#ifndef MIDI_MESSAGE_H
#define MIDI_MESSAGE_H

#include <jack/midiport.h>
#include <jack/ringbuffer.h>

// The only difference between this struct and jack_midi_event_t is that
// the raw data storage is actually contained within the struct
// (as opposed to just a buffer pointer)
struct MidiMessage {
    jack_nframes_t time;
    int len;                     /* Length of MIDI message, in bytes. */
    unsigned char data[3];
};

int midi_message_from_port_buffer(
    struct MidiMessage *message,
    void *port_buffer,
    int event_index
);

int queue_midi_message(
    jack_ringbuffer_t *ringbuffer,
    struct MidiMessage *ev
);

#endif
