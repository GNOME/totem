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
   write to the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301  USA.

   Author: Bastien Nocera <hadess@hadess.net>
 */

#include "config.h"
#include "totem-playlist.h"

#include <stdlib.h>
#include <string.h>
#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <gdk/gdkkeysyms.h>
#include <gio/gio.h>

#include "totem-uri.h"
#include "totem-interface.h"

#define PL_LEN (gtk_tree_model_iter_n_children (playlist->priv->model, NULL))

static gboolean totem_playlist_add_one_mrl (TotemPlaylist *playlist,
					    const char    *mrl,
					    const char    *display_name,
					    const char    *content_type,
					    const char    *subtitle_uri,
					    gint64         starttime,
					    gboolean       playing);

typedef gboolean (*ClearComparisonFunc) (TotemPlaylist *playlist, GtkTreeIter *iter, gconstpointer data);

static void totem_playlist_clear_with_compare (TotemPlaylist *playlist,
					       ClearComparisonFunc func,
					       gconstpointer data);

/* Callback function for GtkBuilder */
G_MODULE_EXPORT void totem_playlist_add_files (GtkWidget *widget, TotemPlaylist *playlist);
G_MODULE_EXPORT void playlist_remove_button_clicked (GtkWidget *button, TotemPlaylist *playlist);
G_MODULE_EXPORT void playlist_copy_location_action_callback (GtkWidget *button, TotemPlaylist *playlist);
G_MODULE_EXPORT void playlist_select_subtitle_action_callback (GtkWidget *button, TotemPlaylist *playlist);


typedef struct {
	TotemPlaylist *playlist;
	TotemPlaylistForeachFunc callback;
	gpointer user_data;
} PlaylistForeachContext;

struct TotemPlaylistPrivate
{
	GtkWidget *treeview;
	GtkTreeModel *model;
	GtkTreePath *current;
	GtkTreeSelection *selection;
	TotemPlParser *parser;

	/* Widgets */
	GtkWidget *remove_button;

	GSettings *settings;
	GSettings *lockdown_settings;

	/* This is a scratch list for when we're removing files */
	GList *list;
	guint current_to_be_removed : 1;

	guint disable_save_to_disk : 1;

	/* Repeat mode */
	guint repeat : 1;
};

/* Signals */
enum {
	CHANGED,
	ITEM_ACTIVATED,
	ACTIVE_NAME_CHANGED,
	CURRENT_REMOVED,
	SUBTITLE_CHANGED,
	ITEM_ADDED,
	ITEM_REMOVED,
	LAST_SIGNAL
};

enum {
	PLAYING_COL,
	FILENAME_COL,
	FILENAME_ESCAPED_COL,
	URI_COL,
	TITLE_CUSTOM_COL,
	SUBTITLE_URI_COL,
	FILE_MONITOR_COL,
	MOUNT_COL,
	MIME_TYPE_COL,
	STARTTIME_COL,
	NUM_COLS
};

enum {
	PROP_0,
	PROP_REPEAT
};

typedef struct {
	const char *name;
	const char *suffix;
	TotemPlParserType type;
} PlaylistSaveType;

static int totem_playlist_table_signals[LAST_SIGNAL];

static void init_treeview (GtkWidget *treeview, TotemPlaylist *playlist);

#define totem_playlist_unset_playing(x) totem_playlist_set_playing(x, TOTEM_PLAYLIST_STATUS_NONE)

G_DEFINE_TYPE (TotemPlaylist, totem_playlist, GTK_TYPE_BOX)

/* Helper functions */
void
totem_playlist_select_subtitle_dialog(TotemPlaylist *playlist, TotemPlaylistSelectDialog mode)
{
	char *subtitle, *current, *uri;
	GFile *file, *dir;
	TotemPlaylistStatus playing;
	GtkTreeIter iter;

	if (mode == TOTEM_PLAYLIST_DIALOG_PLAYING) {
		/* Set subtitle file for the currently playing movie */
		gtk_tree_model_get_iter (playlist->priv->model, &iter, playlist->priv->current);
	} else if (mode == TOTEM_PLAYLIST_DIALOG_SELECTED) {
		/* Set subtitle file in for the first selected playlist item */
		GList *l;

		l = gtk_tree_selection_get_selected_rows (playlist->priv->selection, NULL);
		if (l == NULL)
			return;
		gtk_tree_model_get_iter (playlist->priv->model, &iter, l->data);
		g_list_free_full (l, (GDestroyNotify) gtk_tree_path_free);
	} else {
		g_assert_not_reached ();
	}

	/* Look for the directory of the current movie */
	gtk_tree_model_get (playlist->priv->model, &iter,
			    URI_COL, &current,
			    -1);

	if (current == NULL)
		return;

	uri = NULL;
	file = g_file_new_for_uri (current);
	dir = g_file_get_parent (file);
	g_object_unref (file);
	if (dir != NULL) {
		uri = g_file_get_uri (dir);
		g_object_unref (dir);
	}

	subtitle = totem_add_subtitle (NULL, uri);
	g_free (uri);

	if (subtitle == NULL)
		return;

	gtk_tree_model_get (playlist->priv->model, &iter,
			    PLAYING_COL, &playing,
			    -1);

	gtk_list_store_set (GTK_LIST_STORE(playlist->priv->model), &iter,
			    SUBTITLE_URI_COL, subtitle,
			    -1);

	if (playing != TOTEM_PLAYLIST_STATUS_NONE) {
		g_signal_emit (G_OBJECT (playlist),
			       totem_playlist_table_signals[SUBTITLE_CHANGED], 0,
			       NULL);
	}

	g_free(subtitle);
}

void
totem_playlist_set_current_subtitle (TotemPlaylist *playlist, const char *subtitle_uri)
{
	GtkTreeIter iter;

	if (playlist->priv->current == NULL)
		return;

	gtk_tree_model_get_iter (playlist->priv->model, &iter, playlist->priv->current);

	gtk_list_store_set (GTK_LIST_STORE(playlist->priv->model), &iter,
			    SUBTITLE_URI_COL, subtitle_uri,
			    -1);

	g_signal_emit (G_OBJECT (playlist),
		       totem_playlist_table_signals[SUBTITLE_CHANGED], 0,
		       NULL);
}

/* This one returns a new string, in UTF8 even if the MRL is encoded
 * in the locale's encoding
 */
static char *
totem_playlist_mrl_to_title (const gchar *mrl)
{
	GFile *file;
	char *filename_for_display, *unescaped;

	if (g_str_has_prefix (mrl, "dvd://") != FALSE) {
		/* This is "Title 3", where title is a DVD title
		 * Note: NOT a DVD chapter */
		return g_strdup_printf (_("Title %d"), (int) g_strtod (mrl + 6, NULL));
	} else if (g_str_has_prefix (mrl, "dvb://") != FALSE) {
		/* This is "BBC ONE(BBC)" for "dvb://BBC ONE(BBC)" */
		return g_strdup (mrl + 6);
	}

	file = g_file_new_for_uri (mrl);
	unescaped = g_file_get_basename (file);
	g_object_unref (file);

	filename_for_display = g_filename_to_utf8 (unescaped,
			-1,             /* length */
			NULL,           /* bytes_read */
			NULL,           /* bytes_written */
			NULL);          /* error */

	if (filename_for_display == NULL)
	{
		filename_for_display = g_locale_to_utf8 (unescaped,
				-1, NULL, NULL, NULL);
		if (filename_for_display == NULL) {
			filename_for_display = g_filename_display_name
				(unescaped);
		}
		g_free (unescaped);
		return filename_for_display;
	}

	g_free (unescaped);

	return filename_for_display;
}

