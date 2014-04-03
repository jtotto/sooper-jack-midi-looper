#ifndef MIDI_MESSAGE_H
#define MIDI_MESSAGE_H

#include <jack/midiport.h>

// The only difference between this struct and jack_midi_event_t is that
// the raw data storage is actually contained within the struct
// (as opposed to just a buffer pointer)
struct MidiMessage {
    jack_nframes_t time;
    int len;                     /* Length of MIDI message, in bytes. */
    unsigned char data[3];
};

int midi_message_from_port_buffer(
    struct MidiMessage *message,
    void *port_buffer,
    int event_index
);

#endif
