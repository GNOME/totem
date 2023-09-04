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

#include <libpeas.h>
#include <totem-plugin-activatable.h>
#include <totem.h>

/**
 * TOTEM_PLUGIN_REGISTER:
 * @TYPE_NAME: the name of the plugin type, in UPPER_CASE
 * @TypeName: the name of the plugin type, in CamelCase
 * @type_name: the name of the plugin type, in lower_case
 *
 * Registers a plugin with the Totem plugin system, including registering the
 * type specified in the parameters and declaring its activate and
 * deactivate functions.
 **/
#define TOTEM_PLUGIN_REGISTER(TYPE_NAME, TypeName, type_name)			\
	typedef struct {							\
		PeasExtensionBaseClass parent_class;				\
	} TypeName##Class;							\
	GType type_name##_get_type (void) G_GNUC_CONST;				\
	static void impl_activate (TotemPluginActivatable *plugin);			\
	static void impl_deactivate (TotemPluginActivatable *plugin);			\
	G_MODULE_EXPORT void peas_register_types (PeasObjectModule *module);	\
	static void peas_activatable_iface_init (TotemPluginActivatableInterface *iface); \
	enum {									\
		PROP_0,								\
		PROP_OBJECT							\
	};									\
	G_DEFINE_DYNAMIC_TYPE_EXTENDED (TypeName,				\
					type_name,				\
					PEAS_TYPE_EXTENSION_BASE,		\
					0,					\
					G_IMPLEMENT_INTERFACE_DYNAMIC (TOTEM_TYPE_PLUGIN_ACTIVATABLE, \
								       peas_activatable_iface_init) \
					)					\
	static void								\
	peas_activatable_iface_init (TotemPluginActivatableInterface *iface)		\
	{									\
		iface->activate = impl_activate;				\
		iface->deactivate = impl_deactivate;				\
	}									\
	static void								\
	set_property (GObject      *object,					\
		      guint         prop_id,					\
		      const GValue *value,					\
		      GParamSpec   *pspec)					\
	{									\
		switch (prop_id) {						\
		case PROP_OBJECT:						\
			g_object_set_data_full (object, "object",		\
						g_value_dup_object (value),	\
						g_object_unref);		\
			break;							\
		default:							\
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec); \
			break;							\
		}								\
	}									\
	static void								\
	get_property (GObject    *object,					\
		      guint       prop_id,					\
		      GValue     *value,					\
		      GParamSpec *pspec)					\
	{									\
		switch (prop_id) {						\
		case PROP_OBJECT:						\
			g_value_set_object (value, g_object_get_data (object, "object")); \
			break;							\
		default:							\
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec); \
			break;							\
		}								\
	}									\
	static void								\
	type_name##_class_init (TypeName##Class *klass)				\
	{									\
		GObjectClass *object_class = G_OBJECT_CLASS (klass);		\
										\
		object_class->set_property = set_property;			\
		object_class->get_property = get_property;			\
										\
		g_object_class_override_property (object_class, PROP_OBJECT, "object"); \
	}									\
	static void								\
	type_name##_class_finalize (TypeName##Class *klass)			\
	{									\
	}									\
	static void								\
	type_name##_init (TypeName *plugin)					\
	{									\
	}									\
	G_MODULE_EXPORT void							\
	peas_register_types (PeasObjectModule *module)				\
	{									\
		type_name##_register_type (G_TYPE_MODULE (module));		\
		peas_object_module_register_extension_type (module,		\
							    TOTEM_TYPE_PLUGIN_ACTIVATABLE, \
							    TYPE_NAME);		\
	}
