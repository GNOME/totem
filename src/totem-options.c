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

static struct poptOption options[] = {
	{NULL, '\0', POPT_ARG_INCLUDE_TABLE, NULL, 0, N_("Backend options"), NULL},
	{"debug", '\0', POPT_ARG_NONE, NULL, 0, N_("Enable debug"), NULL},
	{"play-pause", '\0', POPT_ARG_NONE, NULL, 0, N_("Play/Pause"), NULL},
	{"next", '\0', POPT_ARG_NONE, NULL, 0, N_("Next"), NULL},
	{"previous", '\0', POPT_ARG_NONE, NULL, 0, N_("Previous"), NULL},
	{"seek-fwd", '\0', POPT_ARG_NONE, NULL, 0, N_("Seek Forwards"), NULL},
	{"seek-bwd", '\0', POPT_ARG_NONE, NULL, 0, N_("Seek Backwards"), NULL},
	{"volume-up", '\0', POPT_ARG_NONE, NULL, 0, N_("Volume Up"), NULL},
	{"volume-down", '\0', POPT_ARG_NONE, NULL, 0, N_("Volume Down"), NULL},
	{"fullscreen", '\0', POPT_ARG_NONE, NULL, 0, N_("Toggle Fullscreen"), NULL},
	{"toggle-controls", '\0', POPT_ARG_NONE, NULL, 0, N_("Show/Hide Controls"), NULL},
	{"quit", '\0', POPT_ARG_NONE, NULL, 0, N_("Quit"), NULL},
	{"enqueue", '\0', POPT_ARG_NONE, NULL, 0, N_("Enqueue"), NULL},
	{"replace", '\0', POPT_ARG_NONE, NULL, 0, N_("Replace"), NULL},
	{NULL, '\0', 0, NULL, 0} /* end the list */
};

poptOption
totem_options_get_options (void)
{
	options[0].arg = bacon_video_widget_get_popt_table ();
	return options;
}

gboolean
totem_options_process_late (Totem *totem, int *argc, char ***argv)
{
	int i;
	guint options = 0;
	char **args = *argv;
	gboolean session_restored = FALSE;

	if (*argc == 1) {
		*argc = 0;
		*argv = *argv + 1;
		return FALSE;
	}

	for (i = 1; i < *argc; i++)
	{
		if (strcmp (args[i], "--debug") == 0)
		{
			options++;
		} else if (strcmp (args[i], "--fullscreen") == 0) {
			totem_action_fullscreen_toggle (totem);
			options++;
		} else if (strcmp (args[i], "--toggle-controls") == 0) {
			totem_action_toggle_controls (totem);
		} else if (strcmp (args[i], "--sm-config-prefix") == 0
				|| strcmp (args[i], "--sm-client-id") == 0
				|| strcmp (args[i], "--screen") == 0) {
			session_restored = TRUE;
			options++;
			i++;
			if (i < *argc)
				options++;
		} else if (strcmp (args[i], "--playlist-idx") == 0) {
			options++;
			i++;
			if (i < *argc)
			{
				options++;
				totem->index = g_ascii_strtod (args[i], NULL);
			}
		} else if (strcmp (args[i], "--seek") == 0) {
			options++;
			i++;
			if (i < *argc)
			{
				options++;
				if (sscanf (args[i], "%"G_GINT64_FORMAT, &totem->seek_to) != 1)
					totem->seek_to = 0;
			}
		} else if (g_str_has_prefix (args[i], "--") != FALSE) {
			printf (_("Option '%s' is unknown and was ignored\n"),
					args[i]);
			options++;
		}
	}

	*argc = *argc - options;
	*argv = *argv + options + 1;

	return session_restored;
}

void
totem_options_process_early (GConfClient *gc, int argc, char **argv)
{
	int i;

	if (argc == 1)
		return;

	for (i = 1; i < argc; i++)
	{
		if (strcmp (argv[i], "--debug") == 0)
		{
			gconf_client_set_bool (gc, GCONF_PREFIX"/debug",
					TRUE, NULL);
		} else if (strcmp (argv[i], "--quit") == 0) {
			/* If --quit is one of the commands, just quit */
			gdk_notify_startup_complete ();
			exit (0);
		}
	}
}

void
totem_options_process_for_server (BaconMessageConnection *conn,
		int argc, char **argv)
{
	int i, command;
	char *line, *full_path;

	if (argc == 1)
	{
		/* Just show totem if there aren't any arguments */
		line = g_strdup_printf ("%03d ", TOTEM_REMOTE_COMMAND_SHOW);
		bacon_message_connection_send (conn, line);
		g_free (line);

		return;
	}

	i = 2;

	if (strlen (argv[1]) > 3 && g_str_has_prefix (argv[1], "--") == FALSE)
	{
		command = TOTEM_REMOTE_COMMAND_REPLACE;
		i = 1;
	} else if (strcmp (argv[1], "--play-pause") == 0) {
		command = TOTEM_REMOTE_COMMAND_PAUSE;
	} else if (strcmp (argv[1], "--next") == 0) {
		command = TOTEM_REMOTE_COMMAND_NEXT;
	} else if (strcmp (argv[1], "--previous") == 0) {
		command = TOTEM_REMOTE_COMMAND_PREVIOUS;
	} else if (strcmp (argv[1], "--seek-fwd") == 0) {
		command = TOTEM_REMOTE_COMMAND_SEEK_FORWARD;
	} else if (strcmp (argv[1], "--seek-bwd") == 0) {
		command = TOTEM_REMOTE_COMMAND_SEEK_BACKWARD;
	} else if (strcmp (argv[1], "--volume-up") == 0) {
		command = TOTEM_REMOTE_COMMAND_VOLUME_UP;
	} else if (strcmp (argv[1], "--volume-down") == 0) {
		command = TOTEM_REMOTE_COMMAND_VOLUME_DOWN;
	} else if (strcmp (argv[1], "--fullscreen") == 0) {
		command = TOTEM_REMOTE_COMMAND_FULLSCREEN;
	} else if (strcmp (argv[1], "--quit") == 0) {
		command = TOTEM_REMOTE_COMMAND_QUIT;
	} else if (strcmp (argv[1], "--enqueue") == 0) {
		command = TOTEM_REMOTE_COMMAND_ENQUEUE;
	} else if (strcmp (argv[1], "--replace") == 0) {
		command = TOTEM_REMOTE_COMMAND_REPLACE;
	} else if (strcmp (argv[1], "--toggle-controls") == 0) {
		command = TOTEM_REMOTE_COMMAND_TOGGLE_CONTROLS;
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

	for (; argv[i] != NULL; i++)
	{
		full_path = totem_create_full_path (argv[i]);
		line = g_strdup_printf ("%03d %s", command, full_path);
		bacon_message_connection_send (conn, line);
		g_free (line);
		g_free (full_path);
		command = TOTEM_REMOTE_COMMAND_ENQUEUE;
	}
}

