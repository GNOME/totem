/*
 *  Copyright (C) 2004 Bastien Nocera <hadess@hadess.net>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 */

#ifndef __TOTEM_STOCK_ICONS_H
#define __TOTEM_STOCK_ICONS_H

#include <gdk-pixbuf/gdk-pixbuf.h>
#include "totem.h"

G_BEGIN_DECLS

void       totem_named_icons_init		(Totem *totem,
						 gboolean refresh);
GdkPixbuf *totem_get_named_icon_for_id		(const char *id);
void       totem_named_icons_dispose		(Totem *totem);
void       totem_set_default_icons		(Totem *totem);

G_END_DECLS

#endif /* __TOTEM_STOCK_ICONS_H */
