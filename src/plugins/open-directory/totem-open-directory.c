/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * Copyright (C) Matthieu Gautier 2015 <dev@mgautier.fr>
 *
 * SPDX-License-Identifier: GPL-3-or-later
 *
 */

#include "config.h"

#include <glib/gi18n-lib.h>
#include <libpeas.h>
#include "totem-plugin-activatable.h"
#include <libportal-gtk3/portal-gtk3.h>

#include "totem-plugin.h"

#define TOTEM_TYPE_OPEN_DIRECTORY_PLUGIN		(totem_open_directory_plugin_get_type ())
#define TOTEM_OPEN_DIRECTORY_PLUGIN(o)			(G_TYPE_CHECK_INSTANCE_CAST ((o), TOTEM_TYPE_OPEN_DIRECTORY_PLUGIN, TotemOpenDirectoryPlugin))

typedef struct {
	PeasExtensionBase parent;

	TotemObject   *totem;
	XdpPortal     *portal;
	GCancellable  *cancellable;
	char          *mrl;
	GSimpleAction *action;
} TotemOpenDirectoryPlugin;

TOTEM_PLUGIN_REGISTER(TOTEM_TYPE_OPEN_DIRECTORY_PLUGIN, TotemOpenDirectoryPlugin, totem_open_directory_plugin)

static void
open_directory_cb (GObject *object,
		   GAsyncResult *result,
		   gpointer data)
{
	XdpPortal *portal = XDP_PORTAL (object);
	g_autoptr(GError) error = NULL;
	gboolean res;

	res = xdp_portal_open_directory_finish (portal, result, &error);
	if (!res) {
		if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
			g_warning ("Failed to show directory: %s", error->message);
	}
}

static void
totem_open_directory_plugin_open (GSimpleAction       *action,
				  GVariant            *parameter,
				  TotemOpenDirectoryPlugin *pi)
{
	XdpParent *parent;

	g_assert (pi->mrl != NULL);

	parent = xdp_parent_new_gtk (totem_object_get_main_window (pi->totem));
	xdp_portal_open_directory (pi->portal, parent, pi->mrl,
				   XDP_OPEN_URI_FLAG_NONE, pi->cancellable,
				   open_directory_cb, NULL);
	xdp_parent_free (parent);
}

static void
totem_open_directory_file_closed (TotemObject *totem,
				 TotemOpenDirectoryPlugin *pi)
{
	g_clear_pointer (&pi->mrl, g_free);

	g_simple_action_set_enabled (G_SIMPLE_ACTION (pi->action), FALSE);
}

static gboolean
scheme_is_supported (const char *scheme)
{
	const gchar * const *schemes;
	guint i;

	if (!scheme)
		return FALSE;

	if (g_str_equal (scheme, "http") ||
	    g_str_equal (scheme, "https"))
		return FALSE;

	schemes = g_vfs_get_supported_uri_schemes (g_vfs_get_default ());
	for (i = 0; schemes[i] != NULL; i++) {
		if (g_str_equal (schemes[i], scheme))
			return TRUE;
	}
	return FALSE;
}

static void
totem_open_directory_file_opened (TotemObject              *totem,
				  const char               *mrl,
				  TotemOpenDirectoryPlugin *pi)
{
	char *scheme;

	g_clear_pointer (&pi->mrl, g_free);

	if (mrl == NULL)
		return;

	scheme = g_uri_parse_scheme (mrl);
	if (!scheme_is_supported (scheme)) {
		g_debug ("Not enabling open-directory as scheme for '%s' not supported", mrl);
		g_free (scheme);
		return;
	}
	g_free (scheme);

	g_simple_action_set_enabled (G_SIMPLE_ACTION (pi->action), TRUE);
	pi->mrl = g_strdup (mrl);
}

static void
impl_activate (TotemPluginActivatable *plugin)
{
	TotemOpenDirectoryPlugin *pi = TOTEM_OPEN_DIRECTORY_PLUGIN (plugin);
	GMenu *menu;
	GMenuItem *item;
	char *mrl;
	g_autoptr(GError) error = NULL;

	pi->totem = g_object_get_data (G_OBJECT (plugin), "object");
	pi->portal = xdp_portal_initable_new (&error);
	pi->cancellable = g_cancellable_new ();

	if (error) {
		g_warning ("Failed to create XdpPortal instance: %s", error->message);
		return;
	}

	g_signal_connect (pi->totem,
			  "file-opened",
			  G_CALLBACK (totem_open_directory_file_opened),
			  plugin);
	g_signal_connect (pi->totem,
			  "file-closed",
			  G_CALLBACK (totem_open_directory_file_closed),
			  plugin);

	pi->action = g_simple_action_new ("open-dir", NULL);
	g_signal_connect (G_OBJECT (pi->action), "activate",
			  G_CALLBACK (totem_open_directory_plugin_open), plugin);
	g_action_map_add_action (G_ACTION_MAP (pi->totem), G_ACTION (pi->action));
	g_simple_action_set_enabled (G_SIMPLE_ACTION (pi->action), FALSE);

	/* add UI */
	menu = totem_object_get_menu_section (pi->totem, "opendirectory-placeholder");
	item = g_menu_item_new (_("Open Containing Folder"), "app.open-dir");
	g_menu_append_item (G_MENU (menu), item);

	mrl = totem_object_get_current_mrl (pi->totem);
	totem_open_directory_file_opened (pi->totem, mrl, pi);
	g_free (mrl);
}

static void
impl_deactivate (TotemPluginActivatable *plugin)
{
	TotemOpenDirectoryPlugin *pi = TOTEM_OPEN_DIRECTORY_PLUGIN (plugin);

	if (pi->cancellable != NULL) {
		g_cancellable_cancel (pi->cancellable);
		g_clear_object (&pi->cancellable);
	}

	g_signal_handlers_disconnect_by_func (pi->totem, totem_open_directory_file_opened, plugin);
	g_signal_handlers_disconnect_by_func (pi->totem, totem_open_directory_file_closed, plugin);

	totem_object_empty_menu_section (pi->totem, "opendirectory-placeholder");

	pi->totem = NULL;

	g_clear_pointer (&pi->mrl, g_free);
}
