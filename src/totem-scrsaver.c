/*
**  Sinek (Media Player)
**  Copyright (c) 2001-2002 Gurer Ozen
**
**  This code is free software; you can redistribute it and/or
**  modify it under the terms of the GNU General Public License.
**
**  screen saver control
*/

#include "config.h"
#include "scrsaver.h"

#include <glib.h>
#ifdef HAVE_XTEST
#include <X11/extensions/XTest.h>
#endif /* HAVE_XTEST */
#include <X11/keysym.h>

struct ScreenSaver {
	Display *display;
	int disabled;
	int timeout;
	int interval;
	int prefer_blanking;
	int allow_exposures;
	int keycode;
	Bool xtest;
};

#ifdef HAVE_XTEST
static gboolean
fake_event (ScreenSaver *scr)
{
	if (scr->disabled)
	{
		XLockDisplay (scr->display);
		XTestFakeKeyEvent (scr->display, scr->keycode,
				True, CurrentTime);
		XTestFakeKeyEvent (scr->display, scr->keycode,
				False, CurrentTime);
		XUnlockDisplay (scr->display);
	}

	return TRUE;
}
#endif /* HAVE_XTEST */

ScreenSaver
*scrsaver_new (Display *display)
{
	ScreenSaver *scr;
	int a, b, c, d;

	scr = g_new0 (ScreenSaver, 1);
	scr->display = display;

#ifdef HAVE_XTEST
	XLockDisplay (display);
	scr->xtest = XTestQueryExtension (display, &a, &b, &c, &d);
	if(scr->xtest == True)
	{
		scr->keycode = XKeysymToKeycode (display, XK_Shift_L);
		XGetScreenSaver (scr->display, &scr->timeout,
				&scr->interval,
				&scr->prefer_blanking,
				&scr->allow_exposures);
		g_timeout_add (scr->timeout / 2 * 1000,
				(GSourceFunc) fake_event, scr);
	}
	XSync (display, False);
	XUnlockDisplay (display);
#endif /* HAVE_XTEST */

	return scr;
}

void
scrsaver_disable (ScreenSaver *scr)
{
#ifdef HAVE_XTEST
	if (scr->xtest == True)
	{
		scr->disabled = 1;
		return;
	}
#endif /* HAVE_XTEST */
	if (!scr->disabled)
	{
		XLockDisplay (scr->display);
		XGetScreenSaver(scr->display, &scr->timeout,
				&scr->interval,
				&scr->prefer_blanking,
				&scr->allow_exposures);
		XSetScreenSaver(scr->display, 0, 0,
				DontPreferBlanking, DontAllowExposures);
		XUnlockDisplay (scr->display);
		scr->disabled = 1;
	}
}

void
scrsaver_enable (ScreenSaver *scr)
{
#ifdef HAVE_XTEST
	if(scr->xtest == True)
	{
		scr->disabled = 0;
		return;
	}
#endif /* HAVE_XTEST */
	if(scr->disabled)
	{
		XLockDisplay (scr->display);
		XSetScreenSaver(scr->display,
				scr->timeout,
				scr->interval,
				scr->prefer_blanking,
				scr->allow_exposures);
		XUnlockDisplay (scr->display);
		scr->disabled = 0;
	}
}

void scrsaver_free(ScreenSaver *scr)
{
	g_source_remove_by_user_data (scr);
	g_free (scr);
}

