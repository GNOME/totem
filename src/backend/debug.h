
#include <config.h>

#ifndef TOTEM_DEBUG
#define D(x...)
#else
#define D(x...) g_message (x)
#endif

