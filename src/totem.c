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

#include <config.h>
#include <gnome.h>
#include <libgnomevfs/gnome-vfs.h>
#include <gconf/gconf-client.h>
#include <string.h>

/* X11 headers */
#include <gdk/gdkx.h>
#include <X11/Xlib.h>
#ifdef HAVE_XFREE
#include <X11/XF86keysym.h>
#endif
#ifdef HAVE_XSUN
#include <X11/Sunkeysym.h>
#endif

#include "gnome-authn-manager.h"
#include "bacon-video-widget.h"
#include "bacon-video-widget-properties.h"
#include "rb-ellipsizing-label.h"
#include "bacon-cd-selection.h"
#include "totem-statusbar.h"
#include "video-utils.h"
#include "languages.h"

#include "egg-recent-view.h"

#include "totem.h"
#include "totem-private.h"
#include "totem-preferences.h"

#include "debug.h"

#define KEYBOARD_HYSTERISIS_TIMEOUT 500

#define SEEK_FORWARD_OFFSET 60000
#define SEEK_BACKWARD_OFFSET -15000

#define SEEK_FORWARD_SHORT_OFFSET 20000
#define SEEK_BACKWARD_SHORT_OFFSET -20000

#define VOLUME_DOWN_OFFSET -8
#define VOLUME_UP_OFFSET 8

typedef enum {
	STATE_PLAYING,
	STATE_PAUSED,
	STATE_STOPPED
} TotemStates;

static const GtkTargetEntry target_table[] = {
	{ "text/uri-list", 0, 0 },
};

static const GtkTargetEntry source_table[] = {
	{ "text/uri-list", 0, 0 },
};

static const struct poptOption options[] = {
	{"play-pause", '\0', POPT_ARG_NONE, NULL, 0, N_("Play/Pause"), NULL},
	{"next", '\0', POPT_ARG_NONE, NULL, 0, N_("Next"), NULL},
	{"previous", '\0', POPT_ARG_NONE, NULL, 0, N_("Previous"), NULL},
	{"seek-fwd", '\0', POPT_ARG_NONE, NULL, 0, N_("Seek Forwards"), NULL},
	{"seek-bwd", '\0', POPT_ARG_NONE, NULL, 0, N_("Seek Backwards"), NULL},
	{"volume-up", '\0', POPT_ARG_NONE, NULL, 0, N_("Volume Up"), NULL},
	{"volume-down", '\0', POPT_ARG_NONE, NULL, 0, N_("Volume Down"), NULL},
	{"fullscreen", '\0', POPT_ARG_NONE, NULL, 0,
		N_("Toggle Fullscreen"), NULL},
	{"quit", '\0', POPT_ARG_NONE, NULL, 0, N_("Quit"), NULL},
	{"enqueue", '\0', POPT_ARG_NONE, NULL, 0, N_("Enqueue"), NULL},
	{"replace", '\0', POPT_ARG_NONE, NULL, 0, N_("Replace"), NULL},
	{NULL, '\0', 0, NULL, 0} /* end the list */
};

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

static void
long_action (void)
{
	while (gtk_events_pending ())
		gtk_main_iteration ();
}

static char
*language_name_get_from_code (const char *code)
{
	int i;

	for (i = 0; languages[i].code != NULL; i++)
	{
		if (strstr (code, languages[i].code) != NULL)
			return g_strdup (languages[i].language);
	}

	/* Ooops, not found */
	return g_strdup (code);
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
	curdir_withslash = g_strdup_printf ("file:///%s%s",
			curdir, G_DIR_SEPARATOR_S);
	g_free (curdir);

	escaped = gnome_vfs_escape_path_string (path);
	retval = gnome_vfs_uri_make_full_from_relative
		(curdir_withslash, escaped);
	g_free (curdir_withslash);
	g_free (escaped);

	return retval;
}

void
totem_action_error (char *msg, Totem *totem)
{
	GtkWidget *parent, *error_dialog;

	if (totem == NULL)
		parent = NULL;
	else
		parent = totem->win;

	error_dialog =
		gtk_message_dialog_new (GTK_WINDOW (parent),
				GTK_DIALOG_MODAL,
				GTK_MESSAGE_ERROR,
				GTK_BUTTONS_OK,
				"%s", msg);
	gtk_dialog_set_default_response (GTK_DIALOG (error_dialog),
			GTK_RESPONSE_OK);
	g_signal_connect (G_OBJECT (error_dialog), "destroy", G_CALLBACK
			(gtk_widget_destroy), error_dialog);
	g_signal_connect (G_OBJECT (error_dialog), "response", G_CALLBACK
			(gtk_widget_destroy), error_dialog);
	gtk_window_set_modal (GTK_WINDOW (error_dialog), TRUE);

	if (totem->buffer_dialog != NULL)
	{
		gtk_widget_destroy (totem->buffer_dialog);
		totem->buffer_dialog = NULL;
		totem->buffer_label = NULL;
	}

	gtk_widget_show (error_dialog);
}

#ifdef HAVE_X86
static gboolean
totem_action_error_try_download (char *msg, Totem *totem)
{
	GtkWidget *error_dialog;
	GValue value = { 0, };
	guint32 audio_fcc, video_fcc;
	int res;

	bacon_video_widget_get_metadata (totem->bvw,
			BVW_INFO_VIDEO_FOURCC, &value);
	video_fcc = (guint32) g_value_get_int (&value);
	g_value_unset (&value);

	bacon_video_widget_get_metadata (totem->bvw,
			BVW_INFO_AUDIO_FOURCC, &value);
	audio_fcc = (guint32) g_value_get_int (&value);

	if (audio_fcc == 0 && video_fcc == 0)
	{
		totem_action_error (msg, totem);
		return;
	}

	error_dialog =
		gtk_message_dialog_new (GTK_WINDOW (totem->win),
				GTK_DIALOG_MODAL,
				GTK_MESSAGE_ERROR,
				GTK_BUTTONS_NONE,
				"%s", msg);
	gtk_dialog_add_buttons (GTK_DIALOG (error_dialog),
			GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT,
			_("Download"), GTK_RESPONSE_ACCEPT,
			NULL);
	gtk_dialog_set_default_response (GTK_DIALOG (error_dialog),
			GTK_RESPONSE_OK);
	gtk_window_set_modal (GTK_WINDOW (error_dialog), TRUE);
	res = gtk_dialog_run (GTK_DIALOG (error_dialog));
	gtk_widget_destroy (error_dialog);

	if (res != GTK_RESPONSE_ACCEPT)
		return FALSE;

	if (totem_download_from_fourcc (GTK_WINDOW (totem->win),
				video_fcc, audio_fcc) < 0)
		return FALSE;

	return TRUE;
}
#endif

void
totem_action_error_and_exit (char *msg, Totem *totem)
{
	GtkWidget *error_dialog;

	error_dialog =
		gtk_message_dialog_new (NULL,
				GTK_DIALOG_MODAL,
				GTK_MESSAGE_ERROR,
				GTK_BUTTONS_OK,
				"%s", msg);
	gtk_dialog_set_default_response (GTK_DIALOG (error_dialog),
			GTK_RESPONSE_OK);
	gtk_window_set_modal (GTK_WINDOW (error_dialog), TRUE);
	gtk_dialog_run (GTK_DIALOG (error_dialog));

	totem_action_exit (totem);
}

void
totem_action_exit (Totem *totem)
{
	if (gtk_main_level () > 0)
		gtk_main_quit ();

	if (totem == NULL)
		exit (0);

	bacon_message_connection_free (totem->conn);

	if (totem->playlist)
		gtk_widget_hide (GTK_WIDGET (totem->playlist));

	if (totem->win)
		gtk_widget_hide (totem->win);

	if (totem->bvw)
		gtk_widget_destroy (GTK_WIDGET (totem->bvw));

	if (totem->playlist)
	{
		char *path;

		path = g_build_filename (G_DIR_SEPARATOR_S, g_get_home_dir (),
				".gnome2", "totem.pls", NULL);
		gtk_playlist_save_current_playlist (totem->playlist, path);
		g_free (path);

		gtk_widget_destroy (GTK_WIDGET (totem->playlist));
	}

	exit (0);
}

gboolean
main_window_destroy_cb (GtkWidget *widget, GdkEvent *event, Totem *totem)
{
	totem_action_exit (totem);

	return FALSE;
}

