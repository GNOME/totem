/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   cd-drive.c: easy to use cd burner software

   Copyright (C) 2002 Red Hat, Inc.

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public
   License along with this program; if not, write to the
   Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.

   Authors: Alexander Larsson <alexl@redhat.com>
   Bastien Nocera <hadess@hadess.net>
*/

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <math.h>

#ifdef __linux__
#include <scsi/scsi.h>
#include <scsi/sg.h>
#endif /* __linux__ */

#ifdef __FreeBSD__
#include <sys/cdio.h>
#include <sys/cdrio.h>
#include <camlib.h>
#endif /* __FreeBSD__ */

#include <glib.h>
#include <libgnome/gnome-i18n.h>

#include "cd-drive.h"

static struct {
	const char *name;
	gboolean can_write_cdr;
	gboolean can_write_cdrw;
	gboolean can_write_dvdr;
	gboolean can_write_dvdram;
} recorder_whitelist[] = {
	{ "IOMEGA - CDRW9602EXT-B", TRUE, TRUE, FALSE, FALSE },
};

#if defined(__linux__) || defined(__FreeBSD__)

/* For dvd_plus_rw_utils.cpp */
int get_dvd_r_rw_profile (const char *name);
int get_mmc_profile (void *fd);

static void
add_dvd_plus (CDDrive *cdrom)
{
	int caps;

	caps = get_dvd_r_rw_profile (cdrom->device);

	if (caps == -1) {
		return;
	}

	if (caps == 2) {
		cdrom->type |= CDDRIVE_TYPE_DVD_PLUS_RW_RECORDER;
		cdrom->type |= CDDRIVE_TYPE_DVD_PLUS_R_RECORDER;
	} else if (caps == 0) {
		cdrom->type |= CDDRIVE_TYPE_DVD_PLUS_R_RECORDER;
	} else if (caps == 1) {
		cdrom->type |= CDDRIVE_TYPE_DVD_PLUS_RW_RECORDER;
	}
}

static CDMediaType
linux_bsd_media_type (const char *device)
{
	int fd;
	int mmc_profile;

	fd = open (device, O_RDONLY);
	if (fd < 0) {
		return CD_MEDIA_TYPE_ERROR;
	}

	mmc_profile = get_mmc_profile ((void *)&fd);
	close (fd);

	switch (mmc_profile) {
	case -1:        /* Couldn't get the data about the media */
		return CD_MEDIA_TYPE_UNKNOWN;
	case 0x8:	/* Commercial CDs and Audio CD	*/
		return CD_MEDIA_TYPE_CD;
	case 0x9:	/* CD-R                         */
		return CD_MEDIA_TYPE_CDR;
	case 0xa:	/* CD-RW			*/
		return CD_MEDIA_TYPE_CDRW;
	case 0x10:	/* Commercial DVDs		*/
		return CD_MEDIA_TYPE_DVD;
	case 0x11:      /* DVD-R                        */
		return CD_MEDIA_TYPE_DVDR;
	case 0x13:      /* DVD-RW Restricted Overwrite  */
	case 0x14:      /* DVD-RW Sequential            */
		return CD_MEDIA_TYPE_DVDRW;
	case 0x1B:      /* DVD+R                        */
		return CD_MEDIA_TYPE_DVD_PLUS_R;
	case 0x1A:      /* DVD+RW                       */
		return CD_MEDIA_TYPE_DVD_PLUS_RW;
	case 0x12:      /* DVD-RAM                      */
		return CD_MEDIA_TYPE_DVD_RAM;
	default:
		return CD_MEDIA_TYPE_UNKNOWN;
	}
}

#endif /* __linux__ || __FreeBSD__ */

#ifdef __linux__

#ifdef USE_STABLE_LIBGLIB
static gboolean
g_str_has_prefix (gchar *haystack, gchar *needle)
{
	if (haystack == NULL && needle == NULL) {
		return TRUE;
	}

	if (haystack == NULL || needle == NULL) {
		return FALSE;
	}

	if (strncmp (haystack, needle, strlen (needle)) == 0) {
		return TRUE;
	}

	return FALSE;
}
#endif /* USE_STABLE_LIBGLIB */

