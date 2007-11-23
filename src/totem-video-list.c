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
#include <glib/gprintf.h>

#include "totem.h"
#include "totem-video-list.h"
#include "totem-private.h"
#include "totemvideolist-marshal.h"

struct _TotemVideoListPrivate {
	gboolean dispose_has_run;
	gint tooltip_column;
	gint mrl_column;
	Totem *totem;
};

#define TOTEM_VIDEO_LIST_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), TOTEM_TYPE_VIDEO_LIST, TotemVideoListPrivate))

enum {
	PROP_TOOLTIP_COLUMN = 1,
	PROP_MRL_COLUMN,
	PROP_TOTEM
};

enum {
	STARTING_VIDEO,
	LAST_SIGNAL
};

static void totem_video_list_set_property (GObject *object, guint property_id, const GValue *value, GParamSpec *pspec);
static void totem_video_list_get_property (GObject *object, guint property_id, GValue *value, GParamSpec *pspec);
static gboolean query_tooltip_cb (GtkWidget *widget, gint x, gint y, gboolean keyboard_mode, GtkTooltip *tooltip, gpointer user_data);
static void selection_changed_cb (GtkTreeSelection *selection, GtkWidget *tree_view);
static void row_activated_cb (GtkTreeView *tree_view, GtkTreePath *path, GtkTreeViewColumn *column, gpointer user_data);
static gboolean default_starting_video_cb (TotemVideoList *video_list, GtkTreePath *path);

static gint totem_video_list_table_signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE (TotemVideoList, totem_video_list, GTK_TYPE_TREE_VIEW)

TotemVideoList *
totem_video_list_new (void)
{
	return g_object_new (TOTEM_TYPE_VIDEO_LIST, NULL); 
}

static void
totem_video_list_class_init (TotemVideoListClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	g_type_class_add_private (klass, sizeof (TotemVideoListPrivate));

	object_class->set_property = totem_video_list_set_property;
	object_class->get_property = totem_video_list_get_property;

	g_object_class_install_property (object_class, PROP_TOOLTIP_COLUMN,
				g_param_spec_int ("tooltip-column", NULL, NULL,
					-1, G_MAXINT, -1, G_PARAM_READWRITE));
	g_object_class_install_property (object_class, PROP_MRL_COLUMN,
				g_param_spec_int ("mrl-column", NULL, NULL,
					-1, G_MAXINT, -1, G_PARAM_READWRITE));
	g_object_class_install_property (object_class, PROP_TOTEM,
				g_param_spec_object ("totem", NULL, NULL,
					TOTEM_TYPE_OBJECT, G_PARAM_READWRITE));

	klass->starting_video = default_starting_video_cb;
	totem_video_list_table_signals[STARTING_VIDEO] = g_signal_new ("starting-video",
				G_TYPE_FROM_CLASS (object_class),
				G_SIGNAL_RUN_LAST,
				G_STRUCT_OFFSET (TotemVideoListClass, starting_video),
				NULL, NULL,
				totemvideolist_marshal_BOOLEAN__OBJECT_OBJECT,
				G_TYPE_BOOLEAN, 2,
				TOTEM_TYPE_VIDEO_LIST, GTK_TYPE_TREE_PATH);
}

static void
totem_video_list_init (TotemVideoList *self)
{
	self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, TOTEM_TYPE_VIDEO_LIST, TotemVideoListPrivate);
	self->priv->dispose_has_run = FALSE;
	self->priv->totem = NULL;
	self->priv->tooltip_column = -1;
	self->priv->mrl_column = -1;

	/* Set up tooltips */
	g_object_set (self, "has-tooltip", TRUE, NULL);
	g_signal_connect (self, "row-activated", G_CALLBACK (row_activated_cb), NULL);
	g_signal_connect (self, "query-tooltip", G_CALLBACK (query_tooltip_cb), NULL);
	g_signal_connect (gtk_tree_view_get_selection (GTK_TREE_VIEW (self)),
				"changed", G_CALLBACK (selection_changed_cb), GTK_TREE_VIEW (self));
}

