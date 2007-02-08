/* Totem Mozilla plugin
 * 
 * Copyright © 2004-2006 Bastien Nocera <hadess@hadess.net>
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

#include <mozilla-config.h>
#include "config.h"

#include <errno.h>
#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/poll.h>
#include <string.h>
#include <signal.h>

#include <glib.h>

#include <libgnomevfs/gnome-vfs-mime-utils.h>

#include "totem-pl-parser-mini.h"
#include "totem-plugin-viewer-options.h"
#include "totempluginviewer-marshal.h"

#include "npapi.h"
#include "npupp.h"

#include <nsIDOMWindow.h>
#include <nsIURI.h>
#include <nsEmbedString.h>
#include <nsIInterfaceRequestor.h>
#include <nsIInterfaceRequestorUtils.h>
#include <nsIWebNavigation.h>

#include <nsIServiceManager.h>
#include <nsIDOMDocument.h>
#include <nsIDOMElement.h>
#include <nsIDOM3Node.h>
#include <nsIIOService.h>
#include <nsIURI.h>
#include <nsITimer.h>
#include <nsIComponentManager.h>
#include <nsIServiceManager.h>
#include <nsIProtocolHandler.h>
#include <nsIExternalProtocolHandler.h>

/* for NS_IOSERVICE_CONTRACTID */
#include <nsNetCID.h>

#define GNOME_ENABLE_DEBUG 1
/* define GNOME_ENABLE_DEBUG for more debug spew */
/* FIXME define D() so that it prints the |this| pointer, so we can differentiate between different concurrent plugins! */
#include "debug.h"

// really noisy debug
#ifdef G_HAVE_ISO_VARARGS
#define DD(...)
#elif defined(G_HAVE_GNUC_VARARGS)
#define DD(args...)
#endif

#include "totemPluginGlue.h"
#include "totemPlugin.h"

#define DASHES "--"

/* How much data bytes to request */
#define PLUGIN_STREAM_CHUNK_SIZE (8 * 1024)

NPNetscapeFuncs totemPlugin::sNPN;

#if defined(TOTEM_COMPLEX_PLUGIN) && defined(HAVE_NSTARRAY_H)
nsTArray<totemPlugin*> *totemPlugin::sPlugins;

/* Keep the same order as totemPlugin::Control enum! */
static const char *kControl[] = {
	"All",
	"ControlPanel",
	"FFCtrl",
	"HomeCtrl",
	"ImageWindow",
	"InfoPanel",
	"InfoVolumePanel",
	"MuteCtrl",
	"MuteVolume",
	"PauseButton",
	"PlayButton",
	"PlayOnlyButton",
	"PositionField",
	"PositionSlider",
	"RWCtrl",
	"StatusBar",
	"StatusField",
	"StopButton",
	"TACCtrl",
	"VolumeSlider",
};
#endif /* TOTEM_COMPLEX_PLUGIN */

void*
totemPlugin::operator new (size_t aSize) CPP_THROW_NEW
{
	void *object = ::operator new (aSize);
	if (object) {
		memset (object, 0, aSize);
	}

	return object;
}

totemPlugin::totemPlugin (NPP aInstance)
:	mInstance (aInstance),
	mWidth (-1),
	mHeight (-1),
	mViewerFD (-1),
#ifdef TOTEM_COMPLEX_PLUGIN
	mAutostart (PR_FALSE),
#else
	mAutostart (PR_TRUE),
#endif /* TOTEM_NARROWSPACE_PLUGIN */
	mNeedViewer (PR_TRUE)
{
	D ("totemPlugin ctor [%p]", (void*) this);

#if defined(TOTEM_COMPLEX_PLUGIN) && defined(HAVE_NSTARRAY_H)
	/* Add |this| to the global plugins list */
	NS_ASSERTION (sPlugins->IndexOf (this) == NoIndex, "WTF?");
	if (!sPlugins->AppendElement (this)) {
		D ("Couldn't maintain plugin list!");
	}
#endif /* TOTEM_COMPLEX_PLUGIN */
}

totemPlugin::~totemPlugin ()
{
#if defined(TOTEM_COMPLEX_PLUGIN) && defined(HAVE_NSTARRAY_H)
	/* Remove us from the plugins list */
	NS_ASSERTION (sPlugins->IndexOf (this) != NoIndex, "WTF?");
	sPlugins->RemoveElement (this);

	TransferConsole ();
#endif /* TOTEM_COMPLEX_PLUGIN */

	if (mScriptable) {
		mScriptable->SetPlugin (nsnull);
		NS_RELEASE (mScriptable);
		mScriptable = nsnull;
	}

	if (mBusProxy) {
		dbus_g_proxy_disconnect_signal (mBusProxy,
						"NameOwnerChanged",
						G_CALLBACK (NameOwnerChangedCallback),
						NS_REINTERPRET_CAST (void*, this));
		g_object_unref (mBusProxy);
		mBusProxy = NULL;
	}

	ViewerCleanup ();

	if (mTimer) {
		mTimer->Cancel ();
		NS_RELEASE (mTimer);
		mTimer = nsnull;
	}

	NS_IF_RELEASE (mServiceManager);
	NS_IF_RELEASE (mIOService);
	NS_IF_RELEASE (mPluginDOMElement);
	NS_IF_RELEASE (mBaseURI);
	NS_IF_RELEASE (mRequestBaseURI);
	NS_IF_RELEASE (mRequestURI);
	NS_IF_RELEASE (mSrcURI);

#ifdef TOTEM_GMP_PLUGIN
	NS_IF_RELEASE (mURLURI);
#endif

#ifdef TOTEM_NARROWSPACE_PLUGIN
	NS_IF_RELEASE (mHrefURI);
	NS_IF_RELEASE (mQtsrcURI);
#endif

#if defined(TOTEM_COMPLEX_PLUGIN) && defined(HAVE_NSTARRAY_H)
	NS_IF_RELEASE (mPluginOwnerDocument);
#endif /* TOTEM_COMPLEX_PLUGIN */

	D ("totemPlugin dtor [%p]", (void*) this);
}

/* public functions */

nsresult
totemPlugin::DoCommand (const char *aCommand)
{
	D ("DoCommand '%s'", aCommand);

	/* FIXME: queue the action instead */
	if (!mViewerReady)
		return NS_OK;

	NS_ASSERTION (mViewerProxy, "No viewer proxy");
	dbus_g_proxy_call_no_reply (mViewerProxy,
				    "DoCommand",
				    G_TYPE_STRING, aCommand,
				    G_TYPE_INVALID,
				    G_TYPE_INVALID);

	return NS_OK;
}

/* Viewer interaction */

