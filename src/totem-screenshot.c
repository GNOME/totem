/* totem-screenshot.c

   Copyright (C) 2004 Bastien Nocera

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
#include "totem-screenshot.h"

#include <gtk/gtk.h>
#include <glade/glade.h>
#include <gconf/gconf-client.h>
#include <string.h>

#include "debug.h"

#ifdef HAVE_GTK_ONLY
#ifdef ENABLE_NLS
#include <libintl.h>
#define _(String) dgettext(GETTEXT_PACKAGE,String)
#ifdef gettext_noop
#define N_(String) gettext_noop(String)
#else
#define N_(String) (String)
#endif /* gettext_noop */
#else
#define _(String) (String)
#define N_(String) (String)
#endif /* ENABLE_NLS */
#else
#include <libgnome/gnome-i18n.h>
#endif /* HAVE_GTK_ONLY */

struct TotemScreenshotPrivate
{
	GladeXML *xml;
	GdkPixbuf *pixbuf;
};

static const GtkTargetEntry target_table[] = {
	{ "text/uri-list", 0, 0 },
};

static GtkWidgetClass *parent_class = NULL;

static void totem_screenshot_class_init (TotemScreenshotClass *class);
static void totem_screenshot_init       (TotemScreenshot      *screenshot);

GtkType
totem_screenshot_get_type (void)
{
	static GtkType totem_screenshot_type = 0;

	if (!totem_screenshot_type) {
		static const GTypeInfo totem_screenshot_info = {
			sizeof (TotemScreenshotClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) totem_screenshot_class_init,
			(GClassFinalizeFunc) NULL,
			NULL /* class_data */,
			sizeof (TotemScreenshot),
			0 /* n_preallocs */,
			(GInstanceInitFunc) totem_screenshot_init,
		};

		totem_screenshot_type = g_type_register_static (GTK_TYPE_DIALOG,
				"TotemScreenshot", &totem_screenshot_info,
				(GTypeFlags)0);
	}

	return totem_screenshot_type;
}

static void
totem_screenshot_action_error (char *title, char *reason,
		TotemScreenshot *screenshot)
{
	GtkWidget *error_dialog;
	char *title_esc, *reason_esc;

	title_esc = g_markup_escape_text (title, -1);
	reason_esc = g_markup_escape_text (reason, -1);

	error_dialog =
		gtk_message_dialog_new (GTK_WINDOW (screenshot),
				GTK_DIALOG_MODAL,
				GTK_MESSAGE_ERROR,
				GTK_BUTTONS_OK,
				"<b>%s</b>\n%s.", title_esc, reason_esc);
	g_free (title_esc);
	g_free (reason_esc);

	gtk_container_set_border_width (GTK_CONTAINER (error_dialog), 5);
	gtk_dialog_set_default_response (GTK_DIALOG (error_dialog),
			GTK_RESPONSE_OK);
	gtk_label_set_use_markup (GTK_LABEL (GTK_MESSAGE_DIALOG (error_dialog)->label), TRUE);
	g_signal_connect (G_OBJECT (error_dialog), "destroy", G_CALLBACK
			(gtk_widget_destroy), error_dialog);
	g_signal_connect (G_OBJECT (error_dialog), "response", G_CALLBACK
			(gtk_widget_destroy), error_dialog);
	gtk_window_set_modal (GTK_WINDOW (error_dialog), TRUE);

	gtk_widget_show (error_dialog);
}

static char *
screenshot_make_filename_helper (char *filename, gboolean desktop_exists)
{
	gboolean home_as_desktop;
	GConfClient *gc;

	gc = gconf_client_get_default ();
	home_as_desktop = gconf_client_get_bool (gc,
			"/apps/nautilus/preferences/desktop_is_home_dir",
			NULL);
	g_object_unref (G_OBJECT (gc));

	if (desktop_exists != FALSE && home_as_desktop == FALSE)
	{
		char *fullpath;

		fullpath = g_build_path (G_DIR_SEPARATOR_S, g_get_home_dir (),
				"Desktop", NULL);
		desktop_exists = g_file_test (fullpath, G_FILE_TEST_EXISTS);
		g_free (fullpath);

		if (desktop_exists != FALSE)
		{
			return g_build_filename (g_get_home_dir (),
					"Desktop", filename, NULL);
		} else {
			return g_build_filename (g_get_home_dir (),
					 ".gnome-desktop", filename, NULL);
		}
	} else {
		return g_build_filename (g_get_home_dir (), filename, NULL);
	}
}

