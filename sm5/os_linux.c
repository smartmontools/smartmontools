/* 
 *  os_linux.c
 * 
 * Home page of code is: http://smartmontools.sourceforge.net
 *
 * Copyright (C) 2003-4 Bruce Allen <smartmontools-support@lists.sourceforge.net>
 * Copyright (C) 2003-4 Doug Gilbert <dougg@torque.net>
 *
 *  Parts of this file are derived from code that was
 *
 *  Written By: Adam Radford <linux@3ware.com>
 *  Modifications By: Joel Jacobson <linux@3ware.com>
 *                   Arnaldo Carvalho de Melo <acme@conectiva.com.br>
 *                    Brad Strand <linux@3ware.com>
 *
 *  Copyright (C) 1999-2003 3ware Inc.
 *
 *  Kernel compatablity By:     Andre Hedrick <andre@suse.com>
 *  Non-Copyright (C) 2000      Andre Hedrick <andre@suse.com>
 *
 * Other ars of this file are derived from code that was
 * 
 * Copyright (C) 1999-2000 Michael Cornwell <cornwell@acm.org>
 * Copyright (C) 2000 Andre Hedrick <andre@linux-ide.org>
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

// This file contains the linux-specific IOCTL parts of
// smartmontools. It includes one interface routine for ATA devices,
// one for SCSI devices, and one for ATA devices behind escalade
// controllers.

#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <scsi/scsi_ioctl.h>
#include <dirent.h>
#include <sys/stat.h>
#include <glob.h>

#include "config.h"

#ifdef HAVE_LINUX_HDREG_H
#include <linux/hdreg.h>
#else
#define HDIO_DRIVE_TASK   0x031e
#define HDIO_DRIVE_CMD    0x031f
#define HDIO_GET_IDENTITY 0x030d
#endif

#include "atacmds.h"
#include "os_linux.h"
#include "scsicmds.h"
#include "smartd.h"
#include "utility.h"

static const char *filenameandversion="$Id: os_linux.c,v 1.44 2004/02/06 03:52:02 ballen4705 Exp $";

const char *os_XXXX_c_cvsid="$Id: os_linux.c,v 1.44 2004/02/06 03:52:02 ballen4705 Exp $" \
ATACMDS_H_CVSID CONFIG_H_CVSID OS_XXXX_H_CVSID SCSICMDS_H_CVSID SMARTD_H_CVSID UTILITY_H_CVSID;

// to hold onto exit code for atexit routine
extern int exitstatus;

// global variable holding byte count of allocated memory
extern long long bytes;

// equivalent to open(path, flags)
int deviceopen(const char *pathname, char *type){
  if (!strcmp(type,"SCSI")) {
    int fd = open(pathname, O_RDWR | O_NONBLOCK);
    if (fd < 0 && errno == EROFS)
      fd = open(pathname, O_RDONLY | O_NONBLOCK);
    return fd;
  }
  else if (!strcmp(type,"ATA")) 
    return open(pathname, O_RDONLY | O_NONBLOCK);
  else
    return -1;
}

// equivalent to close(file descriptor)
int deviceclose(int fd){
  return close(fd);
}

// print examples for smartctl
void print_smartctl_examples(){
  printf("=================================================== SMARTCTL EXAMPLES =====\n\n");
#ifdef HAVE_GETOPT_LONG
  printf(
         "  smartctl -a /dev/hda                       (Prints all SMART information)\n\n"
         "  smartctl --smart=on --offlineauto=on --saveauto=on /dev/hda\n"
         "                                              (Enables SMART on first disk)\n\n"
         "  smartctl -t long /dev/hda              (Executes extended disk self-test)\n\n"
         "  smartctl --attributes --log=selftest --quietmode=errorsonly /dev/hda\n"
         "                                      (Prints Self-Test & Attribute errors)\n"
         "  smartctl -a -device=3ware,2 /dev/sda\n"
         "          (Prints all SMART info for 3rd ATA disk on 3ware RAID controller)\n"
         );
#else
  printf(
         "  smartctl -a /dev/hda                       (Prints all SMART information)\n"
         "  smartctl -s on -o on -S on /dev/hda         (Enables SMART on first disk)\n"
         "  smartctl -t long /dev/hda              (Executes extended disk self-test)\n"
         "  smartctl -A -l selftest -q errorsonly /dev/hda\n"
         "                                      (Prints Self-Test & Attribute errors)\n"
         "  smartctl -a -d 3ware,2 /dev/sda\n"
         "          (Prints all SMART info for 3rd ATA disk on 3ware RAID controller)\n"
         );
#endif
  return;
}


// we are going to take advantage of the fact that Linux's devfs will only
// have device entries for devices that exist.  So if we get the equivalent of
// ls /dev/hd[a-t], we have all the ATA devices on the system
//
// If any errors occur, leave errno set as it was returned by the
// system call, and return <0.
int get_dev_names(char*** names, const char* pattern, const char* name, int max) {
  int n = 0, retglob, i, lim;
  char** mp;
  glob_t globbuf;
  
  memset(&globbuf, 0, sizeof(globbuf));

  // in case of non-clean exit
  *names=NULL;
  
  // Use glob to look for any directory entries matching the pattern
  if ((retglob=glob(pattern, GLOB_ERR, NULL, &globbuf))) {
    
    //  glob failed: free memory and return
    globfree(&globbuf);
    
    if (retglob==GLOB_NOMATCH){
      pout("glob(3) found no matches for pattern %s\n", pattern);
      return 0;
    }
    
    if (retglob==GLOB_NOSPACE)
      pout("glob(3) ran out of memory matching pattern %s\n", pattern);
    else if (retglob==GLOB_ABORTED)
      pout("glob(3) aborted matching pattern %s\n", pattern);
    else
      pout("Unexplained error in glob(3) of pattern %s\n", pattern);
    
    return -1;
  }

  // did we find too many paths?
  lim = ((int)globbuf.gl_pathc < max) ? (int)globbuf.gl_pathc : max;
  if (lim < (int)globbuf.gl_pathc)
    pout("glob(3) found %d > MAX=%d devices matching pattern %s: ignoring %d paths\n", 
         (int)globbuf.gl_pathc, max, pattern, (int)(globbuf.gl_pathc-max));
  
  // allocate space for up to lim number of ATA devices
  if (!(mp =  (char **)calloc(lim, sizeof(char*)))){
    pout("Out of memory constructing scan device list\n");
    return -1;
  }
  
  // now step through the list returned by glob.  If not a link, copy
  // to list.  If it is a link, evaluate it and see if the path ends
  // in "disc".
  for (i=0; i<lim; i++){
    int retlink;
    
    // prepare a buffer for storing the link
    char linkbuf[1024];
    
    // see if path is a link
    retlink=readlink(globbuf.gl_pathv[i], linkbuf, 1023);
    
    // if not a link (or a strange link), keep it
    if (retlink<=0 || retlink>1023)
      mp[n++] = CustomStrDup(globbuf.gl_pathv[i], 1, __LINE__, filenameandversion);
    else {
      // or if it's a link that points to a disc, follow it
      char *p;
      linkbuf[retlink]='\0';
      if ((p=strrchr(linkbuf,'/')) && !strcmp(p+1, "disc"))
        // This is the branch of the code that gets followed if we are
        // using devfs WITH traditional compatibility links. In this
        // case, we add the traditional device name to the list that
        // is returned.
        mp[n++] = CustomStrDup(globbuf.gl_pathv[i], 1, __LINE__, filenameandversion);
      else {
        // This is the branch of the code that gets followed if we are
        // using devfs WITHOUT traditional compatibility links.  In
        // this case, we check that the link to the directory is of
        // the correct type, and then append "disc" to it.
        char tmpname[1024]={0};
        char *type=strcmp(name,"ATA")?"scsi":"ide";
        if (strstr(linkbuf, type)){
          snprintf(tmpname, 1024, "%s/disc", globbuf.gl_pathv[i]);
          mp[n++] = CustomStrDup(tmpname, 1, __LINE__, filenameandversion);
        }
      }
    }
  }
  
  // free memory, track memory usage
  globfree(&globbuf);
  mp = realloc(mp,n*(sizeof(char*)));
  bytes += n*(sizeof(char*));
  
  // and set up return values
  *names=mp;
  return n;
}

// makes a list of device names to scan, for either ATA or SCSI
// devices.  Return -1 if no memory remaining, else the number of
// devices on the list, which can be >=0.
int make_device_names (char*** devlist, const char* name) {
  int retval, maxdev;
  
#if 0
  // for testing case where no device names are found
  return 0;
#endif
  
  if (!strcmp(name,"SCSI"))
    retval=get_dev_names(devlist,"/dev/sd[a-z]", name, maxdev=26);
  else if (!strcmp(name,"ATA"))
    retval=get_dev_names(devlist,"/dev/hd[a-t]", name, maxdev=20);
  else
    // don't recognize disk type!
    return 0;

  // if we found traditional links, we are done
  if (retval>0)
    return retval;
  
  // else look for devfs entries without traditional links
  return get_dev_names(devlist,"/dev/discs/disc*", name, maxdev);
}


// PURPOSE
//   This is an interface routine meant to isolate the OS dependent
//   parts of the code, and to provide a debugging interface.  Each
//   different port and OS needs to provide it's own interface.  This
//   is the linux one.
// DETAILED DESCRIPTION OF ARGUMENTS
//   device: is the file descriptor provided by open()
//   command: defines the different operations.
//   select: additional input data if needed (which log, which type of
//           self-test).
//   data:   location to write output data, if needed (512 bytes).
//   Note: not all commands use all arguments.
// RETURN VALUES
//  -1 if the command failed
//   0 if the command succeeded,
//   STATUS_CHECK routine: 
//  -1 if the command failed
//   0 if the command succeeded and disk SMART status is "OK"
//   1 if the command succeeded and disk SMART status is "FAILING"


// huge value of buffer size needed because HDIO_DRIVE_CMD assumes
// that buff[3] is the data size.  Since the ATA_SMART_AUTOSAVE and
// ATA_SMART_AUTO_OFFLINE use values of 0xf1 and 0xf8 we need the space.
// Otherwise a 4+512 byte buffer would be enough.
#define STRANGE_BUFFER_LENGTH (4+512*0xf8)

int ata_command_interface(int device, smart_command_set command, int select, char *data){
  unsigned char buff[STRANGE_BUFFER_LENGTH];
  int retval, copydata=0;

  const int HDIO_DRIVE_CMD_OFFSET = 4;

  // See struct hd_drive_cmd_hdr in hdreg.h.  Before calling ioctl()
  // buff[0]: ATA COMMAND CODE REGISTER
  // buff[1]: ATA SECTOR NUMBER REGISTER == LBA LOW REGISTER
  // buff[2]: ATA FEATURES REGISTER
  // buff[3]: ATA SECTOR COUNT REGISTER

  // Note that on return:
  // buff[2] contains the ATA SECTOR COUNT REGISTER
  
  // clear out buff.  Large enough for HDIO_DRIVE_CMD (4+512 bytes)
  memset(buff, 0, STRANGE_BUFFER_LENGTH);

  buff[0]=ATA_SMART_CMD;
  switch (command){
  case CHECK_POWER_MODE:
    buff[0]=ATA_CHECK_POWER_MODE;
    copydata=1;
    break;
  case READ_VALUES:
    buff[2]=ATA_SMART_READ_VALUES;
    buff[3]=1;
    copydata=512;
    break;
  case READ_THRESHOLDS:
    buff[2]=ATA_SMART_READ_THRESHOLDS;
    buff[1]=buff[3]=1;
    copydata=512;
    break;
  case READ_LOG:
    buff[2]=ATA_SMART_READ_LOG_SECTOR;
    buff[1]=select;
    buff[3]=1;
    copydata=512;
    break;
  case IDENTIFY:
    buff[0]=ATA_IDENTIFY_DEVICE;
    buff[3]=1;
    copydata=512;
    break;
  case PIDENTIFY:
    buff[0]=ATA_IDENTIFY_PACKET_DEVICE;
    buff[3]=1;
    copydata=512;
    break;
  case ENABLE:
    buff[2]=ATA_SMART_ENABLE;
    buff[1]=1;
    break;
  case DISABLE:
    buff[2]=ATA_SMART_DISABLE;
    buff[1]=1;
    break;
  case STATUS:
    // this command only says if SMART is working.  It could be
    // replaced with STATUS_CHECK below.
    buff[2]=ATA_SMART_STATUS;
    break;
  case AUTO_OFFLINE:
    buff[2]=ATA_SMART_AUTO_OFFLINE;
    buff[3]=select;   // YET NOTE - THIS IS A NON-DATA COMMAND!!
    break;
  case AUTOSAVE:
    buff[2]=ATA_SMART_AUTOSAVE;
    buff[3]=select;   // YET NOTE - THIS IS A NON-DATA COMMAND!!
    break;
  case IMMEDIATE_OFFLINE:
    buff[2]=ATA_SMART_IMMEDIATE_OFFLINE;
    buff[1]=select;
    break;
  case STATUS_CHECK:
    // This command uses HDIO_DRIVE_TASK and has different syntax than
    // the other commands.
    buff[1]=ATA_SMART_STATUS;
    break;
  default:
    pout("Unrecognized command %d in linux_ata_command_interface()\n"
         "Please contact " PACKAGE_BUGREPORT "\n", command);
    errno=ENOSYS;
    return -1;
  }
  
  // There are two different types of ioctls().  The HDIO_DRIVE_TASK
  // one is this:
  if (command==STATUS_CHECK){

    // NOT DOCUMENTED in /usr/src/linux/include/linux/hdreg.h. You
    // have to read the IDE driver source code.  Sigh.
    // buff[0]: ATA COMMAND CODE REGISTER
    // buff[1]: ATA FEATURES REGISTER
    // buff[2]: ATA SECTOR_COUNT
    // buff[3]: ATA SECTOR NUMBER
    // buff[4]: ATA CYL LO REGISTER
    // buff[5]: ATA CYL HI REGISTER
    // buff[6]: ATA DEVICE HEAD

    unsigned const char normal_lo=0x4f, normal_hi=0xc2;
    unsigned const char failed_lo=0xf4, failed_hi=0x2c;
    buff[4]=normal_lo;
    buff[5]=normal_hi;
    
    if ((retval=ioctl(device, HDIO_DRIVE_TASK, buff)))
      return -1;
    
    // Cyl low and Cyl high unchanged means "Good SMART status"
    if (buff[4]==normal_lo && buff[5]==normal_hi)
      return 0;
    
    // These values mean "Bad SMART status"
    if (buff[4]==failed_lo && buff[5]==failed_hi)
      return 1;
    
    // We haven't gotten output that makes sense; print out some debugging info
    syserror("Error SMART Status command failed");
    pout("Please get assistance from " PACKAGE_HOMEPAGE "\n");
    pout("Register values returned from SMART Status command are:\n");
    pout("CMD=0x%02x\n",(int)buff[0]);
    pout("FR =0x%02x\n",(int)buff[1]);
    pout("NS =0x%02x\n",(int)buff[2]);
    pout("SC =0x%02x\n",(int)buff[3]);
    pout("CL =0x%02x\n",(int)buff[4]);
    pout("CH =0x%02x\n",(int)buff[5]);
    pout("SEL=0x%02x\n",(int)buff[6]);
    return -1;   
  }
  
#if 1
  // Note to people doing ports to other OSes -- don't worry about
  // this block -- you can safely ignore it.  I have put it here
  // because under linux when you do IDENTIFY DEVICE to a packet
  // device, it generates an ugly kernel syslog error message.  This
  // is harmless but frightens users.  So this block detects packet
  // devices and make IDENTIFY DEVICE fail "nicely" without a syslog
  // error message.
  //
  // If you read only the ATA specs, it appears as if a packet device
  // *might* respond to the IDENTIFY DEVICE command.  This is
  // misleading - it's because around the time that SFF-8020 was
  // incorporated into the ATA-3/4 standard, the ATA authors were
  // sloppy. See SFF-8020 and you will see that ATAPI devices have
  // *always* had IDENTIFY PACKET DEVICE as a mandatory part of their
  // command set, and return 'Command Aborted' to IDENTIFY DEVICE.
  if (command==IDENTIFY || command==PIDENTIFY){
    unsigned char deviceid[512];
    // check the device identity, as seen when the system was booted
    // or the device was FIRST registered.  This will not be current
    // if the user has subsequently changed some of the parameters. If
    // device is a packet device, swap the command interpretations.
    if (!ioctl(device, HDIO_GET_IDENTITY, deviceid) && (deviceid[1] & 0x80))
      buff[0]=(command==IDENTIFY)?ATA_IDENTIFY_PACKET_DEVICE:ATA_IDENTIFY_DEVICE;
  }
#endif
  
  // We are now doing the HDIO_DRIVE_CMD type ioctl.
  if ((retval=ioctl(device, HDIO_DRIVE_CMD, buff)))
    return -1;

  // CHECK POWER MODE command returns information in the Sector Count
  // register (buff[3]).  Copy to return data buffer.
  if (command==CHECK_POWER_MODE)
    buff[HDIO_DRIVE_CMD_OFFSET]=buff[2];

  // if the command returns data then copy it back
  if (copydata)
    memcpy(data, buff+HDIO_DRIVE_CMD_OFFSET, copydata);
  
  return 0; 
}

/* SCSI command transmission interface function, implementation is OS
 * specific. Returns 0 if SCSI command successfully launched and response
 * received, else returns a negative errno value */

