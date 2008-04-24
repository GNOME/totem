/* Totem MullY Plugin
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
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301  USA.
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

#include "totemIMullYPlayer.h"

#include "totemDebug.h"
#include "totemClassInfo.h"

#include "totemMullYPlugin.h"

/* 67DABFBF-D0AB-41fa-9C46-CC0F21721616 */
static const nsCID kClassID = 
  { 0x67dabfbf, 0xd0ab, 0x41fa,
    { 0x9c, 0x46, 0xcc, 0x0f, 0x21, 0x72, 0x16, 0x16 } };

static const char kClassDescription[] = "totemMullYPlugin";
static const char kPluginDescription[] = "DivX\xC2\xAE Web Player";
#define TOTEM_MULLY_VERSION "1.4.0.233"
static const char kPluginLongDescription[] = "DivX Web Player version " TOTEM_MULLY_VERSION;

static const totemPluginMimeEntry kMimeTypes[] = {
	{ "video/divx", "divx", "video/x-msvideo" },
};

totemScriptablePlugin::totemScriptablePlugin (totemPlugin *aPlugin)
  : mPlugin(aPlugin)
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

/* static */ char *
totemScriptablePlugin::PluginLongDescription ()
{
  return (char*) kPluginLongDescription;
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
		    totemIMullYPlayer,
		    nsIClassInfo)

/* nsIClassInfo */

TOTEM_CLASSINFO_BEGIN (totemScriptablePlugin,
		       1,
		       kClassID,
		       kClassDescription)
  TOTEM_CLASSINFO_ENTRY (0, totemIMullYPlayer)
TOTEM_CLASSINFO_END

/* totemIMullYPlayer */

#define TOTEM_SCRIPTABLE_INTERFACE "totemIMullYPlayer"

/* ACString GetVersion (); */
NS_IMETHODIMP
totemScriptablePlugin::GetVersion(nsACString & aVersion)
{
  aVersion.Assign (TOTEM_MULLY_VERSION);
  return NS_OK;
}

/* void SetMinVersion (in ACString version); */
NS_IMETHODIMP
totemScriptablePlugin::SetMinVersion(const nsACString & aVersion)
{
  TOTEM_SCRIPTABLE_WARN_UNIMPLEMENTED ();

  return NS_ERROR_NOT_IMPLEMENTED;
}

/* void SetMode (in ACString mode); */
NS_IMETHODIMP
totemScriptablePlugin::SetMode(const nsACString & aMode)
{
  TOTEM_SCRIPTABLE_WARN_UNIMPLEMENTED ();

  return NS_ERROR_NOT_IMPLEMENTED;
}

/* void SetAllowContextMenu (in boolean allow); */
NS_IMETHODIMP
totemScriptablePlugin::SetAllowContextMenu(PRBool allowed)
{
  TOTEM_SCRIPTABLE_WARN_UNIMPLEMENTED ();

  return NS_ERROR_NOT_IMPLEMENTED;
}

/* void SetAutoPlay (in boolean play); */
NS_IMETHODIMP
totemScriptablePlugin::SetAutoPlay(PRBool aPlay)
{
  TOTEM_SCRIPTABLE_WARN_UNIMPLEMENTED ();

  return NS_ERROR_NOT_IMPLEMENTED;
}

/* void SetLoop (in boolean loop); */
NS_IMETHODIMP
totemScriptablePlugin::SetLoop(PRBool aLoop)
{
  TOTEM_SCRIPTABLE_WARN_UNIMPLEMENTED ();

  return NS_ERROR_NOT_IMPLEMENTED;
}

/* void SetBufferingMode (in ACString mode); */
NS_IMETHODIMP
totemScriptablePlugin::SetBufferingMode(const nsACString & aMode)
{
  TOTEM_SCRIPTABLE_WARN_UNIMPLEMENTED ();

  return NS_ERROR_NOT_IMPLEMENTED;
}