static gboolean
totem_playlist_save_iter_foreach (GtkTreeModel *model,
				  GtkTreePath  *path,
				  GtkTreeIter  *iter,
				  gpointer      user_data)
{
	TotemPlPlaylist *playlist = user_data;
	TotemPlPlaylistIter pl_iter;
	gchar *uri, *title, *subtitle_uri, *mime_type, *starttime_str;
	TotemPlaylistStatus status;
	gboolean custom_title;
	gint64 starttime;

	gtk_tree_model_get (model, iter,
			    URI_COL, &uri,
			    FILENAME_COL, &title,
			    TITLE_CUSTOM_COL, &custom_title,
			    SUBTITLE_URI_COL, &subtitle_uri,
			    PLAYING_COL, &status,
			    MIME_TYPE_COL, &mime_type,
			    STARTTIME_COL, &starttime,
			    -1);

	/* Prefer the current position for the starttime, if one is passed */
	starttime_str = NULL;
	if (status != TOTEM_PLAYLIST_STATUS_NONE) {
		gint64 new_starttime;

		new_starttime = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (playlist), "starttime"));
		if (new_starttime != 0)
			starttime = new_starttime;
	}
	if (starttime != 0)
		starttime_str = g_strdup_printf ("%" G_GINT64_FORMAT, starttime);

	totem_pl_playlist_append (playlist, &pl_iter);
	totem_pl_playlist_set (playlist, &pl_iter,
			       TOTEM_PL_PARSER_FIELD_URI, uri,
			       TOTEM_PL_PARSER_FIELD_TITLE, (custom_title) ? title : NULL,
			       TOTEM_PL_PARSER_FIELD_SUBTITLE_URI, subtitle_uri,
			       TOTEM_PL_PARSER_FIELD_PLAYING, status != TOTEM_PLAYLIST_STATUS_NONE ? "true" : "",
			       TOTEM_PL_PARSER_FIELD_CONTENT_TYPE, mime_type,
			       TOTEM_PL_PARSER_FIELD_STARTTIME, starttime_str,
			       NULL);

	g_free (uri);
	g_free (title);
	g_free (subtitle_uri);
	g_free (mime_type);
	g_free (starttime_str);

	return FALSE;
}

