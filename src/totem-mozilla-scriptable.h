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

#ifndef __TOTEM_MOZILLA_SCRIPTABLE_H__
#define __TOTEM_MOZILLA_SCRIPTABLE_H__

#include <dbus/dbus-glib.h>
#include "bacon-message-connection.h"
#include "totem_mozilla_scripting.h"
#include "nsIClassInfo.h"
#include "npupp.h"
#include "npapi.h"

class totemMozillaObject;

typedef struct {
	NPP instance;
	Window window;
	totemMozillaObject *iface;

	char *src, *href, *target;
	int width, height;
	DBusGConnection *conn;
	DBusGProxy *proxy;
	char *wait_for_svc;
	gboolean got_svc;
	int send_fd;
	int player_pid;
	gboolean controller_hidden;
	guint8 stream_type;
	gboolean cache;
	gboolean hidden;

	GByteArray *bytes;
} TotemPlugin;

class totemMozillaMix : public nsIClassInfo {
public:
  // These flags are used by the DOM and security systems to signal that 
  // JavaScript callers are allowed to call this object's scritable methods.
  NS_IMETHOD GetFlags(PRUint32 *aFlags)
    {*aFlags = nsIClassInfo::PLUGIN_OBJECT | nsIClassInfo::DOM_OBJECT;
     return NS_OK;}
  NS_IMETHOD GetImplementationLanguage(PRUint32 *aImplementationLanguage)
    {*aImplementationLanguage = nsIProgrammingLanguage::CPLUSPLUS;
     return NS_OK;}

  // The rest of the methods can safely return error codes...
  NS_IMETHOD GetInterfaces(PRUint32 *count, nsIID * **array)
    {g_message ("GetInterfaces"); return NS_ERROR_NOT_IMPLEMENTED;}
  NS_IMETHOD GetHelperForLanguage(PRUint32 language, nsISupports **_retval)
    {g_message ("GetHelperForLanguage"); return NS_ERROR_NOT_IMPLEMENTED;}
  NS_IMETHOD GetContractID(char * *aContractID)
    {g_message ("GetContractID"); return NS_ERROR_NOT_IMPLEMENTED;}
  NS_IMETHOD GetClassDescription(char * *aClassDescription)
    {g_message ("GetClassDescription"); return NS_ERROR_NOT_IMPLEMENTED;}
  NS_IMETHOD GetClassID(nsCID * *aClassID)
    {g_message ("GetClassID"); return NS_ERROR_NOT_IMPLEMENTED;}
  NS_IMETHOD GetClassIDNoAlloc(nsCID *aClassIDNoAlloc)
    {g_message ("GetClassIDNoAlloc"); return NS_ERROR_NOT_IMPLEMENTED;}
};

class totemMozillaObject : public totemMozillaScript,
			   public totemMozillaMix {
public:
  NS_DECL_ISUPPORTS
  NS_DECL_TOTEMMOZILLASCRIPT

  totemMozillaObject (TotemPlugin *tm);
  ~totemMozillaObject ();
  void invalidatePlugin ();

  TotemPlugin *tm;
};

#endif /* __TOTEM_MOZILLA_SCRIPTABLE_H__ */
