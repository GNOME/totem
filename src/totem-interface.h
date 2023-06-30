/* totem-interface.h

   Copyright (C) 2005,2007 Bastien Nocera <hadess@hadess.net>

   SPDX-License-Identifier: GPL-3-or-later

   Author: Bastien Nocera <hadess@hadess.net>
 */

#pragma once

#include <gtk/gtk.h>
#include "totem.h"

void		 totem_interface_error		(const char *title,
						 const char *reason,
						 GtkWindow *parent);
void		 totem_interface_error_blocking	(const char *title,
						 const char *reason,
						 GtkWindow *parent);
