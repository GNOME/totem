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
#include <X11/Xlib.h>
#include <gnome.h>
#include <glade/glade.h>
#include <libgnomevfs/gnome-vfs.h>
#include <gconf/gconf-client.h>
#include <string.h>

#include "bacon-video-widget.h"

#include "debug.h"

typedef struct Vanity Vanity;

struct Vanity {
	/* Main window */
	GladeXML *xml;
	GtkWidget *win;
	BaconVideoWidget *bvw;

	/* Prefs */
	GtkWidget *prefs;
	GConfClient *gc;

	gboolean debug;
};

static const GtkTargetEntry source_table[] = {
	{ "text/uri-list", 0, 0 },
};

static void vanity_action_exit (Vanity *vanity);

static const struct poptOption options[] = {
	{"debug", '\0', POPT_ARG_NONE, NULL, 0, N_("Debug mode on"), NULL},
	{NULL, '\0', 0, NULL, 0} /* end the list */
};

static void
long_action (void)
{
	while (gtk_events_pending ())
		gtk_main_iteration ();
}
#if 0
static void
vanity_action_error (char *msg, Vanity *vanity)
{
	GtkWidget *parent, *error_dialog;

	if (vanity == NULL)
		parent = NULL;
	else
		parent = vanity->win;

	error_dialog =
		gtk_message_dialog_new (GTK_WINDOW (vanity->win),
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
	g_object_add_weak_pointer (G_OBJECT (error_dialog),
			(void**)&(error_dialog));
	gtk_window_set_modal (GTK_WINDOW (error_dialog), TRUE);

	gtk_widget_show (error_dialog);
}
#endif
static void
vanity_action_error_and_exit (char *msg, Vanity *vanity)
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

	vanity_action_exit (vanity);
}

static void
vanity_action_exit (Vanity *vanity)
{
	if (gtk_main_level () > 0)
		gtk_main_quit ();

	if (vanity == NULL)
		exit (0);

	if (vanity->win)
		gtk_widget_hide (vanity->win);

	if (vanity->bvw)
		gtk_widget_destroy (GTK_WIDGET (vanity->bvw));

	exit (0);
}

static gboolean
main_window_destroy_cb (GtkWidget *widget, GdkEvent *event, Vanity *vanity)
{
	vanity_action_exit (vanity);

	return FALSE;
}

static void
vanity_action_set_scale_ratio (Vanity *vanity, gfloat ratio)
{
	bacon_video_widget_set_scale_ratio (vanity->bvw, ratio);
}

static void
drag_video_cb (GtkWidget *widget,
		GdkDragContext *context,
		GtkSelectionData *selection_data,
		guint info,
		guint32 time,
		gpointer callback_data)
{
#if 0
	Vanity *vanity = (Vanity *) callback_data;
	//FIXME the trick would be to create a file like gnome-panel-screenshot does

	char *text;
	int len;

	g_assert (selection_data != NULL);

	if (vanity->mrl == NULL)
		return;

	if (vanity->mrl[0] == '/')
		text = gnome_vfs_get_uri_from_local_path (vanity->mrl);
	else
		text = g_strdup (vanity->mrl);

	g_return_if_fail (text != NULL);

	len = strlen (text);

	gtk_selection_data_set (selection_data,
			selection_data->target,
			8, (guchar *) text, len);

	g_free (text);
#endif
}

static void
on_zoom_1_2_activate (GtkButton *button, Vanity *vanity)
{
	vanity_action_set_scale_ratio (vanity, 0.5); 
}

static void
on_zoom_1_1_activate (GtkButton *button, Vanity *vanity)
{
	vanity_action_set_scale_ratio (vanity, 1);
}

static void
on_zoom_2_1_activate (GtkButton *button, Vanity *vanity)
{                       
	vanity_action_set_scale_ratio (vanity, 2);
}


static void
on_quit1_activate (GtkButton *button, Vanity *vanity)
{
	vanity_action_exit (vanity);
}

