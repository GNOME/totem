/* Totem NarrowSpace plugin scriptable
 *
 * Copyright (C) 2004 Bastien Nocera <hadess@hadess.net>
 * Copyright (C) 2002 David A. Schleef <ds@schleef.org>
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

#ifndef __NARROWSPACE_PLAYER_H__
#define __NARROWSPACE_PLAYER_H__

#include <nsIClassInfo.h>

#include "totemINarrowSpacePlayer.h"
#include "totemPlugin.h"

class totemScriptablePlugin : public totemINarrowSpacePlayer,
			      public nsIClassInfo
{
  public:
    NS_DECL_ISUPPORTS
    NS_DECL_TOTEMINARROWSPACEPLAYER
    NS_DECL_NSICLASSINFO

    void* operator new (size_t aSize) CPP_THROW_NEW;

    totemScriptablePlugin (totemPlugin *aPlugin);

    PRBool IsValid () { return mPlugin != nsnull; }
    void SetPlugin (totemPlugin *aPlugin) { mPlugin = aPlugin; }

    static char *PluginDescription ();
    static void PluginMimeTypes (const totemPluginMimeEntry **, PRUint32 *);
  private:
    ~totemScriptablePlugin ();

    totemPlugin *mPlugin;

    enum PluginState {
      eState_Complete,
      eState_Error,
      eState_Loading,
      eState_Playable,
      eState_Waiting
    };

    nsCString mBackgroundColour;
    nsCString mMatrix;
    nsCString mRectangle;
    nsCString mMovieName;

    float mRate;

    PRInt32 mVolume;

    PRUint32 mPluginState : 3; /* enough bits for PluginState enum values */

    PRUint32 mAutoPlay : 1;
    PRUint32 mControllerVisible : 1;
    PRUint32 mIsLooping : 1;
    PRUint32 mKioskMode : 1;
    PRUint32 mLoopIsPalindrome : 1;
    PRUint32 mMute : 1;
    PRUint32 mPlayEveryFrame : 1;
    PRUint32 mResetPropertiesOnReload : 1;
};

#endif /* __NARROWSPACE_PLAYER_H__ */
