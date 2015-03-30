/*
 * Copyright (C) 2015 Bastien Nocera
 *
 * Contact: Bastien Nocera <hadess@hadess.net>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; version 2.1 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <grilo.h>
#include <gio/gio.h>
#include <glib/gi18n-lib.h>
#include <string.h>
#include <stdlib.h>

#include "grl-totem.h"

/* --------- Logging  -------- */

#define GRL_LOG_DOMAIN_DEFAULT totem_log_domain
GRL_LOG_DOMAIN_STATIC(totem_log_domain);

/* --- Plugin information --- */

#define PLUGIN_ID   "grl-totem"

#define SOURCE_ID   "grl-totem"
#define SOURCE_NAME _("Local")
#define SOURCE_DESC "An aggregating source for local videos"

#define MAX_DURATION          5

/* --- Grilo Totem Private --- */

#define GRL_TOTEM_SOURCE_GET_PRIVATE(object)           \
	(G_TYPE_INSTANCE_GET_PRIVATE((object),                       \
				     GRL_TOTEM_SOURCE_TYPE,  \
				     GrlTotemSourcePrivate))

struct _GrlTotemSourcePrivate {
	/* The list of initial operations, to populate our cache */
	GPtrArray *op_array;
	/* While initial operations are on-going, the list of application
	 * browses or searches we'll need to fulfill */
	GPtrArray *pending_browse;
	GPtrArray *pending_search;

	GList *metadata_keys;
	GrlSource *local_metadata_src;
	GrlSource *title_parsing_src;
	GrlSource *metadata_store_src;

	/* Top-level media (eg. anything that's not a series) */
	GPtrArray *entries;
};

static const char * const sources[] = {
	"grl-tracker-source",
	"grl-optical-media",
	"grl-bookmarks",
	"grl-local-metadata",
	"grl-video-title-parsing",
	"grl-metadata-store"
};

/* --- Data types --- */

static GrlTotemSource *grl_totem_source_new (void);

static void grl_totem_source_finalize (GObject *object);

gboolean grl_totem_plugin_init (GrlRegistry *registry,
				GrlPlugin   *plugin,
				GList       *configs);

static const GList *grl_totem_source_supported_keys (GrlSource *source);

static void grl_totem_source_browse (GrlSource           *source,
				     GrlSourceBrowseSpec *bs);
static void grl_totem_source_search (GrlSource           *source,
				     GrlSourceSearchSpec *ss);
static void grl_totem_source_store  (GrlSource           *source,
				     GrlSourceStoreSpec  *ss);
static void grl_totem_source_remove (GrlSource           *source,
				     GrlSourceRemoveSpec *rs);

/* =================== Totem Plugin  =============== */

static void
source_added_cb (GrlRegistry *registry,
		 GrlSource   *source,
		 gpointer     user_data)
{
	GrlPlugin *plugin = user_data;
	guint i;
	gboolean found = FALSE;
	GPtrArray *array;

	for (i = 0; i < G_N_ELEMENTS (sources); i++) {
		if (g_strcmp0 (grl_source_get_id (source), sources[i]) == 0) {
			found = TRUE;
			break;
		}
	}

	if (!found)
		return;

	array = g_object_get_data (G_OBJECT (plugin), "sources");
	g_ptr_array_add (array, g_object_ref (source));

	if (array->len == G_N_ELEMENTS (sources)) {
		GrlSource *source = GRL_SOURCE (grl_totem_source_new ());

		grl_registry_register_source (registry,
					      plugin,
					      GRL_SOURCE (source),
					      NULL);
	}
}

static void
source_removed_cb (GrlRegistry *registry,
		   GrlSource   *source,
		   gpointer     user_data)
{
	GrlPlugin *plugin = user_data;
	GPtrArray *array;

	array = g_object_get_data (G_OBJECT (plugin), "sources");
	if (g_ptr_array_remove (array, source)) {
		GrlSource *source = grl_registry_lookup_source (registry, SOURCE_ID);
		grl_registry_unregister_source (registry, source, NULL);
	}
}

