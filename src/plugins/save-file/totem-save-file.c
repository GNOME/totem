/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * Copyright (C) Bastien Nocera 2010 <hadess@hadess.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Library General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor Boston, MA 02110-1301,  USA
 */

#include "config.h"

#include <unistd.h>

#include <glib.h>
#include <glib-object.h>
#include <glib/gi18n-lib.h>
#include <glib/gstdio.h>
#include <gmodule.h>
#include <gdk/gdkx.h>
#include <errno.h>
#include <libpeas/peas-extension-base.h>
#include <libpeas/peas-object-module.h>
#include <libpeas/peas-activatable.h>

#include "totem-plugin.h"
#include "totem-interface.h"

#define TOTEM_TYPE_SAVE_FILE_PLUGIN		(totem_save_file_plugin_get_type ())
#define TOTEM_SAVE_FILE_PLUGIN(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), TOTEM_TYPE_SAVE_FILE_PLUGIN, TotemSaveFilePlugin))
#define TOTEM_SAVE_FILE_PLUGIN_CLASS(k)	(G_TYPE_CHECK_CLASS_CAST((k), TOTEM_TYPE_SAVE_FILE_PLUGIN, TotemSaveFilePluginClass))
#define TOTEM_IS_SAVE_FILE_PLUGIN(o)	(G_TYPE_CHECK_INSTANCE_TYPE ((o), TOTEM_TYPE_SAVE_FILE_PLUGIN))
#define TOTEM_IS_SAVE_FILE_PLUGIN_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), TOTEM_TYPE_SAVE_FILE_PLUGIN))
#define TOTEM_SAVE_FILE_PLUGIN_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), TOTEM_TYPE_SAVE_FILE_PLUGIN, TotemSaveFilePluginClass))

typedef struct {
	TotemObject *totem;
	GtkWidget   *bvw;

	char        *mrl;
	char        *name;
	gboolean     is_tmp;

	GSimpleAction *action;
} TotemSaveFilePluginPrivate;

TOTEM_PLUGIN_REGISTER(TOTEM_TYPE_SAVE_FILE_PLUGIN, TotemSaveFilePlugin, totem_save_file_plugin)

static void
copy_uris_with_nautilus (const char *source,
			 const char *dest)
{
	GError *error = NULL;
	GDBusProxyFlags flags;
	GDBusProxy *proxy;
	GFile *dest_file, *parent;
	char *dest_name, *dest_dir;

	g_return_if_fail (source != NULL);
	g_return_if_fail (dest != NULL);

	flags = G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES;
	proxy = g_dbus_proxy_new_for_bus_sync (G_BUS_TYPE_SESSION,
					       flags,
					       NULL, /* GDBusInterfaceInfo */
					       "org.gnome.Nautilus",
					       "/org/gnome/Nautilus",
					       "org.gnome.Nautilus.FileOperations",
					       NULL, /* GCancellable */
					       &error);
	if (proxy == NULL) {
		g_warning ("Could not contact nautilus: %s", error->message);
		g_error_free (error);
		return;
	}

	dest_file = g_file_new_for_uri (dest);
	dest_name = g_file_get_basename (dest_file);
	parent = g_file_get_parent (dest_file);
	g_object_unref (dest_file);
	dest_dir = g_file_get_uri (parent);
	g_object_unref (parent);

	if (g_dbus_proxy_call_sync (proxy,
				"CopyFile", g_variant_new ("(&s&s&s&s)", source, "", dest_dir, dest_name),
				G_DBUS_CALL_FLAGS_NONE,
				-1, NULL, &error) == FALSE) {
		g_warning ("Could not get nautilus to copy file: %s", error->message);
		g_error_free (error);
	}

	g_free (dest_dir);
	g_free (dest_name);
	g_object_unref (proxy);
}

static char *
get_cache_path (void)
{
	char *path;

	path = g_build_filename (g_get_user_cache_dir (), "totem", "media", NULL);
	g_mkdir_with_parents (path, 0755);
	return path;
}

static const char *
get_videos_dir (void)
{
	const char *videos_dir;

	videos_dir = g_get_user_special_dir (G_USER_DIRECTORY_VIDEOS);
	if (!videos_dir)
		videos_dir = g_get_home_dir ();
	return videos_dir;
}

