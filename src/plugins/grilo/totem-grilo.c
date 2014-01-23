/* -*- Mode: C; indent-tabs-mode: t -*- */

/*
 * Copyright (C) 2010, 2011 Igalia S.L. <info@igalia.com>
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
 * The Totem project hereby grant permission for non-GPL compatible GStreamer
 * plugins to be used and distributed together with GStreamer and Totem. This
 * permission are above and beyond the permissions granted by the GPL license
 * Totem is covered by.
 *
 * See license_change file for details.
 */

#include "config.h"
#include "icon-helpers.h"

#include <time.h>

#include <glib.h>
#include <glib-object.h>
#include <glib/gi18n-lib.h>
#include <grilo.h>

#include <libpeas/peas-extension-base.h>
#include <libpeas/peas-object-module.h>
#include <libpeas/peas-activatable.h>
#include <libpeas-gtk/peas-gtk-configurable.h>

#include <totem-plugin.h>
#include <totem-interface.h>
#include <totem-dirs.h>
#include <totem.h>
#include <totem-private.h>

#include <totem-time-helpers.h>
#include <totem-rtl-helpers.h>

#include "totem-search-entry.h"
#include "totem-main-toolbar.h"
#include "totem-selection-toolbar.h"
#include <libgd/gd.h>

#define TOTEM_TYPE_GRILO_PLUGIN                                         \
	(totem_grilo_plugin_get_type ())
#define TOTEM_GRILO_PLUGIN(o)                                           \
	(G_TYPE_CHECK_INSTANCE_CAST ((o), TOTEM_TYPE_GRILO_PLUGIN, TotemGriloPlugin))
#define TOTEM_GRILO_PLUGIN_CLASS(k)                                     \
	(G_TYPE_CHECK_CLASS_CAST((k), TOTEM_TYPE_GRILO_PLUGIN, TotemGriloPluginClass))
#define TOTEM_IS_GRILO_PLUGIN(o)                                        \
	(G_TYPE_CHECK_INSTANCE_TYPE ((o), TOTEM_TYPE_GRILO_PLUGIN))
#define TOTEM_IS_GRILO_PLUGIN_CLASS(k)                                  \
	(G_TYPE_CHECK_CLASS_TYPE ((k), TOTEM_TYPE_GRILO_PLUGIN))
#define TOTEM_GRILO_PLUGIN_GET_CLASS(o)                                 \
	(G_TYPE_INSTANCE_GET_CLASS ((o), TOTEM_TYPE_GRILO_PLUGIN, TotemGriloPluginClass))

#define BROWSE_FLAGS          (GRL_RESOLVE_FAST_ONLY | GRL_RESOLVE_IDLE_RELAY)
#define RESOLVE_FLAGS         (GRL_RESOLVE_FULL | GRL_RESOLVE_IDLE_RELAY)
#define PAGE_SIZE             50
#define SCROLL_GET_MORE_LIMIT 0.8

#define TOTEM_GRILO_CONFIG_FILE "totem-grilo.conf"

typedef struct {
	Totem *totem;
	GtkWindow *main_window;

	GrlSource *local_metadata_src;

	/* Current media selected in results*/
	GrlMedia *selected_media;

	/* Search related information */
	GrlSource *search_source;
	guint search_id;
	gint search_page;
	guint search_remaining;
	gchar *search_text;

	/* Toolbar widgets */
	GtkWidget *header;
	GSimpleAction *select_all_action;
	GSimpleAction *select_none_action;
	GtkWidget *switcher;
	GtkWidget *recent, *channels;

	/* Browser widgets */
	GtkWidget *browser;
	GtkTreeModel *recent_model;
	GtkTreeModel *recent_sort_model;
	GtkTreeModel *browser_model;
	GtkTreeModel *browser_filter_model;
	gboolean in_search;
	GList *browse_keys;
	GList *search_keys;

	/* Search widgets */
	GtkWidget *search_bar;
	GtkWidget *search_entry;
	GtkTreeModel *search_results_model;
	GHashTable *search_sources_ht;
	GtkWidget *search_sources_list;

	/* Selection toolbar */
	GtkWidget *selection_bar;
	GtkWidget *selection_revealer;

	/* Popup */
	GtkUIManager *ui_manager;
	GtkActionGroup *action_group;
} TotemGriloPluginPrivate;

TOTEM_PLUGIN_REGISTER (TOTEM_TYPE_GRILO_PLUGIN, TotemGriloPlugin, totem_grilo_plugin)

typedef struct {
	TotemGriloPlugin *totem_grilo;
	GtkTreeRowReference *ref_parent;
	GtkTreeModel *model;
} BrowseUserData;

typedef struct {
	TotemGriloPlugin *totem_grilo;
	GrlMedia *media;
	GrlSource *source;
	GtkTreeModel *model;
	GtkTreeRowReference *reference;
} SetThumbnailData;

typedef struct {
	gboolean found;
	GtkTreeIter *iter;
	GrlMedia *media;
} FindMediaData;

enum {
	SEARCH_MODEL_SOURCES_SOURCE = 0,
	SEARCH_MODEL_SOURCES_NAME,
};

enum {
	MODEL_RESULTS_SOURCE = GD_MAIN_COLUMN_LAST,
	MODEL_RESULTS_CONTENT,
	MODEL_RESULTS_IS_PRETHUMBNAIL,
	MODEL_RESULTS_PAGE,
	MODEL_RESULTS_REMAINING,
	MODEL_RESULTS_SORT_PRIORITY
};

static gchar *
get_secondary_text (GrlMedia *media)
{
	const char *artist;
	int duration;

	artist = grl_data_get_string (GRL_DATA (media), GRL_METADATA_KEY_ARTIST);
	if (artist != NULL)
		return g_strdup (artist);
	duration = grl_media_get_duration (media);
	if (duration > 0)
		return totem_time_to_string (duration * 1000, FALSE, FALSE);
	return NULL;
}

static void
get_thumbnail_cb (GObject *source_object,
		  GAsyncResult *res,
		  gpointer user_data)
{
	GtkTreeIter iter;
	SetThumbnailData *thumb_data = (SetThumbnailData *) user_data;
	GtkTreePath *path;
	GdkPixbuf *thumbnail;
	const GdkPixbuf *fallback_thumbnail;
	GtkTreeModel *view_model;
	GError *error = NULL;

	thumbnail = totem_grilo_get_thumbnail_finish (res, &error);
	if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
		goto out;

	path = gtk_tree_row_reference_get_path (thumb_data->reference);
	gtk_tree_model_get_iter (thumb_data->model, &iter, path);

	if (thumbnail == NULL) {
		if (thumb_data->media)
			fallback_thumbnail = totem_grilo_get_video_icon ();
		else
			fallback_thumbnail = totem_grilo_get_box_icon ();
	}

	gtk_tree_store_set (GTK_TREE_STORE (thumb_data->model),
			    &iter,
			    GD_MAIN_COLUMN_ICON, thumbnail ? thumbnail : fallback_thumbnail,
			    -1);
	g_clear_object (&thumbnail);

	/* Can we find that thumbnail in the view model? */
	view_model = gd_main_view_get_model (GD_MAIN_VIEW (thumb_data->totem_grilo->priv->browser));
	if (GTK_IS_TREE_MODEL_FILTER (view_model)) {
		GtkTreePath *parent_path;
		parent_path = gtk_tree_model_filter_convert_child_path_to_path (GTK_TREE_MODEL_FILTER (view_model), path);
		gtk_tree_path_free (path);
		path = parent_path;
	} else if (GTK_IS_TREE_MODEL_SORT (view_model)) {
		GtkTreePath *parent_path;
		parent_path = gtk_tree_model_sort_convert_child_path_to_path (GTK_TREE_MODEL_SORT (view_model), path);
		gtk_tree_path_free (path);
		path = parent_path;
	}

	if (path != NULL && gtk_tree_model_get_iter (view_model, &iter, path))
		gtk_tree_model_row_changed (view_model, path, &iter);
	g_clear_pointer (&path, gtk_tree_path_free);

out:
	g_clear_error (&error);

	/* Free thumb data */
	g_object_unref (thumb_data->totem_grilo);
	g_clear_object (&thumb_data->media);
	g_clear_object (&thumb_data->source);
	g_object_unref (thumb_data->model);
	gtk_tree_row_reference_free (thumb_data->reference);
	g_slice_free (SetThumbnailData, thumb_data);
}

