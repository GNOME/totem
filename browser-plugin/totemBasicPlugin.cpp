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

#include <nsISupportsImpl.h>
#include <nsMemory.h>
#include <nsXPCOM.h>
#include <nsIProgrammingLanguage.h>

#define GNOME_ENABLE_DEBUG 1
/* define GNOME_ENABLE_DEBUG for more debug spew */
#include "debug.h"

#include "totemClassInfo.h"

#include "totemBasicPlugin.h"

/* 11ef8fce-9eb4-494e-804e-d56eae788625 */
static const nsCID kClassID = 
  { 0x11ef8fce, 0x9eb4, 0x494e,
    { 0x80, 0x4e, 0xd5, 0x6e, 0xae, 0x78, 0x86, 0x25 } };

static const char kClassDescription[] = "totemBasicPlugin";
static const char kPluginDescription[] = "Totem Web Browser Plugin " VERSION;

static const totemPluginMimeEntry kMimeTypes[] = {
	{ "application/x-ogg","ogg",NULL },
	{ "application/ogg", "ogg", NULL },
	{ "audio/ogg", "oga", NULL },
	{ "audio/x-ogg", "ogg", NULL },
	{ "video/ogg", "ogv", NULL },
	{ "video/x-ogg", "ogg", NULL },
	{ "application/annodex", "anx", NULL },
	{ "audio/annodex", "axa", NULL },
	{ "video/annodex", "axv", NULL },
	{ "video/mpeg", "mpg, mpeg, mpe", NULL },
	{ "audio/wav", "wav", NULL },
	{ "audio/x-wav", "wav", NULL },
	{ "audio/mpeg", "mp3", NULL },
	{ "application/x-nsv-vp3-mp3", "nsv", "video/x-nsv" },
	{ "video/flv", "flv", "application/x-flash-video" },
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
		    totemIBasicPlayer,
		    nsIClassInfo)

/* nsIClassInfo */

TOTEM_CLASSINFO_BEGIN (totemScriptablePlugin,
		       1,
		       kClassID,
		       kClassDescription)
  TOTEM_CLASSINFO_ENTRY (0, totemIBasicPlayer)
TOTEM_CLASSINFO_END

/* totemIBasicPlayer */

NS_IMETHODIMP
totemScriptablePlugin::Play ()
{
  NS_ENSURE_STATE (IsValid ());

  return mPlugin->DoCommand (TOTEM_COMMAND_PLAY);
}

NS_IMETHODIMP
totemScriptablePlugin::Rewind ()
{
  NS_ENSURE_STATE (IsValid ());

  return mPlugin->DoCommand (TOTEM_COMMAND_PAUSE);
}

NS_IMETHODIMP
totemScriptablePlugin::Stop ()
{
  NS_ENSURE_STATE (IsValid ());

  return mPlugin->DoCommand (TOTEM_COMMAND_STOP);
}
