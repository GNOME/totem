/* 
 * Copyright (C) 2001-2002 Bastien Nocera <hadess@hadess.net>
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
#include <glade/glade.h>
#include <libgnomevfs/gnome-vfs.h>
#include <gconf/gconf-client.h>
#include <string.h>
#include <X11/XF86keysym.h>

#include "gnome-authn-manager.h"
#include "gtk-message.h"
#include "gtk-xine.h"
#include "gtk-xine-properties.h"
#include "gtk-playlist.h"
#include "rb-ellipsizing-label.h"
#include "bacon-cd-selection.h"
#include "totem-statusbar.h"

#include "egg-recent-model.h"
#include "egg-recent-view.h"
#include "egg-recent-view-gtk.h"

#include "totem.h"
#include "totem-remote.h"

#include "debug.h"

#define SEEK_FORWARD_OFFSET 60000
#define SEEK_BACKWARD_OFFSET -15000

#define SEEK_FORWARD_SHORT_OFFSET 20000
#define SEEK_BACKWARD_SHORT_OFFSET -20000

typedef enum {
	STATE_PLAYING,
	STATE_PAUSED,
	STATE_STOPPED
} TotemStates;

struct Totem {
	/* Control window */
	GladeXML *xml;
	GtkWidget *win;
	GtkWidget *treeview;
	GtkWidget *gtx;
	GtkWidget *prefs;
	GtkWidget *properties;
	GtkWidget *statusbar;

	/* Play/Pause */
	GtkWidget *pp_button;
	/* fullscreen Play/Pause */
	GtkWidget *fs_pp_button;

	/* Seek */
	GtkWidget *seek;
	GtkAdjustment *seekadj;
	gboolean seek_lock;

	/* Volume */
	GtkWidget *volume;
	GtkAdjustment *voladj;
	gboolean vol_lock;
	gfloat prev_volume;
	int volume_first_time;

	/* exit fullscreen Popup */
	GtkWidget *exit_popup;

	/* control fullscreen Popup */
	GtkWidget *control_popup;
	GtkWidget *fs_seek;
	GtkAdjustment *fs_seekadj;
	GtkWidget *fs_volume;
	GtkAdjustment *fs_voladj;
	gint control_popup_height;

	guint popup_timeout;

	/* recent file stuff */
	EggRecentModel *recent_model;
	EggRecentViewGtk *recent_view;

	/* other */
	char *mrl;
	GtkPlaylist *playlist;
	GConfClient *gc;
	TotemRemote *remote;
	GtkMessageQueue *queue;
};

static const GtkTargetEntry target_table[] = {
	{ "text/uri-list", 0, 0 },
};

static const GtkTargetEntry source_table[] = {
	{ "_NETSCAPE_URL", 0, 0 },
	{ "text/uri-list", 0, 1 },
};

static gboolean popup_hide (Totem *totem);
static void update_buttons (Totem *totem);
static void update_dvd_menu_items (Totem *totem);
static void on_play_pause_button_clicked (GtkToggleButton *button,
		gpointer user_data);
static void playlist_changed_cb (GtkWidget *playlist, gpointer user_data);

static void
long_action (void)
{
	while (gtk_events_pending ())
		gtk_main_iteration ();
}

static void
totem_action_error (char *msg, GtkWindow *parent)
{
	static GtkWidget *error_dialog = NULL;

	if (error_dialog != NULL)
		return;

	error_dialog =
		gtk_message_dialog_new (parent,
				GTK_DIALOG_MODAL,
				GTK_MESSAGE_ERROR,
				GTK_BUTTONS_OK,
				"%s", msg);
	gtk_dialog_set_default_response (GTK_DIALOG (error_dialog),
			GTK_RESPONSE_OK);
	gtk_widget_show (error_dialog);
	gtk_dialog_run (GTK_DIALOG (error_dialog));
	gtk_widget_destroy (error_dialog);
	error_dialog = NULL;
}

void
totem_action_exit (Totem *totem)
{
	gtk_message_queue_unref (totem->queue);

	gtk_main_quit ();

	gtk_widget_hide (totem->win);
	gtk_widget_hide (GTK_WIDGET (totem->playlist));

	gtk_widget_destroy (totem->gtx);
	gtk_widget_destroy (GTK_WIDGET (totem->playlist));

	exit (0);
}

gboolean
main_window_destroy_cb (GtkWidget *widget, GdkEvent *event, gpointer user_data)
{
	Totem *totem = (Totem *)user_data;

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
	}

	image = glade_xml_get_widget (totem->xml, "pp_image");
	gtk_image_set_from_file (GTK_IMAGE (image), image_path);
	image = glade_xml_get_widget (totem->xml, "fs_pp_image");
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
	image = glade_xml_get_widget (totem->xml, "volume_image");
	gtk_image_set_from_file (GTK_IMAGE (image), path);
	image = glade_xml_get_widget (totem->xml, "fs_volume_image");
	gtk_image_set_from_file (GTK_IMAGE (image), path);
	g_free (path);
}

void
totem_action_play (Totem *totem, int offset)
{
	int retval;

	if (totem->mrl == NULL)
		return;

	retval = gtk_xine_play (GTK_XINE (totem->gtx), offset , 0);
	play_pause_set_label (totem, retval ? STATE_PLAYING : STATE_STOPPED);
}

void
totem_action_set_mrl_and_play (Totem *totem, char *mrl)
{
	if (totem_action_set_mrl (totem, mrl) == TRUE)
		totem_action_play (totem, 0);
}

void
totem_action_play_media (Totem *totem, MediaType type)
{
	const char **mrls;
	char *mrl;

	if (gtk_xine_can_play (GTK_XINE (totem->gtx), type) == FALSE)
	{
		totem_action_error (_("Totem cannot play this type of media because you do not have the appropriate plugins to handle it.\n"
					"Install the necessary plugins and restart Totem to be able to play this media."), GTK_WINDOW (totem->win));
		return;
	}

	mrls = gtk_xine_get_mrls (GTK_XINE (totem->gtx), type);
	if (mrls == NULL)
	{
		totem_action_error (_("Totem could not play this media although a plugin is present to handle it.\n"
					"You might want to check that a disc is present in the drive and that it is correctly configured."),
				GTK_WINDOW (totem->win));
		return;
	}

	totem_action_open_files (totem, (char **)mrls, FALSE);
	mrl = gtk_playlist_get_current_mrl (totem->playlist);
	totem_action_set_mrl_and_play (totem, mrl);
	g_free (mrl);
}

void
totem_action_stop (Totem *totem)
{
	gtk_xine_stop (GTK_XINE (totem->gtx));
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

	if (!gtk_xine_is_playing(GTK_XINE(totem->gtx)))
	{
		totem_action_play (totem, 0);
	} else {
		if (gtk_xine_get_speed (GTK_XINE(totem->gtx)) == SPEED_PAUSE)
		{
			gtk_xine_set_speed (GTK_XINE(totem->gtx), SPEED_NORMAL);
			play_pause_set_label (totem, STATE_PLAYING);
		} else {
			gtk_xine_set_speed (GTK_XINE(totem->gtx), SPEED_PAUSE);
			play_pause_set_label (totem, STATE_PAUSED);
		}
	}
}

void
totem_action_fullscreen_toggle (Totem *totem)
{
	gboolean new_state;

	new_state = !gtk_xine_is_fullscreen (GTK_XINE (totem->gtx));
	gtk_xine_set_fullscreen (GTK_XINE (totem->gtx), new_state);
	/* Hide the popup when switching fullscreen off */
	if (new_state == FALSE)
		popup_hide (totem);
}

void
totem_action_fullscreen (Totem *totem, gboolean state)
{
	if (gtk_xine_is_fullscreen (GTK_XINE (totem->gtx)) == state)
		return;

	totem_action_fullscreen_toggle (totem);
}

