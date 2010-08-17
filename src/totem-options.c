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
   write to the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301  USA.

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
	{"mute", '\0', 0, G_OPTION_ARG_NONE, &optionstate.mute, N_("Mute sound"), NULL},
	{"fullscreen", '\0', 0, G_OPTION_ARG_NONE, &optionstate.fullscreen, N_("Toggle Fullscreen"), NULL},
	{"toggle-controls", '\0', 0, G_OPTION_ARG_NONE, &optionstate.togglecontrols, N_("Show/Hide Controls"), NULL},
	{"quit", '\0', 0, G_OPTION_ARG_NONE, &optionstate.quit, N_("Quit"), NULL},
	{"enqueue", '\0', 0, G_OPTION_ARG_NONE, &optionstate.enqueue, N_("Enqueue"), NULL},
	{"replace", '\0', 0, G_OPTION_ARG_NONE, &optionstate.replace, N_("Replace"), NULL},
	{"no-existing-session", '\0', 0, G_OPTION_ARG_NONE, &optionstate.notconnectexistingsession, N_("Don't connect to an already-running instance"), NULL},
	{"seek", '\0', G_OPTION_FLAG_HIDDEN, G_OPTION_ARG_INT64, &optionstate.seek, N_("Seek"), NULL},
	/* Translators: help for a (hidden) command line option to specify a playlist entry to start playing on load */
	{"playlist-idx", '\0', G_OPTION_FLAG_HIDDEN, G_OPTION_ARG_DOUBLE, &optionstate.playlistidx, N_("Playlist index"), NULL},
	{ "version", 0, G_OPTION_FLAG_NO_ARG | G_OPTION_FLAG_HIDDEN, G_OPTION_ARG_CALLBACK, option_version_cb, NULL, NULL },
	{G_OPTION_REMAINING, '\0', 0, G_OPTION_ARG_FILENAME_ARRAY, &optionstate.filenames, N_("Movies to play"), NULL},
	{NULL} /* end the list */
};

void
totem_options_process_late (Totem *totem, const TotemCmdLineOptions *options)
{
	if (options->togglecontrols) 
		totem_action_toggle_controls (totem);

	/* Handle --playlist-idx */
	totem->index = options->playlistidx;

	/* Handle --seek */
	totem->seek_to_start = options->seek;
}

void
totem_options_register_remote_commands (Totem *totem)
{
	GEnumClass *klass;
	guint i;

	klass = (GEnumClass *) g_type_class_ref (TOTEM_TYPE_REMOTE_COMMAND);
	for (i = TOTEM_REMOTE_COMMAND_UNKNOWN + 1; i < klass->n_values; i++) {
		const GEnumValue *val = g_enum_get_value (klass, i);
		g_application_add_action (G_APPLICATION (totem->app), val->value_name, val->value_nick);
	}
	g_type_class_unref (klass);
}

void
totem_options_process_early (Totem *totem, const TotemCmdLineOptions* options)
{
	if (options->quit) {
		/* If --quit is one of the commands, just quit */
		gdk_notify_startup_complete ();
		exit (0);
	}

	g_settings_set_boolean (totem->settings, "debug", options->debug);
}

static char *
totem_get_action_for_command (const TotemRemoteCommand command)
{
	GEnumClass *klass;
	char *name;

	klass = g_type_class_ref (TOTEM_TYPE_REMOTE_COMMAND);
	name = g_strdup (g_enum_get_value (klass, command)->value_name);
	g_type_class_unref (klass);

	return name;
}

#define UPDATE_ACTION(action, command)                                  \
	do {                                                            \
		g_free ((action));                                      \
		(action) = totem_get_action_for_command ((command));    \
	} while (0)

