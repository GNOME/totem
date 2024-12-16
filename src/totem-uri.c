/* totem-uri.c

   Copyright (C) 2004 Bastien Nocera

   SPDX-License-Identifier: GPL-3-or-later

   Author: Bastien Nocera <hadess@hadess.net>
 */

#include "config.h"

#include <glib.h>
#include <glib/gi18n.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <gio/gio.h>

#define WANT_MIME_TYPES 1
#define WANT_AUDIO_MIME_TYPES 1
#define WANT_VIDEO_MIME_TYPES 1
#include "totem-mime-types.h"
#include "totem-uri.h"
#include "totem-private.h"

static GtkFileFilter *filter_all = NULL;
static GtkFileFilter *filter_subs = NULL;
static GtkFileFilter *filter_video = NULL;

gboolean
totem_playing_dvd (const char *uri)
{
	if (uri == NULL)
		return FALSE;

	return g_str_has_prefix (uri, "dvd:/");
}

static void
totem_ensure_dir (const char *path)
{
	if (g_file_test (path, G_FILE_TEST_IS_DIR) != FALSE)
		return;

	g_mkdir_with_parents (path, 0700);
}

const char *
totem_dot_dir (void)
{
	static char *totem_dir = NULL;

	if (totem_dir != NULL) {
		totem_ensure_dir (totem_dir);
		return totem_dir;
	}

	totem_dir = g_build_filename (g_get_user_config_dir (),
				      "totem",
				      NULL);

	totem_ensure_dir (totem_dir);

	return (const char *)totem_dir;
}

const char *
totem_data_dot_dir (void)
{
	static char *totem_dir = NULL;

	if (totem_dir != NULL) {
		totem_ensure_dir (totem_dir);
		return totem_dir;
	}

	totem_dir = g_build_filename (g_get_user_data_dir (),
				      "totem",
				      NULL);

	totem_ensure_dir (totem_dir);

	return (const char *)totem_dir;
}

static GMount *
totem_get_mount_for_uri (const char *path)
{
	GMount *mount;
	GFile *file;

	file = g_file_new_for_path (path);
	mount = g_file_find_enclosing_mount (file, NULL, NULL);
	g_object_unref (file);

	if (mount == NULL)
		return NULL;

	/* FIXME: We used to explicitly check whether it was a CD/DVD */
	if (g_mount_can_eject (mount) == FALSE) {
		g_object_unref (mount);
		return NULL;
	}

	return mount;
}

static GMount *
totem_get_mount_for_dvd (const char *uri)
{
	GMount *mount;
	char *path;

	mount = NULL;
	path = g_strdup (uri + strlen ("dvd://"));

	/* If it's a device, we need to find the volume that corresponds to it,
	 * and then the mount for the volume */
	if (g_str_has_prefix (path, "/dev/")) {
		GVolumeMonitor *volume_monitor;
		GList *volumes, *l;

		volume_monitor = g_volume_monitor_get ();
		volumes = g_volume_monitor_get_volumes (volume_monitor);
		g_object_unref (volume_monitor);

		for (l = volumes; l != NULL; l = l->next) {
			char *id;

			id = g_volume_get_identifier (l->data, G_VOLUME_IDENTIFIER_KIND_UNIX_DEVICE);
			if (g_strcmp0 (id, path) == 0) {
				g_free (id);
				mount = g_volume_get_mount (l->data);
				break;
			}
			g_free (id);
		}
		g_list_free_full (volumes, (GDestroyNotify) g_object_unref);
	} else {
		mount = totem_get_mount_for_uri (path);
		g_free (path);
	}
	/* We have a path to the file itself */
	return mount;
}

static char *
totem_get_mountpoint_for_vcd (const char *uri)
{
	return NULL;
}

GMount *
totem_get_mount_for_media (const char *uri)
{
	GMount *ret;
	char *mount_path;

	if (uri == NULL)
		return NULL;

	mount_path = NULL;

	if (g_str_has_prefix (uri, "dvd://") != FALSE)
		return totem_get_mount_for_dvd (uri);
	else if (g_str_has_prefix (uri, "vcd:") != FALSE)
		mount_path = totem_get_mountpoint_for_vcd (uri);
	else if (g_str_has_prefix (uri, "file:") != FALSE)
		mount_path = g_filename_from_uri (uri, NULL, NULL);

	if (mount_path == NULL)
		return NULL;

	ret = totem_get_mount_for_uri (mount_path);
	g_free (mount_path);

	return ret;
}

gboolean
totem_is_special_mrl (const char *uri)
{
	GMount *mount;

	if (uri == NULL || g_str_has_prefix (uri, "file:") != FALSE)
		return FALSE;
	if (g_str_has_prefix (uri, "dvb:") != FALSE)
		return TRUE;

	mount = totem_get_mount_for_media (uri);
	if (mount != NULL)
		g_object_unref (mount);

	return (mount != NULL);
}

