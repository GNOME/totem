/* -*- Mode: C; indent-tabs-mode: t -*- */

/*
 * Copyright (C) 2010, 2011 Igalia S.L. <info@igalia.com>
 *
 * SPDX-License-Identifier: GPL-3-or-later
 *
 */

#include "config.h"
#include "icon-helpers.h"

#include <time.h>

#include <glib.h>
#include <glib-object.h>
#include <glib/gi18n-lib.h>
#include <grilo.h>
#include <pls/grl-pls.h>

#include <totem-interface.h>
#include <totem-dirs.h>
#include <totem.h>
#include <totem-private.h>

#include <totem-time-helpers.h>

#include "totem-grilo.h"
#include "totem-search-entry.h"
#include "totem-main-toolbar.h"
#include "totem-selection-toolbar.h"
#include <libgd/gd.h>

#define BROWSE_FLAGS          (GRL_RESOLVE_FAST_ONLY | GRL_RESOLVE_IDLE_RELAY)
#define PAGE_SIZE             50
#define SCROLL_GET_MORE_LIMIT 0.8
#define MIN_DURATION          5
#define MAX_DURATION          G_MAXINT

/* casts are to shut gcc up */
static const GtkTargetEntry target_table[] = {
	{ (gchar*) "text/uri-list", 0, 0 },
	{ (gchar*) "_NETSCAPE_URL", 0, 1 }
};

struct _TotemGrilo {
	GtkBox parent;

	Totem *totem;
	GtkWindow *main_window;

	gboolean plugins_activated;

	GrlSource *tracker_src;
	GrlSource *local_metadata_src;
	GrlSource *title_parsing_src;
	GrlSource *metadata_store_src;
	GrlSource *bookmarks_src;
	gboolean fs_plugin_configured;

	TotemGriloPage current_page;

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
	gboolean show_back_button;
	gboolean show_search_button;
	gboolean show_select_button;
	GMenuModel *selectmenu;
	GSimpleAction *select_all_action;
	GSimpleAction *select_none_action;

	/* Source switcher */
	GtkWidget *switcher;
	GtkWidget *recent, *channels, *search_hidden_button;
	char *last_page;

	/* Browser widgets */
	GtkWidget *browser;
	guint dnd_handler_id;
	GtkTreeModel *recent_model;
	GtkTreeModel *recent_sort_model;
	GtkTreeModel *browser_model;
	GtkTreeModel *browser_filter_model;
	gboolean in_search;
	GList *metadata_keys;
	guint thumbnail_update_id;

	/* Search widgets */
	GtkWidget *search_bar;
	GtkWidget *search_entry;
	GtkTreeModel *search_results_model;
	GHashTable *search_sources_ht;

	/* Selection toolbar */
	GtkWidget *selection_bar;
	GtkWidget *selection_revealer;

	GCancellable *thumbnail_cancellable;
};

enum {
	PROP_0,
	PROP_TOTEM,
	PROP_HEADER,
	PROP_SHOW_BACK_BUTTON,
	PROP_CURRENT_PAGE
};

G_DEFINE_TYPE(TotemGrilo, totem_grilo, GTK_TYPE_BOX)

typedef struct {
	TotemGrilo *totem_grilo;
	gboolean ignore_boxes; /* For the recent view */
	GtkTreeRowReference *ref_parent;
	GtkTreeModel *model;
} BrowseUserData;

typedef struct {
	TotemGrilo *totem_grilo;
	GrlMedia *media;
	GrlSource *source;
	GtkTreeModel *model;
	GtkTreeRowReference *reference;
} SetThumbnailData;

typedef struct {
	gboolean found;
	GrlKeyID key;
	GtkTreeIter *iter;
	GrlMedia *media;
} FindMediaData;

typedef struct {
	GtkTreeModel *model;
	gboolean all_removable;
} CanRemoveData;

enum {
	MODEL_RESULTS_SOURCE = GD_MAIN_COLUMN_LAST,
	MODEL_RESULTS_CONTENT,
	MODEL_RESULTS_IS_PRETHUMBNAIL,
	MODEL_RESULTS_PAGE,
	MODEL_RESULTS_REMAINING,
	MODEL_RESULTS_SORT_PRIORITY,
	MODEL_RESULTS_CAN_REMOVE
};

enum {
	CAN_REMOVE_UNSUPPORTED = -1,
	CAN_REMOVE_FALSE       = 0,
	CAN_REMOVE_TRUE        = 1
};

static gboolean
strv_has_prefix (const char * const *strv,
		 const char         *str)
{
	const char * const *s = strv;

	while (*s) {
		if (g_str_has_prefix (str, *s))
			return TRUE;
		s++;
	}

	return FALSE;
}

static gboolean
source_is_blocked (GrlSource *source)
{
	const char *id;
	const char * const sources[] = {
		"grl-shoutcast",
		"grl-flickr",
		"grl-podcasts",
		"grl-dmap",
		NULL
	};

	id = grl_source_get_id (source);
	g_assert (id);

	return strv_has_prefix (sources, id);
}

static gboolean
source_is_browse_blocked (GrlSource *source)
{
	const char *id;
	const char * const sources[] = {
		/* https://gitlab.gnome.org/GNOME/grilo/issues/36 */
		"grl-youtube",
		NULL
	};

	id = grl_source_get_id (source);
	g_assert (id);

	return strv_has_prefix (sources, id);
}

static gboolean
source_is_search_blocked (GrlSource *source)
{
	const char *id;
	const char * const sources[] = {
		"grl-metadata-store",
		NULL
	};

	id = grl_source_get_id (source);
	g_assert (id);

	return strv_has_prefix (sources, id);
}

static gboolean
source_is_recent (GrlSource *source)
{
	const char *id;
	const char * const sources[] = {
		"grl-tracker-source",
		"grl-tracker3-source",
		"grl-optical-media",
		"grl-bookmarks",
		NULL
	};

	id = grl_source_get_id (source);
	g_assert (id);

	return strv_has_prefix (sources, id);
}

static gboolean
source_is_forbidden (GrlSource *source)
{
	const char **tags;

	tags = grl_source_get_tags (source);
	if (!tags)
		return FALSE;

	return strv_has_prefix (tags, "adult") ||
		strv_has_prefix (tags, "torrent");
}

static gchar *
get_secondary_text (GrlMedia *media)
{
	const char *artist;
	int duration;

	if (grl_data_get_string (GRL_DATA (media), GRL_METADATA_KEY_SHOW) != NULL) {
		int season, episode;

		season = grl_data_get_int (GRL_DATA (media), GRL_METADATA_KEY_SEASON);
		episode = grl_data_get_int (GRL_DATA (media), GRL_METADATA_KEY_EPISODE);
		if (season != 0 && episode != 0)
			return g_strdup_printf (_("Season %d Episode %d"), season, episode);
	}

	artist = grl_data_get_string (GRL_DATA (media), GRL_METADATA_KEY_ARTIST);
	if (artist != NULL)
		return g_strdup (artist);
	duration = grl_media_get_duration (media);
	if (duration > 0)
		return totem_time_to_string (duration * 1000, TOTEM_TIME_FLAG_NONE);
	return NULL;
}

static const char *
get_primary_text (GrlMedia *media)
{
	const char *show;

	show = grl_data_get_string (GRL_DATA (media), GRL_METADATA_KEY_SHOW);
	if (show)
		return show;
	return grl_media_get_title (media);
}

static char *
get_title (GrlMedia *media)
{
	const char *show;

	show = grl_data_get_string (GRL_DATA (media), GRL_METADATA_KEY_SHOW);
	if (show != NULL) {
		int season, episode;

		season = grl_data_get_int (GRL_DATA (media), GRL_METADATA_KEY_SEASON);
		episode = grl_data_get_int (GRL_DATA (media), GRL_METADATA_KEY_EPISODE);
		if (season != 0 && episode != 0) {
			/* translators: The first item is the show name, for example:
			 * Boardwalk Empire (Season 1 Episode 1) */
			return g_strdup_printf (_("%s (Season %d Episode %d)"), show, season, episode);
		}
	}

	return g_strdup (grl_media_get_title (media));
}

