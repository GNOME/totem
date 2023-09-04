/*
 *  Copyright (C) 2013 Bastien Nocera <hadess@hadess.net>
 *
 * SPDX-License-Identifier: GPL-3-or-later
 *
 */


#include "config.h"

#include <glib-object.h>

#include "totem-plugin.h"
#include "totem.h"

#define TOTEM_TYPE_VIMEO_PLUGIN	(totem_vimeo_plugin_get_type ())
#define TOTEM_VIMEO_PLUGIN(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), TOTEM_TYPE_VIMEO_PLUGIN, TotemVimeoPlugin))

typedef struct {
	PeasExtensionBase parent;

	guint signal_id;
	TotemObject *totem;
} TotemVimeoPlugin;

TOTEM_PLUGIN_REGISTER(TOTEM_TYPE_VIMEO_PLUGIN, TotemVimeoPlugin, totem_vimeo_plugin)

static char *
get_user_agent_cb (TotemObject *totem,
		   const char  *mrl)
{
	if (g_str_has_prefix (mrl, "http://vimeo.com") ||
	    g_str_has_prefix (mrl, "http://player.vimeo.com"))
		return g_strdup ("Mozilla/5.0");
	return NULL;
}

static void
impl_activate (TotemPluginActivatable *plugin)
{
	TotemVimeoPlugin *pi = TOTEM_VIMEO_PLUGIN (plugin);

	pi->totem = g_object_ref (g_object_get_data (G_OBJECT (plugin), "object"));
	pi->signal_id = g_signal_connect (G_OBJECT (pi->totem), "get-user-agent",
						G_CALLBACK (get_user_agent_cb), NULL);
}

static void
impl_deactivate (TotemPluginActivatable *plugin)
{
	TotemVimeoPlugin *pi = TOTEM_VIMEO_PLUGIN (plugin);

	if (pi->signal_id) {
		g_signal_handler_disconnect (pi->totem, pi->signal_id);
		pi->signal_id = 0;
	}

	if (pi->totem) {
		g_object_unref (pi->totem);
		pi->totem = NULL;
	}
}
