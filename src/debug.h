
#include <config.h>

#define TE() { gdk_threads_enter (); }
#define TL() { gdk_threads_leave (); }

#ifdef TOTEM_DEBUG
#define D(x...) g_message (x)
#define TE() { g_message ("enter %s", G_GNUC_PRETTY_FUNCTION); gdk_threads_enter (); }
#define TL() { g_message ("leave %s", G_GNUC_PRETTY_FUNCTION); gdk_threads_leave (); }
#else
#define D(x...)
#endif


