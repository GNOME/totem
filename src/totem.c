
#include <config.h>
#include <gnome.h>
#include <glade/glade.h>
#include <libgnomevfs/gnome-vfs.h>
#include <libgnomevfs/gnome-vfs-mime-handlers.h>
#include <eel/eel-ellipsizing-label.h>

#include "gtk-xine.h"
#include "gtk-playlist.h"

#include "debug.h"

#ifndef TOTEM_DEBUG
#include <fcntl.h>
#include <unistd.h>
#endif

typedef struct {
	/* Control window */
	GladeXML *xml;
	GtkWidget *win;
	GtkWidget *treeview;
	GtkWidget *gtx;
	GtkWidget *pp_button;
	guint pp_handler;

	/* Seek */
	GtkWidget *seek;
	GtkAdjustment *seekadj;
	gboolean seek_lock;

	/* Volume */
	GtkWidget *volume;
	GtkAdjustment *voladj;
	gboolean vol_lock;

	/* fullscreen Popup */
	GtkWidget *popup;
	guint popup_timeout;

	/* other */
	char *mrl;
	GtkPlaylist *playlist;
} Totem;

static const GtkTargetEntry target_table[] = {
	{ "text/uri-list", 0, 0 },
};

static void action_set_mrl (Totem *totem, const char *mrl);
static gboolean popup_hide (Totem *totem);
static void update_buttons (Totem *totem);
static void on_play_pause_button_toggled (GtkToggleButton *button,
		gpointer user_data);


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
		return g_strdup_printf ("%2d:%02d", min, sec);
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
action_error (char * msg)
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
	gtk_widget_show (error_dialog);
	gtk_dialog_run (GTK_DIALOG (error_dialog));
	gtk_widget_destroy (error_dialog);
}


#ifndef TOTEM_DEBUG
/* Nicked from aaxine */
static void
disable_error_output (void)
{
	int error_fd;

	if ((error_fd = open ("/dev/null", O_WRONLY)) < 0)
	{
		g_message("cannot open /dev/null");
	} else {
		if (dup2 (error_fd, STDOUT_FILENO) < 0)
			g_message("cannot dup2 stdout");
		if (dup2 (error_fd, STDERR_FILENO) < 0)
			g_message("cannot dup2 stderr");
	}
}
#endif

static void
action_exit (Totem *totem)
{
	gtk_main_quit ();
	exit (0);
}

static void
action_play_pause (Totem *totem)
{
	GtkToggleButton *button = GTK_TOGGLE_BUTTON (totem->pp_button);
	gboolean state;

	state = gtk_toggle_button_get_active (button);
	gtk_toggle_button_set_active (button, !state);
}

static void
play_pause_toggle_disconnected (Totem *totem)
{
	/* Avoid loops by disconnecting the callback first */
	g_signal_handler_disconnect (GTK_OBJECT (totem->pp_button),
		 totem->pp_handler);
	action_play_pause (totem);
	totem->pp_handler = g_signal_connect
		(GTK_OBJECT (totem->pp_button), "toggled",
		 G_CALLBACK (on_play_pause_button_toggled), totem);
}

static void
action_play_pause_real (Totem *totem, int pos)
{
	if (totem->mrl == NULL)
	{
		char *mrl;

		/* Try to pull an mrl from the playlist */
		mrl = gtk_playlist_get_current_mrl (totem->playlist);
		if (mrl == NULL)
		{
			play_pause_toggle_disconnected (totem);
			return;
		} else {
			action_set_mrl (totem, mrl);
			g_free (mrl);
			return;
		}
	}

	if (!gtk_xine_is_playing(GTK_XINE(totem->gtx))) {
		if (gtk_xine_play (GTK_XINE(totem->gtx), totem->mrl, 0, pos)
				== FALSE)
		{
			play_pause_toggle_disconnected (totem);
		}
	} else {
		if (gtk_xine_get_speed (GTK_XINE(totem->gtx)) == SPEED_PAUSE)
		{
			gtk_xine_set_speed (GTK_XINE(totem->gtx), SPEED_NORMAL);
		} else {
			gtk_xine_set_speed (GTK_XINE(totem->gtx), SPEED_PAUSE);
		}
	}
}

