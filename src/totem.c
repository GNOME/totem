/* 
 * Copyright (C) 2001,2002,2003 Bastien Nocera <hadess@hadess.net>
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
 */

#include "config.h"

#ifndef HAVE_GTK_ONLY
#include <gnome.h>
#include "totem-gromit.h"
#else
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include <stdlib.h>
#endif /* !HAVE_GTK_ONLY */

#include <libgnomevfs/gnome-vfs.h>
#include <gconf/gconf-client.h>
#include <string.h>

/* X11 headers */
#include <gdk/gdkx.h>
#include <X11/Xlib.h>
#ifdef HAVE_XFREE
#include <X11/XF86keysym.h>
#endif

#include "bacon-video-widget.h"
#include "bacon-video-widget-properties.h"
#include "rb-ellipsizing-label.h"
#include "bacon-cd-selection.h"
#include "totem-statusbar.h"
#include "totem-time-label.h"
#include "totem-screenshot.h"
#include "video-utils.h"

#include "egg-recent-view.h"

#include "totem.h"
#include "totem-private.h"
#include "totem-preferences.h"
#include "totem-stock-icons.h"
#include "totem-disc.h"

#include "debug.h"

#define KEYBOARD_HYSTERISIS_TIMEOUT 500
#define REWIND_OR_PREVIOUS 4000

#define SEEK_FORWARD_OFFSET 60
#define SEEK_BACKWARD_OFFSET -15

#define SEEK_FORWARD_SHORT_OFFSET 15
#define SEEK_BACKWARD_SHORT_OFFSET -5

#define VOLUME_DOWN_OFFSET -8
#define VOLUME_UP_OFFSET 8

static const GtkTargetEntry target_table[] = {
	{ "text/uri-list", 0, 0 },
};

static const GtkTargetEntry source_table[] = {
	{ "text/uri-list", 0, 0 },
};

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

static gboolean totem_action_open_files (Totem *totem, char **list);
static gboolean totem_action_open_files_list (Totem *totem, GSList *list);
static void update_fullscreen_size (Totem *totem);
static gboolean popup_hide (Totem *totem);
static void update_buttons (Totem *totem);
static void update_media_menu_items (Totem *totem);
static void update_dvd_menu_sub_lang (Totem *totem); 
static void update_seekable (Totem *totem, gboolean force_false);
static void on_play_pause_button_clicked (GtkToggleButton *button,
		Totem *totem);
static void playlist_changed_cb (GtkWidget *playlist, Totem *totem);
static gboolean totem_is_media (const char *mrl); 
static void show_controls (Totem *totem, gboolean visible, gboolean fullscreen_behaviour);
static gboolean totem_is_fullscreen (Totem *totem);
static void play_pause_set_label (Totem *totem, TotemStates state);

static void
long_action (void)
{
	while (gtk_events_pending ())
		gtk_main_iteration ();
}

static void
totem_g_list_deep_free (GList *list)
{
	GList *l;

	for (l = list; l != NULL; l = l->next)
		g_free (l->data);
	g_list_free (list);
}

static char*
totem_create_full_path (const char *path)
{
	char *retval, *curdir, *curdir_withslash, *escaped;

	g_return_val_if_fail (path != NULL, NULL);

	if (strstr (path, "://") != NULL)
		return g_strdup (path);
	if (totem_is_media (path) != FALSE)
		return g_strdup (path);

	if (path[0] == '/')
	{
		escaped = gnome_vfs_escape_path_string (path);

		retval = g_strdup_printf ("file://%s", escaped);
		g_free (escaped);
		return retval;
	}

	curdir = g_get_current_dir ();
	escaped = gnome_vfs_escape_path_string (curdir);
	curdir_withslash = g_strdup_printf ("file://%s%s",
			escaped, G_DIR_SEPARATOR_S);
	g_free (escaped);
	g_free (curdir);

	escaped = gnome_vfs_escape_path_string (path);
	retval = gnome_vfs_uri_make_full_from_relative
		(curdir_withslash, escaped);
	g_free (curdir_withslash);
	g_free (escaped);

	return retval;
}

void
totem_action_error (char *title, char *reason, Totem *totem)
{
	GtkWidget *parent, *error_dialog;
	char *title_esc, *reason_esc;

	if (reason == NULL)
		g_warning ("totem_action_error called with reason == NULL");

	if (totem == NULL)
		parent = NULL;
	else
		parent = totem->win;

	title_esc = g_markup_escape_text (title, -1);
	reason_esc = g_markup_escape_text (reason, -1);

	error_dialog =
		gtk_message_dialog_new (GTK_WINDOW (parent),
				GTK_DIALOG_MODAL,
				GTK_MESSAGE_ERROR,
				GTK_BUTTONS_OK,
				"<b>%s</b>\n%s", title_esc, reason_esc);
	g_free (title_esc);
	g_free (reason_esc);

	gtk_container_set_border_width (GTK_CONTAINER (error_dialog), 5);
	gtk_dialog_set_default_response (GTK_DIALOG (error_dialog),
			GTK_RESPONSE_OK);
	gtk_label_set_use_markup (GTK_LABEL (GTK_MESSAGE_DIALOG (error_dialog)->label), TRUE);
	g_signal_connect (G_OBJECT (error_dialog), "destroy", G_CALLBACK
			(gtk_widget_destroy), error_dialog);
	g_signal_connect (G_OBJECT (error_dialog), "response", G_CALLBACK
			(gtk_widget_destroy), error_dialog);
	gtk_window_set_modal (GTK_WINDOW (error_dialog), TRUE);

	gtk_widget_show (error_dialog);
}

static void
totem_action_error_and_exit (char *title, char *reason, Totem *totem)
{
	GtkWidget *error_dialog;

	error_dialog =
		gtk_message_dialog_new (NULL,
				GTK_DIALOG_MODAL,
				GTK_MESSAGE_ERROR,
				GTK_BUTTONS_OK,
				"<b>%s</b>\n%s", title, reason);
	gtk_container_set_border_width (GTK_CONTAINER (error_dialog), 5);
	gtk_label_set_use_markup (GTK_LABEL (GTK_MESSAGE_DIALOG (error_dialog)->label), TRUE);
	gtk_dialog_set_default_response (GTK_DIALOG (error_dialog),
			GTK_RESPONSE_OK);
	gtk_window_set_modal (GTK_WINDOW (error_dialog), TRUE);
	gtk_dialog_run (GTK_DIALOG (error_dialog));

	totem_action_exit (totem);
}

static void
totem_action_save_size (Totem *totem)
{
	if (totem->bvw == NULL)
		return;

	if (totem->controls_visibility == TOTEM_CONTROLS_FULLSCREEN)
		return;

	/* Save the size of the video widget */
	gconf_client_set_int (totem->gc,
			GCONF_PREFIX"/window_w",
			GTK_WIDGET(totem->bvw)->allocation.width, NULL);                gconf_client_set_int (totem->gc,
			GCONF_PREFIX"/window_h",
			GTK_WIDGET(totem->bvw)->allocation.height,
			NULL);
}

void
totem_action_exit (Totem *totem)
{
	if (gtk_main_level () > 0)
		gtk_main_quit ();

	if (totem == NULL)
		exit (0);

#ifndef HAVE_GTK_ONLY
	totem_gromit_clear (TRUE);
#endif /* !HAVE_GTK_ONLY */
	bacon_message_connection_free (totem->conn);
	totem_action_save_size (totem);

	totem_action_fullscreen (totem, FALSE);

	if (totem->playlist)
		gtk_widget_hide (GTK_WIDGET (totem->playlist));

	if (totem->win)
		gtk_widget_hide (totem->win);

	if (totem->bvw)
		gtk_widget_destroy (GTK_WIDGET (totem->bvw));

	totem_named_icons_dispose (totem);

	if (totem->playlist)
	{
		char *path;

		path = g_build_filename (g_get_home_dir (),
				".gnome2", "totem.pls", NULL);
		totem_playlist_save_current_playlist (totem->playlist, path);
		g_free (path);

		gtk_widget_destroy (GTK_WIDGET (totem->playlist));
	}

	exit (0);
}

static void
action_toggle_playlist (Totem *totem)
{
	GtkWidget *toggle;
	gboolean state;

	toggle = glade_xml_get_widget (totem->xml, "tmw_playlist_button");

	state = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (toggle));
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (toggle), !state);
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
	GtkWidget *image;
	const char *id;

	switch (state)
	{
	case STATE_PLAYING:
		totem_statusbar_set_text (TOTEM_STATUSBAR (totem->statusbar),
				_("Playing"));
		id = "stock-media-pause";
		break;
	case STATE_PAUSED:
		totem_statusbar_set_text (TOTEM_STATUSBAR (totem->statusbar),
				_("Paused"));
		id = "stock-media-play";
		break;
	case STATE_STOPPED:
		totem_statusbar_set_text (TOTEM_STATUSBAR (totem->statusbar),
				_("Stopped"));
		totem_statusbar_set_time_and_length
			(TOTEM_STATUSBAR (totem->statusbar), 0, 0);
		id = "stock-media-play";
		break;
	default:
		return;
	}

	image = glade_xml_get_widget (totem->xml, "tmw_play_pause_button_image");
	gtk_image_set_from_pixbuf (GTK_IMAGE (image),
			totem_get_named_icon_for_id (id));
	image = glade_xml_get_widget (totem->xml, "tcw_pp_button_image");
	gtk_image_set_from_pixbuf (GTK_IMAGE (image),
			totem_get_named_icon_for_id (id));

	totem->state = state;
}

static void
totem_action_eject (Totem *totem)
{
	GError *err = NULL;
	char *cmd, *prefix;
	const char *needle;

	needle = strchr (totem->mrl, ':');
	g_assert (needle != NULL);
	/* we want the ':' as well */
	prefix = g_strndup (totem->mrl, needle - totem->mrl + 1);
	totem_playlist_clear_with_prefix (totem->playlist, prefix);
	g_free (prefix);

	cmd = g_strdup_printf ("eject %s", gconf_client_get_string
			(totem->gc, GCONF_PREFIX"/mediadev", NULL));
	if (g_spawn_command_line_sync (cmd, NULL, NULL, NULL, &err) == FALSE)
	{
		totem_action_error (_("Totem could not eject the optical media."), err->message, totem);
		g_error_free (err);
	}
	g_free (cmd);
}

void
totem_action_play (Totem *totem)
{
	GError *err = NULL;
	int retval;

	if (totem->mrl == NULL)
		return;

	retval = bacon_video_widget_play (totem->bvw,  &err);
	play_pause_set_label (totem, retval ? STATE_PLAYING : STATE_STOPPED);
	if (retval == FALSE)
	{
		char *msg, *disp;

		disp = gnome_vfs_unescape_string_for_display (totem->mrl);
		msg = g_strdup_printf(_("Totem could not play '%s'."), disp);
		g_free (disp);

		totem_playlist_set_playing (totem->playlist, FALSE);
		totem_action_error (msg, err->message, totem);
		if (bacon_video_widget_is_playing (totem->bvw) != FALSE)
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

	retval = bacon_video_widget_seek (totem->bvw, pos, &err);

	if (retval == FALSE)
	{
		char *msg, *disp;

		disp = gnome_vfs_unescape_string_for_display (totem->mrl);
		msg = g_strdup_printf(_("Totem could not play '%s'."), disp);
		g_free (disp);

		totem_playlist_set_playing (totem->playlist, FALSE);
		totem_action_error (msg, err->message, totem);
		if (bacon_video_widget_is_playing (totem->bvw) != FALSE)
			totem_action_stop (totem);
		g_free (msg);
		g_error_free (err);
	}
}

static void
totem_action_set_mrl_and_play (Totem *totem, char *mrl)
{
	if (totem_action_set_mrl (totem, mrl) != FALSE)
		totem_action_play (totem);
}

static char *media_strings[] = {
	N_("DVD"),
	N_("Video CD"),
	N_("Audio CD")
};

static gboolean
totem_action_load_media (Totem *totem, MediaType type)
{
	const char **mrls;
	char *msg;

	if (bacon_video_widget_can_play (totem->bvw, type) == FALSE)
	{
		msg = g_strdup_printf (_("Totem cannot play this type of media (%s) because you do not have the appropriate plugins to handle it."), _(media_strings[type]));
		totem_action_error (msg, _("Please install the necessary plugins and restart Totem to be able to play this media."), totem);
		g_free (msg);
		return FALSE;
	}

	mrls = bacon_video_widget_get_mrls (totem->bvw, type);
	if (mrls == NULL)
	{
		msg = g_strdup_printf (_("Totem could not play this media (%s) although a plugin is present to handle it."), _(media_strings[type]));
		totem_action_error (msg, _("You might want to check that a disc is present in the drive and that it is correctly configured."), totem);
		g_free (msg);
		return FALSE;
	}

	totem_action_open_files (totem, (char **)mrls);
	return TRUE;
}

void
totem_action_play_media (Totem *totem, MediaType type)
{
	char *mrl;

	if (totem_action_load_media (totem, type) != FALSE)
	{
		mrl = totem_playlist_get_current_mrl (totem->playlist);
		totem_action_set_mrl_and_play (totem, mrl);
		g_free (mrl);
	}
}

void
totem_action_stop (Totem *totem)
{
	bacon_video_widget_stop (totem->bvw);
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
	} else {
		bacon_video_widget_pause (totem->bvw);
		play_pause_set_label (totem, STATE_PAUSED);
	}
}

