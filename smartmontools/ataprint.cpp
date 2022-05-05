/*
 * ataprint.cpp
 *
 * Home page of code is: https://www.smartmontools.org
 *
 * Copyright (C) 2002-11 Bruce Allen
 * Copyright (C) 2008-21 Christian Franke
 * Copyright (C) 1999-2000 Michael Cornwell <cornwell@acm.org>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "config.h"
#define __STDC_FORMAT_MACROS 1 // enable PRI* for C++

#include <ctype.h>
#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "atacmdnames.h"
#include "atacmds.h"
#include "ataidentify.h"
#include "dev_interface.h"
#include "ataprint.h"
#include "smartctl.h"
#include "sg_unaligned.h"
#include "utility.h"
#include "knowndrives.h"

const char * ataprint_cpp_cvsid = "$Id$"
                                  ATAPRINT_H_CVSID;


static const char * infofound(const char *output) {
  return (*output ? output : "[No Information Found]");
}

// Return true if '-T permissive' is specified,
// used to ignore missing capabilities
static bool is_permissive()
{
  if (!failuretest_permissive)
    return false;
  failuretest_permissive--;
  return true;
}

/* For the given Command Register (CR) and Features Register (FR), attempts
 * to construct a string that describes the contents of the Status
 * Register (ST) and Error Register (ER).  If the meanings of the flags of
 * the error register are not known for the given command then it returns an
 * empty string.
 *
 * The meanings of the flags of the error register for all commands are
 * described in the ATA spec and could all be supported here in theory.
 * Currently, only a few commands are supported (those that have been seen
 * to produce errors).  If many more are to be added then this function
 * should probably be redesigned.
 */

static std::string format_st_er_desc(
  unsigned char CR, unsigned char FR,
  unsigned char ST, unsigned char ER,
  unsigned short SC,
  const ata_smart_errorlog_error_struct * lba28_regs,
  const ata_smart_exterrlog_error * lba48_regs
)
{
  const char *error_flag[8];
  int i, print_lba=0, print_sector=0;

  // Set of character strings corresponding to different error codes.
  // Please keep in alphabetic order if you add more.
  const char  *abrt  = "ABRT";  // ABORTED
 const char   *amnf  = "AMNF";  // ADDRESS MARK NOT FOUND
 const char   *ccto  = "CCTO";  // COMMAND COMPLETION TIMED OUT
 const char   *eom   = "EOM";   // END OF MEDIA
 const char   *icrc  = "ICRC";  // INTERFACE CRC ERROR
 const char   *idnf  = "IDNF";  // ID NOT FOUND
 const char   *ili   = "ILI";   // MEANING OF THIS BIT IS COMMAND-SET SPECIFIC
 const char   *mc    = "MC";    // MEDIA CHANGED 
 const char   *mcr   = "MCR";   // MEDIA CHANGE REQUEST
 const char   *nm    = "NM";    // NO MEDIA
 const char   *obs   = "obs";   // OBSOLETE
 const char   *tk0nf = "TK0NF"; // TRACK 0 NOT FOUND
 const char   *unc   = "UNC";   // UNCORRECTABLE
 const char   *wp    = "WP";    // WRITE PROTECTED

  /* If for any command the Device Fault flag of the status register is
   * not used then used_device_fault should be set to 0 (in the CR switch
   * below)
   */
  int uses_device_fault = 1;

  /* A value of NULL means that the error flag isn't used */
  for (i = 0; i < 8; i++)
    error_flag[i] = NULL;

  std::string str;

  switch (CR) {
  case 0x10:  // RECALIBRATE
    error_flag[2] = abrt;
    error_flag[1] = tk0nf;
    break;
  case 0x20:  /* READ SECTOR(S) */
  case 0x21:  // READ SECTOR(S)
  case 0x24:  // READ SECTOR(S) EXT
  case 0xC4:  /* READ MULTIPLE */
  case 0x29:  // READ MULTIPLE EXT
    error_flag[6] = unc;
    error_flag[5] = mc;
    error_flag[4] = idnf;
    error_flag[3] = mcr;
    error_flag[2] = abrt;
    error_flag[1] = nm;
    error_flag[0] = amnf;
    print_lba=1;
    break;
  case 0x22:  // READ LONG (with retries)
  case 0x23:  // READ LONG (without retries)
    error_flag[4] = idnf;
    error_flag[2] = abrt;
    error_flag[0] = amnf;
    print_lba=1;
    break;
  case 0x2a:  // READ STREAM DMA
  case 0x2b:  // READ STREAM PIO
    if (CR==0x2a)
      error_flag[7] = icrc;
    error_flag[6] = unc;
    error_flag[5] = mc;
    error_flag[4] = idnf;
    error_flag[3] = mcr;
    error_flag[2] = abrt;
    error_flag[1] = nm;
    error_flag[0] = ccto;
    print_lba=1;
    print_sector=SC;
    break;
  case 0x3A:  // WRITE STREAM DMA
  case 0x3B:  // WRITE STREAM PIO
    if (CR==0x3A)
      error_flag[7] = icrc;
    error_flag[6] = wp;
    error_flag[5] = mc;
    error_flag[4] = idnf;
    error_flag[3] = mcr;
    error_flag[2] = abrt;
    error_flag[1] = nm;
    error_flag[0] = ccto;
    print_lba=1;
    print_sector=SC;
    break;
  case 0x25:  // READ DMA EXT
  case 0x26:  // READ DMA QUEUED EXT
  case 0xC7:  // READ DMA QUEUED
  case 0xC8:  // READ DMA (with retries)
  case 0xC9:  // READ DMA (without retries, obsolete since ATA-5)
  case 0x60:  // READ FPDMA QUEUED (NCQ)
    error_flag[7] = icrc;
    error_flag[6] = unc;
    error_flag[5] = mc;
    error_flag[4] = idnf;
    error_flag[3] = mcr;
    error_flag[2] = abrt;
    error_flag[1] = nm;
    error_flag[0] = amnf;
    print_lba=1;
    if (CR==0x25 || CR==0xC8)
      print_sector=SC;
    break;
  case 0x30:  /* WRITE SECTOR(S) */
  case 0x31:  // WRITE SECTOR(S)
  case 0x34:  // WRITE SECTOR(S) EXT
  case 0xC5:  /* WRITE MULTIPLE */
  case 0x39:  // WRITE MULTIPLE EXT
  case 0xCE:  // WRITE MULTIPLE FUA EXT
    error_flag[6] = wp;
    error_flag[5] = mc;
    error_flag[4] = idnf;
    error_flag[3] = mcr;
    error_flag[2] = abrt;
    error_flag[1] = nm;
    print_lba=1;
    break;
  case 0x32:  // WRITE LONG (with retries)
  case 0x33:  // WRITE LONG (without retries)
    error_flag[4] = idnf;
    error_flag[2] = abrt;
    print_lba=1;
    break;
  case 0x3C:  // WRITE VERIFY
    error_flag[6] = unc;
    error_flag[4] = idnf;
    error_flag[2] = abrt;
    error_flag[0] = amnf;
    print_lba=1;
    break;
  case 0x40: // READ VERIFY SECTOR(S) with retries
  case 0x41: // READ VERIFY SECTOR(S) without retries
  case 0x42: // READ VERIFY SECTOR(S) EXT
    error_flag[6] = unc;
    error_flag[5] = mc;
    error_flag[4] = idnf;
    error_flag[3] = mcr;
    error_flag[2] = abrt;
    error_flag[1] = nm;
    error_flag[0] = amnf;
    print_lba=1;
    break;
  case 0xA0:  /* PACKET */
    /* Bits 4-7 are all used for sense key (a 'command packet set specific error
     * indication' according to the ATA/ATAPI-7 standard), so "Sense key" will
     * be repeated in the error description string if more than one of those
     * bits is set.
     */
    error_flag[7] = "Sense key (bit 3)",
    error_flag[6] = "Sense key (bit 2)",
    error_flag[5] = "Sense key (bit 1)",
    error_flag[4] = "Sense key (bit 0)",
    error_flag[2] = abrt;
    error_flag[1] = eom;
    error_flag[0] = ili;
    break;
  case 0xA1:  /* IDENTIFY PACKET DEVICE */
  case 0xEF:  /* SET FEATURES */
  case 0x00:  /* NOP */
  case 0xC6:  /* SET MULTIPLE MODE */
    error_flag[2] = abrt;
    break;
  case 0x2F:  // READ LOG EXT
    error_flag[6] = unc;
    error_flag[4] = idnf;
    error_flag[2] = abrt;
    error_flag[0] = obs;
    break;
  case 0x3F:  // WRITE LOG EXT
    error_flag[4] = idnf;
    error_flag[2] = abrt;
    error_flag[0] = obs;
    break;
  case 0xB0:  /* SMART */
    switch(FR) {
    case 0xD0:  // SMART READ DATA
    case 0xD1:  // SMART READ ATTRIBUTE THRESHOLDS
    case 0xD5:  /* SMART READ LOG */
      error_flag[6] = unc;
      error_flag[4] = idnf;
      error_flag[2] = abrt;
      error_flag[0] = obs;
      break;
    case 0xD6:  /* SMART WRITE LOG */
      error_flag[4] = idnf;
      error_flag[2] = abrt;
      error_flag[0] = obs;
      break;
    case 0xD2:  // Enable/Disable Attribute Autosave
    case 0xD3:  // SMART SAVE ATTRIBUTE VALUES (ATA-3)
    case 0xD8:  // SMART ENABLE OPERATIONS
    case 0xD9:  /* SMART DISABLE OPERATIONS */
    case 0xDA:  /* SMART RETURN STATUS */
    case 0xDB:  // Enable/Disable Auto Offline (SFF)
      error_flag[2] = abrt;
      break;
    case 0xD4:  // SMART EXECUTE IMMEDIATE OFFLINE
      error_flag[4] = idnf;
      error_flag[2] = abrt;
      break;
    default:
      return str; // ""
      break;
    }
    break;
  case 0xB1:  /* DEVICE CONFIGURATION */
    switch (FR) {
    case 0xC0:  /* DEVICE CONFIGURATION RESTORE */
      error_flag[2] = abrt;
      break;
    default:
      return str; // ""
      break;
    }
    break;
  case 0xCA:  // WRITE DMA (with retries)
  case 0xCB:  // WRITE DMA (without retries, obsolete since ATA-5)
  case 0x35:  // WRITE DMA EXT
  case 0x3D:  // WRITE DMA FUA EXT
  case 0xCC:  // WRITE DMA QUEUED
  case 0x36:  // WRITE DMA QUEUED EXT
  case 0x3E:  // WRITE DMA QUEUED FUA EXT
  case 0x61:  // WRITE FPDMA QUEUED (NCQ)
    error_flag[7] = icrc;
    error_flag[6] = wp;
    error_flag[5] = mc;
    error_flag[4] = idnf;
    error_flag[3] = mcr;
    error_flag[2] = abrt;
    error_flag[1] = nm;
    error_flag[0] = amnf;
    print_lba=1;
    if (CR==0x35)
      print_sector=SC;
    break;
  case 0xE4: // READ BUFFER
  case 0xE8: // WRITE BUFFER
    error_flag[2] = abrt;
    break;
  default:
    return str; // ""
  }

  /* We ignore any status flags other than Device Fault and Error */

  if (uses_device_fault && (ST & (1 << 5))) {
    str = "Device Fault";
    if (ST & 1)  // Error flag
      str += "; ";
  }
  if (ST & 1) {  // Error flag
    int count = 0;

    str += "Error: ";
    for (i = 7; i >= 0; i--)
      if ((ER & (1 << i)) && (error_flag[i])) {
        if (count++ > 0)
           str += ", ";
        str += error_flag[i];
      }
  }

  // If the error was a READ or WRITE error, print the Logical Block
  // Address (LBA) at which the read or write failed.
  if (print_lba) {
    // print number of sectors, if known, and append to print string
    if (print_sector)
      str += strprintf(" %d sectors", print_sector);

    if (lba28_regs) {
      unsigned lba;
      // bits 24-27: bits 0-3 of DH
      lba   = 0xf & lba28_regs->drive_head;
      lba <<= 8;
      // bits 16-23: CH
      lba  |= lba28_regs->cylinder_high;
      lba <<= 8;
      // bits 8-15:  CL
      lba  |= lba28_regs->cylinder_low;
      lba <<= 8;
      // bits 0-7:   SN
      lba  |= lba28_regs->sector_number;
      str += strprintf(" at LBA = 0x%08x = %u", lba, lba);
    }
    else if (lba48_regs) {
      // This assumes that upper LBA registers are 0 for 28-bit commands
      // (TODO: detect 48-bit commands above)
      uint64_t lba48;
      lba48   = lba48_regs->lba_high_register_hi;
      lba48 <<= 8;
      lba48  |= lba48_regs->lba_mid_register_hi;
      lba48 <<= 8;
      lba48  |= lba48_regs->lba_low_register_hi;
      lba48  |= lba48_regs->device_register & 0xf;
      lba48 <<= 8;
      lba48  |= lba48_regs->lba_high_register;
      lba48 <<= 8;
      lba48  |= lba48_regs->lba_mid_register;
      lba48 <<= 8;
      lba48  |= lba48_regs->lba_low_register;
      str += strprintf(" at LBA = 0x%08" PRIx64 " = %" PRIu64, lba48, lba48);
    }
  }

  return str;
}

static inline std::string format_st_er_desc(
  const ata_smart_errorlog_struct * data)
{
  return format_st_er_desc(
    data->commands[4].commandreg,
    data->commands[4].featuresreg,
    data->error_struct.status,
    data->error_struct.error_register,
    data->error_struct.sector_count,
    &data->error_struct, (const ata_smart_exterrlog_error *)0);
}

static inline std::string format_st_er_desc(
  const ata_smart_exterrlog_error_log * data)
{
  return format_st_er_desc(
    data->commands[4].command_register,
    data->commands[4].features_register,
    data->error.status_register,
    data->error.error_register,
    data->error.count_register_hi << 8 | data->error.count_register,
    (const ata_smart_errorlog_error_struct *)0, &data->error);
}


static const char * get_form_factor(unsigned short word168)
{
  // Bits 0:3 are the form factor
  // Table A.32 of T13/2161-D (ACS-3) Revision 5, October 28, 2013
  // Table 247 of T13/BSR INCITS 529 (ACS-4) Revision 20, October 26, 2017
  // Table 254 of T13/BSR INCITS 558 (ACS-5) Revision 10, March 3, 2021
  switch (word168 & 0xF) {
    case 0x1: return "5.25 inches";
    case 0x2: return "3.5 inches";
    case 0x3: return "2.5 inches";
    case 0x4: return "1.8 inches";
    case 0x5: return "< 1.8 inches";
    case 0x6: return "mSATA"; // ACS-4
    case 0x7: return "M.2"; // ACS-4
    case 0x8: return "MicroSSD"; // ACS-4
    case 0x9: return "CFast"; // ACS-4
    default : return 0;
  }
}

static int find_msb(unsigned short word)
{
  for (int bit = 15; bit >= 0; bit--)
    if (word & (1 << bit))
      return bit;
  return -1;
}

static const char * get_ata_major_version(const ata_identify_device * drive)
{
  // Table 13 of T13/1153D (ATA/ATAPI-4) revision 18, August 19, 1998
  // Table 29 of T13/1699-D (ATA8-ACS) Revision 6a, September 6, 2008
  // Table 55 of T13/BSR INCITS 529 (ACS-4) Revision 20, October 26, 2017
  // Table 57 of T13/BSR INCITS 558 (ACS-5) Revision 10, March 3, 2021
  switch (find_msb(drive->major_rev_num)) {
    case 15: return "ACS >5 (15)";
    case 14: return "ACS >5 (14)";
    case 13: return "ACS >5 (13)";
    case 12: return "ACS-5";
    case 11: return "ACS-4";
    case 10: return "ACS-3";
    case  9: return "ACS-2";
    case  8: return "ATA8-ACS";
    case  7: return "ATA/ATAPI-7";
    case  6: return "ATA/ATAPI-6";
    case  5: return "ATA/ATAPI-5";
    case  4: return "ATA/ATAPI-4";
    case  3: return "ATA-3";
    case  2: return "ATA-2";
    case  1: return "ATA-1";
    default: return 0;
  }
}

static const char * get_ata_minor_version(const ata_identify_device * drive)
{
  // Table 10 of X3T13/2008D (ATA-3) Revision 7b, January 27, 1997
  // Table 28 of T13/1410D (ATA/ATAPI-6) Revision 3b, February 26, 2002
  // Table 31 of T13/1699-D (ATA8-ACS) Revision 6a, September 6, 2008
  // Table 52 of T13/2015-D (ACS-2) Revision 7, June 22, 2011
  // Table 47 of T13/2161-D (ACS-3) Revision 5, October 28, 2013
  // Table 57 of T13/BSR INCITS 529 (ACS-4) Revision 20, October 26, 2017
  // Table 59 of T13/BSR INCITS 558 (ACS-5) Revision 10, March 3, 2021
  switch (drive->minor_rev_num) {
    case 0x0001: return "ATA-1 X3T9.2/781D prior to revision 4";
    case 0x0002: return "ATA-1 published, ANSI X3.221-1994";
    case 0x0003: return "ATA-1 X3T9.2/781D revision 4";
    case 0x0004: return "ATA-2 published, ANSI X3.279-1996";
    case 0x0005: return "ATA-2 X3T10/948D prior to revision 2k";
    case 0x0006: return "ATA-3 X3T10/2008D revision 1";
    case 0x0007: return "ATA-2 X3T10/948D revision 2k";
    case 0x0008: return "ATA-3 X3T10/2008D revision 0";
    case 0x0009: return "ATA-2 X3T10/948D revision 3";
    case 0x000a: return "ATA-3 published, ANSI X3.298-1997";
    case 0x000b: return "ATA-3 X3T10/2008D revision 6"; // 1st ATA-3 revision with SMART
    case 0x000c: return "ATA-3 X3T13/2008D revision 7 and 7a";
    case 0x000d: return "ATA/ATAPI-4 X3T13/1153D revision 6";
    case 0x000e: return "ATA/ATAPI-4 T13/1153D revision 13";
    case 0x000f: return "ATA/ATAPI-4 X3T13/1153D revision 7";
    case 0x0010: return "ATA/ATAPI-4 T13/1153D revision 18";
    case 0x0011: return "ATA/ATAPI-4 T13/1153D revision 15";
    case 0x0012: return "ATA/ATAPI-4 published, ANSI NCITS 317-1998";
    case 0x0013: return "ATA/ATAPI-5 T13/1321D revision 3";
    case 0x0014: return "ATA/ATAPI-4 T13/1153D revision 14";
    case 0x0015: return "ATA/ATAPI-5 T13/1321D revision 1";
    case 0x0016: return "ATA/ATAPI-5 published, ANSI NCITS 340-2000";
    case 0x0017: return "ATA/ATAPI-4 T13/1153D revision 17";
    case 0x0018: return "ATA/ATAPI-6 T13/1410D revision 0";
    case 0x0019: return "ATA/ATAPI-6 T13/1410D revision 3a";
    case 0x001a: return "ATA/ATAPI-7 T13/1532D revision 1";
    case 0x001b: return "ATA/ATAPI-6 T13/1410D revision 2";
    case 0x001c: return "ATA/ATAPI-6 T13/1410D revision 1";
    case 0x001d: return "ATA/ATAPI-7 published, ANSI INCITS 397-2005";
    case 0x001e: return "ATA/ATAPI-7 T13/1532D revision 0";
    case 0x001f: return "ACS-3 T13/2161-D revision 3b";

    case 0x0021: return "ATA/ATAPI-7 T13/1532D revision 4a";
    case 0x0022: return "ATA/ATAPI-6 published, ANSI INCITS 361-2002";

    case 0x0027: return "ATA8-ACS T13/1699-D revision 3c";
    case 0x0028: return "ATA8-ACS T13/1699-D revision 6";
    case 0x0029: return "ATA8-ACS T13/1699-D revision 4";

    case 0x0031: return "ACS-2 T13/2015-D revision 2";

    case 0x0033: return "ATA8-ACS T13/1699-D revision 3e";

    case 0x0039: return "ATA8-ACS T13/1699-D revision 4c";

    case 0x0042: return "ATA8-ACS T13/1699-D revision 3f";

    case 0x0052: return "ATA8-ACS T13/1699-D revision 3b";

    case 0x005e: return "ACS-4 T13/BSR INCITS 529 revision 5";

    case 0x006d: return "ACS-3 T13/2161-D revision 5";

    case 0x0082: return "ACS-2 published, ANSI INCITS 482-2012";

    case 0x009c: return "ACS-4 published, ANSI INCITS 529-2018";

    case 0x0107: return "ATA8-ACS T13/1699-D revision 2d";

    case 0x010a: return "ACS-3 published, ANSI INCITS 522-2014";

    case 0x0110: return "ACS-2 T13/2015-D revision 3";

    case 0x011b: return "ACS-3 T13/2161-D revision 4";

    default:     return 0;
  }
}

static const char * get_pata_version(unsigned short word222, char (& buf)[32])
{
  // Table 29 of T13/1699-D (ATA8-ACS) Revision 6a, September 6, 2008
  // Table 57 of T13/BSR INCITS 558 (ACS-5) Revision 10, March 3, 2021
  switch (word222 & 0x0fff) {
    default: snprintf(buf, sizeof(buf),
                       "Unknown (0x%03x)", word222 & 0x0fff); return buf;
    case 0x001:
    case 0x003: return "ATA8-APT";
    case 0x002: return "ATA/ATAPI-7";
  }
}

static const char * get_sata_version(unsigned short word222)
{
  // Table 29 of T13/1699-D (ATA8-ACS) Revision 6a, September 6, 2008
  // Table 50 of T13/2015-D (ACS-2) Revision 7, June 22, 2011
  // Table 45 of T13/2161-D (ACS-3) Revision 5, October 28, 2013
  // Table 55 of T13/BSR INCITS 529 (ACS-4) Revision 20, October 26, 2017
  // Table 57 of T13/BSR INCITS 558 (ACS-5) Revision 10, March 3, 2021
  switch (find_msb(word222 & 0x0fff)) {
    case 11: return "SATA >3.5 (11)";
    case 10: return "SATA 3.5"; // ACS-5
    case  9: return "SATA 3.4"; // ACS-5
    case  8: return "SATA 3.3"; // ACS-4
    case  7: return "SATA 3.2"; // ACS-4
    case  6: return "SATA 3.1"; // ACS-3
    case  5: return "SATA 3.0"; // ACS-2
    case  4: return "SATA 2.6";
    case  3: return "SATA 2.5";
    case  2: return "SATA II Ext";
    case  1: return "SATA 1.0a";
    case  0: return "ATA8-AST";
    default: return 0;
  }
}

