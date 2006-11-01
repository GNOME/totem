/* Totem Mozilla plugin
 * 
 * Copyright (C) 2004-2006 Bastien Nocera <hadess@hadess.net>
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

#include <glib.h>

#include <libgnomevfs/gnome-vfs-mime-utils.h>

#include "totem-pl-parser-mini.h"
#include "totem-mozilla-options.h"

#include "npapi.h"
#include "npupp.h"

#include <nsIDOMWindow.h>
#include <nsIURI.h>
#include <nsEmbedString.h>
#include <nsIInterfaceRequestor.h>
#include <nsIInterfaceRequestorUtils.h>
#include <nsIWebNavigation.h>

#define GNOME_ENABLE_DEBUG 1
/* define GNOME_ENABLE_DEBUG for more debug spew */
#include "debug.h"

#include "totemPluginGlue.h"
#include "totemPlugin.h"

#define DASHES "--"

/* How much data bytes to request */
#define PLUGIN_STREAM_CHUNK_SIZE (8 * 1024)

extern NPNetscapeFuncs sMozillaFuncs;

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
: mInstance(aInstance),
  mWidth(-1),
  mHeight(-1),
  mSendFD(-1)
{
  D ("totemPlugin ctor [%p]", (void*) this);
}

totemPlugin::~totemPlugin ()
{
  if (mScriptable) {
    mScriptable->UnsetPlugin ();
    NS_RELEASE (mScriptable);
  }

  if (mSendFD >= 0) {
    close (mSendFD);
    mSendFD = -1;
  }

  if (mPlayerPID) {
    kill (mPlayerPID, SIGKILL);
    g_spawn_close_pid (mPlayerPID);
    mPlayerPID = 0;
  }

  g_free (mLocal);
  g_free (mTarget);
  g_free (mSrc);
  g_free (mHref);

  if (mProxy)
    g_object_unref (mProxy);

  D ("totemPlugin dtor [%p]", (void*) this);
}

/* public functions */

nsresult
totemPlugin::Play ()
{
	D ("play");

	if (!mGotSvc)
		return NS_OK;

	NS_ASSERTION (mProxy, "No DBUS proxy!");
	dbus_g_proxy_call_no_reply(mProxy, "Play",
				   G_TYPE_INVALID, G_TYPE_INVALID);

	return NS_OK;
}

nsresult
totemPlugin::Stop ()
{
	D ("stop");

	if (!mGotSvc)
		return NS_OK;

	NS_ASSERTION (mProxy, "No DBUS proxy!");
	dbus_g_proxy_call_no_reply (mProxy, "Stop",
				    G_TYPE_INVALID, G_TYPE_INVALID);

	return NS_OK;
}

nsresult
totemPlugin::Pause ()
{
	D ("pause");

	if (!mGotSvc)
		return NS_OK;

	NS_ASSERTION (mProxy, "No DBUS proxy!");
	dbus_g_proxy_call_no_reply (mProxy, "Pause",
				    G_TYPE_INVALID, G_TYPE_INVALID);

	return NS_OK;
}

void
totemPlugin::UnsetStream ()
{
	if (!mStream)
		return;

	if (CallNPN_DestroyStreamProc (sMozillaFuncs.destroystream,
	    			       mInstance,
				       mStream,
				       NPRES_DONE) != NPERR_NO_ERROR) {
		    g_warning ("Couldn't destroy the stream");
		    return;
	}

	mStream = nsnull;
}

/* static */ void PR_CALLBACK 
totemPlugin::NameOwnerChangedCallback (DBusGProxy *proxy,
				       const char *svc,
				       const char *old_owner,
				       const char *new_owner,
				       totemPlugin *plugin)
{
	D ("Received notification for '%s' old-owner '%s' new-owner '%s'",
	   svc, old_owner, new_owner);

	if (plugin->mWaitForSvc &&
	    strcmp (svc, plugin->mWaitForSvc) == 0) {
		plugin->mGotSvc = TRUE;
	}
}

