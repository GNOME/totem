/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Plugin engine for Totem, heavily based on the code from Rhythmbox,
 * which is based heavily on the code from totem.
 *
 * Copyright (C) 2002-2005 Paolo Maggi
 *               2006 James Livingston  <jrl@ids.org.au>
 *               2007 Bastien Nocera <hadess@hadess.net>
 *
 * SPDX-License-Identifier: GPL-3-or-later
 *
 */

#pragma once

#include <glib.h>
#include <libpeas.h>
#include <totem.h>

#define TOTEM_TYPE_PLUGINS_ENGINE              (totem_plugins_engine_get_type ())
G_DECLARE_FINAL_TYPE(TotemPluginsEngine, totem_plugins_engine, TOTEM, PLUGINS_ENGINE, GObject)

GType			  totem_plugins_engine_get_type			(void) G_GNUC_CONST;
TotemPluginsEngine	* totem_plugins_engine_get_default		(TotemObject *totem);
void			  totem_plugins_engine_shut_down		(TotemPluginsEngine *self);
PeasEngine              * totem_plugins_engine_get_engine		(TotemPluginsEngine *self);
