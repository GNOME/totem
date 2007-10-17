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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
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

#include "video-utils.h"
#include "totem-plugin.h"
#include "totem.h"

#include <gdk-pixbuf/gdk-pixbuf.h>

#include <gmyth_backendinfo.h>
#include <gmyth_file_transfer.h>
#include <gmyth_scheduler.h>
#include <gmyth_util.h>

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
	THUMBNAIL_FILENAME_COL,
	THUMBNAIL_COL,
	NAME_COL,
	DESCRIPTION_COL,
	NUM_COLS
};

typedef struct
{
	TotemPlugin parent;

	GMythBackendInfo *b_info;

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

static GdkPixbuf *
get_thumbnail (TotemMythtvPlugin *plugin, char *fname)
{
	GMythFileTransfer *transfer;
	GdkPixbufLoader *loader;
	GdkPixbuf *pixbuf;
	GMythFileReadResult res;
	guint64 to_read;
	GByteArray *data;

	if (gmyth_util_file_exists (plugin->b_info, fname) == FALSE)
		return NULL;

	transfer = gmyth_file_transfer_new (plugin->b_info);
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
	g_object_ref (pixbuf);
	g_object_unref (loader);

	return pixbuf;
}

#define THUMB_HEIGHT 32

static void
set_thumbnail_icon (GtkTreeViewColumn *column,
		    GtkCellRenderer *renderer,
		    GtkTreeModel *model,
		    GtkTreeIter *iter,
		    TotemMythtvPlugin *plugin)
{
	GdkPixbuf *pixbuf, *scaled;
	int width, height;
	GtkTreePath *path;
	char *thumb_fname;

	/* If we already have a pixbuf, use it */
	gtk_tree_model_get (model, iter,
			    THUMBNAIL_COL, &pixbuf, -1);
	if (pixbuf != NULL) {
		g_object_set (renderer, "pixbuf", pixbuf, NULL);
		g_object_unref (pixbuf);
		return;
	}

	/* If there's no backend info, no point in continuing */
	if (plugin->b_info == NULL)
		return;

	/* Filename is zero'ed when we've tried getting the
	 * thumbnail once already */
	gtk_tree_model_get (model, iter,
			    THUMBNAIL_FILENAME_COL, &thumb_fname, -1);
	if (thumb_fname == NULL)
		return;
	pixbuf = get_thumbnail (plugin, thumb_fname);
	g_free (thumb_fname);

	/* No pixbuf */
	if (pixbuf == NULL) {
		gtk_list_store_set (GTK_LIST_STORE (model), iter,
				    THUMBNAIL_FILENAME_COL, NULL, -1);
		g_object_set (renderer, "icon-name", "video", NULL);
		path = gtk_tree_model_get_path (model, iter);
		gtk_tree_model_row_changed (model, path, iter);
		gtk_tree_path_free (path);
		return;
	}

	height = THUMB_HEIGHT;
	width = gdk_pixbuf_get_width (pixbuf) * height / gdk_pixbuf_get_height (pixbuf);
	scaled = gdk_pixbuf_scale_simple (pixbuf, width, height, GDK_INTERP_BILINEAR);
	g_object_unref (pixbuf);

	gtk_list_store_set (GTK_LIST_STORE (model), iter,
			    THUMBNAIL_COL, scaled,
			    THUMBNAIL_FILENAME_COL, NULL, -1);
	g_object_set (renderer, "pixbuf", scaled, NULL);

	path = gtk_tree_model_get_path (model, iter);
	gtk_tree_model_row_changed (model, path, iter);
	gtk_tree_path_free (path);
}

static void
row_activated_cb (GtkTreeView *tree_view,
		  GtkTreePath *path,
		  GtkTreeViewColumn *column,
		  TotemMythtvPlugin *plugin)
{
	GtkTreeIter iter;
	char *uri;

	if (gtk_tree_model_get_iter (plugin->model, &iter, path) == FALSE)
		g_return_if_reached ();

	gtk_tree_model_get (plugin->model, &iter,
			    URI_COL, &uri, -1);
	g_return_if_fail (uri != NULL);

	totem_action_set_mrl_and_play (plugin->totem, uri);
}

static void
create_treeview (TotemMythtvPlugin *plugin)
{
	GtkCellRenderer *renderer;
	GtkTreeViewColumn *column;

	/* Treeview and model */
	plugin->model = GTK_TREE_MODEL (gtk_list_store_new (NUM_COLS,
							    G_TYPE_STRING,
							    G_TYPE_STRING,
							    G_TYPE_STRING,
							    G_TYPE_OBJECT,
							    G_TYPE_STRING,
							    G_TYPE_STRING));
	plugin->treeview = GTK_TREE_VIEW (gtk_tree_view_new_with_model (plugin->model));
	g_object_unref (G_OBJECT(plugin->model));
	gtk_tree_view_set_headers_visible (plugin->treeview, FALSE);

	g_signal_connect (G_OBJECT (plugin->treeview), "row-activated",
			  G_CALLBACK (row_activated_cb), plugin);

	/* Playing pix */
	renderer = gtk_cell_renderer_pixbuf_new ();
	column = gtk_tree_view_column_new ();
	gtk_tree_view_column_pack_start (column, renderer, FALSE);
	g_object_set (G_OBJECT (renderer), "xalign", 0.0, NULL);
	gtk_tree_view_column_set_cell_data_func (column, renderer,
						 (GtkTreeCellDataFunc) set_thumbnail_icon,
						 plugin, NULL);
	gtk_tree_view_append_column (plugin->treeview, column);

	/* Labels */
	renderer = gtk_cell_renderer_text_new ();
	gtk_tree_view_column_pack_start (column, renderer, TRUE);
	g_object_set (G_OBJECT (renderer), "ellipsize", PANGO_ELLIPSIZE_END, "xalign", 0.0, NULL);
	gtk_tree_view_column_set_attributes (column, renderer,
					     "text", NAME_COL, NULL);
}

static void
totem_mythtv_list_recordings (TotemMythtvPlugin *plugin)
{
	GMythScheduler *scheduler;
	GList *list, *l;

	if (plugin->b_info == NULL)
		return;

	scheduler = gmyth_scheduler_new();
	if (gmyth_scheduler_connect_with_timeout(scheduler,
						 plugin->b_info, 5) == FALSE) {
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
		    (plugin->b_info, recorded_info->basename->str)) {
		    	GtkTreeIter iter;
		    	char *full_name = NULL;
		    	char *thumb_fname, *uri;

		    	if (recorded_info->subtitle->str != NULL && recorded_info->subtitle->str[0] != '\0')
		    		full_name = g_strdup_printf ("%s - %s",
		    					     recorded_info->title->str,
		    					     recorded_info->subtitle->str);
		    	thumb_fname = g_strdup_printf ("%s.png", recorded_info->basename->str);
		    	uri = g_strdup_printf ("myth://%s:%d/%s",
		    			       plugin->b_info->hostname,
		    			       plugin->b_info->port,
		    			       recorded_info->basename->str);

		    	gtk_list_store_insert_with_values (GTK_LIST_STORE (plugin->model), &iter, G_MAXINT32,
		    					   FILENAME_COL, recorded_info->basename->str,
		    					   URI_COL, uri,
		    					   THUMBNAIL_FILENAME_COL, thumb_fname,
		    					   NAME_COL, full_name ? full_name : recorded_info->title->str,
		    					   DESCRIPTION_COL, recorded_info->description->str,
		    					   -1);
		    	g_free (full_name);
		    	g_free (thumb_fname);
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
totem_mythtv_update_binfo (TotemMythtvPlugin *plugin)
{
	char *address, *user, *password, *database;
	int port;

	if (plugin->b_info != NULL) {
		//FIXME why would this crash?
		//g_object_unref (plugin->b_info);
		plugin->b_info = NULL;
	}

	if (plugin->client == NULL)
		plugin->client = gconf_client_get_default ();
	if (plugin->client == NULL)
		return;

	address = gconf_client_get_string (plugin->client, CONF_IP, NULL);
	/* No address? */
	if (address == NULL || address[0] == '\0')
		return;

	user = gconf_client_get_string (plugin->client, CONF_USER, NULL);
	if (user == NULL || user[0] == '\0')
		user = g_strdup ("mythtv");
	password = gconf_client_get_string (plugin->client, CONF_PASSWORD, NULL);
	if (password == NULL || password[0] == '\0')
		password = g_strdup ("mythtv");
	database = gconf_client_get_string (plugin->client, CONF_DATABASE, NULL);
	if (database == NULL || database[0] == '\0')
		database = g_strdup ("mythconverg");
	port = gconf_client_get_int (plugin->client, CONF_PORT, NULL);
	if (port == 0)
		port = 6543;

	plugin->b_info = gmyth_backend_info_new_full (address,
						      user,
						      password,
						      database,
						      port);
}

static void
refresh_cb (GtkWidget *button, TotemMythtvPlugin *plugin)
{
	gtk_widget_set_sensitive (button, FALSE);
	totem_mythtv_update_binfo (plugin);
	gtk_list_store_clear (GTK_LIST_STORE (plugin->model));
	totem_gdk_window_set_waiting_cursor (plugin->sidebar->window);
	totem_mythtv_list_recordings (plugin);
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

	if (plugin->b_info != NULL) {
		g_object_unref (plugin->b_info);
		plugin->b_info = NULL;
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
	GtkWidget *viewport, *box, *button;
	TotemMythtvPlugin *tm = TOTEM_MYTHTV_PLUGIN(plugin);

	tm->totem = g_object_ref (totem);

	box = gtk_vbox_new (FALSE, 6);
	button = gtk_button_new_from_stock (GTK_STOCK_REFRESH);
	g_signal_connect (G_OBJECT (button), "clicked",
			  G_CALLBACK (refresh_cb), plugin);
	viewport = gtk_viewport_new (NULL, NULL);
	create_treeview (tm);
	gtk_container_add (GTK_CONTAINER (viewport), GTK_WIDGET (tm->treeview));
	gtk_container_add (GTK_CONTAINER (box), viewport);
	gtk_box_pack_end (GTK_BOX (box), button, FALSE, FALSE, 0);
	gtk_widget_show_all (box);
	totem_add_sidebar_page (totem,
				"mythtv",
				_("MythTV Recordings"),
				box);

	tm->sidebar = g_object_ref (box);

	totem_mythtv_update_binfo (TOTEM_MYTHTV_PLUGIN(plugin));

	totem_gdk_window_set_waiting_cursor (box->window);
	totem_mythtv_list_recordings (TOTEM_MYTHTV_PLUGIN(plugin));
	gdk_window_set_cursor (box->window, NULL);

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

