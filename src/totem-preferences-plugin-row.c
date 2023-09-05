/*
 * SPDX-License-Identifier: GPL-3-or-later
 *
 */

#include "totem-preferences-plugin-row.h"
#include "totem-plugins-engine.h"

struct _TotemPreferencesPluginRow {
	HdyExpanderRow      parent_instance;

	TotemPluginsEngine *plugins_engine;
	PeasPluginInfo     *plugin_info;

	GtkWidget          *plugin_switch;

	GtkWidget          *copyright;
	GtkWidget          *authors;
	GtkWidget          *version;
	GtkWidget          *website;

	gboolean            is_loaded;
};

enum {
	PROP_0,
	PROP_PLUGIN_INFO,
	N_PROPS
};

static GParamSpec *properties[N_PROPS];

G_DEFINE_FINAL_TYPE (TotemPreferencesPluginRow, totem_preferences_plugin_row, HDY_TYPE_EXPANDER_ROW)

static void
set_label_text (GtkLabel *label, const char *text)
{
	if (text != NULL)
		gtk_label_set_text (label, text);
	else
		gtk_widget_hide (GTK_WIDGET (label));

}

static void
totem_preferences_plugin_row_activate_plugin_cb (GtkWidget  *object,
						 GParamSpec *pspec,
						 gpointer    data)
{
	TotemPreferencesPluginRow *self = data;
	PeasEngine *peas_engine = totem_plugins_engine_get_engine (TOTEM_PLUGINS_ENGINE (self->plugins_engine));

	if (gtk_switch_get_active (GTK_SWITCH (object)))
		peas_engine_load_plugin (peas_engine, self->plugin_info);
	else
		peas_engine_unload_plugin (peas_engine, self->plugin_info);
}

static void
totem_preferences_plugin_display_plugin_info (TotemPreferencesPluginRow *self)
{
	g_autofree char *authors = NULL;
	const char  *plugin_copyright;
	const char* const* plugin_authors;
	const char  *plugin_version;
	const char  *plugin_website;

	plugin_copyright = peas_plugin_info_get_copyright (self->plugin_info);
	set_label_text (GTK_LABEL (self->copyright), plugin_copyright);

	plugin_authors = peas_plugin_info_get_authors (self->plugin_info);
	if (plugin_authors != NULL)
		authors = g_strjoinv (", ", (char**)plugin_authors);
	set_label_text (GTK_LABEL (self->authors), authors);

	plugin_version = peas_plugin_info_get_version (self->plugin_info);
	set_label_text (GTK_LABEL (self->version), plugin_version);

	plugin_website = peas_plugin_info_get_website (self->plugin_info);
	if (plugin_website != NULL)
		gtk_link_button_set_uri (GTK_LINK_BUTTON (self->website), plugin_website);
	else
		gtk_widget_hide (self->website);

	self->is_loaded = peas_plugin_info_is_loaded (self->plugin_info);

	gtk_switch_set_active (GTK_SWITCH (self->plugin_switch), self->is_loaded);
}

static void
totem_preferences_plugin_row_get_property (GObject    *object,
					   guint       prop_id,
					   GValue     *value,
					   GParamSpec *pspec)
{
	TotemPreferencesPluginRow *self = TOTEM_PREFERENCES_PLUGIN_ROW (object);

	switch (prop_id)
	{
	case PROP_PLUGIN_INFO:
		g_value_set_object (value, self->plugin_info);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
	}
}

static void
totem_preferences_plugin_row_set_property (GObject      *object,
					   guint         prop_id,
					   const GValue *value,
					   GParamSpec   *pspec)
{
	TotemPreferencesPluginRow *self = TOTEM_PREFERENCES_PLUGIN_ROW (object);

	switch (prop_id)
	{
	case PROP_PLUGIN_INFO:
		self->plugin_info = g_value_get_object (value);
		totem_preferences_plugin_display_plugin_info (self);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
	}
}

static void
totem_preferences_plugin_row_dispose (GObject *object)
{
	TotemPreferencesPluginRow *self = TOTEM_PREFERENCES_PLUGIN_ROW (object);

	g_clear_object (&self->plugins_engine);

	G_OBJECT_CLASS (totem_preferences_plugin_row_parent_class)->dispose (object);
}

static void
totem_preferences_plugin_row_class_init (TotemPreferencesPluginRowClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

	object_class->get_property = totem_preferences_plugin_row_get_property;
	object_class->set_property = totem_preferences_plugin_row_set_property;
	object_class->dispose = totem_preferences_plugin_row_dispose;

	properties[PROP_PLUGIN_INFO] = g_param_spec_object ("plugin-info",
							   "Plugin Info",
							   "",
							   PEAS_TYPE_PLUGIN_INFO,
							   G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

	g_object_class_install_properties (object_class, N_PROPS, properties);

	gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/totem/ui/totem-preferences-plugin-row.ui");

	gtk_widget_class_bind_template_child (widget_class, TotemPreferencesPluginRow, plugin_switch);
	gtk_widget_class_bind_template_child (widget_class, TotemPreferencesPluginRow, copyright);
	gtk_widget_class_bind_template_child (widget_class, TotemPreferencesPluginRow, authors);
	gtk_widget_class_bind_template_child (widget_class, TotemPreferencesPluginRow, version);
	gtk_widget_class_bind_template_child (widget_class, TotemPreferencesPluginRow, website);

	gtk_widget_class_bind_template_callback (widget_class, totem_preferences_plugin_row_activate_plugin_cb);
}

static void
totem_preferences_plugin_row_init (TotemPreferencesPluginRow *self)
{
	gtk_widget_init_template (GTK_WIDGET (self));

	self->plugins_engine = totem_plugins_engine_get_default (NULL);
}

GtkWidget *
totem_preferences_plugin_row_new (PeasPluginInfo *plugin_info)
{
	return g_object_new (TOTEM_TYPE_PREFERENCES_PLUGIN_ROW,
			     "plugin-info", plugin_info,
			     NULL);
}
