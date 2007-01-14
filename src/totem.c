/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * Copyright (C) 2001-2006 Bastien Nocera <hadess@hadess.net>
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * The Totem project hereby grant permission for non-gpl compatible GStreamer
 * plugins to be used and distributed together with GStreamer and Totem. This
 * permission are above and beyond the permissions granted by the GPL license
 * Totem is covered by.
 *
 * Monday 7th February 2005: Christian Schaller: Add excemption clause.
 * See license_change file for details.
 *
 */

#include "config.h"

#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include <stdlib.h>

#ifndef HAVE_GTK_ONLY
#include <gnome.h>
#include "totem-gromit.h"
#endif /* !HAVE_GTK_ONLY */

#include <string.h>

#ifdef GDK_WINDOWING_X11
/* X11 headers */
#include <gdk/gdkx.h>
#include <X11/Xlib.h>
#ifdef HAVE_XFREE
#include <X11/XF86keysym.h>
#endif
#endif

#include "bacon-video-widget.h"
#include "bacon-video-widget-properties.h"
#include "totem-statusbar.h"
#include "totem-time-label.h"
#include "totem-session.h"
#include "totem-screenshot.h"
#include "totem-sidebar.h"
#include "totem-menu.h"
#include "totem-missing-plugins.h"
#include "totem-options.h"
#include "totem-uri.h"
#include "totem-interface.h"
#include "bacon-volume.h"
#include "video-utils.h"

#include "totem.h"
#include "totem-private.h"
#include "totem-preferences.h"
#include "totem-disc.h"

#include "debug.h"

#define REWIND_OR_PREVIOUS 4000

#define SEEK_FORWARD_SHORT_OFFSET 15
#define SEEK_BACKWARD_SHORT_OFFSET -5

#define SEEK_FORWARD_LONG_OFFSET 10*60
#define SEEK_BACKWARD_LONG_OFFSET -3*60

#define ZOOM_UPPER 200
#define ZOOM_RESET 100
#define ZOOM_LOWER 10
#define ZOOM_DISABLE (ZOOM_LOWER - 1)
#define ZOOM_ENABLE (ZOOM_UPPER + 1)

#define FULLSCREEN_POPUP_TIMEOUT 5 * 1000

#define BVW_VBOX_BORDER_WIDTH 1

static const GtkTargetEntry target_table[] = {
	{ "text/uri-list", 0, 0 },
	{ "_NETSCAPE_URL", 0, 1 },
};

static const GtkTargetEntry source_table[] = {
	{ "text/uri-list", 0, 0 },
};

static gboolean totem_action_open_files (Totem *totem, char **list);
static gboolean totem_action_open_files_list (Totem *totem, GSList *list);
static void update_fullscreen_size (Totem *totem);
static gboolean popup_hide (Totem *totem);
static void update_buttons (Totem *totem);
static void update_media_menu_items (Totem *totem);
static void update_seekable (Totem *totem, gboolean force_false);
static void playlist_changed_cb (GtkWidget *playlist, Totem *totem);
static void play_pause_set_label (Totem *totem, TotemStates state);
static gboolean on_video_motion_notify_event (GtkWidget *widget, GdkEventMotion *event, Totem *totem);
static void popup_timeout_remove (Totem *totem);
static gint totem_compare_recent_stream_items (GtkRecentInfo *a, GtkRecentInfo *b);

static void
long_action (void)
{
	while (gtk_events_pending ())
		gtk_main_iteration ();
}

void
totem_action_error (const char *title, const char *reason, Totem *totem)
{
	totem_interface_error (title, reason,
			GTK_WINDOW (totem->win));
}

static void
totem_action_error_and_exit (const char *title,
		const char *reason, Totem *totem)
{
	totem_interface_error_blocking (title, reason,
			GTK_WINDOW (totem->win));
	totem_action_exit (totem);
}

static void
totem_action_save_size (Totem *totem)
{
	GtkWidget *item;

	if (totem->bvw == NULL)
		return;

	if (totem_is_fullscreen (totem) != FALSE)
		return;

	/* Save the size of the video widget */
	item = glade_xml_get_widget (totem->xml, "tmw_main_pane");
	gtk_window_get_size (GTK_WINDOW (totem->win), &totem->window_w,
			&totem->window_h);
	totem->sidebar_w = totem->window_w
		- gtk_paned_get_position (GTK_PANED (item));
}

static void
totem_action_save_state (Totem *totem)
{
	GKeyFile *keyfile;
	char *contents, *filename;
	const char *page_id;

	keyfile = g_key_file_new ();
	g_key_file_set_integer (keyfile, "State",
			"window_w", totem->window_w);
	g_key_file_set_integer (keyfile, "State",
			"window_h", totem->window_h);
	g_key_file_set_boolean (keyfile, "State",
			"show_sidebar", totem_sidebar_is_visible (totem));
	g_key_file_set_boolean (keyfile, "State",
			"maximised", totem->maximised);
	g_key_file_set_integer (keyfile, "State",
			"sidebar_w", totem->sidebar_w);

	page_id = totem_sidebar_get_current_page (totem);
	g_key_file_set_string (keyfile, "State",
			"sidebar_page", page_id);

	contents = g_key_file_to_data (keyfile, NULL, NULL);
	g_key_file_free (keyfile);
	filename = g_build_filename (g_get_home_dir (), ".gnome2", "totem", NULL);
	g_file_set_contents (filename, contents, -1, NULL);

	g_free (filename);
	g_free (contents);
}

void
totem_action_exit (Totem *totem)
{
	GdkDisplay *display = NULL;

	if (totem != NULL)
		popup_timeout_remove (totem);

	if (gtk_main_level () > 0)
		gtk_main_quit ();

	if (totem == NULL)
		exit (0);

	if (totem->win != NULL) {
		gtk_widget_hide (totem->win);
		display = gtk_widget_get_display (totem->win);
	}

	if (totem->prefs != NULL) {
		gtk_widget_hide (totem->prefs);
	}

#ifndef HAVE_GTK_ONLY
	totem_gromit_clear (TRUE);
#endif /* !HAVE_GTK_ONLY */

	if (display != NULL)
		gdk_display_sync (display);

	if (totem->bvw) {
		//FIXME move the volume to the static file?
		gconf_client_set_int (totem->gc,
				GCONF_PREFIX"/volume",
				bacon_video_widget_get_volume (totem->bvw),
				NULL);
		totem_action_save_size (totem);
	}

	bacon_message_connection_free (totem->conn);
	totem_action_save_state (totem);

	totem_action_fullscreen (totem, FALSE);

	totem_sublang_exit (totem);
	totem_destroy_file_filters ();

	if (totem->gc)
		g_object_unref (G_OBJECT (totem->gc));

	if (totem->win)
		gtk_widget_destroy (GTK_WIDGET (totem->win));

	gnome_vfs_shutdown ();

	exit (0);
}

static void
totem_action_menu_popup (Totem *totem, guint button)
{
	GtkWidget *menu;

	menu = gtk_ui_manager_get_widget (totem->ui_manager,
			"/totem-main-popup");
	gtk_menu_popup (GTK_MENU (menu), NULL, NULL, NULL, NULL,
			button, gtk_get_current_event_time ());
	gtk_menu_shell_select_first (GTK_MENU_SHELL (menu), FALSE);
}

static gboolean
main_window_destroy_cb (GtkWidget *widget, GdkEvent *event, Totem *totem)
{
	totem_action_exit (totem);

	return FALSE;
}

static void
play_pause_set_label (Totem *totem, TotemStates state)
{
	GtkAction *action;
	const char *id, *tip;
	GSList *l, *proxies;

	if (state == totem->state)
		return;

	switch (state)
	{
	case STATE_PLAYING:
		totem_statusbar_set_text (TOTEM_STATUSBAR (totem->statusbar),
				_("Playing"));
		id = GTK_STOCK_MEDIA_PAUSE;
		tip = N_("Pause");
		break;
	case STATE_PAUSED:
		totem_statusbar_set_text (TOTEM_STATUSBAR (totem->statusbar),
				_("Paused"));
		id = GTK_STOCK_MEDIA_PLAY;
		tip = N_("Play");
		break;
	case STATE_STOPPED:
		totem_statusbar_set_text (TOTEM_STATUSBAR (totem->statusbar),
				_("Stopped"));
		totem_statusbar_set_time_and_length
			(TOTEM_STATUSBAR (totem->statusbar), 0, 0);
		id = GTK_STOCK_MEDIA_PLAY;
		tip = N_("Play");
		break;
	default:
		return;
	}

	action = gtk_action_group_get_action (totem->main_action_group, "play");
	g_object_set (G_OBJECT (action),
			"tooltip", _(tip),
			"stock-id", id, NULL);

	proxies = gtk_action_get_proxies (action);
	for (l = proxies; l != NULL; l = l->next) {
		atk_object_set_name (gtk_widget_get_accessible (l->data),
				_(tip));
	}

	totem->state = state;
}

void
totem_action_eject (Totem *totem)
{
	GError *err = NULL;
	char *cmd, *prefix;
	const char *needle;
	char *device;

	needle = strchr (totem->mrl, ':');
	g_assert (needle != NULL);
	/* we want the ':' as well */
	prefix = g_strndup (totem->mrl, needle - totem->mrl + 1);
	totem_playlist_clear_with_prefix (totem->playlist, prefix);
	g_free (prefix);

	g_object_get (G_OBJECT (totem->bvw),
			"mediadev", &device, NULL);
	cmd = g_strdup_printf ("eject %s", device);
	g_free (device);

	if (g_spawn_command_line_sync (cmd, NULL, NULL, NULL, &err) == FALSE)
	{
		totem_action_error (_("Totem could not eject the optical media."), err->message, totem);
		g_error_free (err);
	}
	g_free (cmd);
}

void
totem_action_show_properties (Totem *totem)
{
	totem_sidebar_set_current_page (totem, "properties");
}

void
totem_action_play (Totem *totem)
{
	GError *err = NULL;
	int retval;

	if (totem->mrl == NULL)
		return;

	if (bacon_video_widget_is_playing (totem->bvw) != FALSE)
		return;

	retval = bacon_video_widget_play (totem->bvw,  &err);
	play_pause_set_label (totem, retval ? STATE_PLAYING : STATE_STOPPED);
	if (totem_is_fullscreen (totem) != FALSE)
		totem_scrsaver_set_state (totem->scr, !retval);

	if (retval == FALSE)
	{
		char *msg, *disp;

		disp = totem_uri_escape_for_display (totem->mrl);
		msg = g_strdup_printf(_("Totem could not play '%s'."), disp);
		g_free (disp);

		totem_playlist_set_playing (totem->playlist, FALSE);
		totem_action_error (msg, err->message, totem);
		totem_action_stop (totem);
		g_free (msg);
		g_error_free (err);
	}
}

static void
totem_action_seek (Totem *totem, double pos)
{
	GError *err = NULL;
	int retval;

	if (totem->mrl == NULL)
		return;
	if (bacon_video_widget_is_seekable (totem->bvw) == FALSE)
		return;

	retval = bacon_video_widget_seek (totem->bvw, pos, &err);

	if (retval == FALSE)
	{
		char *msg, *disp;

		disp = totem_uri_escape_for_display (totem->mrl);
		msg = g_strdup_printf(_("Totem could not play '%s'."), disp);
		g_free (disp);

		totem_playlist_set_playing (totem->playlist, FALSE);
		totem_action_error (msg, err->message, totem);
		totem_action_stop (totem);
		g_free (msg);
		g_error_free (err);
	}
}

void
totem_action_set_mrl_and_play (Totem *totem, const char *mrl)
{
	if (totem_action_set_mrl (totem, mrl) != FALSE)
		totem_action_play (totem);
}

static gboolean
totem_action_open_dialog (Totem *totem, const char *path, gboolean play)
{
	GSList *filenames;
	gboolean playlist_modified;

	filenames = totem_add_files (GTK_WINDOW (totem->win), path);

	if (filenames == NULL)
		return FALSE;

	playlist_modified = totem_action_open_files_list (totem,
			filenames);

	if (playlist_modified == FALSE) {
		g_slist_foreach (filenames, (GFunc) g_free, NULL);
		g_slist_free (filenames);
		return FALSE;
	}

	g_slist_foreach (filenames, (GFunc) g_free, NULL);
	g_slist_free (filenames);

	if (play != FALSE) {
		char *mrl;

		mrl = totem_playlist_get_current_mrl (totem->playlist);
		totem_action_set_mrl_and_play (totem, mrl);
		g_free (mrl);
	}

	return TRUE;
}

static gboolean
totem_action_load_media (Totem *totem, MediaType type)
{
	char **mrls;
	char *msg;
	gboolean retval;

	if (bacon_video_widget_can_play (totem->bvw, type) == FALSE)
	{
		msg = g_strdup_printf (_("Totem cannot play this type of media (%s) because you do not have the appropriate plugins to handle it."), _(totem_cd_get_human_readable_name (type)));
		totem_action_error (msg, _("Please install the necessary plugins and restart Totem to be able to play this media."), totem);
		g_free (msg);
		return FALSE;
	}

	mrls = bacon_video_widget_get_mrls (totem->bvw, type);
	if (mrls == NULL)
	{
		msg = g_strdup_printf (_("Totem could not play this media (%s) although a plugin is present to handle it."), _(totem_cd_get_human_readable_name (type)));
		totem_action_error (msg, _("You might want to check that a disc is present in the drive and that it is correctly configured."), totem);
		g_free (msg);
		return FALSE;
	}

	retval = totem_action_open_files (totem, mrls);
	g_strfreev (mrls);

	return retval;
}

