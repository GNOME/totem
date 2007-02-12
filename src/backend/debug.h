#ifndef __TOTEM_DEBUG_H__
#define __TOTEM_DEBUG_H__ 1

#ifdef GNOME_ENABLE_DEBUG

#include <sys/time.h>
#include <glib.h>

#ifdef G_HAVE_ISO_VARARGS
#define D(...) g_message (__VA_ARGS__)
#elif defined(G_HAVE_GNUC_VARARGS)
#define D(x...) g_message (x)
#endif

#define TE() { g_message ("enter %s", G_GNUC_PRETTY_FUNCTION); gdk_threads_enter (); }
#define TL() { g_message ("leave %s", G_GNUC_PRETTY_FUNCTION); gdk_threads_leave (); }

#define TOTEM_PROFILE(function)     \
    do{                             \
      struct timeval current_time;  \
      double dtime;                 \
      gettimeofday(&current_time, NULL); \
      dtime = -(current_time.tv_sec + (current_time.tv_usec / 1000000.0)); \
      function;                     \
      gettimeofday(&current_time, NULL); \
      dtime += current_time.tv_sec + (current_time.tv_usec / 1000000.0); \
      printf("(%s:%d) took %lf seconds\n", \
	     G_GNUC_PRETTY_FUNCTION, __LINE__, dtime ); \
    }while(0)

#else /* GNOME_ENABLE_DEBUG */

#ifdef G_HAVE_ISO_VARARGS
#define D(...)
#elif defined(G_HAVE_GNUC_VARARGS)
#define D(x...)
#endif

#define TE() { gdk_threads_enter (); }
#define TL() { gdk_threads_leave (); }
#define TOTEM_PROFILE(function) function

#endif /* GNOME_ENABLE_DEBUG */

#endif /* __TOTEM_DEBUG_H__ */
