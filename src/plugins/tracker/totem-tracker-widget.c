/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-
 *
 * The _get_result_count method taken from the tracker-client.h file from libtracker
 * Copyright (C) 2006, Mr Jamie McCracken (jamiemcc@gnome.org)
 * Copyright (C) 2007 Javier Goday <jgoday@gmail.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 *
 * Author : Javier Goday <jgoday@gmail.com>
 */

#include "config.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <libintl.h>
#include <tracker.h>
#include <tracker-client.h>
#include <glib/gi18n-lib.h>
#include <libgnomevfs/gnome-vfs.h>
#include <libgnomeui/gnome-thumbnail.h>

#include "totem-tracker-widget.h"
#include "totem-cell-renderer-video.h"
#include "totem-playlist.h"
#include "totem-video-list.h"


#define TRACKER_SERVICE			"org.freedesktop.Tracker"
#define TRACKER_OBJECT			"/org/freedesktop/tracker"
#define TRACKER_INTERFACE		"org.freedesktop.Tracker.Search"
#define TOTEM_TRACKER_MAX_RESULTS_SIZE	20

G_DEFINE_TYPE (TotemTrackerWidget, totem_tracker_widget, GTK_TYPE_EVENT_BOX)

struct TotemTrackerWidgetPrivate {
	GtkWidget *search_entry;
	GtkWidget *search_button;
	GtkWidget *status_label;

	GtkWidget *next_button;
	GtkWidget *previous_button;

	int total_result_count;
	int current_result_page;

	GtkListStore *result_store;
	TotemVideoList *result_list;
};

enum {
	IMAGE_COLUMN,
	FILE_COLUMN,
	NAME_COLUMN,
	N_COLUMNS
};

enum {
	PROP_0,
	PROP_TOTEM
};

static GObjectClass *parent_class = NULL;

static void totem_tracker_widget_class_init	(TotemTrackerWidgetClass *klass);
static void totem_tracker_widget_init		(TotemTrackerWidget	 *widget);
static void totem_tracker_widget_set_property	(GObject *object,
						 guint property_id,
						 const GValue *value,
						 GParamSpec *pspec);

