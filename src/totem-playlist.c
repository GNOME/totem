/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* totem-playlist.c

   Copyright (C) 2002, 2003, 2004, 2005 Bastien Nocera

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
#include "totem-playlist.h"

#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <gdk/gdkkeysyms.h>
#include <glade/glade.h>
#include <gconf/gconf-client.h>
#include <libgnomevfs/gnome-vfs.h>
#include <libgnomevfs/gnome-vfs-mime-utils.h>
#include <string.h>

#include "totem-uri.h"
#include "totem-interface.h"
#include "totem-pl-parser.h"
#include "debug.h"

#define PL_LEN (gtk_tree_model_iter_n_children (playlist->_priv->model, NULL))

static void ensure_shuffled (TotemPlaylist *playlist, gboolean shuffle);
static gboolean totem_playlist_add_one_mrl (TotemPlaylist *playlist, const char *mrl, const char *display_name);

typedef gboolean (*PlaylistCallback) (TotemPlaylist *playlist, const char *mrl,
		gpointer data);

typedef struct {
	char *mimetype;
	PlaylistCallback func;
} PlaylistTypes;

struct TotemPlaylistPrivate
{
	GladeXML *xml;

	GtkWidget *treeview;
	GtkTreeModel *model;
	GtkTreePath *current;
	GtkTreeSelection *selection;
	TotemPlParser *parser;

	/* This is the playing icon */
	GdkPixbuf *icon;

	/* This is a scratch list for when we're removing files */
	GList *list;

	/* These is the current paths for the file selectors */
	char *path;
	char *save_path;

	/* Repeat mode */
	gboolean repeat;

	/* Reorder Flag */
	gboolean drag_started;

	/* Shuffle mode */
	gboolean shuffle;
	int *shuffled;
	int current_shuffled, shuffle_len;

	GConfClient *gc;

	int x, y;

	gboolean disable_save_to_disk;
};

/* Signals */
enum {
	CHANGED,
	ITEM_ACTIVATED,
	ACTIVE_NAME_CHANGED,
	CURRENT_REMOVED,
	REPEAT_TOGGLED,
	SHUFFLE_TOGGLED,
	LAST_SIGNAL
};

enum {
	PIX_COL,
	FILENAME_COL,
	URI_COL,
	TITLE_CUSTOM_COL,
	CACHE_TITLE_COL,
	CACHE_ARTIST_COL,
	CACHE_ALBUM_COL,
	NUM_COLS
};

static int totem_playlist_table_signals[LAST_SIGNAL] = { 0 };

static const GtkTargetEntry target_table[] = {
	{ "text/uri-list", 0, 0 },
	{ "_NETSCAPE_URL", 0, 1 },
};

static GtkWidgetClass *parent_class = NULL;

static void totem_playlist_class_init (TotemPlaylistClass *class);
static void totem_playlist_init       (TotemPlaylist      *playlist);

static void init_treeview (GtkWidget *treeview, TotemPlaylist *playlist);

#define totem_playlist_unset_playing(x) totem_playlist_set_playing(x, FALSE)

G_DEFINE_TYPE(TotemPlaylist, totem_playlist, GTK_TYPE_VBOX)

/* Helper functions */
static gboolean
totem_playlist_gtk_tree_model_iter_previous (GtkTreeModel *tree_model,
		GtkTreeIter *iter)
{
	GtkTreePath *path;
	gboolean ret;

	path = gtk_tree_model_get_path (tree_model, iter);
	ret = gtk_tree_path_prev (path);
	if (ret != FALSE)
		gtk_tree_model_get_iter (tree_model, iter, path);

	gtk_tree_path_free (path);
	return ret;
}

static gboolean
totem_playlist_gtk_tree_path_equals (GtkTreePath *path1, GtkTreePath *path2)
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

static GtkWindow *
totem_playlist_get_toplevel (TotemPlaylist *playlist)
{
	return GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (playlist)));
}

static void
totem_playlist_error (char *title, char *reason, TotemPlaylist *playlist)
{
	GtkWidget *error_dialog;

	error_dialog =
		gtk_message_dialog_new (totem_playlist_get_toplevel (playlist),
				GTK_DIALOG_MODAL,
				GTK_MESSAGE_ERROR,
				GTK_BUTTONS_OK,
				title);
	gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (error_dialog),
						  reason);

	gtk_container_set_border_width (GTK_CONTAINER (error_dialog), 5);
	gtk_dialog_set_default_response (GTK_DIALOG (error_dialog),
			GTK_RESPONSE_OK);
	g_signal_connect (G_OBJECT (error_dialog), "destroy", G_CALLBACK
			(gtk_widget_destroy), error_dialog);
	g_signal_connect (G_OBJECT (error_dialog), "response", G_CALLBACK
			(gtk_widget_destroy), error_dialog);
	gtk_window_set_modal (GTK_WINDOW (error_dialog), TRUE);

	gtk_widget_show (error_dialog);
}

/* This one returns a new string, in UTF8 even if the mrl is encoded
 * in the locale's encoding
 */
static char *
totem_playlist_mrl_to_title (const gchar *mrl)
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

	if (filename_for_display == NULL)
	{
		filename_for_display = g_locale_to_utf8 (unescaped,
				-1, NULL, NULL, NULL);
		if (filename_for_display == NULL)
			return unescaped;
	}

	g_free (unescaped);

	return filename_for_display;
}

static gboolean
totem_playlist_is_media (const char *mrl)
{
	if (mrl == NULL)
		return FALSE;

	if (g_str_has_prefix (mrl, "cdda:") != FALSE)
		return TRUE;
	if (g_str_has_prefix (mrl, "dvd:") != FALSE)
		return TRUE;
	if (g_str_has_prefix (mrl, "vcd:") != FALSE)
		return TRUE;

	return FALSE;
}

static void
totem_playlist_update_save_button (TotemPlaylist *playlist)
{
	GtkWidget *button;
	gboolean state;

	button = glade_xml_get_widget (playlist->_priv->xml, "save_button");
	state = (!playlist->_priv->disable_save_to_disk) && (PL_LEN != 0);
	gtk_widget_set_sensitive (button, state);
}

static char*
totem_playlist_create_full_path (const char *path)
{
	char *retval, *curdir, *curdir_withslash, *escaped;

	g_return_val_if_fail (path != NULL, NULL);

	if (strstr (path, "://") != NULL)
		return g_strdup (path);
	if (totem_playlist_is_media (path) != FALSE)
		return g_strdup (path);

	if (path[0] == '/')
	{
		escaped = gnome_vfs_escape_path_string (path);

		retval = g_strdup_printf ("file://%s", escaped);
		g_free (escaped);
		return retval;
	}

	curdir = g_get_current_dir ();
	escaped = gnome_vfs_escape_path_string (curdir);
	curdir_withslash = g_strdup_printf ("file://%s%s",
			escaped, G_DIR_SEPARATOR_S);
	g_free (escaped);
	g_free (curdir);

	escaped = gnome_vfs_escape_path_string (path);
	retval = gnome_vfs_uri_make_full_from_relative
		(curdir_withslash, escaped);
	g_free (curdir_withslash);
	g_free (escaped);

	return retval;
}

static void
totem_playlist_save_get_iter_func (GtkTreeModel *model,
		GtkTreeIter *iter, char **uri, char **title,
		gboolean *custom_title, gpointer user_data)
{
	gtk_tree_model_get (model, iter,
			URI_COL, uri,
			FILENAME_COL, title,
			TITLE_CUSTOM_COL, custom_title,
			-1);
}

