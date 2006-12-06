/* totem-options.c

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

#include <glib/gi18n.h>
#include <string.h>
#include <stdlib.h>

#include "totem-remote.h"
#include "totem-options.h"
#include "totem-uri.h"
#include "bacon-video-widget.h"
#include "totem-private.h"

TotemCmdLineOptions optionstate;	/* Decoded command line options */

const GOptionEntry options[] = {
	{"debug", '\0', 0, G_OPTION_ARG_NONE, &optionstate.debug, N_("Enable debug"), NULL},
	{"play-pause", '\0', 0, G_OPTION_ARG_NONE, &optionstate.playpause, N_("Play/Pause"), NULL},
	{"play", '\0', 0, G_OPTION_ARG_NONE, &optionstate.play, N_("Play"), NULL},
	{"pause", '\0', 0, G_OPTION_ARG_NONE, &optionstate.pause, N_("Pause"), NULL},
	{"next", '\0', 0, G_OPTION_ARG_NONE, &optionstate.next, N_("Next"), NULL},
	{"previous", '\0', 0, G_OPTION_ARG_NONE, &optionstate.previous, N_("Previous"), NULL},
	{"seek-fwd", '\0', 0, G_OPTION_ARG_NONE, &optionstate.seekfwd, N_("Seek Forwards"), NULL},
	{"seek-bwd", '\0', 0, G_OPTION_ARG_NONE, &optionstate.seekbwd, N_("Seek Backwards"), NULL},
	{"volume-up", '\0', 0, G_OPTION_ARG_NONE, &optionstate.volumeup, N_("Volume Up"), NULL},
	{"volume-down", '\0', 0, G_OPTION_ARG_NONE, &optionstate.volumedown, N_("Volume Down"), NULL},
	{"fullscreen", '\0', 0, G_OPTION_ARG_NONE, &optionstate.fullscreen, N_("Toggle Fullscreen"), NULL},
	{"toggle-controls", '\0', 0, G_OPTION_ARG_NONE, &optionstate.togglecontrols, N_("Show/Hide Controls"), NULL},
	{"quit", '\0', 0, G_OPTION_ARG_NONE, &optionstate.quit, N_("Quit"), NULL},
	{"enqueue", '\0', 0, G_OPTION_ARG_NONE, &optionstate.enqueue, N_("Enqueue"), NULL},
	{"replace", '\0', 0, G_OPTION_ARG_NONE, &optionstate.replace, N_("Replace"), NULL},
	{"seek", '\0', G_OPTION_FLAG_HIDDEN, G_OPTION_ARG_INT64, &optionstate.seek, N_("Seek"), NULL},
	{"playlist-idx", '\0', G_OPTION_FLAG_HIDDEN, G_OPTION_ARG_DOUBLE, &optionstate.playlistidx, N_("Playlist index"), NULL},
	{G_OPTION_REMAINING, '\0', 0, G_OPTION_ARG_FILENAME_ARRAY, &optionstate.filenames, N_("Movies to play"), NULL},
	{NULL} /* end the list */
};

void
totem_options_process_late (Totem *totem, const TotemCmdLineOptions* options)
{
	if (options->fullscreen) 
		totem_action_fullscreen_toggle (totem);

	if (options->togglecontrols) 
		totem_action_toggle_controls (totem);

	/* Handle --playlist-idx */
	totem->index = options->playlistidx;

	/* Handle --seek */
	totem->seek_to = options->seek;
}

void
totem_options_process_early (GConfClient *gc, const TotemCmdLineOptions* options)
{
	if (options->debug)
	{
		gconf_client_set_bool (gc, GCONF_PREFIX"/debug",
				TRUE, NULL);
	} else if (options->quit) 
	{
		/* If --quit is one of the commands, just quit */
		gdk_notify_startup_complete ();
		exit (0);
	}
}

static void
totem_print_playing_cb (const gchar *msg, gpointer user_data)
{
	if (strcmp (msg, SHOW_PLAYING_NO_TRACKS) != 0)
		g_print ("%s\n", msg);
	exit (0);
}

void
totem_options_process_for_server (BaconMessageConnection *conn,
		const TotemCmdLineOptions* options)
{
	int i, command;
	char *line, *full_path;

#if 0 	/* FIXME: Handle TOTEM_REMOTE_COMMAND_SHOW */
	/* Just show totem if there aren't any arguments */
	line = g_strdup_printf ("%03d ", TOTEM_REMOTE_COMMAND_SHOW);
	bacon_message_connection_send (conn, line);
	g_free (line);
	return;
#endif

#if 0 	/* FIXME: Handle TOTEM_REMOTE_COMMAND_REPLACE */
	if (strlen (argv[1]) > 3 && g_str_has_prefix (argv[1], "--") == FALSE)
		command = TOTEM_REMOTE_COMMAND_REPLACE;
#endif
	if (options->playpause) 
		command = TOTEM_REMOTE_COMMAND_PLAYPAUSE;

	if (options->play)
		command = TOTEM_REMOTE_COMMAND_PLAY;

	if (options->pause)
		command = TOTEM_REMOTE_COMMAND_PAUSE;

	if (options->next)
		command = TOTEM_REMOTE_COMMAND_NEXT;

	if (options->previous)
		command = TOTEM_REMOTE_COMMAND_PREVIOUS;

	if (options->seekfwd)
		command = TOTEM_REMOTE_COMMAND_SEEK_FORWARD;

	if (options->seekbwd)
		command = TOTEM_REMOTE_COMMAND_SEEK_BACKWARD;

	if (options->volumeup)
		command = TOTEM_REMOTE_COMMAND_VOLUME_UP;

	if (options->volumedown)
		command = TOTEM_REMOTE_COMMAND_VOLUME_DOWN;

	if (options->fullscreen)
		command = TOTEM_REMOTE_COMMAND_FULLSCREEN;

	if (options->quit)
		command = TOTEM_REMOTE_COMMAND_QUIT;

	if (options->enqueue)
		command = TOTEM_REMOTE_COMMAND_ENQUEUE;

	if (options->replace)
		command = TOTEM_REMOTE_COMMAND_REPLACE;

	if (options->togglecontrols)
		command = TOTEM_REMOTE_COMMAND_TOGGLE_CONTROLS;

	if (options->printplaying)
	{
		GMainLoop *loop = g_main_loop_new (NULL, FALSE);

		command = TOTEM_REMOTE_COMMAND_SHOW_PLAYING;
		line = g_strdup_printf ("%03d ", command);
		bacon_message_connection_set_callback (conn,
				totem_print_playing_cb, loop);
		bacon_message_connection_send (conn, line);
		g_free (line);

		g_main_loop_run (loop);
		return;
	} else {
		return;
	}

	if (command != TOTEM_REMOTE_COMMAND_ENQUEUE
				&& command != TOTEM_REMOTE_COMMAND_REPLACE)
	{
		line = g_strdup_printf ("%03d ", command);
		bacon_message_connection_send (conn, line);
		g_free (line);
		return;
	}

	if (options->filenames == NULL)
		return;

	for (i = 0; options->filenames[i] != NULL; i++)
	{
		full_path = totem_create_full_path (options->filenames[i]);
		line = g_strdup_printf ("%03d %s", command, full_path);
		bacon_message_connection_send (conn, line);
		g_free (line);
		g_free (full_path);
		command = TOTEM_REMOTE_COMMAND_ENQUEUE;
	}
}