static const char * get_sata_speed(int speed)
{
  if (speed <= 0)
    return 0;
  // Table 29 of T13/1699-D (ATA8-ACS) Revision 6a, September 6, 2008
  // Table 50 of T13/2015-D (ACS-2) Revision 7, June 22, 2011
  // Table 45 of T13/2161-D (ACS-3) Revision 5, October 28, 2013
  // Table 57 of T13/BSR INCITS 558 (ACS-5) Revision 10, March 3, 2021
  switch (speed) {
    default: return ">6.0 Gb/s (7)";
    case 6:  return ">6.0 Gb/s (6)";
    case 5:  return ">6.0 Gb/s (5)";
    case 4:  return ">6.0 Gb/s (4)";
    case 3:  return "6.0 Gb/s"; // ACS-3
    case 2:  return "3.0 Gb/s";
    case 1:  return "1.5 Gb/s"; // ATA8-ACS
  }
}

static void jset_sata_speed(const char * key, int value, int speed, const char * str)
{
  if (speed <= 0)
    return;
  json::ref jref = jglb["interface_speed"][key];
  jref["sata_value"] = value;
  if (str)
    jref["string"] = str;
  int ups;
  switch (speed) {
    case 3: ups = 60; break;
    case 2: ups = 30; break;
    case 1: ups = 15; break;
    default: return;
  }
  jref["units_per_second"] = ups;
  jref["bits_per_unit"] = 100000000;
}

static void print_sata_version_and_speed(unsigned short word222,
                                         unsigned short word076,
                                         unsigned short word077)
{
  int allspeeds = (!(word076 & 0x0001) ? (word076 & 0x00fe) : 0);
  int maxspeed = (allspeeds ? find_msb(allspeeds) : 0);
  int curspeed = (!(word077 & 0x0001) ? ((word077 >> 1) & 0x7) : 0);

  const char * verstr = get_sata_version(word222);
  const char * maxstr = get_sata_speed(maxspeed);
  const char * curstr = get_sata_speed(curspeed);
  jout("SATA Version is:  %s%s%s%s%s%s\n",
       (verstr ? verstr : "Unknown"),
       (maxstr ? ", " : ""), (maxstr ? maxstr : ""),
       (curstr ? " (current: " : ""), (curstr ? curstr : ""),
       (curstr ? ")" : ""));
  if (verstr)
    jglb["sata_version"]["string"] = verstr;
  jglb["sata_version"]["value"] = word222 & 0x0fff;
  jset_sata_speed("max", allspeeds, maxspeed, maxstr);
  jset_sata_speed("current", curspeed, curspeed, curstr);
}

static void print_drive_info(const ata_identify_device * drive,
                             const ata_size_info & sizes, int rpm,
                             const drive_settings * dbentry, const char * dbversion)
{
  // format drive information (with byte swapping as needed)
  char model[40+1], serial[20+1], firmware[8+1];
  ata_format_id_string(model, drive->model, sizeof(model)-1);
  ata_format_id_string(serial, drive->serial_no, sizeof(serial)-1);
  ata_format_id_string(firmware, drive->fw_rev, sizeof(firmware)-1);

  // Print model family if known
  if (dbentry && *dbentry->modelfamily) {
    jout("Model Family:     %s\n", dbentry->modelfamily);
    jglb["model_family"] = dbentry->modelfamily;
  }

  jout("Device Model:     %s\n", infofound(model));
  jglb["model_name"] = model;

  if (!dont_print_serial_number) {
    jout("Serial Number:    %s\n", infofound(serial));
    jglb["serial_number"] = serial;

    unsigned oui = 0; uint64_t unique_id = 0;
    int naa = ata_get_wwn(drive, oui, unique_id);
    if (naa >= 0) {
      jout("LU WWN Device Id: %x %06x %09" PRIx64 "\n", naa, oui, unique_id);
      jglb["wwn"]["naa"] = naa;
      jglb["wwn"]["oui"] = oui;
      jglb["wwn"]["id"]  = unique_id;
    }
  }

  // Additional Product Identifier (OEM Id) string in words 170-173
  // (e08130r1, added in ACS-2 Revision 1, December 17, 2008)
  if (0x2020 <= drive->words088_255[170-88] && drive->words088_255[170-88] <= 0x7e7e) {
    char add[8+1];
    ata_format_id_string(add, (const unsigned char *)(drive->words088_255+(170-88)), sizeof(add)-1);
    if (add[0]) {
      jout("Add. Product Id:  %s\n", add);
      jglb["ata_additional_product_id"] = add;
    }
  }

  jout("Firmware Version: %s\n", infofound(firmware));
  jglb["firmware_version"] = firmware;

  if (sizes.capacity) {
    // Print capacity
    char num[64], cap[32];
    jout("User Capacity:    %s bytes [%s]\n",
      format_with_thousands_sep(num, sizeof(num), sizes.capacity),
      format_capacity(cap, sizeof(cap), sizes.capacity));
    jglb["user_capacity"]["blocks"].set_unsafe_uint64(sizes.sectors);
    jglb["user_capacity"]["bytes"].set_unsafe_uint64(sizes.capacity);

    // Print sector sizes.
    if (sizes.phy_sector_size == sizes.log_sector_size)
      jout("Sector Size:      %u bytes logical/physical\n", sizes.log_sector_size);
    else {
      jout("Sector Sizes:     %u bytes logical, %u bytes physical",
        sizes.log_sector_size, sizes.phy_sector_size);
      if (sizes.log_sector_offset)
        pout(" (offset %u bytes)", sizes.log_sector_offset);
      jout("\n");
    }
    jglb["logical_block_size"]  = sizes.log_sector_size;
    jglb["physical_block_size"] = sizes.phy_sector_size;
  }

  // Print nominal media rotation rate if reported
  if (rpm) {
    if (rpm == 1)
      jout("Rotation Rate:    Solid State Device\n");
    else if (rpm > 1)
      jout("Rotation Rate:    %d rpm\n", rpm);
    else
      pout("Rotation Rate:    Unknown (0x%04x)\n", -rpm);
    if (rpm > 0)
      jglb["rotation_rate"] = (rpm == 1 ? 0 : rpm);
  }

  // Print form factor if reported
  unsigned short word168 = drive->words088_255[168-88];
  if (word168) {
    const char * form_factor = get_form_factor(word168);
    if (form_factor)
      jout("Form Factor:      %s\n", form_factor);
    else
      jout("Form Factor:      Unknown (0x%04x)\n", word168);
    jglb["form_factor"]["ata_value"] = word168;
    if (form_factor)
      jglb["form_factor"]["name"] = form_factor;
  }

  // Print TRIM support
  bool trim_sup = !!(drive->words088_255[169-88] & 0x0001);
  unsigned short word069 = drive->words047_079[69-47];
  bool trim_det = !!(word069 & 0x4000), trim_zeroed = !!(word069 & 0x0020);
  if (trim_sup || rpm == 1) // HDD: if supported (SMR), SSD: always
    jout("TRIM Command:     %s%s%s\n",
         (!trim_sup ? "Unavailable" : "Available"),
         (!(trim_sup && trim_det) ? "" : ", deterministic"),
         (!(trim_sup && trim_zeroed) ? "" : ", zeroed")     );
  jglb["trim"]["supported"] = trim_sup;
  if (trim_sup) {
    jglb["trim"]["deterministic"] = trim_det;
    jglb["trim"]["zeroed"] = trim_zeroed;
  }

  // Print Zoned Device Capabilities if reported
  // (added in ACS-4, obsoleted in ACS-5)
  unsigned short zoned_caps = word069 & 0x3;
  if (zoned_caps) {
    jout("Zoned Device:     %s\n",
         (zoned_caps == 0x1 ? "Host Aware Zones" :
          zoned_caps == 0x2 ? "Device managed zones" : "Unknown (0x3)"));
    if (zoned_caps < 0x3)
      jglb["zoned_device"]["capabilities"] = (zoned_caps == 0x1 ? "host_aware" : "device_managed");
  }

  // See if drive is recognized
  jout("Device is:        %s%s%s\n",
       (dbentry ? "In smartctl database" : "Not in smartctl database"),
       (*dbversion ? " " : ""), (*dbversion ? dbversion : ""));
  jglb["in_smartctl_database"] = !!dbentry;

  // Print ATA version
  std::string ataver;
  if (   (drive->major_rev_num != 0x0000 && drive->major_rev_num != 0xffff)
      || (drive->minor_rev_num != 0x0000 && drive->minor_rev_num != 0xffff)) {
    const char * majorver = get_ata_major_version(drive);
    const char * minorver = get_ata_minor_version(drive);

    if (majorver && minorver && str_starts_with(minorver, majorver)) {
      // Major and minor strings match, print minor string only
      ataver = minorver;
    }
    else {
      if (majorver)
        ataver = majorver;
      else
        ataver = strprintf("Unknown(0x%04x)", drive->major_rev_num);

      if (minorver)
        ataver += strprintf(", %s", minorver);
      else if (drive->minor_rev_num != 0x0000 && drive->minor_rev_num != 0xffff)
        ataver += strprintf(" (unknown minor revision code: 0x%04x)", drive->minor_rev_num);
      else
        ataver += " (minor revision not indicated)";
    }
  }
  jout("ATA Version is:   %s\n", infofound(ataver.c_str()));
  if (!ataver.empty()) {
    jglb["ata_version"]["string"] = ataver;
    jglb["ata_version"]["major_value"] = drive->major_rev_num;
    jglb["ata_version"]["minor_value"] = drive->minor_rev_num;
  }

  // Print Transport specific version
  unsigned short word222 = drive->words088_255[222-88];
  if (word222 != 0x0000 && word222 != 0xffff) switch (word222 >> 12) {
    case 0x0: // PATA
      {
        char buf[32] = "";
        pout("Transport Type:   Parallel, %s\n", get_pata_version(word222, buf));
      }
      break;
    case 0x1: // SATA
      print_sata_version_and_speed(word222,
                                   drive->words047_079[76-47],
                                   drive->words047_079[77-47]);
      break;
    case 0xe: // PCIe (ACS-4)
      pout("Transport Type:   PCIe (0x%03x)\n", word222 & 0x0fff);
      break;
    default:
      pout("Transport Type:   Unknown (0x%04x)\n", word222);
      break;
  }

  jout_startup_datetime("Local Time is:    ");

  // Print warning message, if there is one
  if (dbentry && *dbentry->warningmsg)
    pout("\n==> WARNING: %s\n\n", dbentry->warningmsg);
}

static const char *OfflineDataCollectionStatus(unsigned char status_byte)
{
  unsigned char stat=status_byte & 0x7f;
  
  switch(stat){
  case 0x00:
    return "was never started";
  case 0x02:
    return "was completed without error";
  case 0x03:
    if (status_byte == 0x03)
      return "is in progress";
    else
      return "is in a Reserved state";
  case 0x04:
    return "was suspended by an interrupting command from host";
  case 0x05:
    return "was aborted by an interrupting command from host";
  case 0x06:
    return "was aborted by the device with a fatal error";
  default:
    if (stat >= 0x40)
      return "is in a Vendor Specific state";
    else
      return "is in a Reserved state";
  }
}
  
  
//  prints verbose value Off-line data collection status byte
static void PrintSmartOfflineStatus(const ata_smart_values * data)
{
  json::ref jref = jglb["ata_smart_data"]["offline_data_collection"]["status"];

  jout("Offline data collection status:  (0x%02x)\t",
       (int)data->offline_data_collection_status);
  jref["value"] = data->offline_data_collection_status;
    
  // Off-line data collection status byte is not a reserved
  // or vendor specific value
  jout("Offline data collection activity\n"
       "\t\t\t\t\t%s.\n", OfflineDataCollectionStatus(data->offline_data_collection_status));
  jref["string"] = OfflineDataCollectionStatus(data->offline_data_collection_status);
  switch (data->offline_data_collection_status & 0x7f) {
    case 0x02: jref["passed"] = true; break;
    case 0x06: jref["passed"] = false; break;
  }

  // Report on Automatic Data Collection Status.  Only IBM documents
  // this bit.  See SFF 8035i Revision 2 for details.
  if (data->offline_data_collection_status & 0x80)
    pout("\t\t\t\t\tAuto Offline Data Collection: Enabled.\n");
  else
    pout("\t\t\t\t\tAuto Offline Data Collection: Disabled.\n");
  
  return;
}

static void PrintSmartSelfExecStatus(const ata_smart_values * data,
                                     firmwarebug_defs firmwarebugs)
{
  unsigned char status = data->self_test_exec_status;
  jout("Self-test execution status:      ");

  switch (data->self_test_exec_status >> 4) {
    case 0:
      jout("(%4d)\tThe previous self-test routine completed\n\t\t\t\t\t", status);
      jout("without error or no self-test has ever \n\t\t\t\t\tbeen run.\n");
      break;
    case 1:
      jout("(%4d)\tThe self-test routine was aborted by\n\t\t\t\t\t", status);
      jout("the host.\n");
      break;
    case 2:
      jout("(%4d)\tThe self-test routine was interrupted\n\t\t\t\t\t", status);
      jout("by the host with a hard or soft reset.\n");
      break;
    case 3:
      jout("(%4d)\tA fatal error or unknown test error\n\t\t\t\t\t", status);
      jout("occurred while the device was executing\n\t\t\t\t\t");
      jout("its self-test routine and the device \n\t\t\t\t\t");
      jout("was unable to complete the self-test \n\t\t\t\t\t");
      jout("routine.\n");
      break;
    case 4:
      jout("(%4d)\tThe previous self-test completed having\n\t\t\t\t\t", status);
      jout("a test element that failed and the test\n\t\t\t\t\t");
      jout("element that failed is not known.\n");
      break;
    case 5:
      jout("(%4d)\tThe previous self-test completed having\n\t\t\t\t\t", status);
      jout("the electrical element of the test\n\t\t\t\t\t");
      jout("failed.\n");
      break;
    case 6:
      jout("(%4d)\tThe previous self-test completed having\n\t\t\t\t\t", status);
      jout("the servo (and/or seek) element of the \n\t\t\t\t\t");
      jout("test failed.\n");
      break;
    case 7:
      jout("(%4d)\tThe previous self-test completed having\n\t\t\t\t\t", status);
      jout("the read element of the test failed.\n");
      break;
    case 8:
      jout("(%4d)\tThe previous self-test completed having\n\t\t\t\t\t", status);
      jout("a test element that failed and the\n\t\t\t\t\t");
      jout("device is suspected of having handling\n\t\t\t\t\t");
      jout("damage.\n");
      break;
    case 15:
      if (firmwarebugs.is_set(BUG_SAMSUNG3) && data->self_test_exec_status == 0xf0) {
        pout("(%4d)\tThe previous self-test routine completed\n\t\t\t\t\t", status);
        pout("with unknown result or self-test in\n\t\t\t\t\t");
        pout("progress with less than 10%% remaining.\n");
      }
      else {
        jout("(%4d)\tSelf-test routine in progress...\n\t\t\t\t\t", status);
        jout("%1d0%% of test remaining.\n", status & 0x0f);
      }
      break;
    default:
      jout("(%4d)\tReserved.\n", status);
      break;
  }

  json::ref jref = jglb["ata_smart_data"]["self_test"]["status"];

  jref["value"] = status;
  const char * msg;
  // TODO: Use common function for smartctl/smartd
  switch (status >> 4) {
    case 0x0: msg = "completed without error"; break;
    case 0x1: msg = "was aborted by the host"; break;
    case 0x2: msg = "was interrupted by the host with a reset"; break;
    case 0x3: msg = "could not complete due to a fatal or unknown error"; break;
    case 0x4: msg = "completed with error (unknown test element)"; break;
    case 0x5: msg = "completed with error (electrical test element)"; break;
    case 0x6: msg = "completed with error (servo/seek test element)"; break;
    case 0x7: msg = "completed with error (read test element)"; break;
    case 0x8: msg = "completed with error (handling damage?)"; break;
    default:  msg = 0;
  }
  if (msg) {
    jref["string"] = msg;
    switch (status >> 4) {
      case 0x1: case 0x2: case 0x3: break; // aborted -> unknown
      default: jref["passed"] = ((status >> 4) == 0x0);
    }
  }
  else if ((status >> 4) == 0xf) {
    jref["string"] = strprintf("in progress, %u0%% remaining", status & 0xf);
    jref["remaining_percent"] = (status & 0xf) * 10;
  }
}

static void PrintSmartTotalTimeCompleteOffline (const ata_smart_values * data)
{
  jout("Total time to complete Offline \n");
  jout("data collection: \t\t(%5d) seconds.\n",
       (int)data->total_time_to_complete_off_line);

  jglb["ata_smart_data"]["offline_data_collection"]["completion_seconds"] =
      data->total_time_to_complete_off_line;
}

static void PrintSmartOfflineCollectCap(const ata_smart_values *data)
{
  json::ref jref = jglb["ata_smart_data"]["capabilities"];

  jout("Offline data collection\n");
  jout("capabilities: \t\t\t (0x%02x) ",
       (int)data->offline_data_collection_capability);
  jref["values"][0] = data->offline_data_collection_capability;
  
  if (data->offline_data_collection_capability == 0x00){
    jout("\tOffline data collection not supported.\n");
  } 
  else {
    jout( "%s\n", isSupportExecuteOfflineImmediate(data)?
          "SMART execute Offline immediate." :
          "No SMART execute Offline immediate.");
    jref["exec_offline_immediate_supported"] = isSupportExecuteOfflineImmediate(data);

    // TODO: Bit 1 is vendor specific
    pout( "\t\t\t\t\t%s\n", isSupportAutomaticTimer(data)? 
          "Auto Offline data collection on/off support.":
          "No Auto Offline data collection support.");

    jout( "\t\t\t\t\t%s\n", isSupportOfflineAbort(data)?
          "Abort Offline collection upon new\n\t\t\t\t\tcommand.":
          "Suspend Offline collection upon new\n\t\t\t\t\tcommand.");
    jref["offline_is_aborted_upon_new_cmd"] = isSupportOfflineAbort(data);

    jout( "\t\t\t\t\t%s\n", isSupportOfflineSurfaceScan(data)?
          "Offline surface scan supported.":
          "No Offline surface scan supported.");
    jref["offline_surface_scan_supported"] = isSupportOfflineSurfaceScan(data);

    jout( "\t\t\t\t\t%s\n", isSupportSelfTest(data)?
          "Self-test supported.":
          "No Self-test supported.");
    jref["self_tests_supported"] = isSupportSelfTest(data);

    jout( "\t\t\t\t\t%s\n", isSupportConveyanceSelfTest(data)?
          "Conveyance Self-test supported.":
          "No Conveyance Self-test supported.");
    jref["conveyance_self_test_supported"] = isSupportConveyanceSelfTest(data);

    jout( "\t\t\t\t\t%s\n", isSupportSelectiveSelfTest(data)?
          "Selective Self-test supported.":
          "No Selective Self-test supported.");
    jref["selective_self_test_supported"] = isSupportSelectiveSelfTest(data);
  }
}

static void PrintSmartCapability(const ata_smart_values *data)
{
  json::ref jref = jglb["ata_smart_data"]["capabilities"];

  jout("SMART capabilities:            ");
  jout("(0x%04x)\t", (int)data->smart_capability);
  jref["values"][1] = data->smart_capability;

  if (data->smart_capability == 0x00)
    jout("Automatic saving of SMART data\t\t\t\t\tis not implemented.\n");
  else {
    jout("%s\n", (data->smart_capability & 0x01)?
         "Saves SMART data before entering\n\t\t\t\t\tpower-saving mode.":
         "Does not save SMART data before\n\t\t\t\t\tentering power-saving mode.");
    jref["attribute_autosave_enabled"] = !!(data->smart_capability & 0x01);

    // TODO: Info possibly invalid or misleading
    // ATA-3 - ATA-5: Bit shall be set
    // ATA-6 - ACS-3: Bit shall be set to indicate support for
    // SMART ENABLE/DISABLE ATTRIBUTE AUTOSAVE
    if (data->smart_capability & 0x02)
      pout("\t\t\t\t\tSupports SMART auto save timer.\n");
  }
}

static void PrintSmartErrorLogCapability(const ata_smart_values * data, const ata_identify_device * identity)
{
  bool capable = isSmartErrorLogCapable(data, identity);
  jout("Error logging capability:        (0x%02x)\tError logging %ssupported.\n",
       data->errorlog_capability, (capable ? "" : "NOT "));
  jglb["ata_smart_data"]["capabilities"]["error_logging_supported"] = capable;
}

static void PrintSmartShortSelfTestPollingTime(const ata_smart_values * data)
{
  jout("Short self-test routine \n");
  if (isSupportSelfTest(data)) {
    jout("recommended polling time: \t (%4d) minutes.\n",
         (int)data->short_test_completion_time);
    jglb["ata_smart_data"]["self_test"]["polling_minutes"]["short"] =
        data->short_test_completion_time;
  }
  else
    jout("recommended polling time: \t        Not Supported.\n");
}

static void PrintSmartExtendedSelfTestPollingTime(const ata_smart_values * data)
{
  jout("Extended self-test routine\n");
  if (isSupportSelfTest(data)) {
    jout("recommended polling time: \t (%4d) minutes.\n",
         TestTime(data, EXTEND_SELF_TEST));
    jglb["ata_smart_data"]["self_test"]["polling_minutes"]["extended"] =
        TestTime(data, EXTEND_SELF_TEST);
  }
  else
    jout("recommended polling time: \t        Not Supported.\n");
}

static void PrintSmartConveyanceSelfTestPollingTime(const ata_smart_values * data)
{
  jout("Conveyance self-test routine\n");
  if (isSupportConveyanceSelfTest(data)) {
    jout("recommended polling time: \t (%4d) minutes.\n",
         (int)data->conveyance_test_completion_time);
    jglb["ata_smart_data"]["self_test"]["polling_minutes"]["conveyance"] =
        data->conveyance_test_completion_time;
  }
  else
    jout("recommended polling time: \t        Not Supported.\n");
}

// Check SMART attribute table for Threshold failure
// onlyfailed=0: are or were any age or prefailure attributes <= threshold
// onlyfailed=1: are any prefailure attributes <= threshold now
static int find_failed_attr(const ata_smart_values * data,
                            const ata_smart_thresholds_pvt * thresholds,
                            const ata_vendor_attr_defs & defs, int onlyfailed)
{
  for (int i = 0; i < NUMBER_ATA_SMART_ATTRIBUTES; i++) {
    const ata_smart_attribute & attr = data->vendor_attributes[i];

    ata_attr_state state = ata_get_attr_state(attr, i, thresholds->thres_entries, defs);

    if (!onlyfailed) {
      if (state >= ATTRSTATE_FAILED_PAST)
        return attr.id;
    }
    else {
      if (state == ATTRSTATE_FAILED_NOW && ATTRIBUTE_FLAGS_PREFAILURE(attr.flags))
        return attr.id;
    }
  }
  return 0;
}

