/* bacon-video-widget-gst-missing-plugins.h

   Copyright (C) 2007 Tim-Philipp Müller <tim centricular net>

   The Gnome Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   The Gnome Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with the Gnome Library; see the file COPYING.LIB.  If not,
   write to the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301  USA.

   Author: Tim-Philipp Müller <tim centricular net>
 */

#pragma once

#include "bacon-video-widget.h"

void bacon_video_widget_gst_missing_plugins_setup (BaconVideoWidget *bvw);
void bacon_video_widget_gst_missing_plugins_block (void);