static void
on_about1_activate (GtkButton *button, Vanity *vanity)
{
	static GtkWidget *about = NULL;
	GdkPixbuf *pixbuf = NULL;
	const gchar *authors[] =
	{
		"Bastien Nocera <hadess@hadess.net>",
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
				"totem/vanity.png",
				TRUE, NULL);

		if (filename != NULL)
		{
			pixbuf = gdk_pixbuf_new_from_file(filename, NULL);
			g_free (filename);
		}
	}

	backend_version = bacon_video_widget_get_backend_name (vanity->bvw);
	description = g_strdup_printf (_("Webcam utility using %s"),
				backend_version);

	about = gnome_about_new(_("Vanity"), VERSION,
			"Copyright \xc2\xa9 2001,2002,2003 Bastien Nocera",
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
			GTK_WINDOW (vanity->win));

	gtk_widget_show(about);
}
#if 0
static char *
screenshot_make_filename_helper (char *filename, gboolean desktop_exists)
{
	if (desktop_exists != FALSE)
	{
		return g_build_filename (g_get_home_dir (), ".gnome-desktop",
				filename, NULL);
	} else {
		return g_build_filename (g_get_home_dir (), filename, NULL);
	}
}

static char *
screenshot_make_filename (Vanity *vanity)
{
	GtkWidget *radiobutton, *entry;
	gboolean on_desktop;
	char *fullpath, *filename;
	int i = 0;
	gboolean desktop_exists;

	radiobutton = glade_xml_get_widget (vanity->xml, "radiobutton2");
	on_desktop = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON
			(radiobutton));

	/* Test if we have a desktop directory */
	fullpath = g_build_path (G_DIR_SEPARATOR_S, g_get_home_dir (),
			".gnome-desktop", NULL);
	desktop_exists = g_file_test (fullpath, G_FILE_TEST_EXISTS);
	g_free (fullpath);

	if (on_desktop != FALSE)
	{
		filename = g_strdup_printf (_("Screenshot%d.png"), i);
		fullpath = screenshot_make_filename_helper (filename,
				desktop_exists);

		while (g_file_test (fullpath, G_FILE_TEST_EXISTS) != FALSE
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
		entry = glade_xml_get_widget (vanity->xml, "combo-entry1");
		if (gtk_entry_get_text (GTK_ENTRY (entry)) == NULL)
			return NULL;

		fullpath = g_strdup (gtk_entry_get_text (GTK_ENTRY (entry)));
	}

	return fullpath;
}
#endif

static void
on_preferences1_activate (GtkButton *button, Vanity *vanity)
{
	gtk_widget_show (vanity->prefs);
}

static gboolean
vanity_action_handle_key (Vanity *vanity, GdkEventKey *event)
{
	gboolean retval = TRUE;

	/* Alphabetical */
	switch (event->keyval) {
#if 0
	case GDK_A:
	case GDK_a:
		vanity_action_toggle_aspect_ratio (vanity);
		if (vanity->action == 0)
			vanity->action++;
		else
			vanity->action = 0;
		break;
	case XF86XK_AudioPrev:
	case GDK_B:
	case GDK_b:
		vanity_action_previous (vanity);
		break;
	case GDK_C:
	case GDK_c:
		bacon_video_widget_dvd_event (vanity->bvw, BVW_DVD_CHAPTER_MENU);
		if (vanity->action == 1)
			vanity->action++;
		else
			vanity->action = 0;
		break;
	case GDK_f:
	case GDK_F:
		if (event->time - vanity->keypress_time
				>= KEYBOARD_HYSTERISIS_TIMEOUT)
			vanity_action_fullscreen_toggle (vanity);

		vanity->keypress_time = event->time;

		break;
	case GDK_h:
	case GDK_H:
		{
			GtkCheckMenuItem *item;
			gboolean value;

			item = GTK_CHECK_MENU_ITEM (glade_xml_get_widget
					(vanity->xml, "show_controls1"));
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
					(vanity->xml, "deinterlace1"));
			value = gtk_check_menu_item_get_active (item);
			gtk_check_menu_item_set_active (item, !value);
		}

		if (vanity->action == 3)
			vanity->action++;
		else
			vanity->action = 0;
		break;
	case GDK_M:
	case GDK_m:
		bacon_video_widget_dvd_event (vanity->bvw, BVW_DVD_ROOT_MENU);
		break;
	case XF86XK_AudioNext:
	case GDK_N:
	case GDK_n:
		vanity_action_next (vanity);
		if (vanity->action == 5)
			vanity_action_set_mrl_and_play (vanity, "v4l://");
		vanity->action = 0;
		break;
	case GDK_O:
	case GDK_o:
		vanity_action_fullscreen (vanity, FALSE);
		on_open1_activate (NULL, (gpointer) vanity);
		if (vanity->action == 4)
			vanity->action++;
		else
			vanity->action = 0;
		break;
	case XF86XK_AudioPlay:
	case XF86XK_AudioPause:
	case GDK_p:
	case GDK_P:
		vanity_action_play_pause (vanity);
		break;
	case GDK_q:
	case GDK_Q:
		vanity_action_exit (vanity);
		break;
	case GDK_s:
	case GDK_S:
		on_skip_to1_activate (NULL, vanity);
		break;
	case GDK_T:
		if (vanity->action == 2)
			vanity->action++;
		else
			vanity->action = 0;
		break;
	case GDK_Escape:
		vanity_action_fullscreen (vanity, FALSE);
		break;
	case GDK_Left:
		vanity_action_seek_relative (vanity, SEEK_BACKWARD_OFFSET);
		break;
	case GDK_Right:
		vanity_action_seek_relative (vanity, SEEK_FORWARD_OFFSET);
		break;
	case GDK_Up:
		vanity_action_volume_relative (vanity, VOLUME_UP_OFFSET);
		break;
	case GDK_Down:
		vanity_action_volume_relative (vanity, VOLUME_DOWN_OFFSET);
		break;
	case GDK_0:
	case GDK_onehalf:
		vanity_action_set_scale_ratio (vanity, 0.5);
		break;
	case GDK_1:
		vanity_action_set_scale_ratio (vanity, 1);
		break;
	case GDK_2:
		vanity_action_set_scale_ratio (vanity, 2);
		break;
#endif
	default:
		retval = FALSE;
	}

	return retval;
}

