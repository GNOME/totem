/* 
   Copyright (C) 2007 Bastien Nocera <hadess@hadess.net>

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
#include "xmlparser.h"
#include "totem-pl-parser.h"
#include "totemplparser-marshal.h"
#include "totem-disc.h"
#endif /* !TOTEM_PL_PARSER_MINI */

#include "totem-pl-parser-mini.h"
#include "totem-pl-parser-podcast.h"
#include "totem-pl-parser-private.h"

gboolean
totem_pl_parser_is_rss (const char *data, gsize len)
{
	char *buffer;

	if (len == 0)
		return FALSE;
	if (len > MIME_READ_CHUNK_SIZE)
		len = MIME_READ_CHUNK_SIZE;

	/* FIXME would be nicer to have an strnstr */
	buffer = g_memdup (data, len);
	if (buffer == NULL) {
		g_warning ("Couldn't dup data in totem_pl_parser_is_rss");
		return FALSE;
	}
	buffer[len - 1] = '\0';
	if (strstr (buffer, "<rss ") != NULL) {
		g_free (buffer);
		return TRUE;
	}
	g_free (buffer);
	return FALSE;
}

gboolean
totem_pl_parser_is_atom (const char *data, gsize len)
{
	char *buffer;

	if (len == 0)
		return FALSE;
	if (len > MIME_READ_CHUNK_SIZE)
		len = MIME_READ_CHUNK_SIZE;

	/* FIXME would be nicer to have an strnstr */
	buffer = g_memdup (data, len);
	if (buffer == NULL) {
		g_warning ("Couldn't dup data in totem_pl_parser_is_atom");
		return FALSE;
	}
	buffer[len - 1] = '\0';
	if (strstr (buffer, "<feed ") != NULL) {
		g_free (buffer);
		return TRUE;
	}
	g_free (buffer);
	return FALSE;
}

gboolean
totem_pl_parser_is_xml_feed (const char *data, gsize len)
{
	if (totem_pl_parser_is_rss (data, len) != FALSE)
		return TRUE;
	if (totem_pl_parser_is_atom (data, len) != FALSE)
		return TRUE;
	return FALSE;
}

#ifndef TOTEM_PL_PARSER_MINI

static TotemPlParserResult
parse_rss_item (TotemPlParser *parser, xml_node_t *parent)
{
	const char *title, *url, *description, *author;
	const char *pub_date, *duration, *filesize;
	xml_node_t *node;

	title = url = description = author = NULL;
	pub_date = duration = filesize = NULL;

	for (node = parent->child; node != NULL; node = node->next) {
		if (node->name == NULL)
			continue;

		if (g_ascii_strcasecmp (node->name, "title") == 0) {
			title = node->data;
		} else if (g_ascii_strcasecmp (node->name, "url") == 0) {
			url = node->data;
		} else if (g_ascii_strcasecmp (node->name, "pubDate") == 0) {
			pub_date = node->data;
		} else if (g_ascii_strcasecmp (node->name, "description") == 0) {
			description = node->data;
		} else if (g_ascii_strcasecmp (node->name, "author") == 0) {
			author = node->data;
		} else if (g_ascii_strcasecmp (node->name, "itunes:duration") == 0) {
			duration = node->data;
		} else if (g_ascii_strcasecmp (node->name, "length") == 0) {
			filesize = node->data;
		} else if (g_ascii_strcasecmp (node->name, "enclosure") == 0) {
			const char *tmp;

			tmp = xml_parser_get_property (node, "url");
			if (tmp != NULL)
				url = tmp;
			else
				continue;
			tmp = xml_parser_get_property (node, "length");
			if (tmp != NULL)
				filesize = tmp;
		}
	}

	if (url != NULL) {
		totem_pl_parser_add_url (parser,
					 TOTEM_PL_PARSER_FIELD_URL, url,
					 TOTEM_PL_PARSER_FIELD_TITLE, title,
					 TOTEM_PL_PARSER_FIELD_PUB_DATE, pub_date,
					 TOTEM_PL_PARSER_FIELD_DESCRIPTION, description,
					 TOTEM_PL_PARSER_FIELD_AUTHOR, author,
					 TOTEM_PL_PARSER_FIELD_DURATION, duration,
					 TOTEM_PL_PARSER_FIELD_FILESIZE, filesize,
					 NULL);
	}

	return TOTEM_PL_PARSER_RESULT_SUCCESS;
}

