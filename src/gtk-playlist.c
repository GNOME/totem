/* gtk-playlist.c

   Copyright (C) 2002 Bastien Nocera

   The Gnome Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   The Gnome Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with the Gnome Library; see the file COPYING.LIB.  If not,
   write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.

   Author: Bastien Nocera <hadess@hadess.net>
 */

#include "config.h"
#include "gtk-playlist.h"

#include <gnome.h>
#include <libxml/tree.h>
#include <libxml/parser.h>
#include <glade/glade.h>
#include <gconf/gconf-client.h>
#include <libgnomevfs/gnome-vfs.h>
#include <libgnomevfs/gnome-vfs-mime-utils.h>
#include <string.h>

#include "debug.h"

#define READ_CHUNK_SIZE 8192

struct GtkPlaylistPrivate
{
	GladeXML *xml;

	GtkWidget *treeview;
	GtkTreeModel *model;
	GtkTreePath *current;

	/* This is the playing icon */
	GdkPixbuf *icon;

	/* This is a scratch list for when we're removing files */
	GList *list;

	/* This is the current path for the file selector */
	char *path;

	/* Repeat mode */
	gboolean repeat;

	GConfClient *gc;
};

/* Signals */
enum {
	CHANGED,
	CURRENT_REMOVED,
	LAST_SIGNAL
};

enum {
	PIX_COL,
	FILENAME_COL,
	URI_COL,
	NUM_COLS
};

static int gtk_playlist_table_signals[LAST_SIGNAL] = { 0 };

static const GtkTargetEntry target_table[] = {
	{ "text/uri-list", 0, 0 },
};

static GtkWidgetClass *parent_class = NULL;

static void gtk_playlist_class_init (GtkPlaylistClass *class);
static void gtk_playlist_init       (GtkPlaylist      *label);

static void init_treeview (GtkWidget *treeview, GtkPlaylist *playlist);
static gboolean gtk_playlist_unset_playing (GtkPlaylist *playlist);

