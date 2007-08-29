/* Totem GMP plugin
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

#ifndef __GMP_PLUGIN_H__
#define __GMP_PLUGIN_H__

#include <nsIClassInfo.h>

#include "totemIGMPPlayer.h"
#include "totemIGMPControls.h"
#include "totemIGMPSettings.h"

#include "totemPlugin.h"

class totemGMPSettings;

class totemScriptablePlugin : public totemIGMPPlayer,
			      public totemIGMPControls,
			      public nsIClassInfo
{
  public:
    NS_DECL_ISUPPORTS
    NS_DECL_TOTEMIGMPPLAYER
    NS_DECL_TOTEMIGMPCONTROLS
    NS_DECL_NSICLASSINFO

    void* operator new (size_t aSize) CPP_THROW_NEW;

    totemScriptablePlugin (totemPlugin *aPlugin);

    PRBool IsValid () { return mPlugin != nsnull; }
    void SetPlugin (totemPlugin *aPlugin) { mPlugin = aPlugin; }

    static char *PluginDescription ();
    static void PluginMimeTypes (const totemPluginMimeEntry **, PRUint32 *);
  private:
    friend class totemGMPSettings;

    ~totemScriptablePlugin ();

    totemPlugin *mPlugin;
    totemGMPSettings *mSettingsTearOff;

    PRUint32 mWindowlessVideo;
};

#endif /* __GMP_PLUGIN_H__ */