static TotemPlParserResult
parse_rss_items (TotemPlParser *parser, const char *url, xml_node_t *parent)
{
	const char *title, *language, *description, *author;
	const char *contact, *img, *pub_date, *copyright;
	xml_node_t *node;
	gboolean started = FALSE;

	title = language = description = author = NULL;
	contact = img = pub_date = copyright = NULL;

	for (node = parent->child; node != NULL; node = node->next) {
		if (node->name == NULL)
			continue;

		//FIXME handle itunes:subtitle?

		if (g_ascii_strcasecmp (node->name, "title") == 0) {
			title = node->data;
		} else if (g_ascii_strcasecmp (node->name, "language") == 0) {
			language = node->data;
		} else if (g_ascii_strcasecmp (node->name, "description") == 0
			 || g_ascii_strcasecmp (node->name, "itunes:summary") == 0) {
		    	description = node->data;
		} else if (g_ascii_strcasecmp (node->name, "author") == 0
			 || g_ascii_strcasecmp (node->name, "itunes:author") == 0
			 || (g_ascii_strcasecmp (node->name, "generator") == 0 && author == NULL)) {
		    	author = node->data;
		} else if (g_ascii_strcasecmp (node->name, "webMaster") == 0) {
			contact = node->data;
		} else if (g_ascii_strcasecmp (node->name, "image") == 0) {
			img = node->data;
		} else if (g_ascii_strcasecmp (node->name, "itunes:image") == 0) {
			const char *href;

			href = xml_parser_get_property (node, "href");
			if (href != NULL)
				img = href;
		} else if (g_ascii_strcasecmp (node->name, "lastBuildDate") == 0
			 || g_ascii_strcasecmp (node->name, "pubDate") == 0) {
		    	pub_date = node->data;
		} else if (g_ascii_strcasecmp (node->name, "copyright") == 0) {
			copyright = node->data;
		}

		if (g_ascii_strcasecmp (node->name, "item") == 0) {
			if (started == FALSE) {
				/* Send the info we already have about the feed */
				totem_pl_parser_add_url (parser,
							 TOTEM_PL_PARSER_FIELD_IS_PLAYLIST, TRUE,
							 TOTEM_PL_PARSER_FIELD_URL, url,
							 TOTEM_PL_PARSER_FIELD_TITLE, title,
							 TOTEM_PL_PARSER_FIELD_LANGUAGE, language,
							 TOTEM_PL_PARSER_FIELD_DESCRIPTION, description,
							 TOTEM_PL_PARSER_FIELD_AUTHOR, author,
							 TOTEM_PL_PARSER_FIELD_PUB_DATE, pub_date,
							 TOTEM_PL_PARSER_FIELD_COPYRIGHT, copyright,
							 TOTEM_PL_PARSER_FIELD_IMAGE_URL, img,
							 NULL);
				started = TRUE;
			}

			parse_rss_item (parser, node);
		}
	}

	totem_pl_parser_playlist_end (parser, url);

	return TOTEM_PL_PARSER_RESULT_SUCCESS;
}

