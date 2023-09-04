/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2007 Bastien Nocera <hadess@hadess.net>
 *
 * SPDX-License-Identifier: GPL-3-or-later
 *
 */

#include "config.h"

#include <glib.h>
#include <glib-object.h>
#include <glib/gi18n-lib.h>
#include <libpeas.h>
#include "totem-plugin-activatable.h"
#include <string.h>

#include "totem-plugin.h"
#include "totem.h"
#include "backend/bacon-video-widget.h"

#define TOTEM_TYPE_SCREENSAVER_PLUGIN		(totem_screensaver_plugin_get_type ())
#define TOTEM_SCREENSAVER_PLUGIN(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), TOTEM_TYPE_SCREENSAVER_PLUGIN, TotemScreensaverPlugin))

typedef struct {
	PeasExtensionBase parent;

	TotemObject *totem;
	BaconVideoWidget *bvw;

	GDBusProxy    *screensaver;
	GCancellable  *cancellable;

	gboolean       inhibit_available;
	guint          handler_id_playing;
	guint          inhibit_cookie;
	guint          uninhibit_timeout;
} TotemScreensaverPlugin;

TOTEM_PLUGIN_REGISTER(TOTEM_TYPE_SCREENSAVER_PLUGIN,
		      TotemScreensaverPlugin,
		      totem_screensaver_plugin)

static void
totem_screensaver_update_from_state (TotemObject *totem,
				     TotemScreensaverPlugin *pi)
{
	if (totem_object_is_playing (totem) != FALSE) {
		if (pi->inhibit_cookie == 0 &&
		    pi->inhibit_available) {
			GtkWindow *window;

			window = totem_object_get_main_window (totem);
			pi->inhibit_cookie = gtk_application_inhibit (GTK_APPLICATION (totem),
										window,
										GTK_APPLICATION_INHIBIT_IDLE,
										_("Playing a movie"));
			if (pi->inhibit_cookie == 0)
				pi->inhibit_available = FALSE;
			g_object_unref (window);
		}
	} else {
		if (pi->inhibit_cookie != 0) {
			gtk_application_uninhibit (GTK_APPLICATION (pi->totem), pi->inhibit_cookie);
			pi->inhibit_cookie = 0;
		}
	}
}

static gboolean
uninhibit_timeout_cb (TotemScreensaverPlugin *pi)
{
	totem_screensaver_update_from_state (pi->totem, pi);
	pi->uninhibit_timeout = 0;
	return G_SOURCE_REMOVE;
}

static void
property_notify_cb (TotemObject *totem,
		    GParamSpec *spec,
		    TotemScreensaverPlugin *pi)
{
	if (pi->uninhibit_timeout != 0) {
		g_source_remove (pi->uninhibit_timeout);
		pi->uninhibit_timeout = 0;
	}

	if (totem_object_is_playing (totem) == FALSE) {
		pi->uninhibit_timeout = g_timeout_add_seconds (5, (GSourceFunc) uninhibit_timeout_cb, pi);
		g_source_set_name_by_id (pi->uninhibit_timeout, "[totem] uninhibit_timeout_cb");
		return;
	}

	totem_screensaver_update_from_state (totem, pi);
}

static void
screensaver_signal_cb (GDBusProxy  *proxy,
		       const gchar *sender_name,
		       const gchar *signal_name,
		       GVariant    *parameters,
		       gpointer     user_data)
{
	TotemScreensaverPlugin *pi = user_data;

	if (g_strcmp0 (signal_name, "ActiveChanged") == 0) {
		gboolean active;

		g_variant_get (parameters, "(b)", &active);
		if (active)
			totem_object_pause (pi->totem);
	}
}

static void
screensaver_proxy_ready_cb (GObject      *source_object,
			    GAsyncResult *res,
			    gpointer      user_data)
{
	TotemScreensaverPlugin *pi;
	GDBusProxy *proxy;
	GError *error = NULL;

	proxy = g_dbus_proxy_new_for_bus_finish (res, &error);
	if (!proxy) {
		if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
			g_warning ("Failed to acquire screensaver proxy: %s", error->message);
		g_error_free (error);
		return;
	}

	pi = TOTEM_SCREENSAVER_PLUGIN (user_data);
	pi->screensaver = proxy;
	g_signal_connect (G_OBJECT (proxy), "g-signal",
			  G_CALLBACK (screensaver_signal_cb), pi);
}

static void
impl_activate (TotemPluginActivatable *plugin)
{
	TotemScreensaverPlugin *pi = TOTEM_SCREENSAVER_PLUGIN (plugin);
	TotemObject *totem;

	pi->inhibit_available = TRUE;

	totem = g_object_get_data (G_OBJECT (plugin), "object");
	pi->bvw = BACON_VIDEO_WIDGET (totem_object_get_video_widget (totem));

	pi->handler_id_playing = g_signal_connect (G_OBJECT (totem),
						   "notify::playing",
						   G_CALLBACK (property_notify_cb),
						   pi);

	pi->totem = g_object_ref (totem);

	pi->cancellable = g_cancellable_new ();
	g_dbus_proxy_new_for_bus (G_BUS_TYPE_SESSION,
				  G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES | G_DBUS_PROXY_FLAGS_DO_NOT_AUTO_START,
				  NULL,
				  "org.gnome.ScreenSaver",
				  "/org/gnome/ScreenSaver",
				  "org.gnome.ScreenSaver",
				  pi->cancellable,
				  screensaver_proxy_ready_cb,
				  pi);

	/* Force setting the current status */
	totem_screensaver_update_from_state (totem, pi);
}

static void
impl_deactivate	(TotemPluginActivatable *plugin)
{
	TotemScreensaverPlugin *pi = TOTEM_SCREENSAVER_PLUGIN (plugin);

	if (pi->cancellable) {
		g_cancellable_cancel (pi->cancellable);
		g_clear_object (&pi->cancellable);
	}
	g_clear_object (&pi->screensaver);

	if (pi->handler_id_playing != 0) {
		TotemObject *totem;
		totem = g_object_get_data (G_OBJECT (plugin), "object");
		g_signal_handler_disconnect (G_OBJECT (totem), pi->handler_id_playing);
		pi->handler_id_playing = 0;
	}

	if (pi->uninhibit_timeout != 0) {
		g_source_remove (pi->uninhibit_timeout);
		pi->uninhibit_timeout = 0;
	}

	if (pi->inhibit_cookie != 0) {
		gtk_application_uninhibit (GTK_APPLICATION (pi->totem), pi->inhibit_cookie);
		pi->inhibit_cookie = 0;
	}

	g_object_unref (pi->totem);
	g_object_unref (pi->bvw);
}