static int
can_remove (GrlSource *source,
	    GrlMedia  *media)
{
	const char *url;
	char *scheme;
	int ret;

	if (g_strcmp0 (grl_source_get_id (source), "grl-bookmarks") == 0)
		return CAN_REMOVE_TRUE;
	if (!media)
		goto fallback;
	if (grl_media_is_container (media))
		return CAN_REMOVE_FALSE;
	url = grl_media_get_url (media);
	if (!url)
		return CAN_REMOVE_FALSE;

	scheme = g_uri_parse_scheme (url);
	ret = (g_strcmp0 (scheme, "file") == 0) ? CAN_REMOVE_TRUE : CAN_REMOVE_FALSE;
	g_free (scheme);

	if (ret == CAN_REMOVE_TRUE)
		return CAN_REMOVE_TRUE;

fallback:
	if (!(grl_source_supported_operations (source) & GRL_OP_REMOVE))
		return CAN_REMOVE_UNSUPPORTED;

	return CAN_REMOVE_FALSE;
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

	thumbnail = totem_grilo_get_thumbnail_finish (source_object, res, &error);
	if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
		goto out;

	if (!thumbnail && GRL_IS_MEDIA (thumb_data->media)) {
		g_debug ("Failed to get thumbnail for '%s': %s",
			 grl_media_get_url (thumb_data->media),
			 error->message);
	}

	path = gtk_tree_row_reference_get_path (thumb_data->reference);
	if (!path)
		goto out;
	gtk_tree_model_get_iter (thumb_data->model, &iter, path);

	if (thumbnail == NULL) {
		if (thumb_data->media)
			fallback_thumbnail = totem_grilo_get_video_icon ();
		else
			fallback_thumbnail = totem_grilo_get_channel_icon ();
	}

	gtk_tree_store_set (GTK_TREE_STORE (thumb_data->model),
			    &iter,
			    GD_MAIN_COLUMN_ICON, thumbnail ? thumbnail : fallback_thumbnail,
			    -1);
	g_clear_object (&thumbnail);

	/* Can we find that thumbnail in the view model? */
	view_model = gd_main_view_get_model (GD_MAIN_VIEW (thumb_data->totem_grilo->browser));
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
set_thumbnail_async (TotemGrilo   *self,
		     GObject      *object,
		     GtkTreeModel *model,
		     GtkTreePath  *path)
{
	SetThumbnailData *thumb_data;

	/* Let's read the thumbnail stream and set the thumbnail */
	thumb_data = g_slice_new0 (SetThumbnailData);
	thumb_data->totem_grilo = g_object_ref (self);
	if (GRL_IS_SOURCE (object))
		thumb_data->source = GRL_SOURCE (g_object_ref (object));
	else
		thumb_data->media = GRL_MEDIA (g_object_ref (object));
	thumb_data->model = g_object_ref (model);
	thumb_data->reference = gtk_tree_row_reference_new (model, path);

	totem_grilo_get_thumbnail (object, self->thumbnail_cancellable, get_thumbnail_cb, thumb_data);
}

static gboolean
update_search_thumbnails_idle (TotemGrilo *self)
{
	GtkTreePath *start_path;
	GtkTreePath *end_path;
	GrlSource *source;
	gboolean is_prethumbnail = FALSE;
	GtkTreeModel *view_model, *model;
	GtkIconView *icon_view;

	self->thumbnail_update_id = 0;

	icon_view = GTK_ICON_VIEW (gd_main_view_get_generic_view (GD_MAIN_VIEW (self->browser)));
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
			set_thumbnail_async (self, media ? G_OBJECT (media) : G_OBJECT (source), model, path);
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
update_search_thumbnails (TotemGrilo *self)
{
	if (self->thumbnail_update_id > 0)
		return;
	self->thumbnail_update_id = g_idle_add_full (G_PRIORITY_LOW, (GSourceFunc) update_search_thumbnails_idle, self, NULL);
	g_source_set_name_by_id (self->thumbnail_update_id, "[totem] update_search_thumbnails_idle");
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
			    GD_MAIN_COLUMN_PRIMARY_TEXT, get_primary_text (media),
			    GD_MAIN_COLUMN_SECONDARY_TEXT, secondary,
			    GD_MAIN_COLUMN_MTIME, mtime ? g_date_time_to_unix (mtime) : 0,
			    -1);

	g_clear_object (&thumbnail);
	g_free (secondary);
}

static void
add_local_metadata (TotemGrilo *self,
		    GrlSource  *source,
		    GrlMedia   *media)
{
	GrlOperationOptions *options;

	/* This is very slow and sync, so don't run it
	 * for non-local media */
	if (!source_is_recent (source))
		return;

	/* Avoid trying to get metadata for web radios */
	if (source == self->bookmarks_src) {
		char *scheme;

		scheme = g_uri_parse_scheme (grl_media_get_url (media));
		if (g_strcmp0 (scheme, "http") == 0 ||
		    g_strcmp0 (scheme, "https") == 0) {
			g_free (scheme);
			return;
		}
		g_free (scheme);
	}

	options = grl_operation_options_new (NULL);
	grl_operation_options_set_resolution_flags (options, GRL_RESOLVE_NORMAL);
	grl_source_resolve_sync (self->title_parsing_src,
				 media,
				 self->metadata_keys,
				 options,
				 NULL);
	grl_source_resolve_sync (self->local_metadata_src,
				 media,
				 self->metadata_keys,
				 options,
				 NULL);
	grl_source_resolve_sync (self->metadata_store_src,
				 media,
				 self->metadata_keys,
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
	if (g_str_equal (id, "grl-bookmarks"))
		return 75;
	if (g_str_equal (id, "grl-tracker-source") ||
	    g_str_equal (id, "grl-tracker3-source"))
		return 50;
	if (g_str_has_prefix (id, "grl-upnp-") ||
	    g_str_has_prefix (id, "grl-dleyna-"))
		return 25;
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
					   GD_MAIN_COLUMN_PRIMARY_TEXT, get_primary_text (media),
					   GD_MAIN_COLUMN_SECONDARY_TEXT, secondary,
					   GD_MAIN_COLUMN_MTIME, mtime ? g_date_time_to_unix (mtime) : 0,
					   MODEL_RESULTS_SORT_PRIORITY, prio,
					   MODEL_RESULTS_CAN_REMOVE, can_remove (source, media),
					   -1);

	g_clear_object (&thumbnail);
	g_free (secondary);
}

static void
browse_cb (GrlSource    *source,
           guint         browse_id,
           GrlMedia     *media,
           guint         remaining,
           gpointer      user_data,
           const GError *error)
{
	BrowseUserData *bud;
	TotemGrilo *self;
	GtkTreeIter parent;
	GtkWindow *window;
	guint remaining_expected;

	bud = (BrowseUserData *) user_data;
	self = bud->totem_grilo;

	if (error != NULL &&
	    g_error_matches (error,
	                     GRL_CORE_ERROR,
	                     GRL_CORE_ERROR_OPERATION_CANCELLED) == FALSE) {
		window = totem_object_get_main_window (self->totem);
		totem_interface_error (_("Browse Error"), error->message, window);
	}

	if (media != NULL) {
		if (bud->ref_parent) {
			GtkTreePath *path;

			path = gtk_tree_row_reference_get_path (bud->ref_parent);
			if (!path ||
			    !gtk_tree_model_get_iter (bud->model, &parent, path)) {
				g_clear_pointer (&path, gtk_tree_path_free);
				return;
			}

			gtk_tree_model_get (bud->model, &parent,
					    MODEL_RESULTS_REMAINING, &remaining_expected,
					    -1);
			remaining_expected--;
			gtk_tree_store_set (GTK_TREE_STORE (bud->model), &parent,
					    MODEL_RESULTS_REMAINING, remaining_expected,
					    -1);
		}

		if (!grl_media_is_image (media) &&
		    !grl_media_is_audio (media)) {
			if (grl_media_is_container (media) && bud->ignore_boxes) {
				/* Ignore boxes for certain sources */
			} else {
				add_local_metadata (self, source, media);
				add_media_to_model (GTK_TREE_STORE (bud->model),
						    bud->ref_parent ? &parent : NULL,
						    source, media);
			}
		} else {
			g_debug ("Ignoring %s browse result at %s",
				 grl_media_get_media_type (media) == GRL_MEDIA_TYPE_IMAGE ? "image" : "audio",
				 grl_media_get_url (media));
		}

		g_object_unref (media);
	}

	if (remaining == 0) {
		g_application_unmark_busy (g_application_get_default ());
		gtk_tree_row_reference_free (bud->ref_parent);
		g_object_unref (bud->totem_grilo);
		g_slice_free (BrowseUserData, bud);

		update_search_thumbnails (self);
	}
}

static void
browse (TotemGrilo   *self,
	GtkTreeModel *model,
        GtkTreePath  *path,
        GrlSource    *source,
        GrlMedia     *container,
        gint          page)
{
	BrowseUserData *bud;
	GrlOperationOptions *default_options;
	GrlCaps *caps;

	g_return_if_fail (source != NULL);
	g_return_if_fail (page >= 1 || page == -1);

	caps = grl_source_get_caps (source, GRL_OP_BROWSE);

	default_options = grl_operation_options_new (NULL);
	grl_operation_options_set_resolution_flags (default_options, BROWSE_FLAGS);
	if (page >= 1) {
		grl_operation_options_set_skip (default_options, (page - 1) * PAGE_SIZE);
		grl_operation_options_set_count (default_options, PAGE_SIZE);
	}
	if (grl_caps_get_type_filter (caps) & GRL_TYPE_FILTER_VIDEO)
		grl_operation_options_set_type_filter (default_options, GRL_TYPE_FILTER_VIDEO);
	if (grl_caps_is_key_range_filter (caps, GRL_METADATA_KEY_DURATION))
		grl_operation_options_set_key_range_filter (default_options,
							    GRL_METADATA_KEY_DURATION, MIN_DURATION, MAX_DURATION,
							    NULL);

	bud = g_slice_new0 (BrowseUserData);
	bud->totem_grilo = g_object_ref (self);
	bud->ignore_boxes = source_is_recent (source);
	if (path)
		bud->ref_parent = gtk_tree_row_reference_new (model, path);
	bud->model = g_object_ref (model);

	g_application_mark_busy (g_application_get_default ());
	grl_source_browse (source,
			   container,
			   self->metadata_keys,
			   default_options,
			   browse_cb,
			   bud);

	g_object_unref (default_options);
}

static void
play (TotemGrilo *self,
      GrlSource  *source,
      GrlMedia   *media,
      gboolean    resolve_url)
{
	const gchar *url;
	char *title;

	url = grl_media_get_url (media);
	if (!url)
		url = grl_media_get_external_url (media);
	if (!url) {
		g_warning ("Cannot find URL for %s (source: %s), please file a bug at https://gitlab.gnome.org/",
			   grl_media_get_id (media),
			   grl_media_get_source (media));
		return;
	}

	totem_object_clear_playlist (self->totem);
	title = get_title (media);
	totem_object_add_to_playlist (self->totem, url,
				      title,
				      TRUE);
	g_free (title);
}

static void
search_cb (GrlSource    *source,
           guint         search_id,
           GrlMedia     *media,
           guint         remaining,
           gpointer      user_data,
           const GError *error)
{
	GtkWindow *window;
	TotemGrilo *self;

	self = TOTEM_GRILO (user_data);

	if (error != NULL) {
		if (g_error_matches (error,
	                             GRL_CORE_ERROR,
	                             GRL_CORE_ERROR_OPERATION_CANCELLED)) {
			g_application_unmark_busy (g_application_get_default ());
			/* Don't zero out self->search_id to avoid a race
			 * condition with next search. Don't update thumbnails. */
			return;
		} else {
			window = totem_object_get_main_window (self->totem);
			totem_interface_error (_("Search Error"), error->message, window);
		}
	}

	if (media != NULL) {
		self->search_remaining--;

		if (!grl_media_is_image (media) &&
		    !grl_media_is_audio (media)) {
			add_local_metadata (self, source, media);
			add_media_to_model (GTK_TREE_STORE (self->search_results_model),
					    NULL, source, media);
		} else {
			g_debug ("Ignoring %s search result at %s",
				 grl_media_get_media_type (media) == GRL_MEDIA_TYPE_IMAGE ? "image" : "audio",
				 grl_media_get_url (media));
		}

		g_object_unref (media);
	}

	if (remaining == 0) {
		g_application_unmark_busy (g_application_get_default ());
		self->search_id = 0;
		update_search_thumbnails (self);
	}
}

static GrlOperationOptions *
get_search_options (TotemGrilo *self)
{
	GrlOperationOptions *default_options;
	GrlOperationOptions *supported_options;

	default_options = grl_operation_options_new (NULL);
	grl_operation_options_set_resolution_flags (default_options, BROWSE_FLAGS);
	grl_operation_options_set_skip (default_options, self->search_page * PAGE_SIZE);
	grl_operation_options_set_count (default_options, PAGE_SIZE);
	grl_operation_options_set_type_filter (default_options, GRL_TYPE_FILTER_VIDEO);
	grl_operation_options_set_key_range_filter (default_options,
						    GRL_METADATA_KEY_DURATION, MIN_DURATION, NULL,
						    NULL);

	/* And now remove all the unsupported filters and options */
	grl_operation_options_obey_caps (default_options,
					 grl_source_get_caps (GRL_SOURCE (self->search_source), GRL_OP_SEARCH),
					 &supported_options,
					 NULL);
	g_object_unref (default_options);

	return supported_options;
}

static void
search_more (TotemGrilo *self)
{
	GrlOperationOptions *search_options;

	search_options = get_search_options (self);

	self->search_page++;
	self->search_remaining = PAGE_SIZE;

	g_application_mark_busy (g_application_get_default ());

	self->search_id =
		grl_source_search (self->search_source,
				   self->search_text,
				   self->metadata_keys,
				   search_options,
				   search_cb,
				   self);
	g_object_unref (search_options);

	if (self->search_id == 0)
		search_cb (self->search_source, 0, NULL, 0, self, NULL);
}

static void
search (TotemGrilo  *self,
	GrlSource   *source,
	const gchar *text)
{
	g_clear_handle_id (&self->search_id, grl_operation_cancel);

	gtk_tree_store_clear (GTK_TREE_STORE (self->search_results_model));
//	g_hash_table_remove_all (self->cache_thumbnails);
	self->search_source = source;
	g_free (self->search_text);
	self->search_text = g_strdup (text);
	self->search_page = 0;
	gd_main_view_set_model (GD_MAIN_VIEW (self->browser),
				self->search_results_model);
	self->browser_filter_model = NULL;
	search_more (self);
}

static void
search_entry_activate_cb (GtkEntry   *entry,
			  TotemGrilo *self)
{
	GrlRegistry *registry;
	const char *id;
	const char *text;
	GrlSource *source;

	g_object_set (self, "show-back-button", FALSE, NULL);

	id = totem_search_entry_get_selected_id (TOTEM_SEARCH_ENTRY (self->search_entry));
	g_return_if_fail (id != NULL);
	registry = grl_registry_get_default ();
	source = grl_registry_lookup_source (registry, id);
	g_return_if_fail (source != NULL);

	text = totem_search_entry_get_text (TOTEM_SEARCH_ENTRY (self->search_entry));
	g_return_if_fail (text != NULL);

	g_object_set (self->header, "search-string", text, NULL);

	self->in_search = TRUE;
	search (self, source, text);
}

static void
set_browser_filter_model_for_path (TotemGrilo    *self,
				   GtkTreePath   *path)
{
	GtkTreeIter iter;
	int can_remove = CAN_REMOVE_FALSE;
	char *text = NULL;

	g_clear_object (&self->browser_filter_model);
	self->browser_filter_model = gtk_tree_model_filter_new (self->browser_model, path);

	gd_main_view_set_model (GD_MAIN_VIEW (self->browser),
				self->browser_filter_model);

	if (path != NULL && gtk_tree_model_get_iter (self->browser_model, &iter, path)) {
		gtk_tree_model_get (self->browser_model, &iter,
				    GD_MAIN_COLUMN_PRIMARY_TEXT, &text,
				    MODEL_RESULTS_CAN_REMOVE, &can_remove,
				    -1);
	}

	g_object_set (self, "show-back-button", path != NULL, NULL);
	if (path == NULL) {
		totem_main_toolbar_set_custom_title (TOTEM_MAIN_TOOLBAR (self->header), self->switcher);
	} else {
		totem_main_toolbar_set_custom_title (TOTEM_MAIN_TOOLBAR (self->header), NULL);
		totem_main_toolbar_set_title (TOTEM_MAIN_TOOLBAR (self->header), text);
	}

	totem_selection_toolbar_set_show_delete_button (TOTEM_SELECTION_TOOLBAR (self->selection_bar),
							can_remove != CAN_REMOVE_UNSUPPORTED);
	g_free (text);
}

static void
browser_activated_cb (GdMainView  *view,
                      GtkTreePath *path,
                      gpointer     user_data)
{
	guint remaining;
	gint page;
	GtkTreeModel *model;
	GtkTreeIter iter;
	GrlMedia *content;
	GrlSource *source;
	TotemGrilo *self = TOTEM_GRILO (user_data);
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
	if (content != NULL && grl_media_is_container (content) == FALSE) {
		play (self, source, content, TRUE);
		goto free_data;
	}

	/* Clicked on a container */
	gtk_tree_model_filter_convert_iter_to_child_iter (GTK_TREE_MODEL_FILTER (model),
							  &real_model_iter, &iter);

	treepath = gtk_tree_model_get_path (self->browser_model, &real_model_iter);
	set_browser_filter_model_for_path (self, treepath);

	/* We need to fill the model with browse data */
	if (remaining == 0) {
		gtk_tree_store_set (GTK_TREE_STORE (self->browser_model), &real_model_iter,
		                    MODEL_RESULTS_PAGE, ++page,
		                    MODEL_RESULTS_REMAINING, PAGE_SIZE,
		                    -1);
		browse (self, self->browser_model, treepath, source, content, page);
	}
	gtk_tree_path_free (treepath);

free_data:
	g_clear_object (&source);
	g_clear_object (&content);
}

static void
search_entry_source_changed_cb (GObject    *object,
                                GParamSpec *pspec,
                                TotemGrilo *self)
{
	/* FIXME: Do we actually want to do that? */
	if (self->search_id > 0) {
		grl_operation_cancel (self->search_id);
		self->search_id = 0;
	}
	gtk_tree_store_clear (GTK_TREE_STORE (self->search_results_model));
}

static void
search_activated_cb (GdMainView  *view,
                     GtkTreePath *path,
                     gpointer     user_data)
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

	play (TOTEM_GRILO (user_data), source, content, TRUE);

	g_clear_object (&source);
	g_clear_object (&content);
}

