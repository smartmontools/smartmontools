/*
 * os_solaris.c
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

// These are needed to define prototypes for the functions defined below
#include "atacmds.h"
#include "scsicmds.h"
#include "utility.h"

// This is to include whatever prototypes you define in os_solaris.h
#include "os_solaris.h"

const char *os_XXXX_c_cvsid="$Id: os_solaris.c,v 1.2 2003/10/12 09:10:03 ballen4705 Exp $" \
ATACMDS_H_CVSID OS_XXXX_H_CVSID SCSICMDS_H_CVSID UTILITY_H_CVSID;

// Like open().  Return positive integer handle, used by functions below only.  type="ATA" or "SCSI".
int deviceopen(const char *pathname, char *type){
  return -1;
}

// Like close().  Acts on handles returned by above function.
int deviceclose(int fd){
  return 0;
}

// tries to guess device type given the name (a path)
int guess_device_type (const char* dev_name) {
    return GUESS_DEVTYPE_DONT_KNOW;
}

// makes a list of ATA or SCSI devices for the DEVICESCAN directive of
// smartd.  Returns number of devices, or -1 if out of memory.
int make_device_names (char*** devlist, const char* name) {
  return 0;
}

// Interface to ATA devices.  See os_linux.c
int ata_command_interface(int fd, smart_command_set command, int select, char *data){
  return -1;
}

// Interface to ATA devices behind 3ware escalade RAID controller cards.  See os_linux.c
int escalade_command_interface(int fd, int disknum, smart_command_set command, int select, char *data){
  return -1;
}

#include <errno.h>
// Interface to SCSI devices.  See os_linux.c
int do_scsi_cmnd_io(int fd, struct scsi_cmnd_io * iop, int report) {
  return -ENOSYS;
}