void
totem_action_fullscreen_toggle (Totem *totem)
{
	if (totem->controls_visibility == TOTEM_CONTROLS_FULLSCREEN)
	{
		popup_hide (totem);
		bacon_video_widget_set_fullscreen (totem->bvw, FALSE);
		gtk_window_unfullscreen (GTK_WINDOW(totem->win));
		bacon_video_widget_set_show_cursor (totem->bvw, TRUE);
		scrsaver_enable (totem->scr);

		if (totem->controls_visibility != TOTEM_CONTROLS_VISIBLE)
		{
			GtkWidget *item;
			item = glade_xml_get_widget
				(totem->xml, "tmw_show_controls_menu_item");
			if (gtk_check_menu_item_get_active
					(GTK_CHECK_MENU_ITEM (item)))
			{
				totem->controls_visibility =
					TOTEM_CONTROLS_VISIBLE;
				show_controls (totem, TRUE, TRUE);
			} else {
				totem->controls_visibility =
					TOTEM_CONTROLS_HIDDEN;
				show_controls (totem, FALSE, TRUE);
			}
		}
	} else {
		totem_action_save_size (totem);
		update_fullscreen_size (totem);
		bacon_video_widget_set_fullscreen (totem->bvw, TRUE);
		bacon_video_widget_set_show_cursor (totem->bvw, FALSE);
		gtk_window_fullscreen (GTK_WINDOW(totem->win));
		scrsaver_disable (totem->scr);

		totem->controls_visibility = TOTEM_CONTROLS_FULLSCREEN;
		show_controls (totem, FALSE, TRUE);
	}
}

void
totem_action_fullscreen (Totem *totem, gboolean state)
{
	if (totem_is_fullscreen (totem) == state)
		return;

	totem_action_fullscreen_toggle (totem);
}

static char *
totem_get_nice_name_for_stream (Totem *totem)
{
	char *title, *artist, *retval;
	GValue value = { 0, };

	bacon_video_widget_get_metadata (totem->bvw, BVW_INFO_TITLE, &value);
	title = g_strdup (g_value_get_string (&value));
	g_value_unset (&value);

	if (title == NULL)
		return NULL;

	bacon_video_widget_get_metadata (totem->bvw, BVW_INFO_ARTIST, &value);
	artist = g_strdup (g_value_get_string (&value));
	g_value_unset (&value);

	if (artist == NULL)
	{
		g_free (title);
		return NULL;
	}

	retval = g_strdup_printf ("%s - %s", artist, title);
	g_free (artist);
	g_free (title);

	return retval;
}

static void
totem_action_restore_pl (Totem *totem)
{
	char *path, *mrl;

	path = g_build_filename (g_get_home_dir (),
			".gnome2", "totem.pls", NULL);

	if (g_file_test (path, G_FILE_TEST_EXISTS) == FALSE)
	{
		g_free (path);
		totem_action_set_mrl (totem, NULL);
		return;
	}

	g_signal_handlers_disconnect_by_func
		(G_OBJECT (totem->playlist),
		 playlist_changed_cb, totem);

	if (totem_playlist_add_mrl (totem->playlist, path, NULL) == FALSE)
	{
		g_signal_connect (G_OBJECT (totem->playlist),
				"changed", G_CALLBACK (playlist_changed_cb),
				totem);

		g_free (path);
		totem_action_set_mrl (totem, NULL);
		return;
	}

	g_signal_connect (G_OBJECT (totem->playlist),
			"changed", G_CALLBACK (playlist_changed_cb),
			totem);

	g_free (path);
	play_pause_set_label (totem, STATE_PAUSED);

	mrl = totem_playlist_get_current_mrl (totem->playlist);
	if (totem->mrl == NULL
			|| (totem->mrl != NULL && mrl != NULL
				&& strcmp (totem->mrl, mrl) != 0))
	{
		totem_action_set_mrl (totem, mrl);
	} else if (totem->mrl != NULL) {
		totem_playlist_set_playing (totem->playlist, TRUE);
	}

	g_free (mrl);

	return;
}

static void
update_skip_to (Totem *totem, gint64 time)
{
	GtkWidget *widget;

	widget = glade_xml_get_widget (totem->xml, "tstw_skip_spinbutton");
	gtk_spin_button_set_range (GTK_SPIN_BUTTON (widget),
			0, (gdouble) time / 1000);
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

		widget = glade_xml_get_widget (totem->xml, "tcw_label_custom");
		rb_ellipsizing_label_set_markup (RB_ELLIPSIZING_LABEL (widget),
				text);
		widget = glade_xml_get_widget (totem->xml, "tmw_title_label");
		rb_ellipsizing_label_set_markup (RB_ELLIPSIZING_LABEL (widget),
				text);

		g_free (text);

		/* Title */
		text = g_strdup_printf (_("%s - Totem Movie Player"), name);
		gtk_window_set_title (GTK_WINDOW (totem->win), text);
		g_free (text);
	} else {
		totem_statusbar_set_time_and_length (TOTEM_STATUSBAR
				(totem->statusbar), 0, 0);
		totem_statusbar_set_text (TOTEM_STATUSBAR (totem->statusbar),
				_("Stopped"));

		update_skip_to (totem, 0);

		/* Update the mrl label */
		text = g_strdup_printf
			("<span size=\"medium\"><b>%s</b></span>",
			 _("No file"));
		widget = glade_xml_get_widget (totem->xml, "tcw_label_custom");
		rb_ellipsizing_label_set_markup (RB_ELLIPSIZING_LABEL (widget),
				text);
		widget = glade_xml_get_widget (totem->xml, "tmw_title_label");
		rb_ellipsizing_label_set_markup (RB_ELLIPSIZING_LABEL (widget),
				text);

		g_free (text);

		/* Title */
		gtk_window_set_title (GTK_WINDOW (totem->win), _("Totem Movie Player"));
	}
}

gboolean
totem_action_set_mrl (Totem *totem, const char *mrl)
{
	GtkWidget *widget;
	gboolean retval = TRUE;

	if (totem->mrl != NULL)
	{
		g_free (totem->mrl);
		totem->mrl = NULL;
		bacon_video_widget_close (totem->bvw);
	}

	/* Reset the properties and wait for the signal*/
	bacon_video_widget_properties_update
		(BACON_VIDEO_WIDGET_PROPERTIES (totem->properties),
		 totem->bvw, TRUE);

	if (mrl == NULL)
	{
		retval = FALSE;

		gtk_window_set_title (GTK_WINDOW (totem->win), _("Totem"));

		/* Play/Pause */
		gtk_widget_set_sensitive (totem->pp_button, FALSE);
		widget = glade_xml_get_widget (totem->xml, "tmw_play_menu_item");
		gtk_widget_set_sensitive (widget, FALSE);

		widget = glade_xml_get_widget (totem->xml, "trcm_play");
		gtk_widget_set_sensitive (widget, FALSE);

		/* Seek bar and seek buttons */
		update_seekable (totem, FALSE);

		/* Volume */
		widget = glade_xml_get_widget (totem->xml, "tmw_volume_hbox");
		gtk_widget_set_sensitive (widget, FALSE);
		widget = glade_xml_get_widget (totem->xml, "tmw_volume_up_menu_item");
		gtk_widget_set_sensitive (widget, FALSE);
		widget = glade_xml_get_widget (totem->xml, "tmw_volume_down_menu_item");
		gtk_widget_set_sensitive (widget, FALSE);
		widget = glade_xml_get_widget (totem->xml, "trcm_volume_up");
		gtk_widget_set_sensitive (widget, FALSE);
		widget = glade_xml_get_widget (totem->xml, "trcm_volume_down");
		gtk_widget_set_sensitive (widget, FALSE);

		/* Control popup */
		gtk_widget_set_sensitive (totem->fs_seek, FALSE);
		gtk_widget_set_sensitive (totem->fs_pp_button, FALSE);
		widget = glade_xml_get_widget (totem->xml,
				"tcw_previous_button");
		gtk_widget_set_sensitive (widget, FALSE);
		widget = glade_xml_get_widget (totem->xml, "tcw_next_button"); 
		gtk_widget_set_sensitive (widget, FALSE);
		widget = glade_xml_get_widget (totem->xml, "trcm_next_chapter");
		gtk_widget_set_sensitive (widget, FALSE);
		widget = glade_xml_get_widget (totem->xml, "trcm_previous_chapter");
		gtk_widget_set_sensitive (widget, FALSE);
		widget = glade_xml_get_widget (totem->xml, "tcw_volume_hbox");
		gtk_widget_set_sensitive (widget, FALSE);

		/* Take a screenshot */
		widget = glade_xml_get_widget (totem->xml,
				"tmw_take_screenshot_menu_item");
		gtk_widget_set_sensitive (widget, FALSE);

		/* Set the logo */
		totem->mrl = g_strdup(LOGO_PATH);
		bacon_video_widget_set_logo_mode (totem->bvw, TRUE);
		bacon_video_widget_set_logo (totem->bvw, totem->mrl);

		update_mrl_label (totem, NULL);
	} else {
		gboolean caps;
		GError *err = NULL;

		bacon_video_widget_set_logo_mode (totem->bvw, FALSE);

		retval = bacon_video_widget_open (totem->bvw, mrl, &err);
		totem->mrl = g_strdup (mrl);

		/* Play/Pause */
		gtk_widget_set_sensitive (totem->pp_button, TRUE);
		widget = glade_xml_get_widget (totem->xml, "tmw_play_menu_item");
		gtk_widget_set_sensitive (widget, TRUE);
		widget = glade_xml_get_widget (totem->xml, "trcm_play");
		gtk_widget_set_sensitive (widget, TRUE);
		gtk_widget_set_sensitive (totem->fs_pp_button, TRUE);

		/* Seek bar */
		update_seekable (totem,
				bacon_video_widget_is_seekable (totem->bvw));

		/* Volume */
		caps = bacon_video_widget_can_set_volume (totem->bvw);
		widget = glade_xml_get_widget (totem->xml, "tmw_volume_hbox");
		gtk_widget_set_sensitive (widget, caps);
		widget = glade_xml_get_widget (totem->xml, "tmw_volume_up_menu_item");
		gtk_widget_set_sensitive (widget, caps);
		widget = glade_xml_get_widget (totem->xml, "tmw_volume_down_menu_item");
		gtk_widget_set_sensitive (widget, caps);
		widget = glade_xml_get_widget (totem->xml, "tcw_volume_hbox");
		gtk_widget_set_sensitive (widget, caps);

		/* Take a screenshot */
		widget = glade_xml_get_widget (totem->xml,
				"tmw_take_screenshot_menu_item");
		gtk_widget_set_sensitive (widget, retval);

		/* Set the playlist */
		totem_playlist_set_playing (totem->playlist, retval);

		if (retval == FALSE)
		{
			char *msg, *disp;

			disp = gnome_vfs_unescape_string_for_display (totem->mrl);
			msg = g_strdup_printf(_("Totem could not play '%s'."), disp);
			g_free (disp);
			totem_action_error (msg, err->message, totem);

			g_free (msg);
			g_error_free (err);
		}
	}
	update_buttons (totem);
	update_media_menu_items (totem);

	widget = glade_xml_get_widget (totem->xml, "tmw_properties_menu_item");
	gtk_widget_set_sensitive (widget, retval);

	return retval;
}

static gboolean
totem_playing_dvd (Totem *totem)
{
	if (totem->mrl == NULL)
		return FALSE;

	return g_str_has_prefix (totem->mrl, "dvd:/");
}

static gboolean
totem_is_media (const char *mrl)
{
	if (mrl == NULL)
		return FALSE;

	if (g_str_has_prefix (mrl, "cdda:") != FALSE)
		return TRUE;
	if (g_str_has_prefix (mrl, "dvd:") != FALSE)
		return TRUE;
	if (g_str_has_prefix (mrl, "vcd:") != FALSE)
		return TRUE;
	if (g_str_has_prefix (mrl, "cd:") != FALSE)
		return TRUE;

	return FALSE;
}

static gboolean
totem_time_within_seconds (Totem *totem)
{
	gint64 time;

	time = bacon_video_widget_get_current_time (totem->bvw);

	return (time < REWIND_OR_PREVIOUS);
}

void
totem_action_previous (Totem *totem)
{
	char *mrl;

	if (totem_playing_dvd (totem) == FALSE &&
		totem_playlist_has_previous_mrl (totem->playlist) == FALSE
		&& totem_playlist_get_repeat (totem->playlist) == FALSE)
		return;

        if (totem_playing_dvd (totem) != FALSE)
        {
                bacon_video_widget_dvd_event (totem->bvw, BVW_DVD_PREV_CHAPTER);
        } else {
		if (bacon_video_widget_is_seekable (totem->bvw) == FALSE
				|| totem_time_within_seconds (totem) != FALSE) {
			totem_playlist_set_previous (totem->playlist);
			mrl = totem_playlist_get_current_mrl (totem->playlist);
			totem_action_set_mrl_and_play (totem, mrl);
			g_free (mrl);
		} else {
			totem_action_seek (totem, 0);
		}
	}
}

