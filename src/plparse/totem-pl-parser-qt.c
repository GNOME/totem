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

#ifndef TOTEM_PL_PARSER_MINI
#include "xmlparser.h"
#include <gtk/gtk.h>
#include <libgnomevfs/gnome-vfs.h>
#include "totem-pl-parser.h"
#include "totemplparser-marshal.h"
#endif /* !TOTEM_PL_PARSER_MINI */

#include "totem-pl-parser-mini.h"
#include "totem-pl-parser-qt.h"
#include "totem-pl-parser-smil.h"
#include "totem-pl-parser-private.h"

#ifndef TOTEM_PL_PARSER_MINI

static TotemPlParserResult
totem_pl_parser_add_quicktime_rtsptext (TotemPlParser *parser,
					const char *url,
					const char *base,
					gpointer data)
{
	char *contents = NULL;
	gboolean dos_mode = FALSE;
	char *volume, *autoplay, *rtspurl;
	int size;
	char **lines;

	if (gnome_vfs_read_entire_file (url, &size, &contents) != GNOME_VFS_OK)
		return TOTEM_PL_PARSER_RESULT_ERROR;

	if (strstr(contents,"\x0d") != NULL)
		dos_mode = TRUE;

	lines = g_strsplit (contents, dos_mode ? "\x0d\n" : "\n", 0);

	volume = totem_pl_parser_read_ini_line_string_with_sep
		(lines, "volume", dos_mode, "=");
	autoplay = totem_pl_parser_read_ini_line_string_with_sep
		(lines, "autoplay", dos_mode, "=");

	rtspurl = g_strdup (lines[0] + strlen ("RTSPtext"));
	g_strstrip (rtspurl);

	totem_pl_parser_add_url (parser,
				 TOTEM_PL_PARSER_FIELD_URL, rtspurl,
				 TOTEM_PL_PARSER_FIELD_VOLUME, volume,
				 TOTEM_PL_PARSER_FIELD_AUTOPLAY, autoplay,
				 NULL);
	g_free (rtspurl);
	g_free (volume);
	g_free (autoplay);
	g_strfreev (lines);

	return TOTEM_PL_PARSER_RESULT_SUCCESS;
}

static TotemPlParserResult
totem_pl_parser_add_quicktime_metalink (TotemPlParser *parser, const char *url,
					const char *base, gpointer data)
{
	xml_node_t *doc, *node;
	int size;
	char *contents;
	const char *item_url, *autoplay;
	gboolean found;

	if (g_str_has_prefix (data, "RTSPtext") != FALSE
			|| g_str_has_prefix (data, "rtsptext") != FALSE) {
		return totem_pl_parser_add_quicktime_rtsptext (parser, url, base, data);
	}
	if (g_str_has_prefix (data, "SMILtext") != FALSE) {
		char *contents;
		int size;
		TotemPlParserResult retval;

		if (gnome_vfs_read_entire_file (url, &size, &contents) != GNOME_VFS_OK)
			return TOTEM_PL_PARSER_RESULT_ERROR;

		retval = totem_pl_parser_add_smil_with_data (parser,
							     url, base,
							     contents + strlen ("SMILtext"),
							     size - strlen ("SMILtext"));
		g_free (contents);
		return retval;
	}

	if (gnome_vfs_read_entire_file (url, &size, &contents) != GNOME_VFS_OK)
		return TOTEM_PL_PARSER_RESULT_ERROR;

	xml_parser_init (contents, size, XML_PARSER_CASE_INSENSITIVE);
	if (xml_parser_build_tree_relaxed (&doc, TRUE) < 0) {
		g_free (contents);
		return TOTEM_PL_PARSER_RESULT_ERROR;
	}
	g_free (contents);

	/* Check for quicktime type */
	for (node = doc, found = FALSE; node != NULL; node = node->next) {
		const char *type;

		if (node->name == NULL)
			continue;
		if (g_ascii_strcasecmp (node->name , "?quicktime") != 0)
			continue;
		type = xml_parser_get_property (node, "type");
		if (g_ascii_strcasecmp ("application/x-quicktime-media-link", type) != 0)
			continue;
		found = TRUE;
	}

	if (found == FALSE) {
		xml_parser_free_tree (doc);
		return TOTEM_PL_PARSER_RESULT_ERROR;
	}

	if (!doc || !doc->name
	    || g_ascii_strcasecmp (doc->name, "embed") != 0) {
		xml_parser_free_tree (doc);
		return TOTEM_PL_PARSER_RESULT_ERROR;
	}

	item_url = xml_parser_get_property (doc, "src");
	if (!item_url) {
		xml_parser_free_tree (doc);
		return TOTEM_PL_PARSER_RESULT_ERROR;
	}

	autoplay = xml_parser_get_property (doc, "autoplay");
	/* Add a default as per the QuickTime docs */
	if (autoplay == NULL)
		autoplay = "true";

	totem_pl_parser_add_url (parser,
				 TOTEM_PL_PARSER_FIELD_URL, item_url,
				 TOTEM_PL_PARSER_FIELD_AUTOPLAY, autoplay,
				 NULL);
	xml_parser_free_tree (doc);

	return TOTEM_PL_PARSER_RESULT_SUCCESS;
}

TotemPlParserResult
totem_pl_parser_add_quicktime (TotemPlParser *parser, const char *url,
			       const char *base, gpointer data)
{
	if (data == NULL || totem_pl_parser_is_quicktime (data, strlen (data)) == FALSE) {
		totem_pl_parser_add_one_url (parser, url, NULL);
		return TOTEM_PL_PARSER_RESULT_SUCCESS;
	}

	return totem_pl_parser_add_quicktime_metalink (parser, url, base, data);
}

#endif /* !TOTEM_PL_PARSER_MINI */

gboolean
totem_pl_parser_is_quicktime (const char *data, gsize len)
{
	char *buffer;

	if (len == 0)
		return FALSE;
	if (len > MIME_READ_CHUNK_SIZE)
		len = MIME_READ_CHUNK_SIZE;

	/* Check for RTSPtextRTSP Quicktime references */
	if (len <= strlen ("RTSPtextRTSP://"))
		return FALSE;
	if (g_str_has_prefix (data, "RTSPtext") != FALSE
			|| g_str_has_prefix (data, "rtsptext") != FALSE) {
		return TRUE;
	}
	if (g_str_has_prefix (data, "SMILtext") != FALSE)
		return TRUE;

	/* FIXME would be nicer to have an strnstr */
	buffer = g_memdup (data, len);
	if (buffer == NULL) {
		g_warning ("Couldn't dup data in totem_pl_parser_is_quicktime");
		return FALSE;
	}
	buffer[len - 1] = '\0';
	if (strstr (buffer, "<?quicktime") != NULL) {
		g_free (buffer);
		return TRUE;
	}
	g_free (buffer);
	return FALSE;
}