NPError
totemPlugin::ViewerFork ()
{
#if defined(TOTEM_COMPLEX_PLUGIN) && defined(HAVE_NSTARRAY_H)
	/* Don't fork a viewer if we're going to use another one */
	if (!mNeedViewer)
		return NPERR_NO_ERROR;
#endif /* TOTEM_COMPLEX_PLUGIN */

	const char *userAgent = CallNPN_UserAgentProc (sNPN.uagent,
						       mInstance);
	if (!userAgent) {
		/* See https://bugzilla.mozilla.org/show_bug.cgi?id=328778 */
		D ("User agent has more than 127 characters; fix your browser!");
	}

        GPtrArray *arr = g_ptr_array_new ();
	/* FIXME! no need to strdup, all args are const! */

	/* FIXME what use is this anyway? */
#ifdef TOTEM_RUN_IN_SOURCE_TREE
	if (g_file_test ("./totem-plugin-viewer",
			 G_FILE_TEST_EXISTS) != FALSE) {
			 g_ptr_array_add (arr, g_strdup ("./totem-plugin-viewer"));
	} else {
		g_ptr_array_add (arr,
				 g_build_filename (LIBEXECDIR, "totem-plugin-viewer", NULL));
	}
#else
	g_ptr_array_add (arr,
			 g_build_filename (LIBEXECDIR, "totem-plugin-viewer", NULL));
#endif

	/* So we can debug X errors in the viewer */
	const char *sync = g_getenv ("TOTEM_EMBEDDED_DEBUG_SYNC");
	if (sync && sync[0] == '1') {
		g_ptr_array_add (arr, g_strdup ("--sync"));
	}

#ifdef GNOME_ENABLE_DEBUG
	const char *fatal = g_getenv ("TOTEM_EMBEDDED_DEBUG_FATAL");
	if (fatal && fatal[0] == '1') {
		g_ptr_array_add (arr, g_strdup ("--g-fatal-warnings"));
	}
#endif

	g_ptr_array_add (arr, g_strdup (DASHES TOTEM_OPTION_PLUGIN_TYPE));
#if defined(TOTEM_BASIC_PLUGIN)
	g_ptr_array_add (arr, g_strdup ("basic"));
#elif defined(TOTEM_GMP_PLUGIN)
	g_ptr_array_add (arr, g_strdup ("gmp"));
#elif defined(TOTEM_COMPLEX_PLUGIN)
	g_ptr_array_add (arr, g_strdup ("complex"));
#elif defined(TOTEM_NARROWSPACE_PLUGIN)
	g_ptr_array_add (arr, g_strdup ("narrowspace"));
#elif defined(TOTEM_MULLY_PLUGIN)
	g_ptr_array_add (arr, g_strdup ("mully"));
#else
#error Unknown plugin type
#endif

	if (userAgent) {
		g_ptr_array_add (arr, g_strdup (DASHES TOTEM_OPTION_USER_AGENT));
		g_ptr_array_add (arr, g_strdup (userAgent));
	}

	/* FIXME: remove this */
	if (!mMimeType.IsEmpty ()) {
		g_ptr_array_add (arr, g_strdup (DASHES TOTEM_OPTION_MIMETYPE));
		g_ptr_array_add (arr, g_strdup (mMimeType.get()));
	}

	if (mControllerHidden) {
		g_ptr_array_add (arr, g_strdup (DASHES TOTEM_OPTION_CONTROLS_HIDDEN));
	}

	if (mShowStatusbar) {
		g_ptr_array_add (arr, g_strdup (DASHES TOTEM_OPTION_STATUSBAR));
	}

	if (mHidden) {
		g_ptr_array_add (arr, g_strdup (DASHES TOTEM_OPTION_HIDDEN));
	}

 	if (mRepeat) {
 		g_ptr_array_add (arr, g_strdup (DASHES TOTEM_OPTION_REPEAT));
 	}

	if (mAudioOnly) {
		g_ptr_array_add (arr, g_strdup (DASHES TOTEM_OPTION_AUDIOONLY));
	}

	if (!mAutostart) {
		g_ptr_array_add (arr, g_strdup (DASHES TOTEM_OPTION_NOAUTOSTART));
	}

	g_ptr_array_add (arr, NULL);
	char **argv = (char **) g_ptr_array_free (arr, FALSE);

#ifdef GNOME_ENABLE_DEBUG
	{
		GString *s;
		int i;

		s = g_string_new ("Launching: ");
		for (i = 0; argv[i] != NULL; i++) {
			g_string_append (s, argv[i]);
			g_string_append (s, " ");
		}
		D ("%s", s->str);
		g_string_free (s, TRUE);
	}
#endif

	mViewerReady = PR_FALSE;

	/* Don't wait forever! */
	const PRUint32 kViewerTimeout = 30 * 1000; /* ms */
	nsresult rv = mTimer->InitWithFuncCallback (ViewerForkTimeoutCallback,
						    NS_REINTERPRET_CAST (void*, this),
						    kViewerTimeout,
						    nsITimer::TYPE_ONE_SHOT);
	if (NS_FAILED (rv)) {
		D ("Failed to initialise timer");
		return NPERR_GENERIC_ERROR;
	}

	/* FIXME: once gecko is multihead-safe, this should use gdk_spawn_on_screen_with_pipes */
	GError *error = NULL;
	if (g_spawn_async_with_pipes (NULL /* working directory FIXME: use $TMPDIR ? */,
				      argv,
				      NULL /* environment */,
				      GSpawnFlags(0),
				      NULL /* child setup func */, NULL,
				      &mViewerPID,
				      &mViewerFD, NULL, NULL,
				      &error) == FALSE)
	{
		g_warning ("Failed to spawn viewer: %s", error->message);
		g_error_free(error);

		g_strfreev (argv);

		return NPERR_GENERIC_ERROR;
	}

	g_strfreev (argv);

	D("Viewer spawned, PID %d", mViewerPID);

	/* FIXME: can this happen? */
	if (mViewerFD < 0) {
		ViewerCleanup ();
		return NPERR_GENERIC_ERROR;
	}

	/* Set mViewerFD nonblocking */
	fcntl (mViewerFD, F_SETFL, O_NONBLOCK);

	return NPERR_NO_ERROR;
}

void
totemPlugin::ViewerSetup ()
{
	/* already set up */
	if (mViewerSetUp)
		return;

	mViewerSetUp = PR_TRUE;

	D ("ViewerSetup");

	/* Cancel timeout */
	nsresult rv = mTimer->Cancel ();
	if (NS_FAILED (rv)) {
		D ("Failed to cancel timer");
	}

	mViewerProxy = dbus_g_proxy_new_for_name (mBusConnection,
						  mViewerServiceName.get (),
						  TOTEM_PLUGIN_VIEWER_DBUS_PATH,
						  TOTEM_PLUGIN_VIEWER_INTERFACE_NAME);

	dbus_g_object_register_marshaller
		(totempluginviewer_marshal_VOID__UINT_UINT,
		 G_TYPE_NONE, G_TYPE_UINT, G_TYPE_UINT, G_TYPE_INVALID);
	dbus_g_proxy_add_signal (mViewerProxy, "ButtonPress",
				 G_TYPE_UINT,
				 G_TYPE_UINT,
				 G_TYPE_INVALID);
	dbus_g_proxy_connect_signal (mViewerProxy,
				     "ButtonPress",
				     G_CALLBACK (ButtonPressCallback),
				     NS_REINTERPRET_CAST (void*, this),
				     NULL);

	dbus_g_proxy_add_signal (mViewerProxy,
				 "StopStream",
				 G_TYPE_INVALID);
	dbus_g_proxy_connect_signal (mViewerProxy,
				     "StopStream",
				     G_CALLBACK (StopStreamCallback),
				     NS_REINTERPRET_CAST (void*, this),
				     NULL);

	if (mHidden) {
		ViewerReady ();
	} else {
		ViewerSetWindow ();
	}

#if defined(TOTEM_COMPLEX_PLUGIN) && defined(HAVE_NSTARRAY_H)
	/* Notify all consoles */
	PRUint32 count = sPlugins->Length ();
	for (PRUint32 i = 0; i < count; ++i) {
		totemPlugin *plugin = sPlugins->ElementAt (i);

		if (plugin->mConsoleClassRepresentant == this)
			plugin->UnownedViewerSetup ();
	}
#endif /* TOTEM_COMPLEX_PLUGIN */
}


void
totemPlugin::ViewerCleanup ()
{
	mViewerReady = PR_FALSE;

	mViewerBusAddress.SetLength (0);
	mViewerServiceName.SetLength (0);

	if (mViewerPendingCall) {
		dbus_g_proxy_cancel_call (mViewerProxy, mViewerPendingCall);
		mViewerPendingCall = NULL;
	}

	if (mViewerProxy) {
		dbus_g_proxy_disconnect_signal (mViewerProxy,
						"ButtonPress",
						G_CALLBACK (ButtonPressCallback),
						NS_REINTERPRET_CAST (void*, this));
		dbus_g_proxy_disconnect_signal (mViewerProxy,
						"StopStream",
						G_CALLBACK (StopStreamCallback),
						NS_REINTERPRET_CAST (void*, this));

		g_object_unref (mViewerProxy);
		mViewerProxy = NULL;
	}

	if (mViewerFD >= 0) {
		close (mViewerFD);
		mViewerFD = -1;
	}

	if (mViewerPID) {
		kill (mViewerPID, SIGKILL);
		g_spawn_close_pid (mViewerPID);
		mViewerPID = 0;
	}
}

void
totemPlugin::ViewerSetWindow ()
{
	if (mWindowSet || mWindow == 0)
		return;

	if (!mViewerProxy) {
		D ("No viewer proxy yet, deferring SetWindow");
		return;
	}

	/* FIXME this shouldn't happen here */
	if (mHidden) {
		mWindowSet = PR_TRUE;
		ViewerReady ();
		return;
	}

	NS_ASSERTION (mViewerPendingCall == NULL, "Have a pending call");

	D ("Calling SetWindow");
	mViewerPendingCall = 
		dbus_g_proxy_begin_call (mViewerProxy,
					 "SetWindow",
					 ViewerSetWindowCallback,
					 NS_REINTERPRET_CAST (void*, this),
					 NULL,
#if defined(TOTEM_COMPLEX_PLUGIN) && defined(HAVE_NSTARRAY_H)
					 G_TYPE_STRING, mControls.get (),
#else
					 G_TYPE_STRING, "All",
#endif
					 G_TYPE_UINT, (guint) mWindow,
					 G_TYPE_INT, (gint) mWidth,
					 G_TYPE_INT, (gint) mHeight,
					 G_TYPE_INVALID);
		
	mWindowSet = PR_TRUE;
}

void
totemPlugin::ViewerReady ()
{
	D ("ViewerReady");

	NS_ASSERTION (!mViewerReady, "Viewer already ready");

	mViewerReady = PR_TRUE;

	if (mAutostart) {
		RequestStream (PR_FALSE);
	} else {
		mWaitingForButtonPress = PR_TRUE;
	}

#ifdef TOTEM_NARROWSPACE_PLUGIN
	/* Tell the viewer it has an href */
	if (!mHref.IsEmpty ()) {
		dbus_g_proxy_call_no_reply (mViewerProxy,
					    "SetHref",
					    G_TYPE_STRING, mHref.get (),
					    G_TYPE_STRING, mTarget.get (),
					    G_TYPE_INVALID);
	}
#endif /* TOTEM_NARROWSPACE_PLUGIN */
}