void
totem_action_next (Totem *totem)
{
	char *mrl;

	if (totem_playing_dvd (totem) == FALSE &&
			totem_playlist_has_next_mrl (totem->playlist) == FALSE
			&& totem_playlist_get_repeat (totem->playlist) == FALSE)
		return;

	if (totem_playing_dvd (totem) != FALSE)
	{
		bacon_video_widget_dvd_event (totem->bvw, BVW_DVD_NEXT_CHAPTER);
	} else {
		totem_playlist_set_next (totem->playlist);
		mrl = totem_playlist_get_current_mrl (totem->playlist);
		totem_action_set_mrl_and_play (totem, mrl);
		g_free (mrl);
	}
}

void
totem_action_seek_relative (Totem *totem, int off_sec)
{
	GError *err = NULL;
	gint64 off_msec, oldsec, sec;

	if (bacon_video_widget_is_seekable (totem->bvw) == FALSE)
		return;
	if (totem->mrl == NULL)
		return;

	off_msec = off_sec * 1000;
	oldsec = bacon_video_widget_get_current_time (totem->bvw);
	sec = MAX (0, oldsec + off_msec);

	bacon_video_widget_seek_time (totem->bvw, sec, &err);

	if (err != NULL)
	{
		char *msg, *disp;

		disp = gnome_vfs_unescape_string_for_display (totem->mrl);
		msg = g_strdup_printf(_("Totem could not play '%s'."), totem->mrl);
		g_free (disp);

		totem_playlist_set_playing (totem->playlist, FALSE);
		if (bacon_video_widget_is_playing (totem->bvw) != FALSE)
			totem_action_stop (totem);
		totem_action_error (msg, err->message, totem);
		g_free (msg);
		g_error_free (err);
	}
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
	GtkWidget  *item;
	int  tmp;
	static char *widgets[] = {
		"tmw_aspect_ratio_auto_menu_item",
		"tmw_aspect_ratio_square_menu_item",
		"tmw_aspect_ratio_fbt_menu_item",
		"tmw_aspect_ratio_anamorphic_menu_item",
		"tmw_aspect_ratio_dvb_menu_item",
	};

	tmp = totem_action_get_aspect_ratio (totem);
	tmp++;
	if (tmp > 4)
		tmp = 0;

	item = glade_xml_get_widget (totem->xml, widgets[tmp]);
	gtk_check_menu_item_set_active ((GtkCheckMenuItem *) item, TRUE);
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

static gboolean
totem_action_drop_files (Totem *totem, GtkSelectionData *data,
		gboolean empty_pl)
{
	GList *list, *p, *file_list;
	gboolean cleared = FALSE;

	list = gnome_vfs_uri_list_parse (data->data);

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
	file_list = g_list_reverse (file_list);

	if (file_list == NULL)
		return FALSE;

	for (p = file_list; p != NULL; p = p->next)
	{
		char *filename;

		if (p->data == NULL)
			continue;

		filename = totem_create_full_path (p->data);

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
		totem_playlist_add_mrl (totem->playlist, filename, NULL);

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

	retval = totem_action_drop_files (totem, data, TRUE);
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

	retval = totem_action_drop_files (totem, data, FALSE);
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
on_play_pause_button_clicked (GtkToggleButton *button, Totem *totem)
{
	TOTEM_PROFILE (totem_action_play_pause (totem));
}

static void
on_previous_button_clicked (GtkButton *button, Totem *totem)
{
	TOTEM_PROFILE (totem_action_previous (totem));
}

static void
on_next_button_clicked (GtkButton *button, Totem *totem)
{
	TOTEM_PROFILE (totem_action_next (totem));
}

static void
on_playlist_button_toggled (GtkToggleButton *button, Totem *totem)
{
	gboolean state;

	state = gtk_toggle_button_get_active (button);
	if (state != FALSE)
		gtk_widget_show (GTK_WIDGET (totem->playlist));
	else
		gtk_widget_hide (GTK_WIDGET (totem->playlist));
}

static void
on_recent_file_activate (EggRecentViewGtk *view, EggRecentItem *item,
                         Totem *totem)
{
	char *uri;
	gboolean playlist_changed;

	uri = egg_recent_item_get_uri (item);

	playlist_changed = totem_playlist_add_mrl (totem->playlist, uri, NULL);
	egg_recent_model_add_full (totem->recent_model, item);
	
	if (playlist_changed)
	{
		char *mrl;
		totem_playlist_set_at_end (totem->playlist);
		mrl = totem_playlist_get_current_mrl (totem->playlist);
		totem_action_set_mrl_and_play (totem, mrl);
		g_free (mrl);   
	}

	g_free (uri);
}

/* This is only called when we are playing a DVD */
static void
on_title_change_event (GtkWidget *win, const char *string, Totem *totem)
{
	update_mrl_label (totem, string);
	totem_playlist_set_title (TOTEM_PLAYLIST (totem->playlist), string);
}

static void
on_channels_change_event (BaconVideoWidget *bvw, Totem *totem)
{
	update_dvd_menu_sub_lang (totem);
}

static void
on_got_metadata_event (BaconVideoWidget *bvw, Totem *totem)
{
        char *name = NULL;
	gboolean custom;

        bacon_video_widget_properties_update
		(BACON_VIDEO_WIDGET_PROPERTIES (totem->properties),
		 totem->bvw, FALSE);

	name = totem_get_nice_name_for_stream (totem);

	if (name == NULL)
	{
		name = totem_playlist_get_current_title
			(totem->playlist, &custom);
		custom = TRUE;
	} else {
		custom = FALSE;
	}

	update_mrl_label (totem, name);

	if (custom == FALSE)
		totem_playlist_set_title
			(TOTEM_PLAYLIST (totem->playlist), name);

	g_free (name);
}

static void
on_error_event (BaconVideoWidget *bvw, char *message,
                gboolean playback_stopped, gboolean fatal, Totem *totem)
{
	if (playback_stopped)
		play_pause_set_label (totem, STATE_STOPPED);

	if (fatal == FALSE) {
		totem_action_error (_("An error occured"), message, totem);
	} else {
		totem_action_error_and_exit (_("An error occured"),
				message, totem);
	}
}

static void
on_speed_warning_event (BaconVideoWidget *bvw, Totem *totem)
{
	g_message ("TBD: Implement speed warning");
}

static void
on_buffering_event (BaconVideoWidget *bvw, int percentage, Totem *totem)
{
	totem_statusbar_push (TOTEM_STATUSBAR (totem->statusbar), percentage);
}

static void
update_seekable (Totem *totem, gboolean seekable)
{
	GtkWidget *widget;

	/* Check if the stream is seekable */
	gtk_widget_set_sensitive (totem->seek, seekable);
	gtk_widget_set_sensitive (totem->fs_seek, seekable);

	widget = glade_xml_get_widget (totem->xml, "tmw_seek_hbox");
	gtk_widget_set_sensitive (widget, seekable);

	widget = glade_xml_get_widget (totem->xml, "tcw_time_hbox");
	gtk_widget_set_sensitive (widget, seekable);

	widget = glade_xml_get_widget (totem->xml,
			"tmw_skip_forward_menu_item");
	gtk_widget_set_sensitive (widget, seekable);
	widget = glade_xml_get_widget (totem->xml,
			"tmw_skip_backwards_menu_item");
	gtk_widget_set_sensitive (widget, seekable);
	widget = glade_xml_get_widget (totem->xml, "trcm_skip_forward");
	gtk_widget_set_sensitive (widget, seekable);
	widget = glade_xml_get_widget (totem->xml, "trcm_skip_backwards");
	gtk_widget_set_sensitive (widget, seekable);
	widget = glade_xml_get_widget (totem->xml, "tmw_skip_to_menu_item");
	gtk_widget_set_sensitive (widget, seekable);
	widget = glade_xml_get_widget (totem->xml, "tstw_ok_button");
	gtk_widget_set_sensitive (widget, seekable);
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

		gtk_adjustment_set_value (totem->seekadj,
				current_position * 65535);
		gtk_adjustment_set_value (totem->fs_seekadj,
				current_position * 65535);

		totem_time_label_set_time
			(TOTEM_TIME_LABEL (totem->tcw_time_label),
			 current_time, stream_length);
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

	volume = bacon_video_widget_get_volume (totem->bvw);

	if (totem->volume_first_time || (totem->prev_volume != volume &&
				totem->prev_volume != -1 && volume != -1))
	{
		totem->volume_first_time = 0;
		gtk_adjustment_set_value (totem->voladj,
				(float) volume);
		gtk_adjustment_set_value (totem->fs_voladj,
				(float) volume);
	}

	totem->prev_volume = volume;
}

static int
gui_update_cb (Totem *totem)
{
	if (totem->bvw == NULL)
		return TRUE;

	update_volume_sliders (totem);

	return TRUE;
}

static gboolean
seek_slider_pressed_cb (GtkWidget *widget, GdkEventButton *event, Totem *totem)
{
	totem->seek_lock = TRUE;
	totem_statusbar_set_seeking (TOTEM_STATUSBAR (totem->statusbar), TRUE);
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
}

static gboolean
seek_slider_released_cb (GtkWidget *widget, GdkEventButton *event, Totem *totem)
{
	if (g_object_get_data (G_OBJECT (widget), "fs") != FALSE)
	{
		totem_action_seek (totem,
				gtk_adjustment_get_value (totem->fs_seekadj) / 65535);
		/* Update the fullscreen seek adjustment */
		gtk_adjustment_set_value (totem->seekadj,
				gtk_adjustment_get_value
				(totem->fs_seekadj));
	} else {
		totem_action_seek (totem,
				gtk_adjustment_get_value (totem->seekadj) / 65535);
		/* Update the seek adjustment */
		gtk_adjustment_set_value (totem->fs_seekadj,
				gtk_adjustment_get_value
				(totem->seekadj));
	}

	totem->seek_lock = FALSE;
	totem_statusbar_set_seeking (TOTEM_STATUSBAR (totem->statusbar), FALSE);
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
			gtk_adjustment_set_value (totem->voladj, 
					gtk_adjustment_get_value
					(totem->fs_voladj));
		} else {
			bacon_video_widget_set_volume
				(totem->bvw, (gint) totem->voladj->value);
			/* Update the volume adjustment */
			gtk_adjustment_set_value (totem->fs_voladj, 
					gtk_adjustment_get_value
					(totem->voladj));

		}

		totem->vol_lock = FALSE;
	}
}

static void
totem_action_add_recent (Totem *totem, const char *filename)
{
	EggRecentItem *item;

	if (strstr (filename, "file:///") == NULL)
		return;

	item = egg_recent_item_new_from_uri (filename);
	egg_recent_item_add_group (item, "Totem");
	egg_recent_model_add_full (totem->recent_model, item);
}

static void
totem_add_cd_track_name (Totem *totem, const char *filename)
{
	char *name;

	bacon_video_widget_open (totem->bvw, filename, NULL);
	name = totem_get_nice_name_for_stream (totem);
	bacon_video_widget_close (totem->bvw);
	totem_playlist_add_mrl (totem->playlist, filename, name);
	g_free (name);
}

static gboolean
totem_action_open_files (Totem *totem, char **list)
{
	GSList *slist = NULL;
	int i, retval;

	for (i =0 ; list[i] != NULL; i++)
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
	gboolean cleared = FALSE;

	if (list == NULL)
		return cleared;

	for (l = list ; l != NULL; l = l->next)
	{
		char *filename, *local_path;
		char *data = l->data;

		if (data == NULL)
			continue;

		/* Ignore relatives paths that start with "--", tough luck */
		if (data[0] == '-' && data[1] == '-')
			continue;

		/* Get the subtitle part out for our tests */
		filename = totem_create_full_path (data);
		local_path = gnome_vfs_get_local_path_from_uri (filename);
		if (local_path != NULL && g_file_test (local_path, G_FILE_TEST_IS_DIR))
		{
			g_free (local_path);
			continue;
		} else if (local_path != NULL && g_file_test (local_path, G_FILE_TEST_EXISTS) == FALSE)
		{
			g_free (local_path);
			continue;
		}

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
				totem_playlist_clear (totem->playlist);
				bacon_video_widget_close (totem->bvw);
				cleared = TRUE;
			}

			if (strcmp (data, "dvd:") == 0)
			{
				totem_action_load_media (totem, MEDIA_TYPE_DVD);
			} else if (strcmp (data, "vcd:") == 0) {
				totem_action_load_media (totem, MEDIA_TYPE_VCD);
			} else if (strcmp (data, "cd:") == 0) {
				totem_action_load_media (totem, MEDIA_TYPE_CDDA);
			} else if (strstr (filename, "cdda:/") != NULL) {
				totem_add_cd_track_name (totem, filename);
			} else if (totem_playlist_add_mrl (totem->playlist,
						filename, NULL) != FALSE) {
				totem_action_add_recent (totem, filename);
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

	return cleared;
}

static void
on_open1_activate (GtkButton *button, Totem *totem)
{
	GtkWidget *fs;
	int response;
	static char *path = NULL;

	fs = gtk_file_chooser_dialog_new (_("Select files"),
			GTK_WINDOW (totem->win), GTK_FILE_CHOOSER_ACTION_OPEN,
			GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
			GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT,
			NULL);
	gtk_dialog_set_default_response (GTK_DIALOG (fs), GTK_RESPONSE_ACCEPT);

	gtk_file_chooser_set_select_multiple (GTK_FILE_CHOOSER (fs), TRUE);
	gtk_file_chooser_set_local_only (GTK_FILE_CHOOSER (fs), FALSE);

	if (path != NULL)
	{
		gtk_file_chooser_set_uri (GTK_FILE_CHOOSER (fs), path);
		g_free (path);
		path = NULL;
	}

	while (1)
	{
		GSList *filenames;
		char *mrl;
		gboolean playlist_modified;

		response = gtk_dialog_run (GTK_DIALOG (fs));
		if (response != GTK_RESPONSE_ACCEPT)
			break;

		filenames = gtk_file_chooser_get_uris (GTK_FILE_CHOOSER (fs));
		if (filenames == NULL)
			continue;

		playlist_modified = totem_action_open_files_list (totem,
				filenames);
		if (playlist_modified == FALSE)
		{
			g_slist_foreach (filenames, (GFunc) g_free, NULL);
			g_slist_free (filenames);
			continue;
		}

		/* Hide the selection widget only if playlist is modified */
		gtk_widget_hide (fs);

		if (filenames->data != NULL)
		{
			char *tmp;

			tmp = g_path_get_dirname (filenames->data);
			path = g_strconcat (tmp, G_DIR_SEPARATOR_S, NULL);
			g_free (tmp);
		}
		g_slist_foreach (filenames, (GFunc) g_free, NULL);
		g_slist_free (filenames);

		mrl = totem_playlist_get_current_mrl (totem->playlist);
		totem_action_set_mrl_and_play (totem, mrl);
		g_free (mrl);
		break;
	}

	gtk_widget_destroy (fs);
}

static void
on_open_location1_activate (GtkButton *button, Totem *totem)
{
	GladeXML *glade;
	char *filename, *mrl;
	GtkWidget *dialog, *entry;
	int response;
	const char *filenames[2];

	filename = g_build_filename (DATADIR,
			"totem", "uri.glade", NULL);
	if (g_file_test (filename, G_FILE_TEST_EXISTS) == FALSE)
	{
		g_free (filename);
		totem_action_error (_("Couldn't load the 'Open Location...' interface."), _("Make sure that Totem is properly installed."), totem);
		return;
	}

	glade = glade_xml_new (filename, NULL, NULL);
	if (glade == NULL)
	{
		g_free (filename);
		totem_action_error (_("Couldn't load the 'Open Location...' interface."), _("Make sure that Totem is properly installed."), totem);
		return;
	}

	g_free (filename);
	dialog = glade_xml_get_widget (glade, "open_uri_dialog");
	entry = glade_xml_get_widget (glade, "uri");

	response = gtk_dialog_run (GTK_DIALOG (dialog));

	if (response == GTK_RESPONSE_OK)
	{
		const char *uri;

		uri = gtk_entry_get_text (GTK_ENTRY (entry));
		if (uri != NULL && strcmp (uri, "") != 0)
		{
			filenames[0] = uri;
			filenames[1] = NULL;
			totem_action_open_files (totem, (char **) filenames);

			mrl = totem_playlist_get_current_mrl (totem->playlist);
			totem_action_set_mrl_and_play (totem, mrl);
			g_free (mrl);
		}
	}

	gtk_widget_destroy (dialog);
	g_object_unref (glade);
}

static void
on_play_disc1_activate (GtkButton *button, Totem *totem)
{
	MediaType type;
	GError *error = NULL;
	const gchar *device;

	device = gconf_client_get_string (totem->gc,
					  GCONF_PREFIX"/mediadev", NULL);
	type = cd_detect_type (device, &error);
	switch (type) {
		case MEDIA_TYPE_ERROR:
			totem_action_error ("Failed to play Audio/Video Disc",
					    error ? error->message : "Reason unknown",
					    totem);
			return;
		case MEDIA_TYPE_DATA:
			/* Maybe set default location to the mountpoint of
			 * this device?... */
			on_open1_activate (button, totem);
			return;
		case MEDIA_TYPE_DVD:
		case MEDIA_TYPE_VCD:
		case MEDIA_TYPE_CDDA:
			totem_action_play_media (totem, type);
			break;
		default:
			g_assert_not_reached ();
	}
}

static void
on_eject1_activate (GtkButton *button, Totem *totem)
{
	totem_action_eject (totem);
}

static void
on_play1_activate (GtkButton *button, Totem *totem)
{
	totem_action_play_pause (totem);
}

static void
on_full_screen1_activate (GtkButton *button, Totem *totem)
{
	totem_action_fullscreen_toggle (totem);
}

static void
on_zoom_1_2_activate (GtkButton *button, Totem *totem)
{
	totem_action_set_scale_ratio (totem, 0.5); 
}

static void
on_zoom_1_1_activate (GtkButton *button, Totem *totem)
{
	totem_action_set_scale_ratio (totem, 1);
}

static void
on_zoom_2_1_activate (GtkButton *button, Totem *totem)
{                       
	totem_action_set_scale_ratio (totem, 2);
}

static void
on_aspect_ratio_auto_activate (GtkButton *button, Totem *totem)
{
	totem_action_set_aspect_ratio (totem, BVW_RATIO_AUTO);
}

static void
on_aspect_ratio_square_activate (GtkButton *button, Totem *totem)
{
	totem_action_set_aspect_ratio (totem, BVW_RATIO_SQUARE);
}

static void
on_aspect_ratio_fbt_activate (GtkButton *button, Totem *totem)
{
	totem_action_set_aspect_ratio (totem, BVW_RATIO_FOURBYTHREE);
}

static void
on_aspect_anamorphic_activate (GtkButton *button, Totem *totem)
{
	totem_action_set_aspect_ratio (totem, BVW_RATIO_ANAMORPHIC);
}

static void
on_aspect_ratio_dvb_activate (GtkButton *button, Totem *totem)
{
	totem_action_set_aspect_ratio (totem, BVW_RATIO_DVB);
}

static void
on_show_playlist1_activate (GtkButton *button, Totem *totem)
{
	action_toggle_playlist (totem);
}

static void
on_fs_exit1_activate (GtkButton *button, Totem *totem)
{
	totem_action_fullscreen_toggle (totem);
}

static void
on_quit1_activate (GtkButton *button, Totem *totem)
{
	totem_action_exit (totem);
}

static void
on_repeat_mode1_toggled (GtkCheckMenuItem *checkmenuitem, Totem *totem)
{
	totem_playlist_set_repeat (totem->playlist,
			gtk_check_menu_item_get_active (checkmenuitem));
}

static void
on_shuffle_mode1_toggled (GtkCheckMenuItem *checkmenuitem, Totem *totem)
{
	totem_playlist_set_shuffle (totem->playlist,
			gtk_check_menu_item_get_active (checkmenuitem));
}

static void
on_always_on_top1_activate (GtkCheckMenuItem *checkmenuitem, Totem *totem)
{
	totem_gdk_window_set_always_on_top (GTK_WIDGET (totem->win)->window,
			gtk_check_menu_item_get_active (checkmenuitem));
	gconf_client_set_bool (totem->gc,
			GCONF_PREFIX"/window_on_top",
			gtk_check_menu_item_get_active (checkmenuitem), NULL);
}

static void
show_controls (Totem *totem, gboolean visible, gboolean fullscreen_behaviour)
{
	GtkWidget *menubar, *controlbar, *statusbar, *item, *bvw_vbox;
	GtkRequisition requisition;
	
	menubar = glade_xml_get_widget (totem->xml, "tmw_menubar");
	controlbar = glade_xml_get_widget (totem->xml, "tmw_controls_vbox");
	statusbar = glade_xml_get_widget (totem->xml, "tmw_statusbar");
	item = glade_xml_get_widget (totem->xml, "trcm_show_controls");
	bvw_vbox = glade_xml_get_widget (totem->xml, "tmw_bvw_vbox");

	if (visible)
	{
		gtk_widget_show (menubar);
		gtk_widget_show (controlbar);
		gtk_widget_show (statusbar);
		gtk_widget_hide (item);
		gtk_container_set_border_width (GTK_CONTAINER (bvw_vbox), 1);
	} else {
		gtk_widget_hide (menubar);
		gtk_widget_hide (controlbar);
		gtk_widget_hide (statusbar);
		 /* We won't show controls in fullscreen */
		if (totem->controls_visibility == TOTEM_CONTROLS_FULLSCREEN)
			gtk_widget_hide (item);
		else
			gtk_widget_show (item);
		gtk_container_set_border_width (GTK_CONTAINER (bvw_vbox), 0);
	}
	
	/* If we are called from fullscreen handlers
	we do not handle the window's size */
	if (fullscreen_behaviour)
		return;
	
	if (totem->controls_visibility == TOTEM_CONTROLS_HIDDEN)
	{
		gtk_window_resize (GTK_WINDOW(totem->win),
			GTK_WIDGET(totem->bvw)->allocation.width,
			GTK_WIDGET(totem->bvw)->allocation.height);
	} else if (totem->controls_visibility == TOTEM_CONTROLS_VISIBLE) {
		/* We get GtkWindow requisition then we substract
		bvw's requisition to get other widget's height and
		use that to resize properly GtkWindow */
		gtk_widget_size_request (totem->win, &requisition);
		/* Getting controls requisition */
		requisition.height = requisition.height
			- GTK_WIDGET(totem->bvw)->requisition.height;
		requisition.width = requisition.width
			- GTK_WIDGET(totem->bvw)->requisition.width;
		gtk_window_resize (GTK_WINDOW(totem->win),
				GTK_WIDGET(totem->bvw)->allocation.width
				+ requisition.width,
				GTK_WIDGET(totem->bvw)->allocation.height
				+ requisition.height);
	}
}

static void
on_show_controls1_activate (GtkCheckMenuItem *checkmenuitem, Totem *totem)
{
	gboolean show;

	show = gtk_check_menu_item_get_active (checkmenuitem);

	/* Let's update our controls visibility */
	if (show)
	{
		totem->controls_visibility = TOTEM_CONTROLS_VISIBLE;
	} else {
		totem->controls_visibility = TOTEM_CONTROLS_HIDDEN;
	}
	show_controls (totem, show, FALSE);
}

static void
on_show_controls2_activate (GtkMenuItem *menuitem, Totem *totem)
{
	GtkWidget *item;

	item = glade_xml_get_widget (totem->xml, "tmw_show_controls_menu_item");
	gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (item), TRUE);
}

#ifndef HAVE_GTK_ONLY
static void
on_help_activate (GtkButton *button, Totem *totem)
{
	GError *err = NULL;

	if (gnome_help_display ("totem.xml", NULL, &err) == FALSE)
	{
		totem_action_error (_("Totem could not display the help contents."), err->message, totem);
		g_error_free (err);
	}
}

static void
on_about1_activate (GtkButton *button, Totem *totem)
{
	static GtkWidget *about = NULL;
	GdkPixbuf *pixbuf = NULL;
	const gchar *authors[] =
	{
		"Bastien Nocera <hadess@hadess.net>",
		"Julien Moutte <julien@moutte.net> (GStreamer backend)",
		"Guenter Bartsch <guenter@users.sourceforge.net>",
		"Ronald Bultje <rbultje@ronald.bitfreak.net>",
		NULL
	};
	const gchar *documenters[] = { NULL };
	const gchar *translator_credits = _("translator_credits");
	char *backend_version, *description;

	if (about != NULL)
	{
		gdk_window_raise (about->window);
		gdk_window_show (about->window);
		return;
	}

	{
		char *filename = NULL;

		filename = gnome_program_locate_file (NULL,
				GNOME_FILE_DOMAIN_APP_DATADIR,
				"totem/media-player-48.png",
				TRUE, NULL);

		if (filename != NULL)
		{
			pixbuf = gdk_pixbuf_new_from_file(filename, NULL);
			g_free (filename);
		}
	}

	backend_version = bacon_video_widget_get_backend_name (totem->bvw);
	description = g_strdup_printf (_("Movie Player using %s"),
				backend_version);

	about = gnome_about_new(_("Totem"), VERSION,
			"Copyright \xc2\xa9 2002-2003 Bastien Nocera",
			(const char *)description,
			(const char **)authors,
			(const char **)documenters,
			strcmp (translator_credits, "translator_credits") != 0 ? translator_credits : NULL,
			pixbuf);

	g_free (backend_version);
	g_free (description);

	if (pixbuf != NULL)
		gdk_pixbuf_unref (pixbuf);

	g_signal_connect (G_OBJECT (about), "destroy", G_CALLBACK
			(gtk_widget_destroyed), &about);
	g_object_add_weak_pointer (G_OBJECT (about), (gpointer *)&about);
	gtk_window_set_transient_for (GTK_WINDOW (about),
			GTK_WINDOW (totem->win));

	gtk_widget_show(about);
}
#endif /* !HAVE_GTK_ONLY */

static void
on_take_screenshot1_activate (GtkButton *button, Totem *totem)
{
	GdkPixbuf *pixbuf;
	GtkWidget *dialog;
	char *filename;
	GError *err = NULL;

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
		totem_action_error (_("Totem could not get a screenshot of that film."), _("Please file a bug, this isn't supposed to happen."), totem);
		return;
	}

	filename = g_build_filename (DATADIR,
			"totem", "screenshot.glade", NULL);

	dialog = totem_screenshot_new (filename, pixbuf);
	g_free (filename);
	gtk_dialog_run (GTK_DIALOG (dialog));
	gtk_widget_destroy (dialog);
	gdk_pixbuf_unref (pixbuf);
}

