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
#include <glib.h>
#include <libxml/tree.h>
#include <libxml/parser.h>
#include <gtk/gtk.h>
#include <libgnomevfs/gnome-vfs.h>
#include "totem-pl-parser.h"
#include "totemplparser-marshal.h"
#endif /* !TOTEM_PL_PARSER_MINI */

#include "totem-pl-parser-mini.h"
#include "totem-pl-parser-smil.h"
#include "totem-pl-parser-private.h"

#ifndef TOTEM_PL_PARSER_MINI
static gboolean
parse_smil_video_entry (TotemPlParser *parser, char *base,
		char *url, char *title)
{
	char *fullpath;

	fullpath = totem_pl_resolve_url (base, url);
	totem_pl_parser_add_one_url (parser, fullpath, title);

	g_free (fullpath);

	return TRUE;
}

static gboolean
parse_smil_entry (TotemPlParser *parser, char *base, xmlDocPtr doc,
		xmlNodePtr parent, xmlChar *parent_title)
{
	xmlNodePtr node;
	xmlChar *title, *url;
	TotemPlParserResult retval = TOTEM_PL_PARSER_RESULT_ERROR;

	title = NULL;
	url = NULL;

	for (node = parent->children; node != NULL; node = node->next)
	{
		if (node->name == NULL)
			continue;

		/* ENTRY should only have one ref and one title nodes */
		if (g_ascii_strcasecmp ((char *)node->name, "video") == 0 || g_ascii_strcasecmp ((char *)node->name, "audio") == 0) {
			url = xmlGetProp (node, (const xmlChar *)"src");
			title = xmlGetProp (node, (const xmlChar *)"title");

			if (url != NULL) {
				if (parse_smil_video_entry (parser,
						base, (char *)url,
						title ? (char *)title
						: (char *)parent_title) != FALSE)
					retval = TOTEM_PL_PARSER_RESULT_SUCCESS;
			}

			if (title)
				xmlFree (title);
			if (url)
				xmlFree (url);
		} else {
			if (parse_smil_entry (parser,
						base, doc, node, parent_title) != FALSE)
				retval = TOTEM_PL_PARSER_RESULT_SUCCESS;
		}
	}

	return retval;
}

static xmlChar *
parse_smil_head (TotemPlParser *parser, xmlDocPtr doc, xmlNodePtr parent)
{
	xmlNodePtr node;
	xmlChar *title = NULL;

	for (node = parent->children; node != NULL; node = node->next) {
		if (g_ascii_strcasecmp ((char *)node->name, "meta") == 0) {
			xmlChar *prop;
			prop = xmlGetProp (node, (const xmlChar *)"name");
			if (prop != NULL && g_ascii_strcasecmp ((char *)prop, "title") == 0) {
				title = xmlGetProp (node, (const xmlChar *)"content");
				if (title != NULL) {
					xmlFree (prop);
					break;
				}
			}
			xmlFree (prop);
		}
	}

	return title;
}

static gboolean
parse_smil_entries (TotemPlParser *parser, char *base, xmlDocPtr doc,
		xmlNodePtr parent)
{
	xmlNodePtr node;
	TotemPlParserResult retval = TOTEM_PL_PARSER_RESULT_ERROR;
	xmlChar *title = NULL;

	for (node = parent->children; node != NULL; node = node->next) {
		if (node->name == NULL)
			continue;

		if (g_ascii_strcasecmp ((char *)node->name, "body") == 0) {
			if (parse_smil_entry (parser, base,
						doc, node, title) != FALSE)
				retval = TOTEM_PL_PARSER_RESULT_SUCCESS;
		} else if (title == NULL) {
			if (g_ascii_strcasecmp ((char *)node->name, "head") == 0)
				title = parse_smil_head (parser, doc, node);
		}
	}

	if (title != NULL)
		xmlFree (title);

	return retval;
}

TotemPlParserResult
totem_pl_parser_add_smil (TotemPlParser *parser, const char *url,
			  const char *_base, gpointer data)
{
	xmlDocPtr doc;
	xmlNodePtr node;
	char *base;
	TotemPlParserResult retval = TOTEM_PL_PARSER_RESULT_UNHANDLED;

	doc = totem_pl_parser_parse_xml_file (url);

	/* If the document has no root, or no name */
	if(!doc || !doc->children
			|| !doc->children->name
			|| g_ascii_strcasecmp ((char *)doc->children->name,
				"smil") != 0) {
		if (doc != NULL)
			xmlFreeDoc (doc);
		return TOTEM_PL_PARSER_RESULT_ERROR;
	}

	base = totem_pl_parser_base_url (url);

	for (node = doc->children; node != NULL; node = node->next)
		if (parse_smil_entries (parser, base, doc, node) != FALSE)
			retval = TOTEM_PL_PARSER_RESULT_SUCCESS;

	g_free (base);
	xmlFreeDoc (doc);

	return retval;
}

#endif /* !TOTEM_PL_PARSER_MINI */

