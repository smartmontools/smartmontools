/*
 * os_freebsd.c
 *
 * Home page of code is: http://smartmontools.sourceforge.net
 *
 * Copyright (C) 2003-8 Eduard Martinescu <smartmontools-support@lists.sourceforge.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * You should have received a copy of the GNU General Public License
 * (for example COPYING); if not, write to the Free
 * Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <stdio.h>
#include <sys/types.h>
#include <dirent.h>
#include <fcntl.h>
#include <err.h>
#include <camlib.h>
#include <cam/scsi/scsi_message.h>
#include <cam/scsi/scsi_pass.h>
#if defined(__DragonFly__)
#include <sys/nata.h>
#else
#include <sys/ata.h>
#endif
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <glob.h>
#include <fcntl.h>
#include <stddef.h>
#include <paths.h>


#include "config.h"
#include "int64.h"
#include "atacmds.h"
#include "scsicmds.h"
#include "cciss.h"
#include "utility.h"
#include "extern.h"
#include "os_freebsd.h"

static __unused const char *filenameandversion="$Id: os_freebsd.cpp,v 1.73 2009/01/14 02:39:00 sxzzsf Exp $";

const char *os_XXXX_c_cvsid="$Id: os_freebsd.cpp,v 1.73 2009/01/14 02:39:00 sxzzsf Exp $" \
ATACMDS_H_CVSID CCISS_H_CVSID CONFIG_H_CVSID INT64_H_CVSID OS_FREEBSD_H_CVSID SCSICMDS_H_CVSID UTILITY_H_CVSID;

extern smartmonctrl * con;

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
         "                                      (Prints Self-Test & Attribute errors)\n\n"
         "  smartctl -a --device=3ware,2 /dev/twa0\n"
         "  smartctl -a --device=3ware,2 /dev/twe0\n"
         "                              (Prints all SMART information for ATA disk on\n"
         "                                 third port of first 3ware RAID controller)\n"
         );
#else
  printf(
         "  smartctl -a /dev/ad0                       (Prints all SMART information)\n"
         "  smartctl -s on -o on -S on /dev/ad0         (Enables SMART on first disk)\n"
         "  smartctl -t long /dev/ad0              (Executes extended disk self-test)\n"
         "  smartctl -A -l selftest -q errorsonly /dev/ad0\n"
         "                                      (Prints Self-Test & Attribute errors)\n"
         "  smartctl -a -d 3ware,2 /dev/twa0\n"
         "  smartctl -a -d 3ware,2 /dev/twe0\n"
         );
#endif
  return;
}

// Like open().  Return positive integer handle, used by functions below only.
int deviceopen (const char* dev, __unused char* mode) {
  struct freebsd_dev_channel *fdchan;
  int parse_ok, i;

  // Search table for a free entry
  for (i=0; i<FREEBSD_MAXDEV; i++)
    if (!devicetable[i])
      break;
  
  // If no free entry found, return error.  We have max allowed number
  // of "file descriptors" already allocated.
  if (i == FREEBSD_MAXDEV) {
    errno = EMFILE;
    return -1;
  }

  fdchan = (struct freebsd_dev_channel *)calloc(1,sizeof(struct freebsd_dev_channel));
  if (fdchan == NULL) {
    // errno already set by call to malloc()
    return -1;
  }

  parse_ok = parse_ata_chan_dev(dev,fdchan);
  if (parse_ok == CONTROLLER_UNKNOWN) {
    free(fdchan);
    errno = ENOTTY;
    return -1; // can't handle what we don't know
  }

  if (parse_ok == CONTROLLER_ATA) {
#ifdef IOCATAREQUEST
    if ((fdchan->device = open(dev,O_RDONLY))<0) {
#else
    if ((fdchan->atacommand = open("/dev/ata",O_RDWR))<0) {
#endif
      int myerror = errno;	// preserve across free call
      free(fdchan);
      errno = myerror;
      return -1;
    }
  }

  if (parse_ok == CONTROLLER_3WARE_678K_CHAR) {
    char buf[512];
    sprintf(buf,"/dev/twe%d",fdchan->device);
#ifdef IOCATAREQUEST
    if ((fdchan->device = open(buf,O_RDWR))<0) {
#else
    if ((fdchan->atacommand = open(buf,O_RDWR))<0) {
#endif
      int myerror = errno;	// preserve across free call
      free(fdchan);
      errno = myerror;
      return -1;
    }
  }

  if (parse_ok == CONTROLLER_3WARE_9000_CHAR) {
    char buf[512];
    sprintf(buf,"/dev/twa%d",fdchan->device);
#ifdef IOCATAREQUEST
    if ((fdchan->device = open(buf,O_RDWR))<0) {
#else
    if ((fdchan->atacommand = open(buf,O_RDWR))<0) {
#endif
      int myerror = errno;	// preserve across free call
      free(fdchan);
      errno = myerror;
      return -1;
    }
  }

  if (parse_ok == CONTROLLER_HPT) {
    if ((fdchan->device = open(dev,O_RDWR))<0) {
      int myerror = errno;	// preserve across free call
      free(fdchan);
      errno = myerror;
      return -1;
    }
  }

  if (parse_ok == CONTROLLER_CCISS) {
    if ((fdchan->device = open(dev,O_RDWR))<0) {
      int myerror = errno;	// preserve across free call
      free(fdchan);
      errno = myerror;
      return -1;
    }
  }

  if (parse_ok == CONTROLLER_SCSI) {
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
  if (fdchan->device)
    failed=close(fdchan->device);
#ifndef IOCATAREQUEST
  if (fdchan->atacommand)
    failed=close(fdchan->atacommand);
#endif
  
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
#define NO_DISK_3WARE 2
#define BAD_KERNEL 3
#define MAX_MSG 3

// Utility function for printing warnings
void printwarning(int msgNo, const char* extra) {
  static int printed[] = {0,0,0,0};
  static const char* message[]={
    "The SMART RETURN STATUS return value (smartmontools -H option/Directive)\n can not be retrieved with this version of ATAng, please do not rely on this value\nYou should update to at least 5.2\n",
    
    "Error SMART Status command failed\nPlease get assistance from \n" PACKAGE_HOMEPAGE "\nRegister values returned from SMART Status command are:\n",
    
    "You must specify a DISK # for 3ware drives with -d 3ware,<n> where <n> begins with 1 for first disk drive\n",
    
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

int marvell_command_interface(__unused int fd, __unused smart_command_set command, __unused int select, __unused char *data) {
  return -1;
}

int highpoint_command_interface(int fd, smart_command_set command, int select, char *data) {
  int ids[2];
  struct freebsd_dev_channel* fbcon;
  HPT_IOCTL_PARAM param;
  HPT_CHANNEL_INFO_V2 info;
  unsigned char* buff[512 + 2 * sizeof(HPT_PASS_THROUGH_HEADER)];
  PHPT_PASS_THROUGH_HEADER pide_pt_hdr, pide_pt_hdr_out;

  // check that "file descriptor" is valid
  if (isnotopen(&fd, &fbcon))
      return -1;

  // get internal deviceid
  ids[0] = con->hpt_data[0] - 1;
  ids[1] = con->hpt_data[1] - 1;

  memset(&param, 0, sizeof(HPT_IOCTL_PARAM));

  param.magic = HPT_IOCTL_MAGIC;
  param.ctrl_code = HPT_IOCTL_GET_CHANNEL_INFO_V2;
  param.in = (unsigned char *)ids;
  param.in_size = sizeof(unsigned int) * 2;
  param.out = (unsigned char *)&info;
  param.out_size = sizeof(HPT_CHANNEL_INFO_V2);

  if (con->hpt_data[2]==1) {
    param.ctrl_code = HPT_IOCTL_GET_CHANNEL_INFO;
    param.out_size = sizeof(HPT_CHANNEL_INFO);
  }
  if (ioctl(fbcon->device, HPT_DO_IOCONTROL, &param)!=0 ||
      info.devices[con->hpt_data[2]-1]==0) {
    return -1;
  }

  // perform smart action
  memset(buff, 0, 512 + 2 * sizeof(HPT_PASS_THROUGH_HEADER));
  pide_pt_hdr = (PHPT_PASS_THROUGH_HEADER)buff;

  pide_pt_hdr->lbamid = 0x4f;
  pide_pt_hdr->lbahigh = 0xc2;
  pide_pt_hdr->command = ATA_SMART_CMD;
  pide_pt_hdr->id = info.devices[con->hpt_data[2] - 1];

  switch (command){
  case READ_VALUES:
    pide_pt_hdr->feature=ATA_SMART_READ_VALUES;
    pide_pt_hdr->protocol=HPT_READ;
    break;
  case READ_THRESHOLDS:
    pide_pt_hdr->feature=ATA_SMART_READ_THRESHOLDS;
    pide_pt_hdr->protocol=HPT_READ;
    break;
  case READ_LOG:
    pide_pt_hdr->feature=ATA_SMART_READ_LOG_SECTOR;
    pide_pt_hdr->lbalow=select;
    pide_pt_hdr->protocol=HPT_READ;
    break;
  case IDENTIFY:
    pide_pt_hdr->command=ATA_IDENTIFY_DEVICE;
    pide_pt_hdr->protocol=HPT_READ;
    break;
  case ENABLE:
    pide_pt_hdr->feature=ATA_SMART_ENABLE;
    break;
  case DISABLE:
    pide_pt_hdr->feature=ATA_SMART_DISABLE;
    break;
  case AUTO_OFFLINE:
    pide_pt_hdr->feature=ATA_SMART_AUTO_OFFLINE;
    pide_pt_hdr->sectorcount=select;
    break;
  case AUTOSAVE:
    pide_pt_hdr->feature=ATA_SMART_AUTOSAVE;
    pide_pt_hdr->sectorcount=select;
    break;
  case IMMEDIATE_OFFLINE:
    pide_pt_hdr->feature=ATA_SMART_IMMEDIATE_OFFLINE;
    pide_pt_hdr->lbalow=select;
    break;
  case STATUS_CHECK:
  case STATUS:
    pide_pt_hdr->feature=ATA_SMART_STATUS;
    break;
  case CHECK_POWER_MODE:
    pide_pt_hdr->command=ATA_CHECK_POWER_MODE;
    break;
  case WRITE_LOG:
    memcpy(buff+sizeof(HPT_PASS_THROUGH_HEADER), data, 512);
    pide_pt_hdr->feature=ATA_SMART_WRITE_LOG_SECTOR;
    pide_pt_hdr->lbalow=select;
    pide_pt_hdr->protocol=HPT_WRITE;
    break;
  default:
    pout("Unrecognized command %d in highpoint_command_interface()\n"
         "Please contact " PACKAGE_BUGREPORT "\n", command);
    errno=ENOSYS;
    return -1;
  }
  if (pide_pt_hdr->protocol!=0) {
    pide_pt_hdr->sectors = 1;
    pide_pt_hdr->sectorcount = 1;
  }

  memset(&param, 0, sizeof(HPT_IOCTL_PARAM));

  param.magic = HPT_IOCTL_MAGIC;
  param.ctrl_code = HPT_IOCTL_IDE_PASS_THROUGH;
  param.in = (unsigned char *)buff;
  param.in_size = sizeof(HPT_PASS_THROUGH_HEADER) + (pide_pt_hdr->protocol==HPT_READ ? 0 : pide_pt_hdr->sectors * 512);
  param.out = (unsigned char *)buff+param.in_size;
  param.out_size = sizeof(HPT_PASS_THROUGH_HEADER) + (pide_pt_hdr->protocol==HPT_READ ? pide_pt_hdr->sectors * 512 : 0);

  pide_pt_hdr_out = (PHPT_PASS_THROUGH_HEADER)param.out;

  if ((ioctl(fbcon->device, HPT_DO_IOCONTROL, &param)!=0) ||
      (pide_pt_hdr_out->command & 1)) {
    return -1;
  }

  if (command==STATUS_CHECK){
    unsigned const char normal_lo=0x4f, normal_hi=0xc2;
    unsigned const char failed_lo=0xf4, failed_hi=0x2c;
    unsigned char low,high;

    high = pide_pt_hdr_out->lbahigh;
    low = pide_pt_hdr_out->lbamid;

    // Cyl low and Cyl high unchanged means "Good SMART status"
    if (low==normal_lo && high==normal_hi)
      return 0;

    // These values mean "Bad SMART status"
    if (low==failed_lo && high==failed_hi)
      return 1;

    // We haven't gotten output that makes sense; print out some debugging info
    char buf[512];
    sprintf(buf,"CMD=0x%02x\nFR =0x%02x\nNS =0x%02x\nSC =0x%02x\nCL =0x%02x\nCH =0x%02x\nRETURN =0x%04x\n",
            (int)pide_pt_hdr_out->command,
            (int)pide_pt_hdr_out->feature,
            (int)pide_pt_hdr_out->sectorcount,
            (int)pide_pt_hdr_out->lbalow,
            (int)pide_pt_hdr_out->lbamid,
            (int)pide_pt_hdr_out->lbahigh,
            (int)pide_pt_hdr_out->sectors);
    printwarning(BAD_SMART,buf);
  }
  else if (command==CHECK_POWER_MODE)
    data[0] = pide_pt_hdr_out->sectorcount & 0xff;
  else if (pide_pt_hdr->protocol==HPT_READ)
    memcpy(data, (unsigned char *)buff + 2 * sizeof(HPT_PASS_THROUGH_HEADER), pide_pt_hdr->sectors * 512);
  return 0;
}

int areca_command_interface(__unused int fd, __unused int disknum, __unused smart_command_set command, __unused int select, __unused char *data) {
  return -1;
}

int ata_command_interface(int fd, smart_command_set command, int select, char *data) {
#if !defined(ATAREQUEST) && !defined(IOCATAREQUEST)
  // sorry, but without ATAng, we can't do anything here
  printwarning(BAD_KERNEL,NULL);
  errno = ENOSYS;
  return -1;
#else
  struct freebsd_dev_channel* con;
  int retval, copydata=0;
#ifdef IOCATAREQUEST
  struct ata_ioc_request request;
#else
  struct ata_cmd iocmd;
#endif
  unsigned char buff[512];

  // check that "file descriptor" is valid
  if (isnotopen(&fd,&con))
      return -1;

  bzero(buff,512);

#ifdef IOCATAREQUEST
  bzero(&request,sizeof(struct ata_ioc_request));
#else
  bzero(&iocmd,sizeof(struct ata_cmd));
#endif
  bzero(buff,512);

#ifndef IOCATAREQUEST
  iocmd.cmd=ATAREQUEST;
  iocmd.channel=con->channel;
  iocmd.device=con->device;
#define request iocmd.u.request
#endif

  request.u.ata.command=ATA_SMART_CMD;
  request.timeout=600;
  switch (command){
  case READ_VALUES:
    request.u.ata.feature=ATA_SMART_READ_VALUES;
    request.u.ata.lba=0xc24f<<8;
    request.flags=ATA_CMD_READ;
    request.data=(char *)buff;
    request.count=512;
    copydata=1;
    break;
  case READ_THRESHOLDS:
    request.u.ata.feature=ATA_SMART_READ_THRESHOLDS;
    request.u.ata.count=1;
    request.u.ata.lba=1|(0xc24f<<8);
    request.flags=ATA_CMD_READ;
    request.data=(char *)buff;
    request.count=512;
    copydata=1;
    break;
  case READ_LOG:
    request.u.ata.feature=ATA_SMART_READ_LOG_SECTOR;
    request.u.ata.lba=select|(0xc24f<<8);
    request.u.ata.count=1;
    request.flags=ATA_CMD_READ;
    request.data=(char *)buff;
    request.count=512;
    copydata=1;
    break;
  case IDENTIFY:
    request.u.ata.command=ATA_IDENTIFY_DEVICE;
    request.flags=ATA_CMD_READ;
    request.data=(char *)buff;
    request.count=512;
    copydata=1;
    break;
  case PIDENTIFY:
    request.u.ata.command=ATA_IDENTIFY_PACKET_DEVICE;
    request.flags=ATA_CMD_READ;
    request.data=(char *)buff;
    request.count=512;
    copydata=1;
    break;
  case ENABLE:
    request.u.ata.feature=ATA_SMART_ENABLE;
    request.u.ata.lba=0xc24f<<8;
    request.flags=ATA_CMD_CONTROL;
    break;
  case DISABLE:
    request.u.ata.feature=ATA_SMART_DISABLE;
    request.u.ata.lba=0xc24f<<8;
    request.flags=ATA_CMD_CONTROL;
    break;
  case AUTO_OFFLINE:
    // NOTE: According to ATAPI 4 and UP, this command is obsolete
    request.u.ata.feature=ATA_SMART_AUTO_OFFLINE;
    request.u.ata.lba=0xc24f<<8;                                                                                                                                         
    request.u.ata.count=select;                                                                                                                                          
    request.flags=ATA_CMD_CONTROL;
    break;
  case AUTOSAVE:
    request.u.ata.feature=ATA_SMART_AUTOSAVE;
    request.u.ata.lba=0xc24f<<8;
    request.u.ata.count=select;
    request.flags=ATA_CMD_CONTROL;
    break;
  case IMMEDIATE_OFFLINE:
    request.u.ata.feature=ATA_SMART_IMMEDIATE_OFFLINE;
    request.u.ata.lba = select|(0xc24f<<8); // put test in sector
    request.flags=ATA_CMD_CONTROL;
    break;
  case STATUS_CHECK: // same command, no HDIO in FreeBSD
  case STATUS:
    // this command only says if SMART is working.  It could be
    // replaced with STATUS_CHECK below.
    request.u.ata.feature=ATA_SMART_STATUS;
    request.u.ata.lba=0xc24f<<8;
    request.flags=ATA_CMD_CONTROL;
    break;
  case CHECK_POWER_MODE:
    request.u.ata.command=ATA_CHECK_POWER_MODE;
    request.u.ata.feature=0;
    request.flags=ATA_CMD_CONTROL;
    break;
  case WRITE_LOG:
    memcpy(buff, data, 512);
    request.u.ata.feature=ATA_SMART_WRITE_LOG_SECTOR;
    request.u.ata.lba=select|(0xc24f<<8);
    request.u.ata.count=1;
    request.flags=ATA_CMD_WRITE;
    request.data=(char *)buff;
    request.count=512;
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
    
#ifdef IOCATAREQUEST
    if ((retval=ioctl(con->device, IOCATAREQUEST, &request)) || request.error)
#else
    if ((retval=ioctl(con->atacommand, IOCATA, &iocmd)) || request.error)
#endif
      return -1;

#if __FreeBSD_version < 502000
    printwarning(NO_RETURN,NULL);
#endif

    high = (request.u.ata.lba >> 16) & 0xff;
    low = (request.u.ata.lba >> 8) & 0xff;
    
    // Cyl low and Cyl high unchanged means "Good SMART status"
    if (low==normal_lo && high==normal_hi)
      return 0;
    
    // These values mean "Bad SMART status"
    if (low==failed_lo && high==failed_hi)
      return 1;
    
    // We haven't gotten output that makes sense; print out some debugging info
    char buf[512];
    sprintf(buf,"CMD=0x%02x\nFR =0x%02x\nNS =0x%02x\nSC =0x%02x\nCL =0x%02x\nCH =0x%02x\nRETURN =0x%04x\n",
            (int)request.u.ata.command,
            (int)request.u.ata.feature,
            (int)request.u.ata.count,
            (int)((request.u.ata.lba) & 0xff),
            (int)((request.u.ata.lba>>8) & 0xff),
            (int)((request.u.ata.lba>>16) & 0xff),
            (int)request.error);
    printwarning(BAD_SMART,buf);
    return 0;   
  }

#ifdef IOCATAREQUEST
  if ((retval=ioctl(con->device, IOCATAREQUEST, &request)) || request.error)
#else
  if ((retval=ioctl(con->atacommand, IOCATA, &iocmd)) || request.error)
#endif
  {
    return -1;
  }
  // 
  if (command == CHECK_POWER_MODE) {
    data[0] = request.u.ata.count & 0xff;
    return 0;
  }
  if (copydata)
    memcpy(data, buff, 512);
  
  return 0;
#endif
}


// Interface to SCSI devices.  See os_linux.c
int do_normal_scsi_cmnd_io(int fd, struct scsi_cmnd_io * iop, int report)
{
  struct freebsd_dev_channel* con = NULL;
  struct cam_device* cam_dev = NULL;
  union ccb *ccb;
  
  
    if (report > 0) {
        unsigned int k;
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
    return -EIO;
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
                /* timout (converted to seconds) */ iop->timeout*1000);
  memcpy(ccb->csio.cdb_io.cdb_bytes,iop->cmnd,iop->cmnd_len);

  if (cam_send_ccb(cam_dev,ccb) < 0) {
    warn("error sending SCSI ccb");
 #if __FreeBSD_version > 500000
    cam_error_print(cam_dev,ccb,CAM_ESF_ALL,CAM_EPF_ALL,stderr);
 #endif
    cam_freeccb(ccb);
    return -EIO;
  }

  if ((ccb->ccb_h.status & CAM_STATUS_MASK) != CAM_REQ_CMP) {
 #if __FreeBSD_version > 500000
    cam_error_print(cam_dev,ccb,CAM_ESF_ALL,CAM_EPF_ALL,stderr);
 #endif
    cam_freeccb(ccb);
    return -EIO;
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

/* Check and call the right interface. Maybe when the do_generic_scsi_cmd_io interface is better
   we can take off this crude way of calling the right interface */
int do_scsi_cmnd_io(int dev_fd, struct scsi_cmnd_io * iop, int report)
{
struct freebsd_dev_channel *fdchan;
     switch(con->controller_type)
     {
         case CONTROLLER_CCISS:
#ifdef HAVE_DEV_CISS_CISSIO_H
	     // check that "file descriptor" is valid
	     if (isnotopen(&dev_fd,&fdchan))
		 return -ENOTTY;
             return cciss_io_interface(fdchan->device, con->controller_port-1, iop, report);
#else
             {
                 static int warned = 0;
                 if (!warned) {
                     pout("CCISS support is not available in this build of smartmontools,\n"
                          "/usr/src/sys/dev/ciss/cissio.h was not available at build time.\n\n");
                     warned = 1;
                 }
             }
             return -ENOSYS;
#endif
             // not reached
             break;
         default:
             return do_normal_scsi_cmnd_io(dev_fd, iop, report);
             // not reached
             break;
     }
}

// Interface to ATA devices behind 3ware escalade RAID controller cards.  See os_linux.c

#define BUFFER_LEN_678K_CHAR ( sizeof(struct twe_usercommand) ) // 520
#define BUFFER_LEN_9000_CHAR ( sizeof(TW_OSLI_IOCTL_NO_DATA_BUF) + sizeof(TWE_Command) ) // 2048
#define TW_IOCTL_BUFFER_SIZE ( MAX(BUFFER_LEN_678K_CHAR, BUFFER_LEN_9000_CHAR) )

int escalade_command_interface(int fd, int disknum, int escalade_type, smart_command_set command, int select, char *data) {
  // to hold true file descriptor
  struct freebsd_dev_channel* con;

  // return value and buffer for ioctl()
  int  ioctlreturn, readdata=0;
  struct twe_usercommand* cmd_twe = NULL;
  TW_OSLI_IOCTL_NO_DATA_BUF* cmd_twa = NULL;
  TWE_Command_ATA* ata = NULL;

  // Used by both the SCSI and char interfaces
  char ioctl_buffer[TW_IOCTL_BUFFER_SIZE];

  if (disknum < 0) {
    printwarning(NO_DISK_3WARE,NULL);
    return -1;
  }

  // check that "file descriptor" is valid
  if (isnotopen(&fd,&con))
      return -1;

  memset(ioctl_buffer, 0, TW_IOCTL_BUFFER_SIZE);

  if (escalade_type==CONTROLLER_3WARE_9000_CHAR) {
    cmd_twa = (TW_OSLI_IOCTL_NO_DATA_BUF*)ioctl_buffer;
    cmd_twa->pdata = ((TW_OSLI_IOCTL_WITH_PAYLOAD*)cmd_twa)->payload.data_buf;
    cmd_twa->driver_pkt.buffer_length = 512;
    ata = (TWE_Command_ATA*)&cmd_twa->cmd_pkt.command.cmd_pkt_7k;
  } else if (escalade_type==CONTROLLER_3WARE_678K_CHAR) {
    cmd_twe = (struct twe_usercommand*)ioctl_buffer;
    ata = &cmd_twe->tu_command.ata;
  } else {
    pout("Unrecognized escalade_type %d in freebsd_3ware_command_interface(disk %d)\n"
         "Please contact " PACKAGE_BUGREPORT "\n", escalade_type, disknum);
    errno=ENOSYS;
    return -1;
  }

  ata->opcode = TWE_OP_ATA_PASSTHROUGH;

  // Same for (almost) all commands - but some reset below
  ata->request_id    = 0xFF;
  ata->unit          = disknum;
  ata->status        = 0;           
  ata->flags         = 0x1;
  ata->drive_head    = 0x0;
  ata->sector_num    = 0;

  // All SMART commands use this CL/CH signature.  These are magic
  // values from the ATA specifications.
  ata->cylinder_lo   = 0x4F;
  ata->cylinder_hi   = 0xC2;
  
  // SMART ATA COMMAND REGISTER value
  ata->command       = ATA_SMART_CMD;
  
  // Is this a command that reads or returns 512 bytes?
  // passthru->param values are:
  // 0x0 - non data command without TFR write check,
  // 0x8 - non data command with TFR write check,
  // 0xD - data command that returns data to host from device
  // 0xF - data command that writes data from host to device
  // passthru->size values are 0x5 for non-data and 0x07 for data
  if (command == READ_VALUES     ||
      command == READ_THRESHOLDS ||
      command == READ_LOG        ||
      command == IDENTIFY        ||
      command == WRITE_LOG ) {
    readdata=1;
    if (escalade_type==CONTROLLER_3WARE_678K_CHAR) {
      cmd_twe->tu_data = data;
      cmd_twe->tu_size = 512;
    }
    ata->sgl_offset = 0x5;
    ata->size         = 0x5;
    ata->param        = 0xD;
    ata->sector_count = 0x1;
    // For 64-bit to work correctly, up the size of the command packet
    // in dwords by 1 to account for the 64-bit single sgl 'address'
    // field. Note that this doesn't agree with the typedefs but it's
    // right (agree with kernel driver behavior/typedefs).
    //if (sizeof(long)==8)
    //  ata->size++;
  }
  else {
    // Non data command -- but doesn't use large sector 
    // count register values.  
    ata->sgl_offset = 0x0;
    ata->size         = 0x5;
    ata->param        = 0x8;
    ata->sector_count = 0x0;
  }
  
  // Now set ATA registers depending upon command
  switch (command){
  case CHECK_POWER_MODE:
    ata->command     = ATA_CHECK_POWER_MODE;
    ata->features    = 0;
    ata->cylinder_lo = 0;
    ata->cylinder_hi = 0;
    break;
  case READ_VALUES:
    ata->features = ATA_SMART_READ_VALUES;
    break;
  case READ_THRESHOLDS:
    ata->features = ATA_SMART_READ_THRESHOLDS;
    break;
  case READ_LOG:
    ata->features = ATA_SMART_READ_LOG_SECTOR;
    // log number to return
    ata->sector_num  = select;
    break;
  case WRITE_LOG:
    readdata=0;
    ata->features     = ATA_SMART_WRITE_LOG_SECTOR;
    ata->sector_count = 1;
    ata->sector_num   = select;
    ata->param        = 0xF;  // PIO data write
    break;
  case IDENTIFY:
    // ATA IDENTIFY DEVICE
    ata->command     = ATA_IDENTIFY_DEVICE;
    ata->features    = 0;
    ata->cylinder_lo = 0;
    ata->cylinder_hi = 0;
    break;
  case PIDENTIFY:
    // 3WARE controller can NOT have packet device internally
    pout("WARNING - NO DEVICE FOUND ON 3WARE CONTROLLER (disk %d)\n", disknum);
    errno=ENODEV;
    return -1;
  case ENABLE:
    ata->features = ATA_SMART_ENABLE;
    break;
  case DISABLE:
    ata->features = ATA_SMART_DISABLE;
    break;
  case AUTO_OFFLINE:
    ata->features     = ATA_SMART_AUTO_OFFLINE;
    // Enable or disable?
    ata->sector_count = select;
    break;
  case AUTOSAVE:
    ata->features     = ATA_SMART_AUTOSAVE;
    // Enable or disable?
    ata->sector_count = select;
    break;
  case IMMEDIATE_OFFLINE:
    ata->features    = ATA_SMART_IMMEDIATE_OFFLINE;
    // What test type to run?
    ata->sector_num  = select;
    break;
  case STATUS_CHECK:
    ata->features = ATA_SMART_STATUS;
    break;
  case STATUS:
    // This is JUST to see if SMART is enabled, by giving SMART status
    // command. But it doesn't say if status was good, or failing.
    // See below for the difference.
    ata->features = ATA_SMART_STATUS;
    break;
  default:
    pout("Unrecognized command %d in freebsd_3ware_command_interface(disk %d)\n"
         "Please contact " PACKAGE_BUGREPORT "\n", command, disknum);
    errno=ENOSYS;
    return -1;
  }

  // Now send the command down through an ioctl()
  if (escalade_type==CONTROLLER_3WARE_9000_CHAR) {
#ifdef IOCATAREQUEST
    ioctlreturn=ioctl(con->device,TW_OSL_IOCTL_FIRMWARE_PASS_THROUGH,cmd_twa);
#else
    ioctlreturn=ioctl(con->atacommand,TW_OSL_IOCTL_FIRMWARE_PASS_THROUGH,cmd_twa);
#endif
  } else {
#ifdef IOCATAREQUEST
    ioctlreturn=ioctl(con->device,TWEIO_COMMAND,cmd_twe);
#else
    ioctlreturn=ioctl(con->atacommand,TWEIO_COMMAND,cmd_twe);
#endif
  }

  // Deal with the different error cases
  if (ioctlreturn) {
    if (!errno)
      errno=EIO;
    return -1;
  }
  
  // See if the ATA command failed.  Now that we have returned from
  // the ioctl() call, if passthru is valid, then:
  // - ata->status contains the 3ware controller STATUS
  // - ata->command contains the ATA STATUS register
  // - ata->features contains the ATA ERROR register
  //
  // Check bits 0 (error bit) and 5 (device fault) of the ATA STATUS
  // If bit 0 (error bit) is set, then ATA ERROR register is valid.
  // While we *might* decode the ATA ERROR register, at the moment it
  // doesn't make much sense: we don't care in detail why the error
  // happened.
  
  if (ata->status || (ata->command & 0x21)) {
    pout("Command failed, ata.status=(0x%2.2x), ata.command=(0x%2.2x), ata.flags=(0x%2.2x)\n",ata->status,ata->command,ata->flags);
    errno=EIO;
    return -1;
  }
  
  // If this is a read data command, copy data to output buffer
  if (readdata) {
    if (escalade_type==CONTROLLER_3WARE_9000_CHAR)
      memcpy(data, cmd_twa->pdata, 512);
  }

  // For STATUS_CHECK, we need to check register values
  if (command==STATUS_CHECK) {
    
    // To find out if the SMART RETURN STATUS is good or failing, we
    // need to examine the values of the Cylinder Low and Cylinder
    // High Registers.
    
    unsigned short cyl_lo=ata->cylinder_lo;
    unsigned short cyl_hi=ata->cylinder_hi;
    
    // If values in Cyl-LO and Cyl-HI are unchanged, SMART status is good.
    if (cyl_lo==0x4F && cyl_hi==0xC2)
      return 0;
    
    // If values in Cyl-LO and Cyl-HI are as follows, SMART status is FAIL
    if (cyl_lo==0xF4 && cyl_hi==0x2C)
      return 1;
    
      errno=EIO;
      return -1;
  }
  
  // copy sector count register (one byte!) to return data
  if (command==CHECK_POWER_MODE)
    *data=*(char *)&(ata->sector_count);
  
  // look for nonexistent devices/ports
  if (command==IDENTIFY && !nonempty((unsigned char *)data, 512)) {
    errno=ENODEV;
    return -1;
  }
  
  return 0;
}

static int get_tw_channel_unit (const char* name, int* unit, int* dev) {
  const char *p;

  /* device node sanity check */
  for (p = name + 3; *p; p++)
    if (*p < '0' || *p > '9')
      return -1;
  if (strlen(name) > 4 && *(name + 3) == '0')
    return -1;

  if (dev != NULL)
    *dev=atoi(name + 3);

  /* no need for unit number */
  if (unit != NULL)
    *unit=0;
  return 0;
}

static int hpt_hba(const char* name) {
  int i=0;
  const char *hpt_node[]={"hptmv", "hptmv6", "hptrr", "hptiop", "hptmviop", "hpt32xx", "rr2320",
                          "rr232x", "rr2310", "rr2310_00", "rr2300", "rr2340", "rr1740", NULL};
  while (hpt_node[i]) {
    if (!strncmp(name, hpt_node[i], strlen(hpt_node[i])))
      return 1;
    i++;
  }
  return 0;
}

#ifndef ATA_DEVICE
#define ATA_DEVICE "/dev/ata"
#endif

#ifndef IOCATAREQUEST
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

  if ((fd = open(ATA_DEVICE, O_RDWR)) < 0)
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
#endif

// Guess device type (ata or scsi) based on device name (FreeBSD
// specific) SCSI device name in FreeBSD can be sd, sr, scd, st, nst,
// osst, nosst and sg.
static const char * fbsd_dev_prefix = _PATH_DEV;
static const char * fbsd_dev_ata_disk_prefix = "ad";
static const char * fbsd_dev_scsi_disk_plus = "da";
static const char * fbsd_dev_scsi_pass = "pass";
static const char * fbsd_dev_scsi_tape1 = "sa";
static const char * fbsd_dev_scsi_tape2 = "nsa";
static const char * fbsd_dev_scsi_tape3 = "esa";
static const char * fbsd_dev_twe_ctrl = "twe";
static const char * fbsd_dev_twa_ctrl = "twa";
static const char * fbsd_dev_cciss = "ciss";

static int parse_ata_chan_dev(const char * dev_name, struct freebsd_dev_channel *chan) {
  int len;
  int dev_prefix_len = strlen(fbsd_dev_prefix);
  
  // if dev_name null, or string length zero
  if (!dev_name || !(len = strlen(dev_name)))
    return CONTROLLER_UNKNOWN;
  
  // Remove the leading /dev/... if it's there
  if (!strncmp(fbsd_dev_prefix, dev_name, dev_prefix_len)) {
    if (len <= dev_prefix_len) 
      // if nothing else in the string, unrecognized
      return CONTROLLER_UNKNOWN;
    // else advance pointer to following characters
    dev_name += dev_prefix_len;
  }
  // form /dev/ad* or ad*
  if (!strncmp(fbsd_dev_ata_disk_prefix, dev_name,
               strlen(fbsd_dev_ata_disk_prefix))) {
#ifndef IOCATAREQUEST
    if (chan != NULL) {
      if (get_ata_channel_unit(dev_name,&(chan->channel),&(chan->device))<0) {
        return CONTROLLER_UNKNOWN;
      }
    }
#endif
    return CONTROLLER_ATA;
  }

  // form /dev/pass* or pass*
  if (!strncmp(fbsd_dev_scsi_pass, dev_name,
               strlen(fbsd_dev_scsi_pass)))
    goto handlescsi;

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
  
  if (!strncmp(fbsd_dev_twa_ctrl,dev_name,
	       strlen(fbsd_dev_twa_ctrl))) {
    if (chan != NULL) {
      if (get_tw_channel_unit(dev_name,&(chan->channel),&(chan->device))<0) {
	return CONTROLLER_UNKNOWN;
      }
    }
    else if (get_tw_channel_unit(dev_name,NULL,NULL)<0) {
	return CONTROLLER_UNKNOWN;
    }
    return CONTROLLER_3WARE_9000_CHAR;
  }

  if (!strncmp(fbsd_dev_twe_ctrl,dev_name,
	       strlen(fbsd_dev_twe_ctrl))) {
    if (chan != NULL) {
      if (get_tw_channel_unit(dev_name,&(chan->channel),&(chan->device))<0) {
	return CONTROLLER_UNKNOWN;
      }
    }
    else if (get_tw_channel_unit(dev_name,NULL,NULL)<0) {
	return CONTROLLER_UNKNOWN;
    }
    return CONTROLLER_3WARE_678K_CHAR;
  }

  if (hpt_hba(dev_name)) {
    return CONTROLLER_HPT;
  }

  // form /dev/ciss*
  if (!strncmp(fbsd_dev_cciss, dev_name,
               strlen(fbsd_dev_cciss)))
    return CONTROLLER_CCISS;

  // we failed to recognize any of the forms
  return CONTROLLER_UNKNOWN;

 handlescsi:
  if (chan != NULL) {
    if (!(chan->devname = (char *)calloc(1,DEV_IDLEN+1)))
      return CONTROLLER_UNKNOWN;
    
    if (cam_get_device(dev_name,chan->devname,DEV_IDLEN,&(chan->unitnum)) == -1)
      return CONTROLLER_UNKNOWN;
  }
  return CONTROLLER_SCSI;
  
}

int guess_device_type (const char* dev_name) {
  return parse_ata_chan_dev(dev_name,NULL);
}

// global variable holding byte count of allocated memory
extern long long bytes;

// we are using CAM subsystem XPT enumerator to found all SCSI devices on system
// despite of it's names
//
// If any errors occur, leave errno set as it was returned by the
// system call, and return <0.
//
// Return values:
// -1:   error
// >=0: number of discovered devices

int get_dev_names_scsi(char*** names) {
  int n = 0;
  char** mp = NULL;
  unsigned int i;
  union ccb ccb;
  int bufsize, fd = -1;
  int skip_device = 0, skip_bus = 0, changed = 0;
  char *devname = NULL;
  int serrno=-1;

  // in case of non-clean exit
  *names=NULL;
  ccb.cdm.matches = NULL;

  if ((fd = open(XPT_DEVICE, O_RDWR)) == -1) {
    if (errno == ENOENT) /* There are no CAM device on this computer */
      return 0;
    serrno = errno;
    pout("%s control device couldn't opened: %s\n", XPT_DEVICE, strerror(errno));
    n = -1;
    goto end;
  }

  // allocate space for up to MAX_NUM_DEV number of ATA devices
  mp =  (char **)calloc(MAX_NUM_DEV, sizeof(char*));
  if (mp == NULL) {
    serrno=errno;
    pout("Out of memory constructing scan device list (on line %d)\n", __LINE__);
    n = -1;
    goto end;
  };

  bzero(&ccb, sizeof(union ccb));

  ccb.ccb_h.path_id = CAM_XPT_PATH_ID;
  ccb.ccb_h.target_id = CAM_TARGET_WILDCARD;
  ccb.ccb_h.target_lun = CAM_LUN_WILDCARD;

  ccb.ccb_h.func_code = XPT_DEV_MATCH;
  bufsize = sizeof(struct dev_match_result) * MAX_NUM_DEV;
  ccb.cdm.match_buf_len = bufsize;
  ccb.cdm.matches = (struct dev_match_result *)malloc(bufsize);
  if (ccb.cdm.matches == NULL) {
	serrno = errno;
	pout("can't malloc memory for matches on line %d\n", __LINE__);
	n = -1;
	goto end;
  }
  ccb.cdm.num_matches = 0;

  ccb.cdm.num_patterns = 0;
  ccb.cdm.pattern_buf_len = 0;

  /*
   * We do the ioctl multiple times if necessary, in case there are
   * more than MAX_NUM_DEV nodes in the EDT.
   */
  do {
    if (ioctl(fd, CAMIOCOMMAND, &ccb) == -1) {
      serrno = errno;
      pout("error sending CAMIOCOMMAND ioctl: %s\n", strerror(errno));
      n = -1;
      break;
    }

    if ((ccb.ccb_h.status != CAM_REQ_CMP)
     || ((ccb.cdm.status != CAM_DEV_MATCH_LAST)
        && (ccb.cdm.status != CAM_DEV_MATCH_MORE))) {
      pout("got CAM error %#x, CDM error %d\n", ccb.ccb_h.status, ccb.cdm.status);
      serrno = ENXIO;
      n = -1;
      goto end;
    }

    for (i = 0; i < ccb.cdm.num_matches && n < MAX_NUM_DEV; i++) {
      struct bus_match_result *bus_result;
      struct device_match_result *dev_result;
      struct periph_match_result *periph_result;

      if (ccb.cdm.matches[i].type == DEV_MATCH_BUS) {
        bus_result = &ccb.cdm.matches[i].result.bus_result;

        if (strcmp(bus_result->dev_name,"ata") == 0 /* ATAPICAM devices will be probed as ATA devices, skip'em there */
         || strcmp(bus_result->dev_name,"xpt") == 0) /* skip XPT bus at all */
          skip_bus = 1;
        else
          skip_bus = 0;
        changed = 1;
      } else if (ccb.cdm.matches[i].type == DEV_MATCH_DEVICE) {
        dev_result = &ccb.cdm.matches[i].result.device_result;

        if (dev_result->flags & DEV_RESULT_UNCONFIGURED || skip_bus == 1)
          skip_device = 1;
        else
          skip_device = 0;

//        /* Shall we skip non T_DIRECT devices ? */
//        if (dev_result->inq_data.device != T_DIRECT)
//          skip_device = 1;
        changed = 1;
      } else if (ccb.cdm.matches[i].type == DEV_MATCH_PERIPH && skip_device == 0) { 
        /* One device may be populated as many peripherals (pass0 & da 0 fo rexample). 
         * We are searching for latest name
        */
        periph_result =  &ccb.cdm.matches[i].result.periph_result;
        free(devname);
        asprintf(&devname, "%s%s%d", _PATH_DEV, periph_result->periph_name, periph_result->unit_number);
        if (devname == NULL) {
          serrno=errno;
          pout("Out of memory constructing scan SCSI device list (on line %d)\n", __LINE__);
          n = -1;
          goto end;
        };
        changed = 0;
      };
      
      if (changed == 1 && devname != NULL) {
        mp[n] = devname;
        devname = NULL;
        bytes+=1+strlen(mp[n]);
        n++;
        changed = 0;
      };
    }

  } while ((ccb.ccb_h.status == CAM_REQ_CMP) && (ccb.cdm.status == CAM_DEV_MATCH_MORE) && n < MAX_NUM_DEV);

  if (devname != NULL) {
    mp[n] = devname;
    devname = NULL;
    bytes+=1+strlen(mp[n]);
    n++;
  };

  mp = (char **)reallocf(mp,n*(sizeof (char*))); // shrink to correct size
  bytes += (n)*(sizeof(char*)); // and set allocated byte count

end:
  free(ccb.cdm.matches);
  if (fd>-1)
    close(fd);
  if (n <= 0) {
    free(mp);
    mp = NULL;
  }

  *names=mp;

  if (serrno>-1)
    errno=serrno;
  return(n);
}

// we are using ATA subsystem enumerator to found all ATA devices on system
// despite of it's names
//
// If any errors occur, leave errno set as it was returned by the
// system call, and return <0.

// Return values:
// -1:   error
// >=0: number of discovered devices
int get_dev_names_ata(char*** names) {
  struct ata_ioc_devices devices;
  int fd=-1,maxchannel,serrno=-1,n=0;
  char **mp = NULL;

  *names=NULL;

  if ((fd = open(ATA_DEVICE, O_RDWR)) < 0) {
    if (errno == ENOENT) /* There are no ATA device on this computer */
      return 0;
    serrno = errno;
    pout("%s control device can't be opened: %s\n", ATA_DEVICE, strerror(errno));
    n = -1;
    goto end;
  };
  
  if (ioctl(fd, IOCATAGMAXCHANNEL, &maxchannel) < 0) {
    serrno = errno;
    pout("ioctl(IOCATAGMAXCHANNEL) on /dev/ata failed: %s\n", strerror(errno));
    n = -1;
    goto end;
  };

  // allocate space for up to MAX_NUM_DEV number of ATA devices
  mp =  (char **)calloc(MAX_NUM_DEV, sizeof(char*));
  if (mp == NULL) {
    serrno=errno;
    pout("Out of memory constructing scan device list (on line %d)\n", __LINE__);
    n = -1;
    goto end;
  };

  for (devices.channel = 0; devices.channel < maxchannel && n < MAX_NUM_DEV; devices.channel++) {
    int j;
    
    if (ioctl(fd, IOCATADEVICES, &devices) < 0) {
      if (errno == ENXIO)
        continue; /* such channel not exist */
      pout("ioctl(IOCATADEVICES) on %s channel %d failed: %s\n", ATA_DEVICE, devices.channel, strerror(errno));
      n = -1;
      goto end;
    };
    for (j=0;j<=1 && n<MAX_NUM_DEV;j++) {
      if (devices.name[j][0] != '\0') {
        asprintf(mp+n, "%s%s", _PATH_DEV, devices.name[j]);
        if (mp[n] == NULL) {
          pout("Out of memory constructing scan ATA device list (on line %d)\n", __LINE__);
          n = -1;
          goto end;
        };
        bytes+=1+strlen(mp[n]);
        n++;
      };
    };
  };  
  mp = (char **)reallocf(mp,n*(sizeof (char*))); // shrink to correct size
  bytes += (n)*(sizeof(char*)); // and set allocated byte count
      
end:
  if (fd>=0)
    close(fd);
  if (n <= 0) {
    free(mp);
    mp = NULL;
  }

  *names=mp;

  if (serrno>-1)
    errno=serrno;
  return n;
}

int make_device_names (char*** devlist, const char* name) {
  if (!strcmp(name,"SCSI"))
    return get_dev_names_scsi(devlist);
  else if (!strcmp(name,"ATA"))
    return get_dev_names_ata(devlist);
  else
    return 0;
}
