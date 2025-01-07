/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * Copyright (C) Bastien Nocera 2010 <hadess@hadess.net>
 *
 * SPDX-License-Identifier: GPL-3-or-later
 *
 */

#include "config.h"

#include <unistd.h>

#include <glib.h>
#include <glib-object.h>
#include <glib/gi18n-lib.h>
#include <glib/gstdio.h>
#include <gmodule.h>
#include <errno.h>
#include <libpeas.h>
#include "totem-plugin-activatable.h"

#include "totem-plugin.h"
#include "totem-interface.h"

#define TOTEM_TYPE_SAVE_FILE_PLUGIN		(totem_save_file_plugin_get_type ())
#define TOTEM_SAVE_FILE_PLUGIN(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), TOTEM_TYPE_SAVE_FILE_PLUGIN, TotemSaveFilePlugin))

typedef struct {
	PeasExtensionBase parent;

	TotemObject *totem;
	GtkWidget   *bvw;

	char        *mrl;
	char        *cache_mrl;
	char        *name;
	gboolean     is_tmp;

	GCancellable *cancellable;
	gboolean      is_flatpaked;

	GSimpleAction *action;
} TotemSaveFilePlugin;

TOTEM_PLUGIN_REGISTER(TOTEM_TYPE_SAVE_FILE_PLUGIN, TotemSaveFilePlugin, totem_save_file_plugin)

static void
copy_uris_with_nautilus (TotemSaveFilePlugin *pi,
			 const char          *source,
			 const char          *dest_dir,
			 const char          *dest_name)
{
	GError *error = NULL;
	GDBusProxyFlags flags;
	GDBusProxy *proxy;
	GVariant *ret;

	g_return_if_fail (source != NULL);
	g_return_if_fail (dest_dir != NULL);
	g_return_if_fail (dest_name != NULL);

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
		GtkWindow *main_window;

		main_window = totem_object_get_main_window (pi->totem);
		totem_interface_error (_("The video could not be made available offline."),
				       /* translators: “Files” refers to nautilus' name */
				       _("“Files” is not available."), main_window);
		g_object_unref (main_window);

		g_debug ("Could not contact nautilus: %s", error->message);
		g_error_free (error);
		return;
	}

	ret = g_dbus_proxy_call_sync (proxy,
				      "CopyFile", g_variant_new ("(&s&s&s&s)", source, "", dest_dir, dest_name),
				      G_DBUS_CALL_FLAGS_NONE,
				      -1, NULL, &error);

	if (ret == NULL) {
		/* nautilus >= 3.30.0? */
		if (g_error_matches (error, G_DBUS_ERROR, G_DBUS_ERROR_UNKNOWN_METHOD)) {
			const char *sources[2] = { source, NULL };

			g_clear_error (&error);
			ret = g_dbus_proxy_call_sync (proxy,
						      "CopyURIs", g_variant_new ("(^ass)", sources, dest_dir),
						      G_DBUS_CALL_FLAGS_NONE,
						      -1, NULL, &error);
		}
	}

	if (ret == NULL) {
		g_warning ("Could not get nautilus to copy file: %s", error->message);
		g_error_free (error);
	} else {
		g_variant_unref (ret);
	}

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

static char *
get_videos_dir_uri (void)
{
	const char *videos_dir;

	videos_dir = g_get_user_special_dir (G_USER_DIRECTORY_VIDEOS);
	if (!videos_dir)
		videos_dir = g_get_home_dir ();
	return g_filename_to_uri (videos_dir, NULL, NULL);
}

static char *
totem_save_file_get_filename (TotemSaveFilePlugin *pi)
{
	char *filename, *basename;
	GFile *file;

	if (pi->name != NULL)
		return g_strdup (pi->name);

	/* Try to get a nice filename from the URI */
	file = g_file_new_for_uri (pi->mrl);
	basename = g_file_get_basename (file);
	g_object_unref (file);

	if (g_utf8_validate (basename, -1, NULL) == FALSE) {
		g_free (basename);
		filename = NULL;
	} else {
		filename = basename;
	}

	if (filename == NULL) {
		/* translators: Movie is the default saved movie filename,
		 * without the suffix */
		filename = g_strdup (_("Movie"));
	}

	return filename;
}

static char *
checksum_path_for_mrl (const char *mrl)
{
	char *dest_dir, *dest_name, *dest_path;

	dest_dir = get_cache_path ();
	dest_name = g_compute_checksum_for_string (G_CHECKSUM_SHA256, mrl, -1);
	dest_path = g_build_filename (dest_dir, dest_name, NULL);
	g_free (dest_name);
	g_free (dest_dir);

	return dest_path;
}

