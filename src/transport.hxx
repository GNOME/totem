//
// This is part of dvd+rw-tools by Andy Polyakov <appro@fy.chalmers.se>
//
// Use-it-on-your-own-risk, GPL bless...
//
// For further details see http://fy.chalmers.se/~appro/linux/DVD+RW/
//

#define CREAM_ON_ERRNO(s)	do {				\
    switch ((s)[2]&0x0F)					\
    {	case 2:	if ((s)[12]==4) errno=EAGAIN;	break;		\
	case 5:	errno=EINVAL;					\
		if ((s)[13]==0)					\
		{   if ((s)[12]==0x21)		errno=ENOSPC;	\
		    else if ((s)[12]==0x20)	errno=ENODEV;	\
		}						\
		break;						\
    }								\
} while(0)
#define ERRCODE(s)	((((s)[2]&0x0F)<<16)|((s)[12]<<8)|((s)[13]))
#define	SK(errcode)	(((errcode)>>16)&0xF)
#define	ASC(errcode)	(((errcode)>>8)&0xFF)
#define ASCQ(errcode)	((errcode)&0xFF)

#if defined(__linux)

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/cdrom.h>
#include <errno.h>
#include <string.h>
#include <mntent.h>
#include <sys/wait.h>
#include <sys/utsname.h>
#include <scsi/scsi.h>
#include <scsi/sg.h>

typedef enum {	NONE=CGC_DATA_NONE,	// 3
		READ=CGC_DATA_READ,	// 2
		WRITE=CGC_DATA_WRITE	// 1
	     } Direction;
#ifdef SG_IO
static const int Dir_xlate [4] = {	// should have been defined
					// private in USE_SG_IO scope,
					// but it appears to be too
		0,			// implementation-dependent...
		SG_DXFER_TO_DEV,	// 1,CGC_DATA_WRITE
		SG_DXFER_FROM_DEV,	// 2,CGC_DATA_READ
		SG_DXFER_NONE	};	// 3,CGC_DATA_NONE
static const class USE_SG_IO {
private:
    int	yes_or_no;
public:
    USE_SG_IO()	{ struct utsname buf;
		    uname (&buf);
		    // was CDROM_SEND_PACKET declared dead in 2.5?
		    yes_or_no=(strcmp(buf.release,"2.5.43")>=0);
		}
    ~USE_SG_IO(){}
    operator int()			const	{ return yes_or_no; }
    int operator[] (Direction dir)	const	{ return Dir_xlate[dir]; }
} use_sg_io;
#endif

