/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Plugin engine for Totem, heavily based on the code from Rhythmbox,
 * which is based heavily on the code from totem.
 *
 * Copyright (C) 2002-2005 Paolo Maggi
 *               2006 James Livingston  <jrl@ids.org.au>
 *               2007 Bastien Nocera <hadess@hadess.net>
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
 * Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301  USA.
 *
 * Sunday 13th May 2007: Bastien Nocera: Add exception clause.
 * See license_change file for details.
 *
 */

#pragma once

#include <glib.h>
#include <libpeas/peas-engine.h>
#include <libpeas/peas-autocleanups.h>
#include <totem.h>

#define TOTEM_TYPE_PLUGINS_ENGINE              (totem_plugins_engine_get_type ())
G_DECLARE_FINAL_TYPE(TotemPluginsEngine, totem_plugins_engine, TOTEM, PLUGINS_ENGINE, PeasEngine)

GType			totem_plugins_engine_get_type			(void) G_GNUC_CONST;
TotemPluginsEngine	*totem_plugins_engine_get_default		(TotemObject *totem);
void			totem_plugins_engine_shut_down			(TotemPluginsEngine *self);
