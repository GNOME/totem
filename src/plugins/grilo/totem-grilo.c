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

#include <video-utils.h>

#include "totem-search-entry.h"

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

#define GRILO_POPUP_MENU                                                \
	"<ui>" \
	"<popup name=\"grilo-popup\">" \
	"<menuitem name=\"add-to-playlist\" action=\"add-to-playlist\"/>" \
	"<menuitem name=\"copy-location\" action=\"copy-location\"/>" \
	"</popup>" \
	"</ui>"

#define BROWSE_FLAGS          (GRL_RESOLVE_FAST_ONLY | GRL_RESOLVE_IDLE_RELAY)
#define PAGE_SIZE             50
#define THUMB_SEARCH_SIZE     128
#define THUMB_BROWSE_SIZE     32
#define SCROLL_GET_MORE_LIMIT 0.8

#define TOTEM_GRILO_CONFIG_FILE "totem-grilo.conf"

const gchar *BLACKLIST_SOURCES[] = { "grl-filesystem",
                                     "grl-bookmarks",
                                     "grl-shoutcast",
                                     "grl-flickr",
                                     NULL };

typedef enum {
	ICON_BOX = 0,
	ICON_AUDIO,
	ICON_VIDEO,
	ICON_DEFAULT
} IconType;

typedef struct {
	Totem *totem;

	/* Current media selected in results*/
	GrlMedia *selected_media;

	/* Thumb cache to speed up loading: maps url strings to GdkPixbuf thumbnails */
	GHashTable *cache_thumbnails;

	/* Search related information */
	GrlMediaSource *search_source;
	guint search_id;
	gint search_page;
	gint search_remaining;
	gchar *search_text;

	/* Browser widgets */
	GtkWidget *browser;
	GtkTreeModel *browser_model;

	/* Search widgets */
	GtkWidget *search_entry;
	GtkTreeModel *search_results_model;
	GHashTable *search_sources_ht;
	GtkWidget *search_sources_list;
	GtkWidget *search_results_view;

	/* Popup */
	GtkUIManager *ui_manager;
	GtkActionGroup *action_group;

	/* Configuration */
	guint max_items;
} TotemGriloPluginPrivate;

TOTEM_PLUGIN_REGISTER (TOTEM_TYPE_GRILO_PLUGIN, TotemGriloPlugin, totem_grilo_plugin)

typedef struct {
	TotemGriloPlugin *totem_grilo;
	GtkTreeRowReference *ref_parent;
} BrowseUserData;

typedef struct {
	TotemGriloPlugin *totem_grilo;
	GrlMedia *media;
	GFile *file;
	GtkTreeRowReference *reference;
	gint thumb_size;
} SetThumbnailData;

enum {
	SEARCH_MODEL_SOURCES_SOURCE = 0,
	SEARCH_MODEL_SOURCES_NAME,
};

enum {
	MODEL_RESULTS_SOURCE = 0,
	MODEL_RESULTS_CONTENT,
	MODEL_RESULTS_THUMBNAIL,
	MODEL_RESULTS_IS_PRETHUMBNAIL,
	MODEL_RESULTS_DESCRIPTION,
	MODEL_RESULTS_DURATION,
	MODEL_RESULTS_PAGE,
	MODEL_RESULTS_REMAINING,
};

static void play (TotemGriloPlugin *self,
                  GrlMediaSource *source,
                  GrlMedia *media,
                  gboolean resolve_url);

static gchar *
get_description (GrlMedia *media)
{
	const gchar *author;

	author = grl_data_get_string (GRL_DATA (media), GRL_METADATA_KEY_AUTHOR);
	if (author == NULL) {
		author = grl_data_get_string (GRL_DATA (media), GRL_METADATA_KEY_ARTIST);
	}

	if (author != NULL) {
		return g_markup_printf_escaped ("<b>%s</b>\n%s",
		                                grl_media_get_title (media),
		                                author);
	} else {
		return g_markup_printf_escaped ("<b>%s</b>",
		                                grl_media_get_title (media));
	}
}

static GList *
browse_keys (void)
{
	static GList *_browse_keys = NULL;

	if (_browse_keys == NULL) {
		_browse_keys = grl_metadata_key_list_new (GRL_METADATA_KEY_ARTIST,
		                                          GRL_METADATA_KEY_AUTHOR,
		                                          GRL_METADATA_KEY_DURATION,
		                                          GRL_METADATA_KEY_THUMBNAIL,
		                                          GRL_METADATA_KEY_URL,
		                                          GRL_METADATA_KEY_TITLE,
		                                          NULL);
	}

	return _browse_keys;
}

static GList *
search_keys (void)
{
	static GList *_search_keys = NULL;

	if (_search_keys == NULL) {
		_search_keys = grl_metadata_key_list_new (GRL_METADATA_KEY_ARTIST,
		                                          GRL_METADATA_KEY_AUTHOR,
		                                          GRL_METADATA_KEY_THUMBNAIL,
		                                          GRL_METADATA_KEY_URL,
		                                          GRL_METADATA_KEY_TITLE,
		                                          NULL);
	}

	return _search_keys;
}

