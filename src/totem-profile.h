/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2001,2002,2003 Bastien Nocera <hadess@hadess.net>
 *
 * SPDX-License-Identifier: GPL-3-or-later
 *
 */

#pragma once

#ifdef GNOME_ENABLE_DEBUG

#include <sys/time.h>
#include <glib.h>

#define TOTEM_PROFILE(function)     \
    do{                             \
      struct timeval current_time;  \
      double dtime;                 \
      gettimeofday(&current_time, NULL); \
      dtime = -(current_time.tv_sec + (current_time.tv_usec / 1000000.0)); \
      function;                     \
      gettimeofday(&current_time, NULL); \
      dtime += current_time.tv_sec + (current_time.tv_usec / 1000000.0); \
      printf("(%s:%d) took %lf seconds\n", \
	     G_STRFUNC, __LINE__, dtime ); \
    }while(0)

#else /* GNOME_ENABLE_DEBUG */

#define TOTEM_PROFILE(function) function

#endif /* GNOME_ENABLE_DEBUG */
