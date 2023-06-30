/*
 * Copyright (C) 2003-2007 the GStreamer project
 *      Julien Moutte <julien@moutte.net>
 *      Ronald Bultje <rbultje@ronald.bitfreak.net>
 * Copyright (C) 2005-2008 Tim-Philipp Müller <tim centricular net>
 * Copyright (C) 2009 Sebastian Dröge <sebastian.droege@collabora.co.uk>
 * Copyright © 2009 Christian Persch
 *
 * SPDX-License-Identifier: GPL-3-or-later
 *
 */

#include "totem-gst-helpers.h"

#include <gst/tag/tag.h>
#include <gst/video/video-format.h>

void
totem_gst_message_print (GstMessage *msg,
			 GstElement *play,
			 const char *filename)
{
  GError *err = NULL;
  char *dbg = NULL;

  g_return_if_fail (GST_MESSAGE_TYPE (msg) == GST_MESSAGE_ERROR);

  if (play != NULL) {
    g_return_if_fail (filename != NULL);

    GST_DEBUG_BIN_TO_DOT_FILE (GST_BIN_CAST (play),
			       GST_DEBUG_GRAPH_SHOW_ALL ^ GST_DEBUG_GRAPH_SHOW_NON_DEFAULT_PARAMS,
			       filename);
  }

  gst_message_parse_error (msg, &err, &dbg);
  if (err) {
    char *uri;

    g_object_get (play, "uri", &uri, NULL);
    GST_ERROR ("message = %s", GST_STR_NULL (err->message));
    GST_ERROR ("domain  = %d (%s)", err->domain,
        GST_STR_NULL (g_quark_to_string (err->domain)));
    GST_ERROR ("code    = %d", err->code);
    GST_ERROR ("debug   = %s", GST_STR_NULL (dbg));
    GST_ERROR ("source  = %" GST_PTR_FORMAT, msg->src);
    GST_ERROR ("uri     = %s", GST_STR_NULL (uri));
    g_free (uri);

    g_error_free (err);
  }
  g_free (dbg);
}

static gboolean
filter_hw_decoders (GstPluginFeature *feature,
		    gpointer          user_data)
{
  GstElementFactory *factory;

  if (!GST_IS_ELEMENT_FACTORY (feature))
    return FALSE;

  factory = GST_ELEMENT_FACTORY (feature);
  if (!gst_element_factory_list_is_type (factory,
					 GST_ELEMENT_FACTORY_TYPE_DECODER |
					 GST_ELEMENT_FACTORY_TYPE_MEDIA_VIDEO |
					 GST_ELEMENT_FACTORY_TYPE_MEDIA_IMAGE)) {
    return FALSE;
  }

  return gst_element_factory_list_is_type (factory,
					   GST_ELEMENT_FACTORY_TYPE_HARDWARE);
}

void
totem_gst_disable_hardware_decoders (void)
{
  GstRegistry *registry;
  g_autolist(GstPluginFeature) hw_list = NULL;
  GList *l;

  registry = gst_registry_get ();
  hw_list = gst_registry_feature_filter (registry, filter_hw_decoders, FALSE, NULL);
  for (l = hw_list; l != NULL; l = l->next) {
    g_debug ("Disabling feature %s",
	     gst_plugin_feature_get_name (l->data));
    gst_registry_remove_feature (registry, l->data);
  }
}

void
totem_gst_ensure_newer_hardware_decoders (void)
{
  GstRegistry *registry;
  g_autolist(GstPluginFeature) hw_list = NULL;
  GList *l;

  registry = gst_registry_get ();
  hw_list = gst_registry_feature_filter (registry, filter_hw_decoders, FALSE, NULL);
  for (l = hw_list; l != NULL; l = l->next) {
    const char *name;
    name = gst_plugin_feature_get_plugin_name (l->data);
    if (g_strcmp0 (name, "va") != 0)
      continue;
    g_debug ("Bumping feature %s of plugin %s to MAX",
             gst_plugin_feature_get_name (l->data), name);
    gst_plugin_feature_set_rank (l->data, UINT_MAX);
  }
}

/*
 * vim: sw=2 ts=8 cindent noai bs=2
 */