void
totem_playlist_save_session_playlist (TotemPlaylist *playlist,
				      GFile         *output,
				      gint64         starttime)
{
	TotemPlPlaylist *pl_playlist;
	GError *error = NULL;
	gboolean retval;

	if (playlist->priv->disable_save_to_disk) {
		/* On lockdown, we do not touch the disk,
		 * even to remove the existing session */
		return;
	}
	if (PL_LEN == 0) {
		g_file_delete (output, NULL, NULL);
		return;
	}

	pl_playlist = totem_pl_playlist_new ();

	if (starttime > 0)
		g_object_set_data (G_OBJECT (pl_playlist), "starttime", GINT_TO_POINTER (starttime));

	gtk_tree_model_foreach (playlist->priv->model,
				totem_playlist_save_iter_foreach,
				pl_playlist);

	retval = totem_pl_parser_save (playlist->priv->parser,
				       pl_playlist,
				       output,
				       NULL, TOTEM_PL_PARSER_XSPF, &error);

	if (retval == FALSE) {
		g_warning ("Failed to save the session playlist: %s", error->message);
		g_error_free (error);
	}

	g_object_unref (pl_playlist);
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

void
playlist_select_subtitle_action_callback (GtkWidget *button, TotemPlaylist *playlist)
{
	totem_playlist_select_subtitle_dialog (playlist, TOTEM_PLAYLIST_DIALOG_SELECTED);
}

void
playlist_copy_location_action_callback (GtkWidget *button, TotemPlaylist *playlist)
{
	GList *l;
	GtkClipboard *clip;
	char *url;
	GtkTreeIter iter;

	l = gtk_tree_selection_get_selected_rows (playlist->priv->selection, NULL);
	if (l == NULL)
		return;

	gtk_tree_model_get_iter (playlist->priv->model, &iter, l->data);
	g_list_free_full (l, (GDestroyNotify) gtk_tree_path_free);

	gtk_tree_model_get (playlist->priv->model,
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

}

static void
selection_changed (GtkTreeSelection *treeselection, TotemPlaylist *playlist)
{
	gboolean sensitivity;

	if (gtk_tree_selection_has_selected (treeselection))
		sensitivity = TRUE;
	else
		sensitivity = FALSE;

	gtk_widget_set_sensitive (playlist->priv->remove_button, sensitivity);
}

/* This function checks if the current item is NULL, and try to update it
 * as the first item of the playlist if so. It returns TRUE if there is a
 * current item */
static gboolean
update_current_from_playlist (TotemPlaylist *playlist)
{
	int indice;

	if (playlist->priv->current != NULL)
		return TRUE;

	if (PL_LEN != 0)
	{
		indice = 0;
		playlist->priv->current = gtk_tree_path_new_from_indices (indice, -1);
	} else {
		return FALSE;
	}

	return TRUE;
}

void
totem_playlist_add_files (GtkWidget *widget, TotemPlaylist *playlist)
{
	GSList *filenames, *l;
	GList *mrl_list = NULL;

	filenames = totem_add_files (NULL, NULL);
	if (filenames == NULL)
		return;

	for (l = filenames; l != NULL; l = l->next) {
		char *mrl = l->data;
		mrl_list = g_list_prepend (mrl_list, totem_playlist_mrl_data_new (mrl, NULL));
		g_free (mrl);
	}

	g_slist_free (filenames);

	if (mrl_list != NULL)
		totem_playlist_add_mrls (playlist, g_list_reverse (mrl_list), TRUE, NULL, NULL, NULL);
}

static void
totem_playlist_foreach_selected (GtkTreeModel *model, GtkTreePath *path,
		GtkTreeIter *iter, gpointer data)
{
	TotemPlaylist *playlist = (TotemPlaylist *)data;
	GtkTreeRowReference *ref;

	/* We can't use gtk_list_store_remove() here
	 * So we build a list a RowReferences */
	ref = gtk_tree_row_reference_new (playlist->priv->model, path);
	playlist->priv->list = g_list_prepend
		(playlist->priv->list, (gpointer) ref);
	if (playlist->priv->current_to_be_removed == FALSE
	    && playlist->priv->current != NULL
	    && gtk_tree_path_compare (path, playlist->priv->current) == 0)
		playlist->priv->current_to_be_removed = TRUE;
}

static void
totem_playlist_emit_item_removed (TotemPlaylist *playlist,
				  GtkTreeIter   *iter)
{
	gchar *filename = NULL;
	gchar *uri = NULL;

	gtk_tree_model_get (playlist->priv->model, iter,
			    URI_COL, &uri, FILENAME_COL, &filename, -1);

	g_signal_emit (playlist,
		       totem_playlist_table_signals[ITEM_REMOVED],
		       0, filename, uri);

	g_free (filename);
	g_free (uri);
}

static void
playlist_remove_files (TotemPlaylist *playlist)
{
	totem_playlist_clear_with_compare (playlist, NULL, NULL);
}

void
playlist_remove_button_clicked (GtkWidget *button, TotemPlaylist *playlist)
{
	playlist_remove_files (playlist);
}

static int
totem_playlist_key_press (GtkWidget *win, GdkEventKey *event, TotemPlaylist *playlist)
{
	/* Special case some shortcuts */
	if (event->state != 0) {
		if ((event->state & GDK_CONTROL_MASK)
		    && event->keyval == GDK_KEY_a) {
			gtk_tree_selection_select_all
				(playlist->priv->selection);
			return TRUE;
		}
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

	if (event->keyval == GDK_KEY_Delete)
	{
		playlist_remove_files (playlist);
		return TRUE;
	}

	return FALSE;
}

static void
set_playing_icon (GtkTreeViewColumn *column, GtkCellRenderer *renderer,
		  GtkTreeModel *model, GtkTreeIter *iter, TotemPlaylist *playlist)
{
	TotemPlaylistStatus playing;
	const char *icon_name;

	gtk_tree_model_get (model, iter, PLAYING_COL, &playing, -1);

	switch (playing) {
		case TOTEM_PLAYLIST_STATUS_PLAYING:
			icon_name = "media-playback-start-symbolic";
			break;
		case TOTEM_PLAYLIST_STATUS_PAUSED:
			icon_name = "media-playback-pause-symbolic";
			break;
		case TOTEM_PLAYLIST_STATUS_NONE:
		default:
			icon_name = NULL;
	}

	g_object_set (renderer, "icon-name", icon_name, NULL);
}

static void
init_columns (GtkTreeView *treeview, TotemPlaylist *playlist)
{
	GtkCellRenderer *renderer;
	GtkTreeViewColumn *column;

	/* Playing pix */
	renderer = gtk_cell_renderer_pixbuf_new ();
	column = gtk_tree_view_column_new ();
	g_object_set (G_OBJECT (column), "title", "Playlist", NULL);
	gtk_tree_view_column_pack_start (column, renderer, FALSE);
	gtk_tree_view_column_set_cell_data_func (column, renderer,
			(GtkTreeCellDataFunc) set_playing_icon, playlist, NULL);
	g_object_set (renderer, "stock-size", GTK_ICON_SIZE_MENU, NULL);
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
	if (gtk_tree_path_compare (arg1, playlist->priv->current) == 0) {
		g_signal_emit (G_OBJECT (playlist),
				totem_playlist_table_signals[ITEM_ACTIVATED], 0,
				NULL);
		return;
	}

	if (playlist->priv->current != NULL) {
		totem_playlist_unset_playing (playlist);
		gtk_tree_path_free (playlist->priv->current);
	}

	playlist->priv->current = gtk_tree_path_copy (arg1);

	g_signal_emit (G_OBJECT (playlist),
			totem_playlist_table_signals[CHANGED], 0,
			NULL);
}

static gboolean
search_equal_is_match (const gchar * s, const gchar * lc_key)
{
	gboolean match = FALSE;

	if (s != NULL) {
		gchar *lc_s;

		/* maybe also normalize both strings? */
		lc_s = g_utf8_strdown (s, -1);
		match = (lc_s != NULL && strstr (lc_s, lc_key) != NULL);
		g_free (lc_s);
	}

	return match;
}

static gboolean
search_equal_func (GtkTreeModel *model, gint col, const gchar *key,
                   GtkTreeIter *iter, gpointer userdata)
{
	gboolean match;
	gchar *lc_key, *fn = NULL;

	lc_key = g_utf8_strdown (key, -1);

        /* type-ahead search: first check display filename / title, then URI */
	gtk_tree_model_get (model, iter, FILENAME_COL, &fn, -1);
	match = search_equal_is_match (fn, lc_key);
	g_free (fn);

	if (!match) {
		gchar *uri = NULL;

		gtk_tree_model_get (model, iter, URI_COL, &uri, -1);
		fn = g_filename_from_uri (uri, NULL, NULL);
		match = search_equal_is_match (fn, lc_key);
		g_free (fn);
		g_free (uri);
	}

	g_free (lc_key);
	return !match; /* needs to return FALSE if row matches */
}

static void
init_treeview (GtkWidget *treeview, TotemPlaylist *playlist)
{
	GtkTreeSelection *selection;

	init_columns (GTK_TREE_VIEW (treeview), playlist);

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (treeview));
	gtk_tree_selection_set_mode (selection, GTK_SELECTION_MULTIPLE);
	g_signal_connect (G_OBJECT (selection), "changed",
			G_CALLBACK (selection_changed), playlist);
	g_signal_connect (G_OBJECT (treeview), "row-activated",
			G_CALLBACK (treeview_row_changed), playlist);

	playlist->priv->selection = selection;

	/* make type-ahead search work in the playlist */
	gtk_tree_view_set_search_equal_func (GTK_TREE_VIEW (treeview),
	                                     search_equal_func, NULL, NULL);

	gtk_widget_show (treeview);
}

static void
update_repeat_cb (GSettings *settings, const gchar *key, TotemPlaylist *playlist)
{
	playlist->priv->repeat = g_settings_get_boolean (settings, "repeat");

	g_signal_emit (G_OBJECT (playlist),
			totem_playlist_table_signals[CHANGED], 0,
			NULL);
	g_object_notify (G_OBJECT (playlist), "repeat");
}

static void
update_lockdown_cb (GSettings *settings, const gchar *key, TotemPlaylist *playlist)
{
	playlist->priv->disable_save_to_disk = g_settings_get_boolean (settings, "disable-save-to-disk");
}

static void
init_config (TotemPlaylist *playlist)
{
	playlist->priv->settings = g_settings_new (TOTEM_GSETTINGS_SCHEMA);
	playlist->priv->lockdown_settings = g_settings_new ("org.gnome.desktop.lockdown");

	playlist->priv->disable_save_to_disk = g_settings_get_boolean (playlist->priv->lockdown_settings, "disable-save-to-disk");

	g_signal_connect (playlist->priv->lockdown_settings, "changed::disable-save-to-disk",
			  G_CALLBACK (update_lockdown_cb), playlist);

	playlist->priv->repeat = g_settings_get_boolean (playlist->priv->settings, "repeat");

	g_signal_connect (playlist->priv->settings, "changed::repeat", (GCallback) update_repeat_cb, playlist);
}

static gboolean
parse_bool_str (const char *str)
{
	if (str == NULL)
		return FALSE;
	if (g_ascii_strcasecmp (str, "true") == 0)
		return TRUE;
	if (g_ascii_strcasecmp (str, "false") == 0)
		return FALSE;
	return atoi (str);
}

static void
totem_playlist_entry_parsed (TotemPlParser *parser,
			     const char *uri,
			     GHashTable *metadata,
			     TotemPlaylist *playlist)
{
	const char *title, *content_type, *subtitle_uri, *starttime_str;
	gint64 duration, starttime;
	gboolean playing;

	/* We ignore 0-length items in playlists, they're usually just banners */
	duration = totem_pl_parser_parse_duration
		(g_hash_table_lookup (metadata, TOTEM_PL_PARSER_FIELD_DURATION), FALSE);
	if (duration == 0)
		return;
	title = g_hash_table_lookup (metadata, TOTEM_PL_PARSER_FIELD_TITLE);
	content_type = g_hash_table_lookup (metadata, TOTEM_PL_PARSER_FIELD_CONTENT_TYPE);
	playing = parse_bool_str (g_hash_table_lookup (metadata, TOTEM_PL_PARSER_FIELD_PLAYING));
	subtitle_uri = g_hash_table_lookup (metadata, TOTEM_PL_PARSER_FIELD_SUBTITLE_URI);
	starttime_str = g_hash_table_lookup (metadata, TOTEM_PL_PARSER_FIELD_STARTTIME);
	starttime = totem_pl_parser_parse_duration (starttime_str, FALSE);
	starttime = MAX (starttime, 0);
	totem_playlist_add_one_mrl (playlist, uri, title, content_type, subtitle_uri, starttime, playing);
}

static gboolean
totem_playlist_compare_with_monitor (TotemPlaylist *playlist, GtkTreeIter *iter, gconstpointer data)
{
	GFileMonitor *monitor = (GFileMonitor *) data;
	GFileMonitor *_monitor;
	gboolean retval = FALSE;

	gtk_tree_model_get (playlist->priv->model, iter,
			    FILE_MONITOR_COL, &_monitor, -1);

	if (_monitor == monitor)
		retval = TRUE;

	if (_monitor != NULL)
		g_object_unref (_monitor);

	return retval;
}

static void
totem_playlist_file_changed (GFileMonitor *monitor,
			     GFile *file,
			     GFile *other_file,
			     GFileMonitorEvent event_type,
			     TotemPlaylist *playlist)
{
	if (event_type == G_FILE_MONITOR_EVENT_PRE_UNMOUNT ||
	    event_type == G_FILE_MONITOR_EVENT_UNMOUNTED) {
		totem_playlist_clear_with_compare (playlist,
						   (ClearComparisonFunc) totem_playlist_compare_with_monitor,
						   monitor);
	}
}

static void
totem_playlist_dispose (GObject *object)
{
	TotemPlaylist *playlist = TOTEM_PLAYLIST (object);

	g_clear_object (&playlist->priv->parser);
	g_clear_object (&playlist->priv->settings);
	g_clear_object (&playlist->priv->lockdown_settings);
	g_clear_pointer (&playlist->priv->current, gtk_tree_path_free);

	G_OBJECT_CLASS (totem_playlist_parent_class)->dispose (object);
}

static void
totem_playlist_init (TotemPlaylist *playlist)
{
	GtkWidget *container;
	GtkBuilder *xml;
	GtkWidget *widget;
	GtkStyleContext *context;

	gtk_orientable_set_orientation (GTK_ORIENTABLE (playlist),
					GTK_ORIENTATION_VERTICAL);

	playlist->priv = G_TYPE_INSTANCE_GET_PRIVATE (playlist, TOTEM_TYPE_PLAYLIST, TotemPlaylistPrivate);
	playlist->priv->parser = totem_pl_parser_new ();

	totem_pl_parser_add_ignored_scheme (playlist->priv->parser, "dvd:");
	totem_pl_parser_add_ignored_scheme (playlist->priv->parser, "vcd:");
	totem_pl_parser_add_ignored_scheme (playlist->priv->parser, "cd:");
	totem_pl_parser_add_ignored_scheme (playlist->priv->parser, "dvb:");
	totem_pl_parser_add_ignored_mimetype (playlist->priv->parser, "application/x-trash");

	g_signal_connect (G_OBJECT (playlist->priv->parser),
			"entry-parsed",
			G_CALLBACK (totem_playlist_entry_parsed),
			playlist);

	xml = totem_interface_load ("playlist.ui", TRUE, NULL, playlist);

	if (xml == NULL)
		return;

	gtk_widget_add_events (GTK_WIDGET (playlist), GDK_KEY_PRESS_MASK);
	g_signal_connect (G_OBJECT (playlist), "key_press_event",
			  G_CALLBACK (totem_playlist_key_press), playlist);

	/* Buttons */
	playlist->priv->remove_button = GTK_WIDGET (gtk_builder_get_object (xml, "remove_button"));

	/* Join treeview and buttons */
	widget = GTK_WIDGET (gtk_builder_get_object (xml, ("scrolledwindow1")));
	context = gtk_widget_get_style_context (widget);
	gtk_style_context_set_junction_sides (context, GTK_JUNCTION_BOTTOM);
	widget = GTK_WIDGET (gtk_builder_get_object (xml, ("toolbar1")));
	context = gtk_widget_get_style_context (widget);
	gtk_style_context_set_junction_sides (context, GTK_JUNCTION_TOP);

	/* Reparent the vbox */
	container = GTK_WIDGET (gtk_builder_get_object (xml, "vbox4"));
	g_object_ref (container);
	gtk_box_pack_start (GTK_BOX (playlist),
			container,
			TRUE,       /* expand */
			TRUE,       /* fill */
			0);         /* padding */
	g_object_unref (container);

	playlist->priv->treeview = GTK_WIDGET (gtk_builder_get_object (xml, "treeview1"));
	init_treeview (playlist->priv->treeview, playlist);
	playlist->priv->model = gtk_tree_view_get_model
		(GTK_TREE_VIEW (playlist->priv->treeview));

	/* tooltips */
	gtk_tree_view_set_tooltip_column(GTK_TREE_VIEW(playlist->priv->treeview),
					 FILENAME_ESCAPED_COL);

	/* The configuration */
	init_config (playlist);

	gtk_widget_show_all (GTK_WIDGET (playlist));

	g_object_unref (xml);
}

GtkWidget*
totem_playlist_new (void)
{
	return GTK_WIDGET (g_object_new (TOTEM_TYPE_PLAYLIST, NULL));
}

static gboolean
totem_playlist_add_one_mrl (TotemPlaylist *playlist,
			    const char *mrl,
			    const char *display_name,
			    const char *content_type,
			    const char *subtitle_uri,
			    gint64      starttime,
			    gboolean    playing)
{
	GtkListStore *store;
	GtkTreeIter iter;
	char *filename_for_display, *uri, *escaped_filename;
	GFileMonitor *monitor;
	GMount *mount;
	GFile *file;

	g_return_val_if_fail (TOTEM_IS_PLAYLIST (playlist), FALSE);
	g_return_val_if_fail (mrl != NULL, FALSE);

	if (display_name == NULL || *display_name == '\0')
		filename_for_display = totem_playlist_mrl_to_title (mrl);
	else
		filename_for_display = g_strdup (display_name);

	uri = totem_create_full_path (mrl);

	g_debug ("totem_playlist_add_one_mrl (): %s %s %s %s %"G_GINT64_FORMAT " %s\n", filename_for_display, uri, display_name, subtitle_uri, starttime, playing ? "true" : "false");

	store = GTK_LIST_STORE (playlist->priv->model);

	/* Get the file monitor */
	file = g_file_new_for_uri (uri ? uri : mrl);
	if (g_file_is_native (file) != FALSE) {
		monitor = g_file_monitor_file (file,
					       G_FILE_MONITOR_NONE,
					       NULL,
					       NULL);
		g_signal_connect (G_OBJECT (monitor),
				  "changed",
				  G_CALLBACK (totem_playlist_file_changed),
				  playlist);
		mount = NULL;
	} else {
		mount = totem_get_mount_for_media (uri ? uri : mrl);
		monitor = NULL;
	}

	escaped_filename = g_markup_escape_text (filename_for_display, -1);
	gtk_list_store_insert_with_values (store, &iter, -1,
					   PLAYING_COL, playing ? TOTEM_PLAYLIST_STATUS_PAUSED : TOTEM_PLAYLIST_STATUS_NONE,
					   FILENAME_COL, filename_for_display,
					   FILENAME_ESCAPED_COL, escaped_filename,
					   URI_COL, uri ? uri : mrl,
					   SUBTITLE_URI_COL, subtitle_uri,
					   TITLE_CUSTOM_COL, display_name ? TRUE : FALSE,
					   FILE_MONITOR_COL, monitor,
					   MOUNT_COL, mount,
					   MIME_TYPE_COL, content_type,
					   STARTTIME_COL, starttime,
					   -1);
	g_free (escaped_filename);

	g_signal_emit (playlist,
		       totem_playlist_table_signals[ITEM_ADDED],
		       0, filename_for_display, uri ? uri : mrl);

	g_free (filename_for_display);
	g_free (uri);

	if (playlist->priv->current == NULL)
		playlist->priv->current = gtk_tree_model_get_path (playlist->priv->model, &iter);

	g_signal_emit (G_OBJECT (playlist),
			totem_playlist_table_signals[CHANGED], 0,
			NULL);

	return TRUE;
}

typedef struct {
	GAsyncReadyCallback callback;
	gpointer user_data;
	gboolean cursor;
	TotemPlaylist *playlist;
	gchar *mrl;
	gchar *display_name;
} AddMrlData;

static void
add_mrl_data_free (AddMrlData *data)
{
	g_object_unref (data->playlist);
	g_free (data->mrl);
	g_free (data->display_name);
	g_slice_free (AddMrlData, data);
}

static gboolean
handle_parse_result (TotemPlParserResult res, TotemPlaylist *playlist, const gchar *mrl, const gchar *display_name, GError **error)
{
	if (res == TOTEM_PL_PARSER_RESULT_UNHANDLED)
		return totem_playlist_add_one_mrl (playlist, mrl, display_name, NULL, NULL, 0, FALSE);
	if (res == TOTEM_PL_PARSER_RESULT_ERROR) {
		g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
			     _("The playlist “%s” could not be parsed. It might be damaged."), display_name ? display_name : mrl);

		return FALSE;
	}
	if (res == TOTEM_PL_PARSER_RESULT_IGNORED)
		return FALSE;

	return TRUE;
}

static void
add_mrl_cb (TotemPlParser *parser, GAsyncResult *result, AddMrlData *data)
{
	TotemPlParserResult res;
	GSimpleAsyncResult *async_result;
	GError *error = NULL;
	gboolean ret;

	g_assert (data != NULL);

	/* Finish parsing the playlist */
	res = totem_pl_parser_parse_finish (parser, result, NULL);

	/* Remove the cursor, if one was set */
	if (data->cursor)
		g_application_unmark_busy (g_application_get_default ());

	/* Create an async result which will return the result to the code which called totem_playlist_add_mrl() */
	ret = handle_parse_result (res, data->playlist, data->mrl, data->display_name, &error);
	async_result = g_simple_async_result_new (G_OBJECT (data->playlist), data->callback, data->user_data, totem_playlist_add_mrl);
	if (error != NULL)
		g_simple_async_result_take_error (async_result, error);

	/* Handle the various return cases from the playlist parser */
	g_simple_async_result_set_op_res_gboolean (async_result, ret);

	/* Free the closure's data, now that we're finished with it */
	add_mrl_data_free (data);

	/* Synchronously call the calling code's callback function (i.e. what was passed to totem_playlist_add_mrl()'s @callback parameter)
	 * in the main thread to return the result */
	g_simple_async_result_complete (async_result);
}

void
totem_playlist_add_mrl (TotemPlaylist *playlist, const char *mrl, const char *display_name, gboolean cursor,
                        GCancellable *cancellable, GAsyncReadyCallback callback, gpointer user_data)
{
	AddMrlData *data;

	g_return_if_fail (mrl != NULL);

	/* Display a waiting cursor if required */
	if (cursor)
		g_application_mark_busy (g_application_get_default ());

	/* Build the data struct to pass to the callback function */
	data = g_slice_new (AddMrlData);
	data->callback = callback;
	data->user_data = user_data;
	data->cursor = cursor;
	data->playlist = g_object_ref (playlist);
	data->mrl = g_strdup (mrl);
	data->display_name = g_strdup (display_name);

	/* Start parsing the playlist. Once this is complete, add_mrl_cb() is called, which will interpret the results and call @callback to
	 * finish the process. */
	totem_pl_parser_parse_async (playlist->priv->parser, mrl, FALSE, cancellable, (GAsyncReadyCallback) add_mrl_cb, data);
}

gboolean
totem_playlist_add_mrl_finish (TotemPlaylist *playlist, GAsyncResult *result, GError **error)
{
	g_assert (g_simple_async_result_get_source_tag (G_SIMPLE_ASYNC_RESULT (result)) == totem_playlist_add_mrl);

	if (g_simple_async_result_get_op_res_gboolean (G_SIMPLE_ASYNC_RESULT (result)))
		return TRUE;
	g_simple_async_result_propagate_error (G_SIMPLE_ASYNC_RESULT (result), error);
	return FALSE;
}

gboolean
totem_playlist_add_mrl_sync (TotemPlaylist *playlist,
			     const char    *mrl)
{
	GtkTreeIter iter;
	gboolean ret;

	g_return_val_if_fail (mrl != NULL, FALSE);

	ret = handle_parse_result (totem_pl_parser_parse (playlist->priv->parser, mrl, FALSE), playlist, mrl, NULL, NULL);
	if (!ret)
		return ret;

	/* Find the currently playing track, and set ->current */
	ret = gtk_tree_model_get_iter_first (playlist->priv->model, &iter);
	while (ret) {
		TotemPlaylistStatus status;

		gtk_tree_model_get (playlist->priv->model, &iter,
				    PLAYING_COL, &status,
				    -1);
		if (status == TOTEM_PLAYLIST_STATUS_PAUSED) {
			gtk_tree_path_free (playlist->priv->current);
			playlist->priv->current = gtk_tree_model_get_path (playlist->priv->model, &iter);
			break;
		}
		ret = gtk_tree_model_iter_next (playlist->priv->model, &iter);
	}

	return TRUE;
}

typedef struct {
	TotemPlaylist *playlist;
	GList *mrls; /* list of TotemPlaylistMrlDatas */
	gboolean cursor;
	GAsyncReadyCallback callback;
	gpointer user_data;

	guint next_index_to_add;
	GList *unadded_entries; /* list of TotemPlaylistMrlDatas */
	volatile gint entries_remaining;
} AddMrlsOperationData;

static void
add_mrls_operation_data_free (AddMrlsOperationData *data)
{
	/* Remove the cursor, if one was set */
	if (data->cursor)
		g_application_unmark_busy (g_application_get_default ());

	g_list_free_full (data->mrls, (GDestroyNotify) totem_playlist_mrl_data_free);
	g_object_unref (data->playlist);

	g_slice_free (AddMrlsOperationData, data);
}

struct TotemPlaylistMrlData {
	gchar *mrl;
	gchar *display_name;
	TotemPlParserResult res;

	/* Implementation details */
	AddMrlsOperationData *operation_data;
	guint index;
};

/**
 * totem_playlist_mrl_data_new:
 * @mrl: a MRL
 * @display_name: (allow-none): a human-readable display name for the MRL, or %NULL
 *
 * Create a new #TotemPlaylistMrlData struct storing the given @mrl and @display_name.
 *
 * This will typically be immediately appended to a #GList to be passed to totem_playlist_add_mrls().
 *
 * Return value: (transfer full): a new #TotemPlaylistMrlData; free with totem_playlist_mrl_data_free()
 *
 * Since: 3.0
 */
TotemPlaylistMrlData *
totem_playlist_mrl_data_new (const gchar *mrl,
                             const gchar *display_name)
{
	TotemPlaylistMrlData *data;

	g_return_val_if_fail (mrl != NULL && *mrl != '\0', NULL);

	data = g_slice_new (TotemPlaylistMrlData);
	data->mrl = g_strdup (mrl);
	data->display_name = g_strdup (display_name);

	return data;
}

/**
 * totem_playlist_mrl_data_free:
 * @data: (transfer full): a #TotemPlaylistMrlData
 *
 * Free the given #TotemPlaylistMrlData struct. This should not generally be called by code outside #TotemPlaylist.
 *
 * Since: 3.0
 */
void
totem_playlist_mrl_data_free (TotemPlaylistMrlData *data)
{
	g_return_if_fail (data != NULL);

	/* NOTE: This doesn't call add_mrls_operation_data_free() on @data->operation_data, since it's shared with other instances of
	 * TotemPlaylistMrlData, and not truly reference counted. */
	g_free (data->display_name);
	g_free (data->mrl);

	g_slice_free (TotemPlaylistMrlData, data);
}

static void
add_mrls_finish_operation (AddMrlsOperationData *operation_data)
{
	/* Check whether this is the final callback invocation; iff it is, we can call the user's callback for the entire operation and free the
	 * operation data */
	if (g_atomic_int_dec_and_test (&(operation_data->entries_remaining)) == TRUE) {
		GSimpleAsyncResult *async_result;

		async_result = g_simple_async_result_new (G_OBJECT (operation_data->playlist), operation_data->callback, operation_data->user_data,
		                                          totem_playlist_add_mrls);
		g_simple_async_result_complete (async_result);
		g_object_unref (async_result);

		add_mrls_operation_data_free (operation_data);
	}
}

/* Called exactly once for each MRL in a totem_playlist_add_mrls() operation. Called in the thread running the main loop. If the MRL which has just
 * been parsed is the next one in the sequence (of entries in @mrls as passed to totem_playlist_add_mrls()), it's added to the playlist proper.
 * Otherwise, it's added to a sorted queue of MRLs which have had their callbacks called out of order.
 * When a MRL is added to the playlist proper, any successor MRLs which are in the sorted queue are also added to the playlist proper.
 * When add_mrls_cb() is called for the last time for a given call to totem_playlist_add_mrls(), it calls the user's callback for the operation
 * (passed as @callback to totem_playlist_add_mrls()) and frees the #AddMrlsOperationData struct. This is handled by add_mrls_finish_operation().
 * The #TotemPlaylistMrlData for each MRL is freed by add_mrls_operation_data_free() at the end of the entire operation. */
static void
add_mrls_cb (TotemPlParser *parser, GAsyncResult *result, TotemPlaylistMrlData *mrl_data)
{
	AddMrlsOperationData *operation_data = mrl_data->operation_data;

	/* Finish parsing the playlist */
	mrl_data->res = totem_pl_parser_parse_finish (parser, result, NULL);

	g_assert (mrl_data->index >= operation_data->next_index_to_add);

	if (mrl_data->index == operation_data->next_index_to_add) {
		GList *i;

		/* The entry is the next one in the order, so doesn't need to be added to the unadded list, and can be added to playlist proper */
		operation_data->next_index_to_add++;
		handle_parse_result (mrl_data->res, operation_data->playlist, mrl_data->mrl, mrl_data->display_name, NULL);

		/* See if we can now add any other entries which have already been processed */
		for (i = operation_data->unadded_entries;
		     i != NULL && ((TotemPlaylistMrlData*) i->data)->index == operation_data->next_index_to_add;
		     i = g_list_delete_link (i, i)) {
			TotemPlaylistMrlData *_mrl_data = (TotemPlaylistMrlData*) i->data;

			operation_data->next_index_to_add++;
			handle_parse_result (_mrl_data->res, operation_data->playlist, _mrl_data->mrl, _mrl_data->display_name, NULL);
		}

		operation_data->unadded_entries = i;
	} else {
		GList *i;

		/* The entry has been parsed out of order, so needs to be added (in the correct position) to the unadded list for latter addition to
		 * the playlist proper */
		for (i = operation_data->unadded_entries; i != NULL && mrl_data->index > ((TotemPlaylistMrlData*) i->data)->index; i = i->next);
		operation_data->unadded_entries = g_list_insert_before (operation_data->unadded_entries, i, mrl_data);
	}

	/* Check whether this is the last callback; call the user's callback for the entire operation and free the operation data if appropriate */
	add_mrls_finish_operation (operation_data);
}

/**
 * totem_playlist_add_mrls:
 * @self: a #TotemPlaylist
 * @mrls: (element-type TotemPlaylistMrlData) (transfer full): a list of #TotemPlaylistMrlData structs
 * @cursor: %TRUE to set a waiting cursor on the playlist for the duration of the operation, %FALSE otherwise
 * @cancellable: (allow-none): a #Cancellable, or %NULL
 * @callback: (scope async) (allow-none): callback to call once all the MRLs have been added to the playlist, or %NULL
 * @user_data: (closure) (allow-none): user data to pass to @callback, or %NULL
 *
 * Add the MRLs listed in @mrls to the playlist asynchronously, and ensuring that they're added to the playlist in the order they appear in the
 * input #GList.
 *
 * @mrls should be a #GList of #TotemPlaylistMrlData structs, each created with totem_playlist_mrl_data_new(). This function takes ownership of both
 * the list and its elements when called, so don't free either after calling totem_playlist_add_mrls().
 *
 * @callback will be called after all the MRLs in @mrls have been parsed and (if they were parsed successfully) added to the playlist. In the
 * callback function, totem_playlist_add_mrls_finish() should be called to check for errors.
 *
 * Since: 3.0
 */
void
totem_playlist_add_mrls (TotemPlaylist *self,
                         GList *mrls,
                         gboolean cursor,
                         GCancellable *cancellable,
                         GAsyncReadyCallback callback,
                         gpointer user_data)
{
	AddMrlsOperationData *operation_data;
	GList *i;
	guint mrl_index = 0;

	g_return_if_fail (TOTEM_IS_PLAYLIST (self));
	g_return_if_fail (mrls != NULL);
	g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));

	/* Build the data struct to pass to the callback function */
	operation_data = g_slice_new (AddMrlsOperationData);
	operation_data->playlist = g_object_ref (self);
	operation_data->mrls = mrls;
	operation_data->cursor = cursor;
	operation_data->callback = callback;
	operation_data->user_data = user_data;
	operation_data->next_index_to_add = mrl_index;
	operation_data->unadded_entries = NULL;
	g_atomic_int_set (&(operation_data->entries_remaining), 1);

	/* Display a waiting cursor if required */
	if (cursor)
		g_application_mark_busy (g_application_get_default ());

	for (i = mrls; i != NULL; i = i->next) {
		TotemPlaylistMrlData *mrl_data = (TotemPlaylistMrlData*) i->data;

		if (mrl_data == NULL)
			continue;

		/* Set the item's parsing index, so that it's inserted into the playlist in the position it appeared in @mrls */
		mrl_data->operation_data = operation_data;
		mrl_data->index = mrl_index++;

		g_atomic_int_inc (&(operation_data->entries_remaining));

		/* Start parsing the playlist. Once this is complete, add_mrls_cb() is called (i.e. it's called exactly once for each entry in
		 * @mrls).
		 * TODO: Cancellation is currently not supoprted, since no consumers of this API make use of it, and it needs careful thought when
		 * being implemented, as a separate #GCancellable instance will have to be created for each parallel computation. */
		totem_pl_parser_parse_async (self->priv->parser, mrl_data->mrl, FALSE, NULL, (GAsyncReadyCallback) add_mrls_cb, mrl_data);
	}

	/* Deal with the case that all async operations completed before we got to this point (since we've held a reference to the operation data so
	 * that it doesn't get freed prematurely if all the scheduled async parse operations complete before we've finished scheduling the rest. */
	add_mrls_finish_operation (operation_data);
}

