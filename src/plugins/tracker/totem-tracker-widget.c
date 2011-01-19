/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-
 *
 * The _get_result_count method taken from the tracker-client.h file from libtracker
 * Copyright (C) 2006, Jamie McCracken <jamiemcc@gnome.org>
 * Copyright (C) 2007, Javier Goday <jgoday@gmail.com>
 * Copyright (C) 2010, Martyn Russell <martyn@lanedo.com>
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
 * Author: Jamie McCracken <jamiemcc@gnome.org>
 *         Javier Goday <jgoday@gmail.com>
 *         Martyn Russell <martyn@lanedo.com>
 */

#include "config.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <libintl.h>

#include <glib/gi18n-lib.h>

#include <gio/gio.h>

#include <libtracker-sparql/tracker-sparql.h>

#include "totem-tracker-widget.h"
#include "totem-cell-renderer-video.h"
#include "totem-playlist.h"
#include "totem-video-list.h"
#include "totem-interface.h"

#define TOTEM_TRACKER_MAX_RESULTS_SIZE	20

G_DEFINE_TYPE (TotemTrackerWidget, totem_tracker_widget, GTK_TYPE_VBOX)

struct TotemTrackerWidgetPrivate {
	GtkWidget *search_entry;
	GtkWidget *search_button;
	GtkWidget *status_label;

	GtkWidget *next_button;
	GtkWidget *previous_button;
	GtkWidget *page_selector;

	guint total_result_count;
	guint current_result_page;

	GtkListStore *result_store;
	TotemVideoList *result_list;

	GSList *thumbnail_requests;
	GdkPixbuf *default_icon;
	gint default_icon_size;
};

typedef struct {
	TotemTrackerWidget *widget;
	TrackerSparqlConnection *connection;
	GCancellable *cancellable;
	gchar *search_text;
	gint offset;
} SearchResultsData;

typedef struct {
	TotemTrackerWidget *widget;
	GCancellable *cancellable;
	GtkTreeIter iter;
} ThumbnailData;

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

static void totem_tracker_widget_dispose      (GObject             *object);
static void totem_tracker_widget_set_property (GObject             *object,
                                               guint                property_id,
                                               const GValue        *value,
                                               GParamSpec          *pspec);
static void page_selector_value_changed_cb    (GtkSpinButton       *self,
                                               TotemTrackerWidget  *widget);
static void thumbnail_data_free               (ThumbnailData       *td);
static void search_get_hits_next              (SearchResultsData   *srd,
                                               TrackerSparqlCursor *cursor);
static void search_get_hits                   (SearchResultsData   *srd);
static void search_finish                     (SearchResultsData   *srd,
                                               GError              *error);

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

	if (self->priv->default_icon != NULL) {
		g_object_unref (self->priv->default_icon);
		self->priv->default_icon = NULL;
	}

	g_slist_foreach (self->priv->thumbnail_requests, (GFunc) thumbnail_data_free, NULL);
	g_slist_free (self->priv->thumbnail_requests);
	self->priv->thumbnail_requests = NULL;

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

static ThumbnailData *
thumbnail_data_new (TotemTrackerWidget *widget,
                    GtkTreeIter         iter)
{
	ThumbnailData *td;

	td = g_slice_new0 (ThumbnailData);

	td->widget = g_object_ref (widget);
	td->cancellable = g_cancellable_new ();
	td->iter = iter;

	return td;
}

static void
thumbnail_data_free (ThumbnailData *td)
{
	if (!td)
		return;

	if (td->cancellable) {
		g_cancellable_cancel (td->cancellable);
		g_object_unref (td->cancellable);
	}

	if (td->widget)
		g_object_unref (td->widget);

	g_slice_free (ThumbnailData, td);
}