static void
play_pause_set_label (Totem *totem, TotemStates state)
{
	GtkWidget *image;
	char *image_path;

	switch (state)
	{
	case STATE_PLAYING:
		totem_statusbar_set_text (TOTEM_STATUSBAR (totem->statusbar),
				_("Playing"));
		image_path = gnome_program_locate_file (NULL,
				GNOME_FILE_DOMAIN_APP_DATADIR,
				"totem/stock_media_pause.png", FALSE, NULL);
		break;
	case STATE_PAUSED:
		totem_statusbar_set_text (TOTEM_STATUSBAR (totem->statusbar),
				_("Paused"));
		image_path = gnome_program_locate_file (NULL,
				GNOME_FILE_DOMAIN_APP_DATADIR,
				"totem/stock_media_play.png", FALSE, NULL);
		break;
	case STATE_STOPPED:
		totem_statusbar_set_text (TOTEM_STATUSBAR (totem->statusbar),
				_("Stopped"));
		image_path = gnome_program_locate_file (NULL,
				GNOME_FILE_DOMAIN_APP_DATADIR,
				"totem/stock_media_play.png", FALSE, NULL);
		break;
	default:
		return;
	}

	image = glade_xml_get_widget (totem->xml, "tmw_play_pause_button_image");
	gtk_image_set_from_file (GTK_IMAGE (image), image_path);
	image = glade_xml_get_widget (totem->xml, "tcw_pp_button_image");
	gtk_image_set_from_file (GTK_IMAGE (image), image_path);
	g_free (image_path);
}

static void
volume_set_image (Totem *totem, int vol)
{
	GtkWidget *image;
	char *filename, *path;

	vol = CLAMP (vol, 0, 100);
	if (vol == 0)
		filename = "totem/rhythmbox-volume-zero.png";
	else if (vol <= 100 / 3)
		filename = "totem/rhythmbox-volume-min.png";
	else if (vol <= 2 * 100 / 3)
		filename = "totem/rhythmbox-volume-medium.png";
	else
		filename = "totem/rhythmbox-volume-max.png";

	path = gnome_program_locate_file (NULL,
			GNOME_FILE_DOMAIN_APP_DATADIR,
			filename,
			FALSE, NULL);
	image = glade_xml_get_widget (totem->xml, "tmw_volume_image");
	gtk_image_set_from_file (GTK_IMAGE (image), path);
	image = glade_xml_get_widget (totem->xml, "tcw_volume_image");
	gtk_image_set_from_file (GTK_IMAGE (image), path);
	g_free (path);
}

void
totem_action_play (Totem *totem, int offset)
{
	GError *err = NULL;
	int retval;

	if (totem->mrl == NULL)
		return;

	retval = bacon_video_widget_play (totem->bvw, offset , 0, &err);
	play_pause_set_label (totem, retval ? STATE_PLAYING : STATE_STOPPED);
	if (retval == FALSE)
	{
		char *msg;

		msg = g_strdup_printf(_("Totem could not play '%s'.\n"
					"Reason: %s."),
				totem->mrl,
				err->message);
		gtk_playlist_set_playing (totem->playlist, FALSE);
		totem_action_error (msg, totem);
		if (bacon_video_widget_is_playing (totem->bvw) != FALSE)
			totem_action_stop (totem);
		g_free (msg);
	}
}

void
totem_action_set_mrl_and_play (Totem *totem, char *mrl)
{
	if (totem_action_set_mrl (totem, mrl) == TRUE)
		totem_action_play (totem, 0);
}

void
totem_action_load_media (Totem *totem, MediaType type)
{
	const char **mrls;
	char *mrl;

	if (bacon_video_widget_can_play (totem->bvw, type) == FALSE)
	{
		totem_action_error (_("Totem cannot play this type of media because you do not have the appropriate plugins to handle it.\n"
					"Install the necessary plugins and restart Totem to be able to play this media."), totem);
		return;
	}

	mrls = bacon_video_widget_get_mrls (totem->bvw, type);
	if (mrls == NULL)
	{
		totem_action_error (_("Totem could not play this media although a plugin is present to handle it.\n"
					"You might want to check that a disc is present in the drive and that it is correctly configured."),
				totem);
		return;
	}

	totem_action_open_files (totem, (char **)mrls, FALSE);
}

void
totem_action_play_media (Totem *totem, MediaType type)
{
	char *mrl;

	totem_action_load_media (totem, type);
	mrl = gtk_playlist_get_current_mrl (totem->playlist);
	totem_action_set_mrl_and_play (totem, mrl);
	g_free (mrl);
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
		mrl = gtk_playlist_get_current_mrl (totem->playlist);
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
		totem_action_play (totem, 0);
	} else {
		if (bacon_video_widget_get_speed (totem->bvw) == SPEED_PAUSE)
		{
			bacon_video_widget_set_speed (totem->bvw, SPEED_NORMAL);
			play_pause_set_label (totem, STATE_PLAYING);
		} else {
			bacon_video_widget_set_speed (totem->bvw, SPEED_PAUSE);
			play_pause_set_label (totem, STATE_PAUSED);
		}
	}
}

void
totem_action_fullscreen_toggle (Totem *totem)
{
	if (totem->controls_visibility == TOTEM_CONTROLS_FULLSCREEN)
	{
		gboolean auto_resize;

		popup_hide (totem);
		bacon_video_widget_set_fullscreen (totem->bvw, FALSE);
		gtk_window_unfullscreen (GTK_WINDOW(totem->win));
		bacon_video_widget_set_show_cursor (totem->bvw, TRUE);
		scrsaver_enable (totem->scr);

		/* Auto-resize */
		auto_resize = gconf_client_get_bool (totem->gc,
				GCONF_PREFIX"/auto_resize", NULL);

		bacon_video_widget_set_auto_resize (totem->bvw, auto_resize);

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
		update_fullscreen_size (totem);
		bacon_video_widget_set_fullscreen (totem->bvw, TRUE);
		bacon_video_widget_set_auto_resize (totem->bvw, FALSE);
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

	path = g_build_filename (G_DIR_SEPARATOR_S, g_get_home_dir (),
			".gnome2", "totem.pls", NULL);

	if (g_file_test (path, G_FILE_TEST_EXISTS) == FALSE)
	{
		g_free (path);
		return;
	}

	g_signal_handlers_disconnect_by_func
		(G_OBJECT (totem->playlist),
		 playlist_changed_cb, totem);

	if (gtk_playlist_add_mrl (totem->playlist, path, NULL) == FALSE)
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

	mrl = gtk_playlist_get_current_mrl (totem->playlist);
	if (totem->mrl == NULL
			|| (totem->mrl != NULL && mrl != NULL
				&& strcmp (totem->mrl, mrl) != 0))
	{
		totem_action_set_mrl (totem, mrl);
	} else if (totem->mrl != NULL) {
		gtk_playlist_set_playing (totem->playlist, TRUE);
	}

	g_free (mrl);

	return;
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

		//FIXME this doesn't work if the metadata arrives later
		totem_statusbar_set_time_and_length (TOTEM_STATUSBAR
				(totem->statusbar), 0, time / 1000);

		widget = glade_xml_get_widget (totem->xml, "tstw_skip_spinbutton");
		gtk_spin_button_set_range (GTK_SPIN_BUTTON (widget),
				0, (gdouble) time / 1000);

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
		text = g_strdup_printf (_("%s - Totem"), name);
		gtk_window_set_title (GTK_WINDOW (totem->win), text);
		g_free (text);
	} else {
		totem_statusbar_set_time_and_length (TOTEM_STATUSBAR
				(totem->statusbar), 0, -1);

		widget = glade_xml_get_widget (totem->xml, "tstw_skip_spinbutton");
		gtk_spin_button_set_range (GTK_SPIN_BUTTON (widget), 0, 0);

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
		gtk_window_set_title (GTK_WINDOW (totem->win), _("Totem"));
	}
}

