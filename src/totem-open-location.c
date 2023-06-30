/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2007 Philip Withnall <philip@tecnocode.co.uk>
 *
 * SPDX-License-Identifier: GPL-3-or-later
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

struct _TotemOpenLocation {
	GtkDialog parent;
	GtkEntry *uri_entry;
};

G_DEFINE_TYPE (TotemOpenLocation, totem_open_location, GTK_TYPE_DIALOG)

/* GtkBuilder callbacks */
G_MODULE_EXPORT void uri_entry_changed_cb (GtkEditable *entry, GtkDialog *dialog);

static void
totem_open_location_class_init (TotemOpenLocationClass *klass)
{
	GtkWidgetClass *widget_class = (GtkWidgetClass *) klass;

	gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/totem/ui/uri.ui");
	gtk_widget_class_bind_template_child (widget_class, TotemOpenLocation, uri_entry);
}

static gboolean
totem_open_location_match (GtkEntryCompletion *completion, const gchar *key, GtkTreeIter *iter, gpointer user_data)
{
	/* Substring-match key against URI */
	char *uri, *match;

	g_return_val_if_fail (GTK_IS_TREE_MODEL (user_data), FALSE);
	g_return_val_if_fail (iter != NULL, FALSE);

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

	g_return_val_if_fail (TOTEM_IS_OPEN_LOCATION (open_location), NULL);

	uri = g_strdup (gtk_entry_get_text (open_location->uri_entry));

	if (*uri == '\0')
		g_clear_pointer (&uri, g_free);

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

	g_return_val_if_fail (TOTEM_IS_OPEN_LOCATION (open_location), NULL);

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

void
uri_entry_changed_cb (GtkEditable *entry, GtkDialog *dialog)
{
	gboolean sensitive = (gtk_entry_get_text_length (GTK_ENTRY (entry)) > 0);
	gtk_dialog_set_response_sensitive (dialog, GTK_RESPONSE_OK, sensitive);
}

GtkWidget *
totem_open_location_new (void)
{
	return GTK_WIDGET (g_object_new (TOTEM_TYPE_OPEN_LOCATION,
					 "use-header-bar", 1,
					 NULL));
}

static void
totem_open_location_init (TotemOpenLocation *open_location)
{
	char *clipboard_location;
	GtkEntryCompletion *completion;
	GtkTreeModel *model;
	GList *recent_items, *streams_recent_items = NULL;

	gtk_widget_init_template (GTK_WIDGET (open_location));
	gtk_dialog_set_response_sensitive (GTK_DIALOG (open_location), GTK_RESPONSE_OK, FALSE);

	/* Get item from clipboard to fill GtkEntry */
	clipboard_location = totem_open_location_set_from_clipboard (open_location);
	if (clipboard_location != NULL && strcmp (clipboard_location, "") != 0)
		gtk_entry_set_text (open_location->uri_entry, clipboard_location);
	g_free (clipboard_location);

	/* Add items in Totem's GtkRecentManager to the URI GtkEntry's GtkEntryCompletion */
	completion = gtk_entry_completion_new();
	model = GTK_TREE_MODEL (gtk_list_store_new (1, G_TYPE_STRING));
	gtk_entry_set_completion (open_location->uri_entry, completion);

	recent_items = gtk_recent_manager_get_items (gtk_recent_manager_get_default ());

	if (recent_items != NULL) {
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
}