static void
action_fullscreen_toggle (Totem *totem)
{
	GtkWidget *widget;
	gboolean new_state;

	new_state = !gtk_xine_is_fullscreen (GTK_XINE (totem->gtx));

	gtk_xine_set_fullscreen (GTK_XINE (totem->gtx), new_state);
}

static void
action_fullscreen (Totem *totem, gboolean state)
{
	if (gtk_xine_is_fullscreen (GTK_XINE (totem->gtx)) == state)
		return;

	action_fullscreen_toggle (totem);
}

static void
action_set_mrl (Totem *totem, const char *mrl)
{
	GtkWidget *widget;

	g_free (totem->mrl);

	if (mrl == NULL) 
	{
		totem->mrl = NULL;

		/* Play/Pause */
		gtk_widget_set_sensitive (totem->pp_button, FALSE);
		widget = glade_xml_get_widget (totem->xml, "play1");
		gtk_widget_set_sensitive (widget, FALSE);

		widget = glade_xml_get_widget (totem->xml, "label1");
		gtk_window_set_title (GTK_WINDOW (totem->win), "Totem");
		eel_ellipsizing_label_set_text (EEL_ELLIPSIZING_LABEL (widget),
				"No file");

		/* Seek bar */
		gtk_widget_set_sensitive (totem->seek, FALSE);

		/* Stop the playback */
		gtk_xine_stop (GTK_XINE (totem->gtx));
	} else {
		char *title, *text, *time_text;
		int time;

		totem->mrl = g_strdup (mrl);

		/* Otherwise we might never change the mrl in GtkXine */
		if (gtk_xine_is_playing(GTK_XINE(totem->gtx))) {
			gtk_xine_play (GTK_XINE(totem->gtx), totem->mrl, 0, 0);
		} else {
			/* Make sure it will actually work first */
			action_play_pause (totem);
		}
		/* Force play button status */
		if (gtk_toggle_button_get_active
				(GTK_TOGGLE_BUTTON(totem->pp_button)) == FALSE)
			play_pause_toggle_disconnected (totem);

		/* Play/Pause */
		gtk_widget_set_sensitive (totem->pp_button, TRUE);
		widget = glade_xml_get_widget (totem->xml, "play1");
		gtk_widget_set_sensitive (widget, TRUE);

		/* Title */
		title = g_strdup_printf ("%s - Totem", g_basename (mrl));
		gtk_window_set_title (GTK_WINDOW (totem->win), title);

		/* Seek bar */
		gtk_widget_set_sensitive (totem->seek,
				gtk_xine_is_seekable(GTK_XINE (totem->gtx)));

		/* Set the playlist */
		gtk_playlist_set_playing (totem->playlist);

		/* Label */
		widget = glade_xml_get_widget (totem->xml, "label1");
		time = gtk_xine_get_stream_length (GTK_XINE (totem->gtx));
		time_text = time_to_string (time);
		text = g_strdup_printf ("%s (%s)",
				g_basename (mrl), time_text);
		eel_ellipsizing_label_set_text (EEL_ELLIPSIZING_LABEL (widget),
				text);
		g_free (text);
		g_free (time_text);
	}
}

static void
action_previous (Totem *totem)
{
	char *mrl;

	gtk_playlist_set_previous (totem->playlist);
	update_buttons (totem);
	mrl = gtk_playlist_get_current_mrl (totem->playlist);
	action_set_mrl (totem, mrl);
	g_free (mrl);
}

static void
action_next (Totem *totem)
{
	char *mrl;

	gtk_playlist_set_next (totem->playlist);
	update_buttons (totem);
	mrl = gtk_playlist_get_current_mrl (totem->playlist);
	action_set_mrl (totem, mrl);
	g_free (mrl);
}

