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

const char *escalade_c_cvsid="$Id: escalade.c,v 1.11 2003/08/14 13:33:06 ballen4705 Exp $" ATACMDS_H_CVSID ESCALADE_H_CVSID UTILITY_H_CVSID;
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

  // All SMART commands use this CL/CH signature.  These are magic
  // values from the ATA specifications.
  passthru.cylinder_lo = 0x4F;
  passthru.cylinder_hi = 0xC2;
  
  // SMART ATA COMMAND REGISTER value
  passthru.command = WIN_SMART;
  
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
  case READ_VALUES:
    passthru.features = SMART_READ_VALUES;
    break;
  case READ_THRESHOLDS:
    passthru.features = SMART_READ_THRESHOLDS;
    break;
  case READ_LOG:
    passthru.features = SMART_READ_LOG_SECTOR;
    // log number to return
    passthru.sector_num  = select;
    break;
  case IDENTIFY:
    // ATA IDENTIFY DEVICE
    passthru.command     = 0xEc;
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
    passthru.features = SMART_ENABLE;
    break;
  case DISABLE:
    passthru.features = SMART_DISABLE;
    break;
  case AUTO_OFFLINE:
    passthru.features = SMART_AUTO_OFFLINE;
    // Enable or disable?
    passthru.sector_count = select;
    break;
  case AUTOSAVE:
    passthru.features = SMART_AUTOSAVE;
    // Enable or disable?
    passthru.sector_count = select;
    break;
  case IMMEDIATE_OFFLINE:
    passthru.features = SMART_IMMEDIATE_OFFLINE;
    // What test type to run?
    passthru.sector_num  = select;
    break;
  case STATUS_CHECK:
    passthru.features = SMART_STATUS;
    break;
  case STATUS:
    // This is JUST to see if SMART is enabled, by giving SMART status
    // command. But it doesn't say if status was good, or failing.
    // See below for the difference.
    passthru.features = SMART_STATUS;
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
  
  // We are finished with all commands except for STATUS_CHECK
  if (command!=STATUS_CHECK) {
    return 0;
  }
  else {

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
}

// Utility function for printing warnings
void printwarning(smart_command_set command){
  static int printed1=0,printed2=0,printed3=0;
  const char* message=
    "can not be passed through the 3ware 3w-xxxx driver.  This can be fixed by\n"
    "applying a simple 3w-xxxx driver patch that can be found here:\n"
    PROJECTHOME "\n"
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