GtkType
gtk_playlist_get_type (void)
{
	static GtkType gtk_playlist_type = 0;

	if (!gtk_playlist_type) {
		static const GTypeInfo gtk_playlist_info = {
			sizeof (GtkPlaylistClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gtk_playlist_class_init,
			(GClassFinalizeFunc) NULL,
			NULL /* class_data */,
			sizeof (GtkPlaylist),
			0 /* n_preallocs */,
			(GInstanceInitFunc) gtk_playlist_init,
		};

		gtk_playlist_type = g_type_register_static (GTK_TYPE_DIALOG,
				"GtkPlaylist", &gtk_playlist_info,
				(GTypeFlags)0);
	}

	return gtk_playlist_type;
}

/* Helper functions */
static gboolean
gtk_tree_model_iter_previous (GtkTreeModel *tree_model, GtkTreeIter *iter)
{
	GtkTreePath *path;
	gboolean ret;

	path = gtk_tree_model_get_path (tree_model, iter);
	ret = gtk_tree_path_prev (path);
	if (ret == TRUE)
		gtk_tree_model_get_iter (tree_model, iter, path);

	gtk_tree_path_free (path);
	return ret;
}

static gboolean
gtk_tree_path_equals (GtkTreePath *path1, GtkTreePath *path2)
{
	char *str1, *str2;
	gboolean retval;

	if (path1 == NULL && path2 == NULL)
		return TRUE;
	if (path1 == NULL || path2 == NULL)
		return FALSE;

	str1 = gtk_tree_path_to_string (path1);
	str2 = gtk_tree_path_to_string (path2);

	if (strcmp (str1, str2) == 0)
		retval = TRUE;
	else
		retval = FALSE;

	g_free (str1);
	g_free (str2);

	return retval;
}

static void
gtk_tree_selection_has_selected_foreach (GtkTreeModel *model,
		GtkTreePath *path, GtkTreeIter *iter, gpointer user_data)
{
	int *retval = (gboolean *)user_data;
	*retval = TRUE;
}

static gboolean
gtk_tree_selection_has_selected (GtkTreeSelection *selection)
{
	int retval, *boolean;

	retval = FALSE;
	boolean = &retval;
	gtk_tree_selection_selected_foreach (selection,
			gtk_tree_selection_has_selected_foreach,
			(gpointer) (boolean));

	return retval;
}

static GnomeVFSResult
eel_read_entire_file (const char *uri,
		int *file_size,
		char **file_contents)
{
	GnomeVFSResult result;
	GnomeVFSHandle *handle;
	char *buffer;
	GnomeVFSFileSize total_bytes_read;
	GnomeVFSFileSize bytes_read;

	*file_size = 0;
	*file_contents = NULL;

	/* Open the file. */
	result = gnome_vfs_open (&handle, uri, GNOME_VFS_OPEN_READ);
	if (result != GNOME_VFS_OK) {
		return result;
	}

	/* Read the whole thing. */
	buffer = NULL;
	total_bytes_read = 0;
	do {
		buffer = g_realloc (buffer, total_bytes_read + READ_CHUNK_SIZE);
		result = gnome_vfs_read (handle,
				buffer + total_bytes_read,
				READ_CHUNK_SIZE,
				&bytes_read);
		if (result != GNOME_VFS_OK && result != GNOME_VFS_ERROR_EOF) {
			g_free (buffer);
			gnome_vfs_close (handle);
			return result;
		}

		/* Check for overflow. */
		if (total_bytes_read + bytes_read < total_bytes_read) {
			g_free (buffer);
			gnome_vfs_close (handle);
			return GNOME_VFS_ERROR_TOO_BIG;
		}

		total_bytes_read += bytes_read;
	} while (result == GNOME_VFS_OK);

	/* Close the file. */
	result = gnome_vfs_close (handle);
	if (result != GNOME_VFS_OK) {
		g_free (buffer);
		return result;
	}

	/* Return the file. */
	*file_size = total_bytes_read;
	*file_contents = g_realloc (buffer, total_bytes_read);
	return GNOME_VFS_OK;
}

static int
read_ini_line_int (char **lines, const char *key)
{
	int retval = -1;
	int i;

	if (lines == NULL || key == NULL)
		return -1;

	for (i = 0; (lines[i] != NULL && retval == -1); i++)
	{
		if (g_ascii_strncasecmp (lines[i], key, strlen (key)) == 0)
		{
			char **bits;

			bits = g_strsplit (lines[i], "=", 2);
			if (bits[0] == NULL || bits [1] == NULL)
			{
				g_strfreev (bits);
				return -1;
			}

			retval = (gint) g_strtod (bits[1], NULL);
			g_strfreev (bits);
		}
	}

	return retval;
}

static char*
read_ini_line_string (char **lines, const char *key)
{
	char *retval = NULL;
	int i;

	if (lines == NULL || key == NULL)
		return NULL;

	for (i = 0; (lines[i] != NULL && retval == NULL); i++)
	{
		if (g_ascii_strncasecmp (lines[i], key, strlen (key)) == 0)
		{
			char **bits;

			bits = g_strsplit (lines[i], "=", 2);
			if (bits[0] == NULL || bits [1] == NULL)
			{
				g_strfreev (bits);
				return NULL;
			}

			retval = g_strdup (bits[1]);
			g_strfreev (bits);
		}
	}

	return retval;
}

static void
drop_cb (GtkWidget     *widget,
         GdkDragContext     *context, 
	 gint                x,
	 gint                y,
	 GtkSelectionData   *data, 
	 guint               info, 
	 guint               time, 
	 gpointer            user_data)
{
	GtkPlaylist *playlist = GTK_PLAYLIST (user_data);
	GList *list, *p, *file_list;

	list = gnome_vfs_uri_list_parse (data->data);

	if (list == NULL) {
		gtk_drag_finish (context, FALSE, FALSE, time);
		return;
	}

	p = list;
	file_list = NULL;

	while (p != NULL)
	{
		file_list = g_list_prepend (file_list,
				gnome_vfs_uri_to_string
				((const GnomeVFSURI*)(p->data), 0));
		p = p->next;
	}

	gnome_vfs_uri_list_free (list);
	file_list = g_list_reverse (file_list);

	if (file_list == NULL)
	{
		gtk_drag_finish (context, FALSE, FALSE, time);
		return;
	}

	for (p = file_list; p != NULL; p = p->next)
	{
		char *filename;

		if (p->data == NULL)
			continue;

		filename = gnome_vfs_get_local_path_from_uri (p->data);
		if (filename == NULL)
			filename = g_strdup (p->data);

		if (filename != NULL &&
				(g_file_test (filename, G_FILE_TEST_IS_REGULAR
					| G_FILE_TEST_EXISTS)
				 || strstr (filename, "://") != NULL))
		{
			g_message ("drop_cb: adding %s", filename);
			gtk_playlist_add_mrl (playlist, filename, NULL);
		}
		g_free (filename);
		g_free (p->data);
	}

	g_list_free (file_list);
	gtk_drag_finish (context, TRUE, FALSE, time);

	g_signal_emit (G_OBJECT (playlist),
			gtk_playlist_table_signals[CHANGED], 0,
			NULL);
}

static void
selection_changed (GtkTreeSelection *treeselection, gpointer user_data)
{
	GtkPlaylist *playlist = GTK_PLAYLIST (user_data);
	GtkWidget *remove_button;

	remove_button = glade_xml_get_widget (playlist->_priv->xml,
			"remove_button");

	if (gtk_tree_selection_has_selected (treeselection))
		gtk_widget_set_sensitive (remove_button, TRUE);
	else
		gtk_widget_set_sensitive (remove_button, FALSE);
}

/* This function checks if the current item is NULL, and try to update it as the
 * first item of the playlist if so. It returns TRUE if there is a current
 * item */
static gboolean
update_current_from_playlist (GtkPlaylist *playlist)
{
	if (playlist->_priv->current == NULL)
	{
		if (gtk_tree_model_iter_n_children (playlist->_priv->model,
					NULL) != 0)
		{
			playlist->_priv->current =
				gtk_tree_path_new_from_string ("0");
		} else {
			return FALSE;
		}
	}
	return TRUE;
}

static void
gtk_playlist_add_files (GtkWidget *widget, gpointer user_data)
{
	GtkPlaylist *playlist = (GtkPlaylist *)user_data;
	GtkWidget *fs;
	int response;

	fs = gtk_file_selection_new (_("Select files"));
	gtk_file_selection_set_select_multiple (GTK_FILE_SELECTION (fs), TRUE);
	if (playlist->_priv->path != NULL)
	{
		gtk_file_selection_set_filename (GTK_FILE_SELECTION (fs),
				playlist->_priv->path);
		g_free (playlist->_priv->path);
		playlist->_priv->path = NULL;
	}
	response = gtk_dialog_run (GTK_DIALOG (fs));
	gtk_widget_hide (fs);
	while (gtk_events_pending())
		gtk_main_iteration();
	
	if (response == GTK_RESPONSE_OK)
	{
		char **filenames;
		int i;

		filenames = gtk_file_selection_get_selections
			(GTK_FILE_SELECTION (fs));

		if (filenames[0] != NULL)
		{
			char *tmp;

			tmp = g_path_get_dirname (filenames[0]);
			playlist->_priv->path = g_strconcat (tmp,
					G_DIR_SEPARATOR_S, NULL);
			g_free (tmp);
		}

		for (i = 0; filenames[i] != NULL; i++)
			gtk_playlist_add_mrl (playlist, filenames[i], NULL);

		g_strfreev (filenames);
	}

	gtk_widget_destroy (fs);
}

static void
gtk_playlist_foreach_selected (GtkTreeModel *model, GtkTreePath *path,
		GtkTreeIter *iter, gpointer data)
{
	GtkPlaylist *playlist = (GtkPlaylist *)data;
	GtkTreeRowReference *ref;

	/* We can't use gtk_list_store_remove() here
	 * So we build a list a RowReferences */
	ref = gtk_tree_row_reference_new (playlist->_priv->model, path);
	playlist->_priv->list = g_list_prepend
		(playlist->_priv->list, (gpointer) ref);
}

static void
gtk_playlist_remove_files (GtkWidget *widget, gpointer user_data)
{
	GtkPlaylist *playlist = (GtkPlaylist *)user_data;
	GtkTreeSelection *selection;
	GtkTreeRowReference *ref = NULL;
	gboolean is_selected = FALSE;

	selection = gtk_tree_view_get_selection
		(GTK_TREE_VIEW (playlist->_priv->treeview));
	if (selection == NULL)
		return;

	gtk_tree_selection_selected_foreach (selection,
			gtk_playlist_foreach_selected,
			(gpointer) playlist);

	/* If the current item is to change, we need to keep an static
	 * reference to it, TreeIter and TreePath don't allow that */
	if (playlist->_priv->current != NULL)
	{
		ref = gtk_tree_row_reference_new (playlist->_priv->model,
				playlist->_priv->current);
		is_selected = gtk_tree_selection_path_is_selected (selection,
				playlist->_priv->current);
		gtk_tree_path_free (playlist->_priv->current);
	}

	/* We destroy the items, one-by-one from the list built above */
	while (playlist->_priv->list != NULL)
	{
		GtkTreePath *path;
		GtkTreeIter iter;

		path = gtk_tree_row_reference_get_path
			((GtkTreeRowReference *)(playlist->_priv->list->data));
		gtk_tree_model_get_iter (playlist->_priv->model, &iter, path);
		gtk_tree_path_free (path);
		gtk_list_store_remove (GTK_LIST_STORE (playlist->_priv->model),
				&iter);

		gtk_tree_row_reference_free
			((GtkTreeRowReference *)(playlist->_priv->list->data));
		playlist->_priv->list = g_list_remove (playlist->_priv->list,
				playlist->_priv->list->data);
	}
	playlist->_priv->list = NULL;

	if (is_selected == TRUE)
	{
		/* The current item was removed from the playlist */
		playlist->_priv->current = NULL;
		g_signal_emit (G_OBJECT (playlist),
				gtk_playlist_table_signals[CURRENT_REMOVED], 0,
				NULL);
	} else {
		if (ref != NULL)
		{
			/* The path to the current item changed */
			playlist->_priv->current =
				gtk_tree_row_reference_get_path (ref);
			gtk_tree_row_reference_free (ref);
		}
		g_signal_emit (G_OBJECT (playlist),
				gtk_playlist_table_signals[CHANGED], 0,
				NULL);
	}
}

static void
init_columns (GtkTreeView *treeview)
{
	GtkCellRenderer *renderer;
	GtkTreeViewColumn *column;

	/* Playing pix */
	renderer = gtk_cell_renderer_pixbuf_new ();
	column = gtk_tree_view_column_new ();
	gtk_tree_view_column_pack_start (column, renderer, FALSE);
	gtk_tree_view_column_set_attributes (column, renderer,
			"pixbuf", PIX_COL, NULL);
	gtk_tree_view_column_set_title (column, _("Filename"));
	gtk_tree_view_append_column (treeview, column);

	/* Labels */
	renderer = gtk_cell_renderer_text_new ();
	gtk_tree_view_column_pack_start (column, renderer, TRUE);
	gtk_tree_view_column_set_attributes (column, renderer,
			"text", FILENAME_COL, NULL);
}

static void
treeview_row_changed (GtkTreeView *treeview, GtkTreePath *arg1,
		GtkTreeViewColumn *arg2, gpointer user_data)
{
	GtkPlaylist *playlist = (GtkPlaylist *)user_data;

	if (gtk_tree_path_equals (arg1, playlist->_priv->current) == TRUE)
		return;

	if (playlist->_priv->current != NULL)
	{
		gtk_playlist_unset_playing (playlist);
		gtk_tree_path_free (playlist->_priv->current);
	}

	playlist->_priv->current = gtk_tree_path_copy (arg1);
	g_signal_emit (G_OBJECT (playlist),
			gtk_playlist_table_signals[CHANGED], 0,
			NULL);
}

static void
init_treeview (GtkWidget *treeview, GtkPlaylist *playlist)
{
	GtkTreeModel *model;
	GtkTreeSelection *selection;

	/* the model */
	model = GTK_TREE_MODEL (gtk_list_store_new (NUM_COLS,
				GDK_TYPE_PIXBUF,
				G_TYPE_STRING,
				G_TYPE_STRING));

	/* the treeview */
	gtk_tree_view_set_model (GTK_TREE_VIEW (treeview), model);
	gtk_tree_view_set_rules_hint (GTK_TREE_VIEW (treeview), TRUE);
	g_object_unref (G_OBJECT (model));

	init_columns (GTK_TREE_VIEW (treeview));

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (treeview));
	gtk_tree_selection_set_mode (selection, GTK_SELECTION_MULTIPLE);
	g_signal_connect (G_OBJECT (selection), "changed",
			G_CALLBACK (selection_changed), playlist);
	g_signal_connect (G_OBJECT (treeview), "row-activated",
			G_CALLBACK (treeview_row_changed), playlist);

	/* Drag'n'Drop */
	g_signal_connect (G_OBJECT (treeview), "drag_data_received",
			G_CALLBACK (drop_cb), playlist);
	gtk_drag_dest_set (treeview, GTK_DEST_DEFAULT_ALL,
			target_table, 1, GDK_ACTION_COPY);

	gtk_widget_show (treeview);
}