static gboolean
totem_action_load_media_device (Totem *totem, const char *device)
{
	MediaType type;
	GError *error = NULL;
	char *device_path, *url;
	gboolean retval;

	if (g_str_has_prefix (device, "file://") != FALSE)
		device_path = g_filename_from_uri (device, NULL, NULL);
	else
		device_path = g_strdup (device);

	type = totem_cd_detect_type_with_url (device_path, &url, &error);

	switch (type) {
		case MEDIA_TYPE_ERROR:
			totem_action_error (_("Totem was not able to play this disc."),
					    error ? error->message : _("No reason."),
					    totem);
			retval = FALSE;
			break;
		case MEDIA_TYPE_DATA:
			/* Set default location to the mountpoint of
			 * this device */
			retval = totem_action_open_dialog (totem, url, FALSE);
			break;
		case MEDIA_TYPE_DVD:
		case MEDIA_TYPE_VCD:
		case MEDIA_TYPE_CDDA:
			bacon_video_widget_set_media_device
				(BACON_VIDEO_WIDGET (totem->bvw), device_path);
			retval = totem_action_load_media (totem, type);
			if (retval == FALSE) {
				totem_action_set_mrl_and_play (totem, NULL);
			}
			break;
		default:
			g_assert_not_reached ();
	}

	g_free (url);
	g_free (device_path);

	return retval;
}

void
totem_action_play_media_device (Totem *totem, const char *device)
{
	char *mrl;

	if (totem_action_load_media_device (totem, device) != FALSE) {
		mrl = totem_playlist_get_current_mrl (totem->playlist);
		totem_action_set_mrl_and_play (totem, mrl);
		g_free (mrl);
	} 
}

void
totem_action_play_media (Totem *totem, MediaType type)
{
	char *mrl;

	if (totem_action_load_media (totem, type) != FALSE) {
		mrl = totem_playlist_get_current_mrl (totem->playlist);
		totem_action_set_mrl_and_play (totem, mrl);
		g_free (mrl);
	}
}

void
totem_action_stop (Totem *totem)
{
	bacon_video_widget_stop (totem->bvw);
	totem_scrsaver_enable (totem->scr);
}

void
totem_action_play_pause (Totem *totem)
{
	if (totem->mrl == NULL)
	{
		char *mrl;

		/* Try to pull an mrl from the playlist */
		mrl = totem_playlist_get_current_mrl (totem->playlist);
		if (mrl == NULL)
		{
			play_pause_set_label (totem, STATE_STOPPED);
			return;
		} else {
			totem_action_set_mrl_and_play (totem, mrl);
			g_free (mrl);
			return;
		}
	}

	if (bacon_video_widget_is_playing (totem->bvw) == FALSE)
	{
		bacon_video_widget_play (totem->bvw, NULL);
		play_pause_set_label (totem, STATE_PLAYING);
		if (totem_is_fullscreen (totem) != FALSE)
			totem_scrsaver_disable (totem->scr);
	} else {
		bacon_video_widget_pause (totem->bvw);
		play_pause_set_label (totem, STATE_PAUSED);
		totem_scrsaver_enable (totem->scr);
	}
}

void
totem_action_pause (Totem *totem)
{
	if (bacon_video_widget_is_playing (totem->bvw) != FALSE)
	{
		bacon_video_widget_pause (totem->bvw);
		play_pause_set_label (totem, STATE_PAUSED);
		totem_scrsaver_enable (totem->scr);
	}
}

static void
totem_action_set_cursor (Totem *totem, gboolean state)
{
	totem->cursor_shown = state;
	bacon_video_widget_set_show_cursor (totem->bvw, state);
}

static gboolean
window_state_event_cb (GtkWidget *window, GdkEventWindowState *event,
		Totem *totem)
{
	if (event->changed_mask == GDK_WINDOW_STATE_MAXIMIZED) {
		totem->maximised = (event->new_window_state
				& GDK_WINDOW_STATE_MAXIMIZED);
		return FALSE;
	}

	if (event->changed_mask != GDK_WINDOW_STATE_FULLSCREEN) {
		return FALSE;
	}

	if (event->new_window_state & GDK_WINDOW_STATE_FULLSCREEN) {
		totem_action_save_size (totem);
		update_fullscreen_size (totem);
		bacon_video_widget_set_fullscreen (totem->bvw, TRUE);
		totem_action_set_cursor (totem, FALSE);

		if (bacon_video_widget_is_playing (totem->bvw) != FALSE)
			totem_scrsaver_disable (totem->scr);

		totem->controls_visibility = TOTEM_CONTROLS_FULLSCREEN;
		show_controls (totem, FALSE);
		totem_action_set_sensitivity ("fullscreen", FALSE);
	} else {
		GtkAction *action;

		popup_hide (totem);
		bacon_video_widget_set_fullscreen (totem->bvw, FALSE);
		totem_action_set_cursor (totem, TRUE);

		totem_scrsaver_enable (totem->scr);

		action = gtk_action_group_get_action (totem->main_action_group,
				"show-controls");

		if (gtk_toggle_action_get_active (GTK_TOGGLE_ACTION (action)))
			totem->controls_visibility = TOTEM_CONTROLS_VISIBLE;
		else
			totem->controls_visibility = TOTEM_CONTROLS_HIDDEN;

		show_controls (totem, TRUE);
		totem_action_set_sensitivity ("fullscreen", TRUE);
	}

	return FALSE;
}

void
totem_action_fullscreen_toggle (Totem *totem)
{
	if (totem_is_fullscreen (totem) != FALSE) {
		gtk_window_unfullscreen (GTK_WINDOW (totem->win));
	} else {
		gtk_window_fullscreen (GTK_WINDOW (totem->win));
	}
}

void
totem_action_fullscreen (Totem *totem, gboolean state)
{
	if (totem_is_fullscreen (totem) == state)
		return;

	totem_action_fullscreen_toggle (totem);
}

void
totem_action_open (Totem *totem)
{
	totem_action_open_dialog (totem, NULL, TRUE);
}

static gint
totem_compare_recent_stream_items (GtkRecentInfo *a, GtkRecentInfo *b)
{
	gboolean has_totem_a, has_totem_b;

	has_totem_a = gtk_recent_info_has_group (a, "TotemStreams");
	has_totem_b = gtk_recent_info_has_group (b, "TotemStreams");

	if (has_totem_a && has_totem_b) {
		time_t time_a, time_b;

		time_a = gtk_recent_info_get_modified (a);
		time_b = gtk_recent_info_get_modified (b);

		return (time_b - time_a);
	} else if (has_totem_a) {
		return -1;
	} else if (has_totem_b) {
		return 1;
	}

	return 0;
}

static char *
totem_open_location_set_from_clipboard (Totem *totem)
{
	GtkClipboard *clipboard;
	gchar *clipboard_content;

	/* Initialize the clipboard and get its content */
	clipboard = gtk_clipboard_get_for_display (gtk_widget_get_display (totem->win), GDK_SELECTION_CLIPBOARD);
	clipboard_content = gtk_clipboard_wait_for_text (clipboard);

	/* Check clipboard for "://". If it exists, return it */
	if (clipboard_content != NULL && strcmp (clipboard_content, "") != 0)
	{
		if (g_strrstr (clipboard_content, "://") != NULL)
			return clipboard_content;
	}

	g_free (clipboard_content);
	return NULL;
}

static gboolean
totem_open_location_match (GtkEntryCompletion *completion, const gchar *key, GtkTreeIter *iter, gpointer user_data)
{
	/* Substring-match key against uri */
	char *uri, *match;

	g_return_val_if_fail (key != NULL, FALSE);
	gtk_tree_model_get (user_data, iter, 0, &uri, -1);
	g_return_val_if_fail (uri != NULL, FALSE);
	match = strstr (uri, key);
	g_free (uri);

	return (match != NULL);
}

void
totem_action_open_location (Totem *totem)
{
	GladeXML *glade;
	char *mrl, *clipboard_location;
	GtkWidget *dialog, *entry;
	int response;
	const char *filenames[2];
	GtkEntryCompletion *completion;
	GtkTreeModel *model;
	GList *recent_items;

	glade = totem_interface_load ("uri.glade", _("Open Location..."),
			FALSE, GTK_WINDOW (totem->win));
	if (glade == NULL)
		return;

	/* Get item from clipboard to fill entry in Open Location... dialog */
	clipboard_location = totem_open_location_set_from_clipboard (totem);
	if (clipboard_location != NULL && strcmp (clipboard_location, "") != 0)
		gtk_entry_set_text (GTK_ENTRY (glade_xml_get_widget (glade, "uri")), clipboard_location);
	g_free (clipboard_location);

	dialog = glade_xml_get_widget (glade, "open_uri_dialog");
	entry = glade_xml_get_widget (glade, "uri");

	completion = gtk_entry_completion_new();
	model = GTK_TREE_MODEL (gtk_list_store_new (1, G_TYPE_STRING));
	gtk_entry_set_completion (GTK_ENTRY (entry), completion);

	/* Add items in Totem's GtkRecentManager to the uri_list GtkEntry's GtkEntryCompletion */
	recent_items = gtk_recent_manager_get_items (totem->recent_manager);

	if (recent_items != NULL)
	{
		GList *p;
		GtkTreeIter iter;

		recent_items = g_list_sort (recent_items, (GCompareFunc) totem_compare_recent_stream_items);

		for (p = recent_items; p != NULL; p = p->next)
		{
			if (!gtk_recent_info_has_group ((GtkRecentInfo *) p->data, "TotemStreams"))
				continue;

			gtk_list_store_append (GTK_LIST_STORE (model), &iter);
			gtk_list_store_set (GTK_LIST_STORE (model), &iter, 0, gtk_recent_info_get_uri ((GtkRecentInfo *) p->data), -1);
			gtk_recent_info_unref ((GtkRecentInfo *) p->data);
		}
	}

	g_list_free (recent_items);

	gtk_entry_completion_set_model (completion, model);
	gtk_entry_completion_set_text_column (completion, 0);
	gtk_entry_completion_set_match_func (completion, (GtkEntryCompletionMatchFunc) totem_open_location_match, model, NULL);

	response = gtk_dialog_run (GTK_DIALOG (dialog));

	if (response == GTK_RESPONSE_OK)
	{
		char *uri;

		uri = g_strdup (gtk_entry_get_text (GTK_ENTRY (entry)));

		if (uri != NULL && strcmp (uri, "") != 0)
		{
			if (g_strrstr (uri, "://") == NULL)
			{
				char *tmp;
				tmp = g_strconcat ("http://", uri, NULL);
				g_free (uri);
				uri = tmp;
			}

			filenames[0] = uri;
			filenames[1] = NULL;
			totem_action_open_files (totem, (char **) filenames);

			mrl = totem_playlist_get_current_mrl (totem->playlist);
			totem_action_set_mrl_and_play (totem, mrl);
			g_free (mrl);
		}
		g_free (uri);
	}

	gtk_widget_destroy (dialog);
	g_object_unref (glade);
}

void
totem_action_take_screenshot (Totem *totem)
{
	GdkPixbuf *pixbuf;
	GtkWidget *dialog;
	char *filename;
	GError *err = NULL;

	if (bacon_video_widget_get_logo_mode (totem->bvw) != FALSE)
		return;

	if (bacon_video_widget_can_get_frames (totem->bvw, &err) == FALSE)
	{
		if (err == NULL)
			return;

		totem_action_error (_("Totem could not get a screenshot of that film."), err->message, totem);
		g_error_free (err);
		return;
	}

	pixbuf = bacon_video_widget_get_current_frame (totem->bvw);
	if (pixbuf == NULL)
	{
		totem_action_error (_("Totem could not get a screenshot of that film."), _("This is not supposed to happen; please file a bug report."), totem);
		return;
	}

	filename = g_build_filename (DATADIR,
			"totem", "screenshot.glade", NULL);
	dialog = totem_screenshot_new (filename, pixbuf);
	g_free (filename);

	gtk_dialog_run (GTK_DIALOG (dialog));
	gtk_widget_destroy (dialog);
	g_object_unref (pixbuf);
}

static char *
totem_get_nice_name_for_stream (Totem *totem)
{
	char *title, *artist, *retval;
	int tracknum;
	GValue value = { 0, };

	bacon_video_widget_get_metadata (totem->bvw, BVW_INFO_TITLE, &value);
	title = g_value_dup_string (&value);
	g_value_unset (&value);

	if (title == NULL)
		return NULL;

	bacon_video_widget_get_metadata (totem->bvw, BVW_INFO_ARTIST, &value);
	artist = g_value_dup_string (&value);
	g_value_unset (&value);

	if (artist == NULL)
		return title;

	bacon_video_widget_get_metadata (totem->bvw, BVW_INFO_TRACK_NUMBER,
			&value);
	tracknum = g_value_get_int (&value);

	if (tracknum != 0) {
		retval = g_strdup_printf ("%02d. %s - %s",
				tracknum, artist, title);
	} else {
		retval = g_strdup_printf ("%s - %s", artist, title);
	}
	g_free (artist);
	g_free (title);

	return retval;
}

static void
update_skip_to (Totem *totem, gint64 time)
{
	if (totem->skipto != NULL)
		totem_skipto_update_range (totem->skipto, time);
}

static void
update_mrl_label (Totem *totem, const char *name)
{
	gint time;
	char *text;
	GtkWidget *widget;

	if (name != NULL)
	{
		char *escaped;

		/* Get the length of the stream */
		time = bacon_video_widget_get_stream_length (totem->bvw);
		totem_statusbar_set_time_and_length (TOTEM_STATUSBAR
				(totem->statusbar), 0, time / 1000);

		update_skip_to (totem, time);

		/* Update the mrl label */
		escaped = g_markup_escape_text (name, strlen (name));
		text = g_strdup_printf
			("<span size=\"medium\"><b>%s</b></span>", escaped);
		g_free (escaped);

		widget = glade_xml_get_widget (totem->xml, "tcw_title_label");
		gtk_label_set_markup (GTK_LABEL (widget), text);

		g_free (text);

		/* Title */
		gtk_window_set_title (GTK_WINDOW (totem->win), name);
	} else {
		totem_statusbar_set_time_and_length (TOTEM_STATUSBAR
				(totem->statusbar), 0, 0);
		totem_statusbar_set_text (TOTEM_STATUSBAR (totem->statusbar),
				_("Stopped"));

		update_skip_to (totem, 0);

		/* Update the mrl label */
		text = g_strdup_printf
			("<span size=\"medium\"><b>%s</b></span>",
			 _("No File"));
		widget = glade_xml_get_widget (totem->xml, "tcw_title_label");
		gtk_label_set_markup (GTK_LABEL (widget), text);

		g_free (text);

		/* Title */
		gtk_window_set_title (GTK_WINDOW (totem->win), _("Totem Movie Player"));
	}
}