static void
item_activated_cb (GdMainView  *view,
		   const char  *id,
		   GtkTreePath *path,
		   gpointer     user_data)
{
	TotemGrilo *self = TOTEM_GRILO (user_data);
	GtkTreeModel *model;

	model = gd_main_view_get_model (view);

	if (model == self->search_results_model) {
		search_activated_cb (view, path, user_data);
	} else {
		totem_main_toolbar_set_search_mode (TOTEM_MAIN_TOOLBAR (self->header), FALSE);
		browser_activated_cb (view, path, user_data);
	}
}

static gboolean
find_media_cb (GtkTreeModel  *model,
	       GtkTreePath   *path,
	       GtkTreeIter   *iter,
	       FindMediaData *data)
{
	GrlMedia *media;
	const char *a, *b;

	gtk_tree_model_get (model, iter,
			    MODEL_RESULTS_CONTENT, &media,
			    -1);
	if (!media)
		return FALSE;

	a = grl_data_get_string (GRL_DATA (media), data->key);
	b = grl_data_get_string (GRL_DATA (data->media), data->key);

	if (g_strcmp0 (a, b) == 0) {
		g_object_unref (media);
		data->found = TRUE;
		data->iter = gtk_tree_iter_copy (iter);
		return TRUE;
	}
	g_object_unref (media);
	return FALSE;
}

static gboolean
find_media (GtkTreeModel  *model,
	    GrlMedia      *media,
	    GtkTreeIter  **iter)
{
	FindMediaData data;

	data.found = FALSE;
	data.key = GRL_METADATA_KEY_ID;
	data.media = media;
	data.iter = NULL;
	gtk_tree_model_foreach (model, (GtkTreeModelForeachFunc) find_media_cb, &data);

	*iter = data.iter;

	return data.found;
}

