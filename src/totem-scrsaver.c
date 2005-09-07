/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-

   Copyright (C) 2004,2005 Bastien Nocera <hadess@hadess.net>

   The Gnome Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   The Gnome Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with the Gnome Library; see the file COPYING.LIB.  If not,
   write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.

   Author: Bastien Nocera <hadess@hadess.net>
 */


#include "config.h"

#include <glib/gi18n.h>
#include <gdk/gdkx.h>

#ifdef HAVE_XTEST
#include <X11/extensions/XTest.h>
#endif /* HAVE_XTEST */
#include <X11/keysym.h>

#ifdef WITH_DBUS
#include <dbus/dbus.h>
#include <dbus/dbus-glib.h>
#include <dbus/dbus-glib-lowlevel.h>

#define GS_SERVICE   "org.gnome.screensaver"
#define GS_PATH      "/org/gnome/screensaver"
#define GS_INTERFACE "org.gnome.screensaver"
#endif /* WITH_DBUS */

#include "totem-scrsaver.h"

#define XSCREENSAVER_MIN_TIMEOUT 60

static GObjectClass *parent_class = NULL;
static void totem_scrsaver_class_init (TotemScrsaverClass *class);
static void totem_scrsaver_init       (TotemScrsaver      *parser);
static void totem_scrsaver_finalize   (GObject *object);


struct TotemScrsaverPrivate {
	/* Whether the screensaver is disabled */
	gboolean disabled;

#ifdef WITH_DBUS
	DBusConnection *connection;
#endif /* WITH_DBUS */

	/* To save the screensaver info */
	int timeout;
	int interval;
	int prefer_blanking;
	int allow_exposures;

	/* For use with XTest */
	int keycode1, keycode2;
	int *keycode;
	Bool have_xtest;
};

G_DEFINE_TYPE(TotemScrsaver, totem_scrsaver, G_TYPE_OBJECT)

static gboolean
screensaver_is_running_dbus (TotemScrsaver *scr)
{
#ifdef WITH_DBUS
	DBusError error;
	gboolean  exists;

	if (! scr->priv->connection)
		return FALSE;

	dbus_error_init (&error);
	exists = dbus_bus_name_has_owner (scr->priv->connection, GS_SERVICE, &error);
	if (dbus_error_is_set (&error))
		dbus_error_free (&error);

	return exists;
#else
	return FALSE;
#endif /* WITH_DBUS */
}

static void
screensaver_inhibit_dbus (TotemScrsaver *scr,
			  gboolean	 inhibit)
{
#ifdef WITH_DBUS
	DBusMessage    *message;
	DBusMessage    *reply;
	DBusMessageIter iter;
	DBusError	error;
	const char     *name;

	g_return_if_fail (scr != NULL);
	g_return_if_fail (scr->priv->connection != NULL);

	if (inhibit) {
		name = "InhibitActivation";
	} else {
		name = "AllowActivation";
	}

	dbus_error_init (&error);

	message = dbus_message_new_method_call (GS_SERVICE,
						GS_PATH,
						GS_INTERFACE,
						name);
	if (message == NULL) {
		g_warning ("Couldn't allocate the dbus message");
		return;
	}

	if (inhibit) {
		char *reason;
		reason = g_strdup (_("Playing movie with totem"));
		dbus_message_iter_init_append (message, &iter);
		dbus_message_iter_append_basic (&iter, DBUS_TYPE_STRING, &reason);
		g_free (reason);
	}

	reply = dbus_connection_send_with_reply_and_block (scr->priv->connection,
							   message,
							   -1, &error);
	if (dbus_error_is_set (&error)) {
		g_warning ("%s raised:\n %s\n\n", error.name, error.message);
		reply = NULL;
	}

	dbus_connection_flush (scr->priv->connection);

	dbus_message_unref (message);
	dbus_error_free (&error);
#endif /* WITH_DBUS */
}

static void
screensaver_enable_dbus (TotemScrsaver *scr)
{
	screensaver_inhibit_dbus (scr, FALSE);
}

static void
screensaver_disable_dbus (TotemScrsaver *scr)
{
	screensaver_inhibit_dbus (scr, TRUE);
}

