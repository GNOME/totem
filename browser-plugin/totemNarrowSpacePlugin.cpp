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

#define GNOME_ENABLE_DEBUG 1
/* define GNOME_ENABLE_DEBUG for more debug spew */
#include "debug.h"

#include "totemDebug.h"
#include "totemClassInfo.h"

#include "totemNarrowSpacePlugin.h"

/* 2e390ee1-f0e3-423c-9764-f5ab50a40c06 */
static const nsCID kClassID = 
{ 0x2e390ee1, 0xf0e3, 0x423c, \
  { 0x97, 0x64, 0xf5, 0xab, 0x50, 0xa4, 0x0c, 0x06 } };

static const char kClassDescription[] = "totemNarrowSpacePlugin";
static const char kPluginDescription[] = "QuickTime Plug-in 7.2.0";

static const totemPluginMimeEntry kMimeTypes[] = {
	{ "video/quicktime", "mov", NULL },
	{ "video/mp4", "mp4", NULL },
	{ "image/x-macpaint", "pntg", NULL },
	{ "image/x-quicktime", "pict, pict1, pict2", "image/x-pict" },
	{ "video/x-m4v", "m4v", NULL },
};

void*
totemScriptablePlugin::operator new (size_t aSize) CPP_THROW_NEW
{
  void *object = ::operator new (aSize);
  if (object) {
    memset (object, 0, aSize);
  }

  return object;
}

totemScriptablePlugin::totemScriptablePlugin (totemPlugin *aPlugin)
  : mPlugin(aPlugin),
    mRate(1.0),
    mVolume(100),
    mPluginState(eState_Waiting)
{
  D ("%s ctor [%p]", kClassDescription, (void*) this);
}

totemScriptablePlugin::~totemScriptablePlugin ()
{
  D ("%s dtor [%p]", kClassDescription, (void*) this);
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
		    totemINarrowSpacePlayer,
		    nsIClassInfo)

/* nsIClassInfo */

TOTEM_CLASSINFO_BEGIN (totemScriptablePlugin,
		       1,
		       kClassID,
		       kClassDescription)
  TOTEM_CLASSINFO_ENTRY (0, totemINarrowSpacePlayer)
TOTEM_CLASSINFO_END

/* totemINarrowSpacePlayer */

#define TOTEM_SCRIPTABLE_INTERFACE "totemINarrowSpacePlayer"

/* boolean GetAutoPlay (); */
NS_IMETHODIMP
totemScriptablePlugin::GetAutoPlay(PRBool *_retval)
{
  TOTEM_SCRIPTABLE_WARN_UNIMPLEMENTED();

  *_retval = mAutoPlay;
  return NS_OK;
}

/* void SetAutoPlay (in boolean enabled); */
NS_IMETHODIMP
totemScriptablePlugin::SetAutoPlay(PRBool enabled)
{
  TOTEM_SCRIPTABLE_WARN_UNIMPLEMENTED();

  mAutoPlay = enabled != PR_FALSE;
  return NS_OK;
}

/* ACString GetBgColor (); */
NS_IMETHODIMP
totemScriptablePlugin::GetBgColor(nsACString & _retval)
{
  TOTEM_SCRIPTABLE_LOG_ACCESS ();

  _retval.Assign (mBackgroundColour);
  return NS_OK;
}

/* void SetBgColor (in ACString colour); */
NS_IMETHODIMP
totemScriptablePlugin::SetBgColor(const nsACString & colour)
{
  TOTEM_SCRIPTABLE_WARN_UNIMPLEMENTED();

  mBackgroundColour = colour;
  return NS_OK;
}

/* ACString GetComponentVersion (in ACString type, in ACString subtype, in ACString vendor); */
NS_IMETHODIMP
totemScriptablePlugin::GetComponentVersion (const nsACString & type,
					    const nsACString & subtype,
					    const nsACString & vendor,
					    nsACString & _retval)
{
  TOTEM_SCRIPTABLE_WARN_UNIMPLEMENTED ();
  D ("GetComponentVersion [%s, %s, %s]",
     nsCString(type).get(),
     nsCString(subtype).get(),
     nsCString(vendor).get());

  _retval.Assign ("1.0");
  return NS_OK;
}

