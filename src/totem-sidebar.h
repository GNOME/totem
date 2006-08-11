/* totem-sidebar.h

   Copyright (C) 2004-2005 Bastien Nocera <hadess@hadess.net>

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

#ifndef TOTEM_SIDEBAR_H
#define TOTEM_SIDEBAR_H

G_BEGIN_DECLS

void totem_sidebar_setup (Totem *totem, gboolean visible);
void totem_sidebar_toggle (Totem *totem);
void totem_sidebar_set_visibility (Totem *totem, gboolean visible);
gboolean totem_sidebar_is_visible (Totem *totem);

G_END_DECLS

#endif /* TOTEM_SIDEBAR_H */