void
totemPlugin::ViewerButtonPressed (guint aTimestamp, guint aButton)
{
	D ("ButtonPress");

#ifdef TOTEM_NARROWSPACE_PLUGIN
	/* FIXME set href="" afterwards, so we don't try to launch again when the user clicks again? */
	if (!mHref.IsEmpty ()) {
		if (g_ascii_strcasecmp (mTarget.get (), "quicktimeplayer") == 0) {
			D ("Opening movie '%s' in external player", mHref.get ());
			dbus_g_proxy_call_no_reply (mViewerProxy,
						    "LaunchPlayer",
						    G_TYPE_STRING, mHref.get (),
						    G_TYPE_UINT, time,
						    G_TYPE_INVALID);
			return;
		}
		if (g_ascii_strcasecmp (mTarget.get (), "myself") == 0 ||
		    mTarget.Equals (NS_LITERAL_CSTRING ("_current")) ||
		    mTarget.Equals (NS_LITERAL_CSTRING ("_self"))) {
			D ("Opening movie '%s'", mHref.get ());
			dbus_g_proxy_call_no_reply (mViewerProxy,
						    "SetHref",
						    G_TYPE_STRING, NULL,
						    G_TYPE_STRING, NULL,
						    G_TYPE_INVALID);
			/* FIXME this isn't right, we should just create a mHrefURI and instruct to load that one */
			SetQtsrc (mHref);
			RequestStream (PR_TRUE);
			return;
		}

		/* Load URL in browser. This will either open a new website,
		 * or execute some javascript.
		 */
		nsCString href;
		if (mHrefURI) {
			mHrefURI->GetSpec (href);
		} else {
			href = mHref;
		}

		if (CallNPN_GetURLProc (sNPN.geturl,
					mInstance,
					href.get (),
					mTarget.get ()) != NPERR_NO_ERROR) {
			D ("Failed to launch URL '%s' in browser", mHref.get ());
		}

		return;
	}
#endif

	if (!mWaitingForButtonPress)
		return;

	mWaitingForButtonPress = PR_FALSE;

	/* Now is the time to start the stream */
	if (!mAutostart &&
	    !mStream) {
		RequestStream (PR_FALSE);
	}
}

void
totemPlugin::NameOwnerChanged (const char *aName,
			       const char *aOldOwner,
			       const char *aNewOwner)
{
	if (!mViewerPID)
		return;

	/* Construct viewer interface name */
	if (NS_UNLIKELY (mViewerServiceName.IsEmpty ())) {
		char name[256];

		g_snprintf (name, sizeof (name),
			    TOTEM_PLUGIN_VIEWER_NAME_TEMPLATE,
			    mViewerPID);
		mViewerServiceName.Assign (name);

		D ("Viewer DBus interface name is '%s'", mViewerServiceName.get ());
	}

	if (!mViewerServiceName.Equals (nsDependentCString (aName)))
		return;

	D ("NameOwnerChanged old-owner '%s' new-owner '%s'", aOldOwner, aNewOwner);

	if (aOldOwner[0] == '\0' /* empty */ &&
	    aNewOwner[0] != '\0' /* non-empty */) {
		if (mViewerBusAddress.Equals (nsDependentCString (aNewOwner))) {
			D ("Already have owner, why are we notified again?");
		} else if (!mViewerBusAddress.IsEmpty ()) {
			D ("WTF, new owner!?");
		} else {
			/* This is the regular case */
			D ("Viewer now connected to the bus");
		}

		mViewerBusAddress.Assign (aNewOwner);

		ViewerSetup ();
	} else if (!mViewerBusAddress.IsEmpty () &&
		   mViewerBusAddress.Equals (nsDependentCString (aOldOwner))) {
		D ("Viewer lost connection!");

		mViewerBusAddress.SetLength (0); /* truncate */
		/* FIXME */
		/* ViewerCleanup () ? */
		/* FIXME if we're not quitting, put up error viewer */
	}
	/* FIXME do we really need the lost-connection case?
	 * We could just disconnect the handler in ViewerSetup
	 */
}

/* Stream handling */

void
totemPlugin::ClearRequest ()
{
	if (mRequestBaseURI) {
		NS_RELEASE (mRequestBaseURI);
		mRequestBaseURI = nsnull;
	}
	if (mRequestURI) {
		NS_RELEASE (mRequestURI);
		mRequestURI = nsnull;
	}
}

void
totemPlugin::RequestStream (PRBool aForceViewer)
{
	NS_ASSERTION (mViewerReady, "Viewer not ready");

	if (mStream) {
		D ("Unexpectedly have a stream!");
		/* FIXME cancel existing stream, schedule new timer to try again */
		return;
	}

	ClearRequest ();

	/* Now work out which URL to request */
	nsIURI *baseURI = nsnull;
	nsIURI *requestURI = nsnull;

#ifdef TOTEM_GMP_PLUGIN
	/* Prefer filename over src */
	if (mURLURI) {
		requestURI = mURLURI;
		baseURI = mSrcURI; /* FIXME: that correct? */
	}
#endif

#ifdef TOTEM_NARROWSPACE_PLUGIN
	/* Prefer qtsrc over src */
	if (mQtsrcURI) {
		requestURI = mQtsrcURI;
		baseURI = mSrcURI;
	}
#if 0
	if (href && !requestURL) {
	 	/* FIXME this looks wrong? any real-world testcase sites around? */
		requestURL = href;
	}
#endif
#endif

	/* Fallback */
	if (!requestURI)
		requestURI = mSrcURI;

	if (!baseURI)
		baseURI = mBaseURI;

	/* Nothing to do */
	if (!requestURI)
		return;

	NS_ADDREF (mRequestBaseURI = baseURI);
	NS_ADDREF (mRequestURI = requestURI);

	/* FIXME use the right base! */
	nsCString baseSpec, spec;
	baseURI->GetSpec (baseSpec);
	requestURI->GetSpec (spec);

	/* Shouldn't happen, but who knows */
	if (spec.IsEmpty ())
		return;

	/* If the URL is supported and the caller isn't asking us to make
	 * the viewer open the stream, we call OpenStream, and
	 * otherwise OpenURI. */
	if (!aForceViewer && IsSchemeSupported (requestURI)) {
		/* This will fail for the 2nd stream, but we shouldn't
		 * ever come to using it for the 2nd stream... */

		mViewerPendingCall =
			dbus_g_proxy_begin_call (mViewerProxy,
						 "OpenStream",
						 ViewerOpenStreamCallback,
						 NS_REINTERPRET_CAST (void*, this),
						 NULL,
						 G_TYPE_STRING, spec.get (),
						 G_TYPE_STRING, baseSpec.get (),
						 G_TYPE_INVALID);
	} else {
		mViewerPendingCall =
			dbus_g_proxy_begin_call (mViewerProxy,
						 "OpenURI",
						 ViewerOpenURICallback,
						 NS_REINTERPRET_CAST (void*, this),
						 NULL,
						 G_TYPE_STRING, spec.get (),
						 G_TYPE_STRING, baseSpec.get (),
						 G_TYPE_INVALID);
	}

	/* FIXME: start playing in the callbacks ! */
}

void
totemPlugin::UnsetStream ()
{
	if (!mStream)
		return;

	if (CallNPN_DestroyStreamProc (sNPN.destroystream,
	    			       mInstance,
				       mStream,
				       NPRES_DONE) != NPERR_NO_ERROR) {
		    g_warning ("Couldn't destroy the stream");
		    /* FIXME: set mStream to NULL here too? */
		    return;
	}

	/* Calling DestroyStream should already have set this to NULL */
	NS_ASSERTION (!mStream, "Should not have a stream anymore");
	mStream = nsnull; /* just to make sure */
}

/* Callbacks */

/* static */ void PR_CALLBACK 
totemPlugin::NameOwnerChangedCallback (DBusGProxy *proxy,
				       const char *aName,
				       const char *aOldOwner,
				       const char *aNewOwner,
				       void *aData)
{
	totemPlugin *plugin = NS_REINTERPRET_CAST (totemPlugin*, aData);

	plugin->NameOwnerChanged (aName, aOldOwner, aNewOwner);
}

/* static */ void PR_CALLBACK
totemPlugin::ViewerForkTimeoutCallback (nsITimer *aTimer,
				        void *aData)
{
	totemPlugin *plugin = NS_REINTERPRET_CAST (totemPlugin*, aData);

	D ("ViewerForkTimeoutCallback");

	/* FIXME: can this really happen? */
	NS_ASSERTION (!plugin->mViewerReady, "Viewer ready but timeout running?");

	plugin->ViewerCleanup ();
	/* FIXME start error viewer */
}

/* static */ void PR_CALLBACK
totemPlugin::ButtonPressCallback (DBusGProxy *proxy,
				  guint aTimestamp,
				  guint aButton,
			          void *aData)
{
	totemPlugin *plugin = NS_REINTERPRET_CAST (totemPlugin*, aData);

	D ("ButtonPress signal received");

	plugin->ViewerButtonPressed (aTimestamp, aButton);
}

/* static */ void PR_CALLBACK
totemPlugin::StopStreamCallback (DBusGProxy *proxy,
			         void *aData)
{
	totemPlugin *plugin = NS_REINTERPRET_CAST (totemPlugin*, aData);

	D ("StopStream signal received");

	plugin->UnsetStream ();
}

