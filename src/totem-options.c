/* totem-options.c

   Copyright (C) 2004 Bastien Nocera

   SPDX-License-Identifier: GPL-3-or-later

   Author: Bastien Nocera <hadess@hadess.net>
 */

#include "config.h"

#include <glib.h>
#include <glib/gi18n.h>
#include <string.h>
#include <stdlib.h>

#include "totem-options.h"
#include "totem-uri.h"
#include "bacon-video-widget.h"
#include "totem-private.h"

TotemCmdLineOptions optionstate;	/* Decoded command line options */

G_GNUC_NORETURN static gboolean
option_version_cb (const gchar *option_name,
	           const gchar *value,
	           gpointer     data,
	           GError     **error)
{
	g_print ("%s %s\n", PACKAGE, VERSION);

	exit (0);
}

const GOptionEntry all_options[] = {
	{"play-pause", '\0', 0, G_OPTION_ARG_NONE, &optionstate.playpause, N_("Play/Pause"), NULL},
	{"play", '\0', 0, G_OPTION_ARG_NONE, &optionstate.play, N_("Play"), NULL},
	{"pause", '\0', 0, G_OPTION_ARG_NONE, &optionstate.pause, N_("Pause"), NULL},
	{"next", '\0', 0, G_OPTION_ARG_NONE, &optionstate.next, N_("Next"), NULL},
	{"previous", '\0', 0, G_OPTION_ARG_NONE, &optionstate.previous, N_("Previous"), NULL},
	{"seek-fwd", '\0', 0, G_OPTION_ARG_NONE, &optionstate.seekfwd, N_("Seek Forwards"), NULL},
	{"seek-bwd", '\0', 0, G_OPTION_ARG_NONE, &optionstate.seekbwd, N_("Seek Backwards"), NULL},
	{"volume-up", '\0', 0, G_OPTION_ARG_NONE, &optionstate.volumeup, N_("Volume Up"), NULL},
	{"volume-down", '\0', 0, G_OPTION_ARG_NONE, &optionstate.volumedown, N_("Volume Down"), NULL},
	{"mute", '\0', 0, G_OPTION_ARG_NONE, &optionstate.mute, N_("Mute sound"), NULL},
	{"fullscreen", '\0', 0, G_OPTION_ARG_NONE, &optionstate.fullscreen, N_("Toggle Fullscreen"), NULL},
	{"quit", '\0', 0, G_OPTION_ARG_NONE, &optionstate.quit, N_("Quit"), NULL},
	{"enqueue", '\0', 0, G_OPTION_ARG_NONE, &optionstate.enqueue, N_("Enqueue"), NULL},
	{"replace", '\0', 0, G_OPTION_ARG_NONE, &optionstate.replace, N_("Replace"), NULL},
	{"seek", '\0', G_OPTION_FLAG_HIDDEN, G_OPTION_ARG_INT64, &optionstate.seek, N_("Seek"), NULL},
	{"version", 0, G_OPTION_FLAG_NO_ARG, G_OPTION_ARG_CALLBACK, option_version_cb, N_("Show version information and exit"), NULL},
	{G_OPTION_REMAINING, '\0', 0, G_OPTION_ARG_FILENAME_ARRAY, &optionstate.filenames, N_("Movies to play"), NULL},
	{NULL} /* end the list */
};

static void
totem_send_remote_command (Totem              *totem,
			   TotemRemoteCommand  action,
			   const char         *url)
{
	GVariant *variant;

	variant = g_variant_new ("(is)", action, url ? url : "");
	g_action_group_activate_action (G_ACTION_GROUP (totem), "remote-command", variant);
}

void
totem_options_process_for_server (Totem               *totem,
				  TotemCmdLineOptions *options)
{
	TotemRemoteCommand action;
	GList *commands, *l;
	char **filenames;
	int i;

	commands = NULL;
	action = TOTEM_REMOTE_COMMAND_REPLACE;

	/* Are we quitting ? */
	if (options->quit) {
		totem_send_remote_command (totem, TOTEM_REMOTE_COMMAND_QUIT, NULL);
		return;
	}

	/* Then handle the things that modify the playlist */
	if (options->replace && options->enqueue) {
		g_warning (_("Can’t enqueue and replace at the same time"));
	} else if (options->replace) {
		action = TOTEM_REMOTE_COMMAND_REPLACE;
	} else if (options->enqueue) {
		action = TOTEM_REMOTE_COMMAND_ENQUEUE;
	}

	filenames = options->filenames;
	options->filenames = NULL;
	options->had_filenames = (filenames != NULL);

	/* Send the files to enqueue */
	for (i = 0; filenames && filenames[i] != NULL; i++) {
		const char *filename;
		char *full_path;

		filename = filenames[i];
		full_path = totem_create_full_path (filename);

		totem_send_remote_command (totem, action, full_path ? full_path : filename);

		g_free (full_path);

		/* Even if the default action is replace, we only want to replace with the
		   first file.  After that, we enqueue. */
		if (i == 0) {
			action = TOTEM_REMOTE_COMMAND_ENQUEUE;
		}
	}

	g_clear_pointer (&filenames, g_strfreev);

	if (options->playpause) {
		commands = g_list_append (commands, GINT_TO_POINTER
					  (TOTEM_REMOTE_COMMAND_PLAYPAUSE));
	}

	if (options->play) {
		commands = g_list_append (commands, GINT_TO_POINTER
					  (TOTEM_REMOTE_COMMAND_PLAY));
	}

	if (options->pause) {
		commands = g_list_append (commands, GINT_TO_POINTER
					  (TOTEM_REMOTE_COMMAND_PAUSE));
	}

	if (options->next) {
		commands = g_list_append (commands, GINT_TO_POINTER
					  (TOTEM_REMOTE_COMMAND_NEXT));
	}

	if (options->previous) {
		commands = g_list_append (commands, GINT_TO_POINTER
					  (TOTEM_REMOTE_COMMAND_PREVIOUS));
	}

	if (options->seekfwd) {
		commands = g_list_append (commands, GINT_TO_POINTER
					  (TOTEM_REMOTE_COMMAND_SEEK_FORWARD));
	}

	if (options->seekbwd) {
		commands = g_list_append (commands, GINT_TO_POINTER
					  (TOTEM_REMOTE_COMMAND_SEEK_BACKWARD));
	}

	if (options->volumeup) {
		commands = g_list_append (commands, GINT_TO_POINTER
					  (TOTEM_REMOTE_COMMAND_VOLUME_UP));
	}

	if (options->volumedown) {
		commands = g_list_append (commands, GINT_TO_POINTER
					  (TOTEM_REMOTE_COMMAND_VOLUME_DOWN));
	}

	if (options->mute) {
		commands = g_list_append (commands, GINT_TO_POINTER
					  (TOTEM_REMOTE_COMMAND_MUTE));
	}

	if (options->fullscreen) {
		commands = g_list_append (commands, GINT_TO_POINTER
					  (TOTEM_REMOTE_COMMAND_FULLSCREEN));
	}

	/* No commands, no files, show ourselves */
	if (commands == NULL &&
	    !(g_application_get_flags (G_APPLICATION (totem)) & G_APPLICATION_IS_SERVICE)) {
		totem_send_remote_command (totem, TOTEM_REMOTE_COMMAND_SHOW, NULL);
		return;
	}

	/* Send commands */
	for (l = commands; l != NULL; l = l->next) {
		totem_send_remote_command (totem, GPOINTER_TO_INT (l->data), NULL);
	}

	g_list_free (commands);
}

