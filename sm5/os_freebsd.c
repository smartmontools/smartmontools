/*
 * os_freebsd.c
 *
 * Home page of code is: http://smartmontools.sourceforge.net
 *
 * Copyright (C) 2003 Eduard Martinescu <smartmontools-support@lists.sourceforge.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * You should have received a copy of the GNU General Public License
 * (for example COPYING); if not, write to the Free
 * Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * This code was originally developed as a Senior Thesis by Michael Cornwell
 * at the Concurrent Systems Laboratory (now part of the Storage Systems
 * Research Center), Jack Baskin School of Engineering, University of
 * California, Santa Cruz. http://ssrc.soe.ucsc.edu/
 *
 */

#include <stdio.h>
#include <sys/types.h>
#include <dirent.h>
#include <err.h>
#include <camlib.h>
#include <cam/scsi/scsi_message.h>
#include <sys/ata.h>


#include "config.h"
#include "atacmds.h"
#include "scsicmds.h"
#include "utility.h"
#include "os_freebsd.h"

const char *os_XXXX_c_cvsid="$Id: os_freebsd.c,v 1.15 2003/10/12 09:10:03 ballen4705 Exp $" \
ATACMDS_H_CVSID CONFIG_H_CVSID OS_XXXX_H_CVSID SCSICMDS_H_CVSID UTILITY_H_CVSID;

// to hold onto exit code for atexit routine
extern int exitstatus;

// Private table of open devices: guaranteed zero on startup since
// part of static data.
struct freebsd_dev_channel *devicetable[FREEBSD_MAXDEV];

// forward declaration
static int parse_ata_chan_dev(const char * dev_name, struct freebsd_dev_channel *ch);

// Like open().  Return positive integer handle, used by functions below only.  mode=="ATA" or "SCSI".
int deviceopen (const char* dev, char* mode) {
  struct freebsd_dev_channel *fdchan;
  int parse_ok, i;

  // Search table for a free entry
  for (i=0; i<FREEBSD_MAXDEV; i++)
    if (!devicetable[i])
      break;
  
  // If no free entry found, return error.  We have max allowed number
  // of "file descriptors" already allocated.
  if (i==FREEBSD_MAXDEV) {
    errno=EMFILE;
    return -1;
  }

  fdchan = calloc(1,sizeof(struct freebsd_dev_channel));
  if (fdchan == NULL) {
    // errno already set by call to malloc()
    return -1;
  }

  parse_ok = parse_ata_chan_dev (dev,fdchan);
  if (parse_ok == GUESS_DEVTYPE_DONT_KNOW) {
    free(fdchan);
    errno = ENOTTY;
    return -1; // can't handle what we don't know
  }

  if (parse_ok == GUESS_DEVTYPE_ATA) {
    if ((fdchan->atacommand = open("/dev/ata",O_RDWR))<0) {
      int myerror = errno;	//preserve across free call
      free (fdchan);
      errno = myerror;
      return -1;
    }
  }

  if (parse_ok == GUESS_DEVTYPE_SCSI) {
    // this is really a NO-OP, as the parse takes care
    // of filling in correct details
  }
  
  // return pointer to "file descriptor" table entry, properly offset.
  devicetable[i]=fdchan;
  return i+FREEBSD_FDOFFSET;
}

// Returns 1 if device not available/open/found else 0.  Also shifts fd into valid range.
static int isnotopen(int *fd, struct freebsd_dev_channel** fdchan) {
  // put valid "file descriptor" into range 0...FREEBSD_MAXDEV-1
  *fd -= FREEBSD_FDOFFSET;
  
  // check for validity of "file descriptor".
  if (*fd<0 || *fd>=FREEBSD_MAXDEV || !((*fdchan)=devicetable[*fd])) {
    errno = ENODEV;
    return 1;
  }
  
  return 0;
}

