/*
 * Copyright (C) 2003 Bastien Nocera <hadess@hadess.net>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <string.h>
#include <time.h>
#include <fcntl.h>

#include <gtk/gtk.h>
#include <gnome.h>
#include <libgnome/gnome-i18n.h>
#include <bonobo.h>
#include <libgnomevfs/gnome-vfs.h>
#include <glade/glade.h>
#include "bacon-video-widget-properties.h"
#include "bacon-video-widget.h"

#define TOTEM_TYPE_PROPERTIES_PAGE		     (totem_properties_page_get_type ())
#define TOTEM_PROPERTIES_PAGE(obj)	     (GTK_CHECK_CAST ((obj), TOTEM_TYPE_PROPERTIES_PAGE, TotemPropertiesPage))
#define TOTEM_PROPERTIES_PAGE_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass), TOTEM_TYPE_PROPERTIES_PAGE, TotemPropertiesPageClass))
#define TOTEM_IS_PROPERTIES_PAGE(obj)	     (GTK_CHECK_TYPE ((obj), TOTEM_TYPE_PROPERTIES_PAGE))
#define TOTEM_IS_PROPERTIES_PAGE_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass), TOTEM_TYPE_PROPERTIES_PAGE))

typedef struct {
	BonoboControl parent;

	gchar *location;

	GtkWidget *vbox;
	BaconVideoWidgetProperties *props;
	BaconVideoWidget *bvw;
} TotemPropertiesPage;

typedef struct {
	BonoboControlClass parent;
} TotemPropertiesPageClass;

GType totem_properties_page_get_type(void);
static GObjectClass *parent_class;

enum {
	PROP_URI,
};

static void totem_properties_page_class_init(TotemPropertiesPageClass *class);

static void totem_properties_page_init(TotemPropertiesPage *view);

BONOBO_TYPE_FUNC(TotemPropertiesPage, BONOBO_TYPE_CONTROL,
                 totem_properties_page);


static void totem_properties_page_finalize (GObject *object);
static void get_property (BonoboPropertyBag *bag,
			  BonoboArg         *arg,
			  guint              arg_id,
			  CORBA_Environment *ev,
			  TotemPropertiesPage *props);
static void set_property (BonoboPropertyBag *bag,
			  const BonoboArg   *arg,
			  guint              arg_id,
			  CORBA_Environment *ev,
			  TotemPropertiesPage *props);

static void
totem_properties_page_class_init(TotemPropertiesPageClass *class)
{
	parent_class = g_type_class_peek_parent(class);
	G_OBJECT_CLASS(class)->finalize = totem_properties_page_finalize;
}

static void
on_got_metadata_event (BaconVideoWidget *bvw, TotemPropertiesPage *props)
{
	bacon_video_widget_properties_update
		(props->props, props->bvw, FALSE);
	bacon_video_widget_close (props->bvw);
}

static void
totem_properties_page_init(TotemPropertiesPage *props)
{
	BonoboPropertyBag *pb;
	GError *err = NULL;

	props->bvw = BACON_VIDEO_WIDGET (bacon_video_widget_new
			(-1, -1, TRUE, &err));
	if (!props->bvw)
		g_error ("Error: %s", err ? err->message : "bla");
	//FIXME

	g_signal_connect (G_OBJECT (props->bvw),
			"got-metadata",
			G_CALLBACK (on_got_metadata_event),
			props);

	props->vbox = bacon_video_widget_properties_new ();
	gtk_widget_show_all (props->vbox);
	props->props = BACON_VIDEO_WIDGET_PROPERTIES (props->vbox);

	bonobo_control_construct (BONOBO_CONTROL (props), props->vbox);

	pb = bonobo_property_bag_new (
			(BonoboPropertyGetFn) get_property,
			(BonoboPropertySetFn) set_property, props);
	bonobo_property_bag_add (pb, "URI", PROP_URI, BONOBO_ARG_STRING,
			NULL, _("URI currently displayed"), 0);
	bonobo_control_set_properties (BONOBO_CONTROL (props),
			BONOBO_OBJREF (pb), NULL);
	bonobo_object_release_unref (BONOBO_OBJREF (pb), NULL);
}

static void
totem_properties_page_finalize (GObject *object)
{
	TotemPropertiesPage *props;

	props = TOTEM_PROPERTIES_PAGE (object);

	g_object_unref (G_OBJECT (props->bvw));
	props->bvw = NULL;
	g_free(props->location);
	props->location = NULL;
	g_object_unref (G_OBJECT (props->props));
	props->props = NULL;

	parent_class->finalize(object);
}

static void
get_property(BonoboPropertyBag *bag,
	     BonoboArg         *arg,
	     guint              arg_id,
	     CORBA_Environment *ev,
	     TotemPropertiesPage *props)
{
	if (arg_id == PROP_URI) {
		BONOBO_ARG_SET_STRING(arg, props->location);
	}
}

static void
set_property(BonoboPropertyBag *bag,
	     const BonoboArg   *arg,
	     guint              arg_id,
	     CORBA_Environment *ev,
	     TotemPropertiesPage *props)
{
	if (arg_id == PROP_URI) {
		g_free(props->location);
		bacon_video_widget_close (props->bvw);
		props->location = g_strdup(BONOBO_ARG_GET_STRING(arg));
		bacon_video_widget_properties_update (props->props,
				props->bvw, TRUE);
		bacon_video_widget_open (props->bvw, props->location, NULL);
		bacon_video_widget_play (props->bvw, NULL);
		bacon_video_widget_properties_update (props->props,
				props->bvw, FALSE);
	}
}

/* --- factory --- */

static BonoboObject *
view_factory(BonoboGenericFactory *this_factory,
	     const gchar *iid,
	     gpointer user_data)
{
	return g_object_new (TOTEM_TYPE_PROPERTIES_PAGE, NULL);
}

int
main(int argc, char *argv[])
{
	static struct poptOption options[] = {
		{NULL, '\0', POPT_ARG_INCLUDE_TABLE, NULL, 0,
			N_("Backend options"), NULL},
		{NULL, '\0', 0, NULL, 0} /* end the list */
	};

	options[0].arg = bacon_video_widget_get_popt_table ();
	gnome_program_init ("totem-video-thumbnailer", VERSION,
			LIBGNOME_MODULE, argc, argv,
			GNOME_PARAM_APP_DATADIR, DATADIR,
			GNOME_PARAM_POPT_TABLE, options,
			GNOME_PARAM_NONE);

	bindtextdomain(GETTEXT_PACKAGE, GNOMELOCALEDIR);
	bind_textdomain_codeset(GETTEXT_PACKAGE, "UTF-8");
	textdomain(GETTEXT_PACKAGE);

	BONOBO_FACTORY_INIT(_("Video and Audio information properties page"),
			VERSION, &argc, argv);

	return bonobo_generic_factory_main
		("OAFIID:Totem_PropertiesPage_Factory",
		 view_factory, NULL);
}

