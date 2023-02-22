include $(top_srcdir)/build/modmake.rulesam
MODNAME=mod_call_bot

mod_LTLIBRARIES = mod_call_bot.la
mod_call_bot_la_SOURCES  = mod_call_bot.c call_bot_glue.cpp smartivrphonegateway.pb.cc smartivrphonegateway.grpc.pb.cc
mod_call_bot_la_CFLAGS   = $(AM_CFLAGS)
mod_call_bot_la_CXXFLAGS = $(AM_CXXFLAGS) -std=c++11 -lprotobuf -pthread  

mod_call_bot_la_LIBADD   = $(switch_builddir)/libfreeswitch.la
mod_call_bot_la_LDFLAGS  = -L/usr/local/lib -avoid-version -module -no-undefined -shared `pkg-config --libs --static protobuf grpc++ grpc`
