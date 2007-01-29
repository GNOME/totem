
#include <glib.h>
#include "video-dev.h"

static void
list_v4l (void)
{
	GList *devs, *l;
	VideoDev *dev;

	devs = scan_for_video_devices ();

	if (devs == NULL)
	{
		g_print ("No v4l devices\n");
		return;
	}

	for (l = devs; l != NULL; l = l->next)
	{
		dev = l->data;

		g_print ("name: %s \ndevice: %s\n",
				dev->display_name, dev->device);
		if (l->next != NULL)
			g_print ("\n");
	}
}

int main (int argc, char **argv)
{
	list_v4l ();
	return 0;
}

