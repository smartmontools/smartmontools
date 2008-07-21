/*
 *  os_linux.c
 *
 * Home page of code is: http://smartmontools.sourceforge.net
 *
 * Copyright (C) 2003-8 Bruce Allen <smartmontools-support@lists.sourceforge.net>
 * Copyright (C) 2003-8 Doug Gilbert <dougg@torque.net>
 * Copyright (C) 2008   Hank Wu <hank@areca.com.tw>
 * Copyright (C) 2008   Oliver Bock <brevilo@users.sourceforge.net>
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

#include "config.h"

#include <errno.h>
#include <fcntl.h>
#include <glob.h>

#include <scsi/scsi_ioctl.h>
#include <scsi/sg.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <unistd.h>
#ifndef makedev // old versions of types.h do not include sysmacros.h
#include <sys/sysmacros.h>
#endif
#ifdef WITH_SELINUX
#include <selinux/selinux.h>
#endif

#include "int64.h"
#include "atacmds.h"
#include "extern.h"
extern smartmonctrl * con;
#include "os_linux.h"
#include "scsicmds.h"
#include "utility.h"
#include "extern.h"
#include "cciss.h"

#ifndef ENOTSUP
#define ENOTSUP ENOSYS
#endif
typedef unsigned long long u8;


#define ARGUSED(x) ((void)(x))

static const char *filenameandversion="$Id: os_linux.cpp,v 1.115 2008/07/21 09:08:26 brevilo Exp $";

const char *os_XXXX_c_cvsid="$Id: os_linux.cpp,v 1.115 2008/07/21 09:08:26 brevilo Exp $" \
ATACMDS_H_CVSID CONFIG_H_CVSID INT64_H_CVSID OS_LINUX_H_CVSID SCSICMDS_H_CVSID UTILITY_H_CVSID;

// global variable holding byte count of allocated memory
extern long long bytes;

/* for passing global control variables */
extern smartmonctrl *con;

/* This function will setup and fix device nodes for a 3ware controller. */
#define MAJOR_STRING_LENGTH 3
#define DEVICE_STRING_LENGTH 32
#define NODE_STRING_LENGTH 16
int setup_3ware_nodes(const char *nodename, const char *driver_name) {
  int              tw_major      = 0;
  int              index         = 0;
  char             majorstring[MAJOR_STRING_LENGTH+1];
  char             device_name[DEVICE_STRING_LENGTH+1];
  char             nodestring[NODE_STRING_LENGTH];
  struct stat      stat_buf;
  FILE             *file;
  int              retval = 0;
#ifdef WITH_SELINUX
  security_context_t orig_context = NULL;
  security_context_t node_context = NULL;
  int                selinux_enabled  = is_selinux_enabled();
  int                selinux_enforced = security_getenforce();
#endif


  /* First try to open up /proc/devices */
  if (!(file = fopen("/proc/devices", "r"))) {
    pout("Error opening /proc/devices to check/create 3ware device nodes\n");
    syserror("fopen");
    return 0;  // don't fail here: user might not have /proc !
  }

  /* Attempt to get device major number */
  while (EOF != fscanf(file, "%3s %32s", majorstring, device_name)) {
    majorstring[MAJOR_STRING_LENGTH]='\0';
    device_name[DEVICE_STRING_LENGTH]='\0';
    if (!strncmp(device_name, nodename, DEVICE_STRING_LENGTH)) {
      tw_major = atoi(majorstring);
      break;
    }
  }
  fclose(file);

  /* See if we found a major device number */
  if (!tw_major) {
    pout("No major number for /dev/%s listed in /proc/devices. Is the %s driver loaded?\n", nodename, driver_name);
    return 2;
  }
#ifdef WITH_SELINUX
  /* Prepare a database of contexts for files in /dev
   * and save the current context */
  if (selinux_enabled) {
    if (matchpathcon_init_prefix(NULL, "/dev") < 0)
      pout("Error initializing contexts database for /dev");
    if (getfscreatecon(&orig_context) < 0) {
      pout("Error retrieving original SELinux fscreate context");
      if (selinux_enforced)
        matchpathcon_fini();
        return 6;
      }
  }
#endif
  /* Now check if nodes are correct */
  for (index=0; index<16; index++) {
    sprintf(nodestring, "/dev/%s%d", nodename, index);
#ifdef WITH_SELINUX
    /* Get context of the node and set it as the default */
    if (selinux_enabled) {
      if (matchpathcon(nodestring, S_IRUSR | S_IWUSR, &node_context) < 0) {
        pout("Could not retrieve context for %s", nodestring);
        if (selinux_enforced) {
          retval = 6;
          break;
        }
      }
      if (setfscreatecon(node_context) < 0) {
        pout ("Error setting default fscreate context");
        if (selinux_enforced) {
          retval = 6;
          break;
        }
      }
    }
#endif
    /* Try to stat the node */
    if ((stat(nodestring, &stat_buf))) {
      pout("Node %s does not exist and must be created. Check the udev rules.\n", nodestring);
      /* Create a new node if it doesn't exist */
      if (mknod(nodestring, S_IFCHR|0600, makedev(tw_major, index))) {
        pout("problem creating 3ware device nodes %s", nodestring);
        syserror("mknod");
        retval = 3;
        break;
      } else {
#ifdef WITH_SELINUX
	if (selinux_enabled && node_context) {
	  freecon(node_context);
	  node_context = NULL;
	}
#endif
        continue;
      }
    }

    /* See if nodes major and minor numbers are correct */
    if ((tw_major != (int)(major(stat_buf.st_rdev))) ||
        (index    != (int)(minor(stat_buf.st_rdev))) ||
        (!S_ISCHR(stat_buf.st_mode))) {
      pout("Node %s has wrong major/minor number and must be created anew."
          " Check the udev rules.\n", nodestring);
      /* Delete the old node */
      if (unlink(nodestring)) {
        pout("problem unlinking stale 3ware device node %s", nodestring);
        syserror("unlink");
        retval = 4;
        break;
      }

      /* Make a new node */
      if (mknod(nodestring, S_IFCHR|0600, makedev(tw_major, index))) {
        pout("problem creating 3ware device nodes %s", nodestring);
        syserror("mknod");
        retval = 5;
        break;
      }
    }
#ifdef WITH_SELINUX
    if (selinux_enabled && node_context) {
      freecon(node_context);
      node_context = NULL;
    }
#endif
  }

#ifdef WITH_SELINUX
  if (selinux_enabled) {
    if(setfscreatecon(orig_context) < 0) {
      pout("Error re-setting original fscreate context");
      if (selinux_enforced)
        retval = 6;
    }
    if(orig_context)
      freecon(orig_context);
    if(node_context)
      freecon(node_context);
    matchpathcon_fini();
  }
#endif
  return retval;
}

static char prev_scsi_dev[128];

// equivalent to open(path, flags)
int deviceopen(const char *pathname, char *type){
  int fd = -1;

  if (0 == strcmp(type,"SCSI")) {
    strncpy(prev_scsi_dev, pathname, sizeof(prev_scsi_dev) - 1);
    fd = open(pathname, O_RDWR | O_NONBLOCK);
    if (fd < 0 && errno == EROFS)
      fd = open(pathname, O_RDONLY | O_NONBLOCK);
  } else if (0 == strcmp(type,"ATA")) {
    // smartd re-opens SCSI devices with "type"==ATA for some reason.
    // If that was a SCSI generic device (e.g. /dev/sg0) then the
    // sg driver wants O_RDWR to allow through ATA PASS-THROUGH commands.
    // The purpose of the next code line is to limit the scope of
    // this change as a release is pending (and smartd needs a rewrite).
    if (0 == strncmp(pathname, prev_scsi_dev, sizeof(prev_scsi_dev)))
      fd = open(pathname, O_RDWR | O_NONBLOCK);
    else
      fd = open(pathname, O_RDONLY | O_NONBLOCK);
  } else if (0 == strcmp(type,"ATA_3WARE_9000")) {
    // the device nodes for this controller are dynamically assigned,
    // so we need to check that they exist with the correct major
    // numbers and if not, create them
    if (setup_3ware_nodes("twa", "3w-9xxx")) {
      if (!errno)
        errno=ENXIO;
      return -1;
    }
    fd = open(pathname, O_RDONLY | O_NONBLOCK);
  }
  else if (0 == strcmp(type,"ATA_3WARE_678K")) {
    // the device nodes for this controller are dynamically assigned,
    // so we need to check that they exist with the correct major
    // numbers and if not, create them
    if (setup_3ware_nodes("twe", "3w-xxxx")) {
      if (!errno)
        errno=ENXIO;
      return -1;
    }
    fd = open(pathname, O_RDONLY | O_NONBLOCK);
  }
  else if(0 == strcmp(type, "CCISS")) {
    // the device is a cciss smart array device.
    fd = open(pathname, O_RDWR | O_NONBLOCK);
  }
  else if (0 == strcmp(type, "ATA_ARECA")) {
    // SCSI Generic (sg) devices support exclusive open (using O_EXCL).
    // For usual files this is only supported during create (O_CREAT)
    // but it's fine to use it while opening existing /dev/sg devices.
    // (Please refer to: http://tldp.org/HOWTO/SCSI-Generic-HOWTO)

    // Try to open device exclusively
    fd = open(pathname, O_RDWR | O_EXCL | O_NONBLOCK);

    if (-1 == fd && EBUSY == errno) {
      // device is locked, show info
      pout("The requested controller is used exclusively by another process!\n" \
           "(e.g. Areca cli[32|64], Areca archttp[32|64], smartctl or smartd)\n" \
           "Please quit the impeding process or try again later...\n\n");
    }
  }

  if (fd != -1) {
    // sets FD_CLOEXEC on the opened device file descriptor.  The
    // descriptor is otherwise leaked to other applications (mail
    // sender) which may be considered a security risk and may result
    // in AVC messages on SELinux-enabled systems.
    if (-1 == fcntl(fd, F_SETFD, FD_CLOEXEC))
      pout("fcntl(set  FD_CLOEXEC) failed, errno=%d [%s]\n", errno, strerror(errno));
  }
  return fd;
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
		  "  smartctl --all /dev/hda                    (Prints all SMART information)\n\n"
		  "  smartctl --smart=on --offlineauto=on --saveauto=on /dev/hda\n"
		  "                                              (Enables SMART on first disk)\n\n"
		  "  smartctl --test=long /dev/hda          (Executes extended disk self-test)\n\n"
		  "  smartctl --attributes --log=selftest --quietmode=errorsonly /dev/hda\n"
		  "                                      (Prints Self-Test & Attribute errors)\n"
		  "  smartctl --all --device=3ware,2 /dev/sda\n"
		  "  smartctl --all --device=3ware,2 /dev/twe0\n"
		  "  smartctl --all --device=3ware,2 /dev/twa0\n"
		  "          (Prints all SMART info for 3rd ATA disk on 3ware RAID controller)\n"
		  "  smartctl --all --device=hpt,1/1/3 /dev/sda\n"
		  "          (Prints all SMART info for the SATA disk attached to the 3rd PMPort\n"
		  "           of the 1st channel on the 1st HighPoint RAID controller)\n"
		  "  smartctl --all --device=areca,3 /dev/sg2\n"
		  "          (Prints all SMART info for 3rd ATA disk on Areca RAID controller)\n"
		  );
