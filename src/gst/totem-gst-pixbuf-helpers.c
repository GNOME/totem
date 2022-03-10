/*
 * Copyright (C) 2003-2007 the GStreamer project
 *      Julien Moutte <julien@moutte.net>
 *      Ronald Bultje <rbultje@ronald.bitfreak.net>
 * Copyright (C) 2007 Wim Taymans <wim.taymans@gmail.com>
 * Copyright (C) 2005-2008 Tim-Philipp Müller <tim centricular net>
 * Copyright (C) 2009,2011 Sebastian Dröge <sebastian.droege@collabora.co.uk>
 * Copyright © 2009 Christian Persch
 *
 * SPDX-License-Identifier: GPL-3-or-later
 *
 */

#include "totem-gst-pixbuf-helpers.h"

#include <gst/tag/tag.h>
#include <gst/video/video-format.h>

typedef enum {
  FRAME_CAPTURE_TYPE_RAW,
  FRAME_CAPTURE_TYPE_GL
} FrameCaptureType;

static void
destroy_pixbuf (guchar *pix, gpointer data)
{
  gst_sample_unref (GST_SAMPLE (data));
}

GdkPixbuf *
totem_gst_playbin_get_frame (GstElement *play, GError **error)
{
  FrameCaptureType capture_type;
  GstStructure *s;
  GstSample *sample = NULL;
  guint bpp;
  GdkPixbuf *pixbuf = NULL;
  GstCaps *to_caps, *sample_caps;
  gint outwidth = 0;
  gint outheight = 0;
  GstMemory *memory;
  GstMapInfo info;
  GdkPixbufRotation rotation = GDK_PIXBUF_ROTATE_NONE;

  g_return_val_if_fail (play != NULL, NULL);
  g_return_val_if_fail (GST_IS_ELEMENT (play), NULL);

  capture_type = gst_bin_get_by_name (GST_BIN (play), "glcolorbalance0") ?
    FRAME_CAPTURE_TYPE_GL : FRAME_CAPTURE_TYPE_RAW;

  /* our desired output format (RGB24) */
  to_caps = gst_caps_new_simple ("video/x-raw",
      "format", G_TYPE_STRING, capture_type == FRAME_CAPTURE_TYPE_RAW ? "RGB" : "RGBA",
      /* Note: we don't ask for a specific width/height here, so that
       * videoscale can adjust dimensions from a non-1/1 pixel aspect
       * ratio to a 1/1 pixel-aspect-ratio. We also don't ask for a
       * specific framerate, because the input framerate won't
       * necessarily match the output framerate if there's a deinterlacer
       * in the pipeline. */
      "pixel-aspect-ratio", GST_TYPE_FRACTION, 1, 1,
      NULL);

  /* get frame */
  g_signal_emit_by_name (play, "convert-sample", to_caps, &sample);
  gst_caps_unref (to_caps);

  if (!sample) {
    g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                         "Failed to retrieve or convert video frame");
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
  bpp = capture_type == FRAME_CAPTURE_TYPE_GL ? 4 : 3;
  pixbuf = gdk_pixbuf_new_from_data (info.data,
      GDK_COLORSPACE_RGB, capture_type == FRAME_CAPTURE_TYPE_GL, 8, outwidth, outheight,
      GST_ROUND_UP_4 (outwidth * bpp), destroy_pixbuf, sample);

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