/* Linux specific code, FreeBSD could conditionally compile in CAM stuff 
 * instead of this. */

/* #include <scsi/scsi.h>       bypass for now */
/* #include <scsi/scsi_ioctl.h> bypass for now */

#define MAX_DXFER_LEN 1024      /* can be increased if necessary */
#define SEND_IOCTL_RESP_SENSE_LEN 16    /* ioctl limitation */
#define DRIVER_SENSE  0x8       /* alternate CHECK CONDITION indication */

#ifndef SCSI_IOCTL_SEND_COMMAND
#define SCSI_IOCTL_SEND_COMMAND 1
#endif
#ifndef SCSI_IOCTL_TEST_UNIT_READY
#define SCSI_IOCTL_TEST_UNIT_READY 2
#endif

struct linux_ioctl_send_command
{
    int inbufsize;
    int outbufsize;
    UINT8 buff[MAX_DXFER_LEN + 16];
};

/* The Linux SCSI_IOCTL_SEND_COMMAND ioctl is primitive and it doesn't 
 * support: CDB length (guesses it from opcode), resid and timeout.
 * Patches in Linux 2.4.21 and 2.5.70 to extend SEND DIAGNOSTIC timeout
 * to 2 hours in order to allow long foreground extended self tests. */
int do_scsi_cmnd_io(int dev_fd, struct scsi_cmnd_io * iop, int report)
{
    struct linux_ioctl_send_command wrk;
    int status, buff_offset;
    size_t len;

    memcpy(wrk.buff, iop->cmnd, iop->cmnd_len);
    buff_offset = iop->cmnd_len;
    if (report > 0) {
        int k, j;
        const unsigned char * ucp = iop->cmnd;
        const char * np;
        char buff[256];
        const int sz = (int)sizeof(buff);

        np = scsi_get_opcode_name(ucp[0]);
        j = snprintf(buff, sz, " [%s: ", np ? np : "<unknown opcode>");
        for (k = 0; k < (int)iop->cmnd_len; ++k)
            j += snprintf(&buff[j], (sz > j ? (sz - j) : 0), "%02x ", ucp[k]);
        if ((report > 1) && 
            (DXFER_TO_DEVICE == iop->dxfer_dir) && (iop->dxferp)) {
            int trunc = (iop->dxfer_len > 256) ? 1 : 0;

            j += snprintf(&buff[j], (sz > j ? (sz - j) : 0), "]\n  Outgoing "
                          "data, len=%d%s:\n", (int)iop->dxfer_len,
                          (trunc ? " [only first 256 bytes shown]" : ""));
            dStrHex(iop->dxferp, (trunc ? 256 : iop->dxfer_len) , 1);
        }
        else
            j += snprintf(&buff[j], (sz > j ? (sz - j) : 0), "]\n");
        pout(buff);
    }
    switch (iop->dxfer_dir) {
        case DXFER_NONE:
            wrk.inbufsize = 0;
            wrk.outbufsize = 0;
            break;
        case DXFER_FROM_DEVICE:
            wrk.inbufsize = 0;
            if (iop->dxfer_len > MAX_DXFER_LEN)
                return -EINVAL;
            wrk.outbufsize = iop->dxfer_len;
            break;
        case DXFER_TO_DEVICE:
            if (iop->dxfer_len > MAX_DXFER_LEN)
                return -EINVAL;
            memcpy(wrk.buff + buff_offset, iop->dxferp, iop->dxfer_len);
            wrk.inbufsize = iop->dxfer_len;
            wrk.outbufsize = 0;
            break;
        default:
            pout("do_scsi_cmnd_io: bad dxfer_dir\n");
            return -EINVAL;
    }
    iop->resp_sense_len = 0;
    iop->scsi_status = 0;
    iop->resid = 0;
    status = ioctl(dev_fd, SCSI_IOCTL_SEND_COMMAND , &wrk);
    if (-1 == status) {
        if (report)
            pout("  status=-1, errno=%d [%s]\n", errno, strerror(errno));
        return -errno;
    }
    if (0 == status) {
        if (report > 0)
            pout("  status=0\n");
        if (DXFER_FROM_DEVICE == iop->dxfer_dir) {
            memcpy(iop->dxferp, wrk.buff, iop->dxfer_len);
            if (report > 1) {
                int trunc = (iop->dxfer_len > 256) ? 1 : 0;

                pout("  Incoming data, len=%d%s:\n", (int)iop->dxfer_len,
                     (trunc ? " [only first 256 bytes shown]" : ""));
                dStrHex(iop->dxferp, (trunc ? 256 : iop->dxfer_len) , 1);
            }
        }
        return 0;
    }
    iop->scsi_status = status & 0x7e; /* bits 0 and 7 used to be for vendors */
    if (DRIVER_SENSE == ((status >> 24) & 0xf))
        iop->scsi_status = SCSI_STATUS_CHECK_CONDITION;
    len = (SEND_IOCTL_RESP_SENSE_LEN < iop->max_sense_len) ?
                SEND_IOCTL_RESP_SENSE_LEN : iop->max_sense_len;
    if ((SCSI_STATUS_CHECK_CONDITION == iop->scsi_status) && 
        iop->sensep && (len > 0)) {
        memcpy(iop->sensep, wrk.buff, len);
        iop->resp_sense_len = len;
        if (report > 1) {
            pout("  >>> Sense buffer, len=%d:\n", (int)len);
            dStrHex(wrk.buff, len , 1);
        }
    }
    if (report) {
        if (SCSI_STATUS_CHECK_CONDITION == iop->scsi_status) {
            pout("  status=%x: sense_key=%x asc=%x ascq=%x\n", status & 0xff,
                 wrk.buff[2] & 0xf, wrk.buff[12], wrk.buff[13]);
        }
        else
            pout("  status=0x%x\n", status);
    }
    if (iop->scsi_status > 0)
        return 0;
    else {
        if (report > 0)
            pout("  ioctl status=0x%x but scsi status=0, fail with EIO\n", 
                 status);
        return -EIO;      /* give up, assume no device there */
    }
}