gboolean
grl_totem_plugin_init (GrlRegistry *registry,
		       GrlPlugin   *plugin,
		       GList       *configs)
{
	GPtrArray *array;
	GList *sources, *l;

	GRL_LOG_DOMAIN_INIT (totem_log_domain, "totem");

	GRL_DEBUG ("%s", __FUNCTION__);

	array = g_ptr_array_new_with_free_func (g_object_unref);
	g_object_set_data_full (G_OBJECT (plugin), "sources", array, (GDestroyNotify) g_ptr_array_unref);

	g_signal_connect (registry, "source-added",
			  G_CALLBACK (source_added_cb), plugin);
	g_signal_connect (registry, "source-removed",
			  G_CALLBACK (source_removed_cb), plugin);

	sources = grl_registry_get_sources (registry, FALSE);
	for (l = sources; l != NULL; l = l->next) {
		source_added_cb (registry, l->data, plugin);
	}
	g_list_free (sources);

	return TRUE;
}

gboolean
grl_totem_plugin_wraps_source (GrlSource *source)
{
	guint i;

	for (i = 0; i < G_N_ELEMENTS (sources); i++) {
		if (g_strcmp0 (grl_source_get_id (source), sources[i]) == 0) {
			return TRUE;
		}
	}
	return FALSE;
}

static GrlPluginDescriptor descriptor = {
	.major_version = 0,
	.minor_version = 3,
	.id = PLUGIN_ID,
	.name = PLUGIN_ID,
	.init = grl_totem_plugin_init,
};

GrlPluginDescriptor *
grl_totem_plugin_get_descriptor (void)
{
	return &descriptor;
}

/* ================== Totem GObject ================ */


G_DEFINE_TYPE (GrlTotemSource,
	       grl_totem_source,
	       GRL_TYPE_SOURCE);

static GrlTotemSource *
grl_totem_source_new (void)
{
	GRL_DEBUG ("%s", __FUNCTION__);

	return g_object_new (GRL_TOTEM_SOURCE_TYPE,
			     "source-id", SOURCE_ID,
			     "source-name", SOURCE_NAME,
			     "source-desc", SOURCE_DESC,
			     "supported-media", GRL_SUPPORTED_MEDIA_VIDEO,
			     NULL);
}

static gboolean
grl_totem_source_notify_change_start_stop (GrlSource *source,
					   GError **error)
{
	return TRUE;
}

static void
grl_totem_source_class_init (GrlTotemSourceClass * klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GrlSourceClass *source_class = GRL_SOURCE_CLASS (klass);

	object_class->finalize = grl_totem_source_finalize;

	source_class->supported_keys = grl_totem_source_supported_keys;
	source_class->browse = grl_totem_source_browse;
	source_class->search = grl_totem_source_search;
	source_class->store = grl_totem_source_store;
	source_class->remove = grl_totem_source_remove;
	source_class->notify_change_start = grl_totem_source_notify_change_start_stop;
	source_class->notify_change_stop = grl_totem_source_notify_change_start_stop;

	g_type_class_add_private (klass, sizeof (GrlTotemSourcePrivate));
}

static void
grl_totem_source_process_search (GrlTotemSource      *source,
				 GrlSourceSearchSpec *ss)
{
	//FIXME
}

static void
grl_totem_source_process_browse (GrlTotemSource      *source,
				 GrlSourceBrowseSpec *bs)
{
	guint skip;
	guint count;
	guint remaining;
	GPtrArray *entries;

	entries = source->priv->entries;

	if (entries->len) {
		skip = grl_operation_options_get_skip (bs->options);
		if (skip > entries->len)
			skip = entries->len;

		count = grl_operation_options_get_count (bs->options);
		if (skip + count > entries->len)
			count = entries->len - skip;

		remaining = MIN (entries->len - skip, count);
	} else {
		skip = 0;
		count = 0;
		remaining = 0;
	}

	GRL_DEBUG ("%s, skip: %d, count: %d, remaining: %d, num entries: %d",
		   __FUNCTION__, skip, count, remaining, entries->len);

	if (remaining) {
		guint i;

		for (i = 0; i < count; i++) {
			GrlMedia *content;

			content = g_ptr_array_index (entries, skip + i);
			g_object_ref (content);
			remaining--;
			bs->callback (bs->source,
				      bs->operation_id,
				      content,
				      remaining,
				      bs->user_data,
				      NULL);
			GRL_DEBUG ("callback called source=%p id=%d content=%p remaining=%d user_data=%p",
				   bs->source, bs->operation_id, content, remaining, bs->user_data);
		}
	} else {
		bs->callback (bs->source,
			      bs->operation_id,
			      NULL,
			      0,
			      bs->user_data,
			      NULL);
	}
}

