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
#include "totem-pl-parser-wm.h"
#include "totem-pl-parser-lines.h"
#include "totem-pl-parser-private.h"

#ifndef TOTEM_PL_PARSER_MINI

static TotemPlParserResult
totem_pl_parser_add_asf_reference_parser (TotemPlParser *parser,
					  const char *url, const char *base,
					  gpointer data)
{
	char *contents, **lines, *ref, *split_char;
	int size;

	if (gnome_vfs_read_entire_file (url, &size, &contents) != GNOME_VFS_OK)
		return TOTEM_PL_PARSER_RESULT_ERROR;

	if (strstr(contents,"\x0d") == NULL) {
		split_char = "\n";
	} else {
		split_char = "\x0d\n";
	}

	lines = g_strsplit (contents, split_char, 0);
	g_free (contents);

	/* Try to get Ref1 first */
	ref = totem_pl_parser_read_ini_line_string (lines, "Ref1", FALSE);
	if (ref == NULL) {
		g_strfreev (lines);
		return totem_pl_parser_add_asx (parser, url, base, data);
	}

	/* change http to mmsh, thanks Microsoft */
	if (g_str_has_prefix (ref, "http") != FALSE)
		memcpy(ref, "mmsh", 4);

	totem_pl_parser_add_one_url (parser, ref, NULL);
	g_free (ref);

	/* Don't try to get Ref2, as it's only ever
	 * supposed to be a fallback */

	g_strfreev (lines);

	return TOTEM_PL_PARSER_RESULT_SUCCESS;
}

static TotemPlParserResult
totem_pl_parser_add_asf_parser (TotemPlParser *parser,
				const char *url, const char *base,
				gpointer data)
{
	TotemPlParserResult retval = TOTEM_PL_PARSER_RESULT_UNHANDLED;
	char *contents, *ref;
	int size;

	if (g_str_has_prefix (data, "[Address]") != FALSE) {
		g_warning ("Implement NSC parsing: http://bugzilla.gnome.org/show_bug.cgi?id=350595");
		return TOTEM_PL_PARSER_RESULT_ERROR;
	}

	if (g_str_has_prefix (data, "ASF ") == FALSE) {
		return totem_pl_parser_add_asf_reference_parser (parser, url, base, data);
	}

	if (gnome_vfs_read_entire_file (url, &size, &contents) != GNOME_VFS_OK)
		return TOTEM_PL_PARSER_RESULT_ERROR;

	if (size <= 4) {
		g_free (contents);
		return TOTEM_PL_PARSER_RESULT_ERROR;
	}

	/* Skip 'ASF ' */
	ref = contents + 4;
	if (g_str_has_prefix (ref, "http") != FALSE) {
		memcpy(ref, "mmsh", 4);
		totem_pl_parser_add_one_url (parser, ref, NULL);
		retval = TOTEM_PL_PARSER_RESULT_SUCCESS;
	}

	g_free (contents);
	return retval;
}

static gboolean
parse_asx_entry (TotemPlParser *parser, char *base, xmlDocPtr doc,
		xmlNodePtr parent, const char *pl_title)
{
	xmlNodePtr node;
	TotemPlParserResult retval = TOTEM_PL_PARSER_RESULT_SUCCESS;
	xmlChar *title, *url;
	char *fullpath = NULL;

	title = NULL;
	url = NULL;

	for (node = parent->children; node != NULL; node = node->next) {
		if (node->name == NULL)
			continue;

		/* ENTRY can only have one title node but multiple REFs */
		if (g_ascii_strcasecmp ((char *)node->name, "ref") == 0
				|| g_ascii_strcasecmp ((char *)node->name, "entryref") == 0) {
			xmlChar *tmp;

			tmp = xmlGetProp (node, (const xmlChar *)"href");
			if (tmp == NULL)
				tmp = xmlGetProp (node, (const xmlChar *)"HREF");
			if (tmp == NULL)
				continue;
			/* FIXME, should we prefer mms streams, or non-mms?
			 * See bug #352559 */
			if (url == NULL)
				url = tmp;
			else
				xmlFree (tmp);

			continue;
		}

		if (g_ascii_strcasecmp ((char *)node->name, "title") == 0)
			title = xmlNodeListGetString(doc, node->children, 1);
	}

	if (url == NULL) {
		if (title)
			xmlFree (title);
		return TOTEM_PL_PARSER_RESULT_ERROR;
	}

	fullpath = totem_pl_resolve_url (base, (char *)url);

	xmlFree (url);

	/* .asx files can contain references to other .asx files */
	retval = totem_pl_parser_parse_internal (parser, fullpath, NULL);
	if (retval != TOTEM_PL_PARSER_RESULT_SUCCESS) {
		totem_pl_parser_add_one_url (parser, fullpath,
				(char *)title ? (char *)title : pl_title);
	}

	g_free (fullpath);
	if (title)
		xmlFree (title);

	return retval;
}