static void
search_results_populate_cb (GObject      *source_object,
                            GAsyncResult *res,
                            gpointer      user_data)
{
	ThumbnailData *td;
	TotemTrackerWidget *widget;
	GFileInfo *info;
	GError *error = NULL;
	GdkPixbuf *thumbnail = NULL;

	td = user_data;
	widget = td->widget;

	info = g_file_query_info_finish (G_FILE (source_object),
	                                 res,
	                                 &error);

	if (error) {
		g_warning ("Call to g_file_query_info_async() failed for '%s': %s",
		           G_FILE_ATTRIBUTE_THUMBNAIL_PATH,
		           error->message);
		g_error_free (error);
	} else {
		const gchar *thumbnail_path;

		thumbnail_path = g_file_info_get_attribute_byte_string (info, G_FILE_ATTRIBUTE_THUMBNAIL_PATH);

		if (thumbnail_path != NULL)
			thumbnail = gdk_pixbuf_new_from_file (thumbnail_path, NULL);
	}

	gtk_list_store_set (GTK_LIST_STORE (widget->priv->result_store), &td->iter,
	                    IMAGE_COLUMN, thumbnail ? thumbnail : widget->priv->default_icon,
	                    -1);

	if (thumbnail)
		g_object_unref (thumbnail);

	if (info)
		g_object_unref (info);

	widget->priv->thumbnail_requests = g_slist_remove (widget->priv->thumbnail_requests, td);
	thumbnail_data_free (td);
}

static void
search_results_populate (SearchResultsData *srd,
                         const gchar       *uri,
                         const gchar       *title)
{
	TotemTrackerWidget *widget;
	ThumbnailData *td;
	GFile *file;
	GtkTreeIter iter;

	widget = srd->widget;

	gtk_list_store_append (GTK_LIST_STORE (widget->priv->result_store), &iter);  /* Acquire an iterator */
	gtk_list_store_set (GTK_LIST_STORE (widget->priv->result_store), &iter,
	                    FILE_COLUMN, uri,
	                    NAME_COLUMN, title,
	                    -1);

	td = thumbnail_data_new (widget, iter);
	widget->priv->thumbnail_requests = g_slist_prepend (widget->priv->thumbnail_requests, td);

	file = g_file_new_for_uri (uri);
	g_file_query_info_async (file,
	                         G_FILE_ATTRIBUTE_THUMBNAIL_PATH,
	                         G_FILE_QUERY_INFO_NONE,
	                         G_PRIORITY_DEFAULT,
	                         td->cancellable,
	                         search_results_populate_cb,
	                         td);
	g_object_unref (file);
}

static SearchResultsData *
search_results_new (TotemTrackerWidget *widget,
		    const gchar        *search_text)
{
	SearchResultsData *srd;
	TrackerSparqlConnection *connection;
	GCancellable *cancellable;
	GError *error = NULL;

	if (!widget) {
		return NULL;
	}

	cancellable = g_cancellable_new ();

	connection = tracker_sparql_connection_get (cancellable, &error);
	if (error) {
		g_warning ("Call to tracker_sparql_connection_get() failed: %s", error->message);
		g_object_unref (cancellable);
		g_error_free (error);
		return NULL;
	}

	srd = g_slice_new0 (SearchResultsData);

	srd->widget = g_object_ref (widget);
	srd->connection = connection;
	srd->cancellable = cancellable;
	srd->search_text = g_strdup (search_text);

	return srd;
}

static void
search_results_free (SearchResultsData *srd)
{
	if (!srd) {
		return;
	}

	if (srd->cancellable) {
		g_cancellable_cancel (srd->cancellable);
		g_object_unref (srd->cancellable);
	}

	if (srd->connection) {
		g_object_unref (srd->connection);
	}

	if (srd->widget) {
		g_object_unref (srd->widget);
	}

	g_free (srd->search_text);
	g_slice_free (SearchResultsData, srd);
}

static gchar *
get_fts_string (GStrv    search_words,
		gboolean use_or_operator)
{
	GString *fts;
	gint i, len;

	if (!search_words) {
		return NULL;
	}

	fts = g_string_new ("");
	len = g_strv_length (search_words);

	for (i = 0; i < len; i++) {
		g_string_append (fts, search_words[i]);
		g_string_append_c (fts, '*');

		if (i < len - 1) { 
			if (use_or_operator) {
				g_string_append (fts, " OR ");
			} else {
				g_string_append (fts, " ");
			}
		}
	}

	return g_string_free (fts, FALSE);
}