static char **
read_lines (char *filename)
{
	char *contents;
	gsize len;
	char *p, *n;
	GPtrArray *array;
	
	if (g_file_get_contents (filename,
				 &contents,
				 &len, NULL)) {
		
		array = g_ptr_array_new ();
		
		p = contents;
		while ((n = memchr (p, '\n', len - (p - contents))) != NULL) {
			*n = 0;
			g_ptr_array_add (array, g_strdup (p));
			p = n + 1;
		}
		if ((gsize)(p - contents) < len) {
			g_ptr_array_add (array, g_strndup (p, len - (p - contents)));
		}

		g_ptr_array_add (array, NULL);
		
		g_free (contents);
		return (char **)g_ptr_array_free (array, FALSE);
	}
	return NULL;
}

struct scsi_unit {
	char *vendor;
	char *model;
	char *rev;
	int bus;
	int id;
	int lun;
	int type;
};

struct cdrom_unit {
	char *device;
	char *display_name;
	int speed;
	gboolean can_write_cdr;
	gboolean can_write_cdrw;
	gboolean can_write_dvdr;
	gboolean can_write_dvdram;
	gboolean can_read_dvd;
};

static char *cdrom_get_name (struct cdrom_unit *cdrom, struct scsi_unit *scsi_units, int n_scsi_units);

static void
add_whitelist (struct cdrom_unit *cdrom_s,
	       struct scsi_unit *scsi_units, int n_scsi_units)
{
	guint i;

	for (i = 0; i < G_N_ELEMENTS (recorder_whitelist); i++) {
		if (cdrom_s->display_name == NULL) {
			continue;
		}

		if (!strcmp (cdrom_s->display_name, recorder_whitelist[i].name)) {
			cdrom_s->can_write_cdr =
				recorder_whitelist[i].can_write_cdr;
			cdrom_s->can_write_cdrw =
				recorder_whitelist[i].can_write_cdrw;
			cdrom_s->can_write_dvdr =
				recorder_whitelist[i].can_write_dvdr;
			cdrom_s->can_write_dvdram =
				recorder_whitelist[i].can_write_dvdram;
		}
	}
}

static void
get_scsi_units (char **device_str, char **devices, struct scsi_unit *scsi_units)
{
	char vendor[9], model[17], rev[5];
	int host_no, access_count, queue_depth, device_busy, online, channel;
	int scsi_id, scsi_lun, scsi_type;
	int i, j;

	for (i = 0, j = 0; device_str[i] != NULL && devices[i] != NULL; i++) {
		if (strcmp (device_str[i], "<no active device>") == 0) {
			continue;
		}
		if (sscanf (devices[i], "%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d",
			    &host_no,
			    &channel, &scsi_id, &scsi_lun, &scsi_type, &access_count, &queue_depth,
				&device_busy, &online) != 9) {
		
			g_warning ("Couldn't match line in /proc/scsi/sg/devices\n");
			continue;
		}
		if (scsi_type == 5) { /* TYPE_ROM (include/scsi/scsi.h) */
			if (sscanf (device_str[i], "%8c\t%16c\t%4c", 
						vendor, model, rev) != 3) {
				g_warning ("Couldn't match line /proc/scsi/sg/device_strs\n");
				continue;
			}
			vendor[8] = '\0'; model[16] = '\0'; rev[4] = '\0';

			scsi_units[j].vendor = g_strdup (g_strstrip (vendor));
			scsi_units[j].model = g_strdup (g_strstrip (model));
			scsi_units[j].rev = g_strdup (g_strstrip (rev));
			scsi_units[j].bus = host_no;
			scsi_units[j].id = scsi_id;
			scsi_units[j].lun = scsi_lun; 
			scsi_units[j].type = scsi_type;
			
			j++;
		}
	}
	
}

