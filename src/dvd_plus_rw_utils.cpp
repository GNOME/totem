
#if defined(__linux__) || defined(__FreeBSD__)

#ifndef _LARGEFILE_SOURCE
#define _LARGEFILE_SOURCE 
#endif
#ifndef _LARGEFILE64_SOURCE
#define _LARGEFILE64_SOURCE
#endif
#ifndef _FILE_OFFSET_BITS
#define _FILE_OFFSET_BITS 64
#endif
#if defined(__linux)
/* ... and "engage" glibc large file support */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#endif

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>
#include <sys/types.h>
#include <signal.h>

#include "transport.hxx"

/* Returns:
 * -1: not a DVD+RW or DVD+R
 * 0: DVD+R
 * 1: DVD+RW
 * 2: both DVD+R and DVD+RW
 */
extern "C"
int get_dvd_r_rw_profile (const char *name)
{
  Scsi_Command cmd;
  int retval = -1;
  unsigned char page[20];
  unsigned char *sense=cmd.sense();
  int i;

  if (!cmd.associate((char *)name))
      return -1;

  cmd[0]=0x46;
  cmd[1]=2;
  cmd[8]=8;
  cmd[9]=0;
  if (cmd.transport(READ,page,8))
  {   if (sense[0]==0) perror("Unable to ioctl");
	  else             fprintf(stderr,"GET CONFIGURATION failed with "
			  "SK=%xh/ASC=%02xh/ASCQ=%02xh\n",
			  sense[2]&0xF,sense[12],sense[13]);
	  goto bail;
  }

  // See if it's 2 gen drive by checking if DVD+R profile is an option
  {
    int len=4+(page[0]<<24|page[1]<<16|page[2]<<8|page[3]);
    if (len>264)
    {
      /* fprintf (stderr,"insane profile list length [%d].\n",len); */
      goto bail;
    }
    unsigned char *list=new unsigned char[len];

    cmd[0]=0x46;
    cmd[1]=2;
    cmd[7]=len>>8;
    cmd[8]=len;
    cmd[9]=0;
    if (cmd.transport(READ,list,len))
    {
      /* fprintf(stderr,"GET CONFIGURATION failed with "
		      "SK=%xh/ASC=%02xh/ASCQ=%02xh\n",
		      sense[2]&0xF,sense[12],sense[13]); */
      goto bail;
    }

    for (i=12;i<list[11];i+=4)
    {
      int profile = (list[i]<<8|list[i+1]);
      /* 0x1B: DVD+R
       * 0x1A: DVD+RW
       */
      if (profile == 0x1B)
      {
        if (retval == 1)
          retval = 2;
	else
          retval = 0;
      } else if (profile == 0x1A) {
        if (retval == 0)
          retval = 2;
	else
          retval = 1;
      }
    }

    delete list;
  }

bail:

  return retval;
}

extern "C"
int get_mmc_profile (void *fd)
{ Scsi_Command cmd(fd);
  unsigned char buf[8];
  int profile=-1,once=1;
  unsigned int len;
  unsigned char formats[260];

    do {
	cmd[0] = 0x46;
	cmd[8] = sizeof(buf);
	cmd[9] = 0;
	if (cmd.transport(READ,buf,sizeof(buf)))
	    /* perror (":-( unable to GET CONFIGURATION, non-MMC unit?"), */
	    return -1;

        if ((profile = buf[6]<<8|buf[7]) || !once) break;

	// no media?
	cmd[0] = 0;		// TEST UNIT READY
	cmd[5] = 0;
	if ((cmd.transport()&0xFFF00) != 0x23A00) break;

	// try to load tray...
	cmd[0] = 0x1B;	// START/STOP UNIT
	cmd[1] = 0x1;	// "IMMED"
	cmd[4] = 0x3;	// "Load"
	cmd[5] = 0;
	if (cmd.transport ())
	    /* perror (":-( unable to LOAD TRAY"), */
	    return -1;

	wait_for_unit (cmd);
    } while (once--);

    if (profile)
    {	cmd[0] = 0x1B;	// START/STOP UNIT
	cmd[4] = 1;	// "Spin-up"
	cmd[5] = 0;
	if (cmd.transport ())
	    /* perror (":-( unable to START UNIT"), */
	    return -1;

	handle_events (cmd);
    }

    if (profile != 0x1A && profile != 0x13)
	return profile;

    cmd[0] = 0x23;	// READ FORMAT CAPACITIES
    cmd[8] = 12;
    cmd[9] = 0;
    if (cmd.transport (READ,formats,12))
	/* perror (":-( unable to READ FORMAT CAPACITIES"), */
	return -1;

    len = formats[3];
    if (len&7 || len<16)
	/* fprintf (stderr,":-( FORMAT allocaion length isn't sane"), */
	return -1;

    cmd[0] = 0x23;	// READ FORMAT CAPACITIES
    cmd[7] = (4+len)>>8;
    cmd[8] = (4+len)&0xFF;
    cmd[9] = 0;
    if (cmd.transport (READ,formats,4+len))
	/* perror (":-( unable to READ FORMAT CAPACITIES"), */
	return -1;

    if (len != formats[3])
	/* fprintf (stderr,":-( parameter length inconsistency\n"), */
	return -1;

  return profile;
}

#endif /* defined(__linux__ || __FreeBSD__) */
