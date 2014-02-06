/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * Copyright (C) 2007 Bastien Nocera <hadess@hadess.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA.
 *
 * The Totem project hereby grant permission for non-gpl compatible GStreamer
 * plugins to be used and distributed together with GStreamer and Totem. This
 * permission are above and beyond the permissions granted by the GPL license
 * Totem is covered by.
 *
 * See license_change file for details.
 *
 */

#include "config.h"

#include <glib.h>
#include <glib-object.h>
#include <glib/gi18n-lib.h>
#include <libpeas/peas-extension-base.h>
#include <libpeas/peas-object-module.h>
#include <libpeas/peas-activatable.h>
#include <string.h>

#include "totem-plugin.h"
#include "totem.h"
#include "backend/bacon-video-widget.h"

#define TOTEM_TYPE_SCREENSAVER_PLUGIN		(totem_screensaver_plugin_get_type ())
#define TOTEM_SCREENSAVER_PLUGIN(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), TOTEM_TYPE_SCREENSAVER_PLUGIN, TotemScreensaverPlugin))
#define TOTEM_SCREENSAVER_PLUGIN_CLASS(k)	(G_TYPE_CHECK_CLASS_CAST((k), TOTEM_TYPE_SCREENSAVER_PLUGIN, TotemScreensaverPluginClass))
#define TOTEM_IS_SCREENSAVER_PLUGIN(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), TOTEM_TYPE_SCREENSAVER_PLUGIN))
#define TOTEM_IS_SCREENSAVER_PLUGIN_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), TOTEM_TYPE_SCREENSAVER_PLUGIN))
#define TOTEM_SCREENSAVER_PLUGIN_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), TOTEM_TYPE_SCREENSAVER_PLUGIN, TotemScreensaverPluginClass))

typedef struct {
	TotemObject *totem;
	BaconVideoWidget *bvw;

	GDBusProxy    *screensaver;
	GCancellable  *cancellable;

	gboolean       inhibit_available;
	guint          handler_id_playing;
	guint          inhibit_cookie;
	guint          uninhibit_timeout;
} TotemScreensaverPluginPrivate;

TOTEM_PLUGIN_REGISTER(TOTEM_TYPE_SCREENSAVER_PLUGIN,
		      TotemScreensaverPlugin,
		      totem_screensaver_plugin)

static void
totem_screensaver_update_from_state (TotemObject *totem,
				     TotemScreensaverPlugin *pi)
{
	if (totem_object_is_playing (totem) != FALSE) {
		if (pi->priv->inhibit_cookie == 0 &&
		    pi->priv->inhibit_available) {
			GtkWindow *window;

			window = totem_object_get_main_window (totem);
			pi->priv->inhibit_cookie = gtk_application_inhibit (GTK_APPLICATION (totem),
										window,
										GTK_APPLICATION_INHIBIT_IDLE,
										_("Playing a movie"));
			if (pi->priv->inhibit_cookie == 0)
				pi->priv->inhibit_available = FALSE;
			g_object_unref (window);
		}
	} else {
		if (pi->priv->inhibit_cookie != 0) {
			gtk_application_uninhibit (GTK_APPLICATION (pi->priv->totem), pi->priv->inhibit_cookie);
			pi->priv->inhibit_cookie = 0;
		}
	}
}

static gboolean
uninhibit_timeout_cb (TotemScreensaverPlugin *pi)
{
	totem_screensaver_update_from_state (pi->priv->totem, pi);
	pi->priv->uninhibit_timeout = 0;
	return G_SOURCE_REMOVE;
}

static void
property_notify_cb (TotemObject *totem,
		    GParamSpec *spec,
		    TotemScreensaverPlugin *pi)
{
	if (pi->priv->uninhibit_timeout != 0) {
		g_source_remove (pi->priv->uninhibit_timeout);
		pi->priv->uninhibit_timeout = 0;
	}

	if (totem_object_is_playing (totem) == FALSE) {
		pi->priv->uninhibit_timeout = g_timeout_add_seconds (5, (GSourceFunc) uninhibit_timeout_cb, pi);
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
			totem_object_pause (pi->priv->totem);
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
	pi->priv->screensaver = proxy;
	g_signal_connect (G_OBJECT (proxy), "g-signal",
			  G_CALLBACK (screensaver_signal_cb), pi);
}

static void
impl_activate (PeasActivatable *plugin)
{
	TotemScreensaverPlugin *pi = TOTEM_SCREENSAVER_PLUGIN (plugin);
	TotemObject *totem;

	pi->priv->inhibit_available = TRUE;

	totem = g_object_get_data (G_OBJECT (plugin), "object");
	pi->priv->bvw = BACON_VIDEO_WIDGET (totem_object_get_video_widget (totem));

	pi->priv->handler_id_playing = g_signal_connect (G_OBJECT (totem),
						   "notify::playing",
						   G_CALLBACK (property_notify_cb),
						   pi);

	pi->priv->totem = g_object_ref (totem);

	pi->priv->cancellable = g_cancellable_new ();
	g_dbus_proxy_new_for_bus (G_BUS_TYPE_SESSION,
				  G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES,
				  NULL,
				  "org.gnome.ScreenSaver",
				  "/org/gnome/ScreenSaver",
				  "org.gnome.ScreenSaver",
				  pi->priv->cancellable,
				  screensaver_proxy_ready_cb,
				  pi);

	/* Force setting the current status */
	totem_screensaver_update_from_state (totem, pi);
}

static void
impl_deactivate	(PeasActivatable *plugin)
{
	TotemScreensaverPlugin *pi = TOTEM_SCREENSAVER_PLUGIN (plugin);

	if (pi->priv->cancellable) {
		g_cancellable_cancel (pi->priv->cancellable);
		g_clear_object (&pi->priv->cancellable);
	}
	g_clear_object (&pi->priv->screensaver);

	if (pi->priv->handler_id_playing != 0) {
		TotemObject *totem;
		totem = g_object_get_data (G_OBJECT (plugin), "object");
		g_signal_handler_disconnect (G_OBJECT (totem), pi->priv->handler_id_playing);
		pi->priv->handler_id_playing = 0;
	}

	if (pi->priv->uninhibit_timeout != 0) {
		g_source_remove (pi->priv->uninhibit_timeout);
		pi->priv->uninhibit_timeout = 0;
	}

	if (pi->priv->inhibit_cookie != 0) {
		gtk_application_uninhibit (GTK_APPLICATION (pi->priv->totem), pi->priv->inhibit_cookie);
		pi->priv->inhibit_cookie = 0;
	}

	g_object_unref (pi->priv->totem);
	g_object_unref (pi->priv->bvw);
}

