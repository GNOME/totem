
#include "video-utils.h"

#include <stdint.h>
#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#include <X11/Xlib.h>

/* Code taken from:
 * transcode Copyright (C) Thomas Oestreich - June 2001
 * enix      enix.berlios.de
 */

void yuy2toyv12 (guint8 *y, guint8 *u, guint8 *v, guint8 *input,
		int width, int height) {
	int i, j, w2;

	w2 = width / 2;

	for (i = 0; i < height; i += 2) {
		for (j = 0; j < w2; j++) {
			/* packed YUV 422 is: Y[i] U[i] Y[i+1] V[i] */
			*(y++) = *(input++);
			*(u++) = *(input++);
			*(y++) = *(input++);
			*(v++) = *(input++);
		}

		/* down sampling */
		for (j = 0; j < w2; j++) {
			/* skip every second line for U and V */
			*(y++) = *(input++);
			input++;
			*(y++) = *(input++);
			input++;
		}
	}
}

#define clip_8_bit(val)				\
{						\
	if (val < 0)				\
		val = 0;			\
	else					\
		if (val > 255) val = 255;	\
}

guint8 * yv12torgb (guint8 *src_y, guint8 *src_u, guint8 *src_v,
		                            int width, int height) {
	  int     i, j;

	  int     y, u, v;
	  int     r, g, b;

	  int     sub_i_uv;
	  int     sub_j_uv;

	  int     uv_width, uv_height;

	  guchar *rgb;

	  uv_width  = width / 2;
	  uv_height = height / 2;

	  rgb = (guchar *) malloc (width * height * 3);
	  if (!rgb)
		  return NULL;

	  for (i = 0; i < height; ++i) {
		  /* calculate u & v rows */
		  sub_i_uv = ((i * uv_height) / height);

		  for (j = 0; j < width; ++j) {
			  /* calculate u & v columns */
			  sub_j_uv = ((j * uv_width) / width);

	  /***************************************************
	   *  Colour conversion from
	   *    http://www.inforamp.net/~poynton/notes/colour_and_gamma/ColorFAQ.html#RTFToC30
	   *
	   *  Thanks to Billy Biggs <vektor@dumbterm.net>
	   *  for the pointer and the following conversion.
	   *
	   *   R' = [ 1.1644         0    1.5960 ]   ([ Y' ]   [  16 ])
	   *   G' = [ 1.1644   -0.3918   -0.8130 ] * ([ Cb ] - [ 128 ])
	   *   B' = [ 1.1644    2.0172         0 ]   ([ Cr ]   [ 128 ])
	   *
	   *  Where in xine the above values are represented as
	   *   Y' == image->y
	   *   Cb == image->u
	   *   Cr == image->v
	   *
	   ***************************************************/

			  y = src_y[(i * width) + j] - 16;
			  u = src_u[(sub_i_uv * uv_width) + sub_j_uv] - 128;
			  v = src_v[(sub_i_uv * uv_width) + sub_j_uv] - 128;

			  r = (1.1644 * y) + (1.5960 * v);
			  g = (1.1644 * y) - (0.3918 * u) - (0.8130 * v);
			  b = (1.1644 * y) + (2.0172 * u);

			  clip_8_bit (r);
			  clip_8_bit (g);
			  clip_8_bit (b);

			  rgb[(i * width + j) * 3 + 0] = r;
			  rgb[(i * width + j) * 3 + 1] = g;
			  rgb[(i * width + j) * 3 + 2] = b;
		  }
	  }

	  return rgb;
}

/* Stolen from GDK */
static void
gdk_wmspec_change_state (gboolean   add,
			 GdkWindow *window,
			 GdkAtom    state1,
			 GdkAtom    state2)
{
  GdkDisplay *display = gdk_screen_get_display (gdk_drawable_get_screen (GDK_DRAWABLE (window)));
  XEvent xev;
  
#define _NET_WM_STATE_REMOVE        0    /* remove/unset property */
#define _NET_WM_STATE_ADD           1    /* add/set property */
#define _NET_WM_STATE_TOGGLE        2    /* toggle property  */  
  
  xev.xclient.type = ClientMessage;
  xev.xclient.serial = 0;
  xev.xclient.send_event = True;
  xev.xclient.window = GDK_WINDOW_XID (window);
  xev.xclient.message_type = gdk_x11_get_xatom_by_name_for_display (display, "_NET_WM_STATE");
  xev.xclient.format = 32;
  xev.xclient.data.l[0] = add ? _NET_WM_STATE_ADD : _NET_WM_STATE_REMOVE;
  xev.xclient.data.l[1] = gdk_x11_atom_to_xatom_for_display (display, state1);
  xev.xclient.data.l[2] = gdk_x11_atom_to_xatom_for_display (display, state2);
  
  XSendEvent (GDK_WINDOW_XDISPLAY (window),
	      GDK_WINDOW_XWINDOW (gdk_screen_get_root_window (gdk_drawable_get_screen (GDK_DRAWABLE (window)))),
	      False, SubstructureRedirectMask | SubstructureNotifyMask,
	      &xev);
}

void
totem_gdk_window_set_always_on_top (GdkWindow *window, gboolean setting)
{
  gdk_wmspec_change_state (setting, window, gdk_atom_intern ("_NET_WM_STATE_ABOVE", FALSE), 0);
}

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

