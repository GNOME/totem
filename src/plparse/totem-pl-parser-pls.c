/* 
   Copyright (C) 2002, 2003, 2004, 2005, 2006, 2007 Bastien Nocera
   Copyright (C) 2003, 2004 Colin Walters <walters@rhythmbox.org>

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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */

#ifndef TOTEM_PL_PARSER_MINI
#include <string.h>
#include <glib.h>
#include <glib/gi18n-lib.h>
#include <gtk/gtk.h>
#include <libgnomevfs/gnome-vfs.h>
#include "totem-pl-parser.h"
#include "totemplparser-marshal.h"
#endif /* !TOTEM_PL_PARSER_MINI */

#include "totem-pl-parser-mini.h"
#include "totem-pl-parser-pls.h"
#include "totem-pl-parser-private.h"

#ifndef TOTEM_PL_PARSER_MINI
gboolean
totem_pl_parser_write_pls (TotemPlParser *parser, GtkTreeModel *model,
			   TotemPlParserIterFunc func, 
			   const char *output, const char *title,
			   gpointer user_data, GError **error)
{
	GnomeVFSHandle *handle;
	GnomeVFSResult res;
	int num_entries_total, num_entries, i;
	char *buf;
	gboolean success;

	num_entries = totem_pl_parser_num_entries (parser, model, func, user_data);
	num_entries_total = gtk_tree_model_iter_n_children (model, NULL);

	res = gnome_vfs_open (&handle, output, GNOME_VFS_OPEN_WRITE);
	if (res == GNOME_VFS_ERROR_NOT_FOUND) {
		res = gnome_vfs_create (&handle, output,
				GNOME_VFS_OPEN_WRITE, FALSE,
				GNOME_VFS_PERM_USER_WRITE
				| GNOME_VFS_PERM_USER_READ
				| GNOME_VFS_PERM_GROUP_READ);
	}

	if (res != GNOME_VFS_OK) {
		g_set_error(error,
			    TOTEM_PL_PARSER_ERROR,
			    TOTEM_PL_PARSER_ERROR_VFS_OPEN,
			    _("Couldn't open file '%s': %s"),
			    output, gnome_vfs_result_to_string (res));
		return FALSE;
	}

	buf = g_strdup ("[playlist]\n");
	success = totem_pl_parser_write_string (handle, buf, error);
	g_free (buf);
	if (success == FALSE)
		return FALSE;

	if (title != NULL) {
		buf = g_strdup_printf ("X-GNOME-Title=%s\n", title);
		success = totem_pl_parser_write_string (handle, buf, error);
		g_free (buf);
		if (success == FALSE)
		{
			gnome_vfs_close (handle);
			return FALSE;
		}
	}

	buf = g_strdup_printf ("NumberOfEntries=%d\n", num_entries);
	success = totem_pl_parser_write_string (handle, buf, error);
	g_free (buf);
	if (success == FALSE)
	{
		gnome_vfs_close (handle);
		return FALSE;
	}

	for (i = 1; i <= num_entries_total; i++) {
		GtkTreeIter iter;
		char *url, *title, *relative;
		gboolean custom_title;

		if (gtk_tree_model_iter_nth_child (model, &iter, NULL, i - 1) == FALSE)
			continue;

		func (model, &iter, &url, &title, &custom_title, user_data);

		if (totem_pl_parser_scheme_is_ignored (parser, url) != FALSE)
		{
			g_free (url);
			g_free (title);
			continue;
		}

		relative = totem_pl_parser_relative (url, output);
		buf = g_strdup_printf ("File%d=%s\n", i,
				relative ? relative : url);
		g_free (relative);
		g_free (url);
		success = totem_pl_parser_write_string (handle, buf, error);
		g_free (buf);
		if (success == FALSE)
		{
			gnome_vfs_close (handle);
			g_free (title);
			return FALSE;
		}

		if (custom_title == FALSE) {
			g_free (title);
			continue;
		}

		buf = g_strdup_printf ("Title%d=%s\n", i, title);
		success = totem_pl_parser_write_string (handle, buf, error);
		g_free (buf);
		g_free (title);
		if (success == FALSE)
		{
			gnome_vfs_close (handle);
			return FALSE;
		}
	}

	gnome_vfs_close (handle);
	return TRUE;
}

