/*
 *  Copyright (C) 2004 Bastien Nocera <hadess@hadess.net>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
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

#include <glib.h>

#ifndef __TOTEM_GROMIT_H
#define __TOTEM_GROMIT_H

G_BEGIN_DECLS

gboolean totem_gromit_available		(void);
void totem_gromit_toggle		(void);
void totem_gromit_clear			(gboolean now);

G_END_DECLS

#endif /* __TOTEM_GROMIT_H */