/* void SetBannerEnabled (in boolean enabled); */
NS_IMETHODIMP
totemScriptablePlugin::SetBannerEnabled(PRBool enabled)
{
  TOTEM_SCRIPTABLE_WARN_UNIMPLEMENTED ();

  return NS_ERROR_NOT_IMPLEMENTED;
}

/* void SetVolume (in unsigned long volume); */
NS_IMETHODIMP
totemScriptablePlugin::SetVolume(PRUint32 aVolume)
{
  TOTEM_SCRIPTABLE_WARN_UNIMPLEMENTED ();

  return NS_ERROR_NOT_IMPLEMENTED;
}

/* void SetMovieTitle (in AUTF8String movieTitle); */
NS_IMETHODIMP
totemScriptablePlugin::SetMovieTitle (const nsACString & aMovieTitle)
{
  TOTEM_SCRIPTABLE_WARN_UNIMPLEMENTED ();

  return NS_ERROR_NOT_IMPLEMENTED;
}

/* void SetPreviewImage (in AUTF8String imageURL); */
NS_IMETHODIMP
totemScriptablePlugin::SetPreviewImage (const nsACString & aImageURL)
{
  TOTEM_SCRIPTABLE_WARN_UNIMPLEMENTED ();

  return NS_ERROR_NOT_IMPLEMENTED;
}

/* void SetPreviewMessage (in AUTF8String message); */
NS_IMETHODIMP
totemScriptablePlugin::SetPreviewMessage (const nsACString & aMessage)
{
  TOTEM_SCRIPTABLE_WARN_UNIMPLEMENTED ();

  return NS_ERROR_NOT_IMPLEMENTED;
}

/* void SetPreviewMessageFontSize (in unsigned long size); */
NS_IMETHODIMP
totemScriptablePlugin::SetPreviewMessageFontSize(PRUint32 aSize)
{
  TOTEM_SCRIPTABLE_WARN_UNIMPLEMENTED ();

  return NS_ERROR_NOT_IMPLEMENTED;
}

/* void Open (in AUTF8String URL); */
NS_IMETHODIMP
totemScriptablePlugin::Open(const nsACString & aURL)
{
  TOTEM_SCRIPTABLE_WARN_UNIMPLEMENTED ();

  return NS_ERROR_NOT_IMPLEMENTED;
}

/* void Play (); */
NS_IMETHODIMP
totemScriptablePlugin::Play ()
{
  NS_ENSURE_STATE (IsValid ());

  return mPlugin->DoCommand (TOTEM_COMMAND_PLAY);
}

/* void Pause (); */
NS_IMETHODIMP
totemScriptablePlugin::Pause ()
{
  NS_ENSURE_STATE (IsValid ());

  return mPlugin->DoCommand (TOTEM_COMMAND_PAUSE);
}

/* void StepForward (); */
NS_IMETHODIMP
totemScriptablePlugin::StepForward()
{
  TOTEM_SCRIPTABLE_WARN_UNIMPLEMENTED ();

  return NS_ERROR_NOT_IMPLEMENTED;
}

/* void StepBackward (); */
NS_IMETHODIMP
totemScriptablePlugin::StepBackward()
{
  TOTEM_SCRIPTABLE_WARN_UNIMPLEMENTED ();

  return NS_ERROR_NOT_IMPLEMENTED;
}

/* void FF (); */
NS_IMETHODIMP
totemScriptablePlugin::FF()
{
  TOTEM_SCRIPTABLE_WARN_UNIMPLEMENTED ();

  return NS_ERROR_NOT_IMPLEMENTED;
}

/* void RW (); */
NS_IMETHODIMP
totemScriptablePlugin::RW()
{
  TOTEM_SCRIPTABLE_WARN_UNIMPLEMENTED ();

  return NS_ERROR_NOT_IMPLEMENTED;
}

/* void Stop (); */
NS_IMETHODIMP
totemScriptablePlugin::Stop()
{
  NS_ENSURE_STATE (IsValid ());

  return mPlugin->DoCommand (TOTEM_COMMAND_STOP);
}

