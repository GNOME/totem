
#include "totem-time-label.h"
#include <glib/gi18n.h>
#include "video-utils.h"

static void totem_time_label_class_init (TotemTimeLabelClass *class);
static void totem_time_label_init       (TotemTimeLabel      *label);

static GObjectClass *parent_class = NULL;

G_DEFINE_TYPE (TotemTimeLabel, totem_time_label, GTK_TYPE_LABEL)

static void
totem_time_label_init (TotemTimeLabel *label)
{
	char *time;
	time = totem_time_to_string (0);
	gtk_label_set_text (GTK_LABEL (label), time);
	g_free (time);
}

GtkWidget*
totem_time_label_new (void)
{
	TotemTimeLabel *label;
  
	label = g_object_new (TOTEM_TYPE_TIME_LABEL, NULL);
  
	return GTK_WIDGET (label);
}

static void
totem_time_label_class_init (TotemTimeLabelClass *klass)
{
	GtkWidgetClass *widget_class;

	parent_class = g_type_class_peek_parent (klass);
	
	widget_class = GTK_WIDGET_CLASS (klass);
}

void
totem_time_label_set_time (TotemTimeLabel *label, gint64 time, gint64 length)
{
	char *label_str;

	if (length < 0)
	{
		label_str = totem_time_to_string (time);
	} else {
		char *time_str, *length_str;

		time_str = totem_time_to_string (time);
		length_str =  totem_time_to_string (length);
		label_str = g_strdup_printf
			(_("%s / %s"), time_str, length_str);
		g_free (time_str);
		g_free (length_str);
	}
		
	gtk_label_set_text (GTK_LABEL (label), label_str);
	g_free (label_str);
}