static void
set_thumbnail_async (TotemGriloPlugin *self,
		     GrlMedia         *media,
		     GtkTreeModel     *model,
		     GtkTreePath      *path)
{
	SetThumbnailData *thumb_data;

	/* Let's read the thumbnail stream and set the thumbnail */
	thumb_data = g_slice_new0 (SetThumbnailData);
	thumb_data->totem_grilo = g_object_ref (self);
	thumb_data->media = g_object_ref (media);
	thumb_data->model = g_object_ref (model);
	thumb_data->reference = gtk_tree_row_reference_new (model, path);

	//FIXME cancellable?
	totem_grilo_get_thumbnail (G_OBJECT (media), NULL, get_thumbnail_cb, thumb_data);
}

static void
set_thumbnail_source_async (TotemGriloPlugin *self,
			    GrlSource        *source,
			    GtkTreeModel     *model,
			    GtkTreePath      *path)
{
	SetThumbnailData *thumb_data;

	/* Let's read the thumbnail stream and set the thumbnail */
	thumb_data = g_slice_new0 (SetThumbnailData);
	thumb_data->totem_grilo = g_object_ref (self);
	thumb_data->source = g_object_ref (source);
	thumb_data->model = g_object_ref (model);
	thumb_data->reference = gtk_tree_row_reference_new (model, path);

	//FIXME cancellable?
	totem_grilo_get_thumbnail (G_OBJECT (source), NULL, get_thumbnail_cb, thumb_data);
}

static gboolean
update_search_thumbnails_idle (TotemGriloPlugin *self)
{
	GtkTreePath *start_path;
	GtkTreePath *end_path;
	GrlSource *source;
	gboolean is_prethumbnail = FALSE;
	GtkTreeModel *view_model, *model;
	GtkIconView *icon_view;

	icon_view = GTK_ICON_VIEW (gd_main_view_get_generic_view (GD_MAIN_VIEW (self->priv->browser)));
	if (!gtk_icon_view_get_visible_range (icon_view, &start_path, &end_path)) {
		return FALSE;
	}

	view_model = gtk_icon_view_get_model (icon_view);
	if (GTK_IS_TREE_MODEL_FILTER (view_model))
		model = gtk_tree_model_filter_get_model (GTK_TREE_MODEL_FILTER (view_model));
	else if (GTK_IS_TREE_MODEL_SORT (view_model))
		model = gtk_tree_model_sort_get_model (GTK_TREE_MODEL_SORT (view_model));
	else
		model = view_model;

	for (;
	     gtk_tree_path_compare (start_path, end_path) <= 0;
	     gtk_tree_path_next (start_path)) {
		GtkTreePath *path;
		GtkTreeIter iter;
		GrlMedia *media;

		if (GTK_IS_TREE_MODEL_FILTER (view_model)) {
			path = gtk_tree_model_filter_convert_path_to_child_path (GTK_TREE_MODEL_FILTER (view_model),
										 start_path);
		} else if (GTK_IS_TREE_MODEL_SORT (view_model)) {
			path = gtk_tree_model_sort_convert_path_to_child_path (GTK_TREE_MODEL_SORT (view_model),
									       start_path);
		} else {
			path = gtk_tree_path_copy (start_path);
		}

		if (gtk_tree_model_get_iter (model, &iter, path) == FALSE) {
			gtk_tree_path_free (path);
			break;
		}

		media = NULL;
		gtk_tree_model_get (model,
		                    &iter,
		                    MODEL_RESULTS_CONTENT, &media,
		                    MODEL_RESULTS_SOURCE, &source,
		                    MODEL_RESULTS_IS_PRETHUMBNAIL, &is_prethumbnail,
		                    -1);
		if ((media != NULL || source != NULL) && is_prethumbnail) {
			if (media)
				set_thumbnail_async (self, media, model, path);
			else
				set_thumbnail_source_async (self, source, model, path);
			gtk_tree_store_set (GTK_TREE_STORE (model),
			                    &iter,
			                    MODEL_RESULTS_IS_PRETHUMBNAIL, FALSE,
			                    -1);
		}

		g_clear_object (&media);
		g_clear_object (&source);
	}
	gtk_tree_path_free (start_path);
	gtk_tree_path_free (end_path);

	return FALSE;
}

static void
update_search_thumbnails (TotemGriloPlugin *self)
{
	g_idle_add ((GSourceFunc) update_search_thumbnails_idle, self);
}

static void
update_media (GtkTreeStore *model,
	      GtkTreeIter  *iter,
	      GrlSource    *source,
	      GrlMedia     *media)
{
	GdkPixbuf *thumbnail;
	gboolean thumbnailing;
	char *secondary;
	GDateTime *mtime;

	thumbnail = totem_grilo_get_icon (media, &thumbnailing);
	secondary = get_secondary_text (media);
	mtime = grl_media_get_modification_date (media);

	gtk_tree_store_set (GTK_TREE_STORE (model), iter,
			    MODEL_RESULTS_SOURCE, source,
			    MODEL_RESULTS_CONTENT, media,
			    GD_MAIN_COLUMN_ICON, thumbnail,
			    MODEL_RESULTS_IS_PRETHUMBNAIL, thumbnailing,
			    GD_MAIN_COLUMN_PRIMARY_TEXT, grl_media_get_title (media),
			    GD_MAIN_COLUMN_SECONDARY_TEXT, secondary,
			    GD_MAIN_COLUMN_MTIME, mtime ? g_date_time_to_unix (mtime) : 0,
			    -1);

	g_clear_object (&thumbnail);
	g_free (secondary);
	g_clear_pointer (&mtime, g_date_time_unref);
}

static void
add_local_metadata (TotemGriloPlugin *self,
		    GrlMedia         *media)
{
	GrlOperationOptions *options;

	/* This is very slow and sync, so don't run it
	 * for non-local media */
	if (!g_str_equal (grl_media_get_source (media), "grl-tracker-source"))
		return;

	options = grl_operation_options_new (NULL);
	grl_operation_options_set_flags (options, GRL_RESOLVE_FULL);
	grl_source_resolve_sync (self->priv->local_metadata_src,
				 media,
				 self->priv->browse_keys,
				 options,
				 NULL);
	g_object_unref (options);
}

static int
get_source_priority (GrlSource *source)
{
	const char *id;

	if (source == NULL)
		return 0;

	id = grl_source_get_id (source);
	if (g_str_equal (id, "grl-optical-media"))
		return 100;
	if (g_str_equal (id, "grl-tracker-source"))
		return 50;
	return 0;
}

static void
add_media_to_model (GtkTreeStore *model,
		    GtkTreeIter  *parent,
		    GrlSource    *source,
		    GrlMedia     *media)
{
	GdkPixbuf *thumbnail;
	gboolean thumbnailing;
	char *secondary;
	GDateTime *mtime;
	int prio;

	thumbnail = totem_grilo_get_icon (media, &thumbnailing);
	secondary = get_secondary_text (media);
	mtime = grl_media_get_modification_date (media);
	prio = get_source_priority (source);

	gtk_tree_store_insert_with_values (GTK_TREE_STORE (model), NULL, parent, -1,
					   MODEL_RESULTS_SOURCE, source,
					   MODEL_RESULTS_CONTENT, media,
					   GD_MAIN_COLUMN_ICON, thumbnail,
					   MODEL_RESULTS_IS_PRETHUMBNAIL, thumbnailing,
					   GD_MAIN_COLUMN_PRIMARY_TEXT, grl_media_get_title (media),
					   GD_MAIN_COLUMN_SECONDARY_TEXT, secondary,
					   GD_MAIN_COLUMN_MTIME, mtime ? g_date_time_to_unix (mtime) : 0,
					   MODEL_RESULTS_SORT_PRIORITY, prio,
					   -1);

	g_clear_object (&thumbnail);
	g_free (secondary);
	g_clear_pointer (&mtime, g_date_time_unref);
}