class Scsi_Command {
private:
    int fd,autoclose;
    char *filename;
    struct cdrom_generic_command cgc;
    union {
	struct request_sense	s;
	unsigned char		u[18];
    } _sense;
#ifdef SG_IO
    struct sg_io_hdr		sg_io;
#else
    struct { int cmd_len,timeout; }	sg_io;
#endif
public:
    Scsi_Command()	{ fd=-1, autoclose=1; filename=NULL; }
    Scsi_Command(int f)	{ fd=f,  autoclose=0; filename=NULL; }
    Scsi_Command(void*f){ fd=(int)f, autoclose=0; filename=NULL; }
    ~Scsi_Command()	{ if (fd>=0 && autoclose) close(fd),fd=-1;
			  if (filename) free(filename),filename=NULL;
			}
    int associate (char *file,struct stat *isb=NULL)
    { struct stat sb;

	if ((fd=open (file,O_RDONLY|O_NONBLOCK|O_EXCL)) < 0)	return 0;
	if (fstat(fd,&sb) < 0)				return 0;
	if (!S_ISBLK(sb.st_mode))	{ errno=ENOTBLK;return 0; }

	if (isb && (isb->st_dev!=sb.st_dev || isb->st_ino!=sb.st_ino))
	{   errno=ENXIO; return 0;   }

	filename=strdup(file);

	return 1;
    }
    unsigned char &operator[] (size_t i)
    {	if (i==0)
	{   memset(&cgc,0,sizeof(cgc)), memset(&_sense,0,sizeof(_sense));
	    cgc.quiet = 1;
	    cgc.sense = &_sense.s;
#ifdef SG_IO
	    if (use_sg_io)
	    {	memset(&sg_io,0,sizeof(sg_io));
		sg_io.interface_id= 'S';
		sg_io.mx_sb_len	= sizeof(_sense);
		sg_io.cmdp	= cgc.cmd;
		sg_io.sbp	= _sense.u;
		sg_io.flags	= SG_FLAG_LUN_INHIBIT|SG_FLAG_DIRECT_IO;
	    }
#endif
	}
	sg_io.cmd_len = i+1;
	return cgc.cmd[i];
    }
    unsigned char &operator()(size_t i)	{ return _sense.u[i]; }
    unsigned char *sense()		{ return _sense.u;    }
    void timeout(int i)			{ cgc.timeout=sg_io.timeout=i*1000; }
#ifdef SG_IO
    size_t residue()			{ return sg_io.resid; }
#else
    size_t residue()			{ return 0; }
#endif
    int transport(Direction dir=NONE,void *buf=NULL,size_t sz=0)
    { int ret = 0;

#ifdef SG_IO
#define KERNEL_BROKEN 0
	if (use_sg_io)
	{   sg_io.dxferp		= buf;
	    sg_io.dxfer_len		= sz;
	    sg_io.dxfer_direction	= use_sg_io[dir];
	    if (ioctl (fd,SG_IO,&sg_io)) return -1;

#if !KERNEL_BROKEN
	    if ((sg_io.info&SG_INFO_OK_MASK) != SG_INFO_OK)
#else
	    if (sg_io.status)
#endif
	    {	errno=EIO; ret=-1;
#if !KERNEL_BROKEN
		if (sg_io.masked_status&CHECK_CONDITION)
#endif
		{   CREAM_ON_ERRNO(sg_io.sbp);
		    ret=ERRCODE(sg_io.sbp);
		    if (ret==0) ret=-1;
		}
	    }
	    return ret;
	}
	else
#undef KERNEL_BROKEN
#endif
	{   cgc.buffer		= (unsigned char *)buf;
	    cgc.buflen		= sz;
	    cgc.data_direction	= dir;
	    if (ioctl (fd,CDROM_SEND_PACKET,&cgc))
	    {	ret = ERRCODE(_sense.u);
		if (ret==0) ret=-1;
	    }
	}
	return ret;
    }
    int umount(int f=-1)
    { struct stat    fsb,msb;
      struct mntent *mb;
      FILE          *fp;
      pid_t          pid,rpid;
      int            ret=0,rval;

	if (f==-1) f=fd;
	if (fstat (f,&fsb) < 0)				return -1;
	if ((fp=setmntent ("/proc/mounts","r"))==NULL)	return -1;

	while ((mb=getmntent (fp))!=NULL)
	{   if (stat (mb->mnt_fsname,&msb) < 0) continue; // corrupted line?
	    if (msb.st_rdev == fsb.st_rdev)
	    {	ret = -1;
		if ((pid = fork()) == (pid_t)-1)	break;
		if (pid == 0) execl ("/bin/umount","umount",mb->mnt_dir,NULL);
		while (1)
		{   rpid = waitpid (pid,&rval,0);
		    if (rpid == (pid_t)-1)
		    {	if (errno==EINTR)	continue;
			else			break;
		    }
		    else if (rpid != pid)
		    {	errno = ECHILD;
			break;
		    }
		    if (WIFEXITED(rval))
		    {	if (WEXITSTATUS(rval) == 0) ret=0;
			else			    errno=EBUSY; // most likely
			break;
		    }
		    else
		    {	errno = ENOLINK;	// some phony errno
			break;
		    }
		}
		break;
	    }
	}
	endmntent (fp);

	return ret;
    }
    int is_reload_needed ()
    {	return ioctl (fd,CDROM_MEDIA_CHANGED,CDSL_CURRENT) == 0;   }
};

#elif defined(__OpenBSD__) || defined(__NetBSD__)

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/scsiio.h>
#include <errno.h>
#include <sys/wait.h>
#include <sys/param.h>
#include <sys/mount.h>

typedef off_t off64_t;
#define stat64   stat
#define fstat64  fstat
#define open64   open
#define pread64	 pread
#define pwrite64 pwrite
#define lseek64  lseek

typedef enum {	NONE=0,
		READ=SCCMD_READ,
		WRITE=SCCMD_WRITE
	     } Direction;