static void
search_get_hits_next_cb (GObject      *source_object,
                         GAsyncResult *res,
                         gpointer      user_data)
{
	TrackerSparqlCursor *cursor;
	SearchResultsData *srd = user_data;
	GError *error = NULL;
	gboolean more_results;

	cursor = TRACKER_SPARQL_CURSOR (source_object);

	more_results = tracker_sparql_cursor_next_finish (cursor,
	                                                  res,
	                                                  &error);
	if (error) {
		g_warning ("Call to tracker_sparql_cursor_next_finish() failed getting next hit: %s", error->message);
		g_object_unref (cursor);
		search_finish (srd, error);
		return;
	}

	if (!more_results) {
		/* got all results */
		g_object_unref (cursor);
		search_finish (srd, NULL);
	} else {
		const gchar *url, *title;

		url = tracker_sparql_cursor_get_string (cursor, 0, NULL);
		title = tracker_sparql_cursor_get_string (cursor, 1, NULL);

		g_message ("  Got hit:'%s' --> '%s'", url, title);
		search_results_populate (srd, url, title);

		/* Now continue with next row in the db */
		search_get_hits_next (srd, cursor);
	}
}

static void
search_get_hits_next (SearchResultsData   *srd,
                      TrackerSparqlCursor *cursor)
{
	tracker_sparql_cursor_next_async (cursor,
	                                  srd->cancellable,
	                                  search_get_hits_next_cb,
	                                  srd);
}

static void
search_get_hits_cb (GObject      *source_object,
                    GAsyncResult *res,
                    gpointer      user_data)
{
	TrackerSparqlCursor *cursor;
	SearchResultsData *srd = user_data;
	GError *error = NULL;

	cursor = tracker_sparql_connection_query_finish (TRACKER_SPARQL_CONNECTION (source_object),
	                                                 res,
	                                                 &error);
	if (error) {
		g_warning ("Call to tracker_sparql_connection_query_finish() failed getting hits: %s", error->message);
		g_object_unref (cursor);
		search_finish (srd, error);
		return;
	}

	search_get_hits_next (srd, cursor);
}

static void
search_get_hits (SearchResultsData *srd)
{
	gchar *fts, *query;

	if (srd->search_text && srd->search_text[0] != '\0') {
		GStrv strv;

		strv = g_strsplit (srd->search_text, " ", -1);
		fts = get_fts_string (strv, FALSE);
		g_strfreev (strv);
	} else {
		fts = NULL;
	}

	if (fts) {
		query = g_strdup_printf ("SELECT nie:url(?urn) nie:title(?urn) "
					 "WHERE {"
					 "  ?urn a nmm:Video ;"
					 "  fts:match \"%s\" ;"
					 "  tracker:available true . "
					 "} "
					 "ORDER BY DESC(fts:rank(?urn)) ASC(nie:url(?urn)) "
					 "OFFSET %d "
					 "LIMIT %d",
					 fts,
					 srd->offset,
					 TOTEM_TRACKER_MAX_RESULTS_SIZE);
	} else {
		query = g_strdup_printf ("SELECT nie:url(?urn) nie:title(?urn) "
					 "WHERE {"
					 "  ?urn a nmm:Video ; "
					 "  tracker:available true . "
					 "} "
					 "ORDER BY DESC(fts:rank(?urn)) ASC(nie:url(?urn)) "
					 "OFFSET %d "
					 "LIMIT %d",
					 srd->offset,
					 TOTEM_TRACKER_MAX_RESULTS_SIZE);
	}

	tracker_sparql_connection_query_async (srd->connection,
	                                       query,
	                                       srd->cancellable,
	                                       search_get_hits_cb,
	                                       srd);
	g_free (query);
	g_free (fts);
}

