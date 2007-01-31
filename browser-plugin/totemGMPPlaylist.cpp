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
 * $Id: totemGMPPlugin.cpp 3790 2006-12-15 23:08:57Z chpe $
 */

#include <mozilla-config.h>
#include "config.h"

#include <nsDOMError.h>
#include <nsIProgrammingLanguage.h>
#include <nsISupportsImpl.h>
#include <nsMemory.h>
#include <nsXPCOM.h>

#define GNOME_ENABLE_DEBUG 1
/* define GNOME_ENABLE_DEBUG for more debug spew */
#include "debug.h"

#include "totemGMPPlugin.h"

#include "totemGMPPlaylist.h"

#define WARN_ACCESS()\
static PRBool warned = PR_FALSE;\
if (!warned) {\
	D ("GMP scriptable: use of forbidden function '" CURRENT_INTERFACE "::%s'", __FUNCTION__);\
	warned = PR_TRUE;\
}

#define WARN_NOT_IMPLEMENTED()\
static PRBool warned = PR_FALSE;\
if (!warned) {\
	D ("GMP scriptable: use of unimplemented function '" CURRENT_INTERFACE "::%s'", __FUNCTION__);\
	warned = PR_TRUE;\
}

#define SHOW_CALLS

#ifdef SHOW_CALLS
#define SHOW_CALL()\
static PRBool called = PR_FALSE;\
if (!called) {\
	D ("NOTE! Use of function '" CURRENT_INTERFACE "::%s'", __FUNCTION__);\
	called = PR_TRUE;\
}
#else
#define SHOW_CALL()
#endif

/* cf079ca3-c94f-4676-a9ae-6d9bffd765bd */
static const nsCID kClassID = 
  { 0xcf079ca3, 0xc94f, 0x4676, \
    { 0xa9, 0xae, 0x6d, 0x9b, 0xff, 0xd7, 0x65, 0xbd } };

static const char kClassDescription[] = "totemGMPPlaylist";

totemGMPPlaylist::totemGMPPlaylist (totemScriptablePlugin *aScriptable)
  : mName("Playlist") /* FIXME */
{
  D ("%s ctor [%p]", kClassDescription, (void*) this);

  /* We keep a reference to the main scriptable, so the code won't be unloaded unless it's ok to do so */
  mScriptable = aScriptable;
  NS_ADDREF (mScriptable);
}

totemGMPPlaylist::~totemGMPPlaylist ()
{
  D ("%s dtor [%p]", kClassDescription, (void*) this);

  NS_RELEASE (mScriptable);
  mScriptable = nsnull;
}

/* Interface implementations */

NS_IMPL_ISUPPORTS2 (totemGMPPlaylist,
		    totemIGMPPlaylist,
		    nsIClassInfo)

/* nsIClassInfo */

NS_IMETHODIMP
totemGMPPlaylist::GetFlags (PRUint32 *aFlags)
{
  /* FIXME: is PLUGIN_OBJECT correct here ? */
  *aFlags = nsIClassInfo::PLUGIN_OBJECT | nsIClassInfo::DOM_OBJECT;
  return NS_OK;
}

NS_IMETHODIMP
totemGMPPlaylist::GetImplementationLanguage (PRUint32 *aImplementationLanguage)
{
  *aImplementationLanguage = nsIProgrammingLanguage::CPLUSPLUS;
  return NS_OK;
}

NS_IMETHODIMP
totemGMPPlaylist::GetInterfaces (PRUint32 *count,
				 nsIID * **array)
{
  *array = NS_STATIC_CAST (nsIID**, nsMemory::Alloc (sizeof (nsIID)));
  if (!*array)
    return NS_ERROR_OUT_OF_MEMORY;

  *count = 1;

  (*array)[0] = NS_STATIC_CAST (nsIID*,
  				nsMemory::Clone (&NS_GET_IID (totemIGMPPlaylist),
						 sizeof(nsIID)));
  if (!(*array)[0]) {
    NS_FREE_XPCOM_ALLOCATED_POINTER_ARRAY (0, *array);
    return NS_ERROR_OUT_OF_MEMORY;
  }

  return NS_OK;
}
     
NS_IMETHODIMP
totemGMPPlaylist::GetHelperForLanguage (PRUint32 language,
					nsISupports **_retval)
{
  *_retval = nsnull;
  return NS_OK;
}
     
NS_IMETHODIMP
totemGMPPlaylist::GetContractID (char * *aContractID)
{
  *aContractID = nsnull;
  return NS_OK;
}

NS_IMETHODIMP
totemGMPPlaylist::GetClassDescription (char * *aClassDescription)
{
  *aClassDescription = NS_STATIC_CAST (char*,
				       nsMemory::Clone (kClassDescription,
						        sizeof (kClassDescription)));
  if (!*aClassDescription)
    return NS_ERROR_OUT_OF_MEMORY;

  return NS_OK;
}

