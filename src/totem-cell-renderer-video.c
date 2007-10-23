/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * Copyright (C) 2001-2007 Philip Withnall <philip@tecnocode.co.uk>
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
 * Monday 7th February 2005: Christian Schaller: Add exception clause.
 * See license_change file for details.
 *
 */

#include "config.h"

#include <glib.h>
#include <glib/gi18n.h>

#include "totem.h"
#include "totem-cell-renderer-video.h"
#include "totem-private.h"

struct _TotemCellRendererVideoPrivate {
	gboolean dispose_has_run;
	gchar *title;
	GdkPixbuf *thumbnail;
	PangoAlignment alignment;
};

#define TOTEM_CELL_RENDERER_VIDEO_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), TOTEM_TYPE_CELL_RENDERER_VIDEO, TotemCellRendererVideoPrivate))

enum {
	PROP_THUMBNAIL = 1,
	PROP_TITLE,
	PROP_ALIGNMENT
};

static void totem_cell_renderer_video_set_property (GObject *object, guint property_id, const GValue *value, GParamSpec *pspec);
static void totem_cell_renderer_video_get_property (GObject *object, guint property_id, GValue *value, GParamSpec *pspec);
static void totem_cell_renderer_video_dispose (GObject *object);
static void totem_cell_renderer_video_get_size (GtkCellRenderer *cell, GtkWidget *widget, GdkRectangle *cell_area, gint *x_offset, gint *y_offset, gint *width, gint *height);
static void totem_cell_renderer_video_render (GtkCellRenderer *cell, GdkDrawable *window, GtkWidget *widget, GdkRectangle *background_area, GdkRectangle *cell_area, GdkRectangle *expose_area, GtkCellRendererState flags);

G_DEFINE_TYPE (TotemCellRendererVideo, totem_cell_renderer_video, GTK_TYPE_CELL_RENDERER)

TotemCellRendererVideo *
totem_cell_renderer_video_new (void)
{
	return g_object_new (TOTEM_TYPE_CELL_RENDERER_VIDEO, NULL); 
}

static void
totem_cell_renderer_video_class_init (TotemCellRendererVideoClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GtkCellRendererClass *renderer_class = GTK_CELL_RENDERER_CLASS (klass);

	g_type_class_add_private (klass, sizeof (TotemCellRendererVideoPrivate));

	object_class->set_property = totem_cell_renderer_video_set_property;
	object_class->get_property = totem_cell_renderer_video_get_property;
	object_class->dispose = totem_cell_renderer_video_dispose;
	renderer_class->get_size = totem_cell_renderer_video_get_size;
	renderer_class->render = totem_cell_renderer_video_render;

	g_object_class_install_property (object_class, PROP_THUMBNAIL,
				g_param_spec_object ("thumbnail", NULL, NULL,
					GDK_TYPE_PIXBUF, G_PARAM_READWRITE));
	g_object_class_install_property (object_class, PROP_TITLE,
				g_param_spec_string ("title", NULL, NULL,
					_("Unknown video"), G_PARAM_READWRITE));
	g_object_class_install_property (object_class, PROP_ALIGNMENT,
				g_param_spec_enum ("alignment", NULL, NULL,
					PANGO_TYPE_ALIGNMENT,
					PANGO_ALIGN_CENTER,
					G_PARAM_READWRITE));
}

static void
totem_cell_renderer_video_init (TotemCellRendererVideo *self)
{
	self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, TOTEM_TYPE_CELL_RENDERER_VIDEO, TotemCellRendererVideoPrivate);
	self->priv->dispose_has_run = FALSE;
	self->priv->title = NULL;
	self->priv->thumbnail = NULL;
	self->priv->alignment = PANGO_ALIGN_CENTER;

	/* Make sure we're in the right mode */
	g_object_set ((gpointer) self, "mode", GTK_CELL_RENDERER_MODE_ACTIVATABLE, NULL);
}

static void
totem_cell_renderer_video_dispose (GObject *object)
{
	TotemCellRendererVideo *self = TOTEM_CELL_RENDERER_VIDEO (object);

	/* Make sure we only run once */
	if (self->priv->dispose_has_run)
		return;
	self->priv->dispose_has_run = TRUE;

	g_free (self->priv->title);
	if (self->priv->thumbnail != NULL)
		g_object_unref (self->priv->thumbnail);

	/* Chain up to the parent class */
	G_OBJECT_CLASS (totem_cell_renderer_video_parent_class)->dispose (object);
}