static int
count_strings (char *p)
{
	int n_strings;

	n_strings = 0;
	while (*p != 0) {
		n_strings++;
		while (*p != '\t' && *p != 0) {
			p++;
		}
		if (*p == '\t') {
			p++;
		}
	}
	return n_strings;
}

static int
get_cd_scsi_id (const char *dev, int *bus, int *id, int *lun)
{
	int fd;
	char *devfile;
	struct {
		long mux4;
		long hostUniqueId;
	} m_idlun;
	
	devfile = g_strdup_printf ("/dev/%s", dev);
	fd = open(devfile, O_RDONLY | O_NONBLOCK);
	g_free (devfile);
	
	if (fd < 0) {
		g_warning ("Failed to open cd device %s\n", dev);
		return 0;
	}
    
	if (ioctl (fd, SCSI_IOCTL_GET_BUS_NUMBER, bus) < 0) {
		g_warning ("Failed to get scsi bus nr\n");
		close (fd);
		return 0;
	}
	if (ioctl (fd, SCSI_IOCTL_GET_IDLUN, &m_idlun) < 0) {
		g_warning ("Failed to get scsi id and lun\n");
		close(fd);
		return 0;
	}
	*id = m_idlun.mux4 & 0xFF;
	*lun = (m_idlun.mux4 >> 8)  & 0xFF;
	
	close(fd);
	return 1;
}

static struct scsi_unit *
lookup_scsi_unit (int bus, int id, int lun,
		  struct scsi_unit *scsi_units, int n_scsi_units)
{
	int i;

	for (i = 0; i < n_scsi_units; i++) {
		if (scsi_units[i].bus == bus &&
		    scsi_units[i].id == id &&
		    scsi_units[i].lun == lun) {
			return &scsi_units[i];
		}
	}
	return NULL;
}

static int
get_device_max_speed (char *id)
{
	int max_speed, i;
	const char *argv[20]; /* Shouldn't need more than 20 arguments */
	char *dev_str, *stdout_data, *speed;

	max_speed = -1;

	i = 0;
	argv[i++] = "cdrecord";
	argv[i++] = "-prcap";
	dev_str = g_strdup_printf ("dev=%s", id);
	argv[i++] = dev_str;
	argv[i++] = NULL;

	if (g_spawn_sync (NULL,
			  (char **)argv,
			  NULL,
			  G_SPAWN_SEARCH_PATH | G_SPAWN_STDERR_TO_DEV_NULL,
			  NULL, NULL,
			  &stdout_data,
			  NULL,
			  NULL,
			  NULL)) {
		speed = strstr (stdout_data, "Maximum write speed in kB/s:");
		if (speed != NULL) {
			speed += strlen ("Maximum write speed in kB/s:");
			max_speed = (int)floor (atol (speed) / 176.0 + 0.5);
		} else {
			speed = strstr (stdout_data, "Maximum write speed:");
			if (speed != NULL) {
				speed += strlen ("Maximum write speed:");
				max_speed = (int)floor (atol (speed) / 176.0 + 0.5);
			}
		}
		g_free (stdout_data);
	}

	g_free (dev_str);
	return max_speed;
}


static char *
get_scsi_cd_name (int bus, int id, int lun, const char *dev,
		  struct scsi_unit *scsi_units, int n_scsi_units)
{
	struct scsi_unit *scsi_unit;
	
	scsi_unit = lookup_scsi_unit (bus, id, lun, scsi_units, n_scsi_units);
	if (scsi_unit == NULL) {
		return g_strdup_printf (_("Unnamed SCSI CD-ROM (%s)"), dev);
	}

	return g_strdup_printf ("%s - %s",
				scsi_unit->vendor,
				scsi_unit->model);
}

