/*
 * SPDX-License-Identifier: GPL-3-or-later
 */

#include <gtk/gtk.h>
#include <grilo.h>
#include <libhandy-1/handy.h>
#include <icon-helpers.h>

typedef struct {
	GrlSource parent;
} GrlRaitvSource;

typedef struct {
	GrlSourceClass parent_class;
} GrlRaitvSourceClass;

GType grl_raitv_source_get_type (void) G_GNUC_CONST;
#define GRL_TYPE_RAITV_SOURCE grl_raitv_source_get_type()
G_DEFINE_TYPE (GrlRaitvSource, grl_raitv_source, GRL_TYPE_SOURCE)

static void
grl_raitv_source_init (GrlRaitvSource *self)
{
}

static void
grl_raitv_source_class_init (GrlRaitvSourceClass *klass)
{
}

static GObject *
channel_object (const char *url)
{
	GrlSource *source;
	GIcon *icon;
	GFile *file;

	file = g_file_new_for_uri (url);
	icon = g_file_icon_new (file);
	g_object_unref (file);
	source = g_object_new (GRL_TYPE_RAITV_SOURCE,
			       "source-icon", icon,
			       NULL);
	g_object_unref (icon);

	return G_OBJECT (source);
}

static GObject *
media_object (const char *uri)
{
	GrlMedia *media;

	media = grl_media_new ();
	grl_media_set_thumbnail (media, uri);
	grl_media_set_source (media, "");

	return G_OBJECT (media);
}

static void
icon_ready (GObject      *source_object,
	    GAsyncResult *res,
	    gpointer      user_data)
{
	GdkPixbuf *pixbuf;
	GtkWidget *image = user_data;

	pixbuf = totem_grilo_get_thumbnail_finish (source_object, res, NULL);
	if (!pixbuf) {
		g_warning ("Failed load thumbnail for icon %s",
			   (char *) g_object_get_data (G_OBJECT (image), "label"));
	}

	gtk_image_set_from_pixbuf (GTK_IMAGE (image), pixbuf);
	g_clear_object (&pixbuf);
}

static void
set_icon_from_grl (GObject   *object,
		   GtkWidget *image)
{
	totem_grilo_get_thumbnail (object, NULL, icon_ready, image);
}

static const char *labels[] = {
	"Rai.tv",
	"Guardian Videos",
	"Most popular",
	"Generic Channel",
	"Generating Thumbnail",
	"No Thumbnail",
	"FPS Russia",
	"American Sniper",
	"DVD"
};

#define NUM_IMAGES G_N_ELEMENTS(labels)

int main (int argc, char **argv)
{
	HdyStyleManager *style_manager;
	GtkWidget *window, *box, *scroll;
	GtkStyleContext *context;
	GtkWidget *images[NUM_IMAGES];
	GObject *object;
	guint i;
	gboolean tmp;

	gtk_init (&argc, &argv);
	grl_init (&argc, &argv);

	hdy_init ();
	style_manager = hdy_style_manager_get_default ();
	hdy_style_manager_set_color_scheme (style_manager, HDY_COLOR_SCHEME_FORCE_DARK);

	window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	totem_grilo_setup_icons ();
	context = gtk_widget_get_style_context (window);
	gtk_style_context_add_class (context, "content-view");
	scroll = gtk_scrolled_window_new (NULL, NULL);
	gtk_container_add (GTK_CONTAINER (window), scroll);

	box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 10);
	gtk_container_add (GTK_CONTAINER (scroll), box);
	g_object_set (G_OBJECT (box),
		      "margin-start", 12,
		      "margin-end", 12,
		      "margin-top", 12,
		      "margin-bottom", 12,
		      NULL);

	for (i = 0; i < NUM_IMAGES; i++) {
		GtkWidget *inbox;

		inbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
		images[i] = gtk_image_new ();
		g_object_set_data_full (G_OBJECT (images[i]), "label", g_strdup (labels[i]), g_free);
		gtk_container_add (GTK_CONTAINER (inbox), images[i]);
		gtk_container_add (GTK_CONTAINER (inbox),
				   gtk_label_new (labels[i]));

		gtk_container_add (GTK_CONTAINER (box), inbox);
	}

	i = 0;

	object = channel_object ("https://raw.githubusercontent.com/GNOME/grilo-plugins/master/src/raitv/channel-rai.svg");
	set_icon_from_grl (object, images[i++]);

	object = channel_object ("https://raw.githubusercontent.com/GNOME/grilo-plugins/master/src/lua-factory/sources/guardianvideos.svg");
	set_icon_from_grl (object, images[i++]);

	gtk_image_set_from_pixbuf (GTK_IMAGE (images[i++]),
				   (GdkPixbuf *) totem_grilo_get_box_icon ());

	gtk_image_set_from_pixbuf (GTK_IMAGE (images[i++]),
				   (GdkPixbuf *) totem_grilo_get_channel_icon ());

	object = media_object ("file:///somewhere/over/the/rainbow.png");
	gtk_image_set_from_pixbuf (GTK_IMAGE (images[i++]),
				   totem_grilo_get_icon (GRL_MEDIA (object), &tmp));

	object = media_object (NULL);
	gtk_image_set_from_pixbuf (GTK_IMAGE (images[i++]),
				   totem_grilo_get_icon (GRL_MEDIA (object), &tmp));

	object = media_object ("https://i.ytimg.com/vi/sEgd5cu_vMg/mqdefault.jpg");
	set_icon_from_grl (object, images[i++]);

	object = media_object ("http://trailers.apple.com/trailers/wb/americansniper/images/poster-xlarge.jpg");
	set_icon_from_grl (object, images[i++]);

	gtk_image_set_from_pixbuf (GTK_IMAGE (images[i++]),
				   (GdkPixbuf *) totem_grilo_get_optical_icon ());

	gtk_widget_show_all (window);
	gtk_window_maximize (GTK_WINDOW (window));
	gtk_main ();

	return 0;
}
