/*
 * os_solaris.c
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

/*
  If you want to do a solaris port, some sample code, showing how to
  access SCSI data under solaris, can be found here:
  http://groups.google.com/groups?hl=en&lr=&ie=UTF-8&oe=UTF-8&selm=2003721.204932.21807%40cable.prodigy.com
  Please contact the smartmontools developers at:
  smartmontools-support@lists.sourceforge.net
*/

// These are needed to define prototypes for the functions defined below
#include "atacmds.h"
#include "scsicmds.h"
#include "utility.h"

// This is to include whatever prototypes you define in os_solaris.h
#include "os_solaris.h"

const char *os_XXXX_c_cvsid="$Id: os_solaris.c,v 1.5 2003/10/14 13:09:06 ballen4705 Exp $" \
ATACMDS_H_CVSID OS_XXXX_H_CVSID SCSICMDS_H_CVSID UTILITY_H_CVSID;

// The printwarning() function warns about unimplemented functions
int printedout[5];
char *unimplemented[5]={
  "guess_device_type()",
  "make_device_names()",
  "ATA command routine ata_command_interface",
  "3ware Escalade Controller command routine escalade_command_interface",
  "SCSI command interface do_scsi_cmnd_io"
};

int printwarning(int which){
  if (!unimplemented[which])
    return 0;

  if (printedout[which])
    return 1;
  
  printedout[which]=1;
  
  pout("\n"
       "#######################################################################\n"
       "%s NOT IMPLEMENTED under Solaris.\n"
       "Please contact smartmontools-support@lists.sourceforge.net if\n"
       "you want to help in porting smartmontools to Solaris.\n"
       "#######################################################################\n"
       "\n",
       unimplemented[which]);

  return 1;
}

// Like open().  Return integer handle, used by functions below only.
// type="ATA" or "SCSI".
int deviceopen(const char *pathname, char *type){
  if (!strcmp(type,"SCSI")) 
    return open(pathname, O_RDWR | O_NONBLOCK);
  else if (!strcmp(type,"ATA")) 
    return open(pathname, O_RDONLY | O_NONBLOCK);
  else
    return -1;
}

// Like close().  Acts on handles returned by above function.
int deviceclose(int fd){
    return close(fd);
}

// tries to guess device type given the name (a path)
int guess_device_type (const char* dev_name) {
  if (printwarning(0))
    return GUESS_DEVTYPE_DONT_KNOW;
  return GUESS_DEVTYPE_DONT_KNOW;
}

// makes a list of ATA or SCSI devices for the DEVICESCAN directive of
// smartd.  Returns number of devices, or -1 if out of memory.
int make_device_names (char*** devlist, const char* name) {
  if (printwarning(1))
    return 0;
  return 0;
}

// Interface to ATA devices.  See os_linux.c
int ata_command_interface(int fd, smart_command_set command, int select, char *data){
  if (printwarning(2))
    return -1;
  return -1;
}

// Interface to ATA devices behind 3ware escalade RAID controller cards.  See os_linux.c
int escalade_command_interface(int fd, int disknum, smart_command_set command, int select, char *data){
  if (printwarning(3))
    return -1;
  return -1;
}

#include <errno.h>
// Interface to SCSI devices.  See os_linux.c
int do_scsi_cmnd_io(int fd, struct scsi_cmnd_io * iop, int report) {
  if (printwarning(4))
    return -ENOSYS;
  return -ENOSYS;
}