static void
action_seek_relative (Totem *totem, int off_sec)
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

	if (gtk_xine_is_playing(GTK_XINE(totem->gtx)) == FALSE)
		action_play_pause (totem);

	gtk_xine_play (GTK_XINE(totem->gtx), totem->mrl, 0, sec);
}

static void
action_volume_relative (Totem *totem, int off_pct)
{
	int vol;

	if (!gtk_xine_can_set_volume (GTK_XINE (totem->gtx)))
		return;

	vol = gtk_xine_get_volume (GTK_XINE (totem->gtx));
	gtk_xine_set_volume (GTK_XINE (totem->gtx), vol + off_pct);
}

static void
action_toggle_aspect_ratio (Totem *totem)
{
	gtk_xine_toggle_aspect_ratio (GTK_XINE (totem->gtx));
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
	int i;

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

	for (p = file_list; p != NULL; p = p->next) {
		char *filename;

		filename = g_filename_from_uri (p->data, NULL, NULL);
		D("dropped filename: %s", filename);
		if (filename != NULL && 
				g_file_test (filename, G_FILE_TEST_IS_REGULAR
					| G_FILE_TEST_EXISTS))
		{
			if (cleared == FALSE)
			{
				gtk_playlist_clear (totem->playlist);
				cleared = TRUE;
			}
			gtk_playlist_add_mrl (totem->playlist, filename); 
		}
		g_free (filename);
		g_free (p->data);
	}

	g_list_free (file_list);
	gtk_drag_finish (context, TRUE, FALSE, time);

	if (cleared == TRUE)
	{
		char *mrl;

		mrl = gtk_playlist_get_current_mrl (totem->playlist);
		action_set_mrl (totem, mrl);
		g_free (mrl);
		update_buttons (totem);
	}
}

static void
on_play_pause_button_toggled (GtkToggleButton *button, gpointer user_data)
{
	Totem *totem = (Totem *) user_data;

	action_play_pause_real (totem, 0);
}

static void
on_previous_button_clicked (GtkButton *button, gpointer user_data)
{
	Totem *totem = (Totem *) user_data;

	action_previous (totem);
}

