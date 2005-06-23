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
#include "gstscreenshot.h"

static GstFlowReturn
have_data (GstPad * pad, GstBuffer * buf)
{
  g_object_set_data (G_OBJECT (pad), "data", buf);
  return GST_FLOW_OK;
}

static GstCaps *
get_any (GstPad * pad)
{
  return gst_caps_new_any ();
}

GstBuffer *
bvw_frame_conv_convert (GstBuffer * buf,
    GstCaps * from, GstCaps * to)
{
  GstElement *pipe, *csp, *scl, *flt;
  GstPad *in, *out, *tmp1, *tmp2;
  GstBuffer *ret;
  GstCaps *tcaps;

  /* setup */
  pipe = gst_pipeline_new ("test");
  csp = gst_element_factory_make ("ffmpegcolorspace", "csp");
  scl = gst_element_factory_make ("identity", "scl");
  flt = gst_element_factory_make ("capsfilter", "flt");
  if (!csp || !scl || !flt) {
    g_warning ("missing elements, please fix installation");
    return NULL;
  }
  gst_bin_add_many (GST_BIN (pipe), csp, scl, flt, NULL);

  /* own pads */
  in = gst_pad_new ("in", GST_PAD_SRC);
  out = gst_pad_new ("out", GST_PAD_SINK);
  gst_pad_set_chain_function (out, have_data);
  gst_pad_set_getcaps_function (in, get_any);
  gst_pad_set_getcaps_function (out, get_any);
  gst_element_add_pad (pipe, in);
  gst_element_add_pad (pipe, out);

  /* link */
  g_object_set (G_OBJECT (flt), "filter-caps", to, NULL);
  tmp1 = gst_element_get_pad (csp, "sink");
  tmp2 = gst_element_get_pad (flt, "src");
  if (gst_pad_link (in, tmp1) < 0 ||
      !gst_element_link_pads (csp, "src", scl, "sink") ||
      !gst_element_link_pads (scl, "src", flt, "sink") ||
      gst_pad_link (tmp2, out) < 0) {
    g_warning ("Setup failed");
    return NULL;
  }
  gst_object_unref (GST_OBJECT (tmp1));
  gst_object_unref (GST_OBJECT (tmp2));

  /* activate */
  if (gst_element_set_state (pipe, GST_STATE_PAUSED) != GST_STATE_SUCCESS) {
    g_warning ("Failed to set state on elements - fixme");
    return NULL;
  } else if (!gst_pad_set_caps (in, from)) {
    gchar *s = gst_caps_to_string (from);
    g_warning ("Pad did not accept %s", s);
    g_free (s);
    return NULL;
  }

  /* now push data and the chain function will be called */
  if (gst_pad_push (in, buf) != GST_FLOW_OK) {
    g_warning ("Wrong state - fixme");
    return NULL;
  }
  ret = g_object_get_data (G_OBJECT (out), "data");
  tcaps = gst_pad_get_negotiated_caps (out);
  if (!gst_caps_is_equal (tcaps, to)) {
    gchar *s1 = gst_caps_to_string (tcaps), *s2 = gst_caps_to_string (to);
    g_warning ("Out caps (%s) is not what we requested (%s)", s1, s2);
    g_free (s1);
    g_free (s1);
    gst_buffer_unref (ret);
    return NULL;
  }
  gst_caps_unref (tcaps);

  /* unref */
  gst_object_unref (GST_OBJECT (pipe));

  return ret;
}