static void
browse_cb (GrlSource *source,
           guint browse_id,
           GrlMedia *media,
           guint remaining,
           gpointer user_data,
           const GError *error)
{
	BrowseUserData *bud;
	TotemGriloPlugin *self;
	GtkTreeIter parent;
	GtkWindow *window;
	guint remaining_expected;

	bud = (BrowseUserData *) user_data;
	self = bud->totem_grilo;

	if (error != NULL &&
	    g_error_matches (error,
	                     GRL_CORE_ERROR,
	                     GRL_CORE_ERROR_OPERATION_CANCELLED) == FALSE) {
		window = totem_object_get_main_window (self->priv->totem);
		totem_interface_error (_("Browse Error"), error->message, window);
	}

	if (media != NULL) {
		if (bud->ref_parent) {
			GtkTreePath *path;

			path = gtk_tree_row_reference_get_path (bud->ref_parent);
			gtk_tree_model_get_iter (bud->model, &parent, path);
			gtk_tree_path_free (path);

			gtk_tree_model_get (bud->model, &parent,
					    MODEL_RESULTS_REMAINING, &remaining_expected,
					    -1);
			remaining_expected--;
			gtk_tree_store_set (GTK_TREE_STORE (bud->model), &parent,
					    MODEL_RESULTS_REMAINING, remaining_expected,
					    -1);
		}

		if (GRL_IS_MEDIA_IMAGE (media) ||
		    GRL_IS_MEDIA_AUDIO (media)) {
			/* This isn't supposed to happen as we filter for videos */
			g_assert_not_reached ();
		}

		add_local_metadata (self, media);
		add_media_to_model (GTK_TREE_STORE (bud->model),
				    bud->ref_parent ? &parent : NULL,
				    source, media);

		g_object_unref (media);
	}

	if (remaining == 0) {
		gtk_tree_row_reference_free (bud->ref_parent);
		g_object_unref (bud->totem_grilo);
		g_slice_free (BrowseUserData, bud);

		update_search_thumbnails (self);
	}
}

static void
browse (TotemGriloPlugin *self,
	GtkTreeModel     *model,
        GtkTreePath      *path,
        GrlSource        *source,
        GrlMedia         *container,
        gint              page)
{
	BrowseUserData *bud;
	GrlOperationOptions *default_options;
	GrlCaps *caps;

	g_return_if_fail (source != NULL);
	g_return_if_fail (page >= 1 || page == -1);

	caps = grl_source_get_caps (source, GRL_OP_BROWSE);

	default_options = grl_operation_options_new (NULL);
	grl_operation_options_set_flags (default_options, BROWSE_FLAGS);
	if (page >= 1) {
		grl_operation_options_set_skip (default_options, (page - 1) * PAGE_SIZE);
		grl_operation_options_set_count (default_options, PAGE_SIZE);
	}
	if (grl_caps_get_type_filter (caps) & GRL_TYPE_FILTER_VIDEO)
		grl_operation_options_set_type_filter (default_options, GRL_TYPE_FILTER_VIDEO);

	bud = g_slice_new0 (BrowseUserData);
	bud->totem_grilo = g_object_ref (self);
	if (path)
		bud->ref_parent = gtk_tree_row_reference_new (model, path);
	bud->model = g_object_ref (model);

	grl_source_browse (source,
			   container,
			   self->priv->browse_keys,
			   default_options,
			   browse_cb,
			   bud);

	g_object_unref (default_options);
}

static void
play (TotemGriloPlugin *self,
      GrlSource *source,
      GrlMedia *media,
      gboolean resolve_url)
{
	const gchar *url;

	url = grl_media_get_url (media);
	if (!url)
		url = grl_media_get_external_url (media);
	if (!url) {
		g_warning ("Cannot find URL for %s (source: %s), please file a bug at https://bugzilla.gnome.org/",
			   grl_media_get_id (media),
			   grl_media_get_source (media));
		return;
	}

	totem_object_clear_playlist (self->priv->totem);
	totem_object_add_to_playlist (self->priv->totem, url,
				      grl_media_get_title (media),
				      TRUE);
}

static void
search_cb (GrlSource *source,
           guint search_id,
           GrlMedia *media,
           guint remaining,
           gpointer user_data,
           const GError *error)
{
	GtkWindow *window;
	TotemGriloPlugin *self;

	self = TOTEM_GRILO_PLUGIN (user_data);

	if (error != NULL &&
	    g_error_matches (error,
	                     GRL_CORE_ERROR,
	                     GRL_CORE_ERROR_OPERATION_CANCELLED) == FALSE) {
		window = totem_object_get_main_window (self->priv->totem);
		totem_interface_error (_("Search Error"), error->message, window);
	}

	if (media != NULL) {
		self->priv->search_remaining--;

		if (GRL_IS_MEDIA_IMAGE (media) ||
		    GRL_IS_MEDIA_AUDIO (media)) {
			/* This isn't supposed to happen as we filter for videos */
			g_assert_not_reached ();
		}

		add_local_metadata (self, media);
		add_media_to_model (GTK_TREE_STORE (self->priv->search_results_model),
				    NULL, source, media);

		g_object_unref (media);
	}

	if (remaining == 0) {
		self->priv->search_id = 0;
		gtk_widget_set_sensitive (self->priv->search_entry, TRUE);
		update_search_thumbnails (self);
	}
}

static GrlOperationOptions *
get_search_options (TotemGriloPlugin *self)
{
	GrlOperationOptions *default_options;
	GrlOperationOptions *supported_options;

	default_options = grl_operation_options_new (NULL);
	grl_operation_options_set_flags (default_options, BROWSE_FLAGS);
	grl_operation_options_set_skip (default_options, self->priv->search_page * PAGE_SIZE);
	grl_operation_options_set_count (default_options, PAGE_SIZE);
	grl_operation_options_set_type_filter (default_options, GRL_TYPE_FILTER_VIDEO);

	/* And now remove all the unsupported filters and options */
	grl_operation_options_obey_caps (default_options,
					 grl_source_get_caps (GRL_SOURCE (self->priv->search_source), GRL_OP_SEARCH),
					 &supported_options,
					 NULL);
	g_object_unref (default_options);

	return supported_options;
}

static void
search_more (TotemGriloPlugin *self)
{
	GrlOperationOptions *search_options;

	search_options = get_search_options (self);

	gtk_widget_set_sensitive (self->priv->search_entry, FALSE);
	self->priv->search_page++;
	self->priv->search_remaining = PAGE_SIZE;
	if (self->priv->search_source != NULL) {
		self->priv->search_id =
			grl_source_search (self->priv->search_source,
			                   self->priv->search_text,
			                   self->priv->search_keys,
			                   search_options,
			                   search_cb,
			                   self);
	} else {
		self->priv->search_id =
			grl_multiple_search (NULL,
			                     self->priv->search_text,
			                     self->priv->search_keys,
			                     search_options,
			                     search_cb,
			                     self);
	}
	g_object_unref (search_options);

	if (self->priv->search_id == 0)
		search_cb (self->priv->search_source, 0, NULL, 0, self, NULL);
}

static void
search (TotemGriloPlugin *self, GrlSource *source, const gchar *text)
{
	gtk_tree_store_clear (GTK_TREE_STORE (self->priv->search_results_model));
//	g_hash_table_remove_all (self->priv->cache_thumbnails);
	gtk_widget_set_sensitive (self->priv->search_entry, FALSE);
	self->priv->search_source = source;
	g_free (self->priv->search_text);
	self->priv->search_text = g_strdup (text);
	self->priv->search_page = 0;
	gd_main_view_set_model (GD_MAIN_VIEW (self->priv->browser),
				self->priv->search_results_model);
	self->priv->browser_filter_model = NULL;
	search_more (self);
}

static void
search_entry_activate_cb (GtkEntry *entry, TotemGriloPlugin *self)
{
	GrlRegistry *registry;
	const char *id;
	const char *text;
	GrlSource *source;

	g_object_set (self->priv->header, "show-back-button", FALSE, NULL);

	id = totem_search_entry_get_selected_id (TOTEM_SEARCH_ENTRY (self->priv->search_entry));
	g_return_if_fail (id != NULL);
	registry = grl_registry_get_default ();
	source = grl_registry_lookup_source (registry, id);
	g_return_if_fail (source != NULL);

	text = totem_search_entry_get_text (TOTEM_SEARCH_ENTRY (self->priv->search_entry));
	g_return_if_fail (text != NULL);

	g_object_set (self->priv->header, "search-string", text, NULL);

	self->priv->in_search = TRUE;
	search (self, source, text);
}

