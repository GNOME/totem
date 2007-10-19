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
#include <zlib.h>
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
		} else if (g_ascii_strcasecmp (node->name, "description") == 0
			   || g_ascii_strcasecmp (node->name, "itunes:summary") == 0) {
			description = node->data;
		} else if (g_ascii_strcasecmp (node->name, "author") == 0
			   || g_ascii_strcasecmp (node->name, "itunes:author") == 0) {
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

		if (g_ascii_strcasecmp (node->name, "title") == 0) {
			title = node->data;
		} else if (g_ascii_strcasecmp (node->name, "language") == 0) {
			language = node->data;
		} else if (g_ascii_strcasecmp (node->name, "description") == 0
			 || g_ascii_strcasecmp (node->name, "itunes:subtitle") == 0) {
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

/* http://www.apple.com/itunes/store/podcaststechspecs.html */
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
	ret = totem_pl_parser_add_rss (parser, new_url, base, data);
	g_free (new_url);

	return ret;
}

/* Atom docs:
 * http://www.atomenabled.org/developers/syndication/atom-format-spec.php#rfc.section.4.1
 * http://tools.ietf.org/html/rfc4287
 * http://tools.ietf.org/html/rfc4946 */
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

/* From libgsf's gsf-utils.h */
#define GSF_LE_GET_GUINT32(p)				\
	(guint32)((((guint8 const *)(p))[0] << 0)  |	\
		  (((guint8 const *)(p))[1] << 8)  |	\
		  (((guint8 const *)(p))[2] << 16) |	\
		  (((guint8 const *)(p))[3] << 24))

/* From libgsf's gsf-input-gzip.c */
/* gzip flag byte */
#define GZIP_IS_ASCII		0x01 /* file contains text ? */
#define GZIP_HEADER_CRC		0x02 /* there is a CRC in the header */
#define GZIP_EXTRA_FIELD	0x04 /* there is an 'extra' field */
#define GZIP_ORIGINAL_NAME	0x08 /* the original is stored */
#define GZIP_HAS_COMMENT	0x10 /* There is a comment in the header */
#define GZIP_HEADER_FLAGS (unsigned)(GZIP_IS_ASCII |GZIP_HEADER_CRC |GZIP_EXTRA_FIELD |GZIP_ORIGINAL_NAME |GZIP_HAS_COMMENT)

static guint32
check_header (char *data, gsize len)
{
	static guint8 const signature[2] = {0x1f, 0x8b};
	unsigned flags;

	if (len < 2 + 1 + 1 + 6)
		return -1;

	/* Check signature */
	if (memcmp (data, signature, sizeof (signature)) != 0)
		return -1;

	/* verify flags and compression type */
	flags  = data[3];
	if (data[2] != Z_DEFLATED || (flags & ~GZIP_HEADER_FLAGS) != 0)
		return -1;

	/* Get the uncompressed size */
	/* FIXME, but how?  The size read here is modulo 2^32.  */
	return GSF_LE_GET_GUINT32 (data + len - 4);
}

static char *
decompress_gzip (char *data, gsize len)
{
	guint32 retlen;
	char *ret;
	int zerr;
	z_stream  stream;

	retlen = check_header (data, len);
	if (retlen < 0)
		return NULL;

	stream.zalloc    = (alloc_func)0;
	stream.zfree     = (free_func)0;
	stream.opaque    = (voidpf)0;
	stream.next_in   = Z_NULL;
	stream.next_out  = Z_NULL;
	stream.avail_in  = stream.avail_out = 0;

	/* 16 + MAX_WBITS as per http://hewgill.com/journal/entries/349 */
	if (Z_OK != inflateInit2 (&stream, 16 + MAX_WBITS))
		return NULL;

	/* +1 so it's NULL-terminated */
	ret = g_malloc0 (retlen + 1);

	stream.next_in = (unsigned char *) data;
	stream.avail_in = len;

	stream.next_out = (unsigned char *) ret;
	stream.avail_out = retlen;

	zerr = inflate (&stream, Z_NO_FLUSH);
	if (zerr != Z_OK && zerr != Z_STREAM_END) {
		g_free (ret);
		return NULL;
	}
	zerr = inflateEnd (&stream);
	if (zerr != Z_OK) {
		g_free (ret);
		return NULL;
	}

	return ret;
}

static const char *
totem_pl_parser_parse_itms_doc (xml_node_t *item)
{
	for (item = item->child; item != NULL; item = item->next) {
		/* What we're looking for looks like:
		 * <key>feedURL</key><string>URL</string> */
		if (g_ascii_strcasecmp (item->name, "key") == 0
		    && g_ascii_strcasecmp (item->data, "feedURL") == 0) {
			item = item->next;
			if (g_ascii_strcasecmp (item->name, "string") == 0)
				return item->data;
		} else {
			const char *ret;

			ret = totem_pl_parser_parse_itms_doc (item);
			if (ret != NULL)
				return ret;
		}
	}

	return NULL;
}

static char *
totem_pl_parser_get_feed_url (const char *data, gsize len)
{
	xml_node_t* doc;
	const char *url;
	char *ret;

	url = NULL;

	xml_parser_init (data, len, XML_PARSER_CASE_INSENSITIVE);
	if (xml_parser_build_tree_with_options (&doc, XML_PARSER_RELAXED | XML_PARSER_MULTI_TEXT) < 0)
		return NULL;

	/* If the document has no name */
	if (doc->name == NULL
	    || g_ascii_strcasecmp (doc->name , "Document") != 0) {
		xml_parser_free_tree (doc);
		return NULL;
	}

	url = totem_pl_parser_parse_itms_doc (doc);
	if (url == NULL) {
		xml_parser_free_tree (doc);
		return NULL;
	}

	ret = g_strdup (url);
	xml_parser_free_tree (doc);

	return ret;
}

static char *
totem_pl_parser_get_itms_url (const char *data)
{
	char *s, *end, *ret;
#define ITMS_OPEN "<body onload=\"return itmsOpen('"

	/* The bit of text looks like:
	 * <body onload="return itmsOpen('itms://ax.phobos.apple.com.edgesuite.net/WebObjects/MZStore.woa/wa/viewPodcast?id=207870198&amp;ign-mscache=1','http://www.apple.com/uk/itunes/affiliates/download/?itmsUrl=itms%3A%2F%2Fax.phobos.apple.com.edgesuite.net%2FWebObjects%2FMZStore.woa%2Fwa%2FviewPodcast%3Fid%3D207870198%26ign-mscache%3D1','userOverridePanel',false)"> */

	s = strstr (data, ITMS_OPEN);
	if (s == NULL)
		return NULL;
	s = s + strlen (ITMS_OPEN);
	end = strchr (s, '\'');
	if (end == NULL)
		return NULL;

	ret = g_strndup (s, end - s);
	memcpy (ret, "http", 4);
	return ret;
}

TotemPlParserResult
totem_pl_parser_add_itms (TotemPlParser *parser,
			  const char *url,
			  const char *base,
			  gpointer data)
{
	char *contents, *uncompressed, *itms_url, *feed_url;
	TotemPlParserResult ret;
	int size;

	/* Get the webpage */
	if (gnome_vfs_read_entire_file (url, &size, &contents) != GNOME_VFS_OK)
		return TOTEM_PL_PARSER_RESULT_ERROR;

	uncompressed = decompress_gzip (contents, size);
	g_free (contents);
	if (uncompressed == NULL)
		return TOTEM_PL_PARSER_RESULT_ERROR;

	/* Look for the link to the itms on phobos */
	itms_url = totem_pl_parser_get_itms_url (uncompressed);
	g_free (uncompressed);

	/* Get the phobos linked, in some weird iTunes only format */
	if (gnome_vfs_read_entire_file (itms_url, &size, &contents) != GNOME_VFS_OK) {
		g_free (itms_url);
		return TOTEM_PL_PARSER_RESULT_ERROR;
	}
	g_free (itms_url);

	uncompressed = decompress_gzip (contents, size);
	g_free (contents);
	if (uncompressed == NULL)
		return TOTEM_PL_PARSER_RESULT_ERROR;

	/* And look in the file for the feedURL */
	feed_url = totem_pl_parser_get_feed_url (uncompressed, strlen (uncompressed) + 1);
	if (feed_url == NULL)
		return TOTEM_PL_PARSER_RESULT_ERROR;

	ret = totem_pl_parser_add_rss (parser, feed_url, NULL, NULL);
	g_free (feed_url);

	return ret;
}

gboolean
totem_pl_parser_is_itms_feed (const char *url)
{
	g_return_val_if_fail (url != NULL, FALSE);

	if (strstr (url, "phobos.apple.com/") != NULL
	    && strstr (url, "viewPodcast") != NULL)
		return TRUE;
	if (strstr (url, "itunes.com/podcast") != NULL)
		return TRUE;

	return FALSE;
}

#endif /* !TOTEM_PL_PARSER_MINI */