static void
on_next_button_clicked (GtkButton *button, gpointer user_data)
{
	Totem *totem = (Totem *) user_data;

	action_next (totem);
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

static int
update_sliders_cb (gpointer user_data)
{
	Totem *totem = (Totem *) user_data;
	gfloat pos;

	if (totem->seek_lock == FALSE)
	{
		totem->seek_lock = TRUE;
		pos = (gfloat) gtk_xine_get_position (GTK_XINE (totem->gtx));
		gtk_adjustment_set_value (totem->seekadj, pos);
		totem->seek_lock = FALSE;
	}

	if (totem->vol_lock == FALSE)
	{
		totem->vol_lock = TRUE;
		pos = (gfloat) gtk_xine_get_volume (GTK_XINE (totem->gtx));
		gtk_adjustment_set_value (totem->voladj, pos);
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
		gtk_xine_play (GTK_XINE (totem->gtx), totem->mrl,
				(gint) totem->seekadj->value, 0);
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
		gtk_xine_set_volume (GTK_XINE (totem->gtx),
				(gint) totem->voladj->value);
		totem->vol_lock = FALSE;
	}
}

static void
action_open_files (Totem *totem, char **list, gboolean ignore_first)
{
	int i;
	gboolean cleared = FALSE;

	i = (ignore_first ? 1 : 0 );

	for ( ; list[i] != NULL; i++)
	{
		if (g_file_test (list[i], G_FILE_TEST_IS_REGULAR
					| G_FILE_TEST_EXISTS))
		{
			if (cleared == FALSE)
			{
				gtk_playlist_clear (totem->playlist);
				cleared = TRUE;
			}
			gtk_playlist_add_mrl (totem->playlist, list[i]);
		}
	}

	if (cleared == TRUE)
	{
		char *mrl;

		mrl = gtk_playlist_get_current_mrl (totem->playlist);
		action_set_mrl (totem, mrl);
		g_free (mrl);
		update_buttons (totem);
	}
}


static void
on_open1_activate (GtkButton * button, gpointer user_data)
{
	Totem *totem = (Totem *) user_data;
	GtkWidget *fs;
	int response;

	fs = gtk_file_selection_new (_("Select files"));
	gtk_file_selection_set_select_multiple (GTK_FILE_SELECTION (fs), TRUE);
	response = gtk_dialog_run (GTK_DIALOG (fs));
	gtk_widget_hide (fs);
	long_action ();

	if (response == GTK_RESPONSE_OK)
	{
		char **filenames;

		filenames = gtk_file_selection_get_selections
			(GTK_FILE_SELECTION (fs));
		action_open_files (totem, filenames, FALSE);
		g_strfreev (filenames);
	}

	gtk_widget_destroy (fs);
}

static void
on_play1_activate (GtkButton * button, gpointer user_data)
{
	Totem *totem = (Totem *) user_data;

	action_play_pause (totem);
}


static void
on_full_screen1_activate (GtkButton * button, gpointer user_data)
{
	Totem *totem = (Totem *) user_data;

	action_fullscreen_toggle (totem);
}

static void
on_toggle_aspect_ratio1_activate (GtkButton * button, gpointer user_data)
{
	Totem *totem = (Totem *) user_data;

	action_toggle_aspect_ratio (totem);
}

static void
on_show_playlist1_activate (GtkButton * button, gpointer user_data)
{
	Totem *totem = (Totem *) user_data;
	GtkWidget *toggle;
	gboolean state;

	toggle = glade_xml_get_widget (totem->xml, "playlist_button");

	state = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (toggle));
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (toggle), !state);
}

static void
on_fs_exit1_activate (GtkButton * button, gpointer user_data)
{
	Totem *totem = (Totem *) user_data;

	popup_hide (totem);
	action_fullscreen_toggle (totem);
}

static void
on_quit1_activate (GtkButton * button, gpointer user_data)
{
	action_exit ((Totem *) user_data);
}

static void
on_about1_activate (GtkButton * button, gpointer user_data)
{
	static GtkWidget *about = NULL;
	Totem *totem = (Totem *) user_data;
	GdkPixbuf *pixbuf = NULL;
	const gchar *authors[] = {"Bastien Nocera", NULL};
	const gchar *documenters[] = { NULL };
	const gchar *translator_credits = _("translator_credits");

	if (about != NULL) {
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

	g_signal_connect (GTK_OBJECT (about), "destroy", G_CALLBACK
			(gtk_widget_destroyed), &about);
	gtk_window_set_transient_for (GTK_WINDOW (about),
			GTK_WINDOW (totem->win));

	gtk_widget_show(about);
}

static void
toggle_playlist_from_playlist (GtkWidget *playlist, int trash,
		gpointer user_data)
{
	Totem *totem = (Totem *) user_data;
	GtkWidget *button;

	button = glade_xml_get_widget (totem->xml, "playlist_button");
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), FALSE);
}

static void
playlist_changed_cb (GtkWidget *playlist, gpointer user_data)
{
	Totem *totem = (Totem *) user_data;

	update_buttons (totem);
}

static void
current_removed_cb (GtkWidget *playlist, gpointer user_data)
{
	Totem *totem = (Totem *) user_data;
	char *mrl;

	/* Force play button status */
	if (gtk_toggle_button_get_active
			(GTK_TOGGLE_BUTTON(totem->pp_button)) == TRUE)
		play_pause_toggle_disconnected (totem);
	gtk_playlist_set_at_start (totem->playlist);
	update_buttons (totem);
	mrl = gtk_playlist_get_current_mrl (totem->playlist);
	action_set_mrl (totem, mrl);
	long_action ();
	action_play_pause (totem);
	g_free (mrl);
}

