/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * Copyright (C) 2001-2004 Bastien Nocera <hadess@hadess.net>
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

#ifndef HAVE_GTK_ONLY
#include <gnome.h>
#include "totem-gromit.h"
#else
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include <stdlib.h>
#endif /* !HAVE_GTK_ONLY */

#include <string.h>

/* X11 headers */
#include <gdk/gdkx.h>
#include <X11/Xlib.h>
#ifdef HAVE_XFREE
#include <X11/XF86keysym.h>
#endif

#include "bacon-video-widget.h"
#include "bacon-video-widget-properties.h"
#include "totem-statusbar.h"
#include "totem-time-label.h"
#include "totem-session.h"
#include "totem-screenshot.h"
#include "totem-sublang.h"
#include "totem-options.h"
#include "totem-uri.h"
#include "totem-interface.h"
#include "video-utils.h"

#include "egg-recent-view.h"

#include "totem.h"
#include "totem-private.h"
#include "totem-preferences.h"
#include "totem-stock-icons.h"
#include "totem-disc.h"

#include "debug.h"

#define REWIND_OR_PREVIOUS 4000

#define SEEK_FORWARD_OFFSET 60
#define SEEK_BACKWARD_OFFSET -15

#define SEEK_FORWARD_SHORT_OFFSET 15
#define SEEK_BACKWARD_SHORT_OFFSET -5

#define SEEK_FORWARD_LONG_OFFSET 10*60
#define SEEK_BACKWARD_LONG_OFFSET -3*60

#define VOLUME_DOWN_OFFSET -8
#define VOLUME_UP_OFFSET 8

#define ZOOM_IN_OFFSET 1
#define ZOOM_OUT_OFFSET -1
#define ZOOM_UPPER 200
#define ZOOM_RESET 100
#define ZOOM_LOWER 10

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
static void on_play_pause_button_clicked (GtkToggleButton *button,
		Totem *totem);
static void playlist_changed_cb (GtkWidget *playlist, Totem *totem);
static void show_controls (Totem *totem, gboolean was_fullscreen);
static gboolean totem_is_fullscreen (Totem *totem);
static void play_pause_set_label (Totem *totem, TotemStates state);
static gboolean on_video_motion_notify_event (GtkWidget *widget, GdkEventMotion *event, Totem *totem);

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
	if (totem->bvw == NULL)
		return;

	if (totem_is_fullscreen (totem) != FALSE)
		return;

	/* Save the size of the video widget */
	gconf_client_set_int (totem->gc,
			GCONF_PREFIX"/window_w",
			GTK_WIDGET(totem->bvw)->allocation.width, NULL);
	gconf_client_set_int (totem->gc,
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

	totem_named_icons_dispose (totem);

	if (totem->bvw)
		gconf_client_set_int (totem->gc,
				GCONF_PREFIX"/volume",
				bacon_video_widget_get_volume (totem->bvw),
				NULL);

	if (totem->playlist)
		gtk_widget_destroy (GTK_WIDGET (totem->playlist));
	if (totem->win)
		gtk_widget_destroy (GTK_WIDGET (totem->win));

	gnome_vfs_shutdown ();

	exit (0);
}