static void
repeat_button_toggled (GtkToggleButton *togglebutton, gpointer user_data)
{
	GtkPlaylist *playlist = (GtkPlaylist *)user_data;
	gboolean repeat;

	repeat = gtk_toggle_button_get_active (togglebutton);
	gconf_client_set_bool (playlist->_priv->gc, "/apps/totem/repeat",
			repeat, NULL);
	playlist->_priv->repeat = repeat;
}

static void
update_repeat_cb (GConfClient *client, guint cnxn_id,
		GConfEntry *entry, gpointer user_data)
{
	GtkPlaylist *playlist = (GtkPlaylist *)user_data;
	GtkWidget *button;
	gboolean repeat;

	repeat = gconf_client_get_bool (client,
			"/apps/totem/repeat", NULL);
	button = glade_xml_get_widget (playlist->_priv->xml, "repeat_button");
	g_signal_handlers_disconnect_by_func (G_OBJECT (button),
			repeat_button_toggled, playlist);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), repeat);
	g_signal_connect (G_OBJECT (button), "toggled",
			G_CALLBACK (repeat_button_toggled),
			(gpointer) playlist);

	g_signal_emit (G_OBJECT (playlist),
			gtk_playlist_table_signals[CHANGED], 0,
			NULL);
}

static void
init_config (GtkPlaylist *playlist)
{
	GtkWidget *button;
	gboolean repeat;

	button = glade_xml_get_widget (playlist->_priv->xml, "repeat_button");
	playlist->_priv->gc = gconf_client_get_default ();

	repeat = gconf_client_get_bool (playlist->_priv->gc,
			"/apps/totem/repeat", NULL);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), repeat);

	gconf_client_add_dir (playlist->_priv->gc, "/apps/totem",
			GCONF_CLIENT_PRELOAD_ONELEVEL, NULL);
	gconf_client_notify_add (playlist->_priv->gc, "/apps/totem/repeat",
			update_repeat_cb, playlist, NULL, NULL);
	g_signal_connect (G_OBJECT (button), "toggled",
			G_CALLBACK (repeat_button_toggled),
			(gpointer) playlist);

	playlist->_priv->repeat = repeat;
}

