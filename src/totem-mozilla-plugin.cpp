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

#include "mozilla-config.h"

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
#include <glib/gi18n.h>
#include <libgnomevfs/gnome-vfs-mime-handlers.h>
#include <libgnomevfs/gnome-vfs-mime-info.h>

#include "totem-mozilla-options.h"
#include "totem-mozilla-scriptable.h"

#include "npapi.h"
#include "npupp.h"

#include <nsIDOMWindow.h>
#include <nsIURI.h>
#include <nsEmbedString.h>
#include <nsIInterfaceRequestor.h>
#include <nsIInterfaceRequestorUtils.h>
#include <docshell/nsIWebNavigation.h>

#define TOTEM_DEBUG 1
/* define TOTEM_DEBUG for more debug spew */
#include "debug.h"

static NPNetscapeFuncs mozilla_functions;
static char *mime_list = NULL;

static void
cb_update_name (DBusGProxy *proxy, char *svc, char *old_owner,
		char *new_owner, TotemPlugin *plugin)
{
	D("Received notification for %s\n", svc);
	if (strcmp (svc, plugin->wait_for_svc) == 0) {
		plugin->got_svc = TRUE;
	}
}

/* You don't update, you die! */
#define MAX_ARGV_LEN 16

static gboolean
is_supported_scheme (const char *url)
{
	if (url == NULL)
		return FALSE;

	if (g_str_has_prefix (url, "mms:") != FALSE)
		return FALSE;

	return TRUE;
}

