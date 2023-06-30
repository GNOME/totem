/*
 * Copyright (c) 2012 Red Hat, Inc.
 *
 * SPDX-License-Identifier: GPL-3-or-later
 *
 * Author: Bastien Nocera <hadess@hadess.net>
 *
 */

#include "totem-search-entry.h"

enum {
	SIGNAL_ACTIVATE,
	LAST_SIGNAL
};

enum {
	PROP_0,
	PROP_SELECTED_ID
};

static guint signals[LAST_SIGNAL] = { 0, };

struct _TotemSearchEntry {
	GtkBox parent;
	GtkWidget *entry;
	GtkWidget *dropdown_button;
	GtkWidget *label;
	GtkWidget *popover;
	GtkWidget *listbox;
};

G_DEFINE_TYPE (TotemSearchEntry, totem_search_entry, GTK_TYPE_BOX)

static void
entry_activate_cb (GtkEntry *entry,
		   TotemSearchEntry *self)
{
	const char *text;

	text = gtk_entry_get_text (GTK_ENTRY (self->entry));
	if (text == NULL || *text == '\0')
		return;
	g_signal_emit (self, signals[SIGNAL_ACTIVATE], 0);
}

static void
listbox_row_activated (GtkListBox    *list_box,
		       GtkListBoxRow *row,
		       gpointer       user_data)
{
	TotemSearchEntry *self = user_data;
	GList *children, *l;

	children = gtk_container_get_children (GTK_CONTAINER (list_box));
	for (l = children; l != NULL; l = l->next) {
		GtkWidget *check;

		check = g_object_get_data (G_OBJECT (l->data), "check");
		if (l->data == row) {
			const char *label;

			gtk_widget_set_opacity (check, 1.0);
			label = g_object_get_data (G_OBJECT (l->data), "label");
			gtk_label_set_text (GTK_LABEL (self->label), label);
			g_object_notify (G_OBJECT (self), "selected-id");
		} else {
			gtk_widget_set_opacity (check, 0.0);
		}
	}
	g_list_free (children);

	gtk_widget_hide (self->popover);
}

static int
sort_sources (GtkListBoxRow *row_a,
	      GtkListBoxRow *row_b,
	      gpointer       user_data)
{
	int prio_a, prio_b;
	const char *name_a, *name_b;

	prio_a = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (row_a), "priority"));
	prio_b = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (row_b), "priority"));

	if (prio_a > prio_b)
		return -1;
	if (prio_b > prio_a)
		return 1;

	name_a = g_object_get_data (G_OBJECT (row_a), "label");
	name_b = g_object_get_data (G_OBJECT (row_b), "label");

	return 0 - g_utf8_collate (name_a, name_b);
}

static void
popover_closed_cb (GtkPopover       *popover,
		   TotemSearchEntry *self)
{
	gtk_widget_grab_focus (self->entry);
}

static void
totem_search_entry_init (TotemSearchEntry *self)
{
	gtk_widget_init_template (GTK_WIDGET (self));

	gtk_list_box_set_sort_func (GTK_LIST_BOX (self->listbox), sort_sources, self, NULL);
}

static void
totem_search_entry_set_property (GObject *object,
				 guint property_id,
                                 const GValue *value,
                                 GParamSpec * pspec)
{
	switch (property_id) {
	case PROP_SELECTED_ID:
		totem_search_entry_set_selected_id (TOTEM_SEARCH_ENTRY (object),
						    g_value_get_string (value));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
	}
}

static void
totem_search_entry_get_property (GObject    *object,
				 guint       property_id,
                                 GValue     *value,
                                 GParamSpec *pspec)
{
	switch (property_id) {
	case PROP_SELECTED_ID:
		g_value_set_string (value,
				    totem_search_entry_get_selected_id (TOTEM_SEARCH_ENTRY (object)));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
	}
}

static void
totem_search_entry_class_init (TotemSearchEntryClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

	gobject_class->set_property = totem_search_entry_set_property;
	gobject_class->get_property = totem_search_entry_get_property;

	signals[SIGNAL_ACTIVATE] =
		g_signal_new ("activate",
			      TOTEM_TYPE_SEARCH_ENTRY,
			      G_SIGNAL_RUN_FIRST | G_SIGNAL_DETAILED,
			      0, NULL, NULL, NULL,
			      G_TYPE_NONE,
			      0, G_TYPE_NONE);

	g_object_class_install_property (gobject_class, PROP_SELECTED_ID,
					 g_param_spec_string ("selected-id", "Selected ID", "The ID for the currently selected source.",
							      NULL,
							      G_PARAM_READWRITE |
							      G_PARAM_STATIC_STRINGS));

	gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/totem/ui/totem-search-entry.ui");

	gtk_widget_class_bind_template_child (widget_class, TotemSearchEntry, entry);
	gtk_widget_class_bind_template_child (widget_class, TotemSearchEntry, dropdown_button);
	gtk_widget_class_bind_template_child (widget_class, TotemSearchEntry, label);
	gtk_widget_class_bind_template_child (widget_class, TotemSearchEntry, popover);
	gtk_widget_class_bind_template_child (widget_class, TotemSearchEntry, listbox);

	gtk_widget_class_bind_template_callback (widget_class, entry_activate_cb);
	gtk_widget_class_bind_template_callback (widget_class, listbox_row_activated);
	gtk_widget_class_bind_template_callback (widget_class, popover_closed_cb);
}

TotemSearchEntry *
totem_search_entry_new (void)
{
	return g_object_new (TOTEM_TYPE_SEARCH_ENTRY, NULL);
}

