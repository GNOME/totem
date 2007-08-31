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
 *  write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 *  Boston, MA 02111-1307, USA.
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

#include "config.h"

#include <glib.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <gdk/gdkx.h>
#include <gconf/gconf-client.h>

#include "totem-interface.h"
#include "totem-private.h"

static GtkWidget *
totem_interface_error_dialog (const char *title, const char *reason,
		GtkWindow *parent)
{
	GtkWidget *error_dialog;

	if (reason == NULL)
		g_warning ("totem_action_error called with reason == NULL");

	error_dialog =
		gtk_message_dialog_new (NULL,
				GTK_DIALOG_MODAL,
				GTK_MESSAGE_ERROR,
				GTK_BUTTONS_OK,
				"%s", title);
	gtk_message_dialog_format_secondary_text
		(GTK_MESSAGE_DIALOG (error_dialog), "%s", reason);

	totem_interface_set_transient_for (GTK_WINDOW (error_dialog),
				GTK_WINDOW (parent));
	gtk_window_set_title (GTK_WINDOW (error_dialog), ""); /* as per HIG */
	gtk_container_set_border_width (GTK_CONTAINER (error_dialog), 5);
	gtk_dialog_set_default_response (GTK_DIALOG (error_dialog),
			GTK_RESPONSE_OK);
	gtk_window_set_modal (GTK_WINDOW (error_dialog), TRUE);

	return error_dialog;
}

void
totem_interface_error (const char *title, const char *reason,
		GtkWindow *parent)
{
	GtkWidget *error_dialog;

	error_dialog = totem_interface_error_dialog (title, reason, parent);

	g_signal_connect (G_OBJECT (error_dialog), "destroy", G_CALLBACK
			(gtk_widget_destroy), error_dialog);
	g_signal_connect (G_OBJECT (error_dialog), "response", G_CALLBACK
			(gtk_widget_destroy), error_dialog);
	gtk_window_set_modal (GTK_WINDOW (error_dialog), TRUE);

	gtk_widget_show (error_dialog);
}

void
totem_interface_error_blocking (const char *title, const char *reason,
		GtkWindow *parent)
{
	GtkWidget *error_dialog;

	error_dialog = totem_interface_error_dialog (title, reason, parent);

	gtk_dialog_run (GTK_DIALOG (error_dialog));
	gtk_widget_destroy (error_dialog);
}

static void
link_button_clicked_cb (GtkWidget *widget, Totem *totem)
{
	const char *uri;
	char *command, *browser, *escaped_uri;
	GError *error = NULL;

	uri = gtk_link_button_get_uri (GTK_LINK_BUTTON (widget));
	escaped_uri = g_shell_quote (uri);
	browser = gconf_client_get_string (totem->gc, "/desktop/gnome/url-handlers/http/command", NULL);

	if (browser == NULL || browser[0] == '\0') {
		char *message;

		message = g_strdup_printf(_("Could not launch URL \"%s\": %s"), uri, _("Default browser not configured"));
		totem_interface_error (_("Error launching URI"), message, GTK_WINDOW (totem->win));
		g_free (message);
	} else {
		char *message;

		command = g_strdup_printf (browser, escaped_uri);
		if (g_spawn_command_line_async ((const char*) command, &error) == FALSE) {
			message = g_strdup_printf(_("Could not launch URL \"%s\": %s"), uri, error->message);
			totem_interface_error (_("Error launching URI"), message, GTK_WINDOW (totem->win));
			g_free (message);
			g_error_free (error);
		}
		g_free (command);
	}

	g_free (escaped_uri);
	g_free (browser);
}