NS_IMETHODIMP
totemGMPPlaylist::GetClassID (nsCID * *aClassID)
{
  *aClassID = NS_STATIC_CAST (nsCID*,
			      nsMemory::Clone (&kClassID,
					       sizeof (nsCID*)));
  if (!*aClassID)
    return NS_ERROR_OUT_OF_MEMORY;

  return NS_OK;
}
     
NS_IMETHODIMP
totemGMPPlaylist::GetClassIDNoAlloc (nsCID *aClassIDNoAlloc)
{
  *aClassIDNoAlloc = kClassID;
  return NS_OK;
}

/* totemIGMPPlayer */

#undef CURRENT_INTERFACE
#define CURRENT_INTERFACE "totemIGMPPlaylist"

/* void appendItem (in totemIGMPMedia item); */
NS_IMETHODIMP 
totemGMPPlaylist::AppendItem(totemIGMPMedia *item)
{
  WARN_NOT_IMPLEMENTED ();

  return NS_OK;
}

/* readonly attribute long attributeCount; */
NS_IMETHODIMP 
totemGMPPlaylist::GetAttributeCount(PRInt32 *aAttributeCount)
{
  WARN_NOT_IMPLEMENTED ();

  *aAttributeCount = 0;
  return NS_OK;
}

/* AUTF8String attributeName (in long index); */
NS_IMETHODIMP 
totemGMPPlaylist::AttributeName(PRInt32 index, nsACString & _retval)
{
  return GetAttributeName (index, _retval);
}

/* AUTF8String getattributeName (in long index); */
NS_IMETHODIMP 
totemGMPPlaylist::GetAttributeName(PRInt32 index, nsACString & _retval)
{
  WARN_NOT_IMPLEMENTED ();

  _retval.Assign ("");
  return NS_OK;
}

/* readonly attribute long count; */
NS_IMETHODIMP 
totemGMPPlaylist::GetCount(PRInt32 *aCount)
{
  WARN_NOT_IMPLEMENTED ();

  *aCount = 0;
  return NS_OK;
}

/* AUTF8String getItemInfo (in AUTF8String name); */
NS_IMETHODIMP 
totemGMPPlaylist::GetItemInfo(const nsACString & name, nsACString & _retval)
{
  WARN_NOT_IMPLEMENTED ();

  _retval.Assign ("");
  return NS_OK;
}

/* void insertItem (in long index, in totemIGMPMedia item); */
NS_IMETHODIMP 
totemGMPPlaylist::InsertItem(PRInt32 index, totemIGMPMedia *item)
{
  WARN_NOT_IMPLEMENTED ();

  return NS_OK;
}

/* boolean isIdentical (in totemIGMPPlaylist playlist); */
NS_IMETHODIMP 
totemGMPPlaylist::IsIdentical(totemIGMPPlaylist *playlist, PRBool *_retval)
{
  nsISupports *thisPlaylist = NS_STATIC_CAST (nsISupports*,
					      NS_STATIC_CAST (totemIGMPPlaylist*, this));

  *_retval = thisPlaylist == playlist;
  return NS_OK;
}

/* totemIGMPMedia item (in long index); */
NS_IMETHODIMP 
totemGMPPlaylist::Item(PRInt32 index, totemIGMPMedia **_retval)
{
  WARN_NOT_IMPLEMENTED ();

  return NS_ERROR_NOT_IMPLEMENTED;
}

/* void moveItem (in long oldIndex, in long newIndex); */
NS_IMETHODIMP 
totemGMPPlaylist::MoveItem(PRInt32 oldIndex, PRInt32 newIndex)
{
  WARN_NOT_IMPLEMENTED ();

  return NS_OK;
}

/* attribute AUTF8String name; */
NS_IMETHODIMP 
totemGMPPlaylist::GetName(nsACString & aName)
{
  SHOW_CALL ();

  aName = mName;
  return NS_OK;
}

NS_IMETHODIMP 
totemGMPPlaylist::SetName(const nsACString & aName)
{
  SHOW_CALL ();

  mName = aName;
  return NS_OK;
}

/* void removeItem (in totemIGMPMedia item); */
NS_IMETHODIMP 
totemGMPPlaylist::RemoveItem(totemIGMPMedia *item)
{
  WARN_NOT_IMPLEMENTED ();

  return NS_OK;
}

/* void setItemInfo (in AUTF8String name, in AUTF8String value); */
NS_IMETHODIMP 
totemGMPPlaylist::SetItemInfo(const nsACString & name, const nsACString & value)
{
  WARN_NOT_IMPLEMENTED ();

  return NS_OK;
}