static void set_json_globals_from_smart_attrib(int id, const char * name,
                                               const ata_vendor_attr_defs & defs,
                                               uint64_t rawval)
{
  switch (id) {
    case 9:
      if (!str_starts_with(name, "Power_On_"))
        return;
      {
        int minutes = -1;
        switch (defs[id].raw_format) {
          case RAWFMT_RAW48: case RAWFMT_RAW64:
          case RAWFMT_RAW16_OPT_RAW16: case RAWFMT_RAW24_OPT_RAW8: break;
          case RAWFMT_SEC2HOUR: minutes = (rawval / 60) % 60; rawval /= 60*60; break;
          case RAWFMT_MIN2HOUR: minutes = rawval % 60; rawval /= 60; break;
          case RAWFMT_HALFMIN2HOUR: minutes = (rawval / 2) % 60; rawval /= 2*60; break;
          case RAWFMT_DEFAULT: // No database entry:
            rawval &= 0xffffffffULL; // ignore milliseconds from RAWFMT_MSEC24_HOUR32
            break;
          case RAWFMT_MSEC24_HOUR32:
            minutes = (int)(rawval >> 32) / (1000*60);
            if (minutes >= 60)
              minutes = -1;
            rawval &= 0xffffffffULL;
            break;
          default: return;
        }
        if (rawval > 0x00ffffffULL)
          return; // assume bogus value
        jglb["power_on_time"]["hours"] = rawval;
        if (minutes >= 0)
          jglb["power_on_time"]["minutes"] = minutes;
      }
      break;
    case 12:
      if (strcmp(name, "Power_Cycle_Count"))
        return;
      switch (defs[id].raw_format) {
        case RAWFMT_DEFAULT: case RAWFMT_RAW48: case RAWFMT_RAW64:
        case RAWFMT_RAW16_OPT_RAW16: case RAWFMT_RAW24_OPT_RAW8: break;
        default: return;
      }
      if (rawval > 0x00ffffffULL)
        return; // assume bogus value
      jglb["power_cycle_count"] = rawval;
      break;
    //case 194:
    // Temperature set separately from ata_return_temperature_value() below
  }
}

// onlyfailed=0 : print all attribute values
// onlyfailed=1:  just ones that are currently failed and have prefailure bit set
// onlyfailed=2:  ones that are failed, or have failed with or without prefailure bit set
static void PrintSmartAttribWithThres(const ata_smart_values * data,
                                      const ata_smart_thresholds_pvt * thresholds,
                                      const ata_vendor_attr_defs & defs, int rpm,
                                      int onlyfailed, unsigned char format)
{
  bool brief  = !!(format & ata_print_options::FMT_BRIEF);
  bool hexid  = !!(format & ata_print_options::FMT_HEX_ID);
  bool hexval = !!(format & ata_print_options::FMT_HEX_VAL);
  bool needheader = true;

  // step through all vendor attributes
  for (int i = 0, ji = 0; i < NUMBER_ATA_SMART_ATTRIBUTES; i++) {
    const ata_smart_attribute & attr = data->vendor_attributes[i];

    // Check attribute and threshold
    unsigned char threshold = 0;
    ata_attr_state state = ata_get_attr_state(attr, i, thresholds->thres_entries, defs, &threshold);
    if (state == ATTRSTATE_NON_EXISTING)
      continue;

    // These break out of the loop if we are only printing certain entries...
    if (onlyfailed == 1 && !(ATTRIBUTE_FLAGS_PREFAILURE(attr.flags) && state == ATTRSTATE_FAILED_NOW))
      continue;

    if (onlyfailed == 2 && state < ATTRSTATE_FAILED_PAST)
      continue;

    // print header only if needed
    if (needheader) {
      if (!onlyfailed) {
        jout("SMART Attributes Data Structure revision number: %d\n",(int)data->revnumber);
        jglb["ata_smart_attributes"]["revision"] = data->revnumber;
        jout("Vendor Specific SMART Attributes with Thresholds:\n");
      }
      if (!brief)
        jout("ID#%s ATTRIBUTE_NAME          FLAG     VALUE WORST THRESH TYPE      UPDATED  WHEN_FAILED RAW_VALUE\n",
             (!hexid ? "" : " "));
      else
        jout("ID#%s ATTRIBUTE_NAME          FLAGS    VALUE WORST THRESH FAIL RAW_VALUE\n",
             (!hexid ? "" : " "));
      needheader = false;
    }

    // Format value, worst, threshold
    std::string valstr, worstr, threstr;
    if (state > ATTRSTATE_NO_NORMVAL)
      valstr = (!hexval ? strprintf("%.3d",   attr.current)
                        : strprintf("0x%02x", attr.current));
    else
      valstr = (!hexval ? "---" : "----");
    if (!(defs[attr.id].flags & ATTRFLAG_NO_WORSTVAL))
      worstr = (!hexval ? strprintf("%.3d",   attr.worst)
                        : strprintf("0x%02x", attr.worst));
    else
      worstr = (!hexval ? "---" : "----");
    if (state > ATTRSTATE_NO_THRESHOLD)
      threstr = (!hexval ? strprintf("%.3d",   threshold)
                         : strprintf("0x%02x", threshold));
    else
      threstr = (!hexval ? "---" : "----");

    // Print line for each valid attribute
    std::string idstr = (!hexid ? strprintf("%3d",    attr.id)
                                : strprintf("0x%02x", attr.id));
    std::string attrname = ata_get_smart_attr_name(attr.id, defs, rpm);
    std::string rawstr = ata_format_attr_raw_value(attr, defs);

    char flagstr[] = {
      (ATTRIBUTE_FLAGS_PREFAILURE(attr.flags)     ? 'P' : '-'),
      (ATTRIBUTE_FLAGS_ONLINE(attr.flags)         ? 'O' : '-'),
      (ATTRIBUTE_FLAGS_PERFORMANCE(attr.flags)    ? 'S' : '-'),
      (ATTRIBUTE_FLAGS_ERRORRATE(attr.flags)      ? 'R' : '-'),
      (ATTRIBUTE_FLAGS_EVENTCOUNT(attr.flags)     ? 'C' : '-'),
      (ATTRIBUTE_FLAGS_SELFPRESERVING(attr.flags) ? 'K' : '-'),
      (ATTRIBUTE_FLAGS_OTHER(attr.flags)          ? '+' : ' '),
      0
    };

    if (!brief)
      jout("%s %-24s0x%04x   %-4s  %-4s  %-4s   %-10s%-9s%-12s%s\n",
           idstr.c_str(), attrname.c_str(), attr.flags,
           valstr.c_str(), worstr.c_str(), threstr.c_str(),
           (ATTRIBUTE_FLAGS_PREFAILURE(attr.flags) ? "Pre-fail" : "Old_age"),
           (ATTRIBUTE_FLAGS_ONLINE(attr.flags)     ? "Always"   : "Offline"),
           (state == ATTRSTATE_FAILED_NOW  ? "FAILING_NOW" :
            state == ATTRSTATE_FAILED_PAST ? "In_the_past"
                                           : "    -"        ) ,
            rawstr.c_str());
    else
      jout("%s %-24s%s  %-4s  %-4s  %-4s   %-5s%s\n",
           idstr.c_str(), attrname.c_str(), flagstr,
           valstr.c_str(), worstr.c_str(), threstr.c_str(),
           (state == ATTRSTATE_FAILED_NOW  ? "NOW"  :
            state == ATTRSTATE_FAILED_PAST ? "Past"
                                           : "-"     ),
            rawstr.c_str());

    if (!jglb.is_enabled())
      continue;

    json::ref jref = jglb["ata_smart_attributes"]["table"][ji++];
    jref["id"] = attr.id;
    jref["name"] = attrname;
    if (state > ATTRSTATE_NO_NORMVAL)
      jref["value"] = attr.current;
    if (!(defs[attr.id].flags & ATTRFLAG_NO_WORSTVAL))
      jref["worst"] = attr.worst;
    if (state > ATTRSTATE_NO_THRESHOLD) {
      jref["thresh"] = threshold;
      jref["when_failed"] = (state == ATTRSTATE_FAILED_NOW  ? "now" :
                             state == ATTRSTATE_FAILED_PAST ? "past"
                                                            : ""     );
    }

    json::ref jreff = jref["flags"];
    jreff["value"] = attr.flags;
    jreff["string"] = flagstr;
    jreff["prefailure"]     = !!ATTRIBUTE_FLAGS_PREFAILURE(attr.flags);
    jreff["updated_online"] = !!ATTRIBUTE_FLAGS_ONLINE(attr.flags);
    jreff["performance"]    = !!ATTRIBUTE_FLAGS_PERFORMANCE(attr.flags);
    jreff["error_rate"]     = !!ATTRIBUTE_FLAGS_ERRORRATE(attr.flags);
    jreff["event_count"]    = !!ATTRIBUTE_FLAGS_EVENTCOUNT(attr.flags);
    jreff["auto_keep"]      = !!ATTRIBUTE_FLAGS_SELFPRESERVING(attr.flags);
    if (ATTRIBUTE_FLAGS_OTHER(attr.flags))
      jreff["other"] = ATTRIBUTE_FLAGS_OTHER(attr.flags);

    uint64_t rawval = ata_get_attr_raw_value(attr, defs);
    jref["raw"]["value"] = rawval;
    jref["raw"]["string"] = rawstr;

    set_json_globals_from_smart_attrib(attr.id, attrname.c_str(), defs, rawval);
  }

  if (!needheader) {
    if (!onlyfailed && brief) {
        int n = (!hexid ? 28 : 29);
        jout("%*s||||||_ K auto-keep\n"
             "%*s|||||__ C event count\n"
             "%*s||||___ R error rate\n"
             "%*s|||____ S speed/performance\n"
             "%*s||_____ O updated online\n"
             "%*s|______ P prefailure warning\n",
             n, "", n, "", n, "", n, "", n, "", n, "");
    }
    pout("\n");
  }

  if (!jglb.is_enabled())
    return;

  // Protocol independent temperature
  unsigned char t = ata_return_temperature_value(data, defs);
  if (t)
    jglb["temperature"]["current"] = t;
}

// Print SMART related SCT capabilities
static void ataPrintSCTCapability(const ata_identify_device *drive)
{
  unsigned short sctcaps = drive->words088_255[206-88];
  if (!(sctcaps & 0x01))
    return;
  json::ref jref = jglb["ata_sct_capabilities"];
  jout("SCT capabilities: \t       (0x%04x)\tSCT Status supported.\n", sctcaps);
  jref["value"] = sctcaps;
  if (sctcaps & 0x08)
    jout("\t\t\t\t\tSCT Error Recovery Control supported.\n");
  jref["error_recovery_control_supported"] = !!(sctcaps & 0x08);
  if (sctcaps & 0x10)
    jout("\t\t\t\t\tSCT Feature Control supported.\n");
  jref["feature_control_supported"] = !!(sctcaps & 0x10);
  if (sctcaps & 0x20)
    jout("\t\t\t\t\tSCT Data Table supported.\n");
  jref["data_table_supported"] = !!(sctcaps & 0x20);
}


static void PrintGeneralSmartValues(const ata_smart_values *data, const ata_identify_device *drive,
                                    firmwarebug_defs firmwarebugs)
{
  jout("General SMART Values:\n");
  
  PrintSmartOfflineStatus(data); 
  
  if (isSupportSelfTest(data)){
    PrintSmartSelfExecStatus(data, firmwarebugs);
  }
  
  PrintSmartTotalTimeCompleteOffline(data);
  PrintSmartOfflineCollectCap(data);
  PrintSmartCapability(data);
  
  PrintSmartErrorLogCapability(data, drive);

  jout( "\t\t\t\t\t%s\n", isGeneralPurposeLoggingCapable(drive)?
        "General Purpose Logging supported.":
        "No General Purpose Logging support.");
  jglb["ata_smart_data"]["capabilities"]["gp_logging_supported"] =
       isGeneralPurposeLoggingCapable(drive);

  if (isSupportSelfTest(data)){
    PrintSmartShortSelfTestPollingTime (data);
    PrintSmartExtendedSelfTestPollingTime (data);
  }
  if (isSupportConveyanceSelfTest(data))
    PrintSmartConveyanceSelfTestPollingTime (data);

  ataPrintSCTCapability(drive);

  jout("\n");
}

// Get # sectors of a log addr, 0 if log does not exist.
static unsigned GetNumLogSectors(const ata_smart_log_directory * logdir, unsigned logaddr, bool gpl)
{
  if (!logdir)
    return 0;
  if (logaddr > 0xff)
    return 0;
  if (logaddr == 0)
    return 1;
  unsigned n = logdir->entry[logaddr-1].numsectors;
  if (gpl)
    // GP logs may have >255 sectors
    n |= logdir->entry[logaddr-1].reserved << 8;
  return n;
}

// Get name of log.
static const char * GetLogName(unsigned logaddr)
{
    // Table A.2 of T13/2015-D (ACS-2) Revision 7, June 22, 2011
    // Table 112 of Serial ATA Revision 3.2, August 7, 2013
    // Table A.2 of T13/2161-D (ACS-3) Revision 5, October 28, 2013
    // Table 213 of T13/BSR INCITS 529 (ACS-4) Revision 20, October 26, 2017
    // Table 213 of T13/BSR INCITS 558 (ACS-5) Revision 10, March 3, 2021
    switch (logaddr) {
      case 0x00: return "Log Directory";
      case 0x01: return "Summary SMART error log";
      case 0x02: return "Comprehensive SMART error log";
      case 0x03: return "Ext. Comprehensive SMART error log";
      case 0x04: return "Device Statistics log";
      case 0x05: return "Reserved for CFA"; // ACS-2
      case 0x06: return "SMART self-test log"; // OBS-ACS-5
      case 0x07: return "Extended self-test log"; // OBS-ACS-5
      case 0x08: return "Power Conditions log"; // ACS-2
      case 0x09: return "Selective self-test log";
      case 0x0a: return "Device Statistics Notification"; // ACS-3
      case 0x0b: return "Reserved for CFA"; // ACS-3
      case 0x0c: return "Pending Defects log"; // ACS-4
      case 0x0d: return "LPS Mis-alignment log"; // ACS-2
      case 0x0e: return "Reserved for ZAC-2"; // ACS-4
      case 0x0f: return "Sense Data for Successful NCQ Cmds log"; // ACS-4
      case 0x10: return "NCQ Command Error log";
      case 0x11: return "SATA Phy Event Counters log";
    //case 0x12: return "SATA NCQ Queue Management log"; // SATA 3.0/3.1, ACS-3
      case 0x12: return "SATA NCQ Non-Data log"; // SATA 3.2, ACS-4
      case 0x13: return "SATA NCQ Send and Receive log"; // SATA 3.1, ACS-3
      case 0x14: return "Hybrid Information log"; // SATA 3.2, ACS-4
      case 0x15: return "Rebuild Assist log"; // SATA 3.2, ACS-4
      case 0x16: return "Out Of Band Management Control log"; // ACS-5
      case 0x17: return "Reserved for Serial ATA";
      case 0x18: return "Command Duration Limits log"; // ACS-5
      case 0x19: return "LBA Status log"; // ACS-3

      case 0x20: return "Streaming performance log [OBS-8]";
      case 0x21: return "Write stream error log";
      case 0x22: return "Read stream error log";
      case 0x23: return "Delayed sector log [OBS-8]";
      case 0x24: return "Current Device Internal Status Data log"; // ACS-3
      case 0x25: return "Saved Device Internal Status Data log"; // ACS-3

      case 0x2f: return "Set Sector Configuration"; // ACS-4
      case 0x30: return "IDENTIFY DEVICE data log"; // ACS-3

      case 0x42: return "Mutate Configurations log"; // ACS-5

      case 0x47: return "Concurrent Positioning Ranges log"; // ACS-5

      case 0x53: return "Sense Data log"; // ACS-5

      case 0xe0: return "SCT Command/Status";
      case 0xe1: return "SCT Data Transfer";
      default:
        if (0xa0 <= logaddr && logaddr <= 0xdf)
          return "Device vendor specific log";
        if (0x80 <= logaddr && logaddr <= 0x9f)
          return "Host vendor specific log";
        return "Reserved";
    }
    /*NOTREACHED*/
}

// Get log access permissions
static const char * get_log_rw(unsigned logaddr)
{
   if (   (                   logaddr <= 0x08)
       || (0x0c <= logaddr && logaddr <= 0x0d)
       || (0x0f <= logaddr && logaddr <= 0x14)
       || (0x19 == logaddr)
       || (0x20 <= logaddr && logaddr <= 0x25)
       || (0x30 == logaddr)
       || (0x42 == logaddr)
       || (0x47 == logaddr)
       || (0x53 == logaddr))
      return "R/O";

   if (   (                   logaddr <= 0x0a)
       || (0x15 <= logaddr && logaddr <= 0x16)
       || (0x18 == logaddr)
       || (0x80 <= logaddr && logaddr <= 0x9f)
       || (0xe0 <= logaddr && logaddr <= 0xe1))
      return "R/W";

   if (0xa0 <= logaddr && logaddr <= 0xdf)
      return "VS"; // Vendor specific

   return "-"; // Unknown/Reserved
}

// Init a fake log directory, assume that standard logs are supported
const ata_smart_log_directory * fake_logdir(ata_smart_log_directory * logdir,
  const ata_print_options & options)
{
  memset(logdir, 0, sizeof(*logdir));
  logdir->logversion = 255;
  logdir->entry[0x01-1].numsectors = 1;
  logdir->entry[0x03-1].numsectors = (options.smart_ext_error_log + (4-1)) / 4;
  logdir->entry[0x04-1].numsectors = 8;
  logdir->entry[0x06-1].numsectors = 1;
  logdir->entry[0x07-1].numsectors = (options.smart_ext_selftest_log + (19-1)) / 19;
  logdir->entry[0x09-1].numsectors = 1;
  logdir->entry[0x11-1].numsectors = 1;
  return logdir;
}

// Print SMART and/or GP Log Directory
static void PrintLogDirectories(const ata_smart_log_directory * gplogdir,
                                const ata_smart_log_directory * smartlogdir)
{
  json::ref jref = jglb["ata_log_directory"];
  if (gplogdir) {
    jout("General Purpose Log Directory Version %u\n", gplogdir->logversion);
    jref["gp_dir_version"] = gplogdir->logversion;
  }
  if (smartlogdir) {
    jout("SMART %sLog Directory Version %u%s\n",
         (gplogdir ? "          " : ""), smartlogdir->logversion,
         (smartlogdir->logversion==1 ? " [multi-sector log support]" : ""));
    jref["smart_dir_version"] = smartlogdir->logversion;
    jref["smart_dir_multi_sector"] = (smartlogdir->logversion == 1);
  }

  jout("Address    Access  R/W   Size  Description\n");

  for (unsigned i = 0, ji = 0; i <= 0xff; i++) {
    // Get number of sectors
    unsigned smart_numsect = GetNumLogSectors(smartlogdir, i, false);
    unsigned gp_numsect    = GetNumLogSectors(gplogdir   , i, true );

    if (!(smart_numsect || gp_numsect))
      continue; // Log does not exist

    const char * acc; unsigned size;
    if (smart_numsect == gp_numsect) {
      acc = "GPL,SL"; size = gp_numsect;
    }
    else if (!smart_numsect) {
      acc = "GPL"; size = gp_numsect;
    }
    else if (!gp_numsect) {
      acc = "    SL"; size = smart_numsect;
    }
    else {
      acc = 0; size = 0;
    }

    unsigned i2 = i;
    if (acc && ((0x80 <= i && i < 0x9f) || (0xa0 <= i && i < 0xdf))) {
      // Find range of Host/Device vendor specific logs with same size
      unsigned imax = (i < 0x9f ? 0x9f : 0xdf);
      for (unsigned j = i+1; j <= imax; j++) {
          unsigned sn = GetNumLogSectors(smartlogdir, j, false);
          unsigned gn = GetNumLogSectors(gplogdir   , j, true );

          if (!(sn == smart_numsect && gn == gp_numsect))
            break;
          i2 = j;
      }
    }

    const char * name = GetLogName(i);
    const char * rw = get_log_rw(i);

    if (i2 > i)
      jout("0x%02x-0x%02x  %-6s  %-3s  %5u  %s\n", i, i2, acc, rw, size, name);
    else if (acc)
      jout(  "0x%02x       %-6s  %-3s  %5u  %s\n", i, acc, rw, size, name);
    else {
      // GPL and SL support different sizes
      jout(  "0x%02x       %-6s  %-3s  %5u  %s\n", i, "GPL", rw, gp_numsect, name);
      jout(  "0x%02x       %-6s  %-3s  %5u  %s\n", i, "SL", rw, smart_numsect, name);
    }

    for (;;) {
      json::ref jrefi = jref["table"][ji++];
      jrefi["address"] = i;
      jrefi["name"] = name;
      if (rw[0] == 'R' && rw[1] && rw[2]) {
        jrefi["read"] = true;
        jrefi["write"] = (rw[2] == 'W');
      }
      if (gp_numsect)
        jrefi["gp_sectors"] = gp_numsect;
      if (smart_numsect)
        jrefi["smart_sectors"] = smart_numsect;
      if (i >= i2)
        break;
      i++;
    }
  }
  jout("\n");
}

// Print hexdump of log pages.
// Format is compatible with 'xxd -r'.
static void PrintLogPages(const char * type, const unsigned char * data,
                          unsigned char logaddr, unsigned page,
                          unsigned num_pages, unsigned max_pages)
{
  pout("%s Log 0x%02x [%s], Page %u-%u (of %u)\n",
    type, logaddr, GetLogName(logaddr), page, page+num_pages-1, max_pages);
  for (unsigned i = 0; i < num_pages * 512; i += 16) {
    const unsigned char * p = data+i;
    pout("%07x: %02x %02x %02x %02x %02x %02x %02x %02x "
               "%02x %02x %02x %02x %02x %02x %02x %02x ",
         (page * 512) + i,
         p[ 0], p[ 1], p[ 2], p[ 3], p[ 4], p[ 5], p[ 6], p[ 7],
         p[ 8], p[ 9], p[10], p[11], p[12], p[13], p[14], p[15]);
#define P(n) (' ' <= p[n] && p[n] <= '~' ? (int)p[n] : '.')
    pout("|%c%c%c%c%c%c%c%c"
          "%c%c%c%c%c%c%c%c|\n",
         P( 0), P( 1), P( 2), P( 3), P( 4), P( 5), P( 6), P( 7),
         P( 8), P( 9), P(10), P(11), P(12), P(13), P(14), P(15));
#undef P
    if ((i & 0x1ff) == 0x1f0)
      pout("\n");
  }
}

///////////////////////////////////////////////////////////////////////
// Device statistics (Log 0x04)

// Section A.5 of T13/2161-D (ACS-3) Revision 5, October 28, 2013
// Section 9.5 of T13/BSR INCITS 529 (ACS-4) Revision 20, October 26, 2017

struct devstat_entry_info
{
  short size; // #bytes of value, -1 for signed char
  const char * name;
};

const devstat_entry_info devstat_info_0x00[] = {
  {  2, "List of supported log pages" },
  {  0, 0 }
};

