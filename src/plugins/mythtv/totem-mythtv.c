/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * Copyright (C) 2007 Bastien Nocera <hadess@hadess.net>
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA.
 *
 * The Totem project hereby grant permission for non-gpl compatible GStreamer
 * plugins to be used and distributed together with GStreamer and Totem. This
 * permission are above and beyond the permissions granted by the GPL license
 * Totem is covered by.
 *
 * See license_change file for details.
 *
 */

#include "config.h"

#include <glib.h>
#include <glib-object.h>
#include <glib/gi18n-lib.h>
#include <gconf/gconf-client.h>
#include <gmodule.h>
#include <string.h>

#include "totem-video-list.h"
#include "totem-cell-renderer-video.h"
#include "video-utils.h"
#include "totem-plugin.h"
#include "totem.h"

#include <gdk-pixbuf/gdk-pixbuf.h>

#include <gmyth/gmyth_backendinfo.h>
#include <gmyth/gmyth_file_transfer.h>
#include <gmyth/gmyth_scheduler.h>
#include <gmyth/gmyth_util.h>
#include <gmyth_upnp.h>


#define TOTEM_TYPE_MYTHTV_PLUGIN		(totem_mythtv_plugin_get_type ())
#define TOTEM_MYTHTV_PLUGIN(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), TOTEM_TYPE_MYTHTV_PLUGIN, TotemMythtvPlugin))
#define TOTEM_MYTHTV_PLUGIN_CLASS(k)	(G_TYPE_CHECK_CLASS_CAST((k), TOTEM_TYPE_MYTHTV_PLUGIN, TotemMythtvPluginClass))
#define TOTEM_IS_MYTHTV_PLUGIN(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), TOTEM_TYPE_MYTHTV_PLUGIN))
#define TOTEM_IS_MYTHTV_PLUGIN_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), TOTEM_TYPE_MYTHTV_PLUGIN))
#define TOTEM_MYTHTV_PLUGIN_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), TOTEM_TYPE_MYTHTV_PLUGIN, TotemMythtvPluginClass))

#define CONF_PREFIX "/apps/totem/plugins/totem_mythtv/"
#define CONF_IP CONF_PREFIX "address"
#define CONF_USER CONF_PREFIX "user"
#define CONF_PASSWORD CONF_PREFIX "password"
#define CONF_DATABASE CONF_PREFIX "database"
#define CONF_PORT CONF_PREFIX "port"

enum {
	FILENAME_COL,
	URI_COL,
	THUMBNAIL_COL,
	NAME_COL,
	DESCRIPTION_COL,
	NUM_COLS
};

typedef struct
{
	TotemPlugin parent;

	GList *lst_b_info;
	GMythUPnP *upnp;

	TotemObject *totem;
	GConfClient *client;
	GtkTreeView *treeview;
	GtkTreeModel *model;
	GtkWidget *sidebar;
} TotemMythtvPlugin;

typedef struct
{
	TotemPluginClass parent_class;
} TotemMythtvPluginClass;


G_MODULE_EXPORT GType register_totem_plugin	(GTypeModule *module);
GType	totem_mythtv_plugin_get_type		(void) G_GNUC_CONST;

static void totem_mythtv_plugin_init		(TotemMythtvPlugin *plugin);
static void totem_mythtv_plugin_finalize	(GObject *object);
static gboolean impl_activate			(TotemPlugin *plugin, TotemObject *totem, GError **error);
static void impl_deactivate			(TotemPlugin *plugin, TotemObject *totem);

TOTEM_PLUGIN_REGISTER(TotemMythtvPlugin, totem_mythtv_plugin)

#define MAX_THUMB_SIZE 500 * 1024 * 1024
#define THUMB_HEIGHT 32