static char *
cdrom_get_name (struct cdrom_unit *cdrom, struct scsi_unit *scsi_units, int n_scsi_units)
{
	char *filename, *line, *retval;
	char stdname[4], devfsname[15];
	int bus, id, lun, i;

	g_return_val_if_fail (cdrom != NULL, FALSE);

	/* clean up the string again if we have devfs */
	i = sscanf(cdrom->device, "%4s %14s", stdname, devfsname);
	if (i < 1) { /* should never happen */
		g_warning("cdrom_get_name: cdrom->device string broken!");
		return NULL;
	}
	if (i == 2) {
		cdrom->device = g_strdup(devfsname);
	}
	stdname[3] = '\0'; devfsname[14] = '\0'; /* just in case */
	
	if (stdname[0] == 's') {
		get_cd_scsi_id (cdrom->device, &bus, &id, &lun);
		retval = get_scsi_cd_name (bus, id, lun, cdrom->device, scsi_units,
			   n_scsi_units);
	} else {
		filename = g_strdup_printf ("/proc/ide/%s/model", stdname);
		if (!g_file_get_contents (filename, &line, NULL, NULL) ||
		    line == NULL) {
			g_free (filename);
			return NULL;
		}
		g_free (filename);

		i = strlen (line);
		if (line[i-1] != '\n') {
			retval = g_strdup (line);
		} else {
			retval = g_strndup (line, i - 1);
		}

		g_free (line);
	}

	return retval;
}

static GList *
add_linux_cd_recorder (GList *cdroms,
		       gboolean recorder_only,
		       struct cdrom_unit *cdrom_s,
		       struct scsi_unit *scsi_units,
		       int n_scsi_units)
{
	int bus, id, lun;
	CDDrive *cdrom;

	cdrom = g_new0 (CDDrive, 1);

	cdrom->type = CDDRIVE_TYPE_CD_DRIVE;
	cdrom->display_name = g_strdup (cdrom_s->display_name);

	if (cdrom_s->device[0] == 's') {
		if (!get_cd_scsi_id (cdrom_s->device, &bus, &id, &lun)) {
			g_free (cdrom->display_name);
			g_free (cdrom);
			return cdroms;
		}
		cdrom->cdrecord_id = g_strdup_printf ("%d,%d,%d",
				bus, id, lun);
	} else {
		/* kernel >=2.5 can write cd w/o ide-scsi */
		cdrom->cdrecord_id = g_strdup_printf ("/dev/%s",
				cdrom_s->device);
	}

	if (recorder_only) {
		cdrom->max_speed_write = get_device_max_speed
			(cdrom->cdrecord_id);
		if (cdrom->max_speed_write == -1) {
			cdrom->max_speed_write = cdrom_s->speed;
		}
	} else {
		/* Have a wild guess, the drive should actually correct us */
		cdrom->max_speed_write = cdrom_s->speed;
	}

	cdrom->device = g_strdup_printf ("/dev/%s", cdrom_s->device);
	cdrom->max_speed_read = cdrom_s->speed;
	if (cdrom_s->can_write_dvdr) {
		cdrom->type |= CDDRIVE_TYPE_DVD_RW_RECORDER;
	}

	if (cdrom_s->can_write_dvdram) {
		cdrom->type |= CDDRIVE_TYPE_DVD_RAM_RECORDER;
	}

	if (cdrom_s->can_write_cdr) {
		cdrom->type |= CDDRIVE_TYPE_CD_RECORDER;
	}
	if (cdrom_s->can_write_cdrw) {
		cdrom->type |= CDDRIVE_TYPE_CDRW_RECORDER;
	}
	if (cdrom_s->can_read_dvd) {
		cdrom->type |= CDDRIVE_TYPE_DVD_DRIVE;
	}

	add_dvd_plus (cdrom);

	return g_list_append (cdroms, cdrom);
}

static GList *
add_linux_cd_drive (GList *cdroms, struct cdrom_unit *cdrom_s,
		    struct scsi_unit *scsi_units, int n_scsi_units)
{
	CDDrive *cdrom;

