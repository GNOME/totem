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

#include <string.h>
#include <glib.h>
#include <glib/gi18n-lib.h>
#include <libgnomevfs/gnome-vfs-mime-utils.h>

#ifndef TOTEM_PL_PARSER_MINI
#include <libxml/tree.h>
#include <libxml/parser.h>
#include <gtk/gtk.h>
#include <libgnomevfs/gnome-vfs.h>
#include <libgnomevfs/gnome-vfs-mime.h>
#include <libgnomevfs/gnome-vfs-utils.h>
#include "totem-pl-parser.h"
#include "totemplparser-marshal.h"
#include "totem-disc.h"
#endif /* !TOTEM_PL_PARSER_MINI */

#include "totem-pl-parser-mini.h"
#include "totem-pl-parser-lines.h"
#include "totem-pl-parser-pls.h"
#include "totem-pl-parser-private.h"

#define EXTINF "#EXTINF:"

#ifndef TOTEM_PL_PARSER_MINI
static char *
totem_pl_parser_url_to_dos (const char *url, const char *output)
{
	char *retval, *i;

	retval = totem_pl_parser_relative (url, output);

	if (retval == NULL)
		retval = g_strdup (url);

	/* Don't change URIs, but change smb:// */
	if (g_str_has_prefix (retval, "smb://") != FALSE)
	{
		char *tmp;
		tmp = g_strdup (retval + strlen ("smb:"));
		g_free (retval);
		retval = tmp;
	}

	if (strstr (retval, "://") != NULL)
		return retval;

	i = retval;
	while (*i != '\0')
	{
		if (*i == '/')
			*i = '\\';
		i++;
	}

	return retval;
}