static GdkPixbuf *
load_icon (TotemGriloPlugin *self, IconType icon_type, gint thumb_size)
{
	GdkScreen *screen;
	GtkIconTheme *theme;

	const gchar *icon_name[] = { GTK_STOCK_DIRECTORY,
	                             "audio-x-generic",
	                             "video-x-generic",
	                             GTK_STOCK_FILE };

	static GdkPixbuf *pixbuf[G_N_ELEMENTS(icon_name)] = { NULL };

	if (pixbuf[icon_type] == NULL) {
		screen = gtk_window_get_screen (totem_get_main_window (self->priv->totem));
		theme = gtk_icon_theme_get_for_screen (screen);
		pixbuf[icon_type] = gtk_icon_theme_load_icon (theme,
		                                              icon_name[icon_type],
		                                              thumb_size, 0, NULL);
	}

	if (pixbuf[icon_type] != NULL) {
		return g_object_ref (pixbuf[icon_type]);
	} else {
		return NULL;
	}
}

static GdkPixbuf *
get_icon (TotemGriloPlugin *self, GrlMedia *media, gint thumb_size)
{
	if (GRL_IS_MEDIA_BOX (media)) {
		return load_icon (self, ICON_BOX, thumb_size);
	} else if (GRL_IS_MEDIA_AUDIO (media)) {
		return load_icon (self, ICON_AUDIO, thumb_size);
	} else if (GRL_IS_MEDIA_VIDEO (media)) {
		return load_icon (self, ICON_VIDEO, thumb_size);
	} else {
		return load_icon (self, ICON_DEFAULT, thumb_size);
	}
}

static void
get_stream_thumbnail_cb (GObject *source_object,
                         GAsyncResult *res,
                         gpointer user_data)
{
	GFileInputStream *stream;
	GdkPixbuf *thumbnail = NULL;
	GtkTreeIter iter;
	SetThumbnailData *thumb_data = (SetThumbnailData *) user_data;

	stream = g_file_read_finish (thumb_data->file, res, NULL);
	if (stream != NULL) {
		thumbnail = gdk_pixbuf_new_from_stream_at_scale (G_INPUT_STREAM (stream),
		                                                 thumb_data->thumb_size,
		                                                 thumb_data->thumb_size,
		                                                 TRUE, NULL, NULL);
		g_object_unref (stream);
	}

	gtk_tree_model_get_iter (thumb_data->totem_grilo->priv->search_results_model,
	                         &iter,
	                         gtk_tree_row_reference_get_path (thumb_data->reference));

	if (thumbnail) {
		gtk_list_store_set (GTK_LIST_STORE (thumb_data->totem_grilo->priv->search_results_model),
		                    &iter,
		                    MODEL_RESULTS_THUMBNAIL, thumbnail,
		                    -1);
		/* Cache it */
		g_hash_table_insert (thumb_data->totem_grilo->priv->cache_thumbnails,
		                     g_file_get_uri (thumb_data->file),
		                     thumbnail);
	}

	/* Free thumb data */
	g_object_unref (thumb_data->totem_grilo);
	g_object_unref (thumb_data->media);
	g_object_unref (thumb_data->file);
	gtk_tree_row_reference_free (thumb_data->reference);
	g_slice_free (SetThumbnailData, thumb_data);
}

static void
set_thumbnail_async (TotemGriloPlugin *self, GrlMedia *media, GtkTreePath *path, gint thumb_size)
{
	const gchar *url_thumb;
	GFile *file_uri;
	SetThumbnailData *thumb_data;
	GdkPixbuf *thumbnail;
	GtkTreeIter iter;

	url_thumb = grl_media_get_thumbnail (media);
	if (url_thumb != NULL) {
		/* Check cache */
		thumbnail = g_hash_table_lookup (self->priv->cache_thumbnails,
		                                 url_thumb);
		if (thumbnail == NULL) {
			/* Let's read the thumbnail stream and set the thumbnail */
			file_uri = g_file_new_for_uri (url_thumb);
			thumb_data = g_slice_new (SetThumbnailData);
			thumb_data->totem_grilo = g_object_ref (self);
			thumb_data->media = g_object_ref (media);
			thumb_data->file = g_object_ref (file_uri);
			thumb_data->reference = gtk_tree_row_reference_new (self->priv->search_results_model, path);
			thumb_data->thumb_size = thumb_size;
			g_file_read_async (file_uri, G_PRIORITY_DEFAULT, NULL,
			                   get_stream_thumbnail_cb, thumb_data);
			g_object_unref (file_uri);
		} else {
			/* Use cached thumbnail */
			gtk_tree_model_get_iter (self->priv->search_results_model, &iter, path);
			gtk_list_store_set (GTK_LIST_STORE (self->priv->search_results_model),
			                    &iter,
			                    MODEL_RESULTS_THUMBNAIL, thumbnail,
			                    -1);
		}
	} else {
		/* Keep the icon */
		gtk_tree_model_get_iter (self->priv->search_results_model, &iter, path);
		gtk_list_store_set (GTK_LIST_STORE (self->priv->search_results_model),
		                    &iter,
		                    MODEL_RESULTS_IS_PRETHUMBNAIL, FALSE,
		                    -1);
	}
}