const devstat_entry_info devstat_info_0x01[] = {
  {  2, "General Statistics" },
  {  4, "Lifetime Power-On Resets" },
  {  4, "Power-on Hours" },
  {  6, "Logical Sectors Written" },
  {  6, "Number of Write Commands" },
  {  6, "Logical Sectors Read" },
  {  6, "Number of Read Commands" },
  {  6, "Date and Time TimeStamp" }, // ACS-3
  {  4, "Pending Error Count" }, // ACS-4
  {  2, "Workload Utilization" }, // ACS-4
  {  6, "Utilization Usage Rate" }, // ACS-4 (TODO: 47:40: Validity, 39:36 Basis, 7:0 Usage rate)
  {  7, "Resource Availability" }, // ACS-4 (TODO: 55:16 Resources, 15:0 Fraction)
  {  1, "Random Write Resources Used" }, // ACS-4
  {  0, 0 }
};

const devstat_entry_info devstat_info_0x02[] = {
  {  2, "Free-Fall Statistics" },
  {  4, "Number of Free-Fall Events Detected" },
  {  4, "Overlimit Shock Events" },
  {  0, 0 }
};

const devstat_entry_info devstat_info_0x03[] = {
  {  2, "Rotating Media Statistics" },
  {  4, "Spindle Motor Power-on Hours" },
  {  4, "Head Flying Hours" },
  {  4, "Head Load Events" },
  {  4, "Number of Reallocated Logical Sectors" },
  {  4, "Read Recovery Attempts" },
  {  4, "Number of Mechanical Start Failures" },
  {  4, "Number of Realloc. Candidate Logical Sectors" }, // ACS-3
  {  4, "Number of High Priority Unload Events" }, // ACS-3
  {  0, 0 }
};

const devstat_entry_info devstat_info_0x04[] = {
  {  2, "General Errors Statistics" },
  {  4, "Number of Reported Uncorrectable Errors" },
//{  4, "Number of Resets Between Command Acceptance and Command Completion" },
  {  4, "Resets Between Cmd Acceptance and Completion" },
  {  4, "Physical Element Status Changed" }, // ACS-4
  {  0, 0 }
};

const devstat_entry_info devstat_info_0x05[] = {
  {  2, "Temperature Statistics" },
  { -1, "Current Temperature" },
  { -1, "Average Short Term Temperature" },
  { -1, "Average Long Term Temperature" },
  { -1, "Highest Temperature" },
  { -1, "Lowest Temperature" },
  { -1, "Highest Average Short Term Temperature" },
  { -1, "Lowest Average Short Term Temperature" },
  { -1, "Highest Average Long Term Temperature" },
  { -1, "Lowest Average Long Term Temperature" },
  {  4, "Time in Over-Temperature" },
  { -1, "Specified Maximum Operating Temperature" },
  {  4, "Time in Under-Temperature" },
  { -1, "Specified Minimum Operating Temperature" },
  {  0, 0 }
};

const devstat_entry_info devstat_info_0x06[] = {
  {  2, "Transport Statistics" },
  {  4, "Number of Hardware Resets" },
  {  4, "Number of ASR Events" },
  {  4, "Number of Interface CRC Errors" },
  {  0, 0 }
};

const devstat_entry_info devstat_info_0x07[] = {
  {  2, "Solid State Device Statistics" },
  {  1, "Percentage Used Endurance Indicator" },
  {  0, 0 }
};

const devstat_entry_info * devstat_infos[] = {
  devstat_info_0x00,
  devstat_info_0x01,
  devstat_info_0x02,
  devstat_info_0x03,
  devstat_info_0x04,
  devstat_info_0x05,
  devstat_info_0x06,
  devstat_info_0x07
  // TODO: 0x08 Zoned Device Statistics (T13/f16136r7, January 2017)
  // TODO: 0x09 Command Duration Limits Statistics (ACS-5 Revision 10, March 2021)
};

const int num_devstat_infos = sizeof(devstat_infos)/sizeof(devstat_infos[0]);

static const char * get_device_statistics_page_name(int page)
{
  if (page < num_devstat_infos)
    return devstat_infos[page][0].name;
  if (page == 0xff)
    return "Vendor Specific Statistics"; // ACS-4
  return "Unknown Statistics";
}

static void set_json_globals_from_device_statistics(int page, int offset, int64_t val)
{
  switch (page) {
    case 1:
      switch (offset) {
        case 0x008: jglb["power_cycle_count"] = val; break; // ~= Lifetime Power-On Resets
        case 0x010: jglb["power_on_time"]["hours"]= val; break;
      }
      break;
    case 5:
      switch (offset) {
        case 0x008: jglb["temperature"]["current"] = val; break;
        case 0x020: jglb["temperature"]["lifetime_max"] = val; break;
        case 0x028: jglb["temperature"]["lifetime_min"] = val; break;
        case 0x050: jglb["temperature"]["lifetime_over_limit_minutes"] = val; break;
        case 0x058: jglb["temperature"]["op_limit_max"] = val; break;
        case 0x060: jglb["temperature"]["lifetime_under_limit_minutes"] = val; break;
        case 0x068: jglb["temperature"]["op_limit_min"] = val; break;
      }
      break;
  }
}

static void print_device_statistics_page(const json::ref & jref, const unsigned char * data, int page)
{
  const devstat_entry_info * info = (page < num_devstat_infos ? devstat_infos[page] : 0);
  const char * name = get_device_statistics_page_name(page);

  // Check page number in header
  static const char line[] = "  =====  =               =  ===  == ";
  if (!data[2]) {
    pout("0x%02x%s%s (empty) ==\n", page, line, name);
    return;
  }
  if (data[2] != page) {
    pout("0x%02x%s%s (invalid page 0x%02x in header) ==\n", page, line, name, data[2]);
    return;
  }

  int rev = data[0] | (data[1] << 8);
  jout("0x%02x%s%s (rev %d) ==\n", page, line, name, rev);
  jref["number"] = page;
  jref["name"] = name;
  jref["revision"] = rev;

  // Print entries
  int ji = 0;
  for (int i = 1, offset = 8; offset < 512-7; i++, offset+=8) {
    // Check for last known entry
    if (info && !info[i].size)
      info = 0;

    // Skip unsupported entries
    unsigned char flags = data[offset+7];
    if (!(flags & 0x80))
      continue;

    // Stop if unknown entries contain garbage data due to buggy firmware
    if (!info && (data[offset+5] || data[offset+6])) {
      pout("0x%02x  0x%03x  -               -  [Trailing garbage ignored]\n", page, offset);
      break;
    }

    // Get value name
    const char * valname = (info           ? info[i].name :
                            (page == 0xff) ? "Vendor Specific" // ACS-4
                                           : "Unknown"        );

    // Get value size, default to max if unknown
    int size = (info ? info[i].size : 7);

    // Get flags (supported flag already checked above)
    bool valid = !!(flags & 0x40);
    bool normalized = !!(flags & 0x20);
    bool supports_dsn = !!(flags & 0x10); // ACS-3
    bool monitored_condition_met = !!(flags & 0x08); // ACS-3
    unsigned char reserved_flags = (flags & 0x07);

    // Format value
    int64_t val = 0;
    char valstr[32];
    if (valid) {
      // Get value
      if (size < 0) {
        val = (signed char)data[offset];
      }
      else {
        for (int j = 0; j < size; j++)
          val |= (int64_t)data[offset+j] << (j*8);
      }
      snprintf(valstr, sizeof(valstr), "%" PRId64, val);
    }
    else {
      // Value not known (yet)
      valstr[0] = '-'; valstr[1] = 0;
    }

    char flagstr[] = {
      (valid ? 'V' : '-'), // JSON only
      (normalized ? 'N' : '-'),
      (supports_dsn ? 'D' : '-'),
      (monitored_condition_met ? 'C' : '-'),
      (reserved_flags ? '+' : ' '),
      0
    };

    jout("0x%02x  0x%03x  %d %15s  %s %s\n",
      page, offset, abs(size), valstr, flagstr+1, valname);

    if (!jglb.is_enabled())
      continue;

    json::ref jrefi = jref["table"][ji++];
    jrefi["offset"] = offset;
    jrefi["name"] = valname;
    jrefi["size"] = abs(size);
    if (valid)
      jrefi["value"] = val; // TODO: May be unsafe JSON int if size > 6

    json::ref jreff = jrefi["flags"];
    jreff["value"] = flags;
    jreff["string"] = flagstr;
    jreff["valid"] = valid;
    jreff["normalized"] = normalized;
    jreff["supports_dsn"] = supports_dsn;
    jreff["monitored_condition_met"] = monitored_condition_met;
    if (reserved_flags)
      jreff["other"] = reserved_flags;

    if (valid)
      set_json_globals_from_device_statistics(page, offset, val);
  }
}

static bool print_device_statistics(ata_device * device, unsigned nsectors,
  const std::vector<int> & single_pages, bool all_pages, bool ssd_page,
  bool use_gplog)
{
  // Read list of supported pages from page 0
  unsigned char page_0[512] = {0, };
  int rc;
  
  if (use_gplog)
    rc = ataReadLogExt(device, 0x04, 0, 0, page_0, 1);
  else
    rc = ataReadSmartLog(device, 0x04, page_0, 1);
  if (!rc) {
    jerr("Read Device Statistics page 0x00 failed\n\n");
    return false;
  }

  unsigned char nentries = page_0[8];
  if (!(page_0[2] == 0 && nentries > 0)) {
    jerr("Device Statistics page 0x00 is invalid (page=0x%02x, nentries=%d)\n\n", page_0[2], nentries);
    return false;
  }

  // Prepare list of pages to print
  std::vector<int> pages;
  unsigned i;
  if (all_pages) {
    // Add all supported pages
    for (i = 0; i < nentries; i++) {
      int page = page_0[8+1+i];
      if (page)
        pages.push_back(page);
    }
    ssd_page = false;
  }
  // Add manually specified pages
  bool print_page_0 = false;
  for (i = 0; i < single_pages.size() || ssd_page; i++) {
    int page = (i < single_pages.size() ? single_pages[i] : 0x07);
    if (!page)
      print_page_0 = true;
    else if (page >= (int)nsectors)
      pout("Device Statistics Log has only 0x%02x pages\n", nsectors);
    else
      pages.push_back(page);
    if (page == 0x07)
      ssd_page = false;
  }

  json::ref jref = jglb["ata_device_statistics"];

  // Print list of supported pages if requested
  if (print_page_0) {
    pout("Device Statistics (%s Log 0x04) supported pages\n", 
      use_gplog ? "GP" : "SMART");
    jout("Page  Description\n");
    for (i = 0; i < nentries; i++) {
      int page = page_0[8+1+i];
      const char * name = get_device_statistics_page_name(page);
      jout("0x%02x  %s\n", page, name);
      jref["supported_pages"][i]["number"] = page;
      jref["supported_pages"][i]["name"] = name;
    }
    jout("\n");
  }

  // Read & print pages
  if (!pages.empty()) {
    pout("Device Statistics (%s Log 0x04)\n",
      use_gplog ? "GP" : "SMART");
    jout("Page  Offset Size        Value Flags Description\n");
    int max_page = 0;

    if (!use_gplog)
      for (i = 0; i < pages.size(); i++) {
        int page = pages[i];
        if (max_page < page && page < 0xff)
          max_page = page;
      }

    raw_buffer pages_buf((max_page+1) * 512);

    if (!use_gplog && !ataReadSmartLog(device, 0x04, pages_buf.data(), max_page+1)) {
      jerr("Read Device Statistics pages 0x00-0x%02x failed\n\n", max_page);
      return false;
    }

    int ji = 0;
    for (i = 0; i <  pages.size(); i++) {
      int page = pages[i];
      if (use_gplog) {
        if (!ataReadLogExt(device, 0x04, 0, page, pages_buf.data(), 1)) {
          jerr("Read Device Statistics page 0x%02x failed\n\n", page);
          return false;
        }
      }
      else if (page > max_page)
        continue;

      int offset = (use_gplog ? 0 : page * 512);
      print_device_statistics_page(jref["pages"][ji++], pages_buf.data() + offset, page);
    }

    jout("%32s|||_ C monitored condition met\n", "");
    jout("%32s||__ D supports DSN\n", "");
    jout("%32s|___ N normalized value\n\n", "");
  }

  return true;
}


///////////////////////////////////////////////////////////////////////
// Pending Defects log (Log 0x0c)

// Section 9.26 of T13/BSR INCITS 529 (ACS-4) Revision 20, October 26, 2017

static bool print_pending_defects_log(ata_device * device, unsigned nsectors,
  unsigned max_entries)
{
  // Read #entries from page 0
  unsigned char page_buf[512] = {0, };
  if (!ataReadLogExt(device, 0x0c, 0, 0, page_buf, 1)) {
    pout("Read Pending Defects log page 0x00 failed\n\n");
    return false;
  }

  jout("Pending Defects log (GP Log 0x0c)\n");
  unsigned nentries = sg_get_unaligned_le32(page_buf);
  json::ref jref = jglb["ata_pending_defects_log"];
  jref["size"] = nsectors * 32 - 1;
  jref["count"] = nentries;
  if (!nentries) {
    jout("No Defects Logged\n\n");
    return true;
  }

  // Print entries
  jout("Index                LBA    Hours\n");
  for (unsigned i = 0, pi = 1, page = 0; i < nentries && i < max_entries; i++, pi++) {
    // Read new page if required
    if (pi >= 32) {
      if (++page >= nsectors) {
        pout("Pending Defects count %u exceeds log size (#pages=%u)\n\n",
             nentries, nsectors);
        return false;
      }
      if (!ataReadLogExt(device, 0x0c, 0, page, page_buf, 1)) {
        pout("Read Pending Defects log page 0x%02x failed\n\n", page);
        return false;
      }
      pi = 0;
    }

    const unsigned char * entry = page_buf + 16 * pi;
    unsigned hours = sg_get_unaligned_le32(entry);
    char hourstr[32];
    if (hours != 0xffffffffU)
      snprintf(hourstr, sizeof(hourstr), "%u", hours);
    else
      hourstr[0] = '-', hourstr[1] = 0;
    uint64_t lba = sg_get_unaligned_le64(entry + 8);
    jout("%5u %18" PRIu64 " %8s\n", i, lba, hourstr);

    json::ref jrefi = jref["table"][i];
    jrefi["lba"].set_unsafe_uint64(lba);
    if (hours != 0xffffffffU)
      jrefi["power_on_hours"] = hours;
  }

  if (nentries > max_entries)
    pout("... (%u entries not shown)\n", nentries - max_entries);
  jout("\n");
  return true;
}


///////////////////////////////////////////////////////////////////////

// Print log 0x11
static void PrintSataPhyEventCounters(const unsigned char * data, bool reset)
{
  if (checksum(data))
    checksumwarning("SATA Phy Event Counters");
  jout("SATA Phy Event Counters (GP Log 0x11)\n");
  if (data[0] || data[1] || data[2] || data[3])
    pout("[Reserved: 0x%02x 0x%02x 0x%02x 0x%02x]\n",
    data[0], data[1], data[2], data[3]);
  jout("ID      Size     Value  Description\n");

  for (unsigned i = 4, ji = 0; ; ) {
    // Get counter id and size (bits 14:12)
    unsigned id = data[i] | (data[i+1] << 8);
    unsigned size = ((id >> 12) & 0x7) << 1;
    id &= 0x8fff;

    // End of counter table ?
    if (!id)
      break;
    i += 2;

    if (!(2 <= size && size <= 8 && i + size < 512)) {
      pout("0x%04x  %u: Invalid entry\n", id, size);
      break;
    }

    // Get value
    uint64_t val = 0, max_val = 0;
    for (unsigned j = 0; j < size; j+=2) {
        val |= (uint64_t)(data[i+j] | (data[i+j+1] << 8)) << (j*8);
        max_val |= (uint64_t)0xffffU << (j*8);
    }
    i += size;

    // Get name
    const char * name;
    switch (id) {
      case 0x001: name = "Command failed due to ICRC error"; break; // Mandatory
      case 0x002: name = "R_ERR response for data FIS"; break;
      case 0x003: name = "R_ERR response for device-to-host data FIS"; break;
      case 0x004: name = "R_ERR response for host-to-device data FIS"; break;
      case 0x005: name = "R_ERR response for non-data FIS"; break;
      case 0x006: name = "R_ERR response for device-to-host non-data FIS"; break;
      case 0x007: name = "R_ERR response for host-to-device non-data FIS"; break;
      case 0x008: name = "Device-to-host non-data FIS retries"; break;
      case 0x009: name = "Transition from drive PhyRdy to drive PhyNRdy"; break;
      case 0x00A: name = "Device-to-host register FISes sent due to a COMRESET"; break; // Mandatory
      case 0x00B: name = "CRC errors within host-to-device FIS"; break;
      case 0x00D: name = "Non-CRC errors within host-to-device FIS"; break;
      case 0x00F: name = "R_ERR response for host-to-device data FIS, CRC"; break;
      case 0x010: name = "R_ERR response for host-to-device data FIS, non-CRC"; break;
      case 0x012: name = "R_ERR response for host-to-device non-data FIS, CRC"; break;
      case 0x013: name = "R_ERR response for host-to-device non-data FIS, non-CRC"; break;
      default:    name = ((id & 0x8000) ? "Vendor specific" : "Unknown"); break;
    }

    // Counters stop at max value, add '+' in this case
    jout("0x%04x  %u %12" PRIu64 "%c %s\n", id, size, val,
      (val == max_val ? '+' : ' '), name);

    json::ref jref = jglb["sata_phy_event_counters"]["table"][ji++];
    jref["id"] = id;
    jref["name"] = name;
    jref["size"] = size;
    jref["value"] = val;
    jref["overflow"] = (val == max_val);
  }
  if (reset)
    jout("All counters reset\n");
  jout("\n");
  jglb["sata_phy_event_counters"]["reset"] = reset;
}

// Format milliseconds from error log entry as "DAYS+H:M:S.MSEC"
static std::string format_milliseconds(unsigned msec)
{
  unsigned days  = msec  / 86400000U;
  msec          -= days  * 86400000U;
  unsigned hours = msec  / 3600000U;
  msec          -= hours * 3600000U;
  unsigned min   = msec  / 60000U;
  msec          -= min   * 60000U;
  unsigned sec   = msec  / 1000U;
  msec          -= sec   * 1000U;

  std::string str;
  if (days)
    str = strprintf("%2ud+", days);
  str += strprintf("%02u:%02u:%02u.%03u", hours, min, sec, msec);
  return str;
}

// Get description for 'state' value from SMART Error Logs
static const char * get_error_log_state_desc(unsigned state)
{
  state &= 0x0f;
  switch (state){
    case 0x0: return "in an unknown state";
    case 0x1: return "sleeping";
    case 0x2: return "in standby mode";
    case 0x3: return "active or idle";
    case 0x4: return "doing SMART Offline or Self-test";
  default:
    return (state < 0xb ? "in a reserved state"
                        : "in a vendor specific state");
  }
}

// returns number of errors
static int PrintSmartErrorlog(const ata_smart_errorlog *data,
                              firmwarebug_defs firmwarebugs)
{
  json::ref jref = jglb["ata_smart_error_log"]["summary"];
  jout("SMART Error Log Version: %d\n", (int)data->revnumber);
  jref["revision"] = data->revnumber;

  // if no errors logged, return
  if (!data->error_log_pointer){
    jout("No Errors Logged\n\n");
    jref["count"] = 0;
    return 0;
  }
  print_on();
  // If log pointer out of range, return
  if (data->error_log_pointer>5){
    pout("Invalid Error Log index = 0x%02x (T13/1321D rev 1c "
         "Section 8.41.6.8.2.2 gives valid range from 1 to 5)\n\n",
         (int)data->error_log_pointer);
    return 0;
  }

  // Some internal consistency checking of the data structures
  if ((data->ata_error_count-data->error_log_pointer) % 5 && !firmwarebugs.is_set(BUG_SAMSUNG2)) {
    pout("Warning: ATA error count %d inconsistent with error log pointer %d\n\n",
         data->ata_error_count,data->error_log_pointer);
  }
  
  // starting printing error log info
  if (data->ata_error_count<=5)
    jout( "ATA Error Count: %d\n", (int)data->ata_error_count);
  else
    jout( "ATA Error Count: %d (device log contains only the most recent five errors)\n",
           (int)data->ata_error_count);
  jref["count"] = data->ata_error_count;
  jref["logged_count"] = (data->ata_error_count <= 5 ? data->ata_error_count : 5);

  print_off();
  jout("\tCR = Command Register [HEX]\n"
       "\tFR = Features Register [HEX]\n"
       "\tSC = Sector Count Register [HEX]\n"
       "\tSN = Sector Number Register [HEX]\n"
       "\tCL = Cylinder Low Register [HEX]\n"
       "\tCH = Cylinder High Register [HEX]\n"
       "\tDH = Device/Head Register [HEX]\n"
       "\tDC = Device Command Register [HEX]\n"
       "\tER = Error register [HEX]\n"
       "\tST = Status register [HEX]\n"
       "Powered_Up_Time is measured from power on, and printed as\n"
       "DDd+hh:mm:SS.sss where DD=days, hh=hours, mm=minutes,\n"
       "SS=sec, and sss=millisec. It \"wraps\" after 49.710 days.\n\n");
  
  // now step through the five error log data structures (table 39 of spec)
  for (int k = 4, ji = 0; k >= 0; k--) {

    // The error log data structure entries are a circular buffer
    int i = (data->error_log_pointer + k) % 5;
    const ata_smart_errorlog_struct * elog = data->errorlog_struct+i;
    const ata_smart_errorlog_error_struct * summary = &(elog->error_struct);

    // Spec says: unused error log structures shall be zero filled
    if (nonempty(elog, sizeof(*elog))){
      // Table 57 of T13/1532D Volume 1 Revision 3
      const char *msgstate = get_error_log_state_desc(summary->state);
      int days = (int)summary->timestamp/24;

      // See table 42 of ATA5 spec
      print_on();
      jout("Error %d occurred at disk power-on lifetime: %d hours (%d days + %d hours)\n",
             (int)(data->ata_error_count+k-4), (int)summary->timestamp, days, (int)(summary->timestamp-24*days));
      print_off();

      json::ref jrefi = jref["table"][ji++];
      jrefi["error_number"] = data->ata_error_count + k - 4;
      jrefi["lifetime_hours"] = summary->timestamp;

      jout("  When the command that caused the error occurred, the device was %s.\n\n", msgstate);
      jout("  After command completion occurred, registers were:\n"
           "  ER ST SC SN CL CH DH\n"
           "  -- -- -- -- -- -- --\n"
           "  %02x %02x %02x %02x %02x %02x %02x",
           (int)summary->error_register,
           (int)summary->status,
           (int)summary->sector_count,
           (int)summary->sector_number,
           (int)summary->cylinder_low,
           (int)summary->cylinder_high,
           (int)summary->drive_head);

      {
        json::ref jrefir = jrefi["completion_registers"];
        jrefir["error"] = summary->error_register;
        jrefir["status"] = summary->status;
        jrefir["count"] = summary->sector_count;
        jrefir["lba"] = (summary->sector_number      )
                      | (summary->cylinder_low  <<  8)
                      | (summary->cylinder_high << 16);
        jrefir["device"] = summary->drive_head;
      }

      // Add a description of the contents of the status and error registers
      // if possible
      std::string st_er_desc = format_st_er_desc(elog);
      if (!st_er_desc.empty()) {
        jout("  %s", st_er_desc.c_str());
        jrefi["error_description"] = st_er_desc;
      }
      jout("\n\n");
      jout("  Commands leading to the command that caused the error were:\n"
           "  CR FR SC SN CL CH DH DC   Powered_Up_Time  Command/Feature_Name\n"
           "  -- -- -- -- -- -- -- --  ----------------  --------------------\n");
      for (int j = 4, jj = 0; j >= 0; j--) {
        const ata_smart_errorlog_command_struct * thiscommand = elog->commands+j;

        // Spec says: unused data command structures shall be zero filled
        if (nonempty(thiscommand, sizeof(*thiscommand))) {
          const char * atacmd = look_up_ata_command(thiscommand->commandreg, thiscommand->featuresreg);
          jout("  %02x %02x %02x %02x %02x %02x %02x %02x  %16s  %s\n",
               (int)thiscommand->commandreg,
               (int)thiscommand->featuresreg,
               (int)thiscommand->sector_count,
               (int)thiscommand->sector_number,
               (int)thiscommand->cylinder_low,
               (int)thiscommand->cylinder_high,
               (int)thiscommand->drive_head,
               (int)thiscommand->devicecontrolreg,
               format_milliseconds(thiscommand->timestamp).c_str(),
               atacmd);

          json::ref jrefic = jrefi["previous_commands"][jj++];
          json::ref jreficr = jrefic["registers"];
          jreficr["command"] = thiscommand->commandreg;
          jreficr["features"] = thiscommand->featuresreg,
          jreficr["count"] = thiscommand->sector_count;
          jreficr["lba"] = (thiscommand->sector_number      )
                         | (thiscommand->cylinder_low   << 8)
                         | (thiscommand->cylinder_high << 16);
          jreficr["device"] = thiscommand->drive_head;
          jreficr["device_control"] = thiscommand->devicecontrolreg;
          jrefic["powerup_milliseconds"] = thiscommand->timestamp;
          jrefic["command_name"] = atacmd;
        }
      }
      jout("\n");
    }
  }
  print_on();
  if (printing_is_switchable)
    pout("\n");
  print_off();
  return data->ata_error_count;  
}