void
totem_playlist_save_current_playlist (TotemPlaylist *playlist, const char *output)
{
	GError *error = NULL;
	gboolean retval;

	retval = totem_pl_parser_write (playlist->_priv->parser,
			playlist->_priv->model,
                        totem_playlist_save_get_iter_func,
			output, TOTEM_PL_PARSER_PLS, NULL, &error);

	if (retval == FALSE)
	{
		totem_playlist_error (_("Could not save the playlist"),
				error->message, playlist);
		g_error_free (error);
	}
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

static void
drop_cb (GtkWidget     *widget,
         GdkDragContext     *context, 
	 gint                x,
	 gint                y,
	 GtkSelectionData   *data, 
	 guint               info, 
	 guint               time, 
	 TotemPlaylist        *playlist)
{
	GList *list, *p, *file_list;

	list = gnome_vfs_uri_list_parse ((char *)data->data);

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
		char *filename, *title;

		if (p->data == NULL)
			continue;

		filename = totem_create_full_path (p->data);
		title = NULL;

		if (info == 1)
		{
			g_free (p->data);
			p = p->next;
			if (p != NULL) {
				if (g_str_has_prefix (p->data, "file:") != FALSE)
					title = (char *)p->data + 5;
				else
					title = p->data;
			}
		}

		totem_playlist_add_mrl (playlist, p->data, title);

		g_free (filename);
		g_free (p->data);
	}

	g_list_free (file_list);
	gtk_drag_finish (context, TRUE, FALSE, time);

	g_signal_emit (G_OBJECT (playlist),
			totem_playlist_table_signals[CHANGED], 0,
			NULL);
}

static void
on_copy1_activate (GtkButton *button, TotemPlaylist *playlist)
{
	GList *l;
	GtkTreePath *path;
	GtkClipboard *clip;
	char *url;
	GtkTreeIter iter;

	l = gtk_tree_selection_get_selected_rows (playlist->_priv->selection,
			NULL);
	path = l->data;

	gtk_tree_model_get_iter (playlist->_priv->model, &iter, path);

	gtk_tree_model_get (playlist->_priv->model,
			&iter,
			URI_COL, &url,
			-1);

	/* Set both the middle-click and the super-paste buffers */
	clip = gtk_clipboard_get_for_display
		(gdk_display_get_default(), GDK_SELECTION_CLIPBOARD);
	gtk_clipboard_set_text (clip, url, -1);
	clip = gtk_clipboard_get_for_display
		(gdk_display_get_default(), GDK_SELECTION_PRIMARY);
	gtk_clipboard_set_text (clip, url, -1);
	g_free (url);

	g_list_foreach (l, (GFunc) gtk_tree_path_free, NULL);
	g_list_free (l);
}

static gboolean
treeview_button_pressed (GtkTreeView *treeview, GdkEventButton *event,
		TotemPlaylist *playlist)
{
	GtkTreePath *path;
	GtkWidget *menu;

	if (event->type != GDK_BUTTON_PRESS
			|| event->button != 3)
		return FALSE;

	if (gtk_tree_view_get_path_at_pos (GTK_TREE_VIEW (treeview),
				event->x, event->y, &path,
				NULL, NULL, NULL) == FALSE)
	{
		return FALSE;
	}

	gtk_tree_selection_unselect_all (playlist->_priv->selection);
	gtk_tree_selection_select_path (playlist->_priv->selection, path);
	gtk_tree_path_free (path);

	menu = glade_xml_get_widget (playlist->_priv->xml, "menu1");
	gtk_menu_popup (GTK_MENU (menu), NULL, NULL, NULL, NULL,
			event->button, event->time);

	return TRUE;
}

static void
totem_playlist_set_reorderable (TotemPlaylist *playlist, gboolean set)
{
	guint num_items, i;

	gtk_tree_view_set_reorderable
		(GTK_TREE_VIEW (playlist->_priv->treeview), set);

	if (set != FALSE)
		return;

	num_items = PL_LEN;
	for (i = 0; i < num_items; i++)
	{
		GtkTreeIter iter;
		char *index;
		GdkPixbuf *pixbuf;
		GtkTreePath *path;

		index = g_strdup_printf ("%d", i);
		if (gtk_tree_model_get_iter_from_string
				(playlist->_priv->model,
				 &iter, index) == FALSE)
		{
			g_free (index);
			continue;
		}
		g_free (index);

		gtk_tree_model_get (playlist->_priv->model, &iter,
				PIX_COL, &pixbuf, -1);

		if (pixbuf == NULL)
			continue;

		gdk_pixbuf_unref (pixbuf);

		/* Only emit the changed signal if we changed the ->current */
		path = gtk_tree_path_new_from_indices (i, -1);
		if (gtk_tree_path_compare (path, playlist->_priv->current) == 0)
		{
			gtk_tree_path_free (path);
		} else {
			gtk_tree_path_free (playlist->_priv->current);
			playlist->_priv->current = path;
			g_signal_emit (G_OBJECT (playlist),
					totem_playlist_table_signals[CHANGED],
					0, NULL);
		}

		break;
	}
}

static gboolean 
button_press_cb (GtkWidget *treeview, GdkEventButton *event, gpointer data)
{ 
	gtk_drag_dest_unset (treeview);
	g_signal_handlers_block_by_func (treeview, (GFunc) drop_cb, data);

	totem_playlist_set_reorderable ((TotemPlaylist *) data, TRUE);

	return FALSE;
}

static gboolean 
button_release_cb (GtkWidget *treeview, GdkEventButton *event, gpointer data)
{
	TotemPlaylist *playlist = (TotemPlaylist *)data;

	if (!playlist->_priv->drag_started)
	{
		totem_playlist_set_reorderable (playlist, FALSE);
		gtk_drag_dest_set (treeview, GTK_DEST_DEFAULT_ALL,
				target_table, G_N_ELEMENTS (target_table),
				GDK_ACTION_COPY | GDK_ACTION_MOVE);

		g_signal_handlers_unblock_by_func (treeview,
				(GFunc) drop_cb, data);
	}

	return FALSE;
}

static void 
drag_begin_cb (GtkWidget *treeview, GdkDragContext *context, gpointer data)
{
	TotemPlaylist *playlist = (TotemPlaylist *)data;

	playlist->_priv->drag_started= TRUE;

	return;
}

static void 
drag_end_cb (GtkWidget *treeview, GdkDragContext *context, gpointer data)
{
	TotemPlaylist *playlist = (TotemPlaylist *)data;

	playlist->_priv->drag_started = FALSE;
	totem_playlist_set_reorderable (playlist, FALSE);
	gtk_drag_dest_set (treeview, GTK_DEST_DEFAULT_ALL, target_table,
        		G_N_ELEMENTS (target_table),
			GDK_ACTION_COPY | GDK_ACTION_MOVE);

	g_signal_handlers_unblock_by_func (treeview, (GFunc) drop_cb, data);

	return;
}

static void
selection_changed (GtkTreeSelection *treeselection, TotemPlaylist *playlist)
{
	GtkWidget *remove_button, *up_button, *down_button;
	gboolean sensitivity;

	remove_button = glade_xml_get_widget (playlist->_priv->xml,
			"remove_button");
	up_button = glade_xml_get_widget (playlist->_priv->xml, "up_button");
	down_button = glade_xml_get_widget (playlist->_priv->xml,
			"down_button");

	if (gtk_tree_selection_has_selected (treeselection))
		sensitivity = TRUE;
	else
		sensitivity = FALSE;

	gtk_widget_set_sensitive (remove_button, sensitivity);
	gtk_widget_set_sensitive (up_button, sensitivity);
	gtk_widget_set_sensitive (down_button, sensitivity);
}

