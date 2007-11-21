/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2007 Openismus GmbH
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * The Totem project hereby grant permission for non-gpl compatible GStreamer
 * plugins to be used and distributed together with GStreamer and Totem. This
 * permission are above and beyond the permissions granted by the GPL license
 * Totem is covered by.
 *
 * See license_change file for details.
 *
 * Author:
 * 	Mathias Hasselmann
 *
 */

#include "config.h"

#include <glib.h>
#include <glib-object.h>
#include <glib/gi18n-lib.h>
#include <gmodule.h>
#include <string.h>

#include "totem-plugin.h"
#include "totem.h"

#define TOTEM_TYPE_PUBLISH_PLUGIN		(totem_publish_plugin_get_type ())
#define TOTEM_PUBLISH_PLUGIN(o)			(G_TYPE_CHECK_INSTANCE_CAST ((o), TOTEM_TYPE_PUBLISH_PLUGIN, TotemPublishPlugin))
#define TOTEM_PUBLISH_PLUGIN_CLASS(k)		(G_TYPE_CHECK_CLASS_CAST((k), TOTEM_TYPE_PUBLISH_PLUGIN, TotemPublishPluginClass))
#define TOTEM_IS_PUBLISH_PLUGIN(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), TOTEM_TYPE_PUBLISH_PLUGIN))
#define TOTEM_IS_PUBLISH_PLUGIN_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), TOTEM_TYPE_PUBLISH_PLUGIN))
#define TOTEM_PUBLISH_PLUGIN_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), TOTEM_TYPE_PUBLISH_PLUGIN, TotemPublishPluginClass))

typedef struct
{
	TotemPlugin   parent;

	GtkWindow     *window;
	guint          handler_id;
} TotemPublishPlugin;

typedef struct
{
	TotemPluginClass parent_class;
} TotemPublishPluginClass;

G_MODULE_EXPORT GType register_totem_plugin	(GTypeModule *module);
static GType totem_publish_plugin_get_type	(void);

TOTEM_PLUGIN_REGISTER(TotemPublishPlugin, totem_publish_plugin)

static void
totem_publish_plugin_init (TotemPublishPlugin *plugin)
{
}

static void
totem_publish_plugin_finalize (GObject *object)
{
	G_OBJECT_CLASS (totem_publish_plugin_parent_class)->finalize (object);
}

static gboolean
totem_publish_plugin_activate (TotemPlugin  *plugin,
			       TotemObject  *totem,
			       GError      **error)
{
	TotemPublishPlugin *self = TOTEM_PUBLISH_PLUGIN (plugin);

	return TRUE;
}

static void
totem_publish_plugin_deactivate (TotemPlugin *plugin,
				 TotemObject *totem)
{
	TotemPublishPlugin *self = TOTEM_PUBLISH_PLUGIN (plugin);
}

static GtkWidget*
totem_publish_plugin_create_configure_dialog (TotemPlugin *plugin)
{
	TotemPublishPlugin *self = TOTEM_PUBLISH_PLUGIN (plugin);
	GtkWidget *dialog;

	dialog = gtk_dialog_new_with_buttons (NULL, NULL,
					      GTK_DIALOG_MODAL |
					      GTK_DIALOG_DESTROY_WITH_PARENT |
					      GTK_DIALOG_NO_SEPARATOR,
					      GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE,
					      NULL);

	g_signal_connect (dialog, "response", G_CALLBACK (gtk_widget_destroy), NULL);

	return dialog;
}

static void
totem_publish_plugin_class_init (TotemPublishPluginClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	TotemPluginClass *plugin_class = TOTEM_PLUGIN_CLASS (klass);

	object_class->finalize = totem_publish_plugin_finalize;

	plugin_class->activate = totem_publish_plugin_activate;
	plugin_class->deactivate = totem_publish_plugin_deactivate;
	plugin_class->create_configure_dialog = totem_publish_plugin_create_configure_dialog;
}

