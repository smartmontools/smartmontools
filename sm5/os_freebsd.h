/*
 * os_freebsd.h
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

#ifndef OS_FREEBSD_H_
#define OS_FREEBSD_H_

#define OS_XXXX_H_CVSID "$Id: os_freebsd.h,v 1.10 2004/08/13 13:57:12 arvoreen Exp $\n"

struct freebsd_dev_channel {
  int   channel;                // the ATA channel to work with
  int   device;                 // the device on the channel
  int   atacommand;             // the ATA Command file descriptor (/dev/ata)
  char* devname;                // the SCSI device name
  int   unitnum;                // the SCSI unit number
  int   scsicontrol;            // the SCSI control interface
};

#define FREEBSD_MAXDEV 64
#define FREEBSD_FDOFFSET 16;
#define MAX_NUM_DEV 26

#include "twereg.h"
#include "tweio.h"
/* 
   The following definitions/macros/prototypes are used for three
   different interfaces, referred to as "the three cases" below.
   THREE_WARE_678K      -- 6000, 7000, and 8000 controllers via /dev/sd?
   THREE_WARE_678K_CHAR -- 6000, 7000, and 8000 controllers via /dev/twe?
   THREE_WARE_9000_CHAR -- 9000 controllers via /dev/twa?
*/


#endif /* OS_FREEBSD_H_ */
