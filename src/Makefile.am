SUBDIRS = GUI

bin_PROGRAMS = jack_midi_looper

jack_midi_looper_CFLAGS = -Wall -std=c99 $(JACK_CFLAGS) $(GLIB_CFLAGS) $(LIBLO_CFLAGS)
jack_midi_looper_LDADD = $(JACK_LIBS) $(GLIB_LIBS) $(LIBLO_LIBS)
jack_midi_looper_SOURCES = \
    looper.c \
    loop.h \
    loop.c \
    loop_buffer.h \
    loop_buffer.c \
    midi_message.h \
    midi_message.c \
    control_action_table.h \
    control_action_table.c