#else
	printf(
		  "  smartctl -a /dev/hda                       (Prints all SMART information)\n"
		  "  smartctl -s on -o on -S on /dev/hda         (Enables SMART on first disk)\n"
		  "  smartctl -t long /dev/hda              (Executes extended disk self-test)\n"
		  "  smartctl -A -l selftest -q errorsonly /dev/hda\n"
		  "                                      (Prints Self-Test & Attribute errors)\n"
		  "  smartctl -a -d 3ware,2 /dev/sda\n"
		  "  smartctl -a -d 3ware,2 /dev/twa0\n"
		  "  smartctl -a -d 3ware,2 /dev/twe0\n"
		  "          (Prints all SMART info for 3rd ATA disk on 3ware RAID controller)\n"
		  "  smartctl -a -d hpt,1/1/3 /dev/sda\n"
		  "          (Prints all SMART info for the SATA disk attached to the 3rd PMPort\n"
		  "           of the 1st channel on the 1st HighPoint RAID controller)\n"
		  "  smartctl -a -d areca,3 /dev/sg2\n"
		  "          (Prints all SMART info for 3rd ATA disk on Areca RAID controller)\n"
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
#ifdef GLOB_ABORTED // missing in old versions of glob.h
    else if (retglob==GLOB_ABORTED)
      pout("glob(3) aborted matching pattern %s\n", pattern);
#endif
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
        const char * type = (strcmp(name,"ATA") ? "scsi" : "ide");
        if (strstr(linkbuf, type)){
          snprintf(tmpname, 1024, "%s/disc", globbuf.gl_pathv[i]);
          mp[n++] = CustomStrDup(tmpname, 1, __LINE__, filenameandversion);
        }
      }
    }
  }

  // free memory, track memory usage
  globfree(&globbuf);
  mp = static_cast<char **>(realloc(mp,n*(sizeof(char*))));
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


#define BUFFER_LENGTH (4+512)

