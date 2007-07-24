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

#ifndef TOTEM_PL_PARSER_MINI
TotemPlParserResult
totem_pl_parser_add_pla (TotemPlParser *parser, const char *url,
			 const char *base, gpointer data)
{
	TotemPlParserResult retval = TOTEM_PL_PARSER_RESULT_UNHANDLED;
	char *contents, *title;
	int size, offset, max_entries, entry;

	if (gnome_vfs_read_entire_file (url, &size, &contents) != GNOME_VFS_OK)
		return TOTEM_PL_PARSER_RESULT_ERROR;

	if (size < 512)
	{
		g_free (contents);
		DEBUG(g_print ("playlist '%s' is too short: %d", url, size));
		return TOTEM_PL_PARSER_RESULT_SUCCESS;
	}

	/* read header block */
	max_entries = GINT32_FROM_BE (*((gint32 *)contents));
	if (strcmp (contents + 4, "iriver UMS PLA") != 0)
	{
		g_free (contents);
		DEBUG(g_print ("playlist '%s' signature doesn't match: %s", url, contents + 4));
		return TOTEM_PL_PARSER_RESULT_ERROR;
	}

	/* read playlist title starting at offset 32 */
	title = NULL;
	if (contents[32] != '\0')
	{
		title = contents + 32;
		totem_pl_parser_playlist_start (parser, title);
	}

	offset = 512;
	entry = 0;
	while (offset + 512 <= size && entry < max_entries) {
		char *p, *path, *uri;

		/* path starts at +2, is at most 500 bytes, in big-endian utf16 .. */
		path = g_convert (contents + offset + 2, 500, "UTF-8", "UTF-16BE", NULL, NULL, NULL);
		if (path == NULL)
		{
			DEBUG(g_print ("error converting entry %d to UTF-8", entry));
			retval = TOTEM_PL_PARSER_RESULT_ERROR;
			break;
		}

		/* .. with backslashes.. */
		p = path;
		for (p = path; p[0] != '\0'; p++) {
			if (*p == '\\')
				*p = '/';
		}

		/* and that's all we get. */
		uri = g_strdup_printf ("%s%s", "file://", path);
		totem_pl_parser_add_url (parser, TOTEM_PL_PARSER_FIELD_URL, uri, NULL);

		g_free (uri);
		g_free (path);
		offset += 512;
		entry++;
	}

	if (title != NULL)
		totem_pl_parser_playlist_end (parser, title);

	g_free (contents);

	return TOTEM_PL_PARSER_RESULT_SUCCESS;
}

#endif /* !TOTEM_PL_PARSER_MINI */

