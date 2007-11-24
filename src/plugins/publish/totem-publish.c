/* * -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
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

#include <glib-object.h>
#include <glib/gi18n-lib.h>

#include <libepc/publisher.h>
#include <libepc/service-monitor.h>

#include "totem-plugin.h"
#include "totem-interface.h"
#include "totem-private.h"
#include "totem.h"

#define TOTEM_TYPE_PUBLISH_PLUGIN		(totem_publish_plugin_get_type ())
#define TOTEM_PUBLISH_PLUGIN(o)			(G_TYPE_CHECK_INSTANCE_CAST ((o), TOTEM_TYPE_PUBLISH_PLUGIN, TotemPublishPlugin))
#define TOTEM_PUBLISH_PLUGIN_CLASS(k)		(G_TYPE_CHECK_CLASS_CAST((k), TOTEM_TYPE_PUBLISH_PLUGIN, TotemPublishPluginClass))
#define TOTEM_IS_PUBLISH_PLUGIN(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), TOTEM_TYPE_PUBLISH_PLUGIN))
#define TOTEM_IS_PUBLISH_PLUGIN_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), TOTEM_TYPE_PUBLISH_PLUGIN))
#define TOTEM_PUBLISH_PLUGIN_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), TOTEM_TYPE_PUBLISH_PLUGIN, TotemPublishPluginClass))

#define TOTEM_PUBLISH_CONFIG_ROOT		GCONF_PREFIX "/plugins/publish"
#define TOTEM_PUBLISH_CONFIG_NAME		GCONF_PREFIX "/plugins/publish/name"
#define TOTEM_PUBLISH_CONFIG_PROTOCOL		GCONF_PREFIX "/plugins/publish/protocol"

typedef struct
{
	TotemPlugin parent;

	TotemObject       *totem;
	EpcPublisher      *publisher;
	EpcServiceMonitor *monitor;
	GtkWidget         *dialog;

	guint name_id;
	guint protocol_id;
	guint item_added_id;
	guint item_removed_id;
	guint service_found_id;
	guint service_removed_id;
} TotemPublishPlugin;

typedef struct
{
	TotemPluginClass parent_class;
} TotemPublishPluginClass;

G_MODULE_EXPORT GType register_totem_plugin		(GTypeModule     *module);
static GType totem_publish_plugin_get_type		(void);

void totem_publish_plugin_service_name_entry_changed_cb	(GtkEntry        *entry,
							 gpointer         data);
void totem_publish_plugin_encryption_button_toggled_cb	(GtkToggleButton *button,
							 gpointer         data);
void totem_publish_plugin_dialog_response_cb		(GtkDialog       *dialog,
							 gint             response,
							 gpointer         data);

TOTEM_PLUGIN_REGISTER(TotemPublishPlugin, totem_publish_plugin)

static void
totem_publish_plugin_name_changed_cb (GConfClient *client,
				      guint        id,
				      GConfEntry  *entry,
				      gpointer     data)
{
	TotemPublishPlugin *self = TOTEM_PUBLISH_PLUGIN (data);
	const gchar *service_name;

	service_name = gconf_value_get_string (entry->value);
	epc_publisher_set_service_name (self->publisher, service_name);
}

void
totem_publish_plugin_service_name_entry_changed_cb (GtkEntry *entry,
						    gpointer  data)
{
	TotemPublishPlugin *self = TOTEM_PUBLISH_PLUGIN (data);

	gconf_client_set_string (self->totem->gc,
				 TOTEM_PUBLISH_CONFIG_NAME,
				 gtk_entry_get_text (entry),
				 NULL);
}

void
totem_publish_plugin_encryption_button_toggled_cb (GtkToggleButton *button,
						   gpointer         data)
{
	TotemPublishPlugin *self = TOTEM_PUBLISH_PLUGIN (data);
	gboolean encryption = gtk_toggle_button_get_active (button);

	gconf_client_set_string (self->totem->gc,
				 TOTEM_PUBLISH_CONFIG_PROTOCOL,
				 encryption ? "https" : "http",
				 NULL);
}

static void
totem_publish_plugin_protocol_changed_cb (GConfClient *client,
					  guint        id,
					  GConfEntry  *entry,
					  gpointer     data)
{
	TotemPublishPlugin *self = TOTEM_PUBLISH_PLUGIN (data);
	const gchar *protocol_name;
	EpcProtocol protocol;

	protocol_name = gconf_value_get_string (entry->value);
	protocol = epc_protocol_from_name (protocol_name, EPC_PROTOCOL_HTTPS);
	epc_publisher_set_protocol (self->publisher, protocol);
}

static gchar*
totem_publish_plugin_build_key (const gchar *filename)
{
	return g_strconcat ("media/", filename, NULL);
}

static EpcContents*
totem_publish_plugin_playlist_cb (EpcPublisher *publisher,
				  const gchar  *key,
				  gpointer      data)
{
	TotemPublishPlugin *self = TOTEM_PUBLISH_PLUGIN (data);
	GString *buffer = g_string_new (NULL);
	EpcContents *contents = NULL;
	GList *files, *iter;
	gint i;

	files = epc_publisher_list (self->publisher, "media/*");

	g_string_append_printf (buffer,
				"[playlist]\nNumberOfEntries=%d\n",
				g_list_length (files));

	for (iter = files, i = 1; iter; iter = iter->next, ++i) {
		gchar *url = epc_publisher_get_url (self->publisher, iter->data);
		gchar *filename = iter->data;

		g_string_append_printf (buffer,
					"File%d=%s\nTitle%d=%s\n",
					i, url, i, filename + 6);

		g_free (filename);
		g_free (url);
	}

	g_list_free (files);

	contents = epc_contents_new ("audio/x-scpls",
				     buffer->str, buffer->len,
				     g_free);

	g_string_free (buffer, FALSE);

	return contents;
}

static gboolean
totem_publish_plugin_stream_cb (EpcContents *contents,
				gpointer     buffer,
				gsize       *length,
				gpointer     data)
{
	GnomeVFSHandle *handle = data;
	GnomeVFSFileSize size = 65536;

	g_return_val_if_fail (NULL != contents, FALSE);
	g_return_val_if_fail (NULL != length, FALSE);

	if (NULL == data || *length < size) {
		*length = MAX (*length, size);
		return FALSE;
	}

	if (GNOME_VFS_OK != gnome_vfs_read (handle, buffer, size, &size)) {
		gnome_vfs_close (handle);
		size = 0;
	}

	*length = size;

	return size > 0;
}

static EpcContents*
totem_publish_plugin_media_cb (EpcPublisher *publisher,
			       const gchar  *key,
			       gpointer      data)
{
	GnomeVFSHandle *handle = NULL;
	const gchar *url = data;

	if (GNOME_VFS_OK == gnome_vfs_open (&handle, url, GNOME_VFS_OPEN_READ))
		return epc_contents_stream_new (NULL, totem_publish_plugin_stream_cb, handle, NULL);

	return NULL;
}

static void
totem_publish_plugin_item_added_cb (TotemPlaylist *playlist,
				    const gchar   *filename,
				    const gchar   *url,
				    gpointer       data)
{
	TotemPublishPlugin *self = TOTEM_PUBLISH_PLUGIN (data);
	gchar *key = totem_publish_plugin_build_key (filename);

	epc_publisher_add_handler (self->publisher, key,
				   totem_publish_plugin_media_cb,
				   g_strdup (url), g_free);

	g_free (key);
}

static void
totem_publish_plugin_item_removed_cb (TotemPlaylist *playlist,
				      const gchar   *filename,
				      const gchar   *url,
				      gpointer       data)
{
	TotemPublishPlugin *self = TOTEM_PUBLISH_PLUGIN (data);
	gchar *key = totem_publish_plugin_build_key (filename);
	epc_publisher_remove (self->publisher, key);
	g_free (key);
}

static void
totem_publish_plugin_service_found_cb (EpcServiceMonitor *monitor,
				       const gchar       *type,
				       const gchar       *name,
				       const gchar       *host,
				       guint              port,
				       gpointer           data)
{
	TotemPublishPlugin *self G_GNUC_UNUSED = TOTEM_PUBLISH_PLUGIN (data);
	g_print ("+ %s (%s, %s:%d)\n", name, type, host, port);
}

static void
totem_publish_plugin_service_removed_cb (EpcServiceMonitor *monitor,
					 const gchar       *type,
					 const gchar       *name,
					 gpointer           data)
{
	TotemPublishPlugin *self G_GNUC_UNUSED = TOTEM_PUBLISH_PLUGIN (data);
	g_print ("- %s (%s)\n", name, type);
}

static gboolean
totem_publish_plugin_activate (TotemPlugin  *plugin,
			       TotemObject  *totem,
			       GError      **error)
{
	TotemPublishPlugin *self = TOTEM_PUBLISH_PLUGIN (plugin);
	TotemPlaylist *playlist = totem_get_playlist (totem);
	EpcProtocol protocol = EPC_PROTOCOL_HTTPS;

	gchar *protocol_name;
	gchar *service_name;

	g_return_val_if_fail (NULL == self->publisher, FALSE);
	g_return_val_if_fail (NULL == self->totem, FALSE);

	self->totem = g_object_ref (totem);

	gconf_client_add_dir (self->totem->gc,
			      TOTEM_PUBLISH_CONFIG_ROOT,
			      GCONF_CLIENT_PRELOAD_ONELEVEL,
			      NULL);

	self->name_id = gconf_client_notify_add (self->totem->gc,
						 TOTEM_PUBLISH_CONFIG_NAME,
						 totem_publish_plugin_name_changed_cb,
						 self, NULL, NULL);
	self->protocol_id = gconf_client_notify_add (self->totem->gc,
						     TOTEM_PUBLISH_CONFIG_PROTOCOL,
						     totem_publish_plugin_protocol_changed_cb,
						     self, NULL, NULL);

	service_name = gconf_client_get_string (self->totem->gc, TOTEM_PUBLISH_CONFIG_NAME, NULL);
	protocol_name = gconf_client_get_string (self->totem->gc, TOTEM_PUBLISH_CONFIG_PROTOCOL, NULL);

	if (protocol_name)
		protocol = epc_protocol_from_name (protocol_name, EPC_PROTOCOL_HTTPS);

	self->monitor = epc_service_monitor_new ("totem", NULL, EPC_PROTOCOL_UNKNOWN);

	self->service_found_id = g_signal_connect (self->monitor, "service-found",
		G_CALLBACK (totem_publish_plugin_service_found_cb), self);
	self->service_removed_id = g_signal_connect (self->monitor, "service-removed",
		G_CALLBACK (totem_publish_plugin_service_removed_cb), self);

	self->publisher = epc_publisher_new (service_name, "totem", NULL);
	epc_publisher_set_protocol (self->publisher, protocol);

	g_free (protocol_name);
	g_free (service_name);


	epc_publisher_add_handler (self->publisher, "playlist.pls",
				   totem_publish_plugin_playlist_cb,
				   self, NULL);

	self->item_added_id = g_signal_connect (playlist, "item-added",
		G_CALLBACK (totem_publish_plugin_item_added_cb), self);
	self->item_removed_id = g_signal_connect (playlist, "item-removed",
		G_CALLBACK (totem_publish_plugin_item_removed_cb), self);

	totem_playlist_foreach (playlist, totem_publish_plugin_item_added_cb, self);

	return epc_publisher_run_async (self->publisher, error);
}

static void
totem_publish_plugin_deactivate (TotemPlugin *plugin,
				 TotemObject *totem)
{
	TotemPublishPlugin *self = TOTEM_PUBLISH_PLUGIN (plugin);

	if (self->monitor) {
		g_object_unref (self->monitor);
		self->monitor = NULL;
	}

	if (self->publisher) {
		epc_publisher_quit (self->publisher);
		g_object_unref (self->publisher);
		self->publisher = NULL;
	}

	if (self->totem) {
		gconf_client_notify_remove (self->totem->gc, self->name_id);
		gconf_client_notify_remove (self->totem->gc, self->protocol_id);
		gconf_client_remove_dir (self->totem->gc, TOTEM_PUBLISH_CONFIG_ROOT, NULL);
		g_object_unref (self->totem);
		self->totem = NULL;
	}

	if (self->item_added_id)
		self->item_added_id = (g_source_remove (self->item_added_id), 0);
	if (self->item_removed_id)
		self->item_removed_id = (g_source_remove (self->item_removed_id), 0);
	if (self->service_found_id)
		self->service_found_id = (g_source_remove (self->service_found_id), 0);
	if (self->service_removed_id)
		self->service_removed_id = (g_source_remove (self->service_removed_id), 0);
}

void
totem_publish_plugin_dialog_response_cb (GtkDialog *dialog,
					 gint       response,
					 gpointer   data)
{
	TotemPublishPlugin *self = TOTEM_PUBLISH_PLUGIN (data);

	if (response) {
		gtk_widget_destroy (GTK_WIDGET (dialog));
		self->dialog = NULL;
	}
}

static GtkWidget*
totem_publish_plugin_create_configure_dialog (TotemPlugin *plugin)
{
	TotemPublishPlugin *self = TOTEM_PUBLISH_PLUGIN (plugin);
	GtkBuilder *builder;

	g_return_val_if_fail (NULL == self->dialog, NULL);

	builder = totem_interface_load ("publish-plugin.ui", TRUE, NULL, self);

	if (builder) {
		const gchar *service_name = epc_publisher_get_service_name (self->publisher);
		EpcProtocol protocol = epc_publisher_get_protocol (self->publisher);
		GtkWidget *widget;

		self->dialog = g_object_ref (gtk_builder_get_object (builder, "publish-settings-dialog"));

		widget = GTK_WIDGET (gtk_builder_get_object (builder, "service-name-entry"));
		gtk_entry_set_text (GTK_ENTRY (widget), service_name);

		widget = GTK_WIDGET (gtk_builder_get_object (builder, "encryption-button"));
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget),
					      EPC_PROTOCOL_HTTPS == protocol);

		g_object_unref (builder);
	}

	return self->dialog;
}

static void
totem_publish_plugin_init (TotemPublishPlugin *self)
{
}

static void
totem_publish_plugin_dispose (GObject *object)
{
	totem_publish_plugin_deactivate (TOTEM_PLUGIN (object), NULL);
	G_OBJECT_CLASS (totem_publish_plugin_parent_class)->dispose (object);
}

static void
totem_publish_plugin_class_init (TotemPublishPluginClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	TotemPluginClass *plugin_class = TOTEM_PLUGIN_CLASS (klass);

	object_class->dispose = totem_publish_plugin_dispose;

	plugin_class->activate = totem_publish_plugin_activate;
	plugin_class->deactivate = totem_publish_plugin_deactivate;
	plugin_class->create_configure_dialog = totem_publish_plugin_create_configure_dialog;
}

