/* gtk-xine-properties.c

   Copyright (C) 2002 Bastien Nocera

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

   Author: Bastien Nocera <hadess@hadess.net>
 */

#include "config.h"
#include "gtk-xine-properties.h"

#include <gtk/gtk.h>
#include <glade/glade.h>
#include <libgnome/gnome-i18n.h>
#include <string.h>

#include "debug.h"

struct GtkXinePropertiesPrivate
{
	GladeXML *xml;
	GtkWidget *vbox;
};

static GtkWidgetClass *parent_class = NULL;

static void gtk_xine_properties_class_init (GtkXinePropertiesClass *class);
static void gtk_xine_properties_init       (GtkXineProperties      *label);

static void init_treeview (GtkWidget *treeview, GtkXineProperties *playlist);
static gboolean gtk_xine_properties_unset_playing (GtkXineProperties *playlist);

GtkType
gtk_xine_properties_get_type (void)
{
	static GtkType gtk_xine_properties_type = 0;

	if (!gtk_xine_properties_type) {
		static const GTypeInfo gtk_xine_properties_info = {
			sizeof (GtkXinePropertiesClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gtk_xine_properties_class_init,
			(GClassFinalizeFunc) NULL,
			NULL /* class_data */,
			sizeof (GtkXineProperties),
			0 /* n_preallocs */,
			(GInstanceInitFunc) gtk_xine_properties_init,
		};

		gtk_xine_properties_type = g_type_register_static
			(GTK_TYPE_DIALOG, "GtkXineProperties",
			 &gtk_xine_properties_info, (GTypeFlags)0);
	}

	return gtk_xine_properties_type;
}

static void
gtk_xine_properties_init (GtkXineProperties *playlist)
{
	playlist->priv = g_new0 (GtkXinePropertiesPrivate, 1);
	playlist->priv->xml = NULL;
	playlist->priv->vbox = NULL;
}

static void
gtk_xine_properties_finalize (GObject *object)
{
	GtkXineProperties *playlist = GTK_XINE_PROPERTIES (object);

	g_return_if_fail (object != NULL);
#if 0
	if (playlist->priv->current != NULL)
		gtk_tree_path_free (playlist->priv->current);
	if (playlist->priv->icon != NULL)
		gdk_pixbuf_unref (playlist->priv->icon);
#endif
	if (G_OBJECT_CLASS (parent_class)->finalize != NULL) {
		(* G_OBJECT_CLASS (parent_class)->finalize) (object);
	}
}
#if 0
static void
gtk_xine_properties_unrealize (GtkWidget *widget)
{
	GtkXineProperties *playlist = GTK_XINE_PROPERTIES (widget);
	int x, y;

	g_return_if_fail (widget != NULL);

	gtk_window_get_position (GTK_WINDOW (widget), &x, &y);
	gconf_client_set_int (playlist->priv->gc, "/apps/totem/playlist_x",
			x, NULL);
	gconf_client_set_int (playlist->priv->gc, "/apps/totem/playlist_y",
			y, NULL);

	if (GTK_WIDGET_CLASS (parent_class)->unrealize != NULL) {
		(* GTK_WIDGET_CLASS (parent_class)->unrealize) (widget);
	}
}

static void
gtk_xine_properties_realize (GtkWidget *widget)
{
	GtkXineProperties *playlist = GTK_XINE_PROPERTIES (widget);
	int x, y;

	g_return_if_fail (widget != NULL);

	if (GTK_WIDGET_CLASS (parent_class)->realize != NULL) {
		(* GTK_WIDGET_CLASS (parent_class)->realize) (widget);
	}

	x = gconf_client_get_int (playlist->priv->gc,
			"/apps/totem/playlist_x", NULL);
	y = gconf_client_get_int (playlist->priv->gc,
			"/apps/totem/playlist_y", NULL);

	if (x == -1 || y == -1
			|| x > gdk_screen_width () || y > gdk_screen_height ())
		return;

	gtk_window_move (GTK_WINDOW (widget), x, y);
}
#endif
static void
hide_dialog (GtkWidget *widget, int trash, gpointer user_data)
{
	gtk_widget_hide (widget);
}


GtkWidget*
gtk_xine_properties_new (void)
{
	GtkXineProperties *props;
	GladeXML *xml;
	char *filename;

	filename = g_build_filename (G_DIR_SEPARATOR_S, DATADIR,
			"totem", "properties.glade", NULL);

	xml = glade_xml_new (filename, "vbox1", NULL);
	g_free (filename);

	if (xml == NULL)
	{
		//FIXME
		g_warning (_("Couldn't find properties.glade"));
		return NULL;
	}

	props = GTK_XINE_PROPERTIES (g_object_new
			(GTK_TYPE_XINE_PROPERTIES, NULL));
	props->priv->xml = xml;
	props->priv->vbox = glade_xml_get_widget (props->priv->xml, "vbox1");

	gtk_window_set_title (GTK_WINDOW (props), _("Properties"));
	gtk_dialog_add_buttons (GTK_DIALOG (props),
			GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE,
			NULL);
	gtk_box_pack_start (GTK_BOX (GTK_DIALOG (props)->vbox),
			props->priv->vbox,
			TRUE,       /* expand */
			TRUE,       /* fill */
			0);         /* padding */

	g_signal_connect (G_OBJECT (props),
			"response", G_CALLBACK (hide_dialog), NULL);
	g_signal_connect (G_OBJECT (props), "delete-event",
			G_CALLBACK (hide_dialog), NULL);

//FIXME	gtk_xine_properties_update (gtx, gtx->priv->properties_reset_state);

	gtk_widget_show_all (GTK_DIALOG (props)->vbox);

	return GTK_WIDGET (props);
}

static void
gtk_xine_properties_class_init (GtkXinePropertiesClass *klass)
{
	parent_class = gtk_type_class (gtk_dialog_get_type ());

	G_OBJECT_CLASS (klass)->finalize = gtk_xine_properties_finalize;
//	GTK_WIDGET_CLASS (klass)->realize = gtk_xine_properties_realize;
//	GTK_WIDGET_CLASS (klass)->unrealize = gtk_xine_properties_unrealize;
}