TotemPlParserResult
totem_pl_parser_add_pls_with_contents (TotemPlParser *parser, const char *url,
				       const char *contents)
{
	TotemPlParserResult retval = TOTEM_PL_PARSER_RESULT_UNHANDLED;
	char **lines;
	int i, num_entries;
	char *split_char, *playlist_title;
	gboolean dos_mode = FALSE;
	gboolean fallback;

	/* figure out whether we're a unix pls or dos pls */
	if (strstr(contents,"\x0d") == NULL) {
		split_char = "\n";
	} else {
		split_char = "\x0d\n";
		dos_mode = TRUE;
	}
	lines = g_strsplit (contents, split_char, 0);

	/* [playlist] */
	i = 0;
	playlist_title = NULL;

	/* Ignore empty lines */
	while (totem_pl_parser_line_is_empty (lines[i]) != FALSE)
		i++;

	if (lines[i] == NULL
			|| g_ascii_strncasecmp (lines[i], "[playlist]",
				(gsize)strlen ("[playlist]")) != 0) {
		goto bail;
	}

	playlist_title = totem_pl_parser_read_ini_line_string (lines,
			"X-GNOME-Title", dos_mode);

	if (playlist_title != NULL)
		totem_pl_parser_playlist_start (parser, playlist_title);

	/* numberofentries=? */
	num_entries = totem_pl_parser_read_ini_line_int (lines, "numberofentries");
	if (num_entries == -1)
		goto bail;

	retval = TOTEM_PL_PARSER_RESULT_SUCCESS;

	for (i = 1; i <= num_entries; i++) {
		char *file, *title, *genre;
		char *file_key, *title_key, *genre_key;

		file_key = g_strdup_printf ("file%d", i);
		title_key = g_strdup_printf ("title%d", i);
		/* Genre is our own little extension */
		genre_key = g_strdup_printf ("genre%d", i);

		file = totem_pl_parser_read_ini_line_string (lines, (const char*)file_key, dos_mode);
		title = totem_pl_parser_read_ini_line_string (lines, (const char*)title_key, dos_mode);
		genre = totem_pl_parser_read_ini_line_string (lines, (const char*)genre_key, dos_mode);

		g_free (file_key);
		g_free (title_key);
		g_free (genre_key);

		if (file == NULL)
		{
			g_free (file);
			g_free (title);
			g_free (genre);
			continue;
		}

		fallback = parser->priv->fallback;
		if (parser->priv->recurse)
			parser->priv->fallback = FALSE;

		if (strstr (file, "://") != NULL || file[0] == G_DIR_SEPARATOR) {
			if (totem_pl_parser_parse_internal (parser, file, NULL) != TOTEM_PL_PARSER_RESULT_SUCCESS) {
				totem_pl_parser_add_one_url_ext (parser, file, title, genre);
			}
		} else {
			char *base;

			/* Try with a base */
			base = totem_pl_parser_base_url (url);

			if (totem_pl_parser_parse_internal (parser, file, base) != TOTEM_PL_PARSER_RESULT_SUCCESS) {
				char *escaped, *uri;

				escaped = gnome_vfs_escape_path_string (file);
				uri = g_strdup_printf ("%s/%s", base, escaped);
				g_free (escaped);
				totem_pl_parser_add_one_url_ext (parser, uri, title, genre);
				g_free (uri);
			}

			g_free (base);
		}

		parser->priv->fallback = fallback;
		g_free (file);
		g_free (title);
		g_free (genre);
	}

	if (playlist_title != NULL)
		totem_pl_parser_playlist_end (parser, playlist_title);

bail:
	g_free (playlist_title);
	g_strfreev (lines);

	return retval;
}

TotemPlParserResult
totem_pl_parser_add_pls (TotemPlParser *parser, const char *url,
			 const char *base, gpointer data)
{
	TotemPlParserResult retval = TOTEM_PL_PARSER_RESULT_UNHANDLED;
	char *contents;
	int size;

	contents = totem_pl_parser_read_entire_file (url, &size);
	if (contents == NULL)
		return TOTEM_PL_PARSER_RESULT_ERROR;

	if (size == 0) {
		g_free (contents);
		return TOTEM_PL_PARSER_RESULT_SUCCESS;
	}

	retval = totem_pl_parser_add_pls_with_contents (parser, url, contents);
	g_free (contents);

	return retval;
}

#endif /* !TOTEM_PL_PARSER_MINI */