static void
gtk_playlist_init (GtkPlaylist *playlist)
{
	playlist->_priv = g_new0 (GtkPlaylistPrivate, 1);
	playlist->_priv->current = NULL;
	playlist->_priv->icon = NULL;
	playlist->_priv->path = NULL;
	playlist->_priv->repeat = FALSE;
	playlist->_priv->gc = NULL;
}

static void
gtk_playlist_finalize (GObject *object)
{
	GtkPlaylist *playlist = GTK_PLAYLIST (object);

	g_return_if_fail (object != NULL);

	if (playlist->_priv->current != NULL)
		gtk_tree_path_free (playlist->_priv->current);
	if (playlist->_priv->icon != NULL)
		gdk_pixbuf_unref (playlist->_priv->icon);

	if (G_OBJECT_CLASS (parent_class)->finalize != NULL) {
		(* G_OBJECT_CLASS (parent_class)->finalize) (object);
	}
}

static void
gtk_playlist_unrealize (GtkWidget *widget)
{
	GtkPlaylist *playlist = GTK_PLAYLIST (widget);
	int x, y;

	g_return_if_fail (widget != NULL);

	gtk_window_get_position (GTK_WINDOW (widget), &x, &y);
	gconf_client_set_int (playlist->_priv->gc, "/apps/totem/playlist_x",
			x, NULL);
	gconf_client_set_int (playlist->_priv->gc, "/apps/totem/playlist_y",
			y, NULL);

	if (GTK_WIDGET_CLASS (parent_class)->unrealize != NULL) {
		(* GTK_WIDGET_CLASS (parent_class)->unrealize) (widget);
	}
}

