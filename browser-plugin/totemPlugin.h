/* Totem browser plugin
 *
 * Copyright © 2004 Bastien Nocera <hadess@hadess.net>
 * Copyright © 2002 David A. Schleef <ds@schleef.org>
 * Copyright © 2006 Christian Persch
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
#include <npapi.h>

#include <nsStringAPI.h>

#ifdef NEED_STRING_GLUE
#include "totemStringGlue.h"
#endif

#if defined(TOTEM_COMPLEX_PLUGIN) && defined(HAVE_NSTARRAY_H)
#include <nsTArray.h>
#endif /* TOTEM_COMPLEX_PLUGIN */

#include "totem-plugin-viewer-commands.h"

class nsIDOMDocument;
class nsIDOMElement;
class nsIIOService;
class nsIServiceManager;
class nsITimer;
class nsIURI;
class nsVoidArray;
class totemScriptablePlugin;
struct _NPNetscapeFuncs;

class totemPlugin {
  public:
    totemPlugin (NPP aInstance);
    ~totemPlugin ();
  
    void* operator new (size_t aSize) CPP_THROW_NEW;

    /* Interface to scriptable */

    nsresult DoCommand (const char *aCommand);
    nsresult SetVolume (gdouble aVolume);

    nsresult SetSrc (const nsACString &aURL);

    /* plugin glue */
    static _NPNetscapeFuncs sNPN;

    static NPError Initialise ();
    static NPError Shutdown ();

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

    void URLNotify (const char *url,
		    NPReason reason,
		    void *notifyData);

    NPError GetScriptable (void *_retval);

  private:

    static void PR_CALLBACK NameOwnerChangedCallback (DBusGProxy *proxy,
						      const char *svc,
						      const char *old_owner,
						      const char *new_owner,
						      void *aData);

    static void PR_CALLBACK ViewerForkTimeoutCallback (nsITimer *aTimer,
						       void *aData);

    static void PR_CALLBACK ButtonPressCallback (DBusGProxy  *proxy,
						 guint aTimestamp,
		    				 guint aButton,
					         void *aData);

    static void PR_CALLBACK StopStreamCallback (DBusGProxy  *proxy,
						void *aData);

    static void PR_CALLBACK ViewerSetWindowCallback (DBusGProxy *aProxy,
						     DBusGProxyCall *aCall,
						     void *aData);
    static void PR_CALLBACK ViewerOpenStreamCallback (DBusGProxy *aProxy,
						      DBusGProxyCall *aCall,
						      void *aData);
    static void PR_CALLBACK ViewerOpenURICallback (DBusGProxy *aProxy,
						   DBusGProxyCall *aCall,
						   void *aData);

    NPError ViewerFork ();
    void ViewerSetup ();
    void ViewerSetWindow ();
    void ViewerReady ();
    void ViewerCleanup ();

    void ViewerButtonPressed (guint aTimestamp,
		    	      guint aButton);
    void NameOwnerChanged (const char *aName,
			   const char *aOldOwner,
			   const char *aNewOwner);

    void ComputeRequest ();
    void ClearRequest ();
    void RequestStream (PRBool aForceViewer);
    void UnsetStream ();

    PRBool IsMimeTypeSupported (const char *aMimeType,
				const char *aURL);
    PRBool IsSchemeSupported (nsIURI *aURI);
    void GetRealMimeType (const char *aMimeType,
			  nsACString &_retval);
    PRBool ParseBoolean (const char *key,
			 const char *value,
			 PRBool default_val);
    PRBool GetBooleanValue (GHashTable *args,
			    const char *key,
			    PRBool default_val);
    PRUint32 GetEnumIndex (GHashTable *args,
			   const char *key,
			   const char *values[],
			   PRUint32 n_values,
			   PRUint32 default_value);

    NPP mInstance;

    /* FIXME make these use nsCOMPtr<> !! */
    totemScriptablePlugin *mScriptable;
    nsIServiceManager *mServiceManager;
    nsIIOService *mIOService;
    nsIDOMElement *mPluginDOMElement;
    nsITimer *mTimer;
    nsIURI *mBaseURI;

    nsIURI *mRequestBaseURI;
    nsIURI *mRequestURI;

    NPStream *mStream;
    PRUint32 mBytesStreamed;
    PRUint8 mStreamType;

    nsCString mMimeType;

    nsCString mSrc;
    nsIURI *mSrcURI;

    Window mWindow;
    PRInt32 mWidth;
    PRInt32 mHeight;

    DBusGConnection *mBusConnection;
    DBusGProxy *mBusProxy;
    DBusGProxy *mViewerProxy;
    DBusGProxyCall *mViewerPendingCall;
    nsCString mViewerBusAddress;
    nsCString mViewerServiceName;
    int mViewerPID;
    int mViewerFD;

#ifdef TOTEM_GMP_PLUGIN
  public:
    nsresult SetURL (const nsACString &aURL);

  private:
    nsIURI *mURLURI;
#endif

#ifdef TOTEM_NARROWSPACE_PLUGIN
  public:
    nsresult SetQtsrc (const nsCString &aURL);
    nsresult SetHref (const nsCString& aURL);

  private:
    PRBool ParseURLExtensions (const nsACString &aString,
			       nsACString &_url,
			       nsACString &_target);

    void LaunchTotem (const nsCString &aURL,
		      PRUint32 aTimestamp);

    nsIURI *mQtsrcURI;

    nsCString mHref;
    nsIURI *mHrefURI;

    nsCString mTarget;
#endif

#if defined(TOTEM_COMPLEX_PLUGIN) && defined(HAVE_NSTARRAY_H)
  public:

    nsresult SetConsole (const nsACString &aConsole);

  private:

    totemPlugin* FindConsoleClassRepresentant ();
    void TransferConsole ();

    void UnownedViewerSetup ();
    void UnownedViewerSetWindow ();
    void UnownedViewerUnsetWindow ();

    nsIDOMDocument *mPluginOwnerDocument;
    nsCString mConsole;
    nsCString mControls;

    /* nsnull if we're the representant ourself */
    totemPlugin *mConsoleClassRepresentant;

    static nsTArray<totemPlugin*> *sPlugins;

#endif /* TOTEM_COMPLEX_PLUGIN */

  private:

    PRUint32 mAutostart : 1;
    PRUint32 mAutoPlay : 1;
    PRUint32 mCache : 1;
    PRUint32 mCheckedForPlaylist : 1;
    PRUint32 mControllerHidden : 1;
    PRUint32 mExpectingStream : 1;
    PRUint32 mHadStream : 1;
    PRUint32 mHidden : 1;
    PRUint32 mIsPlaylist : 1;
    PRUint32 mIsSupportedSrc : 1;
    PRUint32 mNeedViewer : 1;
    PRUint32 mRepeat : 1;
    PRUint32 mRequestIsSrc : 1;
    PRUint32 mShowStatusbar : 1;
    PRUint32 mTimerRunning : 1;
    PRUint32 mUnownedViewerSetUp : 1;
    PRUint32 mViewerReady : 1;
    PRUint32 mViewerSetUp : 1;
    PRUint32 mWaitingForButtonPress : 1;
    PRUint32 mWindowSet : 1;
    PRUint32 mAudioOnly : 1;
};

typedef struct {
  const char *mimetype;
  const char *extensions;
  const char *mime_alias;
} totemPluginMimeEntry;

#endif /* __TOTEM_PLUGIN_H__ */
