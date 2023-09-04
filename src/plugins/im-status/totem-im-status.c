/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2007, 2011 Bastien Nocera <hadess@hadess.net>
 *
 * SPDX-License-Identifier: GPL-3-or-later
 *
 */

#include "config.h"

#include <glib.h>
#include <glib-object.h>
#include <glib/gi18n-lib.h>
#include <string.h>
#include <libpeas.h>
#include "totem-plugin-activatable.h"

#include "totem.h"
#include "totem-plugin.h"

#define TOTEM_TYPE_IM_STATUS_PLUGIN		(totem_im_status_plugin_get_type ())
#define TOTEM_IM_STATUS_PLUGIN(o)			(G_TYPE_CHECK_INSTANCE_CAST ((o), TOTEM_TYPE_IM_STATUS_PLUGIN, TotemImStatusPlugin))

typedef struct {
	PeasExtensionBase parent;

	guint		handler_id_fullscreen;
	guint		handler_id_playing;
	GCancellable   *cancellable;
	gboolean	idle; /* Whether we're idle */
	GDBusProxy     *proxy;
} TotemImStatusPlugin;

enum {
	STATUS_AVAILABLE = 0,
	STATUS_INVISIBLE = 1,
	STATUS_BUSY      = 2,
	STATUS_IDLE      = 3
};

TOTEM_PLUGIN_REGISTER (TOTEM_TYPE_IM_STATUS_PLUGIN, TotemImStatusPlugin, totem_im_status_plugin);

static void
totem_im_status_set_idleness (TotemImStatusPlugin *pi,
			      gboolean             idle)
{
	GVariant *variant;
	GError *error = NULL;

	if (pi->idle == idle)
		return;

	pi->idle = idle;
	variant = g_dbus_proxy_call_sync (pi->proxy,
					  "SetStatus",
					  g_variant_new ("(u)", idle ? STATUS_BUSY : STATUS_AVAILABLE),
					  G_DBUS_CALL_FLAGS_NONE,
					  -1,
					  NULL,
					  &error);
	if (variant == NULL) {
		g_warning ("Failed to set presence: %s", error->message);
		g_error_free (error);
		return;
	}
	g_variant_unref (variant);
}

static void
totem_im_status_update_from_state (TotemObject         *totem,
				   TotemImStatusPlugin *pi)
{
	/* Session Proxy not ready yet */
	if (pi->proxy == NULL)
		return;

	if (totem_object_is_playing (totem) != FALSE
	    && totem_object_is_fullscreen (totem) != FALSE) {
		totem_im_status_set_idleness (pi, TRUE);
	} else {
		totem_im_status_set_idleness (pi, FALSE);
	}
}

static void
property_notify_cb (TotemObject         *totem,
		    GParamSpec          *spec,
		    TotemImStatusPlugin *plugin)
{
	totem_im_status_update_from_state (totem, plugin);
}

static void
got_proxy_cb (GObject             *source_object,
	      GAsyncResult        *res,
	      TotemImStatusPlugin *pi)
{
	GError *error = NULL;
	TotemObject *totem;
	GDBusProxy *proxy;

	proxy = g_dbus_proxy_new_for_bus_finish (res, &error);
	if (!proxy) {
		g_debug ("Could not connect to SessionManager: %s", error->message);
		g_error_free (error);
		return;
	}

	pi->proxy = proxy;
	g_object_unref (pi->cancellable);
	pi->cancellable = NULL;

	g_object_get (pi, "object", &totem, NULL);
	totem_im_status_update_from_state (totem, pi);
	g_object_unref (totem);
}

static void
impl_activate (TotemPluginActivatable *plugin)
{
	TotemImStatusPlugin *pi = TOTEM_IM_STATUS_PLUGIN (plugin);
	TotemObject *totem;

	pi->cancellable = g_cancellable_new ();
	g_dbus_proxy_new_for_bus (G_BUS_TYPE_SESSION,
				  G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES |
				  G_DBUS_PROXY_FLAGS_DO_NOT_AUTO_START,
				  NULL,
				  "org.gnome.SessionManager",
				  "/org/gnome/SessionManager/Presence",
				  "org.gnome.SessionManager.Presence",
				  pi->cancellable,
				  (GAsyncReadyCallback) got_proxy_cb,
				  pi);

	g_object_get (plugin, "object", &totem, NULL);

	pi->handler_id_fullscreen = g_signal_connect (G_OBJECT (totem),
				"notify::fullscreen",
				G_CALLBACK (property_notify_cb),
				pi);
	pi->handler_id_playing = g_signal_connect (G_OBJECT (totem),
				"notify::playing",
				G_CALLBACK (property_notify_cb),
				pi);

	g_object_unref (totem);
}

static void
impl_deactivate (TotemPluginActivatable *plugin)
{
	TotemImStatusPlugin *pi = TOTEM_IM_STATUS_PLUGIN (plugin);
	TotemObject *totem;

	/* In flight? */
	if (pi->cancellable != NULL) {
		g_cancellable_cancel (pi->cancellable);
		g_clear_object (&pi->cancellable);
	}

	if (pi->proxy != NULL) {
		g_object_unref (pi->proxy);
		pi->proxy = NULL;
	}

	g_object_get (plugin, "object", &totem, NULL);

	if (pi->handler_id_fullscreen != 0) {
		g_signal_handler_disconnect (G_OBJECT (totem),
					     pi->handler_id_fullscreen);
		pi->handler_id_fullscreen = 0;
	}
	if (pi->handler_id_playing != 0) {
		g_signal_handler_disconnect (G_OBJECT (totem),
					     pi->handler_id_playing);
		pi->handler_id_playing = 0;
	}

	g_object_unref (totem);
}