/**
 * totem_playlist_add_mrls_finish:
 * @self: a #TotemPlaylist
 * @result: the #GAsyncResult that was provided to the callback
 * @error: (allow-none): a #GError for error reporting, or %NULL
 *
 * Finish an asynchronous batch MRL addition operation started by totem_playlist_add_mrls().
 *
 * Return value: %TRUE on success, %FALSE otherwise
 *
 * Since: 3.0
 */
gboolean
totem_playlist_add_mrls_finish (TotemPlaylist *self,
                                GAsyncResult *result,
                                GError **error)
{
	g_return_val_if_fail (TOTEM_IS_PLAYLIST (self), FALSE);
	g_return_val_if_fail (G_IS_ASYNC_RESULT (result), FALSE);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);
	g_return_val_if_fail (g_simple_async_result_is_valid (result, G_OBJECT (self), totem_playlist_add_mrls), FALSE);

	/* We don't have anything to return at the moment. */
	return TRUE;
}

static gboolean
totem_playlist_clear_cb (GtkTreeModel *model,
			 GtkTreePath  *path,
			 GtkTreeIter  *iter,
			 gpointer      data)
{
	totem_playlist_emit_item_removed (data, iter);
	return FALSE;
}

gboolean
totem_playlist_clear (TotemPlaylist *playlist)
{
	GtkListStore *store;

	g_return_val_if_fail (TOTEM_IS_PLAYLIST (playlist), FALSE);

	if (PL_LEN == 0)
		return FALSE;

	gtk_tree_model_foreach (playlist->priv->model,
				totem_playlist_clear_cb,
				playlist);

	store = GTK_LIST_STORE (playlist->priv->model);
	gtk_list_store_clear (store);

	g_clear_pointer (&playlist->priv->current, gtk_tree_path_free);

	g_signal_emit (G_OBJECT (playlist),
		       totem_playlist_table_signals[CURRENT_REMOVED],
		       0, NULL);

	return TRUE;
}

