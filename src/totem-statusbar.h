/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 * TotemStatusbar Copyright (C) 1998 Shawn T. Amundson
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 */

#ifndef __TOTEM_STATUSBAR_H__
#define __TOTEM_STATUSBAR_H__

#include <gtk/gtkhbox.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


#define TOTEM_TYPE_STATUSBAR            (totem_statusbar_get_type ())
#define TOTEM_STATUSBAR(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), TOTEM_TYPE_STATUSBAR, TotemStatusbar))
#define TOTEM_STATUSBAR_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), TOTEM_TYPE_STATUSBAR, TotemStatusbarClass))
#define TOTEM_IS_STATUSBAR(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), TOTEM_TYPE_STATUSBAR))
#define TOTEM_IS_STATUSBAR_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), TOTEM_TYPE_STATUSBAR))
#define TOTEM_STATUSBAR_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), TOTEM_TYPE_STATUSBAR, TotemStatusbarClass))


typedef struct _TotemStatusbar      TotemStatusbar;
typedef struct _TotemStatusbarClass TotemStatusbarClass;

struct _TotemStatusbar
{
  GtkHBox parent_widget;

  GtkWidget *frame;
  GtkWidget *label;
  GtkWidget *time_label;

  gint time;
  gint length;
  char *saved_label;
  guint timeout;

  GdkWindow *grip_window;
  
  guint has_resize_grip : 1;
  guint pushed : 1;
};

struct _TotemStatusbarClass
{
  GtkHBoxClass parent_class;
};


GType      totem_statusbar_get_type     	(void) G_GNUC_CONST;
GtkWidget* totem_statusbar_new          	(void);

void       totem_statusbar_set_time		(TotemStatusbar *statusbar,
						 gint time);
void       totem_statusbar_set_time_and_length	(TotemStatusbar *statusbar,
						 gint time, gint length);

void       totem_statusbar_set_text             (TotemStatusbar *statusbar,
						 const char *label);
void	   totem_statusbar_push			(TotemStatusbar *statusbar,
						 guint percentage);
void       totem_statusbar_pop			(TotemStatusbar *statusbar);

void     totem_statusbar_set_has_resize_grip (TotemStatusbar *statusbar,
					    gboolean      setting);
gboolean totem_statusbar_get_has_resize_grip (TotemStatusbar *statusbar);

#ifdef __cplusplus
} 
#endif /* __cplusplus */
#endif /* __TOTEM_STATUSBAR_H__ */