static void
set_browser_filter_model_for_path (TotemGriloPlugin *self,
				   GtkTreePath      *path)
{
	g_clear_object (&self->priv->browser_filter_model);
	self->priv->browser_filter_model = gtk_tree_model_filter_new (self->priv->browser_model, path);

	gd_main_view_set_model (GD_MAIN_VIEW (self->priv->browser),
				self->priv->browser_filter_model);

	g_object_set (self->priv->header, "show-back-button", path != NULL, NULL);
	if (path == NULL) {
		totem_main_toolbar_set_custom_title (TOTEM_MAIN_TOOLBAR (self->priv->header), self->priv->switcher);
	} else {
		GtkTreeIter iter;

		totem_main_toolbar_set_custom_title (TOTEM_MAIN_TOOLBAR (self->priv->header), NULL);

		if (gtk_tree_model_get_iter (self->priv->browser_model, &iter, path)) {
			char *text;

			gtk_tree_model_get (self->priv->browser_model, &iter,
					    GD_MAIN_COLUMN_PRIMARY_TEXT, &text,
					    -1);
			totem_main_toolbar_set_title (TOTEM_MAIN_TOOLBAR (self->priv->header), text);
			g_free (text);
		}
	}
}

static void
browser_activated_cb (GdMainView  *view,
                      GtkTreePath *path,
                      gpointer user_data)
{
	guint remaining;
	gint page;
	GtkTreeModel *model;
	GtkTreeIter iter;
	GrlMedia *content;
	GrlSource *source;
	TotemGriloPlugin *self = TOTEM_GRILO_PLUGIN (user_data);
	GtkTreeIter real_model_iter;
	GtkTreePath *treepath;

	model = gd_main_view_get_model (GD_MAIN_VIEW (view));
	gtk_tree_model_get_iter (model, &iter, path);
	gtk_tree_model_get (model, &iter,
	                    MODEL_RESULTS_SOURCE, &source,
	                    MODEL_RESULTS_CONTENT, &content,
	                    MODEL_RESULTS_PAGE, &page,
	                    MODEL_RESULTS_REMAINING, &remaining,
	                    -1);

	/* Activate an item */
	if (content != NULL && GRL_IS_MEDIA_BOX (content) == FALSE) {
		play (self, source, content, TRUE);
		goto free_data;
	}

	/* Clicked on a container */
	gtk_tree_model_filter_convert_iter_to_child_iter (GTK_TREE_MODEL_FILTER (model),
							  &real_model_iter, &iter);

	treepath = gtk_tree_model_get_path (self->priv->browser_model, &real_model_iter);
	set_browser_filter_model_for_path (self, treepath);

	/* We need to fill the model with browse data */
	if (remaining == 0) {
		gtk_tree_store_set (GTK_TREE_STORE (self->priv->browser_model), &real_model_iter,
		                    MODEL_RESULTS_PAGE, ++page,
		                    MODEL_RESULTS_REMAINING, PAGE_SIZE,
		                    -1);
		browse (self, self->priv->browser_model, treepath, source, content, page);
	}
	gtk_tree_path_free (treepath);

free_data:
	g_clear_object (&source);
	g_clear_object (&content);
}

static void
search_entry_source_changed_cb (GObject          *object,
                                GParamSpec       *pspec,
                                TotemGriloPlugin *self)
{
	/* FIXME: Do we actually want to do that? */
	if (self->priv->search_id > 0) {
		grl_operation_cancel (self->priv->search_id);
		self->priv->search_id = 0;
	}
	gtk_tree_store_clear (GTK_TREE_STORE (self->priv->search_results_model));
}

static void
search_activated_cb (GdMainView  *view,
                     GtkTreePath *path,
                     gpointer user_data)
{
	GtkTreeModel *model;
	GtkTreeIter iter;
	GrlSource *source;
	GrlMedia *content;

	model = gd_main_view_get_model (view);
	gtk_tree_model_get_iter (model, &iter, path);
	gtk_tree_model_get (model, &iter,
	                    MODEL_RESULTS_SOURCE, &source,
	                    MODEL_RESULTS_CONTENT, &content,
	                    -1);

	play (TOTEM_GRILO_PLUGIN (user_data), source, content, TRUE);

	g_clear_object (&source);
	g_clear_object (&content);
}

static void
item_activated_cb (GdMainView  *view,
		   const char  *id,
		   GtkTreePath *path,
		   gpointer user_data)
{
	TotemGriloPlugin *self = TOTEM_GRILO_PLUGIN (user_data);

	if (self->priv->in_search)
		search_activated_cb (view, path, user_data);
	else
		browser_activated_cb (view, path, user_data);
}

static gboolean
source_is_blacklisted (GrlSource *source)
{
	const char *id;
	const char const *sources[] = {
		"grl-bookmarks",
		"grl-filesystem",
		"grl-shoutcast",
		"grl-flickr",
		"grl-metadata-store",
		"grl-podcasts",
		NULL
	};
	const char **s = sources;

	id = grl_source_get_id (source);
	g_assert (id);

	while (*s) {
		if (g_str_has_prefix (id, *s))
			return TRUE;
		s++;
	}

	return FALSE;
}

static gboolean
source_is_browse_blacklisted (GrlSource *source)
{
	const char *id;
	const char const *sources[] = {
		/* https://bugzilla.gnome.org/show_bug.cgi?id=722422 */
		"grl-youtube",
		NULL
	};
	const char **s = sources;

	id = grl_source_get_id (source);
	g_assert (id);

	while (*s) {
		if (g_str_has_prefix (id, *s))
			return TRUE;
		s++;
	}

	return FALSE;
}

static gboolean
find_media_cb (GtkTreeModel  *model,
	       GtkTreePath   *path,
	       GtkTreeIter   *iter,
	       FindMediaData *data)
{
	GrlMedia *media;

	gtk_tree_model_get (model, iter,
			    MODEL_RESULTS_CONTENT, &media,
			    -1);
	if (g_strcmp0 (grl_media_get_id (media), grl_media_get_id (data->media)) == 0) {
		g_object_unref (media);
		data->found = TRUE;
		data->iter = iter;
		return TRUE;
	}
	g_object_unref (media);
	return FALSE;
}

static gboolean
find_media (GtkTreeModel *model,
	    GrlMedia     *media,
	    GtkTreeIter  *iter)
{
	FindMediaData data;

	data.found = FALSE;
	data.media = media;
	gtk_tree_model_foreach (model, (GtkTreeModelForeachFunc) find_media_cb, &data);

	*iter = *(data.iter);

	return data.found;
}

static GtkTreeModel *
get_tree_model_for_source (TotemGriloPlugin *self,
			   GrlSource        *source)
{
	const char *id;

	id = grl_source_get_id (source);
	if (g_str_equal (id, "grl-tracker-source") ||
	    g_str_equal (id, "grl-optical-media")) {
		return self->priv->recent_model;
	}

	return self->priv->browser_model;
}

static void
content_changed (TotemGriloPlugin   *self,
		 GrlSource          *source,
		 GPtrArray          *changed_medias)
{
	GtkTreeModel *model;
	guint i;

	model = get_tree_model_for_source (self, source);

	for (i = 0; i < changed_medias->len; i++) {
		GrlMedia *media = changed_medias->pdata[i];
		GtkTreeIter iter;

		if (find_media (model, media, &iter))
			update_media (GTK_TREE_STORE (model), &iter, source, media);
	}
}

static void
content_removed (TotemGriloPlugin   *self,
		 GrlSource          *source,
		 GPtrArray          *changed_medias)
{
	GtkTreeModel *model;
	guint i;

	model = get_tree_model_for_source (self, source);

	for (i = 0; i < changed_medias->len; i++) {
		GrlMedia *media = changed_medias->pdata[i];
		GtkTreeIter iter;

		if (find_media (model, media, &iter))
			gtk_tree_store_remove (GTK_TREE_STORE (model), &iter);
	}
}

static void
content_added (TotemGriloPlugin   *self,
	       GrlSource          *source,
	       GPtrArray          *changed_medias)
{
	GtkTreeModel *model;
	guint i;

	model = get_tree_model_for_source (self, source);
	/* We're missing a container for the new media */
	if (model != self->priv->recent_model)
		return;

	for (i = 0; i < changed_medias->len; i++) {
		GrlMedia *media = changed_medias->pdata[i];

		add_local_metadata (self, media);
		add_media_to_model (GTK_TREE_STORE (model), NULL, source, media);
	}
}

