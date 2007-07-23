
#ifndef TOTEM_TIME_LABEL_H
#define TOTEM_TIME_LABEL_H

#include <gtk/gtklabel.h>

#define TOTEM_TYPE_TIME_LABEL            (totem_time_label_get_type ())
#define TOTEM_TIME_LABEL(obj)            (GTK_CHECK_CAST ((obj), TOTEM_TYPE_TIME_LABEL, TotemTimeLabel))
#define TOTEM_TIME_LABEL_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass), TOTEM_TYPE_TIME_LABEL, TotemTimeLabelClass))
#define TOTEM_IS_TIME_LABEL(obj)         (GTK_CHECK_TYPE ((obj), TOTEM_TYPE_TIME_LABEL))
#define TOTEM_IS_TIME_LABEL_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass), TOTEM_TYPE_TIME_LABEL))

typedef struct TotemTimeLabel	      TotemTimeLabel;
typedef struct TotemTimeLabelClass    TotemTimeLabelClass;
typedef struct TotemTimeLabelPrivate  TotemTimeLabelPrivate;

struct TotemTimeLabel {
	GtkLabel parent;
	TotemTimeLabelPrivate *priv;
};

struct TotemTimeLabelClass {
	GtkLabelClass parent_class;
};

GtkType    totem_time_label_get_type 	(void);
GtkWidget *totem_time_label_new      	(void);
void       totem_time_label_set_time    (TotemTimeLabel *label,
					 gint64 time, gint64 length);
void       totem_time_label_set_seeking (TotemTimeLabel *label,
           gboolean seeking);

#endif /* TOTEM_TIME_LABEL_H */
