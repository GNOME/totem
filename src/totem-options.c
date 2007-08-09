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
	/* translators: this option prints the current movie's title on the command-line */
	{"printplaying", '\0', 0, G_OPTION_ARG_NONE, &optionstate.printplaying, N_("Print playing movie"), NULL},
	{"seek", '\0', G_OPTION_FLAG_HIDDEN, G_OPTION_ARG_INT64, &optionstate.seek, N_("Seek"), NULL},
	{"playlist-idx", '\0', G_OPTION_FLAG_HIDDEN, G_OPTION_ARG_DOUBLE, &optionstate.playlistidx, N_("Playlist index"), NULL},
	{G_OPTION_REMAINING, '\0', 0, G_OPTION_ARG_FILENAME_ARRAY, &optionstate.filenames, N_("Movies to play"), NULL},
	{NULL} /* end the list */
};

void
totem_options_process_late (Totem *totem, const TotemCmdLineOptions* options)
{
	if (options->togglecontrols) 
		totem_action_toggle_controls (totem);

	/* Handle --playlist-idx */
	totem->index = options->playlistidx;

	/* Handle --seek */
	totem->seek_to = options->seek;
}

void
totem_options_process_early (Totem *totem, const TotemCmdLineOptions* options)
{
	if (options->quit) {
		/* If --quit is one of the commands, just quit */
		gdk_notify_startup_complete ();
		exit (0);
	}

	gconf_client_set_bool (totem->gc, GCONF_PREFIX"/debug",
			       options->debug, NULL);
}

static void
totem_print_playing_cb (const gchar *msg, gpointer user_data)
{
	if (strcmp (msg, SHOW_PLAYING_NO_TRACKS) != 0)
		g_print ("%s\n", msg);
	exit (0);
}

static char *
totem_option_create_line (int command)
{
	return g_strdup_printf ("%03d ", command);
}

void
totem_options_process_for_server (BaconMessageConnection *conn,
		const TotemCmdLineOptions* options)
{
	GList *commands, *l;
	int default_action, i;

	commands = NULL;
	default_action = TOTEM_REMOTE_COMMAND_REPLACE;

	/* We can only handle "printplaying" on its own */
	if (options->printplaying)
	{
		char *line;
		GMainLoop *loop = g_main_loop_new (NULL, FALSE);

		line = totem_option_create_line (TOTEM_REMOTE_COMMAND_SHOW_PLAYING);
		bacon_message_connection_set_callback (conn,
				totem_print_playing_cb, loop);
		bacon_message_connection_send (conn, line);
		g_free (line);

		g_main_loop_run (loop);
		return;
	}

	/* Are we quitting ? */
	if (options->quit) {
		char *line;
		line = totem_option_create_line (TOTEM_REMOTE_COMMAND_QUIT);
		bacon_message_connection_send (conn, line);
		g_free (line);
		return;
	}

	/* Then handle the things that modify the playlist */
	if (options->replace && options->enqueue) {
		/* FIXME translate that */
		g_warning ("Can't enqueue and replace at the same time");
	} else if (options->replace) {
		default_action = TOTEM_REMOTE_COMMAND_REPLACE;
	} else if (options->enqueue) {
		default_action = TOTEM_REMOTE_COMMAND_ENQUEUE;
	}

	/* Send the files to enqueue */
	for (i = 0; options->filenames && options->filenames[i] != NULL; i++)
	{
		char *line, *full_path;
		full_path = totem_create_full_path (options->filenames[i]);
		line = g_strdup_printf ("%03d %s", default_action,
					full_path ? full_path : options->filenames[i]);
		bacon_message_connection_send (conn, line);
		/* Even if the default action is replace, we only want to replace with the
		   first file.  After that, we enqueue. */
		default_action = TOTEM_REMOTE_COMMAND_ENQUEUE;
		g_free (line);
		g_free (full_path);
	}

	if (options->playpause) {
		commands = g_list_append (commands, totem_option_create_line
					  (TOTEM_REMOTE_COMMAND_PLAYPAUSE));
	}

	if (options->play) {
		commands = g_list_append (commands, totem_option_create_line
					  (TOTEM_REMOTE_COMMAND_PLAY));
	}

	if (options->pause) {
		commands = g_list_append (commands, totem_option_create_line
					  (TOTEM_REMOTE_COMMAND_PAUSE));
	}

	if (options->next) {
		commands = g_list_append (commands, totem_option_create_line
					  (TOTEM_REMOTE_COMMAND_NEXT));
	}

	if (options->previous) {
		commands = g_list_append (commands, totem_option_create_line
					  (TOTEM_REMOTE_COMMAND_PREVIOUS));
	}

	if (options->seekfwd) {
		commands = g_list_append (commands, totem_option_create_line
					  (TOTEM_REMOTE_COMMAND_SEEK_FORWARD));
	}

	if (options->seekbwd) {
		commands = g_list_append (commands, totem_option_create_line
					  (TOTEM_REMOTE_COMMAND_SEEK_BACKWARD));
	}

	if (options->volumeup) {
		commands = g_list_append (commands, totem_option_create_line
					  (TOTEM_REMOTE_COMMAND_VOLUME_UP));
	}

	if (options->volumedown) {
		commands = g_list_append (commands, totem_option_create_line
					  (TOTEM_REMOTE_COMMAND_VOLUME_DOWN));
	}

	if (options->fullscreen) {
		commands = g_list_append (commands, totem_option_create_line
					  (TOTEM_REMOTE_COMMAND_FULLSCREEN));
	}

	if (options->togglecontrols) {
		commands = g_list_append (commands, totem_option_create_line
					  (TOTEM_REMOTE_COMMAND_TOGGLE_CONTROLS));
	}

	/* No commands, no files, show ourselves */
	if (commands == NULL && options->filenames == NULL) {
		char *line;
		line = totem_option_create_line (TOTEM_REMOTE_COMMAND_SHOW);
		bacon_message_connection_send (conn, line);
		g_free (line);
		return;
	}

	/* Send commands */
	for (l = commands; l != NULL; l = l->next) {
		bacon_message_connection_send (conn, l->data);
		g_free (l->data);
	}
	g_list_free (commands);
}