static void
gtk_playlist_realize (GtkWidget *widget)
{
	GtkPlaylist *playlist = GTK_PLAYLIST (widget);
	int x, y;

	g_return_if_fail (widget != NULL);

	if (GTK_WIDGET_CLASS (parent_class)->realize != NULL) {
		(* GTK_WIDGET_CLASS (parent_class)->realize) (widget);
	}

	x = gconf_client_get_int (playlist->_priv->gc,
			"/apps/totem/playlist_x", NULL);
	y = gconf_client_get_int (playlist->_priv->gc,
			"/apps/totem/playlist_y", NULL);

	if (x == -1 || y == -1
			|| x > gdk_screen_width () || y > gdk_screen_height ())
		return;

	gtk_window_move (GTK_WINDOW (widget), x, y);
}

GtkWidget*
gtk_playlist_new (void)
{
	GtkPlaylist *playlist;
	GtkWidget *container, *item;
	char *filename;

	playlist = GTK_PLAYLIST (g_object_new (GTK_TYPE_PLAYLIST, NULL));

	filename = gnome_program_locate_file (NULL,
			GNOME_FILE_DOMAIN_APP_DATADIR,
			"totem/playlist.glade", TRUE, NULL);
	if (filename == NULL)
	{
		gtk_playlist_finalize (G_OBJECT (playlist));
		return NULL;
	}
	playlist->_priv->xml = glade_xml_new (filename, "vbox4", NULL);
	if (playlist->_priv->xml == NULL)
	{
		gtk_playlist_finalize (G_OBJECT (playlist));
		return NULL;
	}
	g_free (filename);

	gtk_window_set_title (GTK_WINDOW (playlist), _("Playlist"));
	gtk_dialog_add_buttons (GTK_DIALOG (playlist),
			GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE,
			NULL);
	gtk_window_set_default_size (GTK_WINDOW (playlist),
			300, 375);
	g_signal_connect_object (GTK_OBJECT (playlist), "response",
			GTK_SIGNAL_FUNC (gtk_widget_hide), 
			GTK_WIDGET (playlist),
			0);

	/* Connect the buttons */
	item = glade_xml_get_widget (playlist->_priv->xml, "add_button");
	g_signal_connect (GTK_OBJECT (item), "clicked",
			GTK_SIGNAL_FUNC (gtk_playlist_add_files),
			(gpointer) playlist);
	item = glade_xml_get_widget (playlist->_priv->xml, "remove_button");
	g_signal_connect (GTK_OBJECT (item), "clicked",
			GTK_SIGNAL_FUNC (gtk_playlist_remove_files),
			(gpointer) playlist);

	container = glade_xml_get_widget (playlist->_priv->xml, "vbox4");
	gtk_box_pack_start (GTK_BOX (GTK_DIALOG (playlist)->vbox),
			container,
			TRUE,       /* expand */
			TRUE,       /* fill */
			0);         /* padding */

	playlist->_priv->treeview = glade_xml_get_widget
		(playlist->_priv->xml, "treeview1");
	init_treeview (playlist->_priv->treeview, playlist);
	playlist->_priv->model = gtk_tree_view_get_model
		(GTK_TREE_VIEW (playlist->_priv->treeview));

	/* The configuration */
	init_config (playlist);

	filename = gnome_program_locate_file (NULL,
			GNOME_FILE_DOMAIN_APP_DATADIR,
			"totem/playlist-playing.png", TRUE, NULL);
	if (filename != NULL)
	{
		playlist->_priv->icon = gdk_pixbuf_new_from_file
			(filename, NULL);
		g_free (filename);
	} else {
		playlist->_priv->icon = NULL;
	}

	gtk_widget_show_all (GTK_DIALOG (playlist)->vbox);

	return GTK_WIDGET (playlist);
}