class Scsi_Command {
private:
    int fd,autoclose;
    char *filename;
    scsireq_t req;
public:
    Scsi_Command()	{ fd=-1, autoclose=1; filename=NULL; }
    Scsi_Command(int f)	{ fd=f,  autoclose=0; filename=NULL; }
    Scsi_Command(void*f){ fd=(int)f, autoclose=0; filename=NULL; }
    ~Scsi_Command()	{ if (fd>=0 && autoclose) close(fd),fd=-1;
			  if (filename) free(filename),filename=NULL;
			}
    int associate (char *file,struct stat *isb=NULL)
    { struct stat sb;

	fd=open(file,O_RDWR|O_NONBLOCK);
	// this is --^^^^^^-- why we have to run set-root-uid...

	if (fd < 0)					return 0;
	if (fstat(fd,&sb) < 0)				return 0;
	if (!S_ISCHR(sb.st_mode))	{ errno=EINVAL; return 0; }

	if (isb && (isb->st_dev!=sb.st_dev || isb->st_ino!=sb.st_ino))
	{   errno=ENXIO; return 0;   }

	filename=strdup(file);

	return 1;
    }
    unsigned char &operator[] (size_t i)
    {	if (i==0)
	{   memset(&req,0,sizeof(req));
	    req.flags = SCCMD_ESCAPE;
	    req.timeout = 30000;
	    req.senselen = 18; //sizeof(req.sense);
	}
	req.cmdlen = i+1;
	return req.cmd[i];
    }
    unsigned char &operator()(size_t i)	{ return req.sense[i]; }
    unsigned char *sense()		{ return req.sense;    }
    void timeout(int i)			{ req.timeout=i*1000; }
    size_t residue()			{ return req.datalen-req.datalen_used; }
    int transport(Direction dir=NONE,void *buf=NULL,size_t sz=0)
    { int ret=0;

	req.databuf = (caddr_t)buf;
	req.datalen = sz;
	req.flags |= dir;
	if (ioctl (fd,SCIOCCOMMAND,&req) < 0)	return -1;
	if (req.retsts==SCCMD_OK)		return 0;

	errno=EIO; ret=-1;
	if (req.retsts==SCCMD_SENSE)
	{   CREAM_ON_ERRNO(req.sense);
	    ret = ERRCODE(req.sense);
	    if (ret==0) ret=-1;
	}
	return ret;
    }
    // this code is basically redundant... indeed, we normally want to
    // open device O_RDWR, but we can't do that as long as it's mounted.
    // in other words, whenever this routine is invoked, device is not
    // mounted, so that it could as well just return 0;
    int umount(int f=-1)
    { struct stat    fsb,msb;
      struct statfs *mntbuf;
      int            ret=0,mntsize,i;

	if (f==-1) f=fd;

	if (fstat (f,&fsb) < 0)				return -1;
	if ((mntsize=getmntinfo(&mntbuf,MNT_NOWAIT))==0)return -1;

	for (i=0;i<mntsize;i++)
	{ char rdev[MNAMELEN+1],*slash,*rslash;

	    if ((slash=strrchr (mntbuf[i].f_mntfromname,'/'))==NULL) continue;
	    strcpy (rdev,mntbuf[i].f_mntfromname); // rdev is 1 byte larger!
	    rslash = strrchr  (rdev,'/');
	    *(rslash+1) = 'r', strcpy (rslash+2,slash+1);
	    if (stat (rdev,&msb) < 0) continue;
	    if (msb.st_rdev == fsb.st_rdev)
	    {	ret=unmount (mntbuf[i].f_mntonname,0);
		break;
            }
	}

	return ret;
    }
    int is_reload_needed ()
    {	return 1;   }
};

#elif defined(__FreeBSD__)

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <camlib.h>
#include <cam/scsi/scsi_message.h>
#include <cam/scsi/scsi_pass.h>
#include <errno.h>
#include <sys/wait.h>
#include <sys/param.h>
#include <sys/mount.h>
#include <dirent.h>

typedef off_t off64_t;
#define stat64   stat
#define fstat64  fstat
#define open64   open
#define pread64  pread
#define pwrite64 pwrite
#define lseek64  lseek

#define ioctl_fd (((struct cam_device *)ioctl_handle)->fd)

typedef enum {	NONE=CAM_DIR_NONE,
		READ=CAM_DIR_IN,
		WRITE=CAM_DIR_OUT
	     } Direction;

class Scsi_Command {
private:
    int fd,autoclose;
    char *filename;
    struct cam_device  *cam;
    union ccb		ccb;
public:
    Scsi_Command()
    {	cam=NULL, fd=-1, autoclose=1; filename=NULL;   }
    Scsi_Command(int f)
    {	char pass[32];	// periph_name is 16 chars long

	cam=NULL, fd=-1, autoclose=1, filename=NULL;

	memset (&ccb,0,sizeof(ccb));
	ccb.ccb_h.func_code = XPT_GDEVLIST;
	if (ioctl (f,CAMGETPASSTHRU,&ccb) < 0) return;

	sprintf (pass,"/dev/%.15s%u",ccb.cgdl.periph_name,ccb.cgdl.unit_number);
	cam=cam_open_pass (pass,O_RDWR,NULL);
    }
    Scsi_Command(void *f)
    {	cam=(struct cam_device *)f, autoclose=0; fd=-1; filename=NULL;  }
    ~Scsi_Command()
    {	if (cam && autoclose)	cam_close_device(cam), cam=NULL;
	if (fd>=0)		close(fd);
	if (filename)		free(filename), filename=NULL;
    }