static char *
screenshot_make_filename (TotemScreenshot *screenshot)
{
	GtkWidget *radiobutton, *entry;
	gboolean on_desktop;
	char *fullpath, *filename;
	int i = 0;
	gboolean desktop_exists;

	radiobutton = glade_xml_get_widget (screenshot->_priv->xml,
			"tsw_save2desk_radiobutton");
	on_desktop = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON
			(radiobutton));

	/* Test if we have a desktop directory */
	fullpath = g_build_path (G_DIR_SEPARATOR_S, g_get_home_dir (),
			"Desktop", NULL);
	desktop_exists = g_file_test (fullpath, G_FILE_TEST_EXISTS);
	g_free (fullpath);
	if (desktop_exists == FALSE)
	{
		fullpath = g_build_path (G_DIR_SEPARATOR_S, g_get_home_dir (),
				".gnome-desktop", NULL);
		desktop_exists = g_file_test (fullpath, G_FILE_TEST_EXISTS);
		g_free (fullpath);
	}

	if (on_desktop != FALSE)
	{
		filename = g_strdup_printf (_("Screenshot%d.png"), i);
		fullpath = screenshot_make_filename_helper (filename,
				desktop_exists);

		while (g_file_test (fullpath, G_FILE_TEST_EXISTS) != FALSE
				&& i < G_MAXINT)
		{
			i++;
			g_free (filename);
			g_free (fullpath);

			filename = g_strdup_printf (_("Screenshot%d.png"), i);
			fullpath = screenshot_make_filename_helper (filename,
					desktop_exists);
		}

		g_free (filename);
	} else {
		entry = glade_xml_get_widget (screenshot->_priv->xml, "tsw_save2file_combo_entry");
		if (gtk_entry_get_text (GTK_ENTRY (entry)) == NULL)
			return NULL;

		fullpath = g_strdup (gtk_entry_get_text (GTK_ENTRY (entry)));
	}

	return fullpath;
}

static void
on_radiobutton_shot_toggled (GtkToggleButton *togglebutton,
		TotemScreenshot *screenshot)
{	
	GtkWidget *radiobutton, *entry;

	radiobutton = glade_xml_get_widget (screenshot->_priv->xml, "tsw_save2file_radiobutton");
	entry = glade_xml_get_widget (screenshot->_priv->xml, "tsw_save2file_fileentry");
	gtk_widget_set_sensitive (entry, gtk_toggle_button_get_active
			(GTK_TOGGLE_BUTTON (radiobutton)));
}

static void
totem_screenshot_response (TotemScreenshot *screenshot, int response)
{
	char *filename;
	GError *err = NULL;

	if (response == GTK_RESPONSE_OK)
	{
		filename = screenshot_make_filename (screenshot);
		if (g_file_test (filename, G_FILE_TEST_EXISTS) != FALSE)
		{
			char *msg;

			msg = g_strdup_printf (_("File '%s' already exists."), filename);
			totem_screenshot_action_error (msg,
					_("The screenshot was not saved"),
					screenshot);
			g_free (msg);
			g_free (filename);
			return;
		}

		if (gdk_pixbuf_save (screenshot->_priv->pixbuf,
					filename, "png", &err, NULL) == FALSE)
		{
			totem_screenshot_action_error
				(_("There was an error saving the screenshot."),
				 err->message, screenshot);
			g_error_free (err);
		}

		g_free (filename);
	}
}

static void
totem_screenshot_init (TotemScreenshot *screenshot)
{
	screenshot->_priv = g_new0 (TotemScreenshotPrivate, 1);

	gtk_container_set_border_width (GTK_CONTAINER (screenshot), 5);
}

static void
totem_screenshot_finalize (GObject *object)
{
	TotemScreenshot *screenshot = TOTEM_SCREENSHOT (object);

	g_return_if_fail (object != NULL);

	if (screenshot->_priv->pixbuf != NULL)
		gdk_pixbuf_unref (screenshot->_priv->pixbuf);

	if (G_OBJECT_CLASS (parent_class)->finalize != NULL) {
		(* G_OBJECT_CLASS (parent_class)->finalize) (object);
	}
}

