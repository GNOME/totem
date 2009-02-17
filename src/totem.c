/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * Copyright (C) 2001-2007 Bastien Nocera <hadess@hadess.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA.
 *
 * The Totem project hereby grant permission for non-gpl compatible GStreamer
 * plugins to be used and distributed together with GStreamer and Totem. This
 * permission are above and beyond the permissions granted by the GPL license
 * Totem is covered by.
 *
 * Monday 7th February 2005: Christian Schaller: Add exception clause.
 * See license_change file for details.
 *
 */

#include "config.h"

#include <glib-object.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <string.h>

#ifdef GDK_WINDOWING_X11
/* X11 headers */
#include <gdk/gdkx.h>
#include <X11/Xlib.h>
#endif

#include "totem.h"
#include "totem-private.h"
#include "totem-interface.h"
#include "totem-options.h"
#include "totem-menu.h"
#include "totem-session.h"
#include "totem-uri.h"
#include "totem-preferences.h"
#include "totem-sidebar.h"
#include "video-utils.h"

static void
long_action (void)
{
	while (gtk_events_pending ())
		gtk_main_iteration ();
}

static void
totem_message_connection_receive_cb (const char *msg, Totem *totem)
{
	char *command_str, *url;
	int command;

	if (strlen (msg) < 4)
		return;

	command_str = g_strndup (msg, 3);
	sscanf (command_str, "%d", &command);
	g_free (command_str);

	if (msg[4] != '\0')
		url = g_strdup (msg + 4);
	else
		url = NULL;

	totem_action_remote (totem, command, url);

	g_free (url);
}

static void
about_url_hook (GtkAboutDialog *about,
	        const char *link,
	        gpointer user_data)
{
	GError *error = NULL;

	if (!gtk_show_uri (gtk_widget_get_screen (GTK_WIDGET (about)),
	                   link,
	                   gtk_get_current_event_time (),
	                   &error))
	{
	        totem_interface_error (_("Could not open link"),
	                               error->message,
	                               GTK_WINDOW (about));
	        g_error_free (error);
	}
}


static void
about_email_hook (GtkAboutDialog *about,
		  const char *email_address,
		  gpointer user_data)
{
	char *escaped, *uri;

	escaped = g_uri_escape_string (email_address, NULL, FALSE);
	uri = g_strdup_printf ("mailto:%s", escaped);
	g_free (escaped);

	about_url_hook (about, uri, user_data);
	g_free (uri);
}

