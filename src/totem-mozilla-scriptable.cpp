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
 * Waiting for response.
 */

gchar *
totemMozillaObject::wait ()
{
  gchar *msg;

  while (!this->tm->last_msg) {
    g_main_iteration (FALSE);
  }

  msg = this->tm->last_msg;
  this->tm->last_msg = NULL;

  return msg;
}

/*
 * From here on start the javascript-callable implementations.
 */

NS_IMETHODIMP
totemMozillaObject::Play ()
{
  g_message ("play");

  bacon_message_connection_send (this->tm->conn, "PLAY");
  gchar *msg = wait ();
  g_message ("Response: %s", msg);
  g_free (msg);

  return NS_OK;
}

NS_IMETHODIMP
totemMozillaObject::Rewind ()
{
  g_message ("stop");

  bacon_message_connection_send (this->tm->conn, "STOP");
  gchar *msg = wait ();
  g_message ("Response: %s", msg);
  g_free (msg);

  return NS_OK;
}

NS_IMETHODIMP
totemMozillaObject::Stop ()
{
  g_message ("pause");

  bacon_message_connection_send (this->tm->conn, "PAUSE");
  gchar *msg = wait ();
  g_message ("Response: %s", msg);
  g_free (msg);

  return NS_OK;
}
