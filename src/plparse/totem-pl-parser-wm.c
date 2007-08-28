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
#include "xmlparser.h"
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

	/* NSC files are handled directly by GStreamer */
	if (g_str_has_prefix (data, "[Address]") != FALSE)
		return TOTEM_PL_PARSER_RESULT_UNHANDLED;

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
parse_asx_entry (TotemPlParser *parser, const char *base, xml_node_t *parent, const char *pl_title)
{
	xml_node_t *node;
	TotemPlParserResult retval = TOTEM_PL_PARSER_RESULT_SUCCESS;
	char *fullpath;
	const char *url;
	const char *title, *duration, *starttime, *author;
	const char *moreinfo, *abstract, *copyright;

	fullpath = NULL;
	title = NULL;
	url = NULL;
	duration = NULL;
	starttime = NULL;
	moreinfo = NULL;
	abstract = NULL;
	copyright = NULL;
	author = NULL;

	for (node = parent->child; node != NULL; node = node->next) {
		if (node->name == NULL)
			continue;

		/* ENTRY can only have one title node but multiple REFs */
		if (g_ascii_strcasecmp (node->name, "ref") == 0) {
			const char *tmp;

			tmp = xml_parser_get_property (node, "href");
			if (tmp == NULL)
				continue;
			/* FIXME, should we prefer mms streams, or non-mms?
			 * See bug #352559 */
			if (url == NULL)
				url = tmp;

			continue;
		}

		if (g_ascii_strcasecmp (node->name, "title") == 0)
			title = node->data;

		if (g_ascii_strcasecmp (node->name, "author") == 0)
			author = node->data;

		if (g_ascii_strcasecmp (node->name, "moreinfo") == 0) {
			const char *tmp;

			tmp = xml_parser_get_property (node, "href");
			if (tmp == NULL)
				continue;
			moreinfo = tmp;
		}

		if (g_ascii_strcasecmp (node->name, "copyright") == 0)
			copyright = node->data;

		if (g_ascii_strcasecmp (node->name, "abstract") == 0)
			abstract = node->data;

		if (g_ascii_strcasecmp (node->name, "duration") == 0) {
			const char *tmp;

			tmp = xml_parser_get_property (node, "value");
			if (tmp == NULL)
				continue;
			duration = tmp;
		}

		if (g_ascii_strcasecmp (node->name, "starttime") == 0) {
			const char *tmp;

			tmp = xml_parser_get_property (node, "value");
			if (tmp == NULL)
				continue;
			starttime = tmp;
		}

		if (g_ascii_strcasecmp (node->name, "param") == 0) {
			const char *name, *value;

			name = xml_parser_get_property (node, "name");
			if (name == NULL || g_ascii_strcasecmp (name, "showwhilebuffering") != 0)
				continue;
			value = xml_parser_get_property (node, "value");
			if (value == NULL || g_ascii_strcasecmp (value, "true") != 0)
				continue;

			/* We ignore items that are the buffering images */
			retval = TOTEM_PL_PARSER_RESULT_IGNORED;
			goto bail;
		}
	}

	if (url == NULL)
		return TOTEM_PL_PARSER_RESULT_ERROR;

	fullpath = totem_pl_resolve_url (base, url);

	/* .asx files can contain references to other .asx files */
	retval = totem_pl_parser_parse_internal (parser, fullpath, NULL);
	if (retval != TOTEM_PL_PARSER_RESULT_SUCCESS) {
		totem_pl_parser_add_url (parser,
					 TOTEM_PL_PARSER_FIELD_URL, fullpath,
					 TOTEM_PL_PARSER_FIELD_TITLE, title ? title : pl_title,
					 TOTEM_PL_PARSER_FIELD_ABSTRACT, abstract,
					 TOTEM_PL_PARSER_FIELD_COPYRIGHT, copyright,
					 TOTEM_PL_PARSER_FIELD_AUTHOR, author,
					 TOTEM_PL_PARSER_FIELD_STARTTIME, starttime,
					 TOTEM_PL_PARSER_FIELD_DURATION, duration,
					 TOTEM_PL_PARSER_FIELD_MOREINFO, moreinfo,
					 NULL);
		retval = TOTEM_PL_PARSER_RESULT_SUCCESS;
	}

bail:
	g_free (fullpath);

	return retval;
}