static void
totem_cell_renderer_video_set_property (GObject *object, guint property_id, const GValue *value, GParamSpec *pspec)
{
	TotemCellRendererVideoPrivate *priv = TOTEM_CELL_RENDERER_VIDEO_GET_PRIVATE (object);

	switch (property_id)
	{
		case PROP_THUMBNAIL:
			if (priv->thumbnail != NULL)
				g_object_unref (priv->thumbnail);
			priv->thumbnail = (GdkPixbuf*) g_value_dup_object (value);
			break;
		case PROP_TITLE:
			g_free (priv->title);
			priv->title = g_strdup (g_value_get_string (value));
			break;
		case PROP_ALIGNMENT:
			priv->alignment = g_value_get_enum (value);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
	}
}

static void
totem_cell_renderer_video_get_property (GObject *object, guint property_id, GValue *value, GParamSpec *pspec)
{
	TotemCellRendererVideoPrivate *priv = TOTEM_CELL_RENDERER_VIDEO_GET_PRIVATE (object);

	switch (property_id)
	{
		case PROP_THUMBNAIL:
			g_value_set_object (value, G_OBJECT (priv->thumbnail));
			break;
		case PROP_TITLE:
			g_value_set_string (value, priv->title);
			break;
		case PROP_ALIGNMENT:
			g_value_set_enum (value, priv->alignment);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
	}
}

static void
get_size (GtkCellRenderer *cell,
	  GtkWidget *widget,
	  GdkRectangle *cell_area,
	  GdkRectangle *draw_area,
	  GdkRectangle *title_area,
	  GdkRectangle *thumbnail_area)
{
	TotemCellRendererVideoPrivate *priv = TOTEM_CELL_RENDERER_VIDEO_GET_PRIVATE (cell);
	guint pixbuf_width = 0;
	guint pixbuf_height = 0;
	guint title_height;
	guint calc_width;
	guint calc_height;
	PangoContext *context;
	PangoFontMetrics *metrics;
	PangoFontDescription *font_desc;

	/* Calculate thumbnail dimensions */
	if (priv->thumbnail != NULL) {
		pixbuf_width = gdk_pixbuf_get_width (priv->thumbnail);
		pixbuf_height = gdk_pixbuf_get_height (priv->thumbnail);
	}

	/* Calculate title dimensions */
	font_desc = pango_font_description_copy_static (widget->style->font_desc);
	pango_font_description_set_weight (font_desc, PANGO_WEIGHT_BOLD);
	context = gtk_widget_get_pango_context (widget);
	metrics = pango_context_get_metrics (context,
				font_desc,
				pango_context_get_language (context));

	title_height = PANGO_PIXELS (pango_font_metrics_get_ascent (metrics) +
				pango_font_metrics_get_descent (metrics));

	pango_font_metrics_unref (metrics);
	pango_font_description_free (font_desc);

	/* Calculate the total final size */
	calc_width = cell->xpad * 2 + pixbuf_width;
	calc_height = cell->ypad * 3 + pixbuf_height + title_height;

	if (draw_area) {
		if (cell_area && pixbuf_width > 0 && pixbuf_height + title_height + cell->ypad > 0) {
			draw_area->x = (((gtk_widget_get_direction (widget) == GTK_TEXT_DIR_RTL) ?
				(1.0 - cell->xalign) : cell->xalign) * 
				(cell_area->width - calc_width));
			draw_area->x = MAX (draw_area->x, 0);
			draw_area->y = (cell->yalign * (cell_area->height - calc_height));
			draw_area->y = MAX (draw_area->y, 0);
		} else {
			draw_area->x = 0;
			draw_area->y = 0;
		}

		draw_area->width = calc_width;
		draw_area->height = calc_height;

		/*if (cell_area) {
			g_message ("Cell area: X: %i, Y: %i, W: %i, H: %i", cell_area->x, cell_area->y, cell_area->width, cell_area->height);
			g_message ("X-align: %f, Y-align: %f", cell->xalign, cell->yalign);
		}
		g_message ("Draw area: X: %i, Y: %i, W: %i, H: %i", draw_area->x, draw_area->y, draw_area->width, draw_area->height);*/

		if (title_area) {
			if (cell_area) {
				title_area->width = cell_area->width;
				title_area->x = (gtk_widget_get_direction (widget) == GTK_TEXT_DIR_RTL) ? 1.0 : 0.0;
			} else {
				title_area->width = calc_width;
				title_area->x = draw_area->x;
			}

			title_area->height = title_height;
			title_area->y = draw_area->y;

			/*g_message ("Title area: X: %i, Y: %i, W: %i, H: %i", title_area->x, title_area->y, title_area->width, title_area->height);*/
		}

		if (thumbnail_area) {
			thumbnail_area->x = draw_area->x;
			thumbnail_area->y = draw_area->y + title_height + cell->ypad;
			thumbnail_area->width = cell->xpad * 2 + pixbuf_width;
			thumbnail_area->height = pixbuf_height;

			/*g_message ("Thumbnail area: X: %i, Y: %i, W: %i, H: %i", thumbnail_area->x, thumbnail_area->y, thumbnail_area->width, thumbnail_area->height);*/
		}

		/*g_message ("---");*/
	}
}