static void
search_get_count_cb (GObject      *source_object,
                     GAsyncResult *res,
                     gpointer      user_data)
{
	TrackerSparqlCursor *cursor;
	SearchResultsData *srd = user_data;
	GError *error = NULL;

	cursor = tracker_sparql_connection_query_finish (TRACKER_SPARQL_CONNECTION (source_object),
	                                                 res,
	                                                 &error);
	if (error) {
		g_warning ("Call to tracker_sparql_connection_query_finish() failed getting hit count: %s", error->message);
		g_object_unref (cursor);
		search_finish (srd, error);
		return;
	}

	srd->widget->priv->total_result_count = 0;

	if (cursor) {
		tracker_sparql_cursor_next (cursor, srd->cancellable, &error);

		if (error) {
			g_warning ("Call to tracker_sparql_cursor_next() failed getting hit count: %s", error->message);
			g_object_unref (cursor);
			search_finish (srd, error);
			return;
		}

		srd->widget->priv->total_result_count = tracker_sparql_cursor_get_integer (cursor, 0);
		g_object_unref (cursor);
	}

	g_message ("Got hit count:%d", srd->widget->priv->total_result_count);

	/* Now continue with next query */
	search_get_hits (srd);
}

static void
search_get_count (SearchResultsData *srd)
{
	gchar *fts, *query;

	if (srd->search_text && srd->search_text[0] != '\0') {
		GStrv strv;

		strv = g_strsplit (srd->search_text, " ", -1);
		fts = get_fts_string (strv, FALSE);
		g_strfreev (strv);
	} else {
		fts = NULL;
	}

	/* NOTE: We use FILTERS here for the type because we may want
	 * to show more items than just videos in the future - like
	 * music or some other specialised content.
	 */
	if (fts) {
		query = g_strdup_printf ("SELECT COUNT(?urn) "
					 "WHERE {"
					 "  ?urn a nmm:Video ;"
					 "  fts:match \"%s\" ;"
					 "  tracker:available true . "
					 "}",
					 fts);
	} else {
		query = g_strdup_printf ("SELECT COUNT(?urn) "
					 "WHERE {"
					 "  ?urn a nmm:Video ;"
					 "  tracker:available true . "
					 "}");
	}

	tracker_sparql_connection_query_async (srd->connection,
	                                       query,
	                                       srd->cancellable,
	                                       search_get_count_cb,
	                                       srd);
	g_free (query);
	g_free (fts);
}

static void
search_finish (SearchResultsData *srd,
               GError            *error)
{
	TotemTrackerWidgetPrivate *priv = srd->widget->priv;
	gchar *str;
	guint next_page, total_pages;

	/* Always do this first so we can try again */
	gtk_widget_set_sensitive (priv->search_entry, TRUE);

	if (error) {
		gtk_label_set_text (GTK_LABEL (priv->status_label), _("Could not connect to Tracker"));
		/* gtk_list_store_clear (GTK_LIST_STORE (priv->result_store)); */
		g_error_free (error);
		return;
	}

	if (srd->widget->priv->total_result_count < 1) {
		gtk_label_set_text (GTK_LABEL (priv->status_label), _("No results"));
		gtk_widget_set_sensitive (GTK_WIDGET (priv->page_selector), FALSE);
		gtk_widget_set_sensitive (GTK_WIDGET (priv->next_button), FALSE);
	} else {
		next_page = (priv->current_result_page + 1) * TOTEM_TRACKER_MAX_RESULTS_SIZE;
		total_pages = priv->total_result_count / TOTEM_TRACKER_MAX_RESULTS_SIZE + 1;

		/* Set the new range on the page selector's adjustment and ensure the current page is correct */
		gtk_spin_button_set_range (GTK_SPIN_BUTTON (priv->page_selector), 1, total_pages);
		priv->current_result_page = gtk_spin_button_get_value (GTK_SPIN_BUTTON (priv->page_selector)) - 1;

		/* Translators:
		 * This is used to show which items are listed in the
		 * list view, for example:
		 *
		 *   Showing 10-20 of 128 matches
		 *
		 * This is similar to what web searches use, eg.
		 * Google on the top-right of their search results
		 * page show:
		 *
		 *   Personalized Results 1 - 10 of about 4,130,000 for foobar
		 *
		 */
		str = g_strdup_printf (ngettext ("Showing %i - %i of %i match", "Showing %i - %i of %i matches",
		                                 priv->total_result_count),
		                       priv->current_result_page * TOTEM_TRACKER_MAX_RESULTS_SIZE + 1,
		                       next_page > priv->total_result_count ? priv->total_result_count : next_page,
		                       priv->total_result_count);
		gtk_label_set_text (GTK_LABEL (priv->status_label), str);
		g_free (str);

		/* Enable or disable the pager buttons */
		if (priv->current_result_page < priv->total_result_count / TOTEM_TRACKER_MAX_RESULTS_SIZE) {
			gtk_widget_set_sensitive (GTK_WIDGET (priv->page_selector), TRUE);
			gtk_widget_set_sensitive (GTK_WIDGET (priv->next_button), TRUE);
		}

		if (priv->current_result_page > 0) {
			gtk_widget_set_sensitive (GTK_WIDGET (priv->page_selector), TRUE);
			gtk_widget_set_sensitive (GTK_WIDGET (priv->previous_button), TRUE);
		} else {
			gtk_widget_set_sensitive (GTK_WIDGET (priv->previous_button), FALSE);
		}

		if (priv->current_result_page >= total_pages - 1) {
			gtk_widget_set_sensitive (GTK_WIDGET (priv->next_button), FALSE);
		}
	}

	/* Clean up */
	g_signal_handlers_unblock_by_func (priv->page_selector,
	                                   page_selector_value_changed_cb,
	                                   srd->widget);
	search_results_free (srd);
}

