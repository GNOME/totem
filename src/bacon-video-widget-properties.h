/* gtk-xine-properties.h: Properties dialog for BaconVideoWidget

   Copyright (C) 2002 Bastien Nocera <hadess@hadess.net>

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
   write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.

   Author: Bastien Nocera <hadess@hadess.net>
 */

#ifndef BACON_VIDEO_WIDGET_PROPERTIES_H
#define BACON_VIDEO_WIDGET_PROPERTIES_H

#include <gtk/gtkdialog.h>
#include "bacon-video-widget.h"

#define GTK_TYPE_XINE_PROPERTIES            (bacon_video_widget_properties_get_type ())
#define BACON_VIDEO_WIDGET_PROPERTIES(obj)            (GTK_CHECK_CAST ((obj), GTK_TYPE_XINE_PROPERTIES, BaconVideoWidgetProperties))
#define BACON_VIDEO_WIDGET_PROPERTIES_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass), GTK_TYPE_XINE_PROPERTIES, BaconVideoWidgetPropertiesClass))
#define BACON_IS_VIDEO_WIDGET_PROPERTIES(obj)         (GTK_CHECK_TYPE ((obj), GTK_TYPE_XINE_PROPERTIES))
#define BACON_IS_VIDEO_WIDGET_PROPERTIES_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass), GTK_TYPE_XINE_PROPERTIES))

typedef struct BaconVideoWidgetProperties		BaconVideoWidgetProperties;
typedef struct BaconVideoWidgetPropertiesClass		BaconVideoWidgetPropertiesClass;
typedef struct BaconVideoWidgetPropertiesPrivate		BaconVideoWidgetPropertiesPrivate;

struct BaconVideoWidgetProperties {
	GtkDialog parent;
	BaconVideoWidgetPropertiesPrivate *priv;
};

struct BaconVideoWidgetPropertiesClass {
	GtkDialogClass parent_class;
};

GtkType    bacon_video_widget_properties_get_type	(void);
GtkWidget *bacon_video_widget_properties_new	();

void bacon_video_widget_properties_update		(BaconVideoWidgetProperties *props,
					 BaconVideoWidget *bvw, const char *name, gboolean reset);

char      *bacon_video_widget_properties_time_to_string
					(int time);

#endif /* BACON_VIDEO_WIDGET_PROPERTIES_H */
