
#include "totem-time-label.h"
#include <glib/gi18n.h>

static void totem_time_label_class_init (TotemTimeLabelClass *class);
static void totem_time_label_init       (TotemTimeLabel      *label);

static GObjectClass *parent_class = NULL;

G_DEFINE_TYPE (TotemTimeLabel, totem_time_label, GTK_TYPE_LABEL)

static void
totem_time_label_init (TotemTimeLabel *label)
{
}

GtkWidget*
totem_time_label_new (void)
{
	TotemTimeLabel *label;
  
	label = g_object_new (TOTEM_TYPE_TIME_LABEL, NULL);
	gtk_label_set_text (GTK_LABEL (label), _("0:00"));
  
	return GTK_WIDGET (label);
}

static void
totem_time_label_class_init (TotemTimeLabelClass *klass)
{
	GtkWidgetClass *widget_class;

	parent_class = g_type_class_peek_parent (klass);
	
	widget_class = GTK_WIDGET_CLASS (klass);
}