static int
compare_removal (GtkTreeRowReference *ref, GtkTreePath *path)
{
	GtkTreePath *ref_path;
	int ret = -1;

	ref_path = gtk_tree_row_reference_get_path (ref);
	if (gtk_tree_path_compare (path, ref_path) == 0)
		ret = 0;
	gtk_tree_path_free (ref_path);
	return ret;
}

/* Whether the item in question will be removed */
static gboolean
totem_playlist_item_to_be_removed (TotemPlaylist *playlist,
				   GtkTreePath *path,
				   ClearComparisonFunc func)
{
	GList *ret;

	if (func == NULL) {
		GtkTreeSelection *selection;

		selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (playlist->priv->treeview));
		return gtk_tree_selection_path_is_selected (selection, path);
	}

	ret = g_list_find_custom (playlist->priv->list, path, (GCompareFunc) compare_removal);
	return (ret != NULL);
}

static void
totem_playlist_clear_with_compare (TotemPlaylist *playlist,
				   ClearComparisonFunc func,
				   gconstpointer data)
{
	GtkTreeRowReference *ref;
	GtkTreeRowReference *next;

	ref = NULL;
	next = NULL;

	if (func == NULL) {
		GtkTreeSelection *selection;

		selection = gtk_tree_view_get_selection
			(GTK_TREE_VIEW (playlist->priv->treeview));
		if (selection == NULL)
			return;

		gtk_tree_selection_selected_foreach (selection,
						     totem_playlist_foreach_selected,
						     (gpointer) playlist);
	} else {
		guint num_items, i;

		num_items = PL_LEN;
		if (num_items == 0)
			return;

		for (i = 0; i < num_items; i++) {
			GtkTreeIter iter;
			char *playlist_index;

			playlist_index = g_strdup_printf ("%d", i);
			if (gtk_tree_model_get_iter_from_string (playlist->priv->model, &iter, playlist_index) == FALSE) {
				g_free (playlist_index);
				continue;
			}
			g_free (playlist_index);

			if ((* func) (playlist, &iter, data) != FALSE) {
				GtkTreePath *path;
				GtkTreeRowReference *r;

				path = gtk_tree_path_new_from_indices (i, -1);
				r = gtk_tree_row_reference_new (playlist->priv->model, path);
				if (playlist->priv->current_to_be_removed == FALSE && playlist->priv->current != NULL) {
					if (gtk_tree_path_compare (path, playlist->priv->current) == 0) {
						playlist->priv->current_to_be_removed = TRUE;
					}
				}
				playlist->priv->list = g_list_prepend (playlist->priv->list, r);
				gtk_tree_path_free (path);
			}
		}

		if (playlist->priv->list == NULL)
			return;
	}

	/* If the current item is to change, we need to keep an static
	 * reference to it, TreeIter and TreePath don't allow that */
	if (playlist->priv->current_to_be_removed == FALSE &&
	    playlist->priv->current != NULL) {
		ref = gtk_tree_row_reference_new (playlist->priv->model,
						  playlist->priv->current);
	} else if (playlist->priv->current != NULL) {
		GtkTreePath *item;

		item = gtk_tree_path_copy (playlist->priv->current);
		gtk_tree_path_next (item);
		next = gtk_tree_row_reference_new (playlist->priv->model, item);
		while (next != NULL) {
			if (totem_playlist_item_to_be_removed (playlist, item, func) == FALSE) {
				/* Found the item after the current one that
				 * won't be removed, thus the new current */
				break;
			}
			gtk_tree_row_reference_free (next);
			gtk_tree_path_next (item);
			next = gtk_tree_row_reference_new (playlist->priv->model, item);
		}
	}

	/* We destroy the items, one-by-one from the list built above */
	while (playlist->priv->list != NULL) {
		GtkTreePath *path;
		GtkTreeIter iter;

		path = gtk_tree_row_reference_get_path
			((GtkTreeRowReference *)(playlist->priv->list->data));
		gtk_tree_model_get_iter (playlist->priv->model, &iter, path);
		gtk_tree_path_free (path);

		totem_playlist_emit_item_removed (playlist, &iter);
		gtk_list_store_remove (GTK_LIST_STORE (playlist->priv->model), &iter);

		gtk_tree_row_reference_free
			((GtkTreeRowReference *)(playlist->priv->list->data));
		playlist->priv->list = g_list_remove (playlist->priv->list,
				playlist->priv->list->data);
	}
	g_clear_pointer (&playlist->priv->list, g_list_free);

	if (playlist->priv->current_to_be_removed != FALSE) {
		/* The current item was removed from the playlist */
		if (next != NULL) {
			playlist->priv->current = gtk_tree_row_reference_get_path (next);
			gtk_tree_row_reference_free (next);
		} else {
			playlist->priv->current = NULL;
		}

		g_signal_emit (G_OBJECT (playlist),
				totem_playlist_table_signals[CURRENT_REMOVED],
				0, NULL);
	} else {
		if (ref != NULL) {
			/* The path to the current item changed */
			playlist->priv->current = gtk_tree_row_reference_get_path (ref);
		}

		g_signal_emit (G_OBJECT (playlist),
				totem_playlist_table_signals[CHANGED], 0,
				NULL);
	}
	if (ref != NULL)
		gtk_tree_row_reference_free (ref);
	gtk_tree_view_columns_autosize (GTK_TREE_VIEW (playlist->priv->treeview));

	playlist->priv->current_to_be_removed = FALSE;
}

