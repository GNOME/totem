/*
 * Copyright (C) 2010 - Steve Frécinaux
 *
 * SPDX-License-Identifier: GPL-3-or-later
 *
 * Originally from libpeas
 */

/**
 * SECTION:totem-plugin-activatable
 * @short_description: Interface for activatable plugins.
 * @stability: Unstable
 * @include: totem-plugin-activatable.h
 *
 * #TotemPluginActivatable is an interface which should be implemented by plugins
 * which will be used in Totem.
**/

#include "totem-plugin-activatable.h"

G_DEFINE_INTERFACE (TotemPluginActivatable, totem_plugin_activatable, G_TYPE_OBJECT)

static void
totem_plugin_activatable_default_init (TotemPluginActivatableInterface *iface)
{
  /**
   * TotemPluginActivatable:object:
   *
   * The object property contains the targetted object for this #TotemPluginActivatable
   * instance.
   *
   * For example a toplevel window in a typical windowed application. It is set
   * at construction time and won't change.
   */
    g_object_interface_install_property (iface,
                                         g_param_spec_object ("object",
                                                              "object",
                                                              "object",
                                                              G_TYPE_OBJECT,
                                                              G_PARAM_READWRITE |
                                                              G_PARAM_CONSTRUCT_ONLY |
                                                              G_PARAM_STATIC_STRINGS));
}

/**
 * totem_plugin_activatable_activate:
 * @self: A #TotemPluginActivatable.
 *
 * Activates the extension on the targetted object.
 *
 * On activation, the extension should hook itself to the object
 * where it makes sense.
 */
void
totem_plugin_activatable_activate (TotemPluginActivatable *self)
{
    TotemPluginActivatableInterface *iface;

    g_return_if_fail (TOTEM_IS_PLUGIN_ACTIVATABLE (self));

    iface = TOTEM_PLUGIN_ACTIVATABLE_GET_IFACE (self);
    g_return_if_fail (iface->activate != NULL);

    iface->activate(self);
}

/**
 * totem_plugin_activatable_deactivate:
 * @self: A #TotemPluginActivatable.
 *
 * Deactivates the extension on the targetted object.
 *
 * On deactivation, an extension should remove itself from all the hooks it
 * used and should perform any cleanup required, so it can be unreffed safely
 * and without any more effect on the host application.
 */
void
totem_plugin_activatable_deactivate (TotemPluginActivatable *self)
{
    TotemPluginActivatableInterface *iface;

    g_return_if_fail (TOTEM_IS_PLUGIN_ACTIVATABLE (self));

    iface = TOTEM_PLUGIN_ACTIVATABLE_GET_IFACE (self);
    g_return_if_fail (iface->deactivate != NULL);

    iface->deactivate(self);
}