static void
update_mrl_label (Totem *totem, const char *name)
{
	gint time;
	char *text;
	GtkWidget *widget;

	if (name != NULL)
	{
		/* Get the length of the stream */
		time = gtk_xine_get_stream_length (GTK_XINE (totem->gtx));
		totem_statusbar_set_time_and_length (TOTEM_STATUSBAR
				(totem->statusbar), 0, time / 1000);

		widget = glade_xml_get_widget (totem->xml, "spinbutton1");
		gtk_spin_button_set_range (GTK_SPIN_BUTTON (widget),
				0, (gdouble) time / 1000);

		/* Update the mrl label */
		text = g_strdup_printf
			("<span size=\"medium\"><b>%s</b></span>", name);

		widget = glade_xml_get_widget (totem->xml, "label1");
		rb_ellipsizing_label_set_markup (RB_ELLIPSIZING_LABEL (widget),
				text);
		widget = glade_xml_get_widget (totem->xml, "custom2");
		rb_ellipsizing_label_set_markup (RB_ELLIPSIZING_LABEL (widget),
				text);

		g_free (text);

		/* Title */
		text = g_strdup_printf (_("%s - Totem"), name);
		gtk_window_set_title (GTK_WINDOW (totem->win), text);
		g_free (text);
	} else {
		totem_statusbar_set_time_and_length (TOTEM_STATUSBAR
				(totem->statusbar), 0, 0);

		widget = glade_xml_get_widget (totem->xml, "spinbutton1");
		gtk_spin_button_set_range (GTK_SPIN_BUTTON (widget), 0, 0);

		/* Update the mrl label */
		text = g_strdup_printf
			("<span size=\"medium\"><b>%s</b></span>",
			 _("No file"));
		widget = glade_xml_get_widget (totem->xml, "label1");
		rb_ellipsizing_label_set_markup (RB_ELLIPSIZING_LABEL (widget),
				text);
		widget = glade_xml_get_widget (totem->xml, "custom2");
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
	char *text;
	gboolean retval = TRUE;

	gtk_xine_stop (GTK_XINE (totem->gtx));

	if (totem->mrl != NULL)
	{
		g_free (totem->mrl);
		gtk_xine_close (GTK_XINE (totem->gtx));
	}

	if (mrl == NULL)
	{
		retval = FALSE;

		gtk_window_set_title (GTK_WINDOW (totem->win), _("Totem"));

		/* Play/Pause */
		gtk_widget_set_sensitive (totem->pp_button, FALSE);
		widget = glade_xml_get_widget (totem->xml, "play1");
		gtk_widget_set_sensitive (widget, FALSE);

		update_mrl_label (totem, NULL);

		/* Seek bar and seek buttons */
		gtk_widget_set_sensitive (totem->seek, FALSE);
		widget = glade_xml_get_widget (totem->xml, "skip_forward1");
		gtk_widget_set_sensitive (widget, FALSE);
		widget = glade_xml_get_widget (totem->xml, "skip_backwards1");
		gtk_widget_set_sensitive (widget, FALSE);
		widget = glade_xml_get_widget (totem->xml, "skip_to1");
		gtk_widget_set_sensitive (widget, FALSE);

		/* Volume */
		widget = glade_xml_get_widget (totem->xml, "volume_hbox");
		gtk_widget_set_sensitive (widget, FALSE);
		widget = glade_xml_get_widget (totem->xml, "volume_up1");
		gtk_widget_set_sensitive (widget, FALSE);
		widget = glade_xml_get_widget (totem->xml, "volume_down1");
		gtk_widget_set_sensitive (widget, FALSE);

		/* Control popup */
		gtk_widget_set_sensitive (totem->fs_seek, FALSE);
		gtk_widget_set_sensitive (totem->fs_pp_button, FALSE);
		widget = glade_xml_get_widget (totem->xml,
				"fs_previous_button"); 
		gtk_widget_set_sensitive (widget, FALSE);
		widget = glade_xml_get_widget (totem->xml, "fs_next_button"); 
		gtk_widget_set_sensitive (widget, FALSE);
		widget = glade_xml_get_widget (totem->xml, "fs_volume_hbox");
		gtk_widget_set_sensitive (widget, FALSE);

		/* Set the logo */
		totem->mrl = g_strdup (LOGO_PATH);
		gtk_xine_set_logo_mode (GTK_XINE (totem->gtx), TRUE);
		if (gtk_xine_open (GTK_XINE (totem->gtx), totem->mrl) == TRUE)
			gtk_xine_play (GTK_XINE (totem->gtx), 0 , 0);

		/* Reset the properties */
		gtk_xine_properties_update
			(GTK_XINE_PROPERTIES (totem->properties),
			 GTK_XINE (totem->gtx), TRUE);
	} else {
		char *title, *name;
		int time;
		gboolean caps;

		gtk_xine_set_logo_mode (GTK_XINE (totem->gtx), FALSE);

		retval = gtk_xine_open (GTK_XINE (totem->gtx), mrl);

		totem->mrl = g_strdup (mrl);
		name = gtk_playlist_mrl_to_title (mrl);

		/* Play/Pause */
		gtk_widget_set_sensitive (totem->pp_button, TRUE);
		widget = glade_xml_get_widget (totem->xml, "play1");
		gtk_widget_set_sensitive (widget, TRUE);
		gtk_widget_set_sensitive (totem->fs_pp_button, TRUE);

		update_mrl_label (totem, name);

		/* Seek bar */
		caps = gtk_xine_is_seekable (GTK_XINE (totem->gtx));
		gtk_widget_set_sensitive (totem->seek, caps);
		widget = glade_xml_get_widget (totem->xml, "skip_forward1");
		gtk_widget_set_sensitive (widget, caps);
		widget = glade_xml_get_widget (totem->xml, "skip_backwards1");
		gtk_widget_set_sensitive (widget, caps);
		widget = glade_xml_get_widget (totem->xml, "skip_to1");
		gtk_widget_set_sensitive (widget, caps);
		gtk_widget_set_sensitive (totem->fs_seek, caps);

		/* Volume */
		caps = gtk_xine_can_set_volume (GTK_XINE (totem->gtx));
		widget = glade_xml_get_widget (totem->xml, "volume_hbox");
		gtk_widget_set_sensitive (widget, caps);
		widget = glade_xml_get_widget (totem->xml, "volume_up1");
		gtk_widget_set_sensitive (widget, caps);
		widget = glade_xml_get_widget (totem->xml, "volume_down1");
		gtk_widget_set_sensitive (widget, caps);
		widget = glade_xml_get_widget (totem->xml, "fs_volume_hbox");
		gtk_widget_set_sensitive (widget, caps);

		/* Set the playlist */
		gtk_playlist_set_playing (totem->playlist, TRUE);

		/* Update the properties */
		gtk_xine_properties_update
			(GTK_XINE_PROPERTIES (totem->properties),
			 GTK_XINE (totem->gtx), FALSE);
	}
	update_buttons (totem);
	update_dvd_menu_items (totem);

	return retval;
}

static gboolean
totem_playing_dvd (Totem *totem)
{
    if (!totem->mrl)
        return FALSE;

    return !strcmp("dvd:/", totem->mrl);
}

void
totem_action_previous (Totem *totem)
{
	char *mrl;

	if (totem_playing_dvd (totem) == FALSE &&
                gtk_playlist_has_previous_mrl (totem->playlist) == FALSE)
		return;

        if (totem_playing_dvd (totem) == TRUE)
        {
                gtk_xine_dvd_event (GTK_XINE (totem->gtx),
                                        GTX_DVD_PREV_CHAPTER);
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
                gtk_playlist_has_next_mrl (totem->playlist) == FALSE)
                return;

        if (totem_playing_dvd (totem) == TRUE)
        {
                gtk_xine_dvd_event (GTK_XINE (totem->gtx), 
                                        GTX_DVD_NEXT_CHAPTER);
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
	int oldsec,  sec;

	if (!gtk_xine_is_seekable (GTK_XINE(totem->gtx)))
		return;
	if (totem->mrl == NULL)
		return;

	oldsec = gtk_xine_get_current_time (GTK_XINE(totem->gtx));
	if ((oldsec + off_sec) < 0)
		sec = 0;
	else
		sec = oldsec + off_sec;

	gtk_xine_play (GTK_XINE(totem->gtx), 0, sec);
	play_pause_set_label (totem, STATE_PLAYING);
}

void
totem_action_volume_relative (Totem *totem, int off_pct)
{
	int vol;

	if (!gtk_xine_can_set_volume (GTK_XINE (totem->gtx)))
		return;

	vol = gtk_xine_get_volume (GTK_XINE (totem->gtx));
	gtk_xine_set_volume (GTK_XINE (totem->gtx), vol + off_pct);
	volume_set_image (totem, vol + off_pct);
}

void
totem_action_toggle_aspect_ratio (Totem *totem)
{
	gtk_xine_toggle_aspect_ratio (GTK_XINE (totem->gtx));
}

void
totem_action_set_scale_ratio (Totem *totem, gfloat ratio)
{
	gtk_xine_set_scale_ratio (GTK_XINE (totem->gtx), ratio);
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
					 playlist_changed_cb, (gpointer) totem);
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
				(gpointer) totem);
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
	 gpointer            user_data)
{
	Totem *totem = (Totem *)user_data;
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
	       gpointer            user_data)
{
	Totem *totem = (Totem *)user_data;
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
	char *text;// = "file:///tmp/";
	int len;

	g_assert (selection_data != NULL);

	if (totem->mrl == NULL)
		return;

	text = gnome_vfs_get_uri_from_local_path (totem->mrl);
	len = strlen (text);

	g_print ("info: %d text: %s len: %d\n", info, text, len);

	gtk_selection_data_set (selection_data,
			selection_data->target,
			8, (guchar *) text, len);

	g_free (text);
}

static void
on_play_pause_button_clicked (GtkToggleButton *button, gpointer user_data)
{
	Totem *totem = (Totem *) user_data;

	totem_action_play_pause (totem);
}

static void
on_previous_button_clicked (GtkButton *button, gpointer user_data)
{
	Totem *totem = (Totem *) user_data;

	totem_action_previous (totem);
}

