/* Totem Complex Plugin scriptable
 *
 * Copyright © 2004 Bastien Nocera <hadess@hadess.net>
 * Copyright © 2002 David A. Schleef <ds@schleef.org>
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

#include "totemClassInfo.h"

#include "totemComplexPlugin.h"

#define WARN_UNIMPLEMENTED()\
static PRBool warned = PR_FALSE;\
if (!warned) {\
	D ("WARNING! Use of unimplemented function 'totemIComplexPlayer::%s'", __FUNCTION__);\
	warned = PR_TRUE;\
}

#define SHOW_CALLS

#ifdef SHOW_CALLS
#define SHOW_CALL()\
static PRBool called = PR_FALSE;\
if (!called) {\
	D ("NOTE! Use of function 'totemIComplexPlayer::%s'", __FUNCTION__);\
	called = PR_TRUE;\
}
#else
#define SHOW_CALL()
#endif

/* 4ccca83d-30e7-4e9a-918c-09aa9236e3bb */
static const nsCID kClassID = 
{ 0x4ccca83d, 0x30e7, 0x4e9a,
  { 0x91, 0x8c, 0x09, 0xaa, 0x92, 0x36, 0xe3, 0xbb } };

static const char kClassDescription[] = "totemComplexPlugin";
static const char kPluginDescription[] = "Helix DNA Plugin: RealPlayer G2 Plug-In Compatible (compatible; Totem)";

static const totemPluginMimeEntry kMimeTypes[] = {
	{ "audio/x-pn-realaudio-plugin", "rpm", "audio/vnd.rn-realaudio", FALSE },
	{ "audio/x-pn-realaudio", "rpm" , NULL, TRUE },
	{ "application/vnd.rn-realmedia", "rpm", NULL, TRUE },
	{ "application/smil", "smil", NULL, TRUE }
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
    mVolume(50),
    mPlayState(eState_Stopped)
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
		    totemIComplexPlayer,
		    nsIClassInfo)

/* nsIClassInfo */

TOTEM_CLASSINFO_BEGIN (totemScriptablePlugin,
		       1,
		       kClassID,
		       kClassDescription)
  TOTEM_CLASSINFO_ENTRY (0, totemIComplexPlayer)
TOTEM_CLASSINFO_END

/* totemIComplexPlayer */

#if 0
/* boolean AboutBox (); */
NS_IMETHODIMP
totemScriptablePlugin::AboutBox(PRBool *_retval)
{
  WARN_UNIMPLEMENTED ();

  *_retval = PR_TRUE;
  return NS_OK;
}
#endif

/* AUTF8String GetAuthor (); */
NS_IMETHODIMP
totemScriptablePlugin::GetAuthor(nsACString & _retval)
{
  SHOW_CALL ();
  
  _retval.Assign (mAuthor);
  return NS_OK;
}

/* boolean SetAuthor (in AUTF8String author); */
NS_IMETHODIMP
totemScriptablePlugin::SetAuthor(const nsACString & author, PRBool *_retval)
{
  WARN_UNIMPLEMENTED ();

  mAuthor = author;
  *_retval = PR_TRUE;
  return NS_OK;
}

/* boolean GetAutoGoToURL (); */
NS_IMETHODIMP
totemScriptablePlugin::GetAutoGoToURL(PRBool *_retval)
{
  SHOW_CALL ();
  
  *_retval = mAutoGoToURL;
  return NS_OK;
}

/* boolean SetAutoGoToURL (in boolean enabled); */
NS_IMETHODIMP
totemScriptablePlugin::SetAutoGoToURL (PRBool enabled,
				       PRBool *_retval)
{
  WARN_UNIMPLEMENTED ();

  mAutoGoToURL = enabled != PR_FALSE;
  *_retval = PR_TRUE;
  return NS_OK;
}

/* boolean GetAutoStart (); */
NS_IMETHODIMP
totemScriptablePlugin::GetAutoStart(PRBool *_retval)
{
  SHOW_CALL ();
  
  *_retval = mAutoStart;
  return NS_OK;
}

/* boolean SetAutoStart (in boolean enabled); */
NS_IMETHODIMP
totemScriptablePlugin::SetAutoStart (PRBool enabled,
				     PRBool *_retval)
{
  WARN_UNIMPLEMENTED ();

  mAutoStart = enabled != PR_FALSE;
  *_retval = PR_TRUE;
  return NS_OK;
}

/* AUTF8String GetBackgroundColor (); */
NS_IMETHODIMP
totemScriptablePlugin::GetBackgroundColor(nsACString & _retval)
{
  SHOW_CALL ();
  
  _retval.Assign (mBackgroundColour);
  return NS_OK;
}

/* boolean SetBackgroundColor (in AUTF8String colour); */
NS_IMETHODIMP
totemScriptablePlugin::SetBackgroundColor (const nsACString & colour,
					   PRBool *_retval)
{
  WARN_UNIMPLEMENTED ();

  mBackgroundColour = colour;
  *_retval = PR_TRUE;
  return NS_OK;
}

/* long GetBandwidthAverage (); */
NS_IMETHODIMP
totemScriptablePlugin::GetBandwidthAverage(PRInt32 *_retval)
{
  WARN_UNIMPLEMENTED ();

  *_retval = 0;
  return NS_OK;
}

/* long GetBandwidthCurrent (); */
NS_IMETHODIMP
totemScriptablePlugin::GetBandwidthCurrent(PRInt32 *_retval)
{
  WARN_UNIMPLEMENTED ();

  *_retval = 0;
  return NS_OK;
}

/* long GetBufferingTimeElapsed (); */
NS_IMETHODIMP
totemScriptablePlugin::GetBufferingTimeElapsed(PRInt32 *_retval)
{
  WARN_UNIMPLEMENTED ();

  *_retval = 0;
  return NS_OK;
}

/* long GetBufferingTimeRemaining (); */
NS_IMETHODIMP
totemScriptablePlugin::GetBufferingTimeRemaining(PRInt32 *_retval)
{
  WARN_UNIMPLEMENTED ();

  *_retval = 0;
  return NS_OK;
}

/* boolean CanPause (); */
NS_IMETHODIMP
totemScriptablePlugin::CanPause(PRBool *_retval)
{
  SHOW_CALL ();
  
  *_retval = (mPlayState == eState_Playing);
  return NS_OK;
}

/* boolean CanPlay (); */
NS_IMETHODIMP
totemScriptablePlugin::CanPlay(PRBool *_retval)
{
  SHOW_CALL ();
  
  *_retval = (mPlayState != eState_Playing);
  return NS_OK;
}

#if 0
/* boolean CanPlayPause (); */
NS_IMETHODIMP
totemScriptablePlugin::CanPlayPause(PRBool *_retval)
{
  SHOW_CALL ();
  
  *_retval = (mPlayState != eState_Stopped);
  return NS_OK;
}
#endif

/* boolean GetCanSeek (); */
NS_IMETHODIMP
totemScriptablePlugin::GetCanSeek(PRBool *_retval)
{
  WARN_UNIMPLEMENTED ();

  *_retval = PR_TRUE;
  return NS_OK;
}

