
#include <glib.h>
#include "cd-drive.h"

static void
list_cdroms (void)
{
	GList *cdroms, *l;
	CDDrive *cd;

	cdroms = scan_for_cdroms (FALSE, FALSE);

	for (l = cdroms; l != NULL; l = l->next)
	{
		cd = l->data;
		g_print ("name: %s device: %s type: %d\n",
				cd->name, cd->device, cd->type);
	}
}

int main (int argc, char **argv)
{
	list_cdroms ();
}

