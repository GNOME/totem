/*
 * Copyright © 2014 Bastien Nocera <hadess@hadess.net>
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
 * permission is above and beyond the permissions granted by the GPL license
 * Totem is covered by.
 *
 * Monday 7th February 2005: Christian Schaller: Add exception clause.
 * See license_change file for details.
 *
 */

#include "totem-style-helpers.h"

void
totem_set_popover_no_shadow (GtkWidget *widget)
{
	GtkWidget *popover;
	GtkStyleContext *context;
	GtkCssProvider *provider;
	const gchar css[] =
		"GtkPopover {\n"
		"  border-radius: 0px;\n"
		"  margin: 0px;\n"
		"  padding: 0px;\n"
		"}";

	g_return_if_fail (GTK_IS_WIDGET (widget));

	if (GTK_IS_SCALE_BUTTON (widget))
		popover = gtk_scale_button_get_popup (GTK_SCALE_BUTTON (widget));
	else if (GTK_IS_MENU_BUTTON (widget))
		popover = GTK_WIDGET (gtk_menu_button_get_popover (GTK_MENU_BUTTON (widget)));
	else
		g_assert_not_reached();

	context = gtk_widget_get_style_context (popover);
	provider = gtk_css_provider_new ();
	gtk_css_provider_load_from_data (provider, css, -1, NULL);
	gtk_style_context_add_provider (context, GTK_STYLE_PROVIDER (provider),
					GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
	g_object_unref (provider);
}