gboolean
totem_action_set_mrl_with_warning (Totem *totem, const char *mrl,
		gboolean warn)
{
	gboolean retval = TRUE;

	if (totem->mrl != NULL)
	{
		g_free (totem->mrl);
		totem->mrl = NULL;
		bacon_video_widget_close (totem->bvw);
	}

	/* Reset the properties and wait for the signal*/
	bacon_video_widget_properties_reset
		(BACON_VIDEO_WIDGET_PROPERTIES (totem->properties));

	if (mrl == NULL)
	{
		retval = FALSE;

		/* Play/Pause */
		totem_action_set_sensitivity ("play", FALSE);

		/* Seek bar and seek buttons */
		update_seekable (totem, FALSE);

		/* Volume */
		totem_main_set_sensitivity ("tcw_volume_button", FALSE);
		totem_action_set_sensitivity ("volume-up", FALSE);
		totem_action_set_sensitivity ("volume-down", FALSE);
		totem->volume_sensitive = FALSE;

		/* Control popup */
		gtk_widget_set_sensitive (totem->fs_seek, FALSE);
		totem_action_set_sensitivity ("next-chapter", FALSE);
		totem_action_set_sensitivity ("previous-chapter", FALSE);
		totem_main_set_sensitivity ("tcw_volume_hbox", FALSE);

		/* Take a screenshot */
		totem_action_set_sensitivity ("take-screenshot", FALSE);

		/* Clear the playlist */
		totem_action_set_sensitivity ("clear-playlist", FALSE);

		/* Set the logo */
		bacon_video_widget_set_logo_mode (totem->bvw, TRUE);
		gtk_widget_set_sensitive (GTK_WIDGET (totem->properties),
				FALSE);
		update_mrl_label (totem, NULL);
	} else {
		gboolean caps;
		char *subtitle_uri;
		GError *err = NULL;

		bacon_video_widget_set_logo_mode (totem->bvw, FALSE);

		subtitle_uri = totem_uri_get_subtitle_uri (mrl);
		totem_gdk_window_set_waiting_cursor (totem->win->window);
		retval = bacon_video_widget_open_with_subtitle (totem->bvw,
				mrl, subtitle_uri, &err);
		gdk_window_set_cursor (totem->win->window, NULL);
		totem->mrl = g_strdup (mrl);

		/* Play/Pause */
		totem_action_set_sensitivity ("play", TRUE);

		/* Seek bar */
		update_seekable (totem,
				bacon_video_widget_is_seekable (totem->bvw));

		/* Volume */
		caps = bacon_video_widget_can_set_volume (totem->bvw);
		totem_main_set_sensitivity ("tcw_volume_button", caps);
		totem_main_set_sensitivity ("tcw_volume_hbox", caps);
		totem_action_set_sensitivity ("volume-up", caps && totem->prev_volume < 100);
		totem_action_set_sensitivity ("volume-down", caps && totem->prev_volume > 0);
		totem->volume_sensitive = caps;

		/* Take a screenshot */
		totem_action_set_sensitivity ("take-screenshot", retval);

		/* Clear the playlist */
		totem_action_set_sensitivity ("clear-playlist", retval);

		gtk_widget_set_sensitive
			(GTK_WIDGET (totem->properties), retval);

		/* Set the playlist */
		totem_playlist_set_playing (totem->playlist, retval);

		if (retval == FALSE && warn != FALSE)
		{
			char *msg, *disp;

			disp = totem_uri_escape_for_display (totem->mrl);
			msg = g_strdup_printf(_("Totem could not play '%s'."), disp);
			g_free (disp);
			if (err && err->message) {
				totem_action_error (msg, err->message, totem);
			}
			else {
				totem_action_error (msg, _("No error message"), totem);
			}
			g_free (msg);
		}

		if (retval == FALSE)
		{
			if (err) {
				g_error_free (err);
			}
			g_free (totem->mrl);
			totem->mrl = NULL;
			play_pause_set_label (totem, STATE_STOPPED);
			bacon_video_widget_set_logo_mode (totem->bvw, TRUE);
		}
	}
	update_buttons (totem);
	update_media_menu_items (totem);

	return retval;
}

gboolean
totem_action_set_mrl (Totem *totem, const char *mrl)
{
	return totem_action_set_mrl_with_warning (totem, mrl, TRUE);
}

static gboolean
totem_time_within_seconds (Totem *totem)
{
	gint64 time;

	time = bacon_video_widget_get_current_time (totem->bvw);

	return (time < REWIND_OR_PREVIOUS);
}

static void
totem_action_direction (Totem *totem, TotemPlaylistDirection dir)
{
	if (totem_playing_dvd (totem->mrl) == FALSE &&
		totem_playlist_has_direction (totem->playlist, dir) == FALSE
		&& totem_playlist_get_repeat (totem->playlist) == FALSE)
		return;

	if (totem_playing_dvd (totem->mrl) != FALSE)
	{
		bacon_video_widget_dvd_event (totem->bvw,
				dir == TOTEM_PLAYLIST_DIRECTION_NEXT ?
				BVW_DVD_NEXT_CHAPTER :
				BVW_DVD_PREV_CHAPTER);
		return;
	}
	
	if (dir == TOTEM_PLAYLIST_DIRECTION_NEXT
			|| bacon_video_widget_is_seekable (totem->bvw) == FALSE
			|| totem_time_within_seconds (totem) != FALSE)
	{
		char *mrl;

		totem_playlist_set_direction (totem->playlist, dir);
		mrl = totem_playlist_get_current_mrl (totem->playlist);
		totem_action_set_mrl_and_play (totem, mrl);
		g_free (mrl);
	} else {
		totem_action_seek (totem, 0);
	}
}

void
totem_action_previous (Totem *totem)
{
	totem_action_direction (totem, TOTEM_PLAYLIST_DIRECTION_PREVIOUS);
}

void
totem_action_next (Totem *totem)
{
	totem_action_direction (totem, TOTEM_PLAYLIST_DIRECTION_NEXT);
}

void
totem_action_seek_relative (Totem *totem, int off_sec)
{
	GError *err = NULL;
	gint64 off_msec, oldsec, sec;

	if (totem->mrl == NULL)
		return;
	if (bacon_video_widget_is_seekable (totem->bvw) == FALSE)
		return;

	off_msec = off_sec * 1000;
	oldsec = bacon_video_widget_get_current_time (totem->bvw);
	sec = MAX (0, oldsec + off_msec);

	bacon_video_widget_seek_time (totem->bvw, sec, &err);

	if (err != NULL)
	{
		char *msg, *disp;

		disp = totem_uri_escape_for_display (totem->mrl);
		msg = g_strdup_printf(_("Totem could not play '%s'."), totem->mrl);
		g_free (disp);

		totem_playlist_set_playing (totem->playlist, FALSE);
		totem_action_stop (totem);
		totem_action_error (msg, err->message, totem);
		g_free (msg);
		g_error_free (err);
	}
}

static void
totem_action_zoom (Totem *totem, int zoom)
{
	GtkAction *action;
	gboolean zoom_reset, zoom_in, zoom_out;

	if (zoom == ZOOM_ENABLE)
		zoom = bacon_video_widget_get_zoom (totem->bvw);

	if (zoom == ZOOM_DISABLE) {
		zoom_reset = zoom_in = zoom_out = FALSE;
	} else if (zoom < ZOOM_LOWER || zoom > ZOOM_UPPER) {
		return;
	} else {
		bacon_video_widget_set_zoom (totem->bvw, zoom);
		zoom_reset = (zoom != ZOOM_RESET);
		zoom_out = zoom != ZOOM_LOWER;
		zoom_in = zoom != ZOOM_UPPER;
	}

	action = gtk_action_group_get_action (totem->zoom_action_group,
			"zoom-in");
	gtk_action_set_sensitive (action, zoom_in);

	action = gtk_action_group_get_action (totem->zoom_action_group,
			"zoom-out");
	gtk_action_set_sensitive (action, zoom_out);

	action = gtk_action_group_get_action (totem->zoom_action_group,
			"zoom-reset");
	gtk_action_set_sensitive (action, zoom_reset);
}

void
totem_action_zoom_relative (Totem *totem, int off_pct)
{
	int zoom;

	zoom = bacon_video_widget_get_zoom (totem->bvw);
	totem_action_zoom (totem, zoom + off_pct);
}

void
totem_action_zoom_reset (Totem *totem)
{
	totem_action_zoom (totem, 100);
}

void
totem_action_volume_relative (Totem *totem, int off_pct)
{
	int vol;

	if (bacon_video_widget_can_set_volume (totem->bvw) == FALSE)
		return;

	vol = bacon_video_widget_get_volume (totem->bvw);
	bacon_video_widget_set_volume (totem->bvw, vol + off_pct);
}

void
totem_action_toggle_aspect_ratio (Totem *totem)
{		
	GtkAction *action;
	int tmp;

	tmp = totem_action_get_aspect_ratio (totem);
	tmp++;
	if (tmp > 4)
		tmp = 0;

	action = gtk_action_group_get_action (totem->main_action_group, "aspect-ratio-auto");
	gtk_radio_action_set_current_value (GTK_RADIO_ACTION (action), tmp);
}

void
totem_action_set_aspect_ratio (Totem *totem, int  ratio)
{
	bacon_video_widget_set_aspect_ratio (totem->bvw, ratio);
}

int
totem_action_get_aspect_ratio (Totem *totem)
{
	return (bacon_video_widget_get_aspect_ratio (totem->bvw));
}

void
totem_action_set_scale_ratio (Totem *totem, gfloat ratio)
{
	bacon_video_widget_set_scale_ratio (totem->bvw, ratio);
}

void
totem_action_show_help (Totem *totem)
{
#ifndef HAVE_GTK_ONLY
	GError *err = NULL;

	if (gnome_help_display ("totem.xml", NULL, &err) == FALSE)
	{
		totem_action_error (_("Totem could not display the help contents."), err->message, totem);
		g_error_free (err);
	}
#endif /* !HAVE_GTK_ONLY */
}

static gboolean
totem_action_drop_files (Totem *totem, GtkSelectionData *data,
		int drop_type, gboolean empty_pl)
{
	GList *list, *p, *file_list;
	gboolean cleared = FALSE;

	list = gnome_vfs_uri_list_parse ((const char *)data->data);

	if (list == NULL)
		return FALSE;

	p = list;
	file_list = NULL;

	while (p != NULL)
	{
		file_list = g_list_prepend (file_list, 
				gnome_vfs_uri_to_string
				((const GnomeVFSURI*)(p->data), 0));
		p = p->next;
	}

	gnome_vfs_uri_list_free (list);
	if (file_list == NULL)
		return FALSE;

	if (drop_type != 1)
		file_list = g_list_sort (file_list, (GCompareFunc) strcmp);
	else
		file_list = g_list_reverse (file_list);

	for (p = file_list; p != NULL; p = p->next)
	{
		char *filename, *title;

		if (p->data == NULL)
			continue;

		filename = totem_create_full_path (p->data);
		title = NULL;

		if (empty_pl != FALSE && cleared == FALSE)
		{
			/* The function that calls us knows better
			 * if we should be doing something with the 
			 * changed playlist ... */
			g_signal_handlers_disconnect_by_func
				(G_OBJECT (totem->playlist),
				 playlist_changed_cb, totem);
			totem_playlist_clear (totem->playlist);
			cleared = TRUE;
		}

		/* Super _NETSCAPE_URL trick */
		if (drop_type == 1)
		{
			g_free (p->data);
			p = p->next;
			if (p != NULL) {
				if (g_str_has_prefix (p->data, "file:") != FALSE)
					title = (char *)p->data + 5;
				else
					title = p->data;
			}
		}

		totem_playlist_add_mrl (totem->playlist, filename, title);

		g_free (filename);
		g_free (p->data);
	}

	g_list_free (file_list);

	/* ... and reconnect because we're nice people */
	if (cleared != FALSE)
	{
		char *mrl;

		g_signal_connect (G_OBJECT (totem->playlist),
				"changed", G_CALLBACK (playlist_changed_cb),
				totem);
		mrl = totem_playlist_get_current_mrl (totem->playlist);
		totem_action_set_mrl_and_play (totem, mrl);
		g_free (mrl);
	}

	return TRUE;
}

static void
drop_video_cb (GtkWidget     *widget,
	 GdkDragContext     *context,
	 gint                x,
	 gint                y,
	 GtkSelectionData   *data,
	 guint               info,
	 guint               time,
	 Totem              *totem)
{
	gboolean retval;

	retval = totem_action_drop_files (totem, data, info, TRUE);
	gtk_drag_finish (context, retval, FALSE, time);
}

static void
drop_playlist_cb (GtkWidget     *widget,
	       GdkDragContext     *context,
	       gint                x,
	       gint                y,
	       GtkSelectionData   *data,
	       guint               info,
	       guint               time,
	       Totem              *totem)
{
	gboolean retval;

	retval = totem_action_drop_files (totem, data, info, FALSE);
	gtk_drag_finish (context, retval, FALSE, time);
}

static void
drag_video_cb (GtkWidget *widget,
		GdkDragContext *context,
		GtkSelectionData *selection_data,
		guint info,
		guint32 time,
		gpointer callback_data)
{
	Totem *totem = (Totem *) callback_data;
	char *text;
	int len;

	g_assert (selection_data != NULL);

	if (totem->mrl == NULL)
		return;

	if (totem->mrl[0] == '/')
		text = gnome_vfs_get_uri_from_local_path (totem->mrl);
	else
		text = g_strdup (totem->mrl);

	g_return_if_fail (text != NULL);

	len = strlen (text);

	gtk_selection_data_set (selection_data,
			selection_data->target,
			8, (guchar *) text, len);

	g_free (text);
}

