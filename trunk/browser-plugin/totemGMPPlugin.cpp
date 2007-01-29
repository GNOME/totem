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

#include "totemIGMPPlayer.h"
#include "totemIGMPControls.h"

#include "totemClassInfo.h"

#include "totemGMPPlugin.h"

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

/* readonly attribute totemIGMPControls controls; */
NS_IMETHODIMP
totemScriptablePlugin::GetControls(totemIGMPControls * *aControls)
{
  return CallQueryInterface (this, aControls);
}

/* totemIGMPControls */

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

NS_IMETHODIMP
totemScriptablePlugin::Stop ()
{
  NS_ENSURE_STATE (IsValid ());

  return mPlugin->DoCommand (TOTEM_COMMAND_STOP);
}