static void
totem_save_file_plugin_copy (GSimpleAction       *action,
			     GVariant            *parameter,
			     TotemSaveFilePlugin *pi)
{
	char *filename;

	g_assert (pi->mrl != NULL);

	filename = totem_save_file_get_filename (pi);

	if (pi->is_tmp) {
		char *src_path, *dest_path;

		src_path = g_filename_from_uri (pi->cache_mrl, NULL, NULL);
		dest_path = checksum_path_for_mrl (pi->mrl);

		if (link (src_path, dest_path) != 0) {
			int err = errno;
			g_warning ("Failed to link '%s' to '%s': %s",
				   src_path, dest_path, g_strerror (err));
		} else {
			GFile *file;

			g_debug ("Successfully linked '%s' to '%s'",
				 src_path, dest_path);

			file = g_file_new_for_path (dest_path);
			totem_object_add_to_view (pi->totem, file, filename);
			g_object_unref (file);
		}

		g_free (src_path);
		g_free (dest_path);
	} else {
		copy_uris_with_nautilus (pi, pi->mrl, get_videos_dir_uri(), filename);
		/* We don't call Totem to bookmark it, as Tracker should pick it up */
	}

	g_free (filename);
}

static void
totem_save_file_file_closed (TotemObject *totem,
				 TotemSaveFilePlugin *pi)
{
	g_clear_pointer (&pi->mrl, g_free);
	g_clear_pointer (&pi->cache_mrl, g_free);
	g_clear_pointer (&pi->name, g_free);

	g_simple_action_set_enabled (G_SIMPLE_ACTION (pi->action), FALSE);
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
	GFile *cache_dir = NULL;
	char *cache_path, *videos_dir;
	GFile *file;

	g_clear_pointer (&pi->mrl, g_free);
	g_clear_pointer (&pi->name, g_free);

	if (mrl == NULL)
		return;

	pi->mrl = g_strdup (mrl);

	/* We can only save files from file:/// and smb:/// URIs (for now) */
	if (!g_str_has_prefix (mrl, "file:") &&
	    !g_str_has_prefix (mrl, "smb:")) {
		g_debug ("Not enabling offline as scheme for '%s' not supported", mrl);
		return;
	}

	file = g_file_new_for_uri (mrl);

	if (pi->is_flatpaked) {
		GFile *videos_dir_file;

		/* Check whether it's in the Videos dir */
		videos_dir = get_videos_dir_uri ();
		videos_dir_file = g_file_new_for_path (videos_dir);
		if (file_has_ancestor (file, videos_dir_file)) {
			g_debug ("Not enabling offline save, as '%s' already in '%s'", mrl, videos_dir);
			g_object_unref (videos_dir_file);
			g_free (videos_dir);
			goto out;
		}
		g_object_unref (videos_dir_file);
		g_free (videos_dir);
	} else {
		char *path;

		path = g_file_get_path (file);
		/* Check, crudely, whether it's in $HOME */
		if (g_str_has_prefix (path, g_get_home_dir ()) &&
		    g_file_is_native (file)) {
			g_debug ("Not enabling offline save, as '%s' already in $HOME, and native", mrl);
			g_free (path);
			goto out;
		}
		g_free (path);
	}

	/* Already cached? */
	cache_path = get_cache_path ();
	cache_dir = g_file_new_for_path (cache_path);
	g_free (cache_path);
	if (g_file_has_parent (file, cache_dir)) {
		g_debug ("Not enabling offline save, as '%s' already cached", mrl);
		goto out;
	}

	g_simple_action_set_enabled (G_SIMPLE_ACTION (pi->action), TRUE);
	pi->name = totem_object_get_short_title (pi->totem);
	pi->is_tmp = FALSE;

out:
	g_clear_object (&cache_dir);
	g_clear_object (&file);
}

static void
cache_file_exists_cb (GObject      *source_object,
		      GAsyncResult *res,
		      gpointer      user_data)
{
	TotemSaveFilePlugin *pi;
	GFileInfo *info;
	GError *error = NULL;
	char *path;

	path = g_file_get_path (G_FILE (source_object));
	info = g_file_query_info_finish (G_FILE (source_object), res, &error);
	if (!info) {
		if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
			pi = user_data;
			g_simple_action_set_enabled (G_SIMPLE_ACTION (pi->action), TRUE);

			g_debug ("Enabling offline save, as '%s' for '%s'",
				 path, pi->mrl);
		}
		g_free (path);
		g_error_free (error);
		return;
	}
	g_object_unref (info);

	pi = user_data;
	g_debug ("Not enabling offline save, as '%s' already exists for '%s'",
		 path, pi->mrl);
	g_free (path);
}

