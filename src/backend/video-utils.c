
#include "video-utils.h"

#include <stdint.h>
#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#include <X11/Xlib.h>

#define MWM_HINTS_DECORATIONS   (1L << 1)
#define PROP_MWM_HINTS_ELEMENTS 5
typedef struct {
	uint32_t flags;
	uint32_t functions;
	uint32_t decorations;
	int32_t input_mode;
	uint32_t status;
} MWMHints;

static void
wmspec_change_xwindow_state (Window window, GdkAtom state1, GdkAtom state2)
{
	XEvent xev;

#define _NET_WM_STATE_REMOVE        0   /* remove/unset property */
#define _NET_WM_STATE_ADD           1   /* add/set property */
#define _NET_WM_STATE_TOGGLE        2   /* toggle property  */

	xev.xclient.type = ClientMessage;
	xev.xclient.serial = 0;
	xev.xclient.send_event = True;
	xev.xclient.display = gdk_display;
	xev.xclient.window = window;
	xev.xclient.message_type =
		gdk_x11_get_xatom_by_name ("_NET_WM_STATE");
	xev.xclient.format = 32;
	xev.xclient.data.l[0] = _NET_WM_STATE_ADD;
	xev.xclient.data.l[1] = gdk_x11_atom_to_xatom (state1);
	xev.xclient.data.l[2] = gdk_x11_atom_to_xatom (state2);

	XSendEvent (gdk_display,
			GDK_WINDOW_XID (gdk_get_default_root_window ()),
			False,
			SubstructureRedirectMask | SubstructureNotifyMask,
			&xev);
}

void xwindow_set_fullscreen (Display *display, Window window)
{
	MWMHints mwmhints;
	Atom prop;

	/* wm, no borders please */
	prop = XInternAtom (display, "_MOTIF_WM_HINTS", False);
	mwmhints.flags = MWM_HINTS_DECORATIONS;
	mwmhints.decorations = 0;
	XChangeProperty (display, window, prop, prop,
			32, PropModeReplace,
			(unsigned char *) &mwmhints,
			PROP_MWM_HINTS_ELEMENTS);

	XSetTransientForHint (display, window, None);
	XRaiseWindow (display, window);

	wmspec_change_xwindow_state (window,
			gdk_atom_intern ("_NET_WM_STATE_FULLSCREEN", FALSE),
			GDK_NONE);
}