/* boolean SetCanSeek (in boolean enabled); */
NS_IMETHODIMP
totemScriptablePlugin::SetCanSeek(PRBool enabled, PRBool *_retval)
{
  WARN_UNIMPLEMENTED ();

  *_retval = PR_TRUE;
  return NS_OK;
}

/* boolean CanStop (); */
NS_IMETHODIMP
totemScriptablePlugin::CanStop (PRBool *_retval)
{
  WARN_UNIMPLEMENTED ();

  *_retval = PR_TRUE;
  return NS_OK;
}

/* boolean GetCenter (); */
NS_IMETHODIMP
totemScriptablePlugin::GetCenter(PRBool *_retval)
{
  SHOW_CALL ();
  
  *_retval = mCentred;
  return NS_OK;
}

/* boolean SetCenter (in boolean centred); */
NS_IMETHODIMP
totemScriptablePlugin::SetCenter (PRBool centred,
				  PRBool *_retval)
{
  WARN_UNIMPLEMENTED ();

  mCentred = centred != PR_FALSE;

  *_retval = PR_TRUE;
  return NS_OK;
}

/* long GetClipHeight (); */
NS_IMETHODIMP
totemScriptablePlugin::GetClipHeight(PRInt32 *_retval)
{
  WARN_UNIMPLEMENTED ();

  *_retval = 0;
  return NS_OK;
}

/* long GetClipWidth (); */
NS_IMETHODIMP
totemScriptablePlugin::GetClipWidth (PRInt32 *_retval)
{
  WARN_UNIMPLEMENTED ();

  *_retval = 0;
  return NS_OK;
}

/* long GetConnectionBandwidth (); */
NS_IMETHODIMP
totemScriptablePlugin::GetConnectionBandwidth (PRInt32 *_retval)
{
  WARN_UNIMPLEMENTED ();

  *_retval = 0;
  return NS_OK;
}

/* AUTF8String GetConsole (); */
NS_IMETHODIMP
totemScriptablePlugin::GetConsole(nsACString & _retval)
{
  WARN_UNIMPLEMENTED ();
  return NS_OK;
}

/* boolean SetConsole (in AUTF8String console); */
NS_IMETHODIMP
totemScriptablePlugin::SetConsole (const nsACString & console,
				   PRBool *_retval)
{
  WARN_UNIMPLEMENTED ();

  *_retval = PR_TRUE;
  return NS_OK;
}

/* boolean GetConsoleEvents (); */
NS_IMETHODIMP
totemScriptablePlugin::GetConsoleEvents(PRBool *_retval)
{
  SHOW_CALL ();
  
  *_retval = mConsoleEvents;
  return NS_OK;
}

/* boolean SetConsoleEvents (in boolean value); */
NS_IMETHODIMP
totemScriptablePlugin::SetConsoleEvents (PRBool value,
					 PRBool *_retval)
{
  WARN_UNIMPLEMENTED ();

  mConsoleEvents = value != PR_FALSE;
  *_retval = PR_TRUE;
  return NS_OK;
}

#if 0
/* boolean SetConsoleName (in AUTF8String console, in boolean unused); */
NS_IMETHODIMP
totemScriptablePlugin::SetConsoleName (const nsACString & console,
				       PRBool unused,
				       PRBool *_retval)
{
  WARN_UNIMPLEMENTED ();

  *_retval = PR_TRUE;
  return NS_OK;
}

/* boolean GetContextMenu (); */
NS_IMETHODIMP
totemScriptablePlugin::GetContextMenu(PRBool *_retval)
{
  SHOW_CALL ();
  
  *_retval = mContextMenu;
  return NS_OK;
}

/* boolean SetContextMenu (in boolean enabled); */
NS_IMETHODIMP
totemScriptablePlugin::SetContextMenu (PRBool enabled,
				       PRBool *_retval)
{
  WARN_UNIMPLEMENTED ();

  mContextMenu = enabled != PR_FALSE;
  *_retval = PR_TRUE;
  return NS_OK;
}

/* boolean GetContextMenuItem (in long itemID); */
NS_IMETHODIMP
totemScriptablePlugin::GetContextMenuItem (PRInt32 itemID,
					   PRBool *_retval)
{
  SHOW_CALL ();
  
  /* Unimplemented in helix too */
  *_retval = PR_FALSE;
  return NS_OK;
}

/* boolean SetContextMenuItem (in long itemID, in boolean enabled); */
NS_IMETHODIMP
totemScriptablePlugin::SetContextMenuItem (PRInt32 itemID,
					   PRBool enabled,
					   PRBool *_retval)
{
  SHOW_CALL ();
  
  /* Unimplemented in helix too */
  *_retval = PR_FALSE;
  return NS_OK;
}
#endif

/* AUTF8String GetControls (); */
NS_IMETHODIMP
totemScriptablePlugin::GetControls(nsACString & _retval)
{
  WARN_UNIMPLEMENTED ();

  return NS_OK;
}

/* boolean SetControls (in AUTF8String controls); */
NS_IMETHODIMP
totemScriptablePlugin::SetControls (const nsACString & controls,
				    PRBool *_retval)
{
  WARN_UNIMPLEMENTED ();

  *_retval = PR_TRUE;
  return NS_OK;
}

#if 0
/* boolean SetControlString (in AUTF8String controls); */
NS_IMETHODIMP
totemScriptablePlugin::SetControlString (const nsACString & controls,
					 PRBool *_retval)
{
  return SetControls (controls, _retval);
}
#endif

/* AUTF8String GetCopyright (); */
NS_IMETHODIMP
totemScriptablePlugin::GetCopyright (nsACString & _retval)
{
  SHOW_CALL ();
  
  _retval.Assign (mCopywrong);
  return NS_OK;
}

/* boolean SetCopyright (in AUTF8String copywrong); */
NS_IMETHODIMP
totemScriptablePlugin::SetCopyright (const nsACString & copywrong,
				     PRBool *_retval)
{
  SHOW_CALL ();
  
  mCopywrong = copywrong;
  *_retval = PR_TRUE;
  return NS_OK;
}

/* long GetCurrentEntry (); */
NS_IMETHODIMP
totemScriptablePlugin::GetCurrentEntry(PRInt32 *_retval)
{
  WARN_UNIMPLEMENTED ();

  *_retval = 0;
  return NS_OK;
}

/* void DoGotoURL (in AUTF8String url, in AUTF8String target); */
NS_IMETHODIMP
totemScriptablePlugin::DoGotoURL (const nsACString & url, const nsACString & target)
{
  WARN_UNIMPLEMENTED ();

  return NS_OK;
}

/* boolean DoNextEntry (); */
NS_IMETHODIMP
totemScriptablePlugin::DoNextEntry(PRBool *_retval)
{
  WARN_UNIMPLEMENTED ();

  *_retval = PR_TRUE;
  return NS_OK;
}