static gboolean
update_search_thumbnails_idle (TotemGriloPlugin *self)
{
	GtkTreePath *start_path;
	GtkTreePath *end_path;
	GrlMedia *media;
	GtkTreeIter iter;
	gboolean is_prethumbnail = FALSE;

	if (gtk_icon_view_get_visible_range (GTK_ICON_VIEW (self->priv->search_results_view),
	                                     &start_path, &end_path)) {
		for (;
		     gtk_tree_path_compare (start_path, end_path) <= 0;
		     gtk_tree_path_next (start_path)) {
			if (gtk_tree_model_get_iter (self->priv->search_results_model, &iter, start_path) == FALSE) {
				break;
			}

			gtk_tree_model_get (self->priv->search_results_model,
			                    &iter,
			                    MODEL_RESULTS_CONTENT, &media,
			                    MODEL_RESULTS_IS_PRETHUMBNAIL, &is_prethumbnail,
			                    -1);
			if (is_prethumbnail) {
				set_thumbnail_async (self, media, start_path, THUMB_SEARCH_SIZE);
				gtk_list_store_set (GTK_LIST_STORE (self->priv->search_results_model),
				                    &iter,
				                    MODEL_RESULTS_IS_PRETHUMBNAIL, FALSE,
				                    -1);
			}

			g_object_unref (media);
		}
		gtk_tree_path_free (start_path);
		gtk_tree_path_free (end_path);
	}

	return FALSE;
}

static void
update_search_thumbnails (TotemGriloPlugin *self)
{
	g_idle_add ((GSourceFunc) update_search_thumbnails_idle, self);
}

static void
show_sources (TotemGriloPlugin *self)
{
	GList *sources, *source;
	GtkTreeIter iter;
	GrlPluginRegistry *registry;
	const gchar *name;
	GdkPixbuf *icon;

	registry = grl_plugin_registry_get_default ();
	sources = grl_plugin_registry_get_sources_by_operations (registry,
	                                                         GRL_OP_BROWSE,
	                                                         FALSE);

	for (source = sources; source ; source = g_list_next (source)) {
		icon = load_icon (self, ICON_BOX, THUMB_BROWSE_SIZE);
		name = grl_metadata_source_get_name (GRL_METADATA_SOURCE (source->data));
		gtk_tree_store_append (GTK_TREE_STORE (self->priv->browser_model), &iter, NULL);
		gtk_tree_store_set (GTK_TREE_STORE (self->priv->browser_model),
		                    &iter,
		                    MODEL_RESULTS_SOURCE, source->data,
		                    MODEL_RESULTS_CONTENT, NULL,
		                    MODEL_RESULTS_DESCRIPTION, name,
		                    MODEL_RESULTS_THUMBNAIL, icon,
		                    MODEL_RESULTS_IS_PRETHUMBNAIL, FALSE,
		                    -1);
		if (icon != NULL) {
			g_object_unref (icon);
		}
	}
	g_list_free (sources);
}

static void
browse_cb (GrlMediaSource *source,
           guint browse_id,
           GrlMedia *media,
           guint remaining,
           gpointer user_data,
           const GError *error)
{
	gchar *description;
	gchar *pretty_duration;
	gint duration;
	GtkTreeIter iter;
	GdkPixbuf *thumbnail;
	BrowseUserData *bud;
	TotemGriloPlugin *self;
	GtkTreeIter parent;
	GtkTreePath *path;
	GtkWindow *window;
	gint remaining_expected;

	bud = (BrowseUserData *) user_data;
	self = bud->totem_grilo;

	if (error != NULL &&
	    g_error_matches (error,
	                     GRL_CORE_ERROR,
	                     GRL_CORE_ERROR_OPERATION_CANCELLED) == FALSE) {
		window = totem_get_main_window (self->priv->totem);
		totem_interface_error (_("Browse Error"), error->message, window);
	}

	if (media != NULL) {
		gtk_tree_model_get_iter (self->priv->browser_model, &parent, gtk_tree_row_reference_get_path (bud->ref_parent));
		gtk_tree_model_get (self->priv->browser_model, &parent,
		                    MODEL_RESULTS_REMAINING, &remaining_expected,
		                    -1);
		remaining_expected--;
		gtk_tree_store_set (GTK_TREE_STORE (self->priv->browser_model), &parent,
		                    MODEL_RESULTS_REMAINING, &remaining_expected,
		                    -1);
		/* Filter images */
		if (GRL_IS_MEDIA_IMAGE (media) == FALSE) {
			thumbnail = get_icon (self, media, THUMB_BROWSE_SIZE);
			description = get_description (media);
			duration = grl_media_get_duration (media);
			if (duration > 0) {
				pretty_duration = totem_time_to_string (duration * 1000);
			} else {
				pretty_duration = NULL;
			}

			gtk_tree_store_append (GTK_TREE_STORE (self->priv->browser_model), &iter, &parent);
			gtk_tree_store_set (GTK_TREE_STORE (self->priv->browser_model),
			                    &iter,
			                    MODEL_RESULTS_SOURCE, source,
			                    MODEL_RESULTS_CONTENT, media,
			                    MODEL_RESULTS_THUMBNAIL, thumbnail,
			                    MODEL_RESULTS_IS_PRETHUMBNAIL, TRUE,
			                    MODEL_RESULTS_DESCRIPTION, description,
			                    MODEL_RESULTS_DURATION, pretty_duration,
			                    -1);

			if (thumbnail != NULL) {
				g_object_unref (thumbnail);
			}
			g_free (description);
			g_free (pretty_duration);

			path = gtk_tree_model_get_path (self->priv->browser_model, &parent);
			gtk_tree_view_expand_row (GTK_TREE_VIEW (self->priv->browser), path, FALSE);
			gtk_tree_view_columns_autosize (GTK_TREE_VIEW (self->priv->browser));
			gtk_tree_path_free (path);
		}

		g_object_unref (media);
	}

	if (remaining == 0) {
		gtk_tree_row_reference_free (bud->ref_parent);
		g_object_unref (bud->totem_grilo);
		g_slice_free (BrowseUserData, bud);
	}
}

