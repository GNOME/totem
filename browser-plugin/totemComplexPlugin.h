/* Totem Complex plugin
 *
 * Copyright © 2004 Bastien Nocera <hadess@hadess.net>
 * Copyright © 2002 David A. Schleef <ds@schleef.org>
 * Copyright © 2006, 2007, 2008 Christian Persch
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
 */

#ifndef __TOTEM_COMPLEX_PLAYER_H__
#define __TOTEM_COMPLEX_PLAYER_H__

#include "totemNPClass.h"
#include "totemNPObject.h"

class totemComplexPlugin : public totemNPObject
{
  public:
    totemComplexPlugin (NPP);
    virtual ~totemComplexPlugin ();

  private:
    enum Methods {
      eCanPause,
      eCanPlay,
      eCanStop,
      eDoGotoURL,
      eDoNextEntry,
      eDoPause,
      eDoPlay,
      eDoPrevEntry,
      eDoStop,
      eGetAuthor,
      eGetAutoGoToURL,
      eGetAutoStart,
      eGetBackgroundColor,
      eGetBandwidthAverage,
      eGetBandwidthCurrent,
      eGetBufferingTimeElapsed,
      eGetBufferingTimeRemaining,
      eGetCanSeek,
      eGetCenter,
      eGetClipHeight,
      eGetClipWidth,
      eGetConnectionBandwidth,
      eGetConsole,
      eGetConsoleEvents,
      eGetControls,
      eGetCopyright,
      eGetCurrentEntry,
      eGetDoubleSize,
      eGetDRMInfo,
      eGetEnableContextMenu,
      eGetEnableDoubleSize,
      eGetEnableFullScreen,
      eGetEnableOriginalSize,
      eGetEntryAbstract,
      eGetEntryAuthor,
      eGetEntryCopyright,
      eGetEntryTitle,
      eGetFullScreen,
      eGetImageStatus,
      eGetIsPlus,
      eGetLastErrorMoreInfoURL,
      eGetLastErrorRMACode,
      eGetLastErrorSeverity,
      eGetLastErrorUserCode,
      eGetLastErrorUserString,
      eGetLastMessage,
      eGetLastStatus,
      eGetLength,
      eGetLiveState,
      eGetLoop,
      eGetMaintainAspect,
      eGetMute,
      eGetNoLogo,
      eGetNumEntries,
      eGetNumLoop,
      eGetNumSources,
      eGetOriginalSize,
      eGetPacketsEarly,
      eGetPacketsLate,
      eGetPacketsMissing,
      eGetPacketsOutOfOrder,
      eGetPacketsReceived,
      eGetPacketsTotal,
      eGetPlayState,
      eGetPosition,
      eGetPreferedLanguageID,
      eGetPreferedLanguageString,
      eGetPreferredLanguageID,
      eGetPreferredLanguageString,
      eGetPreFetch,
      eGetShowAbout,
      eGetShowPreferences,
      eGetShowStatistics,
      eGetShuffle,
      eGetSource,
      eGetSourceTransport,
      eGetStereoState,
      eGetTitle,
      eGetUserCountryID,
      eGetVersionInfo,
      eGetVolume,
      eGetWantErrors,
      eGetWantKeyboardEvents,
      eGetWantMouseEvents,
      eHasNextEntry,
      eHasPrevEntry,
      eSetAuthor,
      eSetAutoGoToURL,
      eSetAutoStart,
      eSetBackgroundColor,
      eSetCanSeek,
      eSetCenter,
      eSetConsole,
      eSetConsoleEvents,
      eSetControls,
      eSetCopyright,
      eSetDoubleSize,
      eSetEnableContextMenu,
      eSetEnableDoubleSize,
      eSetEnableFullScreen,
      eSetEnableOriginalSize,
      eSetFullScreen,
      eSetImageStatus,
      eSetLoop,
      eSetMaintainAspect,
      eSetMute,
      eSetNoLogo,
      eSetNumLoop,
      eSetOriginalSize,
      eSetPosition,
      eSetPreFetch,
      eSetShowAbout,
      eSetShowPreferences,
      eSetShowStatistics,
      eSetShuffle,
      eSetSource,
      eSetTitle,
      eSetVolume,
      eSetWantErrors,
      eSetWantKeyboardEvents,
      eSetWantMouseEvents
    };

    virtual bool InvokeByIndex (int aIndex, const NPVariant *argv, uint32_t argc, NPVariant *_result);
};

TOTEM_DEFINE_NPCLASS (totemComplexPlugin);

#endif /* __TOTEM_COMPLEX_PLAYER_H__ */