static GdkPixbuf *
get_thumbnail (TotemMythtvPlugin *plugin, GMythBackendInfo *b_info, char *fname)
{
	GMythFileTransfer *transfer;
	GdkPixbufLoader *loader;
	GdkPixbuf *pixbuf;
	GMythFileReadResult res;
	guint64 to_read;
	GByteArray *data;

	if (gmyth_util_file_exists (b_info, fname) == FALSE)
		return NULL;

	transfer = gmyth_file_transfer_new (b_info);
	if (gmyth_file_transfer_open(transfer, fname) == FALSE)
		return NULL;

	to_read = gmyth_file_transfer_get_filesize (transfer);
	/* Leave if the thumbnail is just too big */
	if (to_read > MAX_THUMB_SIZE) {
		gmyth_file_transfer_close (transfer);
		return NULL;
	}

	loader = gdk_pixbuf_loader_new_with_type ("png", NULL);
	data = g_byte_array_sized_new (to_read);

	res = gmyth_file_transfer_read(transfer, data, to_read, FALSE);
	if (gdk_pixbuf_loader_write (loader, data->data, to_read, NULL) == FALSE) {
		res = GMYTH_FILE_READ_ERROR;
	}

	gmyth_file_transfer_close (transfer);
	g_object_unref (transfer);
	g_byte_array_free (data, TRUE);

	if (res != GMYTH_FILE_READ_OK && res != GMYTH_FILE_READ_EOF) {
		g_object_unref (loader);
		return NULL;
	}

	if (gdk_pixbuf_loader_close (loader, NULL) == FALSE) {
		g_object_unref (loader);
		return NULL;
	}

	pixbuf = gdk_pixbuf_loader_get_pixbuf (loader);
	if (pixbuf != NULL)
		g_object_ref (pixbuf);
	g_object_unref (loader);

	if (pixbuf == NULL)
		return NULL;

	if (gdk_pixbuf_get_height (pixbuf) != THUMB_HEIGHT) {
		GdkPixbuf *scaled;
		int height, width;

		height = THUMB_HEIGHT;
		width = gdk_pixbuf_get_width (pixbuf) * height / gdk_pixbuf_get_height (pixbuf);
		scaled = gdk_pixbuf_scale_simple (pixbuf, width, height, GDK_INTERP_BILINEAR);
		g_object_unref (pixbuf);

		return scaled;
	}

	return pixbuf;
}

static void
create_treeview (TotemMythtvPlugin *plugin)
{
	TotemCellRendererVideo *renderer;

	/* Treeview and model */
	plugin->model = GTK_TREE_MODEL (gtk_list_store_new (NUM_COLS,
							    G_TYPE_STRING,
							    G_TYPE_STRING,
							    G_TYPE_OBJECT,
							    G_TYPE_STRING,
							    G_TYPE_STRING));

	plugin->treeview = g_object_new (TOTEM_TYPE_VIDEO_LIST,
					 "totem", plugin->totem,
					 "mrl-column", URI_COL,
					 "tooltip-column", DESCRIPTION_COL,
					 NULL);

	gtk_tree_view_set_model (GTK_TREE_VIEW (plugin->treeview),
				 GTK_TREE_MODEL (plugin->model));

	renderer = totem_cell_renderer_video_new (TRUE);
	gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (plugin->treeview), 0,
						     _("Recordings"), GTK_CELL_RENDERER (renderer),
						     "thumbnail", THUMBNAIL_COL,
						     "title", NAME_COL,
						     NULL);

	gtk_tree_view_set_headers_visible (plugin->treeview, FALSE);
}

