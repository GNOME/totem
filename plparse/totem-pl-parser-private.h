/*
   2002, 2003, 2004, 2005, 2006 Bastien Nocera
   Copyright (C) 2003 Colin Walters <walters@verbum.org>

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

#ifndef TOTEM_PL_PARSER_PRIVATE_H
#define TOTEM_PL_PARSER_PRIVATE_H

#include <glib.h>

#ifndef TOTEM_PL_PARSER_MINI
#include "totem-pl-parser.h"
#include <glib-object.h>
#else
#include "totem-pl-parser-mini.h"
#endif /* !TOTEM_PL_PARSER_MINI */

#define MIME_READ_CHUNK_SIZE 1024
#define DEBUG(x) { if (parser->priv->debug) x; }

struct TotemPlParserPrivate
{
	GList *ignore_schemes;
	GList *ignore_mimetypes;
#ifndef TOTEM_PL_PARSER_MINI
	GParamSpecPool *pspec_pool;
#endif
	guint recurse_level;
	guint fallback : 1;
	guint recurse : 1;
	guint debug : 1;
	guint force : 1;
	guint disable_unsafe : 1;
};

char *totem_pl_parser_read_ini_line_string	(char **lines, const char *key,
						 gboolean dos_mode);
int   totem_pl_parser_read_ini_line_int		(char **lines, const char *key);
char *totem_pl_parser_read_ini_line_string_with_sep (char **lines, const char *key,
						     gboolean dos_mode, const char *sep);
char *totem_pl_parser_base_url			(const char *url);

#ifndef TOTEM_PL_PARSER_MINI
void totem_pl_parser_playlist_end		(TotemPlParser *parser,
						 const char *playlist_title);
int totem_pl_parser_num_entries			(TotemPlParser *parser,
						 GtkTreeModel *model,
						 TotemPlParserIterFunc func,
						 gpointer user_data);
gboolean totem_pl_parser_scheme_is_ignored	(TotemPlParser *parser,
						 const char *url);
gboolean totem_pl_parser_line_is_empty		(const char *line);
gboolean totem_pl_parser_write_string		(GnomeVFSHandle *handle,
						 const char *buf,
						 GError **error);
gboolean totem_pl_parser_write_buffer		(GnomeVFSHandle *handle,
						 const char *buf,
						 guint size,
						 GError **error);
char * totem_pl_parser_relative			(const char *url,
						 const char *output);
TotemPlParserResult totem_pl_parser_parse_internal (TotemPlParser *parser,
						    const char *url,
						    const char *base);
void totem_pl_parser_add_one_url		(TotemPlParser *parser,
						 const char *url,
						 const char *title);
void totem_pl_parser_add_url			(TotemPlParser *parser,
						 const char *first_property_name,
						 ...);
gboolean totem_pl_parser_ignore (TotemPlParser *parser, const char *url);
#endif /* !TOTEM_PL_PARSER_MINI */

G_END_DECLS

#endif /* TOTEM_PL_PARSER_PRIVATE_H */
