/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-

   Copyright (C) 2004-2006 Bastien Nocera <hadess@hadess.net>

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
   write to the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301  USA.

   Author: Bastien Nocera <hadess@hadess.net>
 */


#include "config.h"

#include <glib/gi18n.h>

#include <gdk/gdk.h>

#ifdef GDK_WINDOWING_X11
#include <gdk/gdkx.h>
#include <X11/keysym.h>

#ifdef HAVE_XTEST
#include <X11/extensions/XTest.h>
#endif /* HAVE_XTEST */
#endif /* GDK_WINDOWING_X11 */

#ifdef WITH_DBUS
#define GS_SERVICE   "org.gnome.ScreenSaver"
#define GS_PATH      "/org/gnome/ScreenSaver"
#define GS_INTERFACE "org.gnome.ScreenSaver"
#endif /* WITH_DBUS */

#include "totem-scrsaver.h"

#define XSCREENSAVER_MIN_TIMEOUT 60

static GObjectClass *parent_class = NULL;
static void totem_scrsaver_finalize   (GObject *object);


struct TotemScrsaverPrivate {
	/* Whether the screensaver is disabled */
	gboolean disabled;

#ifdef WITH_DBUS
        GDBusConnection *connection;
        guint watch_id;
	guint32 cookie;
#endif /* WITH_DBUS */

	/* To save the screensaver info */
	int timeout;
	int interval;
	int prefer_blanking;
	int allow_exposures;

	/* For use with XTest */
	int keycode1, keycode2;
	int *keycode;
	gboolean have_xtest;
};

G_DEFINE_TYPE(TotemScrsaver, totem_scrsaver, G_TYPE_OBJECT)

static gboolean
screensaver_is_running_dbus (TotemScrsaver *scr)
{
#ifdef WITH_DBUS
        return scr->priv->connection != NULL;
#else
	return FALSE;
#endif /* WITH_DBUS */
}