/* static */ void PR_CALLBACK
totemPlugin::ViewerSetWindowCallback (DBusGProxy *aProxy,
				      DBusGProxyCall *aCall,
				      void *aData)
{
	totemPlugin *plugin = NS_REINTERPRET_CAST (totemPlugin*, aData);

	D ("SetWindow reply");

	NS_ASSERTION (aCall == plugin->mViewerPendingCall, "SetWindow not the current call");

	plugin->mViewerPendingCall = NULL;

	GError *error = NULL;
	if (!dbus_g_proxy_end_call (aProxy, aCall, &error, G_TYPE_INVALID)) {
		/* FIXME: mViewerFailed = PR_TRUE */
		g_warning ("SetWindow failed: %s", error->message);
		g_error_free (error);
		return;
	}

	plugin->ViewerReady ();
}

/* static */ void PR_CALLBACK
totemPlugin::ViewerOpenStreamCallback (DBusGProxy *aProxy,
				       DBusGProxyCall *aCall,
				       void *aData)
{
	totemPlugin *plugin = NS_REINTERPRET_CAST (totemPlugin*, aData);

	D ("OpenStream reply");

	NS_ASSERTION (aCall == plugin->mViewerPendingCall, "OpenStream not the current call");

	plugin->mViewerPendingCall = NULL;

	GError *error = NULL;
	if (!dbus_g_proxy_end_call (aProxy, aCall, &error, G_TYPE_INVALID)) {
		g_warning ("OpenStream failed: %s", error->message);
		g_error_free (error);
		return;
	}

	/* FIXME this isn't the best way... */
	if (plugin->mHidden &&
	    plugin->mAutostart) {
		plugin->DoCommand (TOTEM_COMMAND_PLAY);
	}

	NS_ASSERTION (!plugin->mExpectingStream, "Already expecting a stream");
	NS_ENSURE_TRUE (plugin->mRequestURI, );

	plugin->mExpectingStream = PR_TRUE;

	nsCString spec;
	plugin->mRequestURI->GetSpec (spec);

	/* Use GetURLNotify so we can reset mExpectingStream on failure */
	NPError err = CallNPN_GetURLNotifyProc (sNPN.geturlnotify,
						plugin->mInstance,
						spec.get (),
						nsnull,
						nsnull);
	if (err != NPERR_NO_ERROR) {
		plugin->mExpectingStream = PR_FALSE;

		D ("GetURLNotify '%s' failed with error %d", spec.get (), err);
	}
}

/* static */ void PR_CALLBACK
totemPlugin::ViewerOpenURICallback (DBusGProxy *aProxy,
				    DBusGProxyCall *aCall,
				    void *aData)
{
	totemPlugin *plugin = NS_REINTERPRET_CAST (totemPlugin*, aData);

	D ("OpenURI reply");

	NS_ASSERTION (aCall == plugin->mViewerPendingCall, "OpenURI not the current call");

	plugin->mViewerPendingCall = NULL;

	GError *error = NULL;
	if (!dbus_g_proxy_end_call (aProxy, aCall, &error, G_TYPE_INVALID)) {
		g_warning ("OpenURI failed: %s", error->message);
		g_error_free (error);
		return;
	}

	/* FIXME this isn't the best way... */
	if (plugin->mAutostart) {
		plugin->DoCommand (TOTEM_COMMAND_PLAY);
	}
}

/* Auxiliary functions */

void
totemPlugin::GetRealMimeType (const char *mimetype,
			      nsACString &_retval)
{
	_retval.Assign ("");

	const totemPluginMimeEntry *mimetypes;
	PRUint32 count;
	totemScriptablePlugin::PluginMimeTypes (&mimetypes, &count);

	for (PRUint32 i = 0; i < count; ++i) {
		if (strcmp (mimetypes[i].mimetype, mimetype) == 0) {
			if (mimetypes[i].mime_alias != NULL) {
				_retval.Assign (mimetypes[i].mime_alias);
			} else {
				_retval.Assign (mimetype);
			}
			return;
		}
	}

	D ("Real mime-type for '%s' not found", mimetype);
}

PRBool
totemPlugin::IsMimeTypeSupported (const char *mimetype,
				  const char *url)
{
	NS_ENSURE_TRUE (mimetype, PR_FALSE);

#if defined(TOTEM_NARROWSPACE_PLUGIN) || defined(TOTEM_GMP_PLUGIN) || defined(TOTEM_MULLY_PLUGIN)
	/* We can always play those image types */
	if (strcmp (mimetype, "image/jpeg") == 0)
		return PR_TRUE;
	if (strcmp (mimetype, "image/gif") == 0)
		return PR_TRUE;
#endif /* TOTEM_NARROWSPACE_PLUGIN || TOTEM_GMP_PLUGIN || TOTEM_MULLY_PLUGIN */

	/* Stupid web servers will do that */
	if (strcmp (mimetype, GNOME_VFS_MIME_TYPE_UNKNOWN) == 0)
		return PR_TRUE;

	const totemPluginMimeEntry *mimetypes;
	PRUint32 count;
	totemScriptablePlugin::PluginMimeTypes (&mimetypes, &count);

	for (PRUint32 i = 0; i < count; ++i) {
		if (strcmp (mimetypes[i].mimetype, mimetype) == 0)
			return PR_TRUE;
	}

	/* Not supported? Probably a broken webserver */
	const char *guessed = gnome_vfs_get_mime_type_for_name (url);

	D ("Guessed mime-type '%s' for '%s'", guessed, url);
	for (PRUint32 i = 0; i < count; ++i) {
		if (strcmp (mimetypes[i].mimetype, guessed) == 0)
			return PR_TRUE;
	}

	/* Still unsupported? Try to get it without the arguments
	 * passed to the script */
	const char *s = strchr (url, '?');
	if (s == NULL)
		return PR_FALSE;

	char *no_args = g_strndup (url, s - url);
	guessed = gnome_vfs_get_mime_type_for_name (no_args);
	D ("Guessed mime-type '%s' for '%s' without the arguments", guessed, url);
	g_free (no_args);

	for (PRUint32 i = 0; i < count; ++i) {
		if (strcmp (mimetypes[i].mimetype, guessed) == 0)
			return PR_TRUE;
	}

	return PR_FALSE;
}

PRBool
totemPlugin::IsSchemeSupported (nsIURI *aURI)
{
	if (!aURI)
		return PR_FALSE;

	nsCString scheme;
	nsresult rv = aURI->GetScheme (scheme);
	if (NS_FAILED (rv) || scheme.IsEmpty ())
		return PR_FALSE;

	nsIProtocolHandler *handler = nsnull;
	rv = mIOService->GetProtocolHandler (scheme.get (), &handler);

	/* Check that it's not the external protocol handler! */
	nsIExternalProtocolHandler *extHandler = nsnull;
	if (NS_SUCCEEDED (rv) && handler) {
		CallQueryInterface (handler, &extHandler);
	}

	PRBool isSupported = NS_SUCCEEDED (rv) && handler && !extHandler;

	NS_IF_RELEASE (handler); /* this nullifies |handler| */
	NS_IF_RELEASE (extHandler);

	D ("IsSchemeSupported scheme '%s': %s", scheme.get (), isSupported ? "yes" : "no");

	return isSupported;
}

PRBool
totemPlugin::ParseBoolean (const char *key,
			   const char *value,
			   PRBool default_val)
{
	if (value == NULL || strcmp (value, "") == 0)
		return default_val;
	if (g_ascii_strcasecmp (value, "false") == 0
			|| g_ascii_strcasecmp (value, "0") == 0)
		return PR_FALSE;
	if (g_ascii_strcasecmp (value, "true") == 0
			|| g_ascii_strcasecmp (value, "1") == 0)
		return PR_TRUE;

	D ("Unknown value '%s' for parameter '%s'", value, key);

	return default_val;
}

PRBool
totemPlugin::GetBooleanValue (GHashTable *args,
			      const char *key,
			      PRBool default_val)
{
	const char *value;

	value = (const char *) g_hash_table_lookup (args, key);
	if (value == NULL)
		return default_val;

	return ParseBoolean (key, value, default_val);
}

PRUint32
totemPlugin::GetEnumIndex (GHashTable *args,
			   const char *key,
			   const char *values[],
			   PRUint32 n_values,
			   PRUint32 default_value)
{
	const char *value = (const char *) g_hash_table_lookup (args, key);
	if (!value)
		return default_value;

	for (PRUint32 i = 0; i < n_values; ++i) {
		if (g_ascii_strcasecmp (value, values[i]) == 0)
			return i;
	}

	return default_value;
}

/* Public functions for use by the scriptable plugin */

nsresult
totemPlugin::SetSrc (const nsACString& aURL)
{
	if (mSrcURI) {
		NS_RELEASE (mSrcURI);
		mSrcURI = nsnull;
	}

	mSrc = aURL;

	/* If |src| is empty, don't resolve the URI! Otherwise we may
	 * try to load an (probably iframe) html document as our video stream.
	 */
	if (mSrc.Length () == 0)
		return NS_OK;

	nsresult rv = mIOService->NewURI (aURL, nsnull /* FIXME! use document charset */,
					  mBaseURI, &mSrcURI);
	if (NS_FAILED (rv)) {
		D ("Failed to create src URI (rv=%x)", rv);
		mSrcURI = nsnull;
	}

	return rv;
}

#ifdef TOTEM_GMP_PLUGIN