static void
totem_mythtv_list_recordings (TotemMythtvPlugin *plugin,
							  GMythBackendInfo *b_info)
{
	GMythScheduler *scheduler;
	GList *list, *l;

	if (b_info == NULL)
		return;

	scheduler = gmyth_scheduler_new();
	if (gmyth_scheduler_connect_with_timeout(scheduler,
						 b_info, 5) == FALSE) {
		g_message ("Couldn't connect to scheduler");
		g_object_unref (scheduler);
		return;
	}

	if (gmyth_scheduler_get_recorded_list(scheduler, &list) < 0) {
		g_message ("Couldn't get recordings list");
		gmyth_scheduler_disconnect (scheduler);
		g_object_unref (scheduler);
		return;
	}

	gmyth_scheduler_disconnect (scheduler);
	g_object_unref (scheduler);

	for (l = list; l != NULL; l = l->next) {
		RecordedInfo *recorded_info = (RecordedInfo *) l->data;

		if (gmyth_util_file_exists
		    (b_info, recorded_info->basename->str)) {
		    	GtkTreeIter iter;
		    	GdkPixbuf *pixbuf;
		    	char *full_name = NULL;
		    	char *thumb_fname, *uri;

		    	if (recorded_info->subtitle->str != NULL && recorded_info->subtitle->str[0] != '\0')
		    		full_name = g_strdup_printf ("%s - %s",
		    					     recorded_info->title->str,
		    					     recorded_info->subtitle->str);
		    	thumb_fname = g_strdup_printf ("%s.png", recorded_info->basename->str);
		    	uri = g_strdup_printf ("myth://%s:%d/%s",
		    			       b_info->hostname,
		    			       b_info->port,
		    			       recorded_info->basename->str);
		    	pixbuf = get_thumbnail (plugin, b_info, thumb_fname);
		    	g_free (thumb_fname);

		    	gtk_list_store_insert_with_values (GTK_LIST_STORE (plugin->model), &iter, G_MAXINT32,
		    					   FILENAME_COL, recorded_info->basename->str,
		    					   URI_COL, uri,
		    					   THUMBNAIL_COL, pixbuf,
		    					   NAME_COL, full_name ? full_name : recorded_info->title->str,
		    					   DESCRIPTION_COL, recorded_info->description->str,
		    					   -1);
		    	g_free (full_name);
		    	g_free (uri);
		}
		gmyth_recorded_info_free(recorded_info);
	}

	g_list_free (list);
}

static void
totem_mythtv_plugin_class_init (TotemMythtvPluginClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	TotemPluginClass *plugin_class = TOTEM_PLUGIN_CLASS (klass);

	object_class->finalize = totem_mythtv_plugin_finalize;

	plugin_class->activate = impl_activate;
	plugin_class->deactivate = impl_deactivate;
}

static void
device_found_cb (GMythUPnP *upnp,
				 GMythBackendInfo *b_info,
				 TotemMythtvPlugin *plugin)
{
	if (!g_list_find (plugin->lst_b_info, b_info)) {
		plugin->lst_b_info = g_list_append (plugin->lst_b_info,
				g_object_ref (b_info));
		totem_mythtv_list_recordings (plugin, b_info);
	}
}

static void
device_lost_cb (GMythUPnP *upnp,
				 GMythBackendInfo *info,
				 TotemMythtvPlugin *plugin)
{
	GList *elem;
	GMythBackendInfo *b_info;

	elem = g_list_find (plugin->lst_b_info, info);
	if (elem && elem->data) {
		b_info = elem->data;
		plugin->lst_b_info = g_list_remove (plugin->lst_b_info,
				b_info);
		g_object_unref (b_info);
	}

}

static void
totem_mythtv_update_binfo (TotemMythtvPlugin *plugin)
{
	GList *lst;
	GList *w;

	/* Clear old b_info */
	if (plugin->lst_b_info != NULL) {
		g_list_foreach (plugin->lst_b_info, (GFunc) g_object_unref, NULL);
		g_list_free (plugin->lst_b_info);
		plugin->lst_b_info = NULL;
	}


	/* Using GMythUPnP to search for avaliable servers */
	if (plugin->upnp == NULL) {
		plugin->upnp = gmyth_upnp_get_instance ();
		g_signal_connect (G_OBJECT (plugin->upnp),
						  "device-found",
						  G_CALLBACK (device_found_cb),
						  plugin);
		plugin->upnp = gmyth_upnp_get_instance ();
		g_signal_connect (G_OBJECT (plugin->upnp),
						  "device-lost",
						  G_CALLBACK (device_lost_cb),
						  plugin);
	}

	/* Load current servers */
	lst = gmyth_upnp_get_devices (plugin->upnp);
	for (w = lst; w != NULL; w = w->next) {
		GMythBackendInfo *b_info;

		b_info = (GMythBackendInfo *) w->data;
		plugin->lst_b_info = g_list_append (plugin->lst_b_info,
				b_info);
		totem_mythtv_list_recordings (plugin, b_info);
	}
	g_list_free (lst);
	gmyth_upnp_search (plugin->upnp);
}

