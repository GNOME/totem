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

#ifndef TOTEM_PL_PARSER_H
#define TOTEM_PL_PARSER_H

#include <glib.h>

#include <gtk/gtktreemodel.h>
#include "totem-pl-parser-features.h"
#include "totem-pl-parser-builtins.h"

G_BEGIN_DECLS

#define TOTEM_TYPE_PL_PARSER            (totem_pl_parser_get_type ())
#define TOTEM_PL_PARSER(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), TOTEM_TYPE_PL_PARSER, TotemPlParser))
#define TOTEM_PL_PARSER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), TOTEM_TYPE_PL_PARSER, TotemPlParserClass))
#define TOTEM_IS_PL_PARSER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), TOTEM_TYPE_PL_PARSER))
#define TOTEM_IS_PL_PARSER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), TOTEM_TYPE_PL_PARSER))

typedef enum
{
	TOTEM_PL_PARSER_RESULT_UNHANDLED,
	TOTEM_PL_PARSER_RESULT_ERROR,
	TOTEM_PL_PARSER_RESULT_SUCCESS,
	TOTEM_PL_PARSER_RESULT_IGNORED
} TotemPlParserResult;

typedef struct TotemPlParser	       TotemPlParser;
typedef struct TotemPlParserClass      TotemPlParserClass;
typedef struct TotemPlParserPrivate    TotemPlParserPrivate;

struct TotemPlParser {
	GObject parent;
	TotemPlParserPrivate *priv;
};

/* Known metadata fields */
#define TOTEM_PL_PARSER_FIELD_URL		"url"
#define TOTEM_PL_PARSER_FIELD_GENRE		"genre"
#define TOTEM_PL_PARSER_FIELD_TITLE		"title"
#define TOTEM_PL_PARSER_FIELD_AUTHOR		"author"
#define TOTEM_PL_PARSER_FIELD_BASE		"base"
#define TOTEM_PL_PARSER_FIELD_VOLUME		"volume"
#define TOTEM_PL_PARSER_FIELD_AUTOPLAY		"autoplay"
#define TOTEM_PL_PARSER_FIELD_DURATION		"duration"
#define TOTEM_PL_PARSER_FIELD_STARTTIME		"starttime"
#define TOTEM_PL_PARSER_FIELD_ENDTIME		"endtime"
#define TOTEM_PL_PARSER_FIELD_COPYRIGHT		"copyright"
#define TOTEM_PL_PARSER_FIELD_ABSTRACT		"abstract"
#define TOTEM_PL_PARSER_FIELD_SUMMARY		TOTEM_PL_PARSER_FIELD_ABSTRACT
#define TOTEM_PL_PARSER_FIELD_DESCRIPTION	"description"
#define TOTEM_PL_PARSER_FIELD_MOREINFO		"moreinfo"
#define TOTEM_PL_PARSER_FIELD_SCREENSIZE	"screensize"
#define TOTEM_PL_PARSER_FIELD_UI_MODE		"ui-mode"
#define TOTEM_PL_PARSER_FIELD_PUB_DATE		"publication-date"
#define TOTEM_PL_PARSER_FIELD_FILESIZE		"filesize"
#define TOTEM_PL_PARSER_FIELD_LANGUAGE		"language"
#define TOTEM_PL_PARSER_FIELD_CONTACT		"contact"
#define TOTEM_PL_PARSER_FIELD_IMAGE_URL		"image-url"

#define TOTEM_PL_PARSER_FIELD_IS_PLAYLIST	"is-playlist"

struct TotemPlParserClass {
	GObjectClass parent_class;

	/* signals */
	void (*entry_parsed) (TotemPlParser *parser, const char *uri,
			      GHashTable *metadata);
	void (*playlist_started) (TotemPlParser *parser,
				  const char *uri,
				  GHashTable *metadata);
	void (*playlist_ended) (TotemPlParser *parser,
				const char *uri);
};

typedef enum
{
	TOTEM_PL_PARSER_PLS,
	TOTEM_PL_PARSER_M3U,
	TOTEM_PL_PARSER_M3U_DOS,
	TOTEM_PL_PARSER_XSPF,
	TOTEM_PL_PARSER_IRIVER_PLA,
} TotemPlParserType;

typedef enum
{
	TOTEM_PL_PARSER_ERROR_VFS_OPEN,
	TOTEM_PL_PARSER_ERROR_VFS_WRITE,
} TotemPlParserError;

#define TOTEM_PL_PARSER_ERROR (totem_pl_parser_error_quark ())

GQuark totem_pl_parser_error_quark (void);

typedef void (*TotemPlParserIterFunc) (GtkTreeModel *model, GtkTreeIter *iter,
				       char **uri, char **title,
				       gboolean *custom_title,
				       gpointer user_data);

GType    totem_pl_parser_get_type (void);

gint64   totem_plparser_parse_duration (const char *duration, gboolean debug);

gboolean totem_pl_parser_write (TotemPlParser *parser, GtkTreeModel *model,
				TotemPlParserIterFunc func,
				const char *output, TotemPlParserType type,
				gpointer user_data,
				GError **error);

gboolean   totem_pl_parser_write_with_title (TotemPlParser *parser,
					     GtkTreeModel *model,
					     TotemPlParserIterFunc func,
					     const char *output,
					     const char *title,
					     TotemPlParserType type,
					     gpointer user_data,
					     GError **error);

void	   totem_pl_parser_add_ignored_scheme (TotemPlParser *parser,
					       const char *scheme);
void       totem_pl_parser_add_ignored_mimetype (TotemPlParser *parser,
						 const char *mimetype);

TotemPlParserResult totem_pl_parser_parse (TotemPlParser *parser,
					   const char *url, gboolean fallback);
TotemPlParserResult totem_pl_parser_parse_with_base (TotemPlParser *parser,
						     const char *url,
						     const char *base,
						     gboolean fallback);

TotemPlParser *totem_pl_parser_new (void);

G_END_DECLS

#endif /* TOTEM_PL_PARSER_H */