static gboolean
gtk_playlist_add_one_mrl (GtkPlaylist *playlist, const char *mrl,
		const char *display_name)
{
	GtkListStore *store;
	GtkTreeIter iter;
	char *filename_for_display;

	g_return_val_if_fail (GTK_IS_PLAYLIST (playlist), FALSE);
	g_return_val_if_fail (mrl != NULL, FALSE);

	if (display_name == NULL)
	{
		char *filename, *unescaped;

		filename = g_path_get_basename (mrl);
		unescaped = gnome_vfs_unescape_string_for_display (filename);
		g_free (filename);
		filename_for_display = g_filename_to_utf8 (unescaped,
				-1,		/* length */
				NULL,		/* bytes_read */
				NULL,		/* bytes_written */
				NULL);		/* error */
		g_free (unescaped);
	} else {
		filename_for_display = g_strdup (display_name);
	}

	store = GTK_LIST_STORE (playlist->_priv->model);
	gtk_list_store_append (store, &iter);
	gtk_list_store_set (store, &iter,
			PIX_COL, NULL,
			FILENAME_COL, filename_for_display,
			URI_COL, mrl,
			-1);

	g_free (filename_for_display);

	if (playlist->_priv->current == NULL)
		playlist->_priv->current = gtk_tree_model_get_path
			(playlist->_priv->model, &iter);

	g_signal_emit (G_OBJECT (playlist),
			gtk_playlist_table_signals[CHANGED], 0,
			NULL);

	return TRUE;
}

static gboolean
gtk_playlist_add_m3u (GtkPlaylist *playlist, const char *mrl)
{
	gboolean retval = FALSE;
	char *contents, **lines;
	int size, i;

	if (eel_read_entire_file (mrl, &size, &contents) != GNOME_VFS_OK)
		return FALSE;

	lines = g_strsplit (contents, "\n", 0);
	g_free (contents);

	for (i = 0; lines[i] != NULL; i++)
	{
		/* Either it's a URI, or it has a proper path */
		if (strstr(lines[i], "://") != NULL
				|| lines[i][0] == G_DIR_SEPARATOR)
		{
			if (gtk_playlist_add_one_mrl (playlist,
						lines[i], NULL) == TRUE)
				retval = TRUE;
		}
	}

	g_strfreev (lines);

	return retval;
}

static gboolean
gtk_playlist_add_pls (GtkPlaylist *playlist, const char *mrl)
{
	gboolean retval = FALSE;
	char *contents, **lines;
	int size, i, num_entries;

	if (eel_read_entire_file (mrl, &size, &contents) != GNOME_VFS_OK)
		return FALSE;

	lines = g_strsplit (contents, "\n", 0);
	g_free (contents);

	/* [playlist] */
	if (g_ascii_strncasecmp (lines[0], "[playlist]",
				(gsize)strlen ("[playlist]")) != 0)
		goto bail;

	/* numberofentries=? */
	num_entries = read_ini_line_int (lines, "numberofentries");
	if (num_entries == -1)
		goto bail;

	for (i = 1; i <= num_entries; i++)
	{
		char *file, *title;
		char *file_key, *title_key;

		file_key = g_strdup_printf ("file%d", i);
		title_key = g_strdup_printf ("title%d", i);

		file = read_ini_line_string (lines, (const char*)file_key);
		title = read_ini_line_string (lines, (const char*)title_key);

		g_free (file_key);
		g_free (title_key);

		if (file != NULL)
		{
			if (gtk_playlist_add_one_mrl (playlist,
						file, title) == TRUE)
				retval = TRUE;
			g_free (file);
			g_free (title);
		} else {
			g_free (title);
		}
	}

bail:
	g_strfreev (lines);

	return retval;
}

static gboolean
parse_entry (GtkPlaylist *playlist, char *base, xmlDocPtr doc,
		xmlNodePtr parent)
{
	xmlNodePtr node;
	char *title, *url;
	gboolean retval = FALSE;

	title = NULL;
	url = NULL;

	for (node = parent->children; node != NULL; node = node->next)
	{
		if (node->name == NULL)
			continue;

		/* ENTRY should only have one ref and one title nodes */
		if (g_ascii_strcasecmp (node->name, "ref") == 0)
		{
			url = xmlGetProp (node, "href");
			continue;
		}

		if (g_ascii_strcasecmp (node->name, "title") == 0)
			title = xmlNodeListGetString(doc, node->children, 1);
	}

	if (url == NULL)
	{
		g_free (title);
		return FALSE;
	}

	if (strstr (url, "://") != NULL || url[0] == '/')
		retval = gtk_playlist_add_one_mrl (playlist, url, title);
	else {
		char *fullpath;

		fullpath = g_strdup_printf ("%s/%s", base, url);
		/* .asx files can contain references to other .asx files */
		retval = gtk_playlist_add_mrl (playlist, fullpath, title);

		g_free (fullpath);
	}

	g_free (title);
	g_free (url);

	return retval;
}