    int associate (char *file,struct stat *isb=NULL)
    {	struct stat sb;
	char pass[32];		// periph_name is 16 chars long

	if (isb) return 1;	// ad-hoc!!! breaks the moment
				// is_reload_needed() returns 1!!!
				// appropriate solution requires
				// some digging in kernel source.
				// <appro>

	fd=open(file,O_RDONLY|O_NONBLOCK);// can't be done to /dev/passN

	if (fd < 0)					return 0;
	if (fstat(fd,&sb) < 0)				return 0;
	if (!S_ISCHR(sb.st_mode))	{ errno=EINVAL;	return 0; }

	memset (&ccb,0,sizeof(ccb));
	ccb.ccb_h.func_code = XPT_GDEVLIST;
	if (ioctl (fd,CAMGETPASSTHRU,&ccb) < 0)		return 0;

	sprintf (pass,"/dev/%.15s%u",ccb.cgdl.periph_name,ccb.cgdl.unit_number);
	cam=cam_open_pass (pass,O_RDWR,NULL);
	if (cam == NULL)				return 0;

	// If /dev/passN is bound dynamically, then this might suffer
	// from race condition. Find some FreeBSD developer who knows
	// CAM guts inside out. Note that for now this code is never
	// engaged (see if(isb) above) and it should remain off till
	// things are cleared up. <appro>
	if (isb)
	{   if (fstat(cam->fd,&sb) < 0)			return 0;
	    if (isb->st_dev!=sb.st_dev || isb->st_ino!=sb.st_ino)
	    {	errno=ENXIO; return 0;   }
	}

	filename=strdup(file);

	return 1;
    }
    unsigned char &operator[] (size_t i)
    {	if (i==0)
	{   memset(&ccb,0,sizeof(ccb));
	    ccb.ccb_h.path_id    = cam->path_id;
	    ccb.ccb_h.target_id  = cam->target_id;
	    ccb.ccb_h.target_lun = cam->target_lun;
	    cam_fill_csio (&(ccb.csio),
			1,				// retries
			NULL,				// cbfncp
			CAM_DEV_QFRZDIS,		// flags
			MSG_SIMPLE_Q_TAG,		// tag_action
			NULL,				// data_ptr
			0,				// dxfer_len
			sizeof(ccb.csio.sense_data),	// sense_len
			0,				// cdb_len
			30*1000);			// timeout
	}
	ccb.csio.cdb_len = i+1;
	return ccb.csio.cdb_io.cdb_bytes[i];
    }
    unsigned char &operator()(size_t i)
			{ return ((unsigned char *)&ccb.csio.sense_data)[i]; }
    unsigned char *sense()
			{ return (unsigned char*)&ccb.csio.sense_data;    }
    void timeout(int i)	{ ccb.ccb_h.timeout=i*1000; }
    size_t residue()	{ return ccb.csio.resid; }
    int transport(Direction dir=NONE,void *buf=NULL,size_t sz=0)
    {	int ret=0;

	ccb.csio.ccb_h.flags |= dir;
	ccb.csio.data_ptr  = (u_int8_t *)buf;
	ccb.csio.dxfer_len = sz;

	if ((ret = cam_send_ccb(cam, &ccb)) < 0)
	    return -1;

	if ((ccb.ccb_h.status & CAM_STATUS_MASK) == CAM_REQ_CMP)
	    return 0;

	errno = EIO;
	ret = ERRCODE(((unsigned char *)&ccb.csio.sense_data));
	if (ret == 0)	ret = -1;
	else		CREAM_ON_ERRNO(((unsigned char *)&ccb.csio.sense_data));

	return ret;
    }
    int umount(int f=-1)
    { struct stat    fsb,msb;
      struct statfs *mntbuf;
      int            ret=0,mntsize,i;

	if (f==-1) f=fd;

	if (fstat (f,&fsb) < 0)				return -1;
	if ((mntsize=getmntinfo(&mntbuf,MNT_NOWAIT))==0)return -1;

	for (i=0;i<mntsize;i++)
	{   if (stat (mntbuf[i].f_mntfromname,&msb) < 0) continue;
	    if (msb.st_rdev == fsb.st_rdev)
	    {	ret=unmount (mntbuf[i].f_mntonname,0);
		break;
	    }
	}

	return ret;
    }
#define RELOAD_NEVER_NEEDED	// according to Matthew Dillon
    int is_reload_needed ()
    {  return 0;   }
};

#elif defined(__sun) || defined(sun)
//
// Commercial licensing terms are to be settled with Inserve Technology,
// Åvägen 6, 412 50 GÖTEBORG, Sweden, tel. +46-(0)31-86 87 88,
// see http://www.inserve.se/ for further details.
//
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <strings.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <volmgt.h>
extern "C" int _dev_unmount(char *); // VolMgt ON Consolidation Private API
#include <sys/scsi/impl/uscsi.h>
#include <errno.h>
#include <sys/mount.h>
#include <sys/mnttab.h>
#include <sys/wait.h>
#include <sys/cdio.h>
#include <sys/utsname.h>

typedef enum {	NONE=0,
		READ=USCSI_READ,
		WRITE=USCSI_WRITE
	     } Direction;