static void
totem_save_file_plugin_copy (GSimpleAction       *action,
			     GVariant            *parameter,
			     TotemSaveFilePlugin *pi)
{
	char *filename;

	g_assert (pi->priv->mrl != NULL);

	if (pi->priv->name != NULL) {
		filename = g_strdup (pi->priv->name);
	} else {
		GFile *file;
		char *basename;

		/* Try to get a nice filename from the URI */
		file = g_file_new_for_uri (pi->priv->mrl);
		basename = g_file_get_basename (file);
		g_object_unref (file);

		if (g_utf8_validate (basename, -1, NULL) == FALSE) {
			g_free (basename);
			filename = NULL;
		} else {
			filename = basename;
		}
	}

	if (filename == NULL) {
		/* translators: Movie is the default saved movie filename,
		 * without the suffix */
		filename = g_strdup (_("Movie"));
	}

	if (pi->priv->is_tmp) {
		char *dest_dir, *dest_name, *dest_path;
		char *src_path;

		dest_dir = get_cache_path ();
		dest_name = g_compute_checksum_for_string (G_CHECKSUM_SHA256, pi->priv->mrl, -1);
		dest_path = g_build_filename (dest_dir, dest_name, NULL);
		g_free (dest_name);
		g_free (dest_dir);

		src_path = g_filename_from_uri (pi->priv->mrl, NULL, NULL);

		if (link (src_path, dest_path) != 0) {
			int err = errno;
			g_warning ("Failed to link '%s' to '%s': %s",
				   src_path, dest_path, g_strerror (err));
		} else {
			GFile *file;

			file = g_file_new_for_path (dest_path);
			totem_object_add_to_view (pi->priv->totem, file, filename);
			g_object_unref (file);
		}

		g_free (src_path);
		g_free (dest_path);
	} else {
		char *dest_path, *dest_uri;

		dest_path = g_build_filename (get_videos_dir (), filename, NULL);
		dest_uri = g_filename_to_uri (dest_path, NULL, NULL);
		g_free (dest_path);

		copy_uris_with_nautilus (pi->priv->mrl, dest_uri);
		g_free (dest_uri);

		/* We don't call Totem to bookmark it, as Tracker should pick it up */
	}

	g_free (filename);
}

static void
totem_save_file_file_closed (TotemObject *totem,
				 TotemSaveFilePlugin *pi)
{
	g_clear_pointer (&pi->priv->mrl, g_free);
	g_clear_pointer (&pi->priv->name, g_free);

	g_simple_action_set_enabled (G_SIMPLE_ACTION (pi->priv->action), FALSE);
}

static gboolean
file_has_ancestor (GFile *file,
		   GFile *ancestor)
{
	GFile *cursor;
	gboolean retval = FALSE;

	if (g_file_has_parent (file, ancestor))
		    return TRUE;

	cursor = g_object_ref (file);

	while (1) {
		GFile *tmp;

		tmp = g_file_get_parent (cursor);
		g_object_unref (cursor);
		cursor = tmp;

		if (cursor == NULL)
			break;

		if (g_file_has_parent (cursor, ancestor)) {
			g_object_unref (cursor);
			retval = TRUE;
			break;
		}
	}

	return retval;
}

static void
totem_save_file_file_opened (TotemObject *totem,
			     const char *mrl,
			     TotemSaveFilePlugin *pi)
{
	GFile *videos_dir;
	GFile *cache_dir = NULL;
	char *cache_path;
	GFile *file;

	g_clear_pointer (&pi->priv->mrl, g_free);
	g_clear_pointer (&pi->priv->name, g_free);

	if (mrl == NULL)
		return;

	/* We can only save files from file:/// and smb:/// URIs (for now) */
	if (!g_str_has_prefix (mrl, "file:") &&
	    !g_str_has_prefix (mrl, "smb:")) {
		g_debug ("Not enabling offline as scheme for '%s' not supported", mrl);
		return;
	}

	file = g_file_new_for_uri (mrl);

	/* We check whether it's in the Videos dir, in the future,
	 * we might want to check if it's native instead */
	videos_dir = g_file_new_for_path (get_videos_dir ());
	if (file_has_ancestor (file, videos_dir)) {
		g_debug ("Not enabling offline save, as '%s' already in '%s'", mrl, get_videos_dir ());
		goto out;
	}

	/* Already cached? */
	cache_path = get_cache_path ();
	cache_dir = g_file_new_for_path (cache_path);
	g_free (cache_path);
	if (g_file_has_parent (file, cache_dir)) {
		g_debug ("Not enabling offline save, as '%s' already cached", mrl);
		goto out;
	}

	g_simple_action_set_enabled (G_SIMPLE_ACTION (pi->priv->action), TRUE);
	pi->priv->mrl = g_strdup (mrl);
	pi->priv->name = totem_object_get_short_title (pi->priv->totem);
	pi->priv->is_tmp = FALSE;

out:
	g_clear_object (&cache_dir);
	g_clear_object (&videos_dir);
	g_clear_object (&file);
}