// Print SMART Extended Comprehensive Error Log (GP Log 0x03)
static int PrintSmartExtErrorLog(ata_device * device,
                                 const firmwarebug_defs & firmwarebugs,
                                 const ata_smart_exterrlog * log,
                                 unsigned nsectors, unsigned max_errors)
{
  json::ref jref = jglb["ata_smart_error_log"]["extended"];
  jout("SMART Extended Comprehensive Error Log Version: %u (%u sectors)\n",
       log->version, nsectors);
  jref["revision"] = log->version;
  jref["sectors"] = nsectors;

  if (!log->device_error_count) {
    jout("No Errors Logged\n\n");
    jref["count"] = 0;
    return 0;
  }
  print_on();

  // Check index
  unsigned nentries = nsectors * 4;
  unsigned erridx = log->error_log_index;
  if (!(1 <= erridx && erridx <= nentries)){
    // Some Samsung disks (at least SP1614C/SW100-25, HD300LJ/ZT100-12) use the
    // former index from Summary Error Log (byte 1, now reserved) and set byte 2-3
    // to 0.
    if (!(erridx == 0 && 1 <= log->reserved1 && log->reserved1 <= nentries)) {
      pout("Invalid Error Log index = 0x%04x (reserved = 0x%02x)\n", erridx, log->reserved1);
      return 0;
    }
    pout("Invalid Error Log index = 0x%04x, trying reserved byte (0x%02x) instead\n", erridx, log->reserved1);
    erridx = log->reserved1;
  }

  // Index base is not clearly specified by ATA8-ACS (T13/1699-D Revision 6a),
  // it is 1-based in practice.
  erridx--;

  // Calculate #errors to print
  unsigned errcnt = log->device_error_count;

  if (errcnt <= nentries)
    jout("Device Error Count: %u\n", log->device_error_count);
  else {
    errcnt = nentries;
    jout("Device Error Count: %u (device log contains only the most recent %u errors)\n",
         log->device_error_count, errcnt);
  }
  jref["count"] = log->device_error_count;
  jref["logged_count"] = errcnt;

  if (max_errors < errcnt)
    errcnt = max_errors;

  print_off();
  jout("\tCR     = Command Register\n"
       "\tFEATR  = Features Register\n"
       "\tCOUNT  = Count (was: Sector Count) Register\n"
       "\tLBA_48 = Upper bytes of LBA High/Mid/Low Registers ]  ATA-8\n"
       "\tLH     = LBA High (was: Cylinder High) Register    ]   LBA\n"
       "\tLM     = LBA Mid (was: Cylinder Low) Register      ] Register\n"
       "\tLL     = LBA Low (was: Sector Number) Register     ]\n"
       "\tDV     = Device (was: Device/Head) Register\n"
       "\tDC     = Device Control Register\n"
       "\tER     = Error register\n"
       "\tST     = Status register\n"
       "Powered_Up_Time is measured from power on, and printed as\n"
       "DDd+hh:mm:SS.sss where DD=days, hh=hours, mm=minutes,\n"
       "SS=sec, and sss=millisec. It \"wraps\" after 49.710 days.\n\n");

  // Recently read log page
  ata_smart_exterrlog log_buf;
  unsigned log_buf_page = ~0;

  // Iterate through circular buffer in reverse direction
  for (unsigned i = 0, errnum = log->device_error_count;
       i < errcnt; i++, errnum--, erridx = (erridx > 0 ? erridx - 1 : nentries - 1)) {

    // Read log page if needed
    const ata_smart_exterrlog * log_p;
    unsigned page = erridx / 4;
    if (page == 0)
      log_p = log;
    else {
      if (page != log_buf_page) {
        memset(&log_buf, 0, sizeof(log_buf));
        if (!ataReadExtErrorLog(device, &log_buf, page, 1, firmwarebugs))
          break;
        log_buf_page = page;
      }
      log_p = &log_buf;
    }

    const ata_smart_exterrlog_error_log & entry = log_p->error_logs[erridx % 4];

    json::ref jrefi = jref["table"][i];
    jrefi["error_number"] = errnum;
    jrefi["log_index"] = erridx;

    // Skip unused entries
    if (!nonempty(&entry, sizeof(entry))) {
      jout("Error %u [%u] log entry is empty\n", errnum, erridx);
      continue;
    }

    // Print error information
    print_on();
    const ata_smart_exterrlog_error & err = entry.error;
    jout("Error %u [%u] occurred at disk power-on lifetime: %u hours (%u days + %u hours)\n",
         errnum, erridx, err.timestamp, err.timestamp / 24, err.timestamp % 24);
    print_off();
    jrefi["lifetime_hours"] = err.timestamp;

    const char * msgstate = get_error_log_state_desc(err.state);
    jout("  When the command that caused the error occurred, the device was %s.\n\n", msgstate);
    jrefi["device_state"]["value"] = err.state;
    jrefi["device_state"]["string"] = msgstate;

    // Print registers
    jout("  After command completion occurred, registers were:\n"
         "  ER -- ST COUNT  LBA_48  LH LM LL DV DC\n"
         "  -- -- -- == -- == == == -- -- -- -- --\n"
         "  %02x -- %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x",
         err.error_register,
         err.status_register,
         err.count_register_hi,
         err.count_register,
         err.lba_high_register_hi,
         err.lba_mid_register_hi,
         err.lba_low_register_hi,
         err.lba_high_register,
         err.lba_mid_register,
         err.lba_low_register,
         err.device_register,
         err.device_control_register);

    {
      json::ref jrefir = jrefi["completion_registers"];
      jrefir["error"] = err.error_register;
      jrefir["status"] = err.status_register,
      jrefir["count"] = (err.count_register_hi << 8) | err.count_register;
      jrefir["lba"] = ((uint64_t)err.lba_high_register_hi << 40)
                    | ((uint64_t)err.lba_mid_register_hi  << 32)
                    | ((uint64_t)err.lba_low_register_hi  << 24)
                    | ((unsigned)err.lba_high_register    << 16)
                    | ((unsigned)err.lba_mid_register     <<  8)
                    | ((unsigned)err.lba_low_register          );
      jrefir["device"] = err.device_register;
      jrefir["device_control"] = err.device_control_register;
    }

    // Add a description of the contents of the status and error registers
    // if possible
    std::string st_er_desc = format_st_er_desc(&entry);
    if (!st_er_desc.empty()) {
      jout("  %s", st_er_desc.c_str());
      jrefi["error_description"] = st_er_desc;
    }
    jout("\n\n");

    // Print command history
    jout("  Commands leading to the command that caused the error were:\n"
         "  CR FEATR COUNT  LBA_48  LH LM LL DV DC  Powered_Up_Time  Command/Feature_Name\n"
         "  -- == -- == -- == == == -- -- -- -- --  ---------------  --------------------\n");
    for (int ci = 4, cji = 0; ci >= 0; ci--) {
      const ata_smart_exterrlog_command & cmd = entry.commands[ci];

      // Skip unused entries
      if (!nonempty(&cmd, sizeof(cmd)))
        continue;

      // Print registers, timestamp and ATA command name
      const char * atacmd = look_up_ata_command(cmd.command_register, cmd.features_register);
      jout("  %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %16s  %s\n",
           cmd.command_register,
           cmd.features_register_hi,
           cmd.features_register,
           cmd.count_register_hi,
           cmd.count_register,
           cmd.lba_high_register_hi,
           cmd.lba_mid_register_hi,
           cmd.lba_low_register_hi,
           cmd.lba_high_register,
           cmd.lba_mid_register,
           cmd.lba_low_register,
           cmd.device_register,
           cmd.device_control_register,
           format_milliseconds(cmd.timestamp).c_str(),
           atacmd);

      json::ref jrefic = jrefi["previous_commands"][cji++];
      json::ref jreficr = jrefic["registers"];
      jreficr["command"] = cmd.command_register;
      jreficr["features"] = (cmd.features_register_hi << 8) | cmd.features_register;
      jreficr["count"] = (cmd.count_register_hi << 8) | cmd.count_register;
      jreficr["lba"] = ((uint64_t)cmd.lba_high_register_hi << 40)
                     | ((uint64_t)cmd.lba_mid_register_hi  << 32)
                     | ((uint64_t)cmd.lba_low_register_hi  << 24)
                     | ((unsigned)cmd.lba_high_register    << 16)
                     | ((unsigned)cmd.lba_mid_register     <<  8)
                     | ((unsigned)cmd.lba_low_register          );
      jreficr["device"] = cmd.device_register;
      jreficr["device_control"] = cmd.device_control_register;
      jrefic["powerup_milliseconds"] = cmd.timestamp;
      jrefic["command_name"] = atacmd;
    }
    jout("\n");
  }

  print_on();
  if (printing_is_switchable)
    pout("\n");
  print_off();
  return log->device_error_count;
}

// Print one self-test log entry.
// Returns:
// -1: self-test failed
//  1: extended self-test completed without error
//  0: otherwise
static int ataPrintSmartSelfTestEntry(const json::ref & jref,
                                      unsigned testnum, unsigned char test_type,
                                      unsigned char test_status,
                                      unsigned short timestamp,
                                      uint64_t failing_lba,
                                      bool print_error_only, bool & print_header)
{
  // Check status and type for return value
  int retval = 0;
  switch (test_status >> 4) {
    case 0x0:
      if ((test_type & 0x7f) == 0x02)
        retval = 1; // extended self-test completed without error
      break;
    case 0x3: case 0x4:
    case 0x5: case 0x6:
    case 0x7: case 0x8:
      retval = -1; // self-test failed
      break;
  }

  if (retval >= 0 && print_error_only)
    return retval;

  std::string msgtest;
  switch (test_type) {
    case 0x00: msgtest = "Offline";            break;
    case 0x01: msgtest = "Short offline";      break;
    case 0x02: msgtest = "Extended offline";   break;
    case 0x03: msgtest = "Conveyance offline"; break;
    case 0x04: msgtest = "Selective offline";  break;
    case 0x7f: msgtest = "Abort offline test"; break;
    case 0x81: msgtest = "Short captive";      break;
    case 0x82: msgtest = "Extended captive";   break;
    case 0x83: msgtest = "Conveyance captive"; break;
    case 0x84: msgtest = "Selective captive";  break;
    default:
      if ((0x40 <= test_type && test_type <= 0x7e) || 0x90 <= test_type)
        msgtest = strprintf("Vendor (0x%02x)", test_type);
      else
        msgtest = strprintf("Reserved (0x%02x)", test_type);
  }

  std::string msgstat;
  switch (test_status >> 4) {
    case 0x0: msgstat = "Completed without error";       break;
    case 0x1: msgstat = "Aborted by host";               break;
    case 0x2: msgstat = "Interrupted (host reset)";      break;
    case 0x3: msgstat = "Fatal or unknown error";        break;
    case 0x4: msgstat = "Completed: unknown failure";    break;
    case 0x5: msgstat = "Completed: electrical failure"; break;
    case 0x6: msgstat = "Completed: servo/seek failure"; break;
    case 0x7: msgstat = "Completed: read failure";       break;
    case 0x8: msgstat = "Completed: handling damage??";  break;
    case 0xf: msgstat = "Self-test routine in progress"; break;
    default:  msgstat = strprintf("Unknown status (0x%x)", test_status >> 4);
  }

  // Print header once
  if (print_header) {
    print_header = false;
    jout("Num  Test_Description    Status                  Remaining  LifeTime(hours)  LBA_of_first_error\n");
  }

  char msglba[32];
  if (retval < 0 && failing_lba < 0xffffffffffffULL)
    snprintf(msglba, sizeof(msglba), "%" PRIu64, failing_lba);
  else {
    msglba[0] = '-'; msglba[1] = 0;
  }

  jout("#%2u  %-19s %-29s %1d0%%  %8u         %s\n", testnum,
       msgtest.c_str(), msgstat.c_str(), test_status & 0x0f, timestamp, msglba);

  jref["type"]["value"] = test_type;
  jref["type"]["string"] = msgtest;

  jref["status"]["value"] = test_status;
  jref["status"]["string"] = msgstat;
  if (test_status & 0x0f)
    jref["status"]["remaining_percent"] = (test_status & 0x0f) * 10;
  switch (test_status >> 4) {
    case 0x1: case 0x2: case 0x3: break; // aborted -> unknown
    default: jref["status"]["passed"] = (retval >= 0);
  }

  jref["lifetime_hours"] = timestamp;

  if (retval < 0 && failing_lba < 0xffffffffffffULL)
    jref["lba"] = failing_lba;

  return retval;
}

// Print SMART Self-test log, return error count
static int ataPrintSmartSelfTestlog(const ata_smart_selftestlog * log, bool allentries,
                                    firmwarebug_defs firmwarebugs)
{
  json::ref jref = jglb["ata_smart_self_test_log"]["standard"];

  if (allentries)
    jout("SMART Self-test log structure revision number %d\n", log->revnumber);
  jref["revision"] = log->revnumber;
  if (log->revnumber != 0x0001 && allentries && !firmwarebugs.is_set(BUG_SAMSUNG))
    pout("Warning: ATA Specification requires self-test log structure revision number = 1\n");
  if (!log->mostrecenttest){
    if (allentries)
      jout("No self-tests have been logged.  [To run self-tests, use: smartctl -t]\n");
    jref["count"] = 0;
    return 0;
  }

  bool noheaderprinted = true;
  int errcnt = 0, igncnt = 0;
  int testnum = 1, ext_ok_testnum = -1;

  // Iterate through circular buffer in reverse direction
  for (int i = 20, ji = 0; i >= 0; i--) {
    int j = (i + log->mostrecenttest) % 21;
    const ata_smart_selftestlog_struct & entry = log->selftest_struct[j];

    // Skip unused entries
    if (!nonempty(&entry, sizeof(entry)))
      continue;

    // Get LBA if valid
    uint64_t lba48 = (entry.lbafirstfailure < 0xffffffff ?
                      entry.lbafirstfailure : 0xffffffffffffULL);

    // Print entry
    int state = ataPrintSmartSelfTestEntry(jref["table"][ji++],
      testnum, entry.selftestnumber, entry.selfteststatus,
      entry.timestamp, lba48, !allentries, noheaderprinted);

    if (state < 0) {
      // Self-test showed an error
      if (ext_ok_testnum < 0)
        errcnt++;
      else
        // Newer successful extended self-test exits
        igncnt++;
    }
    else if (state > 0 && ext_ok_testnum < 0) {
      // Latest successful extended self-test
      ext_ok_testnum = testnum;
    }
    testnum++;
  }

  if (igncnt)
    jout("%d of %d failed self-tests are outdated by newer successful extended offline self-test #%2d\n",
      igncnt, igncnt+errcnt, ext_ok_testnum);
  jref["count"] = testnum - 1;
  jref["error_count_total"] = igncnt + errcnt;
  jref["error_count_outdated"] = igncnt;

  if (!allentries && !noheaderprinted)
    jout("\n");

  return errcnt;
}

// Print SMART Extended Self-test Log (GP Log 0x07)
static int PrintSmartExtSelfTestLog(const ata_smart_extselftestlog * log,
                                    unsigned nsectors, unsigned max_entries)
{
  json::ref jref = jglb["ata_smart_self_test_log"]["extended"];

  jout("SMART Extended Self-test Log Version: %u (%u sectors)\n",
       log->version, nsectors);
  jref["revision"] = log->version;
  jref["sectors"] = nsectors;

  if (!log->log_desc_index){
    jout("No self-tests have been logged.  [To run self-tests, use: smartctl -t]\n\n");
    jref["count"] = 0;
    return 0;
  }

  // Check index
  unsigned nentries = nsectors * 19;
  unsigned logidx = log->log_desc_index;
  if (logidx > nentries) {
    pout("Invalid Self-test Log index = 0x%04x (reserved = 0x%02x)\n", logidx, log->reserved1);
    return 0;
  }

  // Index base is not clearly specified by ATA8-ACS (T13/1699-D Revision 6a),
  // it is 1-based in practice.
  logidx--;

  bool print_header = true;
  int errcnt = 0, igncnt = 0;
  int ext_ok_testnum = -1;
  unsigned testnum = 1;

  // Iterate through circular buffer in reverse direction
  for (unsigned i = 0, ji = 0; i < nentries && testnum <= max_entries;
       i++, logidx = (logidx > 0 ? logidx - 1 : nentries - 1)) {

    const ata_smart_extselftestlog_desc & entry = log[logidx / 19].log_descs[logidx % 19];

    // Skip unused entries
    if (!nonempty(&entry, sizeof(entry)))
      continue;

    // Get LBA
    const unsigned char * b = entry.failing_lba;
    uint64_t lba48 = b[0]
        | (          b[1] <<  8)
        | (          b[2] << 16)
        | ((uint64_t)b[3] << 24)
        | ((uint64_t)b[4] << 32)
        | ((uint64_t)b[5] << 40);

    // Print entry
    int state = ataPrintSmartSelfTestEntry(jref["table"][ji++],
      testnum, entry.self_test_type,
      entry.self_test_status, entry.timestamp, lba48,
      false /*!print_error_only*/, print_header);

    if (state < 0) {
      // Self-test showed an error
      if (ext_ok_testnum < 0)
        errcnt++;
      else
        // Newer successful extended self-test exits
        igncnt++;
    }
    else if (state > 0 && ext_ok_testnum < 0) {
      // Latest successful extended self-test
      ext_ok_testnum = testnum;
    }
    testnum++;
  }

  if (igncnt)
    jout("%d of %d failed self-tests are outdated by newer successful extended offline self-test #%2d\n",
      igncnt, igncnt+errcnt, ext_ok_testnum);
  jref["count"] = testnum - 1;
  jref["error_count_total"] = igncnt + errcnt;
  jref["error_count_outdated"] = igncnt;

  jout("\n");
  return errcnt;
}

static void ataPrintSelectiveSelfTestLog(const ata_selective_self_test_log * log, const ata_smart_values * sv)
{
  json::ref jref = jglb["ata_smart_selective_self_test_log"];

  // print data structure revision number
  jout("SMART Selective self-test log data structure revision number %d\n", log->logversion);
  jref["revision"] = log->logversion;
  if (1 != log->logversion)
    pout("Note: revision number not 1 implies that no selective self-test has ever been run\n");
  
  const char *msg;
  switch((sv->self_test_exec_status)>>4){
  case  0:msg="Completed";
    break;
  case  1:msg="Aborted_by_host";
    break;
  case  2:msg="Interrupted";
    break;
  case  3:msg="Fatal_error";
    break;
  case  4:msg="Completed_unknown_failure";
    break;
  case  5:msg="Completed_electrical_failure";
    break;
  case  6:msg="Completed_servo/seek_failure";
    break;
  case  7:msg="Completed_read_failure";
    break;
  case  8:msg="Completed_handling_damage??";
    break;
  case 15:msg="Self_test_in_progress";
    break;
  default:msg="Unknown_status ";
    break;
  }

  // find the number of columns needed for printing. If in use, the
  // start/end of span being read-scanned...
  uint64_t maxl = 0, maxr = 0;
  uint64_t current = log->currentlba;
  uint64_t currentend = current + 0xffff;
  if (log->currentspan>5) {
    maxl=current;
    maxr=currentend;
  }
  for (int i = 0; i < 5; i++) {
    uint64_t start=log->span[i].start;
    uint64_t end  =log->span[i].end; 
    // ... plus max start/end of each of the five test spans.
    if (start>maxl)
      maxl=start;
    if (end > maxr)
      maxr=end;
  }
  
  // we need at least 7 characters wide fields to accommodate the
  // labels
  int field1,field2;
  char tmp[64];
  if ((field1=snprintf(tmp,64, "%" PRIu64, maxl))<7)
    field1=7;
  if ((field2=snprintf(tmp,64, "%" PRIu64, maxr))<7)
    field2=7;

  // now print the five test spans
  jout(" SPAN  %*s  %*s  CURRENT_TEST_STATUS\n", field1, "MIN_LBA", field2, "MAX_LBA");

  for (int i = 0; i < 5; i++) {
    uint64_t start=log->span[i].start;
    uint64_t end=log->span[i].end;
    bool active = (i + 1 == log->currentspan);

    if (active)
      // this span is currently under test
      jout("    %d  %*" PRIu64 "  %*" PRIu64 "  %s [%01d0%% left] (%" PRIu64 "-%" PRIu64 ")\n",
           i + 1, field1, start, field2, end, msg,
           (sv->self_test_exec_status & 0xf), current, currentend);
    else
      // this span is not currently under test
      jout("    %d  %*" PRIu64 "  %*" PRIu64 "  Not_testing\n",
           i + 1, field1, start, field2, end);

    json::ref jrefi = jref["table"][i];
    jrefi["lba_min"] = start;
    jrefi["lba_max"] = end;
    jrefi["status"]["value"] = sv->self_test_exec_status;
    jrefi["status"]["string"] = (active ? msg : "Not_testing");
    if (active) {
      jrefi["status"]["remaining_percent"] = sv->self_test_exec_status & 0xf;
      jrefi["current_lba_min"] = current;
      jrefi["current_lba_max"] = currentend;
    }
  }

  // if we are currently read-scanning, print LBAs and the status of
  // the read scan
  if (log->currentspan > 5) {
    const char * ost = OfflineDataCollectionStatus(sv->offline_data_collection_status);
    jout("%5d  %*" PRIu64 "  %*" PRIu64 "  Read_scanning %s\n",
         log->currentspan, field1, current, field2, currentend, ost);
    json::ref jrefc = jref["current_read_scan"];
    jrefc["lba_min"] = current;
    jrefc["lba_max"] = currentend;
    jrefc["status"]["value"] = sv->offline_data_collection_status;
    jrefc["status"]["string"] = ost;
  }
  
  /* Print selective self-test flags.  Possible flag combinations are
     (numbering bits from 0-15):
     Bit-1 Bit-3   Bit-4
     Scan  Pending Active
     0     *       *       Don't scan
     1     0       0       Will carry out scan after selective test
     1     1       0       Waiting to carry out scan after powerup
     1     0       1       Currently scanning       
     1     1       1       Currently scanning
  */
  
  jout("Selective self-test flags (0x%x):\n", (unsigned)log->flags);
  json::ref jreff = jref["flags"];
  jreff["value"] = log->flags;
  jreff["remainder_scan_enabled"] = !!(log->flags & SELECTIVE_FLAG_DOSCAN);
  if (log->flags & SELECTIVE_FLAG_DOSCAN) {
   if (log->flags & SELECTIVE_FLAG_ACTIVE)
     jout("  Currently read-scanning the remainder of the disk.\n");
   else if (log->flags & SELECTIVE_FLAG_PENDING)
     jout("  Read-scan of remainder of disk interrupted; will resume %d min after power-up.\n",
          log->pendingtime);
   else
     jout("  After scanning selected spans, read-scan remainder of disk.\n");
   jreff["remainder_scan_active"] = !!(log->flags & SELECTIVE_FLAG_ACTIVE);
   jreff["power_up_scan_pending"] = !!(log->flags & SELECTIVE_FLAG_PENDING);
  }
  else
    jout("  After scanning selected spans, do NOT read-scan remainder of disk.\n");
  
  // print pending time
  jout("If Selective self-test is pending on power-up, resume after %d minute delay.\n",
       log->pendingtime);
  jref["power_up_scan_resume_minutes"] = log->pendingtime;
}

