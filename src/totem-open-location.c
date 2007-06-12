/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * Copyright (C) 2007 Philip Withnall <philip@tecnocode.co.uk>
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
 * The Totem project hereby grant permission for non-gpl compatible GStreamer
 * plugins to be used and distributed together with GStreamer and Totem. This
 * permission are above and beyond the permissions granted by the GPL license
 * Totem is covered by.
 *
 * Monday 7th February 2005: Christian Schaller: Add excemption clause.
 * See license_change file for details.
 *
 * Author: Bastien Nocera <hadess@hadess.net>, Philip Withnall <philip@tecnocode.co.uk>
 */

#include "config.h"

#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/stat.h>

#include "totem.h"
#include "totem-open-location.h"
#include "totem-interface.h"

static GObjectClass *parent_class = NULL;
static void totem_open_location_class_init	(TotemOpenLocationClass *class);
static void totem_open_location_init		(TotemOpenLocation *open_location);
static void totem_open_location_finalize	(GObject *object);

struct TotemOpenLocationPrivate
{
	GladeXML *xml;
	GtkWidget *uri_entry;
};

G_DEFINE_TYPE (TotemOpenLocation, totem_open_location, GTK_TYPE_DIALOG)

static void
totem_open_location_class_init (TotemOpenLocationClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);
	g_type_class_add_private (klass, sizeof (TotemOpenLocationPrivate));

	object_class->finalize = totem_open_location_finalize;
}

static void
totem_open_location_init (TotemOpenLocation *open_location)
{
	open_location->priv = G_TYPE_INSTANCE_GET_PRIVATE (open_location, TOTEM_TYPE_OPEN_LOCATION, TotemOpenLocationPrivate);
}