static void
totem_cell_renderer_video_get_size (GtkCellRenderer *cell,
				    GtkWidget *widget,
				    GdkRectangle *cell_area,
				    gint *x_offset,
				    gint *y_offset,
				    gint *width,
				    gint *height)
{
	GdkRectangle draw_area;
	get_size (cell, widget, cell_area, &draw_area, NULL, NULL);

	if (x_offset)
		*x_offset = draw_area.x;
	if (y_offset)
		*y_offset = draw_area.y;
	if (width)
		*width = draw_area.width;
	if (height)
		*height = draw_area.height;
}

static void
totem_cell_renderer_video_render (GtkCellRenderer *cell,
				  GdkDrawable *window,
				  GtkWidget *widget,
				  GdkRectangle *background_area,
				  GdkRectangle *cell_area,
				  GdkRectangle *expose_area,
				  GtkCellRendererState flags)
{
	TotemCellRendererVideoPrivate *priv = TOTEM_CELL_RENDERER_VIDEO_GET_PRIVATE (cell);
	GdkPixbuf *pixbuf;
	GdkRectangle draw_rect;
	GdkRectangle draw_area;
	GdkRectangle title_area;
	GdkRectangle thumbnail_area;
	cairo_t *cr;
	PangoLayout *layout;
	PangoFontDescription *desc;
	GtkStateType state;

	get_size (cell, widget, cell_area, &draw_area, &title_area, &thumbnail_area);

	draw_area.x += cell_area->x + cell->xpad;
	draw_area.y += cell_area->y + cell->ypad;
	draw_area.width -= cell->xpad * 2;
	draw_area.height -= cell->ypad * 2;

	if (!gdk_rectangle_intersect (cell_area, &draw_area, &draw_rect) ||
				!gdk_rectangle_intersect (expose_area, &draw_rect, &draw_rect))
		return;

	/* Sort out the thumbnail */
	pixbuf = priv->thumbnail;

	if (cell->is_expander || !pixbuf)
		return;

	/* Sort out the title */
	if (!cell->sensitive) {
		state = GTK_STATE_INSENSITIVE;
	} else if ((flags & GTK_CELL_RENDERER_SELECTED) == GTK_CELL_RENDERER_SELECTED) {
		if (GTK_WIDGET_HAS_FOCUS (widget))
			state = GTK_STATE_SELECTED;
		else
			state = GTK_STATE_ACTIVE;
	} else if ((flags & GTK_CELL_RENDERER_PRELIT) == GTK_CELL_RENDERER_PRELIT &&
				GTK_WIDGET_STATE (widget) == GTK_STATE_PRELIGHT) {
		state = GTK_STATE_PRELIGHT;
	} else {
		if (GTK_WIDGET_STATE (widget) == GTK_STATE_INSENSITIVE)
			state = GTK_STATE_INSENSITIVE;
		else
			state = GTK_STATE_NORMAL;
	}

	cr = gdk_cairo_create (window);

	/* Draw the title */
	layout = gtk_widget_create_pango_layout (widget, priv->title);
	desc = pango_font_description_copy_static (widget->style->font_desc);
	pango_font_description_set_weight (desc, PANGO_WEIGHT_BOLD);

	pango_layout_set_font_description (layout, desc);
	pango_layout_set_ellipsize (layout, PANGO_ELLIPSIZE_END);
	pango_layout_set_width (layout, title_area.width * PANGO_SCALE);
	pango_layout_set_alignment (layout, priv->alignment);

	gtk_paint_layout (widget->style,
				window,
				state,
				TRUE,
				expose_area,
				widget,
				"cellrenderervideotitle",
				cell_area->x + title_area.x + cell->xpad,
				cell_area->y + title_area.y + cell->ypad,
				layout);

	pango_font_description_free (desc);
	g_object_unref (layout);

	/* Draw the thumbnail */
	gdk_cairo_set_source_pixbuf (cr, pixbuf, cell_area->x + thumbnail_area.x + cell->xpad, cell_area->y + thumbnail_area.y + cell->ypad);
	gdk_cairo_rectangle (cr, &draw_rect);
	cairo_fill (cr);

	cairo_destroy (cr);
}
