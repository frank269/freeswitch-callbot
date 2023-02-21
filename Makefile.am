include $(top_srcdir)/build/modmake.rulesam
MODNAME=mod_call_bot

mod_LTLIBRARIES = mod_call_bot.la
mod_call_bot_la_SOURCES  = mod_call_bot.c
mod_call_bot_la_CFLAGS   = $(AM_CFLAGS)

mod_call_bot_la_LIBADD   = $(switch_builddir)/libfreeswitch.la
mod_call_bot_la_LDFLAGS  = -avoid-version -module -no-undefined -shared
