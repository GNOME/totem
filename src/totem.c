/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2001-2007 Bastien Nocera <hadess@hadess.net>
 *
 * SPDX-License-Identifier: GPL-3-or-later
 *
 */

#include "config.h"

#include <glib.h>
#include <glib-object.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <locale.h>
#include <string.h>

#include "totem.h"

int
main (int argc, char **argv)
{
	Totem *totem;

	setlocale (LC_ALL, "");
	bindtextdomain (GETTEXT_PACKAGE, GNOMELOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);

	g_set_prgname (APPLICATION_ID);
#if DEVELOPMENT_VERSION
	g_set_application_name (_("Videos Preview"));
#else
	g_set_application_name (_("Videos"));
#endif
	gtk_window_set_default_icon_name (APPLICATION_ID);
	g_setenv("PULSE_PROP_media.role", "video", TRUE);
	g_setenv("PULSE_PROP_application.icon_name", APPLICATION_ID, TRUE);

	/* Build the main Totem object */
	totem = g_object_new (TOTEM_TYPE_OBJECT,
			      "application-id", APPLICATION_ID,
			      "flags", G_APPLICATION_HANDLES_OPEN,
			      NULL);

	g_application_run (G_APPLICATION (totem), argc, argv);

	return 0;
}
