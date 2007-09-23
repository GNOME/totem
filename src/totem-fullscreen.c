/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * Copyright (C) 2001-2007 Bastien Nocera <hadess@hadess.net>
 * Copyright (C) 2007 Sunil Mohan Adapa <sunilmohan@gnu.org.in>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * The Totem project hereby grant permission for non-gpl compatible GStreamer
 * plugins to be used and distributed together with GStreamer and Totem. This
 * permission are above and beyond the permissions granted by the GPL license
 * Totem is covered by.
 *
 */

#include <string.h>

#include <glib.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>

#include "totem-fullscreen.h"
#include "totem-interface.h"
#include "totem-time-label.h"
#include "bacon-video-widget.h"

#define FULLSCREEN_POPUP_TIMEOUT 5

static GObjectClass *parent_class = NULL;

static void     totem_fullscreen_class_init (TotemFullscreenClass *class);
static void     totem_fullscreen_init       (TotemFullscreen      *parser);
static void     totem_fullscreen_finalize   (GObject              *object);
static gboolean totem_fullscreen_popup_hide (TotemFullscreen      *fs);

/* Callback functions for GtkBuilder */
gboolean totem_fullscreen_vol_slider_pressed_cb (GtkWidget *widget, GdkEventButton *event, TotemFullscreen *fs);
gboolean totem_fullscreen_vol_slider_released_cb (GtkWidget *widget, GdkEventButton *event, TotemFullscreen *fs);
gboolean totem_fullscreen_seek_slider_pressed_cb (GtkWidget *widget, GdkEventButton *event, TotemFullscreen *fs);
gboolean totem_fullscreen_seek_slider_released_cb (GtkWidget *widget, GdkEventButton *event, TotemFullscreen *fs);

struct TotemFullscreenPrivate {
	BaconVideoWidget *bvw;
	GtkWidget        *parent_window;

	/* Fullscreen Popups */
	GtkWidget        *exit_popup;
	GtkWidget        *control_popup;

	/* Locks for keeping the popups during adjustments */
	gboolean          seek_lock;

	guint             popup_timeout;
	gboolean          popup_in_progress;

	GtkBuilder       *xml;
};

G_DEFINE_TYPE(TotemFullscreen, totem_fullscreen, G_TYPE_OBJECT)

gboolean
totem_fullscreen_is_fullscreen (TotemFullscreen *fs)
{
	g_return_val_if_fail (TOTEM_IS_FULLSCREEN (fs), FALSE);

	return (fs->is_fullscreen != FALSE);
}

static void
totem_fullscreen_move_popups (TotemFullscreen *fs)
{
	int exit_width,    exit_height;
	int control_width, control_height;
	
	GdkScreen              *screen;
	GdkRectangle            fullscreen_rect;
	TotemFullscreenPrivate *priv = fs->priv;

	/* Obtain the screen rectangle */
	screen = gtk_window_get_screen (GTK_WINDOW (fs->priv->parent_window));
	gdk_screen_get_monitor_geometry (screen,
					 gdk_screen_get_monitor_at_window (screen, fs->priv->parent_window->window),
					 &fullscreen_rect);

	/* Get the popup window sizes */
	gtk_window_get_size (GTK_WINDOW (priv->exit_popup),
			     &exit_width, &exit_height);
	gtk_window_get_size (GTK_WINDOW (priv->control_popup),
			     &control_width, &control_height);

	/* We take the full width of the screen */
	gtk_window_resize (GTK_WINDOW (priv->control_popup),
			   fullscreen_rect.width, control_height);

	if (gtk_widget_get_direction (priv->exit_popup) == GTK_TEXT_DIR_RTL) {
		gtk_window_move (GTK_WINDOW (priv->exit_popup),
				 fullscreen_rect.x,
				 fullscreen_rect.y);
		gtk_window_move (GTK_WINDOW (priv->control_popup),
				 fullscreen_rect.width - control_width,
				 fullscreen_rect.height + fullscreen_rect.y -
				 control_height);
	} else {
		gtk_window_move (GTK_WINDOW (priv->exit_popup),
				 fullscreen_rect.width + fullscreen_rect.x -
				 exit_width,
				 fullscreen_rect.y);
		gtk_window_move (GTK_WINDOW (priv->control_popup),
				 fullscreen_rect.x,
				 fullscreen_rect.height + fullscreen_rect.y -
				 control_height);
	}
}

