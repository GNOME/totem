/* Totem Mozilla plugin
 * 
 * Copyright © 2004-2006 Bastien Nocera <hadess@hadess.net>
 * Copyright © 2002 David A. Schleef <ds@schleef.org>
 * Copyright © 2006, 2007, 2008 Christian Persch
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
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301  USA.
 */

#include <config.h>

#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/poll.h>
#include <sys/wait.h>

#include <glib.h>

#include "totem-pl-parser-mini.h"
#include "totem-plugin-viewer-options.h"
#include "marshal.h"

#include "npapi.h"
#include "npruntime.h"
#include "npupp.h"

#ifdef G_HAVE_ISO_VARARGS
#define D(m, ...) g_debug ("%p: "#m, this, __VA_ARGS__)
#elif defined(G_HAVE_GNUC_VARARGS)
#define D(m, x...) g_debug ("%p: "#m, this, x)
#endif
#define Dm(m) g_debug ("%p: "#m, this)

// Really noisy messages; let's noop them for now
#ifdef G_HAVE_ISO_VARARGS
#define DD(...)
#elif defined(G_HAVE_GNUC_VARARGS)
#define DD(x...)
#endif

#include "totemPlugin.h"

#if defined(TOTEM_GMP_PLUGIN)
#include "totemGMPControls.h"
#include "totemGMPNetwork.h"
#include "totemGMPPlayer.h"
#include "totemGMPSettings.h"
#elif defined(TOTEM_COMPLEX_PLUGIN)
#include "totemComplexPlugin.h"
#elif defined(TOTEM_NARROWSPACE_PLUGIN)
#include "totemNarrowSpacePlugin.h"
#elif defined(TOTEM_MULLY_PLUGIN)
#include "totemMullYPlugin.h"
#elif defined(TOTEM_CONE_PLUGIN)
#include "totemCone.h"
#include "totemConeAudio.h"
#include "totemConeInput.h"
#include "totemConePlaylist.h"
#include "totemConePlaylistItems.h"
#include "totemConeVideo.h"
#else
#error Unknown plugin type
#endif

#define DASHES "--"

/* How much data bytes to request */
#define PLUGIN_STREAM_CHUNK_SIZE (8 * 1024)

static const totemPluginMimeEntry kMimeTypes[] = {
#if defined(TOTEM_GMP_PLUGIN)
  { "application/x-mplayer2", "avi, wma, wmv", "video/x-msvideo" },
  { "video/x-ms-asf-plugin", "asf, wmv", "video/x-ms-asf" },
  { "video/x-msvideo", "asf, wmv", NULL },
  { "video/x-ms-asf", "asf", NULL },
  { "video/x-ms-wmv", "wmv", "video/x-ms-wmv" },
  { "video/x-wmv", "wmv", "video/x-ms-wmv" },
  { "video/x-ms-wvx", "wmv", "video/x-ms-wmv" },
  { "video/x-ms-wm", "wmv", "video/x-ms-wmv" },
  { "video/x-ms-wmp", "wmv", "video/x-ms-wmv" },
  { "application/x-ms-wms", "wms", "video/x-ms-wmv" },
  { "application/x-ms-wmp", "wmp", "video/x-ms-wmv" },
  { "application/asx", "asx", "audio/x-ms-asx" },
  { "audio/x-ms-wma", "wma", NULL }
#elif defined(TOTEM_COMPLEX_PLUGIN)
  { "audio/x-pn-realaudio-plugin", "rpm", "audio/vnd.rn-realaudio" },
#elif defined(TOTEM_NARROWSPACE_PLUGIN)
  { "video/quicktime", "mov", NULL },
  { "video/mp4", "mp4", NULL },
  { "image/x-macpaint", "pntg", NULL },
  { "image/x-quicktime", "pict, pict1, pict2", "image/x-pict" },
  { "video/x-m4v", "m4v", NULL },
#elif defined(TOTEM_MULLY_PLUGIN)
  { "video/divx", "divx", "video/x-msvideo" },
#elif defined(TOTEM_CONE_PLUGIN)
  { "application/x-vlc-plugin", "", "VLC Multimedia Plugin" },
  { "application/vlc", "", "VLC Multimedia Plugin" },
  { "video/x-google-vlc-plugin", "", "VLC Multimedia Plugin" },
  { "application/x-ogg","ogg","application/ogg" },
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
  { "application/x-totem-plugin", "", "Totem Multimedia plugin" },
  { "audio/midi", "mid, midi", NULL },
#else
#error Unknown plugin type
#endif
};

static const char kPluginDescription[] =
#if defined(TOTEM_GMP_PLUGIN)
  "Windows Media Player Plug-in 10 (compatible; Totem)";
#elif defined(TOTEM_COMPLEX_PLUGIN)
  "Helix DNA Plugin: RealPlayer G2 Plug-In Compatible (compatible; Totem)";
#elif defined(TOTEM_NARROWSPACE_PLUGIN)
  "QuickTime Plug-in " TOTEM_NARROWSPACE_VERSION;
#elif defined(TOTEM_MULLY_PLUGIN)
  "DivX\xC2\xAE Web Player";
#elif defined(TOTEM_CONE_PLUGIN)
  "VLC Multimedia Plugin (compatible Totem " VERSION ")";
#else
#error Unknown plugin type
#endif

static const char kPluginLongDescription[] =
#if defined(TOTEM_MULLY_PLUGIN)
  "DivX Web Player version " TOTEM_MULLY_VERSION;
#else
  "The <a href=\"http://www.gnome.org/projects/totem/\">Totem</a> " PACKAGE_VERSION " plugin handles video and audio streams.";
#endif

static const char kPluginUserAgent[] =
#if defined(TOTEM_NARROWSPACE_PLUGIN)
  "Quicktime/"TOTEM_NARROWSPACE_VERSION;
#elif defined(TOTEM_GMP_PLUGIN)
  "Windows-Media-Player/10.00.00.4019";
#else
  "";
#endif

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
totemPlugin::operator new (size_t aSize) throw ()
{
	void *object = ::operator new (aSize);
	if (object) {
		memset (object, 0, aSize);
	}

	return object;
}

totemPlugin::totemPlugin (NPP aNPP)
:	mNPP (aNPP),
        mMimeType (NULL),
        mBaseURI (NULL),
        mSrcURI (NULL),
        mRequestBaseURI (NULL),
        mRequestURI (NULL),
        mViewerBusAddress (NULL),
        mViewerServiceName (NULL),
	mViewerFD (-1),
	mWidth (-1),
	mHeight (-1),
#ifdef TOTEM_COMPLEX_PLUGIN
	mAutoPlay (false),
#else
	mAutoPlay (true),
#endif /* TOTEM_NARROWSPACE_PLUGIN */
	mNeedViewer (true),
	mState (TOTEM_STATE_STOPPED)
{
        TOTEM_LOG_CTOR ();

#if defined(TOTEM_COMPLEX_PLUGIN) && defined(HAVE_NSTARRAY_H)
	/* Add |this| to the global plugins list */
	assert (sPlugins->IndexOf (this) == NoIndex); //, "WTF?");
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

        /* FIXMEchpe invalidate the scriptable object, or is that done automatically? */

	if (mBusProxy) {
		dbus_g_proxy_disconnect_signal (mBusProxy,
						"NameOwnerChanged",
						G_CALLBACK (NameOwnerChangedCallback),
						reinterpret_cast<void*>(this));
		g_object_unref (mBusProxy);
		mBusProxy = NULL;
	}

	ViewerCleanup ();

	if (mTimerID != 0) {
                g_source_remove (mTimerID);
                mTimerID = 0;
	}

#ifdef TOTEM_GMP_PLUGIN
	g_free (mURLURI);
#endif

#ifdef TOTEM_NARROWSPACE_PLUGIN
        g_free (mHref);
        g_free (mTarget);
        g_free (mHrefURI);
        g_free (mQtsrcURI);
#endif

        g_free (mMimeType);

        g_free (mSrcURI);
        g_free (mBaseURI);
        g_free (mRequestURI);
        g_free (mRequestBaseURI);

        g_free (mViewerBusAddress);
        g_free (mViewerServiceName);

        g_free (mBackgroundColor);
        g_free (mMatrix);
        g_free (mRectangle);
        g_free (mMovieName);

        TOTEM_LOG_DTOR ();
}

/* public functions */

void
totemPlugin::Command (const char *aCommand)
{
	D ("Command '%s'", aCommand);

	/* FIXME: queue the action instead */
	if (!mViewerReady)
		return;

	assert (mViewerProxy);
	dbus_g_proxy_call_no_reply (mViewerProxy,
				    "DoCommand",
				    G_TYPE_STRING, aCommand,
				    G_TYPE_INVALID,
				    G_TYPE_INVALID);
}

