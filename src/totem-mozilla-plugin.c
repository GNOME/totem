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

#define XP_UNIX 1
#define MOZ_X11 1
#include "npapi.h"
#include "npupp.h"

#define DEBUG(x) printf(x "\n")
//#define DEBUG(x)

typedef struct {
	NPP instance;
	guint32 window;

	int width, height;
	int recv_fd, send_fd;
	int player_pid;

	GByteArray *bytes;
} TotemPlugin;

static NPNetscapeFuncs mozilla_functions;

/* You don't update, you die! */
#define MAX_ARGV_LEN 10

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
		argv[argc++] = g_strdup (LIBDIR"/totem/totem-mozilla-viewer");
	}

	argv[argc++] = g_strdup ("--xid");
	argv[argc++] = g_strdup_printf ("%d", plugin->window);

	if (plugin->width) {
		argv[argc++] = g_strdup ("--width");
		argv[argc++] = g_strdup_printf ("%d", plugin->width);
	}

	if (plugin->height) {
		argv[argc++] = g_strdup ("--height");
		argv[argc++] = g_strdup_printf ("%d", plugin->height);
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
//				| G_SPAWN_STDOUT_TO_DEV_NULL,
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

	/* mode is NP_EMBED, NP_FULL, or NP_BACKGROUND (see npapi.h) */
	printf("mode %d\n",mode);
	//printf("mime type: %s\n",pluginType);
	plugin->instance = instance;

	for (i=0; i<argc; i++) {
		printf ("argv[%d] %s %s\n", i, argn[i], argv[i]);
		if (strcmp (argn[i],"width") == 0) {
			plugin->width = strtol (argv[i], NULL, 0);
		}
		if (strcmp (argn[i], "height") == 0) {
			plugin->height = strtol (argv[i], NULL, 0);
		}

		//Handle loop
	}

	//totem_plugin_fork(plugin, 0x32);

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

//	close(plugin->send_fd);
//	close(plugin->recv_fd);

	kill (plugin->player_pid, SIGKILL);
	waitpid (plugin->player_pid, NULL, 0);

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
		if (plugin->window == (Window) window->window) {
			DEBUG("resize");
			/* Resize event */
			/* Not currently handled */
		} else {
			DEBUG("change");
			printf ("ack.  window changed!\n");
		}
	} else {
		NPSetWindowCallbackStruct *ws_info;

		DEBUG("about to fork");

		ws_info = window->ws_info;
		plugin->window = (Window) window->window;

		totem_plugin_fork (plugin);

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
	DEBUG("plugin_destroy_stream");

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

	return (8*1024);
//	return 4096;
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

	ret = write (plugin->send_fd, buffer, len);
	if (ret < 0)
		ret = 0;

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
	NPError err = NPERR_NO_ERROR;

	DEBUG("plugin_get_value");

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
	default:
		g_message ("unhandled variable %d", variable);
		err = NPERR_INVALID_PARAM;
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

char *NP_GetMIMEDescription(void)
{
	//FIXME i18n

	return
		/* http://www.apple.com/trailers/ */
		"video/quicktime:mov:QuickTime video;"
		/* http://www.nbc.com/nbc/Late_Night_with_Conan_O'Brien/video/triumph.shtml */
		"application/x-mplayer2::Window Media Plugin;";
/*
    		"application/mpeg:mpg:MPEG video;"
    		"audio/x-pn-windows-acm:wav:WAV audio;"
    		"audio/x-wav:wav:WAV audio;"
    		"audio/x-ogg:ogg:Ogg Vorbis audio;"
    		"audio/basic:au:basic audio;"
    		"audio/mpeg:mp1:MPEG audio;"
    		"audio/mpeg:mp2:MPEG audio;"
    		"audio/mpeg:mp3:MPEG audio;"
		"video/mpeg:mpg:MPEG video;"
		"video/mpeg:vob:MPEG video;"
		"video/quicktime:mov:QuickTime video;"
		"video/x-msvideo:avi:AVI video"; */
}

NPError NP_Initialize (NPNetscapeFuncs * moz_funcs,
		NPPluginFuncs * plugin_funcs)
{
	NPError err = NPERR_NO_ERROR;
	PRBool supportsXEmbed = PR_FALSE;
	NPNToolkitType toolkit = 0;

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