/* This function checks if the current item is NULL, and try to update it
 * as the first item of the playlist if so. It returns TRUE if there is a
 * current item */
static gboolean
update_current_from_playlist (TotemPlaylist *playlist)
{
	int indice;

	if (playlist->_priv->current != NULL)
		return TRUE;

	if (PL_LEN != 0)
	{
		if (playlist->_priv->shuffle == FALSE)
		{
			indice = 0;
		} else {
			indice = playlist->_priv->shuffled[0];
			playlist->_priv->current_shuffled = 0;
		}

		playlist->_priv->current = gtk_tree_path_new_from_indices
			(indice, -1);
	} else {
		return FALSE;
	}

	return TRUE;
}

static void
totem_playlist_add_files (GtkWidget *widget, TotemPlaylist *playlist)
{
	GSList *filenames, *l;
	char *new_path;

	filenames = totem_add_files (totem_playlist_get_toplevel (playlist),
			playlist->_priv->path, &new_path);
	if (filenames == NULL)
		return;

	if (new_path != NULL) {
		g_free (playlist->_priv->path);
		playlist->_priv->path = new_path;
	}

	for (l = filenames; l != NULL; l = l->next) {
		char *mrl;

		mrl = l->data;
		totem_playlist_add_mrl (playlist, mrl, NULL);
		g_free (mrl);
	}

	g_slist_free (filenames);
}

static void
totem_playlist_foreach_selected (GtkTreeModel *model, GtkTreePath *path,
		GtkTreeIter *iter, gpointer data)
{
	TotemPlaylist *playlist = (TotemPlaylist *)data;
	GtkTreeRowReference *ref;

	/* We can't use gtk_list_store_remove() here
	 * So we build a list a RowReferences */
	ref = gtk_tree_row_reference_new (playlist->_priv->model, path);
	playlist->_priv->list = g_list_prepend
		(playlist->_priv->list, (gpointer) ref);
}

static void
totem_playlist_remove_files (GtkWidget *widget, TotemPlaylist *playlist)
{
	GtkTreeSelection *selection;
	GtkTreeRowReference *ref;
	gboolean is_selected = FALSE;
	int next_pos;

	selection = gtk_tree_view_get_selection
		(GTK_TREE_VIEW (playlist->_priv->treeview));
	if (selection == NULL)
		return;

	gtk_tree_selection_selected_foreach (selection,
			totem_playlist_foreach_selected,
			(gpointer) playlist);

	/* If the current item is to change, we need to keep an static
	 * reference to it, TreeIter and TreePath don't allow that */
	if (playlist->_priv->current != NULL)
	{
		int *indices;

		ref = gtk_tree_row_reference_new (playlist->_priv->model,
				playlist->_priv->current);
		is_selected = gtk_tree_selection_path_is_selected (selection,
				playlist->_priv->current);

		indices = gtk_tree_path_get_indices (playlist->_priv->current);
		next_pos = indices[0];

		gtk_tree_path_free (playlist->_priv->current);
	} else {
		ref = NULL;
		next_pos = -1;
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
	g_list_free (playlist->_priv->list);
	playlist->_priv->list = NULL;

	if (is_selected != FALSE)
	{
		/* The current item was removed from the playlist */
		if (next_pos != -1)
		{
			char *str;
			GtkTreeIter iter;
			GtkTreePath *cur;

			str = g_strdup_printf ("%d", next_pos);
			cur = gtk_tree_path_new_from_string (str);

			if (gtk_tree_model_get_iter (playlist->_priv->model,
						&iter, cur) == FALSE)
			{
				playlist->_priv->current = NULL;
				gtk_tree_path_free (cur);
			} else {
				playlist->_priv->current = cur;
			}
			g_free (str);
		} else {
			playlist->_priv->current = NULL;
		}

		playlist->_priv->current_shuffled = -1;
		ensure_shuffled (playlist, playlist->_priv->shuffle);

		g_signal_emit (G_OBJECT (playlist),
				totem_playlist_table_signals[CURRENT_REMOVED],
				0, NULL);
	} else {
		if (ref != NULL)
		{
			/* The path to the current item changed */
			playlist->_priv->current =
				gtk_tree_row_reference_get_path (ref);
			gtk_tree_row_reference_free (ref);
		}

		ensure_shuffled (playlist, playlist->_priv->shuffle);

		g_signal_emit (G_OBJECT (playlist),
				totem_playlist_table_signals[CHANGED], 0,
				NULL);
	}
	totem_playlist_update_save_button (playlist);
	gtk_tree_view_columns_autosize (playlist->_priv->treeview);
}

static void
totem_playlist_save_files (GtkWidget *widget, TotemPlaylist *playlist)
{
	GtkWidget *fs;
	int response;

	fs = gtk_file_chooser_dialog_new (_("Save Playlist"),
			totem_playlist_get_toplevel (playlist),
			GTK_FILE_CHOOSER_ACTION_SAVE,
			GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
			GTK_STOCK_SAVE, GTK_RESPONSE_ACCEPT,
			NULL);
	gtk_dialog_set_default_response (GTK_DIALOG (fs), GTK_RESPONSE_ACCEPT);
	gtk_file_chooser_set_local_only (GTK_FILE_CHOOSER (fs), FALSE);

	if (playlist->_priv->save_path != NULL)
	{
		gtk_file_chooser_set_current_folder_uri (GTK_FILE_CHOOSER (fs),
				playlist->_priv->save_path);
	}

	response = gtk_dialog_run (GTK_DIALOG (fs));
	gtk_widget_hide (fs);
	while (gtk_events_pending())
		gtk_main_iteration();

	if (response == GTK_RESPONSE_ACCEPT)
	{
		char *filename;
		GnomeVFSResult res;
		GnomeVFSFileInfo info;

		filename = gtk_file_chooser_get_uri (GTK_FILE_CHOOSER (fs));

		gtk_widget_destroy (fs);

		if (filename == NULL)
			return;

		g_free (playlist->_priv->save_path);
		playlist->_priv->save_path = g_path_get_dirname (filename);

		res = gnome_vfs_get_file_info (filename, &info, GNOME_VFS_FILE_INFO_DEFAULT);
		if (res != GNOME_VFS_ERROR_NOT_FOUND && res == GNOME_VFS_OK)
		{
			GtkWidget *dialog;

			dialog = gtk_message_dialog_new
				(totem_playlist_get_toplevel (playlist),
				 GTK_DIALOG_MODAL,
				 GTK_MESSAGE_QUESTION,
				 GTK_BUTTONS_NONE,
				 _("Overwrite file?"));
			gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
								  _("A file named '%s' already exists.  Are you sure you want to overwrite it?"),
								  filename);

			gtk_container_set_border_width (GTK_CONTAINER (dialog), 5);
			gtk_dialog_add_buttons (GTK_DIALOG (dialog),
					GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT,
					GTK_STOCK_SAVE, GTK_RESPONSE_ACCEPT,
					NULL);

			response = gtk_dialog_run (GTK_DIALOG (dialog));
			gtk_widget_destroy (dialog);
			if (response != GTK_RESPONSE_ACCEPT)
			{
				g_free (filename);
				return;
			}
		}

		totem_playlist_save_current_playlist (playlist, filename);
		g_free (filename);
	} else {
		gtk_widget_destroy (fs);
	}
}