static void
totem_save_file_download_filename (GObject    *gobject,
				   GParamSpec *pspec,
				   TotemSaveFilePlugin *pi)
{
	char *filename, *cache_path;
	GFile *file;

	/* We're already ready to copy it */
	if (pi->cache_mrl != NULL)
		return;

	filename = NULL;
	g_object_get (G_OBJECT (pi->bvw), "download-filename", &filename, NULL);
	if (filename == NULL)
		return;

	g_debug ("download-filename changed to '%s'", filename);

	pi->cache_mrl = g_filename_to_uri (filename, NULL, NULL);
	g_free (filename);
	pi->name = totem_object_get_short_title (pi->totem);
	pi->is_tmp = TRUE;

	g_debug ("Cache MRL: '%s', name: '%s'", pi->cache_mrl, pi->name);

	cache_path = checksum_path_for_mrl (pi->mrl);
	file = g_file_new_for_path (cache_path);
	g_free (cache_path);

	g_file_query_info_async (file, G_FILE_ATTRIBUTE_STANDARD_TYPE,
				 G_FILE_QUERY_INFO_NONE, 0, pi->cancellable,
				 cache_file_exists_cb, pi);
	g_object_unref (file);
}

static void
impl_activate (TotemPluginActivatable *plugin)
{
	TotemSaveFilePlugin *pi = TOTEM_SAVE_FILE_PLUGIN (plugin);
	GMenu *menu;
	GMenuItem *item;
	char *mrl;
	const char * const accels[] = { "<Primary>S", "Save", NULL };

	pi->totem = g_object_get_data (G_OBJECT (plugin), "object");
	pi->bvw = totem_object_get_video_widget (pi->totem);
	pi->cancellable = g_cancellable_new ();
	pi->is_flatpaked = g_file_test ("/.flatpak-info", G_FILE_TEST_EXISTS);

	g_signal_connect (pi->totem,
			  "file-opened",
			  G_CALLBACK (totem_save_file_file_opened),
			  plugin);
	g_signal_connect (pi->totem,
			  "file-closed",
			  G_CALLBACK (totem_save_file_file_closed),
			  plugin);
	g_signal_connect (pi->bvw,
			  "notify::download-filename",
			  G_CALLBACK (totem_save_file_download_filename),
			  plugin);

	pi->action = g_simple_action_new ("save-as", NULL);
	g_signal_connect (G_OBJECT (pi->action), "activate",
			  G_CALLBACK (totem_save_file_plugin_copy), plugin);
	g_action_map_add_action (G_ACTION_MAP (pi->totem), G_ACTION (pi->action));
	gtk_application_set_accels_for_action (GTK_APPLICATION (pi->totem),
					       "app.save-as",
					       accels);
	g_simple_action_set_enabled (G_SIMPLE_ACTION (pi->action), FALSE);

	/* add UI */
	menu = totem_object_get_menu_section (pi->totem, "save-placeholder");
	item = g_menu_item_new (_("Make Available Offline"), "app.save-as");
	g_menu_item_set_attribute (item, "accel", "s", "<Primary>s");
	g_menu_append_item (G_MENU (menu), item);

	mrl = totem_object_get_current_mrl (pi->totem);
	totem_save_file_file_opened (pi->totem, mrl, pi);
	totem_save_file_download_filename (NULL, NULL, pi);
	g_free (mrl);
}

static void
impl_deactivate (TotemPluginActivatable *plugin)
{
	TotemSaveFilePlugin *pi = TOTEM_SAVE_FILE_PLUGIN (plugin);

	g_signal_handlers_disconnect_by_func (pi->totem, totem_save_file_file_opened, plugin);
	g_signal_handlers_disconnect_by_func (pi->totem, totem_save_file_file_closed, plugin);
	g_signal_handlers_disconnect_by_func (pi->bvw, totem_save_file_download_filename, plugin);

	totem_object_empty_menu_section (pi->totem, "save-placeholder");

	if (pi->cancellable) {
		g_cancellable_cancel (pi->cancellable);
		g_clear_object (&pi->cancellable);
	}

	pi->totem = NULL;
	g_clear_object (&pi->bvw);

	g_clear_pointer (&pi->mrl, g_free);
	g_clear_pointer (&pi->cache_mrl, g_free);
	g_clear_pointer (&pi->name, g_free);
}
