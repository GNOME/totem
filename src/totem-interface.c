/* totem-interface.c
 *
 * Copyright (C) 2005 Bastien Nocera
 *
 * SPDX-License-Identifier: GPL-3-or-later
 *
 * Author: Bastien Nocera <hadess@hadess.net>
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

	g_signal_connect (G_OBJECT (error_dialog), "response",
			  G_CALLBACK(gtk_widget_destroy), error_dialog);

	gtk_window_present (GTK_WINDOW (error_dialog));
}

static void
dialog_response_cb (GtkWidget *dialog,
		    int response_id,
		    gpointer data)
{
	gboolean *done = data;
	*done = TRUE;
}

static void
dialog_destroy_cb (GtkDialog *dialog,
		   gpointer data)
{
	gboolean *destroyed = data;
	*destroyed = TRUE;
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
	gboolean done = FALSE;
	gboolean destroyed = FALSE;

	error_dialog = totem_interface_error_dialog (title, reason, parent);
	g_signal_connect (G_OBJECT (error_dialog), "response",
			  G_CALLBACK(dialog_response_cb), &done);
	g_signal_connect (G_OBJECT (error_dialog), "destroy",
			  G_CALLBACK(dialog_destroy_cb), &destroyed);
	gtk_window_present (GTK_WINDOW (error_dialog));
	while (!done)
		g_main_context_iteration (NULL, TRUE);
	if (!destroyed)
		gtk_widget_destroy (error_dialog);
}