static void
totem_playlist_move_files (TotemPlaylist *playlist, gboolean direction_up)
{
	GtkTreeSelection *selection;
	GtkTreeModel *model;
	GtkListStore *store;
	GtkTreeIter iter;
	GtkTreeRowReference *current;
	GList *paths, *refs, *l;
	int pos;

	selection = gtk_tree_view_get_selection
		(GTK_TREE_VIEW (playlist->_priv->treeview));
	if (selection == NULL)
		return;

	model = gtk_tree_view_get_model
		(GTK_TREE_VIEW (playlist->_priv->treeview));
	store = GTK_LIST_STORE (model);
	pos = -2;
	refs = NULL;

	if (playlist->_priv->current != NULL)
	{
		current = gtk_tree_row_reference_new (model,
				playlist->_priv->current);
	} else {
		current = NULL;
	}

	/* Build a list of tree references */
	paths = gtk_tree_selection_get_selected_rows (selection, NULL);
	for (l = paths; l != NULL; l = l->next)
	{
		GtkTreePath *path = l->data;
		int cur_pos, *indices;

		refs = g_list_prepend (refs,
				gtk_tree_row_reference_new (model, path));
		indices = gtk_tree_path_get_indices (path);
		cur_pos = indices[0];
		if (pos == -2)
		{
			pos = cur_pos;
		} else {
			if (direction_up == FALSE)
				pos = MAX (cur_pos, pos);
			else
				pos = MIN (cur_pos, pos);
		}
	}
	g_list_foreach (paths, (GFunc) gtk_tree_path_free, NULL);
	g_list_free (paths);

	refs = g_list_reverse (refs);

	if (direction_up == FALSE)
		pos = pos + 2;
	else
		pos = pos - 2;

	for (l = refs; l != NULL; l = l->next)
	{
		GtkTreeIter *position, current;
		GtkTreeRowReference *ref = l->data;
		GtkTreePath *path;

		if (pos < 0)
		{
			position = NULL;
		} else {
			char *str;

			str = g_strdup_printf ("%d", pos);
			if (gtk_tree_model_get_iter_from_string (model,
					&iter, str))
				position = &iter;
			else
				position = NULL;

			g_free (str);
		}

		path = gtk_tree_row_reference_get_path (ref);
		gtk_tree_model_get_iter (model, &current, path);
		gtk_tree_path_free (path);

		if (direction_up == FALSE)
		{
			pos--;
			gtk_list_store_move_before (store, &current, position);
		} else {
			gtk_list_store_move_after (store, &current, position);
			pos++;
		}
	}

	g_list_foreach (refs, (GFunc) gtk_tree_row_reference_free, NULL);
	g_list_free (refs);

	/* Update the current path */
	if (current != NULL)
	{
		gtk_tree_path_free (playlist->_priv->current);
		playlist->_priv->current = gtk_tree_row_reference_get_path
			(current);
		gtk_tree_row_reference_free (current);
	}

	g_signal_emit (G_OBJECT (playlist),
			totem_playlist_table_signals[CHANGED], 0,
			NULL);
}

static void
totem_playlist_up_files (GtkWidget *widget, TotemPlaylist *playlist)
{
	totem_playlist_move_files (playlist, TRUE);
}

static void
totem_playlist_down_files (GtkWidget *widget, TotemPlaylist *playlist)
{
	totem_playlist_move_files (playlist, FALSE);
}

static int
totem_playlist_key_press (GtkWidget *win, GdkEventKey *event, TotemPlaylist *playlist)
{
	//FIXME add support for GDK_Menu and Ctrl+F10
	/* Special case some shortcuts */
	if (event->state != 0
			&& (event->state &GDK_CONTROL_MASK))
	{
		switch (event->keyval)
		case GDK_a:
			gtk_tree_selection_select_all (playlist->_priv->selection);
			return TRUE;
	}
	/* If we have modifiers, and either Ctrl, Mod1 (Alt), or any
	 * of Mod3 to Mod5 (Mod2 is num-lock...) are pressed, we
	 * let Gtk+ handle the key */
	if (event->state != 0
			&& ((event->state & GDK_CONTROL_MASK)
			|| (event->state & GDK_MOD1_MASK)
			|| (event->state & GDK_MOD3_MASK)
			|| (event->state & GDK_MOD4_MASK)
			|| (event->state & GDK_MOD5_MASK)))
		return FALSE;

	if (event->keyval == GDK_Delete)
	{
		totem_playlist_remove_files (NULL, playlist);
		return TRUE;
	} else if (event->keyval == GDK_Escape) {
		gtk_dialog_response (GTK_DIALOG (playlist),
				GTK_RESPONSE_CLOSE);
	}

	return FALSE;
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
	gtk_tree_view_append_column (treeview, column);

	/* Labels */
	renderer = gtk_cell_renderer_text_new ();
	gtk_tree_view_column_pack_start (column, renderer, TRUE);
	gtk_tree_view_column_set_attributes (column, renderer,
			"text", FILENAME_COL, NULL);
}

static void
treeview_row_changed (GtkTreeView *treeview, GtkTreePath *arg1,
		GtkTreeViewColumn *arg2, TotemPlaylist *playlist)
{
	if (totem_playlist_gtk_tree_path_equals
			(arg1, playlist->_priv->current) != FALSE)
	{
		g_signal_emit (G_OBJECT (playlist),
				totem_playlist_table_signals[ITEM_ACTIVATED], 0,
				NULL);
		return;
	}

	if (playlist->_priv->current != NULL)
	{
		totem_playlist_unset_playing (playlist);
		gtk_tree_path_free (playlist->_priv->current);
	}

	playlist->_priv->current = gtk_tree_path_copy (arg1);

	if (playlist->_priv->shuffle != FALSE)
	{
		int *indices, indice, i;

		indices = gtk_tree_path_get_indices (playlist->_priv->current);
		indice = indices[0];

		for (i = 0; i < PL_LEN; i++)
		{
			if (playlist->_priv->shuffled[i] == indice)
			{
				playlist->_priv->current_shuffled = i;
				break;
			}
		}
	}
	g_signal_emit (G_OBJECT (playlist),
			totem_playlist_table_signals[CHANGED], 0,
			NULL);

	g_signal_handlers_unblock_by_func (treeview,
			(GFunc) drop_cb, playlist);
}

static void
init_treeview (GtkWidget *treeview, TotemPlaylist *playlist)
{
	GtkTreeModel *model;
	GtkTreeSelection *selection;

	/* the model */
	model = GTK_TREE_MODEL (gtk_list_store_new (NUM_COLS,
				GDK_TYPE_PIXBUF,
				G_TYPE_STRING,
				G_TYPE_STRING,
				G_TYPE_BOOLEAN,
				G_TYPE_STRING,
				G_TYPE_STRING,
				G_TYPE_STRING));

	/* the treeview */
	gtk_tree_view_set_model (GTK_TREE_VIEW (treeview), model);
	gtk_tree_view_set_rules_hint (GTK_TREE_VIEW (treeview), TRUE);
	gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (treeview), FALSE);
	g_object_unref (G_OBJECT (model));

	init_columns (GTK_TREE_VIEW (treeview));

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (treeview));
	gtk_tree_selection_set_mode (selection, GTK_SELECTION_MULTIPLE);
	g_signal_connect (G_OBJECT (selection), "changed",
			G_CALLBACK (selection_changed), playlist);
	g_signal_connect (G_OBJECT (treeview), "row-activated",
			G_CALLBACK (treeview_row_changed), playlist);
	g_signal_connect (G_OBJECT (treeview), "button-press-event",
			G_CALLBACK (treeview_button_pressed), playlist);

	/* Drag'n'Drop */
	g_signal_connect (G_OBJECT (treeview), "drag_data_received",
			G_CALLBACK (drop_cb), playlist);
        g_signal_connect (G_OBJECT (treeview), "button_press_event",
			G_CALLBACK (button_press_cb), playlist);
        g_signal_connect (G_OBJECT (treeview), "button_release_event",
			G_CALLBACK (button_release_cb), playlist);
	g_signal_connect (G_OBJECT (treeview), "drag_begin",
                        G_CALLBACK (drag_begin_cb), playlist);
         g_signal_connect (G_OBJECT (treeview), "drag_end",
                        G_CALLBACK (drag_end_cb), playlist);
	gtk_drag_dest_set (treeview, GTK_DEST_DEFAULT_ALL,
			target_table, G_N_ELEMENTS (target_table),
			GDK_ACTION_COPY | GDK_ACTION_MOVE);

	playlist->_priv->selection = selection;

	gtk_widget_show (treeview);
}