gboolean
totem_pl_parser_write_m3u (TotemPlParser *parser, GtkTreeModel *model,
		TotemPlParserIterFunc func, const char *output,
		gboolean dos_compatible, gpointer user_data, GError **error)
{
	GnomeVFSHandle *handle;
	GnomeVFSResult res;
	int num_entries_total, i;
	gboolean success;
	char *buf;
	char *cr;

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

	cr = dos_compatible ? "\r\n" : "\n";
	num_entries_total = gtk_tree_model_iter_n_children (model, NULL);
	if (num_entries_total == 0)
		return TRUE;

	for (i = 1; i <= num_entries_total; i++) {
		GtkTreeIter iter;
		char *url, *title, *path2;
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

		if (custom_title != FALSE) {
			buf = g_strdup_printf (EXTINF",%s%s", title, cr);
			success = totem_pl_parser_write_string (handle, buf, error);
			g_free (buf);
			if (success == FALSE) {
				g_free (title);
				g_free (url);
				gnome_vfs_close (handle);
				return FALSE;
			}
		}
		g_free (title);

		if (dos_compatible == FALSE) {
			char *tmp;
			tmp = totem_pl_parser_relative (url, output);
			if (tmp == NULL && g_str_has_prefix (url, "file:")) {
				path2 = g_filename_from_uri (url, NULL, NULL);
			} else {
				path2 = tmp;
			}
		} else {
			path2 = totem_pl_parser_url_to_dos (url, output);
		}

		buf = g_strdup_printf ("%s%s", path2 ? path2 : url, cr);
		g_free (path2);
		g_free (url);

		success = totem_pl_parser_write_string (handle, buf, error);
		g_free (buf);

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
totem_pl_parser_add_ram (TotemPlParser *parser, const char *url, gpointer data)
{
	gboolean retval = TOTEM_PL_PARSER_RESULT_UNHANDLED;
	char *contents, **lines;
	int size, i;
	const char *split_char;

	contents = totem_pl_parser_read_entire_file (url, &size);
	if (contents == NULL)
		return TOTEM_PL_PARSER_RESULT_ERROR;

	/* figure out whether we're a unix or dos RAM file */
	if (strstr(contents,"\x0d") == NULL)
		split_char = "\n";
	else
		split_char = "\x0d\n";

	lines = g_strsplit (contents, split_char, 0);
	g_free (contents);

	for (i = 0; lines[i] != NULL; i++) {
		/* Empty line */
		if (totem_pl_parser_line_is_empty (lines[i]) != FALSE)
			continue;

		retval = TOTEM_PL_PARSER_RESULT_SUCCESS;

		/* Either it's a URI, or it has a proper path ... */
		if (strstr(lines[i], "://") != NULL
				|| lines[i][0] == G_DIR_SEPARATOR) {
			/* .ram files can contain .smil entries */
			if (totem_pl_parser_parse_internal (parser, lines[i], NULL) != TOTEM_PL_PARSER_RESULT_SUCCESS)
			{
				totem_pl_parser_add_one_url (parser,
						lines[i], NULL);
			}
		} else if (strcmp (lines[i], "--stop--") == 0) {
			/* For Real Media playlists, handle the stop command */
			break;
		} else {
			char *base;

			/* Try with a base */
			base = totem_pl_parser_base_url (url);

			if (totem_pl_parser_parse_internal (parser, lines[i], base) != TOTEM_PL_PARSER_RESULT_SUCCESS)
			{
				char *fullpath;
				fullpath = g_strdup_printf ("%s/%s", base, lines[i]);
				totem_pl_parser_add_one_url (parser, fullpath, NULL);
				g_free (fullpath);
			}
			g_free (base);
		}
	}

	g_strfreev (lines);

	return retval;
}

static const char *
totem_pl_parser_get_extinfo_title (gboolean extinfo, char **lines, int i)
{
	const char *extinf, *comma;

	if (extinfo == FALSE || lines == NULL || i <= 0)
		return NULL;

	/* It's bound to have an EXTINF if we have extinfo */
	extinf = lines[i-1] + strlen(EXTINF);
	if (extinf[0] == '\0')
		return NULL;
	comma = strstr (extinf, ",");

	if (comma == NULL || comma[1] == '\0') {
		if (extinf[1] == '\0')
			return NULL;
		return extinf;
	}

	comma++;

	return comma;
}

TotemPlParserResult
totem_pl_parser_add_m3u (TotemPlParser *parser, const char *url,
			const char *_base, gpointer data)
{
	TotemPlParserResult retval = TOTEM_PL_PARSER_RESULT_UNHANDLED;
	char *contents, **lines;
	int size, i;
	const char *split_char;
	gboolean extinfo;

	contents = totem_pl_parser_read_entire_file (url, &size);
	if (contents == NULL)
		return TOTEM_PL_PARSER_RESULT_ERROR;

	/* .pls files with a .m3u extension, the nasties */
	if (g_str_has_prefix (contents, "[playlist]") != FALSE
			|| g_str_has_prefix (contents, "[Playlist]") != FALSE
			|| g_str_has_prefix (contents, "[PLAYLIST]") != FALSE) {
		retval = totem_pl_parser_add_pls_with_contents (parser, url, contents);
		g_free (contents);
		return retval;
	}

	/* is TRUE if there's an EXTINF on the previous line */
	extinfo = FALSE;

	/* figure out whether we're a unix m3u or dos m3u */
	if (strstr(contents,"\x0d") == NULL)
		split_char = "\n";
	else
		split_char = "\x0d\n";

	lines = g_strsplit (contents, split_char, 0);
	g_free (contents);

	for (i = 0; lines[i] != NULL; i++) {
		if (lines[i][0] == '\0')
			continue;

		retval = TOTEM_PL_PARSER_RESULT_SUCCESS;

		/* Ignore comments, but mark it if we have extra info */
		if (lines[i][0] == '#') {
			extinfo = g_str_has_prefix (lines[i], EXTINF);
			continue;
		}

		/* Either it's a URI, or it has a proper path ... */
		if (strstr(lines[i], "://") != NULL
				|| lines[i][0] == G_DIR_SEPARATOR) {
			if (totem_pl_parser_parse_internal (parser, lines[i], NULL) != TOTEM_PL_PARSER_RESULT_SUCCESS) {
				totem_pl_parser_add_one_url (parser, lines[i],
						totem_pl_parser_get_extinfo_title (extinfo, lines, i));
			}
			extinfo = FALSE;
		} else if (lines[i][0] == '\\' && lines[i][1] == '\\') {
			/* ... Or it's in the windows smb form
			 * (\\machine\share\filename), Note drive names
			 * (C:\ D:\ etc) are unhandled (unknown base for
			 * drive letters) */
		        char *tmpurl;

			lines[i] = g_strdelimit (lines[i], "\\", '/');
			tmpurl = g_strjoin (NULL, "smb:", lines[i], NULL);

			totem_pl_parser_add_one_url (parser, lines[i],
					totem_pl_parser_get_extinfo_title (extinfo, lines, i));
			extinfo = FALSE;

			g_free (tmpurl);
		} else {
			/* Try with a base */
			char *fullpath, *base, sep;

			base = totem_pl_parser_base_url (url);
			sep = (split_char[0] == '\n' ? '/' : '\\');
			if (sep == '\\')
				lines[i] = g_strdelimit (lines[i], "\\", '/');
			fullpath = g_strdup_printf ("%s/%s", base, lines[i]);
			totem_pl_parser_add_one_url (parser, fullpath,
					totem_pl_parser_get_extinfo_title (extinfo, lines, i));
			g_free (fullpath);
			g_free (base);
			extinfo = FALSE;
		}
	}

	g_strfreev (lines);

	return retval;
}

TotemPlParserResult
totem_pl_parser_add_ra (TotemPlParser *parser, const char *url,
			const char *base, gpointer data)
{
	if (data == NULL || totem_pl_parser_is_uri_list (data, strlen (data)) == FALSE) {
		totem_pl_parser_add_one_url (parser, url, NULL);
		return TOTEM_PL_PARSER_RESULT_SUCCESS;
	}

	return totem_pl_parser_add_ram (parser, url, NULL);
}


#endif /* !TOTEM_PL_PARSER_MINI */

#define CHECK_LEN if (i >= len) { return FALSE; }

gboolean
totem_pl_parser_is_uri_list (const char *data, gsize len)
{
	guint i = 0;

	/* Find the first bits of text */
	while (data[i] == '\n' || data[i] == '\t' || data[i] == ' ') {
		i++;
		CHECK_LEN;
	}
	CHECK_LEN;

	/* scheme always starts with a letter */
	if (g_ascii_isalpha (data[i]) == FALSE)
		return FALSE;
	while (g_ascii_isalnum (data[i]) != FALSE) {
		i++;
		CHECK_LEN;
	}

	CHECK_LEN;

	/* First non-alphanum character should be a ':' */
	if (data[i] != ':')
		return FALSE;
	i++;
	CHECK_LEN;

	if (data[i] != '/')
		return FALSE;
	i++;
	CHECK_LEN;

	if (data[i] != '/')
		return FALSE;

	return TRUE;
}