static void
content_changed_cb (GrlSource          *source,
		    GPtrArray          *changed_medias,
		    GrlSourceChangeType change_type,
		    gboolean            location_unknown,
		    TotemGriloPlugin   *self)
{
	switch (change_type) {
	case GRL_CONTENT_CHANGED:
		content_changed (self, source, changed_medias);
		break;
	case GRL_CONTENT_ADDED:
		/* Added somewhere we don't know?
		 * We'll see it again when we browse away and back again */
		if (location_unknown)
			return;
		content_added (self, source, changed_medias);
		break;
	case GRL_CONTENT_REMOVED:
		content_removed (self, source, changed_medias);
		break;
	}
}

static void
source_added_cb (GrlRegistry *registry,
                 GrlSource *source,
                 gpointer user_data)
{
	const gchar *name;
	TotemGriloPlugin *self;
	GrlSupportedOps ops;
	const char *id;

	if (source_is_blacklisted (source) ||
	    !(grl_source_get_supported_media (source) & GRL_MEDIA_TYPE_VIDEO)) {
		grl_registry_unregister_source (registry,
		                                source,
		                                NULL);
		return;
	}

	self = TOTEM_GRILO_PLUGIN (user_data);
	id = grl_source_get_id (source);
	if (g_str_equal (id, "grl-tracker-source"))
		name = _("Local");
	else
		name = grl_source_get_name (source);
	if (g_str_equal (id, "grl-local-metadata"))
		self->priv->local_metadata_src = source;
	ops = grl_source_supported_operations (source);

	if (ops & GRL_OP_BROWSE) {
		gboolean monitor = FALSE;

		if (g_str_equal (id, "grl-tracker-source") ||
		    g_str_equal (id, "grl-optical-media")) {
			browse (self, self->priv->recent_model,
				NULL, source, NULL, -1);
			monitor = TRUE;
		} else if (!source_is_browse_blacklisted (source)) {
			const GdkPixbuf *icon;

			icon = totem_grilo_get_box_icon ();

			gtk_tree_store_insert_with_values (GTK_TREE_STORE (self->priv->browser_model),
							   NULL, NULL, -1,
							   MODEL_RESULTS_SOURCE, source,
							   MODEL_RESULTS_CONTENT, NULL,
							   GD_MAIN_COLUMN_PRIMARY_TEXT, name,
							   GD_MAIN_COLUMN_ICON, icon,
							   MODEL_RESULTS_IS_PRETHUMBNAIL, TRUE,
							   -1);
			monitor = TRUE;
		}

		if (monitor)
			g_signal_connect (G_OBJECT (source), "content-changed",
					  G_CALLBACK (content_changed_cb), self);
	}
	if (ops & GRL_OP_SEARCH) {
		totem_search_entry_add_source (TOTEM_SEARCH_ENTRY (self->priv->search_entry),
					       grl_source_get_id (source),
					       name,
					       0); /* FIXME: Use correct priority */
	}
}

static gboolean
remove_browse_result (GtkTreeModel *model,
                      GtkTreePath *path,
                      GtkTreeIter *iter,
                      gpointer user_data)
{
	GrlSource *removed_source = GRL_SOURCE (user_data);
	GrlSource *model_source;
	gboolean same_source;

	gtk_tree_model_get (model, iter,
	                    MODEL_RESULTS_SOURCE, &model_source,
	                    -1);

	same_source = (model_source == removed_source);

	if (same_source)
		gtk_tree_store_remove (GTK_TREE_STORE (model), iter);

	g_object_unref (model_source);

	return same_source;
}

static void
source_removed_cb (GrlRegistry *registry,
                   GrlSource *source,
                   gpointer user_data)
{
	GrlSupportedOps ops;
	TotemGriloPlugin *self = TOTEM_GRILO_PLUGIN (user_data);

	ops = grl_source_supported_operations (source);

	/* Remove source and content from browse results */
	if (ops & GRL_OP_BROWSE) {
		gtk_tree_model_foreach (self->priv->browser_model,
		                        remove_browse_result,
		                        source);
	}

	/* If current search results belongs to removed source, clear the results. In
	   any case, remove the source from the list of searchable sources */
	if (ops & GRL_OP_SEARCH) {
		const char *id;

		if (self->priv->search_source == source) {
			gtk_tree_store_clear (GTK_TREE_STORE (self->priv->search_results_model));
			self->priv->search_source = NULL;
		}

		id = grl_source_get_id (source);
		totem_search_entry_remove_source (TOTEM_SEARCH_ENTRY (self->priv->search_entry), id);
	}
}

static void
load_grilo_plugins (TotemGriloPlugin *self)
{
	GrlRegistry *registry;
	GError *error = NULL;

	registry = grl_registry_get_default ();

	g_signal_connect (registry, "source-added",
	                  G_CALLBACK (source_added_cb), self);
	g_signal_connect (registry, "source-removed",
	                  G_CALLBACK (source_removed_cb), self);

	if (grl_registry_load_all_plugins (registry, &error) == FALSE) {
		g_warning ("Failed to load grilo plugins: %s", error->message);
		g_error_free (error);
	}
}

static void
unload_grilo_plugins (TotemGriloPlugin *self)
{
	GrlRegistry *registry;
	GList *l, *plugins;

	registry = grl_registry_get_default ();
	plugins = grl_registry_get_plugins (registry, TRUE);

	for (l = plugins; l != NULL; l = l->next) {
		GrlPlugin *plugin = l->data;
		grl_registry_unload_plugin (registry, grl_plugin_get_id (plugin), NULL);
	}

	g_list_free (plugins);
}

static gboolean
adjustment_over_limit (GtkAdjustment *adjustment)
{
	if ((gtk_adjustment_get_value (adjustment) + gtk_adjustment_get_page_size (adjustment)) / gtk_adjustment_get_upper (adjustment) > SCROLL_GET_MORE_LIMIT) {
		return TRUE;
	} else {
		return FALSE;
	}
}

static void
adjustment_changed_cb (GtkAdjustment *adjustment,
                       TotemGriloPlugin *self)
{
	update_search_thumbnails (self);
}

static void
get_more_browse_results_cb (GtkAdjustment *adjustment,
                            TotemGriloPlugin *self)
{
	GtkTreeModel *model;
	GtkIconView *icon_view;
	GtkTreePath *start_path;
	GtkTreePath *end_path;
	GtkTreePath *parent_path;
	GtkTreeIter iter;
	GrlSource *source;
	GrlMedia *container;
	gint page;
	guint remaining;
	gboolean stop_processing = FALSE;

	if (adjustment_over_limit (adjustment) == FALSE)
		return;

	icon_view = GTK_ICON_VIEW (gd_main_view_get_generic_view (GD_MAIN_VIEW (self->priv->browser)));

	if (gtk_icon_view_get_visible_range (icon_view, &start_path, &end_path) == FALSE)
		return;

	model = gd_main_view_get_model (GD_MAIN_VIEW (self->priv->browser));
	if (model == self->priv->recent_sort_model)
		return;

	/* Start to check from last visible element, and check if its parent can get more elements */
	while (gtk_tree_path_compare (start_path, end_path) <= 0 &&
	       stop_processing == FALSE) {
		GtkTreeModel *real_model;
		GtkTreePath *path = NULL;

		real_model = gtk_tree_model_filter_get_model (GTK_TREE_MODEL_FILTER (model));
		path = gtk_tree_model_filter_convert_path_to_child_path (GTK_TREE_MODEL_FILTER (model), end_path);

		if (gtk_tree_path_get_depth (path) <= 1) {
			goto continue_next;
		}

		parent_path = gtk_tree_path_copy (path);
		if (gtk_tree_path_up (parent_path) == FALSE ||
		    gtk_tree_model_get_iter (real_model, &iter, parent_path) == FALSE) {
			gtk_tree_path_free (parent_path);
			goto continue_next;
		}

		gtk_tree_model_get (real_model,
		                    &iter,
		                    MODEL_RESULTS_SOURCE, &source,
		                    MODEL_RESULTS_CONTENT, &container,
		                    MODEL_RESULTS_PAGE, &page,
		                    MODEL_RESULTS_REMAINING, &remaining,
		                    -1);
		/* Skip non-boxes (they can not be browsed) */
		if (container != NULL &&
		    GRL_IS_MEDIA_BOX (container) == FALSE) {
			goto free_elements;
		}

		/* In case of containers, check that more elements can be obtained */
		if (remaining > 0) {
			goto free_elements;
		}

		/* Continue browsing */
		gtk_tree_store_set (GTK_TREE_STORE (self->priv->browser_model),
		                    &iter,
		                    MODEL_RESULTS_PAGE, ++page,
		                    MODEL_RESULTS_REMAINING, PAGE_SIZE,
		                    -1);
		browse (self, self->priv->browser_model, parent_path, source, container, page);
		stop_processing = TRUE;

	free_elements:
		g_clear_object (&source);
		g_clear_object (&container);
		g_clear_pointer (&parent_path, gtk_tree_path_free);

	continue_next:
		stop_processing = stop_processing || (gtk_tree_path_prev (end_path) == FALSE);
		g_clear_pointer (&path, gtk_tree_path_free);
	}

	gtk_tree_path_free (start_path);
	gtk_tree_path_free (end_path);
}