// Like close().  Acts on handles returned by above function.
int deviceclose (int fd) {
  struct freebsd_dev_channel *fdchan;
  int failed = 0;

  // check for valid file descriptor
  if (isnotopen(&fd, &fdchan))
    return -1;
  

  // did we allocate a SCSI device name?
  if (fdchan->devname)
    free(fdchan->devname);
  
  // close device, if open
  if (fdchan->atacommand)
    failed=close(fdchan->atacommand);

  if (fdchan->scsicontrol)
    failed=close(fdchan->scsicontrol);
  
  // if close succeeded, then remove from device list
  // Eduard, should we also remove it from list if close() fails?  I'm
  // not sure. Here I only remove it from list if close() worked.
  if (!failed) {
    free(fdchan);
    devicetable[fd]=NULL;
  }
  
  return failed;
}

// Interface to ATA devices.  See os_linux.c
int ata_command_interface(int fd, smart_command_set command, int select, char *data) {
  struct freebsd_dev_channel* con;
  int retval, copydata=0;
  struct ata_cmd iocmd;
  unsigned char buff[512];

  // check that "file descriptor" is valid
  if (isnotopen(&fd,&con))
      return -1;

#ifndef ATAREQUEST
  // sorry, but without ATAng, we can't do anything here
  errno = ENOSYS;
  return -;
#else
  bzero(buff,512);

  bzero(&iocmd,sizeof(struct ata_cmd));
  bzero(buff,512);
  iocmd.cmd=ATAREQUEST;
  iocmd.channel=con->channel;
  iocmd.device=con->device;

  iocmd.u.request.u.ata.command=WIN_SMART;
  iocmd.u.request.timeout=600;
  switch (command){
  case READ_VALUES:
    iocmd.u.request.u.ata.feature=SMART_READ_VALUES;
    iocmd.u.request.u.ata.lba=0xc24f<<8;
    iocmd.u.request.flags=ATA_CMD_READ;
    iocmd.u.request.data=buff;
    iocmd.u.request.count=512;
    copydata=1;
    break;
  case READ_THRESHOLDS:
    iocmd.u.request.u.ata.feature=SMART_READ_THRESHOLDS;
    iocmd.u.request.u.ata.count=1;
    iocmd.u.request.u.ata.lba=1|(0xc24f<<8);
    iocmd.u.request.flags=ATA_CMD_READ;
    iocmd.u.request.data=buff;
    iocmd.u.request.count=512;
    copydata=1;
    break;
  case READ_LOG:
    iocmd.u.request.u.ata.feature=SMART_READ_LOG_SECTOR;
    iocmd.u.request.u.ata.lba=select|(0xc24f<<8);
    iocmd.u.request.u.ata.count=1;
    iocmd.u.request.flags=ATA_CMD_READ;
    iocmd.u.request.data=buff;
    iocmd.u.request.count=512;
    copydata=1;
    break;
  case IDENTIFY:
    iocmd.u.request.u.ata.command=WIN_IDENTIFY;
    iocmd.u.request.flags=ATA_CMD_READ;
    iocmd.u.request.data=buff;
    iocmd.u.request.count=512;
    copydata=1;
    break;
  case PIDENTIFY:
    iocmd.u.request.u.ata.command=WIN_PIDENTIFY;
    iocmd.u.request.flags=ATA_CMD_READ;
    iocmd.u.request.data=buff;
    iocmd.u.request.count=512;
    copydata=1;
    break;
  case ENABLE:
    iocmd.u.request.u.ata.feature=SMART_ENABLE;
    iocmd.u.request.u.ata.lba=0xc24f<<8;
    iocmd.u.request.flags=ATA_CMD_CONTROL;
    break;
  case DISABLE:
    iocmd.u.request.u.ata.feature=SMART_DISABLE;
    iocmd.u.request.u.ata.lba=0xc24f<<8;
    iocmd.u.request.flags=ATA_CMD_CONTROL;
    break;
  case AUTO_OFFLINE:
    // NOTE: According to ATAPI 4 and UP, this command is obsolete
    iocmd.u.request.u.ata.feature=SMART_AUTO_OFFLINE;
    iocmd.u.request.u.ata.lba=select|(0xc24f<<8);
    iocmd.u.request.flags=ATA_CMD_CONTROL;
    break;
  case AUTOSAVE:
    iocmd.u.request.u.ata.feature=SMART_AUTOSAVE;
    iocmd.u.request.u.ata.count=0xf1;  // to enable autosave
    iocmd.u.request.u.ata.lba=0xc24f<<8;
    iocmd.u.request.flags=ATA_CMD_CONTROL;
    break;
  case IMMEDIATE_OFFLINE:
    iocmd.u.request.u.ata.feature=SMART_IMMEDIATE_OFFLINE;
    iocmd.u.request.u.ata.lba = select|(0xc24f<<8); // put test in sector
    iocmd.u.request.flags=ATA_CMD_CONTROL;
    break;
  case STATUS_CHECK: // same command, no HDIO in FreeBSD
  case STATUS:
    // this command only says if SMART is working.  It could be
    // replaced with STATUS_CHECK below.
    iocmd.u.request.u.ata.feature=SMART_STATUS;
    iocmd.u.request.u.ata.lba=0xc24f<<8;
    iocmd.u.request.flags=ATA_CMD_CONTROL;
#ifdef ATA_CMD_READ_REG
    // this is not offical ATAng code.  Patch submitted, will remove
    // once accepted and committed.
    iocmd.u.request.flags |= ATA_CMD_READ_REG;
#endif
    break;
  default:
    pout("Unrecognized command %d in linux_ata_command_interface()\n", command);
    EXIT(1);
    break;
  }
  
  if (command==STATUS_CHECK){
    unsigned const char normal_lo=0x4f, normal_hi=0xc2;
    unsigned const char failed_lo=0xf4, failed_hi=0x2c;
    unsigned char low,high;
    
    if ((retval=ioctl(con->atacommand, IOCATA, &iocmd)))
      return -1;

#ifndef ATA_CMD_READ_REG
    pout("The SMART RETURN STATUS return value (smartmontools -H option/Directive)\n can not be retrieved with this version of ATAng, please do not rely on this value\n");
#endif

    high = (iocmd.u.request.u.ata.lba >> 16) & 0xff;
    low = (iocmd.u.request.u.ata.lba >> 8) & 0xff;
    
    // Cyl low and Cyl high unchanged means "Good SMART status"
    if (low==normal_lo && high==normal_hi)
      return 0;
    
    // These values mean "Bad SMART status"
    if (low==failed_lo && high==failed_hi)
      return 1;
    
    // We haven't gotten output that makes sense; print out some debugging info
    syserror("Error SMART Status command failed");
    pout("Please get assistance from %s\n",PROJECTHOME);
    pout("Register values returned from SMART Status command are:\n");
    pout("CMD=0x%02x\n",(int)iocmd.u.request.u.ata.command);
    pout("FR =0x%02x\n",(int)iocmd.u.request.u.ata.feature);
    pout("NS =0x%02x\n",(int)iocmd.u.request.u.ata.count);
    pout("SC =0x%02x\n",(int)((iocmd.u.request.u.ata.lba) & 0xff));
    pout("CL =0x%02x\n",(int)((iocmd.u.request.u.ata.lba>>8) & 0xff));
    pout("CH =0x%02x\n",(int)((iocmd.u.request.u.ata.lba>>16) & 0xff));
    pout("RETURN =0x%04x\n",(int)iocmd.u.request.error);
    return 0;   
  }

  if ((retval=ioctl(con->atacommand, IOCATA, &iocmd))) {
    perror("Failed command: ");
    return -1;
  }
  // 
  if (copydata)
    memcpy(data, buff, 512);
  
  return 0;
#endif
}


