/* totem-options.h

   Copyright (C) 2004,2007 Bastien Nocera <hadess@hadess.net>

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
   write to the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301  USA.

   Author: Bastien Nocera <hadess@hadess.net>
 */

#pragma once

#include <gtk/gtk.h>

#include "totem.h"

/* Stores the state of the command line options */
typedef struct {
	gboolean playpause;
	gboolean play;
	gboolean pause;
	gboolean next;
	gboolean previous;
	gboolean seekfwd;
	gboolean seekbwd;
	gboolean volumeup;
	gboolean volumedown;
	gboolean mute;
	gboolean fullscreen;
	gboolean togglecontrols;
	gboolean quit;
	gboolean enqueue;
	gboolean replace;
	gint64 seek;
	gchar **filenames;
	gboolean had_filenames;
} TotemCmdLineOptions;

extern const GOptionEntry all_options[];
extern TotemCmdLineOptions optionstate;

void totem_options_register_remote_commands (Totem *totem);
void totem_options_process_for_server (Totem *totem,
				       TotemCmdLineOptions* options);