static gboolean
parse_asx_entries (TotemPlParser *parser, char *base, xmlDocPtr doc,
		xmlNodePtr parent)
{
	xmlChar *title = NULL;
	xmlNodePtr node;
	TotemPlParserResult retval = TOTEM_PL_PARSER_RESULT_ERROR;
	xmlChar *newbase = NULL;

	for (node = parent->children; node != NULL; node = node->next) {
		if (node->name == NULL)
			continue;

		if (g_ascii_strcasecmp ((char *)node->name, "title") == 0) {
			title = xmlNodeListGetString(doc, node->children, 1);
		}
		if (g_ascii_strcasecmp ((char *)node->name, "base") == 0) {
			newbase = xmlGetProp (node, (const xmlChar *)"href");
			if (newbase == NULL)
				newbase = xmlGetProp (node, (const xmlChar *)"HREF");
			if (newbase != NULL)
				base = (char *)newbase;
		}

		if (g_ascii_strcasecmp ((char *)node->name, "entry") == 0) {
			/* Whee found an entry here, find the REF and TITLE */
			if (parse_asx_entry (parser, base, doc, node, (char *)title) != FALSE)
				retval = TOTEM_PL_PARSER_RESULT_SUCCESS;
		}
		if (g_ascii_strcasecmp ((char *)node->name, "entryref") == 0) {
			/* Found an entryref, give the parent instead of the
			 * children to the parser */
			if (parse_asx_entry (parser, base, doc, parent, (char *)title) != FALSE)
				retval = TOTEM_PL_PARSER_RESULT_SUCCESS;
		}
		if (g_ascii_strcasecmp ((char *)node->name, "repeat") == 0) {
			/* Repeat at the top-level */
			if (parse_asx_entries (parser, base, doc, node) != FALSE)
				retval = TOTEM_PL_PARSER_RESULT_SUCCESS;
		}
	}

	if (newbase)
		xmlFree (newbase);

	if (title)
		xmlFree (title);

	return retval;
}

TotemPlParserResult
totem_pl_parser_add_asx (TotemPlParser *parser, const char *url,
			 const char *base, gpointer data)
{
	xmlDocPtr doc;
	xmlNodePtr node;
	char *_base;
	TotemPlParserResult retval = TOTEM_PL_PARSER_RESULT_UNHANDLED;

	if (data != NULL && totem_pl_parser_is_uri_list (data, strlen (data)) != FALSE) {
		return totem_pl_parser_add_ram (parser, url, data);
	}

	doc = totem_pl_parser_parse_xml_file (url);

	/* If the document has no root, or no name */
	if(!doc || !doc->children || !doc->children->name) {
		if (doc != NULL)
			xmlFreeDoc(doc);
		return TOTEM_PL_PARSER_RESULT_ERROR;
	}

	if (base == NULL) {
		_base = totem_pl_parser_base_url (url);
	} else {
		_base =  g_strdup (base);
	}

	for (node = doc->children; node != NULL; node = node->next)
		if (parse_asx_entries (parser, _base, doc, node) != FALSE)
			retval = TOTEM_PL_PARSER_RESULT_SUCCESS;

	g_free (_base);
	xmlFreeDoc(doc);
	return retval;
}

TotemPlParserResult
totem_pl_parser_add_asf (TotemPlParser *parser, const char *url,
			 const char *base, gpointer data)
{
	if (data == NULL) {
		totem_pl_parser_add_one_url (parser, url, NULL);
		return TOTEM_PL_PARSER_RESULT_SUCCESS;
	}

	if (totem_pl_parser_is_asf (data, strlen (data)) == FALSE) {
		totem_pl_parser_add_one_url (parser, url, NULL);
		return TOTEM_PL_PARSER_RESULT_SUCCESS;
	}

	return totem_pl_parser_add_asf_parser (parser, url, base, data);
}

#endif /* !TOTEM_PL_PARSER_MINI */

gboolean
totem_pl_parser_is_asx (const char *data, gsize len)
{
	char *buffer;

	if (len == 0)
		return FALSE;

	if (g_ascii_strncasecmp (data, "<ASX", strlen ("<ASX")) == 0)
		return TRUE;

	if (len > MIME_READ_CHUNK_SIZE)
		len = MIME_READ_CHUNK_SIZE;

	/* FIXME would be nicer to have an strnstr */
	buffer = g_memdup (data, len);
	if (buffer == NULL) {
		g_warning ("Couldn't dup data in totem_pl_parser_is_asx");
		return FALSE;
	}
	buffer[len - 1] = '\0';
	if (strstr (buffer, "<ASX") != NULL
			|| strstr (buffer, "<asx") != NULL) {
		g_free (buffer);
		return TRUE;
	}
	g_free (buffer);

	return FALSE;
}

gboolean
totem_pl_parser_is_asf (const char *data, gsize len)
{
	if (len == 0)
		return FALSE;

	if (g_str_has_prefix (data, "[Reference]") != FALSE
			|| g_str_has_prefix (data, "ASF ") != FALSE
			|| g_str_has_prefix (data, "[Address]") != FALSE) {
		return TRUE;
	}

	return totem_pl_parser_is_asx (data, len);
}