// Format SCT Temperature value
static const char * sct_ptemp(signed char x, char (& buf)[20])
{
  if (x == -128 /*0x80 = unknown*/)
    return " ?";
  snprintf(buf, sizeof(buf), "%2d", x);
  return buf;
}

static void sct_jtemp2(const json::ref & jref, const char * name, signed char x)
{
  if (x == -128 /*0x80 = unknown*/)
    return;
  jglb["temperature"][name] = x;
  jref["temperature"][name] = x;
}

static const char * sct_pbar(int x, char (& buf)[64])
{
  if (x <= 19)
    x = 0;
  else
    x -= 19;
  bool ov = false;
  if (x > 40) {
    x = 40; ov = true;
  }
  if (x > 0) {
    memset(buf, '*', x);
    if (ov)
      buf[x-1] = '+';
    buf[x] = 0;
  }
  else {
    buf[0] = '-'; buf[1] = 0;
  }
  return buf;
}

static const char * sct_device_state_msg(unsigned char state)
{
  switch (state) {
    case 0: return "Active";
    case 1: return "Stand-by";
    case 2: return "Sleep";
    case 3: return "DST executing in background";
    case 4: return "SMART Off-line Data Collection executing in background";
    case 5: return "SCT command executing in background";
    default:return "Unknown";
  }
}

// Print SCT Status
static int ataPrintSCTStatus(const ata_sct_status_response * sts)
{
  json::ref jref = jglb["ata_sct_status"];

  jout("SCT Status Version:                  %u\n", sts->format_version);
  jref["format_version"] = sts->format_version;
  jout("SCT Version (vendor specific):       %u (0x%04x)\n", sts->sct_version, sts->sct_version);
  jref["sct_version"] = sts->sct_version;
  // SCT Support Level (1) from original SCT draft was later declared obsolete in ATA-8 ACS.
  // Drives typically return 0 or 1.  Print only if unknown value is returned.
  if (sts->sct_spec > 1)
    pout("SCT Support Level:                   %u\n", sts->sct_spec);
  const char * statestr = sct_device_state_msg(sts->device_state);
  jout("Device State:                        %s (%u)\n", statestr, sts->device_state);
  jref["device_state"]["value"] = sts->device_state;
  jref["device_state"]["string"] = statestr;

  // If "Reserved" fields not set, assume "old" format version 2:
  // Table 11 of T13/1701DT-N (SMART Command Transport) Revision 5, February 2005
  // Table 54 of T13/1699-D (ATA8-ACS) Revision 3e, July 2006
  // ... else assume "new" format version 2 or version 3:
  // T13/e06152r0-3 (Additional SCT Temperature Statistics), August - October 2006
  // Table 60 of T13/1699-D (ATA8-ACS) Revision 3f, December 2006  (format version 2)
  // Table 80 of T13/1699-D (ATA8-ACS) Revision 6a, September 2008 (format version 3)
  // Table 194 of T13/BSR INCITS 529 (ACS-4) Revision 20, October 26, 2017
  // (max_op_limit, smart_status, min_erc_time)
  bool old_format_2 = (   !sts->min_temp && !sts->life_min_temp
                       && !sts->under_limit_count && !sts->over_limit_count);

  char buf1[20], buf2[20];
  jout("Current Temperature:                    %s Celsius\n",
  sct_ptemp(sts->hda_temp, buf1));
  sct_jtemp2(jref, "current", sts->hda_temp);
  jout("Power Cycle Min/Max Temperature:     %s/%s Celsius\n",
    (!old_format_2 ? sct_ptemp(sts->min_temp, buf1) : "--"),
    sct_ptemp(sts->max_temp, buf2));
  if (!old_format_2)
    sct_jtemp2(jref, "power_cycle_min", sts->min_temp);
  sct_jtemp2(jref, "power_cycle_max", sts->max_temp);
  jout("Lifetime    Min/Max Temperature:     %s/%s Celsius\n",
    (!old_format_2 ? sct_ptemp(sts->life_min_temp, buf1) : "--"),
    sct_ptemp(sts->life_max_temp, buf2));
  if (!old_format_2)
    sct_jtemp2(jref, "lifetime_min", sts->life_min_temp);
  sct_jtemp2(jref, "lifetime_max", sts->life_max_temp);
  if (old_format_2)
    return 0;

  if (sts->max_op_limit > 0) { // e06152r0-2: "Average Temperature"
    jout("Specified Max Operating Temperature:   %3d Celsius\n", sts->max_op_limit);
    sct_jtemp2(jref, "op_limit_max", sts->max_op_limit);
  }
  jout("Under/Over Temperature Limit Count:  %2u/%u\n",
    sts->under_limit_count, sts->over_limit_count);
  jref["temperature"]["under_limit_count"] = sts->under_limit_count;
  jref["temperature"]["over_limit_count"] = sts->over_limit_count;

  if (sts->smart_status) { // ACS-4
    int passed = (sts->smart_status == 0x2cf4 ? 0 :
                  sts->smart_status == 0xc24f ? 1 : -1);
    jout("SMART Status:                        0x%04x (%s)\n", sts->smart_status,
          (passed == 0 ? "FAILED" : passed > 0 ? "PASSED" : "Reserved"));
    if (passed >= 0) {
      jref["smart_status"]["passed"] = !!passed;
      jglb["smart_status"]["passed"] = !!passed;
    }
    else
      jref["smart_status"]["reserved_value"] = sts->smart_status;
  }

  if (sts->min_erc_time) // ACS-4
    pout("Minimum supported ERC Time Limit:    %d (%0.1f seconds)\n",
          sts->min_erc_time, sts->min_erc_time/10.0);

  if (nonempty(sts->vendor_specific, sizeof(sts->vendor_specific))) {
    jout("Vendor specific:\n");
    for (unsigned i = 0; i < sizeof(sts->vendor_specific); i++) {
      jout("%02x%c", sts->vendor_specific[i], ((i & 0xf) != 0xf ? ' ' : '\n'));
      jref["vendor_specific"][i] = sts->vendor_specific[i];
    }
  }
  return 0;
}

// Print SCT Temperature History Table
static int ataPrintSCTTempHist(const ata_sct_temperature_history_table * tmh)
{
  json::ref jref = jglb["ata_sct_temperature_history"];

  char buf1[20], buf2[20], buf3[64];
  jout("SCT Temperature History Version:     %u%s\n", tmh->format_version,
       (tmh->format_version != 2 ? " (Unknown, should be 2)" : ""));
  jref["version"] = tmh->format_version;
  jout("Temperature Sampling Period:         %u minute%s\n",
    tmh->sampling_period, (tmh->sampling_period==1?"":"s"));
  jref["sampling_period_minutes"] = tmh->sampling_period;
  jout("Temperature Logging Interval:        %u minute%s\n",
    tmh->interval,        (tmh->interval==1?"":"s"));
  jref["logging_interval_minutes"] = tmh->interval;

  jout("Min/Max recommended Temperature:     %s/%s Celsius\n",
    sct_ptemp(tmh->min_op_limit, buf1), sct_ptemp(tmh->max_op_limit, buf2));
  sct_jtemp2(jref, "op_limit_min", tmh->min_op_limit);
  sct_jtemp2(jref, "op_limit_max", tmh->max_op_limit);
  jout("Min/Max Temperature Limit:           %s/%s Celsius\n",
    sct_ptemp(tmh->under_limit, buf1), sct_ptemp(tmh->over_limit, buf2));
  sct_jtemp2(jref, "limit_min", tmh->under_limit);
  sct_jtemp2(jref, "limit_max", tmh->over_limit);
  jout("Temperature History Size (Index):    %u (%u)\n", tmh->cb_size, tmh->cb_index);
  jref["size"] = tmh->cb_size;
  jref["index"] = tmh->cb_index;

  if (!(0 < tmh->cb_size && tmh->cb_size <= sizeof(tmh->cb) && tmh->cb_index < tmh->cb_size)) {
    if (!tmh->cb_size)
      pout("Temperature History is empty\n");
    else
      pout("Invalid Temperature History Size or Index\n");
    return 0;
  }

  // Print table
  jout("\nIndex    Estimated Time   Temperature Celsius\n");
  unsigned n = 0, i = (tmh->cb_index+1) % tmh->cb_size;
  unsigned interval = (tmh->interval > 0 ? tmh->interval : 1);
  time_t t = time(0) - (time_t)(tmh->cb_size-1) * interval * 60;
  t -= t % (interval * 60);
  while (n < tmh->cb_size) {
    // Find range of identical temperatures
    unsigned n1 = n, n2 = n+1, i2 = (i+1) % tmh->cb_size;
    while (n2 < tmh->cb_size && tmh->cb[i2] == tmh->cb[i]) {
      n2++; i2 = (i2+1) % tmh->cb_size;
    }
    // Print range
    while (n < n2) {
      if (n == n1 || n == n2-1 || n2 <= n1+3) {
        // TODO: Don't print times < boot time
        char date[32] = "";
        struct tm tmbuf;
        strftime(date, sizeof(date), "%Y-%m-%d %H:%M", time_to_tm_local(&tmbuf, t));
        jout(" %3u    %s    %s  %s\n", i, date,
          sct_ptemp(tmh->cb[i], buf1), sct_pbar(tmh->cb[i], buf3));
      }
      else if (n == n1+1) {
        jout(" ...    ..(%3u skipped).    ..  %s\n",
          n2-n1-2, sct_pbar(tmh->cb[i], buf3));
      }
      if (tmh->cb[i] != -128)
        jref["table"][n] = tmh->cb[i];
      t += interval * 60; i = (i+1) % tmh->cb_size; n++;
    }
  }
  //assert(n == tmh->cb_size && i == (tmh->cb_index+1) % tmh->cb_size);

  return 0;
}

// Print SCT Error Recovery Control timers
static void ataPrintSCTErrorRecoveryControl(bool set, unsigned short read_timer, unsigned short write_timer, bool power_on, bool mfg_default = false)
{
  const char* power_on_str = (power_on ? "Power-on " : "");
  json::ref jref = jglb["ata_sct_erc"];
  jout("SCT Error Recovery Control%s:%s\n", (set ? " set to" : ""), (mfg_default ? " default values." : ""));

  if (!mfg_default) {
    jref["read"]["enabled"] = !!read_timer;
    if (!read_timer)
      jout("           %sRead: Disabled\n", power_on_str);
    else {
      jout("           %sRead: %6d (%0.1f seconds)\n", power_on_str, read_timer, read_timer/10.0);
      jref["read"]["deciseconds"] = read_timer;
    }

    jref["write"]["enabled"] = !!write_timer;
    if (!write_timer)
      jout("          %sWrite: Disabled\n", power_on_str);
    else {
      jout("          %sWrite: %6d (%0.1f seconds)\n", power_on_str, write_timer, write_timer/10.0);
      jref["write"]["deciseconds"] = write_timer;
    }
  }
}

static void print_aam_level(const char * msg, int level, int recommended = -1)
{
  // Table 56 of T13/1699-D (ATA8-ACS) Revision 6a, September 6, 2008
  // Obsolete since T13/2015-D (ACS-2) Revision 4a, December 9, 2010
  const char * s;
  if (level == 0)
    s = "vendor specific";
  else if (level < 128)
    s = "unknown/retired";
  else if (level == 128)
    s = "quiet";
  else if (level < 254)
    s = "intermediate";
  else if (level == 254)
    s = "maximum performance";
  else
    s = "reserved";

  if (recommended >= 0)
    jout("%s%d (%s), recommended: %d\n", msg, level, s, recommended);
  else
    jout("%s%d (%s)\n", msg, level, s);

  json::ref jref = jglb["ata_aam"];
  jref["enabled"] = true;
  jref["level"] = level;
  jref["string"] = s;
  if (recommended >= 0)
    jref["recommended_level"] = recommended;
}

static void print_apm_level(const char * msg, int level)
{
  // Table 120 of T13/2015-D (ACS-2) Revision 7, June 22, 2011
  const char * s;
  if (!(1 <= level && level <= 254))
    s = "reserved";
  else if (level == 1)
    s = "minimum power consumption with standby";
  else if (level < 128)
    s = "intermediate level with standby";
  else if (level == 128)
    s = "minimum power consumption without standby";
  else if (level < 254)
    s = "intermediate level without standby";
  else
    s = "maximum performance";

  jout("%s%d (%s)\n", msg, level, s);

  json::ref jref = jglb["ata_apm"];
  jref["enabled"] = true;
  jref["level"] = level;
  jref["string"] = s;
  if (1 <= level && level <= 254) {
    jref["max_performance"] = (level == 254);
    jref["min_power"] = (level == 1 || level == 128);
    jref["with_standby"] = (level < 128);
  }
}

static void print_ata_security_status(const char * msg, unsigned short state, unsigned short master_password_id)
{
  // Table 6 of T13/2015-D (ACS-2) Revision 7, June 22, 2011
  if (!(state & 0x0001)) {
    pout("%sUnavailable\n", msg);
    return;
  }

  const char * s1, * s2 = "", * s3 = "", * s4 = "";
  bool enabled = false, locked = false;
  if (!(state & 0x0002)) {
    s1 = "Disabled, ";
    if (!(state & 0x0008))
      s2 = "NOT FROZEN [SEC1]";
    else
      s2 = "frozen [SEC2]";
  }
  else {
    enabled = true;
    s1 = "ENABLED, PW level ";
    if (!(state & 0x0100))
      s2 = "HIGH";
    else
      s2 = "MAX";

    if (!(state & 0x0004)) {
      s3 = ", not locked, ";
      if (!(state & 0x0008))
        s4 = "not frozen [SEC5]";
      else
        s4 = "frozen [SEC6]";
    }
    else {
      locked = true;
      s3 = ", **LOCKED** [SEC4]";
      if (state & 0x0010)
        s4 = ", PW ATTEMPTS EXCEEDED";
    }
  }

  jout("%s%s%s%s%s\n", msg, s1, s2, s3, s4);

  json::ref jref = jglb["ata_security"];
  jref["state"] = state;
  jref["string"] = strprintf("%s%s%s%s", s1, s2, s3, s4);
  jref["enabled"] = enabled;
  if (!enabled || !locked)
    jref["frozen"] = !!(state & 0x0008);
  if (enabled) {
    jref["pw_level_max"] = !!(state & 0x0100);
    jref["locked"] = locked;
    if (locked)
      jref["pw_attempts_exceeded"] = !!(state & 0x0010);
  }
  jref["master_password_id"] = master_password_id;
}

static void print_standby_timer(const char * msg, int timer, const ata_identify_device & drive)
{
  const char * s1 = 0;
  int hours = 0, minutes = 0 , seconds = 0;

  // Table 63 of T13/2015-D (ACS-2) Revision 7, June 22, 2011
  if (timer == 0)
    s1 = "disabled";
  else if (timer <= 240)
    seconds = timer * 5, minutes = seconds / 60, seconds %= 60;
  else if (timer <= 251)
    minutes = (timer - 240) * 30, hours = minutes / 60, minutes %= 60;
  else if (timer == 252)
    minutes = 21;
  else if (timer == 253)
    s1 = "between 8 hours and 12 hours";
  else if (timer == 255)
    minutes = 21, seconds = 15;
  else
    s1 = "reserved";

  const char * s2 = "", * s3 = "";
  if (!(drive.words047_079[49-47] & 0x2000))
    s2 = " or vendor-specific";
  if (timer > 0 && (drive.words047_079[50-47] & 0xc001) == 0x4001)
    s3 = ", a vendor-specific minimum applies";

  if (s1)
    pout("%s%d (%s%s%s)\n", msg, timer, s1, s2, s3);
  else
    pout("%s%d (%02d:%02d:%02d%s%s)\n", msg, timer, hours, minutes, seconds, s2, s3);
}


