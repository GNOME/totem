/* totem-session.c

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
   write to the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301  USA.

   Author: Bastien Nocera <hadess@hadess.net>
 */

#include "config.h"

#include "totem.h"
#include "totem-private.h"
#include "totem-session.h"
#include "totem-uri.h"

static GFile *
get_session_file (void)
{
	GFile *file;
	char *path;

	path = g_build_filename (g_get_user_config_dir (), "totem", "session_state.xspf", NULL);
	file = g_file_new_for_path (path);
	g_free (path);

	return file;
}

static char *
get_session_filename (void)
{
	GFile *file;
	char *uri;

	file = get_session_file ();
	uri = g_file_get_uri (file);
	g_object_unref (file);

	return uri;
}

gboolean
totem_session_try_restore (Totem *totem)
{
	char *uri;
	char *mrl, *subtitle;

	if (totem_playlist_get_save (totem->playlist) == FALSE)
		return FALSE;

	totem_signal_block_by_data (totem->playlist, totem);

	/* Possibly the only place in Totem where it makes sense to add an MRL to the playlist synchronously, since we haven't yet entered
	 * the GTK+ main loop, and thus can't freeze the application. */
	uri = get_session_filename ();
	if (totem_playlist_add_mrl_sync (totem->playlist, uri, &totem->seek_to_start) == FALSE) {
		totem_signal_unblock_by_data (totem->playlist, totem);
		totem_action_set_mrl (totem, NULL, NULL);
		g_free (uri);
		return FALSE;
	}
	g_free (uri);

	totem_signal_unblock_by_data (totem->playlist, totem);

	subtitle = NULL;
	mrl = totem_playlist_get_current_mrl (totem->playlist, &subtitle);

	totem_action_set_mrl_with_warning (totem, mrl, subtitle, FALSE);

	/* We do the seeking after being told that the stream is seekable,
	 * not straight away */

	g_free (mrl);
	g_free (subtitle);

	return TRUE;
}

void
totem_session_save (Totem *totem)
{
	GFile *file;
	gint64 curr;

	file = get_session_file ();
	curr = bacon_video_widget_get_current_time (totem->bvw);
	totem_playlist_save_session_playlist (totem->playlist, file, curr);
	g_object_unref (file);
}
