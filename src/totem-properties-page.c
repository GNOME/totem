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
#include <libgnome/gnome-i18n.h>
#include <bonobo.h>
#include <libgnomevfs/gnome-vfs.h>
#include <glade/glade.h>
#include "bacon-video-widget.h"

#define TOTEM_TYPE_PROPERTIES_PAGE		     (totem_properties_page_get_type ())
#define TOTEM_PROPERTIES_PAGE(obj)	     (GTK_CHECK_CAST ((obj), TOTEM_TYPE_PROPERTIES_PAGE, TotemPropertiesPage))
#define TOTEM_PROPERTIES_PAGE_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass), TOTEM_TYPE_PROPERTIES_PAGE, TotemPropertiesPageClass))
#define TOTEM_IS_PROPERTIES_PAGE(obj)	     (GTK_CHECK_TYPE ((obj), TOTEM_TYPE_PROPERTIES_PAGE))
#define TOTEM_IS_PROPERTIES_PAGE_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass), TOTEM_TYPE_PROPERTIES_PAGE))

typedef struct {
	BonoboControl parent;

	gchar *location;

	GladeXML *xml;
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

static char *
bacon_video_widget_properties_time_to_string (int time)
{
	char *secs, *mins, *hours, *string;
	int sec, min, hour;

	sec = time % 60;
	time = time - sec;
	min = (time % (60*60)) / 60;
	time = time - (min * 60);
	hour = time / (60*60);

	if (hour == 1)
		/* One hour */
		hours = g_strdup_printf (_("%d hour"), hour);
	else
		/* Multiple hours */
		hours = g_strdup_printf (_("%d hours"), hour);

	if (min == 1)
		/* One minute */
		mins = g_strdup_printf (_("%d minute"), min);
	else
		/* Multiple minutes */
		mins = g_strdup_printf (_("%d minutes"), min);

	if (sec == 1)
		/* One second */
		secs = g_strdup_printf (_("%d second"), sec);
	else
		/* Multiple seconds */
		secs = g_strdup_printf (_("%d seconds"), sec);

	if (hour > 0)
	{
		/* hour:minutes:seconds */
		string = g_strdup_printf (_("%s %s %s"), hours, mins, secs);
	} else if (min > 0) {
		/* minutes:seconds */
		string = g_strdup_printf (_("%s %s"), mins, secs);
	} else if (sec > 0) {
		/* seconds */
		string = g_strdup_printf (_("%s"), secs);
	} else {
		/* 0 seconds */
		string = g_strdup (_("0 seconds"));
	}

	g_free (hours);
	g_free (mins);
	g_free (secs);

	return string;
}

static void
bacon_video_widget_properties_set_label (TotemPropertiesPage *props,
			       const char *name, const char *text)
{
	GtkWidget *item;

	item = glade_xml_get_widget (props->xml, name);
	gtk_label_set_text (GTK_LABEL (item), text);
}

static void
bacon_video_widget_properties_reset (TotemPropertiesPage *props)
{
	GtkWidget *item;

	item = glade_xml_get_widget (props->xml, "video");
	gtk_widget_set_sensitive (item, FALSE);
	item = glade_xml_get_widget (props->xml, "audio");
	gtk_widget_set_sensitive (item, FALSE);

	/* Title */
	bacon_video_widget_properties_set_label (props, "title", _("Unknown"));
	/* Artist */
	bacon_video_widget_properties_set_label (props, "artist", _("Unknown"));
	/* Year */
	bacon_video_widget_properties_set_label (props, "year", _("Unknown"));
	/* Duration */
	bacon_video_widget_properties_set_label (props, "duration", _("0 second"));
	/* Dimensions */
	bacon_video_widget_properties_set_label (props, "dimensions", _("0 x 0"));
	/* Video Codec */
	bacon_video_widget_properties_set_label (props, "vcodec", _("N/A"));
	/* Framerate */
	bacon_video_widget_properties_set_label (props, "framerate",
			_("0 frames per second"));
	/* Bitrate */
	bacon_video_widget_properties_set_label (props, "bitrate", _("0 kbps"));
	/* Audio Codec */
	bacon_video_widget_properties_set_label (props, "acodec", _("N/A"));
}

static void
bacon_video_widget_properties_set_from_current
(TotemPropertiesPage *props, BaconVideoWidget *bvw)
{
	GtkWidget *item;
	GValue value = { 0, };
	char *string;
	int x, y;

	/* General */
	bacon_video_widget_get_metadata (BACON_VIDEO_WIDGET (bvw),
			BVW_INFO_TITLE, &value);
	bacon_video_widget_properties_set_label (props, "title",
			g_value_get_string (&value)
			? g_value_get_string (&value)
			: _("Unknown"));
	g_value_unset (&value);

	bacon_video_widget_get_metadata (BACON_VIDEO_WIDGET (bvw),
			BVW_INFO_ARTIST, &value);
	bacon_video_widget_properties_set_label (props, "artist",
			g_value_get_string (&value)
			? g_value_get_string (&value) : _("Unknown"));
	g_value_unset (&value);

	bacon_video_widget_get_metadata (BACON_VIDEO_WIDGET (bvw),
			BVW_INFO_YEAR, &value);
	bacon_video_widget_properties_set_label (props, "year",
			g_value_get_string (&value)
			? g_value_get_string (&value) : _("Unknown"));
	g_value_unset (&value);

	bacon_video_widget_get_metadata (BACON_VIDEO_WIDGET (bvw),
			BVW_INFO_DURATION, &value);
	string = bacon_video_widget_properties_time_to_string
		(g_value_get_int (&value));
	bacon_video_widget_properties_set_label (props, "duration", string);
	g_free (string);
	g_value_unset (&value);

	/* Video */
	item = glade_xml_get_widget (props->xml, "video");
	bacon_video_widget_get_metadata (BACON_VIDEO_WIDGET (bvw),
			BVW_INFO_HAS_VIDEO, &value);
	if (g_value_get_boolean (&value) == FALSE)
		gtk_widget_set_sensitive (item, FALSE);
	else
		gtk_widget_set_sensitive (item, TRUE);
	g_value_unset (&value);

	bacon_video_widget_get_metadata (BACON_VIDEO_WIDGET (bvw),
			BVW_INFO_DIMENSION_X, &value);
	x = g_value_get_int (&value);
	g_value_unset (&value);
	bacon_video_widget_get_metadata (BACON_VIDEO_WIDGET (bvw),
			BVW_INFO_DIMENSION_Y, &value);
	y = g_value_get_int (&value);
	g_value_unset (&value);
	string = g_strdup_printf ("%d x %d", x, y);
	bacon_video_widget_properties_set_label (props, "dimensions", string);
	g_free (string);

	bacon_video_widget_get_metadata (BACON_VIDEO_WIDGET (bvw),
			BVW_INFO_VIDEO_CODEC, &value);
	bacon_video_widget_properties_set_label (props, "vcodec",
			g_value_get_string (&value)
			? g_value_get_string (&value) : _("N/A"));
	g_value_unset (&value);

	bacon_video_widget_get_metadata (BACON_VIDEO_WIDGET (bvw),
			BVW_INFO_FPS, &value);
	string = g_strdup_printf (_("%d frames per second"),
			g_value_get_int (&value));
	bacon_video_widget_properties_set_label (props, "framerate", string);
	g_free (string);
	g_value_unset (&value);

	/* Audio */
	item = glade_xml_get_widget (props->xml, "audio");
	bacon_video_widget_get_metadata (BACON_VIDEO_WIDGET (bvw),
			BVW_INFO_HAS_AUDIO, &value);
	if (g_value_get_boolean (&value) == FALSE)
		gtk_widget_set_sensitive (item, FALSE);
	else
		gtk_widget_set_sensitive (item, TRUE);
	g_value_unset (&value);

	bacon_video_widget_get_metadata (BACON_VIDEO_WIDGET (bvw),
			BVW_INFO_BITRATE, &value);
	string = g_strdup_printf (_("%d kbps"), g_value_get_int (&value));
	bacon_video_widget_properties_set_label (props, "bitrate", string);
	g_free (string);
	g_value_unset (&value);

	bacon_video_widget_get_metadata (BACON_VIDEO_WIDGET (bvw),
			BVW_INFO_AUDIO_CODEC, &value);
	bacon_video_widget_properties_set_label (props, "acodec",
			g_value_get_string (&value)
			? g_value_get_string (&value) : _("N/A"));
	g_value_unset (&value);
}

static void
bacon_video_widget_properties_update (TotemPropertiesPage *props,
		BaconVideoWidget *bvw,
		gboolean reset)
{
	g_return_if_fail (props != NULL);
	g_return_if_fail ( (props));

	if (reset == TRUE)
	{
		bacon_video_widget_properties_reset (props);
	} else {
		g_return_if_fail (bvw != NULL);
		bacon_video_widget_properties_set_from_current (props, bvw);
	}
}

static void
on_got_metadata_event (BaconVideoWidget *bvw, TotemPropertiesPage *props)
{
	bacon_video_widget_properties_update
		(props, props->bvw, FALSE);
}

static void
totem_properties_page_init(TotemPropertiesPage *props)
{
	GtkWidget *vbox;
	BonoboPropertyBag *pb;
	GError *err = NULL;
	char *filename;

	props->bvw = BACON_VIDEO_WIDGET (bacon_video_widget_new
			(-1, -1, TRUE, &err));
	//FIXME

	g_signal_connect (G_OBJECT (props->bvw),
			"got-metadata",
			G_CALLBACK (on_got_metadata_event),
			props);

	filename = g_build_filename (G_DIR_SEPARATOR_S, DATADIR,
			"totem", "properties.glade", NULL);
	props->xml = glade_xml_new (filename, "vbox1", NULL);
	g_free (filename);

	if (props->xml == NULL)
		return;

	vbox = glade_xml_get_widget (props->xml, "vbox1");
	gtk_widget_show (vbox);

	bonobo_control_construct (BONOBO_CONTROL (props), vbox);

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

	g_free(props->location);
	props->location = NULL;
	g_object_unref (G_OBJECT (props->xml));
	props->xml = NULL;

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
		//FIXME reset
		bacon_video_widget_open (props->bvw, props->location, NULL);
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
	bindtextdomain(GETTEXT_PACKAGE, GNOMELOCALEDIR);
	bind_textdomain_codeset(GETTEXT_PACKAGE, "UTF-8");
	textdomain(GETTEXT_PACKAGE);

	BONOBO_FACTORY_INIT(_("Video and Audio information properties page"),
			VERSION, &argc, argv);

	return bonobo_generic_factory_main
		("OAFIID:Totem_PropertiesPage_Factory",
		 view_factory, NULL);
}

