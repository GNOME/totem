/* 
   Copyright (C) 2004, Bastien Nocera <hadess@hadess.net>

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

   Author: Bastien Nocera <hadess@hadess.net>, Philip Withnall <philip@tecnocode.co.uk>
 */

#include <glib.h>
#include <glib-object.h>

#define TOTEM_TYPE_GALAGO		(totem_galago_get_type ())
#define TOTEM_GALAGO(obj)		(G_TYPE_CHECK_INSTANCE_CAST ((obj), TOTEM_TYPE_GALAGO, TotemGalago))
#define TOTEM_GALAGO_CLASS(klass)	(G_TYPE_CHECK_CLASS_CAST ((klass), TOTEM_TYPE_GALAGO, TotemGalagoClass))
#define TOTEM_IS_GALAGO(obj)		(G_TYPE_CHECK_INSTANCE_TYPE ((obj), TOTEM_TYPE_GALAGO))
#define TOTEM_IS_GALAGO_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE ((klass), TOTEM_TYPE_GALAGO))

typedef struct TotemGalago TotemGalago;
typedef struct TotemGalagoClass TotemGalagoClass;
typedef struct TotemGalagoPrivate TotemGalagoPrivate;

struct TotemGalago {
	GObject parent;
	TotemGalagoPrivate *priv;
};

struct TotemGalagoClass {
	GObjectClass parent_class; 
};

GType totem_galago_get_type		(void);
TotemGalago *totem_galago_new		(void);
void totem_galago_idle			(TotemGalago *scr);
void totem_galago_not_idle		(TotemGalago *scr);
void totem_galago_set_idleness		(TotemGalago *scr, gboolean idle);

