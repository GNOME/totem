/* totem-uri.c

   Copyright (C) 2004 Bastien Nocera

   The Gnome Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   The Gnome Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with the Gnome Library; see the file COPYING.LIB.  If not,
   write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.

   Author: Bastien Nocera <hadess@hadess.net>
 */

#include "config.h"

#include <glib.h>
#include <glib/gi18n.h>
#include <string.h>
#include <libgnomevfs/gnome-vfs-utils.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "totem-mime-types.h"
#include "totem-uri.h"
#include "totem-private.h"

static GtkFileFilter *filter_all = NULL;
static GtkFileFilter *filter_supported = NULL;
static GtkFileFilter *filter_audio = NULL;
static GtkFileFilter *filter_video = NULL;

gboolean
totem_playing_dvd (const char *uri)
{
	if (uri == NULL)
		return FALSE;

	return g_str_has_prefix (uri, "dvd:/");
}

static void
totem_ensure_dot_dir (const char *path)
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
		totem_ensure_dot_dir (totem_dir);
		return totem_dir;
	}

	totem_dir = g_build_filename (g_get_home_dir (),
				      ".gnome2",
				      "Totem",
				      NULL);

	totem_ensure_dot_dir (totem_dir);

	return (const char *)totem_dir;
}

char *
totem_pictures_dir (void)
{
	const char *dir;

	dir = g_get_user_special_dir (G_USER_DIRECTORY_PICTURES);
	if (dir == NULL)
		return NULL;
	return g_strdup (dir);
}

static GnomeVFSVolume *
totem_get_volume_for_uri (GnomeVFSVolumeMonitor *monitor, const char *path)
{
	GnomeVFSVolume *vol;
	GnomeVFSDeviceType type;

	vol = gnome_vfs_volume_monitor_get_volume_for_path (monitor, path);
	if (vol == NULL)
		return NULL;

	type = gnome_vfs_volume_get_device_type (vol);
	if (type != GNOME_VFS_DEVICE_TYPE_AUDIO_CD
	    && type != GNOME_VFS_DEVICE_TYPE_VIDEO_DVD
	    && type != GNOME_VFS_DEVICE_TYPE_CDROM) {
	    	gnome_vfs_volume_unref (vol);
	    	vol = NULL;
	}

	return vol;
}

static char *
totem_get_mountpoint_for_dvd (const char *uri)
{
	if (g_str_has_prefix (uri, "dvd://") == FALSE)
		return NULL;
	return g_strdup (uri + strlen ("dvd://"));
}

static char *
totem_get_mountpoint_for_vcd (const char *uri)
{
	return NULL;
}

GnomeVFSVolume *
totem_get_volume_for_media (const char *uri)
{
	GnomeVFSVolumeMonitor *monitor;
	GnomeVFSVolume *ret;
	char *mount;

	if (uri == NULL)
		return NULL;

	mount = NULL;
	ret = NULL;

	if (g_str_has_prefix (uri, "dvd://") != FALSE)
		mount = totem_get_mountpoint_for_dvd (uri);
	else if (g_str_has_prefix (uri, "vcd:") != FALSE)
		mount = totem_get_mountpoint_for_vcd (uri);
	else if (g_str_has_prefix (uri, "file:") != FALSE)
		mount = g_filename_from_uri (uri, NULL, NULL);

	if (mount == NULL)
		return NULL;

	monitor = gnome_vfs_get_volume_monitor ();
	ret = totem_get_volume_for_uri (monitor, mount);
	g_free (mount);

	return ret;
}