int
main (int argc, char **argv)
{
	Totem *totem;
	GConfClient *gc;
	GError *error = NULL;
	GOptionContext *context;
	GOptionGroup *baconoptiongroup;
	char *sidebar_pageid;

	bindtextdomain (GETTEXT_PACKAGE, GNOMELOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);

#ifdef GDK_WINDOWING_X11
	if (XInitThreads () == 0)
	{
		gtk_init (&argc, &argv);
		g_set_application_name (_("Totem Movie Player"));
		totem_action_error_and_exit (_("Could not initialize the thread-safe libraries."), _("Verify your system installation. Totem will now exit."), NULL);
	}
#endif

	g_thread_init (NULL);
	g_type_init ();

	/* Handle command line arguments */
	context = g_option_context_new (N_("- Play movies and songs"));
	baconoptiongroup = bacon_video_widget_get_option_group();
	g_option_context_add_main_entries (context, options, GETTEXT_PACKAGE);
	g_option_context_set_translation_domain(context, GETTEXT_PACKAGE);
	g_option_context_add_group (context, baconoptiongroup);

	g_option_context_add_group (context, gtk_get_option_group (TRUE));
	if (g_option_context_parse (context, &argc, &argv, &error) == FALSE) {
		g_print (_("%s\nRun '%s --help' to see a full list of available command line options.\n"),
				error->message, argv[0]);
		g_error_free (error);
	        g_option_context_free (context);
		totem_action_exit (NULL);
	}
	g_option_context_free (context);

	g_set_application_name (_("Totem Movie Player"));
	gtk_window_set_default_icon_name ("totem");
	g_setenv("PULSE_PROP_media.role", "video", TRUE);
	gtk_about_dialog_set_url_hook (about_url_hook, NULL, NULL);
	gtk_about_dialog_set_email_hook (about_email_hook, NULL, NULL);

	gc = gconf_client_get_default ();
	if (gc == NULL)
	{
		totem_action_error_and_exit (_("Totem could not initialize the configuration engine."), _("Make sure that GNOME is properly installed."), NULL);
	}

	totem = g_object_new (TOTEM_TYPE_OBJECT, NULL);
	totem->gc = gc;

	/* IPC stuff */
	if (optionstate.notconnectexistingsession == FALSE) {
		totem->conn = bacon_message_connection_new (GETTEXT_PACKAGE);
		if (bacon_message_connection_get_is_server (totem->conn) == FALSE) {
			totem_options_process_for_server (totem->conn, &optionstate);
			gdk_notify_startup_complete ();
			totem_action_exit (totem);
		} else {
			totem_options_process_early (totem, &optionstate);
		}
	} else {
		totem_options_process_early (totem, &optionstate);
	}

	/* Main window */
	totem->xml = totem_interface_load ("totem.ui", TRUE, NULL, totem);
	if (totem->xml == NULL)
		totem_action_exit (NULL);

	totem->win = GTK_WIDGET (gtk_builder_get_object (totem->xml, "totem_main_window"));

	/* Menubar */
	totem_ui_manager_setup (totem);

	/* The sidebar */
	playlist_widget_setup (totem);

	/* The rest of the widgets */
	totem->state = STATE_STOPPED;
	totem->seek = GTK_WIDGET (gtk_builder_get_object (totem->xml, "tmw_seek_hscale"));
	totem->seekadj = gtk_range_get_adjustment (GTK_RANGE (totem->seek));
	totem->volume = GTK_WIDGET (gtk_builder_get_object (totem->xml, "tmw_volume_button"));
	totem->statusbar = GTK_WIDGET (gtk_builder_get_object (totem->xml, "tmw_statusbar"));
	totem->seek_lock = FALSE;
	totem->fs = totem_fullscreen_new (GTK_WINDOW (totem->win));
	gtk_scale_button_set_adjustment (GTK_SCALE_BUTTON (totem->fs->volume),
					 gtk_scale_button_get_adjustment (GTK_SCALE_BUTTON (totem->volume)));
	gtk_range_set_adjustment (GTK_RANGE (totem->fs->seek), totem->seekadj);

	totem_session_setup (totem, argv);
	totem_setup_file_monitoring (totem);
	totem_setup_file_filters ();
	totem_setup_play_disc (totem);
	totem_callback_connect (totem);
	sidebar_pageid = totem_setup_window (totem);

	/* Show ! gtk_main_iteration trickery to show all the widgets
	 * we have so far */
	if (optionstate.fullscreen == FALSE) {
		gtk_widget_show (totem->win);
		totem_gdk_window_set_waiting_cursor (totem->win->window);
		long_action ();
	} else {
		gtk_widget_realize (totem->win);
	}

	totem->controls_visibility = TOTEM_CONTROLS_UNDEFINED;

	/* Show ! (again) the video widget this time. */
	video_widget_create (totem);
	gtk_widget_grab_focus (GTK_WIDGET (totem->bvw));
	totem_fullscreen_set_video_widget (totem->fs, totem->bvw);

	if (optionstate.fullscreen != FALSE) {
		gtk_widget_show (totem->win);
		gdk_flush ();
		totem_action_fullscreen (totem, TRUE);
	}

	/* The prefs after the video widget is connected */
	totem_setup_preferences (totem);

	totem_setup_recent (totem);

	/* Command-line handling */
	totem_options_process_late (totem, &optionstate);

	/* Initialise all the plugins, and set the default page, in case
	 * it comes from a plugin */
	totem_object_plugins_init (totem);
	totem_sidebar_set_current_page (totem, sidebar_pageid, FALSE);
	g_free (sidebar_pageid);

	if (totem->session_restored != FALSE) {
		totem_session_restore (totem, optionstate.filenames);
	} else if (optionstate.filenames != NULL && totem_action_open_files (totem, optionstate.filenames)) {
		totem_action_play_pause (totem);
	} else {
		totem_action_set_mrl (totem, NULL, NULL);
	}

	/* Set the logo at the last minute so we won't try to show it before a video */
	bacon_video_widget_set_logo (totem->bvw, LOGO_PATH);

	if (optionstate.fullscreen == FALSE)
		gdk_window_set_cursor (totem->win->window, NULL);

	if (totem->conn != NULL && bacon_message_connection_get_is_server (totem->conn) != FALSE)
	{
		bacon_message_connection_set_callback (totem->conn,
				(BaconMessageReceivedFunc)
				totem_message_connection_receive_cb, totem);
	}

	gtk_main ();

	return 0;
}