TotemPlParserResult
totem_pl_parser_add_rss (TotemPlParser *parser,
			 const char *url,
			 const char *base,
			 gpointer data)
{
	xml_node_t* doc, *channel;
	char *contents;
	int size;

	if (gnome_vfs_read_entire_file (url, &size, &contents) != GNOME_VFS_OK)
		return TOTEM_PL_PARSER_RESULT_ERROR;

	xml_parser_init (contents, size, XML_PARSER_CASE_INSENSITIVE);
	if (xml_parser_build_tree_with_options (&doc, XML_PARSER_RELAXED | XML_PARSER_MULTI_TEXT) < 0) {
		g_free (contents);
		return TOTEM_PL_PARSER_RESULT_ERROR;
	}
	/* If the document has no name */
	if (doc->name == NULL
	    || g_ascii_strcasecmp (doc->name , "rss") != 0) {
		g_free (contents);
		xml_parser_free_tree (doc);
		return TOTEM_PL_PARSER_RESULT_ERROR;
	}

	for (channel = doc->child; channel != NULL; channel = channel->next) {
		if (g_ascii_strcasecmp (channel->name, "channel") == 0) {
			parse_rss_items (parser, url, channel);
			/* One channel per file */
			break;
		}
	}

	g_free (contents);
	xml_parser_free_tree (doc);

	return TOTEM_PL_PARSER_RESULT_SUCCESS;
}

TotemPlParserResult
totem_pl_parser_add_itpc (TotemPlParser *parser,
			  const char *url,
			  const char *base,
			  gpointer data)
{
	TotemPlParserResult ret;
	char *new_url;

	new_url = g_strdup (url);
	memcpy (new_url, "http", 4);
	ret = totem_pl_parser_add_rss (parser, url, base, data);
	g_free (new_url);

	return ret;
}

static TotemPlParserResult
parse_atom_entry (TotemPlParser *parser, xml_node_t *parent)
{
	const char *title, *author, *img, *url, *filesize;
	const char *copyright, *pub_date, *description;
	xml_node_t *node;

	title = author = img = url = filesize = NULL;
	copyright = pub_date = description = NULL;

	for (node = parent->child; node != NULL; node = node->next) {
		if (node->name == NULL)
			continue;

		if (g_ascii_strcasecmp (node->name, "title") == 0) {
			title = node->data;
		} else if (g_ascii_strcasecmp (node->name, "author") == 0) {
			//FIXME
		} else if (g_ascii_strcasecmp (node->name, "logo") == 0) {
			img = node->data;
		} else if (g_ascii_strcasecmp (node->name, "link") == 0) {
			const char *rel;

			//FIXME how do we choose the default enclosure type?
			rel = xml_parser_get_property (node, "rel");
			if (g_ascii_strcasecmp (rel, "enclosure") == 0) {
				const char *href;

				//FIXME what's the difference between url and href there?
				href = xml_parser_get_property (node, "href");
				if (href == NULL)
					continue;
				url = href;
				filesize = xml_parser_get_property (node, "length");
			} else if (g_ascii_strcasecmp (node->name, "license") == 0) {
				const char *href;

				href = xml_parser_get_property (node, "href");
				if (href == NULL)
					continue;
				/* This isn't really a copyright, but what the hey */
				copyright = href;
			}
		} else if (g_ascii_strcasecmp (node->name, "updated") == 0
			   || (g_ascii_strcasecmp (node->name, "modified") == 0 && pub_date == NULL)) {
			pub_date = node->data;
		} else if (g_ascii_strcasecmp (node->name, "summary") == 0
			   || (g_ascii_strcasecmp (node->name, "content") == 0 && description == NULL)) {
			const char *type;

			type = xml_parser_get_property (node, "content");
			if (type != NULL && g_ascii_strcasecmp (type, "text/plain") == 0)
				description = node->data;
		}
		//FIXME handle category
	}

	if (url != NULL) {
		totem_pl_parser_add_url (parser,
					 TOTEM_PL_PARSER_FIELD_TITLE, title,
					 TOTEM_PL_PARSER_FIELD_AUTHOR, author,
					 TOTEM_PL_PARSER_FIELD_URL, url,
					 TOTEM_PL_PARSER_FIELD_FILESIZE, filesize,
					 TOTEM_PL_PARSER_FIELD_COPYRIGHT, copyright,
					 TOTEM_PL_PARSER_FIELD_PUB_DATE, pub_date,
					 TOTEM_PL_PARSER_FIELD_DESCRIPTION, description,
					 NULL);
	}

	return TOTEM_PL_PARSER_RESULT_SUCCESS;
}

