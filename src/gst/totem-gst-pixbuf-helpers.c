/*
 * Copyright (C) 2003-2007 the GStreamer project
 *      Julien Moutte <julien@moutte.net>
 *      Ronald Bultje <rbultje@ronald.bitfreak.net>
 * Copyright (C) 2007 Wim Taymans <wim.taymans@gmail.com>
 * Copyright (C) 2005-2008 Tim-Philipp Müller <tim centricular net>
 * Copyright (C) 2009,2011 Sebastian Dröge <sebastian.droege@collabora.co.uk>
 * Copyright © 2009 Christian Persch
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
 * The Totem project hereby grant permission for non-gpl compatible GStreamer
 * plugins to be used and distributed together with GStreamer and Totem. This
 * permission is above and beyond the permissions granted by the GPL license
 * Totem is covered by.
 *
 * Monday 7th February 2005: Christian Schaller: Add exception clause.
 * See license_change file for details.
 *
 */

#include "totem-gst-pixbuf-helpers.h"

#include <gst/tag/tag.h>
#include <gst/video/video-format.h>
#include <gst/video/gstvideometa.h>

static void
destroy_pixbuf (guchar *pix, gpointer data)
{
  gst_sample_unref (GST_SAMPLE (data));
}

static gboolean
caps_are_raw (const GstCaps * caps)
{
  guint i, len;

  len = gst_caps_get_size (caps);

  for (i = 0; i < len; i++) {
    GstStructure *st = gst_caps_get_structure (caps, i);
    if (gst_structure_has_name (st, "video/x-raw"))
      return TRUE;
  }

  return FALSE;
}

static gboolean
create_element (const gchar * factory_name, GstElement ** element,
    GError ** err)
{
  *element = gst_element_factory_make (factory_name, NULL);
  if (*element)
    return TRUE;

  if (err && *err == NULL) {
    *err = g_error_new (GST_CORE_ERROR, GST_CORE_ERROR_MISSING_PLUGIN,
        "cannot create element '%s' - please check your GStreamer installation",
        factory_name);
  }

  return FALSE;
}

static GstElement *
get_encoder (const GstCaps * caps, GError ** err)
{
  GList *encoders = NULL;
  GList *filtered = NULL;
  GstElementFactory *factory = NULL;
  GstElement *encoder = NULL;

  encoders =
      gst_element_factory_list_get_elements (GST_ELEMENT_FACTORY_TYPE_ENCODER |
      GST_ELEMENT_FACTORY_TYPE_MEDIA_IMAGE, GST_RANK_NONE);

  if (encoders == NULL) {
    *err = g_error_new (GST_CORE_ERROR, GST_CORE_ERROR_MISSING_PLUGIN,
        "Cannot find any image encoder");
    goto fail;
  }

  GST_INFO ("got factory list %p", encoders);
  gst_plugin_feature_list_debug (encoders);

  filtered =
      gst_element_factory_list_filter (encoders, caps, GST_PAD_SRC, FALSE);
  GST_INFO ("got filtered list %p", filtered);

  if (filtered == NULL) {
    gchar *tmp = gst_caps_to_string (caps);
    *err = g_error_new (GST_CORE_ERROR, GST_CORE_ERROR_MISSING_PLUGIN,
        "Cannot find any image encoder for caps %s", tmp);
    g_free (tmp);
    goto fail;
  }

  gst_plugin_feature_list_debug (filtered);

  factory = (GstElementFactory *) filtered->data;

  GST_INFO ("got factory %p", factory);
  encoder = gst_element_factory_create (factory, NULL);

  GST_INFO ("created encoder element %p, %s", encoder,
      GST_ELEMENT_NAME (encoder));

fail:
  if (encoders)
    gst_plugin_feature_list_free (encoders);
  if (filtered)
    gst_plugin_feature_list_free (filtered);

  return encoder;
}