void printwarning(smart_command_set command);

// PURPOSE
//   This is an interface routine meant to isolate the OS dependent
//   parts of the code, and to provide a debugging interface.  Each
//   different port and OS needs to provide it's own interface.  This
//   is the linux interface to the 3ware 3w-xxxx driver.  It allows ATA
//   commands to be passed through the SCSI driver.
// DETAILED DESCRIPTION OF ARGUMENTS
//   fd: is the file descriptor provided by open()
//   disknum is the disk number (0 to 15) in the RAID array
//   command: defines the different operations.
//   select: additional input data if needed (which log, which type of
//           self-test).
//   data:   location to write output data, if needed (512 bytes).
//   Note: not all commands use all arguments.
// RETURN VALUES
//  -1 if the command failed
//   0 if the command succeeded,
//   STATUS_CHECK routine: 
//  -1 if the command failed
//   0 if the command succeeded and disk SMART status is "OK"
//   1 if the command succeeded and disk SMART status is "FAILING"

int escalade_command_interface(int fd, int disknum, smart_command_set command, int select, char *data){

  // Structures for passing commands through 3Ware Escalade Linux Driver
  TW_Ioctl ioctlbuf;
  TW_Passthru passthru;

  // If command returns 512 bytes, set to 1, else 0
  int readdata=0;

  // Clear out the data structures
  memset (&ioctlbuf, 0, sizeof(TW_Ioctl));
  memset (&passthru, 0, sizeof(TW_Passthru));

  // Same for (almost) all commands - but some reset below
  passthru.byte0.opcode  = 0x11;
  passthru.request_id    = 0xFF;
  passthru.byte3.aport   = disknum;
  passthru.byte3.host_id = 0;
  passthru.status        = 0;           
  passthru.flags         = 0x1;
  passthru.drive_head    = 0x0;
  passthru.sector_num    = 0;

  // All SMART commands use this CL/CH signature.  These are magic
  // values from the ATA specifications.
  passthru.cylinder_lo = 0x4F;
  passthru.cylinder_hi = 0xC2;
  
  // SMART ATA COMMAND REGISTER value
  passthru.command = ATA_SMART_CMD;
  
  // Is this a command that returns 512 bytes?
  if (command == READ_VALUES ||
      command == READ_THRESHOLDS ||
      command == READ_LOG ||
      command == IDENTIFY) {
    readdata=1;
    passthru.byte0.sgloff = 0x5;
    passthru.size         = 0x7;
    passthru.param        = 0xD;
    passthru.sector_count = 0x1;
  }
  else {
    // Non data command -- but doesn't use large sector 
    // count register values.  passthru.param values are:
    // 0x00 - non data command without TFR write check,
    // 0x08 - non data command with TFR write check,
    passthru.byte0.sgloff = 0x0;
    passthru.size         = 0x5;
    passthru.param        = 0x8;
    passthru.sector_count = 0x0;
  }
  
  // Now set ATA registers depending upon command
  switch (command){
  case CHECK_POWER_MODE:
    passthru.command  = ATA_CHECK_POWER_MODE;
    passthru.features    = 0;
    passthru.cylinder_lo = 0;
    passthru.cylinder_hi = 0;
    break;
  case READ_VALUES:
    passthru.features = ATA_SMART_READ_VALUES;
    break;
  case READ_THRESHOLDS:
    passthru.features = ATA_SMART_READ_THRESHOLDS;
    break;
  case READ_LOG:
    passthru.features = ATA_SMART_READ_LOG_SECTOR;
    // log number to return
    passthru.sector_num  = select;
    break;
  case IDENTIFY:
    // ATA IDENTIFY DEVICE
    passthru.command     = ATA_IDENTIFY_DEVICE;
    passthru.features    = 0;
    passthru.cylinder_lo = 0;
    passthru.cylinder_hi = 0;
    break;
  case PIDENTIFY:
    // 3WARE controller can NOT have packet device internally
    pout("WARNING - NO DEVICE FOUND ON 3WARE CONTROLLER (disk %d)\n", disknum);
    errno=ENODEV;
    return -1;
  case ENABLE:
    passthru.features = ATA_SMART_ENABLE;
    break;
  case DISABLE:
    passthru.features = ATA_SMART_DISABLE;
    break;
  case AUTO_OFFLINE:
    passthru.features = ATA_SMART_AUTO_OFFLINE;
    // Enable or disable?
    passthru.sector_count = select;
    break;
  case AUTOSAVE:
    passthru.features = ATA_SMART_AUTOSAVE;
    // Enable or disable?
    passthru.sector_count = select;
    break;
  case IMMEDIATE_OFFLINE:
    passthru.features = ATA_SMART_IMMEDIATE_OFFLINE;
    // What test type to run?
    passthru.sector_num  = select;
    break;
  case STATUS_CHECK:
    passthru.features = ATA_SMART_STATUS;
    break;
  case STATUS:
    // This is JUST to see if SMART is enabled, by giving SMART status
    // command. But it doesn't say if status was good, or failing.
    // See below for the difference.
    passthru.features = ATA_SMART_STATUS;
    break;
  default:
    pout("Unrecognized command %d in linux_3ware_command_interface(disk %d)\n"
         "Please contact " PACKAGE_BUGREPORT "\n", command, disknum);
    errno=ENOSYS;
    return -1;
  }

  /* Copy the passthru command into the ioctl input buffer */
  memcpy(&ioctlbuf.input_data, &passthru, sizeof(TW_Passthru));
  ioctlbuf.cdb[0] = TW_IOCTL;
  ioctlbuf.opcode = TW_ATA_PASSTHRU;

  // CHECKME -- IS THIS RIGHT?? Even for non data I/O commands?
  ioctlbuf.input_length = 512;
  ioctlbuf.output_length = 512;
  
  /* Now send the command down through an ioctl() */
  if (ioctl(fd, SCSI_IOCTL_SEND_COMMAND, &ioctlbuf)) {
    // If error was provoked by driver, tell user how to fix it
    if ((command==AUTO_OFFLINE || command==AUTOSAVE) && select){
      printwarning(command);
      errno=ENOTSUP;
    }
    return -1;
  }

  // If this is a read data command, copy data to output buffer
  if (readdata){
    TW_Output *tw_output=(TW_Output *)&ioctlbuf;
    memcpy(data, tw_output->output_data, 512);
  }
  
  // For STATUS_CHECK, we need to check register values
  if (command==STATUS_CHECK) {
    
    // To find out if the SMART RETURN STATUS is good or failing, we
    // need to examine the values of the Cylinder Low and Cylinder
    // High Registers.
    
    TW_Output *tw_output=(TW_Output *)&ioctlbuf;
    TW_Passthru *tw_passthru_returned=(TW_Passthru *)&(tw_output->output_data);
    unsigned short cyl_lo=tw_passthru_returned->cylinder_lo;
    unsigned short cyl_hi=tw_passthru_returned->cylinder_hi;
    
    // If values in Cyl-LO and Cyl-HI are unchanged, SMART status is good.
    if (cyl_lo==0x4F && cyl_hi==0xC2)
      return 0;
    
    // If values in Cyl-LO and Cyl-HI are as follows, SMART status is FAIL
    if (cyl_lo==0xF4 && cyl_hi==0x2C)
      return 1;
    
    // Any other values mean that something has gone wrong with the command
    printwarning(command);
    errno=ENOSYS;
    return 0;
  }

  // copy sector count register (one byte!) to return data
  if (command==CHECK_POWER_MODE) {
    TW_Output *tw_output=(TW_Output *)&ioctlbuf;
    TW_Passthru *tw_passthru_returned=(TW_Passthru *)&(tw_output->output_data);
    *data=*(char *)&(tw_passthru_returned->sector_count);
  }

  // look for nonexistent devices/ports
  if (command==IDENTIFY && !nonempty((unsigned char *)data, 512)) {
    pout("WARNING - NO DEVICE FOUND ON 3WARE CONTROLLER (disk %d)\n", disknum);
    errno=ENODEV;
    return -1;
  }

  return 0;

}