static gboolean
parse_entries (GtkPlaylist *playlist, char *base, xmlDocPtr doc,
		xmlNodePtr parent)
{
	xmlNodePtr node;
	gboolean retval = FALSE;

	for (node = parent->children; node != NULL; node = node->next)
	{
		if (node->name == NULL)
			continue;

		if (g_ascii_strcasecmp (node->name, "entry") == 0)
		{
			/* Whee found an entry here, find the REF and TITLE */
			if (parse_entry (playlist, base, doc, node) == TRUE)
				retval = TRUE;
		}
	}

	return retval;
}

static gboolean
gtk_playlist_add_asx (GtkPlaylist *playlist, const char *mrl)
{
	xmlDocPtr doc;
	xmlNodePtr node;
	char *contents = NULL, *base;
	int size;
	gboolean retval = FALSE;

	if (eel_read_entire_file (mrl, &size, &contents) != GNOME_VFS_OK)
		return FALSE;

	doc = xmlParseMemory(contents, size);
	g_free (contents);

	/* If the document has no root, or no name */
	if(!doc->children || !doc->children->name)
	{
		xmlFreeDoc(doc);
		return FALSE;
	}

	/* Yay, let's reconstruct the base by hand */
	{
		GnomeVFSURI *uri, *parent;
		uri = gnome_vfs_uri_new (mrl);
		parent = gnome_vfs_uri_get_parent (uri);
		base = gnome_vfs_uri_to_string (parent, 0);

		gnome_vfs_uri_unref (uri);
		gnome_vfs_uri_unref (parent);
	}

	for(node = doc->children; node != NULL; node = node->next)
	{
		if (parse_entries (playlist, base, doc, node) == TRUE)
			retval = TRUE;
	}

	g_free (base);
	xmlFreeDoc(doc);
	return retval;
}

gboolean
gtk_playlist_add_mrl (GtkPlaylist *playlist, const char *mrl,
		const char *display_name)
{
	const char *mimetype;

	g_return_val_if_fail (mrl != NULL, FALSE);

	mimetype = gnome_vfs_get_mime_type (mrl);

	g_message ("trying to add %s (%s)", mrl, mimetype);

	if (mimetype == NULL)
	{
		return gtk_playlist_add_one_mrl (playlist, mrl, display_name);
	} else if (strcmp ("audio/x-mpegurl", mimetype) == 0) {
		return gtk_playlist_add_m3u (playlist, mrl);
	} else if (strcmp ("audio/x-scpls", mimetype) == 0) {
		return gtk_playlist_add_pls (playlist, mrl);
	} else if (strcmp ("audio/x-ms-asx", mimetype) == 0) {
		return gtk_playlist_add_asx (playlist, mrl);
	} else if (strcmp ("x-directory/normal", mimetype) == 0) {
		//Load all the files in the dir ?
	}

	return gtk_playlist_add_one_mrl (playlist, mrl, display_name);
}

void
gtk_playlist_clear (GtkPlaylist *playlist)
{
	GtkListStore *store;

	g_return_if_fail (GTK_IS_PLAYLIST (playlist));

	store = GTK_LIST_STORE (playlist->_priv->model);
	gtk_list_store_clear (store);

	if (playlist->_priv->current != NULL)
		gtk_tree_path_free (playlist->_priv->current);
	playlist->_priv->current = NULL;
}

char
*gtk_playlist_get_current_mrl (GtkPlaylist *playlist)
{
	GtkTreeIter iter;
	char *path;

	g_return_val_if_fail (GTK_IS_PLAYLIST (playlist), NULL);

	if (update_current_from_playlist (playlist) == FALSE)
		return NULL;

	gtk_tree_model_get_iter (playlist->_priv->model,
			&iter,
			playlist->_priv->current);
	gtk_tree_model_get (playlist->_priv->model,
			&iter,
			URI_COL, &path,
			-1);

	return path;
}

gboolean
gtk_playlist_has_previous_mrl (GtkPlaylist *playlist)
{
	GtkTreeIter iter;

	g_return_val_if_fail (GTK_IS_PLAYLIST (playlist), FALSE);

	if (update_current_from_playlist (playlist) == FALSE)
		return FALSE;

	gtk_tree_model_get_iter (playlist->_priv->model,
			&iter,
			playlist->_priv->current);

	return gtk_tree_model_iter_previous (playlist->_priv->model, &iter);
}

gboolean
gtk_playlist_has_next_mrl (GtkPlaylist *playlist)
{
	GtkTreeIter iter;

	g_return_val_if_fail (GTK_IS_PLAYLIST (playlist), FALSE);

	if (update_current_from_playlist (playlist) == FALSE)
		return FALSE;
	if (playlist->_priv->repeat == TRUE)
		return TRUE;

	gtk_tree_model_get_iter (playlist->_priv->model,
			&iter,
			playlist->_priv->current);

	return gtk_tree_model_iter_next (playlist->_priv->model, &iter);
}