static void
on_next_button_clicked (GtkButton *button, gpointer user_data)
{
	Totem *totem = (Totem *) user_data;

	totem_action_next (totem);
}

static void
on_playlist_button_toggled (GtkToggleButton *button, gpointer user_data)
{
	gboolean state;
	Totem *totem = (Totem *) user_data;

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
	gchar *uri;
	gchar *filename;

	uri = egg_recent_item_get_uri (item);

	D ("on_recent_file_activate URI: %s", uri);

	filename = gnome_vfs_get_local_path_from_uri (uri);
	if (filename == NULL)
	{
		g_free (uri);
		return;
	}

	gtk_playlist_add_mrl (totem->playlist, filename, NULL);
	egg_recent_model_add_full (totem->recent_model, item);

	g_free (uri);
	g_free (filename);
}

/* This is only called when xine is playing a DVD */
static void
on_title_change_event (GtkWidget *win, const char *string, gpointer user_data)
{
	Totem *totem = (Totem *) user_data;

	update_mrl_label (totem, string);
	gtk_playlist_set_title (GTK_PLAYLIST (totem->playlist), string);
}

static void
update_seekable (Totem *totem)
{
	/* Check if the stream is seekable */
	gtk_widget_set_sensitive (totem->seek,
			gtk_xine_is_seekable (GTK_XINE (totem->gtx)));
	gtk_widget_set_sensitive (totem->fs_seek,
			gtk_xine_is_seekable (GTK_XINE (totem->gtx)));
}

static void
update_current_time (Totem *totem)
{ 
	int time;

	/* Get the length of the stream */
	time = gtk_xine_get_current_time (GTK_XINE (totem->gtx));
	totem_statusbar_set_time (TOTEM_STATUSBAR (totem->statusbar),
			time / 1000);
}

static void
update_sliders (Totem *totem)
{
	gfloat pos;

	if (totem->seek_lock == FALSE)
	{
		totem->seek_lock = TRUE;
		pos = (gfloat) gtk_xine_get_position (GTK_XINE (totem->gtx));
		gtk_adjustment_set_value (totem->seekadj, pos);
		gtk_adjustment_set_value (totem->fs_seekadj, pos);
		totem->seek_lock = FALSE;
	}

	if (totem->vol_lock == FALSE)
	{
		totem->vol_lock = TRUE;
		pos = (gfloat) gtk_xine_get_volume (GTK_XINE (totem->gtx));

		if (totem->volume_first_time || (totem->prev_volume != pos &&
				totem->prev_volume != -1 && pos != -1))
		{
			totem->volume_first_time = 0;
			gtk_adjustment_set_value (totem->voladj, pos);
			gtk_adjustment_set_value (totem->fs_voladj, pos);
			volume_set_image (totem, (gint) pos);
		}

		totem->prev_volume = pos;
		totem->vol_lock = FALSE;
	}
}

static int
update_cb_often (gpointer user_data)
{
	Totem *totem = (Totem *) user_data;

	if (totem->gtx == NULL)
		return TRUE;

	update_current_time (user_data);
	update_sliders (user_data);

	return TRUE;
}

static int
update_cb_rare (gpointer user_data)
{
	Totem *totem = (Totem *) user_data;

	if (totem->gtx == NULL)
		return TRUE;

	update_seekable (user_data);

	return TRUE;
}

static void
seek_cb (GtkWidget *widget, gpointer user_data)
{
	Totem *totem = (Totem *) user_data;

	if (totem->seek_lock == FALSE)
	{
		totem->seek_lock = TRUE;
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
	}
}

static void
vol_cb (GtkWidget *widget, gpointer user_data)
{
	Totem *totem = (Totem *) user_data;

	if (totem->vol_lock == FALSE)
	{
		totem->vol_lock = TRUE;
		if (GTK_WIDGET(widget) == totem->fs_volume)
		{
			gtk_xine_set_volume (GTK_XINE (totem->gtx),
					(gint) totem->fs_voladj->value);

			/* Update the volume adjustment */
			gtk_adjustment_set_value (totem->voladj, 
					gtk_adjustment_get_value
					(totem->fs_voladj));
		} else {
			gtk_xine_set_volume (GTK_XINE (totem->gtx),
					(gint) totem->voladj->value);
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
		char *filename, *subtitle;

		filename = g_strdup (list[i]);
		subtitle = strrchr (filename, '#');
		if (subtitle != NULL)
		{
			*subtitle = 0;
			subtitle++;
		}

		if (g_file_test (filename, G_FILE_TEST_IS_REGULAR
					| G_FILE_TEST_EXISTS)
				|| strstr (filename, "://") != NULL
				|| strncmp (filename, "dvd:", 4) == 0
				|| strncmp (filename, "vcd:", 4) == 0
				|| strncmp (filename, "cdda:", 5) == 0
				|| strncmp (filename, "cd:", 3) == 0)
		{
			g_free (filename);

			if (cleared == FALSE)
			{
				/* The function that calls us knows better
				 * if we should be doing something with the 
				 * changed playlist ... */
				g_signal_handlers_disconnect_by_func
					(G_OBJECT (totem->playlist),
					 playlist_changed_cb, (gpointer) totem);
				gtk_playlist_clear (totem->playlist);
				cleared = TRUE;
			}
			if (strcmp (list[i], "dvd:") == 0)
			{
				totem_action_play_media (totem, MEDIA_DVD);
				continue;
			} else if (strcmp (list[i], "vcd:") == 0) {
				totem_action_play_media (totem, MEDIA_VCD);
				continue;
			} else if (strcmp (list[i], "cd:") == 0) {
				totem_action_play_media (totem, MEDIA_CDDA);
				continue;
			} else if (gtk_playlist_add_mrl (totem->playlist,
						list[i], NULL) == TRUE)
                        {
                                char *uri;
                                EggRecentItem *item;

				if (list[i][0] != G_DIR_SEPARATOR)
					continue;

				uri = gnome_vfs_get_uri_from_local_path
					(list[i]);

				if (uri == NULL) {
					/* ok, if this fails, then it was
					 * something like dvd:/// and we don't
					 * want to add it
					 */
					continue;
				}

				item = egg_recent_item_new_from_uri (uri);
				egg_recent_item_add_group (item, "Totem");
				egg_recent_model_add_full (totem->recent_model,
						item);

				g_free (uri);
			}
		} else {
			g_free (filename);
		}
	}

	/* ... and reconnect because we're nice people */
	if (cleared == TRUE)
	{
		g_signal_connect (G_OBJECT (totem->playlist),
				"changed", G_CALLBACK (playlist_changed_cb),
				(gpointer) totem);
	}

	return cleared;
}

static void
on_open1_activate (GtkButton *button, gpointer user_data)
{
	Totem *totem = (Totem *) user_data;
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
on_play_dvd1_activate (GtkButton *button, gpointer user_data)
{
	Totem *totem = (Totem *) user_data;

	totem_action_play_media (totem, MEDIA_DVD);
}

static void
on_play_vcd1_activate (GtkButton *button, gpointer user_data)
{
	Totem *totem = (Totem *) user_data;

	totem_action_play_media (totem, MEDIA_VCD);
}

static void
on_play_cd1_activate (GtkButton *button, gpointer user_data)
{
	Totem *totem = (Totem *) user_data;

	totem_action_play_media (totem, MEDIA_CDDA);
}

static void
on_play1_activate (GtkButton *button, gpointer user_data)
{
	Totem *totem = (Totem *) user_data;

	totem_action_play_pause (totem);
}

static void
on_full_screen1_activate (GtkButton *button, gpointer user_data)
{
	Totem *totem = (Totem *) user_data;

	totem_action_fullscreen_toggle (totem);
}

static void
on_zoom_1_2_activate (GtkButton *button, gpointer user_data)
{
	Totem *totem = (Totem *) user_data;

	totem_action_set_scale_ratio (totem, 0.5); 
}

static void
on_zoom_1_1_activate (GtkButton *button, gpointer user_data)
{
	Totem *totem = (Totem *) user_data;

	totem_action_set_scale_ratio (totem, 1);
}

static void
on_zoom_2_1_activate (GtkButton *button, gpointer user_data)
{                       
	Totem *totem = (Totem *) user_data;

	totem_action_set_scale_ratio (totem, 2);
}


static void
on_toggle_aspect_ratio1_activate (GtkButton *button, gpointer user_data)
{
	Totem *totem = (Totem *) user_data;

	totem_action_toggle_aspect_ratio (totem);
}

static void
on_show_playlist1_activate (GtkButton *button, gpointer user_data)
{
	Totem *totem = (Totem *) user_data;
	GtkWidget *toggle;
	gboolean state;

	toggle = glade_xml_get_widget (totem->xml, "playlist_button");

	state = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (toggle));
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (toggle), !state);
}

static void
on_fs_exit1_activate (GtkButton *button, gpointer user_data)
{
	Totem *totem = (Totem *) user_data;

	popup_hide (totem);
	totem_action_fullscreen_toggle (totem);
}