static void
search_start (TotemTrackerWidget *widget)
{
	SearchResultsData *srd;
	const gchar *search_text;

	/* Cancel previous searches */
	/* tracker_cancel_call (widget->priv->cookie_id); */

	/* Clear the list store */
	gtk_list_store_clear (GTK_LIST_STORE (widget->priv->result_store));

	/* Stop pagination temporarily */
	gtk_widget_set_sensitive (GTK_WIDGET (widget->priv->previous_button), FALSE);
	gtk_widget_set_sensitive (GTK_WIDGET (widget->priv->page_selector), FALSE);
	gtk_widget_set_sensitive (GTK_WIDGET (widget->priv->next_button), FALSE);

	/* Stop after clearing the list store if they're just emptying the search entry box */
	search_text = gtk_entry_get_text (GTK_ENTRY (widget->priv->search_entry));

	g_signal_handlers_block_by_func (widget->priv->page_selector, page_selector_value_changed_cb, widget);

	/* Get the tracker connection and data set up */
	srd = search_results_new (widget, search_text);
	if (!srd) {
		gtk_label_set_text (GTK_LABEL (widget->priv->status_label), _("Could not connect to Tracker"));
		return;
	}

	gtk_widget_set_sensitive (widget->priv->search_entry, FALSE);

	srd->offset = widget->priv->current_result_page * TOTEM_TRACKER_MAX_RESULTS_SIZE;

	/* This is how things proceed (everything is done async):
	 * 1. Get the count (but only for the first time)
	 * 2. Get the cursor using the criteria.
	 * 3. Call the cursor_next() as many times as we have results.
	 * 4. Clean up the search UI.
	 */
	if (widget->priv->current_result_page < 1) {
		search_get_count (srd);
	} else {
		search_get_hits (srd);
	}
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

	search_start (widget);
}