// Utility function for printing warnings
void printwarning(smart_command_set command){
  static int printed1=0,printed2=0,printed3=0;
  const char* message=
    "can not be passed through the 3ware 3w-xxxx driver.  This can be fixed by\n"
    "applying a simple 3w-xxxx driver patch that can be found here:\n"
    PACKAGE_HOMEPAGE "\n"
    "Alternatively, upgrade your 3w-xxxx driver to version 1.02.00.037 or greater.\n\n";

  if (command==AUTO_OFFLINE && !printed1) {
    printed1=1;
    pout("The SMART AUTO-OFFLINE ENABLE command (smartmontools -o on option/Directive)\n%s", message);
  } 
  else if (command==AUTOSAVE && !printed2) {
    printed2=1;
    pout("The SMART AUTOSAVE ENABLE command (smartmontools -S on option/Directive)\n%s", message);
  }
  else if (command==STATUS_CHECK && !printed3) {
    printed3=1;
    pout("The SMART RETURN STATUS return value (smartmontools -H option/Directive)\n%s", message);
  }
  
  return;
}

// Guess device type (ata or scsi) based on device name (Linux
// specific) SCSI device name in linux can be sd, sr, scd, st, nst,
// osst, nosst and sg.
static const char * lin_dev_prefix = "/dev/";
static const char * lin_dev_ata_disk_plus = "h";
static const char * lin_dev_ata_devfs_disk_plus = "ide/";
static const char * lin_dev_scsi_devfs_disk_plus = "scsi/";
static const char * lin_dev_scsi_disk_plus = "s";
static const char * lin_dev_scsi_tape1 = "ns";
static const char * lin_dev_scsi_tape2 = "os";
static const char * lin_dev_scsi_tape3 = "nos";