static GstElement *
build_convert_frame_pipeline (GstElement ** src_element,
    GstElement ** sink_element, const GstCaps * from_caps,
    GstVideoCropMeta * cmeta, const GstCaps * to_caps, GError ** err)
{
  GstElement *vcrop = NULL, *csp = NULL, *csp2 = NULL, *vscale = NULL;
  GstElement *src = NULL, *sink = NULL, *encoder = NULL, *pipeline;
  GstVideoInfo info;
  GError *error = NULL;

  if (cmeta) {
    if (!create_element ("videocrop", &vcrop, &error)) {
      g_error_free (error);
      g_warning
          ("build_convert_frame_pipeline: Buffer has crop metadata but videocrop element is not found. Cropping will be disabled");
    } else {
      if (!create_element ("videoconvert", &csp2, &error))
        goto no_elements;
    }
  }

  /* videoscale is here to correct for the pixel-aspect-ratio for us */
  GST_DEBUG ("creating elements");
  if (!create_element ("appsrc", &src, &error) ||
      !create_element ("videoconvert", &csp, &error) ||
      !create_element ("videoscale", &vscale, &error) ||
      !create_element ("appsink", &sink, &error))
    goto no_elements;

  pipeline = gst_pipeline_new ("videoconvert-pipeline");
  if (pipeline == NULL)
    goto no_pipeline;

  /* Add black borders if necessary to keep the DAR */
  g_object_set (vscale, "add-borders", TRUE, NULL);

  GST_DEBUG ("adding elements");
  gst_bin_add_many (GST_BIN (pipeline), src, csp, vscale, sink, NULL);
  if (vcrop)
    gst_bin_add_many (GST_BIN (pipeline), vcrop, csp2, NULL);

  /* set caps */
  g_object_set (src, "caps", from_caps, NULL);
  if (vcrop) {
    gst_video_info_from_caps (&info, from_caps);
    g_object_set (vcrop, "left", cmeta->x, NULL);
    g_object_set (vcrop, "top", cmeta->y, NULL);
    g_object_set (vcrop, "right", GST_VIDEO_INFO_WIDTH (&info) - cmeta->width,
        NULL);
    g_object_set (vcrop, "bottom",
        GST_VIDEO_INFO_HEIGHT (&info) - cmeta->height, NULL);
    GST_DEBUG ("crop meta [x,y,width,height]: %d %d %d %d", cmeta->x, cmeta->y,
        cmeta->width, cmeta->height);
  }
  g_object_set (sink, "caps", to_caps, NULL);

  /* FIXME: linking is still way too expensive, profile this properly */
  if (vcrop) {
    GST_DEBUG ("linking src->csp2");
    if (!gst_element_link_pads (src, "src", csp2, "sink"))
      goto link_failed;

    GST_DEBUG ("linking csp2->vcrop");
    if (!gst_element_link_pads (csp2, "src", vcrop, "sink"))
      goto link_failed;

    GST_DEBUG ("linking vcrop->csp");
    if (!gst_element_link_pads (vcrop, "src", csp, "sink"))
      goto link_failed;
  } else {
    GST_DEBUG ("linking src->csp");
    if (!gst_element_link_pads (src, "src", csp, "sink"))
      goto link_failed;
  }

  GST_DEBUG ("linking csp->vscale");
  if (!gst_element_link_pads_full (csp, "src", vscale, "sink",
          GST_PAD_LINK_CHECK_NOTHING))
    goto link_failed;

  if (caps_are_raw (to_caps)) {
    GST_DEBUG ("linking vscale->sink");

    if (!gst_element_link_pads_full (vscale, "src", sink, "sink",
            GST_PAD_LINK_CHECK_NOTHING))
      goto link_failed;
  } else {
    encoder = get_encoder (to_caps, &error);
    if (!encoder)
      goto no_encoder;
    gst_bin_add (GST_BIN (pipeline), encoder);

    GST_DEBUG ("linking vscale->encoder");
    if (!gst_element_link (vscale, encoder))
      goto link_failed;

    GST_DEBUG ("linking encoder->sink");
    if (!gst_element_link_pads (encoder, "src", sink, "sink"))
      goto link_failed;
  }

  g_object_set (src, "emit-signals", TRUE, NULL);
  g_object_set (sink, "emit-signals", TRUE, NULL);

  *src_element = src;
  *sink_element = sink;

  return pipeline;
  /* ERRORS */
no_encoder:
  {
    gst_object_unref (pipeline);

    GST_ERROR ("could not find an encoder for provided caps");
    if (err)
      *err = error;
    else
      g_error_free (error);

    return NULL;
  }
no_elements:
  {
    if (src)
      gst_object_unref (src);
    if (vcrop)
      gst_object_unref (vcrop);
    if (csp)
      gst_object_unref (csp);
    if (csp2)
      gst_object_unref (csp2);
    if (vscale)
      gst_object_unref (vscale);
    if (sink)
      gst_object_unref (sink);
    GST_ERROR ("Could not convert video frame: %s", error->message);
    if (err)
      *err = error;
    else
      g_error_free (error);
    return NULL;
  }
no_pipeline:
  {
    gst_object_unref (src);
    if (vcrop)
      gst_object_unref (vcrop);
    gst_object_unref (csp);
    if (csp2)
      gst_object_unref (csp2);
    gst_object_unref (vscale);
    gst_object_unref (sink);

    GST_ERROR ("Could not convert video frame: no pipeline (unknown error)");
    if (err)
      *err = g_error_new (GST_CORE_ERROR, GST_CORE_ERROR_FAILED,
          "Could not convert video frame: no pipeline (unknown error)");
    return NULL;
  }
link_failed:
  {
    gst_object_unref (pipeline);

    GST_ERROR ("Could not convert video frame: failed to link elements");
    if (err)
      *err = g_error_new (GST_CORE_ERROR, GST_CORE_ERROR_NEGOTIATION,
          "Could not convert video frame: failed to link elements");
    return NULL;
  }
}