static GtkTreeModel *
get_tree_model_for_source (TotemGrilo *self,
			   GrlSource  *source)
{
	if (source_is_recent (source))
		return self->recent_model;

	return self->browser_model;
}

static void
content_changed (TotemGrilo   *self,
		 GrlSource    *source,
		 GPtrArray    *changed_medias)
{
	GtkTreeModel *model;
	guint i;

	model = get_tree_model_for_source (self, source);

	for (i = 0; i < changed_medias->len; i++) {
		GrlMedia *media = changed_medias->pdata[i];
		GtkTreeIter *iter;
		g_autofree char *str;

		str = grl_media_serialize (media);
		if (!grl_media_is_video (media) &&
		    !grl_media_is_container (media)) {
			g_debug ("Ignoring content changes for %s", str);
			continue;
		}

		g_debug ("About to change %s in the store", str);

		if (find_media (model, media, &iter)) {
			update_media (GTK_TREE_STORE (model), iter, source, media);
			gtk_tree_iter_free (iter);
		} else {
			g_debug ("Could not find '%s' to change in the store",
				 grl_media_get_id (media));
		}
	}
}

static void
content_removed (TotemGrilo   *self,
		 GrlSource    *source,
		 GPtrArray    *changed_medias)
{
	GtkTreeModel *model;
	guint i;

	model = get_tree_model_for_source (self, source);

	for (i = 0; i < changed_medias->len; i++) {
		GrlMedia *media = changed_medias->pdata[i];
		GtkTreeIter *iter;
		g_autofree char *str;

		str = grl_media_serialize (media);
		g_debug ("About to remove %s from the store", str);

		if (find_media (model, media, &iter)) {
			gtk_tree_store_remove (GTK_TREE_STORE (model), iter);
			gtk_tree_iter_free (iter);
		} else {
			g_debug ("Could not find '%s' to remove in the store",
				 grl_media_get_id (media));
		}
	}
}

static void
content_added (TotemGrilo   *self,
	       GrlSource    *source,
	       GPtrArray    *changed_medias)
{
	GtkTreeModel *model;
	guint i;

	model = get_tree_model_for_source (self, source);
	/* We're missing a container for the new media */
	if (model != self->recent_model)
		return;

	for (i = 0; i < changed_medias->len; i++) {
		GrlMedia *media = changed_medias->pdata[i];
		g_autofree char *str;

		str = grl_media_serialize (media);
		if (!grl_media_is_video (media) &&
		    !grl_media_is_container (media)) {
			g_debug ("Ignoring content added for %s", str);
			continue;
		}
		g_debug ("About to add %s to the store", str);

		add_local_metadata (self, source, media);
		add_media_to_model (GTK_TREE_STORE (model), NULL, source, media);
	}
}

static void
content_changed_cb (GrlSource           *source,
		    GPtrArray           *changed_medias,
		    GrlSourceChangeType  change_type,
		    gboolean             location_unknown,
		    TotemGrilo          *self)
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
                 GrlSource   *source,
                 gpointer     user_data)
{
	const gchar *name;
	TotemGrilo *self;
	GrlSupportedOps ops;
	const char *id;

	self = TOTEM_GRILO (user_data);
	id = grl_source_get_id (source);

	/* Metadata */
	if (g_str_equal (id, "grl-video-title-parsing"))
		self->title_parsing_src = source;
	else if (g_str_equal (id, "grl-local-metadata"))
		self->local_metadata_src = source;
	else if (g_str_equal (id, "grl-metadata-store"))
		self->metadata_store_src = source;
	else if (g_str_equal (id, "grl-bookmarks"))
		self->bookmarks_src = source;
	else if (g_str_equal (id, "grl-tracker-source") ||
		 g_str_equal (id, "grl-tracker3-source"))
		self->tracker_src = source;

	if (self->plugins_activated == FALSE)
		return;

	if (source_is_blocked (source) ||
	    source_is_forbidden (source) ||
	    !(grl_source_get_supported_media (source) & GRL_MEDIA_TYPE_VIDEO)) {
		grl_registry_unregister_source (registry,
		                                source,
		                                NULL);
		return;
	}

	/* The filesystem plugin */
	if (g_str_equal (id, "grl-filesystem") &&
	    self->fs_plugin_configured == FALSE) {
		return;
	}

	/* The local search source */
	if (g_str_equal (id, "grl-tracker-source") ||
	    g_str_equal (id, "grl-tracker3-source"))
		name = _("Local");
	else
		name = grl_source_get_name (source);

	ops = grl_source_supported_operations (source);
	if (ops & GRL_OP_BROWSE) {
		gboolean monitor = FALSE;

		if (source_is_recent (source)) {
			browse (self, self->recent_model,
				NULL, source, NULL, -1);
			/* https://gitlab.gnome.org/GNOME/grilo-plugins/merge_requests/29 */
			if (g_str_equal (id, "grl-tracker-source") == FALSE)
				monitor = TRUE;
		} else if (!source_is_browse_blocked (source)) {
			const GdkPixbuf *icon;

			icon = totem_grilo_get_channel_icon ();

			gtk_tree_store_insert_with_values (GTK_TREE_STORE (self->browser_model),
							   NULL, NULL, -1,
							   MODEL_RESULTS_SOURCE, source,
							   MODEL_RESULTS_CONTENT, NULL,
							   GD_MAIN_COLUMN_PRIMARY_TEXT, name,
							   GD_MAIN_COLUMN_ICON, icon,
							   MODEL_RESULTS_IS_PRETHUMBNAIL, TRUE,
							   MODEL_RESULTS_CAN_REMOVE, can_remove (source, NULL),
							   -1);

			if (g_str_equal (id, "grl-filesystem") == FALSE)
				monitor = TRUE;
		}

		if (monitor && (ops & GRL_OP_NOTIFY_CHANGE)) {
			grl_source_notify_change_start (source, NULL);
			g_signal_connect (G_OBJECT (source), "content-changed",
					  G_CALLBACK (content_changed_cb), self);
		}
	}
	if ((ops & GRL_OP_SEARCH) &&
	    !source_is_search_blocked (source)) {
		totem_search_entry_add_source (TOTEM_SEARCH_ENTRY (self->search_entry),
					       grl_source_get_id (source),
					       name,
					       get_source_priority (source));
	}
}

static gboolean
remove_browse_result (GtkTreeModel *model,
                      GtkTreePath  *path,
                      GtkTreeIter  *iter,
                      gpointer      user_data)
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
                   GrlSource   *source,
                   gpointer     user_data)
{
	GrlSupportedOps ops;
	TotemGrilo *self = TOTEM_GRILO (user_data);

	ops = grl_source_supported_operations (source);

	/* Remove source and content from browse results */
	if (ops & GRL_OP_BROWSE) {
		/* Inside the removed browse source? */
		if (self->browser_filter_model) {
			GtkTreePath *path;
			GtkTreeIter iter;

			g_object_get (G_OBJECT (self->browser_filter_model), "virtual-root", &path, NULL);
			if (path != NULL &&
			    gtk_tree_model_get_iter (self->browser_model, &iter, path)) {
				GrlSource *current_source;

				gtk_tree_model_get (self->browser_model, &iter,
						    MODEL_RESULTS_SOURCE, &current_source,
						    -1);
				if (current_source == source)
					set_browser_filter_model_for_path (self, NULL);
				g_clear_object (&current_source);
			}
			g_clear_pointer (&path, gtk_tree_path_free);
		}

		gtk_tree_model_foreach (self->browser_model,
		                        remove_browse_result,
		                        source);
	}

	/* If current search results belongs to removed source, clear the results. In
	   any case, remove the source from the list of searchable sources */
	if (ops & GRL_OP_SEARCH) {
		const char *id;

		if (self->search_source == source) {
			gtk_tree_store_clear (GTK_TREE_STORE (self->search_results_model));
			self->search_source = NULL;
		}

		id = grl_source_get_id (source);
		totem_search_entry_remove_source (TOTEM_SEARCH_ENTRY (self->search_entry), id);
	}
}

static void
load_grilo_plugins (TotemGrilo *self)
{
	GrlRegistry *registry;
	GError *error = NULL;
	GSettings *settings;
	char **configs;
	GrlConfig *config;
	guint i;
	const char *required_plugins[] = {
		"grl-lua-factory",
		"grl-local-metadata",
		"grl-metadata-store",
		"grl-bookmarks"
	};

	registry = grl_registry_get_default ();

	/* Check if there's filesystems to show */
	settings = g_settings_new ("org.gnome.totem");
	configs = g_settings_get_strv (settings, "filesystem-paths");
	g_object_unref (settings);

	for (i = 0; configs[i] != NULL; i++) {

		config = grl_config_new ("grl-filesystem", NULL);
		grl_config_set_string (config, "base-uri", configs[i]);
		grl_registry_add_config (registry, config, NULL);
		self->fs_plugin_configured = TRUE;
	}
	g_strfreev (configs);

	g_signal_connect (registry, "source-added",
	                  G_CALLBACK (source_added_cb), self);
	g_signal_connect (registry, "source-removed",
	                  G_CALLBACK (source_removed_cb), self);

	if (grl_registry_load_all_plugins (registry, FALSE, &error) == FALSE) {
		g_warning ("Failed to load grilo plugins: %s", error->message);
		g_error_free (error);
		return;
	}

	for (i = 0; i < G_N_ELEMENTS(required_plugins); i++) {
		if (!grl_registry_activate_plugin_by_id (registry, required_plugins[i], &error)) {
			g_warning ("Failed to load %s plugin: %s", required_plugins[i], error->message);
			g_clear_error (&error);
		}
	}
}

