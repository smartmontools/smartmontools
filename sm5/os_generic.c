/*
 * os_generic.c
 *
 * Home page of code is: http://smartmontools.sourceforge.net
 *
 * Copyright (C) 2003 NAME HERE <smartmontools-support@lists.sourceforge.net>
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

/*  PORTING NOTES AND COMMENTS

    To port smartmontools to the OS of your choice, you need to:

    [0] Contact smartmontools-support@lists.sourceforge.net to check
    that it's not already been done.

    [1] Make copies of os_generic.[hc] called os_myOS.[hc]

    [2] Modify configure.in so that case "${host}" include myOS

    [3] Verify that ./autogen.sh && ./configure && make compiles the
    code.  If not, fix any compilation problems.

    [4] Provide the functions defined in this file: flesh out the
    skeletons below. Note that for Darwin much of this already
    exists. See some partially developed but incomplete code at:
    http://cvs.sourceforge.net/viewcvs.py/smartmontools/sm5_Darwin/.
    You can entirely eliminate the function 'unsupported()'.

    [5] Contact smartmontools-support@lists.sourceforge.net to see
    about checking your code into the smartmontools CVS archive.

*/

// These are needed to define prototypes for the functions defined below
#include "atacmds.h"
#include "scsicmds.h"
#include "utility.h"

// This is to include whatever prototypes you define in os_generic.h
#include "os_generic.h"

// This is only needed for the 'unsupported()' function.  You can eliminate it.
#include <sys/utsname.h>

// Needed by '-V' option (CVS versioning) of smartd/smartctl
const char *os_XXXX_c_cvsid="$Id: os_generic.c,v 1.5 2003/11/23 06:57:11 ballen4705 Exp $" \
ATACMDS_H_CVSID OS_XXXX_H_CVSID SCSICMDS_H_CVSID UTILITY_H_CVSID;

// You can eliminate this function - it is only here to warn
// unsuspectig users that their Operating System is not supported!
static void unsupported(){
  static int warninggiven;
  extern unsigned char debugmode;

  if (!warninggiven) {
    struct utsname ostype;
    unsigned char savedebugmode=debugmode;
    
    debugmode=1;
    warninggiven=1;
    uname(&ostype);

    pout("\n"
	 "############################################################################\n"
	 "WARNING: smartmontools has not been ported to the %s Operating System.\n"
	 "Please see the files os_generic.h and os_generic.c for porting instructions.\n"
	 "############################################################################\n\n",
	 ostype.sysname);

    debugmode=savedebugmode;
  }
  
  exit(1);
}

// tries to guess device type given the name (a path).  See utility.h
// for return values.
int guess_device_type (const char* dev_name) {
  unsupported();
  return GUESS_DEVTYPE_DONT_KNOW;
}

// makes a list of ATA or SCSI devices for the DEVICESCAN directive of
// smartd.  Returns number N of devices, or -1 if out of
// memory. Allocates N+1 arrays: one of N pointers (devlist), the
// others each contain null-terminated character strings.
int make_device_names (char*** devlist, const char* name) {
  unsupported();
  return 0;
}

// Like open().  Return positive integer handle, only used by
// functions below.  type="ATA" or "SCSI".  If you need to store extra
// information about your devices, create a private internal array
// within this file (see os_freebsd.c for an example).
int deviceopen(const char *pathname, char *type){
  unsupported();
  return -1;
}

// Like close().  Acts only on handles returned by above function.
int deviceclose(int fd){
  unsupported();
  return 0;
}

// Interface to ATA devices.  See os_linux.c
int ata_command_interface(int fd, smart_command_set command, int select, char *data){
  unsupported();
  return -1;
}

// Interface to ATA devices behind 3ware escalade RAID controller cards.  See os_linux.c
int escalade_command_interface(int fd, int disknum, smart_command_set command, int select, char *data){
  unsupported();
  return -1;
}

#include <errno.h>
// Interface to SCSI devices.  See os_linux.c
int do_scsi_cmnd_io(int fd, struct scsi_cmnd_io * iop, int report) {
  unsupported();
  return -ENOSYS;
}
