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
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301  USA.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/gst.h>
#include <string.h>

#include "gstscreenshot.h"

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
    goto bail;
  }

  pipeline = gst_pipeline_new ("screenshot-pipeline");
  if (pipeline == NULL) {
    g_warning ("Could not take screenshot: %s", "no pipeline (unknown error)");
    goto bail;
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
    goto bail;

  GST_DEBUG ("linking csp->filter1");
  if (!gst_element_link_pads (csp, "src", filter1, "sink"))
    goto bail;

  GST_DEBUG ("linking filter1->vscale");
  if (!gst_element_link_pads (filter1, "src", vscale, "sink"))
    goto bail;

  GST_DEBUG ("linking vscale->capsfilter");
  if (!gst_element_link_pads (vscale, "src", filter2, "sink"))
    goto bail;

  GST_DEBUG ("linking capsfilter->sink");
  if (!gst_element_link_pads (filter2, "src", sink, "sink"))
    goto bail;

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

bail:
  gst_buffer_unref (buf);

  return result;
}