static gboolean
adjustment_over_limit (GtkAdjustment *adjustment)
{
#if 0
	g_message ("adj: %lf", gtk_adjustment_get_value (adjustment));
	g_message ("page size: %lf", gtk_adjustment_get_page_size (adjustment));
	g_message ("upper: %lf", gtk_adjustment_get_page_size (adjustment));
	g_message ("total: %lf", (gtk_adjustment_get_value (adjustment) + gtk_adjustment_get_page_size (adjustment)) / gtk_adjustment_get_upper (adjustment));
	g_message ("limit: %lf", SCROLL_GET_MORE_LIMIT);
#endif
	if ((gtk_adjustment_get_value (adjustment) + gtk_adjustment_get_page_size (adjustment)) / gtk_adjustment_get_upper (adjustment) > SCROLL_GET_MORE_LIMIT) {
		return TRUE;
	} else {
		return FALSE;
	}
}

static void
adjustment_changed_cb (GtkAdjustment *adjustment,
                       TotemGrilo *self)
{
	update_search_thumbnails (self);
}

static void
get_more_browse_results_cb (GtkAdjustment *adjustment,
                            TotemGrilo    *self)
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

	icon_view = GTK_ICON_VIEW (gd_main_view_get_generic_view (GD_MAIN_VIEW (self->browser)));

	if (gtk_icon_view_get_visible_range (icon_view, &start_path, &end_path) == FALSE)
		return;

	model = gd_main_view_get_model (GD_MAIN_VIEW (self->browser));
	if (model == self->recent_sort_model)
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
		    grl_media_is_container (container) == FALSE) {
			goto free_elements;
		}

		/* In case of containers, check that more elements can be obtained */
		if (remaining > 0) {
			goto free_elements;
		}

		/* Continue browsing */
		gtk_tree_store_set (GTK_TREE_STORE (self->browser_model),
		                    &iter,
		                    MODEL_RESULTS_PAGE, ++page,
		                    MODEL_RESULTS_REMAINING, PAGE_SIZE,
		                    -1);
		browse (self, self->browser_model, parent_path, source, container, page);
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
                             TotemGrilo    *self)
{
	update_search_thumbnails (self);

	if (self->in_search == FALSE) {
		get_more_browse_results_cb (adjustment, self);
		return;
	}

	/* Do not get more results if search is in progress */
	if (self->search_id != 0)
		return;

	/* Do not get more results if there are no more results to get :) */
	if (self->search_remaining > 0)
		return;

	if (adjustment_over_limit (adjustment))
		search_more (self);
}

void
totem_grilo_back_button_clicked (TotemGrilo *self)
{
	GtkTreePath *path;
	GtkTreeIter iter;

	g_return_if_fail (TOTEM_IS_GRILO (self));

	g_assert (self->show_back_button);
	g_assert (self->browser_filter_model);
	g_object_get (G_OBJECT (self->browser_filter_model), "virtual-root", &path, NULL);
	g_assert (path);

	/* We don't call set_browser_filter_model_for_path() to avoid
	 * the back button getting hidden and re-shown */
	g_clear_object (&self->browser_filter_model);
	gd_main_view_set_model (GD_MAIN_VIEW (self->browser), NULL);

	totem_main_toolbar_set_search_mode (TOTEM_MAIN_TOOLBAR (self->header), FALSE);
	gd_main_view_set_selection_mode (GD_MAIN_VIEW (self->browser), FALSE);

	/* Remove all the items at that level */
	if (gtk_tree_model_get_iter (self->browser_model, &iter, path)) {
		GtkTreeIter child;

		if (gtk_tree_model_iter_children (self->browser_model, &child, &iter)) {
			while (gtk_tree_store_remove (GTK_TREE_STORE (self->browser_model), &child))
				;
		}

		gtk_tree_store_set (GTK_TREE_STORE (self->browser_model), &iter,
		                    MODEL_RESULTS_PAGE, 0,
		                    MODEL_RESULTS_REMAINING, 0,
		                    -1);
	}

	gtk_tree_path_up (path);
	if (path != NULL && gtk_tree_path_get_depth (path) > 0)
		set_browser_filter_model_for_path (self, path);
	else
		set_browser_filter_model_for_path (self, NULL);
	gtk_tree_path_free (path);
}

static gboolean
window_key_press_event_cb (GtkWidget   *win,
			   GdkEventKey *event,
			   TotemGrilo  *self)
{
	guint state;

	/* Check whether we're in the browse panel */
	if (!g_str_equal (totem_object_get_main_page (self->totem), "grilo"))
		return GDK_EVENT_PROPAGATE;

	state = event->state & gtk_accelerator_get_default_mod_mask ();

	/* Handle Ctrl+F */
	if (state == GDK_CONTROL_MASK) {
		if (event->keyval == GDK_KEY_F ||
		    event->keyval == GDK_KEY_f) {
			gtk_search_bar_set_search_mode (GTK_SEARCH_BAR (self->search_bar),
							!gtk_search_bar_get_search_mode (GTK_SEARCH_BAR (self->search_bar)));
			return GDK_EVENT_STOP;
		}
	}

	if (state == 0 &&
	    event->keyval == GDK_KEY_Escape &&
	    gd_main_view_get_selection_mode (GD_MAIN_VIEW (self->browser))) {
		gd_main_view_set_selection_mode (GD_MAIN_VIEW (self->browser), FALSE);
		return GDK_EVENT_STOP;
	}

	return gtk_search_bar_handle_event (GTK_SEARCH_BAR (self->search_bar), (GdkEvent *) event);
}

static void
selection_mode_requested (GdMainView  *view,
			  TotemGrilo  *self)
{
	GtkTreePath *root = NULL;

	/* Don't allow selections when at the root of the
	 * "Channels" view */
	if (self->current_page == TOTEM_GRILO_PAGE_CHANNELS &&
	    self->browser_filter_model != NULL) {
		g_object_get (self->browser_filter_model,
			      "virtual-root", &root,
			      NULL);
		if (root == NULL)
			return;
	}

	gd_main_view_set_selection_mode (GD_MAIN_VIEW (view), TRUE);
	g_clear_pointer (&root, gtk_tree_path_free);
}

static void
can_remove_foreach (gpointer data,
		    gpointer user_data)
{
	CanRemoveData *can_remove_data = user_data;
	GtkTreePath *path = data;
	GtkTreeIter iter;
	int removable;

	gtk_tree_model_get_iter (can_remove_data->model, &iter, path);
	gtk_tree_model_get (can_remove_data->model, &iter,
	                    MODEL_RESULTS_CAN_REMOVE, &removable,
	                    -1);

	if (removable <= CAN_REMOVE_FALSE)
		can_remove_data->all_removable = FALSE;
}

static void
view_selection_changed_cb (GdMainView   *view,
			   TotemGrilo   *self)
{
	GtkTreeModel *model;
	GList *list;
	guint count;
	CanRemoveData data;

	list = gd_main_view_get_selection (view);
	model = gd_main_view_get_model (view);

	count = g_list_length (list);
	if (count == 0) {
		data.all_removable = FALSE;
	} else {
		data.model = model;
		data.all_removable = TRUE;
		g_list_foreach (list, can_remove_foreach, &data);
	}
	g_list_free_full (list, (GDestroyNotify) gtk_tree_path_free);

	totem_main_toolbar_set_n_selected (TOTEM_MAIN_TOOLBAR (self->header), count);
	totem_selection_toolbar_set_n_selected (TOTEM_SELECTION_TOOLBAR (self->selection_bar), count);
	totem_selection_toolbar_set_delete_button_sensitive (TOTEM_SELECTION_TOOLBAR (self->selection_bar), data.all_removable);
}

static void
select_all_action_cb (GSimpleAction    *action,
		      GVariant         *parameter,
		      TotemGrilo       *self)
{
	gd_main_view_select_all (GD_MAIN_VIEW (self->browser));
}

static void
select_none_action_cb (GSimpleAction    *action,
		       GVariant         *parameter,
		       TotemGrilo       *self)
{
	gd_main_view_unselect_all (GD_MAIN_VIEW (self->browser));
}

static void
totem_grilo_drop_files (TotemGrilo       *self,
			GtkSelectionData *data)
{
	char **list;
	guint i;

	list = g_uri_list_extract_uris ((const char *) gtk_selection_data_get_data (data));

	for (i = 0; list[i] != NULL; i++) {
		g_debug ("Preparing to add '%s' as dropped file", list[i]);
		totem_grilo_add_item_to_recent (self, list[i], NULL, FALSE);
	}

	g_strfreev (list);
}

static void
drop_video_cb (GtkWidget          *widget,
	       GdkDragContext     *context,
	       gint                x,
	       gint                y,
	       GtkSelectionData   *data,
	       guint               info,
	       guint               _time,
	       TotemGrilo         *self)
{
	GtkWidget *source_widget;
	GdkDragAction action = gdk_drag_context_get_selected_action (context);

	source_widget = gtk_drag_get_source_widget (context);

	/* Drop of video on itself */
	if (source_widget && widget == source_widget && action == GDK_ACTION_MOVE) {
		gtk_drag_finish (context, FALSE, FALSE, _time);
		return;
	}

	totem_grilo_drop_files (self, data);
	gtk_drag_finish (context, TRUE, FALSE, _time);
}

static void
totem_grilo_set_drop_enabled (TotemGrilo *self,
			      gboolean    enabled)
{
	if (enabled == (self->dnd_handler_id != 0))
		return;

	if (enabled) {
		self->dnd_handler_id = g_signal_connect (G_OBJECT (self->browser), "drag_data_received",
							       G_CALLBACK (drop_video_cb), self);
		gtk_drag_dest_set (GTK_WIDGET (self->browser), GTK_DEST_DEFAULT_ALL,
				   target_table, G_N_ELEMENTS (target_table),
				   GDK_ACTION_MOVE | GDK_ACTION_COPY);
	} else {
		g_signal_handler_disconnect (G_OBJECT (self->browser),
					     self->dnd_handler_id);
		self->dnd_handler_id = 0;
		gtk_drag_dest_unset (GTK_WIDGET (self->browser));
	}
}