void
totem_interface_error_with_link (const char *title, const char *reason,
				 const char *uri, const char *label, GtkWindow *parent, Totem *totem)
{
	GtkWidget *error_dialog, *link_button, *hbox;

	if (label == NULL)
		label = uri;

	error_dialog = totem_interface_error_dialog (title, reason, parent);
	link_button = gtk_link_button_new_with_label (uri, label);
	g_signal_connect (G_OBJECT (link_button), "clicked", G_CALLBACK (link_button_clicked_cb), totem);

	hbox = gtk_hbox_new (TRUE, 0);
	gtk_box_pack_start (GTK_BOX (hbox), link_button, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (GTK_DIALOG (error_dialog)->vbox), hbox, TRUE, FALSE, 0); 

	gtk_dialog_set_default_response (GTK_DIALOG (error_dialog), GTK_RESPONSE_OK);

	g_signal_connect (G_OBJECT (error_dialog), "destroy", G_CALLBACK
			(gtk_widget_destroy), error_dialog);
	g_signal_connect (G_OBJECT (error_dialog), "response", G_CALLBACK
			(gtk_widget_destroy), error_dialog);
	gtk_window_set_modal (GTK_WINDOW (error_dialog), TRUE);

	gtk_widget_show_all (error_dialog);
}

GtkBuilder *
totem_interface_load (const char *name, gboolean fatal, GtkWindow *parent, gpointer user_data)
{
	GtkBuilder *builder = NULL;
	char *filename;

	filename = totem_interface_get_full_path (name);
	builder = totem_interface_load_with_full_path (filename, fatal, parent,
						       user_data);
	g_free (filename);

	return builder;
}

GtkBuilder *
totem_interface_load_with_full_path (const char *filename, gboolean fatal, 
				     GtkWindow *parent, gpointer user_data)
{
	GtkBuilder *builder = NULL;
	GError *error = NULL;

	if (filename != NULL) {
		builder = gtk_builder_new ();
		gtk_builder_set_translation_domain (builder, GETTEXT_PACKAGE);
	}

	if (builder == NULL || gtk_builder_add_from_file (builder, filename, &error) == FALSE)
	{
		char *msg;

		msg = g_strdup_printf (_("Couldn't load the '%s' interface. %s"), filename, error->message);
		if (fatal == FALSE)
			totem_interface_error (msg, _("Make sure that Totem is properly installed."), parent);
		else
			totem_interface_error_blocking (msg, _("Make sure that Totem is properly installed."), parent);

		g_free (msg);
		g_error_free (error);

		return NULL;
	}

	gtk_builder_connect_signals (builder, user_data);

	return builder;
}

GdkPixbuf*
totem_interface_load_pixbuf (const char *name)
{
	GdkPixbuf *pix;
	char *filename;

	filename = totem_interface_get_full_path (name);
	if (filename == NULL)
		return NULL;
	pix = gdk_pixbuf_new_from_file (filename, NULL);
	g_free (filename);
	return pix;
}

char *
totem_interface_get_full_path (const char *name)
{
	char *filename;

#ifdef TOTEM_RUN_IN_SOURCE_TREE
	/* Try the GtkBuilder file in the source tree first */
	filename = g_build_filename ("..", "data", name, NULL);
	if (g_file_test (filename, G_FILE_TEST_EXISTS) == FALSE)
	{
		g_free (filename);
		/* Try the local file */
		filename = g_build_filename (DATADIR,
				"totem", name, NULL);

		if (g_file_test (filename, G_FILE_TEST_EXISTS) == FALSE)
		{
			g_free (filename);
			return NULL;
		}
	}
#else
	filename = g_build_filename (DATADIR,
	                                "totem", name, NULL);
#endif

	return filename;
}

static GdkWindow *
totem_gtk_plug_get_toplevel (GtkPlug *plug)
{
	Window root, parent, *children;
	guint nchildren;
	GdkNativeWindow xid;

	g_return_val_if_fail (GTK_IS_PLUG (plug), NULL);

	xid = gtk_plug_get_id (plug);

	do
	{
		/* FIXME: multi-head */
		if (XQueryTree (GDK_DISPLAY (), xid, &root,
					&parent, &children, &nchildren) == 0)
		{
			g_warning ("Couldn't find window manager window");
			return NULL;
		}

		if (root == parent) {
			GdkWindow *toplevel;
			toplevel = gdk_window_foreign_new (xid);
			return toplevel;
		}

		xid = parent;
	}
	while (TRUE);
}