/* void Mute (); */
NS_IMETHODIMP
totemScriptablePlugin::Mute()
{
  TOTEM_SCRIPTABLE_WARN_UNIMPLEMENTED ();

  return NS_ERROR_NOT_IMPLEMENTED;
}

/* void UnMute (); */
NS_IMETHODIMP
totemScriptablePlugin::UnMute()
{
  TOTEM_SCRIPTABLE_WARN_UNIMPLEMENTED ();

  return NS_ERROR_NOT_IMPLEMENTED;
}

/* void Seek (in ACString method, in unsigned long percent); */
NS_IMETHODIMP
totemScriptablePlugin::Seek(const nsACString & aMethod, PRUint32 aPercent)
{
  TOTEM_SCRIPTABLE_WARN_UNIMPLEMENTED ();

  return NS_ERROR_NOT_IMPLEMENTED;
}

/* void About (); */
NS_IMETHODIMP
totemScriptablePlugin::About()
{
  TOTEM_SCRIPTABLE_WARN_UNIMPLEMENTED ();

  return NS_ERROR_NOT_IMPLEMENTED;
}

/* void ShowPreferences (); */
NS_IMETHODIMP
totemScriptablePlugin::ShowPreferences()
{
  TOTEM_SCRIPTABLE_WARN_UNIMPLEMENTED ();

  return NS_ERROR_NOT_IMPLEMENTED;
}

/* void ShowContextMenu (); */
NS_IMETHODIMP
totemScriptablePlugin::ShowContextMenu()
{
  TOTEM_SCRIPTABLE_WARN_UNIMPLEMENTED ();

  return NS_ERROR_NOT_IMPLEMENTED;
}

/* void GoEmbedded (); */
NS_IMETHODIMP
totemScriptablePlugin::GoEmbedded()
{
  TOTEM_SCRIPTABLE_WARN_UNIMPLEMENTED ();

  return NS_ERROR_NOT_IMPLEMENTED;
}

/* void GoWindowed (); */
NS_IMETHODIMP
totemScriptablePlugin::GoWindowed()
{
  TOTEM_SCRIPTABLE_WARN_UNIMPLEMENTED ();

  return NS_ERROR_NOT_IMPLEMENTED;
}

/* void GoFullscreen (); */
NS_IMETHODIMP
totemScriptablePlugin::GoFullscreen()
{
  TOTEM_SCRIPTABLE_WARN_UNIMPLEMENTED ();

  return NS_ERROR_NOT_IMPLEMENTED;
}

/* void Resize (in unsigned long width, in unsigned long height); */
NS_IMETHODIMP
totemScriptablePlugin::Resize(PRUint32 aWidth, PRUint32 aHeight)
{
  TOTEM_SCRIPTABLE_WARN_UNIMPLEMENTED ();

  return NS_ERROR_NOT_IMPLEMENTED;
}

/* unsigned long GetTotalTime (); */
NS_IMETHODIMP 
totemScriptablePlugin::GetTotalTime(PRUint32 *aTime)
{
  TOTEM_SCRIPTABLE_WARN_UNIMPLEMENTED ();

  return NS_ERROR_NOT_IMPLEMENTED;
}

/* unsigned long GetVideoWidth (); */
NS_IMETHODIMP
totemScriptablePlugin::GetVideoWidth(PRUint32 *aWidth)
{
  TOTEM_SCRIPTABLE_WARN_UNIMPLEMENTED ();

  return NS_ERROR_NOT_IMPLEMENTED;
}

/* unsigned long GetVideoHeight (); */
NS_IMETHODIMP
totemScriptablePlugin::GetVideoHeight(PRUint32 *aHeight)
{
  TOTEM_SCRIPTABLE_WARN_UNIMPLEMENTED ();

  return NS_ERROR_NOT_IMPLEMENTED;
}

/* double GetTotalVideoFrames (); */
NS_IMETHODIMP 
totemScriptablePlugin::GetTotalVideoFrames(double *aPosition)
{
  TOTEM_SCRIPTABLE_WARN_UNIMPLEMENTED ();

  return NS_ERROR_NOT_IMPLEMENTED;
}