#if 0
/* boolean DoNextItem (); */
NS_IMETHODIMP
totemScriptablePlugin::DoNextItem(PRBool *_retval)
{
  SHOW_CALL ();
  
  return DoNextEntry (_retval);
}
#endif

/* boolean DoPause (); */
NS_IMETHODIMP
totemScriptablePlugin::DoPause(PRBool *_retval)
{
  SHOW_CALL ();
  
  NS_ENSURE_STATE (IsValid ());

  nsresult rv = mPlugin->DoCommand (TOTEM_COMMAND_PAUSE);

  mPlayState = eState_Paused;
  *_retval = PR_TRUE;
  return rv;
}

/* boolean DoPlay (); */
NS_IMETHODIMP
totemScriptablePlugin::DoPlay (PRBool *_retval)
{
  SHOW_CALL ();
  
  NS_ENSURE_STATE (IsValid ());

  nsresult rv = mPlugin->DoCommand (TOTEM_COMMAND_PLAY);

  mPlayState = eState_Playing;

  *_retval = PR_TRUE;
  return rv;
}

#if 0
/* boolean DoPlayPause (); */
NS_IMETHODIMP
totemScriptablePlugin::DoPlayPause(PRBool *_retval)
{
  WARN_UNIMPLEMENTED ();

  mPlayState = eState_Paused;
  *_retval = PR_TRUE;
  return NS_OK;
}
#endif

/* boolean DoPrevEntry (); */
NS_IMETHODIMP
totemScriptablePlugin::DoPrevEntry(PRBool *_retval)
{
  WARN_UNIMPLEMENTED ();

  *_retval = PR_TRUE;
  return NS_OK;
}

#if 0
/* boolean DoPrevItem (); */
NS_IMETHODIMP
totemScriptablePlugin::DoPrevItem(PRBool *_retval)
{
  WARN_UNIMPLEMENTED ();

  *_retval = PR_TRUE;
  return NS_OK;
}
#endif

/* boolean DoStop (); */
NS_IMETHODIMP
totemScriptablePlugin::DoStop (PRBool *_retval)
{
  SHOW_CALL ();
  
  NS_ENSURE_STATE (IsValid ());

  nsresult rv = mPlugin->DoCommand (TOTEM_COMMAND_STOP);

  mPlayState = eState_Stopped;
  *_retval = PR_TRUE;
  return rv;
}

/* boolean GetDoubleSize (); */
NS_IMETHODIMP
totemScriptablePlugin::GetDoubleSize(PRBool *_retval)
{
  SHOW_CALL ();
  
  return GetEnableDoubleSize (_retval);
}

/* boolean SetDoubleSize (); */
NS_IMETHODIMP
totemScriptablePlugin::SetDoubleSize(PRBool *_retval)
{
  SHOW_CALL ();
  
  return SetEnableDoubleSize (PR_TRUE, _retval);
}

/* AUTF8String GetDRMInfo (in AUTF8String info); */
NS_IMETHODIMP
totemScriptablePlugin::GetDRMInfo (const nsACString & info,
				   nsACString & _retval)
{
  SHOW_CALL ();
  
  /* Unimplemented in helix too */
  _retval.Assign ("");
  return NS_OK;
}

#if 0
/* boolean EditPreferences (); */
NS_IMETHODIMP
totemScriptablePlugin::EditPreferences (PRBool *_retval)
{
  WARN_UNIMPLEMENTED ();

  *_retval = PR_TRUE;
  return NS_OK;
}
#endif

/* boolean GetEnableContextMenu (); */
NS_IMETHODIMP
totemScriptablePlugin::GetEnableContextMenu(PRBool *_retval)
{
  SHOW_CALL ();
  
  *_retval = mContextMenu;
  return NS_OK;
}

/* boolean SetEnableContextMenu (in boolean enabled); */
NS_IMETHODIMP
totemScriptablePlugin::SetEnableContextMenu (PRBool enabled,
					     PRBool *_retval)
{
  WARN_UNIMPLEMENTED ();

  mContextMenu = enabled != PR_FALSE;
  *_retval = PR_TRUE;
  return NS_OK;
}

/* boolean GetEnableDoubleSize (); */
NS_IMETHODIMP
totemScriptablePlugin::GetEnableDoubleSize(PRBool *_retval)
{
  SHOW_CALL ();
  
  *_retval = mDoubleSize;
  return NS_OK;
}

/* boolean SetEnableDoubleSize (in boolean enabled); */
NS_IMETHODIMP
totemScriptablePlugin::SetEnableDoubleSize (PRBool enabled,
					    PRBool *_retval)
{
  WARN_UNIMPLEMENTED ();

  mDoubleSize = enabled != PR_FALSE;
  *_retval = PR_TRUE;
  return NS_OK;
}

/* boolean GetEnableFullScreen (); */
NS_IMETHODIMP
totemScriptablePlugin::GetEnableFullScreen(PRBool *_retval)
{
  SHOW_CALL ();
  
  *_retval = mFullscreen;
  return NS_OK;
}

/* boolean SetEnableFullScreen (in boolean enabled); */
NS_IMETHODIMP
totemScriptablePlugin::SetEnableFullScreen (PRBool enabled,
					    PRBool *_retval)
{
  WARN_UNIMPLEMENTED ();

  mFullscreen = enabled != PR_FALSE;
  *_retval = PR_TRUE;
  return NS_OK;
}

#if 0
/* boolean EnableMessageBox (in boolean enable); */
NS_IMETHODIMP
totemScriptablePlugin::EnableMessageBox (PRBool enable,
					 PRBool *_retval)
{
  SHOW_CALL ();
  
  return SetEnableMessageBox (enable, _retval);
}

/* boolean GetEnableMessageBox (); */
NS_IMETHODIMP
totemScriptablePlugin::GetEnableMessageBox(PRBool *_retval)
{
  *_retval = mMessageBox;
  return NS_OK;
}

/* boolean SetEnableMessageBox (in boolean enabled); */
NS_IMETHODIMP
totemScriptablePlugin::SetEnableMessageBox (PRBool enabled,
					    PRBool *_retval)
{
  WARN_UNIMPLEMENTED ();

  mMessageBox = enabled != PR_FALSE;
  *_retval = PR_TRUE;
  return NS_OK;
}
#endif

/* boolean GetEnableOriginalSize (); */
NS_IMETHODIMP
totemScriptablePlugin::GetEnableOriginalSize(PRBool *_retval)
{
  SHOW_CALL ();
  
  *_retval = mOriginalSize;
  return NS_OK;
}

/* boolean SetEnableOriginalSize (in boolean enabled); */
NS_IMETHODIMP
totemScriptablePlugin::SetEnableOriginalSize (PRBool enabled,
					      PRBool *_retval)
{
  WARN_UNIMPLEMENTED ();

  mOriginalSize = enabled != PR_FALSE;
  *_retval = PR_TRUE;
  return NS_OK;
}

/* AUTF8String GetEntryAbstract (in long index); */
NS_IMETHODIMP
totemScriptablePlugin::GetEntryAbstract (PRInt32 index,
					 nsACString & _retval)
{
  WARN_UNIMPLEMENTED ();
  return NS_OK;
}

