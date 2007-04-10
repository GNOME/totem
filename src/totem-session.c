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
   write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.

   Author: Bastien Nocera <hadess@hadess.net>
 */

#include "config.h"

#include "totem.h"
#include "totem-private.h"
#include "totem-session.h"
#include "totem-uri.h"

#ifndef HAVE_GTK_ONLY

#include <libgnome/gnome-config.h>
#include <libgnomeui/gnome-client.h>

static char *
totem_session_create_key (void)
{
	char *filename, *path;

	filename = g_strdup_printf ("playlist-%d-%d-%u.pls",
			(int) getpid (),
			(int) time (NULL),
			g_random_int ());
	path = g_build_filename (totem_dot_dir (), filename, NULL);
	g_free (filename);

	return path;
}

static gboolean
totem_save_yourself_cb (GnomeClient *client, int phase, GnomeSaveStyle style,
		gboolean shutting_down, GnomeInteractStyle interact_style,
		gboolean fast, Totem *totem)
{
	char *argv[] = { NULL, NULL, NULL, NULL, NULL, NULL, NULL };
	int i = 0;
	char *path_id, *current, *seek, *uri;

	if (style == GNOME_SAVE_GLOBAL)
		return TRUE;

	path_id = totem_session_create_key ();
	totem_playlist_save_current_playlist (totem->playlist, path_id);

	/* How to discard the save */
	argv[i++] = "rm";
	argv[i++] = "-f";
	argv[i++] = path_id;
	gnome_client_set_discard_command (client, i, argv);

	/* How to clone or restart */
	i = 0;
	current = g_strdup_printf ("%d",
			totem_playlist_get_current (totem->playlist));
	seek = g_strdup_printf ("%"G_GINT64_FORMAT,
			bacon_video_widget_get_current_time (totem->bvw));
	argv[i++] = (char *) totem->argv0;
	argv[i++] = "--playlist-idx";
	argv[i++] = current;
	argv[i++] = "--seek";
	argv[i++] = seek;

	uri = g_filename_to_uri (path_id, NULL, NULL);
	argv[i++] = uri;

	gnome_client_set_clone_command (client, i, argv);
	gnome_client_set_restart_command (client, i, argv);

	g_free (path_id);
	g_free (current);
	g_free (seek);
	g_free (uri);

	return TRUE;
}

static void
totem_client_die_cb (GnomeClient *client, Totem *totem)
{
	totem_action_exit (totem);
}

void
totem_session_setup (Totem *totem, char **argv)
{
	GnomeClient *client;
	GnomeClientFlags flags;

	totem->argv0 = argv[0];

	client = gnome_master_client ();
	g_signal_connect (G_OBJECT (client), "save-yourself",
			G_CALLBACK (totem_save_yourself_cb), totem);
	g_signal_connect (G_OBJECT (client), "die",
			G_CALLBACK (totem_client_die_cb), totem);

	flags = gnome_client_get_flags (client);
	if (flags & GNOME_CLIENT_RESTORED)
		totem->session_restored = TRUE;
}

void
totem_session_restore (Totem *totem, char **filenames)
{
	char *mrl, *uri;

	g_return_if_fail (filenames[0] != NULL);
	uri = filenames[0];

	totem_signal_block_by_data (totem->playlist, totem);

	if (totem_playlist_add_mrl (totem->playlist, uri, NULL) == FALSE)
	{
		totem_signal_unblock_by_data (totem->playlist, totem);
		totem_action_set_mrl (totem, NULL);
		g_free (uri);
		return;
	}

	totem_signal_unblock_by_data (totem->playlist, totem);

	if (totem->index != 0)
		totem_playlist_set_current (totem->playlist, totem->index);
	mrl = totem_playlist_get_current_mrl (totem->playlist);

	totem_action_set_mrl_with_warning (totem, mrl, FALSE);

	if (totem->seek_to != 0)
	{
		bacon_video_widget_seek_time (totem->bvw,
				totem->seek_to, NULL);
	}
	bacon_video_widget_pause (totem->bvw);

	g_free (mrl);

	return;
}

#else

void
totem_session_setup (Totem *totem, char **argv)
{
}

void
totem_session_restore (Totem *totem, char **argv)
{
}

#endif /* !HAVE_GTK_ONLY */