static void
screensaver_inhibit_dbus (TotemScrsaver *scr,
			  gboolean	 inhibit)
{
#ifdef WITH_DBUS
	GError *error = NULL;
        GVariant *value;

        if (scr->priv->connection == NULL)
                return;

	if (inhibit) {
                value = g_dbus_connection_invoke_method_sync (scr->priv->connection,
                                                              GS_SERVICE,
                                                              GS_PATH,
                                                              GS_INTERFACE,
                                                              "Inhibit",
                                                              g_variant_new ("(ss)",
                                                                             "Totem",
                                                                             _("Playing a movie")),
                                                              G_DBUS_INVOKE_METHOD_FLAGS_NO_AUTO_START,
                                                              -1,
                                                              NULL,
                                                              &error);
		if (error && g_error_matches (error, G_DBUS_ERROR, G_DBUS_ERROR_UNKNOWN_METHOD)) {
			/* try the old API */
                        g_clear_error (&error);
                        value = g_dbus_connection_invoke_method_sync (scr->priv->connection,
                                                                      GS_SERVICE,
                                                                      GS_PATH,
                                                                      GS_INTERFACE,
                                                                      "InhibitActivation",
                                                                      g_variant_new ("(s)",
                                                                                     _("Playing a movie")),
                                                                      G_DBUS_INVOKE_METHOD_FLAGS_NO_AUTO_START,
                                                                      -1,
                                                                      NULL,
                                                                      &error);
                }
                if (value != NULL) {
			/* save the cookie */
                        if (g_variant_is_of_type (value, G_VARIANT_TYPE ("(u)")))
			       g_variant_get (value, "(u)", &scr->priv->cookie);
                        else
                                scr->priv->cookie = 0;
                        g_variant_unref (value);
		} else {
			g_warning ("Problem inhibiting the screensaver: %s", error->message);
                        g_error_free (error);
		}

	} else {
                value = g_dbus_connection_invoke_method_sync (scr->priv->connection,
                                                              GS_SERVICE,
                                                              GS_PATH,
                                                              GS_INTERFACE,
                                                              "UnInhibit",
                                                              g_variant_new ("(u)", scr->priv->cookie),
                                                              G_DBUS_INVOKE_METHOD_FLAGS_NO_AUTO_START,
                                                              -1,
                                                              NULL,
                                                              &error);
		if (error && g_error_matches (error, G_DBUS_ERROR, G_DBUS_ERROR_UNKNOWN_METHOD)) {
			/* try the old API */
                        g_clear_error (&error);
                        value = g_dbus_connection_invoke_method_sync (scr->priv->connection,
                                                                      GS_SERVICE,
                                                                      GS_PATH,
                                                                      GS_INTERFACE,
                                                                      "AllowActivation",
                                                                      g_variant_new ("()"),
                                                                      G_DBUS_INVOKE_METHOD_FLAGS_NO_AUTO_START,
                                                                      -1,
                                                                      NULL,
                                                                      &error);
                }
                if (value != NULL) {
			/* clear the cookie */
			scr->priv->cookie = 0;
                        g_variant_unref (value);
		} else {
			g_warning ("Problem uninhibiting the screensaver: %s", error->message);
			g_error_free (error);
		}
	}
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

#ifdef WITH_DBUS
static void
screensaver_dbus_appeared_cb (GDBusConnection *connection,
                              const char      *name,
                              const char      *name_owner,
                              gpointer         user_data)
{
        TotemScrsaver *scr = TOTEM_SCRSAVER (user_data);

        scr->priv->connection = g_object_ref (connection);
}

static void
screensaver_dbus_disappeared_cb (GDBusConnection *connection,
                                 const char      *name,
                                 gpointer         user_data)
{
        TotemScrsaver *scr = TOTEM_SCRSAVER (user_data);

        g_assert (scr->priv->connection == connection);
        g_object_unref (scr->priv->connection);
        scr->priv->connection = NULL;
}
#endif

static void
screensaver_init_dbus (TotemScrsaver *scr)
{
#ifdef WITH_DBUS
        scr->priv->watch_id = g_bus_watch_name (G_BUS_TYPE_SESSION,
                                                GS_SERVICE,
                                                G_BUS_NAME_WATCHER_FLAGS_NONE,
                                                screensaver_dbus_appeared_cb,
                                                screensaver_dbus_disappeared_cb,
                                                scr, NULL);
#endif /* WITH_DBUS */
}

static void
screensaver_finalize_dbus (TotemScrsaver *scr)
{
#ifdef WITH_DBUS
        g_bus_unwatch_name (scr->priv->watch_id);

        if (scr->priv->connection != NULL)
                g_object_unref (scr->priv->connection);
#endif /* WITH_DBUS */
}

#ifdef GDK_WINDOWING_X11
static void
screensaver_enable_x11 (TotemScrsaver *scr)
{

#ifdef HAVE_XTEST
	if (scr->priv->have_xtest != FALSE)
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
	if (scr->priv->have_xtest != FALSE)
	{
		XLockDisplay (GDK_DISPLAY());
		XGetScreenSaver(GDK_DISPLAY(), &scr->priv->timeout,
				&scr->priv->interval,
				&scr->priv->prefer_blanking,
				&scr->priv->allow_exposures);
		XUnlockDisplay (GDK_DISPLAY());

		if (scr->priv->timeout != 0) {
			g_timeout_add_seconds (scr->priv->timeout / 2,
					       (GSourceFunc) fake_event, scr);
		} else {
			g_timeout_add_seconds (XSCREENSAVER_MIN_TIMEOUT / 2,
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
	scr->priv->have_xtest = (XTestQueryExtension (GDK_DISPLAY(), &a, &b, &c, &d) == True);
	if (scr->priv->have_xtest != FALSE)
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
#endif

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
#ifdef GDK_WINDOWING_X11
	screensaver_init_x11 (scr);
#else
#warning Unimplemented
#endif
}

void
totem_scrsaver_disable (TotemScrsaver *scr)
{
	g_return_if_fail (TOTEM_SCRSAVER (scr));

	if (scr->priv->disabled != FALSE)
		return;

	scr->priv->disabled = TRUE;

	if (screensaver_is_running_dbus (scr) != FALSE)
		screensaver_disable_dbus (scr);
	else 
#ifdef GDK_WINDOWING_X11
		screensaver_disable_x11 (scr);
#else
#warning Unimplemented
	{}
#endif
}

void
totem_scrsaver_enable (TotemScrsaver *scr)
{
	g_return_if_fail (TOTEM_SCRSAVER (scr));

	if (scr->priv->disabled == FALSE)
		return;

	scr->priv->disabled = FALSE;

	if (screensaver_is_running_dbus (scr) != FALSE)
		screensaver_enable_dbus (scr);
	else 
#ifdef GDK_WINDOWING_X11
		screensaver_enable_x11 (scr);
#else
#warning Unimplemented
	{}
#endif
}

void
totem_scrsaver_set_state (TotemScrsaver *scr, gboolean enable)
{
	g_return_if_fail (TOTEM_SCRSAVER (scr));

	if (scr->priv->disabled == !enable)
		return;

	if (enable == FALSE)
		totem_scrsaver_disable (scr);
	else
		totem_scrsaver_enable (scr);
}

static void
totem_scrsaver_finalize (GObject *object)
{
	TotemScrsaver *scr = TOTEM_SCRSAVER (object);

	screensaver_finalize_dbus (scr);
#ifdef GDK_WINDOWING_X11
	screensaver_finalize_x11 (scr);
#else
#warning Unimplemented
	{}
#endif

	g_free (scr->priv);

	if (G_OBJECT_CLASS (parent_class)->finalize != NULL) {
		(* G_OBJECT_CLASS (parent_class)->finalize) (object);
	}
}

