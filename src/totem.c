
#include <config.h>
#include <gnome.h>
#include <glade/glade.h>
#include <libgnomevfs/gnome-vfs.h>
#include <gconf/gconf-client.h>
#include <string.h>

#include "gtk-message.h"
#include "gtk-xine.h"
#include "gtk-playlist.h"
#include "rb-ellipsizing-label.h"

#include "egg-recent-model.h"
#include "egg-recent-view.h"
#include "egg-recent-view-gtk.h"

#include "totem.h"
#include "totem-remote.h"

#include "debug.h"

#ifndef TOTEM_DEBUG
#include <fcntl.h>
#include <unistd.h>
#endif

#define TOTEM_GCONF_PREFIX "/apps/totem"
#define LOGO_PATH DATADIR""G_DIR_SEPARATOR_S"totem"G_DIR_SEPARATOR_S"totem_logo.mpv"
#define SEEK_FORWARD_OFFSET 60000
#define SEEK_BACKWARD_OFFSET -15000

struct Totem {
	/* Control window */
	GladeXML *xml;
	GtkWidget *win;
	GtkWidget *treeview;
	GtkWidget *gtx;
	GtkWidget *prefs;

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
};

static const GtkTargetEntry target_table[] = {
	{ "text/uri-list", 0, 0 },
};

static gboolean popup_hide (Totem *totem);
static void update_buttons (Totem *totem);
static void on_play_pause_button_clicked (GtkToggleButton *button,
		gpointer user_data);
static void playlist_changed_cb (GtkWidget *playlist, gpointer user_data);