static void totem_tracker_widget_class_init (TotemTrackerWidgetClass *klass)
{
	GObjectClass *object_class;
	GtkWidgetClass *widget_class;

	g_type_class_add_private (klass, sizeof (TotemTrackerWidgetPrivate));

	parent_class = g_type_class_peek_parent (klass);
	widget_class = GTK_WIDGET_CLASS (klass);

	object_class = G_OBJECT_CLASS (klass);
	object_class->set_property = totem_tracker_widget_set_property;

	g_object_class_install_property (object_class, PROP_TOTEM,
					 g_param_spec_object ("totem", NULL, NULL,
							      TOTEM_TYPE_OBJECT, G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
}

static void
totem_tracker_widget_set_property (GObject *object,
				   guint property_id,
				   const GValue *value,
				   GParamSpec *pspec)
{
	TotemTrackerWidget *widget;

	widget = TOTEM_TRACKER_WIDGET (object);

	switch (property_id)
	{
	case PROP_TOTEM:
		widget->totem = g_object_ref (g_value_get_object (value));
		g_object_set (G_OBJECT (widget->priv->result_list), "totem", widget->totem, NULL);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
	}
}

static void populate_result (TotemTrackerWidget *widget, char *result)
{
	GtkTreeIter iter;
	GnomeVFSFileInfo *info;
	GnomeVFSResult vfs_result;
	GdkPixbuf *thumbnail = NULL;
	char *thumbnail_path;
	char *file_uri;

	info = gnome_vfs_file_info_new ();
	vfs_result = gnome_vfs_get_file_info (result, info, GNOME_VFS_FILE_INFO_NAME_ONLY);

	if (vfs_result == GNOME_VFS_OK) {
		gtk_list_store_append (GTK_LIST_STORE (widget->priv->result_store), &iter);  /* Acquire an iterator */
		file_uri = gnome_vfs_get_uri_from_local_path (result);
		thumbnail_path = gnome_thumbnail_path_for_uri (file_uri, GNOME_THUMBNAIL_SIZE_NORMAL);

		if (thumbnail_path != NULL)
			thumbnail = gdk_pixbuf_new_from_file (thumbnail_path, NULL);

		gtk_list_store_set (GTK_LIST_STORE (widget->priv->result_store), &iter,
				    IMAGE_COLUMN, thumbnail,
				    FILE_COLUMN, file_uri,
				    NAME_COLUMN, info->name,
				    -1);

		g_free (thumbnail_path);
		g_free (file_uri);
	}
	else {
		/* Display an error */
		char *message = g_strdup_printf (_("Could not get metadata for file %s."), result);
		totem_interface_error_blocking	(_("File Error"), message, NULL);
		g_free (message);
	}

	g_free (info);
}

static int get_search_count (TrackerClient *client, const char *search)
{
	GError *error = NULL;
	int count = 0;

	dbus_g_proxy_call (client->proxy_search, "GetHitCount", &error, G_TYPE_STRING, "Videos", 
			   G_TYPE_STRING, search,
			   G_TYPE_INVALID, G_TYPE_INT, &count, G_TYPE_INVALID);

	if (error) {
		g_warning (error->message);
		g_error_free (error);
		return -1;
	}

	return count;
}

struct SearchResultsData {
	TotemTrackerWidget *widget;
	char *search_text;
	TrackerClient *client;
};

static void search_results_cb (char **result, GError *error, gpointer userdata)
{
	struct SearchResultsData *data = (struct SearchResultsData*) userdata;
	char *label;
	int i;
	int next_page;

	if (!error && result) {
		for (i = 0; result [i] != NULL; i++) {
			populate_result (data->widget, result [i]);
		}
	
		next_page = (data->widget->priv->current_result_page + 1) * TOTEM_TRACKER_MAX_RESULTS_SIZE;

		/* Translators:
		 * This is used to show which items are listed in the list view, for example:
		 * Showing 10-20 of 128 matches
		 * This is similar to what web searches use, eg. Google on the top-right of their search results page show:
		 * Personalized Results 1 - 10 of about 4,130,000 for foobar */
		label = g_strdup_printf (ngettext("Showing %i - %i of %i match", "Showing %i - %i of %i matches", data->widget->priv->total_result_count),
					 data->widget->priv->current_result_page * TOTEM_TRACKER_MAX_RESULTS_SIZE, 
					 next_page > data->widget->priv->total_result_count ? data->widget->priv->total_result_count : next_page,
					 data->widget->priv->total_result_count);
		gtk_label_set_text (GTK_LABEL(data->widget->priv->status_label), label);
		g_free (label);

		/* Enable or disable the pager buttons */
		if (data->widget->priv->current_result_page < data->widget->priv->total_result_count / TOTEM_TRACKER_MAX_RESULTS_SIZE)
			gtk_widget_set_sensitive (GTK_WIDGET (data->widget->priv->next_button), TRUE);

		if (data->widget->priv->current_result_page > 0)
			gtk_widget_set_sensitive (GTK_WIDGET (data->widget->priv->previous_button), TRUE);	
	} else {
		g_warning ("Error getting the search results for '%s': %s", data->search_text, error->message ? error->message : "No reason");
	}

	g_free (data->search_text);
	tracker_disconnect (data->client);
	g_free (data);
}

static void do_search (GtkWidget *button, TotemTrackerWidget *widget)
{
	TrackerClient *client;
	struct SearchResultsData *data;

	/* Clear the list store */
	gtk_list_store_clear (GTK_LIST_STORE (widget->priv->result_store));

	gtk_widget_set_sensitive (GTK_WIDGET (widget->priv->previous_button), FALSE);
	gtk_widget_set_sensitive (GTK_WIDGET (widget->priv->next_button), FALSE);


	/* Get the tracker client */
	client = tracker_connect (TRUE);
	if (!client) {
		g_warning ("Error trying to get the tracker client");
		return;
	}

	data = g_new0 (struct SearchResultsData, 1);
	data->widget = widget;
	data->client = client;

	/* Search text */
	data->search_text = g_strdup (gtk_entry_get_text (GTK_ENTRY (widget->priv->search_entry)));

	widget->priv->total_result_count = get_search_count (client, data->search_text);

	tracker_search_text_async (data->client, -1, SERVICE_VIDEOS, data->search_text, 
				   widget->priv->current_result_page * TOTEM_TRACKER_MAX_RESULTS_SIZE, 
				   TOTEM_TRACKER_MAX_RESULTS_SIZE, 
				   search_results_cb, data);
}

static void go_next (GtkWidget *button, TotemTrackerWidget *widget)
{
	if (widget->priv->current_result_page < widget->priv->total_result_count / TOTEM_TRACKER_MAX_RESULTS_SIZE)
		widget->priv->current_result_page ++;

	do_search (button, widget);
}

static void go_previous (GtkWidget *button, TotemTrackerWidget *widget)
{
	if (widget->priv->current_result_page > 0)
		widget->priv->current_result_page --;

	do_search (button, widget);
}

static void init_result_list (TotemTrackerWidget *widget)
{
	/* Initialize the store result list */
	widget->priv->result_store = gtk_list_store_new (N_COLUMNS, 
							 GDK_TYPE_PIXBUF,
							 G_TYPE_STRING,
							 G_TYPE_STRING);

	/* Create the gtktreewidget to show the results */
	widget->priv->result_list = g_object_new (TOTEM_TYPE_VIDEO_LIST,
						  "mrl-column", FILE_COLUMN,
						  "tooltip-column", NAME_COLUMN,
						  NULL);

	gtk_tree_view_set_model (GTK_TREE_VIEW (widget->priv->result_list), 
				 GTK_TREE_MODEL (widget->priv->result_store));
}

static void initialize_list_store (TotemTrackerWidget *widget) 
{
	TotemCellRendererVideo *renderer;
	GtkTreeViewColumn *column;

	/* Initialise the columns of the result list */
	renderer = totem_cell_renderer_video_new (TRUE);
	column = gtk_tree_view_column_new_with_attributes (_("Search results"), GTK_CELL_RENDERER (renderer),
							   "thumbnail", IMAGE_COLUMN, 
							   "title", NAME_COLUMN, 
							   NULL);

	gtk_tree_view_append_column (GTK_TREE_VIEW (widget->priv->result_list), column);
}

static void totem_tracker_widget_init (TotemTrackerWidget *widget)
{
	GtkWidget *v_box;		/* the main vertical box of the widget */
	GtkWidget *pager_box;		/* box that holds the next andd previous buttons */
	GtkWidget *search_box;		/* the search box contains the search entry and the search button */
	GtkScrolledWindow *scroll;	/* make the result list scrollable */

	widget->priv = G_TYPE_INSTANCE_GET_PRIVATE (widget, TOTEM_TYPE_TRACKER_WIDGET, TotemTrackerWidgetPrivate);

	init_result_list (widget);

	v_box = gtk_vbox_new (FALSE, 2);

	/* Search entry */
	widget->priv->search_entry = gtk_entry_new ();

	/* Search button */
	widget->priv->search_button = gtk_button_new_from_stock (GTK_STOCK_FIND);

	/* Add the search entry and button to the search box,
	   and add the search box to the main vertical box */
	search_box = gtk_hbox_new (FALSE, 2);	
	gtk_container_add (GTK_CONTAINER (search_box), widget->priv->search_entry);
	gtk_container_add (GTK_CONTAINER (search_box), widget->priv->search_button);	
	gtk_box_pack_start (GTK_BOX (v_box), search_box, FALSE, FALSE, 2);

	/* Insert the result list and initialize the viewport */
	scroll = GTK_SCROLLED_WINDOW (gtk_scrolled_window_new (NULL, NULL));
	gtk_scrolled_window_set_policy (scroll, GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_add_with_viewport (scroll, GTK_WIDGET (widget->priv->result_list));
	gtk_container_add (GTK_CONTAINER (v_box), GTK_WIDGET (scroll));

	/* Initialise the pager box */
	pager_box = gtk_hbox_new (FALSE, 2);
	widget->priv->next_button = gtk_button_new_from_stock (GTK_STOCK_GO_FORWARD);
	widget->priv->previous_button = gtk_button_new_from_stock (GTK_STOCK_GO_BACK);
	gtk_box_pack_start (GTK_BOX (pager_box), gtk_label_new (""), TRUE, TRUE, 2);
	gtk_box_pack_start (GTK_BOX (pager_box), widget->priv->previous_button, FALSE, FALSE, 2);
	gtk_box_pack_start (GTK_BOX (pager_box), widget->priv->next_button, FALSE, FALSE, 2);
	gtk_box_pack_start (GTK_BOX (v_box), pager_box, FALSE, FALSE, 2);

	gtk_widget_set_sensitive (GTK_WIDGET (widget->priv->previous_button), FALSE);
	gtk_widget_set_sensitive (GTK_WIDGET (widget->priv->next_button), FALSE);

	/* Status label */
	widget->priv->status_label = gtk_label_new ("");
	gtk_box_pack_start (GTK_BOX (v_box), widget->priv->status_label, FALSE, FALSE, 2);

	/* Add the main container to the widget */
	gtk_container_add (GTK_CONTAINER (widget), v_box);

	gtk_widget_show_all (GTK_WIDGET (widget));

	/* Connect the search button clicked signal and the search entry  */
	g_signal_connect (G_OBJECT (widget->priv->search_button), "clicked",
			  G_CALLBACK (do_search), (gpointer) widget);
	g_signal_connect (G_OBJECT (widget->priv->search_entry), "activate",
			  G_CALLBACK (do_search), (gpointer) widget);
	/* Connect the pager buttons */
	g_signal_connect (G_OBJECT (widget->priv->next_button), "clicked",
			  G_CALLBACK (go_next), (gpointer) widget);
	g_signal_connect (G_OBJECT (widget->priv->previous_button), "clicked",
			  G_CALLBACK (go_previous), (gpointer) widget);
}

GtkWidget *totem_tracker_widget_new (TotemObject *totem)
{
	GtkWidget *widget;

	widget = g_object_new (TOTEM_TYPE_TRACKER_WIDGET,
			       "totem", totem, NULL);

	/* Reset the info about the search */
	TOTEM_TRACKER_WIDGET (widget)->priv->total_result_count = 0;
	TOTEM_TRACKER_WIDGET (widget)->priv->current_result_page = 0;

	initialize_list_store (TOTEM_TRACKER_WIDGET (widget));

	return widget;
}

