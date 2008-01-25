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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
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
#include <gconf/gconf-client.h>

#include <gmodule.h>
#include <string.h>

#include "totem-plugin.h"
#include "totem.h"
#include "totem-scrsaver.h"
#include "backend/bacon-video-widget.h"

#define TOTEM_TYPE_SCREENSAVER_PLUGIN		(totem_screensaver_plugin_get_type ())
#define TOTEM_SCREENSAVER_PLUGIN(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), TOTEM_TYPE_SCREENSAVER_PLUGIN, TotemScreensaverPlugin))
#define TOTEM_SCREENSAVER_PLUGIN_CLASS(k)	(G_TYPE_CHECK_CLASS_CAST((k), TOTEM_TYPE_SCREENSAVER_PLUGIN, TotemScreensaverPluginClass))
#define TOTEM_IS_SCREENSAVER_PLUGIN(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), TOTEM_TYPE_SCREENSAVER_PLUGIN))
#define TOTEM_IS_SCREENSAVER_PLUGIN_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), TOTEM_TYPE_SCREENSAVER_PLUGIN))
#define TOTEM_SCREENSAVER_PLUGIN_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), TOTEM_TYPE_SCREENSAVER_PLUGIN, TotemScreensaverPluginClass))

typedef struct
{
	TotemPlugin   parent;
	TotemObject  *totem;

	TotemScrsaver *scr;
	guint          handler_id_playing;
	guint          handler_id_gconf;
} TotemScreensaverPlugin;

typedef struct
{
	TotemPluginClass parent_class;
} TotemScreensaverPluginClass;


G_MODULE_EXPORT GType register_totem_plugin		(GTypeModule *module);
GType	totem_screensaver_plugin_get_type		(void) G_GNUC_CONST;

static void totem_screensaver_plugin_init		(TotemScreensaverPlugin *plugin);
static void totem_screensaver_plugin_finalize		(GObject *object);
static gboolean impl_activate				(TotemPlugin *plugin, TotemObject *totem, GError **error);
static void impl_deactivate				(TotemPlugin *plugin, TotemObject *totem);

TOTEM_PLUGIN_REGISTER(TotemScreensaverPlugin, totem_screensaver_plugin)

static void
totem_screensaver_plugin_class_init (TotemScreensaverPluginClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	TotemPluginClass *plugin_class = TOTEM_PLUGIN_CLASS (klass);

	object_class->finalize = totem_screensaver_plugin_finalize;

	plugin_class->activate = impl_activate;
	plugin_class->deactivate = impl_deactivate;
}

static void
totem_screensaver_plugin_init (TotemScreensaverPlugin *plugin)
{
	plugin->scr = totem_scrsaver_new ();
}

static void
totem_screensaver_plugin_finalize (GObject *object)
{
	TotemScreensaverPlugin *plugin = TOTEM_SCREENSAVER_PLUGIN (object);

	g_object_unref (plugin->scr);

	G_OBJECT_CLASS (totem_screensaver_plugin_parent_class)->finalize (object);
}

static void
totem_screensaver_update_from_state (TotemObject *totem,
				     TotemScreensaverPlugin *pi)
{
	gboolean lock_screensaver_on_audio, visual_effects, can_get_frames;
	BaconVideoWidget *bvw;
	GConfClient *gc;

	bvw = BACON_VIDEO_WIDGET (totem_get_video_widget ((Totem *)(totem)));
	gc = gconf_client_get_default ();

	visual_effects = gconf_client_get_bool (gc,
						GCONF_PREFIX"/show_vfx",
						NULL);
	lock_screensaver_on_audio = gconf_client_get_bool (gc, 
							   GCONF_PREFIX"/lock_screensaver_on_audio",
							   NULL);
	can_get_frames = bacon_video_widget_can_get_frames (bvw, NULL);

	if (totem_is_playing (totem) != FALSE && (lock_screensaver_on_audio || can_get_frames))
		totem_scrsaver_disable (pi->scr);
	else
		totem_scrsaver_enable (pi->scr);

	g_object_unref (gc);
}

static void
property_notify_cb (TotemObject *totem,
		    GParamSpec *spec,
		    TotemScreensaverPlugin *pi)
{
	totem_screensaver_update_from_state (totem, pi);
}

static void
lock_screensaver_on_audio_changed_cb (GConfClient *client, guint cnxn_id,
				      GConfEntry *entry, TotemScreensaverPlugin *pi)
{
	totem_screensaver_update_from_state (pi->totem, pi);
}

static gboolean
impl_activate (TotemPlugin *plugin,
	       TotemObject *totem,
	       GError **error)
{
	TotemScreensaverPlugin *pi = TOTEM_SCREENSAVER_PLUGIN (plugin);
	GConfClient *gc;

	gc = gconf_client_get_default ();
	gconf_client_add_dir (gc, GCONF_PREFIX,
			      GCONF_CLIENT_PRELOAD_ONELEVEL, NULL);
	pi->handler_id_gconf = gconf_client_notify_add (gc, GCONF_PREFIX"/lock_screensaver_on_audio",
							(GConfClientNotifyFunc) lock_screensaver_on_audio_changed_cb,
							plugin, NULL, NULL);
	g_object_unref (gc);

	pi->handler_id_playing = g_signal_connect (G_OBJECT (totem),
				"notify::playing",
				G_CALLBACK (property_notify_cb),
				pi);

	pi->totem = g_object_ref (totem);

	/* Force setting the current status */
	totem_screensaver_update_from_state (totem, pi);

	return TRUE;
}

static void
impl_deactivate	(TotemPlugin *plugin,
		 TotemObject *totem)
{
	TotemScreensaverPlugin *pi = TOTEM_SCREENSAVER_PLUGIN (plugin);
	GConfClient *gc;

	gc = gconf_client_get_default ();
	gconf_client_notify_remove (gc, pi->handler_id_gconf);
	g_object_unref (gc);

	g_signal_handler_disconnect (G_OBJECT (totem), pi->handler_id_playing);

	g_object_unref (pi->totem);

	totem_scrsaver_enable (pi->scr);
}

