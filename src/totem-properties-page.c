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
#define NAUTILUS_RPM_PROPERTIES_PAGE(obj)	     (GTK_CHECK_CAST ((obj), TOTEM_TYPE_PROPERTIES_PAGE, TotemPropertiesPage))
#define NAUTILUS_RPM_PROPERTIES_PAGE_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass), TOTEM_TYPE_PROPERTIES_PAGE, TotemPropertiesPageClass))
#define NAUTILUS_IS_RPM_PROPERTIES_PAGE(obj)	     (GTK_CHECK_TYPE ((obj), TOTEM_TYPE_PROPERTIES_PAGE))
#define NAUTILUS_IS_RPM_PROPERTIES_PAGE_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass), TOTEM_TYPE_PROPERTIES_PAGE))

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
			  TotemPropertiesPage *self);
static void set_property (BonoboPropertyBag *bag,
			  const BonoboArg   *arg,
			  guint              arg_id,
			  CORBA_Environment *ev,
			  TotemPropertiesPage *self);

static void
totem_properties_page_class_init(TotemPropertiesPageClass *class)
{
	parent_class = g_type_class_peek_parent(class);
	G_OBJECT_CLASS(class)->finalize = totem_properties_page_finalize;
}
#if 0
static GtkWidget *
make_bold_label(const gchar *message)
{
	gchar *string;
	GtkWidget *label;

	string = g_strconcat("<b>", message, "</b>", NULL);
	label = gtk_label_new(string);
	g_free(string);
 
	gtk_label_set_use_markup(GTK_LABEL(label), TRUE);
	gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);

	return label;
}
#endif
static void
totem_properties_page_init(TotemPropertiesPage *self)
{
	BonoboPropertyBag *pb;
#if 0
	GtkWidget *label, *textview, *swin;

	self->location = NULL;

	self->table = gtk_table_new(5, 2, FALSE);
	gtk_table_set_row_spacings(GTK_TABLE(self->table), 3);
	gtk_table_set_col_spacings(GTK_TABLE(self->table), 3);
	gtk_container_set_border_width(GTK_CONTAINER(self->table), 3);

	label = make_bold_label(_("Name:"));
	gtk_table_attach(GTK_TABLE(self->table), label, 0,1, 0,1,
			GTK_FILL, GTK_FILL, 0, 0);
	gtk_widget_show(label);
	self->pkg_name = gtk_label_new(_("<none>"));
	gtk_label_set_selectable(GTK_LABEL(self->pkg_name), TRUE);
	gtk_misc_set_alignment(GTK_MISC(self->pkg_name), 0.0, 0.5);
	gtk_table_attach(GTK_TABLE(self->table), self->pkg_name, 1,2, 0,1,
			GTK_FILL|GTK_EXPAND, GTK_FILL, 0, 0);
	gtk_widget_show(self->pkg_name);

	label = make_bold_label(_("Version:"));
	gtk_table_attach(GTK_TABLE(self->table), label, 0,1, 1,2,
			GTK_FILL, GTK_FILL, 0, 0);
	gtk_widget_show(label);
	self->pkg_version = gtk_label_new("");
	gtk_label_set_selectable(GTK_LABEL(self->pkg_version), TRUE);
	gtk_misc_set_alignment(GTK_MISC(self->pkg_version), 0.0, 0.5);
	gtk_table_attach(GTK_TABLE(self->table), self->pkg_version, 1,2, 1,2,
			GTK_FILL|GTK_EXPAND, GTK_FILL, 0, 0);
	gtk_widget_show(self->pkg_version);

	label = make_bold_label(_("Group:"));
	gtk_table_attach(GTK_TABLE(self->table), label, 0,1, 2,3,
			GTK_FILL, GTK_FILL, 0, 0);
	gtk_widget_show(label);
	self->pkg_group = gtk_label_new("");
	gtk_label_set_selectable(GTK_LABEL(self->pkg_group), TRUE);
	gtk_misc_set_alignment(GTK_MISC(self->pkg_group), 0.0, 0.5);
	gtk_table_attach(GTK_TABLE(self->table), self->pkg_group, 1,2, 2,3,
			GTK_FILL|GTK_EXPAND, GTK_FILL, 0, 0);
	gtk_widget_show(self->pkg_group);

	label = make_bold_label(_("Install date:"));
	gtk_table_attach(GTK_TABLE(self->table), label, 0,1, 3,4,
			GTK_FILL, GTK_FILL, 0, 0);
	gtk_widget_show(label);
	self->pkg_installdate = gtk_label_new("");
	gtk_label_set_selectable(GTK_LABEL(self->pkg_installdate), TRUE);
	gtk_misc_set_alignment(GTK_MISC(self->pkg_installdate), 0.0, 0.5);
	gtk_table_attach(GTK_TABLE(self->table), self->pkg_installdate, 1,2, 3,4,
			GTK_FILL|GTK_EXPAND, GTK_FILL, 0, 0);
	gtk_widget_show(self->pkg_installdate);

	swin = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(swin),
			GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(swin),
			GTK_SHADOW_IN);
	gtk_table_attach(GTK_TABLE(self->table), swin, 0,2, 4,5,
			GTK_FILL|GTK_EXPAND, GTK_FILL|GTK_EXPAND, 0, 0);
	gtk_widget_show(swin);

	textview = gtk_text_view_new();
	self->pkg_description = gtk_text_view_get_buffer(GTK_TEXT_VIEW(textview));
	gtk_text_view_set_editable(GTK_TEXT_VIEW(textview), FALSE);
	gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(textview), GTK_WRAP_WORD);
	gtk_container_add(GTK_CONTAINER(swin), textview);
	gtk_widget_show(textview);

	gtk_widget_show(self->table);

	bonobo_control_construct(BONOBO_CONTROL(self), self->table);