static void
browse (TotemGriloPlugin *self,
        GtkTreePath *path,
        GrlMediaSource *source,
        GrlMedia *container,
        gint page)
{
	if (source != NULL) {
		BrowseUserData *bud;
		GrlOperationOptions *default_options;

		default_options = grl_operation_options_new (NULL);
		grl_operation_options_set_flags (default_options, BROWSE_FLAGS);
		grl_operation_options_set_skip (default_options, (page - 1) * PAGE_SIZE);
		grl_operation_options_set_count (default_options, PAGE_SIZE);

		bud = g_slice_new (BrowseUserData);
		bud->totem_grilo = g_object_ref (self);
		bud->ref_parent = gtk_tree_row_reference_new (self->priv->browser_model, path);
		grl_media_source_browse (source,
		                         container,
		                         browse_keys (),
		                         default_options,
		                         browse_cb,
		                         bud);

		g_object_unref (default_options);
	} else {
		show_sources (self);
	}
}

static void
resolve_url_cb (GrlMediaSource *source,
                guint op_id,
                GrlMedia *media,
                gpointer user_data,
                const GError *error)
{
	if (error != NULL) {
		g_warning ("Failed to resolve URL for media: %s", error->message);
		return;
	}

	play (TOTEM_GRILO_PLUGIN (user_data), source, media, FALSE);
}

static void
play (TotemGriloPlugin *self,
      GrlMediaSource *source,
      GrlMedia *media,
      gboolean resolve_url)
{
	const gchar *url;

	url = grl_media_get_url (media);
	if (url != NULL) {
		totem_add_to_playlist_and_play (self->priv->totem, url,
		                                grl_media_get_title (media),
		                                TRUE);
		return;
	}

	/* If url is a slow key, then we need to full resolve it */
	if (resolve_url) {
		const GList *slow_keys;
		GList *url_keys;
		slow_keys = grl_metadata_source_slow_keys (GRL_METADATA_SOURCE (source));
		if (g_list_find ((GList *) slow_keys, GINT_TO_POINTER (GRL_METADATA_KEY_URL)) != NULL) {
			url_keys = grl_metadata_key_list_new (GRL_METADATA_KEY_URL, NULL);
			grl_media_source_metadata (source, media, url_keys, 0, resolve_url_cb, self);
			g_list_free (url_keys);
			return;
		}
	}

	g_warning ("Current element has no URL to play");
}

static void
search_cb (GrlMediaSource *source,
           guint search_id,
           GrlMedia *media,
           guint remaining,
           gpointer user_data,
           const GError *error)
{
	gchar *description;
	GtkTreeIter iter;
	GdkPixbuf *thumbnail;
	GtkWindow *window;
	TotemGriloPlugin *self;

	self = TOTEM_GRILO_PLUGIN (user_data);

	if (error != NULL &&
	    g_error_matches (error,
	                     GRL_CORE_ERROR,
	                     GRL_CORE_ERROR_OPERATION_CANCELLED) == FALSE) {
		window = totem_get_main_window (self->priv->totem);
		totem_interface_error (_("Search Error"), error->message, window);
	}

	if (media != NULL) {
		self->priv->search_remaining--;
		/* Filter images */
		if (GRL_IS_MEDIA_IMAGE (media) == FALSE) {
			thumbnail = get_icon (self, media, THUMB_SEARCH_SIZE);
			description = get_description (media);

			gtk_list_store_append (GTK_LIST_STORE (self->priv->search_results_model), &iter);
			gtk_list_store_set (GTK_LIST_STORE (self->priv->search_results_model),
			                    &iter,
			                    MODEL_RESULTS_SOURCE, source,
			                    MODEL_RESULTS_CONTENT, media,
			                    MODEL_RESULTS_THUMBNAIL, thumbnail,
			                    MODEL_RESULTS_IS_PRETHUMBNAIL, TRUE,
			                    MODEL_RESULTS_DESCRIPTION, description,
			                    -1);

			if (thumbnail != NULL) {
				g_object_unref (thumbnail);
			}
			g_free (description);
		}

		g_object_unref (media);
	}

	if (remaining == 0) {
		self->priv->search_id = 0;
		gtk_widget_set_sensitive (self->priv->search_entry, TRUE);
		update_search_thumbnails (self);
	}
}

static void
search_more (TotemGriloPlugin *self)
{
	GrlOperationOptions *default_options;

	default_options = grl_operation_options_new (NULL);
	grl_operation_options_set_flags (default_options, BROWSE_FLAGS);
	grl_operation_options_set_skip (default_options, (self->priv->search_page - 1) * PAGE_SIZE);
	grl_operation_options_set_count (default_options, PAGE_SIZE);

	gtk_widget_set_sensitive (self->priv->search_entry, FALSE);
	self->priv->search_page++;
	self->priv->search_remaining = PAGE_SIZE;
	if (self->priv->search_source != NULL) {
		self->priv->search_id = grl_media_source_search (self->priv->search_source,
								 self->priv->search_text,
								 search_keys (),
								 default_options,
								 search_cb,
								 self);
	} else {
		self->priv->search_id = grl_multiple_search (NULL,
							     self->priv->search_text,
							     search_keys (),
							     default_options,
							     search_cb,
							     self);
	}
	g_object_unref (default_options);

	if (self->priv->search_id == 0) {
		search_cb (self->priv->search_source, 0, NULL, 0, self, NULL);
	}
}