static char *
get_mount_default_location (GMount *mount)
{
	GFile *file;
	char *path;

	file = g_mount_get_root (mount);
	if (file == NULL)
		return NULL;
	path = g_file_get_path (file);
	g_object_unref (file);
	return path;
}

static gboolean
totem_playlist_compare_with_mount (TotemPlaylist *playlist, GtkTreeIter *iter, gconstpointer data)
{
	GMount *clear_mount = (GMount *) data;
	GMount *mount;
	char *mount_path, *clear_mount_path;
	gboolean retval = FALSE;

	gtk_tree_model_get (playlist->priv->model, iter,
			    MOUNT_COL, &mount, -1);

	if (mount == NULL)
		return FALSE;

	clear_mount_path = NULL;

	mount_path = get_mount_default_location (mount);
	if (mount_path == NULL)
		goto bail;

	clear_mount_path = get_mount_default_location (clear_mount);
	if (clear_mount_path == NULL)
		goto bail;

	if (g_str_equal (mount_path, clear_mount_path))
		retval = TRUE;

bail:
	g_free (mount_path);
	g_free (clear_mount_path);

	if (mount != NULL)
		g_object_unref (mount);

	return retval;
}

void
totem_playlist_clear_with_g_mount (TotemPlaylist *playlist,
				   GMount *mount)
{
	g_return_if_fail (mount != NULL);

	totem_playlist_clear_with_compare (playlist,
					   (ClearComparisonFunc) totem_playlist_compare_with_mount,
					   mount);
}

