/* Totem Mozilla plugin
 * 
 * Copyright (C) <2004> Bastien Nocera <hadess@hadess.net>
 * Copyright (C) <2002> David A. Schleef <ds@schleef.org>
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

#include "config.h"

#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <string.h>

#include <glib.h>
#include <glib/gi18n.h>
#include <libgnomevfs/gnome-vfs-mime-handlers.h>
#include <libgnomevfs/gnome-vfs-mime-info.h>

#include "totem-mozilla-options.h"
#include "totem-mozilla-scriptable.h"

//#define XP_UNIX 1
#define MOZ_X11 1
#include "npapi.h"
#include "npupp.h"

#define DEBUG(x) printf(x "\n")
//#define DEBUG(x)

static NPNetscapeFuncs mozilla_functions;

/* You don't update, you die! */
#define MAX_ARGV_LEN 14

static void totem_plugin_fork (TotemPlugin *plugin)
{
	char **argv;
	int argc = 0;
	GError *err = NULL;

	argv = (char **)g_new0 (char *, MAX_ARGV_LEN);

	if (g_file_test ("./totem-mozilla-viewer",
				G_FILE_TEST_EXISTS) != FALSE) {
		argv[argc++] = g_strdup ("./totem-mozilla-viewer");
	} else {
		argv[argc++] = g_strdup (LIBEXECDIR"/totem-mozilla-viewer");
	}

	argv[argc++] = g_strdup (TOTEM_OPTION_XID);
	argv[argc++] = g_strdup_printf ("%d", plugin->window);

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

	if (plugin->controller_hidden) {
		argv[argc++] = g_strdup ("--nocontrols");
	}

	argv[argc++] = g_strdup ("fd://0");
	argv[argc] = NULL;

	{
		int i;
		g_print ("Launching: ");
		for (i = 0; i < argc; i++) {
			g_print ("%s ", argv[i]);
		}
		g_print ("\n");
	}

	if (g_spawn_async_with_pipes (NULL, argv, NULL,
				G_SPAWN_DO_NOT_REAP_CHILD,
				NULL, NULL, &plugin->player_pid,
				&plugin->send_fd, NULL, NULL, &err) == FALSE)
	{
		DEBUG("Spawn failed");

		if(err)
		{
			fprintf(stderr, "%s\n", err->message);
			g_error_free(err);
		}
	}

	g_strfreev (argv);
}

static void
cb_data (const gchar * msg, gpointer user_data)
{
	TotemPlugin *plugin = (TotemPlugin *) user_data;

	g_free (plugin->last_msg);
	plugin->last_msg = g_strdup (msg);
	g_print ("Read msg '%s'\n", msg);
}

