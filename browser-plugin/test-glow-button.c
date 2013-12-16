
#include <gtk/gtk.h>
#include "totem-glow-button.h"

#if 1
static gboolean
idle_cb (gpointer data)
{
	TotemGlowButton *button = data;

	g_message ("launching the glow");
	totem_glow_button_set_glow (button, TRUE);

	return FALSE;
}
#endif
#if 1
static gboolean
idle_un_cb (gpointer data)
{
	TotemGlowButton *button = data;

	g_message ("stopping the glow");
	totem_glow_button_set_glow (button, FALSE);

	return FALSE;
}
#endif
int main (int argc, char **argv)
{
	GtkWidget *window, *button, *image;

	gtk_init (&argc, &argv);

	window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	button = totem_glow_button_new ();
	image = gtk_image_new_from_icon_name ("media-playback-start", GTK_ICON_SIZE_MENU);
	gtk_button_set_image (GTK_BUTTON (button), image);
	gtk_container_add (GTK_CONTAINER(window), button);

	totem_glow_button_set_glow (TOTEM_GLOW_BUTTON (button), TRUE);

	g_timeout_add_seconds (3, idle_un_cb, button);
	g_timeout_add_seconds (5, idle_cb, button);

	gtk_window_set_default_size (GTK_WINDOW (window), 200, 200);

	gtk_widget_show_all (window);

	gtk_main ();

	return 0;
}

