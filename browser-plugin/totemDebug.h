/* 
 * Copyright © 2006, 2007 Christian Persch
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * $Id$
 */

#ifndef __TOTEM_PLUGIN_DEBUG_H__
#define __TOTEM_PLUGIN_DEBUG_H__

#define TOTEM_SCRIPTABLE_WARN_UNIMPLEMENTED() \
static PRBool warned = PR_FALSE;\
if (!warned) {\
  D ("WARNING: Site uses unimplemented function '" TOTEM_SCRIPTABLE_INTERFACE "::%s'", __func__);\
  warned = PR_TRUE;\
}

#define TOTEM_SCRIPTABLE_WARN_ACCESS() \
static PRBool warned = PR_FALSE;\
if (!warned) {\
  D ("WARNING: Site uses forbidden function '" TOTEM_SCRIPTABLE_INTERFACE "::%s'", __func__);\
  warned = PR_TRUE;\
}

#define TOTEM_SCRIPTABLE_LOG_ACCESS() \
static PRBool logged = PR_FALSE;\
if (!logged) {\
  D ("NOTE: Site uses function '" TOTEM_SCRIPTABLE_INTERFACE "::%s'", __func__);\
  logged = PR_TRUE;\
}

#endif /* !__TOTEM_PLUGIN_DEBUG_H__ */
