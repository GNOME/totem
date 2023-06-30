/*
 *  Copyright (C) 2012 Bastien Nocera <hadess@hadess.net>
 *
 * SPDX-License-Identifier: GPL-3-or-later
 *
 */


#include "config.h"

#include <glib-object.h>

#include "totem-plugin.h"
#include "totem.h"

#define TOTEM_TYPE_APPLE_TRAILERS_PLUGIN	(totem_apple_trailers_plugin_get_type ())
#define TOTEM_APPLE_TRAILERS_PLUGIN(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), TOTEM_TYPE_APPLE_TRAILERS_PLUGIN, TotemAppleTrailersPlugin))

typedef struct {
	PeasExtensionBase parent;

	guint signal_id;
	TotemObject *totem;
} TotemAppleTrailersPlugin;

TOTEM_PLUGIN_REGISTER(TOTEM_TYPE_APPLE_TRAILERS_PLUGIN, TotemAppleTrailersPlugin, totem_apple_trailers_plugin)

static char *
get_user_agent_cb (TotemObject *totem,
		   const char  *mrl)
{
	if (g_str_has_prefix (mrl, "http://movies.apple.com") ||
	    g_str_has_prefix (mrl, "http://trailers.apple.com"))
		return g_strdup ("Quicktime/7.2.0");
	return NULL;
}

static void
impl_activate (PeasActivatable *plugin)
{
	TotemAppleTrailersPlugin *pi = TOTEM_APPLE_TRAILERS_PLUGIN (plugin);

	pi->totem = g_object_ref (g_object_get_data (G_OBJECT (plugin), "object"));
	pi->signal_id = g_signal_connect (G_OBJECT (pi->totem), "get-user-agent",
						G_CALLBACK (get_user_agent_cb), NULL);
}

static void
impl_deactivate (PeasActivatable *plugin)
{
	TotemAppleTrailersPlugin *pi = TOTEM_APPLE_TRAILERS_PLUGIN (plugin);

	if (pi->signal_id) {
		g_signal_handler_disconnect (pi->totem, pi->signal_id);
		pi->signal_id = 0;
	}

	if (pi->totem) {
		g_object_unref (pi->totem);
		pi->totem = NULL;
	}
}