static void
search (TotemGriloPlugin *self, GrlMediaSource *source, const gchar *text)
{
	gtk_list_store_clear (GTK_LIST_STORE (self->priv->search_results_model));
	g_hash_table_remove_all (self->priv->cache_thumbnails);
	gtk_widget_set_sensitive (self->priv->search_entry, FALSE);
	self->priv->search_source = source;
	g_free (self->priv->search_text);
	self->priv->search_text = g_strdup (text);
	self->priv->search_page = 0;
	search_more (self);
}

static void
search_entry_activate_cb (GtkEntry *entry, TotemGriloPlugin *self)
{
	GrlPluginRegistry *registry;
	const char *id;
	const char *text;
	GrlMediaPlugin *source;

	id = totem_search_entry_get_selected_id (TOTEM_SEARCH_ENTRY (self->priv->search_entry));
	g_return_if_fail (id != NULL);
	registry = grl_plugin_registry_get_default ();
	source = grl_plugin_registry_lookup_source (registry, id);
	g_return_if_fail (source != NULL);

	text = totem_search_entry_get_text (TOTEM_SEARCH_ENTRY (self->priv->search_entry));
	g_return_if_fail (text != NULL);
	search (self, GRL_MEDIA_SOURCE (source), text);
}

static void
browser_activated_cb (GtkTreeView *tree_view,
                      GtkTreePath *path,
                      GtkTreeViewColumn *column,
                      gpointer user_data)
{
	gint remaining;
	gint page;
	GtkTreeModel *model;
	GtkTreeIter iter;
	GrlMedia *content;
	GrlMediaSource *source;
	TotemGriloPlugin *self = TOTEM_GRILO_PLUGIN (user_data);

	model = gtk_tree_view_get_model (tree_view);
	gtk_tree_model_get_iter (model, &iter, path);
	gtk_tree_model_get (model, &iter,
	                    MODEL_RESULTS_SOURCE, &source,
	                    MODEL_RESULTS_CONTENT, &content,
	                    MODEL_RESULTS_PAGE, &page,
	                    MODEL_RESULTS_REMAINING, &remaining,
	                    -1);

	if (content != NULL &&
	    GRL_IS_MEDIA_BOX (content) == FALSE) {
		play (self, source, content, TRUE);
		goto free_data;
	}

	if (gtk_tree_model_iter_has_child (model, &iter)) {
		if (gtk_tree_view_row_expanded (tree_view, path)) {
			gtk_tree_view_collapse_row (tree_view, path);
			gtk_tree_view_columns_autosize (GTK_TREE_VIEW (self->priv->browser));
		} else {
			gtk_tree_view_expand_row (tree_view, path, FALSE);
		}
		goto free_data;
	}

	if (remaining == 0) {
		gtk_tree_store_set (GTK_TREE_STORE (model), &iter,
		                    MODEL_RESULTS_PAGE, ++page,
		                    MODEL_RESULTS_REMAINING, PAGE_SIZE,
		                    -1);
		browse (self, path, source, content, page);
	}

 free_data:
	if (source != NULL) {
		g_object_unref (source);
	}
	if (content != NULL) {
		g_object_unref (content);
	}
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
	gtk_list_store_clear (GTK_LIST_STORE (self->priv->search_results_model));
}

static void
search_activated_cb (GtkIconView *icon_view,
                     GtkTreePath *path,
                     gpointer user_data)
{
	GtkTreeModel *model;
	GtkTreeIter iter;
	GrlMediaSource *source;
	GrlMedia *content;

	model = gtk_icon_view_get_model (icon_view);
	gtk_tree_model_get_iter (model, &iter, path);
	gtk_tree_model_get (model, &iter,
	                    MODEL_RESULTS_SOURCE, &source,
	                    MODEL_RESULTS_CONTENT, &content,
	                    -1);

	play (TOTEM_GRILO_PLUGIN (user_data), source, content, TRUE);

	if (source != NULL) {
		g_object_unref (source);
	}

	if (content != NULL) {
		g_object_unref (content);
	}
}

static gboolean
source_is_blacklisted (GrlMediaSource *source)
{
	const gchar *id = grl_metadata_source_get_id (GRL_METADATA_SOURCE (source));
	const gchar **s = BLACKLIST_SOURCES;

	while (*s) {
		if (g_strcmp0 (*s, id) == 0) {
			return TRUE;
		}
		s++;
	}

	return FALSE;
}