static void
hide_props_dialog (GtkWidget *widget, int trash, gpointer user_data)
{
	gtk_widget_hide (widget);
}

static void
on_properties1_activate (GtkButton *button, Totem *totem)
{
	static GtkWidget *dialog;

	if (totem->properties == NULL)
	{
		totem_action_error (_("Totem couldn't show the movie properties window."), _("Make sure that Totem is correctly installed."), totem);
		return;
	}

	if (dialog != NULL)
	{
		gtk_widget_show_all (dialog);
		return;
	}

	dialog = gtk_dialog_new_with_buttons (_("Properties"),
			GTK_WINDOW (totem->win),
			GTK_DIALOG_DESTROY_WITH_PARENT
			| GTK_DIALOG_NO_SEPARATOR,
			GTK_STOCK_CLOSE,
			GTK_RESPONSE_ACCEPT,
			NULL);
	gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox),
			totem->properties, TRUE, TRUE, 0);
	gtk_container_set_border_width (GTK_CONTAINER (dialog), 5);
	g_signal_connect (G_OBJECT (dialog), "response",
			G_CALLBACK (hide_props_dialog), NULL);
	g_signal_connect (G_OBJECT (dialog), "delete-event",
			G_CALLBACK (hide_props_dialog), NULL);
	gtk_widget_show_all (dialog);
}

