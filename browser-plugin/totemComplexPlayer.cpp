/* Totem Complex plugin
 *
 * Copyright © 2004 Bastien Nocera <hadess@hadess.net>
 * Copyright © 2002 David A. Schleef <ds@schleef.org>
 * Copyright © 2006, 2008 Christian Persch
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

#include <config.h>

#include <string.h>

#include <glib.h>

#include "npupp.h"

#include "totemPlugin.h"
#include "totemComplexPlayer.h"


static const char *methodNames[] = {
  "CanPause",
  "CanPlay",
  "CanStop",
  "DoGotoURL",
  "DoNextEntry",
  "DoPause",
  "DoPlay",
  "DoPrevEntry",
  "DoStop",
  "GetAuthor",
  "GetAutoGoToURL",
  "GetAutoStart",
  "GetBackgroundColor",
  "GetBandwidthAverage",
  "GetBandwidthCurrent",
  "GetBufferingTimeElapsed",
  "GetBufferingTimeRemaining",
  "GetCanSeek",
  "GetCenter",
  "GetClipHeight",
  "GetClipWidth",
  "GetConnectionBandwidth",
  "GetConsole",
  "GetConsoleEvents",
  "GetControls",
  "GetCopyright",
  "GetCurrentEntry",
  "GetDoubleSize",
  "GetDRMInfo",
  "GetEnableContextMenu",
  "GetEnableDoubleSize",
  "GetEnableFullScreen",
  "GetEnableOriginalSize",
  "GetEntryAbstract",
  "GetEntryAuthor",
  "GetEntryCopyright",
  "GetEntryTitle",
  "GetFullScreen",
  "GetImageStatus",
  "GetIsPlus",
  "GetLastErrorMoreInfoURL",
  "GetLastErrorRMACode",
  "GetLastErrorSeverity",
  "GetLastErrorUserCode",
  "GetLastErrorUserString",
  "GetLastMessage",
  "GetLastStatus",
  "GetLength",
  "GetLiveState",
  "GetLoop",
  "GetMaintainAspect",
  "GetMute",
  "GetNoLogo",
  "GetNumEntries",
  "GetNumLoop",
  "GetNumSources",
  "GetOriginalSize",
  "GetPacketsEarly",
  "GetPacketsLate",
  "GetPacketsMissing",
  "GetPacketsOutOfOrder",
  "GetPacketsReceived",
  "GetPacketsTotal",
  "GetPlayState",
  "GetPosition",
  "GetPreferedLanguageID",
  "GetPreferedLanguageString",
  "GetPreferredLanguageID",
  "GetPreferredLanguageString",
  "GetPreFetch",
  "GetShowAbout",
  "GetShowPreferences",
  "GetShowStatistics",
  "GetShuffle",
  "GetSource",
  "GetSourceTransport",
  "GetStereoState",
  "GetTitle",
  "GetUserCountryID",
  "GetVersionInfo",
  "GetVolume",
  "GetWantErrors",
  "GetWantKeyboardEvents",
  "GetWantMouseEvents",
  "HasNextEntry",
  "HasPrevEntry",
  "SetAuthor",
  "SetAutoGoToURL",
  "SetAutoStart",
  "SetBackgroundColor",
  "SetCanSeek",
  "SetCenter",
  "SetConsole",
  "SetConsoleEvents",
  "SetControls",
  "SetCopyright",
  "SetDoubleSize",
  "SetEnableContextMenu",
  "SetEnableDoubleSize",
  "SetEnableFullScreen",
  "SetEnableOriginalSize",
  "SetFullScreen",
  "SetImageStatus",
  "SetLoop",
  "SetMaintainAspect",
  "SetMute",
  "SetNoLogo",
  "SetNumLoop",
  "SetOriginalSize",
  "SetPosition",
  "SetPreFetch",
  "SetShowAbout",
  "SetShowPreferences",
  "SetShowStatistics",
  "SetShuffle",
  "SetSource",
  "SetTitle",
  "SetVolume",
  "SetWantErrors",
  "SetWantKeyboardEvents",
  "SetWantMouseEvents"
};

TOTEM_IMPLEMENT_NPCLASS (totemComplexPlayer,
                         propertyNames, G_N_ELEMENTS (propertyNames),
                         methodNames, G_N_ELEMENTS (methodNames),
                         NULL);

totemComplexPlayer::totemComplexPlayer (NPP aNPP)
  : totemNPObject (aNPP)
{
  TOTEM_LOG_CTOR ();
}

totemComplexPlayer::~totemComplexPlayer ()
{
  TOTEM_LOG_DTOR ();
}

bool
totemComplexPlayer::InvokeByIndex (int aIndex,
                               const NPVariant *argv,
                               uint32_t argc,
                               NPVariant *_result)
{
  TOTEM_LOG_INVOKE (aIndex, totemComplexPlayer);

  switch (Methods (aIndex)) {
    case eCanPause:
    case eCanPlay:
    case eCanStop:
    case eDoGotoURL:
    case eDoNextEntry:
    case eDoPause:
    case eDoPlay:
    case eDoPrevEntry:
    case eDoStop:
    case eGetAuthor:
    case eGetAutoGoToURL:
    case eGetAutoStart:
    case eGetBackgroundColor:
    case eGetBandwidthAverage:
    case eGetBandwidthCurrent:
    case eGetBufferingTimeElapsed:
    case eGetBufferingTimeRemaining:
    case eGetCanSeek:
    case eGetCenter:
    case eGetClipHeight:
    case eGetClipWidth:
    case eGetConnectionBandwidth:
    case eGetConsole:
    case eGetConsoleEvents:
    case eGetControls:
    case eGetCopyright:
    case eGetCurrentEntry:
    case eGetDoubleSize:
    case eGetDRMInfo:
    case eGetEnableContextMenu:
    case eGetEnableDoubleSize:
    case eGetEnableFullScreen:
    case eGetEnableOriginalSize:
    case eGetEntryAbstract:
    case eGetEntryAuthor:
    case eGetEntryCopyright:
    case eGetEntryTitle:
    case eGetFullScreen:
    case eGetImageStatus:
    case eGetIsPlus:
    case eGetLastErrorMoreInfoURL:
    case eGetLastErrorRMACode:
    case eGetLastErrorSeverity:
    case eGetLastErrorUserCode:
    case eGetLastErrorUserString:
    case eGetLastMessage:
    case eGetLastStatus:
    case eGetLength:
    case eGetLiveState:
    case eGetLoop:
    case eGetMaintainAspect:
    case eGetMute:
    case eGetNoLogo:
    case eGetNumEntries:
    case eGetNumLoop:
    case eGetNumSources:
    case eGetOriginalSize:
    case eGetPacketsEarly:
    case eGetPacketsLate:
    case eGetPacketsMissing:
    case eGetPacketsOutOfOrder:
    case eGetPacketsReceived:
    case eGetPacketsTotal:
    case eGetPlayState:
    case eGetPosition:
    case eGetPreferedLanguageID:
    case eGetPreferedLanguageString:
    case eGetPreferredLanguageID:
    case eGetPreferredLanguageString:
    case eGetPreFetch:
    case eGetShowAbout:
    case eGetShowPreferences:
    case eGetShowStatistics:
    case eGetShuffle:
    case eGetSource:
    case eGetSourceTransport:
    case eGetStereoState:
    case eGetTitle:
    case eGetUserCountryID:
    case eGetVersionInfo:
    case eGetVolume:
    case eGetWantErrors:
    case eGetWantKeyboardEvents:
    case eGetWantMouseEvents:
    case eHasNextEntry:
    case eHasPrevEntry:
    case eSetAuthor:
    case eSetAutoGoToURL:
    case eSetAutoStart:
    case eSetBackgroundColor:
    case eSetCanSeek:
    case eSetCenter:
    case eSetConsole:
    case eSetConsoleEvents:
    case eSetControls:
    case eSetCopyright:
    case eSetDoubleSize:
    case eSetEnableContextMenu:
    case eSetEnableDoubleSize:
    case eSetEnableFullScreen:
    case eSetEnableOriginalSize:
    case eSetFullScreen:
    case eSetImageStatus:
    case eSetLoop:
    case eSetMaintainAspect:
    case eSetMute:
    case eSetNoLogo:
    case eSetNumLoop:
    case eSetOriginalSize:
    case eSetPosition:
    case eSetPreFetch:
    case eSetShowAbout:
    case eSetShowPreferences:
    case eSetShowStatistics:
    case eSetShuffle:
    case eSetSource:
    case eSetTitle:
    case eSetVolume:
    case eSetWantErrors:
    case eSetWantKeyboardEvents:
    case eSetWantMouseEvents:
      TOTEM_WARN_INVOKE_UNIMPLEMENTED (aIndex, totemComplexPlayer);
      return BoolVariant (_result, true);
  }

  return false;
}
