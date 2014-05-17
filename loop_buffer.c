#include "loop_buffer.h"

#include "midi_message.h"

#include <stdlib.h>

struct loop_buffer_type {
    struct MidiMessage *buffer;
    struct MidiMessage *read_pointer;
    struct MidiMessage *write_pointer;
    struct MidiMessage *end_pointer;
};

struct loop_buffer_type *loop_buffer_init( size_t capacity ) 
{
    struct loop_buffer_type *buffer_struct = malloc(
        sizeof( struct loop_buffer_type )
    );

    if( buffer_struct == NULL ) {
        return NULL;
    }

    buffer_struct->buffer = malloc(
        sizeof( struct MidiMessage ) * capacity
    );

    if( buffer_struct->buffer == NULL ) {
        return NULL;
    }

    loop_buffer_reset_write( buffer_struct );
    buffer_struct->end_pointer = buffer_struct->buffer + capacity;

    return buffer_struct;
}

int loop_buffer_is_valid( struct loop_buffer_type *loop_buffer ) 
{

    if( loop_buffer == NULL || loop_buffer->buffer == NULL ) {
        return 0;
    }

    return 1;
}

void loop_buffer_reset_read( struct loop_buffer_type *loop_buffer )
{
    loop_buffer->read_pointer = loop_buffer->buffer;
}

void loop_buffer_reset_write( struct loop_buffer_type *loop_buffer )
{
    loop_buffer->read_pointer = NULL;
    loop_buffer->write_pointer = loop_buffer->buffer;
}

void loop_buffer_free( struct loop_buffer_type *loop_buffer )
{
    free( loop_buffer->buffer );
    free( loop_buffer );
}

int loop_buffer_push( struct loop_buffer_type *loop_buffer, struct MidiMessage *message )
{

    if( loop_buffer->write_pointer == loop_buffer->end_pointer ) {
        return -10;
    }

    if( loop_buffer->write_pointer == loop_buffer->buffer ) {
        loop_buffer->read_pointer = loop_buffer->buffer;
    }

    *( loop_buffer->write_pointer ) = *message;
    loop_buffer->write_pointer++;

    return 0;
}

struct MidiMessage *loop_buffer_peek( struct loop_buffer_type *loop_buffer, int *wrapped )
{

    if( loop_buffer->read_pointer == loop_buffer->end_pointer ) {
        loop_buffer->read_pointer = loop_buffer->buffer;
        *wrapped = 1;
    } else {
        *wrapped = 0;
    }

    return loop_buffer->read_pointer;
}

void loop_buffer_read_advance( struct loop_buffer_type *loop_buffer )
{
    if( loop_buffer->read_pointer != NULL ) {
        loop_buffer->read_pointer++;
    }
}