static void
on_quit1_activate (GtkButton *button, gpointer user_data)
{
	totem_action_exit ((Totem *) user_data);
}

static void
on_about1_activate (GtkButton *button, gpointer user_data)
{
	static GtkWidget *about = NULL;
	Totem *totem = (Totem *) user_data;
	GdkPixbuf *pixbuf = NULL;
	const gchar *authors[] =
	{
		"Bastien Nocera <hadess@hadess.net>",
		"Guenter Bartsch <guenter@users.sourceforge.net>",
		NULL
	};
	const gchar *documenters[] = { NULL };
	const gchar *translator_credits = _("translator_credits");

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

	about = gnome_about_new(_("Totem"), VERSION,
			"Copyright \xc2\xa9 2002 Bastien Nocera",
			_("Movie Player (based on the Xine libraries)"),
			(const char **)authors,
			(const char **)documenters,
			strcmp (translator_credits, "translator_credits") != 0 ? translator_credits : NULL,
			pixbuf);

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

	radiobutton = glade_xml_get_widget (totem->xml, "radiobutton2");
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
		entry = glade_xml_get_widget (totem->xml, "combo-entry1");
		if (gtk_entry_get_text (GTK_ENTRY (entry)) == NULL)
			return NULL;

		fullpath = g_strdup (gtk_entry_get_text (GTK_ENTRY (entry)));
	}

	return fullpath;
}

static void
on_radiobutton_shot_toggled (GtkToggleButton *togglebutton,
		gpointer user_data)
{	
	Totem *totem = (Totem *)user_data;
	GtkWidget *radiobutton, *entry;

	radiobutton = glade_xml_get_widget (totem->xml, "radiobutton1");
	entry = glade_xml_get_widget (totem->xml, "fileentry1");
	gtk_widget_set_sensitive (entry, gtk_toggle_button_get_active
			(GTK_TOGGLE_BUTTON (radiobutton)));
}

static void
hide_screenshot (GtkWidget *widget, int trash, gpointer user_data)
{
	Totem *totem = (Totem *)user_data;
	GtkWidget *dialog;

	dialog = glade_xml_get_widget (totem->xml, "dialog2");
	gtk_widget_hide (dialog);
}

static void
on_take_screenshot1_activate (GtkButton *button, gpointer user_data)
{
	Totem *totem = (Totem *)user_data;
	GdkPixbuf *pixbuf, *scaled;
	GtkWidget *dialog, *image, *entry;
	int response, width, height;
	char *filename;
	GError *err = NULL;

	pixbuf = gtk_xine_get_current_frame (GTK_XINE (totem->gtx));
	if (pixbuf == NULL)
	{
		totem_action_error (_("Totem could not get a screenshot of that film.\nYou might want to try again at another time."), GTK_WINDOW (totem->win));
		return;
	}

	filename = screenshot_make_filename (totem);
	height = 200;
	width = height * gdk_pixbuf_get_width (pixbuf)
		/ gdk_pixbuf_get_height (pixbuf);
	scaled = gdk_pixbuf_scale_simple (pixbuf, width, height,
			GDK_INTERP_BILINEAR);

	dialog = glade_xml_get_widget (totem->xml, "dialog2");
	image = glade_xml_get_widget (totem->xml, "image1072");
	gtk_image_set_from_pixbuf (GTK_IMAGE (image), scaled);
	gdk_pixbuf_unref (scaled);
	entry = glade_xml_get_widget (totem->xml, "combo-entry1");
	gtk_entry_set_text (GTK_ENTRY (entry), filename);
	g_free (filename);
	response = gtk_dialog_run (GTK_DIALOG (dialog));
	gtk_widget_hide (dialog);

	if (response == GTK_RESPONSE_OK)
	{
		filename = screenshot_make_filename (totem);
		if (g_file_test (filename, G_FILE_TEST_EXISTS) == TRUE)
		{
			totem_action_error (_("File '%s' already exists.\nThe screenshot was not saved."), GTK_WINDOW (totem->win));
			gdk_pixbuf_unref (pixbuf);
			g_free (filename);
			return;
		}

		if (gdk_pixbuf_save (pixbuf, filename, "png", &err, NULL)
				== FALSE)
		{
			char *msg;

			msg = g_strdup_printf (_("There was an error saving the screenshot.\nDetails: %s"), err->message);
			totem_action_error (msg, GTK_WINDOW (totem->win));
			g_free (msg);
			g_error_free (err);
		}

		g_free (filename);
	}

	gdk_pixbuf_unref (pixbuf);
}


static void
on_properties1_activate (GtkButton *button, gpointer user_data)
{
	Totem *totem = (Totem *)user_data;
	GtkWidget *dialog;

	if (totem->properties == NULL)
	{
		totem_action_error (_("Totem couldn't show the movie properties window.\n"
					"Make sure that Totem is correctly installed."),
				GTK_WINDOW (totem->win));
		return;
	}

	gtk_widget_show_all (totem->properties);
	gtk_window_set_transient_for (GTK_WINDOW (totem->properties),
			GTK_WINDOW (totem->win));
}

static void
on_preferences1_activate (GtkButton *button, gpointer user_data)
{
	Totem *totem = (Totem *)user_data;

	gtk_widget_show (totem->prefs);
}

static void
on_dvd_root_menu1_activate (GtkButton *button, gpointer user_data)
{
        Totem *totem = (Totem *)user_data;
        gtk_xine_dvd_event (GTK_XINE (totem->gtx), GTX_DVD_ROOT_MENU);
}

static void
on_dvd_title_menu1_activate (GtkButton *button, gpointer user_data)
{
        Totem *totem = (Totem *)user_data;
        gtk_xine_dvd_event (GTK_XINE (totem->gtx), GTX_DVD_TITLE_MENU);
}

static void
on_dvd_audio_menu1_activate (GtkButton *button, gpointer user_data)
{
        Totem *totem = (Totem *)user_data;
        gtk_xine_dvd_event (GTK_XINE (totem->gtx), GTX_DVD_AUDIO_MENU);
}

static void
on_dvd_angle_menu1_activate (GtkButton *button, gpointer user_data)
{
        Totem *totem = (Totem *)user_data;
        gtk_xine_dvd_event (GTK_XINE (totem->gtx), GTX_DVD_ANGLE_MENU);
}

static void
on_dvd_chapter_menu1_activate (GtkButton *button, gpointer user_data)
{
        Totem *totem = (Totem *)user_data;
        gtk_xine_dvd_event (GTK_XINE (totem->gtx), GTX_DVD_CHAPTER_MENU);
}

static void
commit_hide_skip_to (GtkDialog *dialog, gint response, gpointer user_data)

{
	Totem *totem = (Totem *)user_data;
	GtkWidget *spin;
	int sec;

	gtk_widget_hide (GTK_WIDGET (dialog));

	if (response != GTK_RESPONSE_OK)
		return;

	spin = glade_xml_get_widget (totem->xml, "spinbutton1");
	sec = (int) gtk_spin_button_get_value (GTK_SPIN_BUTTON (spin));

	g_message ("commit_hide_skip_to: %d", sec);

	gtk_xine_play (GTK_XINE(totem->gtx), 0, sec * 1000);
}

static void
hide_skip_to (GtkWidget *widget, GdkEvent *event, gpointer user_data)
{
	gtk_widget_hide (widget);
}

static void
spin_button_value_changed_cb (GtkSpinButton *spinbutton, gpointer user_data)
{
	Totem *totem = (Totem *)user_data;
	GtkWidget *label;
	int sec;
	char *str;

	sec = (int) gtk_spin_button_get_value (GTK_SPIN_BUTTON (spinbutton));
	label = glade_xml_get_widget (totem->xml, "label14");
	str = gtk_xine_properties_time_to_string (sec);
	gtk_label_set_text (GTK_LABEL (label), str);
	g_free (str);
}

static void
on_skip_to1_activate (GtkButton *button, gpointer user_data)
{
	Totem *totem = (Totem *)user_data;
	GtkWidget *dialog;

	dialog = glade_xml_get_widget (totem->xml, "dialog3");
	gtk_widget_show (dialog);
}

static void
on_skip_forward1_activate (GtkButton *button, gpointer user_data)
{
	Totem *totem = (Totem *)user_data;

	totem_action_seek_relative (totem, SEEK_FORWARD_OFFSET);
}

static void
on_skip_backwards1_activate (GtkButton *button, gpointer user_data)
{
	Totem *totem = (Totem *)user_data;

	totem_action_seek_relative (totem, SEEK_BACKWARD_OFFSET);
}

static void
on_volume_up1_activate (GtkButton *button, gpointer user_data)
{
	Totem *totem = (Totem *)user_data;

	totem_action_volume_relative (totem, 8);
}

static void
on_volume_down1_activate (GtkButton *button, gpointer user_data)
{
	Totem *totem = (Totem *)user_data;

	totem_action_volume_relative (totem, -8);
}