nsresult
totemPlugin::SetURL (const nsACString& aURL)
{
	if (mURLURI) {
		NS_RELEASE (mURLURI);
		mURLURI = nsnull;
	}

	/* Don't allow empty URL */
	if (aURL.Length () == 0)
		return NS_OK;

	/* FIXME: what is the correct base for the URL param? */
	nsresult rv;
	nsIURI *baseURI;
	if (mSrcURI) {
		baseURI = mSrcURI;
	} else {
		baseURI = mBaseURI;
	}

	rv = mIOService->NewURI (aURL, nsnull /* FIXME document charset? */,
				 baseURI, &mURLURI);
	if (NS_FAILED (rv)) {
		D ("Failed to create URL URI (rv=%x)", rv);
	}

	/* FIXME: security checks? */

	return rv;
}

#endif /* TOTEM_GMP_PLUGIN */

#ifdef TOTEM_NARROWSPACE_PLUGIN

nsresult
totemPlugin::SetQtsrc (const nsCString& aURL)
{
	/* FIXME can qtsrc have URL extensions? */

	if (mQtsrcURI) {
		NS_RELEASE (mQtsrcURI);
		mQtsrcURI = nsnull;
	}

	/* Don't allow empty qtsrc */
	if (aURL.Length () == 0)
		return NS_OK;

	nsresult rv;
	nsIURI *baseURI;
	if (mSrcURI) {
		baseURI = mSrcURI;
	} else {
		baseURI = mBaseURI;
	}

	rv = mIOService->NewURI (aURL, nsnull /* FIXME document charset? */,
				 baseURI, &mQtsrcURI);
	if (NS_FAILED (rv)) {
		D ("Failed to create QTSRC URI (rv=%x)", rv);
	}

	/* FIXME: security checks? */

	return rv;
}

nsresult
totemPlugin::SetHref (const nsCString& aURL)
{
	nsCString url, target;
	PRBool hasExtensions = ParseURLExtensions (aURL, url, target);

	D ("SetHref '%s' has-extensions %d (url: '%s' target: '%s')",
	   nsCString (aURL).get (), hasExtensions, url.get (), target.get ());

	nsresult rv;
	nsIURI *baseURI;
	if (mQtsrcURI) {
		baseURI = mQtsrcURI;
	} else if (mSrcURI) {
		baseURI = mSrcURI;
	} else {
		baseURI = mBaseURI;
	}

	if (hasExtensions) {
		rv = baseURI->Resolve (url, mHref);

		if (!target.IsEmpty ())
			mTarget = target;
	} else {
		rv = baseURI->Resolve (aURL, mHref);
	}
	if (NS_SUCCEEDED (rv)) {
		D ("Resolved HREF '%s'", mHref.get());
	} else {
		D ("Failed to resolve HREF (rv=%x)", rv);
		mHref = hasExtensions ? url : aURL; /* save unresolved HREF */
	}

	return rv;
}

PRBool
totemPlugin::ParseURLExtensions (const nsACString &aString,
				 nsACString &_url,
				 nsACString &_target)
{
	const nsCString string (aString);

	const char *str = string.get ();
	if (str[0] != '<')
		return PR_FALSE;

	/* The expected form is "<URL> T<target> E<name=value pairs>".
	 * But since this is untrusted input from the web, we'll make sure it conforms to this!
	 */
	const char *end = strchr (str, '>');
	if (!end)
		return PR_FALSE;

	_url = nsDependentCSubstring (string, 1, PRUint32 (end - str - 1));

	const char *ext = strstr (end, " T<");
	if (ext) {
		const char *extend = strchr (ext, '>');
		if (extend) {
			_target = nsDependentCSubstring (ext + 3, PRUint32 (extend - ext - 3));
		}
	}

#if 0
	ext = strstr (end, " E<");
	if (ext) {
		const char *extend = strchr (ext, '>');
		if (extend) {
			D ("E = %s", nsCString (ext + 3, PRUint32 (extend - ext - 3)).get ());
		}
	}
#endif

	return PR_TRUE;
}

#endif /* TOTEM_NARROWSPACE_PLUGIN */

#if defined(TOTEM_COMPLEX_PLUGIN) && defined(HAVE_NSTARRAY_H)

totemPlugin *
totemPlugin::FindConsoleClassRepresentant ()
{
	/* FIXME: this treats "master" incorrectly */
	if (!mSrcURI ||
	    mConsole.IsEmpty () ||
	    mConsole.Equals (NS_LITERAL_CSTRING ("_unique")) ||
	    mConsole.Equals (NS_LITERAL_CSTRING ("_master"))) {
		D ("We're the representant for the console class");
		return nsnull;
	}

	totemPlugin *representant = nsnull;

	/* Try to find a the representant of the console class */
	PRUint32 count = sPlugins->Length ();
	for (PRUint32 i = 0; i < count; ++i) {
		totemPlugin *plugin = sPlugins->ElementAt (i);

		PRBool equal = PR_FALSE;
		/* FIXME: is this correct? Maybe we should use the toplevel document
		 * to allow frames (and check the frames for same-origin, obviously) ?
		 */
		if (plugin != this &&
		    plugin->mPluginOwnerDocument == mPluginOwnerDocument &&
		    mConsole.Equals (plugin->mConsole) &&
		    plugin->mSrcURI &&
		    NS_SUCCEEDED (plugin->mSrcURI->Equals (mSrcURI, &equal)) &&
		    equal) {
			if (plugin->mConsoleClassRepresentant) {
				representant = plugin->mConsoleClassRepresentant;
			} else {
				representant = plugin;
			}
			break;
		}
	}

	D ("Representant for the console class is %p", (void*) representant);

	return representant;
}

nsresult
totemPlugin::SetConsole (const nsACString &aConsole)
{
	/* Can't change console group */
	if (!mConsole.IsEmpty ())
		return NS_ERROR_ALREADY_INITIALIZED;

	/* FIXME: we might allow this, and kill the viewer instead.
	 * Or maybe not spawn the viewer if we don't have a console yet?
	 */
	if (mViewerPID)
		return NS_ERROR_ALREADY_INITIALIZED;

	mConsole = aConsole;

	NS_ASSERTION (mConsoleClassRepresentant == nsnull, "Already have a representant");

	mConsoleClassRepresentant = FindConsoleClassRepresentant ();
	mNeedViewer = (nsnull == mConsoleClassRepresentant);

	return NS_OK;
}

void
totemPlugin::TransferConsole ()
{
	/* Find replacement representant */
	totemPlugin *representant = nsnull;

	PRUint32 i, count = sPlugins->Length ();
	for (i = 0; i < count; ++i) {
		totemPlugin *plugin = sPlugins->ElementAt (i);

		if (plugin->mConsoleClassRepresentant == this) {
			representant = plugin;
			break;
		}
	}

	/* If there are no other elements in this console class, there's nothing to do */
	if (!representant)
		return;

	D ("Transferring console from %p to %p", (void*) this, (void*) representant);

	/* Store new representant in the plugins */
	representant->mConsoleClassRepresentant = nsnull;
	/* We can start at i since we got out when we found the first one in the loop above */
	for ( ; i < count; ++i) {
		totemPlugin *plugin = sPlugins->ElementAt (i);

		if (plugin->mConsoleClassRepresentant == this)
			plugin->mConsoleClassRepresentant = representant;
	}

	/* Now transfer viewer ownership */
	if (mScriptable) {
		NS_ASSERTION (!representant->mScriptable, "WTF");
		representant->mScriptable = mScriptable;
		mScriptable->SetPlugin (representant);
		mScriptable = nsnull;
	}

	representant->mNeedViewer = PR_TRUE;

	representant->mViewerPID = mViewerPID;
	mViewerPID = 0;

	representant->mViewerFD = mViewerFD;
	mViewerFD = -1;

	representant->mViewerBusAddress = mViewerBusAddress;
	representant->mViewerServiceName = mViewerServiceName;

	/* FIXME correct condition? */
	if (mViewerSetUp)
		representant->ViewerSetup ();
}

void
totemPlugin::UnownedViewerSetup ()
{
	/* already set up */
	if (mUnownedViewerSetUp)
		return;

	mUnownedViewerSetUp = PR_TRUE;

	D ("UnownedViewerSetup");

	NS_ASSERTION (mConsoleClassRepresentant, "We own the viewer!?");

	UnownedViewerSetWindow ();
}

void
totemPlugin::UnownedViewerSetWindow ()
{
	if (mWindowSet || mWindow == 0)
		return;

	if (!mUnownedViewerSetUp) {
		D ("No unowned viewer yet, deferring SetWindow");
		return;
	}

	NS_ASSERTION (mConsoleClassRepresentant, "We own the viewer!");

	NS_ENSURE_TRUE (mConsoleClassRepresentant->mViewerProxy, );

	/* FIXME: do we need a reply callback? */
	dbus_g_proxy_call_no_reply (mConsoleClassRepresentant->mViewerProxy,
				    "SetWindow",
				    G_TYPE_STRING, mControls.get (),
				    G_TYPE_UINT, (guint) mWindow,
				    G_TYPE_INT, (gint) mWidth,
				    G_TYPE_INT, (gint) mHeight,
				    G_TYPE_INVALID);

	mWindowSet = PR_TRUE;
}

