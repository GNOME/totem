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

#include "mozilla-config.h"

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <glib.h>

#include "totem-mozilla-scriptable.h"

totemMozillaObject::totemMozillaObject (TotemPlugin * _tm)
{
  tm = _tm;
  g_print ("Init scriptable instance\n");
}

totemMozillaObject::~totemMozillaObject ()
{
  g_print ("Die scriptable instance\n");
}

NS_IMPL_ISUPPORTS2(totemMozillaObject, totemMozillaScript, nsIClassInfo)

void
totemMozillaObject::invalidatePlugin ()
{
  this->tm = NULL;
}

/*
 * From here on start the javascript-callable implementations.
 */

NS_IMETHODIMP
totemMozillaObject::Play ()
{
  if (!this->tm)
    return NS_ERROR_FAILURE;

  g_message ("play");
  dbus_g_proxy_call (this->tm->proxy, "Play", NULL,
		     G_TYPE_INVALID, G_TYPE_INVALID);

  return NS_OK;
}

NS_IMETHODIMP
totemMozillaObject::Rewind ()
{
  if (!this->tm)
    return NS_ERROR_FAILURE;

  g_message ("stop");
  dbus_g_proxy_call (this->tm->proxy, "Stop", NULL,
		     G_TYPE_INVALID, G_TYPE_INVALID);

  return NS_OK;
}

NS_IMETHODIMP
totemMozillaObject::Stop ()
{
  if (!this->tm)
    return NS_ERROR_FAILURE;

  g_message ("pause");
  dbus_g_proxy_call (this->tm->proxy, "Pause", NULL,
		     G_TYPE_INVALID, G_TYPE_INVALID);

  return NS_OK;
}
