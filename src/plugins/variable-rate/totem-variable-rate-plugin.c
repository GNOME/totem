/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2016 Bastien Nocera <hadess@hadess.net>
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA.
 *
 * The Totem project hereby grant permission for non-gpl compatible GStreamer
 * plugins to be used and distributed together with GStreamer and Totem. This
 * permission are above and beyond the permissions granted by the GPL license
 * Totem is covered by.
 *
 * Monday 7th February 2005: Christian Schaller: Add excemption clause.
 * See license_change file for details.
 *
 * Author: Bastien Nocera <hadess@hadess.net>
 */

#include "config.h"

#include <glib/gi18n-lib.h>
#include <gmodule.h>
#include <string.h>
#include <gdk/gdkkeysyms.h>
#include <libpeas/peas-activatable.h>

#include "totem-plugin.h"
#include "totem.h"

#define TOTEM_TYPE_VARIABLE_RATE_PLUGIN		(totem_variable_rate_plugin_get_type ())
#define TOTEM_VARIABLE_RATE_PLUGIN(o)			(G_TYPE_CHECK_INSTANCE_CAST ((o), TOTEM_TYPE_VARIABLE_RATE_PLUGIN, TotemVariableRatePlugin))
#define TOTEM_VARIABLE_RATE_PLUGIN_CLASS(k)		(G_TYPE_CHECK_CLASS_CAST((k), TOTEM_TYPE_VARIABLE_RATE_PLUGIN, TotemVariableRatePluginClass))
#define TOTEM_IS_VARIABLE_RATE_PLUGIN(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), TOTEM_TYPE_VARIABLE_RATE_PLUGIN))
#define TOTEM_IS_VARIABLE_RATE_PLUGIN_CLASS(k)		(G_TYPE_CHECK_CLASS_TYPE ((k), TOTEM_TYPE_VARIABLE_RATE_PLUGIN))
#define TOTEM_VARIABLE_RATE_PLUGIN_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), TOTEM_TYPE_VARIABLE_RATE_PLUGIN, TotemVariableRatePluginClass))

typedef struct {
	TotemObject       *totem;
	guint              handler_id_key_press;
	GSimpleAction     *action;
	GMenuItem         *submenu_item;
} TotemVariableRatePluginPrivate;

#define NUM_RATES 6
#define NORMAL_RATE_IDX 1

static struct {
	float rate;
	const char *label;
	const char *id;
} rates[NUM_RATES] = {
	{ 0.75,  NC_("playback rate", "× 0.75"), "0_75" },
	{ 1.0,   NC_("playback rate", "Normal"), "normal" },
	{ 1.1,   NC_("playback rate", "× 1.1"),  "1_1" },
	{ 1.25,  NC_("playback rate", "× 1.25"), "1_25" },
	{ 1.5,   NC_("playback rate", "× 1.5"),  "1_5" },
	{ 1.75,  NC_("playback rate", "× 1.75"), "1_75" }
};

TOTEM_PLUGIN_REGISTER(TOTEM_TYPE_VARIABLE_RATE_PLUGIN, TotemVariableRatePlugin, totem_variable_rate_plugin)

static char *
get_submenu_label_for_index (guint i)
{
	return g_strdup_printf (_("Speed: %s"),
				g_dpgettext2 (NULL, "playback rate", rates[i].label));
}

static void
variable_rate_action_callback (GSimpleAction           *action,
			       GVariant                *parameter,
			       TotemVariableRatePlugin *plugin)
{
	TotemVariableRatePlugin *pi = TOTEM_VARIABLE_RATE_PLUGIN (plugin);
	TotemVariableRatePluginPrivate *priv = pi->priv;
	const char *rate_id;
	char *label;
	guint i;

	rate_id = g_variant_get_string (parameter, NULL);
	for (i = 0; i < NUM_RATES; i++)
		if (g_strcmp0 (rate_id, rates[i].id) == 0)
			break;

	g_assert (i < NUM_RATES);

	if (!totem_object_set_rate (priv->totem, rates[i].rate)) {
		g_warning ("Failed to set rate to %f, resetting", rates[i].rate);
		i = NORMAL_RATE_IDX;

		if (!totem_object_set_rate (priv->totem, rates[i].rate))
			g_warning ("And failed to reset rate as well...");
	} else {
		g_debug ("Managed to set rate to %f", rates[i].rate);
	}

	g_simple_action_set_state (action, parameter);

	label = get_submenu_label_for_index (i);
	/* FIXME how do we change the section label?
	 * https://gitlab.gnome.org/GNOME/glib/issues/498 */
	g_free (label);
}

