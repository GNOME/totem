/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * Copyright (C) 2004 Bastien Nocera
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
#include "totem-screenshot.h"

#include <glib/gi18n.h>
#include <glib/gstdio.h>
#include <gtk/gtk.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/stat.h>

#include "debug.h"
#include "totem-interface.h"
#include "totem-uri.h"
#include "totem-screenshot-plugin.h"

struct TotemScreenshotPrivate
{
	GtkWidget *chooser;
	GtkWidget *image;
	GdkPixbuf *pixbuf, *scaled;
	char *temp_file;
};

G_DEFINE_TYPE (TotemScreenshot, totem_screenshot, GTK_TYPE_DIALOG)

static void
totem_screenshot_temp_file_create (TotemScreenshot *screenshot)
{
	char *dir, *fulldir;

	dir = g_strdup_printf ("totem-screenshot-%d", getpid ());
	fulldir = g_build_filename (g_get_tmp_dir (), dir, NULL);
	if (g_mkdir (fulldir, 0700) < 0) {
		g_free (fulldir);
		g_free (dir);
		return;
	}
	screenshot->_priv->temp_file = g_build_filename
		(g_get_tmp_dir (),
		 dir, _("Screenshot.png"), NULL);
}

static void
totem_screenshot_temp_file_remove (TotemScreenshot *screenshot)
{
	char *dirname;

	if (screenshot->_priv->temp_file == NULL)
		return;

	unlink (screenshot->_priv->temp_file);
	dirname = g_path_get_dirname (screenshot->_priv->temp_file);
	rmdir (dirname);
	g_free (dirname);

	g_free (screenshot->_priv->temp_file);
}


static void
drag_data_get (GtkWidget          *widget,
	       GdkDragContext     *context,
	       GtkSelectionData   *selection_data,
	       guint               info,
	       guint               time,
	       TotemScreenshot    *screenshot)
{
	char *string;

	/* FIXME We should cancel the drag */
	if (screenshot->_priv->temp_file == NULL)
		return;

	string = g_strdup_printf ("file://%s\r\n",
			screenshot->_priv->temp_file);
	gtk_selection_data_set (selection_data,
			selection_data->target,
			8, (guchar *)string, strlen (string)+1);
	g_free (string);
}

static void
drag_begin (GtkWidget *widget, GdkDragContext *context,
		TotemScreenshot *screenshot)
{
	if (screenshot->_priv->temp_file == NULL)
	{
		gtk_drag_set_icon_pixbuf (context, screenshot->_priv->scaled,
				0, 0);
		totem_screenshot_temp_file_create (screenshot);
		g_return_if_fail (screenshot->_priv->temp_file != NULL);
		gdk_pixbuf_save (screenshot->_priv->pixbuf,
				screenshot->_priv->temp_file, "png",
				NULL, NULL);
	}
}

static void
totem_screenshot_response (GtkDialog *dialog, int response)
{
	TotemScreenshot *screenshot = TOTEM_SCREENSHOT (dialog);
	char *filename;
	GError *err = NULL;

	if (response != GTK_RESPONSE_ACCEPT)
		return;

	filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (screenshot->_priv->chooser));

	if (gdk_pixbuf_save (screenshot->_priv->pixbuf, filename, "png", &err, NULL) == FALSE) {
		totem_interface_error (_("There was an error saving the screenshot."),
				       err->message,
			 	       GTK_WINDOW (screenshot));
		g_error_free (err);
		g_free (filename);
		return;
	}

	totem_screenshot_plugin_update_file_chooser (filename);
	g_free (filename);
}

static void
totem_screenshot_init (TotemScreenshot *screenshot)
{
	GtkWidget *box;

	screenshot->_priv = g_new0 (TotemScreenshotPrivate, 1);

	gtk_container_set_border_width (GTK_CONTAINER (screenshot), 5);

	screenshot->_priv->chooser = gtk_file_chooser_widget_new (GTK_FILE_CHOOSER_ACTION_SAVE);
	totem_add_pictures_dir (screenshot->_priv->chooser);
	box = gtk_hbox_new (FALSE, 5);
	gtk_container_add (GTK_CONTAINER (GTK_DIALOG (screenshot)->vbox), box);
	screenshot->_priv->image = gtk_image_new ();
	gtk_box_pack_start (GTK_BOX (box),
			    screenshot->_priv->image,
			    FALSE,
			    FALSE,
			    0);
	gtk_box_pack_start (GTK_BOX (box),
			    screenshot->_priv->chooser,
			    TRUE,
			    TRUE,
			    0);

	gtk_dialog_add_buttons (GTK_DIALOG (screenshot),
				GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
				GTK_STOCK_SAVE, GTK_RESPONSE_ACCEPT,
				NULL);
	gtk_dialog_set_has_separator (GTK_DIALOG (screenshot), FALSE);
	gtk_window_set_title (GTK_WINDOW (screenshot), _("Save Screenshot"));
	gtk_dialog_set_default_response (GTK_DIALOG (screenshot), GTK_RESPONSE_ACCEPT);

	/* Setup the DnD for the image */
	g_signal_connect (G_OBJECT (screenshot->_priv->image), "drag_begin",
			G_CALLBACK (drag_begin), screenshot);
	g_signal_connect (G_OBJECT (screenshot->_priv->image), "drag_data_get",
			G_CALLBACK (drag_data_get), screenshot);
	gtk_drag_source_set (GTK_WIDGET (screenshot->_priv->image),
			GDK_BUTTON1_MASK | GDK_BUTTON3_MASK,
			NULL, 0,
			GDK_ACTION_COPY);
	gtk_drag_source_add_uri_targets (GTK_WIDGET (screenshot->_priv->image));

	/* Set the default path and filename */
	totem_screenshot_plugin_setup_file_chooser (GTK_FILE_CHOOSER (screenshot->_priv->chooser), N_("Screenshot%d.png"));

	gtk_widget_show_all (GTK_DIALOG (screenshot)->vbox);
}

static void
totem_screenshot_finalize (GObject *object)
{
	TotemScreenshot *screenshot = TOTEM_SCREENSHOT (object);

	g_return_if_fail (object != NULL);

	totem_screenshot_temp_file_remove (screenshot);

	if (screenshot->_priv->pixbuf != NULL)
		g_object_unref (screenshot->_priv->pixbuf);
	if (screenshot->_priv->scaled != NULL)
		g_object_unref (screenshot->_priv->scaled);

	G_OBJECT_CLASS (totem_screenshot_parent_class)->finalize (object);
}

GtkWidget*
totem_screenshot_new (GdkPixbuf *screen_image)
{
	TotemScreenshot *screenshot;
	int width, height;

	screenshot = TOTEM_SCREENSHOT (g_object_new (TOTEM_TYPE_SCREENSHOT, NULL));

	height = 200;
	width = height * gdk_pixbuf_get_width (screen_image)
		/ gdk_pixbuf_get_height (screen_image);
	screenshot->_priv->pixbuf = screen_image;
	g_object_ref (G_OBJECT (screenshot->_priv->pixbuf));
	screenshot->_priv->scaled = gdk_pixbuf_scale_simple (screen_image,
			width, height, GDK_INTERP_BILINEAR);
	gtk_image_set_from_pixbuf (GTK_IMAGE (screenshot->_priv->image),
				   screenshot->_priv->scaled);

	return GTK_WIDGET (screenshot);
}

static void
totem_screenshot_class_init (TotemScreenshotClass *klass)
{
	G_OBJECT_CLASS (klass)->finalize = totem_screenshot_finalize;
	GTK_DIALOG_CLASS (klass)->response = totem_screenshot_response;
}

