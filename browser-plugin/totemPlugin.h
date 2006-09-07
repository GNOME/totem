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

#ifndef __TOTEM_MOZILLA_PLUGIN_H__
#define __TOTEM_MOZILLA_PLUGIN_H__

#include <dbus/dbus-glib.h>
#include "bacon-message-connection.h"
#include "npupp.h"
#include "npapi.h"

class totemScriptablePlugin;

typedef struct {
	NPP instance;
	NPStream *stream;
	Window window;
	totemScriptablePlugin *scriptable;

	char *src, *local, *href, *target, *mimetype;
	int width, height;
	DBusGConnection *conn;
	DBusGProxy *proxy;
	char *wait_for_svc;
	int send_fd;
	int player_pid;
	guint8 stream_type;

	guint got_svc : 1;
	guint controller_hidden : 1;
	guint cache : 1;
	guint hidden : 1;
	guint repeat : 1;
	guint is_playlist : 1;
	guint noautostart : 1;
	guint is_supported_src : 1;
} totemPlugin;

typedef struct {
	const char *mimetype;
	const char *extensions;
	const char *mime_alias;
} totemPluginMimeEntry;

void	totem_plugin_play	(totemPlugin *plugin);

void	totem_plugin_stop	(totemPlugin *plugin);

void	totem_plugin_pause	(totemPlugin *plugin);

#endif /* __TOTEM_MOZILLA_PLUGIN_H__ */