static void
reset_rate (TotemVariableRatePlugin *pi)
{
	TotemVariableRatePluginPrivate *priv = pi->priv;
	GVariant *state;

	g_debug ("Reset rate to 1.0");

	state = g_variant_new_string (rates[NORMAL_RATE_IDX].id);
	g_action_change_state (G_ACTION (priv->action), state);
}

static void
change_rate (TotemVariableRatePlugin *pi,
	     gboolean                 increase)
{
	TotemVariableRatePluginPrivate *priv = pi->priv;
	GVariant *state;
	const char *rate_id;
	int target, i;

	state = g_action_get_state (G_ACTION (priv->action));
	rate_id = g_variant_get_string (state, NULL);
	g_assert (rate_id);

	for (i = 0; i < NUM_RATES; i++)
		if (g_strcmp0 (rate_id, rates[i].id) == 0)
			break;

	g_variant_unref (state);

	if (increase)
		target = i + 1;
	else
		target = i - 1;

	if (target >= NUM_RATES)
		target = 0;
	else if (target < 0)
		target = NUM_RATES - 1;

	g_message ("Switching from rate %s to rate %s",
		 rates[i].label, rates[target].label);

	state = g_variant_new_string (rates[target].id);
	g_action_change_state (G_ACTION (priv->action), state);
}

static gboolean
on_window_key_press_event (GtkWidget *window, GdkEventKey *event, TotemVariableRatePlugin *plugin)
{
	TotemVariableRatePlugin *pi = TOTEM_VARIABLE_RATE_PLUGIN (plugin);

	if (event->state == 0 ||
	    event->state & (GDK_CONTROL_MASK | GDK_SUPER_MASK | GDK_HYPER_MASK | GDK_META_MASK)) {
		return FALSE;
	}

	switch (event->keyval) {
		case GDK_KEY_bracketleft:
			change_rate (pi, FALSE);
			break;
		case GDK_KEY_bracketright:
			change_rate (pi, TRUE);
			break;
		case GDK_KEY_BackSpace:
			reset_rate (pi);
			break;
		default:
			return FALSE;
	}

	return TRUE;
}

static void
impl_activate (PeasActivatable *plugin)
{
	GtkWindow *window;
	TotemVariableRatePlugin *pi = TOTEM_VARIABLE_RATE_PLUGIN (plugin);
	TotemVariableRatePluginPrivate *priv = pi->priv;
	GMenuItem *item;
	GMenu *menu;
	guint i;

	priv->totem = g_object_get_data (G_OBJECT (plugin), "object");

	/* Key press handler */
	window = totem_object_get_main_window (priv->totem);
	priv->handler_id_key_press = g_signal_connect (G_OBJECT(window),
				"key-press-event",
				G_CALLBACK (on_window_key_press_event),
				pi);
	g_object_unref (window);

	/* Create the variable rate action */
	priv->action = g_simple_action_new_stateful ("variable-rate",
						     G_VARIANT_TYPE_STRING,
						     g_variant_new_string (rates[NORMAL_RATE_IDX].id));
	g_signal_connect (G_OBJECT (priv->action), "change-state",
			  G_CALLBACK (variable_rate_action_callback), plugin);
	g_action_map_add_action (G_ACTION_MAP (priv->totem), G_ACTION (priv->action));

	/* Create the submenu */
	menu = totem_object_get_menu_section (priv->totem, "variable-rate-placeholder");
	for (i = 0; i < NUM_RATES; i++) {
		char *target;

		target = g_strdup_printf ("app.variable-rate::%s", rates[i].id);
		item = g_menu_item_new (g_dpgettext2 (NULL, "playback rate", rates[i].label), target);
		g_free (target);
		g_menu_append_item (G_MENU (menu), item);
	}
}

static void
impl_deactivate (PeasActivatable *plugin)
{
	GtkWindow *window;
	TotemObject *totem;
	TotemVariableRatePluginPrivate *priv = TOTEM_VARIABLE_RATE_PLUGIN (plugin)->priv;

	totem = g_object_get_data (G_OBJECT (plugin), "object");

	if (priv->handler_id_key_press != 0) {
		window = totem_object_get_main_window (totem);
		g_signal_handler_disconnect (G_OBJECT(window),
					     priv->handler_id_key_press);
		priv->handler_id_key_press = 0;
		g_object_unref (window);
	}

	/* Remove the menu */
	totem_object_empty_menu_section (priv->totem, "variable-rate-placeholder");

	/* Reset the rate */
	if (!totem_object_set_rate (priv->totem, 1.0))
		g_warning ("Failed to reset the playback rate to 1.0");
}