static void
on_got_redirect (BaconVideoWidget *bvw, const char *mrl, Totem *totem)
{
	gchar *old_mrl, *new_mrl;

	old_mrl = totem_playlist_get_current_mrl (TOTEM_PLAYLIST (totem->playlist));
	new_mrl = totem_resolve_relative_link (old_mrl, mrl);
	g_free (old_mrl);

	bacon_video_widget_close (totem->bvw);
	totem_gdk_window_set_waiting_cursor (totem->win->window);
	bacon_video_widget_open (totem->bvw, new_mrl, NULL);
	gdk_window_set_cursor (totem->win->window, NULL);
	bacon_video_widget_play (bvw, NULL);
	g_free (new_mrl);
}

/* This is only called when we are playing a DVD */
static void
on_title_change_event (BaconVideoWidget *bvw, const char *string, Totem *totem)
{
	update_mrl_label (totem, string);
	update_buttons (totem);
	totem_playlist_set_title (TOTEM_PLAYLIST (totem->playlist),
			string, TRUE);
}

static void
on_channels_change_event (BaconVideoWidget *bvw, Totem *totem)
{
	gchar *name;

	totem_sublang_update (totem);

	/* updated stream info (new song) */
	name = totem_get_nice_name_for_stream (totem);

	bacon_video_widget_properties_update
		(BACON_VIDEO_WIDGET_PROPERTIES (totem->properties),
		 totem->bvw);

	if (name != NULL) {
		update_mrl_label (totem, name);
		totem_playlist_set_title
			(TOTEM_PLAYLIST (totem->playlist), name, TRUE);
		g_free (name);
	}
}

static void
on_playlist_change_name (TotemPlaylist *playlist, Totem *totem)
{
	char *name, *artist, *album, *title;
	gboolean cur;

	if ((name = totem_playlist_get_current_title (playlist,
						      &cur)) != NULL) {
		update_mrl_label (totem, name);
		g_free (name);
	}

	if (totem_playlist_get_current_metadata (playlist, &artist,
						 &title, &album) != FALSE) {
		bacon_video_widget_properties_from_metadata (
			BACON_VIDEO_WIDGET_PROPERTIES (totem->properties),
			artist, title, album);

		g_free (artist);
		g_free (album);
		g_free (title);
	}
}

static void
on_got_metadata_event (BaconVideoWidget *bvw, Totem *totem)
{
        char *name = NULL;

	bacon_video_widget_properties_update
		(BACON_VIDEO_WIDGET_PROPERTIES (totem->properties),
		 totem->bvw);

	name = totem_get_nice_name_for_stream (totem);

	if (name != NULL) {
		totem_playlist_set_title
			(TOTEM_PLAYLIST (totem->playlist), name, FALSE);
		g_free (name);
	}
	on_playlist_change_name 	 
		(TOTEM_PLAYLIST (totem->playlist), totem);
}

static void
on_error_event (BaconVideoWidget *bvw, char *message,
                gboolean playback_stopped, gboolean fatal, Totem *totem)
{
	if (playback_stopped)
		play_pause_set_label (totem, STATE_STOPPED);

	if (fatal == FALSE) {
		totem_action_error (_("An error occurred"), message, totem);
	} else {
		totem_action_error_and_exit (_("An error occurred"),
				message, totem);
	}
}

static void
on_buffering_event (BaconVideoWidget *bvw, int percentage, Totem *totem)
{
	totem_statusbar_push (TOTEM_STATUSBAR (totem->statusbar), percentage);
}

static void
update_seekable (Totem *totem, gboolean seekable)
{
	if (totem->seekable == seekable)
		return;

	totem->seekable = seekable;

	/* Check if the stream is seekable */
	gtk_widget_set_sensitive (totem->seek, seekable);
	gtk_widget_set_sensitive (totem->fs_seek, seekable);

	totem_main_set_sensitivity ("tmw_seek_hbox", seekable);

	totem_main_set_sensitivity ("tcw_time_hbox", seekable);

	totem_action_set_sensitivity ("skip-forward", seekable);
	totem_action_set_sensitivity ("skip-backwards", seekable);
	totem_action_set_sensitivity ("skip-to", seekable);
	if (totem->skipto)
		totem_skipto_set_seekable (totem->skipto, seekable);
}

static void
update_current_time (BaconVideoWidget *bvw,
		gint64 current_time,
		gint64 stream_length,
		float current_position,
		gboolean seekable, Totem *totem)
{
	update_skip_to (totem, stream_length);
	update_seekable (totem, seekable);

	if (totem->seek_lock == FALSE)
	{
		gtk_adjustment_set_value (totem->seekadj,
				current_position * 65535);
		gtk_adjustment_set_value (totem->fs_seekadj,
				current_position * 65535);

		if (stream_length == 0 && totem->mrl != NULL)
		{
			totem_statusbar_set_time_and_length
				(TOTEM_STATUSBAR (totem->statusbar),
				(int) (current_time / 1000), -1);
		} else {
			totem_statusbar_set_time_and_length
				(TOTEM_STATUSBAR (totem->statusbar),
				(int) (current_time / 1000),
				(int) (stream_length / 1000));
		}

		totem_time_label_set_time
			(TOTEM_TIME_LABEL (totem->tcw_time_label),
			 current_time, stream_length);
		bacon_video_widget_properties_from_time
			(BACON_VIDEO_WIDGET_PROPERTIES (totem->properties),
			 stream_length);
	}
}

static gboolean
vol_slider_pressed_cb (GtkWidget *widget, GdkEventButton *event, Totem *totem)
{
	totem->vol_fs_lock = TRUE;
	return FALSE;
}

static gboolean
vol_slider_released_cb (GtkWidget *widget, GdkEventButton *event, Totem *totem)
{
	totem->vol_fs_lock = FALSE;
	return FALSE;
}

static void
update_volume_sliders (Totem *totem)
{
	int volume;
	GtkAction *action;

	volume = bacon_video_widget_get_volume (totem->bvw);

	if (totem->volume_first_time || (totem->prev_volume != volume &&
				totem->prev_volume != -1 && volume != -1))
	{
		totem->volume_first_time = 0;
		bacon_volume_button_set_value (
			BACON_VOLUME_BUTTON (totem->volume), (float) volume);
		gtk_adjustment_set_value (totem->fs_voladj,
				(float) volume);

		action = gtk_action_group_get_action (totem->main_action_group, "volume-down");
		gtk_action_set_sensitive (action, volume > 0 && totem->volume_sensitive);
		
		action = gtk_action_group_get_action (totem->main_action_group, "volume-up");
		gtk_action_set_sensitive (action, volume < 100 && totem->volume_sensitive);
	}

	totem->prev_volume = volume;
}

static void
property_notify_cb (BaconVideoWidget *bvw, GParamSpec *spec, Totem *totem)
{
	if (strcmp ("volume", spec->name) == 0) {
		update_volume_sliders (totem);
	} else if (strcmp ("logo-mode", spec->name) == 0) {
		gboolean enabled;
		enabled = bacon_video_widget_get_logo_mode (totem->bvw);
		totem_action_zoom (totem, enabled ? ZOOM_DISABLE : ZOOM_ENABLE);
	}
}

static gboolean
seek_slider_pressed_cb (GtkWidget *widget, GdkEventButton *event, Totem *totem)
{
	totem->seek_lock = TRUE;
	totem_statusbar_set_seeking (TOTEM_STATUSBAR (totem->statusbar), TRUE);
	totem_time_label_set_seeking (TOTEM_TIME_LABEL (totem->tcw_time_label), TRUE);

	return FALSE;
}

static void
seek_slider_changed_cb (GtkAdjustment *adj, Totem *totem)
{
	double pos;
	gint time;

	if (totem->seek_lock == FALSE)
		return;

	pos = gtk_adjustment_get_value (adj) / 65535;
	time = bacon_video_widget_get_stream_length (totem->bvw);
	totem_statusbar_set_time_and_length (TOTEM_STATUSBAR (totem->statusbar),
			(int) (pos * time / 1000), time / 1000);
	totem_time_label_set_time
			(TOTEM_TIME_LABEL (totem->tcw_time_label),
			 (int) (pos * time), time);

	if (bacon_video_widget_can_direct_seek (totem->bvw) != FALSE)
		totem_action_seek (totem, pos);
}

static gboolean
seek_slider_released_cb (GtkWidget *widget, GdkEventButton *event, Totem *totem)
{
	GtkAdjustment *adj, *other_adj;
	gdouble val;

	adj = gtk_range_get_adjustment (GTK_RANGE (widget));
	other_adj = (adj == totem->seekadj) ? totem->fs_seekadj : totem->seekadj;

	/* set to FALSE here to avoid triggering a final seek when
	 * syncing the adjustments while being in direct seek mode */
	totem->seek_lock = FALSE;

	/* sync both adjustments */
	val = gtk_adjustment_get_value (adj);
	gtk_adjustment_set_value (other_adj, val);

	if (bacon_video_widget_can_direct_seek (totem->bvw) == FALSE)
	{
		totem_action_seek (totem, val / 65535.0);
	}

	totem_statusbar_set_seeking (TOTEM_STATUSBAR (totem->statusbar), FALSE);
	totem_time_label_set_seeking (TOTEM_TIME_LABEL (totem->tcw_time_label),
			FALSE);
	return FALSE;
}

static void
vol_cb (GtkWidget *widget, Totem *totem)
{
	if (totem->vol_lock == FALSE)
	{
		totem->vol_lock = TRUE;

		if (GPOINTER_TO_INT (g_object_get_data (G_OBJECT (widget), "fs")) != FALSE)
		{
			bacon_video_widget_set_volume
				(totem->bvw, (gint) totem->fs_voladj->value);

			/* Update the fullscreen volume adjustment */
			bacon_volume_button_set_value (
				BACON_VOLUME_BUTTON (totem->volume),
				gtk_adjustment_get_value (totem->fs_voladj));
		} else {
			int value = bacon_volume_button_get_value (
				BACON_VOLUME_BUTTON (totem->volume));

			bacon_video_widget_set_volume (totem->bvw, value);
			/* Update the volume adjustment */
			gtk_adjustment_set_value (totem->fs_voladj, value);
		}

		totem->vol_lock = FALSE;
	}
}

static gboolean
totem_action_open_files (Totem *totem, char **list)
{
	GSList *slist = NULL;
	int i, retval;

	for (i = 0 ; list[i] != NULL; i++)
		slist = g_slist_prepend (slist, list[i]);

	slist = g_slist_reverse (slist);
	retval = totem_action_open_files_list (totem, slist);
	g_slist_free (slist);

	return retval;
}

static gboolean
totem_action_open_files_list (Totem *totem, GSList *list)
{
	GSList *l;
	gboolean changed;
	gboolean cleared;

	changed = FALSE;
	cleared = FALSE;

	if (list == NULL)
		return changed;

	for (l = list ; l != NULL; l = l->next)
	{
		char *filename;
		char *data = l->data;

		if (data == NULL)
			continue;

		/* Ignore relatives paths that start with "--", tough luck */
		if (data[0] == '-' && data[1] == '-')
			continue;

		/* Get the subtitle part out for our tests */
		filename = totem_create_full_path (data);

		if (g_file_test (filename, G_FILE_TEST_IS_REGULAR)
				|| strstr (filename, "#") != NULL
				|| strstr (filename, "://") != NULL
				|| g_str_has_prefix (filename, "dvd:") != FALSE
				|| g_str_has_prefix (filename, "vcd:") != FALSE
				|| g_str_has_prefix (filename, "cdda:") != FALSE
				|| g_str_has_prefix (filename, "cd:") != FALSE)
		{
			if (cleared == FALSE)
			{
				/* The function that calls us knows better
				 * if we should be doing something with the 
				 * changed playlist ... */
				g_signal_handlers_disconnect_by_func
					(G_OBJECT (totem->playlist),
					 playlist_changed_cb, totem);
				changed = totem_playlist_clear (totem->playlist);
				bacon_video_widget_close (totem->bvw);
				cleared = TRUE;
			}

			if (totem_is_block_device (filename) != FALSE) {
				totem_action_load_media_device (totem, data);
				changed = TRUE;
			} else if (g_str_has_prefix (filename, "cdda:/") != FALSE) {
				totem_playlist_add_mrl (totem->playlist, data, NULL);
				changed = TRUE;
			} else if (totem_playlist_add_mrl (totem->playlist,
						filename, NULL) != FALSE) {
				totem_action_add_recent (totem, filename);
				changed = TRUE;
			}
		}

		g_free (filename);
	}

	/* ... and reconnect because we're nice people */
	if (cleared != FALSE)
	{
		g_signal_connect (G_OBJECT (totem->playlist),
				"changed", G_CALLBACK (playlist_changed_cb),
				totem);
	}

	return changed;
}

static void
commit_hide_skip_to (GtkDialog *dialog, gint response, Totem *totem)
{
	GError *err = NULL;

	if (response != GTK_RESPONSE_OK)
	{
		gtk_widget_destroy (GTK_WIDGET (totem->skipto));
		return;
	}

	gtk_widget_hide (GTK_WIDGET (dialog));

	bacon_video_widget_seek_time (totem->bvw,
			totem_skipto_get_range (totem->skipto), &err);

	gtk_widget_destroy (GTK_WIDGET (totem->skipto));

	if (err != NULL)
	{
		char *msg, *disp;

		disp = totem_uri_escape_for_display (totem->mrl);
		msg = g_strdup_printf(_("Totem could not seek in '%s'."), disp);
		g_free (disp);
		totem_action_stop (totem);
		totem_playlist_set_playing (totem->playlist, FALSE);
		totem_action_error (msg, err->message, totem);
		g_free (msg);
		g_error_free (err);
	}
}

void
totem_action_skip_to (Totem *totem)
{
	char *filename;
	TotemSkipto **skipto;

	if (totem->seekable == FALSE)
		return;

	if (totem->skipto != NULL)
	{
		gtk_window_present (GTK_WINDOW (totem->skipto));
		return;
	}

	filename = totem_interface_get_full_path ("skip_to.glade");
	totem->skipto = TOTEM_SKIPTO (totem_skipto_new (filename));
	g_free (filename);

	g_signal_connect (G_OBJECT (totem->skipto), "delete-event",
			G_CALLBACK (gtk_widget_destroy), NULL);
	g_signal_connect (G_OBJECT (totem->skipto), "response",
			G_CALLBACK (commit_hide_skip_to), totem);

	skipto = &totem->skipto;
	g_object_add_weak_pointer (G_OBJECT (totem->skipto),
				   (gpointer *) skipto);

	gtk_window_set_transient_for (GTK_WINDOW (totem->skipto),
			GTK_WINDOW (totem->win));
	gtk_widget_show (GTK_WIDGET (totem->skipto));
}