class Scsi_Command {
private:
    int fd,autoclose;
    char *filename;
    struct uscsi_cmd ucmd;
    unsigned char cdb[16], _sense[18];
public:
    Scsi_Command()	{ fd=-1, autoclose=1; filename=NULL; }
    Scsi_Command(int f)	{ fd=f,  autoclose=0; filename=NULL; }
    Scsi_Command(void*f){ fd=(int)f, autoclose=0; filename=NULL; }
    ~Scsi_Command()	{ if (fd>=0 && autoclose) close(fd),fd=-1;
			  if (filename) free(filename),filename=NULL;
			}
    int associate (char *file,struct stat *isb=NULL)
    { class autofree {
      private:
	char *ptr;
      public:
	autofree()			{ ptr=NULL; }
	~autofree()			{ if (ptr) free(ptr); }
	char *operator=(char *str)	{ return ptr=str; }
	operator char *()		{ return ptr; }
      } volname,device;
      struct stat sb;
      int v;

	if ((v=volmgt_running()))
	{   if ((volname=volmgt_symname (file)))
	    {	if ((device=media_findname (volname)) == NULL)
		    return 0;
	    }
	    else if ((device=media_findname (file))==NULL)
		return 0;
	}
	else device=strdup(file);

	fd=open (device,O_RDONLY|O_NONBLOCK);
	if (fd<0)					return 0;
	if (fstat(fd,&sb) < 0)				return 0;
	if (!S_ISCHR(sb.st_mode))	{ errno=ENOTTY;	return 0; }

	if (isb && (isb->st_dev!=sb.st_dev || isb->st_ino!=sb.st_ino))
	{   errno=ENXIO; return 0;   }

	filename=strdup(device);

	return 1;
    }
    unsigned char &operator[] (size_t i)
    {	if (i==0)
	{   memset (&ucmd,0,sizeof(ucmd));
	    memset (cdb,0,sizeof(cdb));
	    memset (_sense,0,sizeof(_sense));
	    ucmd.uscsi_cdb    = (caddr_t)cdb;
	    ucmd.uscsi_rqbuf  = (caddr_t)_sense;
	    ucmd.uscsi_rqlen  = sizeof(_sense);
	    ucmd.uscsi_flags  = USCSI_SILENT  | USCSI_DIAGNOSE |
				USCSI_ISOLATE | USCSI_RQENABLE;
	    ucmd.uscsi_timeout= 60;
	}
	ucmd.uscsi_cdblen = i+1;
	return cdb[i];
    }
    unsigned char &operator()(size_t i)	{ return _sense[i]; }
    unsigned char *sense()		{ return _sense;    }
    void timeout(int i)			{ ucmd.uscsi_timeout=i; }
    size_t residue()			{ return ucmd.uscsi_resid; }
    int transport(Direction dir=NONE,void *buf=NULL,size_t sz=0)
    { int ret=0;

	ucmd.uscsi_bufaddr = (caddr_t)buf;
	ucmd.uscsi_buflen  = sz;
	ucmd.uscsi_flags  |= dir;
	if (ioctl(fd,USCSICMD,&ucmd))
	{   if (errno==EIO && _sense[0]==0)	// USB seems to be broken...
	    {	size_t residue=ucmd.uscsi_resid;
		memset(cdb,0,sizeof(cdb));
		cdb[0]=0x03;			// REQUEST SENSE
		cdb[4]=sizeof(_sense);
		ucmd.uscsi_cdblen  = 6;
		ucmd.uscsi_bufaddr = (caddr_t)_sense;
		ucmd.uscsi_buflen  = sizeof(_sense);
		ucmd.uscsi_flags   = USCSI_SILENT  | USCSI_DIAGNOSE |
				     USCSI_ISOLATE | USCSI_READ;
		ucmd.uscsi_timeout = 1;
		ret = ioctl(fd,USCSICMD,&ucmd);
		ucmd.uscsi_resid = residue;
		if (ret) return -1;
	    }
	    ret  = (_sense[2]&0x0F)<<16,
	    ret |= (_sense[12])<<8,
	    ret |= (_sense[13]);
	    if (ret==0) ret=-1;
	}
	return ret;
    }
    // mimics umount(2), therefore inconsistent return values
    int umount(int f=-1)
    { struct stat   fsb,msb;
      struct mnttab mb;
      FILE         *fp;
      pid_t         pid,rpid;
      int           ret=0,i,rval;

	if (f==-1) f=fd;

	if (fstat (f,&fsb) < 0)			return -1;
	if ((fp=fopen (MNTTAB,"r")) == NULL)	return -1;

	while ((i=getmntent (fp,&mb)) != -1)
	{   if (i != 0)				continue; // ignore corrupted lines
	    if (stat (mb.mnt_special,&msb) < 0)	continue; // also corrupted?
	    if (msb.st_rdev == fsb.st_rdev)
	    {	if (_dev_unmount (mb.mnt_special))	break;
		{  struct utsname uts;
		    if (uname (&uts)>=0 && strcmp(uts.release,"5.8")>=0)
		    {	// M-m-m-m-m! Solaris 8 or later...
			ret = ::umount (mb.mnt_special);
			break;
		    }
		}
		ret = -1;
		if ((pid = fork()) == (pid_t)-1)	break;
		if (pid == 0) execl ("/usr/sbin/umount","umount",mb.mnt_mountp,NULL);
		while (1)
		{   rpid = waitpid (pid,&rval,0);
		    if (rpid == (pid_t)-1)
		    {	if (errno==EINTR)	continue;
			else			break;
		    }
		    else if (rpid != pid)
		    {	errno = ECHILD;
			break;
		    }
		    if (WIFEXITED(rval))
		    {	if (WEXITSTATUS(rval) == 0)	ret=0;
			else				errno=EBUSY; // most likely
			break;
		    }
		    else if (WIFSTOPPED(rval) || WIFCONTINUED(rval))
			continue;
		    else
		    {	errno = ENOLINK;	// some phony errno
			break;
		    }
		}
		break;
	    }
	}
	fclose (fp);

	return ret;
    }
    int is_reload_needed ()
    {	if (!volmgt_running())		return 1;
	if (ioctl (fd,CDROMEJECT)<0)	return 1;
	fprintf (stderr,"%s: push the tray back in...\n",filename);
	return 0;
    }
};

#elif defined(_WIN32)

