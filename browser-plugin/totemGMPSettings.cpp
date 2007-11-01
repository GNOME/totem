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

#include "totemIGMPControls.h"

#include "totemDebug.h"
#include "totemClassInfo.h"

#include "totemGMPPlugin.h"
#include "totemPlugin.h"

#include "totemGMPSettings.h"

/* 693329d8-866e-469a-805c-718c4291be70 */
static const nsCID kClassID =
  { 0x693329d8, 0x866e, 0x469a, \
    { 0x80, 0x5c, 0x71, 0x8c, 0x42, 0x91, 0xbe, 0x70 } };

static const char kClassDescription[] = "totemGMPSettings";

totemGMPSettings::totemGMPSettings (totemScriptablePlugin *aPlugin)
  : mVolume(100),
    mPlugin(aPlugin)
{
  D ("%s ctor [%p]", kClassDescription, (void*) this);
}

totemGMPSettings::~totemGMPSettings ()
{
  D ("%s dtor [%p]", kClassDescription, (void*) this);
}

/* Interface implementations */

NS_IMPL_ISUPPORTS2 (totemGMPSettings,
		    totemIGMPSettings,
		    nsIClassInfo)

/* nsIClassInfo */

TOTEM_CLASSINFO_BEGIN (totemGMPSettings,
		       1,
		       kClassID,
		       kClassDescription)
  TOTEM_CLASSINFO_ENTRY (0, totemIGMPSettings)
TOTEM_CLASSINFO_END

/* totemIGMPSettings */

#undef TOTEM_SCRIPTABLE_INTERFACE
#define TOTEM_SCRIPTABLE_INTERFACE "totemIGMPSettings"

/* attribute boolean autoStart; */
NS_IMETHODIMP 
totemGMPSettings::GetAutoStart(PRBool *aAutoStart)
{
  TOTEM_SCRIPTABLE_WARN_UNIMPLEMENTED ();

  return NS_ERROR_NOT_IMPLEMENTED;
}
NS_IMETHODIMP 
totemGMPSettings::SetAutoStart(PRBool aAutoStart)
{
  TOTEM_SCRIPTABLE_WARN_UNIMPLEMENTED ();

  return NS_ERROR_NOT_IMPLEMENTED;
}

