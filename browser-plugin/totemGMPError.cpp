/* Totem GMP plugin
 *
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
 * $Id: totemGMPPlugin.cpp 3928 2007-01-22 14:59:07Z chpe $
 */

#include <mozilla-config.h>
#include "config.h"

#include <glib.h>

#include <nsDOMError.h>
#include <nsIProgrammingLanguage.h>
#include <nsISupportsImpl.h>
#include <nsMemory.h>
#include <nsXPCOM.h>

#define GNOME_ENABLE_DEBUG 1
/* define GNOME_ENABLE_DEBUG for more debug spew */
#include "debug.h"

#include "totemClassInfo.h"

#include "totemGMPPlugin.h"
#include "totemPlugin.h"

#include "totemGMPError.h"

#define WARN_ACCESS()\
static PRBool warned = PR_FALSE;\
if (!warned) {\
	D ("GMP scriptable: use of forbidden function '" CURRENT_INTERFACE "::%s'", G_GNUC_FUNCTION);\
	warned = PR_TRUE;\
}

#define WARN_NOT_IMPLEMENTED()\
static PRBool warned = PR_FALSE;\
if (!warned) {\
	D ("GMP scriptable: use of unimplemented function '" CURRENT_INTERFACE "::%s'", G_GNUC_FUNCTION);\
	warned = PR_TRUE;\
}

/* 2908e683-6162-45ed-8167-f555579d2411 */
static const nsCID kClassID =
  { 0x2908e683, 0x6162, 0x45ed, \
    { 0x81, 0x67, 0xf5, 0x55, 0x57, 0x9d, 0x24, 0x11 } };

static const char kClassDescription[] = "totemGMPError";

/* NOTE: For now we'll implement totemIGMPError and totemIGMPErrorItem on the same
 * object and only ever present at most _one_ error. If that ever changes,
 * we'll need to split this.
 */

totemGMPError::totemGMPError (totemScriptablePlugin *aPlugin)
  : mPlugin(aPlugin),
    mCount(0)
{
  D ("%s ctor [%p]", kClassDescription, (void*) this);
}

totemGMPError::~totemGMPError ()
{
  D ("%s dtor [%p]", kClassDescription, (void*) this);
}

/* Interface implementations */

NS_IMPL_ISUPPORTS3 (totemGMPError,
		    totemIGMPError,
		    totemIGMPErrorItem,
		    nsIClassInfo)

/* nsIClassInfo */

TOTEM_CLASSINFO_BEGIN (totemGMPError,
		       2,
		       kClassID,
		       kClassDescription)
  TOTEM_CLASSINFO_ENTRY (0, totemIGMPError)
  TOTEM_CLASSINFO_ENTRY (0, totemIGMPErrorItem)
TOTEM_CLASSINFO_END

/* totemIGMPError */

#undef CURRENT_INTERFACE
#define CURRENT_INTERFACE "totemIGMPError"

/* void clearErrorQueue (); */
NS_IMETHODIMP 
totemGMPError::ClearErrorQueue()
{
  WARN_NOT_IMPLEMENTED ();

  mCount = 0;
  return NS_OK;
}

/* readonly attribute long errorCount; */
NS_IMETHODIMP 
totemGMPError::GetErrorCount(PRInt32 *aErrorCount)
{
  WARN_NOT_IMPLEMENTED ();

  *aErrorCount = mCount;
  return NS_OK;
}

/* totemIGMPErrorItem item (in long index); */
NS_IMETHODIMP 
totemGMPError::Item(PRInt32 index, totemIGMPErrorItem **_retval)
{
  WARN_NOT_IMPLEMENTED ();

  if (index < 0 || index >= mCount)
    return NS_ERROR_ILLEGAL_VALUE;

  return CallQueryInterface (this, _retval);
}

/* void webHelp (); */
NS_IMETHODIMP 
totemGMPError::WebHelp()
{
  WARN_NOT_IMPLEMENTED ();

  return NS_OK;
}

/* totemIGMPErrorItem */

#undef CURRENT_INTERFACE
#define CURRENT_INTERFACE "totemIGMPErrorItem"

/* readonly attribute long condition; */
NS_IMETHODIMP 
totemGMPError::GetCondition(PRInt32 *aCondition)
{
  WARN_NOT_IMPLEMENTED ();

  *aCondition = 0;
  return NS_OK;
}

/* readonly attribute AUTF8String customURL; */
NS_IMETHODIMP 
totemGMPError::GetCustomURL(nsACString & aCustomURL)
{
  WARN_NOT_IMPLEMENTED ();

  aCustomURL.Assign ("http://www.gnome.org/projects/totem");
  return NS_OK;
}

/* readonly attribute long errorCode; */
NS_IMETHODIMP 
totemGMPError::GetErrorCode(PRInt32 *aErrorCode)
{
  WARN_NOT_IMPLEMENTED ();

  *aErrorCode = 0;
  return NS_OK;
}

/* readonly attribute AUTF8String errorContext; */
NS_IMETHODIMP 
totemGMPError::GetErrorContext(nsACString & aErrorContext)
{
  WARN_NOT_IMPLEMENTED ();

  aErrorContext.Assign ("");
  return NS_OK;
}

/* readonly attribute AUTF8String errorDescription; */
NS_IMETHODIMP
totemGMPError::GetErrorDescription(nsACString & aErrorDescription)
{
  WARN_NOT_IMPLEMENTED ();

  aErrorDescription.Assign ("");
  return NS_OK;
}
