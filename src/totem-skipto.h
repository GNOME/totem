/* totem-skipto.h

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

#ifndef TOTEM_SKIPTO_H
#define TOTEM_SKIPTO_H

#include <gtk/gtkdialog.h>

G_BEGIN_DECLS

#define GTK_TYPE_SKIPTO            (totem_skipto_get_type ())
#define TOTEM_SKIPTO(obj)            (GTK_CHECK_CAST ((obj), GTK_TYPE_SKIPTO, TotemSkipto))
#define TOTEM_SKIPTO_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass), GTK_TYPE_SKIPTO, TotemSkiptoClass))
#define GTK_IS_SKIPTO(obj)         (GTK_CHECK_TYPE ((obj), GTK_TYPE_SKIPTO))
#define GTK_IS_SKIPTO_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass), GTK_TYPE_SKIPTO))

typedef struct TotemSkipto	       TotemSkipto;
typedef struct TotemSkiptoClass      TotemSkiptoClass;
typedef struct TotemSkiptoPrivate    TotemSkiptoPrivate;

struct TotemSkipto {
	GtkDialog parent;
	TotemSkiptoPrivate *_priv;
};

struct TotemSkiptoClass {
	GtkDialogClass parent_class;
};

GtkType    totem_skipto_get_type (void);
GtkWidget *totem_skipto_new      (const char *glade_filename);
gint64 totem_skipto_get_range    (TotemSkipto *skipto);
void totem_skipto_update_range   (TotemSkipto *skipto, gint64 time);
void totem_skipto_set_seekable   (TotemSkipto *skipto, gboolean seekable);

G_END_DECLS

#endif /* TOTEM_SKIPTO_H */