/* boolean GetControllerVisible (); */
NS_IMETHODIMP
totemScriptablePlugin::GetControllerVisible(PRBool *_retval)
{
  TOTEM_SCRIPTABLE_LOG_ACCESS ();
  *_retval = mControllerVisible;
  return NS_OK;
}

/* void SetControllerVisible (in boolean enabled); */
NS_IMETHODIMP
totemScriptablePlugin::SetControllerVisible(PRBool enabled)
{
  TOTEM_SCRIPTABLE_WARN_UNIMPLEMENTED();

  mControllerVisible = enabled != PR_FALSE;
  return NS_OK;
}

/* unsigned long GetDuration (); */
NS_IMETHODIMP
totemScriptablePlugin::GetDuration(PRUint32 *_retval)
{
  TOTEM_SCRIPTABLE_WARN_UNIMPLEMENTED();

  *_retval = 0;
  return NS_OK;
}

/* unsigned long GetEndTime (); */
NS_IMETHODIMP
totemScriptablePlugin::GetEndTime(PRUint32 *_retval)
{
  TOTEM_SCRIPTABLE_WARN_UNIMPLEMENTED();

  *_retval = 0;
  return NS_OK;
}

/* void SetEndTime (in unsigned long time); */
NS_IMETHODIMP
totemScriptablePlugin::SetEndTime(PRUint32 time)
{
  TOTEM_SCRIPTABLE_WARN_UNIMPLEMENTED();

  return NS_OK;
}

/* float GetFieldOfView (); */
NS_IMETHODIMP
totemScriptablePlugin::GetFieldOfView (float *_retval)
{
  TOTEM_SCRIPTABLE_LOG_ACCESS ();
  return NS_ERROR_NOT_AVAILABLE;
}

/* void SetFieldOfView (in float angle); */
NS_IMETHODIMP
totemScriptablePlugin::SetFieldOfView(float angle)
{
  TOTEM_SCRIPTABLE_LOG_ACCESS ();
  return NS_ERROR_NOT_AVAILABLE;
}

/* void GoPreviousNode (); */
NS_IMETHODIMP
totemScriptablePlugin::GoPreviousNode()
{
  TOTEM_SCRIPTABLE_LOG_ACCESS ();
  return NS_ERROR_NOT_AVAILABLE;
}

/* AUTF8String GetHotspotTarget (in unsigned long id); */
NS_IMETHODIMP
totemScriptablePlugin::GetHotspotTarget(PRUint32 id, nsACString & _retval)
{
  TOTEM_SCRIPTABLE_LOG_ACCESS ();
  return NS_ERROR_NOT_AVAILABLE;
}

/* void SetHotspotTarget (in unsigned long id, in AUTF8String target); */
NS_IMETHODIMP
totemScriptablePlugin::SetHotspotTarget(PRUint32 id, const nsACString & target)
{
  TOTEM_SCRIPTABLE_LOG_ACCESS ();
  return NS_ERROR_NOT_AVAILABLE;
}

/* AUTF8String GetHotspotUrl (in unsigned long id); */
NS_IMETHODIMP
totemScriptablePlugin::GetHotspotUrl(PRUint32 id, nsACString & _retval)
{
  TOTEM_SCRIPTABLE_LOG_ACCESS ();
  return NS_ERROR_NOT_AVAILABLE;
}

/* void SetHotspotUrl (in unsigned long id, in AUTF8String url); */
NS_IMETHODIMP
totemScriptablePlugin::SetHotspotUrl(PRUint32 id, const nsACString & url)
{
  TOTEM_SCRIPTABLE_LOG_ACCESS ();
  return NS_ERROR_NOT_AVAILABLE;
}

/* AUTF8String GetHREF (); */
NS_IMETHODIMP
totemScriptablePlugin::GetHREF(nsACString & _retval)
{
  TOTEM_SCRIPTABLE_WARN_UNIMPLEMENTED();

  return NS_OK;
}

/* void SetHREF (in AUTF8String href); */
NS_IMETHODIMP
totemScriptablePlugin::SetHREF(const nsACString & href)
{
  TOTEM_SCRIPTABLE_WARN_UNIMPLEMENTED();

  return NS_OK;
}

