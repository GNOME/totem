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
#include <gmodule.h>
#include <string.h>

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

enum {
	FILENAME_COL,
	THUMBNAIL_COL,
	NAME_COL,
	DESCRIPTION_COL,
	NUM_COLS
};

typedef struct
{
	TotemPlugin parent;

	GMythBackendInfo *b_info;
	GtkTreeView *treeview;
	GtkTreeModel *model;
	GtkTreeSelection *selection;
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

static void
set_cell_text (GtkTreeViewColumn *column,
	       GtkCellRenderer *renderer,
	       GtkTreeModel *model,
	       GtkTreeIter *iter,
	       TotemMythtvPlugin *plugin)
{
	GtkTreePath *path;
	char *text, *name, *description;
	gboolean is_selected;

	gtk_tree_model_get (model, iter, NAME_COL, &name,
			    DESCRIPTION_COL, &description,
			    -1);
	if (name == NULL) {
		g_free (description);
		return;
	}

	is_selected = gtk_tree_selection_iter_is_selected (plugin->selection, iter);
	if (description != NULL && is_selected != FALSE) {
		text = g_markup_printf_escaped ("<b>%s</b>\n%s",
						name, description);
	} else if (is_selected != FALSE) {
		text = g_markup_printf_escaped ("<b>%s</b>", name);
	} else {
		text = g_strdup (name);
	}

	g_free (name);
	g_free (description);

	g_object_set (renderer, "markup", text, NULL);
	g_free (text);

	path = gtk_tree_model_get_path (model, iter);
	gtk_tree_model_row_changed (model, path, iter);
	gtk_tree_path_free (path);
}

#define BUFFER_SIZE 1024

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

	loader = gdk_pixbuf_loader_new_with_type ("png", NULL);
	to_read = gmyth_file_transfer_get_filesize (transfer);
	data = g_byte_array_sized_new (to_read);

	res = gmyth_file_transfer_read(transfer, data, to_read, FALSE);
	if (gdk_pixbuf_loader_write (loader, data->data, to_read, NULL) == FALSE) {
		res = GMYTH_FILE_READ_ERROR;
	}

	gmyth_file_transfer_close (transfer);
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

#define DEFAULT_BIG_THUMB_HEIGHT 50
#define DEFAULT_SMALL_THUMB_HEIGHT 25

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

	gtk_tree_model_get (model, iter, THUMBNAIL_COL, &pixbuf, -1);
	if (pixbuf == NULL) {
		char *fname, *thumb_fname;

		gtk_tree_model_get (model, iter, FILENAME_COL, &fname, -1);
		if (fname == NULL)
			return;
		thumb_fname = g_strdup_printf ("%s.png", fname);

		pixbuf = get_thumbnail (plugin, thumb_fname);
		gtk_list_store_set (GTK_LIST_STORE (model), iter,
				    THUMBNAIL_COL, pixbuf, -1);
	}

	if (pixbuf == NULL)
		return;

	height = gtk_tree_selection_iter_is_selected (plugin->selection, iter)
		? DEFAULT_BIG_THUMB_HEIGHT : DEFAULT_SMALL_THUMB_HEIGHT;
	width = gdk_pixbuf_get_width (pixbuf) * height / gdk_pixbuf_get_height (pixbuf);
	scaled = gdk_pixbuf_scale_simple (pixbuf, width, height, GDK_INTERP_BILINEAR);
	g_object_unref (pixbuf);

	g_object_set (renderer, "pixbuf", scaled, NULL);
	g_object_unref (scaled);

	path = gtk_tree_model_get_path (model, iter);
	gtk_tree_model_row_changed (model, path, iter);
	gtk_tree_path_free (path);
}

static void
create_treeview (TotemMythtvPlugin *plugin)
{
	GtkCellRenderer *renderer;
	GtkTreeViewColumn *column;

	/* Treeview and model */
	plugin->model = GTK_TREE_MODEL (gtk_list_store_new (NUM_COLS,
						    G_TYPE_STRING,
						    G_TYPE_OBJECT,
						    G_TYPE_STRING,
						    G_TYPE_STRING));
	plugin->treeview = GTK_TREE_VIEW (gtk_tree_view_new_with_model (plugin->model));
	g_object_unref (G_OBJECT(plugin->model));
	gtk_tree_view_set_headers_visible (plugin->treeview, FALSE);

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
	gtk_tree_view_column_set_cell_data_func (column, renderer,
						 (GtkTreeCellDataFunc) set_cell_text,
						 plugin, NULL);

	plugin->selection = gtk_tree_view_get_selection (plugin->treeview);
}

static void
list_recordings (TotemMythtvPlugin *plugin)
{
	GMythScheduler *scheduler;
	GList *list, *l;

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

		    	if (recorded_info->subtitle->str != NULL)
		    		full_name = g_strdup_printf ("%s - %s",
		    					     recorded_info->title->str,
		    					     recorded_info->subtitle->str);

		    	gtk_list_store_insert_with_values (GTK_LIST_STORE (plugin->model), &iter, G_MAXINT32,
		    					   FILENAME_COL, recorded_info->basename->str,
		    					   NAME_COL, full_name ? full_name : recorded_info->title->str,
		    					   DESCRIPTION_COL, recorded_info->description->str,
		    					   -1);
		    	g_free (full_name);
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
totem_mythtv_plugin_init (TotemMythtvPlugin *plugin)
{
	//FIXME obviously
	plugin->b_info = gmyth_backend_info_new_full ("192.168.1.105",
						      "mythtv",
						      "mythtv",
						      "mythconverg",
						      6543);
}

static void
totem_mythtv_plugin_finalize (GObject *object)
{
	TotemMythtvPlugin *plugin = TOTEM_MYTHTV_PLUGIN(object);
	if (plugin->b_info != NULL) {
		g_object_unref (plugin->b_info);
		plugin->b_info = NULL;
	}

	G_OBJECT_CLASS (totem_mythtv_plugin_parent_class)->finalize (object);
}

static gboolean
impl_activate (TotemPlugin *plugin,
	       TotemObject *totem,
	       GError **error)
{
	GtkWidget *viewport;
	TotemMythtvPlugin *tm = TOTEM_MYTHTV_PLUGIN(plugin);

	viewport = gtk_viewport_new (NULL, NULL);
	create_treeview (tm);
	gtk_container_add (GTK_CONTAINER(viewport), GTK_WIDGET (tm->treeview));
	gtk_widget_show_all (viewport);
	totem_add_sidebar_page (totem,
				"mythtv",
				_("MythTV Recordings"),
				viewport);
	g_message ("Just added the mythtv sidebar");

	list_recordings (TOTEM_MYTHTV_PLUGIN(plugin));

	return TRUE;
}

static void
impl_deactivate	(TotemPlugin *plugin,
		 TotemObject *totem)
{
	totem_remove_sidebar_page (totem, "mythtv");
	//FIXME disconnect from stuff
	g_message ("Just removed the mythtv sidebar");
}

