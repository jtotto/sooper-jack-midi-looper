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

#ifndef CONTROL_ACTION_TABLE_H
#define CONTROL_ACTION_TABLE_H

#include "loop.h"

#define LOOP_CONTROL_FUNC_TOGGLE_PLAYBACK loop_toggle_playback
#define LOOP_CONTROL_FUNC_TOGGLE_RECORDING loop_toggle_recording

typedef struct control_action_table_type *ControlActionTable;
typedef void (*LoopControlFunc)( Loop loop, jack_nframes_t time );

enum MidiControlType {
    TYPE_NOTE_ON = 0x0
    , TYPE_NOTE_OFF = 0X1
    , TYPE_CC_ON = 0x2
    , TYPE_CC_OFF = 0x3
};

enum TableChange {
    ACTION_ADD = 0
    , ACTION_REMOVE
};

typedef void (*ChangeNotificationHandler)(
    enum TableChange,
    unsigned char,
    enum MidiControlType,
    unsigned char,
    Loop,
    LoopControlFunc
);

ControlActionTable control_action_table_new( ChangeNotificationHandler handler );
void control_action_table_free( ControlActionTable this );

void control_action_table_insert(
    ControlActionTable this,
    unsigned char midi_channel,
    enum MidiControlType midi_type,
    unsigned char midi_value,
    Loop loop,
    LoopControlFunc loop_action
);

void control_action_table_remove(
    ControlActionTable this,
    unsigned char midi_channel,
    enum MidiControlType midi_type,
    unsigned char midi_value,
    Loop loop,
    LoopControlFunc loop_action
);

void control_action_table_clear_mappings( ControlActionTable this );

void control_action_table_remove_loop_mappings(
    ControlActionTable this,
    Loop loop
);

void control_action_table_invoke(
    ControlActionTable this,
    unsigned char midi_channel,
    enum MidiControlType midi_type,
    unsigned char midi_value,
    jack_nframes_t time
);

void control_action_table_foreach_mapping(
    ControlActionTable this,
    void (*mapping_func)(
        unsigned char,
        enum MidiControlType,
        unsigned char,
        Loop,
        LoopControlFunc,
        void *
    ),
    void *user_data
);

#endif
