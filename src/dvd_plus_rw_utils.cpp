// This code is heavily based on code from dvd+rw-tools 
// by Andy Polyakov <appro@fy.chalmers.se>
//
// For further details see http://fy.chalmers.se/~appro/linux/DVD+RW/
//

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

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

#ifdef LIBSTDCXX_HACK
/* Some C++ stuff needed when we not link to libstdc++ */
void *operator new (size_t sz)
{
	void *ret = malloc (sz);
	if (ret == NULL)
	{
		fputs ("libnautilus-burn memory allocation failed\n", stderr);
		exit (1);
	}
	return ret;
}

void *operator new[] (size_t sz)
{
	return ::operator new(sz);
}

void
operator delete (void *ptr)
{
	free (ptr);
}
#endif /* LIBSTDCXX_HACK */

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

  /* For valgrind */
  memset (&page, 1, sizeof (page));

  cmd[0]=0x46;
  cmd[1]=2;
  cmd[8]=8;
  cmd[9]=0;
  if (cmd.transport(READ,page,8))
  {   /* if (sense[0]==0) perror("Unable to ioctl");
	  else             fprintf(stderr,"GET CONFIGURATION failed with "
			  "SK=%xh/ASC=%02xh/ASCQ=%02xh\n",
			  sense[2]&0xF,sense[12],sense[13]); */
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

    /* For valgrind */
    memset (list, 0, len);

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

    if (len <= 12 || list[11] > len)
    {
      /* fprintf(stderr, "GET CONFIGURATION with len %d, and list[11] %d\n",
                         len, list[11]); */
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
  }

bail:

  return retval;
}

extern "C"
int get_mmc_profile (int fd)
{
  Scsi_Command cmd(fd);
  int retval = -1;
  unsigned char page[20];
  unsigned char *sense=cmd.sense();
  int i;

  /* For valgrind */
  memset (&page, 1, sizeof (page));

  cmd[0]=0x46;
  cmd[1]=2;
  cmd[8]=8;
  cmd[9]=0;
  if (cmd.transport(READ,page,8))
  {    /* if (sense[0]==0) perror("Unable to ioctl");
	  else             fprintf(stderr,"GET CONFIGURATION failed with "
			  "SK=%xh/ASC=%02xh/ASCQ=%02xh\n",
			  sense[2]&0xF,sense[12],sense[13]); */
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
  }

  return page[6]<<8|page[7];

bail:

  return retval;

}

#endif /* defined(__linux__ || __FreeBSD__) */
