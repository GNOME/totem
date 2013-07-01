
#include "config.h"

#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include "bacon-video-widget.h"
#ifdef GDK_WINDOWING_X11
#include <X11/Xlib.h>
#endif

static char **filenames;
static const char *argument;
static char *mrl;

static void
test_bvw_set_mrl (GtkWidget *bvw, const char *path)
{
	mrl = g_strdup (path);
	bacon_video_widget_open (BACON_VIDEO_WIDGET (bvw), mrl);
}

static void
on_redirect (GtkWidget *bvw, const char *redirect_mrl, gpointer data)
{
	g_message ("Redirect to: %s", redirect_mrl);
}

static void
on_eos_event (GtkWidget *bvw, gpointer user_data)
{
	bacon_video_widget_stop (BACON_VIDEO_WIDGET (bvw));
	bacon_video_widget_close (BACON_VIDEO_WIDGET (bvw));
	g_free (mrl);

	test_bvw_set_mrl (bvw, argument);

	bacon_video_widget_play (BACON_VIDEO_WIDGET (bvw), NULL);
}

static void
on_got_metadata (BaconVideoWidget *bvw, gpointer data)
{
	GValue value = { 0, };
	char *title, *artist;

	bacon_video_widget_get_metadata (BACON_VIDEO_WIDGET (bvw),
			BVW_INFO_TITLE, &value);
	title = g_value_dup_string (&value);
	g_value_unset (&value);

	bacon_video_widget_get_metadata (BACON_VIDEO_WIDGET (bvw),
			BVW_INFO_ARTIST, &value);
	artist = g_value_dup_string (&value);
	g_value_unset (&value);

	g_message ("Got metadata: title = %s artist = %s", title, artist);
}

static void
error_cb (GtkWidget *bvw, const char *message,
		gboolean playback_stopped, gboolean fatal)
{
	g_message ("Error: %s, playback stopped: %d, fatal: %d",
			message, playback_stopped, fatal);
}

static GOptionEntry option_entries [] = {
	{ G_OPTION_REMAINING, 0, 0, G_OPTION_ARG_FILENAME_ARRAY /* STRING? */, &filenames, NULL },
	{ NULL }
};

int main
(int argc, char **argv)
{
	GOptionContext *context;
	GOptionGroup *baconoptiongroup;
	GError *error = NULL;
	GtkWidget *win, *bvw;
	GtkSettings *gtk_settings;
	GtkBox *box;
	GtkToolItem *item;
	GtkWidget *image;
	gchar *icon_start, *icon_skip_forward, *icon_skip_backward;

#ifdef GDK_WINDOWING_X11
	XInitThreads ();
#endif

	if (gtk_clutter_init (NULL, NULL) != CLUTTER_INIT_SUCCESS)
		g_assert_not_reached ();

	if (gtk_widget_get_default_direction () == GTK_TEXT_DIR_RTL) {
		icon_start = "media-playback-start-rtl-symbolic";
		icon_skip_forward = "media-skip-forward-rtl-symbolic";
		icon_skip_backward = "media-skip-backward-rtl-symbolic";
	} else {
		icon_start = "media-playback-start-symbolic";
		icon_skip_forward = "media-skip-forward-symbolic";
		icon_skip_backward = "media-skip-backward-symbolic";
	}

	context = g_option_context_new ("- Play audio and video inside a web browser");
	baconoptiongroup = bacon_video_widget_get_option_group();
	g_option_context_add_main_entries (context, option_entries, GETTEXT_PACKAGE);
	g_option_context_set_translation_domain(context, GETTEXT_PACKAGE);
	g_option_context_add_group (context, baconoptiongroup);
	g_option_context_add_group (context, gtk_get_option_group (TRUE));

	if (g_option_context_parse (context, &argc, &argv, &error) == FALSE) {
		g_print ("Failed to parse options: %s\n", error->message);
		g_error_free (error);
		return 1;
	}
	if (filenames != NULL &&
	    g_strv_length (filenames) > 1) {
		char *help;
		help = g_option_context_get_help (context, TRUE, NULL);
		g_print ("%s", help);
		g_free (help);
		return 1;
	}

	gtk_settings = gtk_settings_get_default ();
	g_object_set (G_OBJECT (gtk_settings), "gtk-application-prefer-dark-theme", TRUE, NULL);

	win = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	g_signal_connect (G_OBJECT (win), "destroy",
			G_CALLBACK (gtk_main_quit), NULL);

	bvw = bacon_video_widget_new (NULL);
	bacon_video_widget_set_logo (BACON_VIDEO_WIDGET (bvw), "totem");
	bacon_video_widget_set_show_visualizations (BACON_VIDEO_WIDGET (bvw), TRUE);

	g_signal_connect (G_OBJECT (bvw), "eos", G_CALLBACK (on_eos_event), NULL);
	g_signal_connect (G_OBJECT (bvw), "got-metadata", G_CALLBACK (on_got_metadata), NULL);
	g_signal_connect (G_OBJECT (bvw), "got-redirect", G_CALLBACK (on_redirect), NULL);
	g_signal_connect (G_OBJECT (bvw), "error", G_CALLBACK (error_cb), NULL);

	box = g_object_get_data (bacon_video_widget_get_controls_object (BACON_VIDEO_WIDGET (bvw)), "controls_box");

	/* Previous */
	item = gtk_tool_button_new (NULL, NULL);
	gtk_tool_button_set_icon_name (GTK_TOOL_BUTTON (item), icon_skip_backward);
	gtk_box_pack_start (box, GTK_WIDGET (item), FALSE, FALSE, 0);

	/* Play/Pause */
	item = gtk_tool_button_new (NULL, NULL);
	gtk_tool_button_set_icon_name (GTK_TOOL_BUTTON (item), icon_start);
	gtk_box_pack_start (box, GTK_WIDGET (item), FALSE, FALSE, 0);

	/* Next */
	item = gtk_tool_button_new (NULL, NULL);
	gtk_tool_button_set_icon_name (GTK_TOOL_BUTTON (item), icon_skip_forward);
	gtk_box_pack_start (box, GTK_WIDGET (item), FALSE, FALSE, 0);

	/* Separator */
	item = gtk_separator_tool_item_new ();
	gtk_box_pack_start (box, GTK_WIDGET (item), FALSE, FALSE, 0);

	/* Go button */
	item = g_object_get_data (bacon_video_widget_get_controls_object (BACON_VIDEO_WIDGET (bvw)), "go_button");
	image = gtk_image_new_from_icon_name ("view-more-symbolic", GTK_ICON_SIZE_SMALL_TOOLBAR);
	gtk_button_set_image (GTK_BUTTON (item), image);

	gtk_widget_show_all (GTK_WIDGET (box));

	gtk_container_add (GTK_CONTAINER (win),bvw);

	gtk_widget_realize (GTK_WIDGET (win));
	gtk_widget_realize (bvw);

	gtk_widget_show (win);
	gtk_widget_show (bvw);

	if (filenames && filenames[0]) {
		test_bvw_set_mrl (bvw, filenames[0]);
		argument = g_strdup (filenames[0]);
		bacon_video_widget_play (BACON_VIDEO_WIDGET (bvw), NULL);
	} else {
		bacon_video_widget_set_logo_mode (BACON_VIDEO_WIDGET (bvw), TRUE);
	}

	gtk_main ();

	return 0;
}

