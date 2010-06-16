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

#ifndef __TOTEM_PLUGIN_H__
#define __TOTEM_PLUGIN_H__

#include <libpeas/peas-extension-base.h>
#include <libpeas/peas-object-module.h>
#include <libpeas/peas-activatable.h>

G_BEGIN_DECLS

#define TOTEM_PLUGIN_REGISTER(TYPE_NAME, TypeName, type_name)			\
	static void impl_activate (PeasActivatable *plugin, GObject *totem);	\
	static void impl_deactivate (PeasActivatable *plugin, GObject *totem);	\
	G_MODULE_EXPORT void peas_register_types (PeasObjectModule *module);	\
	static void peas_activatable_iface_init (PeasActivatableInterface *iface); \
	G_DEFINE_DYNAMIC_TYPE_EXTENDED (TypeName,				\
					type_name,				\
					PEAS_TYPE_EXTENSION_BASE,		\
					0,					\
					G_IMPLEMENT_INTERFACE_DYNAMIC (PEAS_TYPE_ACTIVATABLE, \
								       peas_activatable_iface_init)) \
	static void								\
	peas_activatable_iface_init (PeasActivatableInterface *iface)		\
	{									\
		iface->activate = impl_activate;				\
		iface->deactivate = impl_deactivate;				\
	}									\
	static void								\
	type_name##_class_finalize (TypeName##Class *klass)		\
	{									\
	}									\
	G_MODULE_EXPORT void							\
	peas_register_types (PeasObjectModule *module)				\
	{									\
		type_name##_register_type (G_TYPE_MODULE (module));		\
		peas_object_module_register_extension_type (module,		\
							    PEAS_TYPE_ACTIVATABLE, \
							    TYPE_NAME);		\
	}

G_END_DECLS

#endif  /* __TOTEM_PLUGIN_H__ */

