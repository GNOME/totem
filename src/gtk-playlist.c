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

struct GtkPlaylistDetails
{
	GladeXML *xml;
	
	GtkWidget *treeview;
	GtkTreeModel *model;
	GtkTreePath *current;
};

enum {
	PIX_COL,
	FILENAME_COL,
	URI_COL,
	NUM_COLS
};

static void gtk_playlist_class_init (GtkPlaylistClass *class);
static void gtk_playlist_init       (GtkPlaylist      *label);

static void init_treeview (GtkWidget *treeview);

EEL_CLASS_BOILERPLATE (GtkPlaylist, gtk_playlist, gtk_dialog_get_type ());

static gboolean
gtk_tree_model_iter_previous (GtkTreeModel *tree_model, GtkTreeIter *iter)
{
	GtkTreePath *path;
	gboolean ret;

	path = gtk_tree_model_get_path (tree_model, iter);
	ret = gtk_tree_path_prev (path);
	if (ret == TRUE)
	{
		gtk_tree_model_get_iter (tree_model, iter, path);
	}

	gtk_tree_path_free (path);
	return ret;
}

static void
init_columns (GtkTreeView *treeview)
{
	GtkCellRenderer *renderer;
	GtkTreeViewColumn *column;
	GtkWidget *header;
	char *filename;

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
	gtk_widget_show (treeview);
}


static void
gtk_playlist_init (GtkPlaylist *playlist)
{
	playlist->details = g_new0 (GtkPlaylistDetails, 1);
	playlist->details->current = NULL;
}


static void
gtk_playlist_finalize (GObject *object)
{
	GtkPlaylist *label;

	label = GTK_PLAYLIST (object);
//FIXME free all the shite

	EEL_CALL_PARENT (G_OBJECT_CLASS, finalize, (object));
}

GtkWidget*
gtk_playlist_new (GtkWindow *parent)
{
	GtkPlaylist *playlist;
	GtkWidget *container;
	char *filename;

	playlist = GTK_PLAYLIST (g_object_new (GTK_TYPE_PLAYLIST, NULL));

	filename = gnome_program_locate_file (NULL,
			GNOME_FILE_DOMAIN_APP_DATADIR,
			"playlist.glade", TRUE, NULL);
	playlist->details->xml = glade_xml_new (filename, "vbox4", NULL);
	//FIXME check the glade file
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

	container = glade_xml_get_widget (playlist->details->xml, "vbox4");
	gtk_box_pack_start (GTK_BOX (GTK_DIALOG (playlist)->vbox),
			container,
			TRUE,       /* expand */
			TRUE,       /* fill */
			0);         /* padding */

	playlist->details->treeview = glade_xml_get_widget
		(playlist->details->xml, "treeview1");
	init_treeview (playlist->details->treeview);
	playlist->details->model = gtk_tree_view_get_model
		(GTK_TREE_VIEW (playlist->details->treeview));

	if (parent != NULL)
	{
		gtk_window_set_transient_for (GTK_WINDOW (playlist),
				parent);
	}

	gtk_widget_show_all (GTK_DIALOG (playlist)->vbox);

	return GTK_WIDGET (playlist);
}

gboolean
gtk_playlist_add_mrl (GtkPlaylist *playlist, char *mrl)
{
	GtkListStore *store;
	GtkTreeIter iter;
	char *filename;

	g_return_val_if_fail (GTK_IS_PLAYLIST (playlist), FALSE);
	g_return_val_if_fail (mrl != NULL, FALSE);

	filename = g_path_get_basename (mrl);
	g_message ("basename for %s: %s", mrl, filename);

	store = GTK_LIST_STORE (playlist->details->model);
	gtk_list_store_append (store, &iter);
	gtk_list_store_set (store, &iter,
			PIX_COL, NULL,
			FILENAME_COL, filename,
			URI_COL, mrl,
			-1);

	if (playlist->details->current == NULL)
		playlist->details->current = gtk_tree_model_get_path
			(playlist->details->model, &iter);
}

char
*gtk_playlist_get_current_mrl (GtkPlaylist *playlist)
{
	GtkTreeIter iter;
	char *path;

	gtk_tree_model_get_iter (playlist->details->model,
			&iter,
			playlist->details->current);
	gtk_tree_model_get (playlist->details->model,
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

	gtk_tree_model_get_iter (playlist->details->model,
			&iter,
			playlist->details->current);

	return gtk_tree_model_iter_previous (playlist->details->model, &iter);
}

gboolean
gtk_playlist_has_next_mrl (GtkPlaylist *playlist)
{
	GtkTreeIter iter;

	g_return_val_if_fail (GTK_IS_PLAYLIST (playlist), FALSE);

	gtk_tree_model_get_iter (playlist->details->model,
			&iter,
			playlist->details->current);

	return gtk_tree_model_iter_next (playlist->details->model, &iter);
}

static void
gtk_playlist_class_init (GtkPlaylistClass *klass)
{
	G_OBJECT_CLASS (klass)->finalize = gtk_playlist_finalize;
}
