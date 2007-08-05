/* 
   Copyright (C) 2007 Jonathan Matthew

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

   Author: Jonathan Matthew <jonathan@kaolin.wh9.net>
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
#include "totem-pl-parser-pla.h"
#include "totem-pl-parser-private.h"

/* things we know */
#define PATH_OFFSET		2
#define FORMAT_ID_OFFSET	4
#define RECORD_SIZE		512

/* things we guessed */
#define TITLE_OFFSET		32
#define TITLE_SIZE		64

#ifndef TOTEM_PL_PARSER_MINI
gboolean
totem_pl_parser_write_pla (TotemPlParser *parser, GtkTreeModel *model,
			   TotemPlParserIterFunc func, 
			   const char *output, const char *title,
			   gpointer user_data, GError **error)
{
	GnomeVFSHandle *handle;
	GnomeVFSResult res;
	int num_entries_total, num_entries, i;
	char *buffer;
	gboolean ret;

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

	/* write the header */
	buffer = g_malloc0 (RECORD_SIZE);
	*((gint32 *)buffer) = GINT32_TO_BE (num_entries_total);
	strcpy (buffer + FORMAT_ID_OFFSET, "iriver UMS PLA");

	/* the player doesn't display this, but it stores
	 * the 'quick list' name there.
	 */
	strncpy (buffer + TITLE_OFFSET, title, TITLE_SIZE);
	if (totem_pl_parser_write_buffer (handle, buffer, RECORD_SIZE, error) == FALSE)
	{
		DEBUG(g_print ("Couldn't write header block"));
		gnome_vfs_close (handle);
		g_free (buffer);
		return FALSE;
	}

	ret = TRUE;
	for (i = 1; i <= num_entries_total; i++) {
		GtkTreeIter iter;
		char *uri, *title, *path, *converted;
		gsize written;
		gboolean custom_title;

		if (gtk_tree_model_iter_nth_child (model, &iter, NULL, i - 1) == FALSE)
			continue;

		func (model, &iter, &uri, &title, &custom_title, user_data);
		g_free (title);

		memset (buffer, 0, RECORD_SIZE);
		/* this value appears to identify the directory holding the file,
		 * but it doesn't seem to matter if it doesn't.
		 */
		buffer[1] = 0x1A;

		/* convert to filename */
		path = g_filename_from_uri (uri, NULL, error);
		if (path == NULL)
		{
			DEBUG(g_print ("Couldn't convert URI '%s' to a filename: %s\n", uri, (*error)->message));
			g_free (uri);
			ret = FALSE;
			break;
		}
		g_free (uri);

		/* replace slashes */
		g_strdelimit (path, "/", '\\');

		/* convert to big-endian utf16 and write it into the buffer */
		converted = g_convert (path, -1, "UTF-16BE", "UTF-8", NULL, &written, error);
		if (converted == NULL)
		{
			DEBUG(g_print ("Couldn't convert filename '%s' to UTF-16BE\n", path));
			g_free (path);
			ret = FALSE;
			break;
		}
		g_free (path);

		if (written > RECORD_SIZE - PATH_OFFSET)
			written = RECORD_SIZE - PATH_OFFSET;

		memcpy (buffer + PATH_OFFSET, converted, written);
		g_free (converted);

		if (totem_pl_parser_write_buffer (handle, buffer, RECORD_SIZE, error) == FALSE)
		{
			DEBUG(g_print ("Couldn't write entry %d to the file\n", i));
			ret = FALSE;
			break;
		}
	}

	g_free (buffer);
	gnome_vfs_close (handle);
	return ret;
}

TotemPlParserResult
totem_pl_parser_add_pla (TotemPlParser *parser, const char *url,
			 const char *base, gpointer data)
{
	TotemPlParserResult retval = TOTEM_PL_PARSER_RESULT_UNHANDLED;
	char *contents, *title;
	int size, offset, max_entries, entry;

	if (gnome_vfs_read_entire_file (url, &size, &contents) != GNOME_VFS_OK)
		return TOTEM_PL_PARSER_RESULT_ERROR;

	if (size < RECORD_SIZE)
	{
		g_free (contents);
		DEBUG(g_print ("playlist '%s' is too short: %d\n", url, size));
		return TOTEM_PL_PARSER_RESULT_ERROR;
	}

	/* read header block */
	max_entries = GINT32_FROM_BE (*((gint32 *)contents));
	if (strcmp (contents + FORMAT_ID_OFFSET, "iriver UMS PLA") != 0)
	{
		g_free (contents);
		DEBUG(g_print ("playlist '%s' signature doesn't match: %s\n", url, contents + 4));
		return TOTEM_PL_PARSER_RESULT_ERROR;
	}

	/* read playlist title starting at offset 32 */
	title = NULL;
	if (contents[TITLE_OFFSET] != '\0')
	{
		title = contents + TITLE_OFFSET;
		totem_pl_parser_playlist_start (parser, title);
	}

	offset = RECORD_SIZE;
	entry = 0;
	while (offset + RECORD_SIZE <= size && entry < max_entries) {
		char *path, *uri;
		GError *error = NULL;

		/* path starts at +2, is at most 500 bytes, in big-endian utf16 .. */
		path = g_convert (contents + offset + PATH_OFFSET,
				  RECORD_SIZE - PATH_OFFSET,
				  "UTF-8", "UTF-16BE",
				  NULL, NULL, &error);
		if (path == NULL)
		{
			DEBUG(g_print ("error converting entry %d to UTF-8: %s\n", entry, error->message));
			g_error_free (error);
			retval = TOTEM_PL_PARSER_RESULT_ERROR;
			break;
		}

		/* .. with backslashes.. */
		g_strdelimit (path, "\\", '/');

		/* and that's all we get. */
		uri = g_filename_to_uri (path, NULL, NULL);
		if (uri == NULL)
		{
			DEBUG(g_print ("error converting path %s to URI: %s\n", path, error->message));
			g_error_free (error);
			retval = TOTEM_PL_PARSER_RESULT_ERROR;
			break;
		}

		totem_pl_parser_add_url (parser, TOTEM_PL_PARSER_FIELD_URL, uri, NULL);

		g_free (uri);
		g_free (path);
		offset += RECORD_SIZE;
		entry++;
	}

	if (title != NULL)
		totem_pl_parser_playlist_end (parser, title);

	g_free (contents);

	return TOTEM_PL_PARSER_RESULT_SUCCESS;
}

#endif /* !TOTEM_PL_PARSER_MINI */

