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

#include "totemIMullYPlayer.h"

#include "totemClassInfo.h"

#include "totemMullYPlugin.h"

/* 67DABFBF-D0AB-41fa-9C46-CC0F21721616 */
static const nsCID kClassID = 
  { 0x67dabfbf, 0xd0ab, 0x41fa,
    { 0x9c, 0x46, 0xcc, 0x0f, 0x21, 0x72, 0x16, 0x16 } };

static const char kClassDescription[] = "totemMullYPlugin";
static const char kPluginDescription[] = "DivX\xC2\xAE Web Player";

static const totemPluginMimeEntry kMimeTypes[] = {
	{ "video/divx", "divx", "video/x-msvideo", FALSE },
	{ "video/x-msvideo", "avi", NULL, TRUE }
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

NS_IMETHODIMP
totemScriptablePlugin::Rewind ()
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