static void
totem_save_file_download_filename (GObject    *gobject,
				   GParamSpec *pspec,
				   TotemSaveFilePlugin *pi)
{
	char *filename;

	/* We're already ready to copy it */
	if (pi->priv->mrl != NULL)
		return;

	filename = NULL;
	g_object_get (G_OBJECT (pi->priv->bvw), "download-filename", &filename, NULL);
	if (filename == NULL)
		return;

	pi->priv->mrl = g_filename_to_uri (filename, NULL, NULL);
	g_free (filename);
	pi->priv->name = totem_object_get_short_title (pi->priv->totem);
	pi->priv->is_tmp = TRUE;

	g_simple_action_set_enabled (G_SIMPLE_ACTION (pi->priv->action), TRUE);
}

static void
impl_activate (PeasActivatable *plugin)
{
	TotemSaveFilePlugin *pi = TOTEM_SAVE_FILE_PLUGIN (plugin);
	TotemSaveFilePluginPrivate *priv = pi->priv;
	GMenu *menu;
	GMenuItem *item;
	char *path;
	char *mrl;
	const char const *accels[] = { "<Primary>S", "Save", NULL };

	/* make sure nautilus is in the path */
	path = g_find_program_in_path ("nautilus");
	if (!path)
		return;
	g_free (path);

	priv->totem = g_object_get_data (G_OBJECT (plugin), "object");
	priv->bvw = totem_object_get_video_widget (priv->totem);

	g_signal_connect (priv->totem,
			  "file-opened",
			  G_CALLBACK (totem_save_file_file_opened),
			  plugin);
	g_signal_connect (priv->totem,
			  "file-closed",
			  G_CALLBACK (totem_save_file_file_closed),
			  plugin);
	g_signal_connect (priv->bvw,
			  "notify::download-filename",
			  G_CALLBACK (totem_save_file_download_filename),
			  plugin);

	priv->action = g_simple_action_new ("save-as", NULL);
	g_signal_connect (G_OBJECT (priv->action), "activate",
			  G_CALLBACK (totem_save_file_plugin_copy), plugin);
	g_action_map_add_action (G_ACTION_MAP (priv->totem), G_ACTION (priv->action));
	gtk_application_set_accels_for_action (GTK_APPLICATION (priv->totem),
					       "app.save-as",
					       accels);
	g_simple_action_set_enabled (G_SIMPLE_ACTION (priv->action), FALSE);

	/* add UI */
	menu = totem_object_get_menu_section (priv->totem, "save-placeholder");
	item = g_menu_item_new (_("Make Available Offline"), "app.save-as");
	g_menu_item_set_attribute (item, "accel", "s", "<Primary>s");
	g_menu_append_item (G_MENU (menu), item);

	mrl = totem_object_get_current_mrl (priv->totem);
	totem_save_file_file_opened (priv->totem, mrl, pi);
	totem_save_file_download_filename (NULL, NULL, pi);
	g_free (mrl);
}

static void
impl_deactivate (PeasActivatable *plugin)
{
	TotemSaveFilePlugin *pi = TOTEM_SAVE_FILE_PLUGIN (plugin);
	TotemSaveFilePluginPrivate *priv = pi->priv;

	g_signal_handlers_disconnect_by_func (priv->totem, totem_save_file_file_opened, plugin);
	g_signal_handlers_disconnect_by_func (priv->totem, totem_save_file_file_closed, plugin);
	g_signal_handlers_disconnect_by_func (priv->bvw, totem_save_file_download_filename, plugin);

	totem_object_empty_menu_section (priv->totem, "save-placeholder");

	priv->totem = NULL;
	priv->bvw = NULL;

	g_clear_pointer (&pi->priv->mrl, g_free);
	g_clear_pointer (&pi->priv->name, g_free);
}