void
totemPlugin::UnownedViewerUnsetWindow ()
{
	if (!mWindowSet || mWindow == 0)
		return;

	if (!mUnownedViewerSetUp)
		return;

	NS_ASSERTION (mConsoleClassRepresentant, "We own the viewer!");

	NS_ENSURE_TRUE (mConsoleClassRepresentant->mViewerProxy, );

	dbus_g_proxy_call_no_reply (mConsoleClassRepresentant->mViewerProxy,
				    "UnsetWindow",
				    G_TYPE_UINT, (guint) mWindow,
				    G_TYPE_INVALID);

	mWindowSet = PR_FALSE;
}

#endif /* TOTEM_COMPLEX_PLUGIN */

/* Plugin glue functions */

NPError
totemPlugin::Init (NPMIMEType mimetype,
		   uint16_t mode,
		   int16_t argc,
		   char *argn[],
		   char *argv[],
		   NPSavedData *saved)
{
	D ("Init mimetype '%s' mode %d", (const char *) mimetype, mode);

	/* Make sure the plugin stays resident, to avoid
	 * reloading the GObject types.
	 */
	CallNPN_SetValueProc (sNPN.setvalue,
			      mInstance,
			      NPPVpluginKeepLibraryInMemory,
			      NS_INT32_TO_PTR (PR_TRUE));

	/* mode is NP_EMBED, NP_FULL, or NP_BACKGROUND (see npapi.h) */
	/* FIXME we should error out if we are in fullscreen mode
	 * FIXME: This might be possible on gecko trunk by returning an
 	 * error code from the NewStream function.
	 */

	NPError err;
	err = CallNPN_GetValueProc (sNPN.getvalue,
				    mInstance, NPNVserviceManager,
				    NS_REINTERPRET_CAST (void *,
							 NS_REINTERPRET_CAST (void **, &mServiceManager)));
	if (err != NPERR_NO_ERROR || !mServiceManager) {
		D ("Failed to get the service manager");
		return NPERR_GENERIC_ERROR;
	}

	nsresult rv;
	rv = mServiceManager->GetServiceByContractID (NS_IOSERVICE_CONTRACTID,
						      NS_GET_IID (nsIIOService),
						      NS_REINTERPRET_CAST (void **, &mIOService));
	if (NS_FAILED (rv) || !mIOService) {
		D ("Failed to get IO service");
		return NPERR_GENERIC_ERROR;
	}

	err = CallNPN_GetValueProc (sNPN.getvalue,
				    mInstance, NPNVDOMElement,
				    NS_REINTERPRET_CAST (void *,
						         NS_REINTERPRET_CAST (void **, &mPluginDOMElement)));
	if (err != NPERR_NO_ERROR || !mPluginDOMElement) {
		D ("Failed to get our DOM Element");
		return NPERR_GENERIC_ERROR;
	}

#if defined(TOTEM_COMPLEX_PLUGIN) && defined(HAVE_NSTARRAY_H)
	rv = mPluginDOMElement->GetOwnerDocument (&mPluginOwnerDocument);
	if (NS_FAILED (rv) || !mPluginOwnerDocument) {
		D ("Plugin in a document!?");
		return NPERR_GENERIC_ERROR;
	}
#endif /* TOTEM_COMPLEX_PLUGIN */

	/* We'd like to get the base URI of our DOM element as a nsIURI,
	 * but there's no frozen method to do so (nsIContent/nsINode isn't available
	 * for non-MOZILLA_INTERNAL_API code). nsIDOM3Node isn't frozen either,
	 * but should be safe enough.
	 */
	nsIDOM3Node *dom3Node = nsnull;
	rv = CallQueryInterface (mPluginDOMElement, &dom3Node);
	if (NS_FAILED (rv) || !dom3Node) {
		D ("Failed to QI the DOM element to nsIDOM3Node");
		return NPERR_GENERIC_ERROR;
	}

        /* FIXME: can we cache this, or can it change (so we'll need to re-get every time we use it)? */
	nsString baseASpec;
	rv = dom3Node->GetBaseURI (baseASpec);
	if (NS_FAILED (rv) || baseASpec.IsEmpty ()) {
		/* We can't go on without a base URI ! */
		D ("Failed to get base URI spec");
		return NPERR_GENERIC_ERROR;
	}

	NS_ConvertUTF16toUTF8 baseSpec (baseASpec);

	D ("Base URI is '%s'", baseSpec.get ());

	rv = mIOService->NewURI (baseSpec, nsnull /* FIXME: use document charset */,
				 nsnull, &mBaseURI);
	if (NS_FAILED (rv) || !mBaseURI) {
		D ("Failed to construct base URI");
		return NPERR_GENERIC_ERROR;
	}

        /* Create timer */
        nsIComponentManager *compMan = nsnull;
	rv = CallQueryInterface (mServiceManager, &compMan);
        if (NS_FAILED (rv) || !compMan) {
                D ("Failed to get component manager");
                return NPERR_GENERIC_ERROR;
        }

        /* FIXME ? */
        rv = compMan->CreateInstanceByContractID (NS_TIMER_CONTRACTID,
                                                  nsnull,
                                                  NS_GET_IID (nsITimer),
                                                  NS_REINTERPRET_CAST (void **, &mTimer));
        if (NS_FAILED (rv) || !mTimer) {
                D ("Failed to create timer: rv=%x", rv);
                return NPERR_GENERIC_ERROR;
        }

	/* Setup DBus connection handling */
	GError *error = NULL;
	if (!(mBusConnection = dbus_g_bus_get (DBUS_BUS_SESSION, &error))) {
		g_message ("Failed to open DBUS session: %s", error->message);
		g_error_free (error);

		return NPERR_GENERIC_ERROR;
	};

	if (!(mBusProxy = dbus_g_proxy_new_for_name (mBusConnection,
	      					     DBUS_SERVICE_DBUS,
						     DBUS_PATH_DBUS,
						     DBUS_INTERFACE_DBUS))) {
		D ("Failed to get DBUS proxy");
		return NPERR_OUT_OF_MEMORY_ERROR;
	}

	dbus_g_proxy_add_signal (mBusProxy,
				 "NameOwnerChanged",
				 G_TYPE_STRING,
				 G_TYPE_STRING,
				 G_TYPE_STRING,
				 G_TYPE_INVALID);
	dbus_g_proxy_connect_signal (mBusProxy,
				     "NameOwnerChanged",
				     G_CALLBACK (NameOwnerChangedCallback),
				     NS_REINTERPRET_CAST (void*, this),
				     NULL);

	/* Find the "real" mime-type */
	GetRealMimeType (mimetype, mMimeType);

	D ("Real mimetype for '%s' is '%s'", mimetype, mMimeType.get());

	/* Now parse the attributes */
	GHashTable *args = g_hash_table_new_full (g_str_hash,
		g_str_equal,
		(GDestroyNotify) g_free,
		(GDestroyNotify) g_free);
	for (int16_t i = 0; i < argc; i++) {
		printf ("argv[%d] %s %s\n", i, argn[i], argv[i]);
		g_hash_table_insert (args, g_ascii_strdown (argn[i], -1),
				     g_strdup (argv[i]));
	}

	const char *value;

	/* We only use the size attributes to detect whether we're hidden;
	 * we'll get our real size from SetWindow.
	 */
	PRInt32 width = -1, height = -1;

	value = (const char *) g_hash_table_lookup (args, "width");
	if (value != NULL) {
		width = strtol (value, NULL, 0);
	}
	value = (const char *) g_hash_table_lookup (args, "height");
	if (value != NULL) {
		height = strtol (value, NULL, 0);
	}

	/* Are we hidden? */
	/* Treat hidden without a value as TRUE */
	mHidden = g_hash_table_lookup (args, "hidden") != NULL &&
		  GetBooleanValue (args, "hidden", PR_TRUE);

#ifdef TOTEM_GMP_PLUGIN
	if (height == 40) {
		mAudioOnly = PR_TRUE;
	}
#endif /* TOTEM_GMP_PLUGIN */
#if defined(TOTEM_NARROWSPACE_PLUGIN) || defined (TOTEM_BASIC_PLUGIN)
	if (height <= 16) {
		mAudioOnly = PR_TRUE;
	}
#endif /* TOTEM_NARROWSPACE_PLUGIN */

	/* Most for RealAudio streams, but also used as a replacement for
	 * HIDDEN=TRUE attribute.
	 */
	if (width == 0 && height == 0)
		mHidden = PR_TRUE;

	/* Whether to automatically stream and play the content */
	mAutostart = GetBooleanValue (args, "autoplay",
				      GetBooleanValue (args, "autostart", mAutostart));

	/* Whether to loop */
	mRepeat = GetBooleanValue (args, "repeat",
				   GetBooleanValue (args, "loop", PR_FALSE));

	/* Now collect URI attributes */
	const char *src = nsnull, *href = nsnull, *qtsrc = nsnull, *filename = nsnull;

	src = (const char *) g_hash_table_lookup (args, "src");
	/* DATA is only used in OBJECTs, see:
	 * http://developer.mozilla.org/en/docs/Gecko_Plugin_API_Reference:Plug-in_Basics#Plug-in_Display_Modes
	 */
	/* FIXME: this is unnecessary, since gecko will automatically a synthetic
 	 * "src" attribute with the "data" atttribute's content if "src" is missing,
 	 * see http://lxr.mozilla.org/seamonkey/source/layout/generic/nsObjectFrame.cpp#2479
 	 */
	if (!src) {
		src = (const char *) g_hash_table_lookup (args, "data");
	}
	if (src) {
		SetSrc (nsDependentCString (src));
	}

#ifdef TOTEM_GMP_PLUGIN
	/* http://windowssdk.msdn.microsoft.com/en-us/library/aa392440(VS.80).aspx */
	filename = (const char *) g_hash_table_lookup (args, "filename");
	if (!filename)
		filename = (const char *) g_hash_table_lookup (args, "url");
	if (filename) {
		SetURL (nsDependentCString (filename));
	}
#endif /* TOTEM_GMP_PLUGIN */

#ifdef TOTEM_NARROWSPACE_PLUGIN
	/* Target */
	value = (const char *) g_hash_table_lookup (args, "target");
	if (value) {
		mTarget.Assign (value);
	}

	href = (const char *) g_hash_table_lookup (args, "href");
	if (href) {
		SetHref (nsDependentCString (href));
	}

	/* http://developer.apple.com/documentation/QuickTime/QT6WhatsNew/Chap1/chapter_1_section_13.html */
	qtsrc = (const char *) g_hash_table_lookup (args, "qtsrc");
	if (qtsrc) {
		SetQtsrc (nsDependentCString (qtsrc));
	}
#endif /* TOTEM_NARROWSPACE_PLUGIN */

/* FIXME */
#if 0 //def TOTEM_MULLY_PLUGIN
	/* Click to play behaviour of the DivX plugin */
	char *previewimage = (const char *) g_hash_table_lookup (args, "previewimage");
	if (value != NULL)
		mHref = g_strdup (mSrc);
#endif /* TOTEM_NARROWSPACE_PLUGIN */

	/* If we're set to start automatically, we'll use the src stream */
	if (mRequestURI &&
	    mRequestURI == mSrcURI) {
		mExpectingStream = mAutostart;
	}

	/* Caching behaviour */
#ifdef TOTEM_NARROWSPACE_PLUGIN
	if (strcmp (mimetype, "video/quicktime") != 0) {
		mCache = PR_TRUE;
	}

	mCache = GetBooleanValue (args, "cache", mCache);

	mControllerHidden = !GetBooleanValue (args, "controller", PR_TRUE);

#endif /* TOTEM_NARROWSPACE_PLUGIN */

#if defined(TOTEM_COMPLEX_PLUGIN) && defined(HAVE_NSTARRAY_H)
	value = (const char *) g_hash_table_lookup (args, "console");
	if (value) {
		rv = SetConsole (nsDependentCString (value));
		if (NS_FAILED (rv))
			return NPERR_GENERIC_ERROR;
	}

	const char *kControls[] = {
		"All",
		"ControlPanel",
		"FFCtrl",
		"HomeCtrl",
		"ImageWindow",
		"InfoPanel",
		"InfoVolumePanel",
		"MuteCtrl",
		"MuteVolume",
		"PauseButton",
		"PlayButton",
		"PlayOnlyButton",
		"PositionField",
		"PositionSlider",
		"RWCtrl",
		"StatusBar",
		"StatusField",
		"StopButton",
		"TACCtrl",
		"VolumeSlider",
	};
	PRUint32 control = GetEnumIndex (args, "controls",
				         kControls, G_N_ELEMENTS (kControls),
					 0);
	mControls = kControls[control];

#endif /* TOTEM_COMPLEX_PLUGIN */

#ifdef TOTEM_GMP_PLUGIN
	/* uimode is either invisible, none, mini, or full
	 * http://windowssdk.msdn.microsoft.com/en-us/library/aa392439(VS.80).aspx */
	value = (char *) g_hash_table_lookup (args, "uimode");
	if (value != NULL) {
		if (g_ascii_strcasecmp (value, "none") == 0) {
			mControllerHidden = PR_TRUE;
		} else if (g_ascii_strcasecmp (value, "invisible") == 0) {
			mHidden = PR_TRUE;
		} else if (g_ascii_strcasecmp (value, "full") == 0) {
			mShowStatusbar = PR_TRUE;
		} else if (g_ascii_strcasecmp (value, "mini") == 0) {
			;
		}
	}

	/* Whether the controls are all hidden, MSIE parameter
	 * http://www.htmlcodetutorial.com/embeddedobjects/_EMBED_CONTROLLER.html */
	/* ShowXXX parameters as per http://support.microsoft.com/kb/285154 */
	mControllerHidden = !GetBooleanValue (args, "controller",
					      GetBooleanValue (args, "showcontrols", PR_TRUE));

	mShowStatusbar = GetBooleanValue (args, "showstatusbar", mShowStatusbar);
#endif /* TOTEM_GMP_PLUGIN */

	/* Whether to NOT autostart */
	//FIXME Doesn't handle playcount, or loop with numbers
	// http://www.htmlcodetutorial.com/embeddedobjects/_EMBED_LOOP.html

	//FIXME handle starttime and endtime
	// http://www.htmlcodetutorial.com/embeddedobjects/_EMBED_STARTTIME.html

#ifdef TOTEM_NARROWSPACE_PLUGIN
	/* We need to autostart if we're using an HREF
	 * otherwise the start image isn't shown */
	if (!mHref.Equals (NS_LITERAL_CSTRING (""))) {
		mExpectingStream = PR_TRUE;
		mAutostart = PR_TRUE;
	}
#endif /* TOTEM_NARROWSPACE_PLUGIN */

	/* Dump some disagnostics */
	D ("mSrc: %s", mSrc.get ());
	D ("mCache: %d", mCache);
	D ("mControllerHidden: %d", mControllerHidden);
	D ("mShowStatusbar: %d", mShowStatusbar);
	D ("mHidden: %d", mHidden);
	D ("mAudioOnly: %d", mAudioOnly);
	D ("mAutostart: %d, mRepeat: %d", mAutostart, mRepeat);
#ifdef TOTEM_NARROWSPACE_PLUGIN
	D ("mHref: %s", mHref.get ());
	D ("mTarget: %s", mTarget.get ());
#endif /* TOTEM_NARROWSPACE_PLUGIN */
#if defined(TOTEM_COMPLEX_PLUGIN) && defined(HAVE_NSTARRAY_H)
	D ("mConsole: %s", mConsole.get ());
	D ("mControls: %s", mControls.get ());
#endif /* TOTEM_COMPLEX_PLUGIN */

	g_hash_table_destroy (args);

	return ViewerFork ();
}

