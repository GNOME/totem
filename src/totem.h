/* 
 * Copyright (C) 2001,2002,2003,2004,2005 Bastien Nocera <hadess@hadess.net>
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * The Totem project hereby grant permission for non-gpl compatible GStreamer
 * plugins to be used and distributed together with GStreamer and Totem. This
 * permission are above and beyond the permissions granted by the GPL license
 * Totem is covered by.
 *
 * Monday 7th February 2005: Christian Schaller: Add excemption clause.
 * See license_change file for details.
 *
 */

#ifndef __TOTEM_H__
#define __TOTEM_H__

#include <gconf/gconf-client.h>
#include <gtk/gtkwidget.h>
#include "bacon-video-widget.h"

#define TOTEM_GCONF_PREFIX "/apps/totem"

typedef struct Totem Totem;

void	totem_action_exit			(Totem *totem);
void	totem_action_play			(Totem *totem);
void	totem_action_stop			(Totem *totem);
void	totem_action_play_pause			(Totem *totem);
void	totem_action_pause			(Totem *totem);
void	totem_action_fullscreen_toggle		(Totem *totem);
void	totem_action_fullscreen			(Totem *totem, gboolean state);
void	totem_action_next			(Totem *totem);
void	totem_action_previous			(Totem *totem);
void	totem_action_seek_relative		(Totem *totem, int off_sec);
void	totem_action_volume_relative		(Totem *totem, int off_pct);
gboolean totem_action_set_mrl			(Totem *totem,
						 const char *mrl);
void	totem_action_set_mrl_and_play		(Totem *totem,
						 const char *mrl);

gboolean totem_action_set_mrl_with_warning	(Totem *totem,
						 const char *mrl,
						 gboolean warn);

void	totem_action_play_media			(Totem *totem,
						 MediaType type);

void	totem_action_toggle_aspect_ratio	(Totem *totem);
void	totem_action_set_aspect_ratio		(Totem *totem, int ratio);
int	totem_action_get_aspect_ratio		(Totem *totem);
void	totem_action_toggle_controls		(Totem *totem);

void	totem_action_set_scale_ratio		(Totem *totem, gfloat ratio);
void    totem_action_error                      (const char *title,
						 const char *reason,
						 Totem *totem);

GtkWidget * totem_statusbar_create (void);
GtkWidget * totem_time_display_create (void);

#endif /* __TOTEM_H__ */
