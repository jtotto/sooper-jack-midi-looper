AC_INIT([JACK MIDI Looper], [0.1])

AM_INIT_AUTOMAKE
AM_PATH_PYTHON([3.4])

AC_PROG_CC([gcc])

PKG_CHECK_MODULES(JACK, jack)
PKG_CHECK_MODULES(GLIB, glib-2.0)
PKG_CHECK_MODULES(LIBLO, liblo >= 0.10)

AC_ARG_ENABLE([py-info-logging],
    AS_HELP_STRING([--enable-py-info-logging], [Enable info-level Python logging]))

AS_IF([test "x$enable_py_info_logging" = "xyes"], [
    AC_SUBST( [JML_LOG_LEVEL],[logging.INFO] )
], [
    AC_SUBST( [JML_LOG_LEVEL],[logging.WARNING] )
])

AC_CONFIG_HEADER(config.h)

AC_CONFIG_FILES([Makefile src/Makefile src/GUI/Makefile src/GUI/jack_midi_looper_gui/Makefile])

AC_OUTPUT
