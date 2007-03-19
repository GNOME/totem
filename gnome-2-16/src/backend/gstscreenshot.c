/* Small helper element for format conversion
 * (c) 2004 Ronald Bultje <rbultje@ronald.bitfreak.net>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/gst.h>
#include <string.h>

#include "gstscreenshot.h"

#ifdef HAVE_GSTREAMER_010

GST_DEBUG_CATEGORY_EXTERN (_totem_gst_debug_cat);
#define GST_CAT_DEFAULT _totem_gst_debug_cat

static void
feed_fakesrc (GstElement * src, GstBuffer * buf, GstPad * pad, gpointer data)
{
  GstBuffer *in_buf = GST_BUFFER (data);

  g_assert (GST_BUFFER_SIZE (buf) >= GST_BUFFER_SIZE (in_buf));
  g_assert (!GST_BUFFER_FLAG_IS_SET (buf, GST_BUFFER_FLAG_READONLY));

  gst_buffer_set_caps (buf, GST_BUFFER_CAPS (in_buf));

  memcpy (GST_BUFFER_DATA (buf), GST_BUFFER_DATA (in_buf),
      GST_BUFFER_SIZE (in_buf));

  GST_BUFFER_SIZE (buf) = GST_BUFFER_SIZE (in_buf);

  GST_DEBUG ("feeding buffer %p, size %u, caps %" GST_PTR_FORMAT,
      buf, GST_BUFFER_SIZE (buf), GST_BUFFER_CAPS (buf));
}

static void
save_result (GstElement * sink, GstBuffer * buf, GstPad * pad, gpointer data)
{
  GstBuffer **p_buf = (GstBuffer **) data;

  *p_buf = gst_buffer_ref (buf);

  GST_DEBUG ("received converted buffer %p with caps %" GST_PTR_FORMAT,
      *p_buf, GST_BUFFER_CAPS (*p_buf));
}

static gboolean
create_element (const gchar *factory_name, GstElement **element, GError **err)
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

/* takes ownership of the input buffer */
GstBuffer *
bvw_frame_conv_convert (GstBuffer * buf, GstCaps * to_caps)
{
  GstElement *src, *csp, *filter1, *vscale, *filter2, *sink, *pipeline;
  GstMessage *msg;
  GstBuffer *result = NULL;
  GError *error = NULL;
  GstBus *bus;
  GstCaps *to_caps_no_par;

  g_return_val_if_fail (GST_BUFFER_CAPS (buf) != NULL, NULL);

  /* videoscale is here to correct for the pixel-aspect-ratio for us */
  GST_DEBUG ("creating elements");
  if (!create_element ("fakesrc", &src, &error) ||
      !create_element ("ffmpegcolorspace", &csp, &error) ||
      !create_element ("videoscale", &vscale, &error) ||
      !create_element ("capsfilter", &filter1, &error) ||
      !create_element ("capsfilter", &filter2, &error) ||
      !create_element ("fakesink", &sink, &error)) {
    g_warning ("Could not take screenshot: %s", error->message);
    g_error_free (error);
    return NULL;
  }

  pipeline = gst_pipeline_new ("screenshot-pipeline");
  if (pipeline == NULL) {
    g_warning ("Could not take screenshot: %s", "no pipeline (unknown error)");
    return NULL;
  }

  GST_DEBUG ("adding elements");
  gst_bin_add_many (GST_BIN (pipeline), src, csp, filter1, vscale, filter2,
      sink, NULL);

  g_signal_connect (src, "handoff", G_CALLBACK (feed_fakesrc), buf);

  /* set to 'fixed' sizetype */
  g_object_set (src, "sizemax", GST_BUFFER_SIZE (buf), "sizetype", 2,
      "num-buffers", 1, "signal-handoffs", TRUE, NULL);

  /* adding this superfluous capsfilter makes linking cheaper */
  to_caps_no_par = gst_caps_copy (to_caps);
  gst_structure_remove_field (gst_caps_get_structure (to_caps_no_par, 0),
      "pixel-aspect-ratio");
  g_object_set (filter1, "caps", to_caps_no_par, NULL);
  gst_caps_unref (to_caps_no_par);

  g_object_set (filter2, "caps", to_caps, NULL);

  g_signal_connect (sink, "handoff", G_CALLBACK (save_result), &result);

  g_object_set (sink, "preroll-queue-len", 1, "signal-handoffs", TRUE, NULL);

  /* FIXME: linking is still way too expensive, profile this properly */
  GST_DEBUG ("linking src->csp");
  if (!gst_element_link_pads (src, "src", csp, "sink"))
    return NULL;

  GST_DEBUG ("linking csp->filter1");
  if (!gst_element_link_pads (csp, "src", filter1, "sink"))
    return NULL;

  GST_DEBUG ("linking filter1->vscale");
  if (!gst_element_link_pads (filter1, "src", vscale, "sink"))
    return NULL;

  GST_DEBUG ("linking vscale->capsfilter");
  if (!gst_element_link_pads (vscale, "src", filter2, "sink"))
    return NULL;

  GST_DEBUG ("linking capsfilter->sink");
  if (!gst_element_link_pads (filter2, "src", sink, "sink"))
    return NULL;

  GST_DEBUG ("running conversion pipeline");
  gst_element_set_state (pipeline, GST_STATE_PLAYING);

  bus = gst_element_get_bus (pipeline);
  msg = gst_bus_poll (bus, GST_MESSAGE_ERROR | GST_MESSAGE_EOS, 25*GST_SECOND);

  if (msg) {
    switch (GST_MESSAGE_TYPE (msg)) {
      case GST_MESSAGE_EOS: {
        if (result) {
          GST_DEBUG ("conversion successful: result = %p", result);
        } else {
          GST_WARNING ("EOS but no result frame?!");
        }
        break;
      }
      case GST_MESSAGE_ERROR: {
        gchar *dbg = NULL;

        gst_message_parse_error (msg, &error, &dbg);
        if (error) {
          g_warning ("Could not take screenshot: %s", error->message);
          GST_DEBUG ("%s [debug: %s]", error->message, GST_STR_NULL (dbg));
          g_error_free (error);
        } else {
          g_warning ("Could not take screenshot (and NULL error!)");
        }
        g_free (dbg);
        result = NULL;
        break;
      }
      default: {
        g_return_val_if_reached (NULL);
      }
    }
  } else {
    g_warning ("Could not take screenshot: %s", "timeout during conversion");
    result = NULL;
  }

  gst_element_set_state (pipeline, GST_STATE_NULL);
  gst_object_unref (pipeline);

  return result;
}