/* AUTF8String GetEntryAuthor (in long index); */
NS_IMETHODIMP
totemScriptablePlugin::GetEntryAuthor (PRInt32 index,
				       nsACString & _retval)
{
  WARN_UNIMPLEMENTED ();
  return NS_OK;
}

/* AUTF8String GetEntryCopyright (in long index); */
NS_IMETHODIMP
totemScriptablePlugin::GetEntryCopyright (PRInt32 index,
					  nsACString & _retval)
{
  WARN_UNIMPLEMENTED ();
  return NS_OK;
}

/* AUTF8String GetEntryTitle (in long index); */
NS_IMETHODIMP
totemScriptablePlugin::GetEntryTitle(PRInt32 index,
				      nsACString & _retval)
{
  WARN_UNIMPLEMENTED ();
  return NS_OK;
}

/* boolean GetFullScreen (); */
NS_IMETHODIMP
totemScriptablePlugin::GetFullScreen (PRBool *_retval)
{
  SHOW_CALL ();
  
  return GetEnableFullScreen (_retval);
}

/* boolean SetFullScreen (); */
NS_IMETHODIMP
totemScriptablePlugin::SetFullScreen (PRBool *_retval)
{
  SHOW_CALL ();
  
  return SetEnableFullScreen (PR_TRUE, _retval);
}

/* boolean HasNextEntry (); */
NS_IMETHODIMP
totemScriptablePlugin::HasNextEntry (PRBool *_retval)
{
  WARN_UNIMPLEMENTED ();

  *_retval = PR_TRUE;
  return NS_OK;
}

#if 0
/* boolean HasNextItem (); */
NS_IMETHODIMP
totemScriptablePlugin::HasNextItem(PRBool *_retval)
{
  return HasNextEntry (_retval);
}
#endif

/* boolean HasPrevEntry (); */
NS_IMETHODIMP
totemScriptablePlugin::HasPrevEntry(PRBool *_retval)
{
  WARN_UNIMPLEMENTED ();

  *_retval = PR_TRUE;
  return NS_OK;
}

#if 0
/* boolean HasPrevItem (); */
NS_IMETHODIMP
totemScriptablePlugin::HasPrevItem(PRBool *_retval)
{
  return HasPrevEntry (_retval);
}

/* boolean HideShowStatistics (); */
NS_IMETHODIMP
totemScriptablePlugin::HideShowStatistics(PRBool *_retval)
{
  WARN_UNIMPLEMENTED ();

  *_retval = PR_TRUE;
  return NS_OK;
}
#endif

/* boolean GetImageStatus (); */
NS_IMETHODIMP
totemScriptablePlugin::GetImageStatus(PRBool *_retval)
{
  SHOW_CALL ();
  
  *_retval = mImageStatus;
  return NS_OK;
}

/* boolean SetImageStatus (in boolean enabled); */
NS_IMETHODIMP
totemScriptablePlugin::SetImageStatus(PRBool enabled, PRBool *_retval)
{
  WARN_UNIMPLEMENTED ();

  mImageStatus = enabled != PR_FALSE;
  *_retval = PR_TRUE;
  return NS_OK;
}

#if 0
/* boolean IsDone (); */
NS_IMETHODIMP
totemScriptablePlugin::IsDone(PRBool *_retval)
{
  WARN_UNIMPLEMENTED ();

  *_retval = PR_TRUE;
  return NS_OK;
}
#endif

/* boolean GetIsPlus (); */
NS_IMETHODIMP
totemScriptablePlugin::GetIsPlus(PRBool *_retval)
{
  WARN_UNIMPLEMENTED ();

  *_retval = PR_FALSE;
  return NS_OK;
}

#if 0
/* boolean IsStatisticsVisible (); */
NS_IMETHODIMP
totemScriptablePlugin::IsStatisticsVisible(PRBool *_retval)
{
  SHOW_CALL ();
  
  *_retval = mShowStats;
  return NS_OK;
}

/* boolean IsZoomed (); */
NS_IMETHODIMP
totemScriptablePlugin::IsZoomed(PRBool *_retval)
{
  SHOW_CALL ();
  
  *_retval = mZoomed;
  return NS_OK;
}
#endif


/* AUTF8String GetLastErrorMoreInfoURL (); */
NS_IMETHODIMP
totemScriptablePlugin::GetLastErrorMoreInfoURL(nsACString & _retval)
{
  WARN_UNIMPLEMENTED ();
  return NS_OK;
}

/* long GetLastErrorRMACode (); */
NS_IMETHODIMP
totemScriptablePlugin::GetLastErrorRMACode(PRInt32 *_retval)
{
  WARN_UNIMPLEMENTED ();

  *_retval = 0;
  return NS_OK;
}

#if 0
/* AUTF8String GetLastErrorRMACodeString (); */
NS_IMETHODIMP
totemScriptablePlugin::GetLastErrorRMACodeString(nsACString & _retval)
{
  WARN_UNIMPLEMENTED ();
  return NS_OK;
}
#endif

/* long GetLastErrorSeverity (); */
NS_IMETHODIMP
totemScriptablePlugin::GetLastErrorSeverity(PRInt32 *_retval)
{
  WARN_UNIMPLEMENTED ();

  *_retval = eErrorSeverity_General;
  return NS_OK;
}

/* long GetLastErrorUserCode (); */
NS_IMETHODIMP
totemScriptablePlugin::GetLastErrorUserCode(PRInt32 *_retval)
{
  SHOW_CALL ();
  
  /* It's expected to always return 0 */
  *_retval = 0;
  return NS_OK;
}

/* AUTF8String GetLastErrorUserString (); */
NS_IMETHODIMP
totemScriptablePlugin::GetLastErrorUserString(nsACString & _retval)
{
  SHOW_CALL ();
  
  /* It's expected to always return "" */
  _retval.Assign ("");
  return NS_OK;
}

#if 0
/* long GetLastKeyDownKey (); */
NS_IMETHODIMP
totemScriptablePlugin::GetLastKeyDownKey(PRInt32 *_retval)
{
  SHOW_CALL ();
  
  /* Unimplemented in helix too */
  *_retval = 0;
  return NS_OK;
}

/* long GetLastKeyDownTimeStamp (); */
NS_IMETHODIMP
totemScriptablePlugin::GetLastKeyDownTimeStamp (PRInt32 *_retval)
{
  SHOW_CALL ();
  
  /* Unimplemented in helix too */
  *_retval = 0;
  return NS_OK;
}

/* long GetLastKeyPressKey (); */
NS_IMETHODIMP
totemScriptablePlugin::GetLastKeyPressKey(PRInt32 *_retval)
{
  SHOW_CALL ();
  
  /* Unimplemented in helix too */
  *_retval = 0;
  return NS_OK;
}

/* long GetLastKeyPressTimeStamp (); */
NS_IMETHODIMP
totemScriptablePlugin::GetLastKeyPressTimeStamp (PRInt32 *_retval)
{
  SHOW_CALL ();
  
  /* Unimplemented in helix too */
  *_retval = 0;
  return NS_OK;
}

