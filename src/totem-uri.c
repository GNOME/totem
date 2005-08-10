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
#include <string.h>
#include <libgnomevfs/gnome-vfs-utils.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "totem-uri.h"
#include "totem-private.h"

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
		return g_strdup (path);
	if (totem_is_media (path) != FALSE)
		return g_strdup (path);

	if (path[0] == '/')
	{
		escaped = gnome_vfs_escape_path_string (path);

		retval = g_strdup_printf ("file://%s", escaped);
		g_free (escaped);
		return retval;
	}

	curdir = g_get_current_dir ();
	escaped = gnome_vfs_escape_path_string (curdir);
	curdir_withslash = g_strdup_printf ("file://%s%s",
			escaped, G_DIR_SEPARATOR_S);
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
static const char *subtitle_ext[] = {
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

