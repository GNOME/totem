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

#include "totemClassInfo.h"

#include "totemComplexPlugin.h"

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

NS_IMETHODIMP
totemScriptablePlugin::DoPause (PRBool *_retval)
{
  NS_ENSURE_STATE (IsValid ());

  nsresult rv = mPlugin->DoCommand (TOTEM_COMMAND_PAUSE);

  *_retval = PR_TRUE;
  return rv;
}

NS_IMETHODIMP
totemScriptablePlugin::DoPlay (PRBool *_retval)
{
  NS_ENSURE_STATE (IsValid ());

  nsresult rv = mPlugin->DoCommand (TOTEM_COMMAND_PLAY);

  *_retval = PR_TRUE;
  return rv;
}

NS_IMETHODIMP
totemScriptablePlugin::DoStop (PRBool *_retval)
{
  NS_ENSURE_STATE (IsValid ());

  nsresult rv = mPlugin->DoCommand (TOTEM_COMMAND_STOP);

  *_retval = PR_TRUE;
  return rv;
}
