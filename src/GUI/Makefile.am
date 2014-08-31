SUBDIRS = jack_midi_looper_gui

bin_SCRIPTS = jmlgui
CLEANFILES = $(bin_SCRIPTS)
EXTRA_DIST = looper_gui.in

do_substitution = sed -e 's,[@]pythondir[@],$(pythondir),g' \
    -e 's,[@]PACKAGE[@],$(PACKAGE),g' \
    -e 's,[@]VERSION[@],$(VERSION),g' \
    -e 's,[@]LOG_LEVEL[@],@JML_LOG_LEVEL@,g'

jmlgui: looper_gui.in Makefile
	$(do_substitution) < $(srcdir)/looper_gui.in > jmlgui
	chmod +x jmlgui