#include <windows.h>
#include <devioctl.h>
#include <ntddscsi.h>

typedef enum {	NONE=SCSI_IOCTL_DATA_UNSPECIFIED,
		READ=SCSI_IOCTL_DATA_IN,
		WRITE=SCSI_IOCTL_DATA_OUT
	     } Direction;

typedef struct {
    SCSI_PASS_THROUGH_DIRECT	spt;
    unsigned char		sense[18];
} SPKG;

class Scsi_Command {
private:
    HANDLE fd;
    SPKG   p;
public:
    Scsi_Command()	{ fd=INVALID_HANDLE_VALUE; }
    ~Scsi_Command()	{ if (fd!=INVALID_HANDLE_VALUE) CloseHandle (fd); }
    int associate (char *file)
    { char dev[32];
	sprintf(dev,"\\\\.\\%.*s",sizeof(dev)-5,file);
	fd=CreateFile (dev,GENERIC_WRITE|GENERIC_READ,
			   FILE_SHARE_READ,NULL,OPEN_EXISTING,0,NULL);
	return fd!=INVALID_HANDLE_VALUE;
    }
    unsigned char &operator[] (size_t i)
    {	if (i==0)
	{   memset(&p,0,sizeof(p));
	    p.spt.Length = sizeof(p.spt);
	    p.spt.DataIn = SCSI_IOCTL_DATA_UNSPECIFIED;
	    p.spt.TimeOutValue = 30;
	    p.spt.SenseInfoLength = sizeof(p.sense);
	    p.spt.SenseInfoOffset = offsetof(SPKG,sense);
	}
	p.spt.CdbLength = i+1;
	return p.spt.Cdb[i];
    }
    unsigned char &operator()(size_t i)	{ return p.sense[i]; }
    unsigned char *sense()		{ return p.sense;    }
    void timeout(int i)			{ p.spt.TimeOutValue=i; }
    size_t residue()			{ return 0; } // bogus
    int transport(Direction dir=NONE,unsigned char *buf=NULL,size_t sz=0)
    { DWORD bytes;
      int   ret=0;
	p.spt.DataBuffer = buf;
	p.spt.DataTransferLength = sz;
	p.spt.DataIn = dir;
	if (DeviceIoControl (fd,IOCTL_SCSI_PASS_THROUGH_DIRECT,
				&p,sizeof(p.spt),
				&p,sizeof(p),
				&bytes,FALSE) == 0) return -1;
	if (p.sense[0]&0x70)
	{   ret  = (p.sense[2]&0x0F)<<16;
	    ret |= (p.sense[12])<<8;
	    ret |= (p.sense[13]);
	    if (ret==0) ret=-1;
	}
	return ret;
    }
    int umount ()		{   return 0;   }	// bogus
    int is_reload_needed ()	{   return 0;   }	// bogus
};

static void perror (const char *str)
{ LPVOID lpMsgBuf;

    FormatMessage( 
	FORMAT_MESSAGE_ALLOCATE_BUFFER |
	FORMAT_MESSAGE_FROM_SYSTEM | 
	FORMAT_MESSAGE_IGNORE_INSERTS,
	NULL,
	GetLastError(),
	0, // Default language
	(LPTSTR) &lpMsgBuf,
	0,
	NULL 
	);
    if (str)
	fprintf (stderr,"%s: %s",str,lpMsgBuf);
    else
	fprintf (stderr,"%s",lpMsgBuf);

    LocalFree(lpMsgBuf);
}

#else
//#error "Unsupported OS"
#endif

#include <poll.h>
#include <sys/time.h>

#define DUMP_EVENTS 0
static int handle_events (Scsi_Command &cmd)
{ unsigned char  event[8];
  unsigned short profile=0,started=0;
  int err,ret=0;
  unsigned int descr;
  static unsigned char events=0xFF;	// "All events"

    while (events)
    {	cmd[0] = 0x4A;		// GET EVENT
	cmd[1] = 1;		// "Polled"
	cmd[4] = events;
	cmd[8] = sizeof(event);
	cmd[9] = 0;
	if ((err=cmd.transport(READ,event,sizeof(event))))
	{   events=0;
	    fprintf(stderr,":-[ unable to GET EVENT: %08X ]\n",err);
	    return ret;
	}

	events = event[3];
	descr  = event[4]<<24|event[5]<<16|event[6]<<8|event[7];
#if DUMP_EVENTS
	fprintf(stderr,"< %d[%08x],%x >\n",event[2],descr,events);
#endif

	if ((event[4]&0xF)==0)	return ret;	// No Changes

	switch(event[2]&7)
	{   case 0: return ret;			// No [supported] events
	    case 1: ret |= 1<<1;		// Operational Change
		if ((descr&0xFFFF) < 3)
		    goto read_profile;
	    start_unit:
	    	if (!started)
		{   cmd[0]=0x1B;	// START STOP UNIT
		    cmd[4]=1;		// "Start"
		    cmd[5]=0;
		    if ((err=cmd.transport()))
			    fprintf (stderr,":-[ Unit won't start: %X ]\n",err);
		    started=1, profile=0;
		}
	    read_profile:
		if (!profile)
		{   cmd[0] = 0x46;	// GET CONFIGURATION
		    cmd[8] = sizeof(event);
		    cmd[9] = 0;
		    if (!cmd.transport(READ,event,sizeof(event)))
			profile=event[6]<<8|event[7];
		}
		break;
	    case 2: ret |= 1<<2;		// Power Management
		if (event[5]>1)		// State is other than Active
		    goto start_unit;
		break;
	    case 3: ret |= 1<<3; break;		// External Request
	    case 4: ret |= 1<<4;		// Media
		if (event[5]&2)		// Media in
		    goto  start_unit;
		break;
	    case 5: ret |= 1<<5; break;		// Multiple Initiators
	    case 6:				// Device Busy
		if ((event[4]&0xF)==1)	// Timeout occured
		{   poll(NULL,0,(descr&0xFFFF)*100+100);
		    cmd[0] = 0;		// TEST UNIT READY
		    cmd[5] = 0;
		    if ((err=cmd.transport()))
			fprintf(stderr,":-[ Unit not ready: %X ]\n",err);
		    ret |= 1<<6;
		}
		break;
	    case 7: ret |= 1<<7; break;		// Reserved
	}
    }

  return ret;
}
#undef DUMP_EVENTS