void
totemPlugin::SetVolume (double aVolume)
{
	D ("SetVolume '%f'", aVolume);

        mVolume = CLAMP (aVolume, 0.0, 1.0);

	/* FIXME: queue the action instead */
	if (!mViewerReady)
		return;

	assert (mViewerProxy);
	dbus_g_proxy_call_no_reply (mViewerProxy,
				    "SetVolume",
				    G_TYPE_DOUBLE, gdouble (Volume()),
				    G_TYPE_INVALID,
				    G_TYPE_INVALID);
}

void
totemPlugin::SetFullscreen (bool enabled)
{
	D ("SetFullscreen '%d'", enabled);

        mIsFullscreen = enabled;

	/* FIXME: queue the action instead */
	if (!mViewerReady)
		return;

	assert (mViewerProxy);
	dbus_g_proxy_call_no_reply (mViewerProxy,
				    "SetFullscreen",
				    G_TYPE_BOOLEAN, gboolean (IsFullscreen()),
				    G_TYPE_INVALID,
				    G_TYPE_INVALID);
}

void
totemPlugin::ClearPlaylist ()
{
	Dm ("ClearPlaylist");

	/* FIXME: queue the action instead */
	if (!mViewerReady)
		return;

	assert (mViewerProxy);
	dbus_g_proxy_call_no_reply (mViewerProxy,
				    "ClearPlaylist",
				    G_TYPE_INVALID,
				    G_TYPE_INVALID);
}

int32_t
totemPlugin::AddItem (const NPString& aURI)
{
        if (!aURI.UTF8Characters || !aURI.UTF8Length)
                return -1;

        /* FIXMEchpe: resolve against mBaseURI or mSrcURI ?? */

	/* FIXME: queue the action instead */
	if (!mViewerReady)
		return false;

	assert (mViewerProxy);

        char *uri = g_strndup (aURI.UTF8Characters, aURI.UTF8Length);
	D ("AddItem '%s'", uri);

        dbus_g_proxy_call_no_reply (mViewerProxy,
				    "AddItem",
				    G_TYPE_STRING, uri,
				    G_TYPE_INVALID,
				    G_TYPE_INVALID);
        g_free (uri);

	return 0;
}

void
totemPlugin::SetMute (bool enabled)
{
  mMute = enabled;
  /* FIXMEchpe do stuff in the viewer! */
}

void
totemPlugin::SetLooping (bool enabled)
{
  mIsLooping = enabled;
  /* FIXMEchpe do stuff in the viewer! */
}

void
totemPlugin::SetAutoPlay (bool enabled)
{
  mAutoPlay = enabled;
}

void
totemPlugin::SetControllerVisible (bool enabled)
{
  mControllerHidden = !enabled; // FIXMEchpe
}

void
totemPlugin::SetBackgroundColor (const NPString& color)
{
  g_free (mBackgroundColor);
  mBackgroundColor = g_strndup (color.UTF8Characters, color.UTF8Length);
}

void
totemPlugin::SetMatrix (const NPString& matrix)
{
  g_free (mMatrix);
  mMatrix = g_strndup (matrix.UTF8Characters, matrix.UTF8Length);
}

void
totemPlugin::SetRectangle (const NPString& rectangle)
{
  g_free (mRectangle);
  mRectangle = g_strndup (rectangle.UTF8Characters, rectangle.UTF8Length);
}

void
totemPlugin::SetMovieName (const NPString& name)
{
  g_free (mMovieName);
  mMovieName = g_strndup (name.UTF8Characters, name.UTF8Length);
}

void
totemPlugin::SetRate (double rate)
{
  // FIXMEchpe
}