gboolean
totem_action_set_mrl (Totem *totem, const char *mrl)
{
	GtkWidget *widget;
	gboolean retval = TRUE;

	bacon_video_widget_stop (totem->bvw);

	if (totem->mrl != NULL)
	{
		g_free (totem->mrl);
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
		update_seekable (totem, TRUE);

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

		/* Set the logo */
		totem->mrl = g_strdup(LOGO_PATH);
		bacon_video_widget_set_logo_mode (totem->bvw, TRUE);
		bacon_video_widget_set_logo (totem->bvw, totem->mrl);

		update_mrl_label (totem, NULL);
	} else {
		char *name;
		gboolean caps, custom, first_try;
		GError *err = NULL;

		first_try = TRUE;
		bacon_video_widget_set_logo_mode (totem->bvw, FALSE);

try_open_again:
		retval = bacon_video_widget_open (totem->bvw, mrl, &err);
		totem->mrl = g_strdup (mrl);
		custom = FALSE;
		name = totem_get_nice_name_for_stream (totem);
		if (name == NULL)
		{
			name = gtk_playlist_get_current_title
				(totem->playlist,
				 &custom);
		}

		/* Play/Pause */
		gtk_widget_set_sensitive (totem->pp_button, TRUE);
		widget = glade_xml_get_widget (totem->xml, "tmw_play_menu_item");
		gtk_widget_set_sensitive (widget, TRUE);
		widget = glade_xml_get_widget (totem->xml, "trcm_play");
		gtk_widget_set_sensitive (widget, TRUE);
		gtk_widget_set_sensitive (totem->fs_pp_button, TRUE);

		update_mrl_label (totem, name);
		if (custom == FALSE)
		{
			gtk_playlist_set_title
				(GTK_PLAYLIST (totem->playlist), name);
		}

		/* Seek bar */
		update_seekable (totem, FALSE);

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

		/* Set the playlist */
		gtk_playlist_set_playing (totem->playlist, retval);

		g_free (name);

		if (retval == FALSE && first_try == TRUE)
		{
			char *msg;
			gboolean try_again;

			msg = g_strdup_printf(_("Totem could not play '%s'.\n"
						"Reason: %s."),
					mrl,
					err->message);
#ifdef HAVE_X86
			try_again = totem_action_error_try_download
				(msg, totem);
			g_free (msg);
			first_try = FALSE;
			if (try_again != FALSE)
				goto try_open_again;
#else
			totem_action_error (msg, totem);
#endif
			g_free (msg);
		} else if (first_try == FALSE) {
			retval = FALSE;
		}
	}
	update_buttons (totem);
	update_media_menu_items (totem);

	return retval;
}

static gboolean
totem_playing_dvd (Totem *totem)
{
	if (totem->mrl == NULL)
		return FALSE;

	return !strcmp("dvd:/", totem->mrl);
}

static gboolean
totem_is_media (const char *mrl)
{
	if (mrl == NULL)
		return FALSE;

	if (strncmp ("cdda:", mrl, strlen ("cdda:")) == 0)
		return TRUE;
	if (strncmp ("dvd:",mrl, strlen ("dvd:")) == 0)
		return TRUE;
	if (strncmp ("vcd:", mrl, strlen ("vcd:")) == 0)
		return TRUE;

	return FALSE;
}

void
totem_action_previous (Totem *totem)
{
	char *mrl;

	if (totem_playing_dvd (totem) == FALSE &&
		gtk_playlist_has_previous_mrl (totem->playlist) == FALSE
		&& gtk_playlist_get_repeat (totem->playlist) == FALSE)
		return;

        if (totem_playing_dvd (totem) == TRUE)
        {
                bacon_video_widget_dvd_event (totem->bvw, BVW_DVD_PREV_CHAPTER);
        } else {
                gtk_playlist_set_previous (totem->playlist);
                mrl = gtk_playlist_get_current_mrl (totem->playlist);
                totem_action_set_mrl_and_play (totem, mrl);
                g_free (mrl);
        }
}

void
totem_action_next (Totem *totem)
{
	char *mrl;

	if (totem_playing_dvd (totem) == FALSE &&
			gtk_playlist_has_next_mrl (totem->playlist) == FALSE
			&& gtk_playlist_get_repeat (totem->playlist) == FALSE)
		return;

	if (totem_playing_dvd (totem) == TRUE)
	{
		bacon_video_widget_dvd_event (totem->bvw, BVW_DVD_NEXT_CHAPTER);
	} else {
		gtk_playlist_set_next (totem->playlist);
		mrl = gtk_playlist_get_current_mrl (totem->playlist);
		totem_action_set_mrl_and_play (totem, mrl);
		g_free (mrl);
	}
}