NPError
totemPlugin::SetWindow (NPWindow *window)
{
	if (mHidden && window->window != 0) {
		D("SetWindow: hidden, can't set window");
		return NPERR_GENERIC_ERROR;
	}

	if (mWindow != 0 &&
	    mWindow == (Window) window->window) {
		mWidth = window->width;
		mHeight = window->height;
		DD ("Window resized or moved, now %dx%d", mWidth, mHeight);
	} else if (mWindow == 0) {
		mWindow = (Window) window->window;

		mWidth = window->width;
		mHeight = window->height;

		D ("Initial window set, XID %x size %dx%d",
		   (guint) (Window) window->window, mWidth, mHeight);

		ViewerSetWindow ();
	} else {
		D ("Setting a new window != mWindow, this is unsupported!");
	}

	return NPERR_NO_ERROR;
}

NPError
totemPlugin::NewStream (NPMIMEType type,
			NPStream* stream,
			NPBool seekable,
			uint16* stype)
{
	if (!stream || !stream->url)
		return NPERR_GENERIC_ERROR;

	D ("NewStream mimetype '%s' URL '%s'", (const char *) type, stream->url);

	/* We already have a live stream */
	if (mStream) {
		D ("Already have a live stream, aborting stream");

		/* We don't just return NPERR_GENERIC_ERROR (or any other error code),
		 * since, using gecko trunk (1.9), this causes the plugin to be destroyed,
		 * if this is the automatic |src| stream. Same for the other calls below.
		 */
		return CallNPN_DestroyStreamProc (sNPN.destroystream,
						  mInstance,
						  stream,
						  NPRES_DONE);
	}

	/* Either:
	 * - this is the automatic first stream from the |src| or |data| attribute,
	 *   but we want to request a different URL, or
	 * - Gecko sometimes sends us 2 stream, and if the first is already in cache we'll
	 *   be done it before it starts the 2nd time so the "if (mStream)" check above
	 *   doesn't catch always this.
	 */
	if (!mExpectingStream) {
		D ("Not expecting a new stream; aborting stream");

		return CallNPN_DestroyStreamProc (sNPN.destroystream,
						  mInstance,
						  stream,
						  NPRES_DONE);
	}

	/* This was an expected stream, no more expected */
	mExpectingStream = PR_FALSE;

#if 1 // #if 0
	// This is fixed now _except_ the "if (!mViewerReady)" problem in StreamAsFile

	/* For now, we'll re-request the stream when the viewer is ready.
	 * As an optimisation, we could either just allow small (< ~128ko) streams
	 * (which are likely to be playlists!), or any stream and cache them to disk
	 * until the viewer is ready.
	 */
	if (!mViewerReady) {
		D ("Viewer not ready, aborting stream");

		return CallNPN_DestroyStreamProc (sNPN.destroystream,
						  mInstance,
						  stream,
						  NPRES_DONE);
	}
#endif

	if (!IsMimeTypeSupported (type, stream->url)) {
		D ("Unsupported mimetype, aborting stream");

		return CallNPN_DestroyStreamProc (sNPN.destroystream,
						  mInstance,
						  stream,
						  NPRES_DONE);
	}

	/* FIXME: assign the stream URL to mRequestURI ? */

	if (g_str_has_prefix (stream->url, "file://")) {
		*stype = NP_ASFILEONLY;
		mStreamType = NP_ASFILEONLY;
	} else {
		*stype = NP_ASFILE;
		mStreamType = NP_ASFILE;
	}

	mStream = stream;

	mCheckedForPlaylist = PR_FALSE;
	mIsPlaylist = PR_FALSE;

	/* To track how many data we get from ::Write */
	mBytesStreamed = 0;

	return NPERR_NO_ERROR;
}