static gboolean
sources_content_loaded (GrlTotemSource *source)
{
	return (source->priv->op_array->len == 0);
}

static void
add_local_metadata (GrlTotemSource *totem_source,
		    GrlSource      *source,
		    GrlMedia       *media)
{
	GrlOperationOptions *options;

	/* Avoid trying to get metadata for web radios */
	if (g_strcmp0 (grl_source_get_id (source), "grl-bookmarks") == 0) {
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
	grl_source_resolve_sync (totem_source->priv->title_parsing_src,
				 media,
				 totem_source->priv->metadata_keys,
				 options,
				 NULL);
	grl_source_resolve_sync (totem_source->priv->local_metadata_src,
				 media,
				 totem_source->priv->metadata_keys,
				 options,
				 NULL);
	grl_source_resolve_sync (totem_source->priv->metadata_store_src,
				 media,
				 totem_source->priv->metadata_keys,
				 options,
				 NULL);
	g_object_unref (options);
}

static gboolean
filter_out_media (GrlMedia *media)
{
	if (grl_media_is_image (media) ||
	    grl_media_is_audio (media)) {
		return TRUE;
	}

	return FALSE;
}

static void
browse_internal_cb (GrlSource    *source,
		    guint         operation_id,
		    GrlMedia     *media,
		    guint         remaining,
		    gpointer      user_data,
		    const GError *error)
{
	GrlTotemSource *totem_source = GRL_TOTEM_SOURCE (user_data);
	GrlTotemSourcePrivate *priv = totem_source->priv;

	if (media && !filter_out_media (media)) {
		add_local_metadata (totem_source, source, media);
		g_ptr_array_add (priv->entries, g_object_ref (media));
	}

	if (error != NULL || remaining == 0) {
		g_ptr_array_remove (priv->op_array, GUINT_TO_POINTER (operation_id));

		/* Did we get all the data from the underlying sources */
		if (sources_content_loaded (totem_source)) {
			guint i;

			for (i = 0; i < priv->pending_browse->len; i++) {
				grl_totem_source_process_browse (totem_source, priv->pending_browse->pdata[0]);
				g_ptr_array_remove_index (priv->pending_browse, 0);
			}
			for (i = 0; i < priv->pending_search->len; i++) {
				grl_totem_source_process_search (totem_source, priv->pending_search->pdata[0]);
				g_ptr_array_remove_index (priv->pending_search, 0);
			}
		}
	}
}

static void
content_changed_cb (GrlSource           *s,
		    GPtrArray           *changed_medias,
		    GrlSourceChangeType  change_type,
		    gboolean             location_unknown,
		    GrlTotemSource      *source)
{
	/* This will need fixing when we start monitoring tracker again */
	grl_source_notify_change_list (GRL_SOURCE (source), changed_medias, change_type, location_unknown);
}

static void
grl_totem_source_init (GrlTotemSource *source)
{
	GrlTotemSourcePrivate *priv;
	GrlRegistry *registry;
	guint i;

	priv = source->priv = GRL_TOTEM_SOURCE_GET_PRIVATE(source);
	priv->op_array = g_ptr_array_new ();
	priv->entries = g_ptr_array_new_with_free_func (g_object_unref);
	priv->pending_browse = g_ptr_array_new ();
	priv->pending_search = g_ptr_array_new ();

	priv->metadata_keys = grl_metadata_key_list_new (GRL_METADATA_KEY_ARTIST,
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

	registry = grl_registry_get_default ();

	for (i = 0; i < G_N_ELEMENTS (sources); i++) {
		GrlOperationOptions *default_options;
		GrlSupportedOps ops;
		GrlCaps *caps;
		guint opid;
		GrlSource *s;

		s = grl_registry_lookup_source (registry, sources[i]);
		ops = grl_source_supported_operations (s);
		if (!(ops & GRL_OP_BROWSE)) {
			const char *id;

			id = grl_source_get_id (s);
			if (g_str_equal (id, "grl-video-title-parsing"))
				source->priv->title_parsing_src = s;
			else if (g_str_equal (id, "grl-local-metadata"))
				source->priv->local_metadata_src = s;
			else if (g_str_equal (id, "grl-metadata-store"))
				source->priv->metadata_store_src = s;
			continue;
		}

		caps = grl_source_get_caps (s, GRL_OP_BROWSE);

		default_options = grl_operation_options_new (NULL);
		grl_operation_options_set_resolution_flags (default_options, GRL_RESOLVE_FAST_ONLY | GRL_RESOLVE_IDLE_RELAY);
		if (grl_caps_get_type_filter (caps) & GRL_TYPE_FILTER_VIDEO)
			grl_operation_options_set_type_filter (default_options, GRL_TYPE_FILTER_VIDEO);
		if (grl_caps_is_key_range_filter (caps, GRL_METADATA_KEY_DURATION))
			grl_operation_options_set_key_range_filter (default_options,
								    GRL_METADATA_KEY_DURATION, MAX_DURATION, NULL,
								    NULL);

		opid = grl_source_browse (s,
					  NULL,
					  grl_totem_source_supported_keys (GRL_SOURCE (source)),
					  default_options,
					  browse_internal_cb,
					  source);
		g_object_unref (default_options);

		g_ptr_array_add (priv->op_array, GUINT_TO_POINTER (opid));

		/* Until the Tracker plugin is fixed:
		 * https://bugzilla.gnome.org/show_bug.cgi?id=746974 */
		if (g_str_equal (grl_source_get_id (s), "grl-tracker-source"))
			continue;

		grl_source_notify_change_start (s, NULL);
		g_signal_connect (G_OBJECT (s), "content-changed",
				  G_CALLBACK (content_changed_cb), source);
	}
}

static void
grl_totem_source_finalize (GObject *object)
{
	GrlTotemSource *source = GRL_TOTEM_SOURCE (object);
	GrlTotemSourcePrivate *priv = GRL_TOTEM_SOURCE (source)->priv;
	guint i;

	g_clear_pointer (&priv->metadata_keys, g_list_free);
	for (i = 0; i < priv->op_array->len; i++) {
		grl_operation_cancel (GPOINTER_TO_UINT (priv->op_array->pdata[i]));
	}
	g_ptr_array_unref (priv->op_array);
	g_ptr_array_unref (priv->entries);
	g_ptr_array_unref (priv->pending_browse);
	g_ptr_array_unref (priv->pending_search);

	G_OBJECT_CLASS (grl_totem_source_parent_class)->finalize (object);
}

/* ================== API Implementation ================ */

static const GList *
grl_totem_source_supported_keys (GrlSource *source)
{
	GrlTotemSourcePrivate *priv = GRL_TOTEM_SOURCE (source)->priv;
	return priv->metadata_keys;
}

static void
grl_totem_source_browse (GrlSource           *source,
			 GrlSourceBrowseSpec *bs)
{
	GrlTotemSourcePrivate *priv = GRL_TOTEM_SOURCE (source)->priv;

	if (!sources_content_loaded (GRL_TOTEM_SOURCE (source))) {
		g_ptr_array_add (priv->pending_browse, bs);
	} else {
		grl_totem_source_process_browse (GRL_TOTEM_SOURCE (source), bs);
	}
}

static void
grl_totem_source_search (GrlSource           *source,
			 GrlSourceSearchSpec *ss)
{
	GrlTotemSourcePrivate *priv = GRL_TOTEM_SOURCE (source)->priv;

	if (!sources_content_loaded (GRL_TOTEM_SOURCE (source))) {
		g_ptr_array_add (priv->pending_search, ss);
	} else {
		grl_totem_source_process_search (GRL_TOTEM_SOURCE (source), ss);
	}
}

static void
grl_totem_source_store (GrlSource          *source,
			GrlSourceStoreSpec *ss)
{
	//FIXME impl
}

static void
grl_totem_source_remove (GrlSource           *source,
			 GrlSourceRemoveSpec *rs)
{
	//FIXME
}