static void
refresh_cb (GtkWidget *button, TotemMythtvPlugin *plugin)
{
	gtk_widget_set_sensitive (button, FALSE);
	gtk_list_store_clear (GTK_LIST_STORE (plugin->model));
	totem_mythtv_update_binfo (plugin);
	totem_gdk_window_set_waiting_cursor (plugin->sidebar->window);
	gdk_window_set_cursor (plugin->sidebar->window, NULL);
	gtk_widget_set_sensitive (button, TRUE);
}

static void
totem_mythtv_plugin_init (TotemMythtvPlugin *plugin)
{
	totem_mythtv_update_binfo (plugin);
}

static void
totem_mythtv_plugin_finalize (GObject *object)
{
	TotemMythtvPlugin *plugin = TOTEM_MYTHTV_PLUGIN(object);

	if (plugin->lst_b_info != NULL) {
		g_list_foreach (plugin->lst_b_info, (GFunc ) g_object_unref, NULL);
		g_list_free (plugin->lst_b_info);
		plugin->lst_b_info = NULL;
	}
	if (plugin->client != NULL) {
		g_object_unref (plugin->client);
		plugin->client = NULL;
	}

	G_OBJECT_CLASS (totem_mythtv_plugin_parent_class)->finalize (object);
}

static gboolean
impl_activate (TotemPlugin *plugin,
	       TotemObject *totem,
	       GError **error)
{
	GtkWidget *scrolled, *box, *button;
	TotemMythtvPlugin *tm = TOTEM_MYTHTV_PLUGIN(plugin);
 
	tm->totem = g_object_ref (totem);

	box = gtk_vbox_new (FALSE, 6);
	button = gtk_button_new_from_stock (GTK_STOCK_REFRESH);
	g_signal_connect (G_OBJECT (button), "clicked",
			  G_CALLBACK (refresh_cb), plugin);
	scrolled = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled),
					GTK_POLICY_NEVER,
					GTK_POLICY_AUTOMATIC);
	create_treeview (tm);
	gtk_scrolled_window_add_with_viewport (GTK_SCROLLED_WINDOW (scrolled),
					       GTK_WIDGET (tm->treeview));
	gtk_container_add (GTK_CONTAINER (box), scrolled);
	gtk_box_pack_end (GTK_BOX (box), button, FALSE, FALSE, 0);
	gtk_widget_show_all (box);
	totem_add_sidebar_page (totem,
				"mythtv",
				_("MythTV Recordings"),
				box);

	tm->sidebar = g_object_ref (box);

	totem_mythtv_update_binfo (TOTEM_MYTHTV_PLUGIN(plugin));

	/* FIXME we should only do that if it will be done in the background */
#if 0
	totem_gdk_window_set_waiting_cursor (box->window);
	totem_mythtv_list_recordings (TOTEM_MYTHTV_PLUGIN(plugin));
	gdk_window_set_cursor (box->window, NULL);
#endif
	return TRUE;
}

static void
impl_deactivate	(TotemPlugin *plugin,
		 TotemObject *totem)
{
	TotemMythtvPlugin *tm = TOTEM_MYTHTV_PLUGIN(plugin);

	g_object_unref (tm->sidebar);
	totem_remove_sidebar_page (totem, "mythtv");

	if (tm && tm->client != NULL) {
		g_object_unref (tm->client);
		tm->client = NULL;
	}
	g_object_unref (totem);
}