/* attribute AUTF8String baseURL; */
NS_IMETHODIMP 
totemGMPSettings::GetBaseURL(nsACString & aBaseURL)
{
  TOTEM_SCRIPTABLE_WARN_UNIMPLEMENTED ();

  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP 
totemGMPSettings::SetBaseURL(const nsACString & aBaseURL)
{
  TOTEM_SCRIPTABLE_WARN_UNIMPLEMENTED ();

  return NS_ERROR_NOT_IMPLEMENTED;
}

/* readonly attribute long defaultAudioLanguage; */
NS_IMETHODIMP 
totemGMPSettings::GetDefaultAudioLanguage(PRInt32 *aDefaultAudioLanguage)
{
  TOTEM_SCRIPTABLE_WARN_UNIMPLEMENTED ();

  return NS_ERROR_NOT_IMPLEMENTED;
}

/* attribute AUTF8String defaultFrame; */
NS_IMETHODIMP 
totemGMPSettings::GetDefaultFrame(nsACString & aDefaultFrame)
{
  TOTEM_SCRIPTABLE_WARN_UNIMPLEMENTED ();

  return NS_ERROR_NOT_IMPLEMENTED;
}
NS_IMETHODIMP 
totemGMPSettings::SetDefaultFrame(const nsACString & aDefaultFrame)
{
  TOTEM_SCRIPTABLE_WARN_UNIMPLEMENTED ();

  return NS_ERROR_NOT_IMPLEMENTED;
}

/* attribute boolean enableErrorDialogs; */
NS_IMETHODIMP 
totemGMPSettings::GetEnableErrorDialogs(PRBool *aEnableErrorDialogs)
{
  TOTEM_SCRIPTABLE_WARN_UNIMPLEMENTED ();

  return NS_ERROR_NOT_IMPLEMENTED;
}
NS_IMETHODIMP 
totemGMPSettings::SetEnableErrorDialogs(PRBool aEnableErrorDialogs)
{
  TOTEM_SCRIPTABLE_WARN_UNIMPLEMENTED ();

  return NS_ERROR_NOT_IMPLEMENTED;
}

/* attribute boolean invokeURLs; */
NS_IMETHODIMP 
totemGMPSettings::GetInvokeURLs(PRBool *aInvokeURLs)
{
  TOTEM_SCRIPTABLE_WARN_UNIMPLEMENTED ();

  return NS_ERROR_NOT_IMPLEMENTED;
}
NS_IMETHODIMP 
totemGMPSettings::SetInvokeURLs(PRBool aInvokeURLs)
{
  TOTEM_SCRIPTABLE_WARN_UNIMPLEMENTED ();

  return NS_ERROR_NOT_IMPLEMENTED;
}

/* readonly attribute ACString mediaAccessRights; */
NS_IMETHODIMP 
totemGMPSettings::GetMediaAccessRights(nsACString & aMediaAccessRights)
{
  TOTEM_SCRIPTABLE_WARN_UNIMPLEMENTED ();

  return NS_ERROR_NOT_IMPLEMENTED;
}

/* attribute boolean mute; */
NS_IMETHODIMP 
totemGMPSettings::GetMute(PRBool *_retval)
{
  TOTEM_SCRIPTABLE_WARN_UNIMPLEMENTED ();

  *_retval = mMute;
  return NS_OK;
}

NS_IMETHODIMP 
totemGMPSettings::SetMute(PRBool enabled)
{
  TOTEM_SCRIPTABLE_WARN_UNIMPLEMENTED ();

  mMute = enabled != PR_FALSE;
  return NS_OK;
}

/* attribute long playCount; */
NS_IMETHODIMP 
totemGMPSettings::GetPlayCount(PRInt32 *aPlayCount)
{
  TOTEM_SCRIPTABLE_WARN_UNIMPLEMENTED ();

  return NS_ERROR_NOT_IMPLEMENTED;
}
NS_IMETHODIMP 
totemGMPSettings::SetPlayCount(PRInt32 aPlayCount)
{
  TOTEM_SCRIPTABLE_WARN_UNIMPLEMENTED ();

  return NS_ERROR_NOT_IMPLEMENTED;
}

/* attribute double rate; */
NS_IMETHODIMP 
totemGMPSettings::GetRate(double *aRate)
{
  TOTEM_SCRIPTABLE_WARN_UNIMPLEMENTED ();

  return NS_ERROR_NOT_IMPLEMENTED;
}
NS_IMETHODIMP 
totemGMPSettings::SetRate(double aRate)
{
  TOTEM_SCRIPTABLE_WARN_UNIMPLEMENTED ();

  return NS_ERROR_NOT_IMPLEMENTED;
}

/* attribute long balance; */
NS_IMETHODIMP 
totemGMPSettings::GetBalance(PRInt32 *aBalance)
{
  TOTEM_SCRIPTABLE_WARN_UNIMPLEMENTED ();

  return NS_ERROR_NOT_IMPLEMENTED;
}
NS_IMETHODIMP 
totemGMPSettings::SetBalance(PRInt32 aBalance)
{
  TOTEM_SCRIPTABLE_WARN_UNIMPLEMENTED ();

  return NS_ERROR_NOT_IMPLEMENTED;
}

/* attribute long volume; */
NS_IMETHODIMP 
totemGMPSettings::GetVolume(PRInt32 *_retval)
{
  TOTEM_SCRIPTABLE_LOG_ACCESS ();

  *_retval = mVolume;
  return NS_OK;
}
NS_IMETHODIMP 
totemGMPSettings::SetVolume(PRInt32 volume)
{
  TOTEM_SCRIPTABLE_LOG_ACCESS ();

  NS_ENSURE_STATE (IsValid ());

  nsresult rv = mPlugin->mPlugin->SetVolume ((double) volume / 100);

  /* Volume passed in is 0 through to 100 */
  mVolume = volume;

  return NS_OK;
}

/* boolean isAvailable (in ACString setting); */
NS_IMETHODIMP 
totemGMPSettings::IsAvailable(const nsACString & setting, PRBool *_retval)
{
  TOTEM_SCRIPTABLE_WARN_UNIMPLEMENTED ();

  *_retval = PR_FALSE;
  return NS_OK;
}

/* boolean getMode (in ACString mode); */
NS_IMETHODIMP 
totemGMPSettings::GetMode(const nsACString & mode, PRBool *_retval)
{
  TOTEM_SCRIPTABLE_WARN_UNIMPLEMENTED ();

  *_retval = PR_FALSE;
  return NS_OK;
}

/* void setMode (in ACString mode, in boolean enabled); */
NS_IMETHODIMP 
totemGMPSettings::SetMode(const nsACString & mode, PRBool enabled)
{
  TOTEM_SCRIPTABLE_WARN_UNIMPLEMENTED ();

  return NS_OK;
}

/* boolean requestMediaAccessRights (in ACString mode); */
NS_IMETHODIMP 
totemGMPSettings::RequestMediaAccessRights (const nsACString & mode, PRBool *_retval)
{
  TOTEM_SCRIPTABLE_WARN_UNIMPLEMENTED ();

  *_retval = PR_FALSE;
  return NS_OK;
}
