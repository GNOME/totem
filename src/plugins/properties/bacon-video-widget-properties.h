/* bacon-video-widget-properties.h: Properties dialog for BaconVideoWidget

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

#include <gtk/gtkvbox.h>

#define BACON_TYPE_VIDEO_WIDGET_PROPERTIES            (bacon_video_widget_properties_get_type ())
#define BACON_VIDEO_WIDGET_PROPERTIES(obj)            (GTK_CHECK_CAST ((obj), BACON_TYPE_VIDEO_WIDGET_PROPERTIES, BaconVideoWidgetProperties))
#define BACON_VIDEO_WIDGET_PROPERTIES_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass), BACON_TYPE_VIDEO_WIDGET_PROPERTIES, BaconVideoWidgetPropertiesClass))
#define BACON_IS_VIDEO_WIDGET_PROPERTIES(obj)         (GTK_CHECK_TYPE ((obj), BACON_TYPE_VIDEO_WIDGET_PROPERTIES))
#define BACON_IS_VIDEO_WIDGET_PROPERTIES_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass), BACON_TYPE_VIDEO_WIDGET_PROPERTIES))

typedef struct BaconVideoWidgetProperties		BaconVideoWidgetProperties;
typedef struct BaconVideoWidgetPropertiesClass		BaconVideoWidgetPropertiesClass;
typedef struct BaconVideoWidgetPropertiesPrivate	BaconVideoWidgetPropertiesPrivate;

struct BaconVideoWidgetProperties {
	GtkVBox parent;
	BaconVideoWidgetPropertiesPrivate *priv;
};

struct BaconVideoWidgetPropertiesClass {
	GtkVBoxClass parent_class;
};

GtkType bacon_video_widget_properties_get_type		(void);
GtkWidget *bacon_video_widget_properties_new		(void);

void bacon_video_widget_properties_reset		(BaconVideoWidgetProperties *props);
void bacon_video_widget_properties_update		(BaconVideoWidgetProperties *props,
							 GtkWidget *bvw);
void bacon_video_widget_properties_from_metadata	(BaconVideoWidgetProperties *props,
							 const char *title,
							 const char *artist,
							 const char *album);
void bacon_video_widget_properties_from_time		(BaconVideoWidgetProperties *props,
							 int time);

char *bacon_video_widget_properties_time_to_string	(int time);

#endif /* BACON_VIDEO_WIDGET_PROPERTIES_H */
