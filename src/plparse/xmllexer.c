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
 *  $Id: xmllexer.c,v 1.12 2006/06/20 00:35:08 dgp85 Exp $
 *
 */

#define LOG_MODULE "xmllexer"
#define LOG_VERBOSE
/*
#define LOG
*/

#ifdef XINE_COMPILE
#include "xineutils.h"
#else
#define lprintf(...)
#define xine_xmalloc malloc
#endif
#include "xmllexer.h"
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>

/* private constants*/
#define NORMAL       0  /* normal lex mode */
#define DATA         1  /* data lex mode */

/* private global variables */
static const char * lexbuf;
static int lexbuf_size = 0;
static int lexbuf_pos  = 0;
static int lex_mode    = NORMAL;
static int in_comment  = 0;

void lexer_init(const char * buf, int size) {
  lexbuf      = buf;
  lexbuf_size = size;
  lexbuf_pos  = 0;
  lex_mode    = NORMAL;
  in_comment  = 0;

  lprintf("buffer length %d\n", size);
}

int lexer_get_token(char * tok, int tok_size) {
  int tok_pos = 0;
  int state = 0;
  char c;

  if (tok) {
    while ((tok_pos < tok_size) && (lexbuf_pos < lexbuf_size)) {
      c = lexbuf[lexbuf_pos];
      lprintf("c=%c, state=%d, in_comment=%d\n", c, state, in_comment);

      if (lex_mode == NORMAL) {
				/* normal mode */
	switch (state) {
	  /* init state */
	case 0:
	  switch (c) {
	  case '\n':
	  case '\r':
	    state = 1;
	    tok[tok_pos] = c;
	    tok_pos++;
	    break;

	  case ' ':
	  case '\t':
	    state = 2;
	    tok[tok_pos] = c;
	    tok_pos++;
	    break;

	  case '<':
	    state = 3;
	    tok[tok_pos] = c;
	    tok_pos++;
	    break;

	  case '>':
	    state = 4;
	    tok[tok_pos] = c;
	    tok_pos++;
	    break;

	  case '/':
	    if (!in_comment) 
	      state = 5;
	    tok[tok_pos] = c;
	    tok_pos++;
	    break;

	  case '=':
	    state = 6;
	    tok[tok_pos] = c;
	    tok_pos++;
	    break;

	  case '\"': /* " */
	    state = 7;
	    break;

	  case '-':
	    state = 10;
	    tok[tok_pos] = c;
	    tok_pos++;
	    break;

	  case '?':
	    state = 9;
	    tok[tok_pos] = c;
	    tok_pos++;
	    break;

	  default:
	    state = 100;
	    tok[tok_pos] = c;
	    tok_pos++;
	    break;
	  }
	  lexbuf_pos++;
	  break;

	  /* end of line */
	case 1:
	  if (c == '\n' || (c == '\r')) {
	    tok[tok_pos] = c;
	    lexbuf_pos++;
	    tok_pos++;
	  } else {
	    tok[tok_pos] = '\0';
	    return T_EOL;
	  }
	  break;

	  /* T_SEPAR */
	case 2:
	  if (c == ' ' || (c == '\t')) {
	    tok[tok_pos] = c;
	    lexbuf_pos++;
	    tok_pos++;
	  } else {
	    tok[tok_pos] = '\0';
	    return T_SEPAR;
	  }
	  break;

	  /* T_M_START < or </ or <! or <? */
	case 3:
	  switch (c) {
	  case '/':
	    tok[tok_pos] = c;
	    lexbuf_pos++;
	    tok_pos++; /* FIXME */
	    tok[tok_pos] = '\0';
	    return T_M_START_2;
	    break;
	  case '!':
	    tok[tok_pos] = c;
	    lexbuf_pos++;
	    tok_pos++;
	    state = 8;
	    break;
	  case '?':
	    tok[tok_pos] = c;
	    lexbuf_pos++;
	    tok_pos++; /* FIXME */
	    tok[tok_pos] = '\0';
	    return T_TI_START;
	    break;
	  default:
	    tok[tok_pos] = '\0';
	    return T_M_START_1;
	  }
	  break;

	  /* T_M_STOP_1 */
	case 4:
	  tok[tok_pos] = '\0';
	  if (!in_comment)
	    lex_mode = DATA;
	  return T_M_STOP_1;
	  break;

	  /* T_M_STOP_2 */
	case 5:
	  if (c == '>') {
	    tok[tok_pos] = c;
	    lexbuf_pos++;
	    tok_pos++; /* FIXME */
	    tok[tok_pos] = '\0';
	    if (!in_comment)
	      lex_mode = DATA;
	    return T_M_STOP_2;
	  } else {
	    tok[tok_pos] = '\0';
	    return T_ERROR;
	  }
	  break;

	  /* T_EQUAL */
	case 6:
	  tok[tok_pos] = '\0';
	  return T_EQUAL;
	  break;

	  /* T_STRING */
	case 7:
	  tok[tok_pos] = c;
	  lexbuf_pos++;
	  if (c == '\"') { /* " */
	    tok[tok_pos] = '\0'; /* FIXME */
	    return T_STRING;
	  }
	  tok_pos++;
	  break;

	  /* T_C_START or T_DOCTYPE_START */
	case 8:
	  switch (c) {
	  case '-':
	    lexbuf_pos++;
	    if (lexbuf[lexbuf_pos] == '-')
	      {
		lexbuf_pos++;
		tok[tok_pos++] = '-'; /* FIXME */
		tok[tok_pos++] = '-';
		tok[tok_pos] = '\0';
		in_comment = 1;
		return T_C_START;
	      }
	    break;
	  case 'D':
	    lexbuf_pos++;
	    if (strncmp(lexbuf + lexbuf_pos, "OCTYPE", 6) == 0) {
	      strncpy(tok + tok_pos, "DOCTYPE", 7); /* FIXME */
	      lexbuf_pos += 6;
	      return T_DOCTYPE_START;
	    } else {
	      return T_ERROR;
	    }
	    break;
	  default:
	    /* error */
	    return T_ERROR;
	  }
	  break;

	  /* T_TI_STOP */
	case 9:
	  if (c == '>') {
	    tok[tok_pos] = c;
	    lexbuf_pos++;
	    tok_pos++; /* FIXME */
	    tok[tok_pos] = '\0';
	    return T_TI_STOP;
	  } else {
	    tok[tok_pos] = '\0';
	    return T_ERROR;
	  }
	  break;

	  /* -- */
	case 10:
	  switch (c) {
	  case '-':
	    tok[tok_pos] = c;
	    tok_pos++;
	    lexbuf_pos++;
	    state = 11;
	    break;
	  default:
	    tok[tok_pos] = c;
	    tok_pos++;
	    lexbuf_pos++;
	    state = 100;
	  }
	  break;

	  /* --> */
	case 11:
	  switch (c) {
	  case '>':
	    tok[tok_pos] = c;
	    tok_pos++;
	    lexbuf_pos++;
	    tok[tok_pos] = '\0'; /* FIX ME */
	    if (strlen(tok) != 3) {
	      tok[tok_pos - 3] = '\0';
	      lexbuf_pos -= 3;
	      return T_IDENT;
	    } else {
	      in_comment = 0;
	      return T_C_STOP;
	    }
	    break;
	  default:
	    tok[tok_pos] = c;
	    tok_pos++;
	    lexbuf_pos++;
	    state = 100;
	  }
	  break;

	  /* IDENT */
	case 100:
	  switch (c) {
	  case '<':
	  case '>':
	  case '\\':
	  case '\"': /* " */
	  case ' ':
	  case '\t':
	  case '=':
	  case '/':
	    tok[tok_pos] = '\0';
	    return T_IDENT;
	    break;
	  case '?':
	    tok[tok_pos] = c;
	    tok_pos++;
	    lexbuf_pos++;
	    state = 9;
	    break;
	  case '-':
	    tok[tok_pos] = c;
	    tok_pos++;
	    lexbuf_pos++;
	    state = 10;
	    break;
	  default:
	    tok[tok_pos] = c;
	    tok_pos++;
	    lexbuf_pos++;
	  }
	  break;
	default:
	  lprintf("expected char \'%c\'\n", tok[tok_pos - 1]); /* FIX ME */
	  return T_ERROR;
	}
      } else {
				/* data mode, stop if char equal '<' */
        switch (c)
        {
        case '<':
	  tok[tok_pos] = '\0';
	  lex_mode = NORMAL;
	  return T_DATA;
	default:
	  tok[tok_pos] = c;
	  tok_pos++;
	  lexbuf_pos++;
	}
      }
    }
    lprintf ("loop done tok_pos = %d, tok_size=%d, lexbuf_pos=%d, lexbuf_size=%d\n", 
	     tok_pos, tok_size, lexbuf_pos, lexbuf_size);

    /* pb */
    if (tok_pos >= tok_size) {
      lprintf("token buffer is too little\n");
    } else {
      if (lexbuf_pos >= lexbuf_size) {
				/* Terminate the current token */
	tok[tok_pos] = '\0';
	switch (state) {
	case 0:
	case 1:
	case 2:
	  return T_EOF;
	  break;
	case 3:
	  return T_M_START_1;
	  break;
	case 4:
	  return T_M_STOP_1;
	  break;
	case 5:
	  return T_ERROR;
	  break;
	case 6:
	  return T_EQUAL;
	  break;
	case 7:
	  return T_STRING;
	  break;
	case 100:
	  return T_DATA;
	  break;
	default:
	  lprintf("unknown state, state=%d\n", state);
	}
      } else {
	lprintf("abnormal end of buffer, state=%d\n", state);
      }
    }
    return T_ERROR;
  }
  /* tok == null */
  lprintf("token buffer is null\n");
  return T_ERROR;
}

