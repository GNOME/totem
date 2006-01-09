
#include "config.h"

#include "video-utils.h"

#include <glib/gi18n.h>
#include <libintl.h>

#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#include <X11/Xlib.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>

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

void
totem_gdk_window_set_invisible_cursor (GdkWindow *window)
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

	/* When there's no window, there's no bitmap */
	if (empty_bitmap == NULL)
		return;

	cursor = gdk_cursor_new_from_pixmap (empty_bitmap,
			empty_bitmap,
			&useless,
			&useless, 0, 0);

	gdk_window_set_cursor (window, cursor);

	gdk_cursor_unref (cursor);

	g_object_unref (empty_bitmap);
}

void
totem_create_symlinks (const char *orig, const char *dest)
{
	GDir *dir;
	const char *name;

	dir = g_dir_open (orig, 0, NULL);
	if (dir == NULL)
		return;

	if (g_file_test (dest, G_FILE_TEST_IS_DIR) == FALSE)
		return;

	name = g_dir_read_name (dir);
	while (name != NULL)
	{
		char *orig_full, *dest_full;

		orig_full = g_build_path (G_DIR_SEPARATOR_S, orig, name, NULL);
		dest_full = g_build_path (G_DIR_SEPARATOR_S, dest, name, NULL);

		/* We don't check the return value, that's normal,
		 * we're very silent people */
		symlink (orig_full, dest_full);

		g_free (orig_full);
		g_free (dest_full);

		name = g_dir_read_name (dir);
	}

	g_dir_close (dir);
}

gboolean
totem_display_is_local (void)
{
	const char *name, *work;
	int display, screen;
	gboolean has_hostname;

	name = gdk_display_get_name (gdk_display_get_default ());
	if (name == NULL)
		return TRUE;

	work = strstr (name, ":");
	if (work == NULL)
		return TRUE;

	has_hostname = (work - name) > 0;

	/* Get to the character after the colon */
	work++;
	if (work == NULL)
		return TRUE;

	if (sscanf (work, "%d.%d", &display, &screen) != 2)
		return TRUE;

	if (has_hostname == FALSE)
		return TRUE;

	if (display < 10)
		return TRUE;

	return FALSE;
}

char *
totem_time_to_string (gint64 msecs)
{
	int sec, min, hour, time;

	time = (int) (msecs / 1000);
	sec = time % 60;
	time = time - sec;
	min = (time % (60*60)) / 60;
	time = time - (min * 60);
	hour = time / (60*60);

	if (hour > 0)
	{
		/* hour:minutes:seconds */
		return g_strdup_printf ("%d:%02d:%02d", hour, min, sec);
	} else {
		/* minutes:seconds */
		return g_strdup_printf ("%d:%02d", min, sec);
	}

	return NULL;
}

char *
totem_time_to_string_text (gint64 msecs)
{
	char *secs, *mins, *hours, *string;
	int sec, min, hour, time;

	time = (int) (msecs / 1000);
	sec = time % 60;
	time = time - sec;
	min = (time % (60*60)) / 60;
	time = time - (min * 60);
	hour = time / (60*60);

	hours = g_strdup_printf (ngettext ("%d hour", "%d hours", hour), hour);

	mins = g_strdup_printf (ngettext ("%d minute",
					  "%d minutes", min), min);

	secs = g_strdup_printf (ngettext ("%d second",
					  "%d seconds", sec), sec);

	if (hour > 0)
	{
		/* hour:minutes:seconds */
		string = g_strdup_printf (_("%s %s %s"), hours, mins, secs);
	} else if (min > 0) {
		/* minutes:seconds */
		string = g_strdup_printf (_("%s %s"), mins, secs);
	} else if (sec > 0) {
		/* seconds */
		string = g_strdup_printf (_("%s"), secs);
	} else {
		/* 0 seconds */
		string = g_strdup (_("0 seconds"));
	}

	g_free (hours);
	g_free (mins);
	g_free (secs);

	return string;
}

typedef struct _TotemPrefSize {
  gint width, height;
  gulong sig_id;
} TotemPrefSize;

static gboolean
cb_unset_size (gpointer data)
{
  GtkWidget *widget = data;

  gtk_widget_queue_resize_no_redraw (widget);

  return FALSE;
}

static void
cb_set_preferred_size (GtkWidget *widget, GtkRequisition *req,
		       gpointer data)
{
  TotemPrefSize *size = data;

  req->width = size->width;
  req->height = size->height;

  g_signal_handler_disconnect (widget, size->sig_id);
  g_free (size);
  g_idle_add (cb_unset_size, widget);
}

void
totem_widget_set_preferred_size (GtkWidget *widget, gint width,
				 gint height)
{
  TotemPrefSize *size = g_new (TotemPrefSize, 1);

  size->width = width;
  size->height = height;
  size->sig_id = g_signal_connect (widget, "size-request",
				   G_CALLBACK (cb_set_preferred_size),
				   size);

  gtk_widget_queue_resize (widget);
}

gboolean
totem_ratio_fits_screen (GdkWindow *video_window, int video_width,
			 int video_height, gfloat ratio)
{
	GdkRectangle fullscreen_rect;
	int new_w, new_h;

	if (video_width <= 0 || video_height <= 0)
		return TRUE;

	new_w = video_width * ratio;
	new_h = video_height * ratio;

	gdk_screen_get_monitor_geometry (gdk_screen_get_default (),
			gdk_screen_get_monitor_at_window
			(gdk_screen_get_default (),
			 video_window),
			&fullscreen_rect);

	if (new_w > (fullscreen_rect.width - 128) ||
			new_h > (fullscreen_rect.height - 128))
	{
		return FALSE;
	}

	return TRUE;
}