gboolean
totem_is_block_device (const char *uri)
{
	struct stat buf;
	char *local;

	if (uri == NULL)
		return FALSE;

	if (g_str_has_prefix (uri, "file:") == FALSE)
		return FALSE;
	local = g_filename_from_uri (uri, NULL, NULL);
	if (local == NULL)
		return FALSE;
	if (stat (local, &buf) != 0) {
		g_free (local);
		return FALSE;
	}
	g_free (local);

	return (S_ISBLK (buf.st_mode));
}

char *
totem_create_full_path (const char *path)
{
	GFile *file;
	char *retval;

	g_return_val_if_fail (path != NULL, NULL);

	if (strstr (path, "://") != NULL)
		return NULL;
	if (totem_is_special_mrl (path) != FALSE)
		return NULL;

	file = g_file_new_for_commandline_arg (path);
	retval = g_file_get_uri (file);
	g_object_unref (file);

	return retval;
}

static void
totem_object_on_unmount (GVolumeMonitor *volume_monitor,
			 GMount *mount,
			 Totem *totem)
{
	totem_playlist_clear_with_g_mount (totem->playlist, mount);
}

void
totem_setup_file_monitoring (Totem *totem)
{
	totem->monitor = g_volume_monitor_get ();

	g_signal_connect (G_OBJECT (totem->monitor),
			  "mount-pre-unmount",
			  G_CALLBACK (totem_object_on_unmount),
			  totem);
	g_signal_connect (G_OBJECT (totem->monitor),
			  "mount-removed",
			  G_CALLBACK (totem_object_on_unmount),
			  totem);
}

/* List from xine-lib's demux_sputext.c.
 * Keep in sync with the list in totem_setup_file_filters() in this file.
 * Don't add .txt extensions, as there are too many false positives. */
static const char subtitle_ext[][4] = {
	"sub",
	"srt",
	"vtt",
	"smi",
	"ssa",
	"ass",
	"mpl",
	"asc"
};

gboolean
totem_uri_is_subtitle (const char *uri)
{
	guint len, i;

	len = strlen (uri);
	if (len < 4 || uri[len - 4] != '.')
		return FALSE;
	for (i = 0; i < G_N_ELEMENTS (subtitle_ext); i++) {
		if (g_str_has_suffix (uri, subtitle_ext[i]) != FALSE)
			return TRUE;
	}
	return FALSE;
}

char *
totem_uri_escape_for_display (const char *uri)
{
	GFile *file;
	char *disp;

	file = g_file_new_for_uri (uri);
	disp = g_file_get_parse_name (file);
	g_object_unref (file);

	return disp;
}

void
totem_setup_file_filters (void)
{
	guint i;

	filter_all = gtk_file_filter_new ();
	gtk_file_filter_set_name (filter_all, _("All files"));
	gtk_file_filter_add_pattern (filter_all, "*");
	g_object_ref_sink (filter_all);

	/* Video files */
	filter_video = gtk_file_filter_new ();
	gtk_file_filter_set_name (filter_video, _("Video files"));
	for (i = 0; video_mime_types[i] != NULL; i++) {
		gtk_file_filter_add_mime_type (filter_video, video_mime_types[i]);
	}
	/* Add the special Disc-as-files formats */
	gtk_file_filter_add_mime_type (filter_video, "application/x-cd-image");
	gtk_file_filter_add_mime_type (filter_video, "application/x-cue");
	g_object_ref_sink (filter_video);

	/* Subtitles files. Keep in sync with subtitle_ext in this file. */
	filter_subs = gtk_file_filter_new ();
	gtk_file_filter_set_name (filter_subs, _("Subtitle files"));
	gtk_file_filter_add_mime_type (filter_subs, "application/x-subrip"); /* *.srt */
	gtk_file_filter_add_mime_type (filter_subs, "text/plain"); /* *.asc, *.txt */
	gtk_file_filter_add_mime_type (filter_subs, "text/x-mpl2"); /* *.mpl */
	gtk_file_filter_add_mime_type (filter_subs, "text/vtt"); /* *.vtt */
	gtk_file_filter_add_mime_type (filter_subs, "application/x-sami"); /* *.smi, *.sami */
	gtk_file_filter_add_mime_type (filter_subs, "text/x-microdvd"); /* *.sub */
	gtk_file_filter_add_mime_type (filter_subs, "text/x-mpsub"); /* *.sub */
	gtk_file_filter_add_mime_type (filter_subs, "text/x-ssa"); /* *.ssa, *.ass */
	gtk_file_filter_add_mime_type (filter_subs, "text/x-subviewer"); /* *.sub */
	g_object_ref_sink (filter_subs);
}

