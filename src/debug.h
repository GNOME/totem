
#include <config.h>

#ifndef TOTEM_DEBUG
#define D(x...)
#define TE() { g_message ("gdk_threads_enter %s", G_GNUC_PRETTY_FUNCTION); gdk_threads_enter (); }
#define TL() { g_message ("gdk_threads_leave %s", G_GNUC_PRETTY_FUNCTION); gdk_threads_leave (); }
#else
#define D(x...) g_message (x)
#define TE()
#define TL()
#endif


