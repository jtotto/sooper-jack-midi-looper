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
#include "control_action.h"

struct ControlActionListNode {
    struct ControlAction action;
    struct ControlActionListNode *next;
};

void control_action_list_init( struct ControlActionListNode **list )
{
    *list = NULL;
}

void control_action_list_push(
        struct ControlActionListNode **list,
        void (*loop_action)( Loop loop, jack_nframes_t time ),
        Loop loop
    ) {

    struct ControlActionListNode *newNode = malloc( sizeof( **list ) );
    newNode->action.loop_action = loop_action;
    newNode->action.loop = loop;
    newNode->next = *list;

    *list = newNode;
}

void control_action_list_trigger_all(
        struct ControlActionListNode *list,
        jack_nframes_t time
    ) {

    while( list != NULL )
    {
        list->action.loop_action(
            list->action.loop,
            time
        );

        list = list->next;
    }
}