static void
new_search (GtkButton *button, TotemTrackerWidget *widget)
{
	/* Reset from the last search */
	g_signal_handlers_block_by_func (widget->priv->page_selector, page_selector_value_changed_cb, widget);
	gtk_spin_button_set_value (GTK_SPIN_BUTTON (widget->priv->page_selector), 1);
	g_signal_handlers_unblock_by_func (widget->priv->page_selector, page_selector_value_changed_cb, widget);

	search_start (widget);
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
search_entry_changed_cb (GtkWidget *entry,
			 gpointer   user_data)
{
	TotemTrackerWidget *widget = user_data;

	gtk_label_set_text (GTK_LABEL (widget->priv->status_label), "");
}

static void
totem_tracker_widget_init (TotemTrackerWidget *widget)
{
	GtkWidget *v_box;		/* the main vertical box of the widget */
	GtkWidget *h_box;
	GtkWidget *pager_box;		/* box that holds the next and previous buttons */
	GtkScrolledWindow *scroll;	/* make the result list scrollable */
	GtkAdjustment *adjust;		/* adjustment for the page selector spin button */
	GdkScreen *screen;
	GtkIconTheme *theme;

	widget->priv = G_TYPE_INSTANCE_GET_PRIVATE (widget, TOTEM_TYPE_TRACKER_WIDGET, TotemTrackerWidgetPrivate);

	init_result_list (widget);

	v_box = gtk_vbox_new (FALSE, 6);
	h_box = gtk_hbox_new (FALSE, 6);

	/* Search entry */
	widget->priv->search_entry = gtk_entry_new ();

	/* Search button */
	widget->priv->search_button = gtk_button_new_from_stock (GTK_STOCK_FIND);
	gtk_box_pack_start (GTK_BOX (h_box), widget->priv->search_entry, TRUE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (h_box), widget->priv->search_button, FALSE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (v_box), h_box, FALSE, TRUE, 0);

	/* Insert the result list and initialize the viewport */
	scroll = GTK_SCROLLED_WINDOW (gtk_scrolled_window_new (NULL, NULL));
	gtk_scrolled_window_set_policy (scroll, GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
	gtk_container_add (GTK_CONTAINER (scroll), GTK_WIDGET (widget->priv->result_list));
	gtk_container_add (GTK_CONTAINER (v_box), GTK_WIDGET (scroll));

	/* Status label */
	widget->priv->status_label = gtk_label_new ("");
	gtk_misc_set_alignment (GTK_MISC (widget->priv->status_label), 0.0, 0.5);
	gtk_box_pack_start (GTK_BOX (v_box), widget->priv->status_label, FALSE, TRUE, 2);

	/* Initialise the pager box */
	pager_box = gtk_hbox_new (FALSE, 6);
	widget->priv->next_button = gtk_button_new_from_stock (GTK_STOCK_GO_FORWARD);
	widget->priv->previous_button = gtk_button_new_from_stock (GTK_STOCK_GO_BACK);
	adjust = GTK_ADJUSTMENT (gtk_adjustment_new (1, 1, 1, 1, 5, 0));
	widget->priv->page_selector = gtk_spin_button_new (adjust, 1, 0);
	gtk_box_pack_start (GTK_BOX (pager_box), widget->priv->previous_button, TRUE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (pager_box), gtk_label_new (_("Page")), TRUE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (pager_box), widget->priv->page_selector, TRUE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (pager_box), widget->priv->next_button, TRUE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (v_box), pager_box, FALSE, TRUE, 0);

	gtk_widget_set_sensitive (GTK_WIDGET (widget->priv->previous_button), FALSE);
	gtk_widget_set_sensitive (GTK_WIDGET (widget->priv->page_selector), FALSE);
	gtk_widget_set_sensitive (GTK_WIDGET (widget->priv->next_button), FALSE);

	/* Add the main container to the widget */
	gtk_container_add (GTK_CONTAINER (widget), v_box);

	gtk_widget_show_all (GTK_WIDGET (widget));

	/* Get default pixbuf for movies with no thumbnail available yet */
	if (!gtk_icon_size_lookup (GTK_ICON_SIZE_DIALOG,
	                           &widget->priv->default_icon_size,
	                           NULL)) {
		/* default to this if all else fails */
		widget->priv->default_icon_size = 64;
	}

	screen = gdk_display_get_default_screen (gdk_display_get_default());
	theme = gtk_icon_theme_get_for_screen (screen);
	widget->priv->default_icon = gtk_icon_theme_load_icon (theme,
	                                                       "video-x-generic",
	                                                       widget->priv->default_icon_size,
	                                                       GTK_ICON_LOOKUP_USE_BUILTIN,
	                                                       NULL);

	/* Connect the search button clicked signal and the search entry  */
	g_signal_connect (widget->priv->search_entry, "changed", 
			  G_CALLBACK (search_entry_changed_cb), widget);
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