	cdrom = g_new0 (CDDrive, 1);
	cdrom->type = CDDRIVE_TYPE_CD_DRIVE;
	cdrom->cdrecord_id = NULL;
	cdrom->display_name = g_strdup (cdrom_s->display_name);
	cdrom->device = g_strdup_printf ("/dev/%s", cdrom_s->device);
	cdrom->max_speed_write = 0; /* Can't write */
	cdrom->max_speed_read = cdrom_s->speed;
	if (cdrom_s->can_read_dvd) {
		cdrom->type |= CDDRIVE_TYPE_DVD_DRIVE;
	}

	return g_list_append (cdroms, cdrom);
}

static char *
get_cd_device_file (const char *str)
{
	char *devname;
	
	if (str[0] == 's') {
		devname = g_strdup_printf ("/dev/scd%c", str[2]);
		if (g_file_test (devname, G_FILE_TEST_EXISTS)) {
			g_free (devname);
			return g_strdup_printf ("scd%c", str[2]);
		}
		g_free (devname);
	}
	return 	g_strdup (str);
}

static GList *
linux_scan (gboolean recorder_only)
{
	char **device_str, **devices;
	char **cdrom_info;
	struct scsi_unit *scsi_units;
	struct cdrom_unit *cdroms;
	char *p, *t;
	int n_cdroms, maj, min, i, j;
	int n_scsi_units;
	int fd;
	FILE *file;
	GList *cdroms_list;
	gboolean have_devfs;

	/* devfs creates and populates the /dev/cdroms directory when its mounted
	 * the 'old style names' are matched with devfs names below.
	 * The cdroms.device string gets cleaned up again in cdrom_get_name()
	 * we need the oldstyle name to get device->display_name for ide.
	 */
	have_devfs = FALSE;
	if (g_file_test ("/dev/cdroms", G_FILE_TEST_IS_DIR)) {
		have_devfs = TRUE;
	}
	
	cdrom_info = read_lines ("/proc/sys/dev/cdrom/info");
	if (cdrom_info == NULL || cdrom_info[0] == NULL || cdrom_info[1] == NULL) {
		g_warning ("Couldn't read /proc/sys/dev/cdrom/info");
		return NULL;
	}
	if (!g_str_has_prefix (cdrom_info[2], "drive name:\t")) {
		return NULL;
	}
	p = cdrom_info[2] + strlen ("drive name:\t");
	while (*p == '\t') {
		p++;
	}
	n_cdroms = count_strings (p);
	cdroms = g_new0 (struct cdrom_unit, n_cdroms);

	for (j = 0; j < n_cdroms; j++) {
		t = strchr (p, '\t');
		if (t != NULL) {
			*t = 0;
		}
		cdroms[j].device = get_cd_device_file (p);
		if (t != NULL) {
			p = t + 1;
		}
	}

	/* we only have to check the first char, since only ide or scsi 	
	 * devices are listed in /proc/sys/dev/cdrom/info. It will always
	 * be 'h' or 's'
	 */
	n_scsi_units = 0;
	for (i = 0; i < n_cdroms; i++) {
		if (cdroms[i].device[0] == 's') {
			n_scsi_units++;
		}
	}

	if (n_scsi_units > 0) {
		/* open /dev/sg0 to force loading of the sg module if not loaded yet */
		fd = open ("/dev/sg0", O_RDONLY);
		if (fd != -1) {
			close (fd);
		}
		
		devices = read_lines ("/proc/scsi/sg/devices");
		device_str = read_lines ("/proc/scsi/sg/device_strs");
		if (device_str == NULL) {
			g_warning ("Can't read /proc/scsi/sg/device_strs");
			g_strfreev (devices);
			return NULL;
		}
		
		scsi_units = g_new0 (struct scsi_unit, n_scsi_units);
		get_scsi_units (device_str, devices, scsi_units);

		g_strfreev (device_str);
		g_strfreev (devices);
	} else {
		scsi_units = NULL;
	}
	
	for (i = 3; cdrom_info[i] != NULL; i++) {
		if (g_str_has_prefix (cdrom_info[i], "Can write CD-R:")) {
			p = cdrom_info[i] + strlen ("Can write CD-R:");
			while (*p == '\t') {
				p++;
			}
			for (j = 0; j < n_cdroms; j++) {
				cdroms[j].can_write_cdr = *p++ == '1';

				/* Skip tab */
				p++;
			}
		}
		if (g_str_has_prefix (cdrom_info[i], "Can write CD-RW:")) {
			p = cdrom_info[i] + strlen ("Can write CD-RW:");
			while (*p == '\t') {
				p++;
			}
			for (j = 0; j < n_cdroms; j++) {
				cdroms[j].can_write_cdrw = *p++ == '1';

				/* Skip tab */
				p++;
			}
		}
		if (g_str_has_prefix (cdrom_info[i], "Can write DVD-R:")) {
			p = cdrom_info[i] + strlen ("Can write DVD-R:");
			while (*p == '\t') {
				p++;
			}
			for (j = 0; j < n_cdroms; j++) {
				cdroms[j].can_write_dvdr = *p++ == '1';

				/* Skip tab */
				p++;
			}
		}
		if (g_str_has_prefix (cdrom_info[i], "Can write DVD-RAM:")) {
			p = cdrom_info[i] + strlen ("Can write DVD-RAM:");
			while (*p == '\t') {
				p++;
			}
			for (j = 0; j < n_cdroms; j++) {
				cdroms[j].can_write_dvdram = *p++ == '1';

				/* Skip tab */
				p++;
			}
		}
		if (g_str_has_prefix (cdrom_info[i], "Can read DVD:")) {
			p = cdrom_info[i] + strlen ("Can read DVD:");
			while (*p == '\t') {
				p++;
			}
			for (j = 0; j < n_cdroms; j++) {
				cdroms[j].can_read_dvd = *p++ == '1';

				/* Skip tab */
				p++;
			}
		}
		if (g_str_has_prefix (cdrom_info[i], "drive speed:")) {
			p = cdrom_info[i] + strlen ("drive speed:");
			while (*p == '\t') {
				p++;
			}
			for (j = 0; j < n_cdroms; j++) {
  				cdroms[j].speed = atoi (p);

				/* Skip tab */
				p++;
			}
		}
	}
	g_strfreev (cdrom_info);

	/* get kernel major.minor version */
	file = fopen("/proc/sys/kernel/osrelease", "r");
	if (file == NULL || fscanf(file, "%d.%d", &maj, &min) != 2) {
		g_warning("Could not get kernel version.");
		maj = min = 0;
	}
	fclose(file);

	cdroms_list = NULL;
	for (i = n_cdroms - 1, j = 0; i >= 0; i--, j++) {
		if (have_devfs) {
			cdroms[i].device = g_strdup_printf("%s cdroms/cdrom%d",
					cdroms[i].device,  j);
		}
		cdroms[i].display_name = cdrom_get_name (&cdroms[i],
				scsi_units, n_scsi_units);
		add_whitelist (&cdroms[i], scsi_units, n_scsi_units);

		if ((cdroms[i].can_write_cdr ||
		    cdroms[i].can_write_cdrw ||
		    cdroms[i].can_write_dvdr ||
		    cdroms[i].can_write_dvdram) &&
			(cdroms[i].device[0] == 's' ||
			(maj > 2) || (maj == 2 && min >= 5))) {
			cdroms_list = add_linux_cd_recorder (cdroms_list,
					recorder_only, &cdroms[i],
					scsi_units, n_scsi_units);
		} else if (!recorder_only) {
			cdroms_list = add_linux_cd_drive (cdroms_list,
					&cdroms[i], scsi_units, n_scsi_units);
		}
	}

	for (i = n_cdroms - 1; i >= 0; i--) {
		g_free (cdroms[i].display_name);
	}

	g_free (scsi_units);
	g_free (cdroms);

	return cdroms_list;
}
#endif /* __linux__ */