static void
update_repeat_cb (GConfClient *client, guint cnxn_id,
		GConfEntry *entry, TotemPlaylist *playlist)
{
	gboolean repeat;

	repeat = gconf_value_get_bool (entry->value);
	playlist->_priv->repeat = repeat;

	g_signal_emit (G_OBJECT (playlist),
			totem_playlist_table_signals[CHANGED], 0,
			NULL);
	g_signal_emit (G_OBJECT (playlist),
			totem_playlist_table_signals[REPEAT_TOGGLED], 0,
			repeat, NULL);
}

typedef struct {
	int random;
	int index;
} RandomData;

static int
compare_random (gconstpointer ptr_a, gconstpointer ptr_b)
{
	RandomData *a = (RandomData *) ptr_a;
	RandomData *b = (RandomData *) ptr_b;

	if (a->random < b->random)
		return -1;
	else if (a->random > b->random)
		return 1;
	else
		return 0;
}

static void
ensure_shuffled (TotemPlaylist *playlist, gboolean shuffle)
{
	RandomData data;
	GArray *array;
	int i, current;
	int *indices;

	if (shuffle == FALSE || PL_LEN != playlist->_priv->shuffle_len)
	{
		g_free (playlist->_priv->shuffled);
		playlist->_priv->shuffled = NULL;
	}

	if (shuffle == FALSE || PL_LEN == 0)
		return;

	if (playlist->_priv->current != NULL)
	{
		indices = gtk_tree_path_get_indices (playlist->_priv->current);
		current = indices[0];
	} else {
		current = -1;
	}

	playlist->_priv->shuffled = g_new (int, PL_LEN);
	playlist->_priv->shuffle_len = PL_LEN;

	array = g_array_sized_new (FALSE, FALSE,
			sizeof (RandomData), PL_LEN);

	for (i = 0; i < PL_LEN; i++)
	{
		data.random = g_random_int_range (0, PL_LEN);
		data.index = i;

		g_array_append_val (array, data);
	}

	g_array_sort (array, compare_random);

	for (i = 0; i < PL_LEN; i++)
	{
		playlist->_priv->shuffled[i]
			= g_array_index (array, RandomData, i).index;

		if (playlist->_priv->current != NULL
				&& playlist->_priv->shuffled[i] == current)
			playlist->_priv->current_shuffled = i;
	}

	g_array_free (array, TRUE);
}

static void
update_shuffle_cb (GConfClient *client, guint cnxn_id,
		GConfEntry *entry, TotemPlaylist *playlist)
{
	gboolean shuffle;

	shuffle = gconf_value_get_bool (entry->value);
	playlist->_priv->shuffle = shuffle;
	ensure_shuffled (playlist, shuffle);

	g_signal_emit (G_OBJECT (playlist),
			totem_playlist_table_signals[CHANGED], 0,
			NULL);
	g_signal_emit (G_OBJECT (playlist),
			totem_playlist_table_signals[SHUFFLE_TOGGLED], 0,
			shuffle, NULL);
}

static void
update_lockdown (GConfClient *client, guint cnxn_id,
		GConfEntry *entry, TotemPlaylist *playlist)
{
	playlist->_priv->disable_save_to_disk = gconf_client_get_bool
			(playlist->_priv->gc,
			"/desktop/gnome/lockdown/disable_save_to_disk", NULL);
	totem_playlist_update_save_button (playlist);
}

static void
init_config (TotemPlaylist *playlist)
{
	playlist->_priv->gc = gconf_client_get_default ();

	playlist->_priv->disable_save_to_disk = gconf_client_get_bool
	       		(playlist->_priv->gc,
			"/desktop/gnome/lockdown/disable_save_to_disk", NULL);
	totem_playlist_update_save_button (playlist);

	gconf_client_add_dir (playlist->_priv->gc, GCONF_PREFIX,
			GCONF_CLIENT_PRELOAD_ONELEVEL, NULL);
	gconf_client_notify_add (playlist->_priv->gc, GCONF_PREFIX"/repeat",
			(GConfClientNotifyFunc) update_repeat_cb,
			playlist, NULL, NULL);
	gconf_client_notify_add (playlist->_priv->gc, GCONF_PREFIX"/shuffle",
			(GConfClientNotifyFunc) update_shuffle_cb,
			playlist, NULL, NULL);

	gconf_client_add_dir (playlist->_priv->gc, "/desktop/gnome/lockdown",
			GCONF_CLIENT_PRELOAD_ONELEVEL, NULL);
	gconf_client_notify_add (playlist->_priv->gc,
			"/desktop/gnome/lockdown/disable_save_to_disk",
			(GConfClientNotifyFunc) update_lockdown,
			playlist, NULL, NULL);

	playlist->_priv->repeat = gconf_client_get_bool (playlist->_priv->gc,
			GCONF_PREFIX"/repeat", NULL);
	playlist->_priv->shuffle = gconf_client_get_bool (playlist->_priv->gc,
			GCONF_PREFIX"/shuffle", NULL);
}

static void
totem_playlist_entry_parsed (TotemPlParser *parser,
		const char *uri, const char *title,
		const char *genre, TotemPlaylist *playlist)
{
	totem_playlist_add_one_mrl (playlist, uri, title);
}

static void
totem_playlist_init (TotemPlaylist *playlist)
{
	playlist->_priv = g_new0 (TotemPlaylistPrivate, 1);
	playlist->_priv->parser = totem_pl_parser_new ();

	totem_pl_parser_add_ignored_scheme (playlist->_priv->parser, "dvd:");
	totem_pl_parser_add_ignored_scheme (playlist->_priv->parser, "cdda:");
	totem_pl_parser_add_ignored_scheme (playlist->_priv->parser, "vcd:");

	g_signal_connect (G_OBJECT (playlist->_priv->parser),
			"entry",
			G_CALLBACK (totem_playlist_entry_parsed),
			playlist);
}

static void
totem_playlist_finalize (GObject *object)
{
	TotemPlaylist *playlist = TOTEM_PLAYLIST (object);

	g_return_if_fail (object != NULL);

	if (playlist->_priv->current != NULL)
		gtk_tree_path_free (playlist->_priv->current);
	if (playlist->_priv->icon != NULL)
		gdk_pixbuf_unref (playlist->_priv->icon);
	g_object_unref (playlist->_priv->parser);

	if (G_OBJECT_CLASS (parent_class)->finalize != NULL) {
		(* G_OBJECT_CLASS (parent_class)->finalize) (object);
	}
}

