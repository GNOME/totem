/* Encoding stuff
 *
 * SPDX-License-Identifier: GPL-3-or-later
 *
 */

#pragma once

#include <gtk/gtk.h>

void totem_subtitle_encoding_init (GtkComboBox *combo);
void totem_subtitle_encoding_set (GtkComboBox *combo, const char *encoding);
const char * totem_subtitle_encoding_get_selected (GtkComboBox *combo);
