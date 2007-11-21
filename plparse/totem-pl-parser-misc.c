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
#include <libgnomevfs/gnome-vfs-mime-utils.h>

#ifndef TOTEM_PL_PARSER_MINI
#include <gtk/gtk.h>
#include <libgnomevfs/gnome-vfs.h>
#include <libgnomevfs/gnome-vfs-mime.h>
#include <libgnomevfs/gnome-vfs-utils.h>
#include "totem-pl-parser.h"
#include "totemplparser-marshal.h"
#include "totem-disc.h"
#endif /* !TOTEM_PL_PARSER_MINI */

#include "totem-pl-parser-mini.h"
#include "totem-pl-parser-misc.h"
#include "totem-pl-parser-private.h"

#ifndef TOTEM_PL_PARSER_MINI
TotemPlParserResult
totem_pl_parser_add_gvp (TotemPlParser *parser, const char *url,
			 const char *base, gpointer data)
{
	TotemPlParserResult retval = TOTEM_PL_PARSER_RESULT_UNHANDLED;
	char *contents, **lines, *title, *link, *version;
	int size;

	if (gnome_vfs_read_entire_file (url, &size, &contents) != GNOME_VFS_OK)
		return TOTEM_PL_PARSER_RESULT_ERROR;

	if (g_str_has_prefix (contents, "#.download.the.free.Google.Video.Player") == FALSE && g_str_has_prefix (contents, "# download the free Google Video Player") == FALSE) {
		g_free (contents);
		return retval;
	}

	lines = g_strsplit (contents, "\n", 0);
	g_free (contents);

	/* We only handle GVP version 1.1 for now */
	version = totem_pl_parser_read_ini_line_string_with_sep (lines, "gvp_version", FALSE, ":");
	if (version == NULL || strcmp (version, "1.1") != 0) {
		g_free (version);
		g_strfreev (lines);
		return retval;
	}
	g_free (version);

	link = totem_pl_parser_read_ini_line_string_with_sep (lines, "url", FALSE, ":");
	if (link == NULL) {
		g_strfreev (lines);
		return retval;
	}

	retval = TOTEM_PL_PARSER_RESULT_SUCCESS;

	title = totem_pl_parser_read_ini_line_string_with_sep (lines, "title", FALSE, ":");

	totem_pl_parser_add_one_url (parser, link, title);

	g_free (link);
	g_free (title);
	g_strfreev (lines);

	return retval;
}

TotemPlParserResult
totem_pl_parser_add_desktop (TotemPlParser *parser, const char *url,
			     const char *base, gpointer data)
{
	char *contents, **lines;
	const char *path, *display_name, *type;
	int size;
	TotemPlParserResult res = TOTEM_PL_PARSER_RESULT_ERROR;

	if (gnome_vfs_read_entire_file (url, &size, &contents) != GNOME_VFS_OK)
		return res;

	lines = g_strsplit (contents, "\n", 0);
	g_free (contents);

	type = totem_pl_parser_read_ini_line_string (lines, "Type", FALSE);
	if (type == NULL)
		goto bail;
	
	if (g_ascii_strcasecmp (type, "Link") != 0
	    && g_ascii_strcasecmp (type, "FSDevice") != 0) {
		goto bail;
	}

	path = totem_pl_parser_read_ini_line_string (lines, "URL", FALSE);
	if (path == NULL)
		goto bail;

	display_name = totem_pl_parser_read_ini_line_string (lines, "Name", FALSE);

	if (totem_pl_parser_ignore (parser, path) == FALSE
	    && g_ascii_strcasecmp (type, "FSDevice") != 0) {
		totem_pl_parser_add_one_url (parser, path, display_name);
	} else {
		if (totem_pl_parser_parse_internal (parser, path, NULL) != TOTEM_PL_PARSER_RESULT_SUCCESS)
			totem_pl_parser_add_one_url (parser, path, display_name);
	}

	res = TOTEM_PL_PARSER_RESULT_SUCCESS;

bail:
	g_strfreev (lines);

	return res;
}

#endif /* !TOTEM_PL_PARSER_MINI */


