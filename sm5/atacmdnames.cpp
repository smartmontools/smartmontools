/*
 * atacmdnames.cpp
 *
 * This module is based on the T13/1532D Volume 1 Revision 3 (ATA/ATAPI-7)
 * specification, which is available from http://www.t13.org/#FTP_site
 *
 * Home page of code is: http://smartmontools.sourceforge.net
 * Address of support mailing list: smartmontools-support@lists.sourceforge.net
 *
 * Copyright (C) 2003-6 Philip Williams
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
 */

#include "atacmdnames.h"
#include <stdlib.h>
#include <stdio.h>

#define COMMAND_TABLE_SIZE 256

const char *atacmdnames_c_cvsid="$Id: atacmdnames.cpp,v 1.14 2006/08/09 20:40:19 chrfranke Exp $" ATACMDNAMES_H_CVSID;

const char cmd_reserved[]        = "[RESERVED]";
const char cmd_vendor_specific[] = "[VENDOR SPECIFIC]";
const char cmd_reserved_sa[]     = "[RESERVED FOR SERIAL ATA]";
const char cmd_reserved_cf[]     = "[RESERVED FOR COMPACTFLASH ASSOCIATION]";
const char cmd_reserved_mcpt[]   = "[RESERVED FOR MEDIA CARD PASS THROUGH]";
const char cmd_recalibrate_ret4[]= "RECALIBRATE [RET-4]";
const char cmd_seek_ret4[]       = "SEEK [RET-4]";