static int wait_for_unit (Scsi_Command &cmd,volatile int *progress=NULL)
{ unsigned char *sense=cmd.sense(),sensebuf[18];
  int msecs=1000;
  struct timeval tv;

    while (1)
    {	if (msecs > 0) poll(NULL,0,msecs);
	gettimeofday (&tv,NULL);
	msecs = tv.tv_sec*1000+tv.tv_usec/1000;
	cmd[0] = 0;	// TEST UNIT READY
	cmd[5] = 0;
	if (!cmd.transport ()) break;
	// I wish I could test just for sense.valid, but (at least)
	// hp dvd100i returns 0 in valid bit at this point:-( So I
	// check for all bits...
	if ((sense[0]&0x70)==0)
	{   perror ("- [unable to TEST UNIT READY]");
	    return 1;
	}

	while (progress)
	{   if (sense[15]&0x80)
	    {	*progress = sense[16]<<8|sense[17];
		break;
	    }
	    // MMC-3 (draft) specification says that the unit should
	    // return progress indicator in key specific bytes even
	    // in reply to TEST UNIT READY. I.e. as above! But (at
	    // least) hp dvd100i doesn't do that and I have to fetch
	    // it separately:-(
	    cmd[0] = 0x03;	// REQUEST SENSE
	    cmd[4] = sizeof(sensebuf);
	    cmd[5] = 0;
	    if (cmd.transport (READ,sensebuf,sizeof(sensebuf)))
	    {   perror ("- [unable to REQUEST SENSE]");
		return 1;
	    }
	    if (sensebuf[15]&0x80)
		*progress = sensebuf[16]<<8|sensebuf[17];
	    break;
	}
	gettimeofday (&tv,NULL);
	msecs = 1000 - (tv.tv_sec*1000+tv.tv_usec/1000 - msecs);
    }

  return 0;
}

