/*
**  Sinek (Media Player)
**  Copyright (c) 2001-2002 Gurer Ozen
**
**  This code is free software; you can redistribute it and/or
**  modify it under the terms of the GNU General Public License.
**
**  common structures, macros, and prototypes
*/

#include <X11/X.h>
#include <X11/Xlib.h>

typedef struct ScreenSaver ScreenSaver;

ScreenSaver *scrsaver_new(Display *display);
void scrsaver_enable(ScreenSaver *scr);
void scrsaver_disable(ScreenSaver *scr);
void scrsaver_free(ScreenSaver *scr);

