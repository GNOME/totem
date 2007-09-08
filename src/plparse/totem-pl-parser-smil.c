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
#include <gtk/gtk.h>
#include <libgnomevfs/gnome-vfs.h>
#include "xmlparser.h"
#include "totem-pl-parser.h"
#include "totemplparser-marshal.h"
#endif /* !TOTEM_PL_PARSER_MINI */

#include "totem-pl-parser-mini.h"
#include "totem-pl-parser-smil.h"
#include "totem-pl-parser-private.h"

#ifndef TOTEM_PL_PARSER_MINI
static TotemPlParserResult
parse_smil_entry (TotemPlParser *parser,
		  char *base,
		  xml_node_t *doc,
		  xml_node_t *parent,
		  const char *parent_title)
{
	xml_node_t *node;
	const char *title, *url, *author, *abstract, *dur, *clip_begin, *copyright;
	TotemPlParserResult retval = TOTEM_PL_PARSER_RESULT_ERROR;

	title = NULL;
	url = NULL;
	author = NULL;
	abstract = NULL;
	dur = NULL;
	clip_begin = NULL;
	copyright = NULL;

	for (node = parent->child; node != NULL; node = node->next)
	{
		if (node->name == NULL)
			continue;

		/* ENTRY should only have one ref and one title nodes */
		if (g_ascii_strcasecmp (node->name, "video") == 0 || g_ascii_strcasecmp (node->name, "audio") == 0) {
			url = xml_parser_get_property (node, "src");
			title = xml_parser_get_property (node, "title");
			author = xml_parser_get_property (node, "author");
			dur = xml_parser_get_property (node, "dur");
			clip_begin = xml_parser_get_property (node, "clip-begin");
			abstract = xml_parser_get_property (node, "abstract");
			copyright = xml_parser_get_property (node, "copyright");

			if (url != NULL) {
				char *fullpath;

				fullpath = totem_pl_parser_resolve_url (base, url);
				totem_pl_parser_add_url (parser,
							 TOTEM_PL_PARSER_FIELD_URL, fullpath,
							 TOTEM_PL_PARSER_FIELD_TITLE, title ? title : parent_title,
							 TOTEM_PL_PARSER_FIELD_ABSTRACT, abstract,
							 TOTEM_PL_PARSER_FIELD_COPYRIGHT, copyright,
							 TOTEM_PL_PARSER_FIELD_AUTHOR, author,
							 TOTEM_PL_PARSER_FIELD_STARTTIME, clip_begin,
							 TOTEM_PL_PARSER_FIELD_DURATION, dur,
							 NULL);
				g_free (fullpath);
				retval = TOTEM_PL_PARSER_RESULT_SUCCESS;
			}
		} else {
			if (parse_smil_entry (parser,
						base, doc, node, parent_title) != FALSE)
				retval = TOTEM_PL_PARSER_RESULT_SUCCESS;
		}
	}

	return retval;
}

static const char*
parse_smil_head (TotemPlParser *parser, xml_node_t *doc, xml_node_t *parent)
{
	xml_node_t *node;
	const char *title;
	
	title = NULL;

	for (node = parent->child; node != NULL; node = node->next) {
		if (g_ascii_strcasecmp (node->name, "meta") == 0) {
			const char *prop;
			prop = xml_parser_get_property (node, "name");
			if (prop != NULL && g_ascii_strcasecmp (prop, "title") == 0) {
				title = xml_parser_get_property (node, "content");
				if (title != NULL)
					break;
			}
		}
	}

	return title;
}

static TotemPlParserResult
parse_smil_entries (TotemPlParser *parser, char *base, xml_node_t *doc)
{
	TotemPlParserResult retval = TOTEM_PL_PARSER_RESULT_ERROR;
	const char *title;
	xml_node_t *node;

	title = NULL;

	for (node = doc->child; node != NULL; node = node->next) {
		if (node->name == NULL)
			continue;

		if (g_ascii_strcasecmp (node->name, "body") == 0) {
			if (parse_smil_entry (parser, base,
					      doc, node, title) != FALSE) {
				retval = TOTEM_PL_PARSER_RESULT_SUCCESS;
			}
		} else if (title == NULL) {
			if (g_ascii_strcasecmp (node->name, "head") == 0)
				title = parse_smil_head (parser, doc, node);
		}
	}

	return retval;
}

static TotemPlParserResult
totem_pl_parser_add_smil_with_doc (TotemPlParser *parser, const char *url,
				   const char *_base, xml_node_t *doc)
{
	TotemPlParserResult retval = TOTEM_PL_PARSER_RESULT_UNHANDLED;
	char *base;

	/* If the document has no root, or no name */
	if(doc->name == NULL
	   || g_ascii_strcasecmp (doc->name, "smil") != 0) {
		return TOTEM_PL_PARSER_RESULT_ERROR;
	}

	base = totem_pl_parser_base_url (url);

	retval = parse_smil_entries (parser, base, doc);

	g_free (base);

	return retval;
}

TotemPlParserResult
totem_pl_parser_add_smil (TotemPlParser *parser, const char *url,
			  const char *_base, gpointer data)
{
	char *contents;
	int size;
	TotemPlParserResult retval;

	if (gnome_vfs_read_entire_file (url, &size, &contents) != GNOME_VFS_OK)
		return TOTEM_PL_PARSER_RESULT_ERROR;

	retval = totem_pl_parser_add_smil_with_data (parser, url,
						     _base, contents, size);
	g_free (contents);
	return retval;
}

TotemPlParserResult
totem_pl_parser_add_smil_with_data (TotemPlParser *parser, const char *url,
				    const char *_base, const char *contents, int size)
{
	xml_node_t* doc;
	TotemPlParserResult retval;

	xml_parser_init (contents, size, XML_PARSER_CASE_INSENSITIVE);
	if (xml_parser_build_tree_relaxed (&doc, TRUE) < 0)
		return TOTEM_PL_PARSER_RESULT_ERROR;

	retval = totem_pl_parser_add_smil_with_doc (parser, url, _base, doc);
	xml_parser_free_tree (doc);

	return retval;
}

#endif /* !TOTEM_PL_PARSER_MINI */

