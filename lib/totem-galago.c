/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-

   Copyright (C) 2004-2006 Bastien Nocera <hadess@hadess.net>

   The Gnome Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   The Gnome Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with the Gnome Library; see the file COPYING.LIB.  If not,
   write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.

   Author: Bastien Nocera <hadess@hadess.net>, Philip Withnall <philip@tecnocode.co.uk>
 */


#include "config.h"

#include <glib/gi18n.h>
#include <gdk/gdk.h>
#include <libgalago/galago.h>

#include "totem-galago.h"

static GObjectClass *parent_class = NULL;
static void totem_galago_class_init	(TotemGalagoClass *class);
static void totem_galago_init		(TotemGalago *ggo);
static void totem_galago_finalize	(GObject *object);

struct TotemGalagoPrivate {
	gboolean idle; /* Whether we're idle */
	GList *accounts; /* Galago accounts */
};

G_DEFINE_TYPE(TotemGalago, totem_galago, G_TYPE_OBJECT)

static void
totem_galago_class_init (TotemGalagoClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	object_class->finalize = totem_galago_finalize;
}

TotemGalago *
totem_galago_new (void)
{
	return TOTEM_GALAGO (g_object_new (TOTEM_TYPE_GALAGO, NULL));
}

static void
totem_galago_init (TotemGalago *ggo)
{
	GalagoPerson *person;

	ggo->priv = g_new0 (TotemGalagoPrivate, 1);

	if (galago_init (PACKAGE_NAME, GALAGO_INIT_FEED) == FALSE
	    || galago_is_connected () == FALSE) {
		g_warning ("Failed to initialise libgalago.");
		return;
	}

	/* Get "me" and list accounts */
	person = galago_get_me (GALAGO_REMOTE, TRUE);
	ggo->priv->accounts = galago_person_get_accounts (person, TRUE);
	g_object_unref (person);
}

void
totem_galago_idle (TotemGalago *ggo)
{
	totem_galago_set_idleness (ggo, TRUE);
}

void
totem_galago_not_idle (TotemGalago *ggo)
{
	totem_galago_set_idleness (ggo, FALSE);
}

void
totem_galago_set_idleness (TotemGalago *ggo, gboolean idle)
{
	GList *account;
	GalagoPresence *presence;

	if (galago_is_connected () == FALSE)
		return;

	if (ggo->priv->idle == idle)
		return;

	ggo->priv->idle = idle;
	for (account = ggo->priv->accounts; account != NULL; account = g_list_next (account)) {
		presence = galago_account_get_presence ((GalagoAccount *)account->data, TRUE);
		galago_presence_set_idle (presence, idle, time (NULL));
		g_object_unref (presence);
	}
}

static void
totem_galago_finalize (GObject *object)
{
	TotemGalago *ggo = TOTEM_GALAGO (object);

	if (galago_is_connected ())
		galago_uninit ();

	g_list_free (ggo->priv->accounts);
	g_free (ggo->priv);

	if (G_OBJECT_CLASS (parent_class)->finalize != NULL) {
		(* G_OBJECT_CLASS (parent_class)->finalize) (object);
	}
}