static void
on_fs_exit1_activate (GtkButton *button, Totem *totem)
{
	totem_action_fullscreen_toggle (totem);
}

void
show_controls (Totem *totem, gboolean was_fullscreen)
{
	GtkAction *action;
	GtkWidget *menubar, *controlbar, *statusbar, *bvw_vbox, *widget;
	int width = 0, height = 0;

	if (totem->bvw == NULL)
		return;

	menubar = glade_xml_get_widget (totem->xml, "tmw_menubar_box");
	controlbar = glade_xml_get_widget (totem->xml, "tmw_controls_vbox");
	statusbar = glade_xml_get_widget (totem->xml, "tmw_statusbar");
	bvw_vbox = glade_xml_get_widget (totem->xml, "tmw_bvw_vbox");
	widget = GTK_WIDGET (totem->bvw);

	action = gtk_action_group_get_action (totem->main_action_group, "show-controls");
	gtk_action_set_sensitive (action, !totem_is_fullscreen (totem));

	if (totem->controls_visibility == TOTEM_CONTROLS_VISIBLE)
	{
		if (was_fullscreen == FALSE)
		{
			height = widget->allocation.height;
			width =	widget->allocation.width;
		}

		gtk_widget_set_sensitive (menubar, TRUE);
		gtk_widget_show (menubar);
		gtk_widget_show (controlbar);
		gtk_widget_show (statusbar);
		if (totem_sidebar_is_visible (totem) != FALSE) {
			/* This is uglier then you might expect because of the
			   resize handle between the video and sidebar. There
			   is no convenience method to get the handle's width.
			   */
			GValue value = { 0, };
			GtkWidget *pane;
			int handle_size;

			g_value_init (&value, G_TYPE_INT);
			pane = glade_xml_get_widget (totem->xml,
					"tmw_main_pane");
			gtk_widget_style_get_property (pane, "handle-size",
					&value);
			handle_size = g_value_get_int (&value);
			
			gtk_widget_show (totem->sidebar);
			width += totem->sidebar->allocation.width
				+ handle_size;
		} else {
			gtk_widget_hide (totem->sidebar);
		}

		gtk_container_set_border_width (GTK_CONTAINER (bvw_vbox),
				BVW_VBOX_BORDER_WIDTH);

		if (was_fullscreen == FALSE)
		{
			height += menubar->allocation.height
				+ controlbar->allocation.height
				+ statusbar->allocation.height
				+ 2 * BVW_VBOX_BORDER_WIDTH;
			width += 2 * BVW_VBOX_BORDER_WIDTH;
			gtk_window_resize (GTK_WINDOW(totem->win),
					width, height);
		}
	} else {
		if (totem->controls_visibility == TOTEM_CONTROLS_HIDDEN)
		{
			width = widget->allocation.width;
			height = widget->allocation.height;
		}

		/* Hide and make the menubar unsensitive */
		gtk_widget_set_sensitive (menubar, FALSE);
		gtk_widget_hide (menubar);

		gtk_widget_hide (controlbar);
		gtk_widget_hide (statusbar);
		gtk_widget_hide (totem->sidebar);

		 /* We won't show controls in fullscreen */
		gtk_container_set_border_width (GTK_CONTAINER (bvw_vbox), 0);

		if (totem->controls_visibility == TOTEM_CONTROLS_HIDDEN)
		{
			gtk_window_resize (GTK_WINDOW(totem->win),
					width, height);
		}
	}
}

void
totem_action_toggle_controls (Totem *totem)
{
	GtkAction *action;
	gboolean state;

	if (totem_is_fullscreen (totem) != FALSE)
		return;

 	action = gtk_action_group_get_action (totem->main_action_group,
 		"show-controls");
 	state = gtk_toggle_action_get_active (GTK_TOGGLE_ACTION (action));
 	gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action), !state);
}

static void
on_volume_mute_button (GtkButton *button, Totem *totem)
{
	totem_action_volume_relative (totem, -100);
}

static void
on_volume_max_button (GtkButton *button, Totem *totem)
{
	totem_action_volume_relative (totem, 100);
}

static void
totem_action_remote (Totem *totem, TotemRemoteCommand cmd, const char *url)
{
	gboolean handled = TRUE;

	switch (cmd) {
	case TOTEM_REMOTE_COMMAND_PLAY:
		totem_action_play (totem);
		break;
	case TOTEM_REMOTE_COMMAND_PLAYPAUSE:
		totem_action_play_pause (totem);
		break;
	case TOTEM_REMOTE_COMMAND_PAUSE:
		totem_action_pause (totem);
		break;
	case TOTEM_REMOTE_COMMAND_SEEK_FORWARD:
		totem_action_seek_relative (totem, SEEK_FORWARD_OFFSET);
		break;
	case TOTEM_REMOTE_COMMAND_SEEK_BACKWARD:
		totem_action_seek_relative (totem,
				SEEK_BACKWARD_OFFSET);
		break;
	case TOTEM_REMOTE_COMMAND_VOLUME_UP:
		totem_action_volume_relative (totem, VOLUME_UP_OFFSET);
		break;
	case TOTEM_REMOTE_COMMAND_VOLUME_DOWN:
		totem_action_volume_relative (totem, VOLUME_DOWN_OFFSET);
		break;
	case TOTEM_REMOTE_COMMAND_NEXT:
		totem_action_next (totem);
		break;
	case TOTEM_REMOTE_COMMAND_PREVIOUS:
		totem_action_previous (totem);
		break;
	case TOTEM_REMOTE_COMMAND_FULLSCREEN:
		totem_action_fullscreen_toggle (totem);
		break;
	case TOTEM_REMOTE_COMMAND_QUIT:
		totem_action_exit (totem);
		break;
	case TOTEM_REMOTE_COMMAND_ENQUEUE:
		g_assert (url != NULL);
		if (totem_playlist_add_mrl (totem->playlist, url, NULL) != FALSE) {
			totem_action_add_recent (totem, url);
		}
		break;
	case TOTEM_REMOTE_COMMAND_REPLACE:
		g_assert (url != NULL);
		totem_playlist_clear (totem->playlist);
		if (strcmp (url, "dvd:") == 0) {
			totem_action_play_media (totem, MEDIA_TYPE_DVD);
		} else if (strcmp (url, "vcd:") == 0) {
			totem_action_play_media (totem, MEDIA_TYPE_VCD);
		} else if (g_str_has_prefix (url, "cd:") != FALSE) {
			totem_action_play_media (totem, MEDIA_TYPE_CDDA);
		} else if (g_str_has_prefix (url, "cdda:/") != FALSE) {
			totem_playlist_add_mrl (totem->playlist, url, NULL);
		} else if (totem_playlist_add_mrl (totem->playlist,
					url, NULL) != FALSE) {
			totem_action_add_recent (totem, url);
		}
		break;
	case TOTEM_REMOTE_COMMAND_SHOW:
		gtk_window_present (GTK_WINDOW (totem->win));
		break;
	case TOTEM_REMOTE_COMMAND_TOGGLE_CONTROLS:
		if (totem->controls_visibility != TOTEM_CONTROLS_FULLSCREEN)
		{
			GtkToggleAction *action;
			gboolean state;

			action = GTK_TOGGLE_ACTION (gtk_action_group_get_action
					(totem->main_action_group,
					 "show-controls"));
			state = gtk_toggle_action_get_active (action);
			gtk_toggle_action_set_active (action, !state);
		}
		break;
	case TOTEM_REMOTE_COMMAND_SHOW_PLAYING:
		{
			char *title;
			gboolean custom;

			title = totem_playlist_get_current_title
				(totem->playlist, &custom);
			bacon_message_connection_send (totem->conn,
					title ? title : SHOW_PLAYING_NO_TRACKS);
			g_free (title);
		}
		break;
	case TOTEM_REMOTE_COMMAND_UP:
		bacon_video_widget_dvd_event (totem->bvw,
				BVW_DVD_ROOT_MENU_UP);
		break;
	case TOTEM_REMOTE_COMMAND_DOWN:
		bacon_video_widget_dvd_event (totem->bvw,
				BVW_DVD_ROOT_MENU_DOWN);
		break;
	case TOTEM_REMOTE_COMMAND_LEFT:
		bacon_video_widget_dvd_event (totem->bvw,
				BVW_DVD_ROOT_MENU_LEFT);
		break;
	case TOTEM_REMOTE_COMMAND_RIGHT:
		bacon_video_widget_dvd_event (totem->bvw,
				BVW_DVD_ROOT_MENU_RIGHT);
		break;
	case TOTEM_REMOTE_COMMAND_SELECT:
		bacon_video_widget_dvd_event (totem->bvw,
				BVW_DVD_ROOT_MENU_SELECT);
		break;
	case TOTEM_REMOTE_COMMAND_DVD_MENU:
		bacon_video_widget_dvd_event (totem->bvw,
				BVW_DVD_ROOT_MENU);
		break;
	case TOTEM_REMOTE_COMMAND_ZOOM_UP:
		totem_action_zoom_relative (totem, ZOOM_IN_OFFSET);
		break;
	case TOTEM_REMOTE_COMMAND_ZOOM_DOWN:
		totem_action_zoom_relative (totem, ZOOM_OUT_OFFSET);
		break;
	case TOTEM_REMOTE_COMMAND_EJECT:
		totem_action_eject (totem);
		break;
	case TOTEM_REMOTE_COMMAND_PLAY_DVD:
		// TODO - how to see if can, and play the DVD (like the menu item)
		break;
	case TOTEM_REMOTE_COMMAND_MUTE:
		totem_action_volume_relative (totem, -100);
		break;
	default:
		handled = FALSE;
		break;
	}

	if (handled != FALSE &&
			gtk_window_is_active (GTK_WINDOW (totem->win))) {
		on_video_motion_notify_event (NULL, NULL, totem);
	}
}

#ifdef HAVE_REMOTE
static void
totem_button_pressed_remote_cb (TotemRemote *remote, TotemRemoteCommand cmd,
		Totem *totem)
{
	totem_action_remote (totem, cmd, NULL);
}
#endif /* HAVE_REMOTE */

static void
playlist_changed_cb (GtkWidget *playlist, Totem *totem)
{
	char *mrl;

	update_buttons (totem);
	mrl = totem_playlist_get_current_mrl (totem->playlist);

	if (mrl == NULL)
		return;

	if (totem->mrl == NULL
			|| (totem->mrl != NULL && mrl != NULL
			&& strcmp (totem->mrl, mrl) != 0))
	{
		totem_action_set_mrl_and_play (totem, mrl);
	} else if (totem->mrl != NULL) {
		totem_playlist_set_playing (totem->playlist, TRUE);
	}

	g_free (mrl);
}

static void
item_activated_cb (GtkWidget *playlist, Totem *totem)
{
	totem_action_seek (totem, 0);
}

static void
current_removed_cb (GtkWidget *playlist, Totem *totem)
{
	char *mrl;

	/* Set play button status */
	play_pause_set_label (totem, STATE_STOPPED);
	mrl = totem_playlist_get_current_mrl (totem->playlist);

	if (mrl == NULL)
	{
		totem_playlist_set_at_start (totem->playlist);
		update_buttons (totem);
		mrl = totem_playlist_get_current_mrl (totem->playlist);
	} else {
		update_buttons (totem);
	}

	totem_action_set_mrl_and_play (totem, mrl);
	g_free (mrl);
}

static void
playlist_repeat_toggle_cb (TotemPlaylist *playlist, gboolean repeat, Totem *totem)
{
	GtkAction *action;

	action = gtk_action_group_get_action (totem->main_action_group, "repeat-mode");

	g_signal_handlers_block_matched (G_OBJECT (action), G_SIGNAL_MATCH_DATA, 0, 0,
			NULL, NULL, totem);

	gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action), repeat);

	g_signal_handlers_unblock_matched (G_OBJECT (action), G_SIGNAL_MATCH_DATA, 0, 0,
			NULL, NULL, totem);
}

static void
playlist_shuffle_toggle_cb (TotemPlaylist *playlist, gboolean shuffle, Totem *totem)
{
	GtkAction *action;

	action = gtk_action_group_get_action (totem->main_action_group, "shuffle-mode");

	g_signal_handlers_block_matched (G_OBJECT (action), G_SIGNAL_MATCH_DATA, 0, 0,
			NULL, NULL, totem);

	gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action), shuffle);

	g_signal_handlers_unblock_matched (G_OBJECT (action), G_SIGNAL_MATCH_DATA, 0, 0,
			NULL, NULL, totem);
}

static void
update_fullscreen_size (Totem *totem)
{
	GdkScreen *screen;

	screen = gtk_window_get_screen (GTK_WINDOW (totem->win));
	gdk_screen_get_monitor_geometry (screen,
			gdk_screen_get_monitor_at_window
			(screen, totem->win->window),
			&totem->fullscreen_rect);
}

gboolean
totem_is_fullscreen (Totem *totem)
{
	return (totem->controls_visibility == TOTEM_CONTROLS_FULLSCREEN);
}

static void
move_popups (Totem *totem)
{
	int control_width, control_height;
	int exit_width, exit_height;

	update_fullscreen_size (totem);

	gtk_window_get_size (GTK_WINDOW (totem->control_popup),
			&control_width, &control_height);
	gtk_window_get_size (GTK_WINDOW (totem->exit_popup),
			&exit_width, &exit_height);

	/* We take the full width of the screen */
	gtk_window_resize (GTK_WINDOW (totem->control_popup),
			totem->fullscreen_rect.width,
			control_height);

	if (gtk_widget_get_direction (totem->exit_popup) == GTK_TEXT_DIR_RTL)
	{
		gtk_window_move (GTK_WINDOW (totem->exit_popup),
				totem->fullscreen_rect.x,
				totem->fullscreen_rect.y);
		gtk_window_move (GTK_WINDOW (totem->control_popup),
				totem->fullscreen_rect.width - control_width,
				totem->fullscreen_rect.height + totem->fullscreen_rect.y - control_height);
	} else {
		gtk_window_move (GTK_WINDOW (totem->exit_popup),
				totem->fullscreen_rect.width + totem->fullscreen_rect.x - exit_width,
				totem->fullscreen_rect.y);
		gtk_window_move (GTK_WINDOW (totem->control_popup),
				totem->fullscreen_rect.x,
				totem->fullscreen_rect.height + totem->fullscreen_rect.y - control_height);
	}
}

