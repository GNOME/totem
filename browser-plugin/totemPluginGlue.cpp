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

#include <libgnomevfs/gnome-vfs-mime-handlers.h>
#include <libgnomevfs/gnome-vfs-mime-info.h>
#include <libgnomevfs/gnome-vfs-utils.h>
#include <dlfcn.h>

#include "npapi.h"
#include "npupp.h"

#define GNOME_ENABLE_DEBUG 1
/* define GNOME_ENABLE_DEBUG for more debug spew */
#include "debug.h"

#include "totemPluginGlue.h"
#include "totemPlugin.h"

static char *mime_list = NULL;

static NPError
totem_plugin_new_instance (NPMIMEType mimetype,
			   NPP instance,
			   uint16_t mode,
			   int16_t argc,
			   char *argn[],
			   char *argv[],
			   NPSavedData *savedData)
{
	if (!instance)
		return NPERR_INVALID_INSTANCE_ERROR;

	totemPlugin *plugin = new totemPlugin (instance);
	if (!plugin)
		return NPERR_OUT_OF_MEMORY_ERROR;

	NPError rv = plugin->Init (mimetype, mode, argc, argn, argv, savedData);
	if (rv != NPERR_NO_ERROR) {
		delete plugin;
		plugin = nsnull;
	}

	instance->pdata = plugin;

	return rv;
}

static NPError
totem_plugin_destroy_instance (NPP instance,
			       NPSavedData **save)
{
	if (!instance)
		return NPERR_INVALID_INSTANCE_ERROR;

	totemPlugin *plugin = (totemPlugin *) instance->pdata;
	if (!plugin)
		return NPERR_NO_ERROR;

	delete plugin;

	instance->pdata = nsnull;

	return NPERR_NO_ERROR;
}

static NPError
totem_plugin_set_window (NPP instance,
			 NPWindow* window)
{
	if (!instance)
		return NPERR_INVALID_INSTANCE_ERROR;

	totemPlugin *plugin = (totemPlugin *) instance->pdata;
	if (!plugin)
		return NPERR_INVALID_INSTANCE_ERROR;

	return plugin->SetWindow (window);
}

static NPError
totem_plugin_new_stream (NPP instance,
			 NPMIMEType type,
			 NPStream* stream_ptr,
			 NPBool seekable,
			 uint16* stype)
{
	if (!instance)
		return NPERR_INVALID_INSTANCE_ERROR;

	totemPlugin *plugin = (totemPlugin *) instance->pdata;
	if (!plugin)
		return NPERR_INVALID_INSTANCE_ERROR;

	return plugin->NewStream (type, stream_ptr, seekable, stype);
}

static NPError
totem_plugin_destroy_stream (NPP instance,
			     NPStream* stream,
			     NPError reason)
{
	if (!instance) {
		D("totem_plugin_destroy_stream instance is NULL");
		/* FIXME? */
		return NPERR_NO_ERROR;
	}

	totemPlugin *plugin = (totemPlugin *) instance->pdata;
	if (!plugin)
		return NPERR_INVALID_INSTANCE_ERROR;

	return plugin->DestroyStream (stream, reason);
}

static int32
totem_plugin_write_ready (NPP instance,
			  NPStream *stream)
{
	if (!instance)
		return -1;

	totemPlugin *plugin = (totemPlugin *) instance->pdata;
	if (!plugin)
		return -1;

	return plugin->WriteReady (stream);
}

static int32
totem_plugin_write (NPP instance,
		    NPStream *stream,
		    int32 offset,
		    int32 len,
		    void *buffer)
{
	if (!instance)
		return -1;

	totemPlugin *plugin = (totemPlugin *) instance->pdata;
	if (!plugin)
		return -1;

	return plugin->Write (stream, offset, len, buffer);
}

static void
totem_plugin_stream_as_file (NPP instance,
			     NPStream *stream,
			     const char* fname)
{
	if (!instance)
		return;

	totemPlugin *plugin = (totemPlugin *) instance->pdata;
	if (!plugin)
		return;

	plugin->StreamAsFile (stream, fname);
}

static void
totem_plugin_url_notify (NPP instance,
			 const char* url,
			 NPReason reason,
			 void* notifyData)
{
	if (!instance)
		return;

	totemPlugin *plugin = (totemPlugin *) instance->pdata;
	if (!plugin)
		return;

	plugin->URLNotify (url, reason, notifyData);
}

static void
totem_plugin_print (NPP instance,
                    NPPrint* platformPrint)
{
	D ("Print");
}

static char *
totem_plugin_get_description (void)
{
	return "The <a href=\"http://www.gnome.org/projects/totem/\">Totem</a> " PACKAGE_VERSION " plugin handles video and audio streams.";
}