static void
source_switched (GtkToggleButton  *button,
		 TotemGrilo       *self)
{
	const char *id;

	if (!gtk_toggle_button_get_active (button))
		return;

	id = g_object_get_data (G_OBJECT (button), "name");
	if (g_str_equal (id, "recent")) {
		gd_main_view_set_model (GD_MAIN_VIEW (self->browser),
					self->recent_sort_model);
		self->current_page = TOTEM_GRILO_PAGE_RECENT;
		totem_grilo_set_drop_enabled (self, TRUE);
	} else if (g_str_equal (id, "channels")) {
		if (self->browser_filter_model != NULL)
			gd_main_view_set_model (GD_MAIN_VIEW (self->browser),
						self->browser_filter_model);
		else
			set_browser_filter_model_for_path (self, NULL);
		self->current_page = TOTEM_GRILO_PAGE_CHANNELS;
		totem_grilo_set_drop_enabled (self, FALSE);
	} else if (g_str_equal (id, "search")) {
		return;
	}

	g_clear_pointer (&self->last_page, g_free);
	g_object_set (self->header, "search-mode", FALSE, NULL);

	g_object_notify (G_OBJECT (self), "current-page");
}

static GtkWidget *
create_switcher_button (TotemGrilo *self,
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
setup_source_switcher (TotemGrilo *self)
{
	GtkStyleContext *context;

	self->switcher = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_box_set_homogeneous (GTK_BOX (self->switcher), TRUE);

	self->recent = create_switcher_button (self, _("Videos"), "recent");
	gtk_container_add (GTK_CONTAINER (self->switcher), self->recent);

	g_signal_connect (G_OBJECT (self->recent), "drag_data_received",
			  G_CALLBACK (drop_video_cb), self);
	gtk_drag_dest_set (GTK_WIDGET (self->recent), GTK_DEST_DEFAULT_ALL,
			   target_table, G_N_ELEMENTS (target_table),
			   GDK_ACTION_MOVE | GDK_ACTION_COPY);

	self->channels = create_switcher_button (self, _("Channels"), "channels");
	gtk_radio_button_join_group (GTK_RADIO_BUTTON (self->channels),
				     GTK_RADIO_BUTTON (self->recent));
	gtk_container_add (GTK_CONTAINER (self->switcher), self->channels);

	self->search_hidden_button = create_switcher_button (self, "HIDDEN SEARCH BUTTON", "search");
	gtk_radio_button_join_group (GTK_RADIO_BUTTON (self->search_hidden_button),
				     GTK_RADIO_BUTTON (self->recent));
	g_object_ref_sink (G_OBJECT (self->search_hidden_button));

	context = gtk_widget_get_style_context (self->switcher);
	gtk_style_context_add_class (context, "stack-switcher");
	gtk_style_context_add_class (context, GTK_STYLE_CLASS_LINKED);

	gtk_widget_show_all (self->switcher);
	g_object_ref_sink (self->switcher);
}

static void
search_mode_changed (GObject          *gobject,
		     GParamSpec       *pspec,
		     TotemGrilo       *self)
{
	gboolean search_mode;

	search_mode = totem_main_toolbar_get_search_mode (TOTEM_MAIN_TOOLBAR (self->header));
	if (!search_mode) {
		if (self->last_page ==  NULL) {
			/* Already reset */
		} else if (g_str_equal (self->last_page, "recent")) {
			gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (self->recent), TRUE);
		} else if (g_str_equal (self->last_page, "channels")) {
			gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (self->channels), TRUE);
		} else {
			g_assert_not_reached ();
		}
		g_clear_pointer (&self->last_page, g_free);

		self->in_search = search_mode;
	} else {
		GtkTreeModel *model;
		const char *id = NULL;

		/* Try to guess which source should be used for search */
		model = gd_main_view_get_model (GD_MAIN_VIEW (self->browser));
		if (model == self->recent_sort_model) {
			id = grl_source_get_id (self->tracker_src);
			self->last_page = g_strdup ("recent");
		} else {
			GtkTreeIter iter;
			GtkTreePath *path;

			g_object_get (G_OBJECT (model), "virtual-root", &path, NULL);
			if (path != NULL &&
			    gtk_tree_model_get_iter (self->browser_model, &iter, path)) {
				GrlSource *source;

				gtk_tree_model_get (self->browser_model, &iter,
						    MODEL_RESULTS_SOURCE, &source,
						    -1);
				id = source ? grl_source_get_id (source) : NULL;
				g_clear_object (&source);
			}
			g_clear_pointer (&path, gtk_tree_path_free);

			self->last_page = g_strdup ("channels");
		}

		if (id != NULL)
			totem_search_entry_set_selected_id (TOTEM_SEARCH_ENTRY (self->search_entry), id);

		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (self->search_hidden_button), TRUE);
	}

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
play_selection (TotemGrilo *self,
		gboolean    shuffle)
{
	GtkTreeModel *model;
	GList *list;
	GPtrArray *items;
	guint i;

	list = gd_main_view_get_selection (GD_MAIN_VIEW (self->browser));
	model = gd_main_view_get_model (GD_MAIN_VIEW (self->browser));

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

	totem_object_clear_playlist (self->totem);
	list = NULL;

	for (i = 0; i < items->len; i++) {
		GtkTreePath *path = items->pdata[i];
		GtkTreeIter iter;
		GrlMedia *media;
		const gchar *url;
		char *title;

		gtk_tree_model_get_iter (model, &iter, path);
		gtk_tree_model_get (model, &iter,
				    MODEL_RESULTS_CONTENT, &media,
				    -1);

		url = grl_media_get_url (media);
		if (!url)
			url = grl_media_get_external_url (media);
		if (!url) {
			g_warning ("Cannot find URL for %s (source: %s), please file a bug at https://gitlab.gnome.org/",
				   grl_media_get_id (media),
				   grl_media_get_source (media));
			goto next_item;
		}

		title = get_title (media);
		list = g_list_prepend (list, totem_playlist_mrl_data_new (url, title));
		g_free (title);

next_item:
		g_clear_object (&media);
		gtk_tree_path_free (path);
	}

	g_ptr_array_free (items, FALSE);

	totem_object_add_items_to_playlist (self->totem, g_list_reverse (list));

	g_object_set (G_OBJECT (self->browser), "selection-mode", FALSE, NULL);
}

static void
play_cb (TotemSelectionToolbar *bar,
	 TotemGrilo            *self)
{
	play_selection (self, FALSE);
}

static void
shuffle_cb (TotemSelectionToolbar *bar,
	    TotemGrilo            *self)
{
	play_selection (self, TRUE);
}

static void
delete_foreach (gpointer data,
		gpointer user_data)
{
	GtkTreeRowReference *ref = data;
	GtkTreePath *path;
	GtkTreeModel *view_model = user_data;
	GtkTreeIter iter;
	GrlSource *source;
	GrlMedia *media;
	gboolean source_supports_remove;
	gboolean success;
	GError *error = NULL;

	GtkTreeModel *model;
	GtkTreeIter real_model_iter;

	path = gtk_tree_row_reference_get_path (ref);
	if (!path || !gtk_tree_model_get_iter (view_model, &iter, path)) {
		g_warning ("An item that was scheduled for removal isn't available any more");
		gtk_tree_row_reference_free (ref);
		return;
	}

	gtk_tree_model_get (view_model, &iter,
			    MODEL_RESULTS_CONTENT, &media,
			    MODEL_RESULTS_SOURCE, &source,
			    -1);

	source_supports_remove = (grl_source_supported_operations (source) & GRL_OP_REMOVE);
	success = TRUE;

	if (source_supports_remove) {
		g_debug ("Removing item '%s' through Grilo",
			 grl_media_get_id (media));
		grl_source_remove_sync (source, media, &error);
		success = (error == NULL);
	}

	if (!source_supports_remove ||
	    g_strcmp0 (grl_source_get_id (source), "grl-bookmarks") == 0) {
		const char *uri;

		uri = grl_media_get_url (media);
		if (!uri) {
			success = FALSE;
			g_set_error_literal (&error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED,
					     "Item cannot be removed through Grilo and doesn't have a URI, please file a bug");
		} else {
			GFile *file;

			file = g_file_new_for_uri (grl_media_get_url (media));
			success = g_file_trash (file, NULL, &error);
			g_object_unref (file);
		}
	}

	if (!success) {
		g_warning ("Couldn't remove item '%s' (%s): %s",
			   grl_media_get_title (media),
			   grl_media_get_id (media),
			   error->message);
		g_error_free (error);
		goto end;
	}
	if (grl_source_supported_operations (source) & GRL_OP_REMOVE)
		goto end;

	/* In case of success, or if we didn't remove the item through the source */
	if (GTK_IS_TREE_MODEL_FILTER (view_model)) {
		model = gtk_tree_model_filter_get_model (GTK_TREE_MODEL_FILTER (view_model));
		gtk_tree_model_filter_convert_iter_to_child_iter (GTK_TREE_MODEL_FILTER (view_model),
								  &real_model_iter, &iter);
	} else if (GTK_IS_TREE_MODEL_SORT (view_model)) {
		model = gtk_tree_model_sort_get_model (GTK_TREE_MODEL_SORT (view_model));
		gtk_tree_model_sort_convert_iter_to_child_iter (GTK_TREE_MODEL_SORT (view_model),
								&real_model_iter, &iter);
	} else {
		g_assert_not_reached ();
	}

	gtk_tree_store_remove (GTK_TREE_STORE (model), &real_model_iter);

end:
	g_clear_object (&media);
	g_clear_object (&source);
	gtk_tree_row_reference_free (ref);
}