void
totem_action_seek_relative (Totem *totem, int off_sec)
{
	GError *err = NULL;
	int oldsec,  sec;

	if (bacon_video_widget_is_seekable (totem->bvw) == FALSE)
		return;
	if (totem->mrl == NULL)
		return;

	oldsec = bacon_video_widget_get_current_time (totem->bvw);
	if ((oldsec + off_sec) < 0)
		sec = 0;
	else
		sec = oldsec + off_sec;

	bacon_video_widget_play (totem->bvw, 0, sec, &err);
	play_pause_set_label (totem, STATE_PLAYING);
	if (err != NULL)
	{
		char *msg;

		msg = g_strdup_printf(_("Totem could not play '%s'.\n"
					"Reason: %s."),
				totem->mrl,
				err->message);
		gtk_playlist_set_playing (totem->playlist, FALSE);
		if (bacon_video_widget_is_playing (totem->bvw) != FALSE)
			totem_action_stop (totem);
		totem_action_error (msg, totem);
		g_free (msg);
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
	volume_set_image (totem, vol + off_pct);
}

void
totem_action_toggle_aspect_ratio (Totem *totem)
{
	bacon_video_widget_toggle_aspect_ratio (totem->bvw);
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

		/* We can't use g_filename_from_uri, as we don't know if
		 * the uri is in locale or UTF8 encoding */
		filename = gnome_vfs_get_local_path_from_uri (p->data);
		if (filename == NULL)
			filename = g_strdup (p->data);

		if (filename != NULL && 
				(g_file_test (filename, G_FILE_TEST_IS_REGULAR
					| G_FILE_TEST_EXISTS)
				|| strstr (filename, "://") != NULL))
		{
			if (empty_pl == TRUE && cleared == FALSE)
			{
				/* The function that calls us knows better
				 * if we should be doing something with the 
				 * changed playlist ... */
				g_signal_handlers_disconnect_by_func
					(G_OBJECT (totem->playlist),
					 playlist_changed_cb, totem);
				gtk_playlist_clear (totem->playlist);
				cleared = TRUE;
			}
			gtk_playlist_add_mrl (totem->playlist, filename, NULL); 
		}
		g_free (filename);
		g_free (p->data);
	}

	g_list_free (file_list);

	/* ... and reconnect because we're nice people */
	if (cleared == TRUE)
	{
		char *mrl;

		g_signal_connect (G_OBJECT (totem->playlist),
				"changed", G_CALLBACK (playlist_changed_cb),
				totem);
		mrl = gtk_playlist_get_current_mrl (totem->playlist);
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
	totem_action_play_pause (totem);
}

static void
on_previous_button_clicked (GtkButton *button, Totem *totem)
{
	totem_action_previous (totem);
}

static void
on_next_button_clicked (GtkButton *button, Totem *totem)
{
	totem_action_next (totem);
}

static void
on_playlist_button_toggled (GtkToggleButton *button, Totem *totem)
{
	gboolean state;

	state = gtk_toggle_button_get_active (button);
	if (state == TRUE)
		gtk_widget_show (GTK_WIDGET (totem->playlist));
	else
		gtk_widget_hide (GTK_WIDGET (totem->playlist));
}

static void
on_recent_file_activate (EggRecentViewGtk *view, EggRecentItem *item,
                         Totem *totem)
{
	char *uri;

	uri = egg_recent_item_get_uri (item);

	gtk_playlist_add_mrl (totem->playlist, uri, NULL);
	egg_recent_model_add_full (totem->recent_model, item);

	g_free (uri);
}

/* This is only called when we are playing a DVD */
static void
on_title_change_event (GtkWidget *win, const char *string, Totem *totem)
{
	update_mrl_label (totem, string);
	gtk_playlist_set_title (GTK_PLAYLIST (totem->playlist), string);
}

static void
on_channels_change_event (BaconVideoWidget *bvw, Totem *totem)
{
	update_dvd_menu_sub_lang (totem);
}

static void
on_got_metadata_event (BaconVideoWidget *bvw, Totem *totem)
{
	bacon_video_widget_properties_update
		(BACON_VIDEO_WIDGET_PROPERTIES (totem->properties),
		 totem->bvw, FALSE);
}

static int
on_buffering_cancel_event (GtkWidget *dialog, int response, Totem *totem)
{
	bacon_video_widget_close (totem->bvw);
	play_pause_set_label (totem, STATE_PAUSED);

	gtk_widget_destroy (dialog);
	totem->buffer_dialog = NULL;
	totem->buffer_label = NULL;

	g_free (totem->mrl);
	totem->mrl = NULL;

	return TRUE;
}

static void
on_error_event (BaconVideoWidget *bvw, char *message, Totem *totem)
{
	totem_action_error (message, totem);
}

static void
on_buffering_event (BaconVideoWidget *bvw, int percentage, Totem *totem)
{
	char *msg;

	if (percentage > 0 && totem->buffer_dialog == NULL)
	{
		GtkWidget *image, *hbox;

		totem->buffer_dialog = gtk_dialog_new_with_buttons (_("Buffering"),
				GTK_WINDOW (totem->win),
				GTK_DIALOG_DESTROY_WITH_PARENT,
				GTK_STOCK_CANCEL,
				GTK_RESPONSE_REJECT,
				NULL);
		gtk_dialog_set_has_separator (GTK_DIALOG(totem->buffer_dialog),
				FALSE);
		gtk_window_set_modal (GTK_WINDOW (totem->buffer_dialog), TRUE);
		gtk_container_set_border_width
			(GTK_CONTAINER (GTK_DIALOG(totem->buffer_dialog)->vbox),
			 8);
		hbox = gtk_hbox_new (FALSE, 12);
		gtk_container_add
			(GTK_CONTAINER (GTK_DIALOG(totem->buffer_dialog)->vbox),
			 hbox);

		image = gtk_image_new_from_stock (GTK_STOCK_DIALOG_INFO,
				GTK_ICON_SIZE_DIALOG);
		gtk_container_add (GTK_CONTAINER (hbox), image);

		msg = g_strdup_printf (_("Buffering: %d%%"), percentage);
		totem->buffer_label = gtk_label_new (msg);
		g_free (msg);
		gtk_container_add (GTK_CONTAINER(hbox), totem->buffer_label);

		g_signal_connect (G_OBJECT (totem->buffer_dialog),
				"response",
				G_CALLBACK (on_buffering_cancel_event),
				totem);
		g_signal_connect (G_OBJECT (totem->buffer_dialog),
				"delete-event",
				G_CALLBACK (on_buffering_cancel_event),
				totem);

		gtk_widget_show_all (totem->buffer_dialog);
		long_action ();
	} else if (percentage == 100) {
		if (totem->buffer_dialog != NULL)
			gtk_widget_destroy (totem->buffer_dialog);
		totem->buffer_dialog = NULL;
		totem->buffer_label = NULL;
	} else if (totem->buffer_label != NULL) {
		msg = g_strdup_printf (_("Buffering: %d%%"), percentage);
		gtk_label_set_text (GTK_LABEL (totem->buffer_label), msg);
		g_free (msg);
		long_action ();
	}
}

static void
update_seekable (Totem *totem, gboolean force_false)
{
	GtkWidget *widget;
	gboolean caps;

	if (force_false == FALSE)
		caps = bacon_video_widget_is_seekable (totem->bvw);
	else
		caps = FALSE;

	/* Check if the stream is seekable */
	gtk_widget_set_sensitive (totem->seek, caps);
	gtk_widget_set_sensitive (totem->fs_seek, caps);

	widget = glade_xml_get_widget (totem->xml, "tmw_skip_forward_menu_item");
	gtk_widget_set_sensitive (widget, caps);
	widget = glade_xml_get_widget (totem->xml, "tmw_skip_backwards_menu_item");
	gtk_widget_set_sensitive (widget, caps);
	widget = glade_xml_get_widget (totem->xml, "trcm_skip_forward");
	gtk_widget_set_sensitive (widget, caps);
	widget = glade_xml_get_widget (totem->xml, "trcm_skip_backwards");
	gtk_widget_set_sensitive (widget, caps);
	widget = glade_xml_get_widget (totem->xml, "tmw_skip_to_menu_item");
	gtk_widget_set_sensitive (widget, caps);
	widget = glade_xml_get_widget (totem->xml, "tstw_ok_button");
	gtk_widget_set_sensitive (widget, caps);
}

static void
update_current_time (BaconVideoWidget *bvw, int current_time, int stream_length,
		int current_position, Totem *totem)
{
	if (bacon_video_widget_get_logo_mode (totem->bvw) == TRUE
			|| (current_time == 0 && stream_length == 0))
	{
		totem_statusbar_set_time_and_length
			(TOTEM_STATUSBAR (totem->statusbar),
			 current_time / 1000, -1);
	} else {
		totem_statusbar_set_time_and_length
			(TOTEM_STATUSBAR (totem->statusbar),
			current_time / 1000, stream_length / 1000);
	}

	if (totem->seek_lock == FALSE)
	{
		gtk_adjustment_set_value (totem->seekadj,
				(float) current_position);
		gtk_adjustment_set_value (totem->fs_seekadj,
				(float) current_position);
	}
}

static void
update_volume_sliders (Totem *totem)
{
	if (totem->vol_lock == FALSE)
	{
		int volume;

		totem->vol_lock = TRUE;
		volume = bacon_video_widget_get_volume (totem->bvw);

		if (totem->volume_first_time || (totem->prev_volume != volume &&
				totem->prev_volume != -1 && volume != -1))
		{
			totem->volume_first_time = 0;
			gtk_adjustment_set_value (totem->voladj,
					(float) volume);
			gtk_adjustment_set_value (totem->fs_voladj,
					(float) volume);
			volume_set_image (totem, volume);
		}

		totem->prev_volume = volume;
		totem->vol_lock = FALSE;
	}
}

static int
update_cb_often (Totem *totem)
{
	if (totem->bvw == NULL)
		return TRUE;

	update_volume_sliders (totem);

	return TRUE;
}

static int
update_cb_rare (Totem *totem)
{
	if (totem->bvw == NULL)
		return TRUE;

	update_seekable (totem, FALSE);

	return TRUE;
}

static gboolean
seek_slider_pressed_cb (GtkWidget *widget, GdkEventButton *event, Totem *totem)
{
	totem->seek_lock = TRUE;
	return FALSE;
}

static gboolean
seek_slider_released_cb (GtkWidget *widget, GdkEventButton *event, Totem *totem)
{
	if (GTK_WIDGET(widget) == totem->fs_seek)
	{
		totem_action_play (totem,
				(gint) totem->fs_seekadj->value);
		/* Update the seek adjustment */
		gtk_adjustment_set_value (totem->seekadj,
				gtk_adjustment_get_value
				(totem->fs_seekadj));
	} else {
		totem_action_play (totem,
				(gint) totem->seekadj->value);
		/* Update the fullscreen seek adjustment */
		gtk_adjustment_set_value (totem->fs_seekadj,
				gtk_adjustment_get_value
				(totem->seekadj));
	}

	totem->seek_lock = FALSE;
	return FALSE;
}

static void
vol_cb (GtkWidget *widget, Totem *totem)
{
	if (totem->vol_lock == FALSE)
	{
		totem->vol_lock = TRUE;
		if (GTK_WIDGET(widget) == totem->fs_volume)
		{
			bacon_video_widget_set_volume
				(totem->bvw, (gint) totem->fs_voladj->value);

			/* Update the volume adjustment */
			gtk_adjustment_set_value (totem->voladj, 
					gtk_adjustment_get_value
					(totem->fs_voladj));
		} else {
			bacon_video_widget_set_volume
				(totem->bvw, (gint) totem->voladj->value);
			/* Update the fullscreen volume adjustment */
			gtk_adjustment_set_value (totem->fs_voladj, 
					gtk_adjustment_get_value
					(totem->voladj));

		}

		volume_set_image (totem, (gint) totem->voladj->value);
		totem->vol_lock = FALSE;
	}
}

gboolean
totem_action_open_files (Totem *totem, char **list, gboolean ignore_first)
{
	int i;
	gboolean cleared = FALSE;

	i = (ignore_first ? 1 : 0 );

	for ( ; list[i] != NULL; i++)
	{
		char *filename;

		/* Get the subtitle part out for our tests */
		filename = totem_create_full_path (list[i]);

		if (g_file_test (filename, G_FILE_TEST_IS_REGULAR)
				|| strstr (filename, "#") != NULL
				|| strstr (filename, "://") != NULL
				|| strncmp (filename, "dvd:", 4) == 0
				|| strncmp (filename, "vcd:", 4) == 0
				|| strncmp (filename, "cdda:", 5) == 0
				|| strncmp (filename, "cd:", 3) == 0)
		{
			if (cleared == FALSE)
			{
				/* The function that calls us knows better
				 * if we should be doing something with the 
				 * changed playlist ... */
				g_signal_handlers_disconnect_by_func
					(G_OBJECT (totem->playlist),
					 playlist_changed_cb, totem);
				gtk_playlist_clear (totem->playlist);
				cleared = TRUE;
			}
			if (strcmp (list[i], "dvd:") == 0)
			{
				totem_action_load_media (totem, MEDIA_DVD);
				continue;
			} else if (strcmp (list[i], "vcd:") == 0) {
				totem_action_load_media (totem, MEDIA_VCD);
				continue;
			} else if (strcmp (list[i], "cd:") == 0) {
				totem_action_load_media (totem, MEDIA_CDDA);
				continue;
			} else if (gtk_playlist_add_mrl (totem->playlist,
						filename, NULL) == TRUE)
			{
                                EggRecentItem *item;

				if (strstr (filename, "file:///") == NULL)
					continue;

				item = egg_recent_item_new_from_uri (filename);
				egg_recent_item_add_group (item, "Totem");
				egg_recent_model_add_full
					(totem->recent_model, item);
			}
		}

		g_free (filename);
	}

	/* ... and reconnect because we're nice people */
	if (cleared == TRUE)
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

	fs = gtk_file_selection_new (_("Select files"));
	gtk_file_selection_set_select_multiple (GTK_FILE_SELECTION (fs), TRUE);
	if (path != NULL)
	{
		gtk_file_selection_set_filename (GTK_FILE_SELECTION (fs),
				path);
		g_free (path);
		path = NULL;
	}
	response = gtk_dialog_run (GTK_DIALOG (fs));
	gtk_widget_hide (fs);

	if (response == GTK_RESPONSE_OK)
	{
		char **filenames, *mrl;

		filenames = gtk_file_selection_get_selections
			(GTK_FILE_SELECTION (fs));
		totem_action_open_files (totem, filenames, FALSE);
		if (filenames[0] != NULL)
		{
			char *tmp;

			tmp = g_path_get_dirname (filenames[0]);
			path = g_strconcat (tmp, G_DIR_SEPARATOR_S, NULL);
			g_free (tmp);
		}
		g_strfreev (filenames);

		mrl = gtk_playlist_get_current_mrl (totem->playlist);
		totem_action_set_mrl_and_play (totem, mrl);
		g_free (mrl);
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

	filename = gnome_program_locate_file (NULL,
			GNOME_FILE_DOMAIN_APP_DATADIR,
			"totem/uri.glade", TRUE, NULL);
	if (filename == NULL)
	{
		totem_action_error (_("Couldn't load the 'Open Location...'"
					" interface.\nMake sure that Totem"
					" is properly installed."),
				totem);
		return;
	}

	glade = glade_xml_new (filename, NULL, NULL);
	if (glade == NULL)
	{
		g_free (filename);
		totem_action_error (_("Couldn't load the 'Open Location...'"
					" interface.\nMake sure that Totem"
					" is properly installed."),
				totem);
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
			totem_action_open_files (totem,
					(char **) filenames, FALSE);

			mrl = gtk_playlist_get_current_mrl (totem->playlist);
			totem_action_set_mrl_and_play (totem, mrl);
			g_free (mrl);
		}
	}

	gtk_widget_destroy (dialog);
	g_object_unref (glade);
	return;
}

static void
on_play_dvd1_activate (GtkButton *button, Totem *totem)
{
	totem_action_play_media (totem, MEDIA_DVD);
}

static void
on_play_vcd1_activate (GtkButton *button, Totem *totem)
{
	totem_action_play_media (totem, MEDIA_VCD);
}

static void
on_play_cd1_activate (GtkButton *button, Totem *totem)
{
	totem_action_play_media (totem, MEDIA_CDDA);
}

static void
on_eject1_activate (GtkButton *button, Totem *totem)
{
	gtk_playlist_set_playing (totem->playlist, FALSE);

	if (bacon_video_widget_eject (totem->bvw) == FALSE)
	{
		char *msg;

		msg = _("Totem could not eject the optical media.");
		totem_action_error (msg, totem);
	}
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
on_toggle_aspect_ratio1_activate (GtkButton *button, Totem *totem)
{
	totem_action_toggle_aspect_ratio (totem);
}

static void
on_show_playlist1_activate (GtkButton *button, Totem *totem)
{
	GtkWidget *toggle;
	gboolean state;

	toggle = glade_xml_get_widget (totem->xml, "tmw_playlist_button");

	state = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (toggle));
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (toggle), !state);
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
	gtk_playlist_set_repeat (totem->playlist,
			gtk_check_menu_item_get_active (checkmenuitem));
}