/* boolean GetIsLooping (); */
NS_IMETHODIMP
totemScriptablePlugin::GetIsLooping(PRBool *_retval)
{
  TOTEM_SCRIPTABLE_LOG_ACCESS ();

  *_retval = mIsLooping;
  return NS_OK;
}

/* void SetIsLooping (in boolean enabled); */
NS_IMETHODIMP
totemScriptablePlugin::SetIsLooping(PRBool enabled)
{
  TOTEM_SCRIPTABLE_WARN_UNIMPLEMENTED();

  mIsLooping = enabled != PR_FALSE;
  return NS_OK;
}

/* boolean GetIsQuickTimeRegistered (); */
NS_IMETHODIMP
totemScriptablePlugin::GetIsQuickTimeRegistered(PRBool *_retval)
{
  TOTEM_SCRIPTABLE_WARN_UNIMPLEMENTED();

  *_retval = PR_FALSE;
  return NS_OK;
}

/* boolean GetIsVRMovie (); */
NS_IMETHODIMP
totemScriptablePlugin::GetIsVRMovie(PRBool *_retval)
{
  TOTEM_SCRIPTABLE_LOG_ACCESS ();

  *_retval = PR_FALSE;
  return NS_OK;
}

/* boolean GetKioskMode (); */
NS_IMETHODIMP
totemScriptablePlugin::GetKioskMode(PRBool *_retval)
{
  TOTEM_SCRIPTABLE_LOG_ACCESS ();

  *_retval = mKioskMode;
  return NS_OK;
}

/* void SetKioskMode (in boolean enabled); */
NS_IMETHODIMP
totemScriptablePlugin::SetKioskMode(PRBool enabled)
{
  TOTEM_SCRIPTABLE_WARN_UNIMPLEMENTED();

  mKioskMode = enabled != PR_FALSE;
  return NS_OK;
}

/* ACString GetLanguage (); */
NS_IMETHODIMP
totemScriptablePlugin::GetLanguage(nsACString & _retval)
{
  TOTEM_SCRIPTABLE_WARN_UNIMPLEMENTED();

  _retval.Assign ("English");
  return NS_OK;
}

/* void SetLanguage (in ACString language); */
NS_IMETHODIMP
totemScriptablePlugin::SetLanguage(const nsACString & language)
{
  TOTEM_SCRIPTABLE_WARN_UNIMPLEMENTED();

  return NS_OK;
}

/* boolean GetLoopIsPalindrome (); */
NS_IMETHODIMP
totemScriptablePlugin::GetLoopIsPalindrome(PRBool *_retval)
{
  TOTEM_SCRIPTABLE_LOG_ACCESS ();

  *_retval = mLoopIsPalindrome;
  return NS_OK;
}

/* void SetLoopIsPalindrome (in boolean enabled); */
NS_IMETHODIMP
totemScriptablePlugin::SetLoopIsPalindrome(PRBool enabled)
{
  TOTEM_SCRIPTABLE_WARN_UNIMPLEMENTED();

  mLoopIsPalindrome = enabled != PR_FALSE;
  return NS_OK;
}

/* ACString GetMatrix (); */
NS_IMETHODIMP
totemScriptablePlugin::GetMatrix(nsACString & _retval)
{
  TOTEM_SCRIPTABLE_WARN_UNIMPLEMENTED();

  _retval.Assign (mMatrix);
  return NS_OK;
}

/* void SetMatrix (in ACString matrix); */
NS_IMETHODIMP
totemScriptablePlugin::SetMatrix(const nsACString & matrix)
{
  TOTEM_SCRIPTABLE_WARN_UNIMPLEMENTED();

  mMatrix = matrix;
  return NS_OK;
}

/* unsigned long GetMaxBytesLoaded (); */
NS_IMETHODIMP
totemScriptablePlugin::GetMaxBytesLoaded(PRUint32 *_retval)
{
  TOTEM_SCRIPTABLE_WARN_UNIMPLEMENTED();

  return NS_OK;
}