static void
on_preferences1_activate (GtkButton *button, Totem *totem)
{
	gtk_widget_show (totem->prefs);
}

static void
on_dvd_root_menu1_activate (GtkButton *button, Totem *totem)
{
        bacon_video_widget_dvd_event (totem->bvw, BVW_DVD_ROOT_MENU);
}

static void
on_dvd_title_menu1_activate (GtkButton *button, Totem *totem)
{
        bacon_video_widget_dvd_event (totem->bvw, BVW_DVD_TITLE_MENU);
}

static void
on_dvd_audio_menu1_activate (GtkButton *button, Totem *totem)
{
        bacon_video_widget_dvd_event (totem->bvw, BVW_DVD_AUDIO_MENU);
}

static void
on_dvd_angle_menu1_activate (GtkButton *button, Totem *totem)
{
        bacon_video_widget_dvd_event (totem->bvw, BVW_DVD_ANGLE_MENU);
}

static void
on_dvd_chapter_menu1_activate (GtkButton *button, Totem *totem)
{
        bacon_video_widget_dvd_event (totem->bvw, BVW_DVD_CHAPTER_MENU);
}

static void
commit_hide_skip_to (GtkDialog *dialog, gint response, Totem *totem)

{
	GError *err = NULL;
	GtkWidget *spin;
	int sec;

	gtk_widget_hide (GTK_WIDGET (dialog));

	if (response != GTK_RESPONSE_OK)
		return;

	spin = glade_xml_get_widget (totem->xml, "tstw_skip_spinbutton");
	sec = (int) gtk_spin_button_get_value (GTK_SPIN_BUTTON (spin));

	bacon_video_widget_seek_time (totem->bvw, sec * 1000, &err);

	if (err != NULL)
	{
		char *msg, *disp;

		disp = gnome_vfs_unescape_string_for_display (totem->mrl);
		msg = g_strdup_printf(_("Totem could not seek in '%s'."), disp);
		g_free (disp);
		totem_action_stop (totem);
		totem_playlist_set_playing (totem->playlist, FALSE);
		if (bacon_video_widget_is_playing (totem->bvw) != FALSE)
			totem_action_stop (totem);
		totem_action_error (msg, err->message, totem);
		g_free (msg);
		g_error_free (err);
	}
}

static void
hide_skip_to (GtkWidget *widget, GdkEvent *event, gpointer user_data)
{
	gtk_widget_hide (widget);
}

static void
spin_button_value_changed_cb (GtkSpinButton *spinbutton, Totem *totem)
{
	GtkWidget *label;
	int sec;
	char *str;

	sec = (int) gtk_spin_button_get_value (GTK_SPIN_BUTTON (spinbutton));
	label = glade_xml_get_widget (totem->xml, "tstw_position_label");
	str = totem_time_to_string_text (sec * 1000);
	gtk_label_set_text (GTK_LABEL (label), str);
	g_free (str);
}

static void
on_skip_to1_activate (GtkButton *button, Totem *totem)
{
	GtkWidget *dialog;

	dialog = glade_xml_get_widget (totem->xml, "totem_skip_to_window");
	gtk_widget_show (dialog);
}

static void
on_skip_forward1_activate (GtkButton *button, Totem *totem)
{
	totem_action_seek_relative (totem, SEEK_FORWARD_OFFSET);
}

static void
on_skip_backwards1_activate (GtkButton *button, Totem *totem)
{
	totem_action_seek_relative (totem, SEEK_BACKWARD_OFFSET);
}

static void
on_volume_up1_activate (GtkButton *button, Totem *totem)
{
	totem_action_volume_relative (totem, VOLUME_UP_OFFSET);
}

static void
on_volume_down1_activate (GtkButton *button, Totem *totem)
{
	totem_action_volume_relative (totem, VOLUME_DOWN_OFFSET);
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
	switch (cmd) {
	case TOTEM_REMOTE_COMMAND_PLAY:
		totem_action_play (totem);
		break;
	case TOTEM_REMOTE_COMMAND_PAUSE:
		totem_action_play_pause (totem);
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
		if (url != NULL) {
			if (totem_playlist_add_mrl (totem->playlist, url, NULL) != FALSE) {
				totem_action_add_recent (totem, url);
			}
		}
		break;
	case TOTEM_REMOTE_COMMAND_REPLACE:
		if (url != NULL)
		{
			totem_playlist_clear (totem->playlist);
			if (g_str_has_prefix (url, "dvd:"))
			{
				totem_action_play_media (totem, MEDIA_TYPE_DVD);
			} else if (g_str_has_prefix (url, "vcd:") != FALSE) {
				totem_action_play_media (totem, MEDIA_TYPE_VCD);
			} else if (g_str_has_prefix (url, "cd:") != FALSE) {
				totem_action_play_media (totem, MEDIA_TYPE_CDDA);
			} else if (g_str_has_prefix (url, "cdda:/") != FALSE) {
				totem_add_cd_track_name (totem, url);
			} else if (totem_playlist_add_mrl (totem->playlist,
						url, NULL) != FALSE) {
				totem_action_add_recent (totem, url);
			}	
		}
		break;
	case TOTEM_REMOTE_COMMAND_SHOW:
		gtk_window_present (GTK_WINDOW (totem->win));
		break;
	case TOTEM_REMOTE_COMMAND_TOGGLE_CONTROLS:
		if (totem->controls_visibility != TOTEM_CONTROLS_FULLSCREEN)
		{
			GtkCheckMenuItem *item;
			gboolean value;

			item = GTK_CHECK_MENU_ITEM (glade_xml_get_widget
					(totem->xml,
					 "tmw_show_controls_menu_item"));
			value = gtk_check_menu_item_get_active (item);
			gtk_check_menu_item_set_active (item, !value);
		}
		break;
	default:
		break;
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

static int
toggle_playlist_from_playlist (GtkWidget *playlist, int trash, Totem *totem)
{
	GtkWidget *button;

	button = glade_xml_get_widget (totem->xml, "tmw_playlist_button");
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), FALSE);

	return TRUE;
}

static void
playlist_changed_cb (GtkWidget *playlist, Totem *totem)
{
	char *mrl;

	update_buttons (totem);
	mrl = totem_playlist_get_current_mrl (totem->playlist);

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
	GtkWidget *item;

	item = glade_xml_get_widget (totem->xml, "tmw_repeat_mode_menu_item");

	g_signal_handlers_disconnect_by_func (G_OBJECT (item),
			on_repeat_mode1_toggled, totem);

	gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (item), repeat);

	g_signal_connect (G_OBJECT (item), "toggled",
			G_CALLBACK (on_repeat_mode1_toggled), totem);
}

static void
playlist_shuffle_toggle_cb (TotemPlaylist *playlist, gboolean shuffle, Totem *totem)
{
	GtkWidget *item;

	item = glade_xml_get_widget (totem->xml, "tmw_shuffle_mode_menu_item");

	g_signal_handlers_disconnect_by_func (G_OBJECT (item),
			on_shuffle_mode1_toggled, totem);

	gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (item), shuffle);

	g_signal_connect (G_OBJECT (item), "toggled",
			G_CALLBACK (on_shuffle_mode1_toggled), totem);
}

static void
update_fullscreen_size (Totem *totem)
{
	gdk_screen_get_monitor_geometry (gdk_screen_get_default (),
			gdk_screen_get_monitor_at_window
			(gdk_screen_get_default (),
			 totem->win->window),
			&totem->fullscreen_rect);
}

static gboolean
totem_is_fullscreen (Totem *totem)
{
	return (totem->controls_visibility == TOTEM_CONTROLS_FULLSCREEN);
}

static void
move_popups (Totem *totem)
{
	int control_width, control_height;
	int exit_width, exit_height;

	gtk_window_get_size (GTK_WINDOW (totem->control_popup),
			&control_width, &control_height);
	gtk_window_get_size (GTK_WINDOW (totem->exit_popup),
			&exit_width, &exit_height);

	if (gtk_widget_get_direction (totem->exit_popup) == GTK_TEXT_DIR_RTL)
	{
		gtk_window_move (GTK_WINDOW (totem->exit_popup),
				totem->fullscreen_rect.width - exit_width,
				totem->fullscreen_rect.y);
		gtk_window_move (GTK_WINDOW (totem->control_popup),
				totem->fullscreen_rect.width - control_width,
				totem->fullscreen_rect.height
				- control_height);
	} else {
		gtk_window_move (GTK_WINDOW (totem->exit_popup),
				totem->fullscreen_rect.x,
				totem->fullscreen_rect.y);
		gtk_window_move (GTK_WINDOW (totem->control_popup),
				totem->fullscreen_rect.x,
				totem->fullscreen_rect.height
				- control_height);
	}
}

static void
size_changed_cb (GdkScreen *screen, Totem *totem)
{
	update_fullscreen_size (totem);
	move_popups (totem);
}

static gboolean
popup_hide (Totem *totem)
{
	if (totem->bvw == NULL
			|| totem->controls_visibility
			!= TOTEM_CONTROLS_FULLSCREEN)
	{
		return TRUE;
	}

	if (totem->seek_lock != FALSE || totem->vol_fs_lock != FALSE)
		return TRUE;

	gtk_widget_hide (GTK_WIDGET (totem->exit_popup));
	gtk_widget_hide (GTK_WIDGET (totem->control_popup));

	if (totem->popup_timeout != 0)
	{
		gtk_timeout_remove (totem->popup_timeout);
		totem->popup_timeout = 0;
	}

	bacon_video_widget_set_show_cursor (totem->bvw, FALSE);

	return FALSE;
}

static void
on_mouse_click_fullscreen (GtkWidget *widget, Totem *totem)
{
	if (totem->popup_timeout != 0)
	{
		gtk_timeout_remove (totem->popup_timeout);
		totem->popup_timeout = 0;
	}

	totem->popup_timeout = gtk_timeout_add (2000,
		(GtkFunction) popup_hide, totem);
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

	if (totem->popup_timeout != 0)
	{
		gtk_timeout_remove (totem->popup_timeout);
		totem->popup_timeout = 0;
	}

	item = glade_xml_get_widget (totem->xml, "tcw_vbox");
	gtk_widget_show_all (item);
	item = glade_xml_get_widget (totem->xml, "tmw_title_label");
	gtk_widget_realize (item);
	gdk_flush ();

	move_popups (totem);

	gtk_widget_show_all (totem->exit_popup);
	gtk_widget_show_all (totem->control_popup);
	bacon_video_widget_set_show_cursor (totem->bvw, TRUE);

	totem->popup_timeout = gtk_timeout_add (5000,
			(GtkFunction) popup_hide, totem);
	totem->popup_in_progress = FALSE;

	return FALSE;
}

static gboolean
on_video_button_press_event (BaconVideoWidget *bvw, GdkEventButton *event,
		Totem *totem)
{
	if (event->type == GDK_BUTTON_PRESS && event->button == 3)
	{
		GtkWidget *menu;

		menu = glade_xml_get_widget (totem->xml,
				"totem_right_click_menu");
		gtk_menu_popup (GTK_MENU (menu), NULL, NULL, NULL, NULL,
				event->button, event->time);

		return TRUE;
	}

	return FALSE;
}