char *
totem_playlist_get_current_mrl (TotemPlaylist *playlist, char **subtitle)
{
	GtkTreeIter iter;
	char *path;

	if (subtitle != NULL)
		*subtitle = NULL;

	g_return_val_if_fail (TOTEM_IS_PLAYLIST (playlist), NULL);

	if (update_current_from_playlist (playlist) == FALSE)
		return NULL;

	if (gtk_tree_model_get_iter (playlist->priv->model, &iter,
				     playlist->priv->current) == FALSE)
		return NULL;

	if (subtitle != NULL) {
		gtk_tree_model_get (playlist->priv->model, &iter,
				    URI_COL, &path,
				    SUBTITLE_URI_COL, subtitle,
				    -1);
	} else {
		gtk_tree_model_get (playlist->priv->model, &iter,
				    URI_COL, &path,
				    -1);
	}

	return path;
}

char *
totem_playlist_get_current_title (TotemPlaylist *playlist)
{
	GtkTreeIter iter;
	char *title;

	g_return_val_if_fail (TOTEM_IS_PLAYLIST (playlist), NULL);

	if (update_current_from_playlist (playlist) == FALSE)
		return NULL;

	gtk_tree_model_get_iter (playlist->priv->model,
				 &iter,
				 playlist->priv->current);

	gtk_tree_model_get (playlist->priv->model,
			    &iter,
			    FILENAME_COL, &title,
			    -1);
	return title;
}

char *
totem_playlist_get_current_content_type (TotemPlaylist *playlist)
{
	GtkTreeIter iter;
	char *content_type;

	g_return_val_if_fail (TOTEM_IS_PLAYLIST (playlist), NULL);

	if (update_current_from_playlist (playlist) == FALSE)
		return NULL;

	gtk_tree_model_get_iter (playlist->priv->model,
				 &iter,
				 playlist->priv->current);

	gtk_tree_model_get (playlist->priv->model,
			    &iter,
			    MIME_TYPE_COL, &content_type,
			    -1);

	return content_type;
}

gint64
totem_playlist_steal_current_starttime (TotemPlaylist *playlist)
{
	GtkTreeIter iter;
	gint64 starttime;

	g_return_val_if_fail (TOTEM_IS_PLAYLIST (playlist), 0);

	if (update_current_from_playlist (playlist) == FALSE)
		return 0;

	gtk_tree_model_get_iter (playlist->priv->model,
				 &iter,
				 playlist->priv->current);

	gtk_tree_model_get (playlist->priv->model,
			    &iter,
			    STARTTIME_COL, &starttime,
			    -1);

	/* And reset the starttime so it's only used once,
	 * hence the "steal" in the API name */
	gtk_list_store_set (GTK_LIST_STORE (playlist->priv->model),
			    &iter,
			    STARTTIME_COL, 0,
			    -1);

	return starttime;
}

char *
totem_playlist_get_title (TotemPlaylist *playlist, guint title_index)
{
	GtkTreeIter iter;
	GtkTreePath *path;
	char *title;

	g_return_val_if_fail (TOTEM_IS_PLAYLIST (playlist), NULL);

	path = gtk_tree_path_new_from_indices (title_index, -1);

	gtk_tree_model_get_iter (playlist->priv->model,
				 &iter, path);

	gtk_tree_path_free (path);

	gtk_tree_model_get (playlist->priv->model,
			    &iter,
			    FILENAME_COL, &title,
			    -1);

	return title;
}

gboolean
totem_playlist_has_previous_mrl (TotemPlaylist *playlist)
{
	GtkTreeIter iter;

	g_return_val_if_fail (TOTEM_IS_PLAYLIST (playlist), FALSE);

	if (update_current_from_playlist (playlist) == FALSE)
		return FALSE;

	gtk_tree_model_get_iter (playlist->priv->model,
				 &iter,
				 playlist->priv->current);

	return gtk_tree_model_iter_previous (playlist->priv->model, &iter);
}

gboolean
totem_playlist_has_next_mrl (TotemPlaylist *playlist)
{
	GtkTreeIter iter;

	g_return_val_if_fail (TOTEM_IS_PLAYLIST (playlist), FALSE);

	if (update_current_from_playlist (playlist) == FALSE)
		return FALSE;

	gtk_tree_model_get_iter (playlist->priv->model,
				 &iter,
				 playlist->priv->current);

	return gtk_tree_model_iter_next (playlist->priv->model, &iter);
}

gboolean
totem_playlist_set_title (TotemPlaylist *playlist, const char *title)
{
	GtkListStore *store;
	GtkTreeIter iter;
	char *escaped_title;

	g_return_val_if_fail (TOTEM_IS_PLAYLIST (playlist), FALSE);

	if (update_current_from_playlist (playlist) == FALSE)
		return FALSE;

	store = GTK_LIST_STORE (playlist->priv->model);
	gtk_tree_model_get_iter (playlist->priv->model,
			&iter,
			playlist->priv->current);

	escaped_title = g_markup_escape_text (title, -1);
	gtk_list_store_set (store, &iter,
			FILENAME_COL, title,
			FILENAME_ESCAPED_COL, escaped_title,
			TITLE_CUSTOM_COL, TRUE,
			-1);
	g_free (escaped_title);

	g_signal_emit (playlist,
		       totem_playlist_table_signals[ACTIVE_NAME_CHANGED], 0);

	return TRUE;
}

