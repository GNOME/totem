/* Totem Mozilla plugin
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
 */

#ifndef __TOTEM_PLUGIN_H__
#define __TOTEM_PLUGIN_H__

#include <stdint.h>
#include <dbus/dbus-glib.h>
#include "npapi.h"

class totemScriptablePlugin;

class totemPlugin {
  public:
    totemPlugin (NPP aInstance);
    ~totemPlugin ();
  
    void* operator new (size_t aSize) CPP_THROW_NEW;

    nsresult Play ();
    nsresult Stop ();
    nsresult Pause ();

    NPError Init (NPMIMEType mimetype,
                  uint16_t mode,
                  int16_t argc,
                  char *argn[],
                  char *argv[],
                  NPSavedData *saved);
  
    NPError SetWindow (NPWindow *aWindow);
  
    NPError NewStream (NPMIMEType type,
                      NPStream* stream_ptr,
                      NPBool seekable,
                      uint16* stype);
    NPError DestroyStream (NPStream* stream,
                          NPError reason);
  
    int32 WriteReady (NPStream *stream);
    int32 Write (NPStream *stream,
                int32 offset,
                int32 len,
                void *buffer);
    void StreamAsFile (NPStream *stream,
                      const char* fname);
  
    NPError GetScriptable (void *_retval);

    static void PR_CALLBACK NameOwnerChangedCallback (DBusGProxy *proxy,
						      const char *svc,
						      const char *old_owner,
						      const char *new_owner,
						      totemPlugin *plugin);

    static void PR_CALLBACK StopSendingDataCallback (DBusGProxy  *proxy,
						     totemPlugin *plugin);

  private:

    PRBool Fork ();
    void UnsetStream ();

    PRBool IsMimeTypeSupported (const char *aMimeType, const char *aURL);
    PRBool IsSchemeSupported (const char *aURL);
    char *GetRealMimeType (const char *aMimeType);

    NPP mInstance;

    /* FIXME make this a nsCOMPtr */
    totemScriptablePlugin *mScriptable;

    NPStream *mStream;
    char *mMimeType;
    PRUint8 mStreamType;

    Window mWindow;

    char *mSrc;
    char *mLocal;
    char *mHref;
    char *mTarget;

    int mWidth;
    int mHeight;

    DBusGConnection *mConn;
    DBusGProxy *mProxy;
    char *mWaitForSvc;

    int mSendFD;
    int mPlayerPID;

    PRUint32 mCache : 1;
    PRUint32 mControllerHidden : 1;
    PRUint32 mGotSvc : 1;
    PRUint32 mHidden : 1;
    PRUint32 mIsPlaylist : 1;
    PRUint32 mIsSupportedSrc : 1;
    PRUint32 mNoAutostart : 1;
    PRUint32 mRepeat : 1;
    PRUint32 mTriedWrite : 1;
    PRUint32 mStatusbar : 1;
};

typedef struct {
  const char *mimetype;
  const char *extensions;
  const char *mime_alias;
  gboolean    ignore;
} totemPluginMimeEntry;

#endif /* __TOTEM_PLUGIN_H__ */
