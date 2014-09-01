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

#include <assert.h>
#include <sys/mman.h>
#include <stdio.h>
#include <stdlib.h>
#include "control_action_table.h"
#include "loop.h"

#define CONTROL_ACTION_TABLE_COUNT 8192
#define CONTROL_ACTION_TABLE_SIZE (8192*(sizeof(struct ControlActionListNode *)))

struct ControlActionListNode {
    Loop loop;
    LoopControlFunc action;
    struct ControlActionListNode *next;
};

struct control_action_table_type {
    ChangeNotificationHandler table_change_handler;
    struct ControlActionListNode **table;
};

ControlActionTable control_action_table_new( ChangeNotificationHandler handler )
{
    struct control_action_table_type *new_table;
    new_table = malloc( sizeof( new_table ) );
    new_table->table_change_handler = handler;
    new_table->table = calloc(
        CONTROL_ACTION_TABLE_COUNT,
        sizeof( struct ControlActionListNode * )
    );
    if( mlock( new_table->table, CONTROL_ACTION_TABLE_SIZE ) != 0 ) {
        fprintf( stderr, "Unable to mlock MIDI mapping table\n" );
    }
    return new_table;
}

// These functions are particularly speed critical, so the duplication is worth it.
static struct ControlActionListNode **midi_lookup_reference(
        ControlActionTable this,
        unsigned char midi_channel,
        enum MidiControlType midi_type,
        unsigned char midi_value
    ) {
    // Basically a direct hash. 
    unsigned int hash_key = ( midi_channel << 9 ) | ( midi_type << 7 ) | midi_value;
    assert( hash_key >= 0 && hash_key < CONTROL_ACTION_TABLE_COUNT );
    return &( this->table[hash_key] );
}

static struct ControlActionListNode *midi_lookup(
        ControlActionTable this,
        unsigned char midi_channel,
        enum MidiControlType midi_type,
        unsigned char midi_value
    ) {
    // Basically a direct hash. 
    unsigned int hash_key = ( midi_channel << 9 ) | ( midi_type << 7 ) | midi_value;
    assert( hash_key >= 0 && hash_key < CONTROL_ACTION_TABLE_COUNT );
    return this->table[hash_key];
}

static void derive_midi_values(
        unsigned int entry,
        unsigned char *midi_channel_out,
        enum MidiControlType *midi_type_out,
        unsigned char *midi_value_out
    ) {
    *midi_channel_out = entry >> 9;
    *midi_type_out = ( entry >> 7 ) & 0x3;
    *midi_value_out = entry & 0x7f;
}

void control_action_table_insert(
        ControlActionTable this,
        unsigned char midi_channel,
        enum MidiControlType midi_type,
        unsigned char midi_value,
        Loop loop,
        LoopControlFunc control_func
    ) {
    
    control_action_table_remove(
        this, midi_channel, midi_type, midi_value, loop, control_func
    );

    struct ControlActionListNode *new_action = malloc( sizeof( *new_action ) );
    new_action->loop = loop;
    new_action->action = control_func;
    
    struct ControlActionListNode **action_list =
        midi_lookup_reference( this, midi_channel, midi_type, midi_value );
    new_action->next = *action_list;
    *action_list = new_action;

    this->table_change_handler(
        ACTION_ADD,
        midi_channel,
        midi_type,
        midi_value,
        loop,
        control_func
    );
}

void control_action_table_remove(
        ControlActionTable this,
        unsigned char midi_channel,
        enum MidiControlType midi_type,
        unsigned char midi_value,
        Loop loop,
        LoopControlFunc control_func
    ) {
    
    struct ControlActionListNode **list =
        midi_lookup_reference( this, midi_channel, midi_type, midi_value );

    // Should only ever remove one, but keeps going to squash inconsistencies.
    while( *list != NULL ) {
        if( (*list)->action == control_func && (*list)->loop == loop ) {
            struct ControlActionListNode *temp = (*list)->next;
            free( *list );
            *list = temp;

            this->table_change_handler(
                ACTION_REMOVE,
                midi_channel,
                midi_type,
                midi_value,
                loop,
                control_func
            );
        } else {
            list = &( (*list)->next );   
        }
    }
}

typedef int (*RemovalPredicate)( unsigned int, struct ControlActionListNode *, void *user_data );
static void conditional_mapping_removal(
        ControlActionTable this,
        RemovalPredicate pred,
        void *user_data
    ) {

    for( unsigned int i = 0; i < CONTROL_ACTION_TABLE_COUNT; i++ ) {
        struct ControlActionListNode **list = &( this->table[i] );
        while( *list != NULL ) {
            if( pred( i, *list, user_data ) ) {
                unsigned char midi_channel, midi_value;
                enum MidiControlType midi_type;
                derive_midi_values( i, &midi_channel, &midi_type, &midi_value );

                this->table_change_handler(
                    ACTION_REMOVE,
                    midi_channel,
                    midi_type,
                    midi_value,
                    (*list)->loop,
                    (*list)->action
                );

                struct ControlActionListNode *temp = (*list)->next;
                free( *list );
                *list = temp;
            } else {
                list = &( (*list)->next );   
            }
        }
    }
}

int trivial_removal_predicate( unsigned int not_used, struct ControlActionListNode *node, void *user_data )
{
    return 1;
}

void control_action_table_clear_mappings( ControlActionTable this )
{
    conditional_mapping_removal( this, trivial_removal_predicate, NULL );
}

int loop_removal_predicate( unsigned int not_used, struct ControlActionListNode *node, void *user_data )
{
    Loop target = user_data;
    return node->loop == target;
}

// SLOW
void control_action_table_remove_loop_mappings(
        ControlActionTable this,
        Loop loop
    ) {
    conditional_mapping_removal( this, loop_removal_predicate, loop );
}

void control_action_table_invoke(
        ControlActionTable this,
        unsigned char midi_channel,
        enum MidiControlType midi_type,
        unsigned char midi_value,
        jack_nframes_t time
    ) {

    struct ControlActionListNode *list =
        midi_lookup( this, midi_channel, midi_type, midi_value );

    while( list != NULL ) {
        list->action( list->loop, time );
        list = list->next;
    }
}

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
    ) {
    
    for( unsigned int i = 0; i < CONTROL_ACTION_TABLE_COUNT; i++ ) {
        struct ControlActionListNode *list = this->table[i];
        while( list != NULL ) {
            unsigned char midi_channel, midi_value;
            enum MidiControlType midi_type;
            derive_midi_values( i, &midi_channel, &midi_type, &midi_value );
            mapping_func(
                midi_channel,
                midi_type,
                midi_value,
                list->loop,
                list->action,
                user_data
            );
            list = list->next;
        }
    }
}

void control_action_table_free( ControlActionTable this )
{
    control_action_table_clear_mappings( this );
    munlock( this->table, CONTROL_ACTION_TABLE_SIZE );
    free( this->table );
    free( this );
}