GtkWidget*
totem_playlist_new (void)
{
	TotemPlaylist *playlist;
	GtkWidget *container, *item;

	playlist = TOTEM_PLAYLIST (g_object_new (GTK_TYPE_PLAYLIST, NULL));

	playlist->_priv->xml = totem_interface_load ("playlist.glade",
			_("playlist"), TRUE, NULL);

	if (playlist->_priv->xml == NULL)
	{
		totem_playlist_finalize (G_OBJECT (playlist));
		return NULL;
	}

	/* Connect the buttons */
	item = glade_xml_get_widget (playlist->_priv->xml, "add_button");
	g_signal_connect (G_OBJECT (item), "clicked",
			G_CALLBACK (totem_playlist_add_files),
			playlist);
	item = glade_xml_get_widget (playlist->_priv->xml, "remove_button");
	g_signal_connect (G_OBJECT (item), "clicked",
			G_CALLBACK (totem_playlist_remove_files),
			playlist);
	item = glade_xml_get_widget (playlist->_priv->xml, "save_button");
	g_signal_connect (G_OBJECT (item), "clicked",
			G_CALLBACK (totem_playlist_save_files),
			playlist);
	item = glade_xml_get_widget (playlist->_priv->xml, "up_button");
	g_signal_connect (G_OBJECT (item), "clicked",
			G_CALLBACK (totem_playlist_up_files),
			playlist);
	item = glade_xml_get_widget (playlist->_priv->xml, "down_button");
	g_signal_connect (G_OBJECT (item), "clicked",
			G_CALLBACK (totem_playlist_down_files),
			playlist);

	item = glade_xml_get_widget (playlist->_priv->xml, "copy1");
	g_signal_connect (G_OBJECT (item), "activate",
			G_CALLBACK (on_copy1_activate), playlist);

	gtk_widget_add_events (GTK_WIDGET (playlist), GDK_KEY_PRESS_MASK);
	g_signal_connect (G_OBJECT (playlist), "key_press_event",
			G_CALLBACK (totem_playlist_key_press), playlist);

	/* Reparent the vbox */
	item = glade_xml_get_widget (playlist->_priv->xml, "dialog-vbox1");
	container = glade_xml_get_widget (playlist->_priv->xml, "vbox4");
	g_object_ref (container);
	gtk_container_remove (GTK_CONTAINER (item), container);
	gtk_box_pack_start (GTK_BOX (playlist),
			container,
			TRUE,       /* expand */
			TRUE,       /* fill */
			0);         /* padding */
	g_object_unref (container);

	playlist->_priv->treeview = glade_xml_get_widget
		(playlist->_priv->xml, "treeview1");
	init_treeview (playlist->_priv->treeview, playlist);
	playlist->_priv->model = gtk_tree_view_get_model
		(GTK_TREE_VIEW (playlist->_priv->treeview));

	/* The configuration */
	init_config (playlist);

	playlist->_priv->icon =
		totem_interface_load_pixbuf ("playlist-playing.png");

	gtk_widget_show_all (GTK_WIDGET (playlist));

	return GTK_WIDGET (playlist);
}

static gboolean
totem_playlist_add_one_mrl (TotemPlaylist *playlist, const char *mrl,
		const char *display_name)
{
	GtkListStore *store;
	GtkTreeIter iter;
	char *filename_for_display, *uri;

	g_return_val_if_fail (GTK_IS_PLAYLIST (playlist), FALSE);
	g_return_val_if_fail (mrl != NULL, FALSE);

	if (display_name == NULL)
	{
		filename_for_display = totem_playlist_mrl_to_title (mrl);
	} else {
		filename_for_display = g_strdup (display_name);
	}

	uri = totem_playlist_create_full_path (mrl);

	D("totem_playlist_add_one_mrl (): %s %s %s\n",
				filename_for_display, uri, display_name);

	store = GTK_LIST_STORE (playlist->_priv->model);
	gtk_list_store_insert_with_values (store, &iter, G_MAXINT32,
			PIX_COL, NULL,
			FILENAME_COL, filename_for_display,
			URI_COL, uri,
			TITLE_CUSTOM_COL, display_name ? TRUE : FALSE,
			-1);

	g_free (filename_for_display);
	g_free (uri);

	if (playlist->_priv->current == NULL
			&& playlist->_priv->shuffle == FALSE)
		playlist->_priv->current = gtk_tree_model_get_path
			(playlist->_priv->model, &iter);
	ensure_shuffled (playlist, playlist->_priv->shuffle);

	g_signal_emit (G_OBJECT (playlist),
			totem_playlist_table_signals[CHANGED], 0,
			NULL);
	totem_playlist_update_save_button (playlist);

	return TRUE;
}

gboolean
totem_playlist_add_mrl (TotemPlaylist *playlist, const char *mrl,
		const char *display_name)
{
	TotemPlParserResult res;

	g_return_val_if_fail (mrl != NULL, FALSE);

	res = totem_pl_parser_parse (playlist->_priv->parser, mrl, TRUE);

	if (res == TOTEM_PL_PARSER_RESULT_UNHANDLED)
		return totem_playlist_add_one_mrl (playlist, mrl, display_name);
	if (res == TOTEM_PL_PARSER_RESULT_ERROR)
	{
		totem_playlist_error (_("Playlist error"), _("The playlist '%s' could not be parsed, it might be damaged."), playlist);
		return FALSE;
	}
	if (res == TOTEM_PL_PARSER_RESULT_IGNORED)
		return FALSE;

	return TRUE;
}

void
totem_playlist_clear (TotemPlaylist *playlist)
{
	GtkListStore *store;

	g_return_if_fail (GTK_IS_PLAYLIST (playlist));

	store = GTK_LIST_STORE (playlist->_priv->model);
	gtk_list_store_clear (store);

	if (playlist->_priv->current != NULL)
		gtk_tree_path_free (playlist->_priv->current);
	playlist->_priv->current = NULL;
}

static void
totem_playlist_clear_with_compare (TotemPlaylist *playlist, GCompareFunc func,
		gconstpointer data)
{
	GList *list = NULL, *l;
	guint num_items, i;
	gboolean has_items;

	num_items = PL_LEN;
	if (num_items == 0)
		return;

	for (i = 0; i < num_items; i++)
	{
		GtkTreeIter iter;
		char *index;
		char *mrl;

		index = g_strdup_printf ("%d", i);
		if (gtk_tree_model_get_iter_from_string
				(playlist->_priv->model,
				 &iter, index) == FALSE)
		{
			g_free (index);
			continue;
		}
		g_free (index);

		gtk_tree_model_get (playlist->_priv->model, &iter,
				URI_COL, &mrl, -1);

		if ((* func) (mrl, data) != FALSE)
		{
			GtkTreePath *path;
			GtkTreeRowReference *ref;

			path = gtk_tree_path_new_from_indices (i, -1);
			ref = gtk_tree_row_reference_new
				(playlist->_priv->model, path);
			list = g_list_prepend (list, ref);
			gtk_tree_path_free (path);
		}

		g_free (mrl);
	}

	has_items = (list != NULL);

	for (l = list; l != NULL; l = l->next)
	{
		GtkTreePath *path;
		GtkTreeIter iter;

		path = gtk_tree_row_reference_get_path (l->data);
		gtk_tree_model_get_iter (playlist->_priv->model, &iter, path);
		gtk_list_store_remove (GTK_LIST_STORE (playlist->_priv->model),
				&iter);
		gtk_tree_path_free (path);
		gtk_tree_row_reference_free (l->data);
	}
	g_list_free (list);

	if (has_items != FALSE) {
		playlist->_priv->current_shuffled = -1;

		ensure_shuffled (playlist, playlist->_priv->shuffle);
		gtk_tree_path_free (playlist->_priv->current);
		playlist->_priv->current = NULL;
		g_signal_emit (G_OBJECT (playlist),
				totem_playlist_table_signals[CURRENT_REMOVED],
				0, NULL);
	}
}

static int
totem_playlist_compare_with_prefix (gconstpointer a, gconstpointer b)
{
	const char *mrl = (const char *) a;
	const char *prefix = (const char *) b;

	return g_str_has_prefix (mrl, prefix);
}