static void
totem_fullscreen_size_changed_cb (GdkScreen *screen, TotemFullscreen *fs)
{
	totem_fullscreen_move_popups (fs);
}

static void
totem_fullscreen_theme_changed_cb (GtkIconTheme *icon_theme, TotemFullscreen *fs)
{
	totem_fullscreen_move_popups (fs);
}

static void
totem_fullscreen_window_realize_cb (GtkWidget *widget, TotemFullscreen *fs)
{
	GdkScreen *screen;
	
	screen = gtk_widget_get_screen (widget);
	g_signal_connect (G_OBJECT (screen), "size-changed",
			  G_CALLBACK (totem_fullscreen_size_changed_cb), fs);
	g_signal_connect (G_OBJECT (gtk_icon_theme_get_for_screen (screen)),
			  "changed",
			  G_CALLBACK (totem_fullscreen_theme_changed_cb), fs);
}

static void
totem_fullscreen_window_unrealize_cb (GtkWidget *widget, TotemFullscreen *fs)
{
	GdkScreen *screen;

	screen = gtk_widget_get_screen (widget);
	g_signal_handlers_disconnect_by_func (screen,
					      G_CALLBACK (totem_fullscreen_size_changed_cb), fs);
	g_signal_handlers_disconnect_by_func (gtk_icon_theme_get_for_screen (screen),
					      G_CALLBACK (totem_fullscreen_theme_changed_cb), fs);
}

gboolean
totem_fullscreen_seek_slider_pressed_cb (GtkWidget *widget,
					 GdkEventButton *event,
					 TotemFullscreen *fs)
{
	fs->priv->seek_lock = TRUE;
	return FALSE;
}

gboolean
totem_fullscreen_seek_slider_released_cb (GtkWidget *widget,
					  GdkEventButton *event,
					  TotemFullscreen *fs)
{
	fs->priv->seek_lock = FALSE;
	return FALSE;
}

static void
totem_fullscreen_popup_timeout_add (TotemFullscreen *fs)
{
	fs->priv->popup_timeout = g_timeout_add_seconds (FULLSCREEN_POPUP_TIMEOUT,
							 (GSourceFunc) totem_fullscreen_popup_hide, fs);
}

static void
totem_fullscreen_popup_timeout_remove (TotemFullscreen *fs)
{
	if (fs->priv->popup_timeout != 0) {
		g_source_remove (fs->priv->popup_timeout);
		fs->priv->popup_timeout = 0;
	}
}

static void
totem_fullscreen_set_cursor (TotemFullscreen *fs, gboolean state)
{
	if (fs->priv->bvw != NULL)
		bacon_video_widget_set_show_cursor (fs->priv->bvw, state);
}

static gboolean
totem_fullscreen_is_volume_popup_visible (TotemFullscreen *fs)
{
	GtkWidget *toplevel;

	/* FIXME we should use the popup-visible property instead */
	toplevel = gtk_widget_get_toplevel (GTK_SCALE_BUTTON (fs->volume)->plus_button);
	return GTK_WIDGET_VISIBLE (toplevel);
}

static void
totem_fullscreen_force_popup_hide (TotemFullscreen *fs)
{
	/* Popdown the volume button if it's visible */
	if (totem_fullscreen_is_volume_popup_visible (fs))
		gtk_bindings_activate (GTK_OBJECT (fs->volume), GDK_Escape, 0);

	gtk_widget_hide (fs->priv->exit_popup);
	gtk_widget_hide (fs->priv->control_popup);

	totem_fullscreen_popup_timeout_remove (fs);

	totem_fullscreen_set_cursor (fs, FALSE);
}

