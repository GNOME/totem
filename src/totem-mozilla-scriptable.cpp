/* Totem Mozilla plugin
 *
 * Copyright (C) <2004> Bastien Nocera <hadess@hadess.net>
 * Copyright (C) <2002> David A. Schleef <ds@schleef.org>
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <glib.h>

#include "totem-mozilla-scriptable.h"

totemMozillaObject::totemMozillaObject (TotemPlugin * _tm)
{
  tm = _tm;
  NS_INIT_ISUPPORTS();
  g_print ("Init scriptable instance\n");
}

totemMozillaObject::~totemMozillaObject ()
{
}

NS_IMPL_ISUPPORTS2(totemMozillaObject, totemMozillaScript, nsIClassInfo)

/*
 * From here on start the javascript-callable implementations.
 */

NS_IMETHODIMP
totemMozillaObject::Play ()
{
  g_print ("Play!\n");

  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
totemMozillaObject::Rewind ()
{
  g_print ("Stop!\n");

  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
totemMozillaObject::Stop ()
{
  g_print ("Pause!\n");

  return NS_ERROR_NOT_IMPLEMENTED;
}