// Interface to SCSI devices.  See os_linux.c
int do_scsi_cmnd_io(int fd, struct scsi_cmnd_io * iop, int report)
{
  struct freebsd_dev_channel* con = NULL;
  struct cam_device* cam_dev = NULL;
  union ccb *ccb;
  
  
    if (report > 0) {
        int k;
        const unsigned char * ucp = iop->cmnd;
        const char * np;

        np = scsi_get_opcode_name(ucp[0]);
        pout(" [%s: ", np ? np : "<unknown opcode>");
        for (k = 0; k < iop->cmnd_len; ++k)
            pout("%02x ", ucp[k]);
        if ((report > 1) && 
            (DXFER_TO_DEVICE == iop->dxfer_dir) && (iop->dxferp)) {
            int trunc = (iop->dxfer_len > 256) ? 1 : 0;

            pout("]\n  Outgoing data, len=%d%s:\n", (int)iop->dxfer_len,
                 (trunc ? " [only first 256 bytes shown]" : ""));
            dStrHex(iop->dxferp, (trunc ? 256 : iop->dxfer_len) , 1);
        }
        else
            pout("]");
    }

  // check that "file descriptor" is valid
  if (isnotopen(&fd,&con))
      return -ENOTTY;


  if (!(cam_dev = cam_open_spec_device(con->devname,con->unitnum,O_RDWR,NULL))) {
    warnx("%s",cam_errbuf);
    return -1;
  }

  if (!(ccb = cam_getccb(cam_dev))) {
    warnx("error allocating ccb");
    return -ENOMEM;
  }

  // clear out structure, except for header that was filled in for us
  bzero(&(&ccb->ccb_h)[1],
	sizeof(struct ccb_scsiio) - sizeof(struct ccb_hdr));

  cam_fill_csio(&ccb->csio,
		/*retrires*/ 1,
		/*cbfcnp*/ NULL,
		/* flags */ (iop->dxfer_dir == DXFER_NONE ? CAM_DIR_NONE :(iop->dxfer_dir == DXFER_FROM_DEVICE ? CAM_DIR_IN : CAM_DIR_OUT)),
		/* tagaction */ MSG_SIMPLE_Q_TAG,
		/* dataptr */ iop->dxferp,
		/* datalen */ iop->dxfer_len,
		/* senselen */ iop->max_sense_len,
		/* cdblen */ iop->cmnd_len,
		/* timout */ iop->timeout);
  memcpy(ccb->csio.cdb_io.cdb_bytes,iop->cmnd,iop->cmnd_len);

  if (cam_send_ccb(cam_dev,ccb) < 0) {
    warn("error sending SCSI ccb");
    cam_error_print(cam_dev,ccb,CAM_ESF_ALL,CAM_EPF_ALL,stderr);
    cam_freeccb(ccb);
    return -1;
  }

  if ((ccb->ccb_h.status & CAM_STATUS_MASK) != CAM_REQ_CMP) {
    cam_error_print(cam_dev,ccb,CAM_ESF_ALL,CAM_EPF_ALL,stderr);
    cam_freeccb(ccb);
    return -1;
  }

  if (iop->sensep) {
    memcpy(&(ccb->csio.sense_data),iop->sensep,sizeof(struct scsi_sense_data));
    iop->resp_sense_len = sizeof(struct scsi_sense_data);
  }

  iop->scsi_status = ccb->csio.scsi_status;

  cam_freeccb(ccb);
  
  if (cam_dev)
    cam_close_device(cam_dev);

  if (report > 0) {
    pout("  status=0\n");
    int trunc = (iop->dxfer_len > 256) ? 1 : 0;
    
    pout("  Incoming data, len=%d%s:\n", (int)iop->dxfer_len,
	 (trunc ? " [only first 256 bytes shown]" : ""));
    dStrHex(iop->dxferp, (trunc ? 256 : iop->dxfer_len) , 1);
  }
  return 0;
}