/* unsigned long GetMaxTimeLoaded (); */
NS_IMETHODIMP
totemScriptablePlugin::GetMaxTimeLoaded(PRUint32 *_retval)
{
  TOTEM_SCRIPTABLE_WARN_UNIMPLEMENTED();

  return NS_OK;
}

/* ACString GetMIMEType (); */
NS_IMETHODIMP
totemScriptablePlugin::GetMIMEType(nsACString & _retval)
{
  TOTEM_SCRIPTABLE_WARN_UNIMPLEMENTED();

  _retval.Assign ("video/quicktime");
  return NS_OK;
}

/* unsigned long GetMovieID (); */
NS_IMETHODIMP
totemScriptablePlugin::GetMovieID(PRUint32 *_retval)
{
  TOTEM_SCRIPTABLE_WARN_UNIMPLEMENTED();

  return NS_OK;
}

/* void SetMovieID (in unsigned long id); */
NS_IMETHODIMP
totemScriptablePlugin::SetMovieID(PRUint32 id)
{
  TOTEM_SCRIPTABLE_WARN_UNIMPLEMENTED();

  return NS_OK;
}

/* AUTF8String GetMovieName (); */
NS_IMETHODIMP
totemScriptablePlugin::GetMovieName(nsACString & _retval)
{
  TOTEM_SCRIPTABLE_LOG_ACCESS ();

  _retval.Assign (mMovieName);
  return NS_OK;
}

/* void SetMovieName (in AUTF8String name); */
NS_IMETHODIMP
totemScriptablePlugin::SetMovieName(const nsACString & name)
{
  TOTEM_SCRIPTABLE_WARN_UNIMPLEMENTED();

  mMovieName = name;
  return NS_OK;
}

/* unsigned long GetMovieSize (); */
NS_IMETHODIMP
totemScriptablePlugin::GetMovieSize(PRUint32 *_retval)
{
  TOTEM_SCRIPTABLE_WARN_UNIMPLEMENTED();

  *_retval = 0;
  return NS_OK;
}

/* boolean GetMute (); */
NS_IMETHODIMP
totemScriptablePlugin::GetMute(PRBool *_retval)
{
  TOTEM_SCRIPTABLE_LOG_ACCESS ();

  *_retval = mMute;
  return NS_OK;
}

/* void SetMute (in boolean enabled); */
NS_IMETHODIMP
totemScriptablePlugin::SetMute(PRBool enabled)
{
  TOTEM_SCRIPTABLE_WARN_UNIMPLEMENTED();

  mMute = enabled != PR_FALSE;
  return NS_OK;
}

/* unsigned long GetNodeCount (); */
NS_IMETHODIMP
totemScriptablePlugin::GetNodeCount(PRUint32 *_retval)
{
  TOTEM_SCRIPTABLE_LOG_ACCESS ();

  return NS_ERROR_NOT_AVAILABLE;
}

/* unsigned long GetNodeID (); */
NS_IMETHODIMP
totemScriptablePlugin::GetNodeID(PRUint32 *_retval)
{
  TOTEM_SCRIPTABLE_LOG_ACCESS ();

  return NS_ERROR_NOT_AVAILABLE;
}

/* void SetNodeID (in unsigned long id); */
NS_IMETHODIMP
totemScriptablePlugin::SetNodeID(PRUint32 id)
{
  TOTEM_SCRIPTABLE_LOG_ACCESS ();

  return NS_ERROR_NOT_AVAILABLE;
}

/* float GetPanAngle (); */
NS_IMETHODIMP
totemScriptablePlugin::GetPanAngle(float *_retval)
{
  TOTEM_SCRIPTABLE_LOG_ACCESS ();

  return NS_ERROR_NOT_AVAILABLE;
}

/* void SetPanAngle (in float angle); */
NS_IMETHODIMP
totemScriptablePlugin::SetPanAngle(float angle)
{
  TOTEM_SCRIPTABLE_LOG_ACCESS ();

  return NS_ERROR_NOT_AVAILABLE;
}

/* void Play (); */
NS_IMETHODIMP
totemScriptablePlugin::Play ()
{
  TOTEM_SCRIPTABLE_LOG_ACCESS ();

  NS_ENSURE_STATE (IsValid ());

  return mPlugin->DoCommand (TOTEM_COMMAND_PLAY);
}

