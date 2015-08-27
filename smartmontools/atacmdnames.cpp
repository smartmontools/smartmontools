/*
 * atacmdnames.cpp
 *
 * Home page of code is: http://www.smartmontools.org
 *
 * Copyright (C) 2003-8 Philip Williams
 * Copyright (C) 2012 Christian Franke <smartmontools-support@lists.sourceforge.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * You should have received a copy of the GNU General Public License
 * (for example COPYING); If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "atacmdnames.h"
#include <stdlib.h>
#include <stdio.h>

const char * atacmdnames_cpp_cvsid = "$Id$"
  ATACMDNAMES_H_CVSID;

const char cmd_reserved[]        = "[RESERVED]";
const char cmd_vendor_specific[] = "[VENDOR SPECIFIC]";
const char cmd_reserved_sa[]     = "[RESERVED FOR SERIAL ATA]";
const char cmd_reserved_cf[]     = "[RESERVED FOR COMPACTFLASH ASSOCIATION]";
const char cmd_reserved_mcpt[]   = "[RESERVED FOR MEDIA CARD PASS THROUGH]"; // ACS-3: Reserved
const char cmd_recalibrate_ret4[]= "RECALIBRATE [RET-4]";
const char cmd_seek_ret4[]       = "SEEK [RET-4]";

// Tables B.3 and B.4 of T13/2161-D (ACS-3) Revision 4, September 4, 2012

const char * const command_table[] = {
/*-------------------------------------------------- 00h-0Fh -----*/
  "NOP",
  cmd_reserved,
  cmd_reserved,
  "CFA REQUEST EXTENDED ERROR",
  cmd_reserved,
  cmd_reserved,
  "DATA SET MANAGEMENT", // ACS-2
  cmd_reserved,
  "DEVICE RESET",
  cmd_reserved,
  cmd_reserved,
  "REQUEST SENSE DATA EXT", // ACS-2
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
  "READ LONG [OBS-4]",
  "READ LONG (w/o retry) [OBS-4]",
  "READ SECTOR(S) EXT",
  "READ DMA EXT",
  "READ DMA QUEUED EXT [OBS-ACS-2]",
  "READ NATIVE MAX ADDRESS EXT [OBS-ACS-3]",
  cmd_reserved,
  "READ MULTIPLE EXT",
  "READ STREAM DMA",
  "READ STREAM",
  cmd_reserved,
  cmd_reserved,
  cmd_reserved,
  "READ LOG EXT",
/*-------------------------------------------------- 30h-3Fh -----*/
  "WRITE SECTOR(S)",
  "WRITE SECTOR(S) (w/o retry) [OBS-5]",
  "WRITE LONG [OBS-4]",
  "WRITE LONG (w/o retry) [OBS-4]",
  "WRITE SECTORS(S) EXT",
  "WRITE DMA EXT",
  "WRITE DMA QUEUED EXT [OBS-ACS-2]",
  "SET NATIVE MAX ADDRESS EXT [OBS-ACS-3]",
  "CFA WRITE SECTORS WITHOUT ERASE",
  "WRITE MULTIPLE EXT",
  "WRITE STREAM DMA",
  "WRITE STREAM",
  "WRITE VERIFY [OBS-4]",
  "WRITE DMA FUA EXT",
  "WRITE DMA QUEUED FUA EXT [OBS-ACS-2]",
  "WRITE LOG EXT",
/*-------------------------------------------------- 40h-4Fh -----*/
  "READ VERIFY SECTOR(S)",
  "READ VERIFY SECTOR(S) (w/o retry) [OBS-5]",
  "READ VERIFY SECTOR(S) EXT",
  cmd_reserved,
  cmd_reserved,
  "WRITE UNCORRECTABLE EXT", // ATA-8
  cmd_reserved,
  "READ LOG DMA EXT", // ATA-8
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
  "WRITE LOG DMA EXT", // ATA-8
  cmd_reserved,
  cmd_reserved,
  cmd_reserved,
  "TRUSTED NON-DATA", // ATA-8
  "TRUSTED RECEIVE", // ATA-8
  "TRUSTED RECEIVE DMA", // ATA-8
  "TRUSTED SEND", // ATA-8
  "TRUSTED SEND DMA", // ATA-8