static gboolean
totem_fullscreen_popup_hide (TotemFullscreen *fs)
{
	if (fs->priv->bvw == NULL || totem_fullscreen_is_fullscreen (fs) == FALSE)
		return TRUE;

	if (fs->priv->seek_lock != FALSE || totem_fullscreen_is_volume_popup_visible (fs) != FALSE)
		return TRUE;

	totem_fullscreen_force_popup_hide (fs);

	return FALSE;
}

gboolean
totem_fullscreen_motion_notify (GtkWidget *widget, GdkEventMotion *event,
				TotemFullscreen *fs)
{
	GtkWidget *item;

	if (totem_fullscreen_is_fullscreen (fs) == FALSE) 
		return FALSE;

	if (fs->priv->popup_in_progress != FALSE)
		return FALSE;

	if (gtk_window_is_active (GTK_WINDOW (fs->priv->parent_window)) == FALSE)
		return FALSE;

	fs->priv->popup_in_progress = TRUE;

	totem_fullscreen_popup_timeout_remove (fs);

	/* FIXME: is this really required while we are anyway going 
	   to do a show_all on its parent control_popup? */
	item = GTK_WIDGET (gtk_builder_get_object (fs->priv->xml, "tcw_hbox"));
	gtk_widget_show_all (item);
	gdk_flush ();

	/* Show the popup widgets */
	totem_fullscreen_move_popups (fs);
	gtk_widget_show_all (fs->priv->exit_popup);
	gtk_widget_show_all (fs->priv->control_popup);

	/* Show the mouse cursor */
	totem_fullscreen_set_cursor (fs, TRUE);

	/* Reset the popup timeout */
	totem_fullscreen_popup_timeout_add (fs);

	fs->priv->popup_in_progress = FALSE;

	return FALSE;
}

void
totem_fullscreen_set_fullscreen (TotemFullscreen *fs,
				 gboolean fullscreen)
{
	g_return_if_fail (TOTEM_IS_FULLSCREEN (fs));

	totem_fullscreen_force_popup_hide (fs);

	bacon_video_widget_set_fullscreen (fs->priv->bvw, fullscreen);
	totem_fullscreen_set_cursor (fs, !fullscreen);

	fs->is_fullscreen = fullscreen;
}
static void
totem_fullscreen_parent_window_notify (GtkWidget *parent_window,
				       GParamSpec *property,
				       TotemFullscreen *fs)
{
	if (totem_fullscreen_is_fullscreen (fs) == FALSE)
		return;

	if (parent_window == fs->priv->parent_window &&
	    gtk_window_is_active (GTK_WINDOW (parent_window)) == FALSE) {
		totem_fullscreen_force_popup_hide (fs);
		totem_fullscreen_set_cursor (fs, TRUE);
	} else {
		totem_fullscreen_set_cursor (fs, FALSE);
	}
}