/* boolean GetPlayEveryFrame (); */
NS_IMETHODIMP
totemScriptablePlugin::GetPlayEveryFrame(PRBool *_retval)
{
  TOTEM_SCRIPTABLE_LOG_ACCESS ();

  *_retval = mPlayEveryFrame;
  return NS_OK;
}

/* void SetPlayEveryFrame (in boolean enabled); */
NS_IMETHODIMP
totemScriptablePlugin::SetPlayEveryFrame(PRBool enabled)
{
  TOTEM_SCRIPTABLE_WARN_UNIMPLEMENTED();

  mPlayEveryFrame = enabled != PR_FALSE;
  return NS_OK;
}

/* ACString GetPluginStatus (); */
NS_IMETHODIMP
totemScriptablePlugin::GetPluginStatus(nsACString & _retval)
{
  TOTEM_SCRIPTABLE_WARN_UNIMPLEMENTED();

  static const char *kState[] = {
    "Complete",
    "Error:<%d>",
    "Loading",
    "Playable",
    "Waiting"
  };

  if (mPluginState != eState_Error) {
    _retval.Assign (kState[mPluginState]);
  } else {
    /* FIXME */
    _retval.Assign ("Error:<1>");
  }
  return NS_OK;
}

/* ACString GetPluginVersion (); */
NS_IMETHODIMP
totemScriptablePlugin::GetPluginVersion(nsACString & _retval)
{
  TOTEM_SCRIPTABLE_WARN_UNIMPLEMENTED();

  _retval.Assign ("7.0"); /* FIXME */
  return NS_OK;
}

/* AUTF8String GetQTNEXTUrl (in unsigned long index); */
NS_IMETHODIMP
totemScriptablePlugin::GetQTNEXTUrl (PRUint32 index,
				     nsACString & _retval)
{
  TOTEM_SCRIPTABLE_WARN_UNIMPLEMENTED();

  return NS_OK;
}

/* void SetQTNEXTUrl (in unsigned long index, in AUTF8String url); */
NS_IMETHODIMP
totemScriptablePlugin::SetQTNEXTUrl (PRUint32 index,
				     const nsACString & url)
{
  TOTEM_SCRIPTABLE_WARN_UNIMPLEMENTED();

  return NS_OK;
}

/* unsigned long GetQuickTimeConnectionSpeed (); */
NS_IMETHODIMP
totemScriptablePlugin::GetQuickTimeConnectionSpeed(PRUint32 *_retval)
{
  TOTEM_SCRIPTABLE_WARN_UNIMPLEMENTED();

  *_retval = 300000; /* FIXME */
  return NS_OK;
}

/* ACString GetQuickTimeLanguage (); */
NS_IMETHODIMP
totemScriptablePlugin::GetQuickTimeLanguage(nsACString & _retval)
{
  TOTEM_SCRIPTABLE_WARN_UNIMPLEMENTED();

  _retval.Assign ("English"); /* FIXME */
  return NS_OK;
}

/* ACString GetQuickTimeVersion (); */
NS_IMETHODIMP
totemScriptablePlugin::GetQuickTimeVersion(nsACString & _retval)
{
  TOTEM_SCRIPTABLE_WARN_UNIMPLEMENTED();

  _retval.Assign ("7.0"); /* FIXME */
  return NS_OK;
}

/* float GetRate (); */
NS_IMETHODIMP
totemScriptablePlugin::GetRate(float *_retval)
{
  TOTEM_SCRIPTABLE_WARN_UNIMPLEMENTED();

  *_retval = mRate;
  return NS_OK;
}

/* void SetRate (in float rate); */
NS_IMETHODIMP
totemScriptablePlugin::SetRate(float rate)
{
  TOTEM_SCRIPTABLE_WARN_UNIMPLEMENTED();

  mRate = rate;
  return NS_OK;
}

/* void SetRectangle (in ACString rectangle); */
NS_IMETHODIMP
totemScriptablePlugin::SetRectangle(const nsACString & rectangle)
{
  TOTEM_SCRIPTABLE_WARN_UNIMPLEMENTED();

  mRectangle = rectangle;
  return NS_OK;
}