const char *command_table[COMMAND_TABLE_SIZE] = {
/*-------------------------------------------------- 00h-0Fh -----*/
  "NOP",
  cmd_reserved,
  cmd_reserved,
  "CFA REQUEST EXTENDED ERROR CODE",
  cmd_reserved,
  cmd_reserved,
  cmd_reserved,
  cmd_reserved,
  "DEVICE RESET",
  cmd_reserved,
  cmd_reserved,
  cmd_reserved,
  cmd_reserved,
  cmd_reserved,
  cmd_reserved,
  cmd_reserved,
/*-------------------------------------------------- 10h-1Fh -----*/
  "RECALIBRATE [OBS-4]",
  cmd_recalibrate_ret4,
  cmd_recalibrate_ret4,
  cmd_recalibrate_ret4,
  cmd_recalibrate_ret4,
  cmd_recalibrate_ret4,
  cmd_recalibrate_ret4,
  cmd_recalibrate_ret4,
  cmd_recalibrate_ret4,
  cmd_recalibrate_ret4,
  cmd_recalibrate_ret4,
  cmd_recalibrate_ret4,
  cmd_recalibrate_ret4,
  cmd_recalibrate_ret4,
  cmd_recalibrate_ret4,
  cmd_recalibrate_ret4,
/*-------------------------------------------------- 20h-2Fh -----*/
  "READ SECTOR(S)",
  "READ SECTOR(S) [OBS-5]",
  "READ LONG (w/ retry) [OBS-4]",
  "READ LONG (w/o retry) [OBS-4]",
  "READ SECTOR(S) EXT",
  "READ DMA EXT",
  "READ DMA QUEUED EXT",
  "READ NATIVE MAX ADDRESS EXT",
  cmd_reserved,
  "READ MULTIPLE EXT",
  "READ STREAM DMA",
  "READ STREAM PIO",
  cmd_reserved,
  cmd_reserved,
  cmd_reserved,
  "READ LOG EXT",
/*-------------------------------------------------- 30h-3Fh -----*/
  "WRITE SECTOR(S)",
  "WRITE SECTOR(S) [OBS-5]",
  "WRITE LONG(w/ retry) [OBS-4]",
  "WRITE LONG(w/o retry) [OBS-4]",
  "WRITE SECTORS(S) EXT",
  "WRITE DMA EXT",
  "WRITE DMA QUEUED EXT",
  "SET MAX ADDRESS EXT",
  "CFA WRITE SECTORS WITHOUT ERASE",
  "WRITE MULTIPLE EXT",
  "WRITE STREAM DMA",
  "WRITE STREAM PIO",
  "WRITE VERIFY [OBS-4]",
  "WRITE DMA FUA EXT",
  "WRITE DMA QUEUED FUA EXT",
  "WRITE LOG EXT",
/*-------------------------------------------------- 40h-4Fh -----*/
  "READ VERIFY SECTOR(S)",
  "READ VERIFY SECTOR(S) [OBS-5]",
  "READ VERIFY SECTOR(S) EXT",
  cmd_reserved,
  cmd_reserved,
  cmd_reserved,
  cmd_reserved,
  cmd_reserved,
  cmd_reserved,
  cmd_reserved,
  cmd_reserved,
  cmd_reserved,
  cmd_reserved,
  cmd_reserved,
  cmd_reserved,
  cmd_reserved,
/*-------------------------------------------------- 50h-5Fh -----*/
  "FORMAT TRACK [OBS-4]",
  "CONFIGURE STREAM",
  cmd_reserved,
  cmd_reserved,
  cmd_reserved,
  cmd_reserved,
  cmd_reserved,
  cmd_reserved,
  cmd_reserved,
  cmd_reserved,
  cmd_reserved,
  cmd_reserved,
  cmd_reserved,
  cmd_reserved,
  cmd_reserved,
  cmd_reserved,
/*-------------------------------------------------- 60h-6Fh -----*/
  cmd_reserved_sa,
  cmd_reserved_sa,
  cmd_reserved_sa,
  cmd_reserved_sa,
  cmd_reserved_sa,
  cmd_reserved_sa,
  cmd_reserved_sa,
  cmd_reserved_sa,
  cmd_reserved,
  cmd_reserved,
  cmd_reserved,
  cmd_reserved,
  cmd_reserved,
  cmd_reserved,
  cmd_reserved,
  cmd_reserved,
/*-------------------------------------------------- 70h-7Fh -----*/
  "SEEK [OBS-7]",
  cmd_seek_ret4,
  cmd_seek_ret4,
  cmd_seek_ret4,
  cmd_seek_ret4,
  cmd_seek_ret4,
  cmd_seek_ret4,
  cmd_seek_ret4,
  cmd_seek_ret4,
  cmd_seek_ret4,
  cmd_seek_ret4,
  cmd_seek_ret4,
  cmd_seek_ret4,
  cmd_seek_ret4,
  cmd_seek_ret4,
  cmd_seek_ret4,
/*-------------------------------------------------- 80h-8Fh -----*/
  cmd_vendor_specific,
  cmd_vendor_specific,
  cmd_vendor_specific,
  cmd_vendor_specific,
  cmd_vendor_specific,
  cmd_vendor_specific,
  cmd_vendor_specific,
  "CFA TRANSLATE SECTOR [VS IF NO CFA]",
  cmd_vendor_specific,
  cmd_vendor_specific,
  cmd_vendor_specific,
  cmd_vendor_specific,
  cmd_vendor_specific,
  cmd_vendor_specific,
  cmd_vendor_specific,
  cmd_vendor_specific,
/*-------------------------------------------------- 90h-9Fh -----*/
  "EXECUTE DEVICE DIAGNOSTIC",
  "INITIALIZE DEVICE PARAMETERS [OBS-6]",
  "DOWNLOAD MICROCODE",
  cmd_reserved,
  "STANDBY IMMEDIATE [RET-4]",
  "IDLE IMMEDIATE [RET-4]",
  "STANDBY [RET-4]",
  "IDLE [RET-4]",
  "CHECK POWER MODE [RET-4]",
  "SLEEP [RET-4]",
  cmd_vendor_specific,
  cmd_reserved,
  cmd_reserved,
  cmd_reserved,
  cmd_reserved,
  cmd_reserved,
/*-------------------------------------------------- A0h-AFh -----*/
  "PACKET",
  "IDENTIFY PACKET DEVICE",
  "SERVICE",
  cmd_reserved,
  cmd_reserved,
  cmd_reserved,
  cmd_reserved,
  cmd_reserved,
  cmd_reserved,
  cmd_reserved,
  cmd_reserved,
  cmd_reserved,
  cmd_reserved,
  cmd_reserved,
  cmd_reserved,
  cmd_reserved,
/*-------------------------------------------------- B0h-BFh -----*/
  "SMART",
  "DEVICE CONFIGURATION",
  cmd_reserved,
  cmd_reserved,
  cmd_reserved,
  cmd_reserved,
  cmd_reserved,
  cmd_reserved,
  cmd_reserved_cf,
  cmd_reserved_cf,
  cmd_reserved_cf,
  cmd_reserved_cf,
  cmd_reserved_cf,
  cmd_reserved_cf,
  cmd_reserved_cf,
  cmd_reserved_cf,
/*-------------------------------------------------- C0h-CFh -----*/
  "CFA ERASE SECTORS [VS IF NO CFA]",
  cmd_vendor_specific,
  cmd_vendor_specific,
  cmd_vendor_specific,
  "READ MULTIPLE",
  "WRITE MULTIPLE",
  "SET MULTIPLE MODE",
  "READ DMA QUEUED",
  "READ DMA",
  "READ DMA [OBS-5]",
  "WRITE DMA",
  "WRITE DMA [OBS-5]",
  "WRITE DMA QUEUED",
  "CFA WRITE MULTIPLE WITHOUT ERASE",
  "WRITE MULTIPLE FUA EXT",
  cmd_reserved,
/*-------------------------------------------------- D0h-DFh -----*/
  cmd_reserved,
  "CHECK MEDIA CARD TYPE",
  cmd_reserved_mcpt,
  cmd_reserved_mcpt,
  cmd_reserved_mcpt,
  cmd_reserved,
  cmd_reserved,
  cmd_reserved,
  cmd_reserved,
  cmd_reserved,
  "GET MEDIA STATUS",
  "ACKNOWLEDGE MEDIA CHANGE [RET-4]",
  "BOOT POST-BOOT [RET-4]",
  "BOOT PRE-BOOT [RET-4]",
  "MEDIA LOCK",
  "MEDIA UNLOCK",
/*-------------------------------------------------- E0h-EFh -----*/
  "STANDBY IMMEDIATE",
  "IDLE IMMEDIATE",
  "STANDBY",
  "IDLE",
  "READ BUFFER",
  "CHECK POWER MODE",
  "SLEEP",
  "FLUSH CACHE",
  "WRITE BUFFER",
  "WRITE SAME [RET-4]",  /* Warning!  This command is retired but the value of
                            f_reg is used in look_up_ata_command().  If this
                            command code is reclaimed in a future standard then
                            be sure to update look_up_ata_command(). */
  "FLUSH CACHE EXIT",
  cmd_reserved,
  "IDENTIFY DEVICE",
  "MEDIA EJECT",
  "IDENTIFY DEVICE DMA [OBS-4]",
  "SET FEATURES",
/*-------------------------------------------------- F0h-FFh -----*/
  cmd_vendor_specific,
  "SECURITY SET PASSWORD",
  "SECURITY UNLOCK",
  "SECURITY ERASE PREPARE",
  "SECURITY ERASE UNIT",
  "SECURITY FREEZE LOCK",
  "SECURITY DISABLE PASSWORD",
  cmd_vendor_specific,
  "READ NATIVE MAX ADDRESS",
  "SET MAX",
  cmd_vendor_specific,
  cmd_vendor_specific,
  cmd_vendor_specific,
  cmd_vendor_specific,
  cmd_vendor_specific,
  cmd_vendor_specific
};