static int
on_window_key_press_event (GtkWidget *win, GdkEventKey *event, Vanity *vanity)
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

	return vanity_action_handle_key (vanity, event);
}

static void
vanity_callback_connect (Vanity *vanity)
{
	GtkWidget *item;

	/* Menu items */
	item = glade_xml_get_widget (vanity->xml, "zoom_12");
	g_signal_connect (G_OBJECT (item), "activate",
			G_CALLBACK (on_zoom_1_2_activate), vanity);
	item = glade_xml_get_widget (vanity->xml, "zoom_11");
	g_signal_connect (G_OBJECT (item), "activate",
			G_CALLBACK (on_zoom_1_1_activate), vanity);
	item = glade_xml_get_widget (vanity->xml, "zoom_21");
	g_signal_connect (G_OBJECT (item), "activate",
			G_CALLBACK (on_zoom_2_1_activate), vanity);
	item = glade_xml_get_widget (vanity->xml, "quit1");
	g_signal_connect (G_OBJECT (item), "activate",
			G_CALLBACK (on_quit1_activate), vanity);
	item = glade_xml_get_widget (vanity->xml, "about1");
	g_signal_connect (G_OBJECT (item), "activate",
			G_CALLBACK (on_about1_activate), vanity);
	item = glade_xml_get_widget (vanity->xml, "preferences1");
	g_signal_connect (G_OBJECT (item), "activate",
			G_CALLBACK (on_preferences1_activate), vanity);

	/* Exit */
	g_signal_connect (G_OBJECT (vanity->win), "delete-event",
			G_CALLBACK (main_window_destroy_cb), vanity);
	g_signal_connect (G_OBJECT (vanity->win), "destroy",
			G_CALLBACK (main_window_destroy_cb), vanity);

	/* Connect the keys */
	gtk_widget_add_events (vanity->win, GDK_KEY_PRESS_MASK);
	g_signal_connect (G_OBJECT(vanity->win), "key_press_event",
			G_CALLBACK (on_window_key_press_event), vanity);
}