static void
size_changed_cb (GdkScreen *screen, Totem *totem)
{
	move_popups (totem);
}

static void
theme_changed_cb (GtkIconTheme *icon_theme, Totem *totem)
{
	move_popups (totem);
}

static void
window_realize_cb (GtkWidget *widget, Totem *totem)
{
	GdkScreen *screen;

	screen = gtk_widget_get_screen (widget);
	g_signal_connect (G_OBJECT (screen), "size-changed",
			G_CALLBACK (size_changed_cb), totem);
	g_signal_connect (G_OBJECT (gtk_icon_theme_get_for_screen (screen)),
			"changed", G_CALLBACK (theme_changed_cb), totem);
}

static void
popup_timeout_add(Totem* totem)
{
	totem->popup_timeout = g_timeout_add (FULLSCREEN_POPUP_TIMEOUT,
		(GSourceFunc) popup_hide, totem);
}

static void
popup_timeout_remove (Totem *totem)
{
	if (totem->popup_timeout != 0)
	{
		g_source_remove (totem->popup_timeout);
		totem->popup_timeout = 0;
	}
}

static gboolean
popup_hide (Totem *totem)
{
	if (totem->bvw == NULL || totem_is_fullscreen (totem) == FALSE)
		return TRUE;

	if (totem->seek_lock != FALSE || totem->vol_fs_lock != FALSE)
		return TRUE;

	gtk_widget_hide (GTK_WIDGET (totem->exit_popup));
	gtk_widget_hide (GTK_WIDGET (totem->control_popup));

	popup_timeout_remove (totem);

	totem_action_set_cursor (totem, FALSE);

	return FALSE;
}

static void
on_mouse_click_fullscreen (GtkWidget *widget, Totem *totem)
{
	popup_timeout_remove (totem);
	popup_timeout_add (totem);
}

static gboolean
on_video_motion_notify_event (GtkWidget *widget, GdkEventMotion *event,
		Totem *totem)
{
	GtkWidget *item;

	if (totem_is_fullscreen (totem) == FALSE) 
		return FALSE;

	if (totem->popup_in_progress != FALSE)
		return FALSE;

	totem->popup_in_progress = TRUE;

	popup_timeout_remove (totem);

	item = glade_xml_get_widget (totem->xml, "tcw_hbox");
	gtk_widget_show_all (item);
	gdk_flush ();

	move_popups (totem);

	gtk_widget_show_all (totem->exit_popup);
	gtk_widget_show_all (totem->control_popup);
	totem_action_set_cursor (totem, TRUE);

	popup_timeout_add (totem);
	totem->popup_in_progress = FALSE;

	return FALSE;
}

static gboolean
on_video_button_press_event (BaconVideoWidget *bvw, GdkEventButton *event,
		Totem *totem)
{
	if (event->type == GDK_2BUTTON_PRESS && event->button == 1) {
		totem_action_fullscreen_toggle(totem);
		return TRUE;
	} else if (event->type == GDK_BUTTON_PRESS && event->button == 2) {
		totem_action_play_pause(totem);
		return TRUE;
	} else if (event->type == GDK_BUTTON_PRESS && event->button == 3) {
		totem_action_menu_popup (totem, event->button);
		return TRUE;
	}

	return FALSE;
}

static gboolean
on_eos_event (GtkWidget *widget, Totem *totem)
{
	if (bacon_video_widget_get_logo_mode (totem->bvw) != FALSE)
		return FALSE;

	if (totem_playlist_has_next_mrl (totem->playlist) == FALSE
			&& totem_playlist_get_repeat (totem->playlist) == FALSE)
	{
		char *mrl;

		/* Set play button status */
		play_pause_set_label (totem, STATE_PAUSED);
		totem_playlist_set_at_start (totem->playlist);
		update_buttons (totem);
		mrl = totem_playlist_get_current_mrl (totem->playlist);
		totem_action_stop (totem);
		totem_action_set_mrl_with_warning (totem, mrl, FALSE);
		bacon_video_widget_pause (totem->bvw);
		g_free (mrl);
	} else {
		totem_action_next (totem);
	}

	return FALSE;
}

static gboolean
totem_action_handle_key_release (Totem *totem, GdkEventKey *event)
{
	gboolean retval = TRUE;

	switch (event->keyval) {
	case GDK_Left:
	case GDK_Right:
		totem_statusbar_set_seeking
			(TOTEM_STATUSBAR (totem->statusbar), FALSE);
		break;
	}

	return retval;
}

static void
totem_action_handle_seek (Totem *totem, GdkEventKey *event, gboolean is_forward)
{
	if (is_forward != FALSE) {
		if (event->state & GDK_SHIFT_MASK) {
			totem_action_seek_relative (totem,
					SEEK_FORWARD_SHORT_OFFSET);
		} else if (event->state & GDK_CONTROL_MASK) {
			totem_action_seek_relative (totem,
					SEEK_FORWARD_LONG_OFFSET);
		} else {
			totem_action_seek_relative (totem,
					SEEK_FORWARD_OFFSET);
		}
	} else {
		if (event->state & GDK_SHIFT_MASK) {
			totem_action_seek_relative (totem,
					SEEK_BACKWARD_SHORT_OFFSET);
		} else if (event->state & GDK_CONTROL_MASK) {
			totem_action_seek_relative (totem,
					SEEK_BACKWARD_LONG_OFFSET);
		} else {
			totem_action_seek_relative (totem,
					SEEK_BACKWARD_OFFSET);
		}
	}
}

static gboolean
totem_action_handle_key_press (Totem *totem, GdkEventKey *event)
{
	gboolean retval = TRUE, playlist_focused = FALSE;
	GtkWidget *focused;

	focused = gtk_window_get_focus (GTK_WINDOW (totem->win));
	if (focused != NULL && gtk_widget_is_ancestor
			(focused, GTK_WIDGET (totem->playlist)) != FALSE) {
		playlist_focused = TRUE;
	}

	switch (event->keyval) {
	case GDK_A:
	case GDK_a:
		totem_action_toggle_aspect_ratio (totem);
		break;
#ifdef HAVE_XFREE
	case XF86XK_AudioPrev:
#endif /* HAVE_XFREE */
	case GDK_B:
	case GDK_b:
		totem_action_previous (totem);
		break;
	case GDK_C:
	case GDK_c:
		bacon_video_widget_dvd_event (totem->bvw,
				BVW_DVD_CHAPTER_MENU);
		break;
#ifndef HAVE_GTK_ONLY
	case GDK_D:
	case GDK_d:
		totem_gromit_toggle ();
		break;
	case GDK_E:
	case GDK_e:
		totem_gromit_clear (FALSE);
		break;
#endif /* !HAVE_GTK_ONLY */
	case GDK_F11:
	case GDK_f:
	case GDK_F:
		totem_action_fullscreen_toggle (totem);
		break;
	case GDK_g:
	case GDK_G:
		if (totem_playing_dvd (totem->mrl) != FALSE) {
				bacon_video_widget_dvd_event (totem->bvw,
					BVW_DVD_NEXT_ANGLE);
		}
		break;
	case GDK_h:
	case GDK_H:
		totem_action_toggle_controls (totem);
		break;
	case GDK_i:
	case GDK_I:
		{
			GtkToggleAction *action;
			gboolean state;

			action = GTK_TOGGLE_ACTION (gtk_action_group_get_action
					(totem->main_action_group,
					 "deinterlace"));
			state = gtk_toggle_action_get_active (action);
			gtk_toggle_action_set_active (action, !state);
		}
		break;
	case GDK_M:
	case GDK_m:
		bacon_video_widget_dvd_event (totem->bvw, BVW_DVD_ROOT_MENU);
		break;
#ifdef HAVE_XFREE
	case XF86XK_AudioNext:
#endif /* HAVE_XFREE */
	case GDK_N:
	case GDK_n:
		totem_action_next (totem);
		break;
	case GDK_O:
	case GDK_o:
		totem_action_fullscreen (totem, FALSE);
		totem_action_open (totem);
		break;
#ifdef HAVE_XFREE
	case XF86XK_AudioPlay:
	case XF86XK_AudioPause:
#endif /* HAVE_XFREE */
	case GDK_p:
	case GDK_P:
		if (event->state & GDK_CONTROL_MASK)
			totem_action_show_properties (totem);
		else 
			totem_action_play_pause (totem);
		break;
	case GDK_q:
	case GDK_Q:
		totem_action_exit (totem);
		break;
	case GDK_r:
	case GDK_R:
		totem_action_zoom_relative (totem, ZOOM_IN_OFFSET);
		break;
	case GDK_s:
	case GDK_S:
		if (event->state & GDK_CONTROL_MASK) {
			totem_action_take_screenshot (totem);
		} else {
			totem_action_skip_to (totem);
		}
		break;
	case GDK_t:
	case GDK_T:
		totem_action_zoom_relative (totem, ZOOM_OUT_OFFSET);
		break;
	case GDK_Escape:
		if (event->state & GDK_SUPER_MASK)
			bacon_video_widget_dvd_event (totem->bvw, BVW_DVD_ROOT_MENU);
		else
			totem_action_fullscreen (totem, FALSE);
		break;
	case GDK_Left:
		if (playlist_focused != FALSE)
			return FALSE;

		totem_statusbar_set_seeking
			(TOTEM_STATUSBAR (totem->statusbar), TRUE);
		if (gtk_widget_get_direction (totem->win) == GTK_TEXT_DIR_RTL)
			totem_action_handle_seek (totem, event, TRUE);
		else
			totem_action_handle_seek (totem, event, FALSE);
		break;
	case GDK_Right:
		if (playlist_focused != FALSE)
			return FALSE;

		totem_statusbar_set_seeking
			(TOTEM_STATUSBAR (totem->statusbar), TRUE);
		if (gtk_widget_get_direction (totem->win) == GTK_TEXT_DIR_RTL)
			totem_action_handle_seek (totem, event, FALSE);
		else
			totem_action_handle_seek (totem, event, TRUE);
		break;
	case GDK_space:
		if (totem_is_fullscreen (totem) != FALSE || gtk_widget_is_focus (GTK_WIDGET (totem->bvw)) != FALSE)
			totem_action_play_pause (totem);
		else
			retval = FALSE;
		break;
	case GDK_Up:
		if (playlist_focused != FALSE)
			return FALSE;
		totem_action_volume_relative (totem, VOLUME_UP_OFFSET);
		break;
	case GDK_Down:
		if (playlist_focused != FALSE)
			return FALSE;
		totem_action_volume_relative (totem, VOLUME_DOWN_OFFSET);
		break;
	case GDK_0:
		if (event->state & GDK_CONTROL_MASK)
			totem_action_zoom_reset (totem);
		else
			totem_action_set_scale_ratio (totem, 0.5);
		break;
	case GDK_onehalf:
		totem_action_set_scale_ratio (totem, 0.5);
		break;
	case GDK_1:
		totem_action_set_scale_ratio (totem, 1);
		break;
	case GDK_2:
		totem_action_set_scale_ratio (totem, 2);
		break;
	case GDK_Menu:
		if (playlist_focused != FALSE)
			return FALSE;
		totem_action_menu_popup (totem, 0);
		break;
	case GDK_F10:
		if (playlist_focused != FALSE)
			return FALSE;
		if (!(event->state & GDK_SHIFT_MASK))
			return FALSE;

		totem_action_menu_popup (totem, 0);
		break;
	case GDK_plus:
	case GDK_KP_Add:
		if (!(event->state & GDK_CONTROL_MASK))
			return FALSE;

		totem_action_zoom_relative (totem, ZOOM_IN_OFFSET);
		break;
	case GDK_minus:
	case GDK_KP_Subtract:
		if (!(event->state & GDK_CONTROL_MASK))
			return FALSE;

		totem_action_zoom_relative (totem, ZOOM_OUT_OFFSET);
		break;
	case GDK_KP_Up:
	case GDK_KP_8:
		bacon_video_widget_dvd_event (totem->bvw, 
				BVW_DVD_ROOT_MENU_UP);
		break;
	case GDK_KP_Down:
	case GDK_KP_2:
		bacon_video_widget_dvd_event (totem->bvw, 
				BVW_DVD_ROOT_MENU_DOWN);
		break;
	case GDK_KP_Right:
	case GDK_KP_6:
		bacon_video_widget_dvd_event (totem->bvw, 
				BVW_DVD_ROOT_MENU_RIGHT);
		break;
	case GDK_KP_Left:
	case GDK_KP_4:
		bacon_video_widget_dvd_event (totem->bvw, 
				BVW_DVD_ROOT_MENU_LEFT);
		break;
	default:
		retval = FALSE;
	}

	return retval;
}

static gboolean
totem_action_handle_scroll (Totem *totem, GdkScrollDirection direction)
{
	gboolean retval = TRUE;

	on_video_motion_notify_event (NULL, NULL, totem);

	switch (direction) {
	case GDK_SCROLL_UP:
		totem_action_seek_relative
			(totem, SEEK_FORWARD_SHORT_OFFSET);
		break;
	case GDK_SCROLL_DOWN:
		totem_action_seek_relative
			(totem, SEEK_BACKWARD_SHORT_OFFSET);
		break;
	default:
		retval = FALSE;
	}

	return retval;
}

static gboolean
totem_action_handle_volume_scroll (Totem *totem, GdkScrollDirection direction)
{
	gboolean retval = TRUE;

	on_video_motion_notify_event (NULL, NULL, totem);

	switch (direction) {
	case GDK_SCROLL_UP:
		totem_action_volume_relative (totem, VOLUME_UP_OFFSET);
		break;
	case GDK_SCROLL_DOWN:
		totem_action_volume_relative (totem, VOLUME_DOWN_OFFSET);
		break;
	default:
		retval = FALSE;
	}

	return retval;
}