/* ACString GetRectangle (); */
NS_IMETHODIMP
totemScriptablePlugin::GetRectangle(nsACString & _retval)
{
  TOTEM_SCRIPTABLE_WARN_UNIMPLEMENTED();

  _retval.Assign (mRectangle);
  return NS_OK;
}

/* boolean GetResetPropertiesOnReload (); */
NS_IMETHODIMP
totemScriptablePlugin::GetResetPropertiesOnReload(PRBool *_retval)
{
  TOTEM_SCRIPTABLE_LOG_ACCESS ();

  *_retval = mResetPropertiesOnReload;
  return NS_OK;
}

/* void SetResetPropertiesOnReload (in boolean enabled); */
NS_IMETHODIMP
totemScriptablePlugin::SetResetPropertiesOnReload(PRBool enabled)
{
  TOTEM_SCRIPTABLE_WARN_UNIMPLEMENTED();

  mResetPropertiesOnReload = enabled != PR_FALSE;
  return NS_OK;
}

/* void Rewind (); */
NS_IMETHODIMP
totemScriptablePlugin::Rewind ()
{
  TOTEM_SCRIPTABLE_LOG_ACCESS ();

  NS_ENSURE_STATE (IsValid ());

  return mPlugin->DoCommand (TOTEM_COMMAND_PAUSE);
}

/* void ShowDefaultView (); */
NS_IMETHODIMP
totemScriptablePlugin::ShowDefaultView()
{
  TOTEM_SCRIPTABLE_LOG_ACCESS ();

  return NS_OK;
}

/* ACString GetSpriteTrackVariable (in unsigned long track, in unsigned long index); */
NS_IMETHODIMP
totemScriptablePlugin::GetSpriteTrackVariable (PRUint32 track,
					       PRUint32 index,
					       nsACString & _retval)
{
  TOTEM_SCRIPTABLE_WARN_UNIMPLEMENTED();

  return NS_OK;
}

/* void SetSpriteTrackVariable (in unsigned long track, in unsigned long index, in ACString value); */
NS_IMETHODIMP
totemScriptablePlugin::SetSpriteTrackVariable (PRUint32 track,
					       PRUint32 index,
					       const nsACString & value)
{
  TOTEM_SCRIPTABLE_WARN_UNIMPLEMENTED();

  return NS_OK;
}

/* void SetStartTime (in unsigned long time); */
NS_IMETHODIMP
totemScriptablePlugin::SetStartTime(PRUint32 time)
{
  TOTEM_SCRIPTABLE_WARN_UNIMPLEMENTED();

  return NS_OK;
}

/* unsigned long GetStartTime (); */
NS_IMETHODIMP
totemScriptablePlugin::GetStartTime (PRUint32 *_retval)
{
  TOTEM_SCRIPTABLE_WARN_UNIMPLEMENTED();

  *_retval = 0; /* FIXME */
  return NS_OK;
}

/* void Step (in long steps); */
NS_IMETHODIMP
totemScriptablePlugin::Step(PRInt32 steps)
{
  TOTEM_SCRIPTABLE_WARN_UNIMPLEMENTED();

  return NS_OK;
}

/* void Stop (); */
NS_IMETHODIMP
totemScriptablePlugin::Stop ()
{
  TOTEM_SCRIPTABLE_LOG_ACCESS ();

  NS_ENSURE_STATE (IsValid ());

  return mPlugin->DoCommand (TOTEM_COMMAND_STOP);
}

/* AUTF8String GetTarget (); */
NS_IMETHODIMP
totemScriptablePlugin::GetTarget(nsACString & _retval)
{
  TOTEM_SCRIPTABLE_WARN_UNIMPLEMENTED();

  return NS_OK;
}

/* void SetTarget (in AUTF8String target); */
NS_IMETHODIMP
totemScriptablePlugin::SetTarget(const nsACString & target)
{
  TOTEM_SCRIPTABLE_WARN_UNIMPLEMENTED();

  return NS_OK;
}

/* float GetTiltAngle (); */
NS_IMETHODIMP
totemScriptablePlugin::GetTiltAngle(float *_retval)
{
  TOTEM_SCRIPTABLE_LOG_ACCESS ();

  return NS_ERROR_NOT_AVAILABLE;
}