int ata_command_interface(int device, smart_command_set command, int select, char *data){
  unsigned char buff[BUFFER_LENGTH];
  // positive: bytes to write to caller.  negative: bytes to READ from
  // caller. zero: non-data command
  int copydata=0;

  const int HDIO_DRIVE_CMD_OFFSET = 4;

  // See struct hd_drive_cmd_hdr in hdreg.h.  Before calling ioctl()
  // buff[0]: ATA COMMAND CODE REGISTER
  // buff[1]: ATA SECTOR NUMBER REGISTER == LBA LOW REGISTER
  // buff[2]: ATA FEATURES REGISTER
  // buff[3]: ATA SECTOR COUNT REGISTER

  // Note that on return:
  // buff[2] contains the ATA SECTOR COUNT REGISTER

  // clear out buff.  Large enough for HDIO_DRIVE_CMD (4+512 bytes)
  memset(buff, 0, BUFFER_LENGTH);

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
  case WRITE_LOG:
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
    // NOTE: According to ATAPI 4 and UP, this command is obsolete
    // select == 241 for enable but no data transfer.  Use TASK ioctl.
    buff[1]=ATA_SMART_AUTO_OFFLINE;
    buff[2]=select;
    break;
  case AUTOSAVE:
    // select == 248 for enable but no data transfer.  Use TASK ioctl.
    buff[1]=ATA_SMART_AUTOSAVE;
    buff[2]=select;
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

  // This command uses the HDIO_DRIVE_TASKFILE ioctl(). This is the
  // only ioctl() that can be used to WRITE data to the disk.
  if (command==WRITE_LOG) {
    unsigned char task[sizeof(ide_task_request_t)+512];
    ide_task_request_t *reqtask=(ide_task_request_t *) task;
    task_struct_t      *taskfile=(task_struct_t *) reqtask->io_ports;
    int retval;

    memset(task,      0, sizeof(task));

    taskfile->data           = 0;
    taskfile->feature        = ATA_SMART_WRITE_LOG_SECTOR;
    taskfile->sector_count   = 1;
    taskfile->sector_number  = select;
    taskfile->low_cylinder   = 0x4f;
    taskfile->high_cylinder  = 0xc2;
    taskfile->device_head    = 0;
    taskfile->command        = ATA_SMART_CMD;

    reqtask->data_phase      = TASKFILE_OUT;
    reqtask->req_cmd         = IDE_DRIVE_TASK_OUT;
    reqtask->out_size        = 512;
    reqtask->in_size         = 0;

    // copy user data into the task request structure
    memcpy(task+sizeof(ide_task_request_t), data, 512);

    if ((retval=ioctl(device, HDIO_DRIVE_TASKFILE, task))) {
      if (retval==-EINVAL)
        pout("Kernel lacks HDIO_DRIVE_TASKFILE support; compile kernel with CONFIG_IDE_TASKFILE_IO set\n");
      return -1;
    }
    return 0;
  }

  // There are two different types of ioctls().  The HDIO_DRIVE_TASK
  // one is this:
  if (command==STATUS_CHECK || command==AUTOSAVE || command==AUTO_OFFLINE){
    int retval;

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

    if ((retval=ioctl(device, HDIO_DRIVE_TASK, buff))) {
      if (retval==-EINVAL) {
        pout("Error SMART Status command via HDIO_DRIVE_TASK failed");
        pout("Rebuild older linux 2.2 kernels with HDIO_DRIVE_TASK support added\n");
      }
      else
        syserror("Error SMART Status command failed");
      return -1;
    }

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
    pout("ST =0x%02x\n",(int)buff[0]);
    pout("ERR=0x%02x\n",(int)buff[1]);
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
    unsigned short deviceid[256];
    // check the device identity, as seen when the system was booted
    // or the device was FIRST registered.  This will not be current
    // if the user has subsequently changed some of the parameters. If
    // device is a packet device, swap the command interpretations.
    if (!ioctl(device, HDIO_GET_IDENTITY, deviceid) && (deviceid[0] & 0x8000))
      buff[0]=(command==IDENTIFY)?ATA_IDENTIFY_PACKET_DEVICE:ATA_IDENTIFY_DEVICE;
  }
#endif

  // We are now doing the HDIO_DRIVE_CMD type ioctl.
  if ((ioctl(device, HDIO_DRIVE_CMD, buff)))
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

// >>>>>> Start of general SCSI specific linux code

/* Linux specific code.
 * Historically smartmontools (and smartsuite before it) used the
 * SCSI_IOCTL_SEND_COMMAND ioctl which is available to all linux device
 * nodes that use the SCSI subsystem. A better interface has been available
 * via the SCSI generic (sg) driver but this involves the extra step of
 * mapping disk devices (e.g. /dev/sda) to the corresponding sg device
 * (e.g. /dev/sg2). In the linux kernel 2.6 series most of the facilities of
 * the sg driver have become available via the SG_IO ioctl which is available
 * on all SCSI devices (on SCSI tape devices from lk 2.6.6).
 * So the strategy below is to find out if the SG_IO ioctl is available and
 * if so use it; failing that use the older SCSI_IOCTL_SEND_COMMAND ioctl.
 * Should work in 2.0, 2.2, 2.4 and 2.6 series linux kernels. */

#define MAX_DXFER_LEN 1024      /* can be increased if necessary */
#define SEND_IOCTL_RESP_SENSE_LEN 16    /* ioctl limitation */
#define SG_IO_RESP_SENSE_LEN 64 /* large enough see buffer */
#define LSCSI_DRIVER_MASK  0xf /* mask out "suggestions" */
#define LSCSI_DRIVER_SENSE  0x8 /* alternate CHECK CONDITION indication */
#define LSCSI_DRIVER_TIMEOUT  0x6
#define LSCSI_DID_TIME_OUT  0x3
#define LSCSI_DID_BUS_BUSY  0x2
#define LSCSI_DID_NO_CONNECT  0x1

#ifndef SCSI_IOCTL_SEND_COMMAND
#define SCSI_IOCTL_SEND_COMMAND 1
#endif

#define SG_IO_PRESENT_UNKNOWN 0
#define SG_IO_PRESENT_YES 1
#define SG_IO_PRESENT_NO 2

static int sg_io_cmnd_io(int dev_fd, struct scsi_cmnd_io * iop, int report,
                         int unknown);
static int sisc_cmnd_io(int dev_fd, struct scsi_cmnd_io * iop, int report);

static int sg_io_state = SG_IO_PRESENT_UNKNOWN;

/* Preferred implementation for issuing SCSI commands in linux. This
 * function uses the SG_IO ioctl. Return 0 if command issued successfully
 * (various status values should still be checked). If the SCSI command
 * cannot be issued then a negative errno value is returned. */
static int sg_io_cmnd_io(int dev_fd, struct scsi_cmnd_io * iop, int report,
                         int unknown)
{
#ifndef SG_IO
    ARGUSED(dev_fd); ARGUSED(iop); ARGUSED(report);
    return -ENOTTY;
#else
    struct sg_io_hdr io_hdr;

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
            dStrHex((const char *)iop->dxferp,
                    (trunc ? 256 : iop->dxfer_len) , 1);
        }
        else
            j += snprintf(&buff[j], (sz > j ? (sz - j) : 0), "]\n");
        pout(buff);
    }
    memset(&io_hdr, 0, sizeof(struct sg_io_hdr));
    io_hdr.interface_id = 'S';
    io_hdr.cmd_len = iop->cmnd_len;
    io_hdr.mx_sb_len = iop->max_sense_len;
    io_hdr.dxfer_len = iop->dxfer_len;
    io_hdr.dxferp = iop->dxferp;
    io_hdr.cmdp = iop->cmnd;
    io_hdr.sbp = iop->sensep;
    /* sg_io_hdr interface timeout has millisecond units. Timeout of 0
       defaults to 60 seconds. */
    io_hdr.timeout = ((0 == iop->timeout) ? 60 : iop->timeout) * 1000;
    switch (iop->dxfer_dir) {
        case DXFER_NONE:
            io_hdr.dxfer_direction = SG_DXFER_NONE;
            break;
        case DXFER_FROM_DEVICE:
            io_hdr.dxfer_direction = SG_DXFER_FROM_DEV;
            break;
        case DXFER_TO_DEVICE:
            io_hdr.dxfer_direction = SG_DXFER_TO_DEV;
            break;
        default:
            pout("do_scsi_cmnd_io: bad dxfer_dir\n");
            return -EINVAL;
    }
    iop->resp_sense_len = 0;
    iop->scsi_status = 0;
    iop->resid = 0;
    if (ioctl(dev_fd, SG_IO, &io_hdr) < 0) {
        if (report && (! unknown))
            pout("  SG_IO ioctl failed, errno=%d [%s]\n", errno,
                 strerror(errno));
        return -errno;
    }
    iop->resid = io_hdr.resid;
    iop->scsi_status = io_hdr.status;
    if (report > 0) {
        pout("  scsi_status=0x%x, host_status=0x%x, driver_status=0x%x\n"
             "  info=0x%x  duration=%d milliseconds  resid=%d\n", io_hdr.status,
             io_hdr.host_status, io_hdr.driver_status, io_hdr.info,
             io_hdr.duration, io_hdr.resid);
        if (report > 1) {
            if (DXFER_FROM_DEVICE == iop->dxfer_dir) {
                int trunc, len;

		len = iop->dxfer_len - iop->resid;
		trunc = (len > 256) ? 1 : 0;
                if (len > 0) {
                    pout("  Incoming data, len=%d%s:\n", len,
                         (trunc ? " [only first 256 bytes shown]" : ""));
                    dStrHex((const char*)iop->dxferp, (trunc ? 256 : len),
                            1);
                } else
                    pout("  Incoming data trimmed to nothing by resid\n");
            }
        }
    }

    if (io_hdr.info | SG_INFO_CHECK) { /* error or warning */
        int masked_driver_status = (LSCSI_DRIVER_MASK & io_hdr.driver_status);

        if (0 != io_hdr.host_status) {
            if ((LSCSI_DID_NO_CONNECT == io_hdr.host_status) ||
                (LSCSI_DID_BUS_BUSY == io_hdr.host_status) ||
                (LSCSI_DID_TIME_OUT == io_hdr.host_status))
                return -ETIMEDOUT;
            else
                return -EIO;    /* catch all */
        }
        if (0 != masked_driver_status) {
            if (LSCSI_DRIVER_TIMEOUT == masked_driver_status)
                return -ETIMEDOUT;
            else if (LSCSI_DRIVER_SENSE != masked_driver_status)
                return -EIO;
        }
        if (LSCSI_DRIVER_SENSE == masked_driver_status)
            iop->scsi_status = SCSI_STATUS_CHECK_CONDITION;
        iop->resp_sense_len = io_hdr.sb_len_wr;
        if ((SCSI_STATUS_CHECK_CONDITION == iop->scsi_status) &&
            iop->sensep && (iop->resp_sense_len > 0)) {
            if (report > 1) {
                pout("  >>> Sense buffer, len=%d:\n",
                     (int)iop->resp_sense_len);
                dStrHex((const char *)iop->sensep, iop->resp_sense_len , 1);
            }
        }
        if (report) {
            if (SCSI_STATUS_CHECK_CONDITION == iop->scsi_status) {
                if ((iop->sensep[0] & 0x7f) > 0x71)
                    pout("  status=%x: [desc] sense_key=%x asc=%x ascq=%x\n",
                         iop->scsi_status, iop->sensep[1] & 0xf,
                         iop->sensep[2], iop->sensep[3]);
                else
                    pout("  status=%x: sense_key=%x asc=%x ascq=%x\n",
                         iop->scsi_status, iop->sensep[2] & 0xf,
                         iop->sensep[12], iop->sensep[13]);
            }
            else
                pout("  status=0x%x\n", iop->scsi_status);
        }
    }
    return 0;
