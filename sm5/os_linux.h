/* 
 *  os_linux.h
 * 
 * Home page of code is: http://smartmontools.sourceforge.net
 *
 * Copyright (C) 2003-4 Bruce Allen <smartmontools-support@lists.sourceforge.net>
 *
 * Derived from code that was
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

#ifndef OS_LINUX_H_
#define OS_LINUX_H_

#define OS_XXXX_H_CVSID "$Id: os_linux.h,v 1.9 2004/02/06 03:52:02 ballen4705 Exp $\n"

/* Misc defines */
#define TW_IOCTL            0x80
#define TW_ATA_PASSTHRU     0x1e  /* Needed for the driver */
#define TW_MAX_SGL_LENGTH   62

/* Scatter gather list entry */
typedef struct TAG_TW_SG_Entry
{
  unsigned int address;
  unsigned int length;
} TW_SG_Entry;

/* Command header for ATA pass-thru */
// Total size of structure is 528 bytes
typedef struct TAG_TW_Passthru {
  struct {
    unsigned char opcode:5;
    unsigned char sgloff:3;
  } byte0;
  unsigned char size;
  unsigned char request_id;
  struct { 
    unsigned char aport:4;
    unsigned char host_id:4;
  } byte3;
  unsigned char status;
  unsigned char flags;
  unsigned short param;
  unsigned short features;
  unsigned short sector_count;
  unsigned short sector_num;
  unsigned short cylinder_lo;
  unsigned short cylinder_hi;
  unsigned char drive_head;
  unsigned char command;
  TW_SG_Entry sg_list[TW_MAX_SGL_LENGTH];
  unsigned char padding[12];
} TW_Passthru;

/* Ioctl buffer */
// Note that this defn has changed in kernel tree...
// Total size is 1041 bytes
typedef struct TAG_TW_Ioctl { 
  int input_length;
  int output_length;
  unsigned char cdb[16];
  unsigned char opcode;
  unsigned short table_id;
  unsigned char parameter_id;
  unsigned char parameter_size_bytes;
  unsigned char unit_index;
  // Size up to here is 30 bytes
  // CHECK ME -- is this RIGHT??
  unsigned char input_data[499];
  // Reserve lots of extra space for commands that set Sector Count
  // register to large values
  unsigned char output_data[512];
} TW_Ioctl;

/* Ioctl buffer output */
typedef struct TAG_TW_Output {
  // CHECKME - is padding right on machines with 8-byte INTEGERS??
  int padding[2];
  char output_data[512];
} TW_Output; 

#endif /* OS_LINUX_H_ */
