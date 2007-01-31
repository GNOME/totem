/* Totem GMP plugin
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

#include <nsDOMError.h>
#include <nsIProgrammingLanguage.h>
#include <nsISupportsImpl.h>
#include <nsMemory.h>
#include <nsXPCOM.h>

#define GNOME_ENABLE_DEBUG 1
/* define GNOME_ENABLE_DEBUG for more debug spew */
#include "debug.h"

#include "totemIGMPPlayer.h"
#include "totemIGMPControls.h"

#include "totemClassInfo.h"

#include "totemGMPPlaylist.h"
#include "totemGMPSettings.h"

#include "totemGMPPlugin.h"

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

/* 89cf81a7-1156-456f-b060-c2187df9a27c */
static const nsCID kClassID = 
  { 0x89cf81a7, 0x1156, 0x456f,
    { 0xb0, 0x60, 0xc2, 0x18, 0x7d, 0xf9, 0xa2, 0x7c } };

static const char kClassDescription[] = "totemGMPPlugin";
static const char kPluginDescription[] = "Windows Media Player Plug-in 10 (compatible; Totem)";

static const totemPluginMimeEntry kMimeTypes[] = {
	{ "application/x-mplayer2", "avi, wma, wmv", "video/x-msvideo", FALSE },
	{ "video/x-ms-asf-plugin", "asf, wmv", "video/x-ms-asf", FALSE },
	{ "video/x-msvideo", "asf, wmv", NULL, FALSE },
	{ "video/x-ms-asf", "asf", NULL, FALSE },
	{ "video/x-ms-wmv", "wmv", "video/x-ms-wmv", FALSE },
	{ "video/x-wmv", "wmv", "video/x-ms-wmv", FALSE },
	{ "video/x-ms-wvx", "wmv", "video/x-ms-wmv", FALSE },
	{ "video/x-ms-wm", "wmv", "video/x-ms-wmv", FALSE },
	{ "video/x-ms-asx", "asx", NULL, TRUE },
	{ "audio/x-ms-asx", "asx", NULL, TRUE },
	{ "video/x-ms-wma", "wma", NULL, TRUE },
	{ "audio/x-ms-wax", "wax", NULL, TRUE },
	{ "video/mpeg", "mpg", NULL, TRUE },
	{ "audio/mpeg", "mp3", NULL, TRUE },
	{ "audio/x-mpegurl", "m3u", NULL, TRUE }
};

totemScriptablePlugin::totemScriptablePlugin (totemPlugin *aPlugin)
  : mPlugin(aPlugin)
{
  D ("%s ctor [%p]", kClassDescription, (void*) this);
}

