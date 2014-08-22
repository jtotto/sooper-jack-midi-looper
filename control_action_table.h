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

#ifndef CONTROL_ACTION_TABLE_H
#define CONTROL_ACTION_TABLE_H

#include <glib.h>
#include "loop.h"

typedef GHashTable ControlActionTable;

typedef void (*LoopControlFunc)( Loop loop, jack_nframes_t time );
typedef int midi_hash_key_type;

struct ControlAction {
    LoopControlFunc loop_action;
    Loop loop;
};

ControlActionTable *control_action_table_new();

void control_action_table_insert(
    midi_hash_key_type midi,
    struct ControlAction *action // We don't own the actions - they're injected.
);

#endif