static gboolean
popup_hide (Totem *totem)
{
	gtk_widget_hide (totem->popup);
	totem->popup_timeout = 0;

	gtk_xine_set_show_cursor (GTK_XINE (totem->gtx), FALSE);

	return FALSE;
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

	gtk_widget_show (totem->popup);
	gtk_xine_set_show_cursor (GTK_XINE (totem->gtx), TRUE);
	long_action ();

	totem->popup_timeout = gtk_timeout_add (2000,
			(GtkFunction) popup_hide, totem);
	in_progress = FALSE;

	return TRUE;
}

static gboolean
on_eos_event (GtkWidget *widget, gpointer user_data)
{
	Totem *totem = (Totem *) user_data;

	gdk_threads_enter ();

	if (!gtk_playlist_has_next_mrl (totem->playlist))
	{
		char *mrl;

		long_action ();
		/* Force play button status */
		if (gtk_toggle_button_get_active
				(GTK_TOGGLE_BUTTON(totem->pp_button)) == TRUE)
			play_pause_toggle_disconnected (totem);
		gtk_playlist_set_at_start (totem->playlist);
		update_buttons (totem);
		mrl = gtk_playlist_get_current_mrl (totem->playlist);
		action_set_mrl (totem, mrl);
		long_action ();
		action_play_pause (totem);
		g_free (mrl);
	} else {
		action_next (totem);
	}

	gdk_threads_leave ();

	return FALSE;
}

static int
on_window_key_press_event (GtkWidget *win, GdkEventKey *event,
		gpointer user_data)
{
	Totem *totem = (Totem *) user_data;
	gboolean retval = FALSE;

	switch (event->keyval) {
	case GDK_p:
	case GDK_P:
		action_play_pause (totem);
		retval = TRUE;
		break;
	case GDK_Escape:
		action_fullscreen (totem, FALSE);
		retval = TRUE;
		break;
	case GDK_f:
	case GDK_F:
		action_fullscreen_toggle (totem);
		retval = TRUE;
		break;
	case GDK_Left:
		action_seek_relative (totem, -15);
		retval = TRUE;
		break;
	case GDK_Right:
		action_seek_relative (totem, 60);
		retval = TRUE;
		break;
	case GDK_Up:
		action_volume_relative (totem, 8);
		retval = TRUE;
		break;
	case GDK_Down:
		action_volume_relative (totem, -8);
		retval = TRUE;
		break;
	case GDK_A:
	case GDK_a:
		action_toggle_aspect_ratio (totem);
		retval = TRUE;
		break;
	case GDK_B:
	case GDK_b:
		action_previous (totem);
		retval = TRUE;
		break;
	case GDK_N:
	case GDK_n:
		action_next (totem);
		retval = TRUE;
		break;
	case GDK_q:
	case GDK_Q:
		action_exit (totem);
		retval = TRUE;
		break;
	}

	return retval;
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
	item = glade_xml_get_widget (totem->xml, "previous_stream1");
	gtk_widget_set_sensitive (item, has_item);

	/* Next */
	has_item = gtk_playlist_has_next_mrl (totem->playlist);
	item = glade_xml_get_widget (totem->xml, "next_button");
	gtk_widget_set_sensitive (item, has_item);
	item = glade_xml_get_widget (totem->xml, "next_stream1");
	gtk_widget_set_sensitive (item, has_item);
}

static void
video_widget_create (Totem *totem)
{
	GtkWidget *container;
	
	totem->gtx = gtk_xine_new();
	container = glade_xml_get_widget (totem->xml, "frame2");
	gtk_container_add (GTK_CONTAINER (container), totem->gtx);

	g_signal_connect (GTK_OBJECT (totem->gtx),
			"mouse-motion",
			G_CALLBACK (on_mouse_motion_event),
			totem);
	g_signal_connect (GTK_OBJECT (totem->gtx),
			"eos",
			G_CALLBACK (on_eos_event),
			totem);

	gtk_widget_realize (totem->gtx);
	gtk_widget_show (totem->gtx); 
}