#else /* HAVE_GSTREAMER_010 */

#define BVW_TYPE_FRAME_CONV \
  (bvw_frame_conv_get_type ())
#define BVW_FRAME_CONV(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), BVW_TYPE_FRAME_CONV, BvwFrameConv))
#define BVW_FRAME_CONV_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), BVW_TYPE_FRAME_CONV, BvwFrameConvClass))
#define BVW_IS_FRAME_CONV(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), BVW_TYPE_FRAME_CONV))
#define BVW_IS_FRAME_CONV_CLASS(obj) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), BVW_TYPE_FRAME_CONV))

typedef struct _BvwFrameConv {
  GstElement parent;

  GstPad *src;
  GstBuffer *in, *out;
} BvwFrameConv;

typedef struct _BvwFrameConvClass {
  GstElementClass klass;
} BvwFrameConvClass;

static GType	bvw_frame_conv_get_type	(void);
static GstData *bvw_frame_conv_get	(GstPad * pad);
static void	bvw_frame_conv_put	(GstElement * el,
    GstBuffer * buf, GstPad * pad, gpointer data);

GST_BOILERPLATE (BvwFrameConv, bvw_frame_conv, GstElement, GST_TYPE_ELEMENT);

static void
bvw_frame_conv_base_init (gpointer klass)
{
  static GstElementDetails details = {
    "Frame format conversion bin",
    "General/Bin",
    "Converts frames between colorspace formats",
    "Ronald Bultje <rbultje@ronald.bitfreak.net>",
  };

  gst_element_class_set_details (klass, &details);
}