static void
source_added_cb (GrlPluginRegistry *registry,
                 GrlMediaSource *source,
                 gpointer user_data)
{
	const gchar *name;
	gchar *description;
	GdkPixbuf *icon;
	TotemGriloPlugin *self;
	GtkTreeIter iter;
	GrlSupportedOps ops;

	if (source_is_blacklisted (source)) {
		grl_plugin_registry_unregister_source (registry,
		                                       GRL_MEDIA_PLUGIN (source),
		                                       NULL);
		return;
	}

	self = TOTEM_GRILO_PLUGIN (user_data);
	icon = load_icon (self, ICON_BOX, THUMB_BROWSE_SIZE);
	name = grl_metadata_source_get_name (GRL_METADATA_SOURCE (source));
	ops = grl_metadata_source_supported_operations (GRL_METADATA_SOURCE (source));
	if (ops & GRL_OP_BROWSE) {
		description = g_markup_printf_escaped ("<b>%s</b>", name);
		gtk_tree_store_append (GTK_TREE_STORE (self->priv->browser_model), &iter, NULL);
		gtk_tree_store_set (GTK_TREE_STORE (self->priv->browser_model),
		                    &iter,
		                    MODEL_RESULTS_SOURCE, source,
		                    MODEL_RESULTS_CONTENT, NULL,
		                    MODEL_RESULTS_DESCRIPTION, description,
		                    MODEL_RESULTS_THUMBNAIL, icon,
		                    MODEL_RESULTS_IS_PRETHUMBNAIL, TRUE,
		                    -1);
		g_free (description);
	}
	if (ops & GRL_OP_SEARCH) {
		/* FIXME:
		 * Handle tracker/filesystem specifically, so that we have a "local" entry here */
		totem_search_entry_add_source (TOTEM_SEARCH_ENTRY (self->priv->search_entry),
					       grl_metadata_source_get_id (GRL_METADATA_SOURCE (source)),
					       grl_metadata_source_get_name (GRL_METADATA_SOURCE (source)),
					       0); /* FIXME: Use correct priority */
	}

	if (icon != NULL) {
		g_object_unref (icon);
	}
}

static gboolean
remove_browse_result (GtkTreeModel *model,
                      GtkTreePath *path,
                      GtkTreeIter *iter,
                      gpointer user_data)
{
	GrlMediaSource *removed_source = GRL_MEDIA_SOURCE (user_data);
	GrlMediaSource *model_source;
	gboolean same_source;

	gtk_tree_model_get (model, iter,
	                    MODEL_RESULTS_SOURCE, &model_source,
	                    -1);

	same_source = (model_source == removed_source);

	if (same_source) {
		gtk_tree_store_remove (GTK_TREE_STORE (model), iter);
	}

	g_object_unref (model_source);

	return same_source;
}

static void
source_removed_cb (GrlPluginRegistry *registry,
                   GrlMediaSource *source,
                   gpointer user_data)
{
	GrlSupportedOps ops;
	TotemGriloPlugin *self = TOTEM_GRILO_PLUGIN (user_data);

	ops = grl_metadata_source_supported_operations (GRL_METADATA_SOURCE (source));

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
			gtk_list_store_clear (GTK_LIST_STORE (self->priv->search_results_model));
			self->priv->search_source = NULL;
		}

		id = grl_metadata_source_get_id (GRL_METADATA_SOURCE (source));
		totem_search_entry_remove_source (TOTEM_SEARCH_ENTRY (self->priv->search_entry), id);
	}
}

static void
load_grilo_plugins (TotemGriloPlugin *self)
{
	GrlPluginRegistry *registry;
	GError *error = NULL;

	registry = grl_plugin_registry_get_default ();

	g_signal_connect (registry, "source-added",
	                  G_CALLBACK (source_added_cb), self);
	g_signal_connect (registry, "source-removed",
	                  G_CALLBACK (source_removed_cb), self);

	if (grl_plugin_registry_load_all (registry, &error) == FALSE) {
		g_warning ("Failed to load grilo plugins: %s", error->message);
		g_error_free (error);
	}
}

static gboolean
show_popup_menu (TotemGriloPlugin *self, GtkWidget *view, GdkEventButton *event)
{
	GtkWidget *menu;
	gint button = 0;
	guint32 _time;
	GtkAction *action;
	GtkTreeSelection *sel_tree;
	GList *sel_list = NULL;
	GtkTreeModel *model;
	GtkTreeIter iter;
	GrlMediaSource *source;
	const gchar *url = NULL;

	if (view == self->priv->browser) {
		/* Selection happened in browser view */
		sel_tree = gtk_tree_view_get_selection (GTK_TREE_VIEW (view));

		if (gtk_tree_selection_get_selected (sel_tree, &model, &iter) == FALSE) {
			return FALSE;
		}
	} else {
		/* Selection happened in search view */
		sel_list = gtk_icon_view_get_selected_items (GTK_ICON_VIEW (view));
		if (!sel_list) {
			return FALSE;
		}

		model = self->priv->search_results_model;

		gtk_tree_model_get_iter (model,
		                         &iter,
		                         (GtkTreePath *) sel_list->data);

		g_list_foreach (sel_list, (GFunc) gtk_tree_path_free, NULL);
		g_list_free (sel_list);
	}

	/* Get rid of previously selected media */
	if (self->priv->selected_media != NULL) {
		g_object_unref (self->priv->selected_media);
	}

	gtk_tree_model_get (model, &iter,
	                    MODEL_RESULTS_SOURCE, &source,
	                    MODEL_RESULTS_CONTENT, &(self->priv->selected_media),
	                    -1);

	if (event != NULL) {
		button = event->button;
		_time = event->time;
	} else {
		_time = gtk_get_current_event_time ();
	}

	if (self->priv->selected_media != NULL) {
		url = grl_media_get_url (self->priv->selected_media);
	}

	action = gtk_action_group_get_action (self->priv->action_group, "add-to-playlist");
	gtk_action_set_sensitive (action, url != NULL);
	action = gtk_action_group_get_action (self->priv->action_group, "copy-location");
	gtk_action_set_sensitive (action, url != NULL);

	menu = gtk_ui_manager_get_widget (self->priv->ui_manager, "/grilo-popup");
	gtk_menu_shell_select_first (GTK_MENU_SHELL (menu), FALSE);
	gtk_menu_popup (GTK_MENU (menu), NULL, NULL, NULL, NULL,
	                button, _time);

	if (source != NULL) {
		g_object_unref (source);
	}

	return TRUE;
}