static void
video_widget_create (Vanity *vanity) 
{
	GError *err = NULL;
	GtkWidget *container;

	vanity->bvw = BACON_VIDEO_WIDGET
		(bacon_video_widget_new (-1, -1, FALSE, &err));

	if (vanity->bvw == NULL)
	{
		char *msg;

		msg = g_strdup_printf (_("Vanity could not startup:\n%s"),
				err != NULL ? err->message : _("No reason"));

		if (err != NULL)
			g_error_free (err);

		gtk_widget_hide (vanity->win);
		vanity_action_error_and_exit (msg, vanity);
	}

	container = glade_xml_get_widget (vanity->xml, "frame1");
	gtk_container_add (GTK_CONTAINER (container),
			GTK_WIDGET (vanity->bvw));

	/* Events for the widget video window as well */
	gtk_widget_add_events (GTK_WIDGET (vanity->bvw), GDK_KEY_PRESS_MASK);
	g_signal_connect (G_OBJECT(vanity->bvw), "key_press_event",
			G_CALLBACK (on_window_key_press_event), vanity);

	g_signal_connect (G_OBJECT (vanity->bvw), "drag_data_get",
			G_CALLBACK (drag_video_cb), vanity);
	gtk_drag_source_set (GTK_WIDGET (vanity->bvw),
			GDK_BUTTON1_MASK | GDK_BUTTON3_MASK,
			source_table, G_N_ELEMENTS (source_table),
			GDK_ACTION_LINK);

	g_object_add_weak_pointer (G_OBJECT (vanity->bvw),
			(void**)&(vanity->bvw));

	gtk_widget_show (GTK_WIDGET (vanity->bvw));

	if (vanity->debug == FALSE)
	{
		bacon_video_widget_open (vanity->bvw, "v4l:/", &err);

		if (err != NULL)
		{
			char *msg;

			msg = g_strdup_printf (_("Vanity could not contact the webcam.\nReason: %s"), err->message);
			g_error_free (err);
			vanity_action_error_and_exit (msg, vanity);
		}
	} else {
		bacon_video_widget_set_logo (vanity->bvw, LOGO_PATH);
		bacon_video_widget_set_logo_mode (vanity->bvw, TRUE);
		g_message ("%s", LOGO_PATH);
	}

	bacon_video_widget_play (vanity->bvw, &err);

	if (err != NULL)
	{
		char *msg;

		msg = g_strdup_printf (_("Vanity could not play video from the webcam.\nReason: %s"), err->message);
		g_error_free (err);
		gtk_widget_hide (vanity->win);
		vanity_action_error_and_exit (msg, vanity);
	}

	gtk_widget_set_size_request (container, -1, -1);
}

static void
process_command_line (Vanity *vanity, int argc, char **argv)
{
	int i;

	if (argc == 1)
		return;

	for (i = 0; i < argc; i++)
	{
		if (strcmp (argv[i], "--debug") == 0)
			vanity->debug = TRUE;
	}
}

int
main (int argc, char **argv)
{
	Vanity *vanity;
	char *filename;
	GConfClient *gc;
	GError *err = NULL;

	bindtextdomain (GETTEXT_PACKAGE, GNOMELOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);

	g_set_application_name (_("Vanity Webcam Utility"));

	if (XInitThreads () == 0)
	{
		gtk_init (&argc, &argv);
		vanity_action_error_and_exit (_("Could not initialise the "
					"thread-safe libraries.\n"
					"Verify your system installation. "
					"Vanity will now exit."), NULL);
	}

	g_thread_init (NULL);
	gdk_threads_init ();

	gnome_program_init ("vanity", VERSION,
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

		str = g_strdup_printf (_("Vanity couln't initialise the \n"
					"configuration engine:\n%s"),
				err->message);
		vanity_action_error_and_exit (str, NULL);
		g_error_free (err);
		g_free (str);
	}
	gnome_authentication_manager_init ();

	filename = gnome_program_locate_file (NULL,
			GNOME_FILE_DOMAIN_APP_DATADIR,
			"totem/vanity.glade", TRUE, NULL);
	if (filename == NULL)
	{
		vanity_action_error_and_exit (_("Couldn't load the main "
					"interface (vanity.glade).\n"
					"Make sure that Vanity"
					" is properly installed."), NULL);
	}

	vanity = g_new0 (Vanity, 1);

	process_command_line (vanity, argc, argv);

	/* Main window */
	vanity->xml = glade_xml_new (filename, NULL, NULL);
	if (vanity->xml == NULL)
	{
		g_free (filename);
		vanity_action_error_and_exit (_("Couldn't load the main "
					"interface (vanity.glade).\n"
					"Make sure that Vanity"
					" is properly installed."), NULL);
	}
	g_free (filename);

	vanity->win = glade_xml_get_widget (vanity->xml, "app1");
	filename = g_build_filename (DATADIR, "totem", "vanity.png", NULL);
	gtk_window_set_default_icon_from_file (filename, NULL);
	g_free (filename);

	/* The rest of the widgets */
	vanity->prefs = glade_xml_get_widget (vanity->xml, "dialog1");
	vanity_callback_connect (vanity);

	/* Show ! gtk_main_iteration trickery to show all the widgets
	 * we have so far */
	gtk_widget_show_all (vanity->win);
	long_action ();

	/* Show ! (again) the video widget this time. */
	video_widget_create (vanity);

	/* The prefs after the video widget is connected */
//	vanity_setup_preferences (vanity);

	gtk_main ();

	return 0;
}