static gboolean
totem_is_special_mrl (const char *uri)
{
	GnomeVFSVolume *vol;
	gboolean retval;

	if (uri == NULL || g_str_has_prefix (uri, "file:") != FALSE)
		return FALSE;
	if (g_str_has_prefix (uri, "dvb:") != FALSE)
		return TRUE;

	vol = totem_get_volume_for_media (uri);
	retval = (vol != NULL);
	if (vol != NULL)
		gnome_vfs_volume_unref (vol);

	return retval;
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

char*
totem_create_full_path (const char *path)
{
	char *retval, *curdir, *curdir_withslash, *escaped;

	g_return_val_if_fail (path != NULL, NULL);

	if (strstr (path, "://") != NULL)
		return NULL;
	if (totem_is_special_mrl (path) != FALSE)
		return NULL;

	if (path[0] == G_DIR_SEPARATOR) {
		escaped = gnome_vfs_escape_path_string (path);

		retval = g_strdup_printf ("file://%s", escaped);
		g_free (escaped);
		return retval;
	}

	curdir = g_get_current_dir ();
	escaped = gnome_vfs_escape_path_string (curdir);
	curdir_withslash = g_strdup_printf ("file://%s%c",
					    escaped, G_DIR_SEPARATOR);
	g_free (escaped);
	g_free (curdir);

	escaped = gnome_vfs_escape_path_string (path);
	retval = gnome_vfs_uri_make_full_from_relative
		(curdir_withslash, escaped);
	g_free (curdir_withslash);
	g_free (escaped);

	return retval;
}

static void
totem_action_on_unmount (GnomeVFSVolumeMonitor *vfsvolumemonitor,
			 GnomeVFSVolume *volume,
			 Totem *totem)
{
	totem_playlist_clear_with_gnome_vfs_volume (totem->playlist, volume);
}

void
totem_setup_file_monitoring (Totem *totem)
{
	totem->monitor = gnome_vfs_get_volume_monitor ();

	g_signal_connect (G_OBJECT (totem->monitor),
			  "volume_pre_unmount",
			  G_CALLBACK (totem_action_on_unmount),
			  totem);
}

/* List from xine-lib's demux_sputext.c */
static const char subtitle_ext[][4] = {
	"asc",
	"txt",
	"sub",
	"srt",
	"smi",
	"ssa"
};

static char *
totem_uri_get_subtitle_for_uri (const char *uri)
{
	GnomeVFSURI *vfsuri;
	char *subtitle;
	guint len, i, suffix;

        /* Find the filename suffix delimiter */
	len = strlen (uri);
	for (suffix = len - 1; suffix > 0; suffix--) {
		if (uri[suffix] == G_DIR_SEPARATOR ||
                    (uri[suffix] == '/')) {
			/* This filename has no extension, we'll need to 
			 * add one */
			suffix = len;
			break;
		}
		if (uri[suffix] == '.') {
			/* Found our extension marker */
			break;
		}
	}
        if (suffix < 0) {
		return NULL;
	}

	/* Generate a subtitle string with room at the end to store the
	 * 3 character extensions we want to search for */
	subtitle = g_malloc0 (suffix + 4 + 1);
	g_return_val_if_fail (subtitle != NULL, NULL);
	g_strlcpy (subtitle, uri, suffix + 4 + 1);
	g_strlcpy (subtitle + suffix, ".???", 5);
	
	/* Search for any files with one of our known subtitle extensions */
	for (i = 0; i < G_N_ELEMENTS(subtitle_ext) ; i++) {
		memcpy (subtitle + suffix + 1, subtitle_ext[i], 3);

		vfsuri = gnome_vfs_uri_new (subtitle);
		if (vfsuri != NULL) {
			if (gnome_vfs_uri_exists (vfsuri)) {
				gnome_vfs_uri_unref (vfsuri);
				return subtitle;
			}
			gnome_vfs_uri_unref (vfsuri);
		}
	}
	g_free (subtitle);
	return NULL;
}

static char *
totem_uri_get_subtitle_in_subdir (GnomeVFSURI *vfsuri, const char *subdir)
{
	char *filename, *subtitle, *fullpath_str;
	GnomeVFSURI *parent, *fullpath, *directory;

	parent = gnome_vfs_uri_get_parent (vfsuri);
	directory = gnome_vfs_uri_append_path (parent, subdir);
	gnome_vfs_uri_unref (parent);

	filename = g_path_get_basename (gnome_vfs_uri_get_path (vfsuri));
	fullpath = gnome_vfs_uri_append_string (directory, filename);
	gnome_vfs_uri_unref (directory);
	g_free (filename);

	fullpath_str = gnome_vfs_uri_to_string (fullpath, 0);
	gnome_vfs_uri_unref (fullpath);
	subtitle = totem_uri_get_subtitle_for_uri (fullpath_str);
	g_free (fullpath_str);

	return subtitle;
}

char *
totem_uri_get_subtitle_uri (const char *uri)
{
	GnomeVFSURI *vfsuri;
	char *subtitle;

	if (g_str_has_prefix (uri, "http") != FALSE) {
		return NULL;
	}

	/* Has the user specified a subtitle file manually? */
	if (strstr (uri, "#subtitle:") != NULL) {
		return NULL;
	}

	/* Does gnome-vfs support that scheme? */
	vfsuri = gnome_vfs_uri_new (uri);
	if (vfsuri == NULL)
		return NULL;

	/* Try in the current directory */
	subtitle = totem_uri_get_subtitle_for_uri (uri);
	if (subtitle != NULL) {
		gnome_vfs_uri_unref (vfsuri);
		return subtitle;
	}

	subtitle = totem_uri_get_subtitle_in_subdir (vfsuri, "subtitles");
	gnome_vfs_uri_unref (vfsuri);

	return subtitle;
}

char*
totem_uri_escape_for_display (const char *uri)
{
	char *disp, *tmp;

	disp = gnome_vfs_unescape_string_for_display (uri);
	/* If we don't have UTF-8, try to convert */
	if (g_utf8_validate (disp, -1, NULL) != FALSE)
		return disp;

	/* If we don't have UTF-8, try to convert */
	tmp = g_locale_to_utf8 (disp, -1, NULL, NULL, NULL);
	/* If we couldn't convert using the current codeset, try
	 * another one */
	if (tmp != NULL) {
		g_free (disp);
		return tmp;
	}

	tmp = g_convert (disp, -1, "UTF-8", "ISO8859-1", NULL, NULL, NULL);
	if (tmp != NULL) {
		g_free (disp);
		return tmp;
	}

	return g_strdup (uri);
}

void
totem_setup_file_filters (void)
{
	guint i;

	filter_all = gtk_file_filter_new ();
	gtk_file_filter_set_name (filter_all, _("All files"));
	gtk_file_filter_add_pattern (filter_all, "*");
	g_object_ref (filter_all);

	filter_supported = gtk_file_filter_new ();
	gtk_file_filter_set_name (filter_supported, _("Supported files"));
	for (i = 0; i < G_N_ELEMENTS (mime_types); i++) {
		gtk_file_filter_add_mime_type (filter_supported, mime_types[i]);
	}

	/* Add the special Disc-as-files formats */
	gtk_file_filter_add_mime_type (filter_supported, "application/x-cd-image");
	gtk_file_filter_add_mime_type (filter_supported, "application/x-cue");
	g_object_ref (filter_supported);

	/* Audio files */
	filter_audio = gtk_file_filter_new ();
	gtk_file_filter_set_name (filter_audio, _("Audio files"));
	for (i = 0; i < G_N_ELEMENTS (audio_mime_types); i++) {
		gtk_file_filter_add_mime_type (filter_audio, audio_mime_types[i]);
	}
	g_object_ref (filter_audio);

	/* Video files */
	filter_video = gtk_file_filter_new ();
	gtk_file_filter_set_name (filter_video, _("Video files"));
	for (i = 0; i < G_N_ELEMENTS (video_mime_types); i++) {
		gtk_file_filter_add_mime_type (filter_video, video_mime_types[i]);
	}
	gtk_file_filter_add_mime_type (filter_video, "application/x-cd-image");
	gtk_file_filter_add_mime_type (filter_video, "application/x-cue");
	g_object_ref (filter_video);
}

void
totem_destroy_file_filters (void)
{
	if (filter_all != NULL) {
		g_object_unref (filter_all);
		filter_all = NULL;
	}
	if (filter_supported != NULL) {
		g_object_unref (filter_supported);
		filter_supported = NULL;
	}
	if (filter_audio != NULL) {
		g_object_unref (filter_audio);
		filter_audio = NULL;
	}
	if (filter_video != NULL) {
		g_object_unref (filter_video);
		filter_video = NULL;
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

void
totem_add_pictures_dir (GtkWidget *chooser)
{
	const char *dir;

	g_return_if_fail (GTK_IS_FILE_CHOOSER (chooser) != FALSE);

	dir = g_get_user_special_dir (G_USER_DIRECTORY_PICTURES);
	if (dir == NULL)
		return;
	gtk_file_chooser_add_shortcut_folder (GTK_FILE_CHOOSER (chooser), dir, NULL);
}

GSList *
totem_add_files (GtkWindow *parent, const char *path)
{
	GtkWidget *fs;
	int response;
	GSList *filenames;
	char *mrl, *new_path;
	GConfClient *conf;
	gboolean set_folder;

	fs = gtk_file_chooser_dialog_new (_("Select Movies or Playlists"),
			parent,
			GTK_FILE_CHOOSER_ACTION_OPEN,
			GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
			GTK_STOCK_ADD, GTK_RESPONSE_ACCEPT,
			NULL);
	gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (fs), filter_all);
	gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (fs), filter_supported);
	gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (fs), filter_audio);
	gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (fs), filter_video);
	gtk_file_chooser_set_filter (GTK_FILE_CHOOSER (fs), filter_supported);
	gtk_dialog_set_default_response (GTK_DIALOG (fs), GTK_RESPONSE_ACCEPT);
	gtk_file_chooser_set_select_multiple (GTK_FILE_CHOOSER (fs), TRUE);
	gtk_file_chooser_set_local_only (GTK_FILE_CHOOSER (fs), FALSE);

	conf = gconf_client_get_default ();
	set_folder = TRUE;
	if (path != NULL) {
		set_folder = gtk_file_chooser_set_current_folder_uri
			(GTK_FILE_CHOOSER (fs), path);
	} else {
		new_path = gconf_client_get_string (conf, "/apps/totem/open_path", NULL);
		if (new_path != NULL && *new_path != '\0') {
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

	response = gtk_dialog_run (GTK_DIALOG (fs));
	gtk_widget_hide (fs);
	while (gtk_events_pending())
		gtk_main_iteration();

	if (response != GTK_RESPONSE_ACCEPT) {
		gtk_widget_destroy (fs);
		g_object_unref (conf);
		return NULL;
	}

	filenames = gtk_file_chooser_get_uris (GTK_FILE_CHOOSER (fs));
	if (filenames == NULL) {
		gtk_widget_destroy (fs);
		g_object_unref (conf);
		return NULL;
	}

	mrl = filenames->data;
	if (mrl != NULL) {
		new_path = g_path_get_dirname (mrl);
		gconf_client_set_string (conf, "/apps/totem/open_path",
				new_path, NULL);
		g_free (new_path);
	}

	gtk_widget_destroy (fs);
	g_object_unref (conf);

	return filenames;
}