static gboolean
parse_asx_entryref (TotemPlParser *parser, const char *base, xml_node_t *node)
{
	TotemPlParserResult retval = TOTEM_PL_PARSER_RESULT_SUCCESS;
	const char *url;
	char *fullpath;

	fullpath = NULL;
	url = NULL;

	url = xml_parser_get_property (node, "href");

	if (url == NULL) {
		return TOTEM_PL_PARSER_RESULT_ERROR;
	}

	fullpath = totem_pl_resolve_url (base, url);

	/* .asx files can contain references to other .asx files */
	retval = totem_pl_parser_parse_internal (parser, fullpath, NULL);
	if (retval != TOTEM_PL_PARSER_RESULT_SUCCESS) {
		totem_pl_parser_add_url (parser,
					 TOTEM_PL_PARSER_FIELD_URL, fullpath,
					 NULL);
		retval = TOTEM_PL_PARSER_RESULT_SUCCESS;
	}

	g_free (fullpath);

	return retval;
}

static gboolean
parse_asx_entries (TotemPlParser *parser, const char *_base, xml_node_t *parent)
{
	char *title = NULL;
	const char *newbase = NULL, *base = NULL;
	xml_node_t *node;
	TotemPlParserResult retval = TOTEM_PL_PARSER_RESULT_ERROR;

	base = _base;

	for (node = parent->child; node != NULL; node = node->next) {
		if (node->name == NULL)
			continue;

		if (g_ascii_strcasecmp (node->name, "title") == 0) {
			title = g_strdup (node->data);
			totem_pl_parser_playlist_start (parser, title);
		}
		if (g_ascii_strcasecmp (node->name, "base") == 0) {
			newbase = xml_parser_get_property (node, "href");
			if (newbase != NULL)
				base = newbase;
		}
		if (g_ascii_strcasecmp (node->name, "entry") == 0) {
			/* Whee! found an entry here, find the REF and TITLE */
			if (parse_asx_entry (parser, base, node, title) != FALSE)
				retval = TOTEM_PL_PARSER_RESULT_SUCCESS;
		}
		if (g_ascii_strcasecmp (node->name, "entryref") == 0) {
			/* Found an entryref, extract the REF attribute */
			if (parse_asx_entryref (parser, base, node) != FALSE)
				retval = TOTEM_PL_PARSER_RESULT_SUCCESS;
		}
		if (g_ascii_strcasecmp (node->name, "repeat") == 0) {
			/* Repeat at the top-level */
			if (parse_asx_entries (parser, base, node) != FALSE)
				retval = TOTEM_PL_PARSER_RESULT_SUCCESS;
		}
	}

	if (title != NULL)
		totem_pl_parser_playlist_end (parser, title);
	g_free (title);

	return retval;
}

TotemPlParserResult
totem_pl_parser_add_asx (TotemPlParser *parser, const char *url,
			 const char *base, gpointer data)
{
	xml_node_t* doc;
	char *_base, *contents;
	int size;
	TotemPlParserResult retval = TOTEM_PL_PARSER_RESULT_UNHANDLED;

	if (data != NULL && totem_pl_parser_is_uri_list (data, strlen (data)) != FALSE) {
		return totem_pl_parser_add_ram (parser, url, data);
	}

	if (gnome_vfs_read_entire_file (url, &size, &contents) != GNOME_VFS_OK)
		return TOTEM_PL_PARSER_RESULT_ERROR;

	xml_parser_init (contents, size, XML_PARSER_CASE_INSENSITIVE);
	if (xml_parser_build_tree_relaxed (&doc, TRUE) < 0) {
		g_free (contents);
		return TOTEM_PL_PARSER_RESULT_ERROR;
	}
	/* If the document has no name */
	if (doc->name == NULL
	    || g_ascii_strcasecmp (doc->name , "asx") != 0) {
		g_free (contents);
		xml_parser_free_tree (doc);
		return TOTEM_PL_PARSER_RESULT_ERROR;
	}

	if (base == NULL) {
		_base = totem_pl_parser_base_url (url);
	} else {
		_base = g_strdup (base);
	}

	if (parse_asx_entries (parser, _base, doc) != FALSE)
		retval = TOTEM_PL_PARSER_RESULT_SUCCESS;

	g_free (_base);
	g_free (contents);
	xml_parser_free_tree (doc);

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

