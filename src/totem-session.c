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

static GFile *session_file = NULL;

static GFile *
get_session_file (void)
{
	char *path;

	if (session_file)
		return session_file;

	path = g_build_filename (totem_dot_dir (), "session_state.xspf", NULL);
	session_file = g_file_new_for_path (path);
	g_free (path);

	return session_file;
}

static char *
get_session_filename (void)
{
	return g_file_get_uri (get_session_file ());
}

gboolean
totem_session_try_restore (Totem *totem)
{
	char *uri;
	char *mrl, *subtitle;

	g_signal_group_block (totem->playlist_signals);
	totem->pause_start = TRUE;

	/* Possibly the only place in Totem where it makes sense to add an MRL to the playlist synchronously, since we haven't yet entered
	 * the GTK+ main loop, and thus can't freeze the application. */
	uri = get_session_filename ();
	if (totem_playlist_add_mrl_sync (totem->playlist, uri) == FALSE) {
		totem->pause_start = FALSE;
		g_signal_group_unblock (totem->playlist_signals);
		totem_object_set_mrl (totem, NULL, NULL);
		g_free (uri);
		return FALSE;
	}
	g_free (uri);

	g_signal_group_unblock (totem->playlist_signals);

	subtitle = NULL;
	mrl = totem_playlist_get_current_mrl (totem->playlist, &subtitle);

	if (mrl != NULL)
		totem_object_set_main_page (totem, "player");

	totem_object_set_mrl (totem, mrl, subtitle);

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
	gint64 curr = -1;

	if (totem->bvw == NULL)
		return;

	file = get_session_file ();
	if (!totem_playing_dvd (totem->mrl))
		curr = bacon_video_widget_get_current_time (totem->bvw) / 1000;
	totem_playlist_save_session_playlist (totem->playlist, file, curr);
}

void
totem_session_cleanup (Totem *totem)
{
	g_file_delete (get_session_file (), NULL, NULL);
	g_clear_object (&session_file);
}