static gboolean
on_eos_event (GtkWidget *widget, Totem *totem)
{
	if (strcmp (totem->mrl, LOGO_PATH) == 0)
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
		totem_action_set_mrl (totem, mrl);
		g_free (mrl);
	} else {
		totem_action_next (totem);
	}

	return FALSE;
}

static gboolean
totem_action_handle_key (Totem *totem, GdkEventKey *event)
{
	gboolean retval = TRUE;

	/* Alphabetical */
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
		bacon_video_widget_dvd_event (totem->bvw, BVW_DVD_CHAPTER_MENU);
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
	case GDK_f:
	case GDK_F:
		if (event->time - totem->keypress_time
				>= KEYBOARD_HYSTERISIS_TIMEOUT)
			totem_action_fullscreen_toggle (totem);

		totem->keypress_time = event->time;

		break;
	case GDK_h:
	case GDK_H:
		if (totem->controls_visibility != TOTEM_CONTROLS_FULLSCREEN)
		{
			GtkCheckMenuItem *item;
			gboolean value;

			item = GTK_CHECK_MENU_ITEM (glade_xml_get_widget
					(totem->xml,
					 "tmw_show_controls_menu_item"));
			value = gtk_check_menu_item_get_active (item);
			gtk_check_menu_item_set_active (item, !value);
		}
		break;
	case GDK_i:
	case GDK_I:
		{
			GtkCheckMenuItem *item;
			gboolean value;

			item = GTK_CHECK_MENU_ITEM (glade_xml_get_widget
					(totem->xml,
					 "tmw_deinterlace_menu_item"));
			value = gtk_check_menu_item_get_active (item);
			gtk_check_menu_item_set_active (item, !value);
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
		on_open1_activate (NULL, totem);
		break;
#ifdef HAVE_XFREE
	case XF86XK_AudioPlay:
	case XF86XK_AudioPause:
#endif /* HAVE_XFREE */
	case GDK_p:
	case GDK_P:
		/* Playlist keyboard shortcut? */
		if (event->state & GDK_CONTROL_MASK)
			action_toggle_playlist (totem);
		else
			totem_action_play_pause (totem);
		break;
	case GDK_q:
	case GDK_Q:
		totem_action_exit (totem);
		break;
	case GDK_s:
	case GDK_S:
		on_skip_to1_activate (NULL, totem);
		break;
	case GDK_Escape:
		totem_action_fullscreen (totem, FALSE);
		break;
	case GDK_Left:
		if (event->state & GDK_SHIFT_MASK)
		{
			totem_action_seek_relative (totem,
					SEEK_BACKWARD_SHORT_OFFSET);
		} else {
			totem_action_seek_relative (totem,
					SEEK_BACKWARD_OFFSET);
		}
		break;
	case GDK_Right:
		if (event->state & GDK_SHIFT_MASK)
		{
			totem_action_seek_relative (totem,
					SEEK_FORWARD_SHORT_OFFSET);
		} else {
			totem_action_seek_relative (totem, SEEK_FORWARD_OFFSET);
		}
		break;
	case GDK_space:
		if (totem->controls_visibility == TOTEM_CONTROLS_FULLSCREEN)
			totem_action_play_pause (totem);
		else
			retval = FALSE;
		break;
	case GDK_Up:
		totem_action_volume_relative (totem, VOLUME_UP_OFFSET);
		break;
	case GDK_Down:
		totem_action_volume_relative (totem, VOLUME_DOWN_OFFSET);
		break;
	case GDK_0:
	case GDK_onehalf:
		totem_action_set_scale_ratio (totem, 0.5);
		break;
	case GDK_1:
		totem_action_set_scale_ratio (totem, 1);
		break;
	case GDK_2:
		totem_action_set_scale_ratio (totem, 2);
		break;
	case GDK_F10:
		if (event->state & GDK_SHIFT_MASK)
		{
			GtkWidget *menu;
			menu = glade_xml_get_widget (totem->xml,
					"totem_right_click_menu");
			gtk_menu_popup (GTK_MENU (menu), NULL, NULL, NULL, NULL,
					0, event->time);
		} else {
			retval = FALSE;
		}
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
	/* Special case the Playlist and Eject keyboard shortcuts */
	if (event->state != 0
			&& (event->state &GDK_CONTROL_MASK))
	{
		switch (event->keyval)
		case GDK_p:
		case GDK_P:
		case GDK_E:
		case GDK_e:
		case GDK_O:
		case GDK_o:
		case GDK_L:
		case GDK_l:
			return totem_action_handle_key (totem, event);
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

	return totem_action_handle_key (totem, event);
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
        GtkWidget *item;
        gboolean playing;

	playing = totem_playing_dvd (totem);

	item = glade_xml_get_widget (totem->xml, "tmw_dvd_root_menu_item");
	gtk_widget_set_sensitive (item, playing);
        item = glade_xml_get_widget (totem->xml, "tmw_dvd_title_menu_item");
	gtk_widget_set_sensitive (item, playing);
        item = glade_xml_get_widget (totem->xml, "tmw_dvd_audio_menu_item");
	gtk_widget_set_sensitive (item, playing);
        item = glade_xml_get_widget (totem->xml, "tmw_dvd_angle_menu_item");
	gtk_widget_set_sensitive (item, playing);
        item = glade_xml_get_widget (totem->xml, "tmw_dvd_chapter_menu_item");
	gtk_widget_set_sensitive (item, playing);

	playing = totem_is_media (totem->mrl);

	item = glade_xml_get_widget (totem->xml, "tmw_eject_menu_item");
	gtk_widget_set_sensitive (item, playing);
}

static void
update_buttons (Totem *totem)
{
	GtkWidget *item;
	gboolean has_item;

	/* Previous */
	/* FIXME Need way to detect if DVD Title is at first chapter */
	if (totem_playing_dvd (totem) != FALSE)
	{
		has_item = TRUE;
	} else {
		has_item = totem_playlist_has_previous_mrl (totem->playlist);
	}

	item = glade_xml_get_widget (totem->xml, "tmw_previous_button");
	gtk_widget_set_sensitive (item, has_item);
	item = glade_xml_get_widget (totem->xml, "tcw_previous_button");
	gtk_widget_set_sensitive (item, has_item);
	item = glade_xml_get_widget (totem->xml,
			"tmw_previous_chapter_menu_item");
	gtk_widget_set_sensitive (item, has_item);
	item = glade_xml_get_widget (totem->xml, "trcm_previous_chapter");
	gtk_widget_set_sensitive (item, has_item);

	/* Next */
	/* FIXME Need way to detect if DVD Title has no more chapters */
	if (totem_playing_dvd (totem) != FALSE)
	{
		has_item = TRUE;
	} else {
		has_item = totem_playlist_has_next_mrl (totem->playlist);
	}

	item = glade_xml_get_widget (totem->xml, "tmw_next_button");
	gtk_widget_set_sensitive (item, has_item);
	item = glade_xml_get_widget (totem->xml, "tcw_next_button");
	gtk_widget_set_sensitive (item, has_item);
	item = glade_xml_get_widget (totem->xml, "tmw_next_chapter_menu_item");
	gtk_widget_set_sensitive (item, has_item);
	item = glade_xml_get_widget (totem->xml, "trcm_next_chapter");
	gtk_widget_set_sensitive (item, has_item);
}

static void
on_sub_activate (GtkButton *button, Totem *totem)
{
	int rank;
	gboolean is_set;

	rank = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (button), "rank"));
	is_set = gtk_check_menu_item_get_active (GTK_CHECK_MENU_ITEM (button));

	if (is_set != FALSE)
		bacon_video_widget_set_subtitle (totem->bvw, rank);
}

static void
on_lang_activate (GtkButton *button, Totem *totem)
{
	int rank;
	gboolean is_set;

	rank = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (button), "rank"));
	is_set = gtk_check_menu_item_get_active (GTK_CHECK_MENU_ITEM (button));

	if (is_set != FALSE)
		bacon_video_widget_set_language (totem->bvw, rank);
}

static GSList*
add_item_to_menu (Totem *totem, GtkWidget *menu, const char *lang,
		int current_lang, int selection, gboolean is_lang,
		GSList *group)
{
	GtkWidget *item;

	item = gtk_radio_menu_item_new_with_label (group, lang);
	gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (item),
			current_lang == selection ? TRUE : FALSE);
	gtk_widget_show (item);
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);

	g_object_set_data (G_OBJECT (item), "rank",
			GINT_TO_POINTER (selection));

	if (is_lang == FALSE)
		g_signal_connect (G_OBJECT (item), "activate",
				G_CALLBACK (on_sub_activate), totem);
	else
		g_signal_connect (G_OBJECT (item), "activate",
				G_CALLBACK (on_lang_activate), totem);

	return gtk_radio_menu_item_get_group (GTK_RADIO_MENU_ITEM (item));
}

static GtkWidget*
create_submenu (Totem *totem, GList *list, int current, gboolean is_lang)
{
	GtkWidget *menu = NULL;
	int i;
	GList *l;
	GSList *group = NULL;

	if (list == NULL)
	{
		menu = gtk_menu_new ();
		if (is_lang == FALSE)
		{
			group = add_item_to_menu (totem, menu, _("None"),
					current, -2, is_lang, group);
		}

		group = add_item_to_menu (totem, menu, _("Auto"),
				current, -1, is_lang, group);

		gtk_widget_show (menu);

		return menu;
	}

	i = 0;

	for (l = list; l != NULL; l = l->next)
	{
		if (menu == NULL)
		{
			menu = gtk_menu_new ();
			if (is_lang == FALSE)
			{
				group = add_item_to_menu (totem, menu,
						_("None"), current, -2,
						is_lang, group);
			}
			group = add_item_to_menu (totem, menu, _("Auto"),
					current, -1, is_lang, group);
		}

		group = add_item_to_menu (totem, menu, l->data, current,
				i, is_lang, group);
		i++;
	}

	gtk_widget_show (menu);

	return menu;
}

static void
update_dvd_menu_sub_lang (Totem *totem)
{
	GtkWidget *item, *submenu;
	GtkWidget *lang_menu, *sub_menu;
	GList *list;
	int current;

	lang_menu = NULL;
	sub_menu = NULL;

	list = bacon_video_widget_get_languages (totem->bvw);
	if (list != NULL)
	{
		current = bacon_video_widget_get_language (totem->bvw);
		lang_menu = create_submenu (totem, list, current, TRUE);
		totem_g_list_deep_free (list);
	}

	list = bacon_video_widget_get_subtitles (totem->bvw);
	if (list != NULL)
	{
		current = bacon_video_widget_get_subtitle (totem->bvw);
		sub_menu = create_submenu (totem, list, current, FALSE);
		totem_g_list_deep_free (list);
	}

	/* Subtitles */
	item = glade_xml_get_widget (totem->xml, "tmw_subtitles_menu_item");
	submenu = glade_xml_get_widget (totem->xml, "tmw_menu_subtitles");
	if (sub_menu == NULL)
	{
		gtk_menu_item_set_submenu (GTK_MENU_ITEM (item), submenu);
	} else {
		gtk_menu_item_set_submenu (GTK_MENU_ITEM (item), sub_menu);
		totem->subtitles = sub_menu;
	}

	/* Languages */
	item = glade_xml_get_widget (totem->xml, "tmw_languages_menu_item");
	submenu = glade_xml_get_widget (totem->xml, "tmw_menu_languages");
	if (lang_menu == NULL)
	{
		gtk_menu_item_set_submenu (GTK_MENU_ITEM (item), submenu);
	} else {
		gtk_menu_item_set_submenu (GTK_MENU_ITEM (item), lang_menu);
		totem->languages = lang_menu;
	}
}

static void
totem_setup_window (Totem *totem)
{
	GtkWidget *item;
	int w, h;

	w = gconf_client_get_int (totem->gc, GCONF_PREFIX"/window_w", NULL);
	h = gconf_client_get_int (totem->gc, GCONF_PREFIX"/window_h", NULL);

	item = glade_xml_get_widget (totem->xml, "tmw_bvw_vbox");

	if (w > 0 && h > 0)
		gtk_widget_set_size_request (item, w, h);
}

