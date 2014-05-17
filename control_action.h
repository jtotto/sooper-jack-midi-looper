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

#ifndef CONTROL_ACTION_H
#define CONTROL_ACTION_H

#include "loop.h"

struct ControlAction {
    void (*loop_action)( Loop loop, jack_nframes_t time );
    Loop loop;
};

typedef struct ControlActionListNode *ControlActionList;

void control_action_list_init( ControlActionList *list );

// Dynamically allocates memory - NOT safe for use in the process callback.
void control_action_list_push(
    ControlActionList *list,
    void (*loop_action)( Loop loop, jack_nframes_t time ),
    Loop loop
);

void control_action_list_trigger_all(
    ControlActionList list,
    jack_nframes_t time
);

#endif
