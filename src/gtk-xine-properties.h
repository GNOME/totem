/* gtk-xine-properties.h: Properties dialog for GtkXine

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

#ifndef GTK_XINE_PROPERTIES_H
#define GTK_XINE_PROPERTIES_H

#include <gtk/gtkdialog.h>
#include "gtk-xine.h"

#define GTK_TYPE_XINE_PROPERTIES            (gtk_xine_properties_get_type ())
#define GTK_XINE_PROPERTIES(obj)            (GTK_CHECK_CAST ((obj), GTK_TYPE_XINE_PROPERTIES, GtkXineProperties))
#define GTK_XINE_PROPERTIES_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass), GTK_TYPE_XINE_PROPERTIES, GtkXinePropertiesClass))
#define GTK_IS_XINE_PROPERTIES(obj)         (GTK_CHECK_TYPE ((obj), GTK_TYPE_XINE_PROPERTIES))
#define GTK_IS_XINE_PROPERTIES_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass), GTK_TYPE_XINE_PROPERTIES))

typedef struct GtkXineProperties		GtkXineProperties;
typedef struct GtkXinePropertiesClass		GtkXinePropertiesClass;
typedef struct GtkXinePropertiesPrivate		GtkXinePropertiesPrivate;

struct GtkXineProperties {
	GtkDialog parent;
	GtkXinePropertiesPrivate *priv;
};

struct GtkXinePropertiesClass {
	GtkDialogClass parent_class;
};

GtkType    gtk_xine_properties_get_type	(void);
GtkWidget *gtk_xine_properties_new	();

void gtk_xine_properties_update		(GtkXineProperties *props,
					 GtkXine *gtx, gboolean reset);

#endif /* GTK_XINE_PROPERTIES_H */