static void
totem_action_menu_popup (Totem *totem, guint button)
{
	GtkWidget *menu;

	menu = glade_xml_get_widget (totem->xml_popup,
			"totem_right_click_menu");
	gtk_menu_popup (GTK_MENU (menu), NULL, NULL, NULL, NULL,
			button, gtk_get_current_event_time ());
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

	if (state == totem->state)
		return;

	switch (state)
	{
	case STATE_PLAYING:
		totem_statusbar_set_text (TOTEM_STATUSBAR (totem->statusbar),
				_("Playing"));
		id = GTK_STOCK_MEDIA_PAUSE;
		break;
	case STATE_PAUSED:
		totem_statusbar_set_text (TOTEM_STATUSBAR (totem->statusbar),
				_("Paused"));
		id = GTK_STOCK_MEDIA_PLAY;
		break;
	case STATE_STOPPED:
		totem_statusbar_set_text (TOTEM_STATUSBAR (totem->statusbar),
				_("Stopped"));
		totem_statusbar_set_time_and_length
			(TOTEM_STATUSBAR (totem->statusbar), 0, 0);
		id = GTK_STOCK_MEDIA_PLAY;
		break;
	default:
		return;
	}

	image = glade_xml_get_widget (totem->xml,
			"tmw_play_pause_button_image");
	gtk_image_set_from_stock (GTK_IMAGE (image), id, GTK_ICON_SIZE_BUTTON);
	image = glade_xml_get_widget (totem->xml, "tcw_pp_button_image");
	gtk_image_set_from_stock (GTK_IMAGE (image), id, GTK_ICON_SIZE_BUTTON);

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
	if (totem_is_fullscreen (totem) != FALSE)
		totem_scrsaver_disable (totem->scr);

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

static void
totem_action_set_mrl_and_play (Totem *totem, char *mrl)
{
	if (totem_action_set_mrl (totem, mrl) != FALSE)
		totem_action_play (totem);
}

static gboolean
totem_action_load_media (Totem *totem, MediaType type)
{
	const char **mrls;
	char *msg;

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
totem_action_fullscreen_toggle (Totem *totem)
{
	if (totem_is_fullscreen (totem) != FALSE)
	{
		GtkWidget *item;

		popup_hide (totem);
		bacon_video_widget_set_fullscreen (totem->bvw, FALSE);
		gtk_window_unfullscreen (GTK_WINDOW(totem->win));
		bacon_video_widget_set_show_cursor (totem->bvw, TRUE);

		totem_scrsaver_enable (totem->scr);

		totem->controls_visibility = TOTEM_CONTROLS_VISIBLE;

		item = glade_xml_get_widget
			(totem->xml, "tmw_show_controls_menu_item");

		if (gtk_check_menu_item_get_active
				(GTK_CHECK_MENU_ITEM (item)))
		{
			totem->controls_visibility = TOTEM_CONTROLS_VISIBLE;
			show_controls (totem, TRUE);
		} else {
			totem->controls_visibility = TOTEM_CONTROLS_HIDDEN;
			show_controls (totem, TRUE);
		}
	} else {
		totem_action_save_size (totem);
		update_fullscreen_size (totem);
		bacon_video_widget_set_fullscreen (totem->bvw, TRUE);
		bacon_video_widget_set_show_cursor (totem->bvw, FALSE);
		gtk_window_fullscreen (GTK_WINDOW(totem->win));
		if (bacon_video_widget_is_playing (totem->bvw) != FALSE)
			totem_scrsaver_disable (totem->scr);

		totem->controls_visibility = TOTEM_CONTROLS_FULLSCREEN;
		show_controls (totem, FALSE);
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
		widget = glade_xml_get_widget (totem->xml, "tmw_title_label");
		gtk_label_set_markup (GTK_LABEL (widget), text);

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
		widget = glade_xml_get_widget (totem->xml, "tcw_title_label");
		gtk_label_set_markup (GTK_LABEL (widget), text);
		widget = glade_xml_get_widget (totem->xml, "tmw_title_label");
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

		widget = glade_xml_get_widget (totem->xml_popup, "trcm_play");
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
		widget = glade_xml_get_widget (totem->xml_popup, "trcm_volume_up");
		gtk_widget_set_sensitive (widget, FALSE);
		widget = glade_xml_get_widget (totem->xml_popup, "trcm_volume_down");
		gtk_widget_set_sensitive (widget, FALSE);

		/* Control popup */
		gtk_widget_set_sensitive (totem->fs_seek, FALSE);
		gtk_widget_set_sensitive (totem->fs_pp_button, FALSE);
		widget = glade_xml_get_widget (totem->xml,
				"tcw_previous_button");
		gtk_widget_set_sensitive (widget, FALSE);
		widget = glade_xml_get_widget (totem->xml, "tcw_next_button"); 
		gtk_widget_set_sensitive (widget, FALSE);
		widget = glade_xml_get_widget (totem->xml_popup, "trcm_next_chapter");
		gtk_widget_set_sensitive (widget, FALSE);
		widget = glade_xml_get_widget (totem->xml_popup, "trcm_previous_chapter");
		gtk_widget_set_sensitive (widget, FALSE);
		widget = glade_xml_get_widget (totem->xml, "tcw_volume_hbox");
		gtk_widget_set_sensitive (widget, FALSE);

		/* Take a screenshot */
		widget = glade_xml_get_widget (totem->xml,
				"tmw_take_screenshot_menu_item");
		gtk_widget_set_sensitive (widget, FALSE);

		/* Set the logo */
		bacon_video_widget_set_logo_mode (totem->bvw, TRUE);
		bacon_video_widget_set_logo (totem->bvw, LOGO_PATH);

		update_mrl_label (totem, NULL);
	} else {
		gboolean caps;
		char *subtitle_uri;
		GError *err = NULL;

		if (bacon_video_widget_get_logo_mode (totem->bvw) != FALSE)
		{
			bacon_video_widget_close (totem->bvw);
			bacon_video_widget_set_logo_mode (totem->bvw, FALSE);
		}

		subtitle_uri = totem_uri_get_subtitle_uri (mrl);
		retval = bacon_video_widget_open_with_subtitle (totem->bvw,
				mrl, subtitle_uri, &err);
		totem->mrl = g_strdup (mrl);

		/* Play/Pause */
		gtk_widget_set_sensitive (totem->pp_button, TRUE);
		widget = glade_xml_get_widget (totem->xml, "tmw_play_menu_item");
		gtk_widget_set_sensitive (widget, TRUE);
		widget = glade_xml_get_widget (totem->xml_popup, "trcm_play");
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
		widget = glade_xml_get_widget (totem->xml_popup, "trcm_volume_up");                gtk_widget_set_sensitive (widget, caps);
		widget = glade_xml_get_widget (totem->xml_popup, "trcm_volume_down");
		gtk_widget_set_sensitive (widget, caps);


		/* Take a screenshot */
		widget = glade_xml_get_widget (totem->xml,
				"tmw_take_screenshot_menu_item");
		gtk_widget_set_sensitive (widget, retval);

		/* Set the playlist */
		totem_playlist_set_playing (totem->playlist, retval);

		if (retval == FALSE && warn != FALSE)
		{
			char *msg, *disp;

			disp = totem_uri_escape_for_display (totem->mrl);
			msg = g_strdup_printf(_("Totem could not play '%s'."), disp);
			g_free (disp);
			totem_action_error (msg, err->message, totem);
			g_free (msg);
		}

		if (retval == FALSE)
		{
			g_error_free (err);
			g_free (totem->mrl);
			totem->mrl = NULL;
			play_pause_set_label (totem, STATE_STOPPED);
		}
	}
	update_buttons (totem);
	update_media_menu_items (totem);

	widget = glade_xml_get_widget (totem->xml, "tmw_properties_menu_item");
	gtk_widget_set_sensitive (widget, retval);

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
	GtkWidget *item;

	if (zoom < ZOOM_LOWER || zoom > ZOOM_UPPER)
		return;

	bacon_video_widget_set_zoom (totem->bvw, zoom);

	if (zoom == ZOOM_UPPER) {
		item = glade_xml_get_widget
			(totem->xml, "tmw_zoom_in_menu_item");
		gtk_widget_set_sensitive (item, FALSE);
	} else if (zoom == ZOOM_LOWER) {
		item = glade_xml_get_widget
			(totem->xml, "tmw_zoom_out_menu_item");
		gtk_widget_set_sensitive (item, FALSE);
	} else if (zoom == ZOOM_RESET) {
		item = glade_xml_get_widget
			(totem->xml, "tmw_zoom_reset_menu_item");
		gtk_widget_set_sensitive (item, FALSE);
		item = glade_xml_get_widget
			(totem->xml, "tmw_zoom_in_menu_item");
		gtk_widget_set_sensitive (item, TRUE);
		item = glade_xml_get_widget
			(totem->xml, "tmw_zoom_out_menu_item");
		gtk_widget_set_sensitive (item, TRUE);
	} else {
		item = glade_xml_get_widget
			(totem->xml, "tmw_zoom_in_menu_item");
		gtk_widget_set_sensitive (item, TRUE);
		item = glade_xml_get_widget
			(totem->xml, "tmw_zoom_out_menu_item");
		gtk_widget_set_sensitive (item, TRUE);
		item = glade_xml_get_widget
			(totem->xml, "tmw_zoom_reset_menu_item");
		gtk_widget_set_sensitive (item, TRUE);
	}
}

static void
totem_action_zoom_relative (Totem *totem, int off_pct)
{
	int zoom;

	zoom = bacon_video_widget_get_zoom (totem->bvw);
	totem_action_zoom (totem, zoom + off_pct);
}

static void
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
	GtkWidget  *item;
	int  tmp;
	const char const *widgets[] = {
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
		int drop_type, gboolean empty_pl)
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
	guint end;

	uri = egg_recent_item_get_uri (item);

	totem_signal_block_by_data (totem->playlist, totem);

	end = totem_playlist_get_last (totem->playlist);
	playlist_changed = totem_playlist_add_mrl (totem->playlist, uri, NULL);
	egg_recent_model_add_full (totem->recent_model, item);

	totem_signal_unblock_by_data (totem->playlist, totem);

	if (playlist_changed)
	{
		char *mrl;

		totem_playlist_set_current (totem->playlist, end + 1);
		mrl = totem_playlist_get_current_mrl (totem->playlist);
		totem_action_set_mrl_and_play (totem, mrl);
		g_free (mrl);
	}

	g_free (uri);
}

static void
on_got_redirect (BaconVideoWidget *bvw, const char *mrl, Totem *totem)
{
	g_message ("on_got_redirect %s", mrl);
	//FIXME we need to check for relative paths here
	bacon_video_widget_close (totem->bvw);
	bacon_video_widget_open (totem->bvw, mrl, NULL);
	bacon_video_widget_play (bvw, NULL);
}

/* This is only called when we are playing a DVD */
static void
on_title_change_event (BaconVideoWidget *bvw, const char *string, Totem *totem)
{
	update_mrl_label (totem, string);
	totem_playlist_set_title (TOTEM_PLAYLIST (totem->playlist), string);
}

static void
on_channels_change_event (BaconVideoWidget *bvw, Totem *totem)
{
	totem_sublang_update (totem);
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
		totem_action_error (_("An error occurred"), message, totem);
	} else {
		totem_action_error_and_exit (_("An error occurred"),
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

	if (totem->seekable == seekable)
		return;

	totem->seekable = seekable;

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
	widget = glade_xml_get_widget (totem->xml_popup, "trcm_skip_forward");
	gtk_widget_set_sensitive (widget, seekable);
	widget = glade_xml_get_widget (totem->xml_popup,
			"trcm_skip_backwards");
	gtk_widget_set_sensitive (widget, seekable);
	widget = glade_xml_get_widget (totem->xml, "tmw_skip_to_menu_item");
	gtk_widget_set_sensitive (widget, seekable);
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
	gboolean cleared = FALSE;

	if (list == NULL)
		return cleared;

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
		gtk_file_chooser_set_current_folder_uri
			(GTK_FILE_CHOOSER (fs), path);
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

		/* Save the path */
		if (filenames->data != NULL)
		{
			g_free (path);
			path = g_path_get_dirname (filenames->data);
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

//FIXME move the URI to a separate widget
static void
on_open_location1_activate (GtkButton *button, Totem *totem)
{
	GladeXML *glade;
	char *mrl;
	GtkWidget *dialog, *entry, *gentry;
	int response;
	const char *filenames[2];

	glade = totem_interface_load ("uri.glade", _("Open Location..."),
			FALSE, GTK_WINDOW (totem->win));
	if (glade == NULL)
		return;

	dialog = glade_xml_get_widget (glade, "open_uri_dialog");
	entry = glade_xml_get_widget (glade, "uri");
	gentry = glade_xml_get_widget (glade, "uri_list");

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

			gnome_entry_append_history (GNOME_ENTRY (gentry),
					TRUE, uri);
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
	type = totem_cd_detect_type (device, &error);
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
on_resize_1_2_activate (GtkButton *button, Totem *totem)
{
	totem_action_set_scale_ratio (totem, 0.5); 
}

static void
on_resize_1_1_activate (GtkButton *button, Totem *totem)
{
	totem_action_set_scale_ratio (totem, 1);
}

static void
on_resize_2_1_activate (GtkButton *button, Totem *totem)
{                       
	totem_action_set_scale_ratio (totem, 2);
}

static void
on_zoom_in_activate (GtkButton *button, Totem *totem)
{
	totem_action_zoom_relative (totem, ZOOM_IN_OFFSET);
}

static void
on_zoom_reset_activate (GtkButton *button, Totem *totem)
{
	totem_action_zoom_reset (totem);
}

static void
on_zoom_out_activate (GtkButton *button, Totem *totem)
{
	totem_action_zoom_relative (totem, ZOOM_OUT_OFFSET);
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
show_controls (Totem *totem, gboolean was_fullscreen)
{
	GtkWidget *menubar, *controlbar, *statusbar, *item, *bvw_vbox, *widget;
	int width = 0, height = 0;

	if (totem->bvw == NULL)
		return;

	menubar = glade_xml_get_widget (totem->xml, "tmw_menubar");
	controlbar = glade_xml_get_widget (totem->xml, "tmw_controls_vbox");
	statusbar = glade_xml_get_widget (totem->xml, "tmw_statusbar");
	item = glade_xml_get_widget (totem->xml_popup, "trcm_show_controls");
	bvw_vbox = glade_xml_get_widget (totem->xml, "tmw_bvw_vbox");
	widget = GTK_WIDGET (totem->bvw);

	if (totem->controls_visibility == TOTEM_CONTROLS_VISIBLE)
	{
		if (was_fullscreen == FALSE)
		{
			height = widget->allocation.height;
			width =	widget->allocation.width;
		}

		gtk_widget_show (menubar);
		gtk_widget_show (controlbar);
		gtk_widget_show (statusbar);
		gtk_widget_hide (item);
		gtk_container_set_border_width (GTK_CONTAINER (bvw_vbox),
				BVW_VBOX_BORDER_WIDTH);

		if (was_fullscreen == FALSE)
		{
			totem_widget_set_preferred_size (widget,
					width, height);
		}
	} else {
		if (totem->controls_visibility == TOTEM_CONTROLS_HIDDEN)
		{
			width = widget->allocation.width;
			height = widget->allocation.height;
		}

		gtk_widget_hide (menubar);
		gtk_widget_hide (controlbar);
		gtk_widget_hide (statusbar);

		 /* We won't show controls in fullscreen */
		if (totem_is_fullscreen (totem) != FALSE)
			gtk_widget_hide (item);
		else
			gtk_widget_show (item);
		gtk_container_set_border_width (GTK_CONTAINER (bvw_vbox), 0);

		if (totem->controls_visibility == TOTEM_CONTROLS_HIDDEN)
		{
			gtk_window_resize (GTK_WINDOW(totem->win),
					width, height);
		}
	}
}

static void
on_show_controls1_activate (GtkCheckMenuItem *checkmenuitem, Totem *totem)
{
	gboolean show;

	show = gtk_check_menu_item_get_active (checkmenuitem);

	/* Let's update our controls visibility */
	if (show)
		totem->controls_visibility = TOTEM_CONTROLS_VISIBLE;
	else
		totem->controls_visibility = TOTEM_CONTROLS_HIDDEN;

	show_controls (totem, FALSE);
}

static void
on_show_controls2_activate (GtkMenuItem *menuitem, Totem *totem)
{
	totem_action_toggle_controls (totem);
}

void
totem_action_toggle_controls (Totem *totem)
{
	GtkWidget *item;
	gboolean state;

	if (totem_is_fullscreen (totem) != FALSE)
		return;

	item = glade_xml_get_widget (totem->xml,
			"tmw_show_controls_menu_item");
	state = gtk_check_menu_item_get_active (GTK_CHECK_MENU_ITEM (item));
	gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (item), !state);
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
#endif /* !HAVE_GTK_ONLY */

static void
on_about1_activate (GtkButton *button, Totem *totem)
{
	GdkPixbuf *pixbuf = NULL;
	const char *authors[] =
	{
		"Bastien Nocera <hadess@hadess.net>",
		"Ronald Bultje <rbultje@ronald.bitfreak.net>",
		"Julien Moutte <julien@moutte.net> (GStreamer backend)",
		NULL
	};
	const char *artists[] = { "Jakub Steiner <jimmac@ximian.com>", NULL };
	const char *documenters[] =
	{
		"Chee Bin Hoh <cbhoh@gnome.org>",
		NULL
	};
	char *backend_version, *description;
	char *filename;

	if (totem->about != NULL)
	{
		gtk_window_present (GTK_WINDOW (totem->about));
		return;
	}

	filename = g_build_filename (DATADIR,
			"totem", "media-player-48.png", NULL);
	pixbuf = gdk_pixbuf_new_from_file (filename, NULL);
	g_free (filename);

	backend_version = bacon_video_widget_get_backend_name (totem->bvw);
	description = g_strdup_printf (_("Movie Player using %s"),
				backend_version);

	totem->about = g_object_new (GTK_TYPE_ABOUT_DIALOG,
			"name", _("Totem"),
			"version", VERSION,
			"copyright", _("Copyright \xc2\xa9 2002-2005 Bastien Nocera"),
			"comments", description,
			"authors", authors,
			"documenters", documenters,
			"artists", artists, 
			"translator-credits", _("translator-credits"),
			"logo", pixbuf,
			NULL);

	g_free (backend_version);
	g_free (description);

	if (pixbuf != NULL)
		gdk_pixbuf_unref (pixbuf);

	g_object_add_weak_pointer (G_OBJECT (totem->about),
			(gpointer *)&totem->about);
	gtk_window_set_transient_for (GTK_WINDOW (totem->about),
			GTK_WINDOW (totem->win));
	g_signal_connect (G_OBJECT (totem->about), "response",
			G_CALLBACK (gtk_widget_destroy), NULL);

	gtk_widget_show(totem->about);
}

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
		gtk_widget_show (dialog);
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
	gtk_widget_show (dialog);
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

static void
on_skip_to1_activate (GtkButton *button, Totem *totem)
{
	char *filename;

	if (totem->seekable == FALSE)
		return;

	if (totem->skipto != NULL)
	{
		gtk_window_present (GTK_WINDOW (totem->skipto));
		return;
	}

	filename = g_build_filename (DATADIR,
			"totem", "skip_to.glade", NULL);
	totem->skipto = TOTEM_SKIPTO (totem_skipto_new (filename));
	g_free (filename);

	g_signal_connect (G_OBJECT (totem->skipto), "delete-event",
			G_CALLBACK (gtk_widget_destroy), NULL);
	g_signal_connect (G_OBJECT (totem->skipto), "response",
			G_CALLBACK (commit_hide_skip_to), totem);
	g_object_add_weak_pointer (G_OBJECT (totem->skipto),
			(gpointer *)&totem->skipto);
	gtk_window_set_transient_for (GTK_WINDOW (totem->skipto),
			GTK_WINDOW (totem->win));
	gtk_widget_show (GTK_WIDGET (totem->skipto));
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
	gboolean handled = TRUE;

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
		g_assert (url != NULL);
		if (totem_playlist_add_mrl (totem->playlist, url, NULL) != FALSE) {
			totem_action_add_recent (totem, url);
		}
		break;
	case TOTEM_REMOTE_COMMAND_REPLACE:
		g_assert (url != NULL);
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
	GdkScreen *screen;

	screen = gtk_window_get_screen (GTK_WINDOW (totem->win));
	gdk_screen_get_monitor_geometry (screen,
			gdk_screen_get_monitor_at_window
			(screen, totem->win->window),
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

	bacon_video_widget_set_show_cursor (totem->bvw, FALSE);

	return FALSE;
}

static void
on_mouse_click_fullscreen (GtkWidget *widget, Totem *totem)
{
	popup_timeout_remove (totem);

	totem->popup_timeout = g_timeout_add (FULLSCREEN_POPUP_TIMEOUT,
		(GSourceFunc) popup_hide, totem);
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
	item = glade_xml_get_widget (totem->xml, "tmw_title_label");
	gtk_widget_realize (item);
	gdk_flush ();

	move_popups (totem);

	gtk_widget_show_all (totem->exit_popup);
	gtk_widget_show_all (totem->control_popup);
	bacon_video_widget_set_show_cursor (totem->bvw, TRUE);

	totem->popup_timeout = g_timeout_add (FULLSCREEN_POPUP_TIMEOUT,
			(GSourceFunc) popup_hide, totem);
	totem->popup_in_progress = FALSE;

	return FALSE;
}

static gboolean
on_video_button_press_event (BaconVideoWidget *bvw, GdkEventButton *event,
		Totem *totem)
{
	if (event->type == GDK_BUTTON_PRESS && event->button == 3)
	{
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
	case GDK_f:
	case GDK_F:
		totem_action_fullscreen_toggle (totem);
		break;
	case GDK_h:
	case GDK_H:
		totem_action_toggle_controls (totem);
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
	case GDK_Menu:
		totem_action_menu_popup (totem, 0);
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
	case GDK_r:
	case GDK_R:
		totem_action_zoom_relative (totem, ZOOM_IN_OFFSET);
		break;
	case GDK_s:
	case GDK_S:
		on_skip_to1_activate (NULL, totem);
		break;
	case GDK_t:
	case GDK_T:
		totem_action_zoom_relative (totem, ZOOM_OUT_OFFSET);
		break;
	case GDK_Escape:
		totem_action_fullscreen (totem, FALSE);
		break;
	case GDK_Left:
		if (event->state & GDK_SHIFT_MASK)
		{
			totem_action_seek_relative (totem,
					SEEK_BACKWARD_SHORT_OFFSET);
		} else if (event->state & GDK_CONTROL_MASK) {
			totem_action_seek_relative (totem,
					SEEK_BACKWARD_LONG_OFFSET);
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
		} else if (event->state & GDK_CONTROL_MASK) {
			totem_action_seek_relative (totem,
					SEEK_FORWARD_LONG_OFFSET);
		} else {
			totem_action_seek_relative (totem, SEEK_FORWARD_OFFSET);
		}
		break;
	case GDK_space:
		if (totem_is_fullscreen (totem) != FALSE)
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
			totem_action_menu_popup (totem, 0);
		else
			retval = FALSE;
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
	/* Special case the Playlist, Eject, Open, Open URI and
	 * seeking keyboard shortcuts */
	if (event->state != 0
			&& (event->state & GDK_CONTROL_MASK))
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
		case GDK_Right:
		case GDK_Left:
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

	playing = totem_playing_dvd (totem->mrl);

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
	if (totem_playing_dvd (totem->mrl) != FALSE)
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
	item = glade_xml_get_widget (totem->xml_popup, "trcm_previous_chapter");
	gtk_widget_set_sensitive (item, has_item);

	/* Next */
	/* FIXME Need way to detect if DVD Title has no more chapters */
	if (totem_playing_dvd (totem->mrl) != FALSE)
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
	item = glade_xml_get_widget (totem->xml_popup, "trcm_next_chapter");
	gtk_widget_set_sensitive (item, has_item);
}

static void
totem_setup_window (Totem *totem)
{
	GtkWidget *item;
	int w, h;

	w = gconf_client_get_int (totem->gc, GCONF_PREFIX"/window_w", NULL);
	h = gconf_client_get_int (totem->gc, GCONF_PREFIX"/window_h", NULL);

	if (w <= 0 || h <= 0)
		return;

	item = glade_xml_get_widget (totem->xml, "tmw_bvw_vbox");
	gtk_widget_set_size_request (item,
			w + BVW_VBOX_BORDER_WIDTH, h + BVW_VBOX_BORDER_WIDTH);
}

static void
totem_callback_connect (Totem *totem)
{
	GtkWidget *item;

	/* Menu items */
	item = glade_xml_get_widget (totem->xml, "tmw_open_menu_item");
	g_signal_connect (G_OBJECT (item), "activate",
			G_CALLBACK (on_open1_activate), totem);
	item = glade_xml_get_widget (totem->xml,
			"tmw_open_location_menu_item");
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
	item = glade_xml_get_widget (totem->xml, "tmw_resize_1_2_menu_item");
	g_signal_connect (G_OBJECT (item), "activate",
			G_CALLBACK (on_resize_1_2_activate), totem);
	item = glade_xml_get_widget (totem->xml, "tmw_resize_1_1_menu_item");
	g_signal_connect (G_OBJECT (item), "activate",
			G_CALLBACK (on_resize_1_1_activate), totem);
	item = glade_xml_get_widget (totem->xml, "tmw_resize_2_1_menu_item");
	g_signal_connect (G_OBJECT (item), "activate",
			G_CALLBACK (on_resize_2_1_activate), totem);
	if (bacon_video_widget_can_set_zoom (totem->bvw) != FALSE)
	{
		item = glade_xml_get_widget
			(totem->xml, "tmw_zoom_in_menu_item");
		g_signal_connect (G_OBJECT (item), "activate",
				G_CALLBACK (on_zoom_in_activate), totem);
		item = glade_xml_get_widget
			(totem->xml, "tmw_zoom_reset_menu_item");
		g_signal_connect (G_OBJECT (item), "activate",
				G_CALLBACK (on_zoom_reset_activate), totem);
		item = glade_xml_get_widget
			(totem->xml, "tmw_zoom_out_menu_item");
		g_signal_connect (G_OBJECT (item), "activate",
				G_CALLBACK (on_zoom_out_activate), totem);
	} else {
		item = glade_xml_get_widget
			(totem->xml, "tmw_zoom_in_menu_item");
		gtk_widget_hide (item);
		item = glade_xml_get_widget
			(totem->xml, "tmw_zoom_reset_menu_item");
		gtk_widget_hide (item);
		item = glade_xml_get_widget
			(totem->xml, "tmw_zoom_out_menu_item");
		gtk_widget_hide (item);
	}

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

	/* Help */
	item = glade_xml_get_widget (totem->xml, "tmw_about_menu_item");
	g_signal_connect (G_OBJECT (item), "activate",
			G_CALLBACK (on_about1_activate), totem);

#ifndef HAVE_GTK_ONLY
	item = glade_xml_get_widget (totem->xml, "tmw_contents_menu_item");
	g_signal_connect (G_OBJECT (item), "activate",
			G_CALLBACK (on_help_activate), totem);
#else
	item = glade_xml_get_widget (totem->xml, "tmw_contents_menu_item");
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
	item = glade_xml_get_widget (totem->xml,
			"tmw_always_on_top_menu_item");
	g_signal_connect (G_OBJECT (item), "activate",
			G_CALLBACK (on_always_on_top1_activate), totem);
	item = glade_xml_get_widget (totem->xml,
			"tmw_show_controls_menu_item");
	g_signal_connect (G_OBJECT (item), "activate",
			G_CALLBACK (on_show_controls1_activate), totem);

	/* Popup menu */
	item = glade_xml_get_widget (totem->xml_popup, "trcm_play");
	g_signal_connect (G_OBJECT (item), "activate",
			G_CALLBACK (on_play1_activate), totem);
	item = glade_xml_get_widget (totem->xml_popup, "trcm_next_chapter");
	g_signal_connect (G_OBJECT (item), "activate",
			G_CALLBACK (on_next_button_clicked), totem);
	item = glade_xml_get_widget (totem->xml_popup, "trcm_previous_chapter");
	g_signal_connect (G_OBJECT (item), "activate",
			G_CALLBACK (on_previous_button_clicked), totem);
	item = glade_xml_get_widget (totem->xml_popup, "trcm_skip_forward");
	g_signal_connect (G_OBJECT (item), "activate",
			G_CALLBACK (on_skip_forward1_activate), totem);
	item = glade_xml_get_widget (totem->xml_popup, "trcm_skip_backwards");
	g_signal_connect (G_OBJECT (item), "activate",
			G_CALLBACK (on_skip_backwards1_activate), totem);
	item = glade_xml_get_widget (totem->xml_popup, "trcm_volume_up");
	g_signal_connect (G_OBJECT (item), "activate",
			G_CALLBACK (on_volume_up1_activate), totem);
	item = glade_xml_get_widget (totem->xml_popup, "trcm_volume_down");
	g_signal_connect (G_OBJECT (item), "activate",
			G_CALLBACK (on_volume_down1_activate), totem);
	item = glade_xml_get_widget (totem->xml_popup, "trcm_show_controls");
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
			GDK_ACTION_COPY | GDK_ACTION_MOVE);

	/* Main Window */
	g_signal_connect (G_OBJECT (totem->win), "delete-event",
			G_CALLBACK (main_window_destroy_cb), totem);
	g_object_notify (G_OBJECT (totem->win), "is-active");
	g_signal_connect_swapped (G_OBJECT (totem->win), "notify",
			G_CALLBACK (popup_hide), totem);

	/* Screen size and Theme changes */
	g_signal_connect (G_OBJECT (gdk_screen_get_default ()),
			"size-changed", G_CALLBACK (size_changed_cb), totem);
	g_signal_connect (G_OBJECT (gtk_icon_theme_get_default ()),
			"changed", G_CALLBACK (theme_changed_cb), totem);

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
	g_signal_connect (G_OBJECT (totem->playlist), "response",
			G_CALLBACK (toggle_playlist_from_playlist), totem);
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

	/* Subtitle and Languages submenu */
	item = glade_xml_get_widget (totem->xml, "tmw_menu_languages");
	g_object_ref (item);
	item = glade_xml_get_widget (totem->xml, "tmw_menu_subtitles");
	g_object_ref (item);

	/* Named icon support */
	totem_set_default_icons (totem);

	/* Update the UI */
	g_timeout_add (600, (GSourceFunc) gui_update_cb, totem);
}

static void
video_widget_create (Totem *totem) 
{
	GError *err = NULL;
	GtkWidget *container;
	int w, h;

	totem->scr = totem_scrsaver_new ();

	w = gconf_client_get_int (totem->gc, GCONF_PREFIX"/window_w", NULL);
	h = gconf_client_get_int (totem->gc, GCONF_PREFIX"/window_h", NULL);

	totem->bvw = BACON_VIDEO_WIDGET
		(bacon_video_widget_new (w, h, BVW_USE_TYPE_VIDEO, &err));

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
			target_table, G_N_ELEMENTS (target_table),
			GDK_ACTION_COPY | GDK_ACTION_MOVE);

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

	bacon_video_widget_set_volume (totem->bvw,
			gconf_client_get_int (totem->gc,
				GCONF_PREFIX"/volume", NULL));
	update_volume_sliders (totem);
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

int
main (int argc, char **argv)
{
	Totem *totem;
	char *filename;
	GConfClient *gc;

	bindtextdomain (GETTEXT_PACKAGE, GNOMELOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);

	if (XInitThreads () == 0)
	{
		gtk_init (&argc, &argv);
		g_set_application_name (_("Totem Movie Player"));
		totem_action_error_and_exit (_("Could not initialise the thread-safe libraries."), _("Verify your system installation. Totem will now exit."), NULL);
	}

	g_thread_init (NULL);

#ifdef HAVE_GTK_ONLY
	gtk_init (&argc, &argv);
#else
	gnome_program_init ("totem", VERSION,
			LIBGNOMEUI_MODULE,
			argc, argv,
			GNOME_PARAM_APP_DATADIR, DATADIR,
			GNOME_PARAM_POPT_TABLE, totem_options_get_options (),
			GNOME_PARAM_NONE);
#endif /* HAVE_GTK_ONLY */

	g_set_application_name (_("Totem Movie Player"));
	gnome_vfs_init ();

	if ((gc = gconf_client_get_default ()) == NULL)
	{
		totem_action_error_and_exit (_("Totem couln't initialise the configuration engine."), _("Make sure that GNOME is properly installed."), NULL);
	}

#ifndef HAVE_GTK_ONLY
	gnome_authentication_manager_init ();
#endif /* !HAVE_GTK_ONLY */

	totem = g_new0 (Totem, 1);

	/* IPC stuff */
	totem->conn = bacon_message_connection_new (GETTEXT_PACKAGE);
	if (bacon_message_connection_get_is_server (totem->conn) == FALSE)
	{
		totem_options_process_for_server (totem->conn, argc, argv);
		bacon_message_connection_free (totem->conn);
		g_free (totem);
		gdk_notify_startup_complete ();
		exit (0);
	} else {
		totem_options_process_early (gc, argc, argv);
	}

	/* Init totem itself */
	totem->prev_volume = -1;
	totem->gc = gc;

	/* Main window */
	totem->xml = totem_interface_load ("totem.glade", _("main window"),
			TRUE, NULL);
	if (totem->xml == NULL)
		totem_action_exit (NULL);
	totem->xml_popup = totem_interface_load ("popup.glade",
			_("video popup menu"),
			TRUE, NULL);
	if (totem->xml_popup == NULL)
		totem_action_exit (NULL);

	totem->win = glade_xml_get_widget (totem->xml, "totem_main_window");
	filename = totem_interface_get_full_path ("media-player-48.png");
	gtk_window_set_default_icon_from_file (filename, NULL);
	g_free (filename);

	totem_named_icons_init (totem, FALSE);

	/* The playlist */
	totem->playlist = TOTEM_PLAYLIST (totem_playlist_new ());

	if (totem->playlist == NULL)
		totem_action_exit (totem);

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

	totem_session_setup (totem, argv);
	totem_setup_recent (totem);
	totem_callback_connect (totem);
	totem_setup_window (totem);
	totem_setup_file_monitoring (totem);

	/* Show ! gtk_main_iteration trickery to show all the widgets
	 * we have so far */
	gtk_widget_show (totem->win);
	update_fullscreen_size (totem);
	long_action ();

	totem->controls_visibility = TOTEM_CONTROLS_VISIBLE;

	/* Show ! (again) the video widget this time. */
	video_widget_create (totem);
	long_action ();

	/* The prefs after the video widget is connected */
	totem_setup_preferences (totem);

	/* Command-line handling */
	if (totem_options_process_late (totem, &argc, &argv) != FALSE)
	{
		totem_session_restore (totem, argv);
	} else if (argc >= 1 && totem_action_open_files (totem, argv)) {
			totem_action_play_pause (totem);
	} else {
		totem_action_set_mrl (totem, NULL);
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