static void
totem_callback_connect (Totem *totem)
{
	GtkWidget *item;

	/* Menu items */
	item = glade_xml_get_widget (totem->xml, "tmw_open_menu_item");
	g_signal_connect (G_OBJECT (item), "activate",
			G_CALLBACK (on_open1_activate), totem);
	item = glade_xml_get_widget (totem->xml, "tmw_open_location_menu_item");
	g_signal_connect (G_OBJECT (item), "activate",
			G_CALLBACK (on_open_location1_activate), totem);
	item = glade_xml_get_widget (totem->xml, "tmw_play_disc_menu_item");
	g_signal_connect (G_OBJECT (item), "activate",
			G_CALLBACK (on_play_disc1_activate), totem);
	item = glade_xml_get_widget (totem->xml, "tmw_eject_menu_item");
	g_signal_connect (G_OBJECT (item), "activate",
			G_CALLBACK (on_eject1_activate), totem);
	item = glade_xml_get_widget (totem->xml, "tmw_play_menu_item");
	g_signal_connect (G_OBJECT (item), "activate",
			G_CALLBACK (on_play1_activate), totem);
	item = glade_xml_get_widget (totem->xml, "tmw_fullscreen_menu_item");
	g_signal_connect (G_OBJECT (item), "activate",
			G_CALLBACK (on_full_screen1_activate), totem);
	item = glade_xml_get_widget (totem->xml, "tmw_zoom_1_2_menu_item");
	g_signal_connect (G_OBJECT (item), "activate",
			G_CALLBACK (on_zoom_1_2_activate), totem);
	item = glade_xml_get_widget (totem->xml, "tmw_zoom_1_1_menu_item");
	g_signal_connect (G_OBJECT (item), "activate",
			G_CALLBACK (on_zoom_1_1_activate), totem);
	item = glade_xml_get_widget (totem->xml, "tmw_zoom_2_1_menu_item");
	g_signal_connect (G_OBJECT (item), "activate",
			G_CALLBACK (on_zoom_2_1_activate), totem);
	item = glade_xml_get_widget (totem->xml,
			"tmw_aspect_ratio_auto_menu_item");
	g_signal_connect (G_OBJECT (item), "activate",
			G_CALLBACK (on_aspect_ratio_auto_activate), totem);
	item = glade_xml_get_widget (totem->xml,
			"tmw_aspect_ratio_square_menu_item");
        g_signal_connect (G_OBJECT (item), "activate",
			G_CALLBACK (on_aspect_ratio_square_activate), totem);
	item = glade_xml_get_widget (totem->xml,
			"tmw_aspect_ratio_fbt_menu_item");
	g_signal_connect (G_OBJECT (item), "activate",
			G_CALLBACK (on_aspect_ratio_fbt_activate), totem);
	item = glade_xml_get_widget (totem->xml,
			"tmw_aspect_ratio_anamorphic_menu_item");
	g_signal_connect (G_OBJECT (item), "activate",
			G_CALLBACK (on_aspect_anamorphic_activate), totem);
	item = glade_xml_get_widget (totem->xml,
			"tmw_aspect_ratio_dvb_menu_item");
	g_signal_connect (G_OBJECT (item), "activate",
			G_CALLBACK (on_aspect_ratio_dvb_activate), totem);
	item = glade_xml_get_widget (totem->xml, "tmw_show_playlist_menu_item");
	g_signal_connect (G_OBJECT (item), "activate",
			G_CALLBACK (on_show_playlist1_activate), totem);
	item = glade_xml_get_widget (totem->xml, "tmw_repeat_mode_menu_item");
	gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (item),
			totem_playlist_get_repeat (totem->playlist));
	g_signal_connect (G_OBJECT (item), "toggled",
			G_CALLBACK (on_repeat_mode1_toggled), totem);
	item = glade_xml_get_widget (totem->xml, "tmw_shuffle_mode_menu_item");
	gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (item),
			totem_playlist_get_shuffle (totem->playlist));
	g_signal_connect (G_OBJECT (item), "toggled",
			G_CALLBACK (on_shuffle_mode1_toggled), totem);
	item = glade_xml_get_widget (totem->xml, "tmw_quit_menu_item");
	g_signal_connect (G_OBJECT (item), "activate",
			G_CALLBACK (on_quit1_activate), totem);

#ifndef HAVE_GTK_ONLY
	item = glade_xml_get_widget (totem->xml, "tmw_contents_menu_item");
	g_signal_connect (G_OBJECT (item), "activate",
			G_CALLBACK (on_help_activate), totem);
	item = glade_xml_get_widget (totem->xml, "tmw_about_menu_item");
	g_signal_connect (G_OBJECT (item), "activate",
			G_CALLBACK (on_about1_activate), totem);
#else
	item = glade_xml_get_widget (totem->xml, "tmw_menu_item_help");
	gtk_widget_hide (item);
#endif /* !HAVE_GTK_ONLY */

	item = glade_xml_get_widget (totem->xml,
			"tmw_take_screenshot_menu_item");
	g_signal_connect (G_OBJECT (item), "activate",
			G_CALLBACK (on_take_screenshot1_activate), totem);
	item = glade_xml_get_widget (totem->xml, "tmw_preferences_menu_item");
	g_signal_connect (G_OBJECT (item), "activate",
			G_CALLBACK (on_preferences1_activate), totem);
	item = glade_xml_get_widget (totem->xml, "tmw_properties_menu_item");
	g_signal_connect (G_OBJECT (item), "activate",
			G_CALLBACK (on_properties1_activate), totem);
	item = glade_xml_get_widget (totem->xml, "tmw_volume_up_menu_item");
	g_signal_connect (G_OBJECT (item), "activate",
			G_CALLBACK (on_volume_up1_activate), totem);
	item = glade_xml_get_widget (totem->xml, "tmw_volume_down_menu_item");
	g_signal_connect (G_OBJECT (item), "activate",
			G_CALLBACK (on_volume_down1_activate), totem);
	item = glade_xml_get_widget (totem->xml, "tmw_always_on_top_menu_item");
	g_signal_connect (G_OBJECT (item), "activate",
			G_CALLBACK (on_always_on_top1_activate), totem);
	item = glade_xml_get_widget (totem->xml, "tmw_show_controls_menu_item");
	g_signal_connect (G_OBJECT (item), "activate",
			G_CALLBACK (on_show_controls1_activate), totem);

	/* Popup menu */
	item = glade_xml_get_widget (totem->xml, "trcm_play");
	g_signal_connect (G_OBJECT (item), "activate",
			G_CALLBACK (on_play1_activate), totem);
	item = glade_xml_get_widget (totem->xml, "trcm_next_chapter");
	g_signal_connect (G_OBJECT (item), "activate",
			G_CALLBACK (on_next_button_clicked), totem);
	item = glade_xml_get_widget (totem->xml, "trcm_previous_chapter");
	g_signal_connect (G_OBJECT (item), "activate",
			G_CALLBACK (on_previous_button_clicked), totem);
	item = glade_xml_get_widget (totem->xml, "trcm_skip_forward");
	g_signal_connect (G_OBJECT (item), "activate",
			G_CALLBACK (on_skip_forward1_activate), totem);
	item = glade_xml_get_widget (totem->xml, "trcm_skip_backwards");
	g_signal_connect (G_OBJECT (item), "activate",
			G_CALLBACK (on_skip_backwards1_activate), totem);
	item = glade_xml_get_widget (totem->xml, "trcm_volume_up");
	g_signal_connect (G_OBJECT (item), "activate",
			G_CALLBACK (on_volume_up1_activate), totem);
	item = glade_xml_get_widget (totem->xml, "trcm_volume_down");
	g_signal_connect (G_OBJECT (item), "activate",
			G_CALLBACK (on_volume_down1_activate), totem);
	item = glade_xml_get_widget (totem->xml, "trcm_show_controls");
	g_signal_connect (G_OBJECT (item), "activate",
			G_CALLBACK (on_show_controls2_activate), totem);

	/* Controls */
	totem->pp_button = glade_xml_get_widget
		(totem->xml, "tmw_play_pause_button");
	g_signal_connect (G_OBJECT (totem->pp_button), "clicked",
			G_CALLBACK (on_play_pause_button_clicked), totem);
	item = glade_xml_get_widget (totem->xml, "tmw_previous_button");
	g_signal_connect (G_OBJECT (item), "clicked",
			G_CALLBACK (on_previous_button_clicked), totem);
	item = glade_xml_get_widget (totem->xml, "tmw_next_button");
	g_signal_connect (G_OBJECT (item), "clicked", 
			G_CALLBACK (on_next_button_clicked), totem);
	item = glade_xml_get_widget (totem->xml, "tmw_playlist_button");
	g_signal_connect (G_OBJECT (item), "clicked",
			G_CALLBACK (on_playlist_button_toggled), totem);
	item = glade_xml_get_widget (totem->xml, "tmw_volume_mute_button");
	g_signal_connect (G_OBJECT (item), "clicked",
			G_CALLBACK (on_volume_mute_button), totem);
	item = glade_xml_get_widget (totem->xml, "tmw_volume_max_button");
	g_signal_connect (G_OBJECT (item), "clicked",
			G_CALLBACK (on_volume_max_button), totem);

	/* Drag'n'Drop */
	item = glade_xml_get_widget (totem->xml, "tmw_playlist_button");
	g_signal_connect (G_OBJECT (item), "drag_data_received",
			G_CALLBACK (drop_playlist_cb), totem);
	gtk_drag_dest_set (item, GTK_DEST_DEFAULT_ALL,
			target_table, G_N_ELEMENTS (target_table),
			GDK_ACTION_COPY);

	/* Main Window */
	g_signal_connect (G_OBJECT (totem->win), "delete-event",
			G_CALLBACK (main_window_destroy_cb), totem);
	g_signal_connect (G_OBJECT (totem->win), "destroy",
			G_CALLBACK (main_window_destroy_cb), totem);
	g_object_notify (G_OBJECT (totem->win), "is-active");
	g_signal_connect_swapped (G_OBJECT (totem->win), "notify",
			G_CALLBACK (popup_hide), totem);

	/* Screen size changes */
	g_signal_connect (G_OBJECT (gdk_screen_get_default ()),
			"size-changed", G_CALLBACK (size_changed_cb), totem);

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
	g_signal_connect (G_OBJECT (totem->fs_pp_button), "clicked",
			G_CALLBACK (on_play_pause_button_clicked), totem);
	g_signal_connect (G_OBJECT (totem->fs_pp_button), "clicked",
			G_CALLBACK (on_mouse_click_fullscreen), totem);

	item = glade_xml_get_widget (totem->xml, "tcw_previous_button");
	g_signal_connect (G_OBJECT (item), "clicked",
			G_CALLBACK (on_previous_button_clicked), totem);
	g_signal_connect (G_OBJECT (item), "clicked",
			G_CALLBACK (on_mouse_click_fullscreen), totem);

	item = glade_xml_get_widget (totem->xml, "tcw_next_button");
	g_signal_connect (G_OBJECT (item), "clicked", 
			G_CALLBACK (on_next_button_clicked), totem);
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
	g_signal_connect (G_OBJECT(totem->fs_volume), "value-changed",
			G_CALLBACK (vol_cb), totem);
	g_signal_connect (G_OBJECT(totem->fs_volume), "button_press_event",
			G_CALLBACK (vol_slider_pressed_cb), totem);
	g_signal_connect (G_OBJECT(totem->fs_volume), "button_release_event",
			G_CALLBACK (vol_slider_released_cb), totem);

	/* Connect the keys */
	gtk_widget_add_events (totem->win, GDK_KEY_PRESS_MASK);
	g_signal_connect (G_OBJECT(totem->win), "key_press_event",
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

	/* Playlist Disappearance, woop woop */
	g_signal_connect (G_OBJECT (totem->playlist),
			"response", G_CALLBACK (toggle_playlist_from_playlist),
			totem);
	g_signal_connect (G_OBJECT (totem->playlist), "delete-event",
			G_CALLBACK (toggle_playlist_from_playlist),
			totem);

	/* Playlist */
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

	/* DVD menu callbacks */
	item = glade_xml_get_widget (totem->xml, "tmw_dvd_root_menu_item");
	g_signal_connect (G_OBJECT (item), "activate",
			G_CALLBACK (on_dvd_root_menu1_activate), totem);
	item = glade_xml_get_widget (totem->xml, "tmw_dvd_title_menu_item");
	g_signal_connect (G_OBJECT (item), "activate",
			G_CALLBACK (on_dvd_title_menu1_activate), totem);
	item = glade_xml_get_widget (totem->xml, "tmw_dvd_audio_menu_item");
	g_signal_connect (G_OBJECT (item), "activate",
			G_CALLBACK (on_dvd_audio_menu1_activate), totem);
	item = glade_xml_get_widget (totem->xml, "tmw_dvd_angle_menu_item");
	g_signal_connect (G_OBJECT (item), "activate",
			G_CALLBACK (on_dvd_angle_menu1_activate), totem);
	item = glade_xml_get_widget (totem->xml, "tmw_dvd_chapter_menu_item");
	g_signal_connect (G_OBJECT (item), "activate",
			G_CALLBACK (on_dvd_chapter_menu1_activate), totem);
	item = glade_xml_get_widget (totem->xml, "tmw_skip_to_menu_item");
	g_signal_connect (G_OBJECT (item), "activate",
			G_CALLBACK (on_skip_to1_activate), totem);
	item = glade_xml_get_widget (totem->xml, "tmw_next_chapter_menu_item");
	g_signal_connect (G_OBJECT (item), "activate",
			G_CALLBACK (on_next_button_clicked), totem);
	item = glade_xml_get_widget (totem->xml,
			"tmw_previous_chapter_menu_item");
	g_signal_connect (G_OBJECT (item), "activate",
			G_CALLBACK (on_previous_button_clicked), totem);
	item = glade_xml_get_widget (totem->xml, "tmw_skip_forward_menu_item");
	g_signal_connect (G_OBJECT (item), "activate",
			G_CALLBACK (on_skip_forward1_activate), totem);
	item = glade_xml_get_widget (totem->xml,
			"tmw_skip_backwards_menu_item");
	g_signal_connect (G_OBJECT (item), "activate",
			G_CALLBACK (on_skip_backwards1_activate), totem);

	/* Skip dialog */
	item = glade_xml_get_widget (totem->xml, "totem_skip_to_window");
	g_signal_connect (G_OBJECT (item), "response",
			G_CALLBACK (commit_hide_skip_to), totem);
	g_signal_connect (G_OBJECT (item), "delete-event",
			G_CALLBACK (hide_skip_to), totem);
	item = glade_xml_get_widget (totem->xml, "tstw_skip_spinbutton");
	g_signal_connect (G_OBJECT (item), "value-changed",
			G_CALLBACK (spin_button_value_changed_cb), totem);

	/* Subtitle and Languages submenu */
	item = glade_xml_get_widget (totem->xml, "tmw_menu_languages");
	g_object_ref (item);
	item = glade_xml_get_widget (totem->xml, "tmw_menu_subtitles");
	g_object_ref (item);

	/* Named icon support */
	totem_set_default_icons (totem);

	/* Update the UI */
	gtk_timeout_add (600, (GtkFunction) gui_update_cb, totem);
}