/* double GetVideoFramerate (); */
NS_IMETHODIMP 
totemScriptablePlugin::GetVideoFramerate(double *aPosition)
{
  TOTEM_SCRIPTABLE_WARN_UNIMPLEMENTED ();

  return NS_ERROR_NOT_IMPLEMENTED;
}

/* unsigned long GetNumberOfAudioTracks (); */
NS_IMETHODIMP
totemScriptablePlugin::GetNumberOfAudioTracks(PRUint32 *aNumAudioTracks)
{
  TOTEM_SCRIPTABLE_WARN_UNIMPLEMENTED ();

  return NS_ERROR_NOT_IMPLEMENTED;
}

/* unsigned long GetNumberOfSubtitleTracks (); */
NS_IMETHODIMP
totemScriptablePlugin::GetNumberOfSubtitleTracks(PRUint32 *aNumSubtitleTracks)
{
  TOTEM_SCRIPTABLE_WARN_UNIMPLEMENTED ();

  return NS_ERROR_NOT_IMPLEMENTED;
}

/* ACString GetAudioTrackLanguage (in unsigned long trackIndex); */
NS_IMETHODIMP
totemScriptablePlugin::GetAudioTrackLanguage(PRUint32 trackIndex, nsACString & _retval)
{
  TOTEM_SCRIPTABLE_WARN_UNIMPLEMENTED ();

  return NS_ERROR_NOT_IMPLEMENTED;
}

/* ACString GetSubtitleTrackLanguage (in unsigned long trackIndex); */
NS_IMETHODIMP
totemScriptablePlugin::GetSubtitleTrackLanguage(PRUint32 trackIndex, nsACString & _retval)
{
  TOTEM_SCRIPTABLE_WARN_UNIMPLEMENTED ();

  return NS_ERROR_NOT_IMPLEMENTED;
}

/* AUTF8String GetAudioTrackName (in unsigned long trackIndex); */
NS_IMETHODIMP
totemScriptablePlugin::GetAudioTrackName(PRUint32 trackIndex, nsACString & _retval)
{
  TOTEM_SCRIPTABLE_WARN_UNIMPLEMENTED ();

  return NS_ERROR_NOT_IMPLEMENTED;
}

/* AUTF8String GetSubtitleTrackName (in unsigned long trackIndex); */
NS_IMETHODIMP
totemScriptablePlugin::GetSubtitleTrackName(PRUint32 trackIndex, nsACString & _retval)
{
  TOTEM_SCRIPTABLE_WARN_UNIMPLEMENTED ();

  return NS_ERROR_NOT_IMPLEMENTED;
}

/* long GetCurrentAudioTrack (); */
NS_IMETHODIMP
totemScriptablePlugin::GetCurrentAudioTrack(PRInt32 *_retval)
{
  TOTEM_SCRIPTABLE_WARN_UNIMPLEMENTED ();

  return NS_ERROR_NOT_IMPLEMENTED;
}

/* long GetCurrentSubtitleTrack (); */
NS_IMETHODIMP
totemScriptablePlugin::GetCurrentSubtitleTrack(PRInt32 *_retval)
{
  TOTEM_SCRIPTABLE_WARN_UNIMPLEMENTED ();

  return NS_ERROR_NOT_IMPLEMENTED;
}

/* void SetCurrentAudioTrack (in long index); */
NS_IMETHODIMP
totemScriptablePlugin::SetCurrentAudioTrack(PRInt32 index)
{
  TOTEM_SCRIPTABLE_WARN_UNIMPLEMENTED ();

  return NS_ERROR_NOT_IMPLEMENTED;
}

/* void SetCurrentSubtitleTrack (in long index); */
NS_IMETHODIMP
totemScriptablePlugin::SetCurrentSubtitleTrack(PRInt32 index)
{
  TOTEM_SCRIPTABLE_WARN_UNIMPLEMENTED ();

  return NS_ERROR_NOT_IMPLEMENTED;
}

