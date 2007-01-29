/* Totem Mozilla plugin
 * 
 * Copyright (C) 2004-2006 Bastien Nocera <hadess@hadess.net>
 * Copyright (C) 2002 David A. Schleef <ds@schleef.org>
 * Copyright (C) 2006 Christian Persch
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
 */

#ifndef __TOTEM_PLUGIN_GLUE_H__
#define __TOTEM_PLUGIN_GLUE_H__

#if defined(TOTEM_BASIC_PLUGIN)
#include "totemBasicPlugin.h"
#elif defined(TOTEM_GMP_PLUGIN)
#include "totemGMPPlugin.h"
#elif defined(TOTEM_COMPLEX_PLUGIN)
#include "totemComplexPlugin.h"
#elif defined(TOTEM_NARROWSPACE_PLUGIN)
#include "totemNarrowSpacePlugin.h"
#elif defined(TOTEM_MULLY_PLUGIN)
#include "totemMullYPlugin.h"
#else
#error Unknown plugin type
#endif

#endif /* !__TOTEM_PLUGIN_GLUE_H__ */