#endif
}

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
static int sisc_cmnd_io(int dev_fd, struct scsi_cmnd_io * iop, int report)
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
            dStrHex((const char *)iop->dxferp,
                    (trunc ? 256 : iop->dxfer_len) , 1);
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
    status = ioctl(dev_fd, SCSI_IOCTL_SEND_COMMAND, &wrk);
    if (-1 == status) {
        if (report)
            pout("  SCSI_IOCTL_SEND_COMMAND ioctl failed, errno=%d [%s]\n",
                 errno, strerror(errno));
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
                dStrHex((const char*)iop->dxferp,
                        (trunc ? 256 : iop->dxfer_len) , 1);
            }
        }
        return 0;
    }
    iop->scsi_status = status & 0x7e; /* bits 0 and 7 used to be for vendors */
    if (LSCSI_DRIVER_SENSE == ((status >> 24) & 0xf))
        iop->scsi_status = SCSI_STATUS_CHECK_CONDITION;
    len = (SEND_IOCTL_RESP_SENSE_LEN < iop->max_sense_len) ?
                SEND_IOCTL_RESP_SENSE_LEN : iop->max_sense_len;
    if ((SCSI_STATUS_CHECK_CONDITION == iop->scsi_status) &&
        iop->sensep && (len > 0)) {
        memcpy(iop->sensep, wrk.buff, len);
        iop->resp_sense_len = len;
        if (report > 1) {
            pout("  >>> Sense buffer, len=%d:\n", (int)len);
            dStrHex((const char *)wrk.buff, len , 1);
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

/* SCSI command transmission interface function, linux version.
 * Returns 0 if SCSI command successfully launched and response
 * received. Even when 0 is returned the caller should check
 * scsi_cmnd_io::scsi_status for SCSI defined errors and warnings
 * (e.g. CHECK CONDITION). If the SCSI command could not be issued
 * (e.g. device not present or timeout) or some other problem
 * (e.g. timeout) then returns a negative errno value */
static int do_normal_scsi_cmnd_io(int dev_fd, struct scsi_cmnd_io * iop,
                                  int report)
{
    int res;

    /* implementation relies on static sg_io_state variable. If not
     * previously set tries the SG_IO ioctl. If that succeeds assume
     * that SG_IO ioctl functional. If it fails with an errno value
     * other than ENODEV (no device) or permission then assume
     * SCSI_IOCTL_SEND_COMMAND is the only option. */
    switch (sg_io_state) {
    case SG_IO_PRESENT_UNKNOWN:
        /* ignore report argument */
        if (0 == (res = sg_io_cmnd_io(dev_fd, iop, report, 1))) {
            sg_io_state = SG_IO_PRESENT_YES;
            return 0;
        } else if ((-ENODEV == res) || (-EACCES == res) || (-EPERM == res))
            return res;         /* wait until we see a device */
        sg_io_state = SG_IO_PRESENT_NO;
        /* drop through by design */
    case SG_IO_PRESENT_NO:
        return sisc_cmnd_io(dev_fd, iop, report);
    case SG_IO_PRESENT_YES:
        return sg_io_cmnd_io(dev_fd, iop, report, 0);
    default:
        pout(">>>> do_scsi_cmnd_io: bad sg_io_state=%d\n", sg_io_state);
        sg_io_state = SG_IO_PRESENT_UNKNOWN;
        return -EIO;    /* report error and reset state */
    }
}

/* Check and call the right interface. Maybe when the do_generic_scsi_cmd_io interface is better
   we can take off this crude way of calling the right interface */
 int do_scsi_cmnd_io(int dev_fd, struct scsi_cmnd_io * iop, int report)
 {
     switch(con->controller_type)
     {
         case CONTROLLER_CCISS:
#ifdef HAVE_LINUX_CCISS_IOCTL_H
             return cciss_io_interface(dev_fd, con->controller_port-1, iop, report);
#else
             {
                 static int warned = 0;
                 if (!warned) {
                     pout("CCISS support is not available in this build of smartmontools,\n"
                          "<linux/cciss_ioctl.h> was not available at build time.\n\n");
                     warned = 1;
                 }
             }
             errno = ENOSYS;
             return -1;
#endif
             // not reached
             break;
         default:
             return do_normal_scsi_cmnd_io(dev_fd, iop, report);
             // not reached
             break;
     }
 }

// >>>>>> End of general SCSI specific linux code

// prototype
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
//   escalade_type indicates the type of controller type, and if scsi or char interface is used
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


/* 512 is the max payload size: increase if needed */
#define BUFFER_LEN_678K      ( sizeof(TW_Ioctl)                  ) // 1044 unpacked, 1041 packed
#define BUFFER_LEN_678K_CHAR ( sizeof(TW_New_Ioctl)+512-1        ) // 1539 unpacked, 1536 packed
#define BUFFER_LEN_9000      ( sizeof(TW_Ioctl_Buf_Apache)+512-1 ) // 2051 unpacked, 2048 packed
#define TW_IOCTL_BUFFER_SIZE ( MAX(MAX(BUFFER_LEN_678K, BUFFER_LEN_9000), BUFFER_LEN_678K_CHAR) )

int escalade_command_interface(int fd, int disknum, int escalade_type, smart_command_set command, int select, char *data){

  // return value and buffer for ioctl()
  int  ioctlreturn, readdata=0;

  // Used by both the SCSI and char interfaces
  TW_Passthru *passthru=NULL;
  char ioctl_buffer[TW_IOCTL_BUFFER_SIZE];

  // only used for SCSI device interface
  TW_Ioctl   *tw_ioctl=NULL;
  TW_Output *tw_output=NULL;

  // only used for 6000/7000/8000 char device interface
  TW_New_Ioctl *tw_ioctl_char=NULL;

  // only used for 9000 character device interface
  TW_Ioctl_Buf_Apache *tw_ioctl_apache=NULL;

  memset(ioctl_buffer, 0, TW_IOCTL_BUFFER_SIZE);

  if (escalade_type==CONTROLLER_3WARE_9000_CHAR) {
    tw_ioctl_apache                               = (TW_Ioctl_Buf_Apache *)ioctl_buffer;
    tw_ioctl_apache->driver_command.control_code  = TW_IOCTL_FIRMWARE_PASS_THROUGH;
    tw_ioctl_apache->driver_command.buffer_length = 512; /* payload size */
    passthru                                      = (TW_Passthru *)&(tw_ioctl_apache->firmware_command.command.oldcommand);
  }
  else if (escalade_type==CONTROLLER_3WARE_678K_CHAR) {
    tw_ioctl_char                                 = (TW_New_Ioctl *)ioctl_buffer;
    tw_ioctl_char->data_buffer_length             = 512;
    passthru                                      = (TW_Passthru *)&(tw_ioctl_char->firmware_command);
  }
  else if (escalade_type==CONTROLLER_3WARE_678K) {
    tw_ioctl                                      = (TW_Ioctl *)ioctl_buffer;
    tw_ioctl->cdb[0]                              = TW_IOCTL;
    tw_ioctl->opcode                              = TW_ATA_PASSTHRU;
    tw_ioctl->input_length                        = 512; // correct even for non-data commands
    tw_ioctl->output_length                       = 512; // correct even for non-data commands
    tw_output                                     = (TW_Output *)tw_ioctl;
    passthru                                      = (TW_Passthru *)&(tw_ioctl->input_data);
  }
  else {
    pout("Unrecognized escalade_type %d in linux_3ware_command_interface(disk %d)\n"
         "Please contact " PACKAGE_BUGREPORT "\n", escalade_type, disknum);
    errno=ENOSYS;
    return -1;
  }

  // Same for (almost) all commands - but some reset below
  passthru->byte0.opcode  = TW_OP_ATA_PASSTHRU;
  passthru->request_id    = 0xFF;
  passthru->unit          = disknum;
  passthru->status        = 0;
  passthru->flags         = 0x1;
  passthru->drive_head    = 0x0;
  passthru->sector_num    = 0;

  // All SMART commands use this CL/CH signature.  These are magic
  // values from the ATA specifications.
  passthru->cylinder_lo   = 0x4F;
  passthru->cylinder_hi   = 0xC2;

  // SMART ATA COMMAND REGISTER value
  passthru->command       = ATA_SMART_CMD;

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
    passthru->byte0.sgloff = 0x5;
    passthru->size         = 0x7;
    passthru->param        = 0xD;
    passthru->sector_count = 0x1;
    // For 64-bit to work correctly, up the size of the command packet
    // in dwords by 1 to account for the 64-bit single sgl 'address'
    // field. Note that this doesn't agree with the typedefs but it's
    // right (agree with kernel driver behavior/typedefs).
    if (escalade_type==CONTROLLER_3WARE_9000_CHAR && sizeof(long)==8)
      passthru->size++;
  }
  else {
    // Non data command -- but doesn't use large sector
    // count register values.
    passthru->byte0.sgloff = 0x0;
    passthru->size         = 0x5;
    passthru->param        = 0x8;
    passthru->sector_count = 0x0;
  }

  // Now set ATA registers depending upon command
  switch (command){
  case CHECK_POWER_MODE:
    passthru->command     = ATA_CHECK_POWER_MODE;
    passthru->features    = 0;
    passthru->cylinder_lo = 0;
    passthru->cylinder_hi = 0;
    break;
  case READ_VALUES:
    passthru->features = ATA_SMART_READ_VALUES;
    break;
  case READ_THRESHOLDS:
    passthru->features = ATA_SMART_READ_THRESHOLDS;
    break;
  case READ_LOG:
    passthru->features = ATA_SMART_READ_LOG_SECTOR;
    // log number to return
    passthru->sector_num  = select;
    break;
  case WRITE_LOG:
    if (escalade_type == CONTROLLER_3WARE_9000_CHAR)
      memcpy((unsigned char *)tw_ioctl_apache->data_buffer, data, 512);
    else if (escalade_type == CONTROLLER_3WARE_678K_CHAR)
      memcpy((unsigned char *)tw_ioctl_char->data_buffer,   data, 512);
    else {
      // COMMAND NOT SUPPORTED VIA SCSI IOCTL INTERFACE
      // memcpy(tw_output->output_data, data, 512);
      printwarning(command);
      errno=ENOTSUP;
      return -1;
    }
    readdata=0;
    passthru->features     = ATA_SMART_WRITE_LOG_SECTOR;
    passthru->sector_count = 1;
    passthru->sector_num   = select;
    passthru->param        = 0xF;  // PIO data write
    break;
  case IDENTIFY:
    // ATA IDENTIFY DEVICE
    passthru->command     = ATA_IDENTIFY_DEVICE;
    passthru->features    = 0;
    passthru->cylinder_lo = 0;
    passthru->cylinder_hi = 0;
    break;
  case PIDENTIFY:
    // 3WARE controller can NOT have packet device internally
    pout("WARNING - NO DEVICE FOUND ON 3WARE CONTROLLER (disk %d)\n", disknum);
    pout("Note: /dev/sdX many need to be replaced with /dev/tweN or /dev/twaN\n");
    errno=ENODEV;
    return -1;
  case ENABLE:
    passthru->features = ATA_SMART_ENABLE;
    break;
  case DISABLE:
    passthru->features = ATA_SMART_DISABLE;
    break;
  case AUTO_OFFLINE:
    passthru->features     = ATA_SMART_AUTO_OFFLINE;
    // Enable or disable?
    passthru->sector_count = select;
    break;
  case AUTOSAVE:
    passthru->features     = ATA_SMART_AUTOSAVE;
    // Enable or disable?
    passthru->sector_count = select;
    break;
  case IMMEDIATE_OFFLINE:
    passthru->features    = ATA_SMART_IMMEDIATE_OFFLINE;
    // What test type to run?
    passthru->sector_num  = select;
    break;
  case STATUS_CHECK:
    passthru->features = ATA_SMART_STATUS;
    break;
  case STATUS:
    // This is JUST to see if SMART is enabled, by giving SMART status
    // command. But it doesn't say if status was good, or failing.
    // See below for the difference.
    passthru->features = ATA_SMART_STATUS;
    break;
  default:
    pout("Unrecognized command %d in linux_3ware_command_interface(disk %d)\n"
         "Please contact " PACKAGE_BUGREPORT "\n", command, disknum);
    errno=ENOSYS;
    return -1;
  }

  // Now send the command down through an ioctl()
  if (escalade_type==CONTROLLER_3WARE_9000_CHAR)
    ioctlreturn=ioctl(fd, TW_IOCTL_FIRMWARE_PASS_THROUGH, tw_ioctl_apache);
  else if (escalade_type==CONTROLLER_3WARE_678K_CHAR)
    ioctlreturn=ioctl(fd, TW_CMD_PACKET_WITH_DATA, tw_ioctl_char);
  else
    ioctlreturn=ioctl(fd, SCSI_IOCTL_SEND_COMMAND, tw_ioctl);

  // Deal with the different error cases
  if (ioctlreturn) {
    if (CONTROLLER_3WARE_678K==escalade_type && ((command==AUTO_OFFLINE || command==AUTOSAVE) && select)){
      // error here is probably a kernel driver whose version is too old
      printwarning(command);
      errno=ENOTSUP;
    }
    if (!errno)
      errno=EIO;
    return -1;
  }

  // The passthru structure is valid after return from an ioctl if:
  // - we are using the character interface OR
  // - we are using the SCSI interface and this is a NON-READ-DATA command
  // For SCSI interface, note that we set passthru to a different
  // value after ioctl().
  if (CONTROLLER_3WARE_678K==escalade_type) {
    if (readdata)
      passthru=NULL;
    else
      passthru=(TW_Passthru *)&(tw_output->output_data);
  }

  // See if the ATA command failed.  Now that we have returned from
  // the ioctl() call, if passthru is valid, then:
  // - passthru->status contains the 3ware controller STATUS
  // - passthru->command contains the ATA STATUS register
  // - passthru->features contains the ATA ERROR register
  //
  // Check bits 0 (error bit) and 5 (device fault) of the ATA STATUS
  // If bit 0 (error bit) is set, then ATA ERROR register is valid.
  // While we *might* decode the ATA ERROR register, at the moment it
  // doesn't make much sense: we don't care in detail why the error
  // happened.

  if (passthru && (passthru->status || (passthru->command & 0x21))) {
    errno=EIO;
    return -1;
  }

  // If this is a read data command, copy data to output buffer
  if (readdata) {
    if (escalade_type==CONTROLLER_3WARE_9000_CHAR)
      memcpy(data, (unsigned char *)tw_ioctl_apache->data_buffer, 512);
    else if (escalade_type==CONTROLLER_3WARE_678K_CHAR)
      memcpy(data, (unsigned char *)tw_ioctl_char->data_buffer, 512);
    else
      memcpy(data, tw_output->output_data, 512);
  }

  // For STATUS_CHECK, we need to check register values
  if (command==STATUS_CHECK) {

    // To find out if the SMART RETURN STATUS is good or failing, we
    // need to examine the values of the Cylinder Low and Cylinder
    // High Registers.

    unsigned short cyl_lo=passthru->cylinder_lo;
    unsigned short cyl_hi=passthru->cylinder_hi;

    // If values in Cyl-LO and Cyl-HI are unchanged, SMART status is good.
    if (cyl_lo==0x4F && cyl_hi==0xC2)
      return 0;

    // If values in Cyl-LO and Cyl-HI are as follows, SMART status is FAIL
    if (cyl_lo==0xF4 && cyl_hi==0x2C)
      return 1;

    // Any other values mean that something has gone wrong with the command
    if (CONTROLLER_3WARE_678K==escalade_type) {
      printwarning(command);
      errno=ENOSYS;
      return 0;
    }
    else {
      errno=EIO;
      return -1;
    }
  }

  // copy sector count register (one byte!) to return data
  if (command==CHECK_POWER_MODE)
    *data=*(char *)&(passthru->sector_count);

  // look for nonexistent devices/ports
  if (command==IDENTIFY && !nonempty((unsigned char *)data, 512)) {
    errno=ENODEV;
    return -1;
  }

  return 0;
}


// PURPOSE
//   This is an interface routine meant to isolate the OS dependent
//   parts of the code, and to provide a debugging interface.  Each
//   different port and OS needs to provide it's own interface.  This
//   is the linux interface to the Areca "arcmsr" driver.  It allows ATA
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


/*DeviceType*/
#define ARECA_SATA_RAID                      	0x90000000
/*FunctionCode*/
#define FUNCTION_READ_RQBUFFER               	0x0801
#define FUNCTION_WRITE_WQBUFFER              	0x0802
#define FUNCTION_CLEAR_RQBUFFER              	0x0803
#define FUNCTION_CLEAR_WQBUFFER              	0x0804

/* ARECA IO CONTROL CODE*/
#define ARCMSR_IOCTL_READ_RQBUFFER           	(ARECA_SATA_RAID | FUNCTION_READ_RQBUFFER)
#define ARCMSR_IOCTL_WRITE_WQBUFFER          	(ARECA_SATA_RAID | FUNCTION_WRITE_WQBUFFER)
#define ARCMSR_IOCTL_CLEAR_RQBUFFER          	(ARECA_SATA_RAID | FUNCTION_CLEAR_RQBUFFER)
#define ARCMSR_IOCTL_CLEAR_WQBUFFER          	(ARECA_SATA_RAID | FUNCTION_CLEAR_WQBUFFER)
#define ARECA_SIG_STR							"ARCMSR"

// The SRB_IO_CONTROL & SRB_BUFFER structures are used to communicate(to/from) to areca driver
typedef struct _SRB_IO_CONTROL
{
	unsigned int HeaderLength;
	unsigned char Signature[8];
	unsigned int Timeout;
	unsigned int ControlCode;
	unsigned int ReturnCode;
	unsigned int Length;
} sSRB_IO_CONTROL;

typedef struct _SRB_BUFFER
{
	sSRB_IO_CONTROL srbioctl;
	unsigned char   ioctldatabuffer[1032]; // the buffer to put the command data to/from firmware
} sSRB_BUFFER;

// Looks in /proc/scsi to suggest correct areca devices
// If hint not NULL, return device path guess
int find_areca_in_proc(char *hint) {
 
    const char* proc_format_string="host\tchan\tid\tlun\ttype\topens\tqdepth\tbusy\tonline\n";

    // check data formwat
    FILE *fp=fopen("/proc/scsi/sg/device_hdr", "r");
    if (!fp) {
        pout("Unable to open /proc/scsi/sg/device_hdr for reading\n");
        return 1;
     }

     // get line, compare to format
     char linebuf[256];
     linebuf[255]='\0';
     char *out = fgets(linebuf, 256, fp);
     fclose(fp);
     if (!out) {
         pout("Unable to read contents of /proc/scsi/sg/device_hdr\n");
         return 2;
     }

     if (strcmp(linebuf, proc_format_string)) {
     	// wrong format!
	// Fix this by comparing only tokens not white space!!
	pout("Unexpected format %s in /proc/scsi/sg/device_hdr\n", proc_format_string);
	return 3;
     }

    // Format is understood, now search for correct device
    fp=fopen("/proc/scsi/sg/devices", "r");
    if (!fp) return 1;
    int host, chan, id, lun, type, opens, qdepth, busy, online;
    int dev=-1;
    int found=0;
    // search all lines of /proc/scsi/sg/devices
    while (9 == fscanf(fp, "%d %d %d %d %d %d %d %d %d", &host, &chan, &id, &lun, &type, &opens, &qdepth, &busy, &online)) {
        dev++;
	if (id == 16 && type == 3) {
	   // devices with id=16 and type=3 might be Areca controllers
	   if (!found && hint) {
	       sprintf(hint, "/dev/sg%d", dev);
	   }
	   pout("Device /dev/sg%d appears to be an Areca controller.\n", dev);
           found++;
        }
    }
    fclose(fp);
    return 0;
}



void dumpdata( unsigned char *block, int len)
{
	int ln = (len / 16) + 1;	 // total line#
	unsigned char c;
	int pos = 0;

	printf(" Address = %p, Length = (0x%x)%d\n", block, len, len);
	printf("      0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F      ASCII      \n");
	printf("=====================================================================\n");

	for ( int l = 0; l < ln && len; l++ )
	{
		// printf the line# and the HEX data
		// if a line data length < 16 then append the space to the tail of line to reach 16 chars
		printf("%02X | ", l);
		for ( pos = 0; pos < 16 && len; pos++, len-- )
		{
			c = block[l*16+pos];    
			printf("%02X ", c);
		}

		if ( pos < 16 )
		{
			for ( int loop = pos; loop < 16; loop++ )
			{
				printf("   ");
			}
		}

		// print ASCII char
		for ( int loop = 0; loop < pos; loop++ )
		{
			c = block[l*16+loop];
			if ( c >= 0x20 && c <= 0x7F )
			{
				printf("%c", c);
			}
			else
			{
				printf(".");
			}
		}
		printf("\n");
	}   
	printf("=====================================================================\n");
}



int arcmsr_command_handler(int fd, unsigned long arcmsr_cmd, unsigned char *data, int data_len, void *ext_data /* reserved for further use */)
{
	ARGUSED(ext_data);

	int ioctlreturn = 0;
	sSRB_BUFFER sBuf;
	struct scsi_cmnd_io io_hdr;  
	int dir = DXFER_TO_DEVICE;

	UINT8 cdb[10];
	UINT8 sense[32];

	unsigned char *areca_return_packet;
	int total = 0;
	int expected = -1;
	unsigned char return_buff[2048];
	unsigned char *ptr = &return_buff[0];
	memset(return_buff, 0, sizeof(return_buff));

	memset((unsigned char *)&sBuf, 0, sizeof(sBuf));
	memset(&io_hdr, 0, sizeof(io_hdr));
	memset(cdb, 0, sizeof(cdb));
	memset(sense, 0, sizeof(sense));


	sBuf.srbioctl.HeaderLength = sizeof(sSRB_IO_CONTROL);   
	memcpy(sBuf.srbioctl.Signature, ARECA_SIG_STR, strlen(ARECA_SIG_STR));
	sBuf.srbioctl.Timeout = 10000;      
	sBuf.srbioctl.ControlCode = ARCMSR_IOCTL_READ_RQBUFFER;

	switch ( arcmsr_cmd )
	{
	// command for writing data to driver
	case ARCMSR_IOCTL_WRITE_WQBUFFER:   
		if ( data && data_len )
		{
			sBuf.srbioctl.Length = data_len;    
			memcpy((unsigned char *)sBuf.ioctldatabuffer, (unsigned char *)data, data_len);
		}
		// commands for clearing related buffer of driver
	case ARCMSR_IOCTL_CLEAR_RQBUFFER:
	case ARCMSR_IOCTL_CLEAR_WQBUFFER:
		cdb[0] = 0x3B; //SCSI_WRITE_BUF command;
		break;
		// command for reading data from driver
	case ARCMSR_IOCTL_READ_RQBUFFER:    
		cdb[0] = 0x3C; //SCSI_READ_BUF command;
		dir = DXFER_FROM_DEVICE;
		break;
	default:
		// unknown arcmsr commands
		return -1;
	}

	cdb[1] = 0x01;
	cdb[2] = 0xf0;    
	//
	// cdb[5][6][7][8] areca defined command code( to/from driver )
	//    
	cdb[5] = (char)( arcmsr_cmd >> 24);
	cdb[6] = (char)( arcmsr_cmd >> 16);
	cdb[7] = (char)( arcmsr_cmd >> 8);
	cdb[8] = (char)( arcmsr_cmd & 0x0F );

	io_hdr.dxfer_dir = dir;
	io_hdr.dxfer_len = sizeof(sBuf);
	io_hdr.dxferp = (unsigned char *)&sBuf;  
	io_hdr.cmnd = cdb;
	io_hdr.cmnd_len = sizeof(cdb);
	io_hdr.sensep = sense;  
	io_hdr.max_sense_len = sizeof(sense);
	io_hdr.timeout = SCSI_TIMEOUT_DEFAULT;

	while ( 1 )
	{
		ioctlreturn = do_scsi_cmnd_io(fd, &io_hdr, 0);
		if ( ioctlreturn || io_hdr.scsi_status )
		{
			// errors found
			break;
		}

		if ( arcmsr_cmd != ARCMSR_IOCTL_READ_RQBUFFER )
		{
			// if succeeded, just returns the length of outgoing data
			return data_len;
		}

		if ( sBuf.srbioctl.Length )
		{
			//dumpdata(&sBuf.ioctldatabuffer[0], sBuf.srbioctl.Length);
			memcpy(ptr, &sBuf.ioctldatabuffer[0], sBuf.srbioctl.Length);
			ptr += sBuf.srbioctl.Length;
			total += sBuf.srbioctl.Length;
			// the returned bytes enough to compute payload length ?
			if ( expected < 0 && total >= 5 )
			{
				areca_return_packet = (unsigned char *)&return_buff[0];
				if ( areca_return_packet[0] == 0x5E && 
					 areca_return_packet[1] == 0x01 && 
					 areca_return_packet[2] == 0x61 )
				{
					// valid header, let's compute the returned payload length,
					// we expected the total length is 
					// payload + 3 bytes header + 2 bytes length + 1 byte checksum
					expected = areca_return_packet[4] * 256 + areca_return_packet[3] + 6;
				}
			}

			if ( total >= 7 && total >= expected )
			{
				//printf("total bytes received = %d, expected length = %d\n", total, expected);

				// ------ Okay! we received enough --------
				break;
			}
		}
	}

	// Deal with the different error cases
	if ( ioctlreturn )
	{
		printf("do_scsi_cmnd_io with write buffer failed code = %x\n", ioctlreturn);
		return -2;
	}


	if ( io_hdr.scsi_status )
	{
		printf("io_hdr.scsi_status with write buffer failed code = %x\n", io_hdr.scsi_status);
		return -3;
	}


	if ( data )
	{
		memcpy(data, return_buff, total);
	}

	return total;
}


// Areca RAID Controller
int areca_command_interface(int fd, int disknum, smart_command_set command, int select, char *data)
{
	// ATA input registers
	typedef struct _ATA_INPUT_REGISTERS
	{
		unsigned char features;
		unsigned char sector_count;
		unsigned char sector_number;
		unsigned char cylinder_low; 
		unsigned char cylinder_high;    
		unsigned char device_head;  
		unsigned char command;      
		unsigned char reserved[8];
		unsigned char data[512]; // [in/out] buffer for outgoing/incoming data
	} sATA_INPUT_REGISTERS;

	// ATA output registers
	// Note: The output registers is re-sorted for areca internal use only
	typedef struct _ATA_OUTPUT_REGISTERS
	{
		unsigned char error;
		unsigned char status;
		unsigned char sector_count;
		unsigned char sector_number;
		unsigned char cylinder_low; 
		unsigned char cylinder_high;
	}sATA_OUTPUT_REGISTERS;

	// Areca packet format for outgoing:
	// B[0~2] : 3 bytes header, fixed value 0x5E, 0x01, 0x61
	// B[3~4] : 2 bytes command length + variant data length, little endian
	// B[5]   : 1 bytes areca defined command code, ATA passthrough command code is 0x1c
	// B[6~last-1] : variant bytes payload data
	// B[last] : 1 byte checksum, simply sum(B[3] ~ B[last -1])
	// 
	// 
	//   header 3 bytes  length 2 bytes   cmd 1 byte    payload data x bytes  cs 1 byte 
	// +--------------------------------------------------------------------------------+
	// + 0x5E 0x01 0x61 |   0x00 0x00   |     0x1c   | .................... |   0x00    |
	// +--------------------------------------------------------------------------------+
	// 

	//Areca packet format for incoming:
	// B[0~2] : 3 bytes header, fixed value 0x5E, 0x01, 0x61
	// B[3~4] : 2 bytes payload length, little endian
	// B[5~last-1] : variant bytes returned payload data
	// B[last] : 1 byte checksum, simply sum(B[3] ~ B[last -1])
	// 
	// 
	//   header 3 bytes  length 2 bytes   payload data x bytes  cs 1 byte 
	// +-------------------------------------------------------------------+
	// + 0x5E 0x01 0x61 |   0x00 0x00   | .................... |   0x00    |
	// +-------------------------------------------------------------------+
	unsigned char    areca_packet[640];
	int areca_packet_len = sizeof(areca_packet);
	unsigned char cs = 0;	

	sATA_INPUT_REGISTERS *ata_cmd;

	// For debugging
#if 0
	memset(sInq, 0, sizeof(sInq));
	scsiStdInquiry(fd, (unsigned char *)sInq, (int)sizeof(sInq));
	dumpdata((unsigned char *)sInq, sizeof(sInq));
#endif
	memset(areca_packet, 0, areca_packet_len);

	// ----- BEGIN TO SETUP HEADERS -------
	areca_packet[0] = 0x5E;
	areca_packet[1] = 0x01;
	areca_packet[2] = 0x61;
	areca_packet[3] = (unsigned char)((areca_packet_len - 6) & 0xff);
	areca_packet[4] = (unsigned char)(((areca_packet_len - 6) >> 8) & 0xff);
	areca_packet[5] = 0x1c;	// areca defined code for ATA passthrough command


	// ----- BEGIN TO SETUP PAYLOAD DATA -----

	memcpy(&areca_packet[7], "SmrT", 4);	// areca defined password

	ata_cmd = (sATA_INPUT_REGISTERS *)&areca_packet[12];
	ata_cmd->cylinder_low    = 0x4F;
	ata_cmd->cylinder_high   = 0xC2;


	if ( command == READ_VALUES     ||
		 command == READ_THRESHOLDS ||
		 command == READ_LOG ||
		 command == IDENTIFY ||
		 command == PIDENTIFY )
	{
		// the commands will return data
		areca_packet[6] = 0x13;
		ata_cmd->sector_count = 0x1;
	}
	else if ( command == WRITE_LOG )
	{
		// the commands will write data
		areca_packet[6] = 0x14;
	}
	else
	{
		// the commands will return no data
		areca_packet[6] = 0x15;
	}


	ata_cmd->command = ATA_SMART_CMD;
	// Now set ATA registers depending upon command
	switch ( command )
	{
	case CHECK_POWER_MODE:  
		//printf("command = CHECK_POWER_MODE\n");
		ata_cmd->command = ATA_CHECK_POWER_MODE;        
		break;
	case READ_VALUES:
		//printf("command = READ_VALUES\n");
		ata_cmd->features = ATA_SMART_READ_VALUES;
		break;
	case READ_THRESHOLDS:    
		//printf("command = READ_THRESHOLDS\n");
		ata_cmd->features = ATA_SMART_READ_THRESHOLDS;
		break;
	case READ_LOG: 
		//printf("command = READ_LOG\n");
		ata_cmd->features = ATA_SMART_READ_LOG_SECTOR;
		ata_cmd->sector_number = select;        
		break;
	case WRITE_LOG:        
		//printf("command = WRITE_LOG\n");    
		ata_cmd->features = ATA_SMART_WRITE_LOG_SECTOR;
		memcpy(ata_cmd->data, data, 512);
		ata_cmd->sector_count = 1;
		ata_cmd->sector_number = select;
		break;
	case IDENTIFY:
		//printf("command = IDENTIFY\n");   
		ata_cmd->command = ATA_IDENTIFY_DEVICE;         
		break;
	case PIDENTIFY:
		//printf("command = PIDENTIFY\n");
		errno=ENODEV;
		return -1;
	case ENABLE:
		//printf("command = ENABLE\n");
		ata_cmd->features = ATA_SMART_ENABLE;
		break;
	case DISABLE:
		//printf("command = DISABLE\n");
		ata_cmd->features = ATA_SMART_DISABLE;
		break;
	case AUTO_OFFLINE:
		//printf("command = AUTO_OFFLINE\n");
		ata_cmd->features = ATA_SMART_AUTO_OFFLINE;
		// Enable or disable?
		ata_cmd->sector_count = select;
		break;
	case AUTOSAVE:
		//printf("command = AUTOSAVE\n");
		ata_cmd->features = ATA_SMART_AUTOSAVE;
		// Enable or disable?
		ata_cmd->sector_count = select;
		break;
	case IMMEDIATE_OFFLINE:
		//printf("command = IMMEDIATE_OFFLINE\n");
		ata_cmd->features = ATA_SMART_IMMEDIATE_OFFLINE;
		// What test type to run?
		ata_cmd->sector_number = select;
		break;
	case STATUS_CHECK:
		//printf("command = STATUS_CHECK\n");
		ata_cmd->features = ATA_SMART_STATUS;           
		break;
	case STATUS:
		//printf("command = STATUS\n");
		ata_cmd->features = ATA_SMART_STATUS;       
		break;
	default:
		//printf("command = UNKNOWN\n");
		errno=ENOSYS;
		return -1;
	};

	areca_packet[11] = disknum - 1;		   // drive number

	// ----- BEGIN TO SETUP CHECKSUM -----
	for ( int loop = 3; loop < areca_packet_len - 1; loop++ )
	{
		cs += areca_packet[loop]; 
	}
	areca_packet[areca_packet_len-1] = cs;

	// ----- BEGIN TO SEND TO ARECA DRIVER ------
	int expected = 0;	
	unsigned char return_buff[2048];
	memset(return_buff, 0, sizeof(return_buff));

	expected = arcmsr_command_handler(fd, ARCMSR_IOCTL_CLEAR_RQBUFFER, NULL, 0, NULL);
        if (expected==-3) {
	    find_areca_in_proc(NULL);
	    return -1;
	}

	expected = arcmsr_command_handler(fd, ARCMSR_IOCTL_CLEAR_WQBUFFER, NULL, 0, NULL);    
	expected = arcmsr_command_handler(fd, ARCMSR_IOCTL_WRITE_WQBUFFER, areca_packet, areca_packet_len, NULL); 
	if ( expected > 0 )
	{
		expected = arcmsr_command_handler(fd, ARCMSR_IOCTL_READ_RQBUFFER, return_buff, sizeof(return_buff), NULL);
	}
	if ( expected < 0 )
	{
		return -1;
	}

	// ----- VERIFY THE CHECKSUM -----
	cs = 0;
	for ( int loop = 3; loop < expected - 1; loop++ )
	{
		cs += return_buff[loop]; 
	}

	if ( return_buff[expected - 1] != cs )
	{
		errno = EIO;
		return -1;
	}

	sATA_OUTPUT_REGISTERS *ata_out = (sATA_OUTPUT_REGISTERS *)&return_buff[5] ;
	if ( ata_out->status )
	{
		errno = EIO;
		return -1;
	}

	// returns with data
	if ( command == READ_VALUES     ||
		 command == READ_THRESHOLDS ||
		 command == READ_LOG ||
		 command == IDENTIFY ||
		 command == PIDENTIFY )
	{
		memcpy(data, &return_buff[7], 512); 
	}

	if ( command == CHECK_POWER_MODE )
	{
		data[0] = ata_out->sector_count;
	}

	if ( command == STATUS_CHECK &&
		 ( ata_out->cylinder_low == 0xF4 && ata_out->cylinder_high == 0x2C ) )
	{
		return 1;
	}

	return 0;
}


int marvell_command_interface(int device,
                              smart_command_set command,
                              int select,
                              char *data) {
  typedef struct {
    int  inlen;
    int  outlen;
    char cmd[540];
  } mvsata_scsi_cmd;

  int copydata = 0;
  mvsata_scsi_cmd  smart_command;
  unsigned char *buff = (unsigned char *)&smart_command.cmd[6];
  // See struct hd_drive_cmd_hdr in hdreg.h
  // buff[0]: ATA COMMAND CODE REGISTER
  // buff[1]: ATA SECTOR NUMBER REGISTER
  // buff[2]: ATA FEATURES REGISTER
  // buff[3]: ATA SECTOR COUNT REGISTER

  // clear out buff.  Large enough for HDIO_DRIVE_CMD (4+512 bytes)
  memset(&smart_command, 0, sizeof(smart_command));
  smart_command.inlen = 540;
  smart_command.outlen = 540;
  smart_command.cmd[0] = 0xC;  //Vendor-specific code
  smart_command.cmd[4] = 6;     //command length

  buff[0] = ATA_SMART_CMD;
  switch (command){
  case CHECK_POWER_MODE:
    buff[0]=ATA_CHECK_POWER_MODE;
    break;
  case READ_VALUES:
    buff[2]=ATA_SMART_READ_VALUES;
    copydata=buff[3]=1;
    break;
  case READ_THRESHOLDS:
    buff[2]=ATA_SMART_READ_THRESHOLDS;
    copydata=buff[1]=buff[3]=1;
    break;
  case READ_LOG:
    buff[2]=ATA_SMART_READ_LOG_SECTOR;
    buff[1]=select;
    copydata=buff[3]=1;
    break;
  case IDENTIFY:
    buff[0]=ATA_IDENTIFY_DEVICE;
    copydata=buff[3]=1;
    break;
  case PIDENTIFY:
    buff[0]=ATA_IDENTIFY_PACKET_DEVICE;
    copydata=buff[3]=1;
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
  case STATUS_CHECK:
    // this command only says if SMART is working.  It could be
    // replaced with STATUS_CHECK below.
    buff[2] = ATA_SMART_STATUS;
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
  default:
    pout("Unrecognized command %d in mvsata_os_specific_handler()\n", command);
    EXIT(1);
    break;
  }
  // There are two different types of ioctls().  The HDIO_DRIVE_TASK
  // one is this:
  // We are now doing the HDIO_DRIVE_CMD type ioctl.
  if (ioctl(device, SCSI_IOCTL_SEND_COMMAND, (void *)&smart_command))
      return -1;

  if (command==CHECK_POWER_MODE) {
    // LEON -- CHECK THIS PLEASE.  THIS SHOULD BE THE SECTOR COUNT
    // REGISTER, AND IT MIGHT BE buff[2] NOT buff[3].  Bruce
    data[0]=buff[3];
    return 0;
  }

  // Always succeed on a SMART status, as a disk that failed returned
  // buff[4]=0xF4, buff[5]=0x2C, i.e. "Bad SMART status" (see below).
  if (command == STATUS)
    return 0;
  //Data returned is starting from 0 offset
  if (command == STATUS_CHECK)
  {
    // Cyl low and Cyl high unchanged means "Good SMART status"
    if (buff[4] == 0x4F && buff[5] == 0xC2)
      return 0;
    // These values mean "Bad SMART status"
    if (buff[4] == 0xF4 && buff[5] == 0x2C)
      return 1;
    // We haven't gotten output that makes sense; print out some debugging info
    syserror("Error SMART Status command failed");
    pout("Please get assistance from %s\n",PACKAGE_BUGREPORT);
    pout("Register values returned from SMART Status command are:\n");
    pout("CMD =0x%02x\n",(int)buff[0]);
    pout("FR =0x%02x\n",(int)buff[1]);
    pout("NS =0x%02x\n",(int)buff[2]);
    pout("SC =0x%02x\n",(int)buff[3]);
    pout("CL =0x%02x\n",(int)buff[4]);
    pout("CH =0x%02x\n",(int)buff[5]);
    pout("SEL=0x%02x\n",(int)buff[6]);
    return -1;
  }

  if (copydata)
    memcpy(data, buff, 512);
  return 0;
}

// this implementation is derived from ata_command_interface with a header
// packing for highpoint linux driver ioctl interface
//
// ioctl(fd,HPTIO_CTL,buff)
//          ^^^^^^^^^
//
// structure of hpt_buff
// +----+----+----+----+--------------------.....---------------------+
// | 1  | 2  | 3  | 4  | 5                                            |
// +----+----+----+----+--------------------.....---------------------+
//
// 1: The target controller                     [ int    ( 4 Bytes ) ]
// 2: The channel of the target controllee      [ int    ( 4 Bytes ) ]
// 3: HDIO_ ioctl call                          [ int    ( 4 Bytes ) ]
//    available from ${LINUX_KERNEL_SOURCE}/Documentation/ioctl/hdio
// 4: the pmport that disk attached,            [ int    ( 4 Bytes ) ]
//    if no pmport device, set to 1 or leave blank
// 5: data                                      [ void * ( var leangth ) ]
//
#define STRANGE_BUFFER_LENGTH (4+512*0xf8)

int highpoint_command_interface(int device, smart_command_set command,
                                int select, char *data)


{
  unsigned char hpt_buff[4*sizeof(int) + STRANGE_BUFFER_LENGTH];
  unsigned int *hpt = (unsigned int *)hpt_buff;
  unsigned char *buff = &hpt_buff[4*sizeof(int)];
  int copydata = 0;
  const int HDIO_DRIVE_CMD_OFFSET = 4;

  memset(hpt_buff, 0, 4*sizeof(int) + STRANGE_BUFFER_LENGTH);
  hpt[0] = con->hpt_data[0]; // controller id
  hpt[1] = con->hpt_data[1]; // channel number
  hpt[3] = con->hpt_data[2]; // pmport number

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
  case WRITE_LOG:
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
    buff[2]=ATA_SMART_STATUS;
    break;
  case AUTO_OFFLINE:
    buff[2]=ATA_SMART_AUTO_OFFLINE;
    buff[3]=select;
    break;
  case AUTOSAVE:
    buff[2]=ATA_SMART_AUTOSAVE;
    buff[3]=select;
    break;
  case IMMEDIATE_OFFLINE:
    buff[2]=ATA_SMART_IMMEDIATE_OFFLINE;
    buff[1]=select;
    break;
  case STATUS_CHECK:
    buff[1]=ATA_SMART_STATUS;
    break;
  default:
    pout("Unrecognized command %d in linux_highpoint_command_interface()\n"
         "Please contact " PACKAGE_BUGREPORT "\n", command);
    errno=ENOSYS;
    return -1;
  }

  if (command==WRITE_LOG) {
    unsigned char task[4*sizeof(int)+sizeof(ide_task_request_t)+512];
    unsigned int *hpt = (unsigned int *)task;
    ide_task_request_t *reqtask = (ide_task_request_t *)(&task[4*sizeof(int)]);
    task_struct_t *taskfile = (task_struct_t *)reqtask->io_ports;
    int retval;

    memset(task, 0, sizeof(task));

    hpt[0] = con->hpt_data[0]; // controller id
    hpt[1] = con->hpt_data[1]; // channel number
    hpt[3] = con->hpt_data[2]; // pmport number
    hpt[2] = HDIO_DRIVE_TASKFILE; // real hd ioctl

    taskfile->data           = 0;
    taskfile->feature        = ATA_SMART_WRITE_LOG_SECTOR;
    taskfile->sector_count   = 1;
    taskfile->sector_number  = select;
    taskfile->low_cylinder   = 0x4f;
    taskfile->high_cylinder  = 0xc2;
    taskfile->device_head    = 0;
    taskfile->command        = ATA_SMART_CMD;

    reqtask->data_phase      = TASKFILE_OUT;
    reqtask->req_cmd         = IDE_DRIVE_TASK_OUT;
    reqtask->out_size        = 512;
    reqtask->in_size         = 0;

    memcpy(task+sizeof(ide_task_request_t)+4*sizeof(int), data, 512);

    if ((retval=ioctl(device, HPTIO_CTL, task))) {
      if (retval==-EINVAL)
        pout("Kernel lacks HDIO_DRIVE_TASKFILE support; compile kernel with CONFIG_IDE_TASKFILE_IO set\n");
      return -1;
    }
    return 0;
  }

  if (command==STATUS_CHECK){
    int retval;
    unsigned const char normal_lo=0x4f, normal_hi=0xc2;
    unsigned const char failed_lo=0xf4, failed_hi=0x2c;
    buff[4]=normal_lo;
    buff[5]=normal_hi;

    hpt[2] = HDIO_DRIVE_TASK;

    if ((retval=ioctl(device, HPTIO_CTL, hpt_buff))) {
      if (retval==-EINVAL) {
        pout("Error SMART Status command via HDIO_DRIVE_TASK failed");
        pout("Rebuild older linux 2.2 kernels with HDIO_DRIVE_TASK support added\n");
      }
      else
        syserror("Error SMART Status command failed");
      return -1;
    }

    if (buff[4]==normal_lo && buff[5]==normal_hi)
      return 0;

    if (buff[4]==failed_lo && buff[5]==failed_hi)
      return 1;

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
  if (command==IDENTIFY || command==PIDENTIFY) {
    unsigned char deviceid[4*sizeof(int)+512*sizeof(char)];
    unsigned int *hpt = (unsigned int *)deviceid;

    hpt[0] = con->hpt_data[0]; // controller id
    hpt[1] = con->hpt_data[1]; // channel number
    hpt[3] = con->hpt_data[2]; // pmport number

    hpt[2] = HDIO_GET_IDENTITY;
    if (!ioctl(device, HPTIO_CTL, deviceid) && (deviceid[4*sizeof(int)] & 0x8000))
      buff[0]=(command==IDENTIFY)?ATA_IDENTIFY_PACKET_DEVICE:ATA_IDENTIFY_DEVICE;
  }
#endif

  hpt[2] = HDIO_DRIVE_CMD;
  if ((ioctl(device, HPTIO_CTL, hpt_buff)))
    return -1;

  if (command==CHECK_POWER_MODE)
    buff[HDIO_DRIVE_CMD_OFFSET]=buff[2];

  if (copydata)
    memcpy(data, buff+HDIO_DRIVE_CMD_OFFSET, copydata);

  return 0;
}


// Utility function for printing warnings
void printwarning(smart_command_set command){
  static int printed[4]={0,0,0,0};
  const char* message=
    "can not be passed through the 3ware 3w-xxxx driver.  This can be fixed by\n"
    "applying a simple 3w-xxxx driver patch that can be found here:\n"
    PACKAGE_HOMEPAGE "\n"
    "Alternatively, upgrade your 3w-xxxx driver to version 1.02.00.037 or greater.\n\n";

  if (command==AUTO_OFFLINE && !printed[0]) {
    printed[0]=1;
    pout("The SMART AUTO-OFFLINE ENABLE command (smartmontools -o on option/Directive)\n%s", message);
  }
  else if (command==AUTOSAVE && !printed[1]) {
    printed[1]=1;
    pout("The SMART AUTOSAVE ENABLE command (smartmontools -S on option/Directive)\n%s", message);
  }
  else if (command==STATUS_CHECK && !printed[2]) {
    printed[2]=1;
    pout("The SMART RETURN STATUS return value (smartmontools -H option/Directive)\n%s", message);
  }
  else if (command==WRITE_LOG && !printed[3])  {
    printed[3]=1;
    pout("The SMART WRITE LOG command (smartmontools -t selective) only supported via char /dev/tw[ae] interface\n");
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
static const char * lin_dev_3ware_9000_char = "twa";
static const char * lin_dev_3ware_678k_char = "twe";
static const char * lin_dev_cciss_dir = "cciss/";
static const char * lin_dev_areca = "sg";

int guess_device_type(const char * dev_name) {
  int len;
  int dev_prefix_len = strlen(lin_dev_prefix);

  // if dev_name null, or string length zero
  if (!dev_name || !(len = strlen(dev_name)))
    return CONTROLLER_UNKNOWN;

  // Remove the leading /dev/... if it's there
  if (!strncmp(lin_dev_prefix, dev_name, dev_prefix_len)) {
    if (len <= dev_prefix_len)
      // if nothing else in the string, unrecognized
      return CONTROLLER_UNKNOWN;
    // else advance pointer to following characters
    dev_name += dev_prefix_len;
  }

  // form /dev/h* or h*
  if (!strncmp(lin_dev_ata_disk_plus, dev_name,
               strlen(lin_dev_ata_disk_plus)))
    return CONTROLLER_ATA;

  // form /dev/ide/* or ide/*
  if (!strncmp(lin_dev_ata_devfs_disk_plus, dev_name,
               strlen(lin_dev_ata_devfs_disk_plus)))
    return CONTROLLER_ATA;

  // form /dev/s* or s*
  if (!strncmp(lin_dev_scsi_disk_plus, dev_name,
               strlen(lin_dev_scsi_disk_plus)))
    return CONTROLLER_SCSI;

  // form /dev/scsi/* or scsi/*
  if (!strncmp(lin_dev_scsi_devfs_disk_plus, dev_name,
               strlen(lin_dev_scsi_devfs_disk_plus)))
    return CONTROLLER_SCSI;

  // form /dev/ns* or ns*
  if (!strncmp(lin_dev_scsi_tape1, dev_name,
               strlen(lin_dev_scsi_tape1)))
    return CONTROLLER_SCSI;

  // form /dev/os* or os*
  if (!strncmp(lin_dev_scsi_tape2, dev_name,
               strlen(lin_dev_scsi_tape2)))
    return CONTROLLER_SCSI;

  // form /dev/nos* or nos*
  if (!strncmp(lin_dev_scsi_tape3, dev_name,
               strlen(lin_dev_scsi_tape3)))
    return CONTROLLER_SCSI;

  // form /dev/twa*
  if (!strncmp(lin_dev_3ware_9000_char, dev_name,
               strlen(lin_dev_3ware_9000_char)))
    return CONTROLLER_3WARE_9000_CHAR;

  // form /dev/twe*
  if (!strncmp(lin_dev_3ware_678k_char, dev_name,
               strlen(lin_dev_3ware_678k_char)))
    return CONTROLLER_3WARE_678K_CHAR;
  // form /dev/cciss*
  if (!strncmp(lin_dev_cciss_dir, dev_name,
               strlen(lin_dev_cciss_dir)))
    return CONTROLLER_CCISS;
  // form /dev/sg*
  if ( !strncmp(lin_dev_areca, dev_name,
		strlen(lin_dev_areca)) )
      return CONTROLLER_ARECA;

  // we failed to recognize any of the forms
  return CONTROLLER_UNKNOWN;
}
