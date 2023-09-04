/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2016 Bastien Nocera <hadess@hadess.net>
 *
 * SPDX-License-Identifier: GPL-3-or-later
 *
 * Author: Bastien Nocera <hadess@hadess.net>
 */

#include "config.h"

#include <glib/gi18n-lib.h>
#include <gmodule.h>
#include <string.h>
#include <gdk/gdkkeysyms.h>
#include "totem-plugin-activatable.h"

#include "totem-plugin.h"
#include "totem.h"

#define TOTEM_TYPE_VARIABLE_RATE_PLUGIN		(totem_variable_rate_plugin_get_type ())
#define TOTEM_VARIABLE_RATE_PLUGIN(o)			(G_TYPE_CHECK_INSTANCE_CAST ((o), TOTEM_TYPE_VARIABLE_RATE_PLUGIN, TotemVariableRatePlugin))

typedef struct {
	PeasExtensionBase parent;

	TotemObject       *totem;
	guint              handler_id_key_press;
	guint              handler_id_main_page;
	GSimpleAction     *action;
	GMenuItem         *submenu_item;
	gboolean           player_page;
} TotemVariableRatePlugin;

#define NUM_RATES 6
#define NORMAL_RATE_IDX 1

/* NOTE:
 * Keep in sync with mpris/totem-mpris.c */
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
	const char *rate_id;
	char *label;
	guint i;

	rate_id = g_variant_get_string (parameter, NULL);
	for (i = 0; i < NUM_RATES; i++)
		if (g_strcmp0 (rate_id, rates[i].id) == 0)
			break;

	g_assert (i < NUM_RATES);

	if (!totem_object_set_rate (pi->totem, rates[i].rate)) {
		g_warning ("Failed to set rate to %f, resetting", rates[i].rate);
		i = NORMAL_RATE_IDX;

		if (!totem_object_set_rate (pi->totem, rates[i].rate))
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
	GVariant *state;

	g_debug ("Reset rate to 1.0");

	state = g_variant_new_string (rates[NORMAL_RATE_IDX].id);
	g_action_change_state (G_ACTION (pi->action), state);
}

static void
change_rate (TotemVariableRatePlugin *pi,
	     gboolean                 increase)
{
	GVariant *state;
	const char *rate_id;
	int target, i;

	state = g_action_get_state (G_ACTION (pi->action));
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
	g_action_change_state (G_ACTION (pi->action), state);
}

static void
on_totem_main_page_notify (GObject *object, GParamSpec *spec, TotemVariableRatePlugin *plugin)
{
	TotemVariableRatePlugin *pi = TOTEM_VARIABLE_RATE_PLUGIN (plugin);
	char *main_page;

	g_object_get (pi->totem, "main-page", &main_page, NULL);
	pi->player_page = (g_strcmp0 (main_page, "player") == 0);
	g_free (main_page);
}

static gboolean
on_window_key_press_event (GtkWidget *window, GdkEventKey *event, TotemVariableRatePlugin *plugin)
{
	TotemVariableRatePlugin *pi = TOTEM_VARIABLE_RATE_PLUGIN (plugin);

	if (!pi->player_page ||
	    event->state == 0 ||
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
impl_activate (TotemPluginActivatable *plugin)
{
	GtkWindow *window;
	TotemVariableRatePlugin *pi = TOTEM_VARIABLE_RATE_PLUGIN (plugin);
	GMenuItem *item;
	GMenu *menu;
	guint i;

	pi->totem = g_object_get_data (G_OBJECT (plugin), "object");

	/* Cache totem's main page */
	pi->handler_id_main_page = g_signal_connect (G_OBJECT(pi->totem),
						       "notify::main-page",
						       G_CALLBACK (on_totem_main_page_notify),
						       pi);

	/* Key press handler */
	window = totem_object_get_main_window (pi->totem);
	pi->handler_id_key_press = g_signal_connect (G_OBJECT(window),
				"key-press-event",
				G_CALLBACK (on_window_key_press_event),
				pi);
	g_object_unref (window);

	/* Create the variable rate action */
	pi->action = g_simple_action_new_stateful ("variable-rate",
						     G_VARIANT_TYPE_STRING,
						     g_variant_new_string (rates[NORMAL_RATE_IDX].id));
	g_signal_connect (G_OBJECT (pi->action), "change-state",
			  G_CALLBACK (variable_rate_action_callback), plugin);
	g_action_map_add_action (G_ACTION_MAP (pi->totem), G_ACTION (pi->action));

	/* Create the submenu */
	menu = totem_object_get_menu_section (pi->totem, "variable-rate-placeholder");
	for (i = 0; i < NUM_RATES; i++) {
		char *target;

		target = g_strdup_printf ("app.variable-rate::%s", rates[i].id);
		item = g_menu_item_new (g_dpgettext2 (NULL, "playback rate", rates[i].label), target);
		g_free (target);
		g_menu_append_item (G_MENU (menu), item);
	}
}

static void
impl_deactivate (TotemPluginActivatable *plugin)
{
	TotemVariableRatePlugin *pi = TOTEM_VARIABLE_RATE_PLUGIN (plugin);
	GtkWindow *window;
	TotemObject *totem;

	totem = g_object_get_data (G_OBJECT (plugin), "object");

	if (pi->handler_id_key_press != 0) {
		window = totem_object_get_main_window (totem);
		g_signal_handler_disconnect (G_OBJECT(window),
					     pi->handler_id_key_press);
		pi->handler_id_key_press = 0;
		g_object_unref (window);
	}

	if (pi->handler_id_main_page != 0) {
		g_signal_handler_disconnect (G_OBJECT(pi->totem),
					     pi->handler_id_main_page);
		pi->handler_id_main_page = 0;
	}

	/* Remove the menu */
	totem_object_empty_menu_section (pi->totem, "variable-rate-placeholder");

	/* Reset the rate */
	if (!totem_object_set_rate (pi->totem, 1.0))
		g_warning ("Failed to reset the playback rate to 1.0");
}
