/* Totem Cone Plugin
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
 * $Id: totemBasicPlugin.cpp 4463 2007-07-27 16:07:42Z fcrozat $
 */

#include <mozilla-config.h>
#include "config.h"

#include <glib.h>

#include <nsISupportsImpl.h>
#include <nsMemory.h>
#include <nsXPCOM.h>
#include <nsIProgrammingLanguage.h>

#define GNOME_ENABLE_DEBUG 1
/* define GNOME_ENABLE_DEBUG for more debug spew */
#include "debug.h"

#include "totemDebug.h"
#include "totemClassInfo.h"

#include "totemConePlugin.h"

/* 11ef8fce-9eb4-494e-804e-d56eae788625 */
static const nsCID kClassID = 
  { 0x11ef8fce, 0x9eb4, 0x494e,
    { 0x80, 0x4e, 0xd5, 0x6e, 0xae, 0x78, 0x86, 0x25 } };

static const char kClassDescription[] = "totemConePlugin";
#define TOTEM_CONE_VERSION "0.8.6"
static const char kPluginDescription[] = "VLC Multimedia Plugin (compatible Totem " VERSION ")";

static const totemPluginMimeEntry kMimeTypes[] = {
	{ "application/x-vlc-plugin", "", "application/octet-stream" },
	{ "application/vlc", "", "application/octet-stream" },
	{ "video/x-google-vlc-plugin", "", "application/octet-stream" },
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

/* static */ void
totemScriptablePlugin::PluginMimeTypes (const totemPluginMimeEntry **_entries,
					PRUint32 *_count)
{
  *_entries = kMimeTypes;
  *_count = G_N_ELEMENTS (kMimeTypes);
}

/* Interface implementations */

NS_IMPL_ISUPPORTS2 (totemScriptablePlugin,
		    totemICone,
		    nsIClassInfo)

/* nsIClassInfo */

TOTEM_CLASSINFO_BEGIN (totemScriptablePlugin,
		       1,
		       kClassID,
		       kClassDescription)
  TOTEM_CLASSINFO_ENTRY (0, totemICone)
TOTEM_CLASSINFO_END

/* totemICone */

#define TOTEM_SCRIPTABLE_INTERFACE "totemICone"

/* readonly attribute AUTF8String VersionInfo; */
NS_IMETHODIMP
totemScriptablePlugin::GetVersionInfo(nsACString & aVersionInfo)
{
  aVersionInfo.Assign (TOTEM_CONE_VERSION);
  return NS_OK;
}

/* attribute totemIConeAudio audio; */
NS_IMETHODIMP
totemScriptablePlugin::GetAudio(totemIConeAudio * *aAudio)
{
  TOTEM_SCRIPTABLE_WARN_UNIMPLEMENTED();

  return NS_ERROR_NOT_IMPLEMENTED;
}

/* attribute totemIConeInput input; */
NS_IMETHODIMP
totemScriptablePlugin::GetInput(totemIConeInput * *aInput)
{
  TOTEM_SCRIPTABLE_WARN_UNIMPLEMENTED();

  return NS_ERROR_NOT_IMPLEMENTED;
}

/* attribute totemIConePlaylist playlist; */
NS_IMETHODIMP
totemScriptablePlugin::GetPlaylist(totemIConePlaylist * *aPlaylist)
{
  TOTEM_SCRIPTABLE_WARN_UNIMPLEMENTED();

  return NS_ERROR_NOT_IMPLEMENTED;
}

/* attribute totemIConeVideo video; */
NS_IMETHODIMP
totemScriptablePlugin::GetVideo(totemIConeVideo * *aVideo)
{
  TOTEM_SCRIPTABLE_WARN_UNIMPLEMENTED();

  return NS_ERROR_NOT_IMPLEMENTED;
}