TotemFullscreen *
totem_fullscreen_new (GtkWindow *toplevel_window)
{
	TotemFullscreenPrivate * priv;

        TotemFullscreen *fs = TOTEM_FULLSCREEN (g_object_new 
						(TOTEM_TYPE_FULLSCREEN, NULL));


	priv = fs->priv;
	priv->seek_lock = FALSE;
	priv->xml = totem_interface_load ("fullscreen.ui", TRUE, NULL, fs);

	priv->exit_popup = GTK_WIDGET (gtk_builder_get_object (priv->xml,
				"totem_exit_fullscreen_window"));
	priv->control_popup = GTK_WIDGET (gtk_builder_get_object (priv->xml,
				"totem_controls_window"));
	fs->time_label = GTK_WIDGET (gtk_builder_get_object (priv->xml,
				"tcw_time_display_label"));
	fs->buttons_box = GTK_WIDGET (gtk_builder_get_object (fs->priv->xml,
				"tcw_buttons_hbox"));
	fs->exit_button = GTK_WIDGET (gtk_builder_get_object (priv->xml,
				"tefw_fs_exit_button"));

	fs->priv->parent_window = GTK_WIDGET (toplevel_window);

	/* Screen size and Theme changes */
	g_signal_connect (fs->priv->parent_window, "realize",
			  G_CALLBACK (totem_fullscreen_window_realize_cb), fs);
	g_signal_connect (fs->priv->parent_window, "unrealize",
			  G_CALLBACK (totem_fullscreen_window_unrealize_cb), fs);
	g_signal_connect (G_OBJECT (fs->priv->parent_window), "notify::is-active",
			  G_CALLBACK (totem_fullscreen_parent_window_notify), fs);

	/* Volume */
	fs->volume = GTK_WIDGET (gtk_builder_get_object (priv->xml, "tcw_volume_button"));
	
	/* Seek */
	fs->seek = GTK_WIDGET (gtk_builder_get_object (priv->xml, "tcw_seek_hscale"));

	/* Motion notify */
	gtk_widget_add_events (priv->exit_popup, GDK_POINTER_MOTION_MASK);
	gtk_widget_add_events (priv->control_popup, GDK_POINTER_MOTION_MASK);
	gtk_widget_add_events (fs->seek, GDK_POINTER_MOTION_MASK);
	gtk_widget_add_events (fs->exit_button, GDK_POINTER_MOTION_MASK);

	return fs;
}

void
totem_fullscreen_set_video_widget (TotemFullscreen *fs,
				   BaconVideoWidget *bvw)
{
	g_return_if_fail (TOTEM_IS_FULLSCREEN (fs));
	g_return_if_fail (BACON_IS_VIDEO_WIDGET (bvw));

	fs->priv->bvw = bvw;

	g_signal_connect (G_OBJECT (fs->priv->bvw), "motion-notify-event",
			  G_CALLBACK (totem_fullscreen_motion_notify), fs);
}

static void
totem_fullscreen_init (TotemFullscreen *fs)
{
        fs->priv = g_new0 (TotemFullscreenPrivate, 1);
}

static void
totem_fullscreen_finalize (GObject *object)
{
        TotemFullscreen *fs = TOTEM_FULLSCREEN (object);

	totem_fullscreen_popup_timeout_remove (fs);

        g_free (fs->priv);

	(* G_OBJECT_CLASS (parent_class)->finalize) (object);
}

static void
totem_fullscreen_class_init (TotemFullscreenClass *klass)
{
        GObjectClass *object_class = G_OBJECT_CLASS (klass);

        parent_class = g_type_class_peek_parent (klass);

        object_class->finalize = totem_fullscreen_finalize;
}

void
totem_fullscreen_set_title (TotemFullscreen *fs, const char *title)
{
	GtkLabel *widget;
	char *text;

	g_return_if_fail (TOTEM_IS_FULLSCREEN (fs));

	widget = GTK_LABEL (gtk_builder_get_object (fs->priv->xml, "tcw_title_label"));

	if (title != NULL) {
		char *escaped;

		escaped = g_markup_escape_text (title, -1);
		text = g_strdup_printf
			("<span size=\"medium\"><b>%s</b></span>", escaped);
		g_free (escaped);
	} else {
		text = g_strdup_printf
			("<span size=\"medium\"><b>%s</b></span>",
			 _("No File"));
	}

	gtk_label_set_markup (widget, text);
	g_free (text);
}

void
totem_fullscreen_set_seekable (TotemFullscreen *fs, gboolean seekable)
{
	GtkWidget *item;

	g_return_if_fail (TOTEM_IS_FULLSCREEN (fs));

	item = GTK_WIDGET (gtk_builder_get_object (fs->priv->xml, "tcw_time_hbox"));
	gtk_widget_set_sensitive (item, seekable);

	gtk_widget_set_sensitive (fs->seek, seekable);
}

void
totem_fullscreen_set_can_set_volume (TotemFullscreen *fs, gboolean can_set_volume)
{
	g_return_if_fail (TOTEM_IS_FULLSCREEN (fs));

	gtk_widget_set_sensitive (fs->volume, can_set_volume);
}