static void
totem_callback_connect (Totem *totem)
{
	GtkWidget *item;

	/* Menu items */
	item = glade_xml_get_widget (totem->xml, "open1");
	g_signal_connect (GTK_OBJECT (item), "activate",
			G_CALLBACK (on_open1_activate), totem);
	item = glade_xml_get_widget (totem->xml, "play1");
	g_signal_connect (GTK_OBJECT (item), "activate",
			G_CALLBACK (on_play1_activate), totem);
	item = glade_xml_get_widget (totem->xml, "fullscreen1");
	g_signal_connect (GTK_OBJECT (item), "activate",
			G_CALLBACK (on_full_screen1_activate), totem);
	item = glade_xml_get_widget (totem->xml, "toggle_aspect_ratio1");
	g_signal_connect (GTK_OBJECT (item), "activate",
			G_CALLBACK (on_toggle_aspect_ratio1_activate),
			totem);
	item = glade_xml_get_widget (totem->xml, "show_playlist1");
	g_signal_connect (GTK_OBJECT (item), "activate",
			G_CALLBACK (on_show_playlist1_activate), totem);
	item = glade_xml_get_widget (totem->xml, "quit1");
	g_signal_connect (GTK_OBJECT (item), "activate",
			G_CALLBACK (on_quit1_activate), totem);
	item = glade_xml_get_widget (totem->xml, "about1");
	g_signal_connect (GTK_OBJECT (item), "activate",
			G_CALLBACK (on_about1_activate), totem);

	/* Controls */
	totem->pp_button = glade_xml_get_widget
		(totem->xml, "play_pause_button");
	totem->pp_handler = g_signal_connect (GTK_OBJECT (totem->pp_button),
			"toggled", G_CALLBACK (on_play_pause_button_toggled),
			totem);
	item = glade_xml_get_widget (totem->xml, "previous_button");
	g_signal_connect (GTK_OBJECT (item), "clicked",
			G_CALLBACK (on_previous_button_clicked), totem);
	item = glade_xml_get_widget (totem->xml, "next_button");
	g_signal_connect (GTK_OBJECT (item), "clicked", 
			G_CALLBACK (on_next_button_clicked), totem);
	item = glade_xml_get_widget (totem->xml, "playlist_button");
	g_signal_connect (GTK_OBJECT (item), "clicked",
			G_CALLBACK (on_playlist_button_toggled), totem);

	/* Drag'n'Drop */
	item = glade_xml_get_widget (totem->xml, "frame1");
	g_signal_connect (GTK_OBJECT (item), "drag_data_received",
			G_CALLBACK (drop_cb), totem);
	gtk_drag_dest_set (item, GTK_DEST_DEFAULT_ALL,
			target_table, 1, GDK_ACTION_COPY);
#if 0
	g_signal_connect (GTK_OBJECT (totem->treeview), "drag_data_received",
			G_CALLBACK (drop_cb), totem);
	gtk_drag_dest_set (totem->treeview, GTK_DEST_DEFAULT_ALL,
			target_table, 1, GDK_ACTION_COPY);
#endif
	/* Exit */
	g_signal_connect (GTK_OBJECT (totem->win), "delete_event",
			G_CALLBACK (action_exit), totem);
	g_signal_connect (GTK_OBJECT (totem->win), "destroy_event",
			G_CALLBACK (action_exit), totem);

	/* Popup */
	item = glade_xml_get_widget (totem->xml, "fs_exit1");
	g_signal_connect (GTK_OBJECT (item), "clicked",
			G_CALLBACK (on_fs_exit1_activate), totem);

	/* Connect the keys */
	gtk_widget_add_events (totem->win, GDK_KEY_RELEASE_MASK);
	g_signal_connect (GTK_OBJECT(totem->win), "key_press_event",
			G_CALLBACK(on_window_key_press_event), totem);

	/* Sliders */
	g_signal_connect (GTK_OBJECT (totem->seek), "value-changed",
			G_CALLBACK (seek_cb), totem);
	g_signal_connect (GTK_OBJECT (totem->volume), "value-changed",
			G_CALLBACK (vol_cb), totem);
	gtk_timeout_add (500, update_sliders_cb, totem);

	/* Playlist Disappearance, woop woop */
	g_signal_connect (G_OBJECT (totem->playlist),
			"response", G_CALLBACK (toggle_playlist_from_playlist),
			(gpointer) totem);
	g_signal_connect (GTK_OBJECT (totem->playlist),
			"delete-event",
			G_CALLBACK (toggle_playlist_from_playlist),
			(gpointer) totem);
	g_object_add_weak_pointer (G_OBJECT (totem->playlist),
			(void**)&(totem->playlist));

	/* Playlist */
	g_signal_connect (G_OBJECT (totem->playlist),
			"changed", G_CALLBACK (playlist_changed_cb),
			(gpointer) totem);
	g_signal_connect (G_OBJECT (totem->playlist),
			"current-removed", G_CALLBACK (current_removed_cb),
			(gpointer) totem);
}

