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
#include <glib/gi18n-lib.h>
#include <gio/gio.h>
#include <dbus/dbus.h>
#include <tracker.h>

#include "totem-tracker-widget.h"
#include "totem-cell-renderer-video.h"
#include "totem-playlist.h"
#include "totem-video-list.h"
#include "totem-interface.h"

#define TRACKER_SERVICE			"org.freedesktop.Tracker"
#define TRACKER_OBJECT			"/org/freedesktop/tracker"
#define TRACKER_INTERFACE		"org.freedesktop.Tracker.Search"
#define TOTEM_TRACKER_MAX_RESULTS_SIZE	20

G_DEFINE_TYPE (TotemTrackerWidget, totem_tracker_widget, GTK_TYPE_VBOX)

struct TotemTrackerWidgetPrivate {
	GtkWidget *search_entry;
	GtkWidget *search_button;
	GtkWidget *status_label;

	GtkWidget *next_button;
	GtkWidget *previous_button;
	GtkWidget *page_selector;

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

static void totem_tracker_widget_dispose	(GObject *object);
static void totem_tracker_widget_set_property	(GObject *object,
						 guint property_id,
						 const GValue *value,
						 GParamSpec *pspec);
static void page_selector_value_changed_cb (GtkSpinButton *self, TotemTrackerWidget *widget);

static void
totem_tracker_widget_class_init (TotemTrackerWidgetClass *klass)
{
	GObjectClass *object_class;

	g_type_class_add_private (klass, sizeof (TotemTrackerWidgetPrivate));

	object_class = G_OBJECT_CLASS (klass);
	object_class->dispose = totem_tracker_widget_dispose;
	object_class->set_property = totem_tracker_widget_set_property;

	g_object_class_install_property (object_class, PROP_TOTEM,
					 g_param_spec_object ("totem", NULL, NULL,
							      TOTEM_TYPE_OBJECT, G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
}

static void
totem_tracker_widget_dispose (GObject *object)
{
	TotemTrackerWidget *self = TOTEM_TRACKER_WIDGET (object);

	if (self->priv->result_store != NULL) {
		g_object_unref (self->priv->result_store);
		self->priv->result_store = NULL;
	}

	G_OBJECT_CLASS (totem_tracker_widget_parent_class)->dispose (object);
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
		widget->totem = g_value_dup_object (value);
		g_object_set (G_OBJECT (widget->priv->result_list), "totem", widget->totem, NULL);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
	}
}

static void
populate_result (TotemTrackerWidget *widget, char *result)
{
	GtkTreeIter iter;
	GFile *file;
	GFileInfo *info;
	GError *error = NULL;
	GdkPixbuf *thumbnail = NULL;
	const char *thumbnail_path;
	char *file_uri;

	file = g_file_new_for_path (result);
	info = g_file_query_info (file, "standard::display-name,thumbnail::path", G_FILE_QUERY_INFO_NONE, NULL, &error);

	if (error == NULL) {
		gtk_list_store_append (GTK_LIST_STORE (widget->priv->result_store), &iter);  /* Acquire an iterator */
		file_uri = g_file_get_uri (file);
		thumbnail_path = g_file_info_get_attribute_byte_string (info, G_FILE_ATTRIBUTE_THUMBNAIL_PATH);

		if (thumbnail_path != NULL)
			thumbnail = gdk_pixbuf_new_from_file (thumbnail_path, NULL);

		gtk_list_store_set (GTK_LIST_STORE (widget->priv->result_store), &iter,
				    IMAGE_COLUMN, thumbnail,
				    FILE_COLUMN, file_uri,
				    NAME_COLUMN, g_file_info_get_attribute_string (info, G_FILE_ATTRIBUTE_STANDARD_DISPLAY_NAME),
				    -1);

		g_free (file_uri);
		if (thumbnail != NULL)
			g_object_unref (thumbnail);
	} else {
		/* Display an error */
		char *message = g_strdup_printf (_("Could not get metadata for file %s: %s"), result, error->message);
		totem_interface_error_blocking	(_("File Error"), message, NULL);
		g_free (message);
		g_error_free (error);
	}

	g_object_unref (info);
	g_object_unref (file);
}

static int
get_search_count (TrackerClient *client, const char *search)
{
	GError *error = NULL;
	int count = 0;

	dbus_g_proxy_call (client->proxy_search, "GetHitCount", &error, G_TYPE_STRING, "Videos", 
			   G_TYPE_STRING, search,
			   G_TYPE_INVALID, G_TYPE_INT, &count, G_TYPE_INVALID);

	if (error) {
		g_warning ("%s", error->message);
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

static void
search_results_cb (char **result, GError *error, gpointer userdata)
{
	struct SearchResultsData *data = (struct SearchResultsData*) userdata;
	char *label;
	int i, next_page;
	TotemTrackerWidgetPrivate *priv = data->widget->priv;

	if (!error && result) {
		for (i = 0; result [i] != NULL; i++)
			populate_result (data->widget, result [i]);
	
		next_page = (priv->current_result_page + 1) * TOTEM_TRACKER_MAX_RESULTS_SIZE;

		/* Set the new range on the page selector's adjustment and ensure the current page is correct */
		gtk_spin_button_set_range (GTK_SPIN_BUTTON (priv->page_selector), 1,
					   priv->total_result_count / TOTEM_TRACKER_MAX_RESULTS_SIZE + 1);
		priv->current_result_page = gtk_spin_button_get_value (GTK_SPIN_BUTTON (priv->page_selector)) - 1;

		if (priv->total_result_count == 0) {
			gtk_label_set_text (GTK_LABEL (priv->status_label), _("No results"));
		} else {
			/* Translators:
			 * This is used to show which items are listed in the list view, for example:
			 * Showing 10-20 of 128 matches
			 * This is similar to what web searches use, eg. Google on the top-right of their search results page show:
			 * Personalized Results 1 - 10 of about 4,130,000 for foobar */
			label = g_strdup_printf (ngettext ("Showing %i - %i of %i match", "Showing %i - %i of %i matches", priv->total_result_count),
						 priv->current_result_page * TOTEM_TRACKER_MAX_RESULTS_SIZE, 
						 next_page > priv->total_result_count ? priv->total_result_count : next_page,
						 priv->total_result_count);
			gtk_label_set_text (GTK_LABEL (priv->status_label), label);
			g_free (label);
		}

		/* Enable or disable the pager buttons */
		if (priv->current_result_page < priv->total_result_count / TOTEM_TRACKER_MAX_RESULTS_SIZE) {
			gtk_widget_set_sensitive (GTK_WIDGET (priv->page_selector), TRUE);
			gtk_widget_set_sensitive (GTK_WIDGET (priv->next_button), TRUE);
		}

		if (priv->current_result_page > 0) {
			gtk_widget_set_sensitive (GTK_WIDGET (priv->page_selector), TRUE);
			gtk_widget_set_sensitive (GTK_WIDGET (priv->previous_button), TRUE);
		}

		g_signal_handlers_unblock_by_func (priv->page_selector, page_selector_value_changed_cb, data->widget);
	} else {
		g_warning ("Error getting the search results for '%s': %s", data->search_text, error->message ? error->message : "No reason");
	}

	g_free (data->search_text);
	tracker_disconnect (data->client);
	g_free (data);
}

static void
do_search (TotemTrackerWidget *widget)
{
	TrackerClient *client;
	struct SearchResultsData *data;

	/* Clear the list store */
	gtk_list_store_clear (GTK_LIST_STORE (widget->priv->result_store));

	/* Stop pagination temporarily */
	gtk_widget_set_sensitive (GTK_WIDGET (widget->priv->previous_button), FALSE);
	gtk_widget_set_sensitive (GTK_WIDGET (widget->priv->page_selector), FALSE);
	gtk_widget_set_sensitive (GTK_WIDGET (widget->priv->next_button), FALSE);

	/* Stop after clearing the list store if they're just emptying the search entry box */
	if (strcmp (gtk_entry_get_text (GTK_ENTRY (widget->priv->search_entry)), "") == 0) {
		gtk_label_set_text (GTK_LABEL (widget->priv->status_label), _("No results"));
		return;
	}

	g_signal_handlers_block_by_func (widget->priv->page_selector, page_selector_value_changed_cb, widget);

	/* Get the tracker client */
	client = tracker_connect (TRUE);
	if (!client) {
		g_warning ("Error trying to get the Tracker client.");
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

static void
go_next (GtkWidget *button, TotemTrackerWidget *widget)
{
	if (widget->priv->current_result_page < widget->priv->total_result_count / TOTEM_TRACKER_MAX_RESULTS_SIZE)
		gtk_spin_button_spin (GTK_SPIN_BUTTON (widget->priv->page_selector), GTK_SPIN_STEP_FORWARD, 1);

	/* do_search is called via page_selector_value_changed_cb */
}

static void
go_previous (GtkWidget *button, TotemTrackerWidget *widget)
{
	if (widget->priv->current_result_page > 0)
		gtk_spin_button_spin (GTK_SPIN_BUTTON (widget->priv->page_selector), GTK_SPIN_STEP_BACKWARD, 1);

	/* do_search is called via page_selector_value_changed_cb */
}

static void
page_selector_value_changed_cb (GtkSpinButton *self, TotemTrackerWidget *widget)
{
	widget->priv->current_result_page = gtk_spin_button_get_value (self) - 1;
	do_search (widget);
}

static void
new_search (GtkButton *button, TotemTrackerWidget *widget)
{
	/* Reset from the last search */
	g_signal_handlers_block_by_func (widget->priv->page_selector, page_selector_value_changed_cb, widget);
	gtk_spin_button_set_value (GTK_SPIN_BUTTON (widget->priv->page_selector), 1);
	g_signal_handlers_unblock_by_func (widget->priv->page_selector, page_selector_value_changed_cb, widget);

	do_search (widget);
}

static void
init_result_list (TotemTrackerWidget *widget)
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

static void
initialize_list_store (TotemTrackerWidget *widget) 
{
	TotemCellRendererVideo *renderer;
	GtkTreeViewColumn *column;

	/* Initialise the columns of the result list */
	renderer = totem_cell_renderer_video_new (TRUE);
	column = gtk_tree_view_column_new_with_attributes (_("Search Results"), GTK_CELL_RENDERER (renderer),
							   "thumbnail", IMAGE_COLUMN, 
							   "title", NAME_COLUMN, 
							   NULL);

	gtk_tree_view_append_column (GTK_TREE_VIEW (widget->priv->result_list), column);
}

static void
totem_tracker_widget_init (TotemTrackerWidget *widget)
{
	GtkWidget *v_box;		/* the main vertical box of the widget */
	GtkWidget *pager_box;		/* box that holds the next and previous buttons */
	GtkScrolledWindow *scroll;	/* make the result list scrollable */
	GtkAdjustment *adjust;		/* adjustment for the page selector spin button */

	widget->priv = G_TYPE_INSTANCE_GET_PRIVATE (widget, TOTEM_TYPE_TRACKER_WIDGET, TotemTrackerWidgetPrivate);

	init_result_list (widget);

	v_box = gtk_vbox_new (FALSE, 6);

	/* Search entry */
	widget->priv->search_entry = gtk_entry_new ();

	/* Search button */
	widget->priv->search_button = gtk_button_new_from_stock (GTK_STOCK_FIND);
	gtk_box_pack_start (GTK_BOX (v_box), widget->priv->search_entry, FALSE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (v_box), widget->priv->search_button, FALSE, TRUE, 0);

	/* Insert the result list and initialize the viewport */
	scroll = GTK_SCROLLED_WINDOW (gtk_scrolled_window_new (NULL, NULL));
	gtk_scrolled_window_set_policy (scroll, GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
	gtk_container_add (GTK_CONTAINER (scroll), GTK_WIDGET (widget->priv->result_list));
	gtk_container_add (GTK_CONTAINER (v_box), GTK_WIDGET (scroll));

	/* Initialise the pager box */
	pager_box = gtk_hbox_new (FALSE, 2);
	widget->priv->next_button = gtk_button_new_from_stock (GTK_STOCK_GO_FORWARD);
	widget->priv->previous_button = gtk_button_new_from_stock (GTK_STOCK_GO_BACK);
	adjust = GTK_ADJUSTMENT (gtk_adjustment_new (1, 1, 1, 1, 5, 0));
	widget->priv->page_selector = gtk_spin_button_new (adjust, 1, 0);
	gtk_box_pack_start (GTK_BOX (pager_box), widget->priv->previous_button, FALSE, FALSE, 2);
	gtk_box_pack_start (GTK_BOX (pager_box), gtk_label_new (_("Page")), FALSE, FALSE, 2);
	gtk_box_pack_start (GTK_BOX (pager_box), widget->priv->page_selector, TRUE, TRUE, 2);
	gtk_box_pack_start (GTK_BOX (pager_box), widget->priv->next_button, FALSE, FALSE, 2);
	gtk_box_pack_start (GTK_BOX (v_box), pager_box, FALSE, FALSE, 2);

	gtk_widget_set_sensitive (GTK_WIDGET (widget->priv->previous_button), FALSE);
	gtk_widget_set_sensitive (GTK_WIDGET (widget->priv->page_selector), FALSE);
	gtk_widget_set_sensitive (GTK_WIDGET (widget->priv->next_button), FALSE);

	/* Status label */
	widget->priv->status_label = gtk_label_new ("");
	gtk_box_pack_start (GTK_BOX (v_box), widget->priv->status_label, FALSE, FALSE, 2);

	/* Add the main container to the widget */
	gtk_container_add (GTK_CONTAINER (widget), v_box);

	gtk_widget_show_all (GTK_WIDGET (widget));

	/* Connect the search button clicked signal and the search entry  */
	g_signal_connect (widget->priv->search_button, "clicked",
			  G_CALLBACK (new_search), widget);
	g_signal_connect (widget->priv->search_entry, "activate",
			  G_CALLBACK (new_search), widget);
	/* Connect the pager buttons */
	g_signal_connect (widget->priv->next_button, "clicked",
			  G_CALLBACK (go_next), widget);
	g_signal_connect (widget->priv->previous_button, "clicked",
			  G_CALLBACK (go_previous), widget);
	g_signal_connect (widget->priv->page_selector, "value-changed",
			  G_CALLBACK (page_selector_value_changed_cb), widget);
}

GtkWidget *
totem_tracker_widget_new (TotemObject *totem)
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