/*-------------------------------------------------- 60h-6Fh -----*/
  "READ FPDMA QUEUED", // ATA-8
  "WRITE FPDMA QUEUED", // ATA-8
  cmd_reserved_sa,
  "NCQ QUEUE MANAGEMENT", // ACS-3
  "SEND FPDMA QUEUED", // ACS-3
  "RECEIVE FPDMA QUEUED", // ACS-3
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
  "SET DATE & TIME EXT", // ACS-3
  "ACCESSIBLE MAX ADDRESS CONFIGURATION", // ACS-3
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
  "DOWNLOAD MICROCODE DMA", // ACS-2
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
  "SERVICE [OBS-ACS-2]",
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
  "DEVICE CONFIGURATION [OBS-ACS-3]",
  cmd_reserved,
  cmd_reserved,
  "SANITIZE DEVICE", // ACS-2
  cmd_reserved,
  "NV CACHE [OBS-ACS-3]", // ATA-8
  cmd_reserved_cf,
  cmd_reserved_cf,
  cmd_reserved_cf,
  cmd_reserved_cf,
  cmd_reserved_cf,
  cmd_reserved,
  cmd_reserved,
  cmd_reserved,
  cmd_reserved,
/*-------------------------------------------------- C0h-CFh -----*/
  "CFA ERASE SECTORS [VS IF NO CFA]",
  cmd_vendor_specific,
  cmd_vendor_specific,
  cmd_vendor_specific,
  "READ MULTIPLE",
  "WRITE MULTIPLE",
  "SET MULTIPLE MODE",
  "READ DMA QUEUED [OBS-ACS-2]",
  "READ DMA",
  "READ DMA (w/o retry) [OBS-5]",
  "WRITE DMA",
  "WRITE DMA (w/o retry) [OBS-5]",
  "WRITE DMA QUEUED [OBS-ACS-2]",
  "CFA WRITE MULTIPLE WITHOUT ERASE",
  "WRITE MULTIPLE FUA EXT",
  cmd_reserved,
/*-------------------------------------------------- D0h-DFh -----*/
  cmd_reserved,
  "CHECK MEDIA CARD TYPE [OBS-ACS-2]",
  cmd_reserved_mcpt,
  cmd_reserved_mcpt,
  cmd_reserved_mcpt,
  cmd_reserved,
  cmd_reserved,
  cmd_reserved,
  cmd_reserved,
  cmd_reserved,
  "GET MEDIA STATUS [OBS-8]",
  "ACKNOWLEDGE MEDIA CHANGE [RET-4]",
  "BOOT POST-BOOT [RET-4]",
  "BOOT PRE-BOOT [RET-4]",
  "MEDIA LOCK [OBS-8]",
  "MEDIA UNLOCK [OBS-8]",
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
  "READ BUFFER DMA", // ACS-2 (was: WRITE SAME [RET-4])
  "FLUSH CACHE EXT",
  "WRITE BUFFER DMA", // ACS-2
  "IDENTIFY DEVICE",
  "MEDIA EJECT [OBS-8]",
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
  "READ NATIVE MAX ADDRESS [OBS-ACS-3]",
  "SET MAX ADDRESS [OBS-ACS-3]",
  cmd_vendor_specific,
  cmd_vendor_specific,
  cmd_vendor_specific,
  cmd_vendor_specific,
  cmd_vendor_specific,
  cmd_vendor_specific
};

typedef char ASSERT_command_table_size[
  sizeof(command_table)/sizeof(command_table[0]) == 256 ? 1 : -1];

/* Returns the name of the command (and possibly sub-command) with the given
   command code and feature register values.   For most command codes this
   simply returns the corresponding entry in the command_table array, but for
   others the value of the feature register specifies a subcommand or
   distinguishes commands. */
