
#include <config.h>

#ifdef TOTEM_DEBUG

#include <sys/time.h>

#define D(x...) g_message (x)
#define TE() { g_message ("enter %s", __PRETTY_FUNCTION__ ); gdk_threads_enter (); }
#define TL() { g_message ("leave %s", __PRETTY_FUNCTION__); gdk_threads_leave (); }
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
	     __PRETTY_FUNCTION__, __LINE__, dtime ); \
    }while(0)
#else
#define D(x...)
#define TE() { gdk_threads_enter (); }
#define TL() { gdk_threads_leave (); }
#define TOTEM_PROFILE(function) function
#endif

