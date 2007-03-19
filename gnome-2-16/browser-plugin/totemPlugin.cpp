/* Totem Mozilla plugin
 * 
 * Copyright (C) 2004-2006 Bastien Nocera <hadess@hadess.net>
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
#include <dlfcn.h>

#include <glib.h>
#include <libgnomevfs/gnome-vfs-mime-handlers.h>
#include <libgnomevfs/gnome-vfs-mime-info.h>
#include <libgnomevfs/gnome-vfs-utils.h>

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

#if defined(TOTEM_BASIC_PLUGIN)
#include "totemBasicPlugin.h"
#elif defined(TOTEM_GMP_PLUGIN)
#include "totemGMPPlugin.h"
#elif defined(TOTEM_COMPLEX_PLUGIN)
#include "totemComplexPlugin.h"
#elif defined(TOTEM_NARROWSPACE_PLUGIN)
#include "totemNarrowSpacePlugin.h"
#elif defined(TOTEM_MULLY_PLUGIN)
#include "totemMullYPlugin.h"
#else
#error Unknown plugin type
#endif

/* How much data bytes to request */
#define PLUGIN_STREAM_CHUNK_SIZE (8 * 1024)

static NPNetscapeFuncs mozilla_functions;
static char *mime_list = NULL;

/* public functions */

void
totem_plugin_play (totemPlugin *plugin)
{
	D ("play");
	dbus_g_proxy_call (plugin->proxy, "Play", NULL,
			   G_TYPE_INVALID, G_TYPE_INVALID);
}

void
totem_plugin_stop (totemPlugin *plugin)
{
	D ("stop");
	dbus_g_proxy_call (plugin->proxy, "Stop", NULL,
			   G_TYPE_INVALID, G_TYPE_INVALID);
}

void
totem_plugin_pause (totemPlugin *plugin)
{
	D ("pause");
	dbus_g_proxy_call (plugin->proxy, "Pause", NULL,
			   G_TYPE_INVALID, G_TYPE_INVALID);
}

/* private functions */

static void
cb_update_name (DBusGProxy *proxy,
		const char *svc,
		const char *old_owner,
		const char *new_owner,
		totemPlugin *plugin)
{
	D("Received notification for %s\n", svc);
	if (strcmp (svc, plugin->wait_for_svc) == 0) {
		plugin->got_svc = TRUE;
	}
}

static void
cb_stop_sending_data (DBusGProxy  *proxy,
		      totemPlugin *plugin)
{
	if (!plugin->stream)
		return;

	D("Stop sending data signal received");
	if (CallNPN_DestroyStreamProc (mozilla_functions.destroystream,
				plugin->instance,
				plugin->stream,
				NPRES_DONE) != NPERR_NO_ERROR) {
		g_warning ("Couldn't destroy the stream");
		return;
	}

	plugin->stream = nsnull;
}

static char *
get_real_mimetype (const char *mimetype)
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

	g_warning ("Real mime-type for '%s' not found\n", mimetype);
	return NULL;
}

static gboolean
is_supported_scheme (const char *url)
{
	if (url == NULL)
		return FALSE;

	if (g_str_has_prefix (url, "mms:") != FALSE)
		return FALSE;
	if (g_str_has_prefix (url, "rtsp:") != FALSE)
		return FALSE;

	return TRUE;
}

