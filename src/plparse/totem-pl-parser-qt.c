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
#include "totem-pl-parser-qt.h"
#include "totem-pl-parser-private.h"

#ifndef TOTEM_PL_PARSER_MINI

static TotemPlParserResult
totem_pl_parser_add_quicktime_rtsptextrtsp (TotemPlParser *parser,
					    const char *url,
					    const char *base,
					    gpointer data)
{
	char *contents = NULL;
	int size;
	char **lines;

	contents = totem_pl_parser_read_entire_file (url, &size);
	if (contents == NULL)
		return TOTEM_PL_PARSER_RESULT_ERROR;

	lines = g_strsplit (contents, "\n", 0);
	g_free (contents);

	totem_pl_parser_add_one_url (parser, lines[0] + strlen ("RTSPtext"), NULL);
	g_strfreev (lines);

	return TOTEM_PL_PARSER_RESULT_SUCCESS;
}

static TotemPlParserResult
totem_pl_parser_add_quicktime_metalink (TotemPlParser *parser, const char *url,
					const char *base, gpointer data)
{
	xmlDocPtr doc;
	xmlNodePtr node;
	xmlChar *src;

	if (g_str_has_prefix (data, "RTSPtextRTSP://") != FALSE
			|| g_str_has_prefix (data, "rtsptextrtsp://") != FALSE
			|| g_str_has_prefix (data, "RTSPtextrtsp://") != FALSE) {
		return totem_pl_parser_add_quicktime_rtsptextrtsp (parser, url, base, data);
	}

	doc = totem_pl_parser_parse_xml_file (url);

	/* If the document has no root, or no name */
	if(!doc || !doc->children
			|| !doc->children->name
			|| g_ascii_strcasecmp ((char *)doc->children->name,
				"quicktime") != 0) {
		if (doc != NULL)
			xmlFreeDoc (doc);
		return TOTEM_PL_PARSER_RESULT_ERROR;
	}

	if (strstr ((char *) doc->children->content, "type=\"application/x-quicktime-media-link\"") == NULL) {
		xmlFreeDoc (doc);
		return TOTEM_PL_PARSER_RESULT_ERROR;
	}

	node = doc->children->next;
	if (!node || !node->name
			|| g_ascii_strcasecmp ((char *) node->name,
				"embed") != 0) {
		xmlFreeDoc (doc);
		return TOTEM_PL_PARSER_RESULT_ERROR;
	}

	src = xmlGetProp (node, (const xmlChar *)"src");
	if (!src) {
		xmlFreeDoc (doc);
		return TOTEM_PL_PARSER_RESULT_ERROR;
	}

	totem_pl_parser_add_one_url (parser, (char *) src, NULL);

	xmlFree (src);
	xmlFreeDoc (doc);

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
	if (g_str_has_prefix (data, "RTSPtextRTSP://") != FALSE
			|| g_str_has_prefix (data, "rtsptextrtsp://") != FALSE
			|| g_str_has_prefix (data, "RTSPtextrtsp://") != FALSE) {
		return TRUE;
	}

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


