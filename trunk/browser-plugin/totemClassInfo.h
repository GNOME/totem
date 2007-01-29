/* Totem browser plugin
 *
 * Copyright © 2007 Christian Persch
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
 * $Id: totemNarrowSpacePlugin.cpp 3922 2007-01-19 14:32:48Z hadess $
 */

#ifndef __TOTEM_CLASSINFO_H__
#define __TOTEM_CLASSINFO_H__

#include <nsIProgrammingLanguage.h>
#include <nsISupportsImpl.h>
#include <nsMemory.h>
#include <nsXPCOM.h>

#define TOTEM_CLASSINFO_BEGIN(_class,_count,_cid,_description)\
/* nsISupports getHelperForLanguage (in PRUint32 language); */\
NS_IMETHODIMP _class::GetHelperForLanguage(PRUint32 language, nsISupports **_retval)\
{\
  *_retval = nsnull;\
  return NS_OK;\
}\
\
/* readonly attribute string contractID; */\
NS_IMETHODIMP _class::GetContractID(char * *aContractID)\
{\
  *aContractID = nsnull;\
  return NS_OK;\
}\
\
/* readonly attribute string classDescription; */\
NS_IMETHODIMP _class::GetClassDescription(char * *aClassDescription)\
{\
  *aClassDescription = NS_STATIC_CAST (char*,\
				       nsMemory::Clone (_description,\
						        sizeof (_description)));\
  if (!*aClassDescription)\
    return NS_ERROR_OUT_OF_MEMORY;\
\
  return NS_OK;\
}\
\
/* readonly attribute nsCIDPtr classID; */\
NS_IMETHODIMP _class::GetClassID(nsCID * *aClassID)\
{\
  *aClassID = NS_STATIC_CAST (nsCID*,\
			      nsMemory::Clone (&_cid,\
					       sizeof (nsCID*)));\
  if (!*aClassID)\
    return NS_ERROR_OUT_OF_MEMORY;\
\
  return NS_OK;\
}\
\
/* readonly attribute PRUint32 implementationLanguage; */\
NS_IMETHODIMP _class::GetImplementationLanguage(PRUint32 *aImplementationLanguage)\
{\
  *aImplementationLanguage = nsIProgrammingLanguage::CPLUSPLUS;\
  return NS_OK;\
}\
\
/* readonly attribute PRUint32 flags; */\
NS_IMETHODIMP _class::GetFlags(PRUint32 *aFlags)\
{\
  *aFlags = nsIClassInfo::PLUGIN_OBJECT | nsIClassInfo::DOM_OBJECT;\
  return NS_OK;\
}\
\
/* [notxpcom] readonly attribute nsCID classIDNoAlloc; */\
NS_IMETHODIMP _class::GetClassIDNoAlloc(nsCID *aClassIDNoAlloc)\
{\
  /* We don't really need to implement this since we're not implementing nsISerializable */\
  *aClassIDNoAlloc = _cid;\
  return NS_OK;\
}\
\
NS_IMETHODIMP _class::GetInterfaces (PRUint32 *count, nsIID * **array)\
{\
  *array = NS_STATIC_CAST (nsIID**, nsMemory::Alloc (sizeof (nsIID) * _count));\
  if (!*array)\
    return NS_ERROR_OUT_OF_MEMORY;\
\
  *count = _count;

#define TOTEM_CLASSINFO_ENTRY(_i, _interface)\
  (*array)[_i] = NS_STATIC_CAST (nsIID*,\
                                 nsMemory::Clone(&NS_GET_IID(_interface),\
                                                 sizeof(nsIID)));\
  if (!(*array)[_i]) {\
    NS_FREE_XPCOM_ALLOCATED_POINTER_ARRAY (_i, *array);\
    return NS_ERROR_OUT_OF_MEMORY;\
  }


#define TOTEM_CLASSINFO_END \
  return NS_OK;\
}

#endif /* !__TOTEM_CLASSINFO_H__ */