int guess_device_type(const char * dev_name) {
  int len;
  int dev_prefix_len = strlen(lin_dev_prefix);
  
  // if dev_name null, or string length zero
  if (!dev_name || !(len = strlen(dev_name)))
    return GUESS_DEVTYPE_DONT_KNOW;
  
  // Remove the leading /dev/... if it's there
  if (!strncmp(lin_dev_prefix, dev_name, dev_prefix_len)) {
    if (len <= dev_prefix_len)
      // if nothing else in the string, unrecognized
      return GUESS_DEVTYPE_DONT_KNOW;
    // else advance pointer to following characters
    dev_name += dev_prefix_len;
  }
  
  // form /dev/h* or h*
  if (!strncmp(lin_dev_ata_disk_plus, dev_name,
               strlen(lin_dev_ata_disk_plus)))
    return GUESS_DEVTYPE_ATA;
  
  // form /dev/ide/* or ide/*
  if (!strncmp(lin_dev_ata_devfs_disk_plus, dev_name,
               strlen(lin_dev_ata_devfs_disk_plus)))
    return GUESS_DEVTYPE_ATA;

  // form /dev/s* or s*
  if (!strncmp(lin_dev_scsi_disk_plus, dev_name,
               strlen(lin_dev_scsi_disk_plus)))
    return GUESS_DEVTYPE_SCSI;

  // form /dev/scsi/* or scsi/*
  if (!strncmp(lin_dev_scsi_devfs_disk_plus, dev_name,
               strlen(lin_dev_scsi_devfs_disk_plus)))
    return GUESS_DEVTYPE_SCSI;
  
  // form /dev/ns* or ns*
  if (!strncmp(lin_dev_scsi_tape1, dev_name,
               strlen(lin_dev_scsi_tape1)))
    return GUESS_DEVTYPE_SCSI;
  
  // form /dev/os* or os*
  if (!strncmp(lin_dev_scsi_tape2, dev_name,
               strlen(lin_dev_scsi_tape2)))
    return GUESS_DEVTYPE_SCSI;
  
  // form /dev/nos* or nos*
  if (!strncmp(lin_dev_scsi_tape3, dev_name,
               strlen(lin_dev_scsi_tape3)))
    return GUESS_DEVTYPE_SCSI;
  
  // we failed to recognize any of the forms
  return GUESS_DEVTYPE_DONT_KNOW;
}


#if 0

[ed@firestorm ed]$ ls -l  /dev/discs
total 0
lr-xr-xr-x    1 root     root           30 Dec 31  1969 disc0 -> ../ide/host2/bus0/target0/lun0/
lr-xr-xr-x    1 root     root           30 Dec 31  1969 disc1 -> ../ide/host2/bus1/target0/lun0/
[ed@firestorm ed]$ ls -l  dev/ide/host*/bus*/target*/lun*/disc
ls: dev/ide/host*/bus*/target*/lun*/disc: No such file or directory
[ed@firestorm ed]$ ls -l  /dev/ide/host*/bus*/target*/lun*/disc
brw-------    1 root     root      33,   0 Dec 31  1969 /dev/ide/host2/bus0/target0/lun0/disc
brw-------    1 root     root      34,   0 Dec 31  1969 /dev/ide/host2/bus1/target0/lun0/disc
[ed@firestorm ed]$ ls -l  /dev/ide/c*b*t*u*
ls: /dev/ide/c*b*t*u*: No such file or directory
[ed@firestorm ed]$ 
Script done on Fri Nov  7 13:46:28 2003

#endif