static void
adjustment_value_changed_cb (GtkAdjustment *adjustment,
                             TotemGriloPlugin *self)
{
	update_search_thumbnails (self);

	if (self->priv->in_search == FALSE) {
		get_more_browse_results_cb (adjustment, self);
		return;
	}

	/* Do not get more results if search is in progress */
	if (self->priv->search_id != 0)
		return;

	/* Do not get more results if there are no more results to get :) */
	if (self->priv->search_remaining > 0)
		return;

	if (adjustment_over_limit (adjustment))
		search_more (self);
}

static void
back_button_clicked_cb (TotemMainToolbar *toolbar,
			TotemGriloPlugin *self)
{
	GtkTreePath *path;
	GtkTreeIter iter;

	g_assert (self->priv->browser_filter_model);
	g_object_get (G_OBJECT (self->priv->browser_filter_model), "virtual-root", &path, NULL);
	g_assert (path);

	set_browser_filter_model_for_path (self, NULL);

	/* Remove all the items at that level */
	if (gtk_tree_model_get_iter (self->priv->browser_model, &iter, path)) {
		GtkTreeIter child;

		if (gtk_tree_model_iter_children (self->priv->browser_model, &child, &iter)) {
			while (gtk_tree_store_remove (GTK_TREE_STORE (self->priv->browser_model), &child))
				;
		}

		gtk_tree_store_set (GTK_TREE_STORE (self->priv->browser_model), &iter,
		                    MODEL_RESULTS_PAGE, 0,
		                    MODEL_RESULTS_REMAINING, 0,
		                    -1);
	}

	gtk_tree_path_up (path);
	if (gtk_tree_path_get_depth (path) > 0)
		set_browser_filter_model_for_path (self, path);
	gtk_tree_path_free (path);
}

static gboolean
window_key_press_event_cb (GtkWidget        *win,
			   GdkEvent         *event,
			   TotemGriloPlugin *self)
{
	/* Check whether we're in the browse panel */
	if (!g_str_equal (totem_object_get_main_page (self->priv->totem), "grilo"))
		return FALSE;

	return gtk_search_bar_handle_event (GTK_SEARCH_BAR (self->priv->search_bar), event);
}

static void
selection_mode_requested (GdMainView       *view,
			  TotemGriloPlugin *self)
{
	GtkTreePath *root = NULL;

	/* Don't allow selections when at the root of the
	 * "Channels" view */
	if (self->priv->browser_filter_model != NULL) {
		g_object_get (self->priv->browser_filter_model,
			      "virtual-root", &root,
			      NULL);
		if (root == NULL)
			return;
	}

	gd_main_view_set_selection_mode (GD_MAIN_VIEW (view), TRUE);
	g_clear_pointer (&root, gtk_tree_path_free);
}

static void
view_selection_changed_cb (GdMainView       *view,
			   TotemGriloPlugin *self)
{
	GList *list;
	guint count;

	list = gd_main_view_get_selection (view);
	count = g_list_length (list);
	g_list_free_full (list, (GDestroyNotify) gtk_tree_path_free);

	totem_main_toolbar_set_n_selected (TOTEM_MAIN_TOOLBAR (self->priv->header), count);
	totem_selection_toolbar_set_n_selected (TOTEM_SELECTION_TOOLBAR (self->priv->selection_bar), count);
}

static void
select_all_action_cb (GSimpleAction    *action,
		      GVariant         *parameter,
		      TotemGriloPlugin *self)
{
	gd_main_view_select_all (GD_MAIN_VIEW (self->priv->browser));
}

static void
select_none_action_cb (GSimpleAction    *action,
		       GVariant         *parameter,
		       TotemGriloPlugin *self)
{
	gd_main_view_unselect_all (GD_MAIN_VIEW (self->priv->browser));
}

static void
source_switched (GtkToggleButton  *button,
		 TotemGriloPlugin *self)
{
	const char *id;

	if (!gtk_toggle_button_get_active (button))
		return;

	id = g_object_get_data (G_OBJECT (button), "name");
	if (g_str_equal (id, "recent")) {
		gd_main_view_set_model (GD_MAIN_VIEW (self->priv->browser),
					self->priv->recent_sort_model);
	} else if (g_str_equal (id, "channels")) {
		if (self->priv->browser_filter_model != NULL)
			gd_main_view_set_model (GD_MAIN_VIEW (self->priv->browser),
						self->priv->browser_filter_model);
		else
			set_browser_filter_model_for_path (self, NULL);
	} else {
		g_assert_not_reached ();
	}
}

static GtkWidget *
create_switcher_button (TotemGriloPlugin *self,
			const char *label,
			const char *id)
{
	GtkStyleContext *context;
	GtkWidget *button;

	button = gtk_radio_button_new_with_label (NULL, label);
	gtk_toggle_button_set_mode (GTK_TOGGLE_BUTTON (button), FALSE);
	g_object_set_data_full (G_OBJECT (button), "name", g_strdup (id), g_free);
	g_signal_connect (G_OBJECT (button), "toggled",
			  G_CALLBACK (source_switched), self);

	context = gtk_widget_get_style_context (button);
	gtk_style_context_add_class (context, "text-button");

	return button;
}

static void
setup_source_switcher (TotemGriloPlugin *self)
{
	GtkStyleContext *context;

	self->priv->switcher = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);

	self->priv->recent = create_switcher_button (self, _("Recent"), "recent");
	gtk_container_add (GTK_CONTAINER (self->priv->switcher), self->priv->recent);

	self->priv->channels = create_switcher_button (self, _("Channels"), "channels");
	gtk_radio_button_join_group (GTK_RADIO_BUTTON (self->priv->channels),
				     GTK_RADIO_BUTTON (self->priv->recent));
	gtk_container_add (GTK_CONTAINER (self->priv->switcher), self->priv->channels);

	context = gtk_widget_get_style_context (self->priv->switcher);
	gtk_style_context_add_class (context, "stack-switcher");
	gtk_style_context_add_class (context, GTK_STYLE_CLASS_LINKED);

	gtk_widget_show_all (self->priv->switcher);
	g_object_ref_sink (self->priv->switcher);
}

static void
search_mode_changed (GObject          *gobject,
		     GParamSpec       *pspec,
		     TotemGriloPlugin *self)
{
	gboolean search_mode;

	search_mode = totem_main_toolbar_get_search_mode (TOTEM_MAIN_TOOLBAR (self->priv->header));
	if (!search_mode) {
		/* One of those will fail, as there's the toggle
		 * button won't be active */
		source_switched (GTK_TOGGLE_BUTTON (self->priv->recent), self);
		source_switched (GTK_TOGGLE_BUTTON (self->priv->channels), self);
	}

	self->priv->in_search = search_mode;
}

typedef struct {
	int random;
	GtkTreePath *path;
} RandomData;

static int
compare_random (gconstpointer ptr_a, gconstpointer ptr_b)
{
	RandomData *a = (RandomData *) ptr_a;
	RandomData *b = (RandomData *) ptr_b;

	if (a->random < b->random)
		return -1;
	else if (a->random > b->random)
		return 1;
	else
		return 0;
}

static GPtrArray *
shuffle_items (GList *list)
{
	GPtrArray *items;
	GList *l;
	GArray *array;
	RandomData data;
	guint len, i;

	len = g_list_length (list);

	items = g_ptr_array_new ();

	array = g_array_sized_new (FALSE, FALSE, sizeof (RandomData), len);
	for (l = list; l != NULL; l = l->next) {
		data.random = g_random_int_range (0, len);
		data.path = l->data;

		g_array_append_val (array, data);
	}
	g_array_sort (array, compare_random);

	for (i = 0; i < len; i++)
		g_ptr_array_add (items, g_array_index (array, RandomData, i).path);

	g_array_free (array, FALSE);

	return items;
}