static gboolean
popup_menu_cb (GtkWidget *view, TotemGriloPlugin *self)
{
	return show_popup_menu (self, view, NULL);
}

static gboolean
context_button_pressed_cb (GtkWidget *view,
                           GdkEventButton *event,
                           TotemGriloPlugin *self)
{
	if (event->type == GDK_BUTTON_PRESS && event->button == 3) {
		return show_popup_menu (self, view, event);
	}

	return FALSE;
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
adjustment_value_changed_cb (GtkAdjustment *adjustment,
                             TotemGriloPlugin *self)
{
	update_search_thumbnails (self);

	/* Do not get more results if search is in progress */
	if (self->priv->search_id != 0) {
		return;
	}

	/* Do not get more results if there are no more results to get :) */
	if (self->priv->search_remaining > 0) {
		return;
	}

	if (adjustment_over_limit (adjustment)) {
		search_more (self);
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
	GtkTreePath *start_path;
	GtkTreePath *end_path;
	GtkTreePath *parent_path;
	GtkTreeIter iter;
	GrlMediaSource *source;
	GrlMedia *container;
	gint page;
	gint remaining;
	gboolean stop_processing = FALSE;

	if (adjustment_over_limit (adjustment) == FALSE) {
		return;
	}

	if (gtk_tree_view_get_visible_range (GTK_TREE_VIEW (self->priv->browser),
	                                     &start_path, &end_path) == FALSE) {
		return;
	}

	/* Start to check from last visible element, and check if its parent can get more elements */
	while (gtk_tree_path_compare (start_path, end_path) <= 0 &&
	       stop_processing == FALSE) {
		if (gtk_tree_path_get_depth (end_path) <= 1) {
			goto continue_next;
		}
		parent_path = gtk_tree_path_copy (end_path);
		if (gtk_tree_path_up (parent_path) == FALSE ||
		    gtk_tree_model_get_iter (self->priv->browser_model, &iter, parent_path) == FALSE) {
			gtk_tree_path_free (parent_path);
			goto continue_next;
		}
		gtk_tree_model_get (self->priv->browser_model,
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
		browse (self, parent_path, source, container, page);
		stop_processing = TRUE;

	free_elements:
		if (source != NULL) {
			g_object_unref (source);
		}
		if (container != NULL) {
			g_object_unref (container);
		}
		if (parent_path) {
			gtk_tree_path_free (parent_path);
		}

	continue_next:
		stop_processing = stop_processing || (gtk_tree_path_prev (end_path) == FALSE);
	}

	gtk_tree_path_free (start_path);
	gtk_tree_path_free (end_path);
}

static void
setup_sidebar_browse (TotemGriloPlugin *self,
                      GtkBuilder *builder)
{
	self->priv->browser_model = GTK_TREE_MODEL (gtk_builder_get_object (builder, "gw_browse_store_results"));
	self->priv->browser = GTK_WIDGET (gtk_builder_get_object (builder, "gw_browse"));

	g_signal_connect (self->priv->browser, "row-activated",
	                  G_CALLBACK (browser_activated_cb), self);
	g_signal_connect (self->priv->browser, "popup-menu",
	                  G_CALLBACK (popup_menu_cb), self);
	g_signal_connect (self->priv->browser,
	                  "button-press-event",
	                  G_CALLBACK (context_button_pressed_cb), self);
	g_signal_connect (gtk_scrollable_get_vadjustment (GTK_SCROLLABLE (self->priv->browser)),
	                  "value_changed",
	                  G_CALLBACK (get_more_browse_results_cb),
	                  self);

	totem_add_sidebar_page (self->priv->totem,
	                        "grilo-browse", _("Browse"),
	                        GTK_WIDGET (gtk_builder_get_object (builder, "gw_browse_window")));
}

static void
setup_sidebar_search (TotemGriloPlugin *self,
                      GtkBuilder *builder)
{
	self->priv->search_results_model = GTK_TREE_MODEL (gtk_builder_get_object (builder, "gw_search_store_results"));
	self->priv->search_sources_list = GTK_WIDGET (gtk_builder_get_object (builder, "gw_search_select_source"));
	self->priv->search_results_view = GTK_WIDGET (gtk_builder_get_object (builder, "gw_search_results_view"));
	self->priv->search_entry =  GTK_WIDGET (gtk_builder_get_object (builder, "gw_search_text"));

	g_signal_connect (self->priv->search_results_view,
	                  "item-activated",
	                  G_CALLBACK (search_activated_cb),
	                  self);
	g_signal_connect (self->priv->search_results_view,
	                  "popup-menu",
	                  G_CALLBACK (popup_menu_cb), self);
	g_signal_connect (self->priv->search_results_view,
	                  "button-press-event",
	                  G_CALLBACK (context_button_pressed_cb), self);

	g_signal_connect (self->priv->search_entry, "activate",
	                  G_CALLBACK (search_entry_activate_cb),
	                  self);
	g_signal_connect (self->priv->search_entry, "notify::selected-id",
			  G_CALLBACK (search_entry_source_changed_cb), self);

	g_signal_connect (gtk_scrolled_window_get_vadjustment (GTK_SCROLLED_WINDOW (gtk_builder_get_object (builder,
		                    "gw_search_results_window"))),
	                  "value_changed",
	                  G_CALLBACK (adjustment_value_changed_cb),
	                  self);

	g_signal_connect (gtk_scrolled_window_get_vadjustment (GTK_SCROLLED_WINDOW (gtk_builder_get_object (builder,
		                    "gw_search_results_window"))),
	                  "changed",
	                  G_CALLBACK (adjustment_changed_cb),
	                  self);

	totem_add_sidebar_page (self->priv->totem,
	                        "grilo-search", _("Search"),
	                        GTK_WIDGET (gtk_builder_get_object (builder, "gw_search")));
}

static void
add_to_pls_cb (GtkAction *action, TotemGriloPlugin *self)
{
	totem_add_to_playlist_and_play (self->priv->totem,
	                                grl_media_get_url (self->priv->selected_media),
	                                grl_media_get_title (self->priv->selected_media),
	                                TRUE);
}

static void
copy_location_cb (GtkAction *action, TotemGriloPlugin *self)
{
	GtkClipboard *clip;
	const gchar *url;

	url = grl_media_get_url (self->priv->selected_media);
	if (url != NULL) {
		clip = gtk_clipboard_get_for_display (gdk_display_get_default (),
		                                      GDK_SELECTION_CLIPBOARD);
		gtk_clipboard_set_text (clip, url, -1);
		clip = gtk_clipboard_get_for_display (gdk_display_get_default (),
		                                      GDK_SELECTION_PRIMARY);
		gtk_clipboard_set_text (clip, url, -1);
	}
}

static void
setup_menus (TotemGriloPlugin *self,
             GtkBuilder *builder)
{
	GtkAction *action;
	GError *error =NULL;

	self->priv->ui_manager = gtk_ui_manager_new  ();
	self->priv->action_group = gtk_action_group_new ("grilo-action-group");

	action = GTK_ACTION (gtk_builder_get_object (builder, "add-to-playlist"));
	g_signal_connect (action, "activate", G_CALLBACK (add_to_pls_cb), self);
	gtk_action_group_add_action_with_accel (self->priv->action_group, action, NULL);

	action = GTK_ACTION (gtk_builder_get_object (builder, "copy-location"));
	g_signal_connect (action, "activate", G_CALLBACK (copy_location_cb), self);
	gtk_action_group_add_action_with_accel (self->priv->action_group, action, NULL);

	gtk_ui_manager_insert_action_group (self->priv->ui_manager, self->priv->action_group, 1);
	gtk_ui_manager_add_ui_from_string (self->priv->ui_manager,
	                                   GRILO_POPUP_MENU, -1, &error);
	if (error != NULL) {
		g_warning ("grilo-ui: Failed to create popup menu: %s", error->message);
		g_error_free (error);
		return;
	}
}

static void
setup_ui (TotemGriloPlugin *self,
          GtkBuilder *builder)
{
	setup_sidebar_browse (self, builder);
	setup_sidebar_search (self, builder);
	setup_menus (self, builder);
}

static void
setup_config (TotemGriloPlugin *self)
{
	gchar *config_file;
	GrlPluginRegistry *registry = grl_plugin_registry_get_default ();

	/* Setup system-wide plugins configuration */
	config_file = totem_plugin_find_file ("grilo", TOTEM_GRILO_CONFIG_FILE);

	if (g_file_test (config_file, G_FILE_TEST_EXISTS)) {
		grl_plugin_registry_add_config_from_file (registry, config_file, NULL);
	}
	g_free (config_file);

	/* Setup user-defined plugins configuration */
	config_file = g_build_path (G_DIR_SEPARATOR_S,
	                            g_get_user_config_dir (),
	                            g_get_prgname (),
	                            TOTEM_GRILO_CONFIG_FILE,
	                            NULL);

	if (g_file_test (config_file, G_FILE_TEST_EXISTS)) {
		grl_plugin_registry_add_config_from_file (registry, config_file, NULL);
	}
	g_free (config_file);
}

static void
impl_activate (PeasActivatable *plugin)
{
	GtkBuilder *builder;
	GtkWindow *main_window;

	TotemGriloPlugin *self = TOTEM_GRILO_PLUGIN (plugin);
	TotemGriloPluginPrivate *priv = self->priv;
	priv->totem = g_object_ref (g_object_get_data (G_OBJECT (plugin), "object"));
	main_window = totem_get_main_window (priv->totem);
	priv->cache_thumbnails = g_hash_table_new_full (g_str_hash,
	                                                g_str_equal,
	                                                g_free,
	                                                g_object_unref);

	builder = totem_plugin_load_interface ("grilo", "grilo.ui", TRUE, main_window, self);
	g_object_unref (main_window);
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
	GrlPluginRegistry *registry;

	totem_remove_sidebar_page (self->priv->totem, "grilo-browse");
	totem_remove_sidebar_page (self->priv->totem, "grilo-search");

	registry = grl_plugin_registry_get_default ();
	g_signal_handlers_disconnect_by_func (registry, source_added_cb, self);
	g_signal_handlers_disconnect_by_func (registry, source_removed_cb, self);

	/* Shutdown all sources */
	sources  = grl_plugin_registry_get_sources (registry, FALSE);
	for (s = sources; s; s = g_list_next (s)) {
		grl_plugin_registry_unregister_source (registry,
		                                       GRL_MEDIA_PLUGIN (s->data),
		                                       NULL);
	}
	g_list_free (sources);

	/* Empty results */
	gtk_tree_store_clear (GTK_TREE_STORE (self->priv->browser_model));
	gtk_list_store_clear (GTK_LIST_STORE (self->priv->search_results_model));

	g_object_unref (self->priv->totem);
}