/* Returns the name of the command (and possibly sub-command) with the given
   command code and feature register values.   For most command codes this
   simply returns the corresponding entry in the command_table array, but for
   others the value of the feature register specifies a subcommand or
   distinguishes commands. */
const char *look_up_ata_command(unsigned char c_code, unsigned char f_reg) {

  // check that command table not messed up.  The compiler will issue
  // warnings if there are too many array elements, but won't issue
  // warnings if there are not enough of them.
  if (sizeof(command_table) != sizeof(char *)*COMMAND_TABLE_SIZE){
    fprintf(stderr, 
            "Problem in atacmdnames.c.  Command Table command_table[] does\n"
            "not have %d entries!  It has %d entries. Please fix it.\n",
            COMMAND_TABLE_SIZE, (int)(sizeof(command_table)/sizeof(char *)));
    abort();
  }

  switch (c_code) {
  case 0x00:  /* NOP */
    switch (f_reg) {
    case 0x00:
      return "NOP [Abort queued commands]";
    case 0x01:
      return "NOP [Don't abort queued commands]";
    default:
      return "NOP [Reserved subcommand]";
    }
  case 0x92:  /* DOWNLOAD MICROCODE */
    switch (f_reg) {
    case 0x01:
      return "DOWNLOAD MICROCODE [Temporary]";
    case 0x07:
      return "DOWNLOAD MICROCODE [Save]";
    default:
      return "DOWNLOAD MICROCODE [Reserved subcommand]";
    }
  case 0xB0:  /* SMART */
    switch (f_reg) {
    case 0xD0:
      return "SMART READ DATA";
    case 0xD1:
      return "SMART READ ATTRIBUTE THRESHOLDS [OBS-4]";
    case 0xD2:
      return "SMART ENABLE/DISABLE ATTRIBUTE AUTOSAVE";
    case 0xD3:
      return "SMART SAVE ATTRIBUTE VALUES [OBS-6]";
    case 0xD4:
      return "SMART EXECUTE OFF-LINE IMMEDIATE";
    case 0xD5:
      return "SMART READ LOG";
    case 0xD6:
      return "SMART WRITE LOG";
    case 0xD7:
      return "SMART WRITE ATTRIBUTE THRESHOLDS [NS, OBS-4]";
    case 0xD8:
      return "SMART ENABLE OPERATIONS";
    case 0xD9:
      return "SMART DISABLE OPERATIONS";
    case 0xDA:
      return "SMART RETURN STATUS";
    case 0xDB:
      return "SMART EN/DISABLE AUTO OFFLINE [NS (SFF-8035i)]";
    default:
        if (f_reg >= 0xE0)
          return "[Vendor specific SMART command]";
        else
          return "[Reserved SMART command]";
    }
  case 0xB1:  /* DEVICE CONFIGURATION */
    switch (f_reg) {
    case 0xC0:
      return "DEVICE CONFIGURATION RESTORE";
    case 0xC1:
      return "DEVICE CONFIGURATION FREEZE LOCK";
    case 0xC2:
      return "DEVICE CONFIGURATION IDENTIFY";
    case 0xC3:
      return "DEVICE CONFIGURATION SET";
    default:
      return "DEVICE CONFIGURATION [Reserved command]";
    }
  case 0xE9:  /* WRITE SAME */
    switch (f_reg) {
    case 0x22:
      return "WRITE SAME [Start specified] [RET-4]";
    case 0xDD:
      return "WRITE SAME [Start unspecified] [RET-4]";
    default:
      return "WRITE SAME [Invalid subcommand] [RET-4]";
    } 
  case 0xEF:  /* SET FEATURES */
    switch (f_reg) {
    case 0x01:
      return "SET FEATURES [Enable 8-bit PIO]";
    case 0x02:
      return "SET FEATURES [Enable write cache]";
    case 0x03:
      return "SET FEATURES [Set transfer mode]";
    case 0x04:
      return "SET FEATURES [Enable auto DR] [OBS-4]";
    case 0x05:
      return "SET FEATURES [Enable APM]";
    case 0x06:
      return "SET FEATURES [Enable Pwr-Up In Standby]";
    case 0x07:
      return "SET FEATURES [Set device spin-up]";
    case 0x09:
      return "SET FEATURES [Reserved (address offset)]";
    case 0x0A:
      return "SET FEATURES [Enable CFA power mode 1]";
    case 0x10:
      return "SET FEATURES [Reserved for Serial ATA]";
    case 0x20:
      return "SET FEATURES [Set Time-ltd R/W WCT]";
    case 0x21:
      return "SET FEATURES [Set Time-ltd R/W EH]";
    case 0x31:
      return "SET FEATURES [Disable Media Status Notf]";
    case 0x33:
      return "SET FEATURES [Disable retry] [OBS-4]";
    case 0x42:
      return "SET FEATURES [Enable AAM]";
    case 0x43:
      return "SET FEATURES [Set Max Host I/F S Times]";
    case 0x44:
      return "SET FEATURES [Length of VS data] [OBS-4]";
    case 0x54:
      return "SET FEATURES [Set cache segs] [OBS-4]";
    case 0x55:
      return "SET FEATURES [Disable read look-ahead]";
    case 0x5D:
      return "SET FEATURES [Enable release interrupt]";
    case 0x5E:
      return "SET FEATURES [Enable SERVICE interrupt]";
    case 0x66:
      return "SET FEATURES [Disable revert defaults]";
    case 0x77:
      return "SET FEATURES [Disable ECC] [OBS-4]";
    case 0x81:
      return "SET FEATURES [Disable 8-bit PIO]";
    case 0x82:
      return "SET FEATURES [Disable write cache]";
    case 0x84:
      return "SET FEATURES [Disable auto DR] [OBS-4]";
    case 0x85:
      return "SET FEATURES [Disable APM]";
    case 0x86:
      return "SET FEATURES [Disable Pwr-Up In Standby]";
    case 0x88:
      return "SET FEATURES [Disable ECC] [OBS-4]";
    case 0x89:
      return "SET FEATURES [Reserved (address offset)]";
    case 0x8A:
      return "SET FEATURES [Disable CFA power mode 1]";
    case 0x90:
      return "SET FEATURES [Reserved for Serial ATA]";
    case 0x95:
      return "SET FEATURES [Enable Media Status Notf]";
    case 0x99:
      return "SET FEATURES [Enable retries] [OBS-4]";
    case 0x9A:
      return "SET FEATURES [Set max avg curr] [OBS-4]";
    case 0xAA:
      return "SET FEATURES [Enable read look-ahead]";
    case 0xAB:
      return "SET FEATURES [Set max prefetch] [OBS-4]";
    case 0xBB:
      return "SET FEATURES [4 bytes VS data] [OBS-4]";
    case 0xC2:
      return "SET FEATURES [Disable AAM]";
    case 0xCC:
      return "SET FEATURES [Enable revert to defaults]";
    case 0xDD:
      return "SET FEATURES [Disable release interrupt]";
    case 0xDE:
      return "SET FEATURES [Disable SERVICE interrupt]";
    case 0xE0:
      return "SET FEATURES [Obsolete subcommand]";
    default:
      if (f_reg >= 0xF0)
        return "SET FEATURES [Reserved for CFA]";
      else
        return "SET FEATURES [Reserved subcommand]";
    }
  case 0xF9:  /* SET MAX */
    switch (f_reg) {
    case 0x00:
      return "SET MAX ADDRESS [OBS-6]";
    case 0x01:
      return "SET MAX SET PASSWORD";
    case 0x02:
      return "SET MAX LOCK";
    case 0x03:
      return "SET MAX UNLOCK";
    case 0x04:
      return "SET MAX FREEZE LOCK";
    default:
      return "[Reserved SET MAX command]";
    }
  default:
    return command_table[c_code];
  }
}