static void
on_always_on_top1_activate (GtkCheckMenuItem *checkmenuitem, Totem *totem)
{
	totem_gdk_window_set_always_on_top (GTK_WIDGET (totem->win)->window,
			gtk_check_menu_item_get_active (checkmenuitem));
}

static void
show_controls (Totem *totem, gboolean visible, gboolean fullscreen_behaviour)
{
	GtkWidget *menubar, *controlbar, *statusbar, *item, *bvw_vbox;
	GtkRequisition requisition;
	
	menubar = glade_xml_get_widget (totem->xml, "tmw_bonobodockitem");
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
	
	if (totem->controls_visibility == TOTEM_CONTROLS_HIDDEN) {
		gtk_window_resize (GTK_WINDOW(totem->win),
			GTK_WIDGET(totem->bvw)->allocation.width,
			GTK_WIDGET(totem->bvw)->allocation.height);
	}
	else if (totem->controls_visibility == TOTEM_CONTROLS_VISIBLE) {
		/* We get GtkWindow requisition then we substract
		bvw's requisition to get other widget's height and
		use that to resize properly GtkWindow */
		gtk_widget_size_request (totem->win, &requisition);
		/* Getting controls requisition */
		requisition.height = requisition.height - GTK_WIDGET(totem->bvw)->requisition.height;
		requisition.width = requisition.width - GTK_WIDGET(totem->bvw)->requisition.width;
		gtk_window_resize (GTK_WINDOW(totem->win),
			GTK_WIDGET(totem->bvw)->allocation.width + requisition.width,
			GTK_WIDGET(totem->bvw)->allocation.height + requisition.height);
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

static void
on_about1_activate (GtkButton *button, Totem *totem)
{
	static GtkWidget *about = NULL;
	GdkPixbuf *pixbuf = NULL;
	const gchar *authors[] =
	{
		"Bastien Nocera <hadess@hadess.net>",
		"Guenter Bartsch <guenter@users.sourceforge.net>",
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
			"Copyright \xc2\xa9 2002 Bastien Nocera",
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
	g_object_add_weak_pointer (G_OBJECT (about),
			(void**)&(about));
	gtk_window_set_transient_for (GTK_WINDOW (about),
			GTK_WINDOW (totem->win));

	gtk_widget_show(about);
}

static char *
screenshot_make_filename_helper (char *filename, gboolean desktop_exists)
{
	if (desktop_exists == TRUE)
	{
		return g_build_filename (G_DIR_SEPARATOR_S,
				g_get_home_dir (), ".gnome-desktop",
				filename, NULL);
	} else {
		return g_build_filename (G_DIR_SEPARATOR_S,
				g_get_home_dir (), filename, NULL);
	}
}

static char *
screenshot_make_filename (Totem *totem)
{
	GtkWidget *radiobutton, *entry;
	gboolean on_desktop;
	char *fullpath, *filename;
	int i = 0;
	gboolean desktop_exists;

	radiobutton = glade_xml_get_widget (totem->xml, "tsw_save2desk_radiobutton");
	on_desktop = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON
			(radiobutton));

	/* Test if we have a desktop directory */
	fullpath = g_build_path (G_DIR_SEPARATOR_S, g_get_home_dir (),
			".gnome-desktop", NULL);
	desktop_exists = g_file_test (fullpath, G_FILE_TEST_EXISTS);
	g_free (fullpath);

	if (on_desktop == TRUE)
	{
		filename = g_strdup_printf (_("Screenshot%d.png"), i);
		fullpath = screenshot_make_filename_helper (filename,
				desktop_exists);

		while (g_file_test (fullpath, G_FILE_TEST_EXISTS) == TRUE
				&& i < G_MAXINT)
		{
			i++;
			g_free (filename);
			g_free (fullpath);

			filename = g_strdup_printf (_("Screenshot%d.png"), i);
			fullpath = screenshot_make_filename_helper (filename,
					desktop_exists);
		}

		g_free (filename);
	} else {
		entry = glade_xml_get_widget (totem->xml, "tsw_save2file_combo_entry");
		if (gtk_entry_get_text (GTK_ENTRY (entry)) == NULL)
			return NULL;

		fullpath = g_strdup (gtk_entry_get_text (GTK_ENTRY (entry)));
	}

	return fullpath;
}

static void
on_radiobutton_shot_toggled (GtkToggleButton *togglebutton, Totem *totem)
{	
	GtkWidget *radiobutton, *entry;

	radiobutton = glade_xml_get_widget (totem->xml, "tsw_save2file_radiobutton");
	entry = glade_xml_get_widget (totem->xml, "tsw_save2file_fileentry");
	gtk_widget_set_sensitive (entry, gtk_toggle_button_get_active
			(GTK_TOGGLE_BUTTON (radiobutton)));
}

static void
hide_screenshot (GtkWidget *widget, int trash, Totem *totem)
{
	GtkWidget *dialog;

	dialog = glade_xml_get_widget (totem->xml, "totem_screenshot_window");
	gtk_widget_hide (dialog);
}

static void
on_take_screenshot1_activate (GtkButton *button, Totem *totem)
{
	GdkPixbuf *pixbuf, *scaled;
	GtkWidget *dialog, *image, *entry;
	int response, width, height;
	char *filename;
	GError *err = NULL;

	if (bacon_video_widget_can_get_frames (totem->bvw, &err) == FALSE)
	{
		char *msg;

		if (err == NULL)
			return;

		msg = g_strdup_printf (_("Totem could not get a screenshot of that film.\nReason: %s."), err->message);
		g_error_free (err);
		totem_action_error (msg, totem);
		g_free (msg);
		return;
	}

	pixbuf = bacon_video_widget_get_current_frame (totem->bvw);
	if (pixbuf == NULL)
	{
		totem_action_error (_("Totem could not get a screenshot of that film.\nPlease file a bug, this isn't supposed to happen"), totem);
		return;
	}

	filename = screenshot_make_filename (totem);
	height = 200;
	width = height * gdk_pixbuf_get_width (pixbuf)
		/ gdk_pixbuf_get_height (pixbuf);
	scaled = gdk_pixbuf_scale_simple (pixbuf, width, height,
			GDK_INTERP_BILINEAR);

	dialog = glade_xml_get_widget (totem->xml, "totem_screenshot_window");
	image = glade_xml_get_widget (totem->xml, "tsw_shot_image");
	gtk_image_set_from_pixbuf (GTK_IMAGE (image), scaled);
	gdk_pixbuf_unref (scaled);
	entry = glade_xml_get_widget (totem->xml, "tsw_save2file_combo_entry");
	gtk_entry_set_text (GTK_ENTRY (entry), filename);
	g_free (filename);
	response = gtk_dialog_run (GTK_DIALOG (dialog));
	gtk_widget_hide (dialog);

	if (response == GTK_RESPONSE_OK)
	{
		filename = screenshot_make_filename (totem);
		if (g_file_test (filename, G_FILE_TEST_EXISTS) == TRUE)
		{
			totem_action_error (_("File '%s' already exists.\nThe screenshot was not saved."), totem);
			gdk_pixbuf_unref (pixbuf);
			g_free (filename);
			return;
		}

		if (gdk_pixbuf_save (pixbuf, filename, "png", &err, NULL)
				== FALSE)
		{
			char *msg;

			msg = g_strdup_printf (_("There was an error saving the screenshot.\nDetails: %s"), err->message);
			totem_action_error (msg, totem);
			g_free (msg);
			g_error_free (err);
		}

		g_free (filename);
	}

	gdk_pixbuf_unref (pixbuf);
}

static void
on_properties1_activate (GtkButton *button, Totem *totem)
{
	if (totem->properties == NULL)
	{
		totem_action_error (_("Totem couldn't show the movie properties window.\n"
					"Make sure that Totem is correctly installed."),
				totem);
		return;
	}

	gtk_widget_show (totem->properties);
	gtk_window_set_transient_for (GTK_WINDOW (totem->properties),
			GTK_WINDOW (totem->win));
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

	bacon_video_widget_play (totem->bvw, 0, sec * 1000, &err);

	if (err != NULL)
	{
		char *msg;

		msg = g_strdup_printf(_("Totem could not seek in '%s'.\n"
					"Reason: %s."),
				totem->mrl,
				err->message);
		totem_action_stop (totem);
		gtk_playlist_set_playing (totem->playlist, FALSE);
		if (bacon_video_widget_is_playing (totem->bvw) != FALSE)
			totem_action_stop (totem);
		totem_action_error (msg, totem);
		g_free (msg);
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
	str = bacon_video_widget_properties_time_to_string (sec);
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

void
totem_action_remote (Totem *totem, TotemRemoteCommand cmd, const char *url)
{
	switch (cmd) {
	case TOTEM_REMOTE_COMMAND_PLAY:
		totem_action_play (totem, 0);
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
		if (url != NULL)
			gtk_playlist_add_mrl (totem->playlist, url, NULL);
		break;
	case TOTEM_REMOTE_COMMAND_REPLACE:
		if (url != NULL)
		{
			gtk_playlist_clear (totem->playlist);
			gtk_playlist_add_mrl (totem->playlist, url, NULL);
		}
		break;
	default:
		break;
	}
}

void
totem_button_pressed_remote_cb (TotemRemote *remote, TotemRemoteCommand cmd,
		Totem *totem)
{
	totem_action_remote (totem, cmd, NULL);
}

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
	mrl = gtk_playlist_get_current_mrl (totem->playlist);

	if (totem->mrl == NULL
			|| (totem->mrl != NULL && mrl != NULL
			&& strcmp (totem->mrl, mrl) != 0))
	{
		totem_action_set_mrl_and_play (totem, mrl);
	} else if (totem->mrl != NULL) {
		gtk_playlist_set_playing (totem->playlist, TRUE);
	}

	g_free (mrl);
}

static void
current_removed_cb (GtkWidget *playlist, Totem *totem)
{
	char *mrl;

	/* Set play button status */
	play_pause_set_label (totem, STATE_STOPPED);
	mrl = gtk_playlist_get_current_mrl (totem->playlist);

	if (mrl == NULL)
	{
		gtk_playlist_set_at_start (totem->playlist);
		update_buttons (totem);
		mrl = gtk_playlist_get_current_mrl (totem->playlist);
	} else {
		update_buttons (totem);
	}

	totem_action_set_mrl_and_play (totem, mrl);
	g_free (mrl);
}

void
playlist_repeat_toggle_cb (GtkPlaylist *playlist, gboolean repeat, Totem *totem)
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
update_fullscreen_size (Totem *totem)
{
	gdk_screen_get_monitor_geometry (gdk_screen_get_default (),
			gdk_screen_get_monitor_at_window
			(gdk_screen_get_default (),
			 totem->win->window),
			&totem->fullscreen_rect);
}

static gboolean
totem_is_fullscreen (Totem *totem) {
	if (totem->controls_visibility == TOTEM_CONTROLS_FULLSCREEN)
		return TRUE;
	return FALSE;
}

static void
size_changed_cb (GdkScreen *screen, Totem *totem)
{
	update_fullscreen_size (totem);

	gtk_window_move (GTK_WINDOW (totem->exit_popup),
			totem->fullscreen_rect.x,
			totem->fullscreen_rect.y);
	gtk_window_move (GTK_WINDOW (totem->control_popup),
			totem->fullscreen_rect.x,
			totem->fullscreen_rect.height
			- totem->control_popup_height);
}

static gboolean
popup_hide (Totem *totem)
{
	if (totem->seek_lock == TRUE)
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
	int width;
	gint pointer_x, pointer_y;
	GdkModifierType state;
	if (totem_is_fullscreen (totem) == FALSE) 
		return FALSE;

	if (totem->popup_in_progress == TRUE)
		return FALSE;

	totem->popup_in_progress = TRUE;
	if (totem->popup_timeout != 0)
	{
		gtk_timeout_remove (totem->popup_timeout);
		totem->popup_timeout = 0;
	}

	gtk_window_get_size (GTK_WINDOW (totem->control_popup),
			&width, &totem->control_popup_height);
	gtk_window_move (GTK_WINDOW (totem->exit_popup),
			totem->fullscreen_rect.x,
			totem->fullscreen_rect.y);
	gtk_window_move (GTK_WINDOW (totem->control_popup),
			totem->fullscreen_rect.x,
			totem->fullscreen_rect.height
			- totem->control_popup_height);

	gtk_widget_show_all (totem->exit_popup);
	gtk_widget_show_all (totem->control_popup);
	bacon_video_widget_set_show_cursor (totem->bvw, TRUE);

	totem->popup_timeout = gtk_timeout_add (5000,
			(GtkFunction) popup_hide, totem);
	totem->popup_in_progress = FALSE;
	gdk_window_get_pointer (widget->window, &pointer_x, &pointer_y, &state);

	return FALSE;
}

static gboolean
on_video_button_press_event (GtkButton *button, GdkEventButton *event,
		Totem *totem)
{
	if (event->type == GDK_BUTTON_PRESS && event->button == 3)
	{
		GtkWidget *menu;

		menu = glade_xml_get_widget (totem->xml, "totem_right_click_menu");
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

	if (gtk_playlist_has_next_mrl (totem->playlist) == FALSE
			&& gtk_playlist_get_repeat (totem->playlist) == FALSE)
	{
		char *mrl;

		/* Set play button status */
		play_pause_set_label (totem, STATE_PAUSED);
		gtk_playlist_set_at_start (totem->playlist);
		update_buttons (totem);
		mrl = gtk_playlist_get_current_mrl (totem->playlist);
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
		if (totem->action == 0)
			totem->action++;
		else
			totem->action = 0;
		break;
	case XF86XK_AudioPrev:
	case GDK_B:
	case GDK_b:
		totem_action_previous (totem);
		break;
	case GDK_C:
	case GDK_c:
		bacon_video_widget_dvd_event (totem->bvw, BVW_DVD_CHAPTER_MENU);
		if (totem->action == 1)
			totem->action++;
		else
			totem->action = 0;
		break;
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
					(totem->xml, "tmw_show_controls_menu_item"));
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
					(totem->xml, "tmw_deinterlace_menu_item"));
			value = gtk_check_menu_item_get_active (item);
			gtk_check_menu_item_set_active (item, !value);
		}

		if (totem->action == 3)
			totem->action++;
		else
			totem->action = 0;
		break;
	case GDK_M:
	case GDK_m:
		bacon_video_widget_dvd_event (totem->bvw, BVW_DVD_ROOT_MENU);
		break;
	case XF86XK_AudioNext:
	case GDK_N:
	case GDK_n:
		totem_action_next (totem);
		if (totem->action == 5)
			totem_action_set_mrl_and_play (totem, "v4l:/");
		totem->action = 0;
		break;
	case GDK_O:
	case GDK_o:
		totem_action_fullscreen (totem, FALSE);
		on_open1_activate (NULL, totem);
		if (totem->action == 4)
			totem->action++;
		else
			totem->action = 0;
		break;
	case XF86XK_AudioPlay:
	case XF86XK_AudioPause:
	case GDK_p:
	case GDK_P:
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
	case GDK_T:
		if (totem->action == 2)
			totem->action++;
		else
			totem->action = 0;
		break;
	case GDK_Escape:
		totem_action_fullscreen (totem, FALSE);
		break;
	case GDK_Left:
		totem_action_seek_relative (totem, SEEK_BACKWARD_OFFSET);
		break;
	case GDK_Right:
		totem_action_seek_relative (totem, SEEK_FORWARD_OFFSET);
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
	if (totem_playing_dvd (totem) == TRUE)
	{
		has_item = TRUE;
	} else {
		has_item = gtk_playlist_has_previous_mrl (totem->playlist);
	}

	item = glade_xml_get_widget (totem->xml, "tmw_previous_button");
	gtk_widget_set_sensitive (item, has_item);
	item = glade_xml_get_widget (totem->xml, "tcw_previous_button");
	gtk_widget_set_sensitive (item, has_item);
	item = glade_xml_get_widget (totem->xml, "tmw_previous_chapter_menu_item");
	gtk_widget_set_sensitive (item, has_item);
	item = glade_xml_get_widget (totem->xml, "trcm_previous_chapter");
	gtk_widget_set_sensitive (item, has_item);

	/* Next */
	/* FIXME Need way to detect if DVD Title has no more chapters */
	if (totem_playing_dvd (totem) == TRUE)
	{
		has_item = TRUE;
	} else {
		has_item = gtk_playlist_has_next_mrl (totem->playlist);
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
		char *lang;

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

		lang = language_name_get_from_code (l->data);
		group = add_item_to_menu (totem, menu, lang, current,
				i, is_lang, group);
		i++;
		g_free (lang);
	}

	gtk_widget_show (menu);

	return menu;
}

static void
update_dvd_menu_sub_lang (Totem *totem)
{
	GtkWidget *item, *submenu;
	gboolean playing_dvd;
	GtkWidget *lang_menu, *sub_menu;

	lang_menu = NULL;
	sub_menu = NULL;

	playing_dvd = totem_playing_dvd (totem);

	if (playing_dvd != FALSE)
	{
		GList *list;
		int current;

		list = bacon_video_widget_get_languages (totem->bvw);
		current = bacon_video_widget_get_language (totem->bvw);
		lang_menu = create_submenu (totem, list, current, TRUE);
		totem_g_list_deep_free (list);

		list = bacon_video_widget_get_subtitles (totem->bvw);
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
	item = glade_xml_get_widget (totem->xml, "tmw_play_dvd_menu_item");
	g_signal_connect (G_OBJECT (item), "activate",
			G_CALLBACK (on_play_dvd1_activate), totem);
	item = glade_xml_get_widget (totem->xml, "tmw_play_vcd_menu_item");
	g_signal_connect (G_OBJECT (item), "activate",
			G_CALLBACK (on_play_vcd1_activate), totem);
	item = glade_xml_get_widget (totem->xml, "tmw_play_audio_cd_menu_item");
	g_signal_connect (G_OBJECT (item), "activate",
			G_CALLBACK (on_play_cd1_activate), totem);
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
	item = glade_xml_get_widget (totem->xml, "tmw_toggle_aspect_ratio_menu_item");
	g_signal_connect (G_OBJECT (item), "activate",
			G_CALLBACK (on_toggle_aspect_ratio1_activate), totem);
	item = glade_xml_get_widget (totem->xml, "tmw_show_playlist_menu_item");
	g_signal_connect (G_OBJECT (item), "activate",
			G_CALLBACK (on_show_playlist1_activate), totem);
	item = glade_xml_get_widget (totem->xml, "tmw_repeat_mode_menu_item");
	gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (item),
			gtk_playlist_get_repeat (totem->playlist));
	g_signal_connect (G_OBJECT (item), "toggled",
			G_CALLBACK (on_repeat_mode1_toggled), totem);
	item = glade_xml_get_widget (totem->xml, "tmw_quit_menu_item");
	g_signal_connect (G_OBJECT (item), "activate",
			G_CALLBACK (on_quit1_activate), totem);
	item = glade_xml_get_widget (totem->xml, "tmw_about_menu_item");
	g_signal_connect (G_OBJECT (item), "activate",
			G_CALLBACK (on_about1_activate), totem);
	item = glade_xml_get_widget (totem->xml, "tmw_take_screenshot_menu_item");
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

	/* Screenshot dialog */
	item = glade_xml_get_widget (totem->xml, "totem_screenshot_window");
	g_signal_connect (G_OBJECT (item), "delete-event",
			G_CALLBACK (hide_screenshot), totem);
	item = glade_xml_get_widget (totem->xml, "tsw_save2file_radiobutton");
	g_signal_connect (G_OBJECT (item), "toggled",
			G_CALLBACK (on_radiobutton_shot_toggled),
			totem);
	item = glade_xml_get_widget (totem->xml, "tsw_save2desk_radiobutton");
	g_signal_connect (G_OBJECT (item), "toggled",
			G_CALLBACK (on_radiobutton_shot_toggled),
			totem);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (item), TRUE);

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

	/* Drag'n'Drop */
	item = glade_xml_get_widget (totem->xml, "tmw_playlist_button");
	g_signal_connect (G_OBJECT (item), "drag_data_received",
			G_CALLBACK (drop_playlist_cb), totem);
	gtk_drag_dest_set (item, GTK_DEST_DEFAULT_ALL,
			target_table, 1, GDK_ACTION_COPY);

	/* Exit */
	g_signal_connect (G_OBJECT (totem->win), "delete-event",
			G_CALLBACK (main_window_destroy_cb), totem);
	g_signal_connect (G_OBJECT (totem->win), "destroy",
			G_CALLBACK (main_window_destroy_cb), totem);

	/* Screen size changes */
	g_signal_connect (G_OBJECT (gdk_screen_get_default ()),
			"size-changed", G_CALLBACK (size_changed_cb), totem);

	/* Motion notify for the Popups */
	item = glade_xml_get_widget (totem->xml, "totem_exit_fullscreen_window");
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

	/* Control Popup Sliders */
	g_signal_connect (G_OBJECT(totem->fs_seek), "button_press_event",
			G_CALLBACK (seek_slider_pressed_cb), totem);
	g_signal_connect (G_OBJECT(totem->fs_seek), "button_release_event",
			G_CALLBACK (seek_slider_released_cb), totem);
	g_signal_connect (G_OBJECT(totem->fs_volume), "value-changed",
			G_CALLBACK (vol_cb), totem);

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
	g_signal_connect (G_OBJECT(totem->seek), "button_press_event",
			G_CALLBACK (seek_slider_pressed_cb), totem);
	g_signal_connect (G_OBJECT(totem->seek), "button_release_event",
			G_CALLBACK (seek_slider_released_cb), totem);
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
	item = glade_xml_get_widget (totem->xml, "tmw_previous_chapter_menu_item");
	g_signal_connect (G_OBJECT (item), "activate",
			G_CALLBACK (on_previous_button_clicked), totem);
	item = glade_xml_get_widget (totem->xml, "tmw_skip_forward_menu_item");
	g_signal_connect (G_OBJECT (item), "activate",
			G_CALLBACK (on_skip_forward1_activate), totem);
	item = glade_xml_get_widget (totem->xml, "tmw_skip_backwards_menu_item");
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

	/* Update the UI */
	gtk_timeout_add (600, (GtkFunction) update_cb_often, totem);
	gtk_timeout_add (1200, (GtkFunction) update_cb_rare, totem);
}

static void
video_widget_create (Totem *totem) 
{
	GError *err = NULL;
	GtkWidget *container;

	totem->bvw = BACON_VIDEO_WIDGET
		(bacon_video_widget_new (-1, -1, FALSE, &err));

	if (totem->bvw == NULL)
	{
		char *msg;

		msg = g_strdup_printf (_("Totem could not startup:\n%s"),
				err != NULL ? err->message : _("No reason"));
		gtk_playlist_set_playing (totem->playlist, FALSE);

		if (err != NULL)
			g_error_free (err);

		gtk_widget_hide (totem->win);

		totem_action_error_and_exit (msg, totem);
		g_free (msg);
	}

	totem_preferences_tvout_setup (totem);
	totem_preferences_visuals_setup (totem);

	/* Let's set a name. Will make debugging easier */
	gtk_widget_set_name (GTK_WIDGET(totem->bvw), "bvw");
	
	container = glade_xml_get_widget (totem->xml, "tmw_bvw_vbox");
	gtk_container_add (GTK_CONTAINER (container),
			GTK_WIDGET (totem->bvw));

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

static void
totem_setup_recent (Totem *totem)
{
	GtkWidget *menu_item;
	GtkWidget *menu;

	menu_item = glade_xml_get_widget (totem->xml, "tmw_menu_item_movie");
	menu = gtk_menu_item_get_submenu (GTK_MENU_ITEM (menu_item));
	menu_item = glade_xml_get_widget (totem->xml, "tmw_menu_recent_separator");

	g_return_if_fail (menu != NULL);
	g_return_if_fail (menu_item != NULL);

	totem->recent_model = egg_recent_model_new (EGG_RECENT_MODEL_SORT_MRU);

	/* it would be better if we just filtered by mime-type, but there
	 * doesn't seem to be an easy way to figure out which mime-types we
	 * can handle */
	egg_recent_model_set_filter_groups (totem->recent_model, "Totem", NULL);

	totem->recent_view = egg_recent_view_gtk_new (menu, menu_item);
	egg_recent_view_gtk_show_icons (EGG_RECENT_VIEW_GTK
			(totem->recent_view), FALSE);
	egg_recent_model_set_limit (totem->recent_model, 5);
	egg_recent_view_set_model (EGG_RECENT_VIEW (totem->recent_view),
			totem->recent_model);
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

	gtk_window_present (GTK_WINDOW (totem->win));
	totem_action_remote (totem, command, url);

	g_free (url);
}

static void
process_command_line (BaconMessageConnection *conn, int argc, char **argv)
{
	int i, command;
	char *line, *full_path;

	if (argc == 1)
		return;

	i = 2;

	if (strlen (argv[1]) > 3 && strncmp (argv[1], "--", 2) != 0)
	{
		command = TOTEM_REMOTE_COMMAND_ENQUEUE;
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
	GError *err = NULL;
	GdkPixbuf *pix;

	bindtextdomain (GETTEXT_PACKAGE, GNOMELOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);

	/* Please also translate your language into its native language
	 * in src/languages.h
	 * for example: French -> Francais
	 * use C UTF-8 strings */
	g_set_application_name (_("Totem Movie Player"));

	/* FIXME See http://bugzilla.gnome.org/show_bug.cgi?id=111349 */
	if (XInitThreads () == 0)
	{
		gtk_init (&argc, &argv);
		totem_action_error_and_exit (_("Could not initialise the "
					"thread-safe libraries.\n"
					"Verify your system installation. Totem"
					" will now exit."), NULL);
	}

	g_thread_init (NULL);
	gdk_threads_init ();

	gnome_program_init ("totem", VERSION,
			LIBGNOMEUI_MODULE,
			argc, argv,
			GNOME_PARAM_APP_DATADIR, DATADIR,
			GNOME_PARAM_POPT_TABLE, options,
			GNOME_PARAM_NONE);

	glade_init ();
	gnome_vfs_init ();
	if ((gc = gconf_client_get_default ()) == NULL)
	{
		char *str;

		str = g_strdup_printf (_("Totem couln't initialise the \n"
					"configuration engine:\n%s"),
				err->message);
		totem_action_error_and_exit (str, NULL);
		g_error_free (err);
		g_free (str);
	}
	gnome_authentication_manager_init ();

	filename = gnome_program_locate_file (NULL,
			GNOME_FILE_DOMAIN_APP_DATADIR,
			"totem/totem.glade", TRUE, NULL);
	if (filename == NULL)
	{
		totem_action_error_and_exit (_("Couldn't load the main "
					"interface (totem.glade).\n"
					"Make sure that Totem"
					" is properly installed."), NULL);
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
	}

	/* Init totem itself */
	totem->prev_volume = -1;
	totem->gc = gc;

	/* Main window */
	totem->xml = glade_xml_new (filename, NULL, NULL);
	if (totem->xml == NULL)
	{
		g_free (filename);
		totem_action_error_and_exit (_("Couldn't load the main "
					"interface (totem.glade).\n"
					"Make sure that Totem"
					" is properly installed."), NULL);
	}
	g_free (filename);

	totem->win = glade_xml_get_widget (totem->xml, "totem_main_window");
	filename = g_build_filename (G_DIR_SEPARATOR_S, DATADIR,
			"totem", "media-player-48.png", NULL);
	gtk_window_set_default_icon_from_file (filename, NULL);
	g_free (filename);

	/* The playlist */
	filename = g_build_filename (G_DIR_SEPARATOR_S, DATADIR,
			"totem", "playlist-playing.png", NULL);
	pix = gdk_pixbuf_new_from_file (filename, NULL);
	g_free (filename);

	filename = g_build_filename (G_DIR_SEPARATOR_S, DATADIR,
			"totem", "playlist.glade", NULL);
	totem->playlist = GTK_PLAYLIST (gtk_playlist_new (filename, pix));
	g_free (filename);

	if (totem->playlist == NULL)
	{
		totem_action_error_and_exit (_("Couldn't load the interface "
					"for the playlist."
					"\nMake sure that Totem"
					" is properly installed."),
				totem);
	}
	filename = g_build_filename (G_DIR_SEPARATOR_S, DATADIR,
			"totem", "playlist-24.png", NULL);
	gtk_window_set_icon_from_file (GTK_WINDOW (totem->playlist),
			filename, NULL);
	g_free (filename);

	/* The rest of the widgets */
	totem->seek = glade_xml_get_widget (totem->xml, "tmw_seek_hscale");
	totem->seekadj = gtk_range_get_adjustment (GTK_RANGE (totem->seek));
	totem->volume = glade_xml_get_widget (totem->xml, "tmw_volume_hscale");
	totem->voladj = gtk_range_get_adjustment (GTK_RANGE (totem->volume));
	totem->exit_popup = glade_xml_get_widget (totem->xml, "totem_exit_fullscreen_window");
	totem->control_popup = glade_xml_get_widget (totem->xml, "totem_controls_window");
	totem->fs_seek = glade_xml_get_widget (totem->xml, "tcw_seek_hscale");
	totem->fs_seekadj = gtk_range_get_adjustment
		(GTK_RANGE (totem->fs_seek));
	totem->fs_volume = glade_xml_get_widget (totem->xml, "tcw_volume_hscale");
	totem->fs_voladj = gtk_range_get_adjustment
		(GTK_RANGE (totem->fs_volume));
	totem->volume_first_time = 1;
	totem->fs_pp_button = glade_xml_get_widget (totem->xml, "tcw_pp_button");
	totem->statusbar = glade_xml_get_widget (totem->xml, "tmw_statusbar");
	
	/* Properties */
	totem->properties = bacon_video_widget_properties_new ();

	totem_setup_recent (totem);
	totem_callback_connect (totem);

	/* Show ! gtk_main_iteration trickery to show all the widgets
	 * we have so far */
	gtk_widget_show_all (totem->win);
	update_fullscreen_size (totem);
	long_action ();

	totem->controls_visibility = TOTEM_CONTROLS_VISIBLE;

	/* Show ! (again) the video widget this time. */
	video_widget_create (totem);
	long_action ();

	totem->scr = scrsaver_new (GDK_DISPLAY ());

	/* The prefs after the video widget is connected */
	totem_setup_preferences (totem);

	if (argc > 1)
	{
		if (totem_action_open_files (totem, argv, TRUE))
			totem_action_play_pause (totem);
		else
			totem_action_set_mrl (totem, NULL);
	} else {
		totem_action_restore_pl (totem);
	}

	if (bacon_message_connection_get_is_server (totem->conn) == TRUE)
	{
		bacon_message_connection_set_callback (totem->conn,
				(BaconMessageReceivedFunc)
				totem_message_connection_receive_cb, totem);
	}

#ifdef HAVE_REMOTE
	totem->remote = totem_remote_new ();
	g_signal_connect (totem->remote, "button_pressed",
			  G_CALLBACK (totem_button_pressed_remote_cb), totem);
#endif

	gtk_main ();

	return 0;
}