/**
 * totem_gst_video_convert_sample:
 * @sample: a #GstSample
 * @to_caps: the #GstCaps to convert to
 * @timeout: the maximum amount of time allowed for the processing.
 * @error: pointer to a #GError. Can be %NULL.
 *
 * Converts a raw video buffer into the specified output caps.
 *
 * The output caps can be any raw video formats or any image formats (jpeg, png, ...).
 *
 * The width, height and pixel-aspect-ratio can also be specified in the output caps.
 *
 * Returns: The converted #GstSample, or %NULL if an error happened (in which case @err
 * will point to the #GError).
 */
static GstSample *
totem_gst_video_convert_sample (GstSample * sample, const GstCaps * to_caps,
    GstClockTime timeout, GError ** error)
{
  GstMessage *msg;
  GstBuffer *buf;
  GstSample *result = NULL;
  GError *err = NULL;
  GstBus *bus;
  GstCaps *from_caps, *to_caps_copy = NULL;
  GstFlowReturn ret;
  GstElement *pipeline, *src, *sink;
  guint i, n;

  g_return_val_if_fail (sample != NULL, NULL);
  g_return_val_if_fail (to_caps != NULL, NULL);

  buf = gst_sample_get_buffer (sample);
  g_return_val_if_fail (buf != NULL, NULL);

  from_caps = gst_sample_get_caps (sample);
  g_return_val_if_fail (from_caps != NULL, NULL);

  to_caps_copy = gst_caps_new_empty ();
  n = gst_caps_get_size (to_caps);
  for (i = 0; i < n; i++) {
    GstStructure *s = gst_caps_get_structure (to_caps, i);

    s = gst_structure_copy (s);
    gst_structure_remove_field (s, "framerate");
    gst_caps_append_structure (to_caps_copy, s);
  }

  pipeline =
      build_convert_frame_pipeline (&src, &sink, from_caps,
      gst_buffer_get_video_crop_meta (buf), to_caps_copy, &err);
  if (!pipeline)
    goto no_pipeline;

  /* now set the pipeline to the paused state, after we push the buffer into
   * appsrc, this should preroll the converted buffer in appsink */
  GST_DEBUG ("running conversion pipeline to caps %" GST_PTR_FORMAT,
      to_caps_copy);
  if (gst_element_set_state (pipeline,
          GST_STATE_PAUSED) == GST_STATE_CHANGE_FAILURE)
    goto state_change_failed;

  /* feed buffer in appsrc */
  GST_DEBUG ("feeding buffer %p, size %" G_GSIZE_FORMAT ", caps %"
      GST_PTR_FORMAT, buf, gst_buffer_get_size (buf), from_caps);
  g_signal_emit_by_name (src, "push-buffer", buf, &ret);

  /* now see what happens. We either got an error somewhere or the pipeline
   * prerolled */
  bus = gst_element_get_bus (pipeline);
  msg = gst_bus_timed_pop_filtered (bus,
      timeout, GST_MESSAGE_ERROR | GST_MESSAGE_ASYNC_DONE);

  if (msg) {
    switch (GST_MESSAGE_TYPE (msg)) {
      case GST_MESSAGE_ASYNC_DONE:
      {
        /* we're prerolled, get the frame from appsink */
        g_signal_emit_by_name (sink, "pull-preroll", &result);

        if (result) {
          GST_DEBUG ("conversion successful: result = %p", result);
        } else {
          GST_ERROR ("prerolled but no result frame?!");
        }
        break;
      }
      case GST_MESSAGE_ERROR:{
        gchar *dbg = NULL;

        gst_message_parse_error (msg, &err, &dbg);
        if (err) {
          GST_ERROR ("Could not convert video frame: %s", err->message);
          GST_DEBUG ("%s [debug: %s]", err->message, GST_STR_NULL (dbg));
          if (error)
            *error = err;
          else
            g_error_free (err);
        }
        g_free (dbg);
        break;
      }
      default:{
        g_return_val_if_reached (NULL);
      }
    }
    gst_message_unref (msg);
  } else {
    GST_ERROR ("Could not convert video frame: timeout during conversion");
    if (error)
      *error = g_error_new (GST_CORE_ERROR, GST_CORE_ERROR_FAILED,
          "Could not convert video frame: timeout during conversion");
  }

  gst_element_set_state (pipeline, GST_STATE_NULL);
  gst_object_unref (bus);
  gst_object_unref (pipeline);
  gst_caps_unref (to_caps_copy);

  return result;

  /* ERRORS */
no_pipeline:
state_change_failed:
  {
    gst_caps_unref (to_caps_copy);

    if (error)
      *error = err;
    else
      g_error_free (err);

    return NULL;
  }
}