#ifdef __FreeBSD__
static GList *
freebsd_scan (gboolean recorder_only)
{
	GList *cdroms_list = NULL;
	const char *dev_type = "cd";
	int fd;
	int speed = 16; /* XXX Hardcode the write speed for now. */
	int max_speed = CDR_MAX_SPEED;
	int i = 0;

	while (1) {
		CDDriveType type;
		CDDrive *cdrom;
		gchar *cam_path, *dev;
		struct cam_device *cam_dev;

		type = 0;
		cam_path = g_strdup_printf ("/dev/%s%dc", dev_type, i);

		if (!g_file_test (cam_path, G_FILE_TEST_EXISTS)) {
			g_free (cam_path);
			break;
		}

		if ((cam_dev = cam_open_spec_device (dev_type, i, O_RDWR, NULL)) == NULL) {
			i++;
			g_free (cam_path);
			continue;
		}

		/* XXX Other controllers might need to be added. */
		if ((strncmp (cam_dev->sim_name, "ata", 3)) == 0) {
			dev = g_strdup_printf ("/dev/a%s%dc", dev_type, i);
		} else {
			dev = g_strdup (cam_path);
		}

		g_free (cam_path);

		if ((fd = open (dev, O_RDWR, 0)) < 0) {
			g_free (dev);
			free (cam_dev);
			i++;
			continue;
		}

		type |= CDDRIVE_TYPE_CD_DRIVE;
		if (ioctl (fd, CDRIOCWRITESPEED, &max_speed) >= 0) {
			/* XXX is it CD-R or CD-RW ? */
			type |= CDDRIVE_TYPE_CD_RECORDER;
		}

		close (fd);

		if (type & CDDRIVE_TYPE_CD_RECORDER
				|| type & CDDRIVE_TYPE_CDRW_RECORDER
				|| recorder_only == FALSE) {
			cdrom = g_new0 (CDDrive, 1);
			cdrom->display_name = g_strdup_printf ("%s %s", cam_dev->inq_data.vendor, cam_dev->inq_data.revision);
			cdrom->device = g_strdup (dev);
			cdrom->max_speed_read = speed;
			cdrom->max_speed_write = speed;
			cdrom->cdrecord_id = g_strdup_printf ("%d,%d,%d", cam_dev->path_id, cam_dev->target_id, cam_dev->target_lun);
			cdrom->type = type;

			add_dvd_plus (cdrom);

			cdroms_list = g_list_append (cdroms_list, cdrom);
		}

		g_free (dev);
		free (cam_dev);

		i++;
	}

	return cdroms_list;
}
#endif /* __FreeBSD__ */

GList *
scan_for_cdroms (gboolean recorder_only, gboolean add_image)
{
	CDDrive *cdrom;
	GList *cdroms = NULL;

#ifdef __linux__
	cdroms = linux_scan (recorder_only);
#endif

#ifdef __FreeBSD__
	cdroms = freebsd_scan (recorder_only);
#endif

	if (add_image) {
		/* File */
		cdrom = g_new0 (CDDrive, 1);
		cdrom->display_name = g_strdup (_("File image"));
		cdrom->max_speed_read = 0;
		cdrom->max_speed_write = 0;
		cdrom->type = CDDRIVE_TYPE_FILE;

		cdroms = g_list_append (cdroms, cdrom);
	}

	return cdroms;
}

void
cd_drive_free (CDDrive *drive)
{
	g_return_if_fail (drive != NULL);

	g_free (drive->display_name);
	g_free (drive->cdrecord_id);
	g_free (drive->device);
}

CDMediaType
guess_media_type (const char *device_path)
{
#if defined(__linux__) || defined(__FreeBSD__)
	return linux_bsd_media_type (device_path);
#else
	return CD_MEDIA_TYPE_UNKNOWN;
#endif
}

