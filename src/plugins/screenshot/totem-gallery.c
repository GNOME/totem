/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * Copyright (C) 2008 Philip Withnall <philip@tecnocode.co.uk>
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA.
 *
 * The Totem project hereby grant permission for non-gpl compatible GStreamer
 * plugins to be used and distributed together with GStreamer and Totem. This
 * permission are above and beyond the permissions granted by the GPL license
 * Totem is covered by.
 *
 * Monday 7th February 2005: Christian Schaller: Add exception clause.
 * See license_change file for details.
 */

#include "config.h"

#include <glib.h>
#include <gtk/gtk.h>
#include <glib/gi18n-lib.h>

#include "totem-dirs.h"
#include "totem-gallery.h"
#include "totem-gallery-progress.h"
#include "totem-screenshot-plugin.h"

static void dialog_response_callback (GtkDialog *dialog, gint response_id, TotemGallery *self);

static void default_screenshot_count_toggled_callback (GtkToggleButton *toggle_button, TotemGallery *self);

struct _TotemGallery {
	GtkFileChooserDialog parent;
	Totem *totem;
	GtkCheckButton *default_screenshot_count;
	GtkSpinButton *screenshot_count;
	GtkSpinButton *screenshot_width;
};

G_DEFINE_TYPE (TotemGallery, totem_gallery, GTK_TYPE_FILE_CHOOSER_DIALOG)

static void
totem_gallery_class_init (TotemGalleryClass *klass)
{
}

static void
totem_gallery_init (TotemGallery *self)
{
}

TotemGallery *
totem_gallery_new (Totem *totem)
{
	TotemGallery *gallery;
	GtkWidget *container;
	GtkBuilder *builder;
	gchar *movie_title, *uri, *suggested_name;
	GFile *file;

	/* Create the gallery and its interface */
	gallery = g_object_new (TOTEM_TYPE_GALLERY, NULL);

	builder = gtk_builder_new_from_resource ("/org/gnome/totem/plugins/screenshot/gallery.ui");

	/* Grab the widgets */
	gallery->default_screenshot_count = GTK_CHECK_BUTTON (gtk_builder_get_object (builder, "default_screenshot_count"));
	gallery->screenshot_count = GTK_SPIN_BUTTON (gtk_builder_get_object (builder, "screenshot_count"));
	gallery->screenshot_width = GTK_SPIN_BUTTON (gtk_builder_get_object (builder, "screenshot_width"));
	g_signal_connect (gtk_builder_get_object (builder, "default_screenshot_count"), "toggled",
			  G_CALLBACK (default_screenshot_count_toggled_callback), gallery);

	gallery->totem = totem;

	gtk_window_set_title (GTK_WINDOW (gallery), _("Save Gallery"));
	gtk_file_chooser_set_action (GTK_FILE_CHOOSER (gallery), GTK_FILE_CHOOSER_ACTION_SAVE);
	gtk_file_chooser_set_do_overwrite_confirmation (GTK_FILE_CHOOSER (gallery), TRUE);
	/*gtk_window_set_resizable (GTK_WINDOW (gallery), FALSE);
	gtk_dialog_set_has_separator (GTK_DIALOG (gallery), TRUE);*/
	gtk_dialog_add_buttons (GTK_DIALOG (gallery),
			_("_Cancel"), GTK_RESPONSE_CANCEL,
			_("_Save"), GTK_RESPONSE_OK,
			NULL);
	gtk_dialog_set_default_response (GTK_DIALOG (gallery), GTK_RESPONSE_OK);

	g_signal_connect (G_OBJECT (gallery), "response",
			  G_CALLBACK (dialog_response_callback), gallery);

	container = GTK_WIDGET (gtk_builder_get_object (builder,
				"gallery_dialog_content"));
	gtk_file_chooser_set_extra_widget (GTK_FILE_CHOOSER (gallery), container);

	movie_title = totem_object_get_short_title (totem);

	/* Translators: The first argument is the movie title. The second
	 * argument is a number which is used to prevent overwriting files.
	 * Just translate "Gallery", and not the ".jpg". Example:
	 * "Galerie-%s-%d.jpg". */
	uri = totem_screenshot_plugin_setup_file_chooser (N_("Gallery-%s-%d.jpg"), movie_title);
	g_free (movie_title);

	file = g_file_new_for_uri (uri);
	/* We can use g_file_get_basename here and be sure that it's UTF-8
	 * because we provided the name. */
	suggested_name = g_file_get_basename (file);
	g_object_unref (file);

	gtk_file_chooser_set_uri (GTK_FILE_CHOOSER (gallery), uri);
	g_free (uri);
	gtk_file_chooser_set_current_name (GTK_FILE_CHOOSER (gallery), suggested_name);
	g_free (suggested_name);

	gtk_widget_show_all (GTK_WIDGET (gallery));

	g_object_unref (builder);

	return gallery;
}