int ataPrintMain (ata_device * device, const ata_print_options & options)
{
  // If requested, check power mode first
  const char * powername = 0;
  bool powerchg = false;
  if (options.powermode) {
    unsigned char powerlimit = 0xff;
    int powermode = ataCheckPowerMode(device);
    // TODO: Move to new function used by smartctl and smartd.
    switch (powermode) {
      case -1:
        if (device->is_syscall_unsup()) {
          if (options.powerexit_unsup >= 0) {
            jinf("CHECK POWER MODE not implemented, exit(%d)\n", options.powerexit_unsup);
            return options.powerexit_unsup;
          }
          jinf("CHECK POWER MODE not implemented, ignoring -n option\n"); break;
        }
        powername = "SLEEP";   powerlimit = 2;
        break;
      // Table 215 of T13/2015-D (ACS-2) Revision 7, June 22, 2011
      // Table 293 of T13/BSR INCITS 529 (ACS-4) Revision 12, February 18, 2016
      case 0x00: // PM2:Standby, EPC unavailable or Standby_z power condition
        powername = "STANDBY"; powerlimit = 3; break;
      case 0x01: // PM2:Standby, Standby_y power condition
        powername = "STANDBY_Y"; powerlimit = 3; break;
      case 0x80: // PM1:Idle, EPC unavailable
        powername = "IDLE";    powerlimit = 4; break;
      case 0x81: // PM1:Idle, Idle_a power condition
        powername = "IDLE_A";  powerlimit = 4; break;
      case 0x82: // PM1:Idle, Idle_b power condition
        powername = "IDLE_B";  powerlimit = 4; break;
      case 0x83: // PM1:Idle, Idle_c power condition
        powername = "IDLE_C";  powerlimit = 4; break;
      // 0x40/41 were declared obsolete in ACS-3 Revision 1
      case 0x40: // PM0:Active, NV Cache power mode enabled, spun down
        powername = "ACTIVE_NV_DOWN"; break;
      case 0x41: // PM0:Active, NV Cache power mode enabled, spun up
        powername = "ACTIVE_NV_UP"  ; break;
      case 0xff: // PM0:Active or PM1:Idle
        powername = "ACTIVE or IDLE"; break;

      default:
        if (options.powerexit_unsup >= 0) {
          jinf("CHECK POWER MODE returned unknown value 0x%02x, exit(%d)\n", powermode,
               options.powerexit_unsup);
          return options.powerexit_unsup;
        }
        jinf("CHECK POWER MODE returned unknown value 0x%02x, ignoring -n option\n", powermode);
        break;
    }
    if (powername) {
      if (options.powermode >= powerlimit) {
        jinf("Device is in %s mode, exit(%d)\n", powername, options.powerexit);
        return options.powerexit;
      }
      powerchg = (powermode != 0xff); // SMART tests will spin up drives
    }
  }

  // SMART values needed ?
  bool need_smart_val = (
          options.smart_check_status
       || options.smart_general_values
       || options.smart_vendor_attrib
       || options.smart_error_log
       || options.smart_selftest_log
       || options.smart_selective_selftest_log
       || options.smart_ext_error_log
       || options.smart_ext_selftest_log
       || options.smart_auto_offl_enable
       || options.smart_auto_offl_disable
       || options.smart_selftest_type != -1
  );

  // SMART must be enabled ?
  bool need_smart_enabled = (
          need_smart_val
       || options.smart_auto_save_enable
       || options.smart_auto_save_disable
  );

  // SMART feature set needed ?
  bool need_smart_support = (
          need_smart_enabled
       || options.smart_enable
       || options.smart_disable
  );

  // SMART and GP log directories needed ?
  bool need_smart_logdir = ( 
          options.smart_logdir
       || options.devstat_all_pages // devstat fallback to smartlog if needed
       || options.devstat_ssd_page
       || !options.devstat_pages.empty()
    );

  bool need_gp_logdir  = (
          options.gp_logdir
       || options.smart_ext_error_log
       || options.smart_ext_selftest_log
       || options.devstat_all_pages
       || options.devstat_ssd_page
       || !options.devstat_pages.empty()
       || options.pending_defects_log
  );

  unsigned i;
  for (i = 0; i < options.log_requests.size(); i++) {
    if (options.log_requests[i].gpl)
      need_gp_logdir = true;
    else
      need_smart_logdir = true;
  }

  // SCT commands needed ?
  bool need_sct_support = (
          options.sct_temp_sts
       || options.sct_temp_hist
       || options.sct_temp_int
       || options.sct_erc_get
       || options.sct_erc_set
       || options.sct_wcache_reorder_get
       || options.sct_wcache_reorder_set
       || options.sct_wcache_sct_get
       || options.sct_wcache_sct_set
  );

  // Exit if no further options specified
  if (!(   options.drive_info || options.show_presets
        || need_smart_support || need_smart_logdir
        || need_gp_logdir     || need_sct_support
        || options.sataphy
        || options.identify_word_level >= 0
        || options.get_set_used                      )) {
    if (powername)
      pout("Device is in %s mode\n", powername);
    else
      pout("ATA device successfully opened\n\n"
           "Use 'smartctl -a' (or '-x') to print SMART (and more) information\n\n");
    return 0;
  }

  // Start by getting Drive ID information.  We need this, to know if SMART is supported.
  int returnval = 0;
  ata_identify_device drive; memset(&drive, 0, sizeof(drive));
  unsigned char raw_drive[sizeof(drive)]; memset(&raw_drive, 0, sizeof(raw_drive));

  device->clear_err();
  int retid = ata_read_identity(device, &drive, options.fix_swapped_id, raw_drive);
  if (retid < 0) {
    pout("Read Device Identity failed: %s\n\n",
         (device->get_errno() ? device->get_errmsg() : "Unknown error"));
    pout("If this is a USB connected device, look at the various "
         "--device=TYPE variants\n");
    failuretest(MANDATORY_CMD, returnval|=FAILID);
  }
  else if (!nonempty(&drive, sizeof(drive))) {
    pout("Read Device Identity failed: empty IDENTIFY data\n\n");
    failuretest(MANDATORY_CMD, returnval|=FAILID);
  }

  // If requested, show which presets would be used for this drive and exit.
  if (options.show_presets) {
    show_presets(&drive);
    return 0;
  }

  // Use preset vendor attribute options unless user has requested otherwise.
  ata_vendor_attr_defs attribute_defs = options.attribute_defs;
  firmwarebug_defs firmwarebugs = options.firmwarebugs;
  std::string dbversion;
  const drive_settings * dbentry = 0;
  if (!options.ignore_presets) {
    dbentry = lookup_drive_apply_presets(&drive, attribute_defs,
      firmwarebugs, dbversion);
    if (!dbversion.empty())
      jglb["smartctl"]["drive_database_version"]["string"] = dbversion;
  }

  // Get capacity, sector sizes and rotation rate
  ata_size_info sizes;
  ata_get_size_info(&drive, sizes);
  int rpm = ata_get_rotation_rate(&drive);

  // Print ATA IDENTIFY info if requested
  if (options.identify_word_level >= 0) {
    pout("=== ATA IDENTIFY DATA ===\n");
    // Pass raw data without endianness adjustments
    ata_print_identify_data(raw_drive, (options.identify_word_level > 0), options.identify_bit_level);
  }

  // Print most drive identity information if requested
  if (options.drive_info) {
    pout("=== START OF INFORMATION SECTION ===\n");
    print_drive_info(&drive, sizes, rpm, dbentry, dbversion.c_str());
  }

  // Check and print SMART support and state
  int smart_supported = -1, smart_enabled = -1;
  if (need_smart_support || options.drive_info) {

    // Packet device ?
    if (retid > 0) {
      pout("SMART support is: Unavailable - Packet Interface Devices [this device: %s] don't support ATA SMART\n",
           packetdevicetype(retid-1));
    }
    else {
      // Disk device: SMART supported and enabled ?
      smart_supported = ataSmartSupport(&drive);
      smart_enabled = ataIsSmartEnabled(&drive);

      if (smart_supported < 0)
        pout("SMART support is: Ambiguous - ATA IDENTIFY DEVICE words 82-83 don't show if SMART supported.\n");
      if (smart_supported && smart_enabled < 0) {
        pout("SMART support is: Ambiguous - ATA IDENTIFY DEVICE words 85-87 don't show if SMART is enabled.\n");
        if (need_smart_support) {
          failuretest(MANDATORY_CMD, returnval|=FAILSMART);
          // check SMART support by trying a command
          pout("                  Checking to be sure by trying SMART RETURN STATUS command.\n");
          if (ataDoesSmartWork(device))
            smart_supported = smart_enabled = 1;
        }
      }
      else if (smart_supported < 0 && (smart_enabled > 0 || dbentry))
        // Assume supported if enabled or in drive database
        smart_supported = 1;

      if (smart_supported < 0)
        pout("SMART support is: Unknown - Try option -s with argument 'on' to enable it.");
      else if (!smart_supported)
        jout("SMART support is: Unavailable - device lacks SMART capability.\n");
      else {
        if (options.drive_info)
          jout("SMART support is: Available - device has SMART capability.\n");
        if (smart_enabled >= 0) {
          if (device->ata_identify_is_cached()) {
            if (options.drive_info)
              pout("                  %sabled status cached by OS, trying SMART RETURN STATUS cmd.\n",
                      (smart_enabled?"En":"Dis"));
            smart_enabled = ataDoesSmartWork(device);
          }
          if (options.drive_info)
            jout("SMART support is: %s\n",
                  (smart_enabled ? "Enabled" : "Disabled"));
        }
      }
    }

    if (options.drive_info || smart_supported <= 0) {
      jglb["smart_support"]["available"] = (smart_supported > 0);
      if (smart_supported > 0)
        jglb["smart_support"]["enabled"] = (smart_enabled > 0);
    }
  }

  // Print AAM status
  if (options.get_aam) {
    if ((drive.command_set_2 & 0xc200) != 0x4200) // word083
      pout("AAM feature is:   Unavailable\n");
    else if (!(drive.word086 & 0x0200)) {
      jout("AAM feature is:   Disabled\n");
      jglb["ata_aam"]["enabled"] = false;
    }
    else
      print_aam_level("AAM level is:     ", drive.words088_255[94-88] & 0xff,
        drive.words088_255[94-88] >> 8);
  }

  // Print APM status
  if (options.get_apm) {
    if ((drive.command_set_2 & 0xc008) != 0x4008) // word083
      pout("APM feature is:   Unavailable\n");
    else if (!(drive.word086 & 0x0008)) {
      jout("APM feature is:   Disabled\n");
      jglb["ata_apm"]["enabled"] = false;
    }
    else
      print_apm_level("APM level is:     ", drive.words088_255[91-88] & 0xff);
  }

  // Print read look-ahead status
  if (options.get_lookahead) {
    if (   (drive.command_set_2 & 0xc000) != 0x4000 // word083
        || !(drive.command_set_1 & 0x0040)         ) // word082
      pout("Rd look-ahead is: Unavailable\n");
    else {
      bool enabled = !!(drive.cfs_enable_1 & 0x0040); // word085
      jout("Rd look-ahead is: %sabled\n", (enabled ? "En" : "Dis"));
      jglb["read_lookahead"]["enabled"] = enabled;
    }
  }

  // Print write cache status
  if (options.get_wcache) {
    if (   (drive.command_set_2 & 0xc000) != 0x4000 // word083
        || !(drive.command_set_1 & 0x0020)         ) // word082
      pout("Write cache is:   Unavailable\n");
    else {
      bool enabled = !!(drive.cfs_enable_1 & 0x0020); // word085
      jout("Write cache is:   %sabled\n", (enabled ? "En" : "Dis"));
      jglb["write_cache"]["enabled"] = enabled;
    }
  }

  // Print DSN status
  unsigned short word120 = drive.words088_255[120-88];
  unsigned short word119 = drive.words088_255[119-88];
  if (options.get_dsn) {
    if (!(drive.word086 & 0x8000) // word086
       || ((word119 & 0xc200) != 0x4200) // word119
       || ((word120 & 0xc000) != 0x4000)) // word120
      pout("DSN feature is:   Unavailable\n");
    else {
      bool enabled = !!(word120 & 0x200);
      jout("DSN feature is:   %sabled\n", (enabled ? "En" : "Dis"));
      jglb["ata_dsn"]["enabled"] = enabled;
    }
  }

  // Check for ATA Security LOCK
  unsigned short word128 = drive.words088_255[128-88];
  bool locked = ((word128 & 0x0007) == 0x0007); // LOCKED|ENABLED|SUPPORTED

  // Print ATA Security status
  if (options.get_security)
    print_ata_security_status("ATA Security is:  ", word128, drive.words088_255[92-88]);

  // Print write cache reordering status
  if (options.sct_wcache_reorder_get) {
    if (!isSCTFeatureControlCapable(&drive))
      pout("Wt Cache Reorder: Unavailable\n");
    else if (locked)
      pout("Wt Cache Reorder: Unknown (SCT not supported if ATA Security is LOCKED)\n");
    else {
      int wcache_reorder = ataGetSetSCTWriteCacheReordering(device,
        false /*enable*/, false /*persistent*/, false /*set*/);

      if (-1 <= wcache_reorder && wcache_reorder <= 2)
        pout("Wt Cache Reorder: %s\n",
          (wcache_reorder == -1 ? "Unknown (SCT Feature Control command failed)" :
           wcache_reorder == 0  ? "Unknown" : // not defined in standard but returned on some drives if not set
           wcache_reorder == 1  ? "Enabled" : "Disabled"));
      else
        pout("Wt Cache Reorder: Unknown (0x%02x)\n", wcache_reorder);
    }
  }

  const char * sct_write_cache_state_desc[4] = {
    "Unknown",            // 0: not defined in standard but returned on some drives if not set
    "Controlled by ATA",  // 1: controlled ATA Set Features command
    "Force Enabled",      // 2
    "Force Disabled"      // 3
  };

  // Print SCT feature control of write cache
  if (options.sct_wcache_sct_get) {
    if (!isSCTFeatureControlCapable(&drive))
      pout("SCT Write Cache Control: Unavailable\n");
    else if (locked)
      pout("SCT Write Cache Control: Unknown (SCT not supported if ATA Security is LOCKED)\n");
    else {
      int state = ataGetSetSCTWriteCache(device, 1, false /*persistent*/, false /*set*/);
      if (-1 <= state && state <= 3)
        pout("SCT Write Cache Control: %s\n",
             (state == -1 ? "Unknown (SCT Feature Control command failed)" :
              sct_write_cache_state_desc[state]));
      else
        pout("SCT Write Cache Control: Unknown (0x%02x)\n", state);
    }
  }


  // Print remaining drive info
  if (options.drive_info) {
    // Print the (now possibly changed) power mode if available
    if (powername)
      pout("Power mode %s   %s\n", (powerchg?"was:":"is: "), powername);
    pout("\n");
  }

  // Exit if SMART is not supported but must be available to proceed
  if (smart_supported <= 0 && need_smart_support)
    failuretest(MANDATORY_CMD, returnval|=FAILSMART);

  // START OF THE ENABLE/DISABLE SECTION OF THE CODE
  if (   options.smart_disable           || options.smart_enable
      || options.smart_auto_save_disable || options.smart_auto_save_enable
      || options.smart_auto_offl_disable || options.smart_auto_offl_enable
      || options.set_aam || options.set_apm || options.set_lookahead
      || options.set_wcache || options.set_security_freeze || options.set_standby
      || options.sct_wcache_reorder_set || options.sct_wcache_sct_set || options.set_dsn)
    pout("=== START OF ENABLE/DISABLE COMMANDS SECTION ===\n");
  
  // Enable/Disable AAM
  if (options.set_aam) {
    if (options.set_aam > 0) {
      if (!ata_set_features(device, ATA_ENABLE_AAM, options.set_aam-1)) {
        pout("AAM enable failed: %s\n", device->get_errmsg());
        returnval |= FAILSMART;
      }
      else
        print_aam_level("AAM set to level ", options.set_aam-1);
    }
    else {
      if (!ata_set_features(device, ATA_DISABLE_AAM)) {
        pout("AAM disable failed: %s\n", device->get_errmsg());
        returnval |= FAILSMART;
      }
      else
        pout("AAM disabled\n");
    }
  }

  // Enable/Disable APM
  if (options.set_apm) {
    if (options.set_apm > 0) {
      if (!ata_set_features(device, ATA_ENABLE_APM, options.set_apm-1)) {
        pout("APM enable failed: %s\n", device->get_errmsg());
        returnval |= FAILSMART;
      }
      else
        print_apm_level("APM set to level ", options.set_apm-1);
    }
    else {
      if (!ata_set_features(device, ATA_DISABLE_APM)) {
        pout("APM disable failed: %s\n", device->get_errmsg());
        returnval |= FAILSMART;
      }
      else
        pout("APM disabled\n");
    }
  }

  // Enable/Disable read look-ahead
  if (options.set_lookahead) {
    bool enable = (options.set_lookahead > 0);
    if (!ata_set_features(device, (enable ? ATA_ENABLE_READ_LOOK_AHEAD : ATA_DISABLE_READ_LOOK_AHEAD))) {
        pout("Read look-ahead %sable failed: %s\n", (enable ? "en" : "dis"), device->get_errmsg());
        returnval |= FAILSMART;
    }
    else
      pout("Read look-ahead %sabled\n", (enable ? "en" : "dis"));
  }

  // Enable/Disable write cache
  if (options.set_wcache) {
    bool enable = (options.set_wcache > 0);
    if (!ata_set_features(device, (enable ? ATA_ENABLE_WRITE_CACHE : ATA_DISABLE_WRITE_CACHE))) {
        pout("Write cache %sable failed: %s\n", (enable ? "en" : "dis"), device->get_errmsg());
        returnval |= FAILSMART;
    }
    else
      pout("Write cache %sabled\n", (enable ? "en" : "dis"));
  }

  // Enable/Disable DSN
  if (options.set_dsn) {
    bool enable = (options.set_dsn > 0);
    if (!ata_set_features(device, ATA_ENABLE_DISABLE_DSN, (enable ? 0x1 : 0x2))) {
        pout("DSN %sable failed: %s\n", (enable ? "en" : "dis"), device->get_errmsg());
        returnval |= FAILSMART;
    }
    else
      pout("DSN %sabled\n", (enable ? "en" : "dis"));
  }

  // Enable/Disable write cache reordering
  if (options.sct_wcache_reorder_set) {
    bool enable = (options.sct_wcache_reorder_set > 0);
    if (!isSCTFeatureControlCapable(&drive))
      pout("Write cache reordering %sable failed: SCT Feature Control command not supported\n",
        (enable ? "en" : "dis"));
    else if (locked)
      pout("Write cache reordering %sable failed: SCT not supported if ATA Security is LOCKED\n",
        (enable ? "en" : "dis"));
    else if (ataGetSetSCTWriteCacheReordering(device,
               enable, options.sct_wcache_reorder_set_pers, true /*set*/) < 0) {
      pout("Write cache reordering %sable failed: %s\n", (enable ? "en" : "dis"), device->get_errmsg());
      returnval |= FAILSMART;
    }
    else
      pout("Write cache reordering %sabled (%s)\n", (enable ? "en" : "dis"),
           (options.sct_wcache_reorder_set_pers ? "persistent" : "volatile"));
  }

  // Enable/Disable write cache in SCT
  if (options.sct_wcache_sct_set) {
    if (!isSCTFeatureControlCapable(&drive))
      pout("SCT Feature Control of write cache failed: SCT Feature Control command not supported\n");
    else if (locked)
      pout("SCT Feature Control of write cache failed: SCT not supported if ATA Security is LOCKED\n");
    else if (ataGetSetSCTWriteCache(device,
               options.sct_wcache_sct_set, options.sct_wcache_sct_set_pers, true /*set*/) < 0) {
      pout("SCT Feature Control of write cache failed: %s\n", device->get_errmsg());
      returnval |= FAILSMART;
    }
    else
      pout("Write cache SCT Feature Control is set to: %s (%s)\n",
           sct_write_cache_state_desc[options.sct_wcache_sct_set],
           (options.sct_wcache_sct_set_pers ? "persistent" : "volatile"));
  }

  // Freeze ATA security
  if (options.set_security_freeze) {
    if (!ata_nodata_command(device, ATA_SECURITY_FREEZE_LOCK)) {
        pout("ATA SECURITY FREEZE LOCK failed: %s\n", device->get_errmsg());
        returnval |= FAILSMART;
    }
    else
      pout("ATA Security set to frozen mode\n");
  }

  // Set standby timer unless immediate standby is also requested
  if (options.set_standby && !options.set_standby_now) {
    if (!ata_nodata_command(device, ATA_IDLE, options.set_standby-1)) {
      pout("ATA IDLE command failed: %s\n", device->get_errmsg());
      returnval |= FAILSMART;
    }
    else
      print_standby_timer("Standby timer set to ", options.set_standby-1, drive);
  }

  // Enable/Disable SMART commands
  if (options.smart_enable) {
    if (ataEnableSmart(device)) {
      pout("SMART Enable failed: %s\n\n", device->get_errmsg());
      failuretest(MANDATORY_CMD, returnval|=FAILSMART);
    }
    else {
      pout("SMART Enabled.\n");
      smart_enabled = 1;
    }
  }

  // Turn off SMART on device
  if (options.smart_disable) {
    if (ataDisableSmart(device)) {
      pout("SMART Disable failed: %s\n\n", device->get_errmsg());
      failuretest(MANDATORY_CMD,returnval|=FAILSMART);
    }
  }

  // Exit if SMART is disabled but must be enabled to proceed
  if (options.smart_disable || (smart_enabled <= 0 && need_smart_enabled && !is_permissive())) {
    pout("SMART Disabled. Use option -s with argument 'on' to enable it.\n");
    if (!options.smart_disable)
      pout("(override with '-T permissive' option)\n");
    return returnval;
  }

  // Enable/Disable Auto-save attributes
  if (options.smart_auto_save_enable) {
    if (ataEnableAutoSave(device)){
      pout("SMART Enable Attribute Autosave failed: %s\n\n", device->get_errmsg());
      failuretest(MANDATORY_CMD, returnval|=FAILSMART);
    }
    else
      pout("SMART Attribute Autosave Enabled.\n");
  }

  if (options.smart_auto_save_disable) {
    if (ataDisableAutoSave(device)){
      pout("SMART Disable Attribute Autosave failed: %s\n\n", device->get_errmsg());
      failuretest(MANDATORY_CMD, returnval|=FAILSMART);
    }
    else
      pout("SMART Attribute Autosave Disabled.\n");
  }

  // Read SMART values and thresholds if necessary
  ata_smart_values smartval; memset(&smartval, 0, sizeof(smartval));
  ata_smart_thresholds_pvt smartthres; memset(&smartthres, 0, sizeof(smartthres));
  bool smart_val_ok = false, smart_thres_ok = false;

  if (need_smart_val) {
    if (ataReadSmartValues(device, &smartval)) {
      pout("Read SMART Data failed: %s\n\n", device->get_errmsg());
      failuretest(OPTIONAL_CMD, returnval|=FAILSMART);
    }
    else {
      smart_val_ok = true;

      if (options.smart_check_status || options.smart_vendor_attrib) {
        if (ataReadSmartThresholds(device, &smartthres)){
          pout("Read SMART Thresholds failed: %s\n\n", device->get_errmsg());
          failuretest(OPTIONAL_CMD, returnval|=FAILSMART);
        }
        else
          smart_thres_ok = true;
      }
    }
  }

  // Enable/Disable Off-line testing
  bool needupdate = false;
  if (options.smart_auto_offl_enable) {
    if (!isSupportAutomaticTimer(&smartval)){
      pout("SMART Automatic Timers not supported\n\n");
      failuretest(OPTIONAL_CMD, returnval|=FAILSMART);
    }
    needupdate = smart_val_ok;
    if (ataEnableAutoOffline(device)){
      pout("SMART Enable Automatic Offline failed: %s\n\n", device->get_errmsg());
      failuretest(OPTIONAL_CMD, returnval|=FAILSMART);
    }
    else
      pout("SMART Automatic Offline Testing Enabled every four hours.\n");
  }

  if (options.smart_auto_offl_disable) {
    if (!isSupportAutomaticTimer(&smartval)){
      pout("SMART Automatic Timers not supported\n\n");
      failuretest(OPTIONAL_CMD, returnval|=FAILSMART);
    }
    needupdate = smart_val_ok;
    if (ataDisableAutoOffline(device)){
      pout("SMART Disable Automatic Offline failed: %s\n\n", device->get_errmsg());
      failuretest(OPTIONAL_CMD, returnval|=FAILSMART);
    }
    else
      pout("SMART Automatic Offline Testing Disabled.\n");
  }

  if (needupdate && ataReadSmartValues(device, &smartval)){
    pout("Read SMART Data failed: %s\n\n", device->get_errmsg());
    failuretest(OPTIONAL_CMD, returnval|=FAILSMART);
    smart_val_ok = false;
  }

  // all this for a newline!
  if (   options.smart_disable           || options.smart_enable
      || options.smart_auto_save_disable || options.smart_auto_save_enable
      || options.smart_auto_offl_disable || options.smart_auto_offl_enable
      || options.set_aam || options.set_apm || options.set_lookahead
      || options.set_wcache || options.set_security_freeze || options.set_standby
      || options.sct_wcache_reorder_set || options.set_dsn)
    pout("\n");

  // START OF READ-ONLY OPTIONS APART FROM -V and -i
  if (   options.smart_check_status  || options.smart_general_values
      || options.smart_vendor_attrib || options.smart_error_log
      || options.smart_selftest_log  || options.smart_selective_selftest_log
      || options.smart_ext_error_log || options.smart_ext_selftest_log
      || options.sct_temp_sts        || options.sct_temp_hist               )
    pout("=== START OF READ SMART DATA SECTION ===\n");
  
  // Check SMART status
  if (options.smart_check_status) {

    switch (ataSmartStatus2(device)) {

    case 0:
      // The case where the disk health is OK
      jout("SMART overall-health self-assessment test result: PASSED\n");
      jglb["smart_status"]["passed"] = true;
      if (smart_thres_ok && find_failed_attr(&smartval, &smartthres, attribute_defs, 0)) {
        if (options.smart_vendor_attrib)
          pout("See vendor-specific Attribute list for marginal Attributes.\n\n");
        else {
          print_on();
          pout("Please note the following marginal Attributes:\n");
          PrintSmartAttribWithThres(&smartval, &smartthres, attribute_defs, rpm, 2, options.output_format);
        } 
        returnval|=FAILAGE;
      }
      else
        pout("\n");
      break;
      
    case 1:
      // The case where the disk health is NOT OK
      print_on();
      jout("SMART overall-health self-assessment test result: FAILED!\n"
           "Drive failure expected in less than 24 hours. SAVE ALL DATA.\n");
      jglb["smart_status"]["passed"] = false;
      print_off();
      if (smart_thres_ok && find_failed_attr(&smartval, &smartthres, attribute_defs, 1)) {
        returnval|=FAILATTR;
        if (options.smart_vendor_attrib)
          pout("See vendor-specific Attribute list for failed Attributes.\n\n");
        else {
          print_on();
          pout("Failed Attributes:\n");
          PrintSmartAttribWithThres(&smartval, &smartthres, attribute_defs, rpm, 1, options.output_format);
        }
      }
      else
        pout("No failed Attributes found.\n\n");   
      returnval|=FAILSTATUS;
      print_off();
      break;

    case -1:
    default:
      // Something went wrong with the SMART STATUS command.
      // The ATA SMART RETURN STATUS command provides the result in the ATA output
      // registers. Buggy ATA/SATA drivers and SAT Layers often do not properly
      // return the registers values.
      pout("SMART Status %s: %s\n",
           (device->is_syscall_unsup() ? "not supported" : "command failed"),
           device->get_errmsg());
      failuretest(OPTIONAL_CMD, returnval | FAILSMART);
      if (!(device->is_syscall_unsup() && smart_val_ok && smart_thres_ok))
        returnval |= FAILSMART; // Unknown error or attribute check not possible

      if (!(smart_val_ok && smart_thres_ok)) {
        print_on();
        pout("SMART overall-health self-assessment test result: UNKNOWN!\n"
             "SMART Status, Attributes and Thresholds cannot be read.\n\n");
      }
      else if (find_failed_attr(&smartval, &smartthres, attribute_defs, 1)) {
        print_on();
        jout("SMART overall-health self-assessment test result: FAILED!\n"
             "Drive failure expected in less than 24 hours. SAVE ALL DATA.\n");
        jwrn("Warning: This result is based on an Attribute check.\n");
        jglb["smart_status"]["passed"] = false;
        print_off();
        returnval|=FAILATTR;
        returnval|=FAILSTATUS;
        if (options.smart_vendor_attrib)
          pout("See vendor-specific Attribute list for failed Attributes.\n\n");
        else {
          print_on();
          pout("Failed Attributes:\n");
          PrintSmartAttribWithThres(&smartval, &smartthres, attribute_defs, rpm, 1, options.output_format);
        }
      }
      else {
        jout("SMART overall-health self-assessment test result: PASSED\n");
        jwrn("Warning: This result is based on an Attribute check.\n");
        jglb["smart_status"]["passed"] = true;
        if (find_failed_attr(&smartval, &smartthres, attribute_defs, 0)) {
          if (options.smart_vendor_attrib)
            pout("See vendor-specific Attribute list for marginal Attributes.\n\n");
          else {
            print_on();
            pout("Please note the following marginal Attributes:\n");
            PrintSmartAttribWithThres(&smartval, &smartthres, attribute_defs, rpm, 2, options.output_format);
          } 
          returnval|=FAILAGE;
        }
        else
          pout("\n");
      } 
      print_off();
      break;
    } // end of switch statement
    
    print_off();
  } // end of checking SMART Status
  
  // Print general SMART values
  if (smart_val_ok && options.smart_general_values)
    PrintGeneralSmartValues(&smartval, &drive, firmwarebugs);

  // Print vendor-specific attributes
  if (smart_val_ok && options.smart_vendor_attrib) {
    print_on();
    PrintSmartAttribWithThres(&smartval, &smartthres, attribute_defs, rpm,
                              (printing_is_switchable ? 2 : 0), options.output_format);
    print_off();
  }

  // If GP Log is supported use smart log directory for
  // error and selftest log support check.
  bool gp_log_supported = isGeneralPurposeLoggingCapable(&drive);
  if (   gp_log_supported
      && (   options.smart_error_log || options.smart_selftest_log
          || options.retry_error_log || options.retry_selftest_log))
    need_smart_logdir = true;

  ata_smart_log_directory smartlogdir_buf, gplogdir_buf;
  const ata_smart_log_directory * smartlogdir = 0, * gplogdir = 0;

  // Read SMART Log directory
  if (need_smart_logdir) {
    if (firmwarebugs.is_set(BUG_NOLOGDIR))
      smartlogdir = fake_logdir(&smartlogdir_buf, options);
    else if (ataReadLogDirectory(device, &smartlogdir_buf, false)) {
      pout("Read SMART Log Directory failed: %s\n\n", device->get_errmsg());
      failuretest(OPTIONAL_CMD, returnval|=FAILSMART);
    }
    else
      smartlogdir = &smartlogdir_buf;
  }

  // Read GP Log directory
  if (need_gp_logdir) {
    if (firmwarebugs.is_set(BUG_NOLOGDIR))
      gplogdir = fake_logdir(&gplogdir_buf, options);
    else if (!gp_log_supported && !is_permissive()) {
      if (options.gp_logdir)
        pout("General Purpose Log Directory not supported\n\n");
    }
    else if (ataReadLogDirectory(device, &gplogdir_buf, true)) {
      pout("Read GP Log Directory failed\n\n");
      failuretest(OPTIONAL_CMD, returnval|=FAILSMART);
    }
    else
      gplogdir = &gplogdir_buf;
  }

  // Print log directories
  if ((options.gp_logdir && gplogdir) || (options.smart_logdir && smartlogdir)) {
    if (firmwarebugs.is_set(BUG_NOLOGDIR))
      pout("Log Directories not read due to '-F nologdir' option\n\n");
    else
      PrintLogDirectories(gplogdir, smartlogdir);
  }

  // Print log pages
  for (i = 0; i < options.log_requests.size(); i++) {
    const ata_log_request & req = options.log_requests[i];

    const char * type;
    unsigned max_nsectors;
    if (req.gpl) {
      type = "General Purpose";
      max_nsectors = GetNumLogSectors(gplogdir, req.logaddr, true);
    }
    else {
      type = "SMART";
      max_nsectors = GetNumLogSectors(smartlogdir, req.logaddr, false);
    }

    if (!max_nsectors) {
      if (!is_permissive()) {
        pout("%s Log 0x%02x does not exist (override with '-T permissive' option)\n", type, req.logaddr);
        continue;
      }
      max_nsectors = req.page+1;
    }
    if (max_nsectors <= req.page) {
      pout("%s Log 0x%02x has only %u sectors, output skipped\n", type, req.logaddr, max_nsectors);
      continue;
    }

    unsigned ns = req.nsectors;
    if (ns > max_nsectors - req.page) {
      if (req.nsectors != ~0U) // "FIRST-max"
        pout("%s Log 0x%02x has only %u sectors, output truncated\n", type, req.logaddr, max_nsectors);
      ns = max_nsectors - req.page;
    }

    // SMART log don't support sector offset, start with first sector
    unsigned offs = (req.gpl ? 0 : req.page);

    raw_buffer log_buf((offs + ns) * 512);
    bool ok;
    if (req.gpl)
      ok = ataReadLogExt(device, req.logaddr, 0x00, req.page, log_buf.data(), ns);
    else
      ok = ataReadSmartLog(device, req.logaddr, log_buf.data(), offs + ns);
    if (!ok)
      failuretest(OPTIONAL_CMD, returnval|=FAILSMART);
    else
      PrintLogPages(type, log_buf.data() + offs*512, req.logaddr, req.page, ns, max_nsectors);
  }

  // Print SMART Extendend Comprehensive Error Log
  bool do_smart_error_log = options.smart_error_log;
  if (options.smart_ext_error_log) {
    bool ok = false;
    unsigned nsectors = GetNumLogSectors(gplogdir, 0x03, true);
    if (!nsectors)
      pout("SMART Extended Comprehensive Error Log (GP Log 0x03) not supported\n\n");
    else {
      // Read only first sector to get error count and index
      // Print function will read more sectors as needed
      ata_smart_exterrlog log_03; memset(&log_03, 0, sizeof(log_03));
      if (!ataReadExtErrorLog(device, &log_03, 0, 1, firmwarebugs)) {
        pout("Read SMART Extended Comprehensive Error Log failed\n\n");
        failuretest(OPTIONAL_CMD, returnval|=FAILSMART);
      }
      else {
        if (PrintSmartExtErrorLog(device, firmwarebugs, &log_03, nsectors, options.smart_ext_error_log))
          returnval |= FAILERR;
        ok = true;
      }
    }

    if (!ok) {
      if (options.retry_error_log)
        do_smart_error_log = true;
      else if (!do_smart_error_log)
        pout("Try '-l [xerror,]error' to read traditional SMART Error Log\n");
    }
  }

  // Print SMART error log
  if (do_smart_error_log) {
    if (!(   GetNumLogSectors(smartlogdir, 0x01, false)
          || (   !(smartlogdir && gp_log_supported)
              && isSmartErrorLogCapable(&smartval, &drive))
          || is_permissive()                               )) {
      pout("SMART Error Log not supported\n\n");
    }
    else {
      ata_smart_errorlog smarterror; memset(&smarterror, 0, sizeof(smarterror));
      if (ataReadErrorLog(device, &smarterror, firmwarebugs)) {
        pout("Read SMART Error Log failed: %s\n\n", device->get_errmsg());
        failuretest(OPTIONAL_CMD, returnval|=FAILSMART);
      }
      else {
        // quiet mode is turned on inside PrintSmartErrorLog()
        if (PrintSmartErrorlog(&smarterror, firmwarebugs))
	  returnval|=FAILERR;
        print_off();
      }
    }
  }

  // Print SMART Extendend Self-test Log
  bool do_smart_selftest_log = options.smart_selftest_log;
  if (options.smart_ext_selftest_log) {
    bool ok = false;
    unsigned nsectors = GetNumLogSectors(gplogdir, 0x07, true);
    if (!nsectors)
      pout("SMART Extended Self-test Log (GP Log 0x07) not supported\n\n");
    else if (nsectors >= 256)
      pout("SMART Extended Self-test Log size %u not supported\n\n", nsectors);
    else {
      raw_buffer log_07_buf(nsectors * 512);
      ata_smart_extselftestlog * log_07 = reinterpret_cast<ata_smart_extselftestlog *>(log_07_buf.data());
      if (!ataReadExtSelfTestLog(device, log_07, nsectors)) {
        pout("Read SMART Extended Self-test Log failed\n\n");
        failuretest(OPTIONAL_CMD, returnval|=FAILSMART);
      }
      else {
        if (PrintSmartExtSelfTestLog(log_07, nsectors, options.smart_ext_selftest_log))
          returnval |= FAILLOG;
        ok = true;
      }
    }

    if (!ok) {
      if (options.retry_selftest_log)
        do_smart_selftest_log = true;
      else if (!do_smart_selftest_log)
        pout("Try '-l [xselftest,]selftest' to read traditional SMART Self Test Log\n");
    }
  }

  // Print SMART self-test log
  if (do_smart_selftest_log) {
    if (!(   GetNumLogSectors(smartlogdir, 0x06, false)
          || (   !(smartlogdir && gp_log_supported)
              && isSmartTestLogCapable(&smartval, &drive))
          || is_permissive()                              )) {
      pout("SMART Self-test Log not supported\n\n");
    }
    else {
      ata_smart_selftestlog smartselftest; memset(&smartselftest, 0, sizeof(smartselftest));
      if (ataReadSelfTestLog(device, &smartselftest, firmwarebugs)) {
        pout("Read SMART Self-test Log failed: %s\n\n", device->get_errmsg());
        failuretest(OPTIONAL_CMD, returnval|=FAILSMART);
      }
      else {
        print_on();
        if (ataPrintSmartSelfTestlog(&smartselftest, !printing_is_switchable, firmwarebugs))
          returnval |= FAILLOG;
        print_off();
        pout("\n");
      }
    }
  }

  // Print SMART selective self-test log
  if (options.smart_selective_selftest_log) {
    ata_selective_self_test_log log;

    if (!isSupportSelectiveSelfTest(&smartval))
      pout("Selective Self-tests/Logging not supported\n\n");
    else if(ataReadSelectiveSelfTestLog(device, &log)) {
      pout("Read SMART Selective Self-test Log failed: %s\n\n", device->get_errmsg());
      failuretest(OPTIONAL_CMD, returnval|=FAILSMART);
    }
    else {
      print_on();
      // If any errors were found, they are logged in the SMART Self-test log.
      // So there is no need to print the Selective Self Test log in silent
      // mode.
      if (!printing_is_switchable)
        ataPrintSelectiveSelfTestLog(&log, &smartval);
      print_off();
      pout("\n");
    }
  }

  // Check if SCT commands available
  bool sct_ok = isSCTCapable(&drive);
  if (   options.sct_temp_sts || options.sct_temp_hist || options.sct_temp_int
      || options.sct_erc_get  || options.sct_erc_set                          ) {
    if (!sct_ok)
      pout("SCT Commands not supported\n\n");
    else if (locked) {
      pout("SCT Commands not supported if ATA Security is LOCKED\n\n");
      sct_ok = false;
    }
  }

  // Print SCT status and temperature history table
  if (sct_ok && (options.sct_temp_sts || options.sct_temp_hist || options.sct_temp_int)) {
    for (;;) {
      bool sct_temp_hist_ok = isSCTDataTableCapable(&drive);
      ata_sct_status_response sts;

      if (options.sct_temp_sts || (options.sct_temp_hist && sct_temp_hist_ok)) {
        // Read SCT status
        if (ataReadSCTStatus(device, &sts)) {
          pout("\n");
          failuretest(OPTIONAL_CMD, returnval|=FAILSMART);
          break;
        }
        if (options.sct_temp_sts) {
          ataPrintSCTStatus(&sts);
          pout("\n");
        }
      }

      if (!sct_temp_hist_ok && (options.sct_temp_hist || options.sct_temp_int)) {
        pout("SCT Data Table command not supported\n\n");
        failuretest(OPTIONAL_CMD, returnval|=FAILSMART);
        break;
      }

      if (options.sct_temp_hist) {
        // Read SCT temperature history,
        // requires initial SCT status from above
        ata_sct_temperature_history_table tmh;
        if (ataReadSCTTempHist(device, &tmh, &sts)) {
          pout("Read SCT Temperature History failed\n\n");
          failuretest(OPTIONAL_CMD, returnval|=FAILSMART);
          break;
        }
        ataPrintSCTTempHist(&tmh);
        pout("\n");
      }

      if (options.sct_temp_int) {
        // Set new temperature logging interval
        if (!isSCTFeatureControlCapable(&drive)) {
          pout("SCT Feature Control command not supported\n\n");
          failuretest(OPTIONAL_CMD, returnval|=FAILSMART);
          break;
        }
        if (ataSetSCTTempInterval(device, options.sct_temp_int, options.sct_temp_int_pers)) {
          pout("Write Temperature Logging Interval failed\n\n");
          failuretest(OPTIONAL_CMD, returnval|=FAILSMART);
          break;
        }
        pout("Temperature Logging Interval set to %u minute%s (%s)\n",
          options.sct_temp_int, (options.sct_temp_int == 1 ? "" : "s"),
          (options.sct_temp_int_pers ? "persistent" : "volatile"));
      }
      break;
    }
  }

  // SCT Error Recovery Control
  if (sct_ok && (options.sct_erc_get || options.sct_erc_set)) {
    if (!isSCTErrorRecoveryControlCapable(&drive)) {
      pout("SCT Error Recovery Control command not supported\n\n");
      failuretest(OPTIONAL_CMD, returnval|=FAILSMART);
    }
    else {
      int sct_erc_get = options.sct_erc_get;
      if (options.sct_erc_set) {
        // Set SCT Error Recovery Control
        bool set_power_on = (options.sct_erc_set == 2), mfg_default = (options.sct_erc_set == 3);
        if (   ataSetSCTErrorRecoveryControltime(device, 1, options.sct_erc_readtime, set_power_on, mfg_default)
            || ataSetSCTErrorRecoveryControltime(device, 2, options.sct_erc_writetime, set_power_on, mfg_default)) {
          pout("SCT (Set) Error Recovery Control command failed\n");
          if (!(   (options.sct_erc_readtime == 70 && options.sct_erc_writetime == 70)
                || (options.sct_erc_readtime ==  0 && options.sct_erc_writetime ==  0)))
            pout("Retry with: 'scterc,70,70' to enable ERC or 'scterc,0,0' to disable\n");
          failuretest(OPTIONAL_CMD, returnval|=FAILSMART);
          sct_erc_get = 0;
        }
        else if (!sct_erc_get)
          ataPrintSCTErrorRecoveryControl(true, options.sct_erc_readtime,
            options.sct_erc_writetime, set_power_on, mfg_default);
      }

      if (sct_erc_get) {
        // Print SCT Error Recovery Control
        bool get_power_on = (sct_erc_get == 2);
        unsigned short read_timer, write_timer;
        if (   ataGetSCTErrorRecoveryControltime(device, 1, read_timer, get_power_on)
            || ataGetSCTErrorRecoveryControltime(device, 2, write_timer, get_power_on)) {
          pout("SCT (Get) Error Recovery Control command failed\n");
          if (options.sct_erc_set == sct_erc_get) {
            pout("The previous SCT (Set) Error Recovery Control command succeeded\n");
            ataPrintSCTErrorRecoveryControl(true, options.sct_erc_readtime,
              options.sct_erc_writetime, get_power_on);
          }
          failuretest(OPTIONAL_CMD, returnval|=FAILSMART);
        }
        else
          ataPrintSCTErrorRecoveryControl(false, read_timer, write_timer, get_power_on);
      }
      pout("\n");
    }
  }

  // Print Device Statistics
  if (options.devstat_all_pages || options.devstat_ssd_page || !options.devstat_pages.empty()) {
    bool use_gplog = true;
    unsigned nsectors = 0;
    if (gplogdir) 
      nsectors = GetNumLogSectors(gplogdir, 0x04, true);
    else if (smartlogdir){ // for systems without ATA_READ_LOG_EXT
      nsectors = GetNumLogSectors(smartlogdir, 0x04, false);
      use_gplog = false;
    }
    if (!nsectors)
      pout("Device Statistics (GP/SMART Log 0x04) not supported\n\n");
    else if (!print_device_statistics(device, nsectors, options.devstat_pages,
               options.devstat_all_pages, options.devstat_ssd_page, use_gplog))
      failuretest(OPTIONAL_CMD, returnval|=FAILSMART);
  }

  // Print Pending Defects log
  if (options.pending_defects_log) {
    unsigned nsectors = GetNumLogSectors(gplogdir, 0x0c, true);
    if (!nsectors)
      pout("Pending Defects log (GP Log 0x0c) not supported\n\n");
    else if (!print_pending_defects_log(device, nsectors, options.pending_defects_log))
      failuretest(OPTIONAL_CMD, returnval|=FAILSMART);
  }

  // Print SATA Phy Event Counters
  if (options.sataphy) {
    unsigned nsectors = GetNumLogSectors(gplogdir, 0x11, true);
    // Packet interface devices do not provide a log directory, check support bit
    if (!nsectors && (drive.words047_079[76-47] & 0x0401) == 0x0400)
      nsectors = 1;
    if (!nsectors)
      pout("SATA Phy Event Counters (GP Log 0x11) not supported\n\n");
    else if (nsectors != 1)
      pout("SATA Phy Event Counters with %u sectors not supported\n\n", nsectors);
    else {
      unsigned char log_11[512] = {0, };
      unsigned char features = (options.sataphy_reset ? 0x01 : 0x00);
      if (!ataReadLogExt(device, 0x11, features, 0, log_11, 1)) {
        pout("Read SATA Phy Event Counters failed\n\n");
        failuretest(OPTIONAL_CMD, returnval|=FAILSMART);
      }
      else
        PrintSataPhyEventCounters(log_11, options.sataphy_reset);
    }
  }

  // Set to standby (spindown) mode and set standby timer if not done above
  // (Above commands may spinup drive)
  if (options.set_standby_now) {
    if (options.set_standby) {
      if (!ata_nodata_command(device, ATA_STANDBY, options.set_standby-1)) {
        pout("ATA STANDBY command failed: %s\n", device->get_errmsg());
        returnval |= FAILSMART;
      }
      else {
        print_standby_timer("Standby timer set to ", options.set_standby-1, drive);
        pout("Device placed in STANDBY mode\n");
      }
    }
    else {
      if (!ata_nodata_command(device, ATA_STANDBY_IMMEDIATE)) {
        pout("ATA STANDBY IMMEDIATE command failed: %s\n", device->get_errmsg());
        returnval |= FAILSMART;
      }
      else
        pout("Device placed in STANDBY mode\n");
    }
  }

  // START OF THE TESTING SECTION OF THE CODE.  IF NO TESTING, RETURN
  if (!smart_val_ok || options.smart_selftest_type == -1)
    return returnval;
  
  pout("=== START OF OFFLINE IMMEDIATE AND SELF-TEST SECTION ===\n");
  // if doing a self-test, be sure it's supported by the hardware
  switch (options.smart_selftest_type) {
  case OFFLINE_FULL_SCAN:
    if (!isSupportExecuteOfflineImmediate(&smartval)){
      pout("Execute Offline Immediate function not supported\n\n");
      failuretest(OPTIONAL_CMD, returnval|=FAILSMART);
    }
    break;
  case ABORT_SELF_TEST:
  case SHORT_SELF_TEST:
  case EXTEND_SELF_TEST:
  case SHORT_CAPTIVE_SELF_TEST:
  case EXTEND_CAPTIVE_SELF_TEST:
    if (!isSupportSelfTest(&smartval)){
      pout("Self-test functions not supported\n\n");
      failuretest(OPTIONAL_CMD, returnval|=FAILSMART);
    }
    break;
  case CONVEYANCE_SELF_TEST:
  case CONVEYANCE_CAPTIVE_SELF_TEST:
    if (!isSupportConveyanceSelfTest(&smartval)){
      pout("Conveyance Self-test functions not supported\n\n");
      failuretest(OPTIONAL_CMD, returnval|=FAILSMART);
    }
    break;
  case SELECTIVE_SELF_TEST:
  case SELECTIVE_CAPTIVE_SELF_TEST:
    if (!isSupportSelectiveSelfTest(&smartval)){
      pout("Selective Self-test functions not supported\n\n");
      failuretest(MANDATORY_CMD, returnval|=FAILSMART);
    }
    break;
  default:
    break; // Vendor specific type
  }

  // Now do the test.  Note ataSmartTest prints its own error/success
  // messages
  if (ataSmartTest(device, options.smart_selftest_type, options.smart_selftest_force,
                   options.smart_selective_args, &smartval, sizes.sectors            ))
    failuretest(OPTIONAL_CMD, returnval|=FAILSMART);
  else {  
    // Tell user how long test will take to complete.  This is tricky
    // because in the case of an Offline Full Scan, the completion
    // timer is volatile, and needs to be read AFTER the command is
    // given. If this will interrupt the Offline Full Scan, we don't
    // do it, just warn user.
    if (options.smart_selftest_type == OFFLINE_FULL_SCAN) {
      if (isSupportOfflineAbort(&smartval))
	pout("Note: giving further SMART commands will abort Offline testing\n");
      else if (ataReadSmartValues(device, &smartval)){
        pout("Read SMART Data failed: %s\n\n", device->get_errmsg());
	failuretest(OPTIONAL_CMD, returnval|=FAILSMART);
      }
    }
    
    // Now say how long the test will take to complete
    int timewait = TestTime(&smartval, options.smart_selftest_type);
    if (timewait) {
      time_t t=time(NULL);
      if (options.smart_selftest_type == OFFLINE_FULL_SCAN) {
	t+=timewait;
	pout("Please wait %d seconds for test to complete.\n", (int)timewait);
      } else {
	t+=timewait*60;
	pout("Please wait %d minutes for test to complete.\n", (int)timewait);
      }
      char comptime[DATEANDEPOCHLEN];
      dateandtimezoneepoch(comptime, t);
      pout("Test will complete after %s\n", comptime);
      
      if (   options.smart_selftest_type != SHORT_CAPTIVE_SELF_TEST
          && options.smart_selftest_type != EXTEND_CAPTIVE_SELF_TEST
          && options.smart_selftest_type != CONVEYANCE_CAPTIVE_SELF_TEST
          && options.smart_selftest_type != SELECTIVE_CAPTIVE_SELF_TEST )
        pout("Use smartctl -X to abort test.\n");
    }
  }

  return returnval;
}
