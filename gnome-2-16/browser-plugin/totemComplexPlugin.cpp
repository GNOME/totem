/* Totem Basic Plugin
 *
 * Copyright (C) 2004 Bastien Nocera <hadess@hadess.net>
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
 *
 * $Id$
 */

#include <mozilla-config.h>
#include "config.h"

#include <glib.h>

#include <nsIProgrammingLanguage.h>
#include <nsISupportsImpl.h>
#include <nsMemory.h>
#include <nsXPCOM.h>

#include "totemComplexPlugin.h"

/* 4ccca83d-30e7-4e9a-918c-09aa9236e3bb */
static const nsCID kClassID = 
{ 0x4ccca83d, 0x30e7, 0x4e9a,
  { 0x91, 0x8c, 0x09, 0xaa, 0x92, 0x36, 0xe3, 0xbb } };

static const char kClassDescription[] = "totemComplexPlugin";
static const char kPluginDescription[] = "Helix DNA Plugin: RealPlayer G2 Plug-In Compatible (compatible; Totem)";

static const totemPluginMimeEntry kMimeTypes[] = {
	{ "audio/x-pn-realaudio-plugin", "rpm", "audio/vnd.rn-realaudio" }
};

totemScriptablePlugin::totemScriptablePlugin (totemPlugin *aPlugin)
  : mPlugin(aPlugin)
{
  g_print ("%s ctor [%p]\n", kClassDescription, (void*) this);
}

totemScriptablePlugin::~totemScriptablePlugin ()
{
  g_print ("%s dtor [%p]\n", kClassDescription, (void*) this);
}

/* static */ char *
totemScriptablePlugin::PluginDescription ()
{
  return (char*) kPluginDescription;
}

/* static */ void
totemScriptablePlugin::PluginMimeTypes (const totemPluginMimeEntry **_entries,
					PRUint32 *_count)
{
  *_entries = kMimeTypes;
  *_count = G_N_ELEMENTS (kMimeTypes);
}

/* Interface implementations */

NS_IMPL_ISUPPORTS2 (totemScriptablePlugin,
		    totemIComplexPlayer,
		    nsIClassInfo)

/* nsIClassInfo */

NS_IMETHODIMP
totemScriptablePlugin::GetFlags (PRUint32 *aFlags)
{
  *aFlags = nsIClassInfo::PLUGIN_OBJECT | nsIClassInfo::DOM_OBJECT;
  return NS_OK;
}

NS_IMETHODIMP
totemScriptablePlugin::GetImplementationLanguage (PRUint32 *aImplementationLanguage)
{
  *aImplementationLanguage = nsIProgrammingLanguage::CPLUSPLUS;
  return NS_OK;
}

NS_IMETHODIMP
totemScriptablePlugin::GetInterfaces (PRUint32 *count,
				      nsIID * **array)
{
  *array = NS_STATIC_CAST (nsIID**, nsMemory::Alloc (sizeof (nsIID)));
  if (!*array)
    return NS_ERROR_OUT_OF_MEMORY;

  *count = 1;

  (*array)[0] = NS_STATIC_CAST (nsIID*,
  				nsMemory::Clone (&NS_GET_IID (totemIComplexPlayer),
						 sizeof(nsIID)));
  if (!(*array)[0]) {
    NS_FREE_XPCOM_ALLOCATED_POINTER_ARRAY (0, *array);
    return NS_ERROR_OUT_OF_MEMORY;
  }

  g_message ("GetInterfaces");
  return NS_OK;
}
     
NS_IMETHODIMP
totemScriptablePlugin::GetHelperForLanguage (PRUint32 language,
					     nsISupports **_retval)
{
  *_retval = nsnull;
  g_message ("GetHelperForLanguage %d", language);
  return NS_OK;
}
     
NS_IMETHODIMP
totemScriptablePlugin::GetContractID (char * *aContractID)
{
  *aContractID = nsnull;
  return NS_OK;
}

NS_IMETHODIMP
totemScriptablePlugin::GetClassDescription (char * *aClassDescription)
{
  *aClassDescription = NS_STATIC_CAST (char*,
				       nsMemory::Clone (kClassDescription,
						        sizeof (kClassDescription)));
  if (!*aClassDescription)
    return NS_ERROR_OUT_OF_MEMORY;

  g_message ("GetClassDescription: %s", *aClassDescription);
  return NS_OK;
}

NS_IMETHODIMP
totemScriptablePlugin::GetClassID (nsCID * *aClassID)
{
  *aClassID = NS_STATIC_CAST (nsCID*,
			      nsMemory::Clone (&kClassID,
					       sizeof (nsCID*)));
  if (!*aClassID)
    return NS_ERROR_OUT_OF_MEMORY;

  g_message ("GetClassID");
  return NS_OK;
}
     
NS_IMETHODIMP
totemScriptablePlugin::GetClassIDNoAlloc (nsCID *aClassIDNoAlloc)
{
  /* We don't need to implement this since we're not implementing nsISerializable */
  return NS_ERROR_NOT_AVAILABLE;
  *aClassIDNoAlloc = kClassID;
  g_message ("GetClassIDNoAlloc");
  return NS_OK;
}

/* totemIComplexPlayer */

NS_IMETHODIMP
totemScriptablePlugin::DoPause (PRBool *_retval)
{
  NS_ENSURE_STATE (mPlugin);

  totem_plugin_pause (mPlugin);

  *_retval = PR_TRUE;
  return NS_OK;
}

NS_IMETHODIMP
totemScriptablePlugin::DoPlay (PRBool *_retval)
{
  NS_ENSURE_STATE (mPlugin);

  totem_plugin_play (mPlugin);

  *_retval = PR_TRUE;
  return NS_OK;
}

NS_IMETHODIMP
totemScriptablePlugin::DoStop (PRBool *_retval)
{
  NS_ENSURE_STATE (mPlugin);

  totem_plugin_stop (mPlugin);

  *_retval = PR_TRUE;
  return NS_OK;
}
