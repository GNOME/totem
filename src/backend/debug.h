
#include <config.h>

#ifdef TOTEM_DEBUG
#define D(x...) g_message (x)
#define TE() { g_message ("enter %s", __PRETTY_FUNCTION__ ); gdk_threads_enter (); }
#define TL() { g_message ("leave %s", __PRETTY_FUNCTION__); gdk_threads_leave (); }
#else
#define D(x...)
#define TE() { gdk_threads_enter (); }
#define TL() { gdk_threads_leave (); }
#endif


