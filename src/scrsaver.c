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
#include <gdk/gdkx.h>
#ifdef HAVE_XTEST
#include <X11/extensions/XTest.h>
#endif /* HAVE_XTEST */
#include <X11/keysym.h>

#define XSCREENSAVER_MIN_TIMEOUT 60

struct ScreenSaver {
	/* Whether the screensaver is disabled */
	gboolean disabled;

	/* To save the screensaver info */
	int timeout;
	int interval;
	int prefer_blanking;
	int allow_exposures;

	/* For use with XTest */
	int keycode;
	Bool have_xtest;
};

#ifdef HAVE_XTEST
static gboolean
fake_event (ScreenSaver *scr)
{
	if (scr->disabled)
	{
		XLockDisplay (GDK_DISPLAY());
		XTestFakeKeyEvent (GDK_DISPLAY(), scr->keycode,
				True, CurrentTime);
		XTestFakeKeyEvent (GDK_DISPLAY(), scr->keycode,
				False, CurrentTime);
		XUnlockDisplay (GDK_DISPLAY());
	}

	return TRUE;
}
#endif /* HAVE_XTEST */

ScreenSaver
*scrsaver_new (void)
{
	ScreenSaver *scr;
	int a, b, c, d;

	scr = g_new0 (ScreenSaver, 1);

#ifdef HAVE_XTEST
	XLockDisplay (GDK_DISPLAY());
	scr->have_xtest = XTestQueryExtension (GDK_DISPLAY(), &a, &b, &c, &d);
	if(scr->have_xtest == True)
	{
		scr->keycode = XKeysymToKeycode (GDK_DISPLAY(), XK_Shift_L);
	}
	XUnlockDisplay (GDK_DISPLAY());
#endif /* HAVE_XTEST */

	return scr;
}

void
scrsaver_disable (ScreenSaver *scr)
{
	g_return_if_fail (scr->disabled == FALSE);

	scr->disabled = TRUE;

#ifdef HAVE_XTEST
	if (scr->have_xtest == True)
	{
		XLockDisplay (GDK_DISPLAY());
		XGetScreenSaver(GDK_DISPLAY(), &scr->timeout,
				&scr->interval,
				&scr->prefer_blanking,
				&scr->allow_exposures);
		XUnlockDisplay (GDK_DISPLAY());

		if (scr->timeout != 0)
		{
			g_timeout_add (scr->timeout / 2 * 1000,
					(GSourceFunc) fake_event, scr);
		} else {
			g_timeout_add (XSCREENSAVER_MIN_TIMEOUT / 2 * 1000,
					(GSourceFunc) fake_event, scr);
		}

		return;
	}
#endif /* HAVE_XTEST */

	XLockDisplay (GDK_DISPLAY());
	XGetScreenSaver(GDK_DISPLAY(), &scr->timeout,
			&scr->interval,
			&scr->prefer_blanking,
			&scr->allow_exposures);
	XSetScreenSaver(GDK_DISPLAY(), 0, 0,
			DontPreferBlanking, DontAllowExposures);
	XUnlockDisplay (GDK_DISPLAY());
}

void
scrsaver_enable (ScreenSaver *scr)
{
	g_return_if_fail (scr->disabled == TRUE);

	scr->disabled = FALSE;

#ifdef HAVE_XTEST
	if (scr->have_xtest == True)
	{
		g_source_remove_by_user_data (scr);
		return;
	}

#endif /* HAVE_XTEST */
	XLockDisplay (GDK_DISPLAY());
	XSetScreenSaver (GDK_DISPLAY(),
			scr->timeout,
			scr->interval,
			scr->prefer_blanking,
			scr->allow_exposures);
	XUnlockDisplay (GDK_DISPLAY());
}

void scrsaver_free(ScreenSaver *scr)
{
	g_source_remove_by_user_data (scr);
	g_free (scr);
}

