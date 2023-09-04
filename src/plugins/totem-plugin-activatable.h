/*
 * Copyright (C) 2010 - Steve Frécinaux
 *
 * SPDX-License-Identifier: GPL-3-or-later
 *
 * Originally from libpeas
 */

#pragma once

#include <glib-object.h>

#define TOTEM_TYPE_PLUGIN_ACTIVATABLE       (totem_plugin_activatable_get_type())
G_DECLARE_INTERFACE (TotemPluginActivatable, totem_plugin_activatable, TOTEM, PLUGIN_ACTIVATABLE, GObject)

struct _TotemPluginActivatableInterface {
    GTypeInterface parent;

    void    (*activate)     (TotemPluginActivatable *self);
    void    (*deactivate)   (TotemPluginActivatable *self);
};

void    totem_plugin_activatable_activate       (TotemPluginActivatable *self);
void    totem_plugin_activatable_deactivate     (TotemPluginActivatable *self);
