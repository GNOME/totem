
#ifndef TOTEM_TIME_LABEL_H
#define TOTEM_TIME_LABEL_H

#include <gtk/gtklabel.h>

#define TOTEM_TYPE_TIME_LABEL            (totem_time_label_get_type ())
#define TOTEM_TIME_LABEL(obj)            (GTK_CHECK_CAST ((obj), TOTEM_TYPE_ELLIPSIZING_LABEL, TotemTimeLabel))
#define TOTEM_TIME_LABEL_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass), TOTEM_TYPE_ELLIPSIZING_LABEL, TotemTimeLabelClass))
#define TOTEM_IS_TIME_LABEL(obj)         (GTK_CHECK_TYPE ((obj), TOTEM_TYPE_ELLIPSIZING_LABEL))
#define TOTEM_IS_TIME_LABEL_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass), TOTEM_TYPE_ELLIPSIZING_LABEL))

typedef struct TotemTimeLabel	      TotemTimeLabel;
typedef struct TotemTimeLabelClass    TotemTimeLabelClass;

struct TotemTimeLabel {
	GtkLabel parent;
};

struct TotemTimeLabelClass {
	GtkLabelClass parent_class;
};

GtkType    totem_time_label_get_type 	(void);
GtkWidget *totem_time_label_new      	(void);
void       totem_time_label_set_text 	(TotemTimeLabel *label,
					 const char          *string);

#endif /* TOTEM_TIME_LABEL_H */