static void
hide_prefs (GtkWidget *widget, int trash, gpointer user_data)
{
	Totem *totem = (Totem *)user_data;

	gtk_widget_hide (totem->prefs);
}

static void
on_checkbutton1_toggled (GtkToggleButton *togglebutton, gpointer user_data)
{
	Totem *totem = (Totem *)user_data;
	gboolean value;

	value = gtk_toggle_button_get_active (togglebutton);
	gconf_client_set_bool (totem->gc, GCONF_PREFIX"/auto_resize",
			value, NULL);
}

static void              
on_checkbutton2_toggled (GtkToggleButton *togglebutton, gpointer user_data)
{                               
	Totem *totem = (Totem *)user_data;
	gboolean value;

	value = gtk_toggle_button_get_active (togglebutton);
	gconf_client_set_bool (totem->gc, GCONF_PREFIX"/show_vfx",
			value, NULL);
}

static void
on_combo_entry1_changed (BaconCdSelection *bcs, char *device,
		gpointer user_data)
{
	Totem *totem = (Totem *)user_data;
	const char *str;

	str = bacon_cd_selection_get_device (bcs);
	gconf_client_set_string (totem->gc, GCONF_PREFIX"/mediadev",
			str, NULL);
}

static void
auto_resize_changed_cb (GConfClient *client, guint cnxn_id,
		GConfEntry *entry, gpointer user_data)
{
	Totem *totem = (Totem *) user_data;
	GtkWidget *item;

	item = glade_xml_get_widget (totem->xml, "checkbutton1");
	g_signal_handlers_disconnect_by_func (G_OBJECT (item),
			on_checkbutton1_toggled, totem);

	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (item),
			gconf_client_get_bool (totem->gc,
				GCONF_PREFIX"/auto_resize", NULL));

	g_signal_connect (G_OBJECT (item), "toggled",
			G_CALLBACK (on_checkbutton1_toggled), totem);
}

static void
show_vfx_changed_cb (GConfClient *client, guint cnxn_id,
		GConfEntry *entry, gpointer user_data)
{
	Totem *totem = (Totem *) user_data;
	GtkWidget *item;

	item = glade_xml_get_widget (totem->xml, "checkbutton2");
	g_signal_handlers_disconnect_by_func (G_OBJECT (item),
			on_checkbutton2_toggled, totem);

	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (item),
			gconf_client_get_bool (totem->gc,
				GCONF_PREFIX"/show_vfx", NULL));

	g_signal_connect (G_OBJECT (item), "toggled",
			G_CALLBACK (on_checkbutton2_toggled), totem);
}

static void
mediadev_changed_cb (GConfClient *client, guint cnxn_id,
		GConfEntry *entry, gpointer user_data)
{
	Totem *totem = (Totem *) user_data;
	GtkWidget *item;

	item = glade_xml_get_widget (totem->xml, "custom3");
	g_signal_handlers_disconnect_by_func (G_OBJECT (item),
			on_combo_entry1_changed, totem);

	bacon_cd_selection_set_device (BACON_CD_SELECTION (item),
			gconf_client_get_string
			(totem->gc, GCONF_PREFIX"/mediadev", NULL));

	g_signal_connect (G_OBJECT (item), "device-changed",
			G_CALLBACK (on_combo_entry1_changed), totem);
}

void
totem_button_pressed_remote_cb (TotemRemote *remote, TotemRemoteCommand cmd,
				Totem *totem)
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
			totem_action_volume_relative (totem, 8);
		break;
		
		case TOTEM_REMOTE_COMMAND_VOLUME_DOWN:
			totem_action_volume_relative (totem, -8);
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
		default:
		break;
	}
}

static int
toggle_playlist_from_playlist (GtkWidget *playlist, int trash,
		gpointer user_data)
{
	Totem *totem = (Totem *) user_data;
	GtkWidget *button;

	button = glade_xml_get_widget (totem->xml, "playlist_button");
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), FALSE);

	return TRUE;
}

