/*
 *  Copyright (C) 2002-2003,2007 the xine project
 *
 *  This file is part of xine, a free video player.
 *
 * The xine-lib XML parser is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * The xine-lib XML parser is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with the Gnome Library; see the file COPYING.LIB.  If not,
 * write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * $Id: xmllexer.h,v 1.8 2007/03/04 16:19:12 hadess Exp $
 *
 */

/* xml lexer */
#ifndef XML_LEXER_H
#define XML_LEXER_H

#ifndef XINE_PROTECTED
#define XINE_PROTECTED
#endif

/* public constants */
#define T_ERROR         -1   /* lexer error */
#define T_EOF            0   /* end of file */
#define T_EOL            1   /* end of line */
#define T_SEPAR          2   /* separator ' ' '/t' '\n' '\r' */
#define T_M_START_1      3   /* markup start < */
#define T_M_START_2      4   /* markup start </ */
#define T_M_STOP_1       5   /* markup stop > */
#define T_M_STOP_2       6   /* markup stop /> */
#define T_EQUAL          7   /* = */
#define T_QUOTE          8   /* \" or \' */
#define T_STRING         9   /* "string" */
#define T_IDENT         10   /* identifier */
#define T_DATA          11   /* data */
#define T_C_START       12   /* <!-- */
#define T_C_STOP        13   /* --> */
#define T_TI_START      14   /* <? */
#define T_TI_STOP       15   /* ?> */
#define T_DOCTYPE_START 16   /* <!DOCTYPE */
#define T_DOCTYPE_STOP  17   /* > */


/* public functions */
void lexer_init(const char * buf, int size) XINE_PROTECTED;
int lexer_get_token(char * tok, int tok_size) XINE_PROTECTED;
char *lexer_decode_entities (const char *tok) XINE_PROTECTED;

#endif