/* long GetLastKeyUpKey (); */
NS_IMETHODIMP
totemScriptablePlugin::GetLastKeyUpKey(PRInt32 *_retval)
{
  SHOW_CALL ();
  
  /* Unimplemented in helix too */
  *_retval = 0;
  return NS_OK;
}

/* long GetLastKeyUpTimeStamp (); */
NS_IMETHODIMP
totemScriptablePlugin::GetLastKeyUpTimeStamp (PRInt32 *_retval)
{
  SHOW_CALL ();
  
  /* Unimplemented in helix too */
  *_retval = 0;
  return NS_OK;
}

/* long GetLastLeftButtonDblKeyFlags (); */
NS_IMETHODIMP
totemScriptablePlugin::GetLastLeftButtonDblKeyFlags(PRInt32 *_retval)
{
  SHOW_CALL ();
  
  /* Unimplemented in helix too */
  *_retval = 0;
  return NS_OK;
}

/* long GetLastLeftButtonDblTimeStamp (); */
NS_IMETHODIMP
totemScriptablePlugin::GetLastLeftButtonDblTimeStamp (PRInt32 *_retval)
{
  SHOW_CALL ();
  
  /* Unimplemented in helix too */
  *_retval = 0;
  return NS_OK;
}

/* long GetLastLeftButtonDblXPos (); */
NS_IMETHODIMP
totemScriptablePlugin::GetLastLeftButtonDblXPos(PRInt32 *_retval)
{
  SHOW_CALL ();
  
  /* Unimplemented in helix too */
  *_retval = 0;
  return NS_OK;
}

/* long GetLastLeftButtonDblYPos (); */
NS_IMETHODIMP
totemScriptablePlugin::GetLastLeftButtonDblYPos(PRInt32 *_retval)
{
  SHOW_CALL ();
  
  /* Unimplemented in helix too */
  *_retval = 0;
  return NS_OK;
}

/* long GetLastLeftButtonDownKeyFlags (); */
NS_IMETHODIMP
totemScriptablePlugin::GetLastLeftButtonDownKeyFlags(PRInt32 *_retval)
{
  SHOW_CALL ();
  
  /* Unimplemented in helix too */
  *_retval = 0;
  return NS_OK;
}

/* long GetLastLeftButtonDownTimeStamp (); */
NS_IMETHODIMP
totemScriptablePlugin::GetLastLeftButtonDownTimeStamp (PRInt32 *_retval)
{
  SHOW_CALL ();
  
  /* Unimplemented in helix too */
  *_retval = 0;
  return NS_OK;
}

/* long GetLastLeftButtonDownXPos (); */
NS_IMETHODIMP
totemScriptablePlugin::GetLastLeftButtonDownXPos(PRInt32 *_retval)
{
  SHOW_CALL ();
  
  /* Unimplemented in helix too */
  *_retval = 0;
  return NS_OK;
}

/* long GetLastLeftButtonDownYPos (); */
NS_IMETHODIMP
totemScriptablePlugin::GetLastLeftButtonDownYPos(PRInt32 *_retval)
{
  SHOW_CALL ();
  
  /* Unimplemented in helix too */
  *_retval = 0;
  return NS_OK;
}

/* long GetLastLeftButtonUpKeyFlags (); */
NS_IMETHODIMP
totemScriptablePlugin::GetLastLeftButtonUpKeyFlags(PRInt32 *_retval)
{
  SHOW_CALL ();
  
  /* Unimplemented in helix too */
  *_retval = 0;
  return NS_OK;
}

/* long GetLastLeftButtonUpTimeStamp (); */
NS_IMETHODIMP
totemScriptablePlugin::GetLastLeftButtonUpTimeStamp (PRInt32 *_retval)
{
  SHOW_CALL ();
  
  /* Unimplemented in helix too */
  *_retval = 0;
  return NS_OK;
}

/* long GetLastLeftButtonUpXPos (); */
NS_IMETHODIMP
totemScriptablePlugin::GetLastLeftButtonUpXPos(PRInt32 *_retval)
{
  SHOW_CALL ();
  
  /* Unimplemented in helix too */
  *_retval = 0;
  return NS_OK;
}

/* long GetLastLeftButtonUpYPos (); */
NS_IMETHODIMP
totemScriptablePlugin::GetLastLeftButtonUpYPos(PRInt32 *_retval)
{
  SHOW_CALL ();
  
  /* Unimplemented in helix too */
  *_retval = 0;
  return NS_OK;
}

/* long GetLastMouseMoveKeyFlags (); */
NS_IMETHODIMP
totemScriptablePlugin::GetLastMouseMoveKeyFlags(PRInt32 *_retval)
{
  SHOW_CALL ();
  
  /* Unimplemented in helix too */
  *_retval = 0;
  return NS_OK;
}

/* long GetLastMouseMoveTimeStamp (); */
NS_IMETHODIMP
totemScriptablePlugin::GetLastMouseMoveTimeStamp (PRInt32 *_retval)
{
  SHOW_CALL ();
  
  /* Unimplemented in helix too */
  *_retval = 0;
  return NS_OK;
}

/* long GetLastMouseMoveXPos (); */
NS_IMETHODIMP
totemScriptablePlugin::GetLastMouseMoveXPos(PRInt32 *_retval)
{
  SHOW_CALL ();
  
  /* Unimplemented in helix too */
  *_retval = 0;
  return NS_OK;
}

/* long GetLastMouseMoveYPos (); */
NS_IMETHODIMP
totemScriptablePlugin::GetLastMouseMoveYPos(PRInt32 *_retval)
{
  SHOW_CALL ();
  
  /* Unimplemented in helix too */
  *_retval = 0;
  return NS_OK;
}

/* long GetLastRightButtonDblKeyFlags (); */
NS_IMETHODIMP
totemScriptablePlugin::GetLastRightButtonDblKeyFlags(PRInt32 *_retval)
{
  SHOW_CALL ();
  
  /* Unimplemented in helix too */
  *_retval = 0;
  return NS_OK;
}

/* long GetLastRightButtonDblTimeStamp (); */
NS_IMETHODIMP
totemScriptablePlugin::GetLastRightButtonDblTimeStamp (PRInt32 *_retval)
{
  SHOW_CALL ();
  
  /* Unimplemented in helix too */
  *_retval = 0;
  return NS_OK;
}

/* long GetLastRightButtonDblXPos (); */
NS_IMETHODIMP
totemScriptablePlugin::GetLastRightButtonDblXPos(PRInt32 *_retval)
{
  SHOW_CALL ();
  
  /* Unimplemented in helix too */
  *_retval = 0;
  return NS_OK;
}

/* long GetLastRightButtonDblYPos (); */
NS_IMETHODIMP
totemScriptablePlugin::GetLastRightButtonDblYPos(PRInt32 *_retval)
{
  SHOW_CALL ();
  
  /* Unimplemented in helix too */
  *_retval = 0;
  return NS_OK;
}

