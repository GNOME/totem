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

#include <gdk/gdkx.h>

//#include <X11/X.h>
//#include <X11/Xlib.h>

#ifdef HAVE_XTEST
#include <X11/extensions/XTest.h>
#endif /* HAVE_XTEST */
#include <X11/keysym.h>

#define XSCREENSAVER_MIN_TIMEOUT 60

static GObjectClass *parent_class = NULL;
static void totem_scrsaver_class_init (TotemScrsaverClass *class);
static void totem_scrsaver_init       (TotemScrsaver      *parser);
static void totem_scrsaver_finalize   (GObject *object);


struct TotemScrsaverPrivate {
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

G_DEFINE_TYPE(TotemScrsaver, totem_scrsaver, G_TYPE_OBJECT)

static void
totem_scrsaver_class_init (TotemScrsaverClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	object_class->finalize = totem_scrsaver_finalize;
}

TotemScrsaver *
totem_scrsaver_new (void)
{
	return TOTEM_SCRSAVER (g_object_new (TOTEM_TYPE_SCRSAVER, NULL));
}

#ifdef HAVE_XTEST
static gboolean
fake_event (TotemScrsaver *scr)
{
	if (scr->priv->disabled)
	{
		XLockDisplay (GDK_DISPLAY());
		XTestFakeKeyEvent (GDK_DISPLAY(), scr->priv->keycode,
				True, CurrentTime);
		XTestFakeKeyEvent (GDK_DISPLAY(), scr->priv->keycode,
				False, CurrentTime);
		XUnlockDisplay (GDK_DISPLAY());
	}

	return TRUE;
}
#endif /* HAVE_XTEST */

static void
totem_scrsaver_init (TotemScrsaver *scr)
{
	int a, b, c, d;

	scr->priv = g_new0 (TotemScrsaverPrivate, 1);

#ifdef HAVE_XTEST
	XLockDisplay (GDK_DISPLAY());
	scr->priv->have_xtest = XTestQueryExtension (GDK_DISPLAY(), &a, &b, &c, &d);
	if(scr->priv->have_xtest == True)
	{
		scr->priv->keycode = XKeysymToKeycode (GDK_DISPLAY(), XK_Shift_L);
	}
	XUnlockDisplay (GDK_DISPLAY());
#endif /* HAVE_XTEST */
}

void
totem_scrsaver_disable (TotemScrsaver *scr)
{
	g_return_if_fail (scr->priv->disabled == FALSE);

	scr->priv->disabled = TRUE;

#ifdef HAVE_XTEST
	if (scr->priv->have_xtest == True)
	{
		XLockDisplay (GDK_DISPLAY());
		XGetScreenSaver(GDK_DISPLAY(), &scr->priv->timeout,
				&scr->priv->interval,
				&scr->priv->prefer_blanking,
				&scr->priv->allow_exposures);
		XUnlockDisplay (GDK_DISPLAY());

		if (scr->priv->timeout != 0)
		{
			g_timeout_add (scr->priv->timeout / 2 * 1000,
					(GSourceFunc) fake_event, scr);
		} else {
			g_timeout_add (XSCREENSAVER_MIN_TIMEOUT / 2 * 1000,
					(GSourceFunc) fake_event, scr);
		}

		return;
	}
#endif /* HAVE_XTEST */

	XLockDisplay (GDK_DISPLAY());
	XGetScreenSaver(GDK_DISPLAY(), &scr->priv->timeout,
			&scr->priv->interval,
			&scr->priv->prefer_blanking,
			&scr->priv->allow_exposures);
	XSetScreenSaver(GDK_DISPLAY(), 0, 0,
			DontPreferBlanking, DontAllowExposures);
	XUnlockDisplay (GDK_DISPLAY());
}

void
totem_scrsaver_enable (TotemScrsaver *scr)
{
	g_return_if_fail (scr->priv->disabled != FALSE);

	scr->priv->disabled = FALSE;

#ifdef HAVE_XTEST
	if (scr->priv->have_xtest == True)
	{
		g_source_remove_by_user_data (scr);
		return;
	}

#endif /* HAVE_XTEST */
	XLockDisplay (GDK_DISPLAY());
	XSetScreenSaver (GDK_DISPLAY(),
			scr->priv->timeout,
			scr->priv->interval,
			scr->priv->prefer_blanking,
			scr->priv->allow_exposures);
	XUnlockDisplay (GDK_DISPLAY());
}

static void
totem_scrsaver_finalize (GObject *object)
{
	TotemScrsaver *scr = TOTEM_SCRSAVER (object);

	g_source_remove_by_user_data (scr);
	g_free (scr->priv);

	if (G_OBJECT_CLASS (parent_class)->finalize != NULL) {
		(* G_OBJECT_CLASS (parent_class)->finalize) (object);
	}
}