double
totemPlugin::Rate () const
{
  double rate;
  if (mState == TOTEM_STATE_PLAYING) {
    rate = 1.0;
  } else {
    rate = 0.0;
  }
  return rate;
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

	const char *userAgent = kPluginUserAgent;

	if (*kPluginUserAgent == '\0') {
		userAgent = NPN_UserAgent (mNPP);
		if (!userAgent) {
			/* See https://bugzilla.mozilla.org/show_bug.cgi?id=328778 */
			Dm ("User agent has more than 127 characters; fix your browser!");
		}
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
#if defined(TOTEM_GMP_PLUGIN)
	g_ptr_array_add (arr, g_strdup ("gmp"));
#elif defined(TOTEM_COMPLEX_PLUGIN)
	g_ptr_array_add (arr, g_strdup ("complex"));
#elif defined(TOTEM_NARROWSPACE_PLUGIN)
	g_ptr_array_add (arr, g_strdup ("narrowspace"));
#elif defined(TOTEM_MULLY_PLUGIN)
	g_ptr_array_add (arr, g_strdup ("mully"));
#elif defined(TOTEM_CONE_PLUGIN)
	g_ptr_array_add (arr, g_strdup ("cone"));
#else
#error Unknown plugin type
#endif

	if (userAgent) {
		g_ptr_array_add (arr, g_strdup (DASHES TOTEM_OPTION_USER_AGENT));
		g_ptr_array_add (arr, g_strdup (userAgent));
	}

	/* FIXME: remove this */
	if (mMimeType) {
		g_ptr_array_add (arr, g_strdup (DASHES TOTEM_OPTION_MIMETYPE));
		g_ptr_array_add (arr, g_strdup (mMimeType));
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

	if (!mAutoPlay) {
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

	mViewerReady = false;

	/* Don't wait forever! */
	const guint kViewerTimeout = 30; /* seconds */
        mTimerID = g_timeout_add_seconds (kViewerTimeout,
					  (GSourceFunc) ViewerForkTimeoutCallback,
					  reinterpret_cast<void*>(this));

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

	mViewerSetUp = true;

	Dm ("ViewerSetup");

	/* Cancel timeout */
        if (mTimerID != 0) {
          g_source_remove (mTimerID);
          mTimerID = 0;
	}

	mViewerProxy = dbus_g_proxy_new_for_name (mBusConnection,
						  mViewerServiceName,
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
				     reinterpret_cast<void*>(this),
				     NULL);

	dbus_g_proxy_add_signal (mViewerProxy,
				 "StopStream",
				 G_TYPE_INVALID);
	dbus_g_proxy_connect_signal (mViewerProxy,
				     "StopStream",
				     G_CALLBACK (StopStreamCallback),
				     reinterpret_cast<void*>(this),
				     NULL);

	dbus_g_object_register_marshaller
		(totempluginviewer_marshal_VOID__UINT_UINT_STRING,
		 G_TYPE_NONE, G_TYPE_UINT, G_TYPE_UINT, G_TYPE_STRING, G_TYPE_INVALID);
	dbus_g_proxy_add_signal (mViewerProxy,
				 "Tick",
				 G_TYPE_UINT,
				 G_TYPE_UINT,
				 G_TYPE_STRING,
				 G_TYPE_INVALID);
	dbus_g_proxy_connect_signal (mViewerProxy,
				     "Tick",
				     G_CALLBACK (TickCallback),
				     reinterpret_cast<void*>(this),
				     NULL);

	dbus_g_object_register_marshaller
		(totempluginviewer_marshal_VOID__STRING_BOXED,
		 G_TYPE_NONE, G_TYPE_STRING, G_TYPE_BOXED, G_TYPE_INVALID);
	dbus_g_proxy_add_signal (mViewerProxy,
				 "PropertyChange",
				 G_TYPE_STRING,
				 G_TYPE_VALUE,
				 G_TYPE_INVALID);
	dbus_g_proxy_connect_signal (mViewerProxy,
				     "PropertyChange",
				     G_CALLBACK (PropertyChangeCallback),
				     reinterpret_cast<void*>(this),
				     NULL);

	if (mHidden) {
		ViewerReady ();
	} else {
		ViewerSetWindow ();
	}

#if defined(TOTEM_COMPLEX_PLUGIN) && defined(HAVE_NSTARRAY_H)
	/* Notify all consoles */
	uint32_t count = sPlugins->Length ();
	for (uint32_t i = 0; i < count; ++i) {
		totemPlugin *plugin = sPlugins->ElementAt (i);

		if (plugin->mConsoleClassRepresentant == this)
			plugin->UnownedViewerSetup ();
	}
#endif /* TOTEM_COMPLEX_PLUGIN */
}


void
totemPlugin::ViewerCleanup ()
{
	mViewerReady = false;

        g_free (mViewerBusAddress);
        mViewerBusAddress = NULL;
        g_free (mViewerServiceName);
        mViewerServiceName = NULL;

	if (mViewerPendingCall) {
		dbus_g_proxy_cancel_call (mViewerProxy, mViewerPendingCall);
		mViewerPendingCall = NULL;
	}

	if (mViewerProxy) {
		dbus_g_proxy_disconnect_signal (mViewerProxy,
						"ButtonPress",
						G_CALLBACK (ButtonPressCallback),
						reinterpret_cast<void*>(this));
		dbus_g_proxy_disconnect_signal (mViewerProxy,
						"StopStream",
						G_CALLBACK (StopStreamCallback),
						reinterpret_cast<void*>(this));
		dbus_g_proxy_disconnect_signal (mViewerProxy,
						"Tick",
						G_CALLBACK (TickCallback),
						reinterpret_cast<void*>(this));
		dbus_g_proxy_disconnect_signal (mViewerProxy,
						"PropertyChange",
						G_CALLBACK (PropertyChangeCallback),
						reinterpret_cast<void*>(this));

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
		Dm ("No viewer proxy yet, deferring SetWindow");
		return;
	}

	/* FIXME this shouldn't happen here */
	if (mHidden) {
		mWindowSet = true;
		ViewerReady ();
		return;
	}

	assert (mViewerPendingCall == NULL); /* Have a pending call */

	Dm ("Calling SetWindow");
	mViewerPendingCall = 
		dbus_g_proxy_begin_call (mViewerProxy,
					 "SetWindow",
					 ViewerSetWindowCallback,
					 reinterpret_cast<void*>(this),
					 NULL,
#if defined(TOTEM_COMPLEX_PLUGIN) && defined(HAVE_NSTARRAY_H)
					 G_TYPE_STRING, mControls.get (),
#else
					 G_TYPE_STRING, "All",
#endif
					 G_TYPE_UINT, (guint) mWindow,
					 G_TYPE_INT, mWidth,
					 G_TYPE_INT, mHeight,
					 G_TYPE_INVALID);
		
	mWindowSet = true;
}

void
totemPlugin::ViewerReady ()
{
	Dm ("ViewerReady");

	assert (!mViewerReady);

	mViewerReady = true;

	if (mAutoPlay) {
		RequestStream (false);
	} else {
		mWaitingForButtonPress = true;
	}

#ifdef TOTEM_NARROWSPACE_PLUGIN
	/* Tell the viewer it has an href */
	if (mHref) {
		dbus_g_proxy_call_no_reply (mViewerProxy,
					    "SetHref",
					    G_TYPE_STRING, mHref,
					    G_TYPE_STRING, mTarget ? mTarget : "",
					    G_TYPE_INVALID);
	}
	if (mHref && mAutoHref)
		ViewerButtonPressed (0, 0);
#endif /* TOTEM_NARROWSPACE_PLUGIN */
}

void
totemPlugin::ViewerButtonPressed (guint aTimestamp, guint aButton)
{
	Dm ("ButtonPress");

#ifdef TOTEM_NARROWSPACE_PLUGIN
	/* FIXME set href="" afterwards, so we don't try to launch again when the user clicks again? */
	if (mHref) {
		if (mTarget &&
                    g_ascii_strcasecmp (mTarget, "quicktimeplayer") == 0) {
			D ("Opening movie '%s' in external player", mHref);
			dbus_g_proxy_call_no_reply (mViewerProxy,
						    "LaunchPlayer",
						    G_TYPE_STRING, mHref,
						    G_TYPE_UINT, time,
						    G_TYPE_INVALID);
			return;
		}
                if (mTarget &&
                    (g_ascii_strcasecmp (mTarget, "myself") == 0 ||
                     g_ascii_strcasecmp (mTarget, "_current") == 0 ||
                     g_ascii_strcasecmp (mTarget, "_self") == 0)) {
                        D ("Opening movie '%s'", mHref);
			dbus_g_proxy_call_no_reply (mViewerProxy,
						    "SetHref",
						    G_TYPE_STRING, NULL,
						    G_TYPE_STRING, NULL,
						    G_TYPE_INVALID);
			/* FIXME this isn't right, we should just create a mHrefURI and instruct to load that one */
			SetQtsrc (mHref);
			RequestStream (true);
			return;
		}

		/* Load URL in browser. This will either open a new website,
		 * or execute some javascript.
		 */
                const char *href = NULL;
		if (mHrefURI) {
			href = mHrefURI;
		} else {
			href = mHref;
		}

		/* By default, an empty target will make the movie load
		 * inside our existing instance, so use a target to be certain
		 * it opens in the current frame, as before */
		if (NPN_GetURL (mNPP, href, mTarget ? mTarget : "_current") != NPERR_NO_ERROR) {
			D ("Failed to launch URL '%s' in browser", mHref);
		}

		return;
	}
#endif

	if (!mWaitingForButtonPress)
		return;

	mWaitingForButtonPress = false;

	/* Now is the time to start the stream */
	if (!mAutoPlay &&
	    !mStream) {
		RequestStream (false);
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
	if (G_UNLIKELY (!mViewerServiceName)) {
		mViewerServiceName = g_strdup_printf (TOTEM_PLUGIN_VIEWER_NAME_TEMPLATE, mViewerPID);
		D ("Viewer DBus interface name is '%s'", mViewerServiceName);
	}

	if (strcmp (mViewerServiceName, aName) != 0)
		return;

	D ("NameOwnerChanged old-owner '%s' new-owner '%s'", aOldOwner, aNewOwner);

	if (aOldOwner[0] == '\0' /* empty */ &&
	    aNewOwner[0] != '\0' /* non-empty */) {
		if (mViewerBusAddress &&
                    strcmp (mViewerBusAddress, aNewOwner) == 0) {
			Dm ("Already have owner, why are we notified again?");
                        g_free (mViewerBusAddress);
		} else if (mViewerBusAddress) {
			Dm ("WTF, new owner!?");
                        g_free (mViewerBusAddress);
		} else {
			/* This is the regular case */
			Dm ("Viewer now connected to the bus");
		}

		mViewerBusAddress = g_strdup (aNewOwner);

		ViewerSetup ();
	} else if (mViewerBusAddress &&
		   strcmp (mViewerBusAddress, aOldOwner) == 0) {
		Dm ("Viewer lost connection!");

		g_free (mViewerBusAddress);
                mViewerBusAddress = NULL;

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
  g_free (mRequestURI);
  mRequestURI = NULL;

  g_free (mRequestBaseURI);
  mRequestBaseURI = NULL;
}

void
totemPlugin::RequestStream (bool aForceViewer)
{
	D ("Stream requested (force viewer: %d)", aForceViewer);

//        assert (mViewerReady);
        if (!mViewerReady)
          return;//FIXMEchpe

	if (mStream) {
		Dm ("Unexpectedly have a stream!");
		/* FIXME cancel existing stream, schedule new timer to try again */
		return;
	}

	ClearRequest ();

	/* Now work out which URL to request */
	const char *baseURI = NULL;
	const char *requestURI = NULL;

#ifdef TOTEM_GMP_PLUGIN
	/* Prefer filename over src */
	if (mURLURI) {
		requestURI = mURLURI;
		baseURI = mSrcURI; /* FIXME: that correct? */
	}
#endif /* TOTEM_GMP_PLUGIN */

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
#endif /* TOTEM_NARROWSPACE_PLUGIN */

#ifdef TOTEM_MULLY_PLUGIN
	aForceViewer = true;
#endif /* TOTEM_MULLY_PLUGIN */

	/* Fallback */
	if (!requestURI)
		requestURI = mSrcURI;

	if (!baseURI)
		baseURI = mBaseURI;

	/* Nothing to do */
	if (!requestURI || !requestURI[0])
		return;

	/* If we don't have a proxy yet */
	if (!mViewerReady)
		return;

        mRequestURI = g_strdup (requestURI);
        mRequestBaseURI = g_strdup (baseURI);

	/* If the URL is supported and the caller isn't asking us to make
	 * the viewer open the stream, we call OpenStream, and
	 * otherwise OpenURI. */
	if (!aForceViewer && IsSchemeSupported (requestURI, baseURI)) {
		/* This will fail for the 2nd stream, but we shouldn't
		 * ever come to using it for the 2nd stream... */

		mViewerPendingCall =
			dbus_g_proxy_begin_call (mViewerProxy,
						 "OpenStream",
						 ViewerOpenStreamCallback,
						 reinterpret_cast<void*>(this),
						 NULL,
						 G_TYPE_STRING, requestURI,
						 G_TYPE_STRING, baseURI,
						 G_TYPE_INVALID);
	} else {
		mViewerPendingCall =
			dbus_g_proxy_begin_call (mViewerProxy,
						 "OpenURI",
						 ViewerOpenURICallback,
						 reinterpret_cast<void*>(this),
						 NULL,
						 G_TYPE_STRING, requestURI,
						 G_TYPE_STRING, baseURI,
						 G_TYPE_INVALID);
	}

	/* FIXME: start playing in the callbacks ! */

#ifdef TOTEM_NARROWSPACE_PLUGIN
        if (!mNPObjects[ePluginScriptable].IsNull ()) {
                NPObject *object = mNPObjects[ePluginScriptable];
                totemNarrowSpacePlayer *scriptable = static_cast<totemNarrowSpacePlayer*>(object);
		scriptable->mPluginState = totemNarrowSpacePlayer::eState_Playable;
	}
#endif /* TOTEM_NARROWSPACE_PLUGIN */
#ifdef TOTEM_GMP_PLUGIN
        if (!mNPObjects[ePluginScriptable].IsNull ()) {
                NPObject *object = mNPObjects[ePluginScriptable];
                totemGMPPlayer *scriptable = static_cast<totemGMPPlayer*>(object);
		scriptable->mPluginState = totemGMPPlayer::eState_Waiting;
	}
#endif /* TOTEM_GMP_PLUGIN */
}

void
totemPlugin::UnsetStream ()
{
	if (!mStream)
		return;

	if (NPN_DestroyStream (mNPP,
                               mStream,
                               NPRES_DONE) != NPERR_NO_ERROR) {
                  g_warning ("Couldn't destroy the stream");
                  /* FIXME: set mStream to NULL here too? */
                  return;
	}

	/* Calling DestroyStream should already have set this to NULL */
	assert (!mStream); /* Should not have a stream anymore */
	mStream = NULL; /* just to make sure */

#ifdef TOTEM_NARROWSPACE_PLUGIN
        if (!mNPObjects[ePluginScriptable].IsNull ()) {
                NPObject *object = mNPObjects[ePluginScriptable];
                totemNarrowSpacePlayer *scriptable = static_cast<totemNarrowSpacePlayer*>(object);
		scriptable->mPluginState = totemNarrowSpacePlayer::eState_Waiting;
	}
#endif /* TOTEM_NARROWSPACE_PLUGIN */
#ifdef TOTEM_GMP_PLUGIN
        if (!mNPObjects[ePluginScriptable].IsNull ()) {
                NPObject *object = mNPObjects[ePluginScriptable];
                totemGMPPlayer *scriptable = static_cast<totemGMPPlayer*>(object);
		scriptable->mPluginState = totemGMPPlayer::eState_MediaEnded;
	}
#endif /* TOTEM_GMP_PLUGIN */
}

/* Callbacks */

/* static */ void 
totemPlugin::NameOwnerChangedCallback (DBusGProxy *proxy,
				       const char *aName,
				       const char *aOldOwner,
				       const char *aNewOwner,
				       void *aData)
{
	totemPlugin *plugin = reinterpret_cast<totemPlugin*>(aData);

	plugin->NameOwnerChanged (aName, aOldOwner, aNewOwner);
}

/* static */ gboolean
totemPlugin::ViewerForkTimeoutCallback (void *aData)
{
	totemPlugin *plugin = reinterpret_cast<totemPlugin*>(aData);

        plugin->mTimerID = 0;

	g_debug ("ViewerForkTimeoutCallback");

	/* FIXME: can this really happen? */
	assert (!plugin->mViewerReady); /* Viewer ready but timeout running? */

	plugin->ViewerCleanup ();
	/* FIXME start error viewer */

        return FALSE; /* don't run again */
}

/* static */ void
totemPlugin::ButtonPressCallback (DBusGProxy *proxy,
				  guint aTimestamp,
				  guint aButton,
			          void *aData)
{
	totemPlugin *plugin = reinterpret_cast<totemPlugin*>(aData);

	g_debug ("ButtonPress signal received");

	plugin->ViewerButtonPressed (aTimestamp, aButton);
}

/* static */ void
totemPlugin::StopStreamCallback (DBusGProxy *proxy,
			         void *aData)
{
	totemPlugin *plugin = reinterpret_cast<totemPlugin*>(aData);

	g_debug ("StopStream signal received");

	plugin->UnsetStream ();
}

/* static */ void
totemPlugin::TickCallback (DBusGProxy *proxy,
			   guint aTime,
			   guint aDuration,
			   char *aState,
			   void *aData)
{
	totemPlugin *plugin = reinterpret_cast<totemPlugin*>(aData);
	guint i;

	//assert (aState != NULL) /* aState is NULL probably garbage */
        if (!aState)
                return;

	DD ("Tick signal received, aState %s, aTime %d, aDuration %d", aState, aTime, aDuration);

	for (i = 0; i < TOTEM_STATE_INVALID; i++) {
		if (strcmp (aState, totem_states[i]) == 0) {
			plugin->mState = (TotemStates) i;
			break;
		}
	}

	plugin->mTime = aTime;
	plugin->mDuration = aDuration;

#ifdef TOTEM_GMP_PLUGIN
        if (!plugin->mNPObjects[ePluginScriptable].IsNull ()) {
                NPObject *object = plugin->mNPObjects[ePluginScriptable];
                totemGMPPlayer *scriptable = static_cast<totemGMPPlayer*>(object);
                switch (plugin->mState) {
		case TOTEM_STATE_PLAYING:
			scriptable->mPluginState = totemGMPPlayer::eState_Playing;
			break;
		case TOTEM_STATE_PAUSED:
			scriptable->mPluginState = totemGMPPlayer::eState_Paused;
			break;
		case TOTEM_STATE_STOPPED:
			scriptable->mPluginState = totemGMPPlayer::eState_Stopped;
			break;
		default:
			scriptable->mPluginState = totemGMPPlayer::eState_Undefined;
		}
	}
#endif /* TOTEM_GMP_PLUGIN */
}

/* static */ void
totemPlugin::PropertyChangeCallback (DBusGProxy  *proxy,
				     const char *aType,
				     GValue *value,
				     void *aData)
{
	totemPlugin *plugin = reinterpret_cast<totemPlugin*>(aData);

	//NS_ASSERTION (aType != NULL, "aType is NULL probably garbage");
        if (!aType)
                return;

	DD ("PropertyChange signal received, aType %s", aType);

	if (strcmp (aType, TOTEM_PROPERTY_VOLUME) == 0) {
		plugin->mVolume = g_value_get_double (value);
	} else if (strcmp (aType, TOTEM_PROPERTY_ISFULLSCREEN) == 0) {
		plugin->mIsFullscreen = g_value_get_boolean (value);
	}
}

/* static */ void
totemPlugin::ViewerSetWindowCallback (DBusGProxy *aProxy,
				      DBusGProxyCall *aCall,
				      void *aData)
{
	totemPlugin *plugin = reinterpret_cast<totemPlugin*>(aData);

	g_debug ("SetWindow reply");

	//assert (aCall == plugin->mViewerPendingCall, "SetWindow not the current call");
        if (aCall != plugin->mViewerPendingCall)
          return;

	plugin->mViewerPendingCall = NULL;

	GError *error = NULL;
	if (!dbus_g_proxy_end_call (aProxy, aCall, &error, G_TYPE_INVALID)) {
		/* FIXME: mViewerFailed = true */
		g_warning ("SetWindow failed: %s", error->message);
		g_error_free (error);
		return;
	}

	plugin->ViewerReady ();
}

/* static */ void
totemPlugin::ViewerOpenStreamCallback (DBusGProxy *aProxy,
				       DBusGProxyCall *aCall,
				       void *aData)
{
	totemPlugin *plugin = reinterpret_cast<totemPlugin*>(aData);

	g_debug ("OpenStream reply");

// 	assert (aCall == plugin->mViewerPendingCall, "OpenStream not the current call");
        if (aCall != plugin->mViewerPendingCall)
          return;

	plugin->mViewerPendingCall = NULL;

	GError *error = NULL;
	if (!dbus_g_proxy_end_call (aProxy, aCall, &error, G_TYPE_INVALID)) {
		g_warning ("OpenStream failed: %s", error->message);
		g_error_free (error);
		return;
	}

	/* FIXME this isn't the best way... */
	if (plugin->mHidden &&
	    plugin->mAutoPlay) {
		plugin->Command (TOTEM_COMMAND_PLAY);
	}

	assert (!plugin->mExpectingStream); /* Already expecting a stream */

        //assert (plugin->mRequestURI);
	if (!plugin->mRequestURI)
          return;

	plugin->mExpectingStream = true;

	/* Use GetURLNotify so we can reset mExpectingStream on failure */
	NPError err = NPN_GetURLNotify (plugin->mNPP,
                                        plugin->mRequestURI,
                                        NULL,
                                        NULL);
	if (err != NPERR_NO_ERROR) {
		plugin->mExpectingStream = false;

		g_debug ("GetURLNotify '%s' failed with error %d", plugin->mRequestURI, err);
		return;
	}

#ifdef TOTEM_NARROWSPACE_PLUGIN
        if (!plugin->mNPObjects[ePluginScriptable].IsNull ()) {
                NPObject *object = plugin->mNPObjects[ePluginScriptable];
                totemNarrowSpacePlayer *scriptable = static_cast<totemNarrowSpacePlayer*>(object);
		scriptable->mPluginState = totemNarrowSpacePlayer::eState_Playable;
	}
#endif /* TOTEM_NARROWSPACE_PLUGIN */
#ifdef TOTEM_GMP_PLUGIN
        if (!plugin->mNPObjects[ePluginScriptable].IsNull ()) {
                NPObject *object = plugin->mNPObjects[ePluginScriptable];
                totemGMPPlayer *scriptable = static_cast<totemGMPPlayer*>(object);
		scriptable->mPluginState = totemGMPPlayer::eState_Waiting;
	}
#endif /* TOTEM_GMP_PLUGIN */
}

/* static */ void
totemPlugin::ViewerOpenURICallback (DBusGProxy *aProxy,
				    DBusGProxyCall *aCall,
				    void *aData)
{
	totemPlugin *plugin = reinterpret_cast<totemPlugin*>(aData);

	g_debug ("OpenURI reply");

// 	//assert (aCall == plugin->mViewerPendingCall, "OpenURI not the current call");
        if (aCall != plugin->mViewerPendingCall)
          return;

	plugin->mViewerPendingCall = NULL;

	GError *error = NULL;
	if (!dbus_g_proxy_end_call (aProxy, aCall, &error, G_TYPE_INVALID)) {
		g_warning ("OpenURI failed: %s", error->message);
		g_error_free (error);
		return;
	}

#ifdef TOTEM_NARROWSPACE_PLUGIN
        if (!plugin->mNPObjects[ePluginScriptable].IsNull ()) {
                NPObject *object = plugin->mNPObjects[ePluginScriptable];
                totemNarrowSpacePlayer *scriptable = static_cast<totemNarrowSpacePlayer*>(object);
		scriptable->mPluginState = totemNarrowSpacePlayer::eState_Playable;
	}
#endif /* TOTEM_NARROWSPACE_PLUGIN */
#ifdef TOTEM_GMP_PLUGIN
        if (!plugin->mNPObjects[ePluginScriptable].IsNull ()) {
                NPObject *object = plugin->mNPObjects[ePluginScriptable];
                totemGMPPlayer *scriptable = static_cast<totemGMPPlayer*>(object);
		scriptable->mPluginState = totemGMPPlayer::eState_Ready;
	}
#endif /* TOTEM_GMP_PLUGIN */

	/* FIXME this isn't the best way... */
	if (plugin->mAutoPlay) {
		plugin->Command (TOTEM_COMMAND_PLAY);
	}
}

/* Auxiliary functions */


/* static */ char *
totemPlugin::PluginDescription ()
{
  return (char*) kPluginDescription;
}

/* static */ char *
totemPlugin::PluginLongDescription ()
{
  return (char*) kPluginLongDescription;
}

/* static */ void
totemPlugin::PluginMimeTypes (const totemPluginMimeEntry **_entries,
					uint32_t *_count)
{
  *_entries = kMimeTypes;
  *_count = G_N_ELEMENTS (kMimeTypes);
}

void
totemPlugin::SetRealMimeType (const char *mimetype)
{
	for (uint32_t i = 0; i < G_N_ELEMENTS (kMimeTypes); ++i) {
		if (strcmp (kMimeTypes[i].mimetype, mimetype) == 0) {
			if (kMimeTypes[i].mime_alias != NULL) {
				mMimeType = g_strdup (kMimeTypes[i].mime_alias);
			} else {
				mMimeType = g_strdup (mimetype);
			}
			return;
		}
	}

	D ("Real mime-type for '%s' not found", mimetype);
}

bool
totemPlugin::IsSchemeSupported (const char *aURI, const char *aBaseURI)
{
  if (!aURI)
    return false;

  char *scheme = g_uri_parse_scheme (aURI);
  if (!scheme) {
    scheme = g_uri_parse_scheme (aBaseURI);
    if (!scheme)
      return false;
  }

  bool isSupported = false;
  if (g_ascii_strcasecmp (scheme, "http") == 0 ||
      g_ascii_strcasecmp (scheme, "https") == 0 ||
      g_ascii_strcasecmp (scheme, "ftp") == 0)
    isSupported = true;

  D("IsSchemeSupported scheme '%s': %s", scheme, isSupported ? "yes" : "no");

  g_free (scheme);

  return isSupported;
}

bool
totemPlugin::ParseBoolean (const char *key,
			   const char *value,
			   bool default_val)
{
	if (value == NULL || strcmp (value, "") == 0)
		return default_val;
	if (g_ascii_strcasecmp (value, "false") == 0 || g_ascii_strcasecmp (value, "no") == 0)
		return false;
	if (g_ascii_strcasecmp (value, "true") == 0 || g_ascii_strcasecmp (value, "yes") == 0)
		return true;

        char *endptr = NULL;
        errno = 0;
        long num = g_ascii_strtoll (value, &endptr, 0);
        if (endptr != value && errno == 0) {
                return num > 0;
        }

	D ("Unknown value '%s' for parameter '%s'", value, key);

	return default_val;
}

bool
totemPlugin::GetBooleanValue (GHashTable *args,
			      const char *key,
			      bool default_val)
{
	const char *value;

	value = (const char *) g_hash_table_lookup (args, key);
	if (value == NULL)
		return default_val;

	return ParseBoolean (key, value, default_val);
}

uint32_t
totemPlugin::GetEnumIndex (GHashTable *args,
			   const char *key,
			   const char *values[],
			   uint32_t n_values,
			   uint32_t default_value)
{
	const char *value = (const char *) g_hash_table_lookup (args, key);
	if (!value)
		return default_value;

	for (uint32_t i = 0; i < n_values; ++i) {
		if (g_ascii_strcasecmp (value, values[i]) == 0)
			return i;
	}

	return default_value;
}

/* Public functions for use by the scriptable plugin */

bool
totemPlugin::SetSrc (const char* aURL)
{
        g_free (mSrcURI);

	/* If |src| is empty, don't resolve the URI! Otherwise we may
	 * try to load an (probably iframe) html document as our video stream.
	 */
	if (!aURL || !aURL[0]) {
              mSrcURI = NULL;
              return true;
        }

        mSrcURI = g_strdup (aURL);

        if (mAutoPlay) {
                RequestStream (false);
        } else {
                mWaitingForButtonPress = true;
        }

        return true;
}

bool
totemPlugin::SetSrc (const NPString& aURL)
{
        g_free (mSrcURI);

	/* If |src| is empty, don't resolve the URI! Otherwise we may
	 * try to load an (probably iframe) html document as our video stream.
	 */
	if (!aURL.UTF8Characters || !aURL.UTF8Length) {
              mSrcURI = NULL;
              return true;
        }

        mSrcURI = g_strndup (aURL.UTF8Characters, aURL.UTF8Length);

        if (mAutoPlay) {
                RequestStream (false);
        } else {
                mWaitingForButtonPress = true;
        }

        return true;
}

#ifdef TOTEM_GMP_PLUGIN

void
totemPlugin::SetURL (const char* aURL)
{
        g_free (mURLURI);

	/* Don't allow empty URL */
        if (!aURL || !aURL[0]) {
                mURLURI = NULL;
		return;
        }

        mURLURI = g_strdup (aURL);

	/* FIXME: what is the correct base for the URL param? mSrcURI or mBaseURI? */
	/* FIXME: security checks? */
        /* FIXMEchpe: resolve the URI here? */
}

#endif /* TOTEM_GMP_PLUGIN */

#ifdef TOTEM_NARROWSPACE_PLUGIN

bool
totemPlugin::SetQtsrc (const char* aURL)
{
	/* FIXME can qtsrc have URL extensions? */

        g_free (mQtsrcURI);
        
	/* Don't allow empty qtsrc */
	if (!aURL || !aURL[0]) {
                mQtsrcURI = NULL;
		return true;
        }

        mQtsrcURI = g_strdup (aURL);

	/* FIXME: security checks? */
        /* FIXMEchpe: resolve the URI here? */

        return true;
}

bool
totemPlugin::SetHref (const char* aURL)
{
	char *url = NULL, *target = NULL;
	bool hasExtensions = ParseURLExtensions (aURL, &url, &target);

	D ("SetHref '%s' has-extensions %d (url: '%s' target: '%s')",
	   aURL ? aURL : "", hasExtensions, url ? url : "", target ? target : "");

#if 0
// 	nsresult rv = NS_OK;
	char *baseURI;
	if (mQtsrcURI) {
		baseURI = mQtsrcURI;
	} else if (mSrcURI) {
		baseURI = mSrcURI;
	} else {
		baseURI = mBaseURI;
	}
#endif

	if (hasExtensions) {
                g_free (mHref);
                mHref = g_strdup (url && url[0] ? url : NULL);
#if 0
		rv = baseURI->Resolve (url, mHref);
#endif
                g_free (mTarget);
                mTarget = g_strdup (target);
	} else {
                g_free (mHref);
                mHref = g_strdup (aURL && aURL[0] ? aURL : NULL);
#if 0
		rv = baseURI->Resolve (aURL, mHref);
#endif

                g_free (mTarget);
                mTarget = NULL;
	}
#if 0
	if (NS_SUCCEEDED (rv)) {
		D ("Resolved HREF '%s'", mHref ? mHref : "");
	} else {
		D ("Failed to resolve HREF (rv=%x)", rv);
		mHref = hasExtensions ? g_strdup (url) : g_strdup (aURL); /* save unresolved HREF */
	}
#endif

        g_free (url);
        g_free (target);

	return true; // FIXMEchpe
}

bool
totemPlugin::ParseURLExtensions (const char* str,
				 char **_url,
				 char **_target)
{
        if (!str || !str[0])
                return false;

        /* FIXMEchpe allo whitespace in front? */
	if (str[0] != '<')
		return false;

	/* The expected form is "<URL> T<target> E<name=value pairs>".
	 * But since this is untrusted input from the web, we'll make sure it conforms to this!
	 */
	const char *end = strchr (str, '>');
	if (!end)
		return false;

//	_url = nsDependentCSubstring (string, 1, uint32_t (end - str - 1));
        *_url = g_strndup (str + 1, end - str - 1);

	const char *ext = strstr (end, " T<");
	if (ext) {
		const char *extend = strchr (ext, '>');
		if (extend) {
                        *_target = g_strndup (ext + 3, extend - ext - 3);
		//	_target = nsDependentCSubstring (ext + 3, uint32_t (extend - ext - 3));
		}
	}

#if 0
	ext = strstr (end, " E<");
	if (ext) {
		const char *extend = strchr (ext, '>');
		if (extend) {
			D ("E = %s", nsCString (ext + 3, uint32_t (extend - ext - 3)).get ());
		}
	}
#endif

	return true;
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
		return NULL;
	}

	totemPlugin *representant = NULL;

	/* Try to find a the representant of the console class */
	uint32_t count = sPlugins->Length ();
	for (uint32_t i = 0; i < count; ++i) {
		totemPlugin *plugin = sPlugins->ElementAt (i);

		bool equal = false;
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

bool
totemPlugin::SetConsole (const char * aConsole)
{
	/* Can't change console group */
	if (mConsole) {
// 		return NS_ERROR_ALREADY_INITIALIZED;
          // FIXMEchpe set exception
          return false;
        }

	/* FIXME: we might allow this, and kill the viewer instead.
	 * Or maybe not spawn the viewer if we don't have a console yet?
	 */
	if (mViewerPID)
		return false; // FIXMEchpe throw exception

        mConsole = g_strdup (aConsole);

// 	assert (mConsoleClassRepresentant == NULL, "Already have a representant");

	mConsoleClassRepresentant = FindConsoleClassRepresentant ();
	mNeedViewer = (NULL == mConsoleClassRepresentant);

	return true;
}

void
totemPlugin::TransferConsole ()
{
	/* Find replacement representant */
	totemPlugin *representant = NULL;

	uint32_t i, count = sPlugins->Length ();
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
	representant->mConsoleClassRepresentant = NULL;
	/* We can start at i since we got out when we found the first one in the loop above */
	for ( ; i < count; ++i) {
		totemPlugin *plugin = sPlugins->ElementAt (i);

		if (plugin->mConsoleClassRepresentant == this)
			plugin->mConsoleClassRepresentant = representant;
	}

	/* Now transfer viewer ownership */
#if 0
	if (mScriptable) {
		assert (!representant->mScriptable, "WTF");
		representant->mScriptable = mScriptable;
		mScriptable->SetPlugin (representant);
		mScriptable = NULL;
	}
#endif

	representant->mNeedViewer = true;

	representant->mViewerPID = mViewerPID;
	mViewerPID = 0;

	representant->mViewerFD = mViewerFD;
	mViewerFD = -1;

	representant->mViewerBusAddress = g_strdup (mViewerBusAddress);
	representant->mViewerServiceName = g_strdup (mViewerServiceName);

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

	mUnownedViewerSetUp = true;

	D ("UnownedViewerSetup");

	assert (mConsoleClassRepresentant); /* We own the viewer!? */

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

	assert (mConsoleClassRepresentant); /* We own the viewer! */

	assert (mConsoleClassRepresentant->mViewerProxy);

	/* FIXME: do we need a reply callback? */
	dbus_g_proxy_call_no_reply (mConsoleClassRepresentant->mViewerProxy,
				    "SetWindow",
				    G_TYPE_STRING, mControls.get (),
				    G_TYPE_UINT, (guint) mWindow,
				    G_TYPE_INT, mWidth,
				    G_TYPE_INT, mHeight,
				    G_TYPE_INVALID);

	mWindowSet = true;
}

void
totemPlugin::UnownedViewerUnsetWindow ()
{
	if (!mWindowSet || mWindow == 0)
		return;

	if (!mUnownedViewerSetUp)
		return;

	assert (mConsoleClassRepresentant); /* We own the viewer! */

	assert (mConsoleClassRepresentant->mViewerProxy);

	dbus_g_proxy_call_no_reply (mConsoleClassRepresentant->mViewerProxy,
				    "UnsetWindow",
				    G_TYPE_UINT, (guint) mWindow,
				    G_TYPE_INVALID);

	mWindowSet = false;
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

	/* mode is NP_EMBED, NP_FULL, or NP_BACKGROUND (see npapi.h) */
	/* FIXME we should error out if we are in fullscreen mode
	 * FIXME: This might be possible on gecko trunk by returning an
 	 * error code from the NewStream function.
	 */

	NPError err;
        err = NPN_GetValue (mNPP,
                            NPNVPluginElementNPObject,
                            getter_Retains (mPluginElement));
	if (err != NPERR_NO_ERROR || mPluginElement.IsNull ()) {
		Dm ("Failed to get our DOM Element NPObject");
		return NPERR_GENERIC_ERROR;
	}

#if defined(TOTEM_COMPLEX_PLUGIN) && defined(HAVE_NSTARRAY_H)
        /* FIXMEchpe untested; test this! */
        if (!NPN_GetProperty (mNPP,
                              mPluginElement,
                              NPN_GetStringIdentifier ("ownerDocument"),
                              getter_Retains (mPluginOwnerDocument)) ||
            mPluginOwnerDocument.IsNull ()) {
		Dm ("Failed to get the plugin element's ownerDocument");
		return NPERR_GENERIC_ERROR;
	}
#endif /* TOTEM_COMPLEX_PLUGIN */

	/* We'd like to get the base URI of our DOM element as a nsIURI,
	 * but there's no frozen method to do so (nsIContent/nsINode isn't available
	 * for non-MOZILLA_INTERNAL_API code). nsIDOM3Node isn't frozen either,
	 * but should be safe enough.
	 */
	
        /* This is a property on nsIDOM3Node */
        totemNPVariantWrapper baseURI;
        if (!NPN_GetProperty (mNPP,
                              mPluginElement,
                              NPN_GetStringIdentifier ("baseURI"),
                              getter_Copies (baseURI)) ||
            !baseURI.IsString ()) {
          Dm ("Failed to get the base URI");
          return NPERR_GENERIC_ERROR;
        }

	mBaseURI = g_strndup (baseURI.GetString (), baseURI.GetStringLen());
	D ("Base URI is '%s'", mBaseURI ? mBaseURI : "");

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
		Dm ("Failed to get DBUS proxy");
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
				     reinterpret_cast<void*>(this),
				     NULL);

	/* Find the "real" mime-type */
	SetRealMimeType (mimetype);

	D ("Real mimetype for '%s' is '%s'", mimetype, mMimeType ? mMimeType : "(null)");

	/* Now parse the attributes */
	/* Note: argv[i] is NULL for the "PARAM" arg which separates the attributes
	 * of an <object> tag from the <param> values under it.
	 */
	GHashTable *args = g_hash_table_new_full (g_str_hash,
		g_str_equal,
		(GDestroyNotify) g_free,
		(GDestroyNotify) g_free);
	for (int16_t i = 0; i < argc; i++) {
		printf ("argv[%d] %s %s\n", i, argn[i], argv[i] ? argv[i] : "");
		if (argv[i]) {
			g_hash_table_insert (args, g_ascii_strdown (argn[i], -1),
					     g_strdup (argv[i]));
		}
	}

	const char *value;

	/* We only use the size attributes to detect whether we're hidden;
	 * we'll get our real size from SetWindow.
	 */
	int width = -1, height = -1;

	value = (const char *) g_hash_table_lookup (args, "width");
	if (value != NULL) {
		width = strtol (value, NULL, 0);
	}
	value = (const char *) g_hash_table_lookup (args, "height");
	if (value != NULL) {
		height = strtol (value, NULL, 0);
	}

#ifdef TOTEM_GMP_PLUGIN
	value = (const char *) g_hash_table_lookup (args, "vidwidth");
	if (value != NULL) {
		width = strtol (value, NULL, 0);
	}
	value = (const char *) g_hash_table_lookup (args, "vidheight");
	if (value != NULL) {
		height = strtol (value, NULL, 0);
	}
#endif /* TOTEM_GMP_PLUGIN */

	/* Are we hidden? */
	/* Treat hidden without a value as TRUE */
	mHidden = g_hash_table_lookup (args, "hidden") != NULL &&
		  GetBooleanValue (args, "hidden", true);

	/* Used as a replacement for HIDDEN=TRUE attribute.
	 * See http://mxr.mozilla.org/mozilla/source/modules/plugin/base/src/ns4xPluginInstance.cpp#1135
	 * We don't use:
	 * width <= 0 || height <= 0
	 * as -1 is our value for unset/unknown sizes
	 */
	if (width == 0 || height == 0)
		mHidden = true;

	/* Whether to automatically stream and play the content */
	mAutoPlay = GetBooleanValue (args, "autoplay",
				      GetBooleanValue (args, "autostart", mAutoPlay));

	/* Whether to loop */
	mRepeat = GetBooleanValue (args, "repeat",
				   GetBooleanValue (args, "loop", false));

	/* Now collect URI attributes */
	const char *src = (const char *) g_hash_table_lookup (args, "src");
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

        SetSrc (src);

#ifdef TOTEM_GMP_PLUGIN
	/* http://windowssdk.msdn.microsoft.com/en-us/library/aa392440(VS.80).aspx */
	const char *filename = (const char *) g_hash_table_lookup (args, "filename");
	if (!filename)
		filename = (const char *) g_hash_table_lookup (args, "url");

        if (filename) {
                SetURL (filename);
        }
#endif /* TOTEM_GMP_PLUGIN */

#ifdef TOTEM_NARROWSPACE_PLUGIN
	const char *href = (const char *) g_hash_table_lookup (args, "href");
	if (href) {
		SetHref (href);
	}

	/* Target, set it after SetHref() call, otherwise mTarget will be empty */
	value = (const char *) g_hash_table_lookup (args, "target");
	if (value) {
                mTarget = g_strdup (value);
	}

	/* http://www.apple.com/quicktime/tutorials/embed2.html */
	mAutoHref = g_hash_table_lookup (args, "autohref") != NULL &&
		GetBooleanValue (args, "autohref", false);

	/* http://developer.apple.com/documentation/QuickTime/QT6WhatsNew/Chap1/chapter_1_section_13.html */
	const char *qtsrc = (const char *) g_hash_table_lookup (args, "qtsrc");
	if (qtsrc) {
		SetQtsrc (qtsrc);
	}
#endif /* TOTEM_NARROWSPACE_PLUGIN */

/* FIXME */
#ifdef TOTEM_MULLY_PLUGIN
	value = (const char *) g_hash_table_lookup (args, "video");
	if (value) {
                SetSrc (value);
	}
#endif /* TOTEM_MULLY_PLUGIN */

#ifdef TOTEM_CONE_PLUGIN
	value = (const char *) g_hash_table_lookup (args, "target");
	if (value) {
                SetSrc (value);
	}
#endif /* TOTEM_CONE_PLUGIN */

#if 0 //def TOTEM_MULLY_PLUGIN
	/* Click to play behaviour of the DivX plugin */
	char *previewimage = (const char *) g_hash_table_lookup (args, "previewimage");
	if (value != NULL)
		mHref = g_strdup (mSrc);
#endif /* TOTEM_MULLY_PLUGIN */

        /* FIXMEchpe: check if this doesn't work anymore because the URLs aren't fully resolved! */
	/* If we're set to start automatically, we'll use the src stream */
	if (mRequestURI &&
            mSrcURI &&
            strcmp (mRequestURI, mSrcURI) == 0) {
		mExpectingStream = mAutoPlay;
	}

	/* Caching behaviour */
#ifdef TOTEM_NARROWSPACE_PLUGIN
	if (strcmp (mimetype, "video/quicktime") != 0) {
		mCache = true;
	}

	mCache = GetBooleanValue (args, "cache", mCache);
#endif /* TOTEM_NARROWSPACE_PLUGIN */

#if defined (TOTEM_NARROWSPACE_PLUGIN)
	mControllerHidden = !GetBooleanValue (args, "controller", true);

	mAutoPlay = GetBooleanValue (args, "autoplay", true);

#endif /* TOTEM_NARROWSPACE_PLUGIN */

/* VLC plugin defaults to have its controller hidden
 * But we don't want to hide it if VLC wasn't explicitely
 * requested */
#ifdef TOTEM_CONE_PLUGIN
	if (strstr ((const char *) mimetype, "vlc") != NULL)
		mControllerHidden = true;
#endif

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
	uint32_t control = GetEnumIndex (args, "controls",
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
			mControllerHidden = true;
		} else if (g_ascii_strcasecmp (value, "invisible") == 0) {
			mHidden = true;
		} else if (g_ascii_strcasecmp (value, "full") == 0) {
			mShowStatusbar = true;
		} else if (g_ascii_strcasecmp (value, "mini") == 0) {
			;
		}
	}

	/* Whether the controls are all hidden, MSIE parameter
	 * http://www.htmlcodetutorial.com/embeddedobjects/_EMBED_CONTROLLER.html */
	/* ShowXXX parameters as per http://support.microsoft.com/kb/285154 */
	mControllerHidden = !GetBooleanValue (args, "controller",
					      GetBooleanValue (args, "showcontrols", true));

	mShowStatusbar = GetBooleanValue (args, "showstatusbar", mShowStatusbar);
#endif /* TOTEM_GMP_PLUGIN */

	/* Whether to NOT autostart */
	//FIXME Doesn't handle playcount, or loop with numbers
	// http://www.htmlcodetutorial.com/embeddedobjects/_EMBED_LOOP.html

	//FIXME handle starttime and endtime
	// http://www.htmlcodetutorial.com/embeddedobjects/_EMBED_STARTTIME.html

	/* Minimum heights of the different plugins, note that the
	 * controllers need to be showing, otherwise it's useless */
#ifdef TOTEM_GMP_PLUGIN
	if (height == 40 && !mControllerHidden) {
		mAudioOnly = true;
	}
#endif /* TOTEM_GMP_PLUGIN */
#if defined(TOTEM_NARROWSPACE_PLUGIN)
	if (height <= 16 && !mControllerHidden) {
		mAudioOnly = true;
	}
#endif /* TOTEM_NARROWSPACE_PLUGIN */

#ifdef TOTEM_NARROWSPACE_PLUGIN
	/* We need to autostart if we're using an HREF
	 * otherwise the start image isn't shown */
	if (mHref) {
		mExpectingStream = true;
		mAutoPlay = true;
	}
#endif /* TOTEM_NARROWSPACE_PLUGIN */

	/* Dump some disagnostics */
	D ("mSrcURI: %s", mSrcURI ? mSrcURI : "");
	D ("mCache: %d", mCache);
	D ("mControllerHidden: %d", mControllerHidden);
	D ("mShowStatusbar: %d", mShowStatusbar);
	D ("mHidden: %d", mHidden);
	D ("mAudioOnly: %d", mAudioOnly);
	D ("mAutoPlay: %d, mRepeat: %d", mAutoPlay, mRepeat);
#ifdef TOTEM_NARROWSPACE_PLUGIN
	D ("mHref: %s", mHref ? mHref : "");
	D ("mTarget: %s", mTarget ? mTarget : "");
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
		Dm ("SetWindow: hidden, can't set window");
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
		Dm ("Setting a new window != mWindow, this is unsupported!");
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
		Dm ("Already have a live stream, aborting stream");

		/* We don't just return NPERR_GENERIC_ERROR (or any other error code),
		 * since, using gecko trunk (1.9), this causes the plugin to be destroyed,
		 * if this is the automatic |src| stream. Same for the other calls below.
		 */
		return NPN_DestroyStream (mNPP,
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
		Dm ("Not expecting a new stream; aborting stream");

		return NPN_DestroyStream (mNPP,
                                          stream,
                                          NPRES_DONE);
	}

	/* This was an expected stream, no more expected */
	mExpectingStream = false;

#if 1 // #if 0
	// This is fixed now _except_ the "if (!mViewerReady)" problem in StreamAsFile

	/* For now, we'll re-request the stream when the viewer is ready.
	 * As an optimisation, we could either just allow small (< ~128ko) streams
	 * (which are likely to be playlists!), or any stream and cache them to disk
	 * until the viewer is ready.
	 */
	if (!mViewerReady) {
		Dm ("Viewer not ready, aborting stream");

		return NPN_DestroyStream (mNPP,
                                          stream,
                                          NPRES_DONE);
	}
#endif

	/* FIXME: assign the stream URL to mRequestURI ? */

	if (g_str_has_prefix (stream->url, "file://")) {
		*stype = NP_ASFILEONLY;
		mStreamType = NP_ASFILEONLY;
	} else {
		*stype = NP_ASFILE;
		mStreamType = NP_ASFILE;
	}

#ifdef TOTEM_NARROWSPACE_PLUGIN
        if (!mNPObjects[ePluginScriptable].IsNull ()) {
                NPObject *object = mNPObjects[ePluginScriptable];
                totemNarrowSpacePlayer *scriptable = static_cast<totemNarrowSpacePlayer*>(object);
		scriptable->mPluginState = totemNarrowSpacePlayer::eState_Loading;
	}
#endif /* TOTEM_NARROWSPACE_PLUGIN */
#ifdef TOTEM_GMP_PLUGIN
        if (!mNPObjects[ePluginScriptable].IsNull ()) {
                NPObject *object = mNPObjects[ePluginScriptable];
                totemGMPPlayer *scriptable = static_cast<totemGMPPlayer*>(object);
		scriptable->mPluginState = totemGMPPlayer::eState_Buffering;
	}
#endif /* TOTEM_GMP_PLUGIN */

	mStream = stream;

	mCheckedForPlaylist = false;
	mIsPlaylist = false;

	/* To track how many data we get from ::Write */
	mBytesStreamed = 0;
	mBytesLength = stream->end;

	return NPERR_NO_ERROR;
}

NPError
totemPlugin::DestroyStream (NPStream* stream,
			    NPError reason)
{
	if (!mStream || mStream != stream)
		return NPERR_GENERIC_ERROR;

	D ("DestroyStream reason %d", reason);

	mStream = NULL;
	mBytesStreamed = 0;
	mBytesLength = 0;

	int ret = close (mViewerFD);
	if (ret < 0) {
		int err = errno;
		D ("Failed to close viewer stream with errno %d: %s", err, g_strerror (err));
	}

	mViewerFD = -1;

	return NPERR_NO_ERROR;
}

int32_t
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

int32_t
totemPlugin::Write (NPStream *stream,
		    int32_t offset,
		    int32_t len,
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
		assert (offset == 0); /* Not checked for playlist but not at the start of the stream!? */

		mCheckedForPlaylist = true;

		if (totem_pl_parser_can_parse_from_data ((const char *) buffer, len, TRUE /* FIXME */)) {
			Dm ("Is playlist; need to wait for the file to be downloaded completely");
			mIsPlaylist = true;

			/* Close the viewer */
			dbus_g_proxy_call_no_reply (mViewerProxy,
						    "CloseStream",
						    G_TYPE_INVALID,
						    G_TYPE_INVALID);

			return len;
		} else {
			D ("Is not playlist: totem_pl_parser_can_parse_from_data failed (len %d)", len);
		}
	}

	int ret = write (mViewerFD, buffer, len);
	/* FIXME shouldn't we retry if errno is EINTR ? */

	if (G_UNLIKELY (ret < 0)) {
		int err = errno;
		D ("Write failed with errno %d: %s", err, g_strerror (err));

		if (err == EPIPE) {
			/* fd://0 got closed, probably because the backend
			 * crashed on us. Destroy the stream.
			 */
			if (NPN_DestroyStream (mNPP,
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

	if (!mCheckedForPlaylist) {
		mIsPlaylist = totem_pl_parser_can_parse_from_filename
				(fname, TRUE) != FALSE;
	}

	/* FIXME! This happens when we're using the automatic |src| stream and
	 * it finishes before we're ready.
	 */
	if (!mViewerReady) {
		Dm ("Viewer not ready yet, deferring SetLocalFile");
		return;
	}

	assert (mViewerProxy); /* No viewer proxy!? */
	assert (mViewerReady); /* Viewer not ready? */

	//assert (mRequestBaseURI && mRequestURI, );
        if (!mRequestBaseURI || !mRequestURI)
          return;

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
					    G_TYPE_STRING, mRequestURI,
					    G_TYPE_STRING, mRequestBaseURI,
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
					    G_TYPE_STRING, mRequestURI,
					    G_TYPE_STRING, mRequestBaseURI,
					    G_TYPE_INVALID,
					    G_TYPE_INVALID);
	}
	/* If the file has finished streaming from the network
	 * and is on the disk, then we should be able to play
	 * it back from the cache, rather than just stopping there */
	else {
		D ("mBytesStreamed %u", mBytesStreamed);
		retval = dbus_g_proxy_call (mViewerProxy,
					    "SetLocalCache",
					    &error,
					    G_TYPE_STRING, fname,
					    G_TYPE_INVALID,
					    G_TYPE_INVALID);
	}

	if (!retval) {
		g_warning ("Viewer error: %s", error->message);
		g_error_free (error);
		return;
	}
#ifdef TOTEM_NARROWSPACE_PLUGIN
        if (!mNPObjects[ePluginScriptable].IsNull ()) {
                NPObject *object = mNPObjects[ePluginScriptable];
                totemNarrowSpacePlayer *scriptable = static_cast<totemNarrowSpacePlayer*>(object);
		scriptable->mPluginState = totemNarrowSpacePlayer::eState_Complete;
	}
#endif /* TOTEM_NARROWSPACE_PLUGIN */
#ifdef TOTEM_GMP_PLUGIN
        if (!mNPObjects[ePluginScriptable].IsNull ()) {
                NPObject *object = mNPObjects[ePluginScriptable];
                totemGMPPlayer *scriptable = static_cast<totemGMPPlayer*>(object);
		scriptable->mPluginState = totemGMPPlayer::eState_Ready;
	}
#endif /* TOTEM_GMP_PLUGIN */
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
			Dm ("Failed to get stream");
			/* FIXME: show error to user? */
		}

		mExpectingStream = false;
	}
}

NPObject*
totemPlugin::GetNPObject (ObjectEnum which)
{
  if (!mNPObjects[which].IsNull ())
    return mNPObjects[which];

  totemNPClass_base *npclass = 0;

#if defined(TOTEM_GMP_PLUGIN)
  switch (which) {
    case ePluginScriptable:
      npclass = totemGMPPlayerNPClass::Instance();
      break;
    case eGMPControls:
      npclass = totemGMPControlsNPClass::Instance();
      break;
    case eGMPNetwork:
      npclass = totemGMPNetworkNPClass::Instance();
      break;
    case eGMPSettings:
      npclass = totemGMPSettingsNPClass::Instance();
      break;
    case eLastNPObject:
      g_assert_not_reached ();
  }
#elif defined(TOTEM_COMPLEX_PLUGIN)
  npclass = totemComplexPluginNPClass::Instance();
#elif defined(TOTEM_NARROWSPACE_PLUGIN)
  npclass = totemNarrowSpacePlayerNPClass::Instance();
#elif defined(TOTEM_MULLY_PLUGIN)
  npclass = totemMullYPlayerNPClass::Instance();
#elif defined(TOTEM_CONE_PLUGIN)
  switch (which) {
    case ePluginScriptable:
      npclass = totemConeNPClass::Instance();
      break;
    case eConeAudio:
      npclass = totemConeAudioNPClass::Instance();
      break;
    case eConeInput:
      npclass = totemConeInputNPClass::Instance();
      break;
    case eConePlaylist:
      npclass = totemConePlaylistNPClass::Instance();
      break;
    case eConePlaylistItems:
      npclass = totemConePlaylistItemsNPClass::Instance();
      break;
    case eConeVideo:
      npclass = totemConeVideoNPClass::Instance();
      break;
    case eLastNPObject:
      g_assert_not_reached ();
  }
#else
#error Unknown plugin type
#endif

  if (!npclass)
    return NULL;

  mNPObjects[which] = do_CreateInstance (npclass, mNPP);
  if (mNPObjects[which].IsNull ()) {
    Dm ("Creating scriptable NPObject failed!");
    return NULL;
  }

  return mNPObjects[which];
}

NPError
totemPlugin::GetScriptableNPObject (void *_retval)
{
  D ("GetScriptableNPObject [%p]", (void *) this);

  NPObject *scriptable = GetNPObject (ePluginScriptable);
  if (!scriptable)
    return NPERR_GENERIC_ERROR;

  NPN_RetainObject (scriptable);

  *reinterpret_cast<NPObject**>(_retval) = scriptable;
  return NPERR_NO_ERROR;
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
		sPlugins = NULL;
	}
#endif /* TOTEM_COMPLEX_PLUGIN */

#if defined(TOTEM_GMP_PLUGIN)
        totemGMPPlayerNPClass::Shutdown ();
        totemGMPControlsNPClass::Shutdown ();
        totemGMPNetworkNPClass::Shutdown ();
        totemGMPSettingsNPClass::Shutdown ();
#elif defined(TOTEM_COMPLEX_PLUGIN)
	totemComplexPluginNPClass::Shutdown ();
#elif defined(TOTEM_NARROWSPACE_PLUGIN)
        totemNarrowSpacePlayerNPClass::Shutdown ();
#elif defined(TOTEM_MULLY_PLUGIN)
        totemMullYPlayerNPClass::Shutdown ();
#elif defined(TOTEM_CONE_PLUGIN)
        totemConeNPClass::Shutdown ();
        totemConeAudioNPClass::Shutdown ();
        totemConeInputNPClass::Shutdown ();
        totemConePlaylistNPClass::Shutdown ();
        totemConePlaylistItemsNPClass::Shutdown ();
        totemConeVideoNPClass::Shutdown ();
#else
#error Unknown plugin type
#endif

	return NPERR_NO_ERROR;
}