/* long GetLastRightButtonDownKeyFlags (); */
NS_IMETHODIMP
totemScriptablePlugin::GetLastRightButtonDownKeyFlags(PRInt32 *_retval)
{
  SHOW_CALL ();
  
  /* Unimplemented in helix too */
  *_retval = 0;
  return NS_OK;
}

/* long GetLastRightButtonDownTimeStamp (); */
NS_IMETHODIMP
totemScriptablePlugin::GetLastRightButtonDownTimeStamp (PRInt32 *_retval)
{
  SHOW_CALL ();
  
  /* Unimplemented in helix too */
  *_retval = 0;
  return NS_OK;
}

/* long GetLastRightButtonDownXPos (); */
NS_IMETHODIMP
totemScriptablePlugin::GetLastRightButtonDownXPos(PRInt32 *_retval)
{
  SHOW_CALL ();
  
  /* Unimplemented in helix too */
  *_retval = 0;
  return NS_OK;
}

/* long GetLastRightButtonDownYPos (); */
NS_IMETHODIMP
totemScriptablePlugin::GetLastRightButtonDownYPos(PRInt32 *_retval)
{
  SHOW_CALL ();
  
  /* Unimplemented in helix too */
  *_retval = 0;
  return NS_OK;
}

/* long GetLastRightButtonUpKeyFlags (); */
NS_IMETHODIMP
totemScriptablePlugin::GetLastRightButtonUpKeyFlags(PRInt32 *_retval)
{
  SHOW_CALL ();
  
  /* Unimplemented in helix too */
  *_retval = 0;
  return NS_OK;
}

/* long GetLastRightButtonUpTimeStamp (); */
NS_IMETHODIMP
totemScriptablePlugin::GetLastRightButtonUpTimeStamp (PRInt32 *_retval)
{
  SHOW_CALL ();
  
  /* Unimplemented in helix too */
  *_retval = 0;
  return NS_OK;
}

/* long GetLastRightButtonUpXPos (); */
NS_IMETHODIMP
totemScriptablePlugin::GetLastRightButtonUpXPos(PRInt32 *_retval)
{
  SHOW_CALL ();
  
  /* Unimplemented in helix too */
  *_retval = 0;
  return NS_OK;
}

/* long GetLastRightButtonUpYPos (); */
NS_IMETHODIMP
totemScriptablePlugin::GetLastRightButtonUpYPos(PRInt32 *_retval)
{
  SHOW_CALL ();
  
  /* Unimplemented in helix too */
  *_retval = 0;
  return NS_OK;
}
#endif

/* AUTF8String GetLastMessage (); */
NS_IMETHODIMP
totemScriptablePlugin::GetLastMessage(nsACString & _retval)
{
  WARN_UNIMPLEMENTED ();

  _retval.Assign ("");
  return NS_OK;
}

/* AUTF8String GetLastStatus (); */
NS_IMETHODIMP
totemScriptablePlugin::GetLastStatus(nsACString & _retval)
{
  WARN_UNIMPLEMENTED ();

  return NS_OK;
}

/* long GetLength (); */
NS_IMETHODIMP
totemScriptablePlugin::GetLength (PRInt32 *_retval)
{
  WARN_UNIMPLEMENTED ();

  *_retval = 1;
  return NS_OK;
}

/* boolean GetLiveState (); */
NS_IMETHODIMP
totemScriptablePlugin::GetLiveState(PRBool *_retval)
{
  WARN_UNIMPLEMENTED ();

  *_retval = PR_TRUE;
  return NS_OK;
}

/* boolean GetLoop (); */
NS_IMETHODIMP
totemScriptablePlugin::GetLoop (PRBool *_retval)
{
  SHOW_CALL ();
  
  *_retval = mLoop;
  return NS_OK;
}

/* boolean SetLoop (in boolean enabled); */
NS_IMETHODIMP
totemScriptablePlugin::SetLoop (PRBool enabled,
			        PRBool *_retval)
{
  WARN_UNIMPLEMENTED ();

  mLoop = enabled != PR_FALSE;
  *_retval = PR_TRUE;
  return NS_OK;
}

/* boolean GetMaintainAspect (); */
NS_IMETHODIMP
totemScriptablePlugin::GetMaintainAspect (PRBool *_retval)
{
  SHOW_CALL ();
  
  *_retval = mMaintainAspect;
  return NS_OK;
}

/* boolean SetMaintainAspect (in boolean enabled); */
NS_IMETHODIMP
totemScriptablePlugin::SetMaintainAspect (PRBool enabled,
					  PRBool *_retval)
{
  WARN_UNIMPLEMENTED ();

  mMaintainAspect = enabled != PR_FALSE;
  *_retval = PR_TRUE;
  return NS_OK;
}

/* boolean GetMute (); */
NS_IMETHODIMP
totemScriptablePlugin::GetMute(PRBool *_retval)
{
  SHOW_CALL ();
  
  *_retval = mMute;
  return NS_OK;
}

/* boolean SetMute (in boolean enabled); */
NS_IMETHODIMP
totemScriptablePlugin::SetMute (PRBool enabled,
				PRBool *_retval)
{
  WARN_UNIMPLEMENTED ();

  mMute = enabled != PR_FALSE;
  *_retval = PR_TRUE;
  return NS_OK;
}

#if 0
/* boolean GetNoLabels (); */
NS_IMETHODIMP
totemScriptablePlugin::GetNoLabels(PRBool *_retval)
{
  SHOW_CALL ();
  
  *_retval = mNoLabels;
  return NS_OK;
}

/* boolean SetNoLabels (in boolean enabled); */
NS_IMETHODIMP
totemScriptablePlugin::SetNoLabels(PRBool enabled, PRBool *_retval)
{
  WARN_UNIMPLEMENTED ();

  mNoLabels = enabled != PR_FALSE;
  *_retval = PR_TRUE;
  return NS_OK;
}
#endif

/* boolean GetNoLogo (); */
NS_IMETHODIMP
totemScriptablePlugin::GetNoLogo(PRBool *_retval)
{
  SHOW_CALL ();
  
  *_retval = mNoLogo;
  return NS_OK;
}

/* boolean SetNoLogo (in boolean enabled); */
NS_IMETHODIMP
totemScriptablePlugin::SetNoLogo(PRBool enabled, PRBool *_retval)
{
  WARN_UNIMPLEMENTED ();

  mNoLogo = enabled != PR_FALSE;
  *_retval = PR_TRUE;
  return NS_OK;
}

/* long GetNumEntries (); */
NS_IMETHODIMP
totemScriptablePlugin::GetNumEntries(PRInt32 *_retval)
{
  WARN_UNIMPLEMENTED ();

  *_retval = 1;
  return NS_OK;
}

/* boolean SetNumLoop (in long numLoops); */
NS_IMETHODIMP
totemScriptablePlugin::SetNumLoop (PRInt32 numLoops,
				   PRBool *_retval)
{
  WARN_UNIMPLEMENTED ();

  mNumLoops = numLoops;
  *_retval = PR_TRUE;
  return NS_OK;
}