gboolean
gtk_playlist_set_playing (GtkPlaylist *playlist, gboolean state)
{
	GtkListStore *store;
	GtkTreeIter iter;

	g_return_val_if_fail (GTK_IS_PLAYLIST (playlist), FALSE);

	if (update_current_from_playlist (playlist) == FALSE)
		return FALSE;

	store = GTK_LIST_STORE (playlist->_priv->model);
	gtk_tree_model_get_iter (playlist->_priv->model,
			&iter,
			playlist->_priv->current);

	if (&iter == NULL)
		return FALSE;

	if (state == TRUE)
		gtk_list_store_set (store, &iter,
				PIX_COL, playlist->_priv->icon,
				-1);
	else
		gtk_list_store_set (store, &iter,
				PIX_COL, NULL,
				-1);
	return TRUE;
}

static gboolean
gtk_playlist_unset_playing (GtkPlaylist *playlist)
{
	GtkListStore *store;
	GtkTreeIter iter;

	if (update_current_from_playlist (playlist) == FALSE)
		return FALSE;

	store = GTK_LIST_STORE (playlist->_priv->model);
	gtk_tree_model_get_iter (playlist->_priv->model,
			&iter,
			playlist->_priv->current);

	if (&iter == NULL)
		return FALSE;

	gtk_list_store_set (store, &iter,
			PIX_COL, NULL,
			-1);
	return TRUE;
}

void
gtk_playlist_set_previous (GtkPlaylist *playlist)
{
	GtkTreeIter iter;

	g_return_if_fail (GTK_IS_PLAYLIST (playlist));
	
	if (gtk_playlist_has_previous_mrl (playlist) == FALSE)
		return;

	gtk_playlist_unset_playing (playlist);

	gtk_tree_model_get_iter (playlist->_priv->model,
			&iter,
			playlist->_priv->current);

	gtk_tree_model_iter_previous (playlist->_priv->model, &iter);
	gtk_tree_path_free (playlist->_priv->current);
	playlist->_priv->current = gtk_tree_model_get_path
		(playlist->_priv->model, &iter);
}

void
gtk_playlist_set_next (GtkPlaylist *playlist)
{
	GtkTreeIter iter;

	g_return_if_fail (GTK_IS_PLAYLIST (playlist));

	if (gtk_playlist_has_next_mrl (playlist) == FALSE)
	{
		if (playlist->_priv->repeat == TRUE)
			gtk_playlist_set_at_start (playlist);

		return;
	}

	gtk_playlist_unset_playing (playlist);

	gtk_tree_model_get_iter (playlist->_priv->model,
			&iter,
			playlist->_priv->current);

	gtk_tree_model_iter_next (playlist->_priv->model, &iter);
	gtk_tree_path_free (playlist->_priv->current);
	playlist->_priv->current = gtk_tree_model_get_path
		(playlist->_priv->model, &iter);
}

void
gtk_playlist_set_at_start (GtkPlaylist *playlist)
{
	g_return_if_fail (GTK_IS_PLAYLIST (playlist));

	gtk_playlist_unset_playing (playlist);

	if (playlist->_priv->current != NULL)
	{
		gtk_tree_path_free (playlist->_priv->current);
		playlist->_priv->current = NULL;
	}
	update_current_from_playlist (playlist);
}

/* This one returns a new string, in UTF8 even if the mrl is encoded
 * in the locale's encoding
 */
gchar *
gtk_playlist_mrl_to_title (const gchar *mrl)
{
	char *filename_for_display, *filename, *unescaped;

	filename = g_path_get_basename (mrl);
	unescaped = gnome_vfs_unescape_string_for_display (filename);
	g_free (filename);
	filename_for_display = g_filename_to_utf8 (unescaped,
			-1,             /* length */
			NULL,           /* bytes_read */
			NULL,           /* bytes_written */
			NULL);          /* error */

	g_free (unescaped);

	return filename_for_display;
}

static void
gtk_playlist_class_init (GtkPlaylistClass *klass)
{
	parent_class = gtk_type_class (gtk_dialog_get_type ());

	G_OBJECT_CLASS (klass)->finalize = gtk_playlist_finalize;
	GTK_WIDGET_CLASS (klass)->realize = gtk_playlist_realize;
	GTK_WIDGET_CLASS (klass)->unrealize = gtk_playlist_unrealize;

	/* Signals */
	gtk_playlist_table_signals[CHANGED] =
		g_signal_new ("changed",
				G_TYPE_FROM_CLASS (klass),
				G_SIGNAL_RUN_LAST,
				G_STRUCT_OFFSET (GtkPlaylistClass, changed),
				NULL, NULL,
				g_cclosure_marshal_VOID__VOID,
				G_TYPE_NONE, 0);
	gtk_playlist_table_signals[CURRENT_REMOVED] =
		g_signal_new ("current-removed",
				G_TYPE_FROM_CLASS (klass),
				G_SIGNAL_RUN_LAST,
				G_STRUCT_OFFSET (GtkPlaylistClass,
					current_removed),
				NULL, NULL,
				g_cclosure_marshal_VOID__VOID,
				G_TYPE_NONE, 0);
}