static void
delete_cb (TotemSelectionToolbar *bar,
	   TotemGrilo            *self)
{
	GtkTreeModel *model;
	GList *list, *l;

	g_signal_handlers_block_by_func (self->browser, view_selection_changed_cb, self);

	model = gd_main_view_get_model (GD_MAIN_VIEW (self->browser));
	list = gd_main_view_get_selection (GD_MAIN_VIEW (self->browser));

	/* GList of GtkTreePaths to a GList of GtkTreeRowReferences */
	for (l = list; l != NULL; l = l->next) {
		GtkTreeRowReference *ref;

		ref = gtk_tree_row_reference_new (model, l->data);
		gtk_tree_path_free (l->data);
		l->data = ref;
	}
	g_list_foreach (list, delete_foreach, model);

	g_signal_handlers_unblock_by_func (self->browser, view_selection_changed_cb, self);

	g_object_set (G_OBJECT (self->browser), "selection-mode", FALSE, NULL);
}

static void
setup_browse (TotemGrilo *self)
{
	GtkAdjustment *adj;
	const char * const select_all_accels[] = { "<Primary>A", NULL };
	const char * const select_none_accels[] = { "<Shift><Primary>A", NULL };

	/* Search */
	gtk_search_bar_connect_entry (GTK_SEARCH_BAR (self->search_bar),
				      totem_search_entry_get_entry (TOTEM_SEARCH_ENTRY (self->search_entry)));

	g_signal_connect (self->main_window, "key-press-event",
			  G_CALLBACK (window_key_press_event_cb), self);
	g_signal_connect (self->search_entry, "activate",
	                  G_CALLBACK (search_entry_activate_cb),
	                  self);

	//FIXME also setup a timeout for that
	g_signal_connect (self->search_entry, "notify::selected-id",
			  G_CALLBACK (search_entry_source_changed_cb), self);

	/* Toolbar */
	self->select_all_action = g_simple_action_new ("select-all", NULL);
	g_signal_connect (G_OBJECT (self->select_all_action), "activate",
			  G_CALLBACK (select_all_action_cb), self);
	g_action_map_add_action (G_ACTION_MAP (self->totem), G_ACTION (self->select_all_action));
	gtk_application_set_accels_for_action (GTK_APPLICATION (self->totem), "app.select-all", select_all_accels);
	g_object_bind_property (self->header, "select-mode",
				self->select_all_action, "enabled",
				G_BINDING_SYNC_CREATE);

	self->select_none_action = g_simple_action_new ("select-none", NULL);
	g_signal_connect (G_OBJECT (self->select_none_action), "activate",
			  G_CALLBACK (select_none_action_cb), self);
	g_action_map_add_action (G_ACTION_MAP (self->totem), G_ACTION (self->select_none_action));
	gtk_application_set_accels_for_action (GTK_APPLICATION (self->totem), "app.select-none", select_none_accels);
	g_object_bind_property (self->header, "select-mode",
				self->select_none_action, "enabled",
				G_BINDING_SYNC_CREATE);

	setup_source_switcher (self);
	totem_main_toolbar_set_custom_title (TOTEM_MAIN_TOOLBAR (self->header), self->switcher);

	g_object_bind_property (self->header, "search-mode",
				self->search_bar, "search-mode-enabled",
				G_BINDING_BIDIRECTIONAL);
	g_signal_connect (self->header, "notify::search-mode",
			  G_CALLBACK (search_mode_changed), self);

	/* Main view */
	self->recent_sort_model = gtk_tree_model_sort_new_with_model (self->recent_model);
	/* FIXME: Sorting is disabled for now
	 * https://bugzilla.gnome.org/show_bug.cgi?id=722781
	gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (self->recent_sort_model),
					      MODEL_RESULTS_SORT_PRIORITY, GTK_SORT_DESCENDING); */

	g_object_bind_property (self->header, "select-mode",
				self->browser, "selection-mode",
				G_BINDING_BIDIRECTIONAL);
	g_object_bind_property (self->header, "select-mode",
				gtk_bin_get_child (GTK_BIN (self->header)), "show-close-button",
				G_BINDING_INVERT_BOOLEAN);

	g_signal_connect (self->browser, "view-selection-changed",
			  G_CALLBACK (view_selection_changed_cb), self);
	g_signal_connect (self->browser, "item-activated",
	                  G_CALLBACK (item_activated_cb), self);
	g_signal_connect (self->browser, "selection-mode-request",
			  G_CALLBACK (selection_mode_requested), self);

	totem_grilo_set_drop_enabled (self, TRUE);

	/* Selection toolbar */
	g_object_set (G_OBJECT (self->header), "select-menu-model", self->selectmenu, NULL);
	self->selection_bar = totem_selection_toolbar_new ();
	totem_selection_toolbar_set_show_delete_button (TOTEM_SELECTION_TOOLBAR (self->selection_bar), TRUE);
	gtk_container_add (GTK_CONTAINER (self->selection_revealer),
			   self->selection_bar);
	gtk_widget_show (self->selection_bar);
	g_object_bind_property (self->header, "select-mode",
				self->selection_revealer, "reveal-child",
				G_BINDING_SYNC_CREATE);
	g_signal_connect (self->selection_bar, "play-clicked",
			  G_CALLBACK (play_cb), self);
	g_signal_connect (self->selection_bar, "shuffle-clicked",
			  G_CALLBACK (shuffle_cb), self);
	g_signal_connect (self->selection_bar, "delete-clicked",
			  G_CALLBACK (delete_cb), self);

	/* Loading thumbnails or more search results */
	adj = gtk_scrolled_window_get_vadjustment (GTK_SCROLLED_WINDOW (self->browser));
	g_signal_connect (adj, "value_changed",
	                  G_CALLBACK (adjustment_value_changed_cb), self);
	g_signal_connect (adj, "changed",
	                  G_CALLBACK (adjustment_changed_cb), self);

	gd_main_view_set_model (GD_MAIN_VIEW (self->browser),
				self->recent_sort_model);
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
int_to_text (GtkTreeViewColumn *column,
	     GtkCellRenderer   *cell,
	     GtkTreeModel      *model,
	     GtkTreeIter       *iter,
	     gpointer           user_data)
{
	int column_num;
	gint page;
	char *text;

	column_num = GPOINTER_TO_INT (user_data);
	gtk_tree_model_get (model, iter, column_num, &page, -1);
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
media_to_text (GtkTreeViewColumn *column,
	       GtkCellRenderer   *cell,
	       GtkTreeModel      *model,
	       GtkTreeIter       *iter,
	       gpointer           user_data)
{
	int column_num;
	GrlMedia *media;
	const char *text;

	column_num = GPOINTER_TO_INT (user_data);

	gtk_tree_model_get (model, iter, MODEL_RESULTS_CONTENT, &media, -1);
	if (media == NULL)
		return;

	if (column_num == GD_MAIN_COLUMN_ID)
		text = grl_media_get_id (media);
	else if (column_num == GD_MAIN_COLUMN_URI)
		text = grl_media_get_url (media);
	else
		g_assert_not_reached ();

	g_object_set (cell, "text", text, NULL);
	g_object_unref (media);

}

static void
create_debug_window (TotemGrilo       *self,
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
	gtk_widget_grab_focus (GTK_WIDGET (tree));
	gtk_container_add (GTK_CONTAINER (scrolled), tree);

	gtk_tree_view_insert_column_with_data_func (GTK_TREE_VIEW (tree), -1,
						    "ID", gtk_cell_renderer_text_new (),
						    media_to_text, GINT_TO_POINTER (GD_MAIN_COLUMN_ID), NULL);
	gtk_tree_view_insert_column_with_data_func (GTK_TREE_VIEW (tree), -1,
						    "URI", gtk_cell_renderer_text_new (),
						    media_to_text, GINT_TO_POINTER (GD_MAIN_COLUMN_URI), NULL);
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
						    int_to_text, GINT_TO_POINTER (MODEL_RESULTS_PAGE), NULL);
	gtk_tree_view_insert_column_with_data_func (GTK_TREE_VIEW (tree), -1,
						    "Remaining", gtk_cell_renderer_text_new (),
						    remaining_to_text, NULL, NULL);
	gtk_tree_view_insert_column_with_data_func (GTK_TREE_VIEW (tree), -1,
						    "Can Remove", gtk_cell_renderer_text_new (),
						    int_to_text, GINT_TO_POINTER (MODEL_RESULTS_CAN_REMOVE), NULL);

	gtk_tree_view_set_model (GTK_TREE_VIEW (tree), model);

	gtk_widget_show_all (window);
}

static void
setup_ui (TotemGrilo *self)
{
	totem_grilo_setup_icons ();
	setup_browse (self);

	/* create_debug_window (self, self->browser_model); */
	/* create_debug_window (self, self->recent_model); */
	/* create_debug_window (self, self->search_results_model); */
}

static void
setup_config (TotemGrilo *self)
{
	GrlRegistry *registry = grl_registry_get_default ();
	grl_registry_add_config_from_resource (registry, "/org/gnome/totem/grilo/totem-grilo.conf", NULL);
}

GtkWidget *
totem_grilo_new (TotemObject *totem,
		 GtkWidget   *header)
{
	g_return_val_if_fail (TOTEM_IS_OBJECT (totem), NULL);

	return GTK_WIDGET (g_object_new (TOTEM_TYPE_GRILO,
					 "totem", totem,
					 "header", header, NULL));
}

static void
totem_grilo_finalize (GObject *object)
{
	TotemGrilo *self = TOTEM_GRILO (object);
	GrlRegistry *registry;

	if (self->thumbnail_update_id > 0) {
		g_source_remove (self->thumbnail_update_id);
		self->thumbnail_update_id = 0;
	}

	g_cancellable_cancel (self->thumbnail_cancellable);
	g_clear_object (&self->thumbnail_cancellable);

	registry = grl_registry_get_default ();
	g_signal_handlers_disconnect_by_func (registry, source_added_cb, self);
	g_signal_handlers_disconnect_by_func (registry, source_removed_cb, self);

	g_clear_pointer (&self->metadata_keys, g_list_free);

	grl_deinit ();

	totem_grilo_clear_icons ();

	g_clear_object (&self->switcher);
	g_clear_object (&self->search_hidden_button);

	/* Empty results */
	//FIXME
	//gd_main_view_set_model (GD_MAIN_VIEW (self->browser), NULL);
	//g_clear_object (&self->browser_filter_model);
	//gtk_tree_store_clear (GTK_TREE_STORE (self->browser_model));
	//gtk_tree_store_clear (GTK_TREE_STORE (self->search_results_model));

	g_object_unref (self->main_window);
	g_object_unref (self->totem);

	G_OBJECT_CLASS (totem_grilo_parent_class)->finalize (object);
}

void
totem_grilo_start (TotemGrilo *self)
{
	GError *error = NULL;
	GrlRegistry *registry;

	g_debug ("TotemGrilo: Resuming videos thumbnailing");

	totem_grilo_resume_icon_thumbnailing ();

	if (self->plugins_activated)
		return;

	g_debug ("TotemGrilo: Activating plugins");

	registry = grl_registry_get_default ();
	self->plugins_activated = TRUE;

	/* Load the others */
	if (grl_registry_load_all_plugins (registry, TRUE, &error) == FALSE) {
		g_warning ("Failed to activate grilo plugins: %s", error->message);
		g_error_free (error);
	}
}

void
totem_grilo_pause (TotemGrilo *self)
{
	g_debug ("TotemGrilo: Pausing videos thumbnailing");
	totem_grilo_pause_icon_thumbnailing ();
}

static void
totem_grilo_constructed (GObject *object)
{
	TotemGrilo *self = TOTEM_GRILO (object);

	self->main_window = totem_object_get_main_window (self->totem);

	setup_ui (self);
	grl_init (0, NULL);
	setup_config (self);
	load_grilo_plugins (self);
}

gboolean
totem_grilo_get_show_back_button (TotemGrilo *self)
{
	g_return_val_if_fail (TOTEM_IS_GRILO (self), FALSE);

	return self->show_back_button;
}

void
totem_grilo_set_current_page (TotemGrilo     *self,
			      TotemGriloPage  page)
{
	GtkWidget *button;

	g_return_if_fail (TOTEM_IS_GRILO (self));

	if (page == TOTEM_GRILO_PAGE_RECENT)
		button = self->recent;
	else if (page == TOTEM_GRILO_PAGE_CHANNELS)
		button = self->channels;
	else
		g_assert_not_reached ();

	self->current_page = page;

	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), TRUE);

	g_object_notify (G_OBJECT (self), "current-page");
}