GtkWidget
*label_create (void)
{
	GtkWidget *label;

	label = eel_ellipsizing_label_new ("No File");
	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
	return label;
}

int
main (int argc, char **argv)
{
	Totem *totem;
	char *filename;

	bindtextdomain (GETTEXT_PACKAGE, GNOMELOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);

	g_thread_init (NULL);
	gdk_threads_init ();
	gnome_program_init ("totem", VERSION,
			LIBGNOMEUI_MODULE,
			argc, argv,
			GNOME_PARAM_APP_DATADIR, DATADIR,
			NULL);

	glade_gnome_init ();
	gnome_vfs_init ();

#ifndef TOTEM_DEBUG
	disable_error_output ();
#endif

	totem = g_new (Totem, 1);
	totem->mrl = NULL;
	totem->seek_lock = FALSE;
	totem->vol_lock = FALSE;
	totem->pp_handler = 0;
	totem->popup_timeout = 0;

	filename = gnome_program_locate_file (NULL,
			GNOME_FILE_DOMAIN_APP_DATADIR,
			"totem/totem.glade", TRUE, NULL);
	if (filename == NULL)
	{
		action_error (_("Couldn't load the main Glade file"
					" (totem.glade).\nMake sure that Totem"
					"is properly installed."));
		exit (1);
	}
	totem->xml = glade_xml_new (filename, NULL, NULL);
	if (totem->xml == NULL)
	{
		action_error (_("Couldn't load the main Glade file"
					" (totem.glade).\nMake sure that Totem"
					"is properly installed."));
		exit (1);
	}
	g_free (filename);

	video_widget_create (totem);
	totem->win = glade_xml_get_widget (totem->xml, "app1");

	totem->playlist = GTK_PLAYLIST
		(gtk_playlist_new (GTK_WINDOW (totem->win)));
	if (totem->playlist == NULL)
	{
		action_error (_("Couldn't load the interface for the playlist."
					"\nMake sure that Totem"
					"is properly installed."));
		exit (1);
	}

	totem->seek = glade_xml_get_widget (totem->xml, "hscale1");
	totem->seekadj = gtk_range_get_adjustment (GTK_RANGE (totem->seek));
	totem->volume = glade_xml_get_widget (totem->xml, "hscale2");
	totem->voladj = gtk_range_get_adjustment (GTK_RANGE (totem->volume));
	totem->popup = glade_xml_get_widget (totem->xml, "window1");
	update_sliders_cb ((gpointer) totem);
	totem_callback_connect (totem);

	/* Show ! */
	gtk_widget_show_all (totem->win);
	long_action ();

	if (argc > 1)
		action_open_files (totem, argv, TRUE);

	gtk_main ();

	return 0;
}

