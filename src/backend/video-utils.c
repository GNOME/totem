
#include "video-utils.h"

#include <stdint.h>
#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#include <X11/Xlib.h>

void
eel_gdk_window_set_invisible_cursor (GdkWindow *window)
{
	GdkBitmap *empty_bitmap;
	GdkCursor *cursor;
	GdkColor useless;
	char invisible_cursor_bits[] = { 0x0 }; 

	useless.red = useless.green = useless.blue = 0;
	useless.pixel = 0;

	empty_bitmap = gdk_bitmap_create_from_data (window,
			invisible_cursor_bits,
			1, 1);

	cursor = gdk_cursor_new_from_pixmap (empty_bitmap,
			empty_bitmap,
			&useless,
			&useless, 0, 0);

	gdk_window_set_cursor (window, cursor);

	gdk_cursor_unref (cursor);

	g_object_unref (empty_bitmap);
}