static TotemPlParserResult
parse_atom_entries (TotemPlParser *parser, const char *url, xml_node_t *parent)
{
	const char *title, *pub_date, *description;
	const char *author, *img;
	xml_node_t *node;
	gboolean started = FALSE;

	title = pub_date = description = NULL;
	author = img = NULL;

	for (node = parent->child; node != NULL; node = node->next) {
		if (node->name == NULL)
			continue;

		if (g_ascii_strcasecmp (node->name, "title") == 0) {
			title = node->data;
		} else if (g_ascii_strcasecmp (node->name, "tagline") == 0) {
		    	description = node->data;
		} else if (g_ascii_strcasecmp (node->name, "modified") == 0
			   || g_ascii_strcasecmp (node->name, "updated") == 0) {
			pub_date = node->data;
		} else if (g_ascii_strcasecmp (node->name, "author") == 0
			 || (g_ascii_strcasecmp (node->name, "generator") == 0 && author == NULL)) {
		    	author = node->data;
		} else if ((g_ascii_strcasecmp (node->name, "icon") == 0 && img == NULL)
			   || g_ascii_strcasecmp (node->name, "logo") == 0) {
			img = node->data;
		}

		if (g_ascii_strcasecmp (node->name, "entry") == 0) {
			if (started == FALSE) {
				/* Send the info we already have about the feed */
				totem_pl_parser_add_url (parser,
							 TOTEM_PL_PARSER_FIELD_IS_PLAYLIST, TRUE,
							 TOTEM_PL_PARSER_FIELD_URL, url,
							 TOTEM_PL_PARSER_FIELD_TITLE, title,
							 TOTEM_PL_PARSER_FIELD_DESCRIPTION, description,
							 TOTEM_PL_PARSER_FIELD_AUTHOR, author,
							 TOTEM_PL_PARSER_FIELD_PUB_DATE, pub_date,
							 TOTEM_PL_PARSER_FIELD_IMAGE_URL, img,
							 NULL);
				started = TRUE;
			}

			parse_atom_entry (parser, node);
		}
	}

	totem_pl_parser_playlist_end (parser, url);

	return TOTEM_PL_PARSER_RESULT_SUCCESS;
}

TotemPlParserResult
totem_pl_parser_add_atom (TotemPlParser *parser,
			  const char *url,
			  const char *base,
			  gpointer data)
{
	xml_node_t* doc;
	char *contents;
	int size;

	if (gnome_vfs_read_entire_file (url, &size, &contents) != GNOME_VFS_OK)
		return TOTEM_PL_PARSER_RESULT_ERROR;

	xml_parser_init (contents, size, XML_PARSER_CASE_INSENSITIVE);
	if (xml_parser_build_tree_with_options (&doc, XML_PARSER_RELAXED | XML_PARSER_MULTI_TEXT) < 0) {
		g_free (contents);
		return TOTEM_PL_PARSER_RESULT_ERROR;
	}
	/* If the document has no name */
	if (doc->name == NULL
	    || g_ascii_strcasecmp (doc->name , "feed") != 0) {
		g_free (contents);
		xml_parser_free_tree (doc);
		return TOTEM_PL_PARSER_RESULT_ERROR;
	}

	parse_atom_entries (parser, url, doc);

	g_free (contents);
	xml_parser_free_tree (doc);

	return TOTEM_PL_PARSER_RESULT_SUCCESS;
}

TotemPlParserResult
totem_pl_parser_add_xml_feed (TotemPlParser *parser,
			      const char *url,
			      const char *base,
			      gpointer data)
{
	return TOTEM_PL_PARSER_RESULT_ERROR;
}

#endif /* !TOTEM_PL_PARSER_MINI */

