/*
 * Copyright (C) 2005  Bastien Nocera <hadess@hadess.net>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
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

#include <config.h>
#include <string.h>
#include <glib/gi18n-lib.h>
#include "totem-properties-view.h"
#include "bacon-video-widget.h"

static int i;

static gboolean
main_loop_exit (gpointer data)
{
	g_print ("Finishing %d\n", i);
	gtk_main_quit ();
	return FALSE;
}

int main (int argc, char **argv)
{
	GtkWidget *window, *props, *label;
	int times = 10;

	bindtextdomain (GETTEXT_PACKAGE, GNOMELOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);

	g_thread_init (NULL);
	gtk_init (&argc, &argv);

	if (argc < 2) {
		g_print ("Usage: %s [URI]\n", argv[0]);
		return 1;
	}

	bacon_video_widget_init_backend (NULL, NULL);

	label = gtk_label_new ("Audio/Video");
	window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	props = totem_properties_view_new (argv[1], label);
	gtk_container_add (GTK_CONTAINER (window), props);

	if (argc == 3) {
		times = g_strtod (argv[2], NULL);
	}

	gtk_widget_show_all (window);
	for (i = 0; i < times; i++) {
		g_print ("Setting %d\n", i);
		totem_properties_view_set_location (TOTEM_PROPERTIES_VIEW (props), argv[1]);
		g_timeout_add (4000, main_loop_exit, NULL);
		gtk_main ();
	}

	gtk_main ();

	return 0;
}