GdkPixbuf *
totem_gst_playbin_get_frame (GstElement *play, GError **error)
{
  GstStructure *s;
  GstSample *sample = NULL;
  GstSample *last_sample = NULL;
  GdkPixbuf *pixbuf = NULL;
  GstCaps *to_caps, *sample_caps;
  gint outwidth = 0;
  gint outheight = 0;
  GstMemory *memory;
  GstMapInfo info;
  GdkPixbufRotation rotation = GDK_PIXBUF_ROTATE_NONE;

  g_return_val_if_fail (play != NULL, NULL);
  g_return_val_if_fail (GST_IS_ELEMENT (play), NULL);

  /* our desired output format (RGB24) */
  to_caps = gst_caps_new_simple ("video/x-raw",
      "format", G_TYPE_STRING, "RGB",
      /* Note: we don't ask for a specific width/height here, so that
       * videoscale can adjust dimensions from a non-1/1 pixel aspect
       * ratio to a 1/1 pixel-aspect-ratio. We also don't ask for a
       * specific framerate, because the input framerate won't
       * necessarily match the output framerate if there's a deinterlacer
       * in the pipeline. */
      "pixel-aspect-ratio", GST_TYPE_FRACTION, 1, 1,
      NULL);

  /* get frame */
  g_object_get (G_OBJECT (play), "sample", &last_sample, NULL);
  if (!last_sample) {
    g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                         "Failed to retrieve video frame");
    return NULL;
  }
  sample = totem_gst_video_convert_sample (last_sample, to_caps, 25 * GST_SECOND, error);
  gst_sample_unref (last_sample);
  gst_caps_unref (to_caps);

  if (!sample) {
    g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                         "Failed to convert video frame");
    return NULL;
  }

  sample_caps = gst_sample_get_caps (sample);
  if (!sample_caps) {
    g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                         "No caps on output buffer");
    return NULL;
  }

  GST_DEBUG ("frame caps: %" GST_PTR_FORMAT, sample_caps);

  s = gst_caps_get_structure (sample_caps, 0);
  gst_structure_get_int (s, "width", &outwidth);
  gst_structure_get_int (s, "height", &outheight);
  if (outwidth <= 0 || outheight <= 0) {
    g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                 "Could not prepare buffer memory for image dimensions %dx%d", outwidth, outheight);
    goto done;
  }

  memory = gst_buffer_get_memory (gst_sample_get_buffer (sample), 0);
  gst_memory_map (memory, &info, GST_MAP_READ);

  /* create pixbuf from that - use our own destroy function */
  pixbuf = gdk_pixbuf_new_from_data (info.data,
      GDK_COLORSPACE_RGB, FALSE, 8, outwidth, outheight,
      GST_ROUND_UP_4 (outwidth * 3), destroy_pixbuf, sample);

  gst_memory_unmap (memory, &info);
  gst_memory_unref (memory);