static void
play_selection (TotemGriloPlugin *self,
		gboolean          shuffle)
{
	GtkTreeModel *model;
	GList *list;
	gboolean first = TRUE;
	GPtrArray *items;
	guint i;

	list = gd_main_view_get_selection (GD_MAIN_VIEW (self->priv->browser));
	model = gd_main_view_get_model (GD_MAIN_VIEW (self->priv->browser));

	/* Stuff the items in an array */
	if (shuffle) {
		items = shuffle_items (list);
	} else {
		GList *l;

		items = g_ptr_array_new ();
		for (l = list; l != NULL; l = l->next)
			g_ptr_array_add (items, l->data);
	}
	g_list_free (list);

	totem_object_clear_playlist (self->priv->totem);

	for (i = 0; i < items->len; i++) {
		GtkTreePath *path = items->pdata[i];
		GtkTreeIter iter;
		GrlMedia *media;
		const gchar *url;

		gtk_tree_model_get_iter (model, &iter, path);
		gtk_tree_model_get (model, &iter,
				    MODEL_RESULTS_CONTENT, &media,
				    -1);

		url = grl_media_get_url (media);
		if (!url)
			url = grl_media_get_external_url (media);
		if (!url) {
			g_warning ("Cannot find URL for %s (source: %s), please file a bug at https://bugzilla.gnome.org/",
				   grl_media_get_id (media),
				   grl_media_get_source (media));
			goto next_item;
		}

		totem_object_add_to_playlist (self->priv->totem, url,
					      grl_media_get_title (media),
					      first);
		first = FALSE;

next_item:
		g_clear_object (&media);
		gtk_tree_path_free (path);
	}

	g_ptr_array_free (items, FALSE);
}

static void
play_cb (TotemSelectionToolbar *bar,
	 TotemGriloPlugin      *self)
{
	play_selection (self, FALSE);
}

static void
shuffle_cb (TotemSelectionToolbar *bar,
	    TotemGriloPlugin      *self)
{
	play_selection (self, TRUE);
}

static void
delete_cb (TotemSelectionToolbar *bar,
	   TotemGriloPlugin      *self)
{
	/* FIXME: Call grl_source_remove(); */
}

static void
setup_browse (TotemGriloPlugin *self,
	      GtkBuilder *builder)
{
	GtkAdjustment *adj;

	/* Search */
	self->priv->search_bar = GTK_WIDGET (gtk_builder_get_object (builder, "gw_searchbar"));
	self->priv->search_results_model = GTK_TREE_MODEL (gtk_builder_get_object (builder, "gw_search_store_results"));
	self->priv->search_sources_list = GTK_WIDGET (gtk_builder_get_object (builder, "gw_search_select_source"));
	self->priv->search_entry =  GTK_WIDGET (gtk_builder_get_object (builder, "gw_search_text"));
	gtk_search_bar_connect_entry (GTK_SEARCH_BAR (self->priv->search_bar),
				      totem_search_entry_get_entry (TOTEM_SEARCH_ENTRY (self->priv->search_entry)));

	g_signal_connect (self->priv->main_window, "key-press-event",
			  G_CALLBACK (window_key_press_event_cb), self);
	g_signal_connect (self->priv->search_entry, "activate",
	                  G_CALLBACK (search_entry_activate_cb),
	                  self);
	//FIXME also setup a timeout for that
	g_signal_connect (self->priv->search_entry, "notify::selected-id",
			  G_CALLBACK (search_entry_source_changed_cb), self);

	/* Toolbar */
	self->priv->header = GTK_WIDGET (gtk_builder_get_object (builder, "gw_headerbar"));
	totem_main_toolbar_set_select_menu_model (TOTEM_MAIN_TOOLBAR (self->priv->header),
						  G_MENU_MODEL (gtk_builder_get_object (builder, "selectmenu")));

	self->priv->select_all_action = g_simple_action_new ("select-all", NULL);
	g_signal_connect (G_OBJECT (self->priv->select_all_action), "activate",
			  G_CALLBACK (select_all_action_cb), self);
	g_action_map_add_action (G_ACTION_MAP (self->priv->totem), G_ACTION (self->priv->select_all_action));
	gtk_application_add_accelerator (GTK_APPLICATION (self->priv->totem), "<Primary>A", "app.select-all", NULL);
	g_object_bind_property (self->priv->header, "select-mode",
				self->priv->select_all_action, "enabled",
				G_BINDING_SYNC_CREATE);

	self->priv->select_none_action = g_simple_action_new ("select-none", NULL);
	g_signal_connect (G_OBJECT (self->priv->select_none_action), "activate",
			  G_CALLBACK (select_none_action_cb), self);
	g_action_map_add_action (G_ACTION_MAP (self->priv->totem), G_ACTION (self->priv->select_none_action));
	g_object_bind_property (self->priv->header, "select-mode",
				self->priv->select_none_action, "enabled",
				G_BINDING_SYNC_CREATE);

	setup_source_switcher (self);
	totem_main_toolbar_set_custom_title (TOTEM_MAIN_TOOLBAR (self->priv->header), self->priv->switcher);

	g_signal_connect (self->priv->header, "back-clicked",
			  G_CALLBACK (back_button_clicked_cb), self);
	g_object_bind_property (self->priv->header, "search-mode",
				self->priv->search_bar, "search-mode-enabled",
				G_BINDING_BIDIRECTIONAL);
	g_signal_connect (self->priv->header, "notify::search-mode",
			  G_CALLBACK (search_mode_changed), self);

	/* Main view */
	self->priv->browser_model = GTK_TREE_MODEL (gtk_builder_get_object (builder, "gw_browse_store_results"));
	self->priv->recent_model = GTK_TREE_MODEL (gtk_builder_get_object (builder, "browser_recent_model"));
	self->priv->recent_sort_model = gtk_tree_model_sort_new_with_model (self->priv->recent_model);
	/* FIXME: Sorting is disabled for now
	 * https://bugzilla.gnome.org/show_bug.cgi?id=722781
	gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (self->priv->recent_sort_model),
					      MODEL_RESULTS_SORT_PRIORITY, GTK_SORT_DESCENDING); */

	self->priv->browser = GTK_WIDGET (gtk_builder_get_object (builder, "gw_browse"));
	g_object_bind_property (self->priv->header, "select-mode",
				self->priv->browser, "selection-mode",
				G_BINDING_BIDIRECTIONAL);

	g_signal_connect (self->priv->browser, "view-selection-changed",
			  G_CALLBACK (view_selection_changed_cb), self);
	g_signal_connect (self->priv->browser, "item-activated",
	                  G_CALLBACK (item_activated_cb), self);
	g_signal_connect (self->priv->browser, "selection-mode-request",
			  G_CALLBACK (selection_mode_requested), self);

	/* Selection toolbar */
	self->priv->selection_revealer = GTK_WIDGET (gtk_builder_get_object (builder, "selection_revealer"));
	self->priv->selection_bar = totem_selection_toolbar_new ();
	totem_selection_toolbar_set_show_delete_button (TOTEM_SELECTION_TOOLBAR (self->priv->selection_bar), FALSE);
	gtk_container_add (GTK_CONTAINER (self->priv->selection_revealer),
			   self->priv->selection_bar);
	gtk_widget_show (self->priv->selection_bar);
	g_object_bind_property (self->priv->header, "select-mode",
				self->priv->selection_revealer, "reveal-child",
				G_BINDING_SYNC_CREATE);
	g_signal_connect (self->priv->selection_bar, "play-clicked",
			  G_CALLBACK (play_cb), self);
	g_signal_connect (self->priv->selection_bar, "shuffle-clicked",
			  G_CALLBACK (shuffle_cb), self);
	g_signal_connect (self->priv->selection_bar, "delete-clicked",
			  G_CALLBACK (delete_cb), self);

	/* Loading thumbnails or more search results */
	adj = gtk_scrolled_window_get_vadjustment (GTK_SCROLLED_WINDOW (gtk_builder_get_object (builder, "gw_browse")));
	g_signal_connect (adj, "value_changed",
	                  G_CALLBACK (adjustment_value_changed_cb), self);
	g_signal_connect (adj, "changed",
	                  G_CALLBACK (adjustment_changed_cb), self);

	gd_main_view_set_model (GD_MAIN_VIEW (self->priv->browser),
				self->priv->recent_sort_model);

	totem_object_add_main_page (self->priv->totem,
				    "grilo",
				    GTK_WIDGET (gtk_builder_get_object (builder, "gw_search")));
}

