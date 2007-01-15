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
#include <string.h>
#include <glib.h>
#include <gtk/gtk.h>
#include <libgnomevfs/gnome-vfs.h>
#include "totem-pl-parser.h"
#include "totemplparser-marshal.h"
#include "totem-disc.h"
#endif /* !TOTEM_PL_PARSER_MINI */

#include "totem-pl-parser-mini.h"
#include "totem-pl-parser-media.h"
#include "totem-pl-parser-private.h"

#ifndef TOTEM_PL_PARSER_MINI
/* Returns NULL if we don't have an ISO image,
 * or an empty string if it's non-UTF-8 data */
static char *
totem_pl_parser_iso_get_title (const char *url)
{
	char *fname;
	FILE  *file;
#define BUFFER_SIZE 128
	char buf [BUFFER_SIZE+1];
	int res;
	char *str;

	fname = g_filename_from_uri (url, NULL, NULL);
	if (fname == NULL)
		return NULL;

	file = fopen (fname, "rb");
	if (file == NULL)
		return NULL;

	/* Verify we have an ISO image */
	/* This check is for the raw sector images */
	res = fseek (file, 37633L, SEEK_SET);
	if (res != 0) {
		fclose (file);
		return NULL;
	}

	res = fread (buf, sizeof (char), 5, file);
	if (res != 5 || strncmp (buf, "CD001", 5) != 0) {
		/* Standard ISO images */
		res = fseek (file, 32769L, SEEK_SET);
		if (res != 0) {
			fclose (file);
			return NULL;
		}
		res = fread (buf, sizeof (char), 5, file);
		if (res != 5 || strncmp (buf, "CD001", 5) != 0) {
			/* High Sierra images */
			res = fseek (file, 32776L, SEEK_SET);
			if (res != 0) {
				fclose (file);
				return NULL;
			}
			res = fread (buf, sizeof (char), 5, file);
			if (res != 5 || strncmp (buf, "CDROM", 5) != 0) {
				fclose (file);
				return NULL;
			}
		}
	}
	/* Extract the volume label from the image */
	res = fseek (file, 32808L, SEEK_SET);
	if (res != 0) {
		fclose (file);
		return NULL;
	}
	res = fread (buf, sizeof(char), BUFFER_SIZE, file);
	fclose (file);
	if (res != BUFFER_SIZE)
		return NULL;

	buf [BUFFER_SIZE] = '\0';
	str = g_strdup (g_strstrip (buf));
	if (!g_utf8_validate (str, -1, NULL)) {
		g_free (str);
		return g_strdup ("");
	}

	return str;
}

TotemPlParserResult
totem_pl_parser_add_iso (TotemPlParser *parser, const char *url,
			 const char *base, gpointer data)
{
	GnomeVFSFileInfo *info;
	char *item, *label;

	/* This is a hack, it could be a VCD or DVD */
	if (g_str_has_prefix (url, "file://") == FALSE)
		return TOTEM_PL_PARSER_RESULT_IGNORED;

	label = totem_pl_parser_iso_get_title (url);
	if (label == NULL) {
		/* Not an ISO image */
		return TOTEM_PL_PARSER_RESULT_UNHANDLED;
	}
	if (label[0] == '\0') {
		g_free (label);
		label = NULL;
	}

	info = gnome_vfs_file_info_new ();
	if (gnome_vfs_get_file_info (url, info, GNOME_VFS_FILE_INFO_FOLLOW_LINKS) != GNOME_VFS_OK) {
		gnome_vfs_file_info_unref (info);
		return TOTEM_PL_PARSER_RESULT_IGNORED;
	}

	/* Less than 700 megs, and it's a VCD */
	if (info->size < 700 * 1024 * 1024) {
		item = totem_cd_mrl_from_type ("vcd", url);
	} else {
		item = totem_cd_mrl_from_type ("dvd", url);
	}

	gnome_vfs_file_info_unref (info);

	totem_pl_parser_add_one_url (parser, item, label);
	g_free (label);
	g_free (item);

	return TOTEM_PL_PARSER_RESULT_SUCCESS;
}

