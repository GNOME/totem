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
#include <eel/eel-gtk-macros.h>
#include <glade/glade.h>

#include "debug.h"

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

static void gtk_playlist_class_init (GtkPlaylistClass *class);
static void gtk_playlist_init       (GtkPlaylist      *label);

static void init_treeview (GtkWidget *treeview);

EEL_CLASS_BOILERPLATE (GtkPlaylist, gtk_playlist, gtk_dialog_get_type ());

/* Helper functions */
static gboolean
gtk_tree_model_iter_previous (GtkTreeModel *tree_model, GtkTreeIter *iter)
{
	GtkTreePath *path;
	gboolean ret;

	D("gtk_tree_model_iter_previous");

	path = gtk_tree_model_get_path (tree_model, iter);
	ret = gtk_tree_path_prev (path);
	if (ret == TRUE)
		gtk_tree_model_get_iter (tree_model, iter, path);

	gtk_tree_path_free (path);
	return ret;
}

/* This function checks if the current item is NULL, and try to update it as the
 * first item of the playlist if so. It returns TRUE if there is a current
 * item */
static gboolean
update_current_from_playlist (GtkPlaylist *playlist)
{
	D("update_current_from_playlist");

	if (playlist->_priv->current == NULL)
	{
		D("update_current_from_playlist: current is NULL");
		if (gtk_tree_model_iter_n_children (playlist->_priv->model,
					NULL) != 0)
		{
			D("The playlist isn't empty");
			playlist->_priv->current =
				gtk_tree_path_new_from_string ("0");
		} else {
			D("The playlist is empty");
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

	D("gtk_playlist_add_files");

	fs = gtk_file_selection_new (_("Select files"));
	gtk_file_selection_set_select_multiple (GTK_FILE_SELECTION (fs), TRUE);
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

		for (i = 0; filenames[i] != NULL; i++)
			gtk_playlist_add_mrl (playlist, filenames[i]);

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

	D("gtk_playlist_foreach_selected");
#ifdef TOTEM_DEBUG
	{
		char *file;

		gtk_tree_model_get (model, iter, URI_COL, &file, -1);
		g_message ("removing path: %s", file);
		g_free (file);
	}
#endif
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

	D("gtk_playlist_remove_files");

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
	GtkWidget *header;
	char *filename;

	D("init_columns");

	/* Playing pix */
	renderer = gtk_cell_renderer_pixbuf_new ();
	filename = gnome_program_locate_file (NULL,
			GNOME_FILE_DOMAIN_APP_DATADIR,
			"playlist-16.png", TRUE, NULL);
	header = gtk_image_new_from_file (filename);
	g_free (filename);
	gtk_widget_show (header);
	column = gtk_tree_view_column_new_with_attributes (_("Playing"),
			renderer,
			"pixbuf", PIX_COL,
			NULL);
	gtk_tree_view_column_set_fixed_width (column, 20);
	gtk_tree_view_column_set_widget (column, header);
	gtk_tree_view_append_column (treeview, column);

	/* Display Name */
	renderer = gtk_cell_renderer_text_new ();

	column = gtk_tree_view_column_new_with_attributes (_("Filename"),
			renderer,
			"text", FILENAME_COL,
			NULL);
	gtk_tree_view_append_column (treeview, column);
}

static void
init_treeview (GtkWidget *treeview)
{
	GtkTreeModel *model;

	D("init_treeview");

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
	gtk_tree_selection_set_mode (gtk_tree_view_get_selection
			GTK_TREE_VIEW ((treeview)),
			GTK_SELECTION_MULTIPLE);
	gtk_widget_show (treeview);
}

static void
gtk_playlist_init (GtkPlaylist *playlist)
{
	D("gtk_playlist_init");

	playlist->_priv = g_new0 (GtkPlaylistPrivate, 1);
	playlist->_priv->current = NULL;
	playlist->_priv->icon = NULL;
}


static void
gtk_playlist_finalize (GObject *object)
{
	GtkPlaylist *playlist;

	D("gtk_playlist_finalize");

	playlist = GTK_PLAYLIST (object);
	if (playlist->_priv->current != NULL)
		gtk_tree_path_free (playlist->_priv->current);
	if (playlist->_priv->icon != NULL)
		gdk_pixbuf_unref (playlist->_priv->icon);

	EEL_CALL_PARENT (G_OBJECT_CLASS, finalize, (object));
}

GtkWidget*
gtk_playlist_new (GtkWindow *parent)
{
	GtkPlaylist *playlist;
	GtkWidget *container, *item;
	char *filename;

	D("gtk_playlist_new");

	playlist = GTK_PLAYLIST (g_object_new (GTK_TYPE_PLAYLIST, NULL));

	filename = gnome_program_locate_file (NULL,
			GNOME_FILE_DOMAIN_APP_DATADIR,
			"playlist.glade", TRUE, NULL);
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
	init_treeview (playlist->_priv->treeview);
	playlist->_priv->model = gtk_tree_view_get_model
		(GTK_TREE_VIEW (playlist->_priv->treeview));

	if (parent != NULL)
		gtk_window_set_transient_for (GTK_WINDOW (playlist), parent);

	filename = gnome_program_locate_file (NULL,
			GNOME_FILE_DOMAIN_APP_DATADIR,
			"playlist-playing.png", TRUE, NULL);
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

gboolean
gtk_playlist_add_mrl (GtkPlaylist *playlist, const char *mrl)
{
	GtkListStore *store;
	GtkTreeIter iter;
	char *filename_utf8, *filename;

	D("gtk_playlist_add_mrl");

	g_return_val_if_fail (GTK_IS_PLAYLIST (playlist), FALSE);
	g_return_val_if_fail (mrl != NULL, FALSE);

	filename = g_path_get_basename (mrl);
	filename_utf8 = g_filename_to_utf8 (filename,
			-1,		/* length */
			NULL,		/* bytes_read */
			NULL,		/* bytes_written */
			NULL);		/* error */

	store = GTK_LIST_STORE (playlist->_priv->model);
	gtk_list_store_append (store, &iter);
	gtk_list_store_set (store, &iter,
			PIX_COL, NULL,
			FILENAME_COL, filename_utf8,
			URI_COL, mrl,
			-1);

	g_free (filename_utf8);

	if (playlist->_priv->current == NULL)
		playlist->_priv->current = gtk_tree_model_get_path
			(playlist->_priv->model, &iter);

	g_signal_emit (G_OBJECT (playlist),
			gtk_playlist_table_signals[CHANGED], 0,
			NULL);
}

void
gtk_playlist_clear (GtkPlaylist *playlist)
{
	GtkListStore *store;

	D("gtk_playlist_clear");

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

	D("gtk_playlist_get_current_mrl");

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

	D("gtk_playlist_get_current_mrl: returns %s", path);

	return path;
}

gboolean
gtk_playlist_has_previous_mrl (GtkPlaylist *playlist)
{
	GtkTreeIter iter;

	D("gtk_playlist_has_previous_mrl");

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

	D("gtk_playlist_has_next_mrl");

	g_return_val_if_fail (GTK_IS_PLAYLIST (playlist), FALSE);

	if (update_current_from_playlist (playlist) == FALSE)
		return FALSE;

	gtk_tree_model_get_iter (playlist->_priv->model,
			&iter,
			playlist->_priv->current);

	return gtk_tree_model_iter_next (playlist->_priv->model, &iter);
}

gboolean
gtk_playlist_set_playing (GtkPlaylist *playlist)
{
	GtkListStore *store;
	GtkTreeIter iter;

	D("gtk_playlist_set_playing");

	g_return_val_if_fail (GTK_IS_PLAYLIST (playlist), FALSE);

	if (update_current_from_playlist (playlist) == FALSE)
		return FALSE;

	store = GTK_LIST_STORE (playlist->_priv->model);
	gtk_tree_model_get_iter (playlist->_priv->model,
			&iter,
			playlist->_priv->current);

	if (&iter == NULL)
		return FALSE;

	gtk_list_store_set (store, &iter,
			PIX_COL, playlist->_priv->icon,
			-1);
	return TRUE;
}

static gboolean
gtk_playlist_unset_playing (GtkPlaylist *playlist)
{
	GtkListStore *store;
	GtkTreeIter iter;

	D("gtk_playlist_unset_playing");

	if (update_current_from_playlist (playlist) == FALSE)
		return FALSE;

	/* No type-checking, it's supposed to be safe here */

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

	D("gtk_playlist_set_previous");

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

	D("gtk_playlist_set_next");

	g_return_if_fail (GTK_IS_PLAYLIST (playlist));

	if (gtk_playlist_has_next_mrl (playlist) == FALSE)
		return;

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
	GtkTreeIter iter;

	D("gtk_playlist_set_at_start");

	g_return_if_fail (GTK_IS_PLAYLIST (playlist));

	gtk_playlist_unset_playing (playlist);

	if (playlist->_priv->current != NULL)
	{
		gtk_tree_path_free (playlist->_priv->current);
		playlist->_priv->current = NULL;
	}
	update_current_from_playlist (playlist);
}

static void
gtk_playlist_class_init (GtkPlaylistClass *klass)
{
	D("gtk_playlist_class_init");

	G_OBJECT_CLASS (klass)->finalize = gtk_playlist_finalize;

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
				G_STRUCT_OFFSET (GtkPlaylistClass, changed),
				NULL, NULL,
				g_cclosure_marshal_VOID__VOID,
				G_TYPE_NONE, 0);
}