void
totem_playlist_clear_with_prefix (TotemPlaylist *playlist, const char *prefix)
{
	totem_playlist_clear_with_compare (playlist,
			(GCompareFunc) totem_playlist_compare_with_prefix,
			(void *)prefix);
}

static int
totem_playlist_compare_with_volume (gpointer a, gpointer b)
{
	GnomeVFSVolume *clear_volume = (GnomeVFSVolume *) b;
	const char *mrl = (const char *) a;

	char *filename;
	GnomeVFSVolume *volume;
	GnomeVFSVolumeMonitor *monitor;
	gboolean retval = FALSE;

	if (g_str_has_prefix (mrl, "file:///") == FALSE)
		return FALSE;

	monitor = gnome_vfs_get_volume_monitor ();

	filename = g_filename_from_uri (mrl, NULL, NULL);
	volume = gnome_vfs_volume_monitor_get_volume_for_path
		(monitor, filename);
	g_free (filename);

	if (volume == clear_volume)
		retval = TRUE;

	gnome_vfs_volume_unref (volume);

	return retval;
}

void
totem_playlist_clear_with_gnome_vfs_volume (TotemPlaylist *playlist,
		GnomeVFSVolume *volume)
{
	totem_playlist_clear_with_compare (playlist,
			(GCompareFunc) totem_playlist_compare_with_volume,
			volume);
}

char
*totem_playlist_get_current_mrl (TotemPlaylist *playlist)
{
	GtkTreeIter iter;
	char *path;

	g_return_val_if_fail (GTK_IS_PLAYLIST (playlist), NULL);

	if (update_current_from_playlist (playlist) == FALSE)
		return NULL;

	if (gtk_tree_model_get_iter (playlist->_priv->model, &iter,
			playlist->_priv->current) == FALSE)
		return NULL;

	gtk_tree_model_get (playlist->_priv->model,
			&iter,
			URI_COL, &path,
			-1);

	return path;
}

char
*totem_playlist_get_current_title (TotemPlaylist *playlist, gboolean *custom)
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
			FILENAME_COL, &path,
			TITLE_CUSTOM_COL, custom,
			-1);

	return path;
}

gboolean
totem_playlist_get_current_metadata (TotemPlaylist *playlist,
				     char         **artist,
				     char         **title,
				     char         **album)
{
	GtkTreeIter iter;

        g_return_val_if_fail (GTK_IS_PLAYLIST (playlist), FALSE);

	if (update_current_from_playlist (playlist) == FALSE)
		return FALSE;

	gtk_tree_model_get_iter (playlist->_priv->model,
			&iter,
			playlist->_priv->current);

	*artist = NULL;
	gtk_tree_model_get (playlist->_priv->model,
			&iter,
			CACHE_ARTIST_COL, artist,
			CACHE_TITLE_COL, title,
			CACHE_ALBUM_COL, album,
			-1);

	return (*artist != NULL);
}

gboolean
totem_playlist_has_previous_mrl (TotemPlaylist *playlist)
{
	GtkTreeIter iter;

	g_return_val_if_fail (GTK_IS_PLAYLIST (playlist), FALSE);

	if (update_current_from_playlist (playlist) == FALSE)
		return FALSE;

	if (playlist->_priv->repeat != FALSE)
		return TRUE;

	if (playlist->_priv->shuffle == FALSE)
	{
		gtk_tree_model_get_iter (playlist->_priv->model,
				&iter,
				playlist->_priv->current);

		return totem_playlist_gtk_tree_model_iter_previous
			(playlist->_priv->model, &iter);
	} else {
		if (playlist->_priv->current_shuffled == 0)
			return FALSE;
	}

	return TRUE;
}

gboolean
totem_playlist_has_next_mrl (TotemPlaylist *playlist)
{
	GtkTreeIter iter;

	g_return_val_if_fail (GTK_IS_PLAYLIST (playlist), FALSE);

	if (update_current_from_playlist (playlist) == FALSE)
		return FALSE;

	if (playlist->_priv->repeat != FALSE)
		return TRUE;

	if (playlist->_priv->shuffle == FALSE)
	{
		gtk_tree_model_get_iter (playlist->_priv->model,
				&iter,
				playlist->_priv->current);

		return gtk_tree_model_iter_next (playlist->_priv->model, &iter);
	} else {
		if (playlist->_priv->current_shuffled == PL_LEN - 1)
			return FALSE;
	}

	return TRUE;
}

gboolean
totem_playlist_set_title (TotemPlaylist *playlist, const char *title)
{
	GtkListStore *store;
	GtkTreeIter iter;
	gboolean custom_title;

	g_return_val_if_fail (GTK_IS_PLAYLIST (playlist), FALSE);

	if (update_current_from_playlist (playlist) == FALSE)
		return FALSE;

	store = GTK_LIST_STORE (playlist->_priv->model);
	gtk_tree_model_get_iter (playlist->_priv->model,
			&iter,
			playlist->_priv->current);

	if (&iter == NULL)
		return FALSE;

	gtk_tree_model_get (playlist->_priv->model, &iter,
			TITLE_CUSTOM_COL, &custom_title,
			-1);
	if (custom_title != FALSE)
		return TRUE;

	gtk_list_store_set (store, &iter,
			FILENAME_COL, title,
			TITLE_CUSTOM_COL, TRUE,
			-1);

	g_signal_emit (playlist,
		       totem_playlist_table_signals[ACTIVE_NAME_CHANGED], 0);

	return TRUE;
}

gboolean
totem_playlist_set_playing (TotemPlaylist *playlist, gboolean state)
{
	GtkListStore *store;
	GtkTreeIter iter;
	GtkTreePath *path;

	g_return_val_if_fail (GTK_IS_PLAYLIST (playlist), FALSE);

	if (update_current_from_playlist (playlist) == FALSE)
		return FALSE;

	store = GTK_LIST_STORE (playlist->_priv->model);
	gtk_tree_model_get_iter (playlist->_priv->model,
			&iter,
			playlist->_priv->current);

	g_return_val_if_fail (&iter != NULL, FALSE);

	if (state != FALSE)
		gtk_list_store_set (store, &iter,
				PIX_COL, playlist->_priv->icon,
				-1);
	else
		gtk_list_store_set (store, &iter,
				PIX_COL, NULL,
				-1);

	if (state == FALSE)
		return TRUE;

	path = gtk_tree_model_get_path (GTK_TREE_MODEL (store), &iter);
	gtk_tree_view_scroll_to_cell (GTK_TREE_VIEW (playlist->_priv->treeview),
				      path, NULL,
				      TRUE, 0.5, 0);
	gtk_tree_path_free (path);
	
	return TRUE;
}

void
totem_playlist_set_previous (TotemPlaylist *playlist)
{
	GtkTreeIter iter;

	g_return_if_fail (GTK_IS_PLAYLIST (playlist));

	if (totem_playlist_has_previous_mrl (playlist) == FALSE)
		return;

	totem_playlist_unset_playing (playlist);

	if (playlist->_priv->shuffle == FALSE)
	{
		char *path;

		path = gtk_tree_path_to_string (playlist->_priv->current);
		if (strcmp (path, "0") == 0)
		{
			totem_playlist_set_at_end (playlist);
			g_free (path);
			return;
		}
		g_free (path);

		gtk_tree_model_get_iter (playlist->_priv->model,
				&iter,
				playlist->_priv->current);

		totem_playlist_gtk_tree_model_iter_previous
			(playlist->_priv->model, &iter);
		gtk_tree_path_free (playlist->_priv->current);
		playlist->_priv->current = gtk_tree_model_get_path
			(playlist->_priv->model, &iter);
	} else {
		int indice;

		gtk_tree_path_free (playlist->_priv->current);
		playlist->_priv->current_shuffled--;
		if (playlist->_priv->current_shuffled < 0) {
			indice = playlist->_priv->shuffled[PL_LEN -1];
			playlist->_priv->current_shuffled = PL_LEN -1;
		} else {
			indice = playlist->_priv->shuffled[playlist->_priv->current_shuffled];
		}
		playlist->_priv->current = gtk_tree_path_new_from_indices
			(indice, -1);
	}
}