void
totem_options_process_for_server (GApplication *app,
				  const TotemCmdLineOptions* options)
{
	gchar *action = NULL;
	GList *commands, *l;
	int i;

	commands = NULL;
	UPDATE_ACTION (action, TOTEM_REMOTE_COMMAND_REPLACE);

	/* Are we quitting ? */
	if (options->quit) {
		g_application_invoke_action (G_APPLICATION (app),
					     totem_get_action_for_command (TOTEM_REMOTE_COMMAND_QUIT),
					     NULL);
		return;
	}

	/* Then handle the things that modify the playlist */
	if (options->replace && options->enqueue) {
		/* FIXME translate that */
		g_warning ("Can't enqueue and replace at the same time");
	} else if (options->replace) {
		UPDATE_ACTION (action, TOTEM_REMOTE_COMMAND_REPLACE);
	} else if (options->enqueue) {
		UPDATE_ACTION (action, TOTEM_REMOTE_COMMAND_ENQUEUE);
	}

	/* Send the files to enqueue */
	for (i = 0; options->filenames && options->filenames[i] != NULL; i++) {
		GVariant *data;
		char *full_path;
		GVariantBuilder builder;

		full_path = totem_create_full_path (options->filenames[i]);

		g_variant_builder_init (&builder, G_VARIANT_TYPE ("a{sv}"));
		g_variant_builder_add (&builder, "{sv}",
				       "url", g_variant_new_string (full_path ? full_path : options->filenames[i]));
		data = g_variant_builder_end (&builder);

		g_application_invoke_action (G_APPLICATION (app), action, data);

		g_free (full_path);
		g_variant_unref (data);

		/* Even if the default action is replace, we only want to replace with the
		   first file.  After that, we enqueue. */
		if (i == 0) {
			UPDATE_ACTION (action, TOTEM_REMOTE_COMMAND_ENQUEUE);
		}
	}

	if (options->playpause) {
		commands = g_list_append (commands, totem_get_action_for_command
					  (TOTEM_REMOTE_COMMAND_PLAYPAUSE));
	}

	if (options->play) {
		commands = g_list_append (commands, totem_get_action_for_command
					  (TOTEM_REMOTE_COMMAND_PLAY));
	}

	if (options->pause) {
		commands = g_list_append (commands, totem_get_action_for_command
					  (TOTEM_REMOTE_COMMAND_PAUSE));
	}

	if (options->next) {
		commands = g_list_append (commands, totem_get_action_for_command
					  (TOTEM_REMOTE_COMMAND_NEXT));
	}

	if (options->previous) {
		commands = g_list_append (commands, totem_get_action_for_command
					  (TOTEM_REMOTE_COMMAND_PREVIOUS));
	}

	if (options->seekfwd) {
		commands = g_list_append (commands, totem_get_action_for_command
					  (TOTEM_REMOTE_COMMAND_SEEK_FORWARD));
	}

	if (options->seekbwd) {
		commands = g_list_append (commands, totem_get_action_for_command
					  (TOTEM_REMOTE_COMMAND_SEEK_BACKWARD));
	}

	if (options->volumeup) {
		commands = g_list_append (commands, totem_get_action_for_command
					  (TOTEM_REMOTE_COMMAND_VOLUME_UP));
	}

	if (options->volumedown) {
		commands = g_list_append (commands, totem_get_action_for_command
					  (TOTEM_REMOTE_COMMAND_VOLUME_DOWN));
	}

	if (options->mute) {
		commands = g_list_append (commands, totem_get_action_for_command
					  (TOTEM_REMOTE_COMMAND_MUTE));
	}

	if (options->fullscreen) {
		commands = g_list_append (commands, totem_get_action_for_command
					  (TOTEM_REMOTE_COMMAND_FULLSCREEN));
	}

	if (options->togglecontrols) {
		commands = g_list_append (commands, totem_get_action_for_command
					  (TOTEM_REMOTE_COMMAND_TOGGLE_CONTROLS));
	}

	/* No commands, no files, show ourselves */
	if (commands == NULL && options->filenames == NULL) {
		g_application_invoke_action (G_APPLICATION (app),
					     totem_get_action_for_command (TOTEM_REMOTE_COMMAND_SHOW),
					     NULL);
		return;
	}

	/* Send commands */
	for (l = commands; l != NULL; l = l->next) {
		g_application_invoke_action (G_APPLICATION (app), l->data, NULL);
	}

	g_free (action);
	g_list_foreach (commands, (GFunc) g_free, NULL);
	g_list_free (commands);
}

#undef UPDATE_ACTION