/* void SetTiltAngle (in float angle); */
NS_IMETHODIMP
totemScriptablePlugin::SetTiltAngle(float angle)
{
  TOTEM_SCRIPTABLE_LOG_ACCESS ();

  return NS_ERROR_NOT_AVAILABLE;
}

/* unsigned long GetTime (); */
NS_IMETHODIMP
totemScriptablePlugin::GetTime(PRUint32 *_retval)
{
  TOTEM_SCRIPTABLE_WARN_UNIMPLEMENTED();

  *_retval = 0;
  return NS_OK;
}

/* void SetTime (in unsigned long time); */
NS_IMETHODIMP
totemScriptablePlugin::SetTime(PRUint32 time)
{
  TOTEM_SCRIPTABLE_WARN_UNIMPLEMENTED();

  return NS_OK;
}

/* unsigned long GetTimeScale (); */
NS_IMETHODIMP
totemScriptablePlugin::GetTimeScale(PRUint32 *_retval)
{
  TOTEM_SCRIPTABLE_WARN_UNIMPLEMENTED();

  *_retval = 0;
  return NS_OK;
}

/* unsigned long GetTrackCount (); */
NS_IMETHODIMP
totemScriptablePlugin::GetTrackCount(PRUint32 *_retval)
{
  TOTEM_SCRIPTABLE_WARN_UNIMPLEMENTED();

  *_retval = 1;
  return NS_OK;
}

/* boolean GetTrackEnabled (in unsigned long index); */
NS_IMETHODIMP
totemScriptablePlugin::GetTrackEnabled (PRUint32 index,
					PRBool *_retval)
{
  TOTEM_SCRIPTABLE_WARN_UNIMPLEMENTED();

  *_retval = PR_TRUE;
  return NS_OK;
}

/* void SetTrackEnabled (in unsigned long index, in boolean enabled); */
NS_IMETHODIMP
totemScriptablePlugin::SetTrackEnabled (PRUint32 index,
					PRBool enabled)
{
  TOTEM_SCRIPTABLE_WARN_UNIMPLEMENTED();

  return NS_OK;
}

/* AUTF8String GetTrackName (in unsigned long index); */
NS_IMETHODIMP
totemScriptablePlugin::GetTrackName (PRUint32 index,
				     nsACString & _retval)
{
  TOTEM_SCRIPTABLE_WARN_UNIMPLEMENTED();

  return NS_OK;
}

/* AUTF8String GetTrackType (in unsigned long index); */
NS_IMETHODIMP
totemScriptablePlugin::GetTrackType (PRUint32 index,
				     nsACString & _retval)
{
  TOTEM_SCRIPTABLE_WARN_UNIMPLEMENTED();

  return NS_OK;
}

/* AUTF8String GetURL (); */
NS_IMETHODIMP
totemScriptablePlugin::GetURL (nsACString & _retval)
{
  TOTEM_SCRIPTABLE_WARN_UNIMPLEMENTED();

  return NS_OK;
}

/* void SetURL (in AUTF8String url); */
NS_IMETHODIMP
totemScriptablePlugin::SetURL (const nsACString & url)
{
  TOTEM_SCRIPTABLE_WARN_UNIMPLEMENTED();

  return NS_OK;
}

/* AUTF8String GetUserData (in AUTF8String identifier); */
NS_IMETHODIMP
totemScriptablePlugin::GetUserData  (const nsACString & identifier,
				     nsACString & _retval)
{
  TOTEM_SCRIPTABLE_WARN_UNIMPLEMENTED();

  return NS_OK;
}

/* unsigned long GetVolume (); */
NS_IMETHODIMP
totemScriptablePlugin::GetVolume(PRUint32 *_retval)
{
  *_retval = mVolume;
  return NS_OK;
}

/* void SetVolume (in unsigned long volume); */
NS_IMETHODIMP
totemScriptablePlugin::SetVolume(PRUint32 volume)
{
  TOTEM_SCRIPTABLE_WARN_UNIMPLEMENTED();

  mVolume = volume;
  return NS_OK;
}