static void
video_widget_create (Totem *totem) 
{
	GError *err = NULL;
	GtkWidget *container;
	int w, h;

	totem->scr = scrsaver_new ();

	w = gconf_client_get_int (totem->gc, GCONF_PREFIX"/window_w", NULL);
	h = gconf_client_get_int (totem->gc, GCONF_PREFIX"/window_h", NULL);

	totem->bvw = BACON_VIDEO_WIDGET
		(bacon_video_widget_new (w, h, FALSE, &err));

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

	/* Let's set a name. Will make debugging easier */
	gtk_widget_set_name (GTK_WIDGET(totem->bvw), "bvw");

	g_signal_connect (G_OBJECT (totem->bvw),
			"motion-notify-event",
			G_CALLBACK (on_video_motion_notify_event),
			totem);
	g_signal_connect (G_OBJECT (totem->bvw),
			"button-press-event",
			G_CALLBACK (on_video_button_press_event),
			totem);
	g_signal_connect (G_OBJECT (totem->bvw),
			"eos",
			G_CALLBACK (on_eos_event),
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
	g_signal_connect (G_OBJECT (totem->bvw),
			"speed-warning",
			G_CALLBACK (on_speed_warning_event),
			totem);

	container = glade_xml_get_widget (totem->xml, "tmw_bvw_vbox");
	gtk_container_add (GTK_CONTAINER (container),
			GTK_WIDGET (totem->bvw));

	/* Events for the widget video window as well */
	gtk_widget_add_events (GTK_WIDGET (totem->bvw), GDK_KEY_PRESS_MASK);
	g_signal_connect (G_OBJECT(totem->bvw), "key_press_event",
			G_CALLBACK (on_window_key_press_event), totem);

	g_signal_connect (G_OBJECT (totem->bvw), "drag_data_received",
			G_CALLBACK (drop_video_cb), totem);
	gtk_drag_dest_set (GTK_WIDGET (totem->bvw), GTK_DEST_DEFAULT_ALL,
			target_table, 1, GDK_ACTION_COPY);

	g_signal_connect (G_OBJECT (totem->bvw), "drag_data_get",
			G_CALLBACK (drag_video_cb), totem);
	gtk_drag_source_set (GTK_WIDGET (totem->bvw),
			GDK_BUTTON1_MASK | GDK_BUTTON3_MASK,
			source_table, G_N_ELEMENTS (source_table),
			GDK_ACTION_LINK);

	g_object_add_weak_pointer (G_OBJECT (totem->bvw),
			(void**)&(totem->bvw));

	gtk_widget_show (GTK_WIDGET (totem->bvw));

	gtk_widget_set_size_request (container, -1, -1);
}

GtkWidget *
label_create (void)
{
	GtkWidget *label;
	char *text;

	label = rb_ellipsizing_label_new ("");
	gtk_label_set_use_markup (GTK_LABEL (label), TRUE);
	gtk_label_set_selectable (GTK_LABEL (label), FALSE);
	gtk_misc_set_alignment (GTK_MISC (label), 0.5, 0);

	/* Set default */
	text = g_strdup_printf
		("<span size=\"medium\"><b>%s</b></span>",
		 _("No file"));
	rb_ellipsizing_label_set_markup (RB_ELLIPSIZING_LABEL (label), text);
	g_free (text);

	return label;
}

GtkWidget *
totem_statusbar_create (void)
{
	GtkWidget *widget;

	widget = totem_statusbar_new ();
	gtk_widget_show (widget);

	return widget;
}

GtkWidget *
totem_time_display_create (void)
{
	GtkWidget *widget;

	widget = totem_time_label_new ();
	gtk_widget_show (widget);

	return widget;
}

static void
totem_setup_recent (Totem *totem)
{
	GtkWidget *menu_item;
	GtkWidget *menu;

	menu_item = glade_xml_get_widget (totem->xml, "tmw_menu_item_movie");
	menu = gtk_menu_item_get_submenu (GTK_MENU_ITEM (menu_item));
	menu_item = glade_xml_get_widget (totem->xml,
			"tmw_menu_recent_separator");

	g_return_if_fail (menu != NULL);
	g_return_if_fail (menu_item != NULL);

	/* it would be better if we just filtered by mime-type, but there
	 * doesn't seem to be an easy way to figure out which mime-types we
	 * can handle */
	totem->recent_model = egg_recent_model_new (EGG_RECENT_MODEL_SORT_MRU);

	totem->recent_view = egg_recent_view_gtk_new (menu, menu_item);
	egg_recent_view_gtk_show_icons (EGG_RECENT_VIEW_GTK
			(totem->recent_view), FALSE);
	egg_recent_model_set_limit (totem->recent_model, 5);
	egg_recent_view_set_model (EGG_RECENT_VIEW (totem->recent_view),
			totem->recent_model);
	egg_recent_model_set_filter_groups (totem->recent_model,
			"Totem", NULL);
	egg_recent_view_gtk_set_trailing_sep (totem->recent_view, TRUE);

	g_signal_connect (totem->recent_view, "activate",
			G_CALLBACK (on_recent_file_activate), totem);
}

GConfClient *
totem_get_gconf_client (Totem *totem)
{
	return totem->gc;
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
process_options (Totem *totem, int *argc, char ***argv)
{
	int i;
	guint options = 0;
	char **args = *argv;

	if (*argc == 1) {
		*argc = 0;
		*argv = *argv + 1;
		return;
	}

	for (i = 1; i < *argc; i++)
	{
		if (strcmp (args[i], "--debug") == 0)
		{
			options++;
		} else if (strcmp (args[i], "--fullscreen") == 0) {
			totem_action_fullscreen_toggle (totem);
			options++;
		} else if (g_str_has_prefix (args[i], "--") != FALSE) {
			printf (_("Option '%s' is unknown and was ignored\n"),
					args[i]);
			options++;
		}
	}

	*argc = *argc - options;
	*argv = *argv + options + 1;
}

static void
process_command_line_early (GConfClient *gc, int argc, char **argv)
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

static void
process_command_line (BaconMessageConnection *conn, int argc, char **argv)
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

int
main (int argc, char **argv)
{
	Totem *totem;
	char *filename;
	GConfClient *gc;
	GdkPixbuf *pix;

	bindtextdomain (GETTEXT_PACKAGE, GNOMELOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);

	g_set_application_name (_("Totem Movie Player"));

	if (XInitThreads () == 0)
	{
		gtk_init (&argc, &argv);
		totem_action_error_and_exit (_("Could not initialise the thread-safe libraries."), _("Verify your system installation. Totem will now exit."), NULL);
	}

	g_thread_init (NULL);
	gdk_threads_init ();

	gtk_init (&argc, &argv);

	options[0].arg = bacon_video_widget_get_popt_table ();
#ifndef HAVE_GTK_ONLY
	gnome_program_init ("totem", VERSION,
			LIBGNOMEUI_MODULE,
			argc, argv,
			GNOME_PARAM_APP_DATADIR, DATADIR,
			GNOME_PARAM_POPT_TABLE, options,
			GNOME_PARAM_NONE);
#endif /* !HAVE_GTK_ONLY */

	gnome_vfs_init ();

	if ((gc = gconf_client_get_default ()) == NULL)
	{
		totem_action_error_and_exit (_("Totem couln't initialise the configuration engine."), _("Make sure that GNOME is properly installed."), NULL);
	}

#ifndef HAVE_GTK_ONLY
	gnome_authentication_manager_init ();
#endif /* !HAVE_GTK_ONLY */

	if (g_file_test ("../data/totem.glade", G_FILE_TEST_EXISTS) != FALSE)
		filename = g_strdup ("../data/totem.glade");
	else
		filename = g_build_filename (DATADIR,
				"totem", "totem.glade", NULL);

	if (g_file_test (filename, G_FILE_TEST_EXISTS) == FALSE)
	{
		g_free (filename);
		totem_action_error_and_exit (_("Couldn't load the main interface (totem.glade)."), _("Make sure that Totem is properly installed."), NULL);
	}

	totem = g_new0 (Totem, 1);

	/* IPC stuff */
	totem->conn = bacon_message_connection_new (GETTEXT_PACKAGE);
	if (bacon_message_connection_get_is_server (totem->conn) == FALSE)
	{
		process_command_line (totem->conn, argc, argv);
		bacon_message_connection_free (totem->conn);
		g_free (totem);
		gdk_notify_startup_complete ();
		exit (0);
	} else {
		process_command_line_early (gc, argc, argv);
	}

	/* Init totem itself */
	totem->prev_volume = -1;
	totem->gc = gc;

	/* Main window */
	totem->xml = glade_xml_new (filename, NULL, NULL);
	if (totem->xml == NULL)
	{
		g_free (filename);
		totem_action_error_and_exit (_("Couldn't load the main interface (totem.glade)."), _("Make sure that Totem is properly installed."), NULL);
	}
	g_free (filename);

	totem->win = glade_xml_get_widget (totem->xml, "totem_main_window");
	filename = g_build_filename (DATADIR,
			"totem", "media-player-48.png", NULL);
	gtk_window_set_default_icon_from_file (filename, NULL);
	g_free (filename);

	totem_named_icons_init (totem, FALSE);

	/* The playlist */
	filename = g_build_filename (DATADIR,
			"totem", "playlist-playing.png", NULL);
	pix = gdk_pixbuf_new_from_file (filename, NULL);
	g_free (filename);

	filename = g_build_filename (DATADIR,
			"totem", "playlist.glade", NULL);
	totem->playlist = TOTEM_PLAYLIST (totem_playlist_new (filename, pix));
	g_free (filename);

	if (totem->playlist == NULL)
	{
		totem_action_error_and_exit (_("Couldn't load the interface for the playlist."), _("Make sure that Totem is properly installed."), totem);
	}
	filename = g_build_filename (DATADIR,
			"totem", "playlist-24.png", NULL);
	gtk_window_set_icon_from_file (GTK_WINDOW (totem->playlist),
			filename, NULL);
	g_free (filename);

	/* The rest of the widgets */
	totem->state = STATE_STOPPED;
	totem->seek = glade_xml_get_widget (totem->xml, "tmw_seek_hscale");
	totem->seekadj = gtk_range_get_adjustment (GTK_RANGE (totem->seek));
	g_object_set_data (G_OBJECT (totem->seek), "fs", GINT_TO_POINTER (0));
	totem->volume = glade_xml_get_widget (totem->xml, "tmw_volume_hscale");
	totem->voladj = gtk_range_get_adjustment (GTK_RANGE (totem->volume));
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
	g_object_set_data (G_OBJECT (totem->fs_volume), "fs", GINT_TO_POINTER (1));
	totem->volume_first_time = 1;
	totem->fs_pp_button = glade_xml_get_widget
		(totem->xml, "tcw_pp_button");
	totem->statusbar = glade_xml_get_widget (totem->xml, "tmw_statusbar");
	totem->tcw_time_label = glade_xml_get_widget (totem->xml,
			"tcw_time_display_label");
	totem->seek_lock = totem->vol_lock = totem->vol_fs_lock = FALSE;

	/* Properties */
	totem->properties = bacon_video_widget_properties_new ();

	totem_setup_recent (totem);
	totem_callback_connect (totem);
	totem_setup_window (totem);

	/* Show ! gtk_main_iteration trickery to show all the widgets
	 * we have so far */
	gtk_widget_show_all (totem->win);
	update_fullscreen_size (totem);
	long_action ();

	totem->controls_visibility = TOTEM_CONTROLS_VISIBLE;

	/* Show ! (again) the video widget this time. */
	video_widget_create (totem);
	long_action ();

	/* The prefs after the video widget is connected */
	totem_setup_preferences (totem);

	/* Command-line handling */
	process_options (totem, &argc, &argv);

	if (argc >= 1)
	{
		if (totem_action_open_files (totem, argv))
			totem_action_play_pause (totem);
		else
			totem_action_set_mrl (totem, NULL);
	} else {
		totem_action_restore_pl (totem);
	}

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

	return 0;
}