static void
playlist_changed_cb (GtkWidget *playlist, gpointer user_data)
{
	Totem *totem = (Totem *) user_data;
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
current_removed_cb (GtkWidget *playlist, gpointer user_data)
{
	Totem *totem = (Totem *) user_data;
	char *mrl;

	/* Set play button status */
	play_pause_set_label (totem, STATE_STOPPED);
	gtk_playlist_set_at_start (totem->playlist);
	update_buttons (totem);
	mrl = gtk_playlist_get_current_mrl (totem->playlist);
	totem_action_set_mrl_and_play (totem, mrl);
	g_free (mrl);
}

static gboolean
popup_hide (Totem *totem)
{
	gtk_widget_hide (GTK_WIDGET (totem->exit_popup));
	gtk_widget_hide (GTK_WIDGET (totem->control_popup));

	if (totem->popup_timeout != 0)
	{
		gtk_timeout_remove (totem->popup_timeout);
		totem->popup_timeout = 0;
	}

	gtk_xine_set_show_cursor (GTK_XINE (totem->gtx), FALSE);

	return FALSE;
}

static void
on_mouse_click_fullscreen (GtkWidget *widget, gpointer user_data)
{
	Totem *totem = (Totem *)user_data;

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
		gpointer user_data)
{
	static gboolean in_progress = FALSE;
	Totem *totem = (Totem *) user_data;

	if (gtk_xine_is_fullscreen (GTK_XINE (totem->gtx)) == FALSE)
		return FALSE;

	if (in_progress == TRUE)
		return FALSE;

	in_progress = TRUE;
	if (totem->popup_timeout != 0)
	{
		gtk_timeout_remove (totem->popup_timeout);
		totem->popup_timeout = 0;
	}

	gtk_window_move (GTK_WINDOW (totem->control_popup), 0,
			gdk_screen_height () - totem->control_popup_height);
	gtk_widget_show_all (totem->exit_popup);
	gtk_widget_show_all (totem->control_popup);
	gtk_xine_set_show_cursor (GTK_XINE (totem->gtx), TRUE);

	totem->popup_timeout = gtk_timeout_add (2000,
			(GtkFunction) popup_hide, totem);
	in_progress = FALSE;

	return FALSE;
}

static gboolean
on_eos_event (GtkWidget *widget, gpointer user_data)
{
	Totem *totem = (Totem *) user_data;

	if (strcmp (totem->mrl, LOGO_PATH) == 0)
		return FALSE;

	if (!gtk_playlist_has_next_mrl (totem->playlist))
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

static void
on_error_event (GtkWidget *gtx, GtkXineError error, const char *message,
		gpointer user_data)
{
	Totem *totem = (Totem *) user_data;
	char *msg = NULL;
	gboolean crap_out = FALSE;

	/* We show errors all the time, and don't skip to the next */

	switch (error)
	{
	case GTX_STARTUP:
		msg = g_strdup_printf (_("Totem could not startup:\n%s"),
				message);
		crap_out = TRUE;
		break;
	case GTX_NO_INPUT_PLUGIN:
	case GTX_NO_DEMUXER_PLUGIN:
		msg = g_strdup_printf (_("There is no plugin for Totem to "
					"handle '%s'.\nTotem will not be able "
					"to play it."), totem->mrl);
		totem_action_stop (totem);
		break;
	case GTX_DEMUXER_FAILED:
		msg = g_strdup_printf (_("'%s' is broken, and Totem can not "
					"play it further."), totem->mrl);
		totem_action_stop (totem);
		break;
	case GTX_NO_CODEC:
		msg = g_strdup_printf(_("Totem could not play '%s':\n%s"),
				totem->mrl, message);
		totem_action_stop (totem);
		break;
	case GTX_MALFORMED_MRL:
		msg = g_strdup_printf(_("Totem could not play '%s'.\n"
					"This location is not a valid one."),
					totem->mrl);
		break;
	default:
		g_assert_not_reached ();
	}

	totem_action_error (msg, GTK_WINDOW (totem->win));
	g_free (msg);
	gtk_playlist_set_playing (totem->playlist, FALSE);

	if (crap_out == TRUE)
		totem_action_exit (totem);
}

static gboolean
totem_action_handle_key (Totem *totem, guint keyval)
{
	gboolean retval = TRUE;

	switch (keyval) {
	case XF86XK_AudioPlay:
	case XF86XK_AudioPause:
	case GDK_p:
	case GDK_P:
		totem_action_play_pause (totem);
		break;
	case GDK_Escape:
		totem_action_fullscreen (totem, FALSE);
		break;
	case GDK_f:
	case GDK_F:
		totem_action_fullscreen_toggle (totem);
		break;
	case GDK_O:
	case GDK_o:
		totem_action_fullscreen (totem, FALSE);
		on_open1_activate (NULL, (gpointer) totem);
		break;
	case GDK_Left:
		totem_action_seek_relative (totem, SEEK_BACKWARD_OFFSET);
		break;
	case GDK_Right:
		totem_action_seek_relative (totem, SEEK_FORWARD_OFFSET);
		break;
	case GDK_Up:
		totem_action_volume_relative (totem, 8);
		break;
	case GDK_Down:
		totem_action_volume_relative (totem, -8);
		break;
	case GDK_A:
	case GDK_a:
		totem_action_toggle_aspect_ratio (totem);
		break;
	case GDK_C:
	case GDK_c:
		gtk_xine_dvd_event (GTK_XINE (totem->gtx),
				GTX_DVD_CHAPTER_MENU);
		break;
	case GDK_M:
	case GDK_m:
		gtk_xine_dvd_event (GTK_XINE (totem->gtx), GTX_DVD_ROOT_MENU);
	case XF86XK_AudioPrev:
	case GDK_B:
	case GDK_b:
		totem_action_previous (totem);
		break;
	case XF86XK_AudioNext:
	case GDK_N:
	case GDK_n:
		totem_action_next (totem);
		break;
	case GDK_q:
	case GDK_Q:
		totem_action_exit (totem);
		break;
	case GDK_s:
	case GDK_S:
		on_skip_to1_activate (NULL, totem);
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

	switch (direction) {
		case GDK_SCROLL_UP:
			totem_action_seek_relative (totem, SEEK_FORWARD_SHORT_OFFSET);
			break;
		case GDK_SCROLL_DOWN:
			totem_action_seek_relative (totem, SEEK_BACKWARD_SHORT_OFFSET);
			break;
		default:
			retval = FALSE;
	}
			
	return retval;
}

static int
on_window_key_press_event (GtkWidget *win, GdkEventKey *event,
		                gpointer user_data)
{
	Totem *totem = (Totem *) user_data;

	if (event->state != 0)
		return FALSE;

	return totem_action_handle_key (totem, event->keyval);
}

static int
on_window_scroll_event (GtkWidget *win, GdkEventScroll *event,
				gpointer user_data)
{
	Totem *totem = (Totem *) user_data;
	return totem_action_handle_scroll (totem, event->direction);
}

static void
update_dvd_menu_items (Totem *totem)
{
        GtkWidget *item;
        gboolean playing_dvd;

        playing_dvd = totem_playing_dvd (totem);

        item = glade_xml_get_widget (totem->xml, "dvd_root_menu");
	gtk_widget_set_sensitive (item, playing_dvd);
        item = glade_xml_get_widget (totem->xml, "dvd_title_menu");
	gtk_widget_set_sensitive (item, playing_dvd);
        item = glade_xml_get_widget (totem->xml, "dvd_audio_menu");
	gtk_widget_set_sensitive (item, playing_dvd);
        item = glade_xml_get_widget (totem->xml, "dvd_angle_menu");
	gtk_widget_set_sensitive (item, playing_dvd);
        item = glade_xml_get_widget (totem->xml, "dvd_chapter_menu");
	gtk_widget_set_sensitive (item, playing_dvd);

        return;
}

static void
update_buttons (Totem *totem)
{
	GtkWidget *item;
	gboolean has_item;

	/* Previous */
        /* FIXME Need way to detect if DVD Title is at first chapter */
        if (totem_playing_dvd (totem))
                has_item = TRUE;
        else
                has_item = gtk_playlist_has_previous_mrl (totem->playlist);

	item = glade_xml_get_widget (totem->xml, "previous_button");
	gtk_widget_set_sensitive (item, has_item);
	item = glade_xml_get_widget (totem->xml, "fs_previous_button");
	gtk_widget_set_sensitive (item, has_item);
	item = glade_xml_get_widget (totem->xml, "previous_chapter1");
	gtk_widget_set_sensitive (item, has_item);

	/* Next */
        /* FIXME Need way to detect if DVD Title has no more chapters */
        if (totem_playing_dvd (totem))
                has_item = TRUE;
        else
		has_item = gtk_playlist_has_next_mrl (totem->playlist);

	item = glade_xml_get_widget (totem->xml, "next_button");
	gtk_widget_set_sensitive (item, has_item);
	item = glade_xml_get_widget (totem->xml, "fs_next_button");
	gtk_widget_set_sensitive (item, has_item);
	item = glade_xml_get_widget (totem->xml, "next_chapter1");
	gtk_widget_set_sensitive (item, has_item);
}

static void
totem_callback_connect (Totem *totem)
{
	GtkWidget *item;

	/* Menu items */
	item = glade_xml_get_widget (totem->xml, "open1");
	g_signal_connect (G_OBJECT (item), "activate",
			G_CALLBACK (on_open1_activate), totem);
	item = glade_xml_get_widget (totem->xml, "play_dvd1");
	g_signal_connect (G_OBJECT (item), "activate",
			G_CALLBACK (on_play_dvd1_activate), totem);
	item = glade_xml_get_widget (totem->xml, "play_vcd1");
	g_signal_connect (G_OBJECT (item), "activate",
			G_CALLBACK (on_play_vcd1_activate), totem);
	item = glade_xml_get_widget (totem->xml, "play_audio_cd1");
	g_signal_connect (G_OBJECT (item), "activate",
			G_CALLBACK (on_play_cd1_activate), totem);
	item = glade_xml_get_widget (totem->xml, "play1");
	g_signal_connect (G_OBJECT (item), "activate",
			G_CALLBACK (on_play1_activate), totem);
	item = glade_xml_get_widget (totem->xml, "fullscreen1");
	g_signal_connect (G_OBJECT (item), "activate",
			G_CALLBACK (on_full_screen1_activate), totem);
	item = glade_xml_get_widget (totem->xml, "zoom_1_2");
	g_signal_connect (G_OBJECT (item), "activate",
			G_CALLBACK (on_zoom_1_2_activate), totem);
	item = glade_xml_get_widget (totem->xml, "zoom_1_1");
	g_signal_connect (G_OBJECT (item), "activate",
			G_CALLBACK (on_zoom_1_1_activate), totem);
	item = glade_xml_get_widget (totem->xml, "zoom_2_1");
	g_signal_connect (G_OBJECT (item), "activate",
			G_CALLBACK (on_zoom_2_1_activate), totem);
	item = glade_xml_get_widget (totem->xml, "toggle_aspect_ratio1");
	g_signal_connect (G_OBJECT (item), "activate",
			G_CALLBACK (on_toggle_aspect_ratio1_activate), totem);
	item = glade_xml_get_widget (totem->xml, "show_playlist1");
	g_signal_connect (G_OBJECT (item), "activate",
			G_CALLBACK (on_show_playlist1_activate), totem);
	item = glade_xml_get_widget (totem->xml, "quit1");
	g_signal_connect (G_OBJECT (item), "activate",
			G_CALLBACK (on_quit1_activate), totem);
	item = glade_xml_get_widget (totem->xml, "about1");
	g_signal_connect (G_OBJECT (item), "activate",
			G_CALLBACK (on_about1_activate), totem);
	item = glade_xml_get_widget (totem->xml, "take_screenshot1");
	g_signal_connect (G_OBJECT (item), "activate",
			G_CALLBACK (on_take_screenshot1_activate), totem);
	item = glade_xml_get_widget (totem->xml, "preferences1");
	g_signal_connect (G_OBJECT (item), "activate",
			G_CALLBACK (on_preferences1_activate), totem);
	item = glade_xml_get_widget (totem->xml, "properties1");
	g_signal_connect (G_OBJECT (item), "activate",
			G_CALLBACK (on_properties1_activate), totem);
	item = glade_xml_get_widget (totem->xml, "volume_up1");
	g_signal_connect (G_OBJECT (item), "activate",
			G_CALLBACK (on_volume_up1_activate), totem);
	item = glade_xml_get_widget (totem->xml, "volume_down1");
	g_signal_connect (G_OBJECT (item), "activate",
			G_CALLBACK (on_volume_down1_activate), totem);

	/* Screenshot dialog */
	item = glade_xml_get_widget (totem->xml, "dialog2");
	g_signal_connect (G_OBJECT (item), "delete-event",
			G_CALLBACK (hide_screenshot), (gpointer) totem);
	item = glade_xml_get_widget (totem->xml, "radiobutton1");
	g_signal_connect (G_OBJECT (item), "toggled",
			G_CALLBACK (on_radiobutton_shot_toggled),
			(gpointer) totem);
	item = glade_xml_get_widget (totem->xml, "radiobutton2");
	g_signal_connect (G_OBJECT (item), "toggled",
			G_CALLBACK (on_radiobutton_shot_toggled),
			(gpointer) totem);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (item), TRUE);

	/* Controls */
	totem->pp_button = glade_xml_get_widget
		(totem->xml, "play_pause_button");
	g_signal_connect (G_OBJECT (totem->pp_button), "clicked",
			G_CALLBACK (on_play_pause_button_clicked), totem);
	item = glade_xml_get_widget (totem->xml, "previous_button");
	g_signal_connect (G_OBJECT (item), "clicked",
			G_CALLBACK (on_previous_button_clicked), totem);
	item = glade_xml_get_widget (totem->xml, "next_button");
	g_signal_connect (G_OBJECT (item), "clicked", 
			G_CALLBACK (on_next_button_clicked), totem);
	item = glade_xml_get_widget (totem->xml, "playlist_button");
	g_signal_connect (G_OBJECT (item), "clicked",
			G_CALLBACK (on_playlist_button_toggled), totem);

	/* Drag'n'Drop */
	item = glade_xml_get_widget (totem->xml, "playlist_button");
	g_signal_connect (G_OBJECT (item), "drag_data_received",
			G_CALLBACK (drop_playlist_cb), totem);
	gtk_drag_dest_set (item, GTK_DEST_DEFAULT_ALL,
			target_table, 1, GDK_ACTION_COPY);

	/* Exit */
	g_signal_connect (G_OBJECT (totem->win), "delete-event",
			G_CALLBACK (main_window_destroy_cb), totem);
	g_signal_connect (G_OBJECT (totem->win), "destroy",
			G_CALLBACK (main_window_destroy_cb), totem);

	/* Motion notify for the Popups */
	item = glade_xml_get_widget (totem->xml, "window1");
	gtk_widget_add_events (item, GDK_POINTER_MOTION_MASK);
	g_signal_connect (G_OBJECT (item), "motion-notify-event",
			G_CALLBACK (on_video_motion_notify_event), totem);
	item = glade_xml_get_widget (totem->xml, "window2");
	gtk_widget_add_events (item, GDK_POINTER_MOTION_MASK);
	g_signal_connect (G_OBJECT (item), "motion-notify-event",
			G_CALLBACK (on_video_motion_notify_event), totem);

	/* Popup */
	item = glade_xml_get_widget (totem->xml, "fs_exit1");
	g_signal_connect (G_OBJECT (item), "clicked",
			G_CALLBACK (on_fs_exit1_activate), totem);
	g_signal_connect (G_OBJECT (item), "motion-notify-event",
			G_CALLBACK (on_video_motion_notify_event), totem);

	/* Control Popup */
	g_signal_connect (G_OBJECT (totem->fs_pp_button), "clicked",
			G_CALLBACK (on_play_pause_button_clicked), totem);
	g_signal_connect (G_OBJECT (totem->fs_pp_button), "clicked",
			G_CALLBACK (on_mouse_click_fullscreen), totem);

	item = glade_xml_get_widget (totem->xml, "fs_previous_button");
	g_signal_connect (G_OBJECT (item), "clicked",
			G_CALLBACK (on_previous_button_clicked), totem);
	g_signal_connect (G_OBJECT (item), "clicked",
			G_CALLBACK (on_mouse_click_fullscreen), totem);

	item = glade_xml_get_widget (totem->xml, "fs_next_button");
	g_signal_connect (G_OBJECT (item), "clicked", 
			G_CALLBACK (on_next_button_clicked), totem);
	g_signal_connect (G_OBJECT (item), "clicked",
			G_CALLBACK (on_mouse_click_fullscreen), totem);

	/* Control Popup Sliders */
	g_signal_connect (G_OBJECT(totem->fs_seek), "value-changed",
			G_CALLBACK (seek_cb), totem);

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

	/* Sliders */
	g_signal_connect (G_OBJECT (totem->seek), "value-changed",
			G_CALLBACK (seek_cb), totem);
	g_signal_connect (G_OBJECT (totem->volume), "value-changed",
			G_CALLBACK (vol_cb), totem);

	/* Playlist Disappearance, woop woop */
	g_signal_connect (G_OBJECT (totem->playlist),
			"response", G_CALLBACK (toggle_playlist_from_playlist),
			(gpointer) totem);
	g_signal_connect (G_OBJECT (totem->playlist), "delete-event",
			G_CALLBACK (toggle_playlist_from_playlist),
			(gpointer) totem);

	/* Playlist */
	g_signal_connect (G_OBJECT (totem->playlist),
			"changed", G_CALLBACK (playlist_changed_cb),
			(gpointer) totem);
	g_signal_connect (G_OBJECT (totem->playlist),
			"current-removed", G_CALLBACK (current_removed_cb),
			(gpointer) totem);


	/* DVD menu callbacks */
	item = glade_xml_get_widget (totem->xml, "dvd_root_menu");
	g_signal_connect (G_OBJECT (item), "activate",
			G_CALLBACK (on_dvd_root_menu1_activate), totem);
	item = glade_xml_get_widget (totem->xml, "dvd_title_menu");
	g_signal_connect (G_OBJECT (item), "activate",
			G_CALLBACK (on_dvd_title_menu1_activate), totem);
	item = glade_xml_get_widget (totem->xml, "dvd_audio_menu");
	g_signal_connect (G_OBJECT (item), "activate",
			G_CALLBACK (on_dvd_audio_menu1_activate), totem);
	item = glade_xml_get_widget (totem->xml, "dvd_audio_menu");
	g_signal_connect (G_OBJECT (item), "activate",
			G_CALLBACK (on_dvd_audio_menu1_activate), totem);
	item = glade_xml_get_widget (totem->xml, "dvd_angle_menu");
	g_signal_connect (G_OBJECT (item), "activate",
			G_CALLBACK (on_dvd_angle_menu1_activate), totem);
	item = glade_xml_get_widget (totem->xml, "dvd_chapter_menu");
	g_signal_connect (G_OBJECT (item), "activate",
			G_CALLBACK (on_dvd_chapter_menu1_activate), totem);
	item = glade_xml_get_widget (totem->xml, "skip_to1");
	g_signal_connect (G_OBJECT (item), "activate",
			G_CALLBACK (on_skip_to1_activate), totem);
	item = glade_xml_get_widget (totem->xml, "next_chapter1");
	g_signal_connect (G_OBJECT (item), "activate",
			G_CALLBACK (on_next_button_clicked), totem);
	item = glade_xml_get_widget (totem->xml, "previous_chapter1");
	g_signal_connect (G_OBJECT (item), "activate",
			G_CALLBACK (on_previous_button_clicked), totem);
	item = glade_xml_get_widget (totem->xml, "skip_forward1");
	g_signal_connect (G_OBJECT (item), "activate",
			G_CALLBACK (on_skip_forward1_activate), totem);
	item = glade_xml_get_widget (totem->xml, "skip_backwards1");
	g_signal_connect (G_OBJECT (item), "activate",
			G_CALLBACK (on_skip_backwards1_activate), totem);

	/* Skip dialog */
	item = glade_xml_get_widget (totem->xml, "dialog3");
	g_signal_connect (G_OBJECT (item), "response",
			G_CALLBACK (commit_hide_skip_to), totem);
	g_signal_connect (G_OBJECT (item), "delete-event",
			G_CALLBACK (hide_skip_to), totem);
	item = glade_xml_get_widget (totem->xml, "spinbutton1");
	g_signal_connect (G_OBJECT (item), "value-changed",
			G_CALLBACK (spin_button_value_changed_cb), totem);

	/* Update the UI */
	gtk_timeout_add (600, update_cb_often, totem);
	gtk_timeout_add (1200, update_cb_rare, totem);
}

static void
video_widget_create (Totem *totem) 
{
	GtkWidget *container;

	totem->gtx = gtk_xine_new (-1, -1, FALSE);
	container = glade_xml_get_widget (totem->xml, "frame2");
	gtk_container_add (GTK_CONTAINER (container), totem->gtx);

	g_signal_connect (G_OBJECT (totem->gtx),
			"motion-notify-event",
			G_CALLBACK (on_video_motion_notify_event),
			totem);
	g_signal_connect (G_OBJECT (totem->gtx),
			"eos",
			G_CALLBACK (on_eos_event),
			totem);
	g_signal_connect (G_OBJECT (totem->gtx),
			"error",
			G_CALLBACK (on_error_event),
			totem);
	g_signal_connect (G_OBJECT(totem->gtx),
			"title-change",
			G_CALLBACK (on_title_change_event),
			totem);

	g_signal_connect (G_OBJECT (totem->gtx), "drag_data_received",
			G_CALLBACK (drop_video_cb), totem);
	gtk_drag_dest_set (totem->gtx, GTK_DEST_DEFAULT_ALL,
			target_table, 1, GDK_ACTION_COPY);

	g_signal_connect (G_OBJECT (totem->gtx), "drag_data_get",
			G_CALLBACK (drag_video_cb), totem);
	gtk_drag_source_set (totem->gtx, GDK_BUTTON1_MASK | GDK_BUTTON3_MASK,
			source_table, G_N_ELEMENTS (source_table),
			GDK_ACTION_LINK);

	g_object_add_weak_pointer (G_OBJECT (totem->gtx),
			(void**)&(totem->gtx));

	gtk_widget_realize (totem->gtx);
	gtk_widget_show (totem->gtx);
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
bacon_cd_selection_create (void)
{
	GtkWidget *widget;

	widget = bacon_cd_selection_new ();
	gtk_widget_show (widget);

	return widget;
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

	menu_item = glade_xml_get_widget (totem->xml, "movie1");
	menu = gtk_menu_item_get_submenu (GTK_MENU_ITEM (menu_item));
	menu_item = glade_xml_get_widget (totem->xml, "recent_separator");

	g_return_if_fail (menu != NULL);
	g_return_if_fail (menu_item != NULL);

	totem->recent_model = egg_recent_model_new (EGG_RECENT_MODEL_SORT_MRU);

	/* it would be better if we just filtered by mime-type, but there
	 * doesn't seem to be an easy way to figure out which mime-types we
	 * can handle */
	egg_recent_model_set_filter_groups (totem->recent_model, "Totem", NULL);

	totem->recent_view = egg_recent_view_gtk_new (menu, menu_item);
	egg_recent_view_gtk_show_icons (EGG_RECENT_VIEW_GTK
						(totem->recent_view), TRUE);
	egg_recent_view_set_model (EGG_RECENT_VIEW (totem->recent_view),
			totem->recent_model);
	egg_recent_view_gtk_set_trailing_sep (totem->recent_view, TRUE);

	g_signal_connect (totem->recent_view, "activate",
			G_CALLBACK (on_recent_file_activate), totem);
}

static void
totem_setup_preferences (Totem *totem)
{
	GtkWidget *item;
	const char *device;

	g_return_if_fail (totem->gc != NULL);

	gconf_client_add_dir (totem->gc, "/apps/totem",
			GCONF_CLIENT_PRELOAD_ONELEVEL, NULL);
	gconf_client_notify_add (totem->gc, GCONF_PREFIX"/auto_resize",
			auto_resize_changed_cb, totem, NULL, NULL);
	gconf_client_notify_add (totem->gc, GCONF_PREFIX"/show_vfx",
			show_vfx_changed_cb, totem, NULL, NULL);
	gconf_client_notify_add (totem->gc, GCONF_PREFIX"/mediadev",
			mediadev_changed_cb, totem, NULL, NULL);

	totem->prefs = glade_xml_get_widget (totem->xml, "dialog1");

	g_signal_connect (G_OBJECT (totem->prefs),
			"response", G_CALLBACK (hide_prefs), (gpointer) totem);
	g_signal_connect (G_OBJECT (totem->prefs), "delete-event",
			G_CALLBACK (hide_prefs), (gpointer) totem);

	item = glade_xml_get_widget (totem->xml, "checkbutton1");
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (item),
			gconf_client_get_bool (totem->gc,
				GCONF_PREFIX"/auto_resize", NULL));
	g_signal_connect (G_OBJECT (item), "toggled",
			G_CALLBACK (on_checkbutton1_toggled), totem);

	item = glade_xml_get_widget (totem->xml, "checkbutton2");
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (item),
			gconf_client_get_bool (totem->gc,
				GCONF_PREFIX"/show_vfx", NULL));
	g_signal_connect (G_OBJECT (item), "toggled",
			G_CALLBACK (on_checkbutton2_toggled), totem);

	item = glade_xml_get_widget (totem->xml, "custom3");
	device = gconf_client_get_string
		(totem->gc, GCONF_PREFIX"/mediadev", NULL);
	if (device == NULL || (strcmp (device, "") == 0)
			|| (strcmp (device, "auto") == 0))
	{
		device = bacon_cd_selection_get_default_device
			(BACON_CD_SELECTION (item));
		gconf_client_set_string (totem->gc, GCONF_PREFIX"/mediadev",
				device, NULL);
	}

	bacon_cd_selection_set_device (BACON_CD_SELECTION (item),
			gconf_client_get_string
			(totem->gc, GCONF_PREFIX"/mediadev", NULL));
	g_signal_connect (G_OBJECT (item), "device-changed",
			G_CALLBACK (on_combo_entry1_changed), totem);
}

GConfClient *
totem_get_gconf_client (Totem *totem)
{
	return totem->gc;
}

static void
process_queue (GtkMessageQueue *queue, char **argv)
{
	if (gtk_message_queue_is_server (queue) == FALSE)
	{
		g_message ("send to existing GUI");
	} else {
		g_message ("setup the server thingo");
	}
}

int
main (int argc, char **argv)
{
	Totem *totem;
	char *filename;
	int width = 0;
	GConfClient *gc;
	GtkMessageQueue *q;
	GError *err = NULL;
	GdkPixbuf *pix;

	bindtextdomain (GETTEXT_PACKAGE, GNOMELOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);

	gnome_program_init ("totem", VERSION,
			LIBGNOMEUI_MODULE,
			argc, argv,
			GNOME_PARAM_APP_DATADIR, DATADIR,
			NULL);

	glade_gnome_init ();
	gnome_vfs_init ();
	gconf_init (argc, argv, &err);
	if (err != NULL || (gc = gconf_client_get_default ()) == NULL)
	{
		char *str;

		str = g_strdup_printf (_("Totem couln't initialise the \n"
					"configuration engine:\n%s"),
				err->message);
		totem_action_error (str, NULL);
		g_free (str);
		exit (1);
	}
	gnome_authentication_manager_init ();

	/* We need it right now for the queue */
	filename = gnome_program_locate_file (NULL,
			GNOME_FILE_DOMAIN_APP_DATADIR,
			"totem/totem.glade", TRUE, NULL);
	if (filename == NULL)
	{
		totem_action_error (_("Couldn't load the main interface"
					" (totem.glade).\nMake sure that Totem"
					" is properly installed."), NULL);
		exit (1);
	}

	q = NULL;
#if 0
	if (gconf_client_get_bool
			(gc, GCONF_PREFIX"/launch_once", NULL) == TRUE)
	{
		q = gtk_message_queue_new ("totem", filename);
		process_queue (q, argv);
	}
#endif
	totem = g_new (Totem, 1);
	totem->mrl = NULL;
	totem->seek_lock = FALSE;
	totem->vol_lock = FALSE;
	totem->prev_volume = -1;
	totem->popup_timeout = 0;
	totem->gtx = NULL;
	totem->gc = gc;
	totem->queue = q;

	/* Main window */
	totem->xml = glade_xml_new (filename, NULL, NULL);
	if (totem->xml == NULL)
	{
		g_free (filename);
		totem_action_error (_("Couldn't load the main interface"
					" (totem.glade).\nMake sure that Totem"
					" is properly installed."), NULL);
		exit (1);
	}
	g_free (filename);

	totem->win = glade_xml_get_widget (totem->xml, "app1");

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
		totem_action_error (_("Couldn't load the interface for the playlist."
					"\nMake sure that Totem"
					" is properly installed."),
				GTK_WINDOW (totem->win));
		exit (1);
	}
	filename = g_build_filename (G_DIR_SEPARATOR_S, DATADIR,
			"totem", "playlist-24.png", NULL);
	gtk_window_set_icon_from_file (GTK_WINDOW (totem->playlist),
			filename, NULL);
	g_free (filename);

	/* The rest of the widgets */
	totem->seek = glade_xml_get_widget (totem->xml, "hscale1");
	totem->seekadj = gtk_range_get_adjustment (GTK_RANGE (totem->seek));
	totem->volume = glade_xml_get_widget (totem->xml, "hscale2");
	totem->voladj = gtk_range_get_adjustment (GTK_RANGE (totem->volume));
	totem->exit_popup = glade_xml_get_widget (totem->xml, "window1");
	totem->control_popup = glade_xml_get_widget (totem->xml, "window2");
	totem->fs_seek = glade_xml_get_widget (totem->xml, "hscale4");
	totem->fs_seekadj = gtk_range_get_adjustment
		(GTK_RANGE (totem->fs_seek));
	totem->fs_volume     = glade_xml_get_widget (totem->xml, "hscale5");
	totem->fs_voladj = gtk_range_get_adjustment
		(GTK_RANGE (totem->fs_volume));
	totem->volume_first_time = 1;
	totem->fs_pp_button = glade_xml_get_widget (totem->xml, "fs_pp_button");
	totem->properties = gtk_xine_properties_new ();
	totem->statusbar = glade_xml_get_widget (totem->xml, "custom4");

	/* Calculate the height of the control popup window */
	gtk_window_get_size (GTK_WINDOW (totem->control_popup),
			&width, &totem->control_popup_height);

	totem_setup_recent (totem);
	totem_setup_preferences (totem);
	totem_callback_connect (totem);

	/* Show ! gtk_main_iteration trickery to show all the widgets
	 * we have so far */
	gtk_widget_show_all (totem->win);
	long_action ();

	/* Show ! (again) the video widget this time. */
	video_widget_create (totem);

	if (argc > 1)
	{
		/* Use gtk_xine_check to wait until xine has finished
		 * initialising completely, otherwise this can turn up nasty */
		while (gtk_xine_check (GTK_XINE (totem->gtx)) == FALSE)
			usleep (100000);
		if (totem_action_open_files (totem, argv, TRUE))
			totem_action_play_pause (totem);
		else
			totem_action_set_mrl (totem, NULL);
	} else {
		totem_action_set_mrl (totem, NULL);
	}

#ifdef HAVE_REMOTE
	totem->remote = totem_remote_new ();
	g_signal_connect (totem->remote, "button_pressed",
			  G_CALLBACK (totem_button_pressed_remote_cb), totem);
#endif

	gtk_main ();

	return 0;
}