#define FEATURE21_BROKEN 1
static void page05_setup (Scsi_Command &cmd, unsigned short profile=0,
	unsigned char p32=0xC0)
	// 5 least significant bits of p32 go to p[2], Test Write&Write Type
	// 2 most significant bits go to p[3], Multi-session field
	// 0xC0 means "Multi-session, no Test Write, Incremental"
{ unsigned int   len;
  unsigned char  header[8],track[32],*p;
#if !FEATURE21_BROKEN
  unsigned char  feature21[24];
#endif
  int            errcode;
  class autofree {
    private:
	unsigned char *ptr;
    public:
	autofree()			{ ptr=NULL; }
	~autofree()			{ if (ptr) free(ptr); }
	unsigned char *operator=(unsigned char *str)
					{ return ptr=str; }
	operator unsigned char *()	{ return ptr; }
  } page05;

    if (profile==0)
    {	cmd[0] = 0x46;	// GET CONFIGURATION
	cmd[8] = sizeof(header);
	cmd[9] = 0;
	if (cmd.transport(READ,header,sizeof(header)))
	    perror (":-( unable to GET CONFIGURATION"), exit(1);

	profile = header[6]<<8|header[7];
    }

#if !FEATURE21_BROKEN
    if (profile==0x11 || profile==0x14)
    {	cmd[0] = 0x46;	// GET CONFIGURATION
	cmd[1] = 2;	// ask for the only feature...
	cmd[3] = 0x21;	// the "Incremental Streaming Writable" one
	cmd[8] = 8;	// read the header only to start with
	cmd[9] = 0;
	if (cmd.transport(READ,feature21,8))
	    perror (":-( unable to GET CONFIGURATION"), exit(1);

	len = feature21[0]<<24|feature21[1]<<16|feature21[2]<<8|feature21[3];
	len += 4;
	if (len>sizeof(feature21))
	    len = sizeof(feature21);
	else if (len<(8+8))
	    fprintf (stderr,":-( READ FEATURE DESCRIPTOR 0021h: insane length\n"),
	    exit(1);

	cmd[0] = 0x46;	// GET CONFIGURATION
	cmd[1] = 2;	// ask for the only feature...
	cmd[3] = 0x21;	// the "Incremental Streaming Writable" one
	cmd[8] = len;	// this time with real length
	cmd[9] = 0;
	if (cmd.transport(READ,feature21,len))
	    perror (":-( unable to READ FEATURE DESCRIPTOR 0021h"), exit(1);

	if ((feature21[8+2]&1)==0)
	    fprintf (stderr,":-( FEATURE 0021h is not in effect\n"),
	    exit(1);
    }
    else
	feature21[8+2]=0;
#endif

    cmd[0] = 0x52;	// READ TRACK INFORMATION
    cmd[1] = 1;		// TRACK INFORMATION
    cmd[5] = 1;		// track#1, in DVD context it's safe to assume
    			//          that all tracks are in the same mode
    cmd[8] = sizeof(track);
    cmd[9] = 0;
    if (cmd.transport(READ,track,sizeof(track)))
	perror (":-( unable to READ TRACK INFORMATION"), exit(1);

    // WRITE PAGE SETUP //
    cmd[0] = 0x5A;		// MODE SENSE
    cmd[2] = 0x05;		// "Write Page"
    cmd[8] = sizeof(header);	// header only to start with
    cmd[9] = 0;
    if (cmd.transport(READ,header,sizeof(header)))
	perror (":-( unable to MODE SENSE"), exit(1);

    len = (header[0]<<8|header[1])+2;
    page05 = (unsigned char *)malloc(len);
    if (page05 == NULL)
	fprintf (stderr,":-( memory exhausted\n"), exit(1);

    cmd[0] = 0x5A;		// MODE SENSE
    cmd[2] = 0x05;		// "Write Page"
    cmd[7] = len>>8;
    cmd[8] = len;		// real length this time
    cmd[9] = 0;
    if ((errcode=cmd.transport(READ,page05,len)))
	fprintf (stderr,":-[ MODE SENSE failed with "
			"SK=%Xh/ASC=%02Xh/ASCQ=%02Xh\n",
			SK(errcode),ASC(errcode),ASCQ(errcode)),
	exit(1);

    p = page05 + 8 + (page05[6]<<8|page05[7]);
    memset (p-8,0,8);
    p[0] &= 0x7F;

    // copy "Test Write" and "Write Type" from p32
    p[2] &= ~0x1F, p[2] |= p32&0x1F;	
    p[2] |= 0x40;	// insist on BUFE on

    // setup Preferred Link Size
#if !FEATURE21_BROKEN
    if (feature21[8+2]&1)
    {	if (feature21[8+7])	p[2] |= 0x20,  p[5] = feature21[8+8];
	else			p[2] &= ~0x20, p[5] = 0;
    }
#else	// At least Pioneer DVR-104 returns some bogus data in
	// Preferred Link Size...
    if (profile==0x11 || profile==0x14)	// Sequential recordings...
	p[2] |= 0x20, p[5] = 0x10;
#endif
    else
	p[2] &= ~0x20, p[5] = 0;

    // copy Track Mode from TRACK INFORMATION
    p[3] &= ~0x0F, p[3] |= track[5]&0x0F;

    // copy "Multi-session" bits from p32
    p[3] &= ~0xC0, p[3] |= p32&0xC0;
    if (profile == 0x13)	// DVD-RW Restricted Overwrite
    	p[3] &= 0x3F;		// always Single-session?

    // setup Data Block Type
    if ((track[6]&0x0F)==1)	p[4] = 8;
    else	fprintf (stderr,":-( none Mode 1 track\n"), exit(1);

    // setup Packet Size
    if (track[6]&0x10)
	p[3] |= 0x20,	memcpy (p+10,track+20,4);	// Fixed
    else
	p[3] &= ~0x20,	memset (p+10,0,4);		// Variable
    switch (profile)
    {	case 0x13:	// DVD-RW Restricted Overwrite
	    if (!(track[6]&0x10))
		fprintf (stderr,":-( track is not formatted for fixed packet size\n"),
		exit(1);
	    break;
	case 0x14:	// DVD-RW Sequential Recording
	case 0x11:	// DVD-R  Sequential Recording
	    if (track[6]&0x10)
		fprintf (stderr,":-( track is formatted for fixed packet size\n"),
		exit(1);
	    break;
	default:
#if 0
	    fprintf (stderr,":-( invalid profile %04xh\n",profile);
	    exit(1);
#endif
	    break;
    }

    len = 8+2+p[1];

    cmd[0] = 0x55;	// MODE SELECT
    cmd[1] = 0x10;	// conformant
    cmd[7] = len>>8;
    cmd[8] = len;
    cmd[9] = 0;
    if ((errcode=cmd.transport(WRITE,p-8,len)))
	fprintf (stderr,":-[ MODE SELECT failed with "
			"SK=%Xh/ASC=%02Xh/ASCQ=%02Xh ]\n",
			SK(errcode),ASC(errcode),ASCQ(errcode)),
	exit(1);
    // END OF WRITE PAGE SETUP //
}
#undef FEATURE21_BROKEN