GtkWidget*
totem_screenshot_new (const char *glade_filename, GdkPixbuf *screen_image)
{
	TotemScreenshot *screenshot;
	GtkWidget *container, *item;
	char *filename;
	GdkPixbuf *scaled;
	GtkWidget *dialog, *image, *entry;
	int width, height;

	g_return_val_if_fail (glade_filename != NULL, NULL);

	screenshot = TOTEM_SCREENSHOT (g_object_new (GTK_TYPE_SCREENSHOT, NULL));

	screenshot->_priv->xml = glade_xml_new (glade_filename, "vbox11", NULL);
	if (screenshot->_priv->xml == NULL)
	{
		totem_screenshot_finalize (G_OBJECT (screenshot));
		return NULL;
	}

	screenshot->_priv->pixbuf = screen_image;
	g_object_ref (screenshot->_priv->pixbuf);

	gtk_window_set_title (GTK_WINDOW (screenshot), _("Save Screenshot"));
	gtk_dialog_add_buttons (GTK_DIALOG (screenshot),
			GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE,
			GTK_STOCK_SAVE, GTK_RESPONSE_OK,
			NULL);
	g_signal_connect (G_OBJECT (screenshot), "response",
			G_CALLBACK (totem_screenshot_response),
			screenshot);
	/* Screenshot dialog */
	item = glade_xml_get_widget (screenshot->_priv->xml,
			"totem_screenshot_window");
	//FIXME
//	g_signal_connect (G_OBJECT (item), "delete-event",
//			G_CALLBACK (hide_screenshot), totem);
	item = glade_xml_get_widget (screenshot->_priv->xml,
			"tsw_save2file_radiobutton");
	g_signal_connect (G_OBJECT (item), "toggled",
			G_CALLBACK (on_radiobutton_shot_toggled),
			screenshot);
	item = glade_xml_get_widget (screenshot->_priv->xml,
			"tsw_save2desk_radiobutton");
	g_signal_connect (G_OBJECT (item), "toggled",
			G_CALLBACK (on_radiobutton_shot_toggled),
			screenshot);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (item), TRUE);

	filename = screenshot_make_filename (screenshot);
	height = 200;
	width = height * gdk_pixbuf_get_width (screen_image)
		/ gdk_pixbuf_get_height (screen_image);
	scaled = gdk_pixbuf_scale_simple (screen_image,
			width, height, GDK_INTERP_BILINEAR);

	dialog = glade_xml_get_widget (screenshot->_priv->xml,
			"totem_screenshot_window");
	image = glade_xml_get_widget (screenshot->_priv->xml, "tsw_shot_image");
	gtk_image_set_from_pixbuf (GTK_IMAGE (image), scaled);
	gdk_pixbuf_unref (scaled);
	entry = glade_xml_get_widget (screenshot->_priv->xml,
			"tsw_save2file_combo_entry");
	gtk_entry_set_text (GTK_ENTRY (entry), filename);
	g_free (filename);

	item = glade_xml_get_widget (screenshot->_priv->xml,
			"tsw_save2file_fileentry");
	{
		GValue value = { 0, };
		g_value_init (&value, G_TYPE_BOOLEAN);
		g_value_set_boolean (&value, TRUE);
		g_object_set_property (G_OBJECT (item),
				"use_filechooser", &value);
	}

	container = glade_xml_get_widget (screenshot->_priv->xml, "vbox11");
	gtk_box_pack_start (GTK_BOX (GTK_DIALOG (screenshot)->vbox),
			container,
			TRUE,       /* expand */
			TRUE,       /* fill */
			0);         /* padding */

	gtk_widget_show_all (GTK_DIALOG (screenshot)->vbox);

	return GTK_WIDGET (screenshot);
}

static void
totem_screenshot_class_init (TotemScreenshotClass *klass)
{
	parent_class = gtk_type_class (gtk_dialog_get_type ());

	G_OBJECT_CLASS (klass)->finalize = totem_screenshot_finalize;
	//FIXME override response
}