static NPError totem_plugin_new_instance (NPMIMEType mime_type, NPP instance,
		uint16_t mode, int16_t argc, char *argn[], char *argv[],
		NPSavedData *saved)
{
	TotemPlugin *plugin;
	int i;

	DEBUG("totem_plugin_new_instance");

	if (instance == NULL)
		return NPERR_INVALID_INSTANCE_ERROR;

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
	plugin->conn = bacon_message_connection_new ("totem-mozilla");
	if (!plugin->conn) {
		delete plugin->iface;
		mozilla_functions.memfree (plugin);
		return NPERR_OUT_OF_MEMORY_ERROR;
	}
	bacon_message_connection_set_callback (plugin->conn, cb_data, plugin);
	NS_ADDREF (plugin->iface);

	/* mode is NP_EMBED, NP_FULL, or NP_BACKGROUND (see npapi.h) */
	printf("mode %d\n",mode);
	//printf("mime type: %s\n",pluginType);
	plugin->instance = instance;
	plugin->send_fd = -1;

	for (i=0; i<argc; i++) {
		printf ("argv[%d] %s %s\n", i, argn[i], argv[i]);
		if (g_ascii_strcasecmp (argn[i],"width") == 0) {
			plugin->width = strtol (argv[i], NULL, 0);
		}
		if (g_ascii_strcasecmp (argn[i], "height") == 0) {
			plugin->height = strtol (argv[i], NULL, 0);
		}
		//FIXME we can have some relative paths here as well!
		if (g_ascii_strcasecmp (argn[i], "src") == 0) {
			plugin->src = g_strdup (argv[i]);
		}
		if (g_ascii_strcasecmp (argn[i], "href") == 0) {
			plugin->href = g_strdup (argv[i]);
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

	return NPERR_NO_ERROR;
}

static NPError totem_plugin_destroy_instance (NPP instance, NPSavedData **save)
{
	TotemPlugin * plugin;

	DEBUG("plugin_destroy");

	if (instance == NULL)
		return NPERR_INVALID_INSTANCE_ERROR;

	plugin = (TotemPlugin *) instance->pdata;
	if (plugin == NULL)
		return NPERR_NO_ERROR;

	if (plugin->send_fd >= 0)
		close(plugin->send_fd);

	if (plugin->player_pid) {
		kill (plugin->player_pid, SIGKILL);
		waitpid (plugin->player_pid, NULL, 0);
	}

	NS_RELEASE (plugin->iface);
	delete plugin->iface;
	bacon_message_connection_free (plugin->conn);
	g_free (plugin->last_msg);
	mozilla_functions.memfree (instance->pdata);
	instance->pdata = NULL;

	return NPERR_NO_ERROR;
}

static NPError totem_plugin_set_window (NPP instance, NPWindow* window)
{
	TotemPlugin *plugin;

	DEBUG("plugin_set_window");

	if (instance == NULL)
		return NPERR_INVALID_INSTANCE_ERROR;

	plugin = (TotemPlugin *) instance->pdata;
	if (plugin == NULL)
		return NPERR_INVALID_INSTANCE_ERROR;

	if (plugin->window) {
		DEBUG ("existing window");
		if (plugin->window == (guint32) window->window) {
			DEBUG("resize");
			/* Resize event */
			/* Not currently handled */
		} else {
			DEBUG("change");
			printf ("ack.  window changed!\n");
		}
	} else {
		gchar *msg;

		DEBUG("about to fork");

		plugin->window = (guint32) window->window;
		totem_plugin_fork (plugin);
                msg = plugin->iface->wait ();

		if (plugin->send_fd > 0)
			fcntl(plugin->send_fd, F_SETFL, O_NONBLOCK);
	}

	DEBUG("leaving plugin_set_window");

	return NPERR_NO_ERROR;
}

static NPError totem_plugin_new_stream (NPP instance, NPMIMEType type,
		NPStream* stream_ptr, NPBool seekable, uint16* stype)
{
	DEBUG("plugin_new_stream");

	if (instance == NULL)
		return NPERR_INVALID_INSTANCE_ERROR;

	return NPERR_NO_ERROR;
}

static NPError totem_plugin_destroy_stream (NPP instance, NPStream* stream,
		NPError reason)
{
	TotemPlugin *plugin;

	DEBUG("plugin_destroy_stream");

	if (instance == NULL) {
		DEBUG("totem_plugin_destroy_stream instance is NULL");
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

	DEBUG("plugin_write_ready");

	if (instance == NULL) {
		DEBUG("plugin_write_ready instance is NULL");
		return 0;
	}

	plugin = (TotemPlugin *) instance->pdata;

	if (plugin->send_fd >0)
		return (8*1024);

	return 0;
}

static int32 totem_plugin_write (NPP instance, NPStream *stream, int32 offset,
	int32 len, void *buffer)
{
	TotemPlugin *plugin;
	int ret;

	DEBUG("plugin_write");

	if (instance == NULL)
		return 0;

	plugin = (TotemPlugin *) instance->pdata;

	if (plugin == NULL)
		return 0;

	if (!plugin->player_pid)
		return 0;

	if (plugin->send_fd < 0)
		return 0;

//	g_message ("write %d %p %d", plugin->send_fd, buffer, len);
	ret = write (plugin->send_fd, buffer, len);
	if (ret < 0) {
		g_message ("ret %d", ret);
		ret = 0;
	}

	return ret;
}

static void totem_plugin_stream_as_file (NPP instance, NPStream *stream,
	const char* fname)
{
	TotemPlugin *plugin;

	DEBUG("plugin_stream_as_file");

	if (instance == NULL)
		return;
	plugin = (TotemPlugin *) instance->pdata;

	if (plugin == NULL)
		return;

	printf("plugin_stream_as_file\n");
}

static void
totem_plugin_url_notify (NPP instance, const char* url,
		NPReason reason, void* notifyData)
{
	DEBUG("plugin_url_notify");
}

static NPError
totem_plugin_get_value (NPP instance, NPPVariable variable,
		                        void *value)
{
	TotemPlugin *plugin;
	NPError err = NPERR_NO_ERROR;

	printf ("plugin_get_value %d\n", variable);

        if (instance == NULL)
                return NPERR_GENERIC_ERROR;
        plugin = (TotemPlugin *) instance->pdata;

	switch (variable) {
	case NPPVpluginNameString:
		*((char **)value) = "Totem Mozilla Plugin";
		break;
	case NPPVpluginDescriptionString:
		*((char **)value) =
			"The <a href=\"http://hadess.net/totem.php3\">Totem</a> plugin handles video and audio streams.";
		break;
	case NPPVpluginNeedsXEmbed:
		*((PRBool *)value) = PR_TRUE;
		break;
	case NPPVpluginScriptableIID: {
		static nsIID sIID = TOTEMMOZILLASCRIPT_IID;
		nsIID* ptr = (nsIID *) mozilla_functions.memalloc (sizeof (nsIID));

		if (ptr) {
			*ptr = sIID;
			* (nsIID **) value = ptr;
			g_print ("Returning that we support iface\n");
		} else {
			err = NPERR_OUT_OF_MEMORY_ERROR;
		}
		break;
	}
	case NPPVpluginScriptableInstance: {
		NS_ADDREF (plugin->iface);
		plugin->iface->QueryInterface (NS_GET_IID (nsISupports),
					       (void **) value);
//		* (nsISupports **) value = static_cast<totemMozillaScript *>(plugin->iface);
		g_print ("Returning instance %p\n", plugin->iface);
		break;
	}
	default:
		g_message ("unhandled variable %d", variable);
		err = NPERR_INVALID_PARAM;
		break;
	}

	return err;
}

static NPError
totem_plugin_set_value (NPP instance, NPNVariable variable,
		                        void *value)
{
	DEBUG("plugin_set_value");

	return NPERR_NO_ERROR;
}

NPError
NP_GetValue(void *future, NPPVariable variable, void *value)
{
	return totem_plugin_get_value (NULL, variable, value);
}

#define NUM_MIME_TYPES 4
static struct {
	const char *mime_type;
	const char *extensions;
	const char *mime_alias;
} mime_types[] = {
	{ "video/quicktime", "mov" },
	{ "application/x-mplayer2", "avi, wma, wmv", "video/x-msvideo" },
	{ "video/mpeg", "mpg, mpeg, mpe" },
	{ "video/x-ms-asf-plugin", "asf, wmv", "video/x-ms-asf" }
};

char *NP_GetMIMEDescription(void)
{
	GString *list;
	char *mime_list;
	guint i;

	list = g_string_new (NULL);

	for (i = 0; i < NUM_MIME_TYPES; i++) {
		const char *desc;
		char *item;

		desc = gnome_vfs_mime_get_description (mime_types[i].mime_type);
		if (desc == NULL) {
			desc = gnome_vfs_mime_get_description
				(mime_types[i].mime_alias);
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
	PRBool supportsXEmbed = PR_FALSE;
	NPNToolkitType toolkit = (NPNToolkitType) 0;

	printf ("NP_Initialize\n");

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
	return NPERR_NO_ERROR;
}