static GtkWidget *
padded_label_new (const char *text)
{
	GtkWidget *widget;

	widget = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 10);
	gtk_widget_set_margin_top (widget, 10);
	gtk_widget_set_margin_bottom (widget, 10);
	gtk_widget_set_margin_start (widget, 10);
	gtk_widget_set_margin_end (widget, 10);
	gtk_box_pack_start (GTK_BOX (widget), gtk_label_new (text), FALSE, FALSE, 0);

	return widget;
}

void
totem_search_entry_add_source (TotemSearchEntry *self,
			       const gchar      *id,
			       const gchar      *label,
			       int               priority)
{
	GtkWidget *item;
	GtkWidget *check;
	GtkWidget *box;

	g_return_if_fail (TOTEM_IS_SEARCH_ENTRY (self));

	gtk_widget_set_sensitive (GTK_WIDGET (self), TRUE);
	gtk_widget_show (self->label);

	item = gtk_list_box_row_new ();
	box = padded_label_new (label);
	gtk_container_add (GTK_CONTAINER (item), box);

	check = gtk_image_new ();
	gtk_image_set_from_icon_name (GTK_IMAGE (check), "object-select-symbolic", GTK_ICON_SIZE_MENU);
	gtk_widget_set_opacity (check, 0.0);
	g_object_set (check, "icon-size", GTK_ICON_SIZE_MENU, NULL);
	gtk_box_pack_start (GTK_BOX (box), check, FALSE, FALSE, 0);
	gtk_box_reorder_child (GTK_BOX (box), check, 0);

	g_object_set_data (G_OBJECT (item), "check", check);
	g_object_set_data_full (G_OBJECT (item), "id", g_strdup (id), g_free);
	g_object_set_data_full (G_OBJECT (item), "label", g_strdup (label), g_free);
	g_object_set_data (G_OBJECT (item), "priority", GINT_TO_POINTER (priority));

	gtk_widget_show_all (item);
	gtk_list_box_insert (GTK_LIST_BOX (self->listbox), item, -1);

	/* Is this the local one? */
	if (priority == 50) {
		listbox_row_activated (GTK_LIST_BOX (self->listbox),
				       GTK_LIST_BOX_ROW (item),
				       self);
	}
}

void
totem_search_entry_remove_source (TotemSearchEntry *self,
				  const gchar *id)
{
	GList *children, *l;
	guint num_items;
	gboolean current_removed = FALSE;

	g_return_if_fail (TOTEM_IS_SEARCH_ENTRY (self));

	children = gtk_container_get_children (GTK_CONTAINER (self->listbox));
	if (children == NULL)
		return;

	num_items = g_list_length (children) - 1;
	for (l = children; l != NULL; l = l->next) {
		const char *tmp_id;

		tmp_id = g_object_get_data (G_OBJECT (l->data), "id");
		if (g_strcmp0 (id, tmp_id) == 0) {
			GtkWidget *check;

			check = g_object_get_data (G_OBJECT (l->data), "check");
			if (gtk_widget_get_opacity (check) == 1.0)
				current_removed = TRUE;

			gtk_widget_destroy (l->data);
		}
	}

	if (current_removed) {
		if (!totem_search_entry_set_selected_id (self, "grl-tracker3-source"))
			totem_search_entry_set_selected_id (self, "grl-tracker-source");
	}

	if (num_items == 0) {
		gtk_widget_hide (self->label);
		gtk_widget_set_sensitive (GTK_WIDGET (self), FALSE);
	}
}

const char *
totem_search_entry_get_text (TotemSearchEntry *self)
{
	g_return_val_if_fail (TOTEM_IS_SEARCH_ENTRY (self), NULL);

	return gtk_entry_get_text (GTK_ENTRY (self->entry));
}

const char *
totem_search_entry_get_selected_id (TotemSearchEntry *self)
{
	GList *children, *l;
	const char *id = NULL;

	g_return_val_if_fail (TOTEM_IS_SEARCH_ENTRY (self), NULL);
	children = gtk_container_get_children (GTK_CONTAINER (self->listbox));
	for (l = children; l != NULL; l = l->next) {
		GtkWidget *check;

		check = g_object_get_data (G_OBJECT (l->data), "check");
		if (gtk_widget_get_opacity (check) == 1.0) {
			id = g_object_get_data (G_OBJECT (l->data), "id");
			break;
		}
	}
	g_list_free (children);

	return id;
}

gboolean
totem_search_entry_set_selected_id (TotemSearchEntry *self,
				    const char       *id)
{
	GList *children, *l;
	gboolean ret = FALSE;

	g_return_val_if_fail (TOTEM_IS_SEARCH_ENTRY (self), FALSE);
	g_return_val_if_fail (id != NULL, FALSE);

	children = gtk_container_get_children (GTK_CONTAINER (self->listbox));
	for (l = children; l != NULL; l = l->next) {
		const char *item_id;

		item_id = g_object_get_data (G_OBJECT (l->data), "id");
		if (g_strcmp0 (item_id, id) == 0) {
			listbox_row_activated (GTK_LIST_BOX (self->listbox),
					       GTK_LIST_BOX_ROW (l->data),
					       self);
			ret = TRUE;
			goto end;
		}
	}

	g_debug ("Could not find ID '%s' in TotemSearchEntry %p", id, self);

end:
	g_list_free (children);
	return ret;
}

GtkEntry *
totem_search_entry_get_entry (TotemSearchEntry *self)
{
	g_return_val_if_fail (TOTEM_IS_SEARCH_ENTRY (self), NULL);

	return GTK_ENTRY (self->entry);
}