static char
*time_to_string (int time)
{
	int sec, min, hour;

	sec = time % 60;
	time = time - sec;
	min = (time % (60*60)) / 60;
	time = time - (min * 60);
	hour = time / (60*60);

	if (hour > 0)
	{
		/* hour:minutes:seconds */
		return g_strdup_printf ("%d:%02d:%02d", hour, min, sec);
	} else if (min > 0) {
		/* minutes:seconds */
		return g_strdup_printf ("%d:%02d", min, sec);
	} else {
		/* seconds */
		return g_strdup_printf ("%d sec", sec);
	}

	return NULL;
}

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
	gtk_main_quit ();

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
play_pause_set_label (Totem *totem, gboolean playing)
{
	GtkWidget *image;
	char *image_path;


	if (playing == TRUE)
	{
		image_path = gnome_program_locate_file (NULL,
				GNOME_FILE_DOMAIN_APP_DATADIR,
				"totem/stock_media_pause.png", FALSE, NULL);
	} else {
		image_path = gnome_program_locate_file (NULL,
				GNOME_FILE_DOMAIN_APP_DATADIR,
				"totem/stock_media_play.png", FALSE, NULL);
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

	D("action_play");

	if (totem->mrl == NULL)
		return;

	retval = gtk_xine_play (GTK_XINE (totem->gtx), offset , 0);
	play_pause_set_label (totem, retval);
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
	totem_action_set_mrl (totem, mrl);
	g_free (mrl);
	totem_action_play (totem, 0);
}

void
totem_action_stop (Totem *totem)
{
	D("action_pause");

	gtk_xine_stop (GTK_XINE (totem->gtx));
}

void
totem_action_play_pause (Totem *totem)
{
	D("action_play_pause");

	if (totem->mrl == NULL)
	{
		char *mrl;

		/* Try to pull an mrl from the playlist */
		mrl = gtk_playlist_get_current_mrl (totem->playlist);
		if (mrl == NULL)
		{
			play_pause_set_label (totem, FALSE);
			return;
		} else {
			totem_action_set_mrl (totem, mrl);
			g_free (mrl);
		}
	}

	if (!gtk_xine_is_playing(GTK_XINE(totem->gtx)))
	{
		totem_action_play (totem, 0);
	} else {
		if (gtk_xine_get_speed (GTK_XINE(totem->gtx)) == SPEED_PAUSE)
		{
			gtk_xine_set_speed (GTK_XINE(totem->gtx), SPEED_NORMAL);
			play_pause_set_label (totem, TRUE);
		} else {
			gtk_xine_set_speed (GTK_XINE(totem->gtx), SPEED_PAUSE);
			play_pause_set_label (totem, FALSE);
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

void
totem_action_set_mrl (Totem *totem, const char *mrl)
{
	GtkWidget *widget;
	char *text;

	gtk_xine_stop (GTK_XINE (totem->gtx));

	if (totem->mrl != NULL)
	{
		g_free (totem->mrl);
		gtk_xine_close (GTK_XINE (totem->gtx));
	}

	if (mrl == NULL)
	{
		/* Play/Pause */
		gtk_widget_set_sensitive (totem->pp_button, FALSE);
		widget = glade_xml_get_widget (totem->xml, "play1");
		gtk_widget_set_sensitive (widget, FALSE);

		/* Label */
		widget = glade_xml_get_widget (totem->xml, "label1");
		gtk_window_set_title (GTK_WINDOW (totem->win), "Totem");
		text = g_strdup_printf
			(_("<span size=\"medium\"><b>No file</b></span>"));
		rb_ellipsizing_label_set_markup (RB_ELLIPSIZING_LABEL (widget),
				text);
		g_free (text);

		/* Title */
		gtk_window_set_title (GTK_WINDOW (totem->win), "Totem");

		/* Seek bar and seek buttons */
		gtk_widget_set_sensitive (totem->seek, FALSE);
		widget = glade_xml_get_widget (totem->xml, "skip_forward1");
		gtk_widget_set_sensitive (totem->seek, FALSE);
		widget = glade_xml_get_widget (totem->xml, "skip_backwards1");
		gtk_widget_set_sensitive (totem->seek, FALSE);

		/* Volume */
		widget = glade_xml_get_widget (totem->xml, "volume_hbox");
		gtk_widget_set_sensitive (widget, FALSE);

		/* Control popup */
		gtk_widget_set_sensitive (totem->fs_seek, FALSE);
		gtk_widget_set_sensitive (totem->fs_volume, FALSE);

		gtk_widget_set_sensitive (totem->fs_pp_button, FALSE);
		widget = glade_xml_get_widget (totem->xml,
				"fs_previous_button"); 
		gtk_widget_set_sensitive (widget, FALSE);
		widget = glade_xml_get_widget (totem->xml, "fs_next_button"); 
		gtk_widget_set_sensitive (widget, FALSE);

		/* Set the logo */
		totem->mrl = g_strdup (LOGO_PATH);
		gtk_xine_open (GTK_XINE (totem->gtx), totem->mrl);
	} else {
		char *title, *time_text, *name;
		int time;

		gtk_xine_open (GTK_XINE (totem->gtx), mrl);

		totem->mrl = g_strdup (mrl);
		name = gtk_playlist_mrl_to_title (mrl);

		/* Play/Pause */
		gtk_widget_set_sensitive (totem->pp_button, TRUE);
		widget = glade_xml_get_widget (totem->xml, "play1");
		gtk_widget_set_sensitive (widget, TRUE);

		/* Title */
		title = g_strdup_printf ("%s - Totem", name);
		gtk_window_set_title (GTK_WINDOW (totem->win), title);
		g_free (title);

		/* Seek bar */
		gtk_widget_set_sensitive (totem->seek,
				gtk_xine_is_seekable(GTK_XINE (totem->gtx)));
		widget = glade_xml_get_widget (totem->xml, "skip_forward1");
		gtk_widget_set_sensitive (widget, TRUE);
		widget = glade_xml_get_widget (totem->xml, "skip_backwards1");
		gtk_widget_set_sensitive (widget, TRUE);

		/* Control popup */
		gtk_widget_set_sensitive (totem->fs_seek, 
                gtk_xine_is_seekable(GTK_XINE (totem->gtx)));
		gtk_widget_set_sensitive (totem->fs_pp_button, TRUE);
		widget = glade_xml_get_widget (totem->xml,
				"fs_previous_button"); 
		gtk_widget_set_sensitive (widget, TRUE);
		widget = glade_xml_get_widget (totem->xml, "fs_next_button"); 
		gtk_widget_set_sensitive (widget, TRUE);

		/* Volume */
		widget = glade_xml_get_widget (totem->xml, "volume_hbox");
		gtk_widget_set_sensitive (widget, gtk_xine_can_set_volume
				(GTK_XINE (totem->gtx)));

		/* Set the playlist */
		gtk_playlist_set_playing (totem->playlist, TRUE);

		/* Label */
		widget = glade_xml_get_widget (totem->xml, "label1");
		time = gtk_xine_get_stream_length (GTK_XINE (totem->gtx));
		time_text = time_to_string (time/1000);
		text = g_strdup_printf
			("<span size=\"medium\"><b>%s (%s)</b></span>",
			 name, time_text);
		rb_ellipsizing_label_set_markup (RB_ELLIPSIZING_LABEL (widget),
				text);
		g_free (text);
		g_free (time_text);
		g_free (name);
	}
}

void
totem_action_previous (Totem *totem)
{
	char *mrl;

	if (gtk_playlist_has_previous_mrl (totem->playlist) == FALSE)
		return;

	gtk_playlist_set_previous (totem->playlist);
	update_buttons (totem);
	mrl = gtk_playlist_get_current_mrl (totem->playlist);
	totem_action_set_mrl (totem, mrl);
	totem_action_play (totem, 0);
	g_free (mrl);
}

void
totem_action_next (Totem *totem)
{
	char *mrl;

	if (gtk_playlist_has_next_mrl (totem->playlist) == FALSE)
		                return;

	gtk_playlist_set_next (totem->playlist);
	update_buttons (totem);
	mrl = gtk_playlist_get_current_mrl (totem->playlist);
	totem_action_set_mrl (totem, mrl);
	totem_action_play (totem, 0);
	g_free (mrl);
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
	play_pause_set_label (totem, TRUE);
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

static void
drop_cb (GtkWidget     *widget,
	 GdkDragContext     *context,
	 gint                x,
	 gint                y,
	 GtkSelectionData   *data,
	 guint               info,
	 guint               time,
	 gpointer            user_data)
{
	Totem *totem = (Totem *)user_data;
	GList *list, *p, *file_list;
	gboolean cleared = FALSE;

	list = gnome_vfs_uri_list_parse (data->data);

	if (list == NULL) {
		gtk_drag_finish (context, FALSE, FALSE, time);
		return;
	}

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
	{
		gtk_drag_finish (context, FALSE, FALSE, time);
		return;
	}

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
		totem_action_set_mrl (totem, mrl);
		update_buttons (totem);
		g_free (mrl);
		totem_action_play (totem, 0);
	}

	gtk_drag_finish (context, TRUE, FALSE, time);
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

static int
update_sliders_cb (gpointer user_data)
{
	Totem *totem = (Totem *) user_data;
	gfloat pos;

	if (totem->gtx == NULL)
		return TRUE;

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
		gtk_adjustment_set_value (totem->voladj, pos);
		gtk_adjustment_set_value (totem->fs_voladj, pos);
		volume_set_image (totem, (gint) pos);
		totem->vol_lock = FALSE;
	}

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
			totem_action_play (totem, (gint) totem->fs_seekadj->value);
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
            gtk_adjustment_set_value(totem->voladj, 
                    gtk_adjustment_get_value(totem->fs_voladj));
        }
        else
        {
            gtk_xine_set_volume (GTK_XINE (totem->gtx),
                    (gint) totem->voladj->value);
            /* Update the fullscreen volume adjustment */
            gtk_adjustment_set_value(totem->fs_voladj, 
                    gtk_adjustment_get_value(totem->voladj));

        }

        volume_set_image (totem, (gint) totem->voladj->value);
		totem->vol_lock = FALSE;
	}
}

void
totem_action_open_files (Totem *totem, char **list, gboolean ignore_first)
{
	int i;
	gboolean cleared = FALSE;

	i = (ignore_first ? 1 : 0 );

	for ( ; list[i] != NULL; i++)
	{
		if (g_file_test (list[i], G_FILE_TEST_IS_REGULAR
					| G_FILE_TEST_EXISTS)
				|| strstr (list[i], "://") != NULL
				|| strcmp (list[i], "dvd:") == 0
				|| strcmp (list[i], "vcd:") == 0)
		{
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
		}
	}

	/* ... and reconnect because we're nice people */
	if (cleared == TRUE)
	{
		update_buttons (totem);
		g_signal_connect (G_OBJECT (totem->playlist),
				"changed", G_CALLBACK (playlist_changed_cb),
				(gpointer) totem);
	}
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
		totem_action_set_mrl (totem, mrl);
		g_free (mrl);
		totem_action_play (totem, 0);
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
			_("(C) 2002 Bastien Nocera"),
			_("Movie Player (based on the Xine libraries)"),
			(const char **)authors,
			(const char **)documenters,
			strcmp (translator_credits, "translator_credits") != 0 ? translator_credits : NULL,
			pixbuf);

	g_signal_connect (G_OBJECT (about), "destroy", G_CALLBACK
			(gtk_widget_destroyed), &about);
	g_object_add_weak_pointer (G_OBJECT (about),
			(void**)&(about));
	gtk_window_set_transient_for (GTK_WINDOW (about),
			GTK_WINDOW (totem->win));

	gtk_widget_show(about);
}

static void
on_preferences1_activate (GtkButton *button, gpointer user_data)
{
	Totem *totem = (Totem *)user_data;

	gtk_widget_show (totem->prefs);
}

static void
hide_prefs (GtkWidget *playlist, int trash, gpointer user_data)
{
	Totem *totem = (Totem *)user_data;

	gtk_widget_hide (totem->prefs);
}

static void
on_checkbutton1_toggled (GtkToggleButton *togglebutton, gpointer user_data)
{
	Totem *totem = (Totem *)user_data;
	gboolean value;

	D("on_checkbutton1_toggled");
	value = gtk_toggle_button_get_active (togglebutton);
	gconf_client_set_bool (totem->gc, "/apps/totem/auto_resize",
			value, NULL);
}

static void
on_combo_entry1_changed (GtkEntry *entry, gpointer user_data)
{
	Totem *totem = (Totem *)user_data;
	const char *str;

	D("on_combo_entry1_activate");
	str = gtk_entry_get_text (entry);
	gconf_client_set_string (totem->gc, "/apps/totem/mediadev",
			str, NULL);
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

	D("playlist_changed_cb");

	update_buttons (totem);
	mrl = gtk_playlist_get_current_mrl (totem->playlist);

	if (totem->mrl == NULL
			|| (totem->mrl != NULL && mrl != NULL
			&& strcmp (totem->mrl, mrl) != 0))
	{
		totem_action_set_mrl (totem, mrl);
		totem_action_play (totem, 0);
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

	D("current_removed_cb");

	/* Set play button status */
	play_pause_set_label (totem, FALSE);
	gtk_playlist_set_at_start (totem->playlist);
	update_buttons (totem);
	mrl = gtk_playlist_get_current_mrl (totem->playlist);
	totem_action_set_mrl (totem, mrl);
	if (mrl != NULL)
	{
		totem_action_play (totem, 0);
		g_free (mrl);
	} else {
		totem_action_stop (totem);
	}
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
on_mouse_motion_event (GtkWidget *widget, gpointer user_data)
{
	static gboolean in_progress = FALSE;
	Totem *totem = (Totem *) user_data;

	if (in_progress == TRUE)
		return TRUE;

	in_progress = TRUE;
	if (totem->popup_timeout != 0)
	{
		gtk_timeout_remove (totem->popup_timeout);
		totem->popup_timeout = 0;
	}

	gtk_window_move (GTK_WINDOW (totem->control_popup), 0,
			gdk_screen_height () - totem->control_popup_height);
	gtk_widget_show (totem->exit_popup);
	gtk_widget_show (totem->control_popup);
	gtk_xine_set_show_cursor (GTK_XINE (totem->gtx), TRUE);

	totem->popup_timeout = gtk_timeout_add (2000,
			(GtkFunction) popup_hide, totem);
	in_progress = FALSE;

	return TRUE;
}

static gboolean
on_motion_notify_event (GtkWidget *widget, GdkEventMotion *event,
		gpointer user_data)
{
	return on_mouse_motion_event (widget, user_data);
}

static gboolean
on_eos_event (GtkWidget *widget, gpointer user_data)
{
	Totem *totem = (Totem *) user_data;

	D("on_eos_event");

	if (!gtk_playlist_has_next_mrl (totem->playlist))
	{
		char *mrl;

		/* Set play button status */
		play_pause_set_label (totem, FALSE);
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

	D("play_error");

	if (gtk_playlist_has_next_mrl (totem->playlist))
	{
		totem_action_next (totem);
		return;
	}

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
					"handle '%s'\nTotem will not be able "
					"to play it."), totem->mrl);
		break;
	case GTX_DEMUXER_FAILED:
		msg = g_strdup_printf (_("'%s' is broken, and Totem can not "
					"play it further."), totem->mrl);
		break;
	case GTX_NO_CODEC:
		msg = g_strdup_printf(_("Totem could not play '%s':\n%s"),
				totem->mrl, message);
		totem_action_stop (totem);
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
	case GDK_B:
	case GDK_b:
		totem_action_previous (totem);
		break;
	case GDK_N:
	case GDK_n:
		totem_action_next (totem);
		break;
	case GDK_q:
	case GDK_Q:
		totem_action_exit (totem);
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

static int
on_video_key_press_event (GtkWidget *win, guint keyval, gpointer user_data)
{
	Totem *totem = (Totem *) user_data;

	return totem_action_handle_key (totem, keyval);
}

static int
on_window_key_press_event (GtkWidget *win, GdkEventKey *event,
		                gpointer user_data)
{
	Totem *totem = (Totem *) user_data;

	return totem_action_handle_key (totem, event->keyval);
}

static void
update_buttons (Totem *totem)
{
	GtkWidget *item;
	gboolean has_item;

	/* Previous */
	has_item = gtk_playlist_has_previous_mrl (totem->playlist);
	item = glade_xml_get_widget (totem->xml, "previous_button");
	gtk_widget_set_sensitive (item, has_item);
	item = glade_xml_get_widget (totem->xml, "fs_previous_button");
	gtk_widget_set_sensitive (item, has_item);
	item = glade_xml_get_widget (totem->xml, "previous_stream1");
	gtk_widget_set_sensitive (item, has_item);

	/* Next */
	has_item = gtk_playlist_has_next_mrl (totem->playlist);
	item = glade_xml_get_widget (totem->xml, "next_button");
	gtk_widget_set_sensitive (item, has_item);
	item = glade_xml_get_widget (totem->xml, "fs_next_button");
	gtk_widget_set_sensitive (item, has_item);
	item = glade_xml_get_widget (totem->xml, "next_stream1");
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
	item = glade_xml_get_widget (totem->xml, "preferences1");
	g_signal_connect (G_OBJECT (item), "activate",
			G_CALLBACK (on_preferences1_activate), totem);

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
	item = glade_xml_get_widget (totem->xml, "frame1");
	g_signal_connect (G_OBJECT (item), "drag_data_received",
			G_CALLBACK (drop_cb), totem);
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
			G_CALLBACK (on_motion_notify_event), totem);
	item = glade_xml_get_widget (totem->xml, "window2");
	gtk_widget_add_events (item, GDK_POINTER_MOTION_MASK);
	g_signal_connect (G_OBJECT (item), "motion-notify-event",
			G_CALLBACK (on_motion_notify_event), totem);

	/* Popup */
	item = glade_xml_get_widget (totem->xml, "fs_exit1");
	g_signal_connect (G_OBJECT (item), "clicked",
			G_CALLBACK (on_fs_exit1_activate), totem);
	g_signal_connect (G_OBJECT (item), "motion-notify-event",
			G_CALLBACK (on_motion_notify_event), totem);

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

	/* Sliders */
	g_signal_connect (G_OBJECT (totem->seek), "value-changed",
			G_CALLBACK (seek_cb), totem);
	g_signal_connect (G_OBJECT (totem->volume), "value-changed",
			G_CALLBACK (vol_cb), totem);
	gtk_timeout_add (500, update_sliders_cb, totem);

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
}

static void
video_widget_create (Totem *totem) 
{
	GtkWidget *container, *widget;

	g_message ("video_widget_create");

	totem->gtx = gtk_xine_new(-1, -1);
	container = glade_xml_get_widget (totem->xml, "frame2");
	gtk_container_add (GTK_CONTAINER (container), totem->gtx);

	g_signal_connect (G_OBJECT (totem->gtx),
			"mouse-motion",
			G_CALLBACK (on_mouse_motion_event),
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
			"key-press",
			G_CALLBACK (on_video_key_press_event),
			totem);

	g_object_add_weak_pointer (G_OBJECT (totem->gtx),
			(void**)&(totem->gtx));

	gtk_widget_realize (totem->gtx);
	gtk_widget_show (totem->gtx);
}

GtkWidget
*label_create (void)
{
	GtkWidget *label;
	char *text;

	label = rb_ellipsizing_label_new ("");
	gtk_label_set_use_markup (GTK_LABEL (label), TRUE);
	gtk_label_set_selectable (GTK_LABEL (label), FALSE);
	gtk_misc_set_alignment (GTK_MISC (label), 0.5, 0);

	/* Set default */
	text = g_strdup_printf
		(_("<span size=\"medium\"><b>No file</b></span>"));
	rb_ellipsizing_label_set_markup (RB_ELLIPSIZING_LABEL (label), text);
	g_free (text);

	return label;
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

	totem->prefs = glade_xml_get_widget (totem->xml, "dialog1");

	g_signal_connect (G_OBJECT (totem->prefs),
			"response", G_CALLBACK (hide_prefs), (gpointer) totem);
	g_signal_connect (G_OBJECT (totem->prefs), "delete-event",
			G_CALLBACK (hide_prefs), (gpointer) totem);

	item = glade_xml_get_widget (totem->xml, "checkbutton1");
	g_signal_connect (G_OBJECT (item), "toggled",
			G_CALLBACK (on_checkbutton1_toggled), totem);

	item = glade_xml_get_widget (totem->xml, "combo-entry1");
	g_signal_connect (G_OBJECT (item), "changed",
			G_CALLBACK (on_combo_entry1_changed), totem);
}

GConfClient *
totem_get_gconf_client (Totem *totem)
{
	return totem->gc;
}

int
main (int argc, char **argv)
{
	Totem *totem;
	char *filename;
	int width = 0;
	GConfClient *gc;
	GError *err = NULL;

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
	if (err != NULL)
	{
		char *str;

		str = g_strdup_printf (_("Totem couln't initialise the \n"
					"configuration engine:\n%s"),
				err->message);
		totem_action_error (str, NULL);
		g_free (str);
		exit (1);
	}

	gc = gconf_client_get_default ();

	if (gtk_program_register ("totem") == FALSE
			&& gconf_client_get_bool
			(gc, "/apps/totem/launch_once", NULL) == TRUE)
	{
		//FIXME
		g_message ("Send message to the existing GUI");
		return 0;
	}

	totem = g_new (Totem, 1);
	totem->mrl = NULL;
	totem->seek_lock = FALSE;
	totem->vol_lock = FALSE;
	totem->popup_timeout = 0;
	totem->gtx = NULL;
	totem->gc = gc;

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
	totem->xml = glade_xml_new (filename, NULL, NULL);
	if (totem->xml == NULL)
	{
		totem_action_error (_("Couldn't load the main interface"
					" (totem.glade).\nMake sure that Totem"
					" is properly installed."), NULL);
		exit (1);
	}
	g_free (filename);

	totem->win = glade_xml_get_widget (totem->xml, "app1");

	totem->playlist = GTK_PLAYLIST
		(gtk_playlist_new (GTK_WINDOW (totem->win)));
	if (totem->playlist == NULL)
	{
		totem_action_error (_("Couldn't load the interface for the playlist."
					"\nMake sure that Totem"
					" is properly installed."),
				GTK_WINDOW (totem->win));
		exit (1);
	}

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
	totem->fs_pp_button = glade_xml_get_widget(totem->xml, "fs_pp_button");

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
		totem_action_open_files (totem, argv, TRUE);
		totem_action_play_pause (totem);
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