static void
totem_open_location_finalize (GObject *object)
{
	g_return_if_fail (object != NULL);

	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static gboolean
totem_open_location_match (GtkEntryCompletion *completion, const gchar *key, GtkTreeIter *iter, gpointer user_data)
{
	/* Substring-match key against URI */
	char *uri, *match;

	g_return_val_if_fail (key != NULL, FALSE);
	gtk_tree_model_get (user_data, iter, 0, &uri, -1);
	g_return_val_if_fail (uri != NULL, FALSE);
	match = strstr (uri, key);
	g_free (uri);

	return (match != NULL);
}

static gint
totem_compare_recent_stream_items (GtkRecentInfo *a, GtkRecentInfo *b)
{
	time_t time_a, time_b;

	time_a = gtk_recent_info_get_modified (a);
	time_b = gtk_recent_info_get_modified (b);

	return (time_b - time_a);
}

char *
totem_open_location_get_uri (TotemOpenLocation *open_location)
{
	char *uri;

	uri = g_strdup (gtk_entry_get_text (GTK_ENTRY (open_location->priv->uri_entry)));

	if (strcmp (uri, "") == 0)
		uri = NULL;

	if (uri != NULL && g_strrstr (uri, "://") == NULL)
	{
		char *tmp;
		tmp = g_strconcat ("http://", uri, NULL);
		g_free (uri);
		uri = tmp;
	}

	return uri;
}

static char *
totem_open_location_set_from_clipboard (TotemOpenLocation *open_location)
{
	GtkClipboard *clipboard;
	gchar *clipboard_content;

	/* Initialize the clipboard and get its content */
	clipboard = gtk_clipboard_get_for_display (gtk_widget_get_display (GTK_WIDGET (open_location)), GDK_SELECTION_CLIPBOARD);
	clipboard_content = gtk_clipboard_wait_for_text (clipboard);

	/* Check clipboard for "://". If it exists, return it */
	if (clipboard_content != NULL && strcmp (clipboard_content, "") != 0)
	{
		if (g_strrstr (clipboard_content, "://") != NULL)
			return clipboard_content;
	}

	g_free (clipboard_content);
	return NULL;
}

GtkWidget*
totem_open_location_new (Totem *totem)
{
	TotemOpenLocation *open_location;
	char *clipboard_location;
	GtkEntryCompletion *completion;
	GtkTreeModel *model;
	GList *recent_items, *streams_recent_items = NULL;
	GtkWidget *container;

	open_location = TOTEM_OPEN_LOCATION (g_object_new (TOTEM_TYPE_OPEN_LOCATION, NULL));

	open_location->priv->xml = totem_interface_load_with_root ("uri.glade", "open_uri_dialog_content",
				_("Open Location..."), FALSE, totem_get_main_window (totem));
	if (open_location->priv->xml == NULL)
	{
		totem_open_location_finalize (G_OBJECT (open_location));
		return NULL;
	}
	open_location->priv->uri_entry = glade_xml_get_widget (open_location->priv->xml, "uri");

	gtk_window_set_title (GTK_WINDOW (open_location), _("Open Location..."));
	gtk_dialog_set_has_separator (GTK_DIALOG (open_location), FALSE);
	gtk_dialog_add_buttons (GTK_DIALOG (open_location),
			GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
			GTK_STOCK_OPEN, GTK_RESPONSE_OK,
			NULL);
	gtk_dialog_set_default_response (GTK_DIALOG (open_location), GTK_RESPONSE_OK);

	g_signal_connect (G_OBJECT (open_location), "delete-event",
			G_CALLBACK (gtk_widget_destroy), open_location);

	/* Get item from clipboard to fill GtkEntry */
	clipboard_location = totem_open_location_set_from_clipboard (open_location);
	if (clipboard_location != NULL && strcmp (clipboard_location, "") != 0)
		gtk_entry_set_text (GTK_ENTRY (open_location->priv->uri_entry), clipboard_location);
	g_free (clipboard_location);

	/* Add items in Totem's GtkRecentManager to the URI GtkEntry's GtkEntryCompletion */
	completion = gtk_entry_completion_new();
	model = GTK_TREE_MODEL (gtk_list_store_new (1, G_TYPE_STRING));
	gtk_entry_set_completion (GTK_ENTRY (open_location->priv->uri_entry), completion);

	recent_items = gtk_recent_manager_get_items (gtk_recent_manager_get_for_screen (
				gtk_widget_get_screen (GTK_WIDGET (open_location))));

	if (recent_items != NULL)
	{
		GList *p;
		GtkTreeIter iter;

		/* Filter out non-Totem items */
		for (p = recent_items; p != NULL; p = p->next)
		{
			GtkRecentInfo *info = (GtkRecentInfo *) p->data;
			if (!gtk_recent_info_has_group (info, "TotemStreams")) {
				gtk_recent_info_unref (info);
				continue;
			}
			streams_recent_items = g_list_prepend (streams_recent_items, info);
		}

		streams_recent_items = g_list_sort (streams_recent_items, (GCompareFunc) totem_compare_recent_stream_items);

		/* Populate the list store for the combobox */
		for (p = streams_recent_items; p != NULL; p = p->next)
		{
			GtkRecentInfo *info = (GtkRecentInfo *) p->data;
			gtk_list_store_append (GTK_LIST_STORE (model), &iter);
			gtk_list_store_set (GTK_LIST_STORE (model), &iter, 0, gtk_recent_info_get_uri (info), -1);
			gtk_recent_info_unref (info);
		}

		g_list_free (streams_recent_items);
	}

	g_list_free (recent_items);

	gtk_entry_completion_set_model (completion, model);
	gtk_entry_completion_set_text_column (completion, 0);
	gtk_entry_completion_set_match_func (completion, (GtkEntryCompletionMatchFunc) totem_open_location_match, model, NULL);

	container = glade_xml_get_widget (open_location->priv->xml,
				"open_uri_dialog_content");
	gtk_box_pack_start (GTK_BOX (GTK_DIALOG (open_location)->vbox),
				container,
				TRUE,       /* expand */
				TRUE,       /* fill */
				0);         /* padding */

	gtk_widget_show_all (GTK_DIALOG (open_location)->vbox);

	return GTK_WIDGET (open_location);
}