void
totem_interface_set_transient_for (GtkWindow *window, GtkWindow *parent)
{
	if (GTK_IS_PLUG (parent)) {
		GdkWindow *toplevel;

		gtk_widget_realize (GTK_WIDGET (window));
		toplevel = totem_gtk_plug_get_toplevel (GTK_PLUG (parent));
		if (toplevel != NULL) {
			gdk_window_set_transient_for
				(GTK_WIDGET (window)->window, toplevel);
			g_object_unref (toplevel);
		}
	} else {
		gtk_window_set_transient_for (GTK_WINDOW (window),
				GTK_WINDOW (parent));
	}
}

char *
totem_interface_get_license (void)
{
	const char *license[] = {
		N_("Totem is free software; you can redistribute it and/or modify "
		   "it under the terms of the GNU General Public License as published by "
		   "the Free Software Foundation; either version 2 of the License, or "
		   "(at your option) any later version."),
		N_("Totem is distributed in the hope that it will be useful, "
		   "but WITHOUT ANY WARRANTY; without even the implied warranty of "
		   "MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the "
		   "GNU General Public License for more details."),
		N_("You should have received a copy of the GNU General Public License "
		   "along with Totem; if not, write to the Free Software Foundation, Inc., "
		   "59 Temple Place, Suite 330, Boston, MA  02111-1307  USA"),
		N_("Totem contains an exception to allow the use of proprietary "
		   "GStreamer plugins.")
	};
	return g_strjoin ("\n\n",
			  _(license[0]),
			  _(license[1]),
			  _(license[2]),
			  _(license[3]),
			  NULL);
}

void
totem_interface_boldify_label (GtkBuilder *builder, const char *name)
{
	GtkLabel *widget;
	gchar *str_final;

	widget = GTK_LABEL (gtk_builder_get_object (builder, name));

	if (widget == NULL) {
		g_warning ("widget '%s' not found", name);
		return;
	}

	/* this way is probably better, but for some reason doesn't work with
	 * labels with mnemonics.

	static PangoAttrList *pattrlist = NULL;

	if (pattrlist == NULL) {
		PangoAttribute *attr;

		pattrlist = pango_attr_list_new ();
		attr = pango_attr_weight_new (PANGO_WEIGHT_BOLD);
		attr->start_index = 0;
		attr->end_index = G_MAXINT;
		pango_attr_list_insert (pattrlist, attr);
	}
	gtk_label_set_attributes (widget, pattrlist);*/

	str_final = g_strdup_printf ("<b>%s</b>", gtk_label_get_label (widget));
	gtk_label_set_markup_with_mnemonic (widget, str_final);
	g_free (str_final);
}

void
totem_interface_italicise_label (GtkBuilder *builder, const char *name)
{
	GtkLabel *widget;
	gchar *str_final;

	widget = GTK_LABEL (gtk_builder_get_object (builder, name));

	if (widget == NULL) {
		g_warning ("widget '%s' not found", name);
		return;
	}

	/* this way is probably better, but for some reason doesn't work with
	 * labels with mnemonics.

	static PangoAttrList *pattrlist = NULL;

	if (pattrlist == NULL) {
		PangoAttribute *attr;

		pattrlist = pango_attr_list_new ();
		attr = pango_attr_style_new (PANGO_STYLE_ITALIC);
		attr->start_index = 0;
		attr->end_index = G_MAXINT;
		pango_attr_list_insert (pattrlist, attr);
	}
	gtk_label_set_attributes (widget, pattrlist);*/

	str_final = g_strdup_printf ("<i>%s</i>", gtk_label_get_label (widget));
	gtk_label_set_markup_with_mnemonic (widget, str_final);
	g_free (str_final);
}

