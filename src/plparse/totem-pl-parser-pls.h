/* 
   2002, 2003, 2004, 2005, 2006, 2007 Bastien Nocera
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

#ifndef TOTEM_PL_PARSER_PLS_H
#define TOTEM_PL_PARSER_PLS_H

G_BEGIN_DECLS

#ifdef TOTEM_PL_PARSER_MINI
#include "totem-pl-parser-mini.h"
#endif /* TOTEM_PL_PARSER_MINI */

#ifndef TOTEM_PL_PARSER_MINI
gboolean totem_pl_parser_write_pls				(TotemPlParser *parser,
								 GtkTreeModel *model,
								 TotemPlParserIterFunc func, 
								 const char *output,
								 const char *title,
								 gpointer user_data,
								 GError **error);
TotemPlParserResult totem_pl_parser_add_pls_with_contents	(TotemPlParser *parser,
								 const char *url,
								 const char *contents);
TotemPlParserResult totem_pl_parser_add_pls			(TotemPlParser *parser,
								 const char *url,
								 const char *base,
								 gpointer data);
#endif /* !TOTEM_PL_PARSER_MINI */

G_END_DECLS

#endif /* TOTEM_PL_PARSER_PLS_H */
