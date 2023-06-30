/* totem-options.h

   Copyright (C) 2004,2007 Bastien Nocera <hadess@hadess.net>

   SPDX-License-Identifier: GPL-3-or-later

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
