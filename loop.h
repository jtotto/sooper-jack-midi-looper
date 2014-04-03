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

#ifndef LOOP_H
#define LOOP_H

#include <jack/jack.h>

typedef struct loop_type *Loop;

int loop_init(
    Loop this,
    jack_client_t *jack_client,
    char *name,
    int midi_through,
    int playback_after_recording
);

void loop_free( Loop this );

typedef enum {
    STATE_RECORDING,
    STATE_PLAYBACK,
    STATE_IDLE
} LoopState;

void loop_schedule_state_change(
    Loop this,
    LoopState state,
    jack_nframes_t time
);

void loop_set_midi_through( Loop this, int set );
void loop_set_playback_after_recording( Loop this, int set );

int loop_process_callback( jack_nframes_t nframes );

#endif