static void
totem_video_list_set_property (GObject *object, guint property_id, const GValue *value, GParamSpec *pspec)
{
	TotemVideoListPrivate *priv = TOTEM_VIDEO_LIST_GET_PRIVATE (object);

	switch (property_id)
	{
		case PROP_TOOLTIP_COLUMN:
			priv->tooltip_column = g_value_get_int (value);
			break;
		case PROP_MRL_COLUMN:
			priv->mrl_column = g_value_get_int (value);
			break;
		case PROP_TOTEM:
			if (priv->totem != NULL)
				g_object_unref (priv->totem);
			priv->totem = (Totem*) g_value_dup_object (value);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
	}
}

static void
totem_video_list_get_property (GObject *object, guint property_id, GValue *value, GParamSpec *pspec)
{
	TotemVideoListPrivate *priv = TOTEM_VIDEO_LIST_GET_PRIVATE (object);

	switch (property_id)
	{
		case PROP_TOOLTIP_COLUMN:
			g_value_set_int (value, priv->tooltip_column);
			break;
		case PROP_MRL_COLUMN:
			g_value_set_int (value, priv->mrl_column);
			break;
		case PROP_TOTEM:
			g_value_set_object (value, G_OBJECT (priv->totem));
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
	}
}

static gboolean
query_tooltip_cb (GtkWidget *widget, gint x, gint y, gboolean keyboard_mode, GtkTooltip *tooltip, gpointer user_data)
{
	TotemVideoList *self = TOTEM_VIDEO_LIST (widget);
	GtkTreeIter iter;
	gchar *tooltip_text;
	gchar *mrl_text;
	gchar *final_text;
	GtkTreeView *tree_view = GTK_TREE_VIEW (widget);
	GtkTreeModel *model = gtk_tree_view_get_model (tree_view);
	GtkTreePath *path = NULL;

	if (self->priv->tooltip_column == -1)
		return FALSE;

	if (!gtk_tree_view_get_tooltip_context (tree_view, &x, &y,
				keyboard_mode,
				&model, &path, &iter))
		return FALSE;

	if (self->priv->mrl_column == -1) {
		gtk_tree_model_get (model, &iter, self->priv->tooltip_column, &tooltip_text, -1);
		gtk_tooltip_set_text (tooltip, tooltip_text);
		g_free (tooltip_text);
	} else {
		gtk_tree_model_get (model, &iter,
				self->priv->tooltip_column, &tooltip_text,
				self->priv->mrl_column, &mrl_text,
				-1);
		final_text = g_strdup_printf ("%s\n%s", tooltip_text, mrl_text);
		gtk_tooltip_set_text (tooltip, final_text);

		g_free (tooltip_text);
		g_free (mrl_text);
		g_free (final_text);
	}

	gtk_tree_view_set_tooltip_row (tree_view, tooltip, path);
	gtk_tree_path_free (path);

	return TRUE;
}

static void
selection_changed_cb (GtkTreeSelection *selection, GtkWidget *tree_view)
{
	gtk_widget_trigger_tooltip_query (tree_view);
}

static void
row_activated_cb (GtkTreeView *tree_view, GtkTreePath *path, GtkTreeViewColumn *column, gpointer user_data)
{
	GtkTreeIter iter;
	gchar *mrl, *display_name;
	gboolean play_video = TRUE;
	TotemVideoList *self = TOTEM_VIDEO_LIST (tree_view);
	GtkTreeModel *model = gtk_tree_view_get_model (tree_view);

	if (self->priv->mrl_column == -1)
		return;

	g_signal_emit (G_OBJECT (tree_view), totem_video_list_table_signals[STARTING_VIDEO], 0,
				self,
				path,
				&play_video);

	if (play_video == FALSE)
		return;

	gtk_tree_model_get_iter (model, &iter, path);
	gtk_tree_model_get (model, &iter,
				self->priv->mrl_column, &mrl,
				self->priv->tooltip_column, &display_name,
				-1);

	totem_add_to_playlist_and_play (self->priv->totem, mrl, display_name, FALSE);

	g_free (mrl);
	g_free (display_name);
}

static gboolean
default_starting_video_cb (TotemVideoList *video_list, GtkTreePath *path)
{
	/* Play the video by default */
	return TRUE;
}