/* long GetNumLoop (); */
NS_IMETHODIMP
totemScriptablePlugin::GetNumLoop (PRInt32 *_retval)
{
  *_retval = mNumLoops;
  return NS_OK;
}

/* long GetNumSources (); */
NS_IMETHODIMP
totemScriptablePlugin::GetNumSources(PRInt32 *_retval)
{
  WARN_UNIMPLEMENTED ();

  *_retval = 1;
  return NS_OK;
}

/* boolean GetOriginalSize (); */
NS_IMETHODIMP
totemScriptablePlugin::GetOriginalSize(PRBool *_retval)
{
  SHOW_CALL ();
  
  return GetEnableOriginalSize (_retval);
}

/* boolean SetOriginalSize (); */
NS_IMETHODIMP
totemScriptablePlugin::SetOriginalSize(PRBool *_retval)
{
  SHOW_CALL ();
  
  return SetEnableOriginalSize (PR_TRUE, _retval);
}

/* long GetPacketsLate (); */
NS_IMETHODIMP
totemScriptablePlugin::GetPacketsEarly(PRInt32 *_retval)
{
  WARN_UNIMPLEMENTED ();

  *_retval = 0;
  return NS_OK;
}

/* long GetPacketsLate (); */
NS_IMETHODIMP
totemScriptablePlugin::GetPacketsLate(PRInt32 *_retval)
{
  WARN_UNIMPLEMENTED ();

  *_retval = 0;
  return NS_OK;
}

/* long GetPacketsMissing (); */
NS_IMETHODIMP
totemScriptablePlugin::GetPacketsMissing(PRInt32 *_retval)
{
  WARN_UNIMPLEMENTED ();

  *_retval = 0;
  return NS_OK;
}

/* long GetPacketsOutOfOrder (); */
NS_IMETHODIMP
totemScriptablePlugin::GetPacketsOutOfOrder(PRInt32 *_retval)
{
  WARN_UNIMPLEMENTED ();

  *_retval = 0;
  return NS_OK;
}

/* long GetPacketsReceived (); */
NS_IMETHODIMP
totemScriptablePlugin::GetPacketsReceived(PRInt32 *_retval)
{
  WARN_UNIMPLEMENTED ();

  *_retval = 0;
  return NS_OK;
}

/* long GetPacketsTotal (); */
NS_IMETHODIMP
totemScriptablePlugin::GetPacketsTotal (PRInt32 *_retval)
{
  WARN_UNIMPLEMENTED ();

  *_retval = 0;
  return NS_OK;
}

/* long GetPlayState (); */
NS_IMETHODIMP
totemScriptablePlugin::GetPlayState(PRInt32 *_retval)
{
  SHOW_CALL ();
  
  *_retval = mPlayState;
  return NS_OK;
}

/* boolean SetPosition (in long position); */
NS_IMETHODIMP
totemScriptablePlugin::SetPosition (PRInt32 position,
				    PRBool *_retval)
{
  WARN_UNIMPLEMENTED ();

  *_retval = PR_TRUE;
  return NS_OK;
}

/* long GetPosition (); */
NS_IMETHODIMP
totemScriptablePlugin::GetPosition(PRInt32 *_retval)
{
  WARN_UNIMPLEMENTED ();

  *_retval = 0;
  return NS_OK;
}

/* long GetPreferedLanguageID (); */
NS_IMETHODIMP
totemScriptablePlugin::GetPreferedLanguageID(PRInt32 *_retval)
{
  return GetPreferredLanguageID (_retval);
}

/* long GetPreferredLanguageID (); */
NS_IMETHODIMP 
totemScriptablePlugin::GetPreferredLanguageID(PRInt32 *_retval)
{
  WARN_UNIMPLEMENTED ();

  *_retval = 0;
  return NS_OK;
}

/* AUTF8String GetPreferedLanguageString (); */
NS_IMETHODIMP
totemScriptablePlugin::GetPreferedLanguageString(nsACString & _retval)
{
  return GetPreferredLanguageString (_retval);
}

/* AUTF8String GetPreferredLanguageString (); */
NS_IMETHODIMP 
totemScriptablePlugin::GetPreferredLanguageString(nsACString & _retval)
{
  WARN_UNIMPLEMENTED ();
  return NS_OK;
}

/* boolean GetPreFetch (); */
NS_IMETHODIMP
totemScriptablePlugin::GetPreFetch (PRBool *_retval)
{
  *_retval = mPrefetch;
  return NS_OK;
}

/* boolean SetPreFetch (in boolean enabled); */
NS_IMETHODIMP
totemScriptablePlugin::SetPreFetch (PRBool enabled,
				    PRBool *_retval)
{
  WARN_UNIMPLEMENTED ();

  mPrefetch = enabled != PR_FALSE;
  *_retval = PR_TRUE;
  return NS_OK;
}

#if 0
/* boolean ProcessIdle (); */
NS_IMETHODIMP
totemScriptablePlugin::ProcessIdle(PRBool *_retval)
{
  WARN_UNIMPLEMENTED ();

  *_retval = PR_TRUE;
  return NS_OK;
}
#endif

/* boolean GetShowAbout (); */
NS_IMETHODIMP
totemScriptablePlugin::GetShowAbout(PRBool *_retval)
{
  SHOW_CALL ();
  
  *_retval = mShowAbout;
  return NS_OK;
}

/* boolean SetShowAbout (in boolean enabled); */
NS_IMETHODIMP
totemScriptablePlugin::SetShowAbout (PRBool enabled,
				     PRBool *_retval)
{
  SHOW_CALL ();
  
  /* Web page has no business doing this, but remember the state */
  mShowAbout = enabled != PR_FALSE;
  *_retval = PR_TRUE;
  return NS_OK;
}

/* boolean GetShowPreferences (); */
NS_IMETHODIMP
totemScriptablePlugin::GetShowPreferences(PRBool *_retval)
{
  SHOW_CALL ();
  
  *_retval = mShowPrefs;
  return NS_OK;
}

/* boolean SetShowPreferences (in boolean enabled); */
NS_IMETHODIMP
totemScriptablePlugin::SetShowPreferences (PRBool enabled,
					   PRBool *_retval)
{
  WARN_UNIMPLEMENTED ();

  /* Web page has no business doing this, but remember the state */
  mShowPrefs = enabled != PR_FALSE;
  *_retval = PR_TRUE;
  return NS_OK;
}

/* boolean GetShowStatistics (); */
NS_IMETHODIMP
totemScriptablePlugin::GetShowStatistics(PRBool *_retval)
{
  SHOW_CALL ();
  
  *_retval = mShowStats;
  return NS_OK;
}

/* boolean SetShowStatistics (in boolean enabled); */
NS_IMETHODIMP
totemScriptablePlugin::SetShowStatistics (PRBool enabled,
					  PRBool *_retval)
{
  WARN_UNIMPLEMENTED ();

  mShowStats = enabled != PR_FALSE;
  *_retval = PR_TRUE;
  return NS_OK;
}