static void
mtime_to_text (GtkTreeViewColumn *column,
	       GtkCellRenderer   *cell,
	       GtkTreeModel      *model,
	       GtkTreeIter       *iter,
	       gpointer           user_data)
{
	GDateTime *date;
	gint64 mtime;
	char *text;

	gtk_tree_model_get (model, iter, GD_MAIN_COLUMN_MTIME, &mtime, -1);
	if (mtime == 0)
		return;

	date = g_date_time_new_from_unix_utc (mtime);
	text = g_date_time_format (date, "%FT%H:%M:%SZ");
	g_date_time_unref (date);
	g_object_set (cell, "text", text, NULL);
	g_free (text);
}

static void
page_to_text (GtkTreeViewColumn *column,
	      GtkCellRenderer   *cell,
	      GtkTreeModel      *model,
	      GtkTreeIter       *iter,
	      gpointer           user_data)
{
	gint page;
	char *text;

	gtk_tree_model_get (model, iter, MODEL_RESULTS_PAGE, &page, -1);
	text = g_strdup_printf ("%d", page);
	g_object_set (cell, "text", text, NULL);
	g_free (text);
}

static void
remaining_to_text (GtkTreeViewColumn *column,
		   GtkCellRenderer   *cell,
		   GtkTreeModel      *model,
		   GtkTreeIter       *iter,
		   gpointer           user_data)
{
	guint remaining;
	char *text;

	gtk_tree_model_get (model, iter, MODEL_RESULTS_REMAINING, &remaining, -1);
	text = g_strdup_printf ("%u", remaining);
	g_object_set (cell, "text", text, NULL);
	g_free (text);
}

static void
create_debug_window (TotemGriloPlugin *self,
		     GtkTreeModel     *model)
{
	GtkWidget *window, *scrolled, *tree;

	window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	gtk_window_set_position (GTK_WINDOW (window), GTK_WIN_POS_CENTER);
	gtk_window_set_default_size (GTK_WINDOW (window), 800, 600);
	g_signal_connect (G_OBJECT (window), "delete-event",
			  G_CALLBACK(gtk_widget_hide_on_delete), NULL);

	scrolled = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled),
				       GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_container_add (GTK_CONTAINER (window), scrolled);

	tree = gtk_tree_view_new ();
	gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (tree), TRUE);
	gtk_tree_view_set_rules_hint (GTK_TREE_VIEW (tree), TRUE);
	gtk_widget_grab_focus (GTK_WIDGET (tree));
	gtk_container_add (GTK_CONTAINER (scrolled), tree);

	gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW (tree), -1,
						    "ID", gtk_cell_renderer_text_new (),
						    "text", GD_MAIN_COLUMN_ID, NULL);
	gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW (tree), -1,
						    "URI", gtk_cell_renderer_text_new (),
						    "text", GD_MAIN_COLUMN_URI, NULL);
	gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW (tree), -1,
						    "Primary text", gtk_cell_renderer_text_new (),
						    "text", GD_MAIN_COLUMN_PRIMARY_TEXT, NULL);
	gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW (tree), -1,
						    "Secondary text", gtk_cell_renderer_text_new (),
						    "text", GD_MAIN_COLUMN_SECONDARY_TEXT, NULL);
	gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW (tree), -1,
						    "Icon", gtk_cell_renderer_pixbuf_new (),
						    "pixbuf", GD_MAIN_COLUMN_ICON, NULL);
	gtk_tree_view_insert_column_with_data_func (GTK_TREE_VIEW (tree), -1,
						    "MTime", gtk_cell_renderer_text_new (),
						    mtime_to_text, NULL, NULL);
	gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW (tree), -1,
						    "Selected", gtk_cell_renderer_toggle_new (),
						    "active", GD_MAIN_COLUMN_SELECTED, NULL);
	gtk_tree_view_insert_column_with_data_func (GTK_TREE_VIEW (tree), -1,
						    "Page", gtk_cell_renderer_text_new (),
						    page_to_text, NULL, NULL);
	gtk_tree_view_insert_column_with_data_func (GTK_TREE_VIEW (tree), -1,
						    "Remaining", gtk_cell_renderer_text_new (),
						    remaining_to_text, NULL, NULL);

	gtk_tree_view_set_model (GTK_TREE_VIEW (tree), model);

	gtk_widget_show_all (window);
}

static void
setup_ui (TotemGriloPlugin *self,
          GtkBuilder *builder)
{
	totem_grilo_setup_icons (self->priv->totem);
	setup_browse (self, builder);

	/* create_debug_window (self, self->priv->browser_model); */
	/* create_debug_window (self, self->priv->recent_model); */
}

static void
setup_config (TotemGriloPlugin *self)
{
	gchar *config_file;
	GrlRegistry *registry = grl_registry_get_default ();

	/* Setup system-wide plugins configuration */
	config_file = totem_plugin_find_file ("grilo", TOTEM_GRILO_CONFIG_FILE);

	if (g_file_test (config_file, G_FILE_TEST_EXISTS))
		grl_registry_add_config_from_file (registry, config_file, NULL);
	g_free (config_file);

	/* Setup user-defined plugins configuration */
	config_file = g_build_path (G_DIR_SEPARATOR_S,
	                            g_get_user_config_dir (),
	                            g_get_prgname (),
	                            TOTEM_GRILO_CONFIG_FILE,
	                            NULL);

	if (g_file_test (config_file, G_FILE_TEST_EXISTS))
		grl_registry_add_config_from_file (registry, config_file, NULL);
	g_free (config_file);
}

static void
impl_activate (PeasActivatable *plugin)
{
	GtkBuilder *builder;

	TotemGriloPlugin *self = TOTEM_GRILO_PLUGIN (plugin);
	TotemGriloPluginPrivate *priv = self->priv;
	priv->totem = g_object_ref (g_object_get_data (G_OBJECT (plugin), "object"));
	priv->main_window = totem_object_get_main_window (priv->totem);

	priv->browse_keys = grl_metadata_key_list_new (GRL_METADATA_KEY_ARTIST,
						       GRL_METADATA_KEY_AUTHOR,
						       GRL_METADATA_KEY_DURATION,
						       GRL_METADATA_KEY_THUMBNAIL,
						       GRL_METADATA_KEY_URL,
						       GRL_METADATA_KEY_TITLE,
						       NULL);
	priv->search_keys = grl_metadata_key_list_new (GRL_METADATA_KEY_ARTIST,
						       GRL_METADATA_KEY_AUTHOR,
						       GRL_METADATA_KEY_DURATION,
						       GRL_METADATA_KEY_THUMBNAIL,
						       GRL_METADATA_KEY_URL,
						       GRL_METADATA_KEY_TITLE,
						       NULL);

	builder = gtk_builder_new_from_resource ("/org/totem/grilo/grilo.ui");
	setup_ui (self, builder);
	grl_init (0, NULL);
	setup_config (self);
	load_grilo_plugins (self);
}

static void
impl_deactivate (PeasActivatable *plugin)
{
	TotemGriloPlugin *self = TOTEM_GRILO_PLUGIN (plugin);
	GList *sources;
	GList *s;
	GrlRegistry *registry;

	/* FIXME remove main page? */

	registry = grl_registry_get_default ();
	g_signal_handlers_disconnect_by_func (registry, source_added_cb, self);
	g_signal_handlers_disconnect_by_func (registry, source_removed_cb, self);

	g_clear_pointer (&self->priv->browse_keys, g_list_free);
	g_clear_pointer (&self->priv->search_keys, g_list_free);

	/* Shutdown all plugins */
	unload_grilo_plugins (self);

	totem_grilo_clear_icons ();

	g_clear_object (&self->priv->switcher);

	/* Empty results */
	gd_main_view_set_model (GD_MAIN_VIEW (self->priv->browser), NULL);
	g_clear_object (&self->priv->browser_filter_model);
	gtk_tree_store_clear (GTK_TREE_STORE (self->priv->browser_model));
	gtk_tree_store_clear (GTK_TREE_STORE (self->priv->search_results_model));

	g_object_unref (self->priv->main_window);
	g_object_unref (self->priv->totem);
}
