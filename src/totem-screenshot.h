/* totem-screenshot.h: Simple screenshot dialog

   Copyright (C) 2004 Bastien Nocera <hadess@hadess.net>

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

#ifndef TOTEM_SCREENSHOT_H
#define TOTEM_SCREENSHOT_H

#include <gtk/gtkdialog.h>

G_BEGIN_DECLS

#define GTK_TYPE_SCREENSHOT            (totem_screenshot_get_type ())
#define TOTEM_SCREENSHOT(obj)            (GTK_CHECK_CAST ((obj), GTK_TYPE_SCREENSHOT, TotemScreenshot))
#define TOTEM_SCREENSHOT_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass), GTK_TYPE_SCREENSHOT, TotemScreenshotClass))
#define GTK_IS_SCREENSHOT(obj)         (GTK_CHECK_TYPE ((obj), GTK_TYPE_SCREENSHOT))
#define GTK_IS_SCREENSHOT_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass), GTK_TYPE_SCREENSHOT))

typedef struct TotemScreenshot	       TotemScreenshot;
typedef struct TotemScreenshotClass      TotemScreenshotClass;
typedef struct TotemScreenshotPrivate    TotemScreenshotPrivate;

struct TotemScreenshot {
	GtkDialog parent;
	TotemScreenshotPrivate *_priv;
};

struct TotemScreenshotClass {
	GtkDialogClass parent_class;
};

GtkType    totem_screenshot_get_type (void);
GtkWidget *totem_screenshot_new      (const char *glade_filename,
				      GdkPixbuf *playing_pix);

G_END_DECLS

#endif /* TOTEM_SCREENSHOT_H */