gboolean
totem_playlist_set_playing (TotemPlaylist *playlist, TotemPlaylistStatus state)
{
	GtkListStore *store;
	GtkTreeIter iter;
	GtkTreePath *path;

	g_return_val_if_fail (TOTEM_IS_PLAYLIST (playlist), FALSE);

	if (update_current_from_playlist (playlist) == FALSE)
		return FALSE;

	store = GTK_LIST_STORE (playlist->priv->model);
	gtk_tree_model_get_iter (playlist->priv->model,
			&iter,
			playlist->priv->current);

	gtk_list_store_set (store, &iter,
			PLAYING_COL, state,
			-1);

	if (state == FALSE)
		return TRUE;

	path = gtk_tree_model_get_path (GTK_TREE_MODEL (store), &iter);
	gtk_tree_view_scroll_to_cell (GTK_TREE_VIEW (playlist->priv->treeview),
				      path, NULL,
				      TRUE, 0.5, 0);
	gtk_tree_path_free (path);

	return TRUE;
}

TotemPlaylistStatus
totem_playlist_get_playing (TotemPlaylist *playlist)
{
	GtkTreeIter iter;
	TotemPlaylistStatus status;

	g_return_val_if_fail (TOTEM_IS_PLAYLIST (playlist), TOTEM_PLAYLIST_STATUS_NONE);

	if (gtk_tree_model_get_iter (playlist->priv->model, &iter, playlist->priv->current) == FALSE)
		return TOTEM_PLAYLIST_STATUS_NONE;

	gtk_tree_model_get (playlist->priv->model,
			    &iter,
			    PLAYING_COL, &status,
			    -1);

	return status;
}

void
totem_playlist_set_previous (TotemPlaylist *playlist)
{
	GtkTreeIter iter;
	char *path;

	g_return_if_fail (TOTEM_IS_PLAYLIST (playlist));

	if (totem_playlist_has_previous_mrl (playlist) == FALSE)
		return;

	totem_playlist_unset_playing (playlist);

	path = gtk_tree_path_to_string (playlist->priv->current);
	if (g_str_equal (path, "0")) {
		totem_playlist_set_at_end (playlist);
		g_free (path);
		return;
	}
	g_free (path);

	gtk_tree_model_get_iter (playlist->priv->model,
				 &iter,
				 playlist->priv->current);

	if (!gtk_tree_model_iter_previous (playlist->priv->model, &iter))
		g_assert_not_reached ();
	gtk_tree_path_free (playlist->priv->current);
	playlist->priv->current = gtk_tree_model_get_path
		(playlist->priv->model, &iter);
}

void
totem_playlist_set_next (TotemPlaylist *playlist)
{
	GtkTreeIter iter;

	g_return_if_fail (TOTEM_IS_PLAYLIST (playlist));

	if (totem_playlist_has_next_mrl (playlist) == FALSE) {
		totem_playlist_set_at_start (playlist);
		return;
	}

	totem_playlist_unset_playing (playlist);

	gtk_tree_model_get_iter (playlist->priv->model,
				 &iter,
				 playlist->priv->current);

	if (!gtk_tree_model_iter_next (playlist->priv->model, &iter))
		g_assert_not_reached ();
	gtk_tree_path_free (playlist->priv->current);
	playlist->priv->current = gtk_tree_model_get_path (playlist->priv->model, &iter);
}

gboolean
totem_playlist_get_repeat (TotemPlaylist *playlist)
{
	g_return_val_if_fail (TOTEM_IS_PLAYLIST (playlist), FALSE);

	return playlist->priv->repeat;
}

void
totem_playlist_set_repeat (TotemPlaylist *playlist, gboolean repeat)
{
	g_return_if_fail (TOTEM_IS_PLAYLIST (playlist));

	g_settings_set_boolean (playlist->priv->settings, "repeat", repeat);
}

void
totem_playlist_set_at_start (TotemPlaylist *playlist)
{
	g_return_if_fail (TOTEM_IS_PLAYLIST (playlist));

	totem_playlist_unset_playing (playlist);
	g_clear_pointer (&playlist->priv->current, gtk_tree_path_free);
	update_current_from_playlist (playlist);
}

void
totem_playlist_set_at_end (TotemPlaylist *playlist)
{
	int indice;

	g_return_if_fail (TOTEM_IS_PLAYLIST (playlist));

	totem_playlist_unset_playing (playlist);
	g_clear_pointer (&playlist->priv->current, gtk_tree_path_free);

	if (PL_LEN) {
		indice = PL_LEN - 1;
		playlist->priv->current = gtk_tree_path_new_from_indices
			(indice, -1);
	}
}

int
totem_playlist_get_current (TotemPlaylist *playlist)
{
	char *path;
	double current_index;

	g_return_val_if_fail (TOTEM_IS_PLAYLIST (playlist), -1);

	if (playlist->priv->current == NULL)
		return -1;
	path = gtk_tree_path_to_string (playlist->priv->current);
	if (path == NULL)
		return -1;

	current_index = g_ascii_strtod (path, NULL);
	g_free (path);

	return current_index;
}

int
totem_playlist_get_last (TotemPlaylist *playlist)
{
	guint len = PL_LEN;

	g_return_val_if_fail (TOTEM_IS_PLAYLIST (playlist), -1);

	if (len == 0)
		return -1;

	return len - 1;
}

void
totem_playlist_set_current (TotemPlaylist *playlist, guint current_index)
{
	g_return_if_fail (TOTEM_IS_PLAYLIST (playlist));

	if (current_index >= (guint) PL_LEN)
		return;

	totem_playlist_unset_playing (playlist);
	gtk_tree_path_free (playlist->priv->current);
	playlist->priv->current = gtk_tree_path_new_from_indices (current_index, -1);
}

static void
totem_playlist_set_property (GObject      *object,
			     guint         property_id,
			     const GValue *value,
			     GParamSpec   *pspec)
{
	TotemPlaylist *playlist;

	playlist = TOTEM_PLAYLIST (object);

	switch (property_id) {
	case PROP_REPEAT:
		g_settings_set_boolean (playlist->priv->settings, "repeat", g_value_get_boolean (value));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
		break;
	}
}

static void
totem_playlist_get_property (GObject    *object,
			     guint       property_id,
			     GValue     *value,
			     GParamSpec *pspec)
{
	TotemPlaylist *playlist;

	playlist = TOTEM_PLAYLIST (object);

	switch (property_id) {
	case PROP_REPEAT:
		g_value_set_boolean (value, playlist->priv->repeat);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
		break;
	}
}

static void
totem_playlist_class_init (TotemPlaylistClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	g_type_class_add_private (klass, sizeof (TotemPlaylistPrivate));

	object_class->set_property = totem_playlist_set_property;
	object_class->get_property = totem_playlist_get_property;
	object_class->dispose = totem_playlist_dispose;

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
	totem_playlist_table_signals[SUBTITLE_CHANGED] =
		g_signal_new ("subtitle-changed",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (TotemPlaylistClass,
					       subtitle_changed),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE, 0);
	totem_playlist_table_signals[ITEM_ADDED] =
		g_signal_new ("item-added",
				G_TYPE_FROM_CLASS (klass),
				G_SIGNAL_RUN_LAST,
				G_STRUCT_OFFSET (TotemPlaylistClass,
					item_added),
				NULL, NULL,
				g_cclosure_marshal_generic,
				G_TYPE_NONE, 2, G_TYPE_STRING, G_TYPE_STRING);
	totem_playlist_table_signals[ITEM_REMOVED] =
		g_signal_new ("item-removed",
				G_TYPE_FROM_CLASS (klass),
				G_SIGNAL_RUN_LAST,
				G_STRUCT_OFFSET (TotemPlaylistClass,
					item_removed),
				NULL, NULL,
				g_cclosure_marshal_generic,
				G_TYPE_NONE, 2, G_TYPE_STRING, G_TYPE_STRING);

	g_object_class_install_property (object_class, PROP_REPEAT,
					 g_param_spec_boolean ("repeat", "Repeat",
							       "Whether repeat mode is enabled.", FALSE,
							       G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
}