done:
  if (!pixbuf) {
    g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                         "Could not create pixbuf");
    gst_sample_unref (sample);
  }

  /* Did we check whether we need to rotate the video? */
  if (g_object_get_data (G_OBJECT (play), "orientation-checked") == NULL) {
    GstTagList *tags = NULL;

    g_signal_emit_by_name (G_OBJECT (play), "get-video-tags", 0, &tags);
    if (tags) {
      char *orientation_str;
      gboolean ret;

      ret = gst_tag_list_get_string_index (tags, GST_TAG_IMAGE_ORIENTATION, 0, &orientation_str);
      if (!ret || !orientation_str)
        rotation = GDK_PIXBUF_ROTATE_NONE;
      else if (g_str_equal (orientation_str, "rotate-90"))
        rotation = GDK_PIXBUF_ROTATE_CLOCKWISE;
      else if (g_str_equal (orientation_str, "rotate-180"))
        rotation = GDK_PIXBUF_ROTATE_UPSIDEDOWN;
      else if (g_str_equal (orientation_str, "rotate-270"))
        rotation = GDK_PIXBUF_ROTATE_COUNTERCLOCKWISE;

      gst_tag_list_unref (tags);
    }

    g_object_set_data (G_OBJECT (play), "orientation-checked", GINT_TO_POINTER(1));
    g_object_set_data (G_OBJECT (play), "orientation", GINT_TO_POINTER(rotation));
  }

  rotation = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (play), "orientation"));
  if (rotation != GDK_PIXBUF_ROTATE_NONE) {
    GdkPixbuf *rotated;

    rotated = gdk_pixbuf_rotate_simple (pixbuf, rotation);
    if (rotated) {
      g_object_unref (pixbuf);
      pixbuf = rotated;
    }
  }

  return pixbuf;
}

static GdkPixbuf *
totem_gst_buffer_to_pixbuf (GstBuffer *buffer)
{
  GdkPixbufLoader *loader;
  GdkPixbuf *pixbuf = NULL;
  GError *err = NULL;
  GstMapInfo info;

  if (!gst_buffer_map (buffer, &info, GST_MAP_READ)) {
    GST_WARNING("could not map memory buffer");
    return NULL;
  }

  loader = gdk_pixbuf_loader_new ();

  if (gdk_pixbuf_loader_write (loader, info.data, info.size, &err) &&
      gdk_pixbuf_loader_close (loader, &err)) {
    pixbuf = gdk_pixbuf_loader_get_pixbuf (loader);
    if (pixbuf)
      g_object_ref (pixbuf);
  } else {
    GST_WARNING("could not convert tag image to pixbuf: %s", err->message);
    g_error_free (err);
  }

  g_object_unref (loader);

  gst_buffer_unmap (buffer, &info);

  return pixbuf;
}

static GstSample *
totem_gst_tag_list_get_cover_real (GstTagList *tag_list)
{
  GstSample *cover_sample = NULL;
  guint i;

  for (i = 0; ; i++) {
    GstSample *sample;
    GstCaps *caps;
    const GstStructure *caps_struct;
    int type = GST_TAG_IMAGE_TYPE_UNDEFINED;

    if (!gst_tag_list_get_sample_index (tag_list, GST_TAG_IMAGE, i, &sample))
      break;

    caps = gst_sample_get_caps (sample);
    caps_struct = gst_caps_get_structure (caps, 0);
    gst_structure_get_enum (caps_struct,
			    "image-type",
			    GST_TYPE_TAG_IMAGE_TYPE,
			    &type);
    if (type == GST_TAG_IMAGE_TYPE_UNDEFINED) {
      if (cover_sample == NULL) {
	/* take a ref here since we will continue and unref below */
	cover_sample = gst_sample_ref (sample);
      }
    } else if (type == GST_TAG_IMAGE_TYPE_FRONT_COVER) {
      cover_sample = sample;
      break;
    }
    gst_sample_unref (sample);
  }

  return cover_sample;
}

GdkPixbuf *
totem_gst_tag_list_get_cover (GstTagList *tag_list)
{
  GstSample *cover_sample;

  g_return_val_if_fail (tag_list != NULL, FALSE);

  cover_sample = totem_gst_tag_list_get_cover_real (tag_list);
  /* Fallback to preview */
    if (!cover_sample) {
      gst_tag_list_get_sample_index (tag_list, GST_TAG_PREVIEW_IMAGE, 0,
				     &cover_sample);
    }

  if (cover_sample) {
    GstBuffer *buffer;
    GdkPixbuf *pixbuf;

    buffer = gst_sample_get_buffer (cover_sample);
    pixbuf = totem_gst_buffer_to_pixbuf (buffer);
    gst_sample_unref (cover_sample);
    return pixbuf;
  }

  return NULL;
}

/*
 * vim: sw=2 ts=8 cindent noai bs=2
 */
