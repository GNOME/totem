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

#include "config.h"

#include <glib-object.h>
#include <gtk/gtkwindow.h>

#include "totem.h"
#include "totem-private.h"
#include "totem-plugins-engine.h"
#include "ev-sidebar.h"

enum {
	PROP_0,
	PROP_FULLSCREEN,
	PROP_PLAYING
};

static void totem_object_set_property		(GObject *object,
						 guint property_id,
						 const GValue *value,
						 GParamSpec *pspec);
static void totem_object_get_property		(GObject *object,
						 guint property_id,
						 GValue *value,
						 GParamSpec *pspec);
static void totem_object_finalize (GObject *totem);

G_DEFINE_TYPE(TotemObject, totem_object, G_TYPE_OBJECT)

static GObjectClass *parent_class = NULL;

static void
totem_object_class_init (TotemObjectClass *klass)
{
	GObjectClass *object_class;

	object_class = (GObjectClass *) klass;

	parent_class = g_type_class_peek_parent (klass);

	object_class->set_property = totem_object_set_property;
	object_class->get_property = totem_object_get_property;
	object_class->finalize = totem_object_finalize;

	g_object_class_install_property (object_class, PROP_FULLSCREEN,
					 g_param_spec_boolean ("fullscreen", NULL, NULL,
							       FALSE, G_PARAM_READABLE));
	g_object_class_install_property (object_class, PROP_PLAYING,
					 g_param_spec_boolean ("playing", NULL, NULL,
							       FALSE, G_PARAM_READABLE));
	//FIXME properties and signals
}

static void
totem_object_init (TotemObject *totem)
{
	//FIXME nothing yet
}

static void
totem_object_finalize (GObject *totem)
{
	totem_plugins_engine_shutdown ();
}

static void
totem_object_set_property (GObject *object,
			   guint property_id,
			   const GValue *value,
			   GParamSpec *pspec)
{
	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
totem_object_get_property (GObject *object,
			   guint property_id,
			   GValue *value,
			   GParamSpec *pspec)
{
	TotemObject *totem;

	totem = TOTEM_OBJECT (object);

	switch (property_id)
	{
	case PROP_FULLSCREEN:
		g_value_set_boolean (value, totem_is_fullscreen (totem));
		break;
	case PROP_PLAYING:
		g_value_set_boolean (value, totem_is_playing (totem));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
	}
}

void
totem_object_plugins_init (TotemObject *totem)
{
	totem_plugins_engine_init (totem);
}

GtkWindow *
totem_get_main_window (Totem *totem)
{
	g_return_val_if_fail (TOTEM_IS_OBJECT (totem), NULL);

	g_object_ref (G_OBJECT (totem->win));

	return GTK_WINDOW (totem->win);
}

void
totem_add_sidebar_page (Totem *totem,
			const char *page_id,
			const char *title,
			GtkWidget *main_widget)
{
	ev_sidebar_add_page (EV_SIDEBAR (totem->sidebar),
			     page_id,
			     title,
			     main_widget);
}

void
totem_remove_sidebar_page (Totem *totem,
			   const char *page_id)
{
	ev_sidebar_remove_page (EV_SIDEBAR (totem->sidebar),
				page_id);
}