void
totem_playlist_set_next (TotemPlaylist *playlist)
{
	GtkTreeIter iter;

	g_return_if_fail (GTK_IS_PLAYLIST (playlist));

	if (totem_playlist_has_next_mrl (playlist) == FALSE)
	{
		totem_playlist_set_at_start (playlist);
		return;
	}

	totem_playlist_unset_playing (playlist);

	if (playlist->_priv->shuffle == FALSE)
	{
		gtk_tree_model_get_iter (playlist->_priv->model,
				&iter,
				playlist->_priv->current);

		gtk_tree_model_iter_next (playlist->_priv->model, &iter);
		gtk_tree_path_free (playlist->_priv->current);
		playlist->_priv->current = gtk_tree_model_get_path
			(playlist->_priv->model, &iter);
	} else {
		int indice;

		gtk_tree_path_free (playlist->_priv->current);
		playlist->_priv->current_shuffled++;
		if (playlist->_priv->current_shuffled == PL_LEN)
			playlist->_priv->current_shuffled = 0;
		indice = playlist->_priv->shuffled[playlist->_priv->current_shuffled];
		playlist->_priv->current = gtk_tree_path_new_from_indices
			                        (indice, -1);
	}
}

gboolean
totem_playlist_get_repeat (TotemPlaylist *playlist)
{
	g_return_val_if_fail (GTK_IS_PLAYLIST (playlist), FALSE);

	return playlist->_priv->repeat;
}
	
void
totem_playlist_set_repeat (TotemPlaylist *playlist, gboolean repeat)
{
	g_return_if_fail (GTK_IS_PLAYLIST (playlist));

	gconf_client_set_bool (playlist->_priv->gc, GCONF_PREFIX"/repeat",
			repeat, NULL);
}

gboolean
totem_playlist_get_shuffle (TotemPlaylist *playlist)
{
	g_return_val_if_fail (GTK_IS_PLAYLIST (playlist), FALSE);

	return playlist->_priv->shuffle;
}

void
totem_playlist_set_shuffle (TotemPlaylist *playlist, gboolean shuffle)
{
	g_return_if_fail (GTK_IS_PLAYLIST (playlist));

	gconf_client_set_bool (playlist->_priv->gc, GCONF_PREFIX"/shuffle",
			shuffle, NULL);
}

void
totem_playlist_set_at_start (TotemPlaylist *playlist)
{
	g_return_if_fail (GTK_IS_PLAYLIST (playlist));

	totem_playlist_unset_playing (playlist);

	if (playlist->_priv->current != NULL)
	{
		gtk_tree_path_free (playlist->_priv->current);
		playlist->_priv->current = NULL;
	}
	update_current_from_playlist (playlist);
}

void
totem_playlist_set_at_end (TotemPlaylist *playlist)
{
	int indice;

	g_return_if_fail (GTK_IS_PLAYLIST (playlist));

	totem_playlist_unset_playing (playlist);

	if (playlist->_priv->current != NULL)
	{
		gtk_tree_path_free (playlist->_priv->current);
		playlist->_priv->current = NULL;
	}

	if (PL_LEN)
	{
		if (playlist->_priv->shuffle == FALSE)
			indice = PL_LEN - 1;
		else
			indice = playlist->_priv->shuffled[PL_LEN - 1];

		playlist->_priv->current = gtk_tree_path_new_from_indices
			(indice, -1);
	}
}

guint
totem_playlist_get_current (TotemPlaylist *playlist)
{
	char *path;
	double index;

	g_return_val_if_fail (GTK_IS_PLAYLIST (playlist), 0);

	path = gtk_tree_path_to_string (playlist->_priv->current);
	if (path == NULL)
		return -1;

	index = g_ascii_strtod (path, NULL);
	g_free (path);

	return index;
}

guint
totem_playlist_get_last (TotemPlaylist *playlist)
{
	g_return_val_if_fail (GTK_IS_PLAYLIST (playlist), -1);

	return PL_LEN - 1;
}

void
totem_playlist_set_current (TotemPlaylist *playlist, guint index)
{
	g_return_if_fail (GTK_IS_PLAYLIST (playlist));

	if (index >= (guint) PL_LEN)
		return;

	totem_playlist_unset_playing (playlist);
	//FIXME problems when shuffled?
	gtk_tree_path_free (playlist->_priv->current);
	playlist->_priv->current = gtk_tree_path_new_from_indices (index, -1);
}

static void
totem_playlist_class_init (TotemPlaylistClass *klass)
{
	parent_class = gtk_type_class (gtk_dialog_get_type ());

	G_OBJECT_CLASS (klass)->finalize = totem_playlist_finalize;

	/* Signals */
	totem_playlist_table_signals[CHANGED] =
		g_signal_new ("changed",
				G_TYPE_FROM_CLASS (klass),
				G_SIGNAL_RUN_LAST,
				G_STRUCT_OFFSET (TotemPlaylistClass, changed),
				NULL, NULL,
				g_cclosure_marshal_VOID__VOID,
				G_TYPE_NONE, 0);
	totem_playlist_table_signals[ITEM_ACTIVATED] =
		g_signal_new ("item-activated",
				G_TYPE_FROM_CLASS (klass),
				G_SIGNAL_RUN_LAST,
				G_STRUCT_OFFSET (TotemPlaylistClass, item_activated),
				NULL, NULL,
				g_cclosure_marshal_VOID__VOID,
				G_TYPE_NONE, 0);
	totem_playlist_table_signals[ACTIVE_NAME_CHANGED] =
		g_signal_new ("active-name-changed",
				G_TYPE_FROM_CLASS (klass),
				G_SIGNAL_RUN_LAST,
				G_STRUCT_OFFSET (TotemPlaylistClass, active_name_changed),
				NULL, NULL,
				g_cclosure_marshal_VOID__VOID,
				G_TYPE_NONE, 0);
	totem_playlist_table_signals[CURRENT_REMOVED] =
		g_signal_new ("current-removed",
				G_TYPE_FROM_CLASS (klass),
				G_SIGNAL_RUN_LAST,
				G_STRUCT_OFFSET (TotemPlaylistClass,
					current_removed),
				NULL, NULL,
				g_cclosure_marshal_VOID__VOID,
				G_TYPE_NONE, 0);
	totem_playlist_table_signals[REPEAT_TOGGLED] =
		g_signal_new ("repeat-toggled",
				G_TYPE_FROM_CLASS (klass),
				G_SIGNAL_RUN_LAST,
				G_STRUCT_OFFSET (TotemPlaylistClass,
					repeat_toggled),
				NULL, NULL,
				g_cclosure_marshal_VOID__BOOLEAN,
				G_TYPE_NONE, 1, G_TYPE_BOOLEAN);
	totem_playlist_table_signals[SHUFFLE_TOGGLED] =
		g_signal_new ("shuffle-toggled",
				G_TYPE_FROM_CLASS (klass),
				G_SIGNAL_RUN_LAST,
				G_STRUCT_OFFSET (TotemPlaylistClass,
					shuffle_toggled),
				NULL, NULL,
				g_cclosure_marshal_VOID__BOOLEAN,
				G_TYPE_NONE, 1, G_TYPE_BOOLEAN);
}

