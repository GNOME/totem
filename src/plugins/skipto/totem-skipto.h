/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * Copyright (C) 2007 Bastien Nocera <hadess@hadess.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA.
 *
 * The Totem project hereby grant permission for non-gpl compatible GStreamer
 * plugins to be used and distributed together with GStreamer and Totem. This
 * permission are above and beyond the permissions granted by the GPL license
 * Totem is covered by.
 *
 * Monday 7th February 2005: Christian Schaller: Add excemption clause.
 * See license_change file for details.
 *
 * Author: Bastien Nocera <hadess@hadess.net>, Philip Withnall <philip@tecnocode.co.uk>
 */

#pragma once

#include <gtk/gtk.h>

#include "totem.h"

#define TOTEM_TYPE_SKIPTO		(totem_skipto_get_type ())
G_DECLARE_FINAL_TYPE(TotemSkipto, totem_skipto, TOTEM, SKIPTO, GtkDialog)

GType totem_skipto_get_type	(void);
GtkWidget *totem_skipto_new	(TotemObject *totem);
gint64 totem_skipto_get_range	(TotemSkipto *skipto);
void totem_skipto_update_range	(TotemSkipto *skipto, gint64 _time);
void totem_skipto_set_seekable	(TotemSkipto *skipto, gboolean seekable);
void totem_skipto_set_current	(TotemSkipto *skipto, gint64 _time);
