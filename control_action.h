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