#endif
	pb = bonobo_property_bag_new (
			(BonoboPropertyGetFn) get_property,
			(BonoboPropertySetFn) set_property, self);
	bonobo_property_bag_add(pb, "URI", PROP_URI, BONOBO_ARG_STRING,
			NULL, _("URI currently displayed"), 0);
	bonobo_control_set_properties(BONOBO_CONTROL(self),
			BONOBO_OBJREF(pb), NULL);
	bonobo_object_release_unref(BONOBO_OBJREF(pb), NULL);
}

static void
totem_properties_page_finalize (GObject *object)
{
	TotemPropertiesPage *self;

	self = NAUTILUS_RPM_PROPERTIES_PAGE (object);

	g_free(self->location);
	self->location = NULL;

	parent_class->finalize(object);
}

static void
load_location (TotemPropertiesPage *self,
	       const char *location)
{
#if 0
	gchar *filename = NULL;
	GtkTextIter start, end;
	rpmdb db = NULL;
	rpmdbMatchIterator mi = NULL;
	Header header;
	gchar *value, *version, *release, *description;
	gint32 *intval;
	gint i;

	g_assert (NAUTILUS_IS_RPM_PROPERTIES_PAGE (self));
	g_assert (location != NULL);

	/* clear out any existing info */
	gtk_label_set_text(GTK_LABEL(self->pkg_name), _("<none>"));
	gtk_label_set_text(GTK_LABEL(self->pkg_version), "");
	gtk_label_set_text(GTK_LABEL(self->pkg_group), "");
	gtk_label_set_text(GTK_LABEL(self->pkg_installdate), "");
	gtk_text_buffer_get_bounds(self->pkg_description, &start, &end);
	gtk_text_buffer_delete(self->pkg_description, &start, &end);

	filename = gnome_vfs_get_local_path_from_uri(location);
	if (rpmdbOpen(NULL, &db, O_RDONLY, 0644)) goto end;

	mi = rpmdbInitIterator(db, RPMTAG_BASENAMES, filename, 0);
	if (!mi) goto end;

	header = rpmdbNextIterator(mi);
	if (!header) goto end;

	/* we now have the header structure for the first package
	 * owning this file */
	rpmHeaderGetEntry(header, RPMTAG_NAME, NULL, (void **)&value, NULL);
	gtk_label_set_text(GTK_LABEL(self->pkg_name), value);

	rpmHeaderGetEntry(header, RPMTAG_VERSION, NULL, (void **)&version, NULL);
	rpmHeaderGetEntry(header, RPMTAG_RELEASE, NULL, (void **)&release, NULL);
	value = g_strconcat(version, "-", release, NULL);
	gtk_label_set_text(GTK_LABEL(self->pkg_version), value);
	g_free(value);

	rpmHeaderGetEntry(header, RPMTAG_GROUP, NULL, (void **)&value, NULL);
	g_strchomp(value);
	gtk_label_set_text(GTK_LABEL(self->pkg_group), value);

	rpmHeaderGetEntry(header, RPMTAG_INSTALLTIME, NULL, (void **)&intval,NULL);
	if (intval) {
		gchar buf[100];
		time_t tm = *intval;

		strftime(buf, sizeof(buf), "%a %b %d %I:%M:%S %Z %Y", localtime(&tm));
		gtk_label_set_label(GTK_LABEL(self->pkg_installdate), buf);
	}

	rpmHeaderGetEntry(header, RPMTAG_DESCRIPTION, NULL, (void **)&value, NULL);
	description = g_strdup(value);
	for (i = 0; description[i] != '\0'; i++) {
		gboolean rewrap = (description[i] != ' ');

		while (description[i] != '\n' && description[i] != '\0') i++;
		if (description[i] == '\n') {
			if (rewrap && description[i+1] != '\n')
				description[i] = ' ';
		}
		i++;
	}
	gtk_text_buffer_set_text(self->pkg_description, description, -1);
	g_free(description);

end:
	if (mi) rpmdbFreeIterator(mi);
	rpmdbClose(db);
	g_free(filename);
#endif
}

static void
get_property(BonoboPropertyBag *bag,
	     BonoboArg         *arg,
	     guint              arg_id,
	     CORBA_Environment *ev,
	     TotemPropertiesPage *self)
{
	if (arg_id == PROP_URI) {
		BONOBO_ARG_SET_STRING(arg, self->location);
	}
}

static void
set_property(BonoboPropertyBag *bag,
	     const BonoboArg   *arg,
	     guint              arg_id,
	     CORBA_Environment *ev,
	     TotemPropertiesPage *self)
{
	if (arg_id == PROP_URI) {
		g_free(self->location);
		self->location = g_strdup(BONOBO_ARG_GET_STRING(arg));

		load_location(self, self->location);
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
		("OAFIID:Nautilus_Rpm_PropertiesPage_Factory",
		 view_factory, NULL);
}

