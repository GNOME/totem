
#include <X11/X.h>
#include <X11/Xlib.h>

gboolean bacon_resize_init (void);
void bacon_resize (int height, int width);
int bacon_resize_get_current (void);
void bacon_restore (int id);

