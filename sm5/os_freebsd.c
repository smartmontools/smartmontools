/*
 * os_freebsd.c
 *
 * Home page of code is: http://smartmontools.sourceforge.net
 *
 * Copyright (C) 2003-4 Eduard Martinescu <smartmontools-support@lists.sourceforge.net>
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
#include <sys/stat.h>
#include <unistd.h>
#include <glob.h>


#include "config.h"
#include "atacmds.h"
#include "scsicmds.h"
#include "utility.h"
#include "os_freebsd.h"

static const char *filenameandversion="$Id: os_freebsd.c,v 1.33 2004/03/10 18:35:01 arvoreen Exp $";

const char *os_XXXX_c_cvsid="$Id: os_freebsd.c,v 1.33 2004/03/10 18:35:01 arvoreen Exp $" \
ATACMDS_H_CVSID CONFIG_H_CVSID OS_XXXX_H_CVSID SCSICMDS_H_CVSID UTILITY_H_CVSID;

// to hold onto exit code for atexit routine
extern int exitstatus;

// Private table of open devices: guaranteed zero on startup since
// part of static data.
struct freebsd_dev_channel *devicetable[FREEBSD_MAXDEV];

// forward declaration
static int parse_ata_chan_dev(const char * dev_name, struct freebsd_dev_channel *ch);

// print examples for smartctl
void print_smartctl_examples(){
  printf("=================================================== SMARTCTL EXAMPLES =====\n\n");
#ifdef HAVE_GETOPT_LONG
  printf(
         "  smartctl -a /dev/ad0                       (Prints all SMART information)\n\n"
         "  smartctl --smart=on --offlineauto=on --saveauto=on /dev/ad0\n"
         "                                              (Enables SMART on first disk)\n\n"
         "  smartctl -t long /dev/ad0              (Executes extended disk self-test)\n\n"
         "  smartctl --attributes --log=selftest --quietmode=errorsonly /dev/ad0\n"
         "                                      (Prints Self-Test & Attribute errors)\n"
//         "  smartctl -a -device=3ware,2 /dev/sda\n"
//         "          (Prints all SMART info for 3rd ATA disk on 3ware RAID controller)\n"
         );
#else
  printf(
         "  smartctl -a /dev/ad0                       (Prints all SMART information)\n"
         "  smartctl -s on -o on -S on /dev/ad0         (Enables SMART on first disk)\n"
         "  smartctl -t long /dev/ad0              (Executes extended disk self-test)\n"
         "  smartctl -A -l selftest -q errorsonly /dev/ad0\n"
         "                                      (Prints Self-Test & Attribute errors)\n"
//         "  smartctl -a -d 3ware,2 /dev/sda\n"
//         "          (Prints all SMART info for 3rd ATA disk on 3ware RAID controller)\n"
         );
#endif
  return;
}

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
      int myerror = errno;      //preserve across free call
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

#define NO_RETURN 0
#define BAD_SMART 1
#define NO_3WARE 2
#define BAD_KERNEL 3
#define MAX_MSG 3

// Utility function for printing warnings
void printwarning(int msgNo, const char* extra) {
  static int printed[] = {0,0,0,0};
  static const char* message[]={
    "The SMART RETURN STATUS return value (smartmontools -H option/Directive)\n can not be retrieved with this version of ATAng, please do not rely on this value\n",
    
    "Error SMART Status command failed\nPlease get assistance from \n" PACKAGE_HOMEPAGE "\nRegister values returned from SMART Status command are:\n",
    
    PACKAGE_STRING " does not currentlly support TWE devices (3ware Escalade)\n",
    
    "ATA support is not provided for this kernel version. Please ugrade to a recent 5-CURRENT kernel (post 09/01/2003 or so)\n"
  };

  if (msgNo >= 0 && msgNo <= MAX_MSG) {
    if (!printed[msgNo]) {
      printed[msgNo] = 1;
      pout("%s", message[msgNo]);
      if (extra)
        pout("%s",extra);
    }
  }
  return;
}


// Interface to ATA devices.  See os_linux.c
int ata_command_interface(int fd, smart_command_set command, int select, char *data) {
#ifndef ATAREQUEST
  // sorry, but without ATAng, we can't do anything here
  printwarning(BAD_KERNEL,NULL);
  errno = ENOSYS;
  return -1;
#else
  struct freebsd_dev_channel* con;
  int retval, copydata=0;
  struct ata_cmd iocmd;
  unsigned char buff[512];

  // check that "file descriptor" is valid
  if (isnotopen(&fd,&con))
      return -1;

  bzero(buff,512);

  bzero(&iocmd,sizeof(struct ata_cmd));
  bzero(buff,512);
  iocmd.cmd=ATAREQUEST;
  iocmd.channel=con->channel;
  iocmd.device=con->device;

  iocmd.u.request.u.ata.command=ATA_SMART_CMD;
  iocmd.u.request.timeout=600;
  switch (command){
  case READ_VALUES:
    iocmd.u.request.u.ata.feature=ATA_SMART_READ_VALUES;
    iocmd.u.request.u.ata.lba=0xc24f<<8;
    iocmd.u.request.flags=ATA_CMD_READ;
    iocmd.u.request.data=buff;
    iocmd.u.request.count=512;
    copydata=1;
    break;
  case READ_THRESHOLDS:
    iocmd.u.request.u.ata.feature=ATA_SMART_READ_THRESHOLDS;
    iocmd.u.request.u.ata.count=1;
    iocmd.u.request.u.ata.lba=1|(0xc24f<<8);
    iocmd.u.request.flags=ATA_CMD_READ;
    iocmd.u.request.data=buff;
    iocmd.u.request.count=512;
    copydata=1;
    break;
  case READ_LOG:
    iocmd.u.request.u.ata.feature=ATA_SMART_READ_LOG_SECTOR;
    iocmd.u.request.u.ata.lba=select|(0xc24f<<8);
    iocmd.u.request.u.ata.count=1;
    iocmd.u.request.flags=ATA_CMD_READ;
    iocmd.u.request.data=buff;
    iocmd.u.request.count=512;
    copydata=1;
    break;
  case IDENTIFY:
    iocmd.u.request.u.ata.command=ATA_IDENTIFY_DEVICE;
    iocmd.u.request.flags=ATA_CMD_READ;
    iocmd.u.request.data=buff;
    iocmd.u.request.count=512;
    copydata=1;
    break;
  case PIDENTIFY:
    iocmd.u.request.u.ata.command=ATA_IDENTIFY_PACKET_DEVICE;
    iocmd.u.request.flags=ATA_CMD_READ;
    iocmd.u.request.data=buff;
    iocmd.u.request.count=512;
    copydata=1;
    break;
  case ENABLE:
    iocmd.u.request.u.ata.feature=ATA_SMART_ENABLE;
    iocmd.u.request.u.ata.lba=0xc24f<<8;
    iocmd.u.request.flags=ATA_CMD_CONTROL;
    break;
  case DISABLE:
    iocmd.u.request.u.ata.feature=ATA_SMART_DISABLE;
    iocmd.u.request.u.ata.lba=0xc24f<<8;
    iocmd.u.request.flags=ATA_CMD_CONTROL;
    break;
  case AUTO_OFFLINE:
    // NOTE: According to ATAPI 4 and UP, this command is obsolete
    iocmd.u.request.u.ata.feature=ATA_SMART_AUTO_OFFLINE;
    iocmd.u.request.u.ata.lba=select|(0xc24f<<8);
    iocmd.u.request.flags=ATA_CMD_CONTROL;
    break;
  case AUTOSAVE:
    iocmd.u.request.u.ata.feature=ATA_SMART_AUTOSAVE;
    iocmd.u.request.u.ata.count=0xf1;  // to enable autosave
    iocmd.u.request.u.ata.lba=0xc24f<<8;
    iocmd.u.request.flags=ATA_CMD_CONTROL;
    break;
  case IMMEDIATE_OFFLINE:
    iocmd.u.request.u.ata.feature=ATA_SMART_IMMEDIATE_OFFLINE;
    iocmd.u.request.u.ata.lba = select|(0xc24f<<8); // put test in sector
    iocmd.u.request.flags=ATA_CMD_CONTROL;
    break;
  case STATUS_CHECK: // same command, no HDIO in FreeBSD
  case STATUS:
    // this command only says if SMART is working.  It could be
    // replaced with STATUS_CHECK below.
    iocmd.u.request.u.ata.feature=ATA_SMART_STATUS;
    iocmd.u.request.u.ata.lba=0xc24f<<8;
    iocmd.u.request.flags=ATA_CMD_CONTROL;
#ifdef ATA_CMD_READ_REG
    // this is not offical ATAng code.  Patch submitted, will remove
    // once accepted and committed.
    iocmd.u.request.flags |= ATA_CMD_READ_REG;
#endif
    break;
  default:
    pout("Unrecognized command %d in ata_command_interface()\n"
         "Please contact " PACKAGE_BUGREPORT "\n", command);
    errno=ENOSYS;
    return -1;
  }
  
  if (command==STATUS_CHECK){
    unsigned const char normal_lo=0x4f, normal_hi=0xc2;
    unsigned const char failed_lo=0xf4, failed_hi=0x2c;
    unsigned char low,high;
    
    if ((retval=ioctl(con->atacommand, IOCATA, &iocmd)))
      return -1;

#ifndef ATA_CMD_READ_REG
    printwarning(NO_RETURN,NULL);
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
    char buf[512];
    sprintf(buf,"CMD=0x%02x\nFR =0x%02x\nNS =0x%02x\nSC =0x%02x\nCL =0x%02x\nCH =0x%02x\nRETURN =0x%04x\n",
            (int)iocmd.u.request.u.ata.command,
            (int)iocmd.u.request.u.ata.feature,
            (int)iocmd.u.request.u.ata.count,
            (int)((iocmd.u.request.u.ata.lba) & 0xff),
            (int)((iocmd.u.request.u.ata.lba>>8) & 0xff),
            (int)((iocmd.u.request.u.ata.lba>>16) & 0xff),
            (int)iocmd.u.request.error);
    printwarning(BAD_SMART,buf);
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
 #if __FreeBSD_version > 500000
    cam_error_print(cam_dev,ccb,CAM_ESF_ALL,CAM_EPF_ALL,stderr);
 #endif
    cam_freeccb(ccb);
    return -1;
  }

  if ((ccb->ccb_h.status & CAM_STATUS_MASK) != CAM_REQ_CMP) {
 #if __FreeBSD_version > 500000
    cam_error_print(cam_dev,ccb,CAM_ESF_ALL,CAM_EPF_ALL,stderr);
 #endif
    cam_freeccb(ccb);
    return -1;
  }

  if (iop->sensep) {
    memcpy(iop->sensep,&(ccb->csio.sense_data),sizeof(struct scsi_sense_data));
    iop->resp_sense_len = sizeof(struct scsi_sense_data);
  }

  iop->scsi_status = ccb->csio.scsi_status;

  cam_freeccb(ccb);
  
  if (cam_dev)
    cam_close_device(cam_dev);

  if (report > 0) {
    int trunc;

    pout("  status=0\n");
    trunc = (iop->dxfer_len > 256) ? 1 : 0;
    
    pout("  Incoming data, len=%d%s:\n", (int)iop->dxfer_len,
         (trunc ? " [only first 256 bytes shown]" : ""));
    dStrHex(iop->dxferp, (trunc ? 256 : iop->dxfer_len) , 1);
  }
  return 0;
}

// Interface to ATA devices behind 3ware escalade RAID controller cards.  See os_linux.c
int escalade_command_interface(int fd, int disknum, smart_command_set command, int select, char *data) {
  printwarning(NO_3WARE,NULL);
  return -1;
}


static int get_ata_channel_unit ( const char* name, int* unit, int* dev) {
#ifndef ATAREQUEST
  *dev=0;
  *unit=0;
return 0;
#else
  // there is no direct correlation between name 'ad0, ad1, ...' and
  // channel/unit number.  So we need to iterate through the possible
  // channels and check each unit to see if we match names
  struct ata_cmd iocmd;
  int fd,maxunit;
  
  bzero(&iocmd, sizeof(struct ata_cmd));

  if ((fd = open("/dev/ata", O_RDWR)) < 0)
    return -errno;
  
  iocmd.cmd = ATAGMAXCHANNEL;
  if (ioctl(fd, IOCATA, &iocmd) < 0) {
    return -errno;
    close(fd);
  }
  maxunit = iocmd.u.maxchan;
  for (*unit = 0; *unit < maxunit; (*unit)++) {
    iocmd.channel = *unit;
    iocmd.device = -1;
    iocmd.cmd = ATAGPARM;
    if (ioctl(fd, IOCATA, &iocmd) < 0) {
      close(fd);
      return -errno;
    }
    if (iocmd.u.param.type[0] && !strcmp(name,iocmd.u.param.name[0])) {
      *dev = 0;
      break;
    }
    if (iocmd.u.param.type[1] && !strcmp(name,iocmd.u.param.name[1])) {
      *dev = 1;
      break;
    }
  }
  close(fd);
  if (*unit == maxunit)
    return -1;
  else
    return 0;
#endif
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
    if (chan != NULL) {
      if (get_ata_channel_unit(dev_name,&(chan->channel),&(chan->device))<0) {
        return GUESS_DEVTYPE_DONT_KNOW;
      }
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

// we are going to take advantage of the fact that FreeBSD's devfs will only
// have device entries for devices that exist.  So if we get the equivilent of
// ls /dev/ad?, we have all the ATA devices on the system
//
// If any errors occur, leave errno set as it was returned by the
// system call, and return <0.

// Return values:
// -1 out of memory
// -2 to -5 errors in glob

int get_dev_names(char*** names, const char* prefix) {
  int n = 0;
  char** mp;
  int retglob,lim;
  glob_t globbuf;
  int i;
  char pattern1[128],pattern2[128];

  bzero(&globbuf,sizeof(globbuf));
  // in case of non-clean exit
  *names=NULL;

  // handle 0-99 possible devices, will still be limited by MAX_NUM_DEV
  sprintf(pattern1,"/dev/%s[0-9]",prefix);
  sprintf(pattern2,"/dev/%s[0-9][0-9]",prefix);
  
  // Use glob to look for any directory entries matching the patterns
  // first call inits with first pattern match, second call appends
  // to first list. Turn on NOCHECK for second call. This results in no
  // error if no more matches found, however it does append the actual
  // pattern to the list of paths....
  if ((retglob=glob(pattern1, GLOB_ERR, NULL, &globbuf)) ||
      (retglob=glob(pattern2, GLOB_ERR|GLOB_APPEND|GLOB_NOCHECK,NULL,&globbuf))) {
     int retval = -1;
    // glob failed
    if (retglob==GLOB_NOSPACE)
      pout("glob(3) ran out of memory matching patterns (%s),(%s)\n",
           pattern1, pattern2);
    else if (retglob==GLOB_ABORTED)
      pout("glob(3) aborted matching patterns (%s),(%s)\n",
           pattern1, pattern2);
    else if (retglob==GLOB_NOMATCH) {
      pout("glob(3) found no matches for patterns (%s),(%s)\n",
           pattern1, pattern2);
      retval = 0;
    }
    else if (retglob)
      pout("Unexplained error in glob(3) of patterns (%s),(%s)\n",
           pattern1, pattern2);
    
    //  Free memory and return
    globfree(&globbuf);

    return retval;
  }

  // did we find too many paths?
  // did we find too many paths?
  lim = globbuf.gl_pathc < MAX_NUM_DEV ? globbuf.gl_pathc : MAX_NUM_DEV;
  if (lim < globbuf.gl_pathc)
    pout("glob(3) found %d > MAX=%d devices matching patterns (%s),(%s): ignoring %d paths\n", 
         globbuf.gl_pathc, MAX_NUM_DEV, pattern1,pattern2,
         globbuf.gl_pathc-MAX_NUM_DEV);
  
  // allocate space for up to lim number of ATA devices
  if (!(mp =  (char **)calloc(lim, sizeof(char*)))){
    pout("Out of memory constructing scan device list\n");
    return -1;
  }

  // now step through the list returned by glob.  No link checking needed
  // in FreeBSD
  for (i=0; i<globbuf.gl_pathc; i++){
    // becuase of the NO_CHECK on second call to glob,
    // the pattern itself will be added to path list..
    // so ignore any paths that have the ']' from pattern
    if (strchr(globbuf.gl_pathv[i],']') == NULL)
      mp[n++] = CustomStrDup(globbuf.gl_pathv[i], 1, __LINE__, filenameandversion);
  }

  globfree(&globbuf);
  mp = realloc(mp,n*(sizeof(char*))); // shrink to correct size
  bytes += (n)*(sizeof(char*)); // and set allocated byte count
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