static int
on_window_key_press_event (GtkWidget *win, GdkEventKey *event, Totem *totem)
{
	/* Special case Eject, Open, Open URI and
	 * seeking keyboard shortcuts */
	if (event->state != 0
			&& (event->state & GDK_CONTROL_MASK))
	{
		switch (event->keyval)
		case GDK_E:
		case GDK_e:
		case GDK_O:
		case GDK_o:
		case GDK_L:
		case GDK_l:
		case GDK_q:
		case GDK_Q:
		case GDK_S:
		case GDK_s:
		case GDK_Right:
		case GDK_Left:
		case GDK_plus:
		case GDK_KP_Add:
		case GDK_minus:
		case GDK_KP_Subtract:
		case GDK_0:
			if (event->type == GDK_KEY_PRESS) {
				return totem_action_handle_key_press (totem, event);
			} else {
				return totem_action_handle_key_release (totem, event);
			}
	}

	if (event->state != 0
			&& (event->state & GDK_SUPER_MASK)) {
		switch (event->keyval)
		case GDK_Escape:
			if (event->type == GDK_KEY_PRESS) {
				return totem_action_handle_key_press (totem, event);
			} else {
				return totem_action_handle_key_release (totem, event);
			}
	}


	/* If we have modifiers, and either Ctrl, Mod1 (Alt), or any
	 * of Mod3 to Mod5 (Mod2 is num-lock...) are pressed, we
	 * let Gtk+ handle the key */
	if (event->state != 0
			&& ((event->state & GDK_CONTROL_MASK)
			|| (event->state & GDK_MOD1_MASK)
			|| (event->state & GDK_MOD3_MASK)
			|| (event->state & GDK_MOD4_MASK)
			|| (event->state & GDK_MOD5_MASK)))
		return FALSE;

	if (event->type == GDK_KEY_PRESS) {
		return totem_action_handle_key_press (totem, event);
	} else {
		return totem_action_handle_key_release (totem, event);
	}
}

static int
on_window_scroll_event (GtkWidget *win, GdkEventScroll *event, Totem *totem)
{
	return totem_action_handle_scroll (totem, event->direction);
}

static int
on_volume_scroll_event (GtkWidget *win, GdkEventScroll *event, Totem *totem)
{
	return totem_action_handle_volume_scroll (totem, event->direction);
}

static void
update_media_menu_items (Totem *totem)
{
        gboolean playing;

	playing = totem_playing_dvd (totem->mrl);

	totem_action_set_sensitivity ("dvd-root-menu", playing);
	totem_action_set_sensitivity ("dvd-title-menu", playing);
	totem_action_set_sensitivity ("dvd-audio-menu", playing);
	totem_action_set_sensitivity ("dvd-angle-menu", playing);
	totem_action_set_sensitivity ("dvd-chapter-menu", playing);
	/* FIXME we should only show that if we have multiple angles */
	totem_action_set_sensitivity ("next-angle", playing);

	playing = totem_is_media (totem->mrl);
	totem_action_set_sensitivity ("eject", playing);
}

static void
update_buttons (Totem *totem)
{
	gboolean has_item;

	/* Previous */
	if (totem_playing_dvd (totem->mrl) != FALSE)
		has_item = bacon_video_widget_has_previous_track (totem->bvw);
	else
		has_item = totem_playlist_has_previous_mrl (totem->playlist);

	totem_action_set_sensitivity ("previous-chapter", has_item);

	/* Next */
	if (totem_playing_dvd (totem->mrl) != FALSE)
		has_item = bacon_video_widget_has_next_track (totem->bvw);
	else
		has_item = totem_playlist_has_next_mrl (totem->playlist);

	totem_action_set_sensitivity ("next-chapter", has_item);
}

static void
main_pane_size_allocated (GtkWidget *main_pane, GtkAllocation *allocation, Totem *totem)
{
	gulong handler_id;

	if (!totem->maximised || GTK_WIDGET_MAPPED (totem->win)) {
		handler_id = g_signal_handler_find (main_pane, 
				G_SIGNAL_MATCH_FUNC | G_SIGNAL_MATCH_DATA,
				0, 0, NULL,
				main_pane_size_allocated, totem);
		g_signal_handler_disconnect (main_pane, handler_id);

		gtk_paned_set_position (GTK_PANED (main_pane), allocation->width - totem->sidebar_w);
	}
}

static void
totem_setup_window (Totem *totem)
{
	GKeyFile *keyfile;
	int w, h;
	gboolean show_sidebar;
	char *filename, *page_id;
	GError *err = NULL;
	GtkWidget *main_pane;

	filename = g_build_filename (g_get_home_dir (), ".gnome2", "totem", NULL);
	keyfile = g_key_file_new ();
	if (g_key_file_load_from_file (keyfile, filename,
			G_KEY_FILE_NONE, NULL) == FALSE) {
		totem->sidebar_w = w = h = 0;
		show_sidebar = TRUE;
		page_id = NULL;
		g_free (filename);
	} else {
		g_free (filename);

		w = g_key_file_get_integer (keyfile, "State", "window_w", &err);
		if (err != NULL) {
			w = 0;
			g_error_free (err);
			err = NULL;
		}

		h = g_key_file_get_integer (keyfile, "State", "window_h", &err);
		if (err != NULL) {
			h = 0;
			g_error_free (err);
			err = NULL;
		}

		show_sidebar = g_key_file_get_boolean (keyfile, "State",
				"show_sidebar", &err);
		if (err != NULL) {
			show_sidebar = TRUE;
			g_error_free (err);
			err = NULL;
		}

		totem->maximised = g_key_file_get_boolean (keyfile, "State",
				"maximised", &err);
		if (err != NULL) {
			g_error_free (err);
			err = NULL;
		}

		page_id = g_key_file_get_string (keyfile, "State",
				"sidebar_page", &err);
		if (err != NULL) {
			g_error_free (err);
			page_id = NULL;
			err = NULL;
		}

		totem->sidebar_w = g_key_file_get_integer (keyfile, "State",
				"sidebar_w", &err);
		if (err != NULL) {
			g_error_free (err);
			totem->sidebar_w = 0;
		}
		g_key_file_free (keyfile);
	}

	if (w > 0 && h > 0 && totem->maximised == FALSE) {
		gtk_window_set_default_size (GTK_WINDOW (totem->win),
				w, h);
		totem->window_w = w;
		totem->window_h = h;
	} else if (totem->maximised != FALSE) {
		gtk_window_maximize (GTK_WINDOW (totem->win));
	}
	
	main_pane = glade_xml_get_widget (totem->xml, "tmw_main_pane");
	g_signal_connect (G_OBJECT (main_pane), "size-allocate", G_CALLBACK (main_pane_size_allocated), totem);

	totem_sidebar_setup (totem, show_sidebar, page_id);
}

static void
totem_callback_connect (Totem *totem)
{
	GtkWidget *item, *box, *arrow;
	GtkAction *action;

	/* Menu items */
	gtk_action_group_set_visible (totem->zoom_action_group,
		bacon_video_widget_can_set_zoom (totem->bvw));

	action = gtk_action_group_get_action (totem->main_action_group, "repeat-mode");
	gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action),
		totem_playlist_get_repeat (totem->playlist));
	action = gtk_action_group_get_action (totem->main_action_group, "shuffle-mode");
	gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action),
		totem_playlist_get_shuffle (totem->playlist));

	/* Controls */
	box = glade_xml_get_widget (totem->xml, "tmw_buttons_hbox");

	action = gtk_action_group_get_action (totem->main_action_group,
			"previous-chapter");
	item = gtk_action_create_tool_item (action);
	atk_object_set_name (gtk_widget_get_accessible (item),
			_("Previous Chapter/Movie"));
	gtk_box_pack_start (GTK_BOX (box), item, FALSE, FALSE, 0);

	action = gtk_action_group_get_action (totem->main_action_group, "play");
	item = gtk_action_create_tool_item (action);
	atk_object_set_name (gtk_widget_get_accessible (item),
			_("Play / Pause"));
	gtk_box_pack_start (GTK_BOX (box), item, FALSE, FALSE, 0);

	action = gtk_action_group_get_action (totem->main_action_group,
			"next-chapter");
	item = gtk_action_create_tool_item (action);
	atk_object_set_name (gtk_widget_get_accessible (item),
			_("Next Chapter/Movie"));
	gtk_box_pack_start (GTK_BOX (box), item, FALSE, FALSE, 0);

	/* Sidebar button (Drag'n'Drop) */
	box = glade_xml_get_widget (totem->xml, "tmw_sidebar_button_hbox");
	action = gtk_action_group_get_action (totem->main_action_group, "sidebar");
	item = gtk_toggle_button_new ();
	gtk_action_connect_proxy (action, item);
	arrow = gtk_arrow_new (GTK_ARROW_RIGHT, GTK_SHADOW_NONE);
	gtk_widget_show (arrow);
	gtk_button_set_image (GTK_BUTTON (item), arrow);
	gtk_box_pack_start (GTK_BOX (box), item, FALSE, FALSE, 0);
	g_signal_connect (G_OBJECT (item), "drag_data_received",
			G_CALLBACK (drop_playlist_cb), totem);
	gtk_drag_dest_set (item, GTK_DEST_DEFAULT_ALL,
			target_table, G_N_ELEMENTS (target_table),
			GDK_ACTION_COPY | GDK_ACTION_MOVE);

	/* Main Window */
	g_signal_connect (G_OBJECT (totem->win), "delete-event",
			G_CALLBACK (main_window_destroy_cb), totem);
	g_object_notify (G_OBJECT (totem->win), "is-active");
	g_signal_connect_swapped (G_OBJECT (totem->win), "notify",
			G_CALLBACK (popup_hide), totem);
	g_signal_connect (G_OBJECT (totem->win), "window-state-event",
			G_CALLBACK (window_state_event_cb), totem);

	/* Screen size and Theme changes */
	g_signal_connect (totem->win, "realize",
			G_CALLBACK (window_realize_cb), totem);

	/* Motion notify for the Popups */
	item = glade_xml_get_widget (totem->xml,
			"totem_exit_fullscreen_window");
	gtk_widget_add_events (item, GDK_POINTER_MOTION_MASK);
	g_signal_connect (G_OBJECT (item), "motion-notify-event",
			G_CALLBACK (on_video_motion_notify_event), totem);
	item = glade_xml_get_widget (totem->xml, "totem_controls_window");
	gtk_widget_add_events (item, GDK_POINTER_MOTION_MASK);
	g_signal_connect (G_OBJECT (item), "motion-notify-event",
			G_CALLBACK (on_video_motion_notify_event), totem);

	/* Popup */
	item = glade_xml_get_widget (totem->xml, "tefw_fs_exit_button");
	g_signal_connect (G_OBJECT (item), "clicked",
			G_CALLBACK (on_fs_exit1_activate), totem);
	g_signal_connect (G_OBJECT (item), "motion-notify-event",
			G_CALLBACK (on_video_motion_notify_event), totem);

	/* Control Popup */
	box = glade_xml_get_widget (totem->xml, "tcw_buttons_hbox");

	action = gtk_action_group_get_action (totem->main_action_group, "previous-chapter");
	item = gtk_action_create_tool_item (action);
	gtk_box_pack_start (GTK_BOX (box), item, FALSE, FALSE, 0);
	g_signal_connect (G_OBJECT (item), "clicked",
			G_CALLBACK (on_mouse_click_fullscreen), totem);

	action = gtk_action_group_get_action (totem->main_action_group, "play");
	item = gtk_action_create_tool_item (action);
	gtk_box_pack_start (GTK_BOX (box), item, FALSE, FALSE, 0);
	g_signal_connect (G_OBJECT (item), "clicked",
			G_CALLBACK (on_mouse_click_fullscreen), totem);

	action = gtk_action_group_get_action (totem->main_action_group, "next-chapter");
	item = gtk_action_create_tool_item (action);
	gtk_box_pack_start (GTK_BOX (box), item, FALSE, FALSE, 0);
	g_signal_connect (G_OBJECT (item), "clicked",
			G_CALLBACK (on_mouse_click_fullscreen), totem);


	item = glade_xml_get_widget (totem->xml, "tcw_volume_mute_button");
	g_signal_connect (G_OBJECT (item), "clicked",
			G_CALLBACK (on_volume_mute_button), totem);
	item = glade_xml_get_widget (totem->xml, "tcw_volume_max_button");
	g_signal_connect (G_OBJECT (item), "clicked",
			G_CALLBACK (on_volume_max_button), totem);

	/* Control Popup Sliders */
	g_signal_connect (G_OBJECT(totem->fs_seek), "button_press_event",
			G_CALLBACK (seek_slider_pressed_cb), totem);
	g_signal_connect (G_OBJECT(totem->fs_seek), "button_release_event",
			G_CALLBACK (seek_slider_released_cb), totem);
	g_signal_connect (G_OBJECT (totem->fs_seekadj), "value-changed",
			  G_CALLBACK (seek_slider_changed_cb), totem);
	g_signal_connect (G_OBJECT(totem->fs_volume), "value-changed",
			G_CALLBACK (vol_cb), totem);
	g_signal_connect (G_OBJECT(totem->fs_volume), "button_press_event",
			G_CALLBACK (vol_slider_pressed_cb), totem);
	g_signal_connect (G_OBJECT(totem->fs_volume), "button_release_event",
			G_CALLBACK (vol_slider_released_cb), totem);

	/* Connect the keys */
	gtk_widget_add_events (totem->win, GDK_KEY_PRESS_MASK | GDK_KEY_RELEASE_MASK);
	g_signal_connect (G_OBJECT(totem->win), "key_press_event",
			G_CALLBACK (on_window_key_press_event), totem);
	g_signal_connect (G_OBJECT(totem->win), "key_release_event",
			G_CALLBACK (on_window_key_press_event), totem);

	/* Connect the mouse wheel */
	gtk_widget_add_events (totem->win, GDK_SCROLL_MASK);
	g_signal_connect (G_OBJECT(totem->win), "scroll_event",
			G_CALLBACK (on_window_scroll_event), totem);
	gtk_widget_add_events (totem->seek, GDK_SCROLL_MASK);
	g_signal_connect (G_OBJECT (totem->seek), "scroll_event",
			G_CALLBACK (on_window_scroll_event), totem);
	gtk_widget_add_events (totem->fs_seek, GDK_SCROLL_MASK);
	g_signal_connect (G_OBJECT (totem->fs_seek), "scroll_event",
			G_CALLBACK (on_window_scroll_event), totem);
	gtk_widget_add_events (totem->volume, GDK_SCROLL_MASK);
	g_signal_connect (G_OBJECT (totem->volume), "scroll_event",
			G_CALLBACK (on_volume_scroll_event), totem);
	gtk_widget_add_events (totem->fs_volume, GDK_SCROLL_MASK);
	g_signal_connect (G_OBJECT (totem->fs_volume), "scroll_event",
			G_CALLBACK (on_volume_scroll_event), totem);

	/* Sliders */
	g_signal_connect (G_OBJECT (totem->seek), "button_press_event",
			G_CALLBACK (seek_slider_pressed_cb), totem);
	g_signal_connect (G_OBJECT (totem->seek), "button_release_event",
			G_CALLBACK (seek_slider_released_cb), totem);
	g_signal_connect (G_OBJECT (totem->seekadj), "value_changed",
			  G_CALLBACK (seek_slider_changed_cb), totem);
	g_signal_connect (G_OBJECT (totem->volume), "value-changed",
			G_CALLBACK (vol_cb), totem);

	/* Set sensitivity of the toolbar buttons */
	totem_action_set_sensitivity ("play", FALSE);
	totem_action_set_sensitivity ("next-chapter", FALSE);
	totem_action_set_sensitivity ("previous-chapter", FALSE);
}