NPError
totemPlugin::DestroyStream (NPStream* stream,
			    NPError reason)
{
	if (!mStream || mStream != stream)
		return NPERR_GENERIC_ERROR;

	D ("DestroyStream reason %d", reason);

	mStream = nsnull;

	int ret = close (mViewerFD);
	if (ret < 0) {
		int err = errno;
		D ("Failed to close viewer stream with errno %d: %s", err, g_strerror (err));
	}

	mViewerFD = -1;

	return NPERR_NO_ERROR;
}

int32
totemPlugin::WriteReady (NPStream *stream)
{
	/* FIXME this could probably be an assertion instead */
	if (!mStream || mStream != stream)
		return -1;

	/* Suspend the request until the viewer is ready;
	 * we'll wake up in 100ms for another try.
	 */
	if (!mViewerReady)
		return 0;

	DD ("WriteReady");

	struct pollfd fds;
	fds.events = POLLOUT;
	fds.fd = mViewerFD;
	if (poll (&fds, 1, 0) > 0)
		return (PLUGIN_STREAM_CHUNK_SIZE);

	/* suspend the request, we'll wake up in 100ms for another try */
	return 0;
}

int32
totemPlugin::Write (NPStream *stream,
		    int32 offset,
		    int32 len,
		    void *buffer)
{
	/* FIXME this could probably be an assertion instead */
	if (!mStream || mStream != stream)
		return -1;

	DD ("Write offset %d len %d", offset, len);

	/* We already know it's a playlist, just wait for it to be on-disk. */
	if (mIsPlaylist)
		return len;

	/* Check for playlist.
	 * Ideally we'd just always forward the data to the viewer and the viewer
	 * always parse the playlist itself, but that's not yet implemented.
	 */
	/* FIXME we can only look at the current buffer, not at all the data so far.
	 * So we can only do this at the start of the stream.
	 */
	if (!mCheckedForPlaylist) {
		NS_ASSERTION (offset == 0, "Not checked for playlist but not at the start of the stream!?");

		mCheckedForPlaylist = PR_TRUE;

		if (totem_pl_parser_can_parse_from_data ((const char *) buffer, len, TRUE /* FIXME */)) {
			D ("Is playlist; need to wait for the file to be downloaded completely");
			mIsPlaylist = PR_TRUE;

			/* Close the viewer */
			dbus_g_proxy_call_no_reply (mViewerProxy,
						    "CloseStream",
						    G_TYPE_INVALID,
						    G_TYPE_INVALID);

			return len;
		}
	}

	int ret = write (mViewerFD, buffer, len);
	/* FIXME shouldn't we retry if errno is EINTR ? */

	if (NS_UNLIKELY (ret < 0)) {
		int err = errno;
		D ("Write failed with errno %d: %s", err, g_strerror (err));

		if (err == EPIPE) {
			/* fd://0 got closed, probably because the backend
			 * crashed on us. Destroy the stream.
			 */
			if (CallNPN_DestroyStreamProc (sNPN.destroystream,
			    			       mInstance,
						       mStream,
						       NPRES_DONE) != NPERR_NO_ERROR) {
				g_warning ("Couldn't destroy the stream");
			}
		}
	} else /* ret >= 0 */ {
		DD ("Wrote %d bytes", ret);

		mBytesStreamed += ret;
	}

	return ret;
}

void
totemPlugin::StreamAsFile (NPStream *stream,
			   const char* fname)
{
	if (!mStream || mStream != stream)
		return;

	D ("StreamAsFile filename '%s'", fname);

	/* FIXME: this reads the whole file into memory... try to
	 * find a way to avoid it.
	 */
	if (!mCheckedForPlaylist) {
		mIsPlaylist = totem_pl_parser_can_parse_from_filename
				(fname, TRUE) != FALSE;
	}

	/* FIXME! This happens when we're using the automatic |src| stream and
	 * it finishes before we're ready.
	 */
	if (!mViewerReady) {
		D ("Viewer not ready yet, deferring SetLocalFile");
		return;
	}

	NS_ASSERTION (mViewerProxy, "No viewer proxy");
	NS_ASSERTION (mViewerReady, "Viewer not ready");

	NS_ENSURE_TRUE (mRequestBaseURI && mRequestURI, );

	nsCString baseSpec, spec;
	mRequestBaseURI->GetSpec (baseSpec);
	mRequestURI->GetSpec (spec);

	/* FIXME: these calls need to be async!!
	 * But the file may be unlinked as soon as we return from this
	 * function... do we need to keep a link?
	 */
	gboolean retval = TRUE;
	GError *error = NULL;
	if (mIsPlaylist) {
		retval = dbus_g_proxy_call (mViewerProxy,
					    "SetPlaylist",
					    &error,
					    G_TYPE_STRING, fname,
					    G_TYPE_STRING, spec.get (),
					    G_TYPE_STRING, baseSpec.get (),
					    G_TYPE_INVALID,
					    G_TYPE_INVALID);
	}
	/* Only call SetLocalFile if we haven't already streamed the file!
	 * (It happens that we get no ::Write calls if the file is
	 * completely in the cache.)
	 */
	else if (mBytesStreamed == 0) {
		retval = dbus_g_proxy_call (mViewerProxy,
					    "SetLocalFile",
					    &error,
					    G_TYPE_STRING, fname,
					    G_TYPE_STRING, spec.get (),
					    G_TYPE_STRING, baseSpec.get (),
					    G_TYPE_INVALID,
					    G_TYPE_INVALID);
	} else {
		D ("mBytesStreamed %u", mBytesStreamed);
	}

	if (!retval) {
		g_warning ("Viewer error: %s", error->message);
		g_error_free (error);
	}
}
    
void
totemPlugin::URLNotify (const char *url,
		        NPReason reason,
		        void *notifyData)
{
	D ("URLNotify URL '%s' reason %d", url ? url : "", reason);

	/* If we get called when we expect a stream,
	 * it means that the stream failed.
	 */
	if (mExpectingStream) {
		if (reason == NPRES_NETWORK_ERR) {
			dbus_g_proxy_call (mViewerProxy,
					   "SetErrorLogo",
					   NULL,
					   G_TYPE_INVALID,
					   G_TYPE_INVALID);
		} else if (reason != NPRES_DONE) {
			D ("Failed to get stream");
			/* FIXME: show error to user? */
		}

		mExpectingStream = PR_FALSE;
	}
}

NPError
totemPlugin::GetScriptable (void *_retval)
{
	D ("GetScriptable [%p]", (void*) this);

#if defined(TOTEM_COMPLEX_PLUGIN) && defined(HAVE_NSTARRAY_H)
	if (mConsoleClassRepresentant) {
		return mConsoleClassRepresentant->GetScriptable (_retval);
	}
#endif /* TOTEM_COMPLEX_PLUGIN */

	if (!mScriptable) {
		mScriptable = new totemScriptablePlugin (this);
		if (!mScriptable)
			return NPERR_OUT_OF_MEMORY_ERROR;

		NS_ADDREF (mScriptable);
	}

	nsresult rv = mScriptable->QueryInterface (NS_GET_IID (nsISupports),
						   NS_REINTERPRET_CAST (void **, _retval));

	return NS_SUCCEEDED (rv) ? NPERR_NO_ERROR : NPERR_GENERIC_ERROR;
}

/* static */ NPError
totemPlugin::Initialise ()
{
#if defined(TOTEM_COMPLEX_PLUGIN) && defined(HAVE_NSTARRAY_H)
	sPlugins = new nsTArray<totemPlugin*> (32);
	if (!sPlugins)
		return NPERR_OUT_OF_MEMORY_ERROR;
#endif /* TOTEM_COMPLEX_PLUGIN */

	return NPERR_NO_ERROR;
}

/* static */ NPError
totemPlugin::Shutdown ()
{
#if defined(TOTEM_COMPLEX_PLUGIN) && defined(HAVE_NSTARRAY_H)
	if (sPlugins) {
		if (!sPlugins->IsEmpty ()) {
			D ("WARNING: sPlugins not empty on shutdown, count: %d", sPlugins->Length ());
		}

		delete sPlugins;
		sPlugins = nsnull;
	}
#endif /* TOTEM_COMPLEX_PLUGIN */

	return NPERR_NO_ERROR;
}