totemScriptablePlugin::~totemScriptablePlugin ()
{
  D ("%s dtor [%p]", kClassDescription, (void*) this);

  NS_IF_RELEASE (mSettingsTearOff);
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

NS_IMPL_ISUPPORTS3 (totemScriptablePlugin,
		    totemIGMPPlayer,
		    totemIGMPControls,
		    nsIClassInfo)

/* nsIClassInfo */

TOTEM_CLASSINFO_BEGIN (totemScriptablePlugin,
		       2,
		       kClassID,
		       kClassDescription)
  TOTEM_CLASSINFO_ENTRY (0, totemIGMPPlayer)
  TOTEM_CLASSINFO_ENTRY (1, totemIGMPControls)
TOTEM_CLASSINFO_END

/* totemIGMPPlayer */

#undef CURRENT_INTERFACE
#define CURRENT_INTERFACE "totemIGMPPlayer"

/* readonly attribute totemIGMPCdromCollection cdromCollection; */
NS_IMETHODIMP 
totemScriptablePlugin::GetCdromCollection(totemIGMPCdromCollection * *aCdromCollection)
{
  WARN_ACCESS ();

  return NS_ERROR_DOM_SECURITY_ERR;
}

/* readonly attribute totemIGMPClosedCaption closedCaption; */
NS_IMETHODIMP 
totemScriptablePlugin::GetClosedCaption(totemIGMPClosedCaption * *aClosedCaption)
{
  WARN_NOT_IMPLEMENTED ();

  return NS_ERROR_NOT_IMPLEMENTED;
}

/* readonly attribute totemIGMPControls controls; */
NS_IMETHODIMP 
totemScriptablePlugin::GetControls(totemIGMPControls * *aControls)
{
  return CallQueryInterface (this, aControls);
}

/* attribute totemIGMPMedia currentMedia; */
NS_IMETHODIMP 
totemScriptablePlugin::GetCurrentMedia(totemIGMPMedia * *aCurrentMedia)
{
  WARN_NOT_IMPLEMENTED ();

  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP 
totemScriptablePlugin::SetCurrentMedia(totemIGMPMedia * aCurrentMedia)
{
  WARN_NOT_IMPLEMENTED ();

  return NS_ERROR_NOT_IMPLEMENTED;
}

/* attribute totemIGMPPlaylist currentPlaylist; */
NS_IMETHODIMP 
totemScriptablePlugin::GetCurrentPlaylist(totemIGMPPlaylist * *aCurrentPlaylist)
{
  WARN_NOT_IMPLEMENTED ();

  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP 
totemScriptablePlugin::SetCurrentPlaylist(totemIGMPPlaylist * aCurrentPlaylist)
{
  WARN_NOT_IMPLEMENTED ();

  return NS_ERROR_NOT_IMPLEMENTED;
}

/* readonly attribute totemIGMPDVD dvd; */
NS_IMETHODIMP 
totemScriptablePlugin::GetDvd(totemIGMPDVD * *aDvd)
{
  WARN_ACCESS ();

  return NS_ERROR_DOM_SECURITY_ERR;
}

/* attribute boolean enableContextMenu; */
NS_IMETHODIMP 
totemScriptablePlugin::GetEnableContextMenu(PRBool *aEnableContextMenu)
{
  WARN_NOT_IMPLEMENTED ();

  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP 
totemScriptablePlugin::SetEnableContextMenu(PRBool aEnableContextMenu)
{
  WARN_NOT_IMPLEMENTED ();

  return NS_ERROR_NOT_IMPLEMENTED;
}

/* attribute boolean enabled; */
NS_IMETHODIMP 
totemScriptablePlugin::GetEnabled(PRBool *aEnabled)
{
  WARN_NOT_IMPLEMENTED ();

  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP 
totemScriptablePlugin::SetEnabled(PRBool aEnabled)
{
  WARN_NOT_IMPLEMENTED ();

  return NS_ERROR_NOT_IMPLEMENTED;
}

/* readonly attribute totemIGMPError error; */
NS_IMETHODIMP 
totemScriptablePlugin::GetError(totemIGMPError * *aError)
{
  WARN_NOT_IMPLEMENTED ();

  return NS_ERROR_NOT_IMPLEMENTED;
}

/* attribute boolean fullScreen; */
NS_IMETHODIMP 
totemScriptablePlugin::GetFullScreen(PRBool *aFullScreen)
{
  WARN_NOT_IMPLEMENTED ();

  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP 
totemScriptablePlugin::SetFullScreen(PRBool aFullScreen)
{
  WARN_NOT_IMPLEMENTED ();

  return NS_ERROR_NOT_IMPLEMENTED;
}

/* readonly attribute boolean isOnline; */
NS_IMETHODIMP 
totemScriptablePlugin::GetIsOnline(PRBool *aIsOnline)
{
  WARN_NOT_IMPLEMENTED ();

  return NS_ERROR_NOT_IMPLEMENTED;
}

/* readonly attribute boolean isRemote; */
NS_IMETHODIMP 
totemScriptablePlugin::GetIsRemote(PRBool *aIsRemote)
{
  WARN_NOT_IMPLEMENTED ();

  return NS_ERROR_DOM_SECURITY_ERR;
}

/* readonly attribute totemIGMPMediaCollection mediaCollection; */
NS_IMETHODIMP 
totemScriptablePlugin::GetMediaCollection(totemIGMPMediaCollection * *aMediaCollection)
{
  WARN_ACCESS ();

  return NS_ERROR_DOM_SECURITY_ERR;
}

/* readonly attribute totemIGMPNetwork network; */
NS_IMETHODIMP 
totemScriptablePlugin::GetNetwork(totemIGMPNetwork * *aNetwork)
{
  WARN_NOT_IMPLEMENTED ();

  return NS_ERROR_NOT_IMPLEMENTED;
}

/* readonly attribute long openState; */
NS_IMETHODIMP 
totemScriptablePlugin::GetOpenState(PRInt32 *aOpenState)
{
  WARN_NOT_IMPLEMENTED ();

  return NS_ERROR_NOT_IMPLEMENTED;
}

/* readonly attribute totemIGMPPlayerApplication playerApplication; */
NS_IMETHODIMP 
totemScriptablePlugin::GetPlayerApplication(totemIGMPPlayerApplication * *aPlayerApplication)
{
  WARN_ACCESS ();

  return NS_ERROR_DOM_SECURITY_ERR;
}

/* readonly attribute totemIGMPPlaylistCollection playlistCollection; */
NS_IMETHODIMP 
totemScriptablePlugin::GetPlaylistCollection(totemIGMPPlaylistCollection * *aPlaylistCollection)
{
  WARN_ACCESS ();

  return NS_ERROR_DOM_SECURITY_ERR;
}

/* readonly attribute long playState; */
NS_IMETHODIMP 
totemScriptablePlugin::GetPlayState(PRInt32 *aPlayState)
{
  WARN_NOT_IMPLEMENTED ();

  return NS_ERROR_NOT_IMPLEMENTED;
}

/* readonly attribute totemIGMPSettings settings; */
NS_IMETHODIMP 
totemScriptablePlugin::GetSettings(totemIGMPSettings * *aSettings)
{
  NS_ENSURE_STATE (IsValid ());

  if (!mSettingsTearOff) {
    mSettingsTearOff = new totemGMPSettings (this);
    if (!mSettingsTearOff)
      return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF (mSettingsTearOff);
  }

  return CallQueryInterface (mSettingsTearOff, aSettings);
}

/* readonly attribute AUTF8String status; */
NS_IMETHODIMP 
totemScriptablePlugin::GetStatus(nsACString & aStatus)
{
  WARN_NOT_IMPLEMENTED ();

  return NS_ERROR_NOT_IMPLEMENTED;
}

/* attribute boolean stretchToFit; */
NS_IMETHODIMP 
totemScriptablePlugin::GetStretchToFit(PRBool *aStretchToFit)
{
  WARN_NOT_IMPLEMENTED ();

  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP 
totemScriptablePlugin::SetStretchToFit(PRBool aStretchToFit)
{
  WARN_NOT_IMPLEMENTED ();

  return NS_ERROR_NOT_IMPLEMENTED;
}

/* attribute AUTF8String uiMode; */
NS_IMETHODIMP 
totemScriptablePlugin::GetUiMode(nsACString & aUiMode)
{
  WARN_NOT_IMPLEMENTED ();

  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP 
totemScriptablePlugin::SetUiMode(const nsACString & aUiMode)
{
  WARN_NOT_IMPLEMENTED ();

  return NS_ERROR_NOT_IMPLEMENTED;
}

/* attribute AUTF8String URL; */
NS_IMETHODIMP 
totemScriptablePlugin::GetURL(nsACString & aURL)
{
  WARN_NOT_IMPLEMENTED ();

  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP 
totemScriptablePlugin::SetURL(const nsACString & aURL)
{
  WARN_NOT_IMPLEMENTED ();

  return NS_ERROR_NOT_IMPLEMENTED;
}

/* readonly attribute AUTF8String versionInfo; */
NS_IMETHODIMP 
totemScriptablePlugin::GetVersionInfo(nsACString & aVersionInfo)
{
  WARN_NOT_IMPLEMENTED ();

  return NS_ERROR_NOT_IMPLEMENTED;
}

/* attribute boolean windowlessVideo; */
NS_IMETHODIMP 
totemScriptablePlugin::GetWindowlessVideo(PRBool *aWindowlessVideo)
{
  WARN_NOT_IMPLEMENTED ();

  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP 
totemScriptablePlugin::SetWindowlessVideo(PRBool aWindowlessVideo)
{
  WARN_NOT_IMPLEMENTED ();

  return NS_ERROR_NOT_IMPLEMENTED;
}

/* void close (); */
NS_IMETHODIMP 
totemScriptablePlugin::Close()
{
  WARN_ACCESS ();

  return NS_ERROR_DOM_SECURITY_ERR;
}

/* totemIGMPMedia newMedia (in AUTF8String aURL); */
NS_IMETHODIMP 
totemScriptablePlugin::NewMedia(const nsACString & aURL, totemIGMPMedia **_retval)
{
  WARN_ACCESS ();

  return NS_ERROR_DOM_SECURITY_ERR;
}

/* void openPlayer (in AUTF8String aURL); */
NS_IMETHODIMP 
totemScriptablePlugin::OpenPlayer (const nsACString & aURL)
{
  WARN_ACCESS ();

  return NS_ERROR_DOM_SECURITY_ERR;
}

/* totemIGMPPlaylist newPlaylist (in AUTF8String aName, in AUTF8String aURL); */
NS_IMETHODIMP 
totemScriptablePlugin::NewPlaylist (const nsACString & aName,
				    const nsACString & aURL,
				    totemIGMPPlaylist **_retval)
{
  WARN_NOT_IMPLEMENTED ();

  return NS_ERROR_NOT_IMPLEMENTED;
}

/* void launchURL (in AUTF8String aURL); */
NS_IMETHODIMP 
totemScriptablePlugin::LaunchURL(const nsACString & aURL)
{
  WARN_NOT_IMPLEMENTED ();

  return NS_ERROR_DOM_SECURITY_ERR;
}

/* totemIGMPControls */

#undef CURRENT_INTERFACE
#define CURRENT_INTERFACE "totemIGMPControls"

/* readonly attribute long audioLanguageCount; */
NS_IMETHODIMP 
totemScriptablePlugin::GetAudioLanguageCount(PRInt32 *aAudioLanguageCount)
{
  WARN_NOT_IMPLEMENTED ();

  return NS_ERROR_NOT_IMPLEMENTED;
}

/* attribute long currentAudioLanguage; */
NS_IMETHODIMP 
totemScriptablePlugin::GetCurrentAudioLanguage(PRInt32 *aCurrentAudioLanguage)
{
  WARN_NOT_IMPLEMENTED ();

  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP 
totemScriptablePlugin::SetCurrentAudioLanguage(PRInt32 aCurrentAudioLanguage)
{
  WARN_NOT_IMPLEMENTED ();

  return NS_ERROR_NOT_IMPLEMENTED;
}

/* attribute long currentAudioLanguageIndex; */
NS_IMETHODIMP 
totemScriptablePlugin::GetCurrentAudioLanguageIndex(PRInt32 *aCurrentAudioLanguageIndex)
{
  WARN_NOT_IMPLEMENTED ();

  return NS_ERROR_NOT_IMPLEMENTED;
}
NS_IMETHODIMP 
totemScriptablePlugin::SetCurrentAudioLanguageIndex(PRInt32 aCurrentAudioLanguageIndex)
{
  WARN_NOT_IMPLEMENTED ();

  return NS_ERROR_NOT_IMPLEMENTED;
}

/* attribute totemIGMPMedia currentItem; */
NS_IMETHODIMP 
totemScriptablePlugin::GetCurrentItem(totemIGMPMedia * *aCurrentItem)
{
  WARN_NOT_IMPLEMENTED ();

  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP 
totemScriptablePlugin::SetCurrentItem(totemIGMPMedia * aCurrentItem)
{
  WARN_NOT_IMPLEMENTED ();

  return NS_ERROR_NOT_IMPLEMENTED;
}

/* attribute long currentMarker; */
NS_IMETHODIMP 
totemScriptablePlugin::GetCurrentMarker(PRInt32 *aCurrentMarker)
{
  WARN_NOT_IMPLEMENTED ();

  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP 
totemScriptablePlugin::SetCurrentMarker(PRInt32 aCurrentMarker)
{
  WARN_NOT_IMPLEMENTED ();

  return NS_ERROR_NOT_IMPLEMENTED;
}

/* attribute double currentPosition; */
NS_IMETHODIMP 
totemScriptablePlugin::GetCurrentPosition(double *aCurrentPosition)
{
  WARN_NOT_IMPLEMENTED ();

  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP 
totemScriptablePlugin::SetCurrentPosition(double aCurrentPosition)
{
  WARN_NOT_IMPLEMENTED ();

  return NS_ERROR_NOT_IMPLEMENTED;
}

/* readonly attribute ACString currentPositionString; */
NS_IMETHODIMP 
totemScriptablePlugin::GetCurrentPositionString(nsACString & aCurrentPositionString)
{
  WARN_NOT_IMPLEMENTED ();

  return NS_ERROR_NOT_IMPLEMENTED;
}

/* attribute ACString currentPositionTimecode; */
NS_IMETHODIMP 
totemScriptablePlugin::GetCurrentPositionTimecode(nsACString & aCurrentPositionTimecode)
{
  WARN_NOT_IMPLEMENTED ();

  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP 
totemScriptablePlugin::SetCurrentPositionTimecode(const nsACString & aCurrentPositionTimecode)
{
  WARN_NOT_IMPLEMENTED ();

  return NS_ERROR_NOT_IMPLEMENTED;
}

/* void fastForward (); */
NS_IMETHODIMP 
totemScriptablePlugin::FastForward()
{
  WARN_NOT_IMPLEMENTED ();

  return NS_ERROR_NOT_IMPLEMENTED;
}

/* void fastReverse (); */
NS_IMETHODIMP 
totemScriptablePlugin::FastReverse()
{
  WARN_NOT_IMPLEMENTED ();

  return NS_ERROR_NOT_IMPLEMENTED;
}

/* AUTF8String getAudioLanguageDescription (in long index); */
NS_IMETHODIMP 
totemScriptablePlugin::GetAudioLanguageDescription(PRInt32 index, nsACString & _retval)
{
  WARN_NOT_IMPLEMENTED ();

  return NS_ERROR_NOT_IMPLEMENTED;
}

/* long getAudioLanguageID (in long index); */
NS_IMETHODIMP 
totemScriptablePlugin::GetAudioLanguageID(PRInt32 index, PRInt32 *_retval)
{
  WARN_NOT_IMPLEMENTED ();

  return NS_ERROR_NOT_IMPLEMENTED;
}

/* AUTF8String getLanguageName (in long LCID); */
NS_IMETHODIMP 
totemScriptablePlugin::GetLanguageName(PRInt32 LCID, nsACString & _retval)
{
  WARN_NOT_IMPLEMENTED ();

  return NS_ERROR_NOT_IMPLEMENTED;
}

/* boolean isAvailable (in ACString name); */
NS_IMETHODIMP 
totemScriptablePlugin::IsAvailable(const nsACString & name, PRBool *_retval)
{
  WARN_NOT_IMPLEMENTED ();

  return NS_ERROR_NOT_IMPLEMENTED;
}

/* void next (); */
NS_IMETHODIMP 
totemScriptablePlugin::Next()
{
  WARN_NOT_IMPLEMENTED ();

  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
totemScriptablePlugin::Pause ()
{
  NS_ENSURE_STATE (IsValid ());

  return mPlugin->DoCommand (TOTEM_COMMAND_PAUSE);
}

NS_IMETHODIMP
totemScriptablePlugin::Play ()
{
  NS_ENSURE_STATE (IsValid ());

  return mPlugin->DoCommand (TOTEM_COMMAND_PLAY);
}

/* void playItem (in totemIGMPMedia theMediaItem); */
NS_IMETHODIMP 
totemScriptablePlugin::PlayItem(totemIGMPMedia *theMediaItem)
{
  WARN_NOT_IMPLEMENTED ();

  return NS_ERROR_NOT_IMPLEMENTED;
}

/* void previous (); */
NS_IMETHODIMP 
totemScriptablePlugin::Previous()
{
  WARN_NOT_IMPLEMENTED ();

  return NS_ERROR_NOT_IMPLEMENTED;
}

/* void step (in long frameCount); */
NS_IMETHODIMP 
totemScriptablePlugin::Step(PRInt32 frameCount)
{
  WARN_NOT_IMPLEMENTED ();

  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
totemScriptablePlugin::Stop ()
{
  NS_ENSURE_STATE (IsValid ());

  return mPlugin->DoCommand (TOTEM_COMMAND_STOP);
}
