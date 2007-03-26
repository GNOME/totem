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

static GtkFileFilter *filter_all;
static GtkFileFilter *filter_supported;
static GtkFileFilter *filter_audio, *filter_video;

static char *xdg_user_dir_lookup (const char *type);

gboolean
totem_playing_dvd (const char *uri)
{
	if (uri == NULL)
		return FALSE;

	return g_str_has_prefix (uri, "dvd:/");
}

gboolean
totem_is_media (const char *uri)
{
	if (uri == NULL)
		return FALSE;

	if (g_str_has_prefix (uri, "cdda:") != FALSE)
		return TRUE;
	if (g_str_has_prefix (uri, "dvd:") != FALSE)
		return TRUE;
	if (g_str_has_prefix (uri, "vcd:") != FALSE)
		return TRUE;
	if (g_str_has_prefix (uri, "cd:") != FALSE)
		return TRUE;

	return FALSE;
}

gboolean
totem_is_special_mrl (const char *uri)
{
	if (uri == NULL)
		return FALSE;
	if (g_str_has_prefix (uri, "dvb:") != FALSE)
		return TRUE;

	return totem_is_media (uri);
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
		GnomeVFSVolume *volume, Totem *totem)
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

char *
totem_uri_get_subtitle_uri (const char *uri)
{
	char *suffix, *subtitle;
	guint len, i;
	GnomeVFSURI *vfsuri;

	if (g_str_has_prefix (uri, "http") != FALSE) {
		return NULL;
	}

	/* Does gnome-vfs support that scheme? */
	vfsuri = gnome_vfs_uri_new (uri);
	if (vfsuri == NULL)
		return NULL;
	gnome_vfs_uri_unref (vfsuri);

	if (strstr (uri, "#subtitle:") != NULL) {
		return NULL;
	}

	len = strlen (uri);
	if (uri[len-4] != '.') {
		return NULL;
	}

	subtitle = g_strdup (uri);
	suffix = subtitle + len - 4;
	for (i = 0; i < G_N_ELEMENTS(subtitle_ext) ; i++) {
		memcpy (suffix + 1, subtitle_ext[i], 3);

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
	g_object_unref (filter_all);
	g_object_unref (filter_supported);
	g_object_unref (filter_audio);
	g_object_unref (filter_video);
}

static const char *dir_types[] = {
	"VIDEOS",
	"MUSIC"
};

static void
totem_add_default_dirs (GtkFileChooser *dialog)
{
	guint i;
	for (i = 0; i < G_N_ELEMENTS (dir_types); i++) {
		char *dir;

		dir = xdg_user_dir_lookup (dir_types[i]);
		if (dir == NULL)
			continue;
		gtk_file_chooser_add_shortcut_folder (dialog, dir, NULL);
		g_free (dir);
	}
}

GSList *
totem_add_files (GtkWindow *parent, const char *path)
{
	GtkWidget *fs;
	int response;
	GSList *filenames;
	char *mrl, *new_path;
	GConfClient *conf;

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
	if (path != NULL) {
		gtk_file_chooser_set_current_folder_uri
			(GTK_FILE_CHOOSER (fs), path);
	} else {
		new_path = gconf_client_get_string (conf, "/apps/totem/open_path", NULL);
		if (new_path != NULL && new_path != '\0') {
			gtk_file_chooser_set_current_folder_uri
				(GTK_FILE_CHOOSER (fs), new_path);
		}
		g_free (new_path);
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

/* Copied from xdg-user-dir-lookup.c */
#include <stdlib.h>

static char *
xdg_user_dir_lookup (const char *type)
{
  FILE *file;
  char *home_dir, *config_home, *config_file;
  char buffer[512];
  char *user_dir;
  char *p, *d;
  int len;
  int relative;
  
  home_dir = getenv ("HOME");

  if (home_dir == NULL)
    return strdup ("/tmp");

  config_home = getenv ("XDG_CONFIG_HOME");
  if (config_home == NULL || config_home[0] == 0)
    {
      config_file = malloc (strlen (home_dir) + strlen ("/.config/user-dirs.dirs") + 1);
      strcpy (config_file, home_dir);
      strcat (config_file, "/.config/user-dirs.dirs");
    }
  else
    {
      config_file = malloc (strlen (config_home) + strlen ("/user-dirs.dirs") + 1);
      strcpy (config_file, config_home);
      strcat (config_file, "/user-dirs.dirs");
    }

  file = fopen (config_file, "r");
  free (config_file);
  if (file == NULL)
    goto error;

  user_dir = NULL;
  while (fgets (buffer, sizeof (buffer), file))
    {
      /* Remove newline at end */
      len = strlen (buffer);
      if (len > 0 && buffer[len-1] == '\n')
	buffer[len-1] = 0;
      
      p = buffer;
      while (*p == ' ' || *p == '\t')
	p++;
      
      if (strncmp (p, "XDG_", 4) != 0)
	continue;
      p += 4;
      if (strncmp (p, type, strlen (type)) != 0)
	continue;
      p += strlen (type);
      if (strncmp (p, "_DIR", 4) != 0)
	continue;
      p += 4;

      while (*p == ' ' || *p == '\t')
	p++;

      if (*p != '=')
	continue;
      p++;
      
      while (*p == ' ' || *p == '\t')
	p++;

      if (*p != '"')
	continue;
      p++;
      
      relative = 0;
      if (strncmp (p, "$HOME/", 6) == 0)
	{
	  p += 6;
	  relative = 1;
	}
      else if (*p != '/')
	continue;
      
      if (relative)
	{
	  user_dir = malloc (strlen (home_dir) + 1 + strlen (p) + 1);
	  strcpy (user_dir, home_dir);
	  strcat (user_dir, "/");
	}
      else
	{
	  user_dir = malloc (strlen (p) + 1);
	  *user_dir = 0;
	}
      
      d = user_dir + strlen (user_dir);
      while (*p && *p != '"')
	{
	  if ((*p == '\\') && (*(p+1) != 0))
	    p++;
	  *d++ = *p++;
	}
      *d = 0;
    }  
  fclose (file);

  if (user_dir)
    return user_dir;

 error:
  /* Special case desktop for historical compatibility */
  if (strcmp (type, "DESKTOP") == 0)
    {
      user_dir = malloc (strlen (home_dir) + strlen ("/Desktop") + 1);
      strcpy (user_dir, home_dir);
      strcat (user_dir, "/Desktop");
      return user_dir;
    }
  else
    return strdup (home_dir);
}
