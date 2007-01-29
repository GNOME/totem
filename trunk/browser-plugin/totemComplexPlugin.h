/* Totem Complex Plugin scriptable
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

#ifndef __GMP_PLAYER_H__
#define __GMP_PLAYER_H__

#include <nsIClassInfo.h>

#include "totemIComplexPlayer.h"
#include "totemPlugin.h"

class totemScriptablePlugin : public totemIComplexPlayer,
			      public nsIClassInfo
{
  public:
    NS_DECL_ISUPPORTS
    NS_DECL_TOTEMICOMPLEXPLAYER
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

    PRInt32 mNumLoops;
    PRInt32 mVolume;

    nsCString mAuthor;
    nsCString mBackgroundColour;
    nsCString mCopywrong;
    nsCString mSource;
    nsCString mTitle;

    enum PlayState {
      eState_Stopped,
      eState_Contacting,
      eState_Buffering,
      eState_Playing,
      eState_Paused,
      eState_Seeking
    };

    enum ErrorSeverity {
      eErrorSeverity_Panic,
      eErrorSeverity_Severe,
      eErrorSeverity_Critical,
      eErrorSeverity_General,
      eErrorSeverity_Warning,
      eErrorSeverity_Notice,
      eErrorSeverity_Informational,
      eErrorSeverity_Debug
    };

    PRUint32 mPlayState : 3; /* PlayState enum values have to fit */

    PRUint32 mAutoGoToURL : 1;
    PRUint32 mAutoStart : 1;
    PRUint32 mCentred : 1;
    PRUint32 mConsoleEvents : 1;
    PRUint32 mContextMenu : 1;
    PRUint32 mDoubleSize : 1;
    PRUint32 mFullscreen : 1;
    PRUint32 mMessageBox : 1;
    PRUint32 mOriginalSize : 1;
    PRUint32 mImageStatus : 1;
    PRUint32 mLoop : 1;
    PRUint32 mMaintainAspect : 1;
    PRUint32 mMute : 1;
    PRUint32 mNoLabels : 1;
    PRUint32 mNoLogo : 1;
    PRUint32 mPrefetch : 1;
    PRUint32 mShowAbout : 1;
    PRUint32 mShowPrefs : 1;
    PRUint32 mShowStats : 1;
    PRUint32 mShuffle : 1;
    PRUint32 mWantErrors : 1;
    PRUint32 mWantKeyEvents : 1;
    PRUint32 mWantMouseEvents : 1;
    PRUint32 mZoomed : 1;
};

#endif /* __GMP_PLAYER_H__ */