static gboolean
totem_plugin_fork (TotemPlugin *plugin)
{
	GTimeVal then, now;
	char *svcname;
	char **argv;
	int argc = 0;
	GError *err = NULL;
	totemMozillaObject *iface = plugin->iface;

	argv = (char **)g_new0 (char *, MAX_ARGV_LEN);

	if (g_file_test ("./totem-mozilla-viewer",
				G_FILE_TEST_EXISTS) != FALSE) {
		argv[argc++] = g_strdup ("./totem-mozilla-viewer");
	} else {
		argv[argc++] = g_strdup (LIBEXECDIR"/totem-mozilla-viewer");
	}

	argv[argc++] = g_strdup (TOTEM_OPTION_XID);
	argv[argc++] = g_strdup_printf ("%lu", plugin->window);

	if (plugin->width) {
		argv[argc++] = g_strdup (TOTEM_OPTION_WIDTH);
		argv[argc++] = g_strdup_printf ("%d", plugin->width);
	}

	if (plugin->height) {
		argv[argc++] = g_strdup (TOTEM_OPTION_HEIGHT);
		argv[argc++] = g_strdup_printf ("%d", plugin->height);
	}

	if (plugin->src) {
		argv[argc++] = g_strdup (TOTEM_OPTION_URL);
		argv[argc++] = g_strdup (plugin->src);
	}

	if (plugin->href) {
		argv[argc++] = g_strdup (TOTEM_OPTION_HREF);
		argv[argc++] = g_strdup (plugin->href);
	}

	if (plugin->target) {
		argv[argc++] = g_strdup (TOTEM_OPTION_TARGET);
		argv[argc++] = g_strdup (plugin->target);
	}

	if (plugin->controller_hidden) {
		argv[argc++] = g_strdup ("--nocontrols");
	}

	if (is_supported_scheme (plugin->src) == FALSE)
		argv[argc++] = g_strdup (plugin->src);
	else
		argv[argc++] = g_strdup ("fd://0");

	argv[argc] = NULL;

#ifdef TOTEM_DEBUG
	{
		int i;
		g_print ("Launching: ");
		for (i = 0; i < argc; i++) {
			g_print ("%s ", argv[i]);
		}
		g_print ("\n");
	}
#endif

	if (g_spawn_async_with_pipes (NULL, argv, NULL,
				G_SPAWN_DO_NOT_REAP_CHILD,
				NULL, NULL, &plugin->player_pid,
				&plugin->send_fd, NULL, NULL, &err) == FALSE)
	{
		D("Spawn failed");

		if (err)
		{
			fprintf(stderr, "%s\n", err->message);
			g_error_free(err);
		}

		g_strfreev (argv);
		return FALSE;
	}

	g_strfreev (argv);

	/* now wait until startup is complete */
	plugin->got_svc = FALSE;
	plugin->wait_for_svc =
		g_strdup_printf ("org.totem_%d.MozillaPluginService",
				 plugin->player_pid);
	D("waiting for signal %s", plugin->wait_for_svc);
	dbus_g_proxy_add_signal (plugin->proxy, "NameOwnerChanged",
				 G_TYPE_STRING, G_TYPE_STRING,
				 G_TYPE_STRING, G_TYPE_INVALID);
	dbus_g_proxy_connect_signal (plugin->proxy, "NameOwnerChanged",
				     G_CALLBACK (cb_update_name),
				     plugin, NULL);
	g_get_current_time (&then);
	g_time_val_add (&then, G_USEC_PER_SEC * 5);
	NS_ADDREF (iface);
	do {
		g_main_context_iteration (NULL, TRUE);
		g_get_current_time (&now);
	} while (iface->tm != NULL && !plugin->got_svc &&
		 (now.tv_sec <= then.tv_sec));
	if (!iface->tm) {
		/* we were destroyed in one of the iterations of the
		 * mainloop, get out ASAP */
		D("We no longer exist");
		NS_RELEASE (iface);
		return FALSE;
	}
	NS_RELEASE (iface);
	dbus_g_proxy_disconnect_signal (plugin->proxy, "NameOwnerChanged",
					G_CALLBACK (cb_update_name), plugin);
	if (!plugin->got_svc) {
		fprintf (stderr, "Failed to receive DBUS interface response\n");
		g_free (plugin->wait_for_svc);

		if (plugin->player_pid) {
			kill (plugin->player_pid, SIGKILL);
			waitpid (plugin->player_pid, NULL, 0);
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
	g_free (plugin->wait_for_svc);
	D("Done forking, new proxy=%p", plugin->proxy);

	return TRUE;
}

static char *
resolve_relative_uri (nsIURI *docURI, const char *uri)
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

static NPError totem_plugin_new_instance (NPMIMEType mime_type, NPP instance,
		uint16_t mode, int16_t argc, char *argn[], char *argv[],
		NPSavedData *saved)
{
	TotemPlugin *plugin;
	GError *e = NULL;
	int i;

	D("totem_plugin_new_instance");

	if (instance == NULL)
		return NPERR_INVALID_INSTANCE_ERROR;

	/* Make sure the plugin stays resident to avoid crashers when 
	 * reloading the GObject types */
	mozilla_functions.setvalue (instance,
			NPPVpluginKeepLibraryInMemory, (void *)TRUE);

	instance->pdata = mozilla_functions.memalloc(sizeof(TotemPlugin));
	plugin = (TotemPlugin *) instance->pdata;

	if (plugin == NULL)
		return NPERR_OUT_OF_MEMORY_ERROR;

	memset(plugin, 0, sizeof(TotemPlugin));
	plugin->iface = new totemMozillaObject (plugin);
	if (!plugin->iface) {
		mozilla_functions.memfree (plugin);
		return NPERR_OUT_OF_MEMORY_ERROR;
	}
	NS_ADDREF (plugin->iface);
	if (!(plugin->conn = dbus_g_bus_get (DBUS_BUS_SESSION, &e))) {
		printf ("Failed to open DBUS session: %s\n", e->message);
		g_error_free (e);
		NS_RELEASE (plugin->iface);
		mozilla_functions.memfree (plugin);
		return NPERR_OUT_OF_MEMORY_ERROR;
	} else if (!(plugin->proxy = dbus_g_proxy_new_for_name (plugin->conn,
					"org.freedesktop.DBus",
					"/org/freedesktop/DBus",
					"org.freedesktop.DBus"))) {
		printf ("Failed to open DBUS proxy: %s\n", e->message);
		g_error_free (e);
		g_object_unref (G_OBJECT (plugin->conn));
		NS_RELEASE (plugin->iface);
		mozilla_functions.memfree (plugin);
		return NPERR_OUT_OF_MEMORY_ERROR;
	}

	/* mode is NP_EMBED, NP_FULL, or NP_BACKGROUND (see npapi.h) */
	//FIXME we should error out if we are in fullscreen mode
	printf("mode %d\n",mode);
	printf("mime type: %s\n", mime_type);
	plugin->instance = instance;
	plugin->send_fd = -1;

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
	if (strcmp (mime_type, "video/quicktime") != 0) {
		plugin->cache = TRUE;
	}

	for (i=0; i<argc; i++) {
		printf ("argv[%d] %s %s\n", i, argn[i], argv[i]);
		if (g_ascii_strcasecmp (argn[i],"width") == 0) {
			plugin->width = strtol (argv[i], NULL, 0);
		}
		if (g_ascii_strcasecmp (argn[i], "height") == 0) {
			plugin->height = strtol (argv[i], NULL, 0);
		}
		if (g_ascii_strcasecmp (argn[i], "src") == 0) {
			plugin->src = resolve_relative_uri (docURI, argv[i]);
		}
		if (g_ascii_strcasecmp (argn[i], "href") == 0) {
			plugin->href = resolve_relative_uri (docURI, argv[i]);
		}
		if (g_ascii_strcasecmp (argn[i], "cache") == 0) {
			plugin->cache = TRUE;
			if (g_ascii_strcasecmp (argv[i], "false") == 0) {
				plugin->cache = FALSE;
			}
		}
		if (g_ascii_strcasecmp (argn[i], "target") == 0) {
			plugin->target = g_strdup (argv[i]);
		}
		if (g_ascii_strcasecmp (argn[i], "controller") == 0) {
			if (g_ascii_strcasecmp (argv[i], "false") == 0) {
				plugin->controller_hidden = TRUE;
			}
			//FIXME see http://www.htmlcodetutorial.com/embeddedobjects/_EMBED_CONTROLS.html
		}
		if (g_ascii_strcasecmp (argn[i], "hidden") == 0) {
			//FIXME
		}
		if (g_ascii_strcasecmp (argn[i], "autostart") == 0
				|| g_ascii_strcasecmp (argn[i], "autoplay") == 0) {
			//FIXME
		}
		if (g_ascii_strcasecmp (argn[i], "loop") == 0 ||
				g_ascii_strcasecmp (argn[i], "playcount") == 0) {
			//FIXME see http://www.htmlcodetutorial.com/embeddedobjects/_EMBED_LOOP.html
		}
		if (g_ascii_strcasecmp (argn[i], "starttime") == 0) {
			//FIXME see http://www.htmlcodetutorial.com/embeddedobjects/_EMBED_STARTTIME.html
		}
		if (g_ascii_strcasecmp (argn[i], "endtime") == 0) {
			//FIXME see above
		}
	}

	NS_IF_RELEASE (docURI);	

	return NPERR_NO_ERROR;
}

static NPError totem_plugin_destroy_instance (NPP instance, NPSavedData **save)
{
	TotemPlugin * plugin;

	D("plugin_destroy");

	if (instance == NULL)
		return NPERR_INVALID_INSTANCE_ERROR;

	plugin = (TotemPlugin *) instance->pdata;
	if (plugin == NULL)
		return NPERR_NO_ERROR;

	if (!plugin || !plugin->iface || !plugin->iface->tm)
		return NPERR_INVALID_INSTANCE_ERROR;

	plugin->iface->Rewind ();
	plugin->iface->invalidatePlugin ();

	if (plugin->send_fd >= 0)
		close(plugin->send_fd);

	if (plugin->player_pid) {
		kill (plugin->player_pid, SIGKILL);
		waitpid (plugin->player_pid, NULL, 0);
	}

	NS_RELEASE (plugin->iface);

	g_free (plugin->target);
	g_free (plugin->src);
	g_free (plugin->href);

	g_object_unref (G_OBJECT (plugin->proxy));
	//g_object_unref (G_OBJECT (plugin->conn));
	mozilla_functions.memfree (instance->pdata);
	instance->pdata = NULL;

	return NPERR_NO_ERROR;
}

static NPError totem_plugin_set_window (NPP instance, NPWindow* window)
{
	TotemPlugin *plugin;

	D("plugin_set_window");

	if (instance == NULL)
		return NPERR_INVALID_INSTANCE_ERROR;

	plugin = (TotemPlugin *) instance->pdata;
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
		gchar *msg;

		D("about to fork");

		plugin->window = (Window) window->window;
		if (!totem_plugin_fork (plugin))
			return NPERR_GENERIC_ERROR;

		if (plugin->send_fd > 0)
			fcntl(plugin->send_fd, F_SETFL, O_NONBLOCK);
	}

	D("leaving plugin_set_window");

	return NPERR_NO_ERROR;
}

static NPError totem_plugin_new_stream (NPP instance, NPMIMEType type,
		NPStream* stream_ptr, NPBool seekable, uint16* stype)
{
	TotemPlugin *plugin;

	D("plugin_new_stream");

	if (instance == NULL)
		return NPERR_INVALID_INSTANCE_ERROR;

	plugin = (TotemPlugin *) instance->pdata;

	D("plugin_new_stream type: %s url: %s", type, plugin->src);

	//FIXME need to find better semantics?
	//what about saving the state, do we get confused?
	if (g_str_has_prefix (plugin->src, "file://")) {
		*stype = NP_ASFILEONLY;
		plugin->stream_type = NP_ASFILEONLY;
	} else if (plugin->cache != FALSE) {
		*stype = NP_ASFILE;
		plugin->stream_type = NP_ASFILE;
	} else {
		*stype = NP_NORMAL;
		plugin->stream_type = NP_NORMAL;
	}
		

	return NPERR_NO_ERROR;
}

static NPError totem_plugin_destroy_stream (NPP instance, NPStream* stream,
		NPError reason)
{
	TotemPlugin *plugin;

	D("plugin_destroy_stream");

	if (instance == NULL) {
		D("totem_plugin_destroy_stream instance is NULL");
		return NPERR_NO_ERROR;
	}

	plugin = (TotemPlugin *) instance->pdata;

	close(plugin->send_fd);
	plugin->send_fd = -1;

	return NPERR_NO_ERROR;
}

static int32 totem_plugin_write_ready (NPP instance, NPStream *stream)
{
	TotemPlugin *plugin;
	struct pollfd fds;

	D("plugin_write_ready");

	if (instance == NULL) {
		D("plugin_write_ready instance is NULL");
		return 0;
	}

	plugin = (TotemPlugin *) instance->pdata;

	if (is_supported_scheme (plugin->src) == FALSE)
		return 0;

	if (plugin->send_fd < 0) {
		if (!totem_plugin_fork (plugin))
			return 0;

		if (plugin->send_fd > 0)
			fcntl(plugin->send_fd, F_SETFL, O_NONBLOCK);
		else
			return 0;
	}

	fds.events = POLLOUT;
	fds.fd = plugin->send_fd;
	if (plugin->send_fd > 0 && poll (&fds, 1, 0) > 0)
		return (8*1024);

	return 0;
}

static int32 totem_plugin_write (NPP instance, NPStream *stream, int32 offset,
	int32 len, void *buffer)
{
	TotemPlugin *plugin;
	int ret;

	D("plugin_write");

	if (instance == NULL)
		return -1;

	plugin = (TotemPlugin *) instance->pdata;

	if (plugin == NULL)
		return -1;

	if (!plugin->player_pid)
		return -1;

	if (plugin->send_fd < 0)
		return -1;

	if (is_supported_scheme (plugin->src) == FALSE)
		return -1;

	ret = write (plugin->send_fd, buffer, len);
	if (ret < 0) {
		D("ret %d: [%d]%s", ret, errno, g_strerror (errno));
	}

	return ret;
}

static void totem_plugin_stream_as_file (NPP instance, NPStream *stream,
	const char* fname)
{
	TotemPlugin *plugin;
	GError *err = NULL;

	D("plugin_stream_as_file: %s", fname);

	if (instance == NULL)
		return;
	plugin = (TotemPlugin *) instance->pdata;

	if (plugin == NULL)
		return;

	if (!dbus_g_proxy_call (plugin->proxy, "SetLocalFile", &err,
			G_TYPE_STRING, fname, G_TYPE_INVALID,
			G_TYPE_INVALID)) {
		g_printerr ("Error: %s\n", err->message);
	}

	D("plugin_stream_as_file\n");
}

static void
totem_plugin_url_notify (NPP instance, const char* url,
		NPReason reason, void* notifyData)
{
	D("plugin_url_notify");
}

static char *
totem_plugin_get_description (void)
{
	return "The <a href=\"http://www.gnome.org/projects/totem/\">Totem</a> "PACKAGE_VERSION" plugin handles video and audio streams.";
}

static NPError
totem_plugin_get_value (NPP instance, NPPVariable variable,
		                        void *value)
{
	TotemPlugin *plugin;
	NPError err = NPERR_NO_ERROR;

	/* See NPPVariable in npapi.h */
	D("plugin_get_value %d\n", variable);

	switch (variable) {
	case NPPVpluginNameString:
		*((char **)value) = "Totem Mozilla Plugin";
		break;
	case NPPVpluginDescriptionString:
		*((char **)value) = totem_plugin_get_description();
		break;
	case NPPVpluginNeedsXEmbed:
		*((NPBool *)value) = PR_TRUE;
		break;
	case NPPVpluginScriptableIID: {
		static nsIID sIID = TOTEMMOZILLASCRIPT_IID;
		nsIID* ptr = (nsIID *) mozilla_functions.memalloc (sizeof (nsIID));

		if (ptr) {
			*ptr = sIID;
			* (nsIID **) value = ptr;
			D("Returning that we support iface");
		} else {
			err = NPERR_OUT_OF_MEMORY_ERROR;
		}
		break;
	}
	case NPPVpluginScriptableInstance: {
	        if (instance == NULL) {
			err = NPERR_GENERIC_ERROR;
		} else {
		        plugin = (TotemPlugin *) instance->pdata;
			NS_ENSURE_TRUE (plugin && plugin->iface && plugin->iface->tm, NPERR_INVALID_INSTANCE_ERROR);

			plugin->iface->QueryInterface (NS_GET_IID (nsISupports),
						       (void **) value);
//			* (nsISupports **) value = static_cast<totemMozillaScript *>(plugin->iface);
			D("Returning instance %p", plugin->iface);
		}
		break;
	}
	default:
		D("unhandled variable %d", variable);
		err = NPERR_INVALID_PARAM;
		break;
	}

	return err;
}

static NPError
totem_plugin_set_value (NPP instance, NPNVariable variable,
		                        void *value)
{
	D("plugin_set_value");

	return NPERR_NO_ERROR;
}

NPError
NP_GetValue(void *future, NPPVariable variable, void *value)
{
	return totem_plugin_get_value (NULL, variable, value);
}

static struct {
	const char *mime_type;
	const char *extensions;
	const char *mime_alias;
} mime_types[] = {
	{ "video/quicktime", "mov", NULL },
	{ "application/x-mplayer2", "avi, wma, wmv", "video/x-msvideo" },
	{ "video/mpeg", "mpg, mpeg, mpe", NULL },
	{ "video/x-ms-asf-plugin", "asf, wmv", "video/x-ms-asf" },
	{ "video/x-ms-wmv", "wmv", "video/x-ms-wmv" },
	{ "video/x-wmv", "wmv", "video/x-ms-wmv" },
	{ "application/ogg", "ogg", NULL },
	{ "video/divx", "divx", "video/x-msvideo" },
	{ "audio/wav", "wav", NULL }
};
#define NUM_MIME_TYPES G_N_ELEMENTS(mime_types)

char *NP_GetMIMEDescription(void)
{
	GString *list;
	guint i;

	if (mime_list != NULL)
		return mime_list;

	list = g_string_new (NULL);

	for (i = 0; i < NUM_MIME_TYPES; i++) {
		const char *desc;
		char *item;

		desc = gnome_vfs_mime_get_description (mime_types[i].mime_type);
		if (desc == NULL && mime_types[i].mime_alias != NULL) {
			desc = gnome_vfs_mime_get_description
				(mime_types[i].mime_alias);
		}
		if (desc == NULL) {
			desc = mime_types[i].mime_alias;
		}

		item = g_strdup_printf ("%s:%s:%s;", mime_types[i].mime_type,
				mime_types[i].extensions, desc);
		list = g_string_append (list, item);
		g_free (item);
	}

	mime_list = g_string_free (list, FALSE);

	return mime_list;
}

NPError NP_Initialize (NPNetscapeFuncs * moz_funcs,
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
	plugin_funcs->print = NewNPP_PrintProc(NULL);
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