void
totem_destroy_file_filters (void)
{
	if (filter_all != NULL) {
		g_object_unref (filter_all);
		filter_all = NULL;
		g_object_unref (filter_video);
		g_object_unref (filter_subs);
	}
}

static const GUserDirectory dir_types[] = {
	G_USER_DIRECTORY_VIDEOS,
	G_USER_DIRECTORY_MUSIC
};

static void
totem_add_default_dirs (GtkFileChooser *dialog)
{
	guint i;
	for (i = 0; i < G_N_ELEMENTS (dir_types); i++) {
		const char *dir;

		dir = g_get_user_special_dir (dir_types[i]);
		if (dir == NULL)
			continue;
		gtk_file_chooser_add_shortcut_folder (dialog, dir, NULL);
	}
}

GObject *
totem_add_subtitle (GtkWindow *parent, const char *uri)
{
	GtkFileChooserNative *fs;
	g_autoptr(GSettings) settings = NULL;
	char *new_path;
	gboolean folder_set;

	fs = gtk_file_chooser_native_new (_("Select Text Subtitles"),
					  parent,
					  GTK_FILE_CHOOSER_ACTION_OPEN,
					  _("_Open"), _("_Cancel"));
	gtk_file_chooser_set_local_only (GTK_FILE_CHOOSER (fs), FALSE);
	gtk_file_chooser_set_filter (GTK_FILE_CHOOSER (fs), filter_subs);

	settings = g_settings_new (TOTEM_GSETTINGS_SCHEMA);
	folder_set = FALSE;

	/* Add the subtitles cache dir as a shortcut */
	new_path = g_build_filename (g_get_user_cache_dir (),
				     "totem",
				     "subtitles",
				     NULL);
	gtk_file_chooser_add_shortcut_folder_uri (GTK_FILE_CHOOSER (fs), new_path, NULL);
	g_free (new_path);

	/* Add the last open path as a shortcut */
	new_path = g_settings_get_string (settings, "open-uri");
	if (*new_path != '\0')
		gtk_file_chooser_add_shortcut_folder_uri (GTK_FILE_CHOOSER (fs), new_path, NULL);
	g_free (new_path);

	/* Try to set the passed path as the current folder */
	if (uri != NULL) {
		folder_set = gtk_file_chooser_set_current_folder_uri
			(GTK_FILE_CHOOSER (fs), uri);
		gtk_file_chooser_add_shortcut_folder_uri (GTK_FILE_CHOOSER (fs), uri, NULL);
	}
	
	/* And set it as home if it fails */
	if (folder_set == FALSE) {
		gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (fs),
						     g_get_home_dir ());
	}
	totem_add_default_dirs (GTK_FILE_CHOOSER (fs));

	return G_OBJECT(fs);
}

static void
update_open_uri_settings_cb (GObject   *dialog,
                             int        response_id,
                             gpointer   user_data)
{
	g_autofree char *filename = NULL;
	GSettings *settings = user_data;

	if (response_id == GTK_RESPONSE_ACCEPT)
		filename = gtk_file_chooser_get_uri (GTK_FILE_CHOOSER (dialog));

	if (filename != NULL) {
		g_autofree char *new_path = g_path_get_dirname (filename);
		g_settings_set_string (settings, "open-uri", new_path);
	}

	g_clear_object (&settings);
}

GObject *
totem_add_files (GtkWindow *parent, const char *path)
{
	GtkFileChooserNative *fs;
	char *new_path;
	GSettings *settings;
	gboolean set_folder;

	fs = gtk_file_chooser_native_new (_("Add Videos"),
					  parent,
					  GTK_FILE_CHOOSER_ACTION_OPEN,
					  _("_Add"), _("_Cancel"));
	gtk_file_chooser_set_filter (GTK_FILE_CHOOSER (fs), filter_video);
	gtk_file_chooser_set_select_multiple (GTK_FILE_CHOOSER (fs), TRUE);
	gtk_file_chooser_set_local_only (GTK_FILE_CHOOSER (fs), FALSE);

	settings = g_settings_new (TOTEM_GSETTINGS_SCHEMA);
	set_folder = TRUE;
	if (path != NULL) {
		set_folder = gtk_file_chooser_set_current_folder_uri
			(GTK_FILE_CHOOSER (fs), path);
	} else {
		new_path = g_settings_get_string (settings, "open-uri");
		if (*new_path != '\0') {
			set_folder = gtk_file_chooser_set_current_folder_uri
				(GTK_FILE_CHOOSER (fs), new_path);
		}
		g_free (new_path);
	}

	/* We didn't manage to change the directory */
	if (set_folder == FALSE) {
		gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (fs),
						     g_get_home_dir ());
	}
	totem_add_default_dirs (GTK_FILE_CHOOSER (fs));

	g_signal_connect (fs, "response", G_CALLBACK (update_open_uri_settings_cb), settings);

	return G_OBJECT (fs);
}