// Interface to ATA devices behind 3ware escalade RAID controller cards.  See os_linux.c
int escalade_command_interface(int fd, int disknum, smart_command_set command, int select, char *data) {
  // not currently supported
  return -1;
}

// Guess device type (ata or scsi) based on device name (FreeBSD
// specific) SCSI device name in FreeBSD can be sd, sr, scd, st, nst,
// osst, nosst and sg.
static const char * fbsd_dev_prefix = "/dev/";
static const char * fbsd_dev_ata_disk_prefix = "ad";
static const char * fbsd_dev_scsi_disk_plus = "da";
static const char * fbsd_dev_scsi_tape1 = "sa";
static const char * fbsd_dev_scsi_tape2 = "nsa";
static const char * fbsd_dev_scsi_tape3 = "esa";

static int parse_ata_chan_dev(const char * dev_name, struct freebsd_dev_channel *chan) {
  int len;
  int dev_prefix_len = strlen(fbsd_dev_prefix);
  
  // if dev_name null, or string length zero
  if (!dev_name || !(len = strlen(dev_name)))
    return GUESS_DEVTYPE_DONT_KNOW;
  
  // Remove the leading /dev/... if it's there
  if (!strncmp(fbsd_dev_prefix, dev_name, dev_prefix_len)) {
    if (len <= dev_prefix_len) 
      // if nothing else in the string, unrecognized
      return GUESS_DEVTYPE_DONT_KNOW;
    // else advance pointer to following characters
    dev_name += dev_prefix_len;
  }
  // form /dev/ad* or ad*
  if (!strncmp(fbsd_dev_ata_disk_prefix, dev_name,
	       strlen(fbsd_dev_ata_disk_prefix))) {
    int  devnum = *(dev_name += strlen(fbsd_dev_ata_disk_prefix)) - '0';
    if (chan != NULL) {
      chan->channel=devnum/2;	// 2 drives per channel
      chan->device=devnum%2;	// so dividend = channel, remainder=device
    }
    return GUESS_DEVTYPE_ATA;
  }
  
  // form /dev/da* or da*
  if (!strncmp(fbsd_dev_scsi_disk_plus, dev_name,
	       strlen(fbsd_dev_scsi_disk_plus)))
    goto handlescsi;

  // form /dev/sa* or sa*
  if (!strncmp(fbsd_dev_scsi_tape1, dev_name,
	      strlen(fbsd_dev_scsi_tape1)))
    goto handlescsi;

  // form /dev/nsa* or nsa*
  if (!strncmp(fbsd_dev_scsi_tape2, dev_name,
	      strlen(fbsd_dev_scsi_tape2)))
    goto handlescsi;

  // form /dev/esa* or esa*
  if (!strncmp(fbsd_dev_scsi_tape3, dev_name,
	      strlen(fbsd_dev_scsi_tape3)))
    goto handlescsi;
  
  // we failed to recognize any of the forms
  return GUESS_DEVTYPE_DONT_KNOW;

 handlescsi:
  if (chan != NULL) {
    if (!(chan->devname = calloc(1,DEV_IDLEN+1)))
      return GUESS_DEVTYPE_DONT_KNOW;
    
    if (cam_get_device(dev_name,chan->devname,DEV_IDLEN,&(chan->unitnum)) == -1)
      return GUESS_DEVTYPE_DONT_KNOW;
  }
  return GUESS_DEVTYPE_SCSI;
  
}