TotemPlParserResult
totem_pl_parser_add_cue (TotemPlParser *parser, const char *url,
			 const char *base, gpointer data)
{
	char *vcdurl;

	vcdurl = totem_cd_mrl_from_type ("vcd", url);
	totem_pl_parser_add_one_url (parser, vcdurl, NULL);
	g_free (vcdurl);

	return TOTEM_PL_PARSER_RESULT_SUCCESS;
}

static int
totem_pl_parser_dir_compare (GnomeVFSFileInfo *a, GnomeVFSFileInfo *b)
{
	if (a->name == NULL) {
		if (b->name == NULL)
			return 0;
		else
			return -1;
	} else {
		if (b->name == NULL)
			return 1;
		else
			return strcmp (a->name, b->name);
	}
}

TotemPlParserResult
totem_pl_parser_add_directory (TotemPlParser *parser, const char *url,
			       const char *base, gpointer data)
{
	MediaType type;
	GList *list, *l;
	GnomeVFSResult res;
	char *media_url;

	type = totem_cd_detect_type_from_dir (url, &media_url, NULL);
	if (type != MEDIA_TYPE_DATA && type != MEDIA_TYPE_ERROR) {
		if (media_url != NULL) {
			char *basename = NULL, *fname;

			fname = g_filename_from_uri (url, NULL, NULL);
			if (fname != NULL) {
				basename = g_filename_display_basename (fname);
				g_free (fname);
			}
			totem_pl_parser_add_one_url (parser, media_url, basename);
			g_free (basename);
			g_free (media_url);
			return TOTEM_PL_PARSER_RESULT_SUCCESS;
		}
	}

	res = gnome_vfs_directory_list_load (&list, url,
			GNOME_VFS_FILE_INFO_DEFAULT);
	if (res != GNOME_VFS_OK)
		return TOTEM_PL_PARSER_RESULT_ERROR;

	list = g_list_sort (list, (GCompareFunc) totem_pl_parser_dir_compare);
	l = list;

	while (l != NULL) {
		char *name, *fullpath;
		GnomeVFSFileInfo *info = l->data;
		TotemPlParserResult ret;

		if (info->name != NULL && (strcmp (info->name, ".") == 0
					|| strcmp (info->name, "..") == 0)) {
			l = l->next;
			continue;
		}

		name = gnome_vfs_escape_string (info->name);
		fullpath = g_strconcat (url, "/", name, NULL);
		g_free (name);

		ret = totem_pl_parser_parse_internal (parser, fullpath, NULL);
		if (ret != TOTEM_PL_PARSER_RESULT_SUCCESS && ret != TOTEM_PL_PARSER_RESULT_IGNORED)
			totem_pl_parser_add_one_url (parser, fullpath, NULL);

		l = l->next;
	}

	g_list_foreach (list, (GFunc) gnome_vfs_file_info_unref, NULL);
	g_list_free (list);

	return TOTEM_PL_PARSER_RESULT_SUCCESS;
}

TotemPlParserResult
totem_pl_parser_add_block (TotemPlParser *parser, const char *url,
			   const char *base, gpointer data)
{
	MediaType type;
	char *media_url;
	GError *err = NULL;

	type = totem_cd_detect_type_with_url (url, &media_url, &err);
	if (err != NULL)
		DEBUG(g_print ("Couldn't get CD type for URL '%s': %s\n", url, err->message));
	if (type == MEDIA_TYPE_DATA || media_url == NULL)
		return TOTEM_PL_PARSER_RESULT_UNHANDLED;
	else if (type == MEDIA_TYPE_ERROR)
		return TOTEM_PL_PARSER_RESULT_ERROR;

	totem_pl_parser_add_one_url (parser, media_url, NULL);
	g_free (media_url);
	return TOTEM_PL_PARSER_RESULT_SUCCESS;
}

#endif /* !TOTEM_PL_PARSER_MINI */