static gboolean
totem_plugin_fork (totemPlugin *plugin)
{
	GTimeVal then, now;
	char *svcname;
	GPtrArray *arr;
	char **argv;
	GError *err = NULL;
	totemScriptablePlugin *scriptable = plugin->scriptable;
	gboolean use_fd = FALSE;

	/* Make sure we don't get both an XID and hidden */
	if (plugin->window && plugin->hidden) {
		g_warning ("Both hidden and a window!");
		return FALSE;
	}

	arr = g_ptr_array_new ();

	if (g_file_test ("./totem-mozilla-viewer",
				G_FILE_TEST_EXISTS) != FALSE) {
		g_ptr_array_add (arr, g_strdup ("./totem-mozilla-viewer"));
	} else {
		g_ptr_array_add (arr,
				g_strdup (LIBEXECDIR"/totem-mozilla-viewer"));
	}

	/* For the RealAudio streams */
	if (plugin->width == 0 && plugin->height == 0) {
		plugin->window = 0;
		plugin->hidden = TRUE;
	}

	if (plugin->window) {
		g_ptr_array_add (arr, g_strdup (TOTEM_OPTION_XID));
		g_ptr_array_add (arr, g_strdup_printf ("%lu", plugin->window));
	}

	if (plugin->width > 0) {
		g_ptr_array_add (arr, g_strdup (TOTEM_OPTION_WIDTH));
		g_ptr_array_add (arr, g_strdup_printf ("%d", plugin->width));
	}

	if (plugin->height > 0) {
		g_ptr_array_add (arr, g_strdup (TOTEM_OPTION_HEIGHT));
		g_ptr_array_add (arr, g_strdup_printf ("%d", plugin->height));
	}

	if (plugin->src) {
		g_ptr_array_add (arr, g_strdup (TOTEM_OPTION_URL));
		g_ptr_array_add (arr, g_strdup (plugin->src));
	}

	if (plugin->href) {
		g_ptr_array_add (arr, g_strdup (TOTEM_OPTION_HREF));
		g_ptr_array_add (arr, g_strdup (plugin->href));
	}

	if (plugin->target) {
		g_ptr_array_add (arr, g_strdup (TOTEM_OPTION_TARGET));
		g_ptr_array_add (arr, g_strdup (plugin->target));
	}

	if (plugin->mimetype) {
		g_ptr_array_add (arr, g_strdup (TOTEM_OPTION_MIMETYPE));
		g_ptr_array_add (arr, g_strdup (plugin->mimetype));
	}

	if (plugin->controller_hidden) {
		g_ptr_array_add (arr, g_strdup (TOTEM_OPTION_CONTROLS_HIDDEN));
	}

	if (plugin->hidden) {
		g_ptr_array_add (arr, g_strdup (TOTEM_OPTION_HIDDEN));
	}

 	if (plugin->repeat) {
 		g_ptr_array_add (arr, g_strdup (TOTEM_OPTION_REPEAT));
 	}

	if (plugin->noautostart) {
		g_ptr_array_add (arr, g_strdup (TOTEM_OPTION_NOAUTOSTART));
	}
 
 	if (plugin->is_playlist) {
 		g_ptr_array_add (arr, g_strdup (TOTEM_OPTION_PLAYLIST));
 		g_ptr_array_add (arr, g_strdup (plugin->local));
 	} else {
		/* plugin->local is only TRUE for playlists */
		if (plugin->is_supported_src == FALSE) {
			g_ptr_array_add (arr, g_strdup (plugin->src));
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
				(GSpawnFlags) 0, NULL, NULL, &plugin->player_pid,
				use_fd ? &plugin->send_fd : NULL, NULL, NULL, &err) == FALSE)
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

	if (plugin->send_fd > 0)
		fcntl(plugin->send_fd, F_SETFL, O_NONBLOCK);

	/* now wait until startup is complete */
	plugin->got_svc = FALSE;
	plugin->wait_for_svc = g_strdup_printf
		("org.totem_%d.MozillaPluginService", plugin->player_pid);
	D("waiting for signal %s", plugin->wait_for_svc);
	dbus_g_proxy_add_signal (plugin->proxy, "NameOwnerChanged",
				 G_TYPE_STRING, G_TYPE_STRING,
				 G_TYPE_STRING, G_TYPE_INVALID);
	dbus_g_proxy_connect_signal (plugin->proxy, "NameOwnerChanged",
				     G_CALLBACK (cb_update_name),
				     plugin, NULL);
	g_get_current_time (&then);
	g_time_val_add (&then, G_USEC_PER_SEC * 5);
	NS_ADDREF (scriptable);
	do {
		g_main_context_iteration (NULL, TRUE);
		g_get_current_time (&now);
	} while (scriptable->IsValid () && !plugin->got_svc &&
		 (now.tv_sec <= then.tv_sec));

	if (!scriptable->IsValid ()) {
		/* we were destroyed in one of the iterations of the
		 * mainloop, get out ASAP */
		D("We no longer exist");
		NS_RELEASE (scriptable);
		return FALSE;
	}
	NS_RELEASE (scriptable);
	dbus_g_proxy_disconnect_signal (plugin->proxy, "NameOwnerChanged",
					G_CALLBACK (cb_update_name), plugin);
	if (!plugin->got_svc) {
		g_warning ("Failed to receive DBUS interface response\n");
		g_free (plugin->wait_for_svc);

		if (plugin->player_pid) {
			kill (plugin->player_pid, SIGKILL);
			g_spawn_close_pid (plugin->player_pid);
			plugin->player_pid = 0;
		}
		return FALSE;
	}
	g_object_unref (plugin->proxy);

	/* now get the proxy for the player functions */
	plugin->proxy =
		dbus_g_proxy_new_for_name (plugin->conn, plugin->wait_for_svc,
					   "/TotemEmbedded",
					   "org.totem.MozillaPluginInterface");
	dbus_g_proxy_add_signal (plugin->proxy, "StopSendingData",
				 G_TYPE_INVALID);
	dbus_g_proxy_connect_signal (plugin->proxy, "StopSendingData",
				     G_CALLBACK (cb_stop_sending_data),
				     plugin, NULL);

	g_free (plugin->wait_for_svc);
	D("Done forking, new proxy=%p", plugin->proxy);

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

static NPError
totem_plugin_new_instance (NPMIMEType mimetype,
			   NPP instance,
			   uint16_t mode,
			   int16_t argc,
			   char *argn[],
			   char *argv[],
			   NPSavedData *saved)
{
	totemPlugin *plugin;
	GError *e = NULL;
	gboolean need_req = FALSE;
	int i;

	D("totem_plugin_new_instance");

	if (instance == NULL)
		return NPERR_INVALID_INSTANCE_ERROR;

	instance->pdata = mozilla_functions.memalloc(sizeof(totemPlugin));
	plugin = (totemPlugin *) instance->pdata;

	if (plugin == NULL)
		return NPERR_OUT_OF_MEMORY_ERROR;

	memset(plugin, 0, sizeof(totemPlugin));
	plugin->scriptable = new totemScriptablePlugin (plugin);
	if (!plugin->scriptable) {
		mozilla_functions.memfree (plugin);
		return NPERR_OUT_OF_MEMORY_ERROR;
	}
	NS_ADDREF (plugin->scriptable);
	if (!(plugin->conn = dbus_g_bus_get (DBUS_BUS_SESSION, &e))) {
		printf ("Failed to open DBUS session: %s\n", e->message);
		g_error_free (e);
		NS_RELEASE (plugin->scriptable);
		mozilla_functions.memfree (plugin);
		return NPERR_OUT_OF_MEMORY_ERROR;
	} else if (!(plugin->proxy = dbus_g_proxy_new_for_name (plugin->conn,
					"org.freedesktop.DBus",
					"/org/freedesktop/DBus",
					"org.freedesktop.DBus"))) {
		printf ("Failed to open DBUS proxy: %s\n", e->message);
		g_error_free (e);
		g_object_unref (G_OBJECT (plugin->conn));
		NS_RELEASE (plugin->scriptable);
		mozilla_functions.memfree (plugin);
		return NPERR_OUT_OF_MEMORY_ERROR;
	}

	/* mode is NP_EMBED, NP_FULL, or NP_BACKGROUND (see npapi.h) */
	//FIXME we should error out if we are in fullscreen mode
	printf("mode %d\n",mode);
	printf("mime type: %s\n", mimetype);
	plugin->instance = instance;
	plugin->send_fd = -1;
	plugin->width = plugin->height = -1;

	/* to resolve relative URLs */
	nsIDOMWindow *domWin = nsnull;
	mozilla_functions.getvalue (instance, NPNVDOMWindow,
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

	/* Set the default cache behaviour */
	if (strcmp (mimetype, "video/quicktime") != 0) {
		plugin->cache = TRUE;
	}
	/* Find the "real" mime-type */
	plugin->mimetype = get_real_mimetype (mimetype);

	for (i = 0; i < argc; i++) {
		printf ("argv[%d] %s %s\n", i, argn[i], argv[i]);
		if (g_ascii_strcasecmp (argn[i],"width") == 0) {
			plugin->width = strtol (argv[i], NULL, 0);
		} else if (g_ascii_strcasecmp (argn[i], "height") == 0) {
			plugin->height = strtol (argv[i], NULL, 0);
		} else if (g_ascii_strcasecmp (argn[i], "src") == 0) {
			plugin->src = resolve_relative_uri (docURI, argv[i]);
		} else if (g_ascii_strcasecmp (argn[i], "filename") == 0) {
			/* Windows Media Player parameter */
			if (plugin->src == NULL) {
				plugin->src = resolve_relative_uri (docURI, argv[i]);
				need_req = TRUE;
			}
		} else if (g_ascii_strcasecmp (argn[i], "href") == 0) {
			plugin->href = resolve_relative_uri (docURI, argv[i]);
		} else if (g_ascii_strcasecmp (argn[i], "cache") == 0) {
			plugin->cache = TRUE;
			if (g_ascii_strcasecmp (argv[i], "false") == 0) {
				plugin->cache = FALSE;
			}
		} else if (g_ascii_strcasecmp (argn[i], "target") == 0) {
			plugin->target = g_strdup (argv[i]);
		} else if (g_ascii_strcasecmp (argn[i], "controller") == 0) {
			/* Quicktime parameter */
			if (g_ascii_strcasecmp (argv[i], "false") == 0) {
				plugin->controller_hidden = TRUE;
			}
			//FIXME see http://www.htmlcodetutorial.com/embeddedobjects/_EMBED_CONTROLS.html
		} else if (g_ascii_strcasecmp (argn[i], "uimode") == 0) {
			/* Windows Media Player parameter */
			if (g_ascii_strcasecmp (argv[i], "none") == 0) {
				plugin->controller_hidden = TRUE;
			}
		} else if (g_ascii_strcasecmp (argn[i], "showcontrols") == 0) {
			if (g_ascii_strcasecmp (argv[i], "false") == 0) {
				plugin->controller_hidden = TRUE;
			}
		} else if (g_ascii_strcasecmp (argn[i], "hidden") == 0) {
			if (g_ascii_strcasecmp (argv[i], "false") != 0) {
				plugin->hidden = TRUE;
			}
		} else if (g_ascii_strcasecmp (argn[i], "autostart") == 0
				|| g_ascii_strcasecmp (argn[i], "autoplay") == 0) {
			if (g_ascii_strcasecmp (argv[i], "false") == 0
					|| g_ascii_strcasecmp (argv[i], "0") == 0) {
				plugin->noautostart = TRUE;
			}
		} else if (g_ascii_strcasecmp (argn[i], "loop") == 0
				|| g_ascii_strcasecmp (argn[i], "repeat") == 0) {
			if (g_ascii_strcasecmp (argv[i], "true") == 0) {
				plugin->repeat = TRUE;
			}

			// FIXME Doesn't handle playcount, or loop with numbers
			// http://www.htmlcodetutorial.com/embeddedobjects/_EMBED_LOOP.html
		} else if (g_ascii_strcasecmp (argn[i], "starttime") == 0) {
			//FIXME see http://www.htmlcodetutorial.com/embeddedobjects/_EMBED_STARTTIME.html
		} else if (g_ascii_strcasecmp (argn[i], "endtime") == 0) {
			//FIXME see above
		}
	}

	NS_IF_RELEASE (docURI);

	plugin->is_supported_src = is_supported_scheme (plugin->src);

	/* If filename is used, we need to request the stream ourselves */
	if (need_req == TRUE) {
		if (plugin->is_supported_src != FALSE) {
			CallNPN_GetURLProc(mozilla_functions.geturl,
					instance, plugin->src, NULL);
		}
	}

	return NPERR_NO_ERROR;
}

static NPError
totem_plugin_destroy_instance (NPP instance,
			       NPSavedData **save)
{
	D("plugin_destroy");

	if (instance == NULL)
		return NPERR_INVALID_INSTANCE_ERROR;

	totemPlugin *plugin = (totemPlugin *) instance->pdata;
	if (plugin == NULL)
		return NPERR_NO_ERROR;

	if (!plugin || !plugin->scriptable || !plugin->scriptable->IsValid ())
		return NPERR_INVALID_INSTANCE_ERROR;

	plugin->scriptable->UnsetPlugin ();

	if (plugin->send_fd >= 0) {
		close (plugin->send_fd);
		plugin->send_fd = -1;
	}

	if (plugin->player_pid) {
		kill (plugin->player_pid, SIGKILL);
		g_spawn_close_pid (plugin->player_pid);
		plugin->player_pid = 0;
	}

	NS_RELEASE (plugin->scriptable);

	g_free (plugin->local);
	g_free (plugin->target);
	g_free (plugin->src);
	g_free (plugin->href);

	g_object_unref (G_OBJECT (plugin->proxy));
	//g_object_unref (G_OBJECT (plugin->conn));
	mozilla_functions.memfree (instance->pdata);
	instance->pdata = NULL;

	return NPERR_NO_ERROR;
}

static NPError
totem_plugin_set_window (NPP instance,
			 NPWindow* window)
{
	D("plugin_set_window");

	if (instance == NULL)
		return NPERR_INVALID_INSTANCE_ERROR;

	totemPlugin *plugin = (totemPlugin *) instance->pdata;
	if (plugin == NULL)
		return NPERR_INVALID_INSTANCE_ERROR;

	if (plugin->window) {
		D ("existing window");
		if (plugin->window == (Window) window->window) {
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
		plugin->window = (Window) window->window;
		if (plugin->stream && plugin->is_supported_src) {
			if (!totem_plugin_fork (plugin))
				return NPERR_GENERIC_ERROR;
		} else {
			D("waiting for data to come");
		}
	}

	D("leaving plugin_set_window");

	return NPERR_NO_ERROR;
}

static NPError
totem_plugin_new_stream (NPP instance,
			 NPMIMEType type,
			 NPStream* stream_ptr,
			 NPBool seekable,
			 uint16* stype)
{
	D("plugin_new_stream");

	if (instance == NULL)
		return NPERR_INVALID_INSTANCE_ERROR;

	totemPlugin *plugin = (totemPlugin *) instance->pdata;

	/* We already have a live stream */
	if (plugin->stream)
		return NPERR_GENERIC_ERROR;

	D("plugin_new_stream type: %s url: %s", type, plugin->src);

	//FIXME need to find better semantics?
	//what about saving the state, do we get confused?
	if (g_str_has_prefix (plugin->src, "file://")) {
		*stype = NP_ASFILEONLY;
		plugin->stream_type = NP_ASFILEONLY;
	} else {
		*stype = NP_ASFILE;
		plugin->stream_type = NP_ASFILE;
	}

	plugin->stream = stream_ptr;

	return NPERR_NO_ERROR;
}

static NPError
totem_plugin_destroy_stream (NPP instance,
			     NPStream* stream,
			     NPError reason)
{
	D("plugin_destroy_stream, reason: %d", reason);

	if (instance == NULL) {
		D("totem_plugin_destroy_stream instance is NULL");
		return NPERR_NO_ERROR;
	}

	totemPlugin *plugin = (totemPlugin *) instance->pdata;

	if (plugin->send_fd >= 0) {
		close(plugin->send_fd);
		plugin->send_fd = -1;
	}

	plugin->stream = nsnull;

	return NPERR_NO_ERROR;
}

static int32 totem_plugin_write_ready (NPP instance, NPStream *stream)
{
	totemPlugin *plugin;
	struct pollfd fds;

	//D("plugin_write_ready");

	if (instance == NULL)
		return 0;

	plugin = (totemPlugin *) instance->pdata;

	if (plugin->is_supported_src == FALSE)
		return 0;

	if (plugin->send_fd < 0)
		return (PLUGIN_STREAM_CHUNK_SIZE);

	fds.events = POLLOUT;
	fds.fd = plugin->send_fd;
	if (poll (&fds, 1, 0) > 0)
		return (PLUGIN_STREAM_CHUNK_SIZE);

	return 0;
}

static int32 totem_plugin_write (NPP instance, NPStream *stream, int32 offset,
	int32 len, void *buffer)
{
	int ret;

	//D("plugin_write");

	if (instance == NULL)
		return -1;

	totemPlugin *plugin = (totemPlugin *) instance->pdata;
	if (plugin == NULL)
		return -1;

	/* We already know it's a playlist, don't try to check it again
	 * and just wait for it to be on-disk */
	if (plugin->is_playlist != FALSE)
		return len;

	if (!plugin->player_pid) {
		if (!plugin->stream) {
			g_warning ("No stream in NPP_Write!?");
			return -1;
		}

		plugin->tried_write = TRUE;

		/* FIXME this looks wrong since it'll look at the current data buffer,
		 * not the cumulative data since the stream started 
		 */
		if (totem_pl_parser_can_parse_from_data ((const char *) buffer, len, TRUE /* FIXME */) != FALSE) {
			D("Need to wait for the file to be downloaded completely");
			plugin->is_playlist = TRUE;
			return len;
		}

		if (!totem_plugin_fork (plugin))
			return -1;
	}

	if (plugin->send_fd < 0)
		return -1;

	if (plugin->is_supported_src == FALSE)
		return -1;

	ret = write (plugin->send_fd, buffer, len);
	if (ret < 0) {
		int err = errno;
		D("ret %d: [%d]%s", ret, errno, g_strerror (err));
		if (err == EPIPE) {
			/* fd://0 got closed, probably because the backend
			 * crashed on us */
			if (CallNPN_DestroyStreamProc
					(mozilla_functions.destroystream,
					 plugin->instance,
					 plugin->stream,
					 NPRES_DONE) != NPERR_NO_ERROR) {
				g_warning ("Couldn't destroy the stream");
			}
		}
	}

	return ret;
}

static void totem_plugin_stream_as_file (NPP instance, NPStream *stream,
	const char* fname)
{
	totemPlugin *plugin;
	GError *err = NULL;

	D("plugin_stream_as_file: %s", fname);

	if (instance == NULL)
		return;
	plugin = (totemPlugin *) instance->pdata;

	if (plugin == NULL)
		return;

	if (plugin->tried_write == FALSE) {
		plugin->is_playlist = totem_pl_parser_can_parse_from_filename
			(fname, TRUE);
	}

	if (!plugin->player_pid && plugin->is_playlist != FALSE) {
		plugin->local = g_filename_to_uri (fname, NULL, NULL);
		totem_plugin_fork (plugin);
		return;
	} else if (!plugin->player_pid) {
		if (!totem_plugin_fork (plugin))
			return;
	}

	if (plugin->is_playlist != FALSE)
		return;

	if (!dbus_g_proxy_call (plugin->proxy, "SetLocalFile", &err,
			G_TYPE_STRING, fname, G_TYPE_INVALID,
			G_TYPE_INVALID)) {
		g_warning ("Error: %s\n", err->message);
	}

	D("plugin_stream_as_file\n");
}

static void
totem_plugin_url_notify (NPP instance, const char* url,
		NPReason reason, void* notifyData)
{
	D("plugin_url_notify");
}

static void
totem_plugin_print (NPP instance,
                    NPPrint* platformPrint)
{
	D("plugin_print");
}

static char *
totem_plugin_get_description (void)
{
	return "The <a href=\"http://www.gnome.org/projects/totem/\">Totem</a> "PACKAGE_VERSION" plugin handles video and audio streams.";
}

static NPError
totem_plugin_get_value (NPP instance,
			NPPVariable variable,
		        void *value)
{
	totemPlugin *plugin;
	NPError err = NPERR_NO_ERROR;

	/* See NPPVariable in npapi.h */
	D("plugin_get_value %d (%x)\n", variable, variable);

	switch (variable) {
	case NPPVpluginNameString:
		*((char **)value) = totemScriptablePlugin::PluginDescription ();
		break;
	case NPPVpluginDescriptionString:
		*((char **)value) = totem_plugin_get_description();
		break;
	case NPPVpluginNeedsXEmbed:
		*((NPBool *)value) = PR_TRUE;
		break;
	case NPPVpluginScriptableIID: {
		nsIID* ptr = NS_STATIC_CAST (nsIID *, mozilla_functions.memalloc (sizeof (nsIID)));
		if (ptr) {
			*ptr = NS_GET_IID (nsISupports);
			*NS_STATIC_CAST (nsIID **, value) = ptr;
		} else {
			err = NPERR_OUT_OF_MEMORY_ERROR;
		}
		break;
	}
	case NPPVpluginScriptableInstance: {
	        if (instance == NULL) {
			err = NPERR_GENERIC_ERROR;
		} else {
		        plugin = (totemPlugin *) instance->pdata;
			if (!plugin ||
			    !plugin->scriptable ||
			    !plugin->scriptable->IsValid ())
				return NPERR_INVALID_INSTANCE_ERROR;

			plugin->scriptable->QueryInterface (NS_GET_IID (nsISupports),
							    NS_REINTERPRET_CAST (void **, value));
		}
		break;
	}
	default:
		D("unhandled variable %d (%x)", variable, variable);
		err = NPERR_INVALID_PARAM;
		break;
	}

	return err;
}

static NPError
totem_plugin_set_value (NPP instance,
			NPNVariable variable,
			void *value)
{
	D("plugin_set_value %d (%x)", variable, variable);

	return NPERR_NO_ERROR;
}

NPError
NP_GetValue (void *future,
	     NPPVariable variable,
	     void *value)
{
	return totem_plugin_get_value (NULL, variable, value);
}

char *
NP_GetMIMEDescription (void)
{
	GString *list;
	guint i;

	if (mime_list != NULL)
		return mime_list;

	list = g_string_new (NULL);

	const totemPluginMimeEntry *mimetypes;
	PRUint32 count;
	totemScriptablePlugin::PluginMimeTypes (&mimetypes, &count);
	for (PRUint32 i = 0; i < count; ++i) {
		const char *desc;
		char *item;

		desc = gnome_vfs_mime_get_description (mimetypes[i].mimetype);
		if (desc == NULL && mimetypes[i].mime_alias != NULL) {
			desc = gnome_vfs_mime_get_description
				(mimetypes[i].mime_alias);
		}
		if (desc == NULL) {
			desc = mimetypes[i].mime_alias;
		}

		g_string_append_printf (list,"%s:%s:%s;",
					mimetypes[i].mimetype,
					mimetypes[i].extensions,
					desc ? desc : "-");
	}

	mime_list = g_string_free (list, FALSE);

	return mime_list;
}

NPError
NP_Initialize (NPNetscapeFuncs * moz_funcs,
	       NPPluginFuncs * plugin_funcs)
{
	NPError err = NPERR_NO_ERROR;
	NPBool supportsXEmbed = PR_FALSE;
	NPNToolkitType toolkit = (NPNToolkitType) 0;

	D("NP_Initialize\n");

	/* Do we support XEMBED? */
	err = CallNPN_GetValueProc (moz_funcs->getvalue, NULL,
			NPNVSupportsXEmbedBool,
			(void *)&supportsXEmbed);

	if (err != NPERR_NO_ERROR || supportsXEmbed != PR_TRUE)
		return NPERR_INCOMPATIBLE_VERSION_ERROR;

	/* Are we using a GTK+ 2.x Moz? */
	err = CallNPN_GetValueProc (moz_funcs->getvalue, NULL,
			NPNVToolkit, (void *)&toolkit);

	if (err != NPERR_NO_ERROR || toolkit != NPNVGtk2)
		return NPERR_INCOMPATIBLE_VERSION_ERROR;

	if(moz_funcs == NULL || plugin_funcs == NULL)
		return NPERR_INVALID_FUNCTABLE_ERROR;

	if ((moz_funcs->version >> 8) > NP_VERSION_MAJOR)
		return NPERR_INCOMPATIBLE_VERSION_ERROR;
	if (moz_funcs->size < sizeof (NPNetscapeFuncs))
		return NPERR_INVALID_FUNCTABLE_ERROR;
	if (plugin_funcs->size < sizeof (NPPluginFuncs))
		return NPERR_INVALID_FUNCTABLE_ERROR;

	/* we want to open libdbus-glib-1.so.2 in such a way
	 * in such a way that it becomes permanentely resident */
	void *handle;
	handle = dlopen ("libdbus-glib-1.so.2", RTLD_NOW | RTLD_NODELETE);
	if (!handle) {
		fprintf (stderr, "%s\n", dlerror()); 
		return NPERR_MODULE_LOAD_FAILED_ERROR;
	}
	/* RTLD_NODELETE allows us to close right away ... */
	dlclose(handle);

	/*
	 * Copy all of the fields of the Mozilla function table into our
	 * copy so we can call back into Mozilla later.  Note that we need
	 * to copy the fields one by one, rather than assigning the whole
	 * structure, because the Mozilla function table could actually be
	 * bigger than what we expect.
	 */
	mozilla_functions.size             = moz_funcs->size;
	mozilla_functions.version          = moz_funcs->version;
	mozilla_functions.geturl           = moz_funcs->geturl;
	mozilla_functions.posturl          = moz_funcs->posturl;
	mozilla_functions.requestread      = moz_funcs->requestread;
	mozilla_functions.newstream        = moz_funcs->newstream;
	mozilla_functions.write            = moz_funcs->write;
	mozilla_functions.destroystream    = moz_funcs->destroystream;
	mozilla_functions.status           = moz_funcs->status;
	mozilla_functions.uagent           = moz_funcs->uagent;
	mozilla_functions.memalloc         = moz_funcs->memalloc;
	mozilla_functions.memfree          = moz_funcs->memfree;
	mozilla_functions.memflush         = moz_funcs->memflush;
	mozilla_functions.reloadplugins    = moz_funcs->reloadplugins;
	mozilla_functions.getJavaEnv       = moz_funcs->getJavaEnv;
	mozilla_functions.getJavaPeer      = moz_funcs->getJavaPeer;
	mozilla_functions.geturlnotify     = moz_funcs->geturlnotify;
	mozilla_functions.posturlnotify    = moz_funcs->posturlnotify;
	mozilla_functions.getvalue         = moz_funcs->getvalue;
	mozilla_functions.setvalue         = moz_funcs->setvalue;
	mozilla_functions.invalidaterect   = moz_funcs->invalidaterect;
	mozilla_functions.invalidateregion = moz_funcs->invalidateregion;
	mozilla_functions.forceredraw      = moz_funcs->forceredraw;

	/*
	 * Set up a plugin function table that Mozilla will use to call
	 * into us.  Mozilla needs to know about our version and size and
	 * have a UniversalProcPointer for every function we implement.
	 */

	plugin_funcs->size = sizeof(NPPluginFuncs);
	plugin_funcs->version = (NP_VERSION_MAJOR << 8) + NP_VERSION_MINOR;
	plugin_funcs->newp = NewNPP_NewProc(totem_plugin_new_instance);
	plugin_funcs->destroy =
		NewNPP_DestroyProc(totem_plugin_destroy_instance);
	plugin_funcs->setwindow =
		NewNPP_SetWindowProc(totem_plugin_set_window);
	plugin_funcs->newstream =
		NewNPP_NewStreamProc(totem_plugin_new_stream);
	plugin_funcs->destroystream =
		NewNPP_DestroyStreamProc(totem_plugin_destroy_stream);
	plugin_funcs->asfile =
		NewNPP_StreamAsFileProc(totem_plugin_stream_as_file);
	plugin_funcs->writeready =
		NewNPP_WriteReadyProc(totem_plugin_write_ready);
	plugin_funcs->write = NewNPP_WriteProc(totem_plugin_write);
	/* Printing ? */
	plugin_funcs->print = NewNPP_PrintProc(totem_plugin_print);
	/* What's that for ? */
	plugin_funcs->event = NewNPP_HandleEventProc(NULL);
	plugin_funcs->urlnotify =
		NewNPP_URLNotifyProc(totem_plugin_url_notify);
	plugin_funcs->javaClass = NULL;
	plugin_funcs->getvalue = NewNPP_GetValueProc(totem_plugin_get_value);
	plugin_funcs->setvalue = NewNPP_SetValueProc(totem_plugin_set_value);

	return NPERR_NO_ERROR;
}

NPError NP_Shutdown(void)
{
	g_free (mime_list);
	mime_list = NULL;

	return NPERR_NO_ERROR;
}
