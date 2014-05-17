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