static void
playlist_widget_setup (Totem *totem)
{
	totem->playlist = TOTEM_PLAYLIST (totem_playlist_new ());

	if (totem->playlist == NULL)
		totem_action_exit (totem);

	gtk_widget_show_all (GTK_WIDGET (totem->playlist));

	g_signal_connect (G_OBJECT (totem->playlist), "active-name-changed",
			G_CALLBACK (on_playlist_change_name), totem);
	g_signal_connect (G_OBJECT (totem->playlist), "item-activated",
			G_CALLBACK (item_activated_cb), totem);
	g_signal_connect (G_OBJECT (totem->playlist),
			"changed", G_CALLBACK (playlist_changed_cb),
			totem);
	g_signal_connect (G_OBJECT (totem->playlist),
			"current-removed", G_CALLBACK (current_removed_cb),
			totem);
	g_signal_connect (G_OBJECT (totem->playlist),
			"repeat-toggled",
			G_CALLBACK (playlist_repeat_toggle_cb),
			totem);
	g_signal_connect (G_OBJECT (totem->playlist),
			"shuffle-toggled",
			G_CALLBACK (playlist_shuffle_toggle_cb),
			totem);

}

static void
video_widget_create (Totem *totem) 
{
	GError *err = NULL;
	GtkWidget *container;
	BaconVideoWidget **bvw;

	totem->scr = totem_scrsaver_new ();

	totem->bvw = BACON_VIDEO_WIDGET
		(bacon_video_widget_new (-1, -1, BVW_USE_TYPE_VIDEO, &err));

	if (totem->bvw == NULL)
	{
		totem_playlist_set_playing (totem->playlist, FALSE);

		gtk_widget_hide (totem->win);

		totem_action_error_and_exit (_("Totem could not startup."), err != NULL ? err->message : _("No reason."), totem);
		if (err != NULL)
			g_error_free (err);
	}

	totem_preferences_tvout_setup (totem);
	totem_preferences_visuals_setup (totem);
	totem_action_zoom (totem, ZOOM_RESET);

	g_signal_connect (G_OBJECT (totem->bvw),
			"motion-notify-event",
			G_CALLBACK (on_video_motion_notify_event),
			totem);
	g_signal_connect_after (G_OBJECT (totem->bvw),
			"button-press-event",
			G_CALLBACK (on_video_button_press_event),
			totem);
	g_signal_connect (G_OBJECT (totem->bvw),
			"eos",
			G_CALLBACK (on_eos_event),
			totem);
	g_signal_connect (G_OBJECT (totem->bvw),
			"got-redirect",
			G_CALLBACK (on_got_redirect),
			totem);
	g_signal_connect (G_OBJECT(totem->bvw),
			"title-change",
			G_CALLBACK (on_title_change_event),
			totem);
	g_signal_connect (G_OBJECT(totem->bvw),
			"channels-change",
			G_CALLBACK (on_channels_change_event),
			totem);
	g_signal_connect (G_OBJECT (totem->bvw),
			"tick",
			G_CALLBACK (update_current_time),
			totem);
	g_signal_connect (G_OBJECT (totem->bvw),
			"got-metadata",
			G_CALLBACK (on_got_metadata_event),
			totem);
	g_signal_connect (G_OBJECT (totem->bvw),
			"buffering",
			G_CALLBACK (on_buffering_event),
			totem);
	g_signal_connect (G_OBJECT (totem->bvw),
			"error",
			G_CALLBACK (on_error_event),
			totem);

	totem_missing_plugins_setup (totem);

	container = glade_xml_get_widget (totem->xml, "tmw_bvw_vbox");
	gtk_container_add (GTK_CONTAINER (container),
			GTK_WIDGET (totem->bvw));

	/* Events for the widget video window as well */
	gtk_widget_add_events (GTK_WIDGET (totem->bvw),
			GDK_KEY_PRESS_MASK | GDK_KEY_RELEASE_MASK);
	g_signal_connect (G_OBJECT(totem->bvw), "key_press_event",
			G_CALLBACK (on_window_key_press_event), totem);
	g_signal_connect (G_OBJECT(totem->bvw), "key_release_event",
			G_CALLBACK (on_window_key_press_event), totem);

	g_signal_connect (G_OBJECT (totem->bvw), "drag_data_received",
			G_CALLBACK (drop_video_cb), totem);
	gtk_drag_dest_set (GTK_WIDGET (totem->bvw), GTK_DEST_DEFAULT_ALL,
			target_table, G_N_ELEMENTS (target_table),
			GDK_ACTION_COPY | GDK_ACTION_MOVE);

	g_signal_connect (G_OBJECT (totem->bvw), "drag_data_get",
			G_CALLBACK (drag_video_cb), totem);
	gtk_drag_source_set (GTK_WIDGET (totem->bvw),
			GDK_BUTTON1_MASK | GDK_BUTTON3_MASK,
			source_table, G_N_ELEMENTS (source_table),
			GDK_ACTION_LINK);

	bvw = &(totem->bvw);
	g_object_add_weak_pointer (G_OBJECT (totem->bvw),
				   (gpointer *) bvw);

	gtk_widget_show (GTK_WIDGET (totem->bvw));

	bacon_video_widget_set_volume (totem->bvw,
			gconf_client_get_int (totem->gc,
				GCONF_PREFIX"/volume", NULL));
	g_signal_connect (G_OBJECT (totem->bvw), "notify",
			G_CALLBACK (property_notify_cb), totem);
	update_volume_sliders (totem);
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

GtkWidget *
totem_volume_create (void)
{
	GtkWidget *widget;

	widget = bacon_volume_button_new (GTK_ICON_SIZE_SMALL_TOOLBAR,
					  0, 100, 1);
	gtk_widget_set_sensitive (widget, FALSE);
	gtk_widget_show (widget);

	return widget;
}

int
main (int argc, char **argv)
{
	Totem *totem;
	GConfClient *gc;
#ifndef HAVE_GTK_ONLY
	GnomeProgram *program;
#endif
	GOptionContext *context;
	GOptionGroup *baconoptiongroup;

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

	/* Handle command line arguments */
	context = g_option_context_new (_("- Play movies and songs"));
	baconoptiongroup = bacon_video_widget_get_option_group();
	g_option_context_add_main_entries (context, options, GETTEXT_PACKAGE);
	g_option_context_add_group (context, baconoptiongroup);

#ifdef HAVE_GTK_ONLY
	g_option_context_add_group (context, gtk_get_option_group (TRUE));
	if (g_option_context_parse (context, &argc, argv, &error) == FALSE) {
		totem_action_error_and_exit (_("Totem could not parse the command-line options"), error->message);
	}
#else
	program = gnome_program_init (PACKAGE, VERSION,
			LIBGNOMEUI_MODULE,
			argc, argv,
			GNOME_PARAM_APP_DATADIR, DATADIR,
			GNOME_PARAM_GOPTION_CONTEXT, context,
			GNOME_PARAM_NONE);
#endif /* HAVE_GTK_ONLY */

	g_set_application_name (_("Totem Movie Player"));
	gtk_window_set_default_icon_name ("totem");

	gnome_vfs_init ();

	gc = gconf_client_get_default ();
	if (gc == NULL)
	{
		totem_action_error_and_exit (_("Totem could not initialize the configuration engine."), _("Make sure that GNOME is properly installed."), NULL);
	}

#ifndef HAVE_GTK_ONLY
	gnome_authentication_manager_init ();
#endif /* !HAVE_GTK_ONLY */

	totem = g_new0 (Totem, 1);

	/* IPC stuff */
	totem->conn = bacon_message_connection_new (GETTEXT_PACKAGE);
	if (bacon_message_connection_get_is_server (totem->conn) == FALSE)
	{
		totem_options_process_for_server (totem->conn, &optionstate);
		bacon_message_connection_free (totem->conn);
		g_free (totem);
		gdk_notify_startup_complete ();
		exit (0);
	} else {
		totem_options_process_early (gc, &optionstate);
	}

	/* Init totem itself */
	totem->prev_volume = -1;
	totem->gc = gc;
	totem->cursor_shown = TRUE;

	/* Main window */
	totem->xml = totem_interface_load ("totem.glade", _("main window"),
			TRUE, NULL);
	if (totem->xml == NULL)
		totem_action_exit (NULL);

	totem->win = glade_xml_get_widget (totem->xml, "totem_main_window");
	
	/* Menubar */
	totem_ui_manager_setup (totem);

	/* The sidebar */
	playlist_widget_setup (totem);
	totem->properties = bacon_video_widget_properties_new ();

	/* The rest of the widgets */
	totem->state = STATE_STOPPED;
	totem->seek = glade_xml_get_widget (totem->xml, "tmw_seek_hscale");
	totem->seekadj = gtk_range_get_adjustment (GTK_RANGE (totem->seek));
	g_object_set_data (G_OBJECT (totem->seek), "fs", GINT_TO_POINTER (0));
	totem->volume = glade_xml_get_widget (totem->xml, "tcw_volume_button");
	g_object_set_data (G_OBJECT (totem->volume), "fs", GINT_TO_POINTER (0));
	totem->exit_popup = glade_xml_get_widget
		(totem->xml, "totem_exit_fullscreen_window");
	totem->control_popup = glade_xml_get_widget
		(totem->xml, "totem_controls_window");
	totem->fs_seek = glade_xml_get_widget (totem->xml, "tcw_seek_hscale");
	totem->fs_seekadj = gtk_range_get_adjustment
		(GTK_RANGE (totem->fs_seek));
	g_object_set_data (G_OBJECT (totem->fs_seek), "fs", GINT_TO_POINTER (1));
	totem->fs_volume = glade_xml_get_widget
		(totem->xml, "tcw_volume_hscale");
	totem->fs_voladj = gtk_range_get_adjustment
		(GTK_RANGE (totem->fs_volume));
	totem->tooltip = gtk_tooltips_new ();
	g_object_set_data (G_OBJECT (totem->fs_volume), "fs", GINT_TO_POINTER (1));
	totem->volume_first_time = 1;
	totem->statusbar = glade_xml_get_widget (totem->xml, "tmw_statusbar");
	totem->tcw_time_label = glade_xml_get_widget (totem->xml,
			"tcw_time_display_label");
	totem->seek_lock = totem->vol_lock = totem->vol_fs_lock = FALSE;

	totem_session_setup (totem, argv);
	totem_setup_recent (totem);
	totem_setup_file_monitoring (totem);
	totem_setup_file_filters ();
	totem_setup_play_disc (totem);
	totem_callback_connect (totem);
	totem_setup_window (totem);

	/* Show ! gtk_main_iteration trickery to show all the widgets
	 * we have so far */
	gtk_widget_show (totem->win);
	totem_gdk_window_set_waiting_cursor (totem->win->window);
	update_fullscreen_size (totem);
	long_action ();

	totem->controls_visibility = TOTEM_CONTROLS_VISIBLE;

	/* Show ! (again) the video widget this time. */
	video_widget_create (totem);
	long_action ();
	gtk_widget_grab_focus (GTK_WIDGET (totem->bvw));
	bacon_video_widget_set_logo (totem->bvw, LOGO_PATH);

	/* The prefs after the video widget is connected */
	totem_setup_preferences (totem);

	/* Command-line handling */
	totem_options_process_late (totem, &optionstate);

	if (totem->session_restored != FALSE)
	{
		totem_session_restore (totem, optionstate.filenames);
	} else if (optionstate.filenames != NULL && totem_action_open_files (totem, optionstate.filenames)) {
		totem_action_play_pause (totem);
	} else {
		totem_action_set_mrl (totem, NULL);
	}
	gdk_window_set_cursor (totem->win->window, NULL);

	if (bacon_message_connection_get_is_server (totem->conn) != FALSE)
	{
		bacon_message_connection_set_callback (totem->conn,
				(BaconMessageReceivedFunc)
				totem_message_connection_receive_cb, totem);
	}

#ifdef HAVE_REMOTE
	totem->remote = totem_remote_new ();
	g_signal_connect (totem->remote, "button_pressed",
			  G_CALLBACK (totem_button_pressed_remote_cb), totem);
#endif /* HAVE_REMOTE */

	gtk_main ();

#ifndef HAVE_GTK_ONLY
	/* Will destroy GOption allocated data automatically */
	g_object_unref (program);
#endif	
	return 0;
}