/* boolean GetShuffle (); */
NS_IMETHODIMP
totemScriptablePlugin::GetShuffle(PRBool *_retval)
{
  SHOW_CALL ();
  
  *_retval = mShuffle;
  return NS_OK;
}

/* boolean SetShuffle (in boolean enabled); */
NS_IMETHODIMP
totemScriptablePlugin::SetShuffle (PRBool enabled,
				   PRBool *_retval)
{
  WARN_UNIMPLEMENTED ();

  mShuffle = enabled != PR_FALSE;
  *_retval = PR_TRUE;
  return NS_OK;
}

/* AUTF8String GetSource (); */
NS_IMETHODIMP
totemScriptablePlugin::GetSource(nsACString & _retval)
{
  SHOW_CALL ();
  
  _retval.Assign (mSource);
  return NS_OK;
}

/* boolean SetSource (in AUTF8String url); */
NS_IMETHODIMP
totemScriptablePlugin::SetSource(const nsACString & source, PRBool *_retval)
{
  WARN_UNIMPLEMENTED ();

  mSource = source;
  *_retval = PR_TRUE;
  return NS_OK;
}

/* AUTF8String GetSourceTransport (in long index); */
NS_IMETHODIMP
totemScriptablePlugin::GetSourceTransport (PRInt32 index,
					   nsACString & _retval)
{
  SHOW_CALL ();
  
  /* Unimplemented in helix too */
  _retval.Assign (mSource);
  return NS_OK;
}

#if 0
/* boolean StatusScan (in AUTF8String statusString); */
NS_IMETHODIMP
totemScriptablePlugin::StatusScan (const nsACString & statusString,
				   PRBool *_retval)
{
  SHOW_CALL ();
  
  /* Unimplemented in helix too */
  *_retval = PR_TRUE;
  return NS_OK;
}

/* boolean StatusScanEnd (); */
NS_IMETHODIMP
totemScriptablePlugin::StatusScanEnd(PRBool *_retval)
{
  SHOW_CALL ();
  
  /* Unimplemented in helix too */
  *_retval = PR_TRUE;
  return NS_OK;
}

/* boolean StatusScanStart (); */
NS_IMETHODIMP
totemScriptablePlugin::StatusScanStart(PRBool *_retval)
{
  SHOW_CALL ();
  
  /* Unimplemented in helix too */
  *_retval = PR_TRUE;
  return NS_OK;
}
#endif

/* long GetStereoState (); */
NS_IMETHODIMP
totemScriptablePlugin::GetStereoState(PRInt32 *_retval)
{
  WARN_UNIMPLEMENTED ();

  *_retval = 0;
  return NS_OK;
}

/* AUTF8String GetTitle (); */
NS_IMETHODIMP
totemScriptablePlugin::GetTitle(nsACString & _retval)
{
  SHOW_CALL ();
  
  _retval.Assign (mTitle);
  return NS_OK;
}

/* boolean SetTitle (in AUTF8String title); */
NS_IMETHODIMP
totemScriptablePlugin::SetTitle(const nsACString & title, PRBool *_retval)
{
  WARN_UNIMPLEMENTED ();

  mTitle = title;
  *_retval = PR_TRUE;
  return NS_OK;
}

/* long GetUserCountryID (); */
NS_IMETHODIMP
totemScriptablePlugin::GetUserCountryID(PRInt32 *_retval)
{
  WARN_UNIMPLEMENTED ();

  *_retval = 0;
  return NS_OK;
}

/* AUTF8String GetVersionInfo (); */
NS_IMETHODIMP
totemScriptablePlugin::GetVersionInfo(nsACString & _retval)
{
  WARN_UNIMPLEMENTED ();
  _retval.Assign ("1.0");
  return NS_OK;
}

#if 0
/* boolean SetVideoState (in long state); */
NS_IMETHODIMP
totemScriptablePlugin::SetVideoState (PRInt32 state,
				      PRBool *_retval)
{
  WARN_UNIMPLEMENTED ();

  *_retval = PR_TRUE;
  return NS_OK;
}

/* long GetVideoState (); */
NS_IMETHODIMP
totemScriptablePlugin::GetVideoState (PRInt32 *_retval)
{
  WARN_UNIMPLEMENTED ();

  *_retval = 0;
  return NS_OK;
}
#endif

/* void SetVolume (in long volume); */
NS_IMETHODIMP
totemScriptablePlugin::SetVolume (PRInt32 volume)
{
  WARN_UNIMPLEMENTED ();

  mVolume = volume;
  return NS_OK;
}

/* long GetVolume (); */
NS_IMETHODIMP
totemScriptablePlugin::GetVolume(PRInt32 *_retval)
{
  SHOW_CALL ();
  
  *_retval = mVolume;
  return NS_OK;
}

/* boolean GetWantErrors (); */
NS_IMETHODIMP
totemScriptablePlugin::GetWantErrors(PRBool *_retval)
{
  SHOW_CALL ();
  
  *_retval = mWantErrors;
  return NS_OK;
}

/* boolean SetWantErrors (in boolean enabled); */
NS_IMETHODIMP
totemScriptablePlugin::SetWantErrors (PRBool enabled,
				      PRBool *_retval)
{
  WARN_UNIMPLEMENTED ();

  mWantErrors = enabled != PR_FALSE;
  *_retval = PR_TRUE;
  return NS_OK;
}

/* boolean GetWantKeyboardEvents (); */
NS_IMETHODIMP
totemScriptablePlugin::GetWantKeyboardEvents(PRBool *_retval)
{
  SHOW_CALL ();
  
  *_retval = mWantKeyEvents;
  return NS_OK;
}

/* boolean SetWantKeyboardEvents (in boolean enabled); */
NS_IMETHODIMP
totemScriptablePlugin::SetWantKeyboardEvents (PRBool enabled,
					      PRBool *_retval)
{
  WARN_UNIMPLEMENTED ();

  mWantKeyEvents = enabled != PR_FALSE;
  *_retval = PR_TRUE;
  return NS_OK;
}

/* boolean GetWantMouseEvents (); */
NS_IMETHODIMP
totemScriptablePlugin::GetWantMouseEvents(PRBool *_retval)
{
  SHOW_CALL ();
  
  *_retval = mWantMouseEvents;
  return NS_OK;
}

/* boolean SetWantMouseEvents (in boolean enabled); */
NS_IMETHODIMP
totemScriptablePlugin::SetWantMouseEvents (PRBool enabled,
					   PRBool *_retval)
{
  WARN_UNIMPLEMENTED ();

  mWantMouseEvents = enabled != PR_FALSE;
  *_retval = PR_TRUE;
  return NS_OK;
}

#if 0
/* boolean SetZoomed (in boolean zoomed); */
NS_IMETHODIMP
totemScriptablePlugin::SetZoomed (PRBool zoomed,
				  PRBool *_retval)
{
  WARN_UNIMPLEMENTED ();

  mZoomed = zoomed != PR_FALSE;
  *_retval = PR_TRUE;
  return NS_OK;
}
#endif