static void
screensaver_init_dbus (TotemScrsaver *scr)
{
#ifdef WITH_DBUS
	DBusError dbus_error;	     

	dbus_error_init (&dbus_error);
	scr->priv->connection = dbus_bus_get (DBUS_BUS_SESSION, &dbus_error);
	if (! scr->priv->connection) {
		g_warning ("Failed to connect to the D-BUS daemon: %s", dbus_error.message);
		dbus_error_free (&dbus_error);
		return;
	}

	dbus_connection_setup_with_g_main (scr->priv->connection, NULL);
#endif /* WITH_DBUS */
}

static void
screensaver_finalize_dbus (TotemScrsaver *scr)
{
#ifdef WITH_DBUS
	if (scr->priv->connection)
		dbus_connection_disconnect (scr->priv->connection);
#endif /* WITH_DBUS */
}

static void
screensaver_enable_x11 (TotemScrsaver *scr)
{

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

#ifdef HAVE_XTEST
static gboolean
fake_event (TotemScrsaver *scr)
{
	if (scr->priv->disabled)
	{
		XLockDisplay (GDK_DISPLAY());
		XTestFakeKeyEvent (GDK_DISPLAY(), *scr->priv->keycode,
				True, CurrentTime);
		XTestFakeKeyEvent (GDK_DISPLAY(), *scr->priv->keycode,
				False, CurrentTime);
		XUnlockDisplay (GDK_DISPLAY());
		/* Swap the keycode */
		if (scr->priv->keycode == &scr->priv->keycode1)
			scr->priv->keycode = &scr->priv->keycode2;
		else
			scr->priv->keycode = &scr->priv->keycode1;
	}

	return TRUE;
}
#endif /* HAVE_XTEST */

static void
screensaver_disable_x11 (TotemScrsaver *scr)
{

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

static void
screensaver_init_x11 (TotemScrsaver *scr)
{
#ifdef HAVE_XTEST
	int a, b, c, d;

	XLockDisplay (GDK_DISPLAY());
	scr->priv->have_xtest = XTestQueryExtension (GDK_DISPLAY(), &a, &b, &c, &d);
	if(scr->priv->have_xtest == True)
	{
		scr->priv->keycode1 = XKeysymToKeycode (GDK_DISPLAY(), XK_Alt_L);
		if (scr->priv->keycode1 == 0) {
			g_warning ("scr->priv->keycode1 not existant");
		}
		scr->priv->keycode2 = XKeysymToKeycode (GDK_DISPLAY(), XK_Alt_R);
		if (scr->priv->keycode2 == 0) {
			scr->priv->keycode2 = XKeysymToKeycode (GDK_DISPLAY(), XK_Alt_L);
			if (scr->priv->keycode2 == 0) {
				g_warning ("scr->priv->keycode2 not existant");
			}
		}
		scr->priv->keycode = &scr->priv->keycode1;
	}
	XUnlockDisplay (GDK_DISPLAY());
#endif /* HAVE_XTEST */
}

static void
screensaver_finalize_x11 (TotemScrsaver *scr)
{
	g_source_remove_by_user_data (scr);
}

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

static void
totem_scrsaver_init (TotemScrsaver *scr)
{
	scr->priv = g_new0 (TotemScrsaverPrivate, 1);

	screensaver_init_dbus (scr);
	screensaver_init_x11 (scr);
}

void
totem_scrsaver_disable (TotemScrsaver *scr)
{
	if (scr->priv->disabled != FALSE)
		return;

	scr->priv->disabled = TRUE;

	if (screensaver_is_running_dbus (scr))
		screensaver_disable_dbus (scr);
	else 
		screensaver_disable_x11 (scr);
}

void
totem_scrsaver_enable (TotemScrsaver *scr)
{
	if (scr->priv->disabled == FALSE)
		return;

	scr->priv->disabled = FALSE;

	if (screensaver_is_running_dbus (scr))
		screensaver_enable_dbus (scr);
	else 
		screensaver_enable_x11 (scr);
}

static void
totem_scrsaver_finalize (GObject *object)
{
	TotemScrsaver *scr = TOTEM_SCRSAVER (object);

	screensaver_finalize_dbus (scr);
	screensaver_finalize_x11 (scr);

	g_free (scr->priv);

	if (G_OBJECT_CLASS (parent_class)->finalize != NULL) {
		(* G_OBJECT_CLASS (parent_class)->finalize) (object);
	}
}