static struct {
  char code;
  unsigned char namelen;
  char name[6];
} lexer_entities[] = {
  { '"',  4, "quot" },
  { '&',  3, "amp" },
  { '\'', 4, "apos" },
  { '<',  2, "lt" },
  { '>',  2, "gt" },
  { '\0', 0, "" }
};

char *lexer_decode_entities (const char *tok)
{
  char *buf = xine_xmalloc (strlen (tok) + 1);
  char *bp = buf;
  char c;

  while ((c = *tok++))
  {
    if (c != '&')
      *bp++ = c;
    else
    {
      /* parse the character entity (on failure, treat it as literal text) */
      const char *tp = tok;
      long i;

      for (i = 0; lexer_entities[i].code; ++i)
	if (!strncmp (lexer_entities[i].name, tok, lexer_entities[i].namelen)
	    && tok[lexer_entities[i].namelen] == ';')
	  break;
      if (lexer_entities[i].code)
      {
        tok += lexer_entities[i].namelen + 1;
	*bp++ = lexer_entities[i].code;
	continue;
      }

      if (*tp++ != '#')
      {
        /* not a recognised name and not numeric */
	*bp++ = '&';
	continue;
      }

      /* entity is a number
       * (note: strtol() allows "0x" prefix for hexadecimal, but we don't)
       */
      if (*tp == 'x' && tp[1] && tp[2] != 'x')
	i = strtol (tp + 1, (char **)&tp, 16);
      else
	i = strtol (tp, (char **)&tp, 10);

      if (i < 1 || i > 255 || *tp != ';')
      {
        /* out of range, or format error */
	*bp++ = '&';
	continue;
      }

      tok = tp + 1;
      *bp++ = i;
    }
  }
  *bp = 0;
  return buf;
}