static NPError
totem_plugin_get_value (NPP instance,
			NPPVariable variable,
		        void *value)
{
	totemPlugin *plugin = nsnull;
	NPError err = NPERR_NO_ERROR;

	/* See NPPVariable in npapi.h */
	D ("GetValue variable %d (%x)", variable, variable);

	if (instance) {
		plugin = (totemPlugin *) instance->pdata;
	}

	switch (variable) {
	case NPPVpluginNameString:
		*((char **)value) = totemScriptablePlugin::PluginDescription ();
		break;
	case NPPVpluginDescriptionString:
		*((char **)value) = totem_plugin_get_description();
		break;
	case NPPVpluginNeedsXEmbed:
		*((NPBool *)value) = TRUE;
		break;
	case NPPVpluginScriptableIID: {
		nsIID* ptr = NS_STATIC_CAST (nsIID *, totemPlugin::sNPN.memalloc (sizeof (nsIID)));
		if (ptr) {
			*ptr = NS_GET_IID (nsISupports);
			*NS_STATIC_CAST (nsIID **, value) = ptr;
		} else {
			err = NPERR_OUT_OF_MEMORY_ERROR;
		}
		break;
	}
	case NPPVpluginScriptableInstance: {
		if (plugin) {
			err = plugin->GetScriptable (value);
		}
		else {
			err = NPERR_INVALID_PLUGIN_ERROR;
		}
		break;
	}
	case NPPVjavascriptPushCallerBool:
		D ("Unhandled variable NPPVjavascriptPushCallerBool");
		err = NPERR_INVALID_PARAM;
		break;
	case NPPVpluginKeepLibraryInMemory:
		D ("Unhandled variable NPPVpluginKeepLibraryInMemory");
		err = NPERR_INVALID_PARAM;
		break;
	case NPPVpluginScriptableNPObject:
		D ("Unhandled variable NPPVpluginScriptableNPObject");
		err = NPERR_INVALID_PARAM;
		break;
	default:
		D ("Unhandled variable");
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
	D ("SetValue variable %d (%x)", variable, variable);

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

	if (mime_list != NULL)
		return mime_list;

	list = g_string_new (NULL);

	const totemPluginMimeEntry *mimetypes;
	PRUint32 count;
	totemScriptablePlugin::PluginMimeTypes (&mimetypes, &count);
	for (PRUint32 i = 0; i < count; ++i) {
		const char *desc;

		if (mimetypes[i].ignore != FALSE)
			continue;

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
NP_Initialize (NPNetscapeFuncs * aMozillaFuncs,
	       NPPluginFuncs * plugin_funcs)
{
	NPError err = NPERR_NO_ERROR;
	NPBool supportsXEmbed = PR_FALSE;
	NPNToolkitType toolkit = (NPNToolkitType) 0;

	D ("NP_Initialize");

	/* Do we support XEMBED? */
	err = CallNPN_GetValueProc (aMozillaFuncs->getvalue, NULL,
			NPNVSupportsXEmbedBool,
			(void *)&supportsXEmbed);

	if (err != NPERR_NO_ERROR || supportsXEmbed != PR_TRUE)
		return NPERR_INCOMPATIBLE_VERSION_ERROR;

	/* Are we using a GTK+ 2.x Moz? */
	err = CallNPN_GetValueProc (aMozillaFuncs->getvalue, NULL,
			NPNVToolkit, (void *)&toolkit);

	if (err != NPERR_NO_ERROR || toolkit != NPNVGtk2)
		return NPERR_INCOMPATIBLE_VERSION_ERROR;

	if(aMozillaFuncs == NULL || plugin_funcs == NULL)
		return NPERR_INVALID_FUNCTABLE_ERROR;

	if ((aMozillaFuncs->version >> 8) > NP_VERSION_MAJOR)
		return NPERR_INCOMPATIBLE_VERSION_ERROR;
	/* FIXME: check instead: indexof (last known entry in NPNetscapeFuncs) */
	if (aMozillaFuncs->size < sizeof (NPNetscapeFuncs))
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
	totemPlugin::sNPN.size             = aMozillaFuncs->size;
	totemPlugin::sNPN.version          = aMozillaFuncs->version;
	totemPlugin::sNPN.geturl           = aMozillaFuncs->geturl;
	totemPlugin::sNPN.posturl          = aMozillaFuncs->posturl;
	totemPlugin::sNPN.requestread      = aMozillaFuncs->requestread;
	totemPlugin::sNPN.newstream        = aMozillaFuncs->newstream;
	totemPlugin::sNPN.write            = aMozillaFuncs->write;
	totemPlugin::sNPN.destroystream    = aMozillaFuncs->destroystream;
	totemPlugin::sNPN.status           = aMozillaFuncs->status;
	totemPlugin::sNPN.uagent           = aMozillaFuncs->uagent;
	totemPlugin::sNPN.memalloc         = aMozillaFuncs->memalloc;
	totemPlugin::sNPN.memfree          = aMozillaFuncs->memfree;
	totemPlugin::sNPN.memflush         = aMozillaFuncs->memflush;
	totemPlugin::sNPN.reloadplugins    = aMozillaFuncs->reloadplugins;
	totemPlugin::sNPN.getJavaEnv       = aMozillaFuncs->getJavaEnv;
	totemPlugin::sNPN.getJavaPeer      = aMozillaFuncs->getJavaPeer;
	totemPlugin::sNPN.geturlnotify     = aMozillaFuncs->geturlnotify;
	totemPlugin::sNPN.posturlnotify    = aMozillaFuncs->posturlnotify;
	totemPlugin::sNPN.getvalue         = aMozillaFuncs->getvalue;
	totemPlugin::sNPN.setvalue         = aMozillaFuncs->setvalue;
	totemPlugin::sNPN.invalidaterect   = aMozillaFuncs->invalidaterect;
	totemPlugin::sNPN.invalidateregion = aMozillaFuncs->invalidateregion;
	totemPlugin::sNPN.forceredraw      = aMozillaFuncs->forceredraw;

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

	D ("NP_Initialize succeeded");

	return totemPlugin::Initialise ();
}

NPError
NP_Shutdown(void)
{
	D ("NP_Shutdown");

	g_free (mime_list);
	mime_list = NULL;

	return totemPlugin::Shutdown ();
}
