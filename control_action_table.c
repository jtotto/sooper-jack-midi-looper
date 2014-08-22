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

#include <stdlib.h>
#include "control_action_table.h"
#include "loop.h"

ControlActionTable *control_action_table_new()
{
    return g_hash_table_new( g_direct_hash, g_direct_equal );
}

void control_action_table_insert(
        ControlActionTable *this,
        midi_hash_key_type midi,
        Loop loop,
        LoopControlFunc control_func
    ) {
    
    struct ControlActionListNode *new_action = malloc( sizeof( *new_action ) );
    new_action->action_data.loop_action = control_func;
    new_action->action_data.loop = loop;
    
    struct ControlActionListNode *action_list = g_hash_table_lookup( this, GINT_TO_POINTER( midi ) );
    if( action_list ) {
        new_action->next = action_list;
    }
    g_hash_table_insert( this, GINT_TO_POINTER( midi ), new_action );
}

void control_action_table_remove_mapping

struct ControlActionListNode {
    struct ControlAction *action_data;
    struct ControlActionListNode *next;
};

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


void control_action_list_init( struct ControlActionListNode **list )
{
    *list = NULL;
}

void control_action_list_trigger_all(
        struct ControlActionListNode *list,
        jack_nframes_t time
    ) {

    while( list != NULL ) {
        list->action.loop_action(
            list->action.loop,
            time
        );

        list = list->next;
    }
}