static void
default_screenshot_count_toggled_callback (GtkToggleButton *toggle_button, TotemGallery *self)
{
	/* Only have the screenshot count spin button sensitive when the default screenshot count
	 * check button is unchecked. */
	gtk_widget_set_sensitive (GTK_WIDGET (self->screenshot_count), !gtk_toggle_button_get_active (toggle_button));
}

static void
dialog_response_callback (GtkDialog *dialog, gint response_id, TotemGallery *self)
{
	gchar *filename, *video_mrl, *argv[6];
	guint screenshot_count, i;
	gint stdout_fd;
	GPid child_pid;
	GtkWidget *progress_dialog;
	gboolean ret;
	GError *error = NULL;

	if (response_id != GTK_RESPONSE_OK)
		return;
	gtk_widget_hide (GTK_WIDGET (dialog));

	/* Don't call in here again */
	g_signal_handlers_disconnect_by_func (G_OBJECT (self), dialog_response_callback, self);

	if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (self->default_screenshot_count)) == TRUE)
		screenshot_count = 0;
	else
		screenshot_count = gtk_spin_button_get_value_as_int (self->screenshot_count);

	filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (self));
	video_mrl = totem_object_get_current_mrl (self->totem);
	totem_screenshot_plugin_update_file_chooser (gtk_file_chooser_get_uri (GTK_FILE_CHOOSER (self)));

	/* Build the command and arguments to pass it */
	argv[0] = (gchar*) LIBEXECDIR "/totem-gallery-thumbnailer"; /* a little hacky, but only the allocated stuff is freed below */
	argv[1] = g_strdup_printf ("--gallery=%u", screenshot_count); /* number of screenshots to output */
	argv[2] = g_strdup_printf ("--size=%u", gtk_spin_button_get_value_as_int (self->screenshot_width)); /* screenshot width */
	argv[3] = video_mrl; /* video to thumbnail */
	argv[4] = filename; /* output filename */
	argv[5] = NULL;

	/* Run the command */
	ret = g_spawn_async_with_pipes (NULL, argv, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL,
					&child_pid, NULL, &stdout_fd, NULL, &error);

	/* Free argv, minus the filename */
	for (i = 1; i < G_N_ELEMENTS (argv) - 2; i++)
		g_free (argv[i]);

	if (ret == FALSE) {
		g_warning ("Error spawning totem-video-thumbnailer: %s", error->message);
		g_error_free (error);
		return;
	}

	/* Create the progress dialogue */
	progress_dialog = GTK_WIDGET (totem_gallery_progress_new (child_pid, filename));
	g_free (filename);
	totem_gallery_progress_run (TOTEM_GALLERY_PROGRESS (progress_dialog), stdout_fd);
	gtk_dialog_run (GTK_DIALOG (progress_dialog));
	gtk_widget_destroy (progress_dialog);

	gtk_dialog_response (GTK_DIALOG (self), 0);
}