const char *look_up_ata_command(unsigned char c_code, unsigned char f_reg) {

  switch (c_code) {
  case 0x00:  /* NOP */
    switch (f_reg) {
    case 0x00:
      return "NOP [Abort queued commands]";
    case 0x01:
      return "NOP [Don't abort queued commands] [OBS-ACS-2]";
    default:
      return "NOP [Reserved subcommand] [OBS-ACS-2]";
    }
  case 0x92:  /* DOWNLOAD MICROCODE */
    switch (f_reg) {
    case 0x01:
      return "DOWNLOAD MICROCODE [Temporary] [OBS-8]";
    case 0x03:
      return "DOWNLOAD MICROCODE [Save with offsets]"; // ATA-8
    case 0x07:
      return "DOWNLOAD MICROCODE [Save]";
    case 0x0e:
      return "DOWNLOAD MICROCODE [Save for future use]"; // ACS-3
    case 0x0f:
      return "DOWNLOAD MICROCODE [Activate]"; // ACS-3
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
          return "SMART [Vendor specific subcommand]";
        else
          return "SMART [Reserved subcommand]";
    }
  case 0xB1:  /* DEVICE CONFIGURATION */
    switch (f_reg) {
    case 0xC0:
      return "DEVICE CONFIGURATION RESTORE [OBS-ACS-3]";
    case 0xC1:
      return "DEVICE CONFIGURATION FREEZE LOCK [OBS-ACS-3]";
    case 0xC2:
      return "DEVICE CONFIGURATION IDENTIFY [OBS-ACS-3]";
    case 0xC3:
      return "DEVICE CONFIGURATION SET [OBS-ACS-3]";
    default:
      return "DEVICE CONFIGURATION [Reserved subcommand] [OBS-ACS-3]";
    }
  case 0xEF:  /* SET FEATURES */
    switch (f_reg) {
    case 0x01:
      return "SET FEATURES [Enable 8-bit PIO] [OBS-3]"; // Now CFA
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
      return "SET FEATURES [Reserved (address offset)] [OPS-ACS-3]";
    case 0x0A:
      return "SET FEATURES [Enable CFA power mode 1]";
    case 0x10:
      return "SET FEATURES [Enable SATA feature]"; // ACS-3
    case 0x20:
      return "SET FEATURES [Set Time-ltd R/W WCT]";
    case 0x21:
      return "SET FEATURES [Set Time-ltd R/W EH]";
    case 0x31:
      return "SET FEATURES [Disable Media Status Notf] [OBS-8]";
    case 0x33:
      return "SET FEATURES [Disable retry] [OBS-4]";
    case 0x41:
      return "SET FEATURES [Enable Free-fall Control]"; // ATA-8
    case 0x42:
      return "SET FEATURES [Enable AAM] [OBS-ACS-2]";
    case 0x43:
      return "SET FEATURES [Set Max Host I/F S Times]";
    case 0x44:
      return "SET FEATURES [Length of VS data] [OBS-4]";
    case 0x4a:
      return "SET FEATURES [Ext. Power Conditions]"; // ACS-2
    case 0x54:
      return "SET FEATURES [Set cache segs] [OBS-4]";
    case 0x55:
      return "SET FEATURES [Disable read look-ahead]";
    case 0x5D:
      return "SET FEATURES [Enable release interrupt] [OBS-ACS-2]";
    case 0x5E:
      return "SET FEATURES [Enable SERVICE interrupt] [OBS-ACS-2]";
    case 0x66:
      return "SET FEATURES [Disable revert defaults]";
    case 0x69:
      return "SET FEATURES [LPS Error Reporting Control]"; // ACS-2
    case 0x77:
      return "SET FEATURES [Disable ECC] [OBS-4]";
    case 0x81:
      return "SET FEATURES [Disable 8-bit PIO] [OBS-3]"; // Now CFA
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
      return "SET FEATURES [Disable SATA feature]"; // ACS-3
    case 0x95:
      return "SET FEATURES [Enable Media Status Notf] [OBS-8]";
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
    case 0xC1:
      return "SET FEATURES [Disable Free-fall Control]"; // ATA-8
    case 0xC2:
      return "SET FEATURES [Disable AAM] [OBS-ACS-2]";
    case 0xC3:
      return "SET FEATURES [Sense Data Reporting]"; // ACS-2
    case 0xCC:
      return "SET FEATURES [Enable revert to defaults]";
    case 0xDD:
      return "SET FEATURES [Disable release interrupt] [OBS-ACS-2]";
    case 0xDE:
      return "SET FEATURES [Disable SERVICE interrupt] [OBS-ACS-2]";
    case 0xE0:
      return "SET FEATURES [Vendor specific] [OBS-7]";
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
      return "SET MAX SET PASSWORD [OBS-ACS-3]";
    case 0x02:
      return "SET MAX LOCK [OBS-ACS-3]";
    case 0x03:
      return "SET MAX UNLOCK [OBS-ACS-3]";
    case 0x04:
      return "SET MAX FREEZE LOCK [OBS-ACS-3]";
    default:
      return "SET MAX [Reserved subcommand] [OBS-ACS-3]";
    }
  default:
    return command_table[c_code];
  }
}