TotemGriloPage
totem_grilo_get_current_page (TotemGrilo *self)
{
	g_return_val_if_fail (TOTEM_IS_GRILO (self), TOTEM_GRILO_PAGE_RECENT);

	return self->current_page;
}

gboolean
totem_grilo_add_item_to_recent (TotemGrilo *self,
				const char *uri,
				const char *title,
				gboolean    is_web)
{
	GrlMedia *media;
	GFile *file;
	FindMediaData data;

	g_return_val_if_fail (TOTEM_IS_GRILO (self), FALSE);

	file = g_file_new_for_uri (uri);

	if (is_web) {
		char *basename;

		/* This should only come from the
		 * "Open Location" dialogue, so it
		 * shouldn't have a title */
		g_assert (title == NULL);

		media = grl_media_video_new ();

		basename = g_file_get_basename (file);
		grl_media_set_title (media, basename);
		g_free (basename);

		grl_media_set_url (media, uri);
	} else {
		GrlOperationOptions *options;

		options = grl_operation_options_new (NULL);
		media = grl_media_video_new ();
		media = grl_pls_file_to_media (media,
					       file,
					       NULL,
					       FALSE,
					       options);
		if (media && title)
			grl_media_set_title (media, title);

		g_object_unref (options);
	}

	g_object_unref (file);

	if (!media)
		return FALSE;

	data.found = FALSE;
	data.key = GRL_METADATA_KEY_URL;
	data.media = media;
	data.iter = NULL;
	gtk_tree_model_foreach (self->recent_model, (GtkTreeModelForeachFunc) find_media_cb, &data);

	if (data.found) {
		g_debug ("URI '%s' is already present in the bookmarks, not adding duplicate", uri);
		gtk_tree_iter_free (data.iter);
		g_object_unref (media);
		return FALSE;
	}

	/* This should be quick, just adding the item to the DB */
	grl_source_store_sync (self->bookmarks_src,
			       NULL,
			       media,
			       GRL_WRITE_NORMAL,
			       NULL);
	return TRUE;
}

static void
totem_grilo_set_property (GObject         *object,
                          guint            prop_id,
                          const GValue    *value,
                          GParamSpec      *pspec)
{
	TotemGrilo *self = TOTEM_GRILO (object);

	switch (prop_id)
	{
	case PROP_TOTEM:
		self->totem = g_value_dup_object (value);
		break;

	case PROP_HEADER:
		self->header = g_value_dup_object (value);
		break;

	case PROP_SHOW_BACK_BUTTON:
		self->show_back_button = g_value_get_boolean (value);
		g_object_set (self->header, "show-back-button", self->show_back_button, NULL);
		break;

	case PROP_CURRENT_PAGE:
		totem_grilo_set_current_page (self, g_value_get_int (value));
		break;

	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
totem_grilo_get_property (GObject         *object,
                          guint            prop_id,
                          GValue          *value,
                          GParamSpec      *pspec)
{
	TotemGrilo *self = TOTEM_GRILO (object);

	switch (prop_id) {
	case PROP_SHOW_BACK_BUTTON:
		g_value_set_boolean (value, self->show_back_button);
		break;

	case PROP_CURRENT_PAGE:
		g_value_set_int (value, self->current_page);
		break;

	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
totem_grilo_class_init (TotemGriloClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

	object_class->finalize = totem_grilo_finalize;
	object_class->constructed = totem_grilo_constructed;
	object_class->set_property = totem_grilo_set_property;
	object_class->get_property = totem_grilo_get_property;

	g_object_class_install_property (object_class,
					 PROP_TOTEM,
					 g_param_spec_object ("totem",
							      "Totem",
							      "Totem.",
							      TOTEM_TYPE_OBJECT,
							      G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

	g_object_class_install_property (object_class,
					 PROP_HEADER,
					 g_param_spec_object ("header",
							      "Headerbar",
							      "Headerbar.",
							      TOTEM_TYPE_MAIN_TOOLBAR,
							      G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

	g_object_class_install_property (object_class,
					 PROP_SHOW_BACK_BUTTON,
					 g_param_spec_boolean ("show-back-button",
							       "Show Back Button",
							       "Whether the back button is visible",
							       FALSE,
							       G_PARAM_READWRITE | G_PARAM_CONSTRUCT));

	g_object_class_install_property (object_class,
					 PROP_CURRENT_PAGE,
					 g_param_spec_int ("current-page",
							   "Current page",
							   "The name of the currently visible page",
							   TOTEM_GRILO_PAGE_RECENT, TOTEM_GRILO_PAGE_CHANNELS, TOTEM_GRILO_PAGE_RECENT,
							   G_PARAM_READWRITE));

	gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/totem/grilo/grilo.ui");
	gtk_widget_class_bind_template_child (widget_class, TotemGrilo, selectmenu);
	gtk_widget_class_bind_template_child (widget_class, TotemGrilo, search_bar);
	gtk_widget_class_bind_template_child (widget_class, TotemGrilo, search_entry);
	gtk_widget_class_bind_template_child (widget_class, TotemGrilo, browser);
	gtk_widget_class_bind_template_child (widget_class, TotemGrilo, selection_revealer);

	gtk_widget_class_bind_template_child (widget_class, TotemGrilo, search_results_model);
	gtk_widget_class_bind_template_child (widget_class, TotemGrilo, browser_model);
	gtk_widget_class_bind_template_child (widget_class, TotemGrilo, recent_model);
}

static void
totem_grilo_init (TotemGrilo *self)
{
	self->thumbnail_cancellable = g_cancellable_new ();
	self->metadata_keys = grl_metadata_key_list_new (GRL_METADATA_KEY_ARTIST,
							 GRL_METADATA_KEY_AUTHOR,
							 GRL_METADATA_KEY_DURATION,
							 GRL_METADATA_KEY_MODIFICATION_DATE,
							 GRL_METADATA_KEY_THUMBNAIL,
							 GRL_METADATA_KEY_URL,
							 GRL_METADATA_KEY_EXTERNAL_URL,
							 GRL_METADATA_KEY_TITLE,
							 GRL_METADATA_KEY_SHOW,
							 GRL_METADATA_KEY_SEASON,
							 GRL_METADATA_KEY_EPISODE,
							 GRL_METADATA_KEY_TITLE_FROM_FILENAME,
							 NULL);

	gtk_widget_init_template (GTK_WIDGET (self));
}
