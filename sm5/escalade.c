/* 
 *  escalade.c
 * 
 * Home page of code is: http://smartmontools.sourceforge.net
 *
 * Copyright (C) 2003 Bruce Allen <smartmontools-support@lists.sourceforge.net>
 *
 * Derived from code that was
 *
 *  Written By: Adam Radford <linux@3ware.com>
 *  Modifications By: Joel Jacobson <linux@3ware.com>
 *  		     Arnaldo Carvalho de Melo <acme@conectiva.com.br>
 *                    Brad Strand <linux@3ware.com>
 *
 *  Copyright (C) 1999-2003 3ware Inc.
 *
 *  Kernel compatablity By:	Andre Hedrick <andre@suse.com>
 *  Non-Copyright (C) 2000	Andre Hedrick <andre@suse.com>
 *
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

#include <string.h>
#include <scsi/scsi_ioctl.h>
#include <errno.h>
#include "atacmds.h"
#include "escalade.h"
#include "utility.h"

const char *escalade_c_cvsid="$Id: escalade.c,v 1.6 2003/08/05 10:07:35 ballen4705 Exp $" ATACMDS_H_CVSID ESCALADE_H_CVSID UTILITY_H_CVSID;

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


int linux_3ware_command_interface(int fd, int disknum, smart_command_set command, int select, char *data){

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
  passthru.sector_num  = 0;

  // All SMART commands use this CL/CH signature
  passthru.cylinder_lo = 0x4F;
  passthru.cylinder_hi = 0xC2;
  
  // SMART ATA COMMAND REGISTER value
  passthru.command = 0xB0;
  
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
    passthru.byte0.sgloff = 0x0;
    passthru.size         = 0x5;
    passthru.param        = 0x8;
    passthru.sector_count = 0x0;
  }

  switch (command){
  case READ_VALUES:
    passthru.features = 0xD0;  // SMART READ DATA
    break;
  case READ_THRESHOLDS:
    passthru.features = 0xD1;  // SMART READ ATTRIBUTE THRESHOLDS
    break;
  case READ_LOG:
    passthru.features = 0xD5;      // SMART READ LOG
    passthru.sector_num  = select; // What log to return?
    break;
  case IDENTIFY:
    passthru.command     = 0xEc;   // ATA IDENTIFY DEVICE
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
    passthru.features = 0xD8;  // SMART ENABLE OPERATIONS
    break;
  case DISABLE:
    passthru.features = 0xD9;  // SMART DISABLE OPERATIONS
    break;
  case AUTO_OFFLINE:
    // NON-DATA COMMAND FROM THE SFF-8035i SPECIFICATIONS. To enable
    // requires a sector count value of 0xF8.
    passthru.features = 0xDB;       // SMART AUTO OFFLINE
    passthru.sector_count = select; // Enable or disable?
    if (select){
      pout("WARNING - SMART AUTO OFFLINE ENABLE NOT IMPLEMENTED FOR 3WARE CONTROLLER (disk %d)\n", disknum);
      errno=ENOSYS;
      return -1;
    }
    break;
  case AUTOSAVE:
    // NOTE -- this is a NON-DATA COMMAND - same as above. To enable
    // requires a sector count value of 0xF1.
    passthru.features = 0xD2;        // SMART AUTOSAVE
    passthru.sector_count = select; // Enable or disable?
    if (select){
      pout("WARNING - SMART AUTOSAVE ENABLE NOT YET IMPLEMENTED FOR 3WARE CONTROLLER (disk %d)\n", disknum);
      errno=ENOSYS;
      return -1;
    }
    break;
  case IMMEDIATE_OFFLINE:
    passthru.features = 0xD4;      //SMART EXECUTE OFF-LINE IMMEDIATE
    passthru.sector_num  = select; // What test type to run?
    break;
  case STATUS_CHECK:
    passthru.features = 0xDA;      //SMART RETURN STATUS
    break;
  case STATUS:
    // This is JUST to see if SMART is enabled, by giving SMART status
    // command. But it doesn't say if status was good, or failing.
    passthru.features = 0xDA;      //SMART RETURN STATUS
    break;
  default:
    pout("Unrecognized command %d in linux_3ware_command_interface(disk %d)\n", command, disknum);
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
    return -1;
  }

#if (0)
  pout("Registers are CL=0x%02u CH=0x%02u\n", 
       (unsigned)ioctlbuf.parameter_size_bytes,
       (unsigned)ioctlbuf.input_data[0]);
#endif

  if (readdata){
    TW_Output *tw_output;
    tw_output = (TW_Output *)&ioctlbuf;
    memcpy(data, tw_output->output_data, 512);
  }

  if (command!=STATUS_CHECK)
    return 0;

  // STATUS CHECK requires looking at CYL-LO and CYL-HI.  These seem to be in:
  if (ioctlbuf.parameter_size_bytes==0x4F && ioctlbuf.input_data[0]==0xC2)
    return 0;
  
  if (ioctlbuf.parameter_size_bytes==0xF4 && ioctlbuf.input_data[0]==0x2C)
    return 1;
  
  // NOT YET IMPLEMENTED -- SHOULD BE SMART RETURN STATUS
  pout("WARNING - SMART CHECK STATUS NOT YET IMPLEMENTED FOR 3WARE CONTROLLER (disk %d)\n", disknum);
  errno=ENOSYS;
  return -1;
}