static void
bvw_frame_conv_class_init (BvwFrameConvClass * klass)
{
}

static void
bvw_frame_conv_init (BvwFrameConv * conv)
{
  conv->src = gst_pad_new ("src", GST_PAD_SRC);
  gst_pad_use_explicit_caps (conv->src);
  gst_pad_set_get_function (conv->src, bvw_frame_conv_get);
  gst_element_add_pad (GST_ELEMENT (conv), conv->src);

  conv->in = conv->out = NULL;
}

static GstData *
bvw_frame_conv_get (GstPad * pad)
{
  BvwFrameConv *conv = BVW_FRAME_CONV (gst_pad_get_parent (pad));
  GstData *ret;

  if (conv->in) {
    ret = GST_DATA (conv->in);
    conv->in = NULL;
  } else {
    ret = GST_DATA (gst_event_new (GST_EVENT_EOS));
    gst_element_set_eos (GST_ELEMENT (conv));
  }

  return ret;
}

static void
bvw_frame_conv_put (GstElement * el,
    GstBuffer * buf, GstPad * pad, gpointer data)
{
  BvwFrameConv *conv = BVW_FRAME_CONV (data);

  g_assert (conv->out == NULL);
  conv->out = gst_buffer_ref (buf);
}

GstBuffer *
bvw_frame_conv_convert (GstBuffer * buf,
    GstCaps * from, GstCaps * to)
{
  GstElement *pipe, *csp, *scl, *sink;
  BvwFrameConv *conv;
  GstBuffer *ret;

  /* simple setup: source, colorspace, scale, sink */
  pipe = gst_pipeline_new ("pipe");
  conv = g_object_new (BVW_TYPE_FRAME_CONV, NULL);
  gst_object_set_name (GST_OBJECT (conv), "conv");
  csp = gst_element_factory_make ("ffmpegcolorspace", "csp");
  scl = gst_element_factory_make ("videoscale", "scl");
  sink = gst_element_factory_make ("fakesink", "sink");
  g_object_set (G_OBJECT (sink), "signal-handoffs", TRUE, NULL);
  if (!csp || !scl || !sink) {
    g_warning ("missing elements, please fix installation");
    return NULL;
  }
  if (!gst_element_link_pads (GST_ELEMENT (conv),
          "src", csp, "sink") ||
      !gst_element_link_pads (csp, "src", scl, "sink") ||
      !gst_element_link_pads_filtered (scl, "src",
           sink, "sink", to)) {
    g_warning ("link failed");
    return NULL;
  }
  gst_bin_add_many (GST_BIN (pipe),
      GST_ELEMENT (conv), csp, scl, sink, NULL);
  g_signal_connect (sink, "handoff",
      G_CALLBACK (bvw_frame_conv_put), conv);
  conv->in = buf;

  /* set to paused (ready for nego), set explicit caps
   * on source and set caps on output. */
  if (gst_element_set_state (GST_ELEMENT (pipe),
          GST_STATE_PAUSED) != GST_STATE_SUCCESS ||
      !gst_pad_set_explicit_caps (conv->src, from) ||
      gst_element_set_state (GST_ELEMENT (pipe),
          GST_STATE_PLAYING) != GST_STATE_SUCCESS) {
    gst_object_unref (GST_OBJECT (pipe));
    gst_buffer_unref (buf);
    return NULL;
  }

  /* iterate until we've got output */
  while (conv->in != NULL || conv->out == NULL) {
    if (!gst_bin_iterate (GST_BIN (pipe)))
      break;
  }
  ret = conv->out;

  /* clean up */
  gst_element_set_state (GST_ELEMENT (pipe), GST_STATE_NULL);
  gst_object_unref (GST_OBJECT (pipe));

  return ret;
}

#endif /* HAVE_GSTREAMER_010 */