/* static */ void PR_CALLBACK
totemPlugin::StopSendingDataCallback (DBusGProxy *proxy,
				      totemPlugin *plugin)
{
	D("Stop sending data signal received");

	/* FIXME do it from a timer? */
	plugin->UnsetStream ();
}

char *
totemPlugin::GetRealMimeType (const char *mimetype)
{
	const totemPluginMimeEntry *mimetypes;
	PRUint32 count;

	totemScriptablePlugin::PluginMimeTypes (&mimetypes, &count);

	for (PRUint32 i = 0; i < count; ++i) {
		if (strcmp (mimetypes[i].mimetype, mimetype) == 0) {
			if (mimetypes[i].mime_alias != NULL)
				return g_strdup (mimetypes[i].mime_alias);
			else
				return g_strdup (mimetype);
		}
	}

	D ("Real mime-type for '%s' not found", mimetype);
	return NULL;
}

PRBool
totemPlugin::IsMimeTypeSupported (const char *mimetype, const char *url)
{
	const totemPluginMimeEntry *mimetypes;
	PRUint32 count;
	const char *guessed;

#if defined(TOTEM_MULLY_PLUGIN) || defined (TOTEM_GMP_PLUGIN)
	/* We can always play those image types */
	if (strcmp (mimetype, "image/jpeg") == 0)
		return PR_TRUE;
	if (strcmp (mimetype, "image/gif") == 0)
		return PR_TRUE;
#endif /* TOTEM_MULLY_PLUGIN || TOTEM_GMP_PLUGIN */

	/* Stupid web servers will do that */
	if (strcmp (mimetype, GNOME_VFS_MIME_TYPE_UNKNOWN) == 0)
		return PR_TRUE;

	totemScriptablePlugin::PluginMimeTypes (&mimetypes, &count);

	for (PRUint32 i = 0; i < count; ++i) {
		if (strcmp (mimetypes[i].mimetype, mimetype) == 0)
			return PR_TRUE;
	}

	/* Not supported? Probably a broken webserver */
	guessed = gnome_vfs_get_mime_type_for_name (url);

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
totemPlugin::IsSchemeSupported (const char *url)
{
	if (url == NULL)
		return PR_FALSE;

	if (g_str_has_prefix (url, "mms:") != FALSE)
		return PR_FALSE;
	if (g_str_has_prefix (url, "rtsp:") != FALSE)
		return PR_FALSE;

	return PR_TRUE;
}

PRBool
totemPlugin::Fork ()
{
	GTimeVal then, now;
	GPtrArray *arr;
	char **argv;
	GError *err = NULL;
	gboolean use_fd = FALSE;

	if (mSrc == NULL) {
		g_warning ("No URLs passed!");
		return FALSE;
	}

	/* Make sure we don't get both an XID and hidden */
	if (mWindow && mHidden) {
		g_warning ("Both hidden and a window!");
		return FALSE;
	}

	arr = g_ptr_array_new ();

#ifdef TOTEM_RUN_IN_SOURCE_TREE
	if (g_file_test ("./totem-mozilla-viewer",
				G_FILE_TEST_EXISTS) != FALSE) {
		g_ptr_array_add (arr, g_strdup ("./totem-mozilla-viewer"));
	} else {
		g_ptr_array_add (arr,
				g_strdup (LIBEXECDIR"/totem-mozilla-viewer"));
	}
#else
	g_ptr_array_add (arr,
			g_strdup (LIBEXECDIR"/totem-mozilla-viewer"));
#endif

	/* Most for RealAudio streams, but also used as a replacement for
	 * HIDDEN=TRUE */
	if (mWidth == 0 && mHeight == 0) {
		mWindow = 0;
		mHidden = TRUE;
	}

	if (mWindow) {
		g_ptr_array_add (arr, g_strdup (DASHES TOTEM_OPTION_XID));
		g_ptr_array_add (arr, g_strdup_printf ("%lu", mWindow));
	}

	if (mWidth > 0) {
		g_ptr_array_add (arr, g_strdup (DASHES TOTEM_OPTION_WIDTH));
		g_ptr_array_add (arr, g_strdup_printf ("%d", mWidth));
	}

	if (mHeight > 0) {
		g_ptr_array_add (arr, g_strdup (DASHES TOTEM_OPTION_HEIGHT));
		g_ptr_array_add (arr, g_strdup_printf ("%d", mHeight));
	}

	if (mSrc) {
		g_ptr_array_add (arr, g_strdup (DASHES TOTEM_OPTION_URL));
		g_ptr_array_add (arr, g_strdup (mSrc));
	}

	if (mHref) {
		g_ptr_array_add (arr, g_strdup (DASHES TOTEM_OPTION_HREF));
		g_ptr_array_add (arr, g_strdup (mHref));
	}

	if (mTarget) {
		g_ptr_array_add (arr, g_strdup (DASHES TOTEM_OPTION_TARGET));
		g_ptr_array_add (arr, g_strdup (mTarget));
	}

	if (mMimeType) {
		g_ptr_array_add (arr, g_strdup (DASHES TOTEM_OPTION_MIMETYPE));
		g_ptr_array_add (arr, g_strdup (mMimeType));
	}

	if (mControllerHidden) {
		g_ptr_array_add (arr, g_strdup (DASHES TOTEM_OPTION_CONTROLS_HIDDEN));
	}

	if (mStatusbar) {
		g_ptr_array_add (arr, g_strdup (DASHES TOTEM_OPTION_STATUSBAR));
	}

	if (mHidden) {
		g_ptr_array_add (arr, g_strdup (DASHES TOTEM_OPTION_HIDDEN));
	}

 	if (mRepeat) {
 		g_ptr_array_add (arr, g_strdup (DASHES TOTEM_OPTION_REPEAT));
 	}

	if (mNoAutostart) {
		g_ptr_array_add (arr, g_strdup (DASHES TOTEM_OPTION_NOAUTOSTART));
	}
 
 	if (mIsPlaylist) {
 		g_ptr_array_add (arr, g_strdup (DASHES TOTEM_OPTION_PLAYLIST));
 		g_ptr_array_add (arr, g_strdup (mLocal));
 	} else {
		/* mLocal is only TRUE for playlists */
		if (mIsSupportedSrc == FALSE) {
			g_ptr_array_add (arr, g_strdup (mSrc));
		} else {
			g_ptr_array_add (arr, g_strdup ("fd://0"));
			use_fd = TRUE;
		}
	}

	g_ptr_array_add (arr, NULL);
	argv = (char **) g_ptr_array_free (arr, FALSE);

#ifdef GNOME_ENABLE_DEBUG
	{
		GString *s;
		int i;

		s = g_string_new ("Launching: ");
		for (i = 0; argv[i] != NULL; i++) {
			g_string_append (s, argv[i]);
			g_string_append (s, " ");
		}
		g_string_append (s, "\n");
		D("%s", s->str);
		g_string_free (s, TRUE);
	}
#endif

	if (g_spawn_async_with_pipes (NULL, argv, NULL,
				      GSpawnFlags(0), NULL, NULL, &mPlayerPID,
				      use_fd ? &mSendFD : NULL, NULL, NULL, &err) == FALSE)
	{
		D("Spawn failed");

		if (err) {
			g_warning ("%s\n", err->message);
			g_error_free(err);
		}

		g_strfreev (argv);
		return FALSE;
	}

	g_strfreev (argv);

	D("player PID: %d", mPlayerPID);

	/* FIXME: not >= 0 ? */
	if (mSendFD > 0)
		fcntl(mSendFD, F_SETFL, O_NONBLOCK);

	/* now wait until startup is complete */
	mGotSvc = FALSE;
	mWaitForSvc = g_strdup_printf
		("org.totem_%d.MozillaPluginService", mPlayerPID);
	D("waiting for signal %s", mWaitForSvc);
	dbus_g_proxy_add_signal (mProxy, "NameOwnerChanged",
				 G_TYPE_STRING, G_TYPE_STRING,
				 G_TYPE_STRING, G_TYPE_INVALID);
	dbus_g_proxy_connect_signal (mProxy, "NameOwnerChanged",
				     G_CALLBACK (NameOwnerChangedCallback),
				     this, NULL);

	/* Store this in a local, since we cannot access mScriptable if we get deleted in the loop */
	totemScriptablePlugin *scriptable = mScriptable;
	g_get_current_time (&then);
	g_time_val_add (&then, G_USEC_PER_SEC * 5);
	NS_ADDREF (scriptable);
	/* FIXME FIXME! this loop is evil! */
	do {
		g_main_context_iteration (NULL, TRUE);
		g_get_current_time (&now);
	} while (scriptable->IsValid () &&
		 !mGotSvc &&
		 (now.tv_sec <= then.tv_sec));

	if (!scriptable->IsValid ()) {
		/* we were destroyed in one of the iterations of the
		 * mainloop, get out ASAP */
		D("We no longer exist");
		NS_RELEASE (scriptable);
		return FALSE;
	}
	NS_RELEASE (scriptable);

	dbus_g_proxy_disconnect_signal (mProxy, "NameOwnerChanged",
					G_CALLBACK (NameOwnerChangedCallback), this);
	if (!mGotSvc) {
		g_warning ("Failed to receive DBUS interface response");
		g_free (mWaitForSvc);
		mWaitForSvc = nsnull;

		if (mPlayerPID) {
			kill (mPlayerPID, SIGKILL);
			g_spawn_close_pid (mPlayerPID);
			mPlayerPID = 0;
		}
		return FALSE;
	}
	g_object_unref (mProxy);

	/* now get the proxy for the player functions */
	mProxy =
		dbus_g_proxy_new_for_name (mConn, mWaitForSvc,
					   "/TotemEmbedded",
					   "org.totem.MozillaPluginInterface");
	dbus_g_proxy_add_signal (mProxy, "StopSendingData",
				 G_TYPE_INVALID);
	dbus_g_proxy_connect_signal (mProxy, "StopSendingData",
				     G_CALLBACK (StopSendingDataCallback),
				     this, NULL);

	g_free (mWaitForSvc);
	mWaitForSvc = nsnull;
	D("Done forking, new proxy=%p", mProxy);

	return TRUE;
}

static char *
resolve_relative_uri (nsIURI *docURI,
		      const char *uri)
{
	if (docURI) {
		nsresult rv;
		nsEmbedCString resolved;
		rv = docURI->Resolve (nsEmbedCString (uri), resolved);
		if (NS_SUCCEEDED (rv)) {
			return g_strdup (resolved.get());
		}
	}

	return g_strdup (uri);
}

#ifdef TOTEM_NARROWSPACE_PLUGIN
static char *
parse_url_extensions (char *href)
{
	char *uri, *s1, *s2;

	g_return_val_if_fail (href != NULL || href[0] != '<', NULL);

	s1 = href + 1;
	s2 = strchr (s1, '>');
	if (s2 == NULL)
		return g_strdup (href);

	uri = g_strndup (s1, s2 - s1);

	return uri;
}
#endif

gboolean
totem_parse_boolean (char *key, char *value, gboolean default_val)
{
	if (value == NULL || strcmp (value, "") == 0)
		return default_val;
	if (g_ascii_strcasecmp (value, "false") == 0
			|| g_ascii_strcasecmp (value, "0") == 0)
		return FALSE;
	if (g_ascii_strcasecmp (value, "true") == 0
			|| g_ascii_strcasecmp (value, "1") == 0)
		return TRUE;

	g_warning ("Unknown value '%s' for parameter '%s'", value, key);
	return default_val;
}

gboolean
totem_get_boolean_value (GHashTable *args, char *key, gboolean default_val)
{
	char *value;

	value = (char *) g_hash_table_lookup (args, key);
	if (value == NULL)
		return default_val;
	return totem_parse_boolean (key, value, default_val);
}

NPError
totemPlugin::Init (NPMIMEType mimetype,
		   uint16_t mode,
		   int16_t argc,
		   char *argn[],
		   char *argv[],
		   NPSavedData *saved)
{
	GError *e = NULL;
	gboolean need_req = FALSE;
	int i;
	GHashTable *args;
	char *value;

	mScriptable = new totemScriptablePlugin (this);
	if (!mScriptable)
		return NPERR_OUT_OF_MEMORY_ERROR;

	NS_ADDREF (mScriptable);

	if (!(mConn = dbus_g_bus_get (DBUS_BUS_SESSION, &e))) {
		printf ("Failed to open DBUS session: %s\n", e->message);
		g_error_free (e);
		return NPERR_OUT_OF_MEMORY_ERROR;
	} else if (!(mProxy = dbus_g_proxy_new_for_name (mConn,
					"org.freedesktop.DBus",
					"/org/freedesktop/DBus",
					"org.freedesktop.DBus"))) {
		printf ("Failed to open DBUS proxy: %s\n", e->message);
		g_error_free (e);
		return NPERR_OUT_OF_MEMORY_ERROR;
	}

	/* mode is NP_EMBED, NP_FULL, or NP_BACKGROUND (see npapi.h) */
	//FIXME we should error out if we are in fullscreen mode
	printf("mode %d\n",mode);
	printf("mime type: %s\n", mimetype);

	/* to resolve relative URLs */
	nsIDOMWindow *domWin = nsnull;
	sMozillaFuncs.getvalue (mInstance, NPNVDOMWindow,
				    NS_REINTERPRET_CAST (void**, &domWin));
	nsIInterfaceRequestor *irq = nsnull;
	if (domWin) {
		domWin->QueryInterface (NS_GET_IID (nsIInterfaceRequestor),
					NS_REINTERPRET_CAST (void**, &irq));
		NS_RELEASE (domWin);
	}

	nsIWebNavigation *webNav = nsnull;
	if (irq) {
		irq->GetInterface (NS_GET_IID (nsIWebNavigation),
				   NS_REINTERPRET_CAST (void**, &webNav));
		NS_RELEASE (irq);
	}

	nsIURI *docURI = nsnull;
	if (webNav) {
		webNav->GetCurrentURI (&docURI);
		NS_RELEASE (webNav);
	}

	/* Find the "real" mime-type */
	mMimeType = GetRealMimeType (mimetype);

	args = g_hash_table_new_full (g_str_hash, g_str_equal,
			(GDestroyNotify) g_free, (GDestroyNotify) g_free);
	for (i = 0; i < argc; i++) {
		printf ("argv[%d] %s %s\n", i, argn[i], argv[i]);
		g_hash_table_insert (args, g_ascii_strdown (argn[i], -1),
				g_strdup (argv[i]));
	}

	/* The QuickTime href system, with the src video just being a link
	 * to this video */
#ifdef TOTEM_NARROWSPACE_PLUGIN
	/* New URLs extensions:
	 * http://developer.apple.com/documentation/QuickTime/WhatsNewQT5/QT5NewChapt1/chapter_1_section_32.html
	 * http://developer.apple.com/documentation/QuickTime/Conceptual/QTScripting_HTML/QTScripting_HTML_Document/chapter_1000_section_3.html */

	value = (char *) g_hash_table_lookup (args, "href");
	if (value != NULL) {
		if (value[0] == '<') {
			//FIXME fill the hashtable with the new values
			//from the extended URLs
			char *href = parse_url_extensions (value);

			//FIXME
			// http://developer.apple.com/documentation/QuickTime/Conceptual/QTScripting_HTML/QTScripting_HTML_Document/chapter_1000_section_3.html
			// "Important: If you pass a relative URL in the HREF parameter, it must be relative to the currentlyloadedmovie, not relative to the current web page. If your movies are in a separate folder, specify URLs relative to the movies folder."
			mHref = resolve_relative_uri (docURI, href);
			g_free (href);
		} else {
			//FIXME see above
			mHref = resolve_relative_uri (docURI, value);
		}
	}
#endif /* TOTEM_NARROWSPACE_PLUGIN */

	value = (char *) g_hash_table_lookup (args, "width");
	if (value != NULL) {
		/* FIXME! sanitize this value */
		mWidth = strtol (value, NULL, 0);
	}

	value = (char *) g_hash_table_lookup (args, "height");
	if (value != NULL) {
		/* FIXME! sanitize this value */
		mHeight = strtol (value, NULL, 0);
	}

	//FIXME get the base URL here

	value = (char *) g_hash_table_lookup (args, "src");
	if (value != NULL)
		mSrc = resolve_relative_uri (docURI, value);
	/* DATA is only used in OBJECTs, see:
	 * http://developer.mozilla.org/en/docs/Gecko_Plugin_API_Reference:Plug-in_Basics#Plug-in_Display_Modes */
	if (mSrc == NULL) {
		value = (char *) g_hash_table_lookup (args, "data");
		if (value != NULL)
			mSrc = resolve_relative_uri (docURI, value);
	}

	/* Those parameters might replace the current src */
#ifdef TOTEM_GMP_PLUGIN
	/* http://windowssdk.msdn.microsoft.com/en-us/library/aa392440(VS.80).aspx */
	value = (char *) g_hash_table_lookup (args, "filename");
	if (value == NULL)
		value = (char *) g_hash_table_lookup (args, "url");
#elif TOTEM_NARROWSPACE_PLUGIN
	/* http://developer.apple.com/documentation/QuickTime/QT6WhatsNew/Chap1/chapter_1_section_13.html */
	value = (char *) g_hash_table_lookup (args, "qtsrc");
#elif TOTEM_MULLY_PLUGIN
	/* Click to play behaviour of the DivX plugin */
	value = (char *) g_hash_table_lookup (args, "previewimage");
	if (value != NULL)
		mHref = g_strdup (mSrc);
#else
	value = NULL;
#endif
	if (value != NULL) {
		if (mSrc == NULL || strcmp (mSrc, value) != 0) {
			//FIXME need to cancel SRC if there's one
			g_free (mSrc);
			mSrc = resolve_relative_uri (docURI, value);
			need_req = TRUE;
		}
	}

#ifdef TOTEM_NARROWSPACE_PLUGIN
	/* Caching behaviour */
	mCache = totem_get_boolean_value (args, "cache", FALSE);
	/* Target */
	value = (char *) g_hash_table_lookup (args, "target");
	if (value != NULL)
		mTarget = g_strdup (value);
#endif /* TOTEM_NARROWSPACE_PLUGIN */

	/* Whether the controls are all hidden, MSIE parameter
	 * http://www.htmlcodetutorial.com/embeddedobjects/_EMBED_CONTROLLER.html */
	gboolean controller;
	controller = totem_get_boolean_value (args, "controller", TRUE);
	mControllerHidden = (controller == FALSE);

	//FIXME add Netscape controls support
	// http://www.htmlcodetutorial.com/embeddedobjects/_EMBED_CONTROLS.html

	/* Are we hidden? */
	mHidden = totem_get_boolean_value (args, "hidden", FALSE);

#ifdef TOTEM_GMP_PLUGIN
	/* uimode is either invisible, none, mini, or full
	 * http://windowssdk.msdn.microsoft.com/en-us/library/aa392439(VS.80).aspx */
	value = (char *) g_hash_table_lookup (args, "uimode");
	if (value != NULL) {
		if (g_ascii_strcasecmp (value, "none") == 0) {
			mControllerHidden = TRUE;
		} else if (g_ascii_strcasecmp (value, "invisible") == 0) {
			mHidden = TRUE;
		} else if (g_ascii_strcasecmp (value, "full") == 0) {
			mStatusbar = TRUE;
		} else if (g_ascii_strcasecmp (value, "mini") == 0) {
			;
		}
	}
	/* ShowXXX parameters as per http://support.microsoft.com/kb/285154 */
	controller = totem_get_boolean_value (args, "showcontrols", TRUE);
	if (mControllerHidden == FALSE)
		mControllerHidden = (controller == FALSE);

	gboolean statusbar;
	statusbar = totem_get_boolean_value (args, "showstatusbar", mStatusbar);
	mStatusbar = (statusbar == FALSE);
	//FIXME add showdisplay
	// see http://msdn.microsoft.com/library/default.asp?url=/library/en-us/wmp6sdk/htm/userinterfaceelements.asp
#endif /* TOTEM_GMP_PLUGIN */

	/* Whether to NOT autostart */
	//FIXME Doesn't handle playcount, or loop with numbers
	// http://www.htmlcodetutorial.com/embeddedobjects/_EMBED_LOOP.html
	gboolean autostart;
	autostart = totem_get_boolean_value (args, "autostart", TRUE);
	autostart = totem_get_boolean_value (args, "autoplay", autostart);
	mNoAutostart = (autostart == FALSE);

	/* Whether to loop */
	mRepeat = totem_get_boolean_value (args, "loop", FALSE);
	mRepeat = totem_get_boolean_value (args, "repeat", mRepeat);

	g_message ("mSrc: %s", mSrc);
	g_message ("mHref: %s", mHref);
	g_message ("mHeight: %d mWidth: %d", mHeight, mWidth);
	g_message ("mCache: %d", mCache);
	g_message ("mTarget: %s", mTarget);
	g_message ("mControllerHidden: %d", mControllerHidden);
	g_message ("mStatusbar: %d", mStatusbar);
	g_message ("mHidden: %d", mHidden);
	g_message ("mNoAutostart: %d mRepeat: %d", mNoAutostart, mRepeat);

	//FIXME handle starttime and endtime
	// http://www.htmlcodetutorial.com/embeddedobjects/_EMBED_STARTTIME.html

	g_hash_table_destroy (args);
	NS_IF_RELEASE (docURI);

	mIsSupportedSrc = IsSchemeSupported (mSrc);

	/* If filename is used, we need to request the stream ourselves */
	if (need_req != FALSE) {
		if (mIsSupportedSrc != FALSE) {
			CallNPN_GetURLProc(sMozillaFuncs.geturl,
					mInstance, mSrc, NULL);
		}
	}

	return NPERR_NO_ERROR;
}

NPError
totemPlugin::SetWindow (NPWindow *window)
{
	D("SetWindow [%p]", (void*) this);

	if (mWindow) {
		D ("existing window");
		if (mWindow == (Window) window->window) {
			D("resize");
			/* Resize event */
			/* Not currently handled */
		} else {
			D("change");
			printf ("ack.  window changed!\n");
		}
	} else {
		/* If not, wait for the first bits of data to come,
		 * but not when the stream type isn't supported */
		mWindow = (Window) window->window;
		if (mStream && mIsSupportedSrc) {
			if (!Fork ())
				return NPERR_GENERIC_ERROR;
		/* If it's not a supported stream, we need
		 * to launch now */
		} else if (!mIsSupportedSrc) {
			if (!Fork ())
				return NPERR_GENERIC_ERROR;
		} else {
			D("waiting for data to come");
		}
	}

	D("leaving plugin_set_window");

	return NPERR_NO_ERROR;
}

NPError
totemPlugin::NewStream (NPMIMEType type,
			NPStream* stream_ptr,
			NPBool seekable,
			uint16* stype)
{
	D("plugin_new_stream");

	/* We already have a live stream */
	if (mStream) {
		D("plugin_new_stream exiting, already have a live stream");
		return NPERR_GENERIC_ERROR;
	}

	D("plugin_new_stream type: %s url: %s", type, mSrc);

	if (IsMimeTypeSupported (type, mSrc) == FALSE) {
		D("plugin_new_stream type: %s not supported, exiting\n", type);
		return NPERR_INVALID_PLUGIN_ERROR;
	}

	//FIXME need to find better semantics?
	//what about saving the state, do we get confused?
	if (g_str_has_prefix (mSrc, "file://")) {
		*stype = NP_ASFILEONLY;
		mStreamType = NP_ASFILEONLY;
	} else {
		*stype = NP_ASFILE;
		mStreamType = NP_ASFILE;
	}

	mStream = stream_ptr;

	return NPERR_NO_ERROR;
}

NPError
totemPlugin::DestroyStream (NPStream* stream,
			    NPError reason)
{
	D("plugin_destroy_stream, reason: %d", reason);

	if (mSendFD >= 0) {
		close(mSendFD);
		mSendFD = -1;
	}

	mStream = nsnull;

	return NPERR_NO_ERROR;
}

int32
totemPlugin::WriteReady (NPStream *stream)
{
	//D("plugin_write_ready");

	if (mIsSupportedSrc == FALSE)
		return 0; /* FIXME not -1 ? */

	if (mSendFD < 0)
		return (PLUGIN_STREAM_CHUNK_SIZE);

	struct pollfd fds;
	fds.events = POLLOUT;
	fds.fd = mSendFD;
	if (poll (&fds, 1, 0) > 0)
		return (PLUGIN_STREAM_CHUNK_SIZE);

	return 0;
}

int32
totemPlugin::Write (NPStream *stream,
		    int32 offset,
		    int32 len,
		    void *buffer)
{
	int ret;

	//D("plugin_write");

	/* We already know it's a playlist, don't try to check it again
	 * and just wait for it to be on-disk */
	if (mIsPlaylist != FALSE)
		return len;

	if (!mPlayerPID) {
		if (!mStream) {
			g_warning ("No stream in NPP_Write!?");
			return -1;
		}

		mTriedWrite = TRUE;

		/* FIXME this looks wrong since it'll look at the current data buffer,
		 * not the cumulative data since the stream started 
		 */
		if (totem_pl_parser_can_parse_from_data ((const char *) buffer, len, TRUE /* FIXME */) != FALSE) {
			D("Need to wait for the file to be downloaded completely");
			mIsPlaylist = TRUE;
			return len;
		}

		if (!Fork ())
			return -1;
	}

	if (mSendFD < 0)
		return -1;

	if (mIsSupportedSrc == FALSE)
		return -1;

	ret = write (mSendFD, buffer, len);
	if (ret < 0) {
		/* FIXME what's this for? gecko will destroy the stream automatically if we return -1 */
		int err = errno;
		D("ret %d: [%d]%s", ret, errno, g_strerror (err));
		if (err == EPIPE) {
			/* fd://0 got closed, probably because the backend
			 * crashed on us */
			if (CallNPN_DestroyStreamProc
					(sMozillaFuncs.destroystream,
					 mInstance,
					 mStream,
					 NPRES_DONE) != NPERR_NO_ERROR) {
				g_warning ("Couldn't destroy the stream");
			}
		}
		/* FIXME shouldn't we set ret=0 here? otherwise the stream will be destroyed */
	}

	return ret;
}

void
totemPlugin::StreamAsFile (NPStream *stream,
			   const char* fname)
{
	NS_ASSERTION (stream == mStream, "Unknown stream");

	D("plugin_stream_as_file: %s", fname);

	if (!mTriedWrite) {
		mIsPlaylist = totem_pl_parser_can_parse_from_filename
			(fname, TRUE);
	}

	if (!mPlayerPID && mIsPlaylist) {
		mLocal = g_filename_to_uri (fname, NULL, NULL);
		Fork ();
		return;
	} else if (!mPlayerPID) {
		if (!Fork ())
			return;
	}

	if (mIsPlaylist != FALSE)
		return;

	GError *err = NULL;
	if (!dbus_g_proxy_call (mProxy, "SetLocalFile", &err,
			G_TYPE_STRING, fname, G_TYPE_INVALID,
			G_TYPE_INVALID)) {
		g_warning ("Error: %s", err->message);
		g_error_free (err);
	}

	D("plugin_stream_as_file done");
}

NPError
totemPlugin::GetScriptable (void *_retval)
{
	D ("GetScriptable [%p], mInstance %p", (void*) this, mInstance);

	/* FIXME this shouldn't happen, but seems to happen nevertheless?!? */
	if (!mScriptable)
		return NPERR_INVALID_PLUGIN_ERROR;

	nsresult rv = mScriptable->QueryInterface (NS_GET_IID (nsISupports),
						   NS_REINTERPRET_CAST (void **, _retval));

	return NS_SUCCEEDED (rv) ? NPERR_NO_ERROR : NPERR_GENERIC_ERROR;
}
