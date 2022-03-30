/* totem-interface.c
 *
 *  Copyright (C) 2005 Bastien Nocera
 *
 *  The Gnome Library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public License as
 *  published by the Free Software Foundation; either version 2 of the
 *  License, or (at your option) any later version.
 *
 *  The Gnome Library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public
 *  License along with the Gnome Library; see the file COPYING.LIB.  If not,
 *  see <http://www.gnu.org/licenses/>.
 *
 *  Author: Bastien Nocera <hadess@hadess.net>
 * 
 * The Totem project hereby grant permission for non-gpl compatible GStreamer
 * plugins to be used and distributed together with GStreamer and Totem. This
 * permission are above and beyond the permissions granted by the GPL license
 * Totem is covered by.
 *
 * Monday 7th February 2005: Christian Schaller: Add exception clause.
 * See license_change file for details.
 *
 */

/**
 * SECTION:totem-interface
 * @short_description: interface utility/loading/error functions
 * @stability: Unstable
 * @include: totem-interface.h
 *
 * A collection of interface utility functions, for loading interfaces and displaying errors.
 **/

#include "config.h"

#include <glib.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>

#include "totem-interface.h"

static GtkWidget *
totem_interface_error_dialog (const char *title, const char *reason,
		GtkWindow *parent)
{
	GtkWidget *error_dialog;

	if (reason == NULL)
		g_warning ("%s called with reason == NULL", G_STRFUNC);

	error_dialog =
		gtk_message_dialog_new (NULL,
				GTK_DIALOG_MODAL,
				GTK_MESSAGE_ERROR,
				GTK_BUTTONS_OK,
				"%s", title);
	gtk_message_dialog_format_secondary_text
		(GTK_MESSAGE_DIALOG (error_dialog), "%s", reason);

	gtk_window_set_transient_for (GTK_WINDOW (error_dialog),
				      GTK_WINDOW (parent));
	gtk_window_set_title (GTK_WINDOW (error_dialog), ""); /* as per HIG */
	gtk_dialog_set_default_response (GTK_DIALOG (error_dialog),
			GTK_RESPONSE_OK);
	gtk_window_set_modal (GTK_WINDOW (error_dialog), TRUE);

	return error_dialog;
}

/**
 * totem_interface_error:
 * @title: the error title
 * @reason: the error reason (secondary text)
 * @parent: the error dialogue's parent #GtkWindow
 *
 * Display a modal error dialogue with @title as its primary error text, and @reason
 * as its secondary text.
 **/
void
totem_interface_error (const char *title, const char *reason,
		GtkWindow *parent)
{
	GtkWidget *error_dialog;

	error_dialog = totem_interface_error_dialog (title, reason, parent);

	g_signal_connect (G_OBJECT (error_dialog), "response", G_CALLBACK
			(gtk_widget_destroy), error_dialog);

	gtk_window_present (GTK_WINDOW (error_dialog));
}

/**
 * totem_interface_error_blocking:
 * @title: the error title
 * @reason: the error reason (secondary text)
 * @parent: the error dialogue's parent #GtkWindow
 *
 * Display a modal error dialogue like totem_interface_error() which blocks until the user has
 * dismissed it.
 **/
void
totem_interface_error_blocking (const char *title, const char *reason,
		GtkWindow *parent)
{
	GtkWidget *error_dialog;

	error_dialog = totem_interface_error_dialog (title, reason, parent);

	gtk_dialog_run (GTK_DIALOG (error_dialog));
	gtk_widget_destroy (error_dialog);
}

/**
 * totem_interface_create_header_button:
 * @header: The header widget to put the button in
 * @button: The button to use in the header
 * @icon_name: The icon name for the button image
 * @pack_type: A #GtkPackType to tell us where to include the button
 *
 * Put the given @icon_name into @button, and pack @button into @header
 * according to @pack_type.
 *
 * Return value: (transfer none): the button passed as input
 */
GtkWidget *
totem_interface_create_header_button (GtkWidget  *header,
				      GtkWidget  *button,
				      const char *icon_name,
				      GtkPackType pack_type)
{
	GtkWidget *image;
	GtkStyleContext *context;

	image = gtk_image_new_from_icon_name (icon_name, GTK_ICON_SIZE_MENU);
	gtk_button_set_image (GTK_BUTTON (button), image);
	context = gtk_widget_get_style_context (button);
	gtk_style_context_add_class (context, "image-button");
	g_object_set (G_OBJECT (button), "valign", GTK_ALIGN_CENTER, NULL);
	if (GTK_IS_MENU_BUTTON (button))
		g_object_set (G_OBJECT (button), "use-popover", TRUE, NULL);

	if (pack_type == GTK_PACK_END)
		gtk_header_bar_pack_end (GTK_HEADER_BAR (header), button);
	else
		gtk_header_bar_pack_start (GTK_HEADER_BAR (header), button);
	gtk_widget_show_all (button);

	return button;
}
