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
#include "totem-pl-parser.h"
#include "totemplparser-marshal.h"
#include "totem-disc.h"
#endif /* !TOTEM_PL_PARSER_MINI */

#include "totem-pl-parser-mini.h"
#include "totem-pl-parser-podcast.h"
#include "totem-pl-parser-private.h"

#ifndef TOTEM_PL_PARSER_MINI
TotemPlParserResult
totem_pl_parser_add_rss (TotemPlParser *parser,
			 const char *url,
			 const char *base,
			 gpointer data)
{
	return TOTEM_PL_PARSER_RESULT_UNHANDLED;
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

TotemPlParserResult
totem_pl_parser_add_atom (TotemPlParser *parser,
			  const char *url,
			  const char *base,
			  gpointer data)
{
	return TOTEM_PL_PARSER_RESULT_ERROR;
}

#endif /* !TOTEM_PL_PARSER_MINI */