int guess_device_type (const char* dev_name) {
  return parse_ata_chan_dev(dev_name,NULL);
}

// global variable holding byte count of allocated memory
extern long long bytes;

void *FreeNonZero(void* address, int size);

// we are going to take advantage of the fact that FreeBSD's devfs will only
// have device entries for devices that exist.  So if we get the equivilent of
// ls /dev/ad?, we have all the ATA devices on the system
int get_dev_names(char*** names, const char* prefix) {
  DIR* dir;
  struct dirent* dirent;
  int n = 0;
  char** mp;

  // first, preallocate space for upto max number of ATA devices
  if (!(mp =  (char **)calloc(MAX_NUM_DEV,sizeof(char*))))
    return -1;
  
  bytes += (sizeof(char*)*MAX_NUM_DEV);

  dir = opendir("/dev");
  if (dir == NULL) {
    int myerr = errno;
    mp= FreeNonZero(mp,(sizeof (char*) * MAX_NUM_DEV));
    errno = myerr;
    return -1;
  }
  
  // now step through names
  while ((dirent = readdir(dir)) && (n < MAX_NUM_DEV)) {
    if (dirent->d_type == DT_CHR &&
	(strstr(dirent->d_name,prefix) != NULL) &&
	(dirent->d_namlen == 3)) {
      mp[n++] = CustomStrDup(dirent->d_name,1,__LINE__);
    }
  }
  closedir(dir);
  mp = realloc(mp,n*(sizeof(char*))); // shrink to correct size
  bytes -= (MAX_NUM_DEV-n)*(sizeof(char*)); // and correct allocated bytes
  *names=mp;
  return n;
}

int make_device_names (char*** devlist, const char* name) {
  if (!strcmp(name,"SCSI"))
    return get_dev_names(devlist,"da");
  else if (!strcmp(name,"ATA"))
    return get_dev_names(devlist,"ad");
  else
    return 0;
}
