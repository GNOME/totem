/*
 * Copyright (C) 2001,2002,2003,2004,2005 Bastien Nocera <hadess@hadess.net>
 *
 * SPDX-License-Identifier: GPL-3-or-later
 *
 */

#pragma once

#include <gst/gst.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

GdkPixbuf * totem_gst_playbin_get_frame (GstElement *play, GError **error);
GdkPixbuf * totem_gst_tag_list_get_cover (GstTagList *tag_list);
