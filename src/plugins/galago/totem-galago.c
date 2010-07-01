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
#include <string.h>
#include <libgalago/galago.h>
#include <libpeas/peas-extension-base.h>
#include <libpeas/peas-object-module.h>
#include <libpeas/peas-activatable.h>

#include "totem.h"
#include "totem-interface.h"
#include "totem-plugin.h"

#define TOTEM_TYPE_GALAGO_PLUGIN		(totem_galago_plugin_get_type ())
#define TOTEM_GALAGO_PLUGIN(o)			(G_TYPE_CHECK_INSTANCE_CAST ((o), TOTEM_TYPE_GALAGO_PLUGIN, TotemGalagoPlugin))
#define TOTEM_GALAGO_PLUGIN_CLASS(k)		(G_TYPE_CHECK_CLASS_CAST((k), TOTEM_TYPE_GALAGO_PLUGIN, TotemGalagoPluginClass))
#define TOTEM_IS_GALAGO_PLUGIN(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), TOTEM_TYPE_GALAGO_PLUGIN))
#define TOTEM_IS_GALAGO_PLUGIN_CLASS(k)		(G_TYPE_CHECK_CLASS_TYPE ((k), TOTEM_TYPE_GALAGO_PLUGIN))
#define TOTEM_GALAGO_PLUGIN_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), TOTEM_TYPE_GALAGO_PLUGIN, TotemGalagoPluginClass))

typedef struct {
	PeasExtensionBase parent;

	guint		handler_id_fullscreen;
	guint		handler_id_playing;
	gboolean	idle; /* Whether we're idle */
	GalagoPerson	*me; /* Me! */
} TotemGalagoPlugin;

typedef struct {
	PeasExtensionBaseClass parent_class;
} TotemGalagoPluginClass;

GType totem_galago_plugin_get_type		(void) G_GNUC_CONST;

static void totem_galago_plugin_dispose		(GObject *object);
static void totem_galago_plugin_finalize	(GObject *object);

TOTEM_PLUGIN_REGISTER (TOTEM_TYPE_GALAGO_PLUGIN, TotemGalagoPlugin, totem_galago_plugin);

static void
totem_galago_plugin_class_init (TotemGalagoPluginClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->dispose = totem_galago_plugin_dispose;
	object_class->finalize = totem_galago_plugin_finalize;
}

static void
totem_galago_plugin_init (TotemGalagoPlugin *plugin)
{
	if (galago_init (PACKAGE_NAME, GALAGO_INIT_FEED) == FALSE
	    || galago_is_connected () == FALSE) {
		g_warning ("Failed to initialise libgalago.");
		return;
	}

	/* Get "me" and list accounts */
	plugin->me = galago_get_me (GALAGO_REMOTE, TRUE);
}

static void
totem_galago_plugin_dispose (GObject *object)
{
	TotemGalagoPlugin *plugin = TOTEM_GALAGO_PLUGIN (object);

	if (plugin->me != NULL) {
		g_object_unref (plugin->me);
		plugin->me = NULL;
	}

	G_OBJECT_CLASS (totem_galago_plugin_parent_class)->dispose (object);
}

static void
totem_galago_plugin_finalize (GObject *object)
{
	if (galago_is_connected ())
		galago_uninit ();

	G_OBJECT_CLASS (totem_galago_plugin_parent_class)->finalize (object);
}

static void
totem_galago_set_idleness (TotemGalagoPlugin *plugin, gboolean idle)
{
	GList *account;
	GalagoPresence *presence;

	if (galago_is_connected () == FALSE)
		return;

	if (plugin->idle == idle)
		return;

	plugin->idle = idle;
	for (account = galago_person_get_accounts (plugin->me, TRUE); account != NULL; account = g_list_next (account)) {
		presence = galago_account_get_presence ((GalagoAccount *)account->data, TRUE);
		if (presence != NULL)
			galago_presence_set_idle (presence, idle, time (NULL));
	}
}

static void
totem_galago_update_from_state (TotemObject *totem,
				TotemGalagoPlugin *plugin)
{
	if (totem_is_playing (totem) != FALSE
	    && totem_is_fullscreen (totem) != FALSE) {
		totem_galago_set_idleness (plugin, TRUE);
	} else {
		totem_galago_set_idleness (plugin, FALSE);
	}
}

static void
property_notify_cb (TotemObject *totem,
		    GParamSpec *spec,
		    TotemGalagoPlugin *plugin)
{
	totem_galago_update_from_state (totem, plugin);
}

static void
impl_activate (PeasActivatable *plugin, GObject *object)
{
	TotemGalagoPlugin *pi = TOTEM_GALAGO_PLUGIN (plugin);
	TotemObject *totem = TOTEM_OBJECT (object);

	if (!galago_is_connected ()) {
		GtkWindow *window = totem_get_main_window (totem);
		totem_interface_error (_("Error loading Galago plugin"), _("Could not connect to the Galago daemon."), window);
		g_object_unref (window);

		return;
	}

	pi->handler_id_fullscreen = g_signal_connect (G_OBJECT (totem),
				"notify::fullscreen",
				G_CALLBACK (property_notify_cb),
				pi);
	pi->handler_id_playing = g_signal_connect (G_OBJECT (totem),
				"notify::playing",
				G_CALLBACK (property_notify_cb),
				pi);

	/* Force setting the current status */
	totem_galago_update_from_state (totem, pi);
}

static void
impl_deactivate (PeasActivatable *plugin, GObject *object)
{
	TotemGalagoPlugin *pi = TOTEM_GALAGO_PLUGIN (plugin);
	TotemObject *totem = TOTEM_OBJECT (object);

	/* Failed to initialise */
	if (!galago_is_connected ())
		return;

	g_signal_handler_disconnect (G_OBJECT (totem), pi->handler_id_fullscreen);
	g_signal_handler_disconnect (G_OBJECT (totem), pi->handler_id_playing);

	totem_galago_set_idleness (pi, FALSE);
}
