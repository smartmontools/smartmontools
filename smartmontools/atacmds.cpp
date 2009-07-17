/*
 * atacmds.cpp
 * 
 * Home page of code is: http://smartmontools.sourceforge.net
 *
 * Copyright (C) 2002-9 Bruce Allen <smartmontools-support@lists.sourceforge.net>
 * Copyright (C) 2008-9 Christian Franke <smartmontools-support@lists.sourceforge.net>
 * Copyright (C) 1999-2000 Michael Cornwell <cornwell@acm.org>
 * Copyright (C) 2000 Andre Hedrick <andre@linux-ide.org>
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

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <ctype.h>

#include "config.h"
#include "int64.h"
#include "atacmds.h"
#include "scsiata.h"
#include "extern.h"
#include "utility.h"
#include "dev_ata_cmd_set.h" // for parsed_ata_device

#include <algorithm> // std::sort

const char *atacmds_c_cvsid="$Id: atacmds.cpp,v 1.219 2009/07/07 19:28:29 chrfranke Exp $"
ATACMDS_H_CVSID CONFIG_H_CVSID EXTERN_H_CVSID INT64_H_CVSID SCSIATA_H_CVSID UTILITY_H_CVSID;

// for passing global control variables
extern smartmonctrl *con;

#define SMART_CYL_LOW  0x4F
#define SMART_CYL_HI   0xC2

// SMART RETURN STATUS yields SMART_CYL_HI,SMART_CYL_LOW to indicate drive
// is healthy and SRET_STATUS_HI_EXCEEDED,SRET_STATUS_MID_EXCEEDED to
// indicate that a threshhold exceeded condition has been detected.
// Those values (byte pairs) are placed in ATA register "LBA 23:8".
#define SRET_STATUS_HI_EXCEEDED 0x2C
#define SRET_STATUS_MID_EXCEEDED 0xF4

// These Drive Identity tables are taken from hdparm 5.2, and are also
// given in the ATA/ATAPI specs for the IDENTIFY DEVICE command.  Note
// that SMART was first added into the ATA/ATAPI-3 Standard with
// Revision 3 of the document, July 25, 1995.  Look at the "Document
// Status" revision commands at the beginning of
// http://www.t13.org/project/d2008r6.pdf to see this.
#define NOVAL_0                 0x0000
#define NOVAL_1                 0xffff
/* word 81: minor version number */
#define MINOR_MAX 0x22
static const char * const minor_str[] = {       /* word 81 value: */
  "Device does not report version",             /* 0x0000       */
  "ATA-1 X3T9.2 781D prior to revision 4",      /* 0x0001       */
  "ATA-1 published, ANSI X3.221-1994",          /* 0x0002       */
  "ATA-1 X3T9.2 781D revision 4",               /* 0x0003       */
  "ATA-2 published, ANSI X3.279-1996",          /* 0x0004       */
  "ATA-2 X3T10 948D prior to revision 2k",      /* 0x0005       */
  "ATA-3 X3T10 2008D revision 1",               /* 0x0006       */ /* SMART NOT INCLUDED */
  "ATA-2 X3T10 948D revision 2k",               /* 0x0007       */
  "ATA-3 X3T10 2008D revision 0",               /* 0x0008       */ 
  "ATA-2 X3T10 948D revision 3",                /* 0x0009       */
  "ATA-3 published, ANSI X3.298-199x",          /* 0x000a       */
  "ATA-3 X3T10 2008D revision 6",               /* 0x000b       */ /* 1st VERSION WITH SMART */
  "ATA-3 X3T13 2008D revision 7 and 7a",        /* 0x000c       */
  "ATA/ATAPI-4 X3T13 1153D revision 6",         /* 0x000d       */
  "ATA/ATAPI-4 T13 1153D revision 13",          /* 0x000e       */
  "ATA/ATAPI-4 X3T13 1153D revision 7",         /* 0x000f       */
  "ATA/ATAPI-4 T13 1153D revision 18",          /* 0x0010       */
  "ATA/ATAPI-4 T13 1153D revision 15",          /* 0x0011       */
  "ATA/ATAPI-4 published, ANSI NCITS 317-1998", /* 0x0012       */
  "ATA/ATAPI-5 T13 1321D revision 3",           /* 0x0013       */
  "ATA/ATAPI-4 T13 1153D revision 14",          /* 0x0014       */
  "ATA/ATAPI-5 T13 1321D revision 1",           /* 0x0015       */
  "ATA/ATAPI-5 published, ANSI NCITS 340-2000", /* 0x0016       */
  "ATA/ATAPI-4 T13 1153D revision 17",          /* 0x0017       */
  "ATA/ATAPI-6 T13 1410D revision 0",           /* 0x0018       */
  "ATA/ATAPI-6 T13 1410D revision 3a",          /* 0x0019       */
  "ATA/ATAPI-7 T13 1532D revision 1",           /* 0x001a       */
  "ATA/ATAPI-6 T13 1410D revision 2",           /* 0x001b       */
  "ATA/ATAPI-6 T13 1410D revision 1",           /* 0x001c       */
  "ATA/ATAPI-7 published, ANSI INCITS 397-2005",/* 0x001d       */
  "ATA/ATAPI-7 T13 1532D revision 0",           /* 0x001e       */
  "reserved",                                   /* 0x001f       */
  "reserved",                                   /* 0x0020       */
  "ATA/ATAPI-7 T13 1532D revision 4a",          /* 0x0021       */
  "ATA/ATAPI-6 published, ANSI INCITS 361-2002" /* 0x0022       */
};

// NOTE ATA/ATAPI-4 REV 4 was the LAST revision where the device
// attribute structures were NOT completely vendor specific.  So any
// disk that is ATA/ATAPI-4 or above can not be trusted to show the
// vendor values in sensible format.

// Negative values below are because it doesn't support SMART
static const int actual_ver[] = { 
  /* word 81 value: */
  0,            /* 0x0000       WARNING:        */
  1,            /* 0x0001       WARNING:        */
  1,            /* 0x0002       WARNING:        */
  1,            /* 0x0003       WARNING:        */
  2,            /* 0x0004       WARNING:   This array           */
  2,            /* 0x0005       WARNING:   corresponds          */
  -3, /*<== */  /* 0x0006       WARNING:   *exactly*            */
  2,            /* 0x0007       WARNING:   to the ATA/          */
  -3, /*<== */  /* 0x0008       WARNING:   ATAPI version        */
  2,            /* 0x0009       WARNING:   listed in            */
  3,            /* 0x000a       WARNING:   the                  */
  3,            /* 0x000b       WARNING:   minor_str            */
  3,            /* 0x000c       WARNING:   array                */
  4,            /* 0x000d       WARNING:   above.               */
  4,            /* 0x000e       WARNING:                        */
  4,            /* 0x000f       WARNING:   If you change        */
  4,            /* 0x0010       WARNING:   that one,            */
  4,            /* 0x0011       WARNING:   change this one      */
  4,            /* 0x0012       WARNING:   too!!!               */
  5,            /* 0x0013       WARNING:        */
  4,            /* 0x0014       WARNING:        */
  5,            /* 0x0015       WARNING:        */
  5,            /* 0x0016       WARNING:        */
  4,            /* 0x0017       WARNING:        */
  6,            /* 0x0018       WARNING:        */
  6,            /* 0x0019       WARNING:        */
  7,            /* 0x001a       WARNING:        */
  6,            /* 0x001b       WARNING:        */
  6,            /* 0x001c       WARNING:        */
  7,            /* 0x001d       WARNING:        */
  7,            /* 0x001e       WARNING:        */
  0,            /* 0x001f       WARNING:        */
  0,            /* 0x0020       WARNING:        */
  7,            /* 0x0021       WARNING:        */
  6             /* 0x0022       WARNING:        */
};

// When you add additional items to this list, you should then:
// 0 -- update this list
// 1 -- if needed, modify ataPrintSmartAttribRawValue()
// 2 -  if needed, modify ataPrintSmartAttribName()
// 3 -- add drive in question into builtin_knowndrives[] table in knowndrives.cpp
// 4 -- update smartctl.8
// 5 -- update smartd.8
// 6 -- do "make smartd.conf.5" to update smartd.conf.5
// 7 -- update CHANGELOG file

struct vendor_attr_arg_entry
{
  unsigned char id;  // attribute ID, 0 for all
  const char * name; // attribute name
  unsigned char val; // value for attribute defs array
};

// The order of these entries is (only) relevant for '-v help' output.
const vendor_attr_arg_entry vendor_attribute_args[] = {
  {  9,"halfminutes", 4},
  {  9,"minutes", 1},
  {  9,"seconds", 3},
  {  9,"temp", 2},
  {192,"emergencyretractcyclect", 1},
  {193,"loadunload", 1},
  {194,"10xCelsius", 1},
  {194,"unknown", 2},
  {197,"increasing", 1},
  {198,"offlinescanuncsectorct", 2},
  {198,"increasing", 1},
  {200,"writeerrorcount", 1},
  {201,"detectedtacount", 1},
  {220,"temp", 1},
  {  0,"raw8", 253},
  {  0,"raw16", 254},
  {  0,"raw48", 255},
};

const unsigned num_vendor_attribute_args = sizeof(vendor_attribute_args)/sizeof(vendor_attribute_args[0]);

// Get ID and increase flag of current pending or offline
// uncorrectable attribute.
unsigned char get_unc_attr_id(bool offline, const unsigned char * defs,
                              bool & increase)
{
  unsigned char id = (!offline ? 197 : 198);
  increase = (defs[id] == 1);
  return id;
}

// This are the meanings of the Self-test failure checkpoint byte.
// This is in the self-test log at offset 4 bytes into the self-test
// descriptor and in the SMART READ DATA structure at byte offset
// 371. These codes are not well documented.  The meanings returned by
// this routine are used (at least) by Maxtor and IBM. Returns NULL if
// not recognized.  Currently the maximum length is 15 bytes.
const char *SelfTestFailureCodeName(unsigned char which){
  
  switch (which) {
  case 0:
    return "Write_Test";
  case 1:
    return "Servo_Basic";
  case 2:
    return "Servo_Random";
  case 3:
    return "G-list_Scan";
  case 4:
    return "Handling_Damage";
  case 5:
    return "Read_Scan";
  default:
    return NULL;
  }
}

// This is a utility function for parsing pairs like "9,minutes" or
// "220,temp", and putting the correct flag into the attributedefs
// array.  Returns 1 if problem, 0 if pair has been recongized.
int parse_attribute_def(const char * pair, unsigned char * defs)
{
  int id = 0, nc = -1;
  char name[32+1];
  if (pair[0] == 'N') {
    // "N,name"
    if (!(sscanf(pair, "N,%32s%n", name, &nc) == 1 && nc == (int)strlen(pair)))
      return 1;
  }
  else {
    // "attr,name"
    if (!(   sscanf(pair, "%d,%32s%n", &id, name, &nc) == 2
          && 1 <= id && id <= 255 && nc == (int)strlen(pair)))
      return 1;
  }

  // Find pair
  unsigned i;
  for (i = 0; ; i++) {
    if (i >= num_vendor_attribute_args)
      return 1; // Not found
    if (   (!vendor_attribute_args[i].id || vendor_attribute_args[i].id == id)
        && !strcmp(vendor_attribute_args[i].name, name)                       )
      break;
  }

  if (!id) {
    // "N,name" -> set all entries
    for (int j = 0; j < MAX_ATTRIBUTE_NUM; j++)
      defs[j] = vendor_attribute_args[i].val;
  }
  else
    // "attr,name"
    defs[id] = vendor_attribute_args[i].val;

  return 0;
}

// Return a multiline string containing a list of valid arguments for
// parse_attribute_def().  The strings are preceeded by tabs and followed
// (except for the last) by newlines.
std::string create_vendor_attribute_arg_list()
{
  std::string s;
  for (unsigned i = 0; i < num_vendor_attribute_args; i++) {
    if (i > 0)
      s += '\n';
    if (!vendor_attribute_args[i].id)
      s += "\tN,";
    else
      s += strprintf("\t%d,", vendor_attribute_args[i].id);
    s += vendor_attribute_args[i].name;
  }
  return s;
}

// swap two bytes.  Point to low address
void swap2(char *location){
  char tmp=*location;
  *location=*(location+1);
  *(location+1)=tmp;
  return;
}

// swap four bytes.  Point to low address
void swap4(char *location){
  char tmp=*location;
  *location=*(location+3);
  *(location+3)=tmp;
  swap2(location+1);
  return;
}

// swap eight bytes.  Points to low address
void swap8(char *location){
  char tmp=*location;
  *location=*(location+7);
  *(location+7)=tmp;
  tmp=*(location+1);
  *(location+1)=*(location+6);
  *(location+6)=tmp;
  swap4(location+2);
  return;
}

// Invalidate serial number and adjust checksum in IDENTIFY data
static void invalidate_serno(ata_identify_device * id){
  unsigned char sum = 0;
  for (unsigned i = 0; i < sizeof(id->serial_no); i++) {
    sum += id->serial_no[i]; sum -= id->serial_no[i] = 'X';
  }
#ifndef __NetBSD__
  bool must_swap = !!isbigendian();
  if (must_swap)
    swapx(id->words088_255+255-88);
#endif
  if ((id->words088_255[255-88] & 0x00ff) == 0x00a5)
    id->words088_255[255-88] += sum << 8;
#ifndef __NetBSD__
  if (must_swap)
    swapx(id->words088_255+255-88);
#endif
}

static const char * const commandstrings[]={
  "SMART ENABLE",
  "SMART DISABLE",
  "SMART AUTOMATIC ATTRIBUTE SAVE",
  "SMART IMMEDIATE OFFLINE",
  "SMART AUTO OFFLINE",
  "SMART STATUS",
  "SMART STATUS CHECK",
  "SMART READ ATTRIBUTE VALUES",
  "SMART READ ATTRIBUTE THRESHOLDS",
  "SMART READ LOG",
  "IDENTIFY DEVICE",
  "IDENTIFY PACKET DEVICE",
  "CHECK POWER MODE",
  "SMART WRITE LOG",
  "WARNING (UNDEFINED COMMAND -- CONTACT DEVELOPERS AT " PACKAGE_BUGREPORT ")\n"
};


static const char * preg(const ata_register & r, char * buf)
{
  if (!r.is_set())
    //return "n/a ";
    return "....";
  sprintf(buf, "0x%02x", r.val()); return buf;
}

void print_regs(const char * prefix, const ata_in_regs & r, const char * suffix = "\n")
{
  char bufs[7][4+1+13];
  pout("%s FR=%s, SC=%s, LL=%s, LM=%s, LH=%s, DEV=%s, CMD=%s%s", prefix,
    preg(r.features, bufs[0]), preg(r.sector_count, bufs[1]), preg(r.lba_low, bufs[2]),
    preg(r.lba_mid, bufs[3]), preg(r.lba_high, bufs[4]), preg(r.device, bufs[5]),
    preg(r.command, bufs[6]), suffix);
}

void print_regs(const char * prefix, const ata_out_regs & r, const char * suffix = "\n")
{
  char bufs[7][4+1+13];
  pout("%sERR=%s, SC=%s, LL=%s, LM=%s, LH=%s, DEV=%s, STS=%s%s", prefix,
    preg(r.error, bufs[0]), preg(r.sector_count, bufs[1]), preg(r.lba_low, bufs[2]),
    preg(r.lba_mid, bufs[3]), preg(r.lba_high, bufs[4]), preg(r.device, bufs[5]),
    preg(r.status, bufs[6]), suffix);
}

static void prettyprint(const unsigned char *p, const char *name){
  pout("\n===== [%s] DATA START (BASE-16) =====\n", name);
  for (int i=0; i<512; i+=16, p+=16)
    // print complete line to avoid slow tty output and extra lines in syslog.
    pout("%03d-%03d: %02x %02x %02x %02x %02x %02x %02x %02x "
                    "%02x %02x %02x %02x %02x %02x %02x %02x\n",
         i, i+16-1,
         p[ 0], p[ 1], p[ 2], p[ 3], p[ 4], p[ 5], p[ 6], p[ 7],
         p[ 8], p[ 9], p[10], p[11], p[12], p[13], p[14], p[15]);
  pout("===== [%s] DATA END (512 Bytes) =====\n\n", name);
}

// This function provides the pretty-print reporting for SMART
// commands: it implements the various -r "reporting" options for ATA
// ioctls.
int smartcommandhandler(ata_device * device, smart_command_set command, int select, char *data){
  // TODO: Rework old stuff below
  // This conditional is true for commands that return data
  int getsdata=(command==PIDENTIFY || 
                command==IDENTIFY || 
                command==READ_LOG || 
                command==READ_THRESHOLDS || 
                command==READ_VALUES ||
                command==CHECK_POWER_MODE);

  int sendsdata=(command==WRITE_LOG);
  
  // If reporting is enabled, say what the command will be before it's executed
  if (con->reportataioctl){
          // conditional is true for commands that use parameters
          int usesparam=(command==READ_LOG || 
                         command==AUTO_OFFLINE || 
                         command==AUTOSAVE || 
                         command==IMMEDIATE_OFFLINE ||
                         command==WRITE_LOG);
                  
    pout("\nREPORT-IOCTL: Device=%s Command=%s", device->get_dev_name(), commandstrings[command]);
    if (usesparam)
      pout(" InputParameter=%d\n", select);
    else
      pout("\n");
  }
  
  if ((getsdata || sendsdata) && !data){
    pout("REPORT-IOCTL: Unable to execute command %s : data destination address is NULL\n", commandstrings[command]);
    return -1;
  }
  
  // The reporting is cleaner, and we will find coding bugs faster, if
  // the commands that failed clearly return empty (zeroed) data
  // structures
  if (getsdata) {
    if (command==CHECK_POWER_MODE)
      data[0]=0;
    else
      memset(data, '\0', 512);
  }


  // if requested, pretty-print the input data structure
  if (con->reportataioctl>1 && sendsdata)
    //pout("REPORT-IOCTL: Device=%s Command=%s\n", device->get_dev_name(), commandstrings[command]);
    prettyprint((unsigned char *)data, commandstrings[command]);

  // now execute the command
  int retval = -1;
  {
    ata_cmd_in in;
    // Set common register values
    switch (command) {
      default: // SMART commands
        in.in_regs.command = ATA_SMART_CMD;
        in.in_regs.lba_high = SMART_CYL_HI; in.in_regs.lba_mid = SMART_CYL_LOW;
        break;
      case IDENTIFY: case PIDENTIFY: case CHECK_POWER_MODE: // Non SMART commands
        break;
    }
    // Set specific values
    switch (command) {
      case IDENTIFY:
        in.in_regs.command = ATA_IDENTIFY_DEVICE;
        in.set_data_in(data, 1);
        break;
      case PIDENTIFY:
        in.in_regs.command = ATA_IDENTIFY_PACKET_DEVICE;
        in.set_data_in(data, 1);
        break;
      case CHECK_POWER_MODE:
        in.in_regs.command = ATA_CHECK_POWER_MODE;
        in.out_needed.sector_count = true; // Powermode returned here
        break;
      case READ_VALUES:
        in.in_regs.features = ATA_SMART_READ_VALUES;
        in.set_data_in(data, 1);
        break;
      case READ_THRESHOLDS:
        in.in_regs.features = ATA_SMART_READ_THRESHOLDS;
        in.in_regs.lba_low = 1; // TODO: CORRECT ???
        in.set_data_in(data, 1);
        break;
      case READ_LOG:
        in.in_regs.features = ATA_SMART_READ_LOG_SECTOR;
        in.in_regs.lba_low = select;
        in.set_data_in(data, 1);
        break;
      case WRITE_LOG:
        in.in_regs.features = ATA_SMART_WRITE_LOG_SECTOR;
        in.in_regs.lba_low = select;
        in.set_data_out(data, 1);
        break;
      case ENABLE:
        in.in_regs.features = ATA_SMART_ENABLE;
        in.in_regs.lba_low = 1; // TODO: CORRECT ???
        break;
      case DISABLE:
        in.in_regs.features = ATA_SMART_DISABLE;
        in.in_regs.lba_low = 1;  // TODO: CORRECT ???
        break;
      case STATUS_CHECK:
        in.out_needed.lba_high = in.out_needed.lba_mid = true; // Status returned here
      case STATUS:
        in.in_regs.features = ATA_SMART_STATUS;
        break;
      case AUTO_OFFLINE:
        in.in_regs.features = ATA_SMART_AUTO_OFFLINE;
        in.in_regs.sector_count = select;  // Caution: Non-DATA command!
        break;
      case AUTOSAVE:
        in.in_regs.features = ATA_SMART_AUTOSAVE;
        in.in_regs.sector_count = select;  // Caution: Non-DATA command!
        break;
      case IMMEDIATE_OFFLINE:
        in.in_regs.features = ATA_SMART_IMMEDIATE_OFFLINE;
        in.in_regs.lba_low = select;
        break;
      default:
        pout("Unrecognized command %d in smartcommandhandler()\n"
             "Please contact " PACKAGE_BUGREPORT "\n", command);
        device->set_err(ENOSYS);
        errno = ENOSYS;
        return -1;
    }

    if (con->reportataioctl)
      print_regs(" Input:  ", in.in_regs,
        (in.direction==ata_cmd_in::data_in ? " IN\n":
         in.direction==ata_cmd_in::data_out ? " OUT\n":"\n"));

    ata_cmd_out out;
    bool ok = device->ata_pass_through(in, out);

    if (con->reportataioctl && out.out_regs.is_set())
      print_regs(" Output: ", out.out_regs);

    if (ok) switch (command) {
      default:
        retval = 0;
        break;
      case CHECK_POWER_MODE:
        data[0] = out.out_regs.sector_count;
        retval = 0;
        break;
      case STATUS_CHECK:
        // Cyl low and Cyl high unchanged means "Good SMART status"
        if ((out.out_regs.lba_high == SMART_CYL_HI) &&
            (out.out_regs.lba_mid == SMART_CYL_LOW))
          retval = 0;
        // These values mean "Bad SMART status"
        else if ((out.out_regs.lba_high == SRET_STATUS_HI_EXCEEDED) &&
                 (out.out_regs.lba_mid == SRET_STATUS_MID_EXCEEDED))
          retval = 1;
        else if (out.out_regs.lba_mid == SMART_CYL_LOW) {
          retval = 0;
          if (con->reportataioctl)
            pout("SMART STATUS RETURN: half healthy response sequence, "
                 "probable SAT/USB truncation\n");
          } else if (out.out_regs.lba_mid == SRET_STATUS_MID_EXCEEDED) {
          retval = 1;
          if (con->reportataioctl)
            pout("SMART STATUS RETURN: half unhealthy response sequence, "
                 "probable SAT/USB truncation\n");
        } else {
          // We haven't gotten output that makes sense; print out some debugging info
          pout("Error SMART Status command failed\n"
               "Please get assistance from %s\n", PACKAGE_HOMEPAGE);
          errno = EIO;
          retval = -1;
        }
        break;
    }
  }

  // If requested, invalidate serial number before any printing is done
  if ((command == IDENTIFY || command == PIDENTIFY) && !retval && con->dont_print_serial)
    invalidate_serno((ata_identify_device *)data);

  // If reporting is enabled, say what output was produced by the command
  if (con->reportataioctl){
    if (device->get_errno())
      pout("REPORT-IOCTL: Device=%s Command=%s returned %d errno=%d [%s]\n",
           device->get_dev_name(), commandstrings[command], retval,
           device->get_errno(), device->get_errmsg());
    else
      pout("REPORT-IOCTL: Device=%s Command=%s returned %d\n",
           device->get_dev_name(), commandstrings[command], retval);
    
    // if requested, pretty-print the output data structure
    if (con->reportataioctl>1 && getsdata) {
      if (command==CHECK_POWER_MODE)
	pout("Sector Count Register (BASE-16): %02x\n", (unsigned char)(*data));
      else
	prettyprint((unsigned char *)data, commandstrings[command]);
    }
  }

  errno = device->get_errno(); // TODO: Callers should not call syserror()
  return retval;
}

// Get number of sectors from IDENTIFY sector. If the drive doesn't
// support LBA addressing or has no user writable sectors
// (eg, CDROM or DVD) then routine returns zero.
uint64_t get_num_sectors(const ata_identify_device * drive)
{
  unsigned short command_set_2  = drive->command_set_2;
  unsigned short capabilities_0 = drive->words047_079[49-47];
  unsigned short sects_16       = drive->words047_079[60-47];
  unsigned short sects_32       = drive->words047_079[61-47];
  unsigned short lba_16         = drive->words088_255[100-88];
  unsigned short lba_32         = drive->words088_255[101-88];
  unsigned short lba_48         = drive->words088_255[102-88];
  unsigned short lba_64         = drive->words088_255[103-88];

  // LBA support?
  if (!(capabilities_0 & 0x0200))
    return 0; // No

  // if drive supports LBA addressing, determine 32-bit LBA capacity
  uint64_t lba32 = (unsigned int)sects_32 << 16 |
                   (unsigned int)sects_16 << 0  ;

  uint64_t lba64 = 0;
  // if drive supports 48-bit addressing, determine THAT capacity
  if ((command_set_2 & 0xc000) == 0x4000 && (command_set_2 & 0x0400))
      lba64 = (uint64_t)lba_64 << 48 |
              (uint64_t)lba_48 << 32 |
              (uint64_t)lba_32 << 16 |
              (uint64_t)lba_16 << 0  ;

  // return the larger of the two possible capacities
  return (lba32 > lba64 ? lba32 : lba64);
}

// This function computes the checksum of a single disk sector (512
// bytes).  Returns zero if checksum is OK, nonzero if the checksum is
// incorrect.  The size (512) is correct for all SMART structures.
unsigned char checksum(const unsigned char *buffer)
{
  unsigned char sum=0;
  int i;
  
  for (i=0; i<512; i++)
    sum+=buffer[i];

  return sum;
}

// Copies n bytes (or n-1 if n is odd) from in to out, but swaps adjacents
// bytes.
static void swapbytes(char * out, const char * in, size_t n)
{
  for (size_t i = 0; i < n; i += 2) {
    out[i]   = in[i+1];
    out[i+1] = in[i];
  }
}

// Copies in to out, but removes leading and trailing whitespace.
static void trim(char * out, const char * in)
{
  // Find the first non-space character (maybe none).
  int first = -1;
  int i;
  for (i = 0; in[i]; i++)
    if (!isspace((int)in[i])) {
      first = i;
      break;
    }

  if (first == -1) {
    // There are no non-space characters.
    out[0] = '\0';
    return;
  }

  // Find the last non-space character.
  for (i = strlen(in)-1; i >= first && isspace((int)in[i]); i--)
    ;
  int last = i;

  strncpy(out, in+first, last-first+1);
  out[last-first+1] = '\0';
}

// Convenience function for formatting strings from ata_identify_device
void format_ata_string(char * out, const char * in, int n, bool fix_swap)
{
  bool must_swap = !fix_swap;
#ifdef __NetBSD__
  /* NetBSD kernel delivers IDENTIFY data in host byte order (but all else is LE) */
  if (isbigendian())
    must_swap = !must_swap;
#endif

  char tmp[65];
  n = n > 64 ? 64 : n;
  if (!must_swap)
    strncpy(tmp, in, n);
  else
    swapbytes(tmp, in, n);
  tmp[n] = '\0';
  trim(out, tmp);
}

// returns -1 if command fails or the device is in Sleep mode, else
// value of Sector Count register.  Sector Count result values:
//   00h device is in Standby mode. 
//   80h device is in Idle mode.
//   FFh device is in Active mode or Idle mode.

int ataCheckPowerMode(ata_device * device) {
  unsigned char result;

  if ((smartcommandhandler(device, CHECK_POWER_MODE, 0, (char *)&result)))
    return -1;

  if (result!=0 && result!=0x80 && result!=0xff)
    pout("ataCheckPowerMode(): ATA CHECK POWER MODE returned unknown Sector Count Register value %02x\n", result);

  return (int)result;
}




// Reads current Device Identity info (512 bytes) into buf.  Returns 0
// if all OK.  Returns -1 if no ATA Device identity can be
// established.  Returns >0 if Device is ATA Packet Device (not SMART
// capable).  The value of the integer helps identify the type of
// Packet device, which is useful so that the user can connect the
// formal device number with whatever object is inside their computer.
int ataReadHDIdentity (ata_device * device, struct ata_identify_device *buf){
  unsigned short *rawshort=(unsigned short *)buf;
  unsigned char  *rawbyte =(unsigned char  *)buf;

  // See if device responds either to IDENTIFY DEVICE or IDENTIFY
  // PACKET DEVICE
  if ((smartcommandhandler(device, IDENTIFY, 0, (char *)buf))){
    if (smartcommandhandler(device, PIDENTIFY, 0, (char *)buf)){
      return -1; 
    }
  }

#ifndef __NetBSD__
  // if machine is big-endian, swap byte order as needed
  // NetBSD kernel delivers IDENTIFY data in host byte order
  if (isbigendian()){
    int i;
    
    // swap various capability words that are needed
    for (i=0; i<33; i++)
      swap2((char *)(buf->words047_079+i));
    
    for (i=80; i<=87; i++)
      swap2((char *)(rawshort+i));
    
    for (i=0; i<168; i++)
      swap2((char *)(buf->words088_255+i));
  }
#endif
  
  // If there is a checksum there, validate it
  if ((rawshort[255] & 0x00ff) == 0x00a5 && checksum(rawbyte))
    checksumwarning("Drive Identity Structure");
  
  // If this is a PACKET DEVICE, return device type
  if (rawbyte[1] & 0x80)
    return 1+(rawbyte[1] & 0x1f);
  
  // Not a PACKET DEVICE
  return 0;
}

// Returns ATA version as an integer, and a pointer to a string
// describing which revision.  Note that Revision 0 of ATA-3 does NOT
// support SMART.  For this one case we return -3 rather than +3 as
// the version number.  See notes above.
int ataVersionInfo(const char ** description, const ata_identify_device * drive, unsigned short * minor)
{
  // check that arrays at the top of this file are defined
  // consistently
  if (sizeof(minor_str) != sizeof(char *)*(1+MINOR_MAX)){
    pout("Internal error in ataVersionInfo().  minor_str[] size %d\n"
         "is not consistent with value of MINOR_MAX+1 = %d\n", 
         (int)(sizeof(minor_str)/sizeof(char *)), MINOR_MAX+1);
    fflush(NULL);
    abort();
  }
  if (sizeof(actual_ver) != sizeof(int)*(1+MINOR_MAX)){
    pout("Internal error in ataVersionInfo().  actual_ver[] size %d\n"
         "is not consistent with value of MINOR_MAX = %d\n",
         (int)(sizeof(actual_ver)/sizeof(int)), MINOR_MAX+1);
    fflush(NULL);
    abort();
  }

  // get major and minor ATA revision numbers
  unsigned short major = drive->major_rev_num;
  *minor=drive->minor_rev_num;
  
  // First check if device has ANY ATA version information in it
  if (major==NOVAL_0 || major==NOVAL_1) {
    *description=NULL;
    return -1;
  }
  
  // The minor revision number has more information - try there first
  if (*minor && (*minor<=MINOR_MAX)){
    int std = actual_ver[*minor];
    if (std) {
      *description=minor_str[*minor];
      return std;
    }
  }

  // Try new ATA-8 minor revision numbers (Table 31 of T13/1699-D Revision 6)
  // (not in actual_ver/minor_str to avoid large sparse tables)
  const char *desc;
  switch (*minor) {
    case 0x0027: desc = "ATA-8-ACS revision 3c"; break;
    case 0x0028: desc = "ATA-8-ACS revision 6"; break;
    case 0x0029: desc = "ATA-8-ACS revision 4"; break;
    case 0x0033: desc = "ATA-8-ACS revision 3e"; break;
    case 0x0039: desc = "ATA-8-ACS revision 4c"; break;
    case 0x0042: desc = "ATA-8-ACS revision 3f"; break;
    case 0x0052: desc = "ATA-8-ACS revision 3b"; break;
    case 0x0107: desc = "ATA-8-ACS revision 2d"; break;
    default:     desc = 0; break;
  }
  if (desc) {
    *description = desc;
    return 8;
  }

  // HDPARM has a very complicated algorithm from here on. Since SMART only
  // exists on ATA-3 and later standards, let's punt on this.  If you don't
  // like it, please fix it.  The code's in CVS.
  int i;
  for (i=15; i>0; i--)
    if (major & (0x1<<i))
      break;
  
  *description=NULL; 
  if (i==0)
    return 1;
  else
    return i;
}

// returns 1 if SMART supported, 0 if SMART unsupported, -1 if can't tell
int ataSmartSupport(const ata_identify_device * drive)
{
  unsigned short word82=drive->command_set_1;
  unsigned short word83=drive->command_set_2;
  
  // check if words 82/83 contain valid info
  if ((word83>>14) == 0x01)
    // return value of SMART support bit 
    return word82 & 0x0001;
  
  // since we can're rely on word 82, we don't know if SMART supported
  return -1;
}

// returns 1 if SMART enabled, 0 if SMART disabled, -1 if can't tell
int ataIsSmartEnabled(const ata_identify_device * drive)
{
  unsigned short word85=drive->cfs_enable_1;
  unsigned short word87=drive->csf_default;
  
  // check if words 85/86/87 contain valid info
  if ((word87>>14) == 0x01)
    // return value of SMART enabled bit
    return word85 & 0x0001;
  
  // Since we can't rely word85, we don't know if SMART is enabled.
  return -1;
}


// Reads SMART attributes into *data
int ataReadSmartValues(ata_device * device, struct ata_smart_values *data){
  
  if (smartcommandhandler(device, READ_VALUES, 0, (char *)data)){
    syserror("Error SMART Values Read failed");
    return -1;
  }

  // compute checksum
  if (checksum((unsigned char *)data))
    checksumwarning("SMART Attribute Data Structure");
  
  // swap endian order if needed
  if (isbigendian()){
    int i;
    swap2((char *)&(data->revnumber));
    swap2((char *)&(data->total_time_to_complete_off_line));
    swap2((char *)&(data->smart_capability));
    for (i=0; i<NUMBER_ATA_SMART_ATTRIBUTES; i++){
      struct ata_smart_attribute *x=data->vendor_attributes+i;
      swap2((char *)&(x->flags));
    }
  }

  return 0;
}


// This corrects some quantities that are byte reversed in the SMART
// SELF TEST LOG
static void fixsamsungselftestlog(ata_smart_selftestlog * data)
{
  // bytes 508/509 (numbered from 0) swapped (swap of self-test index
  // with one byte of reserved.
  swap2((char *)&(data->mostrecenttest));

  // LBA low register (here called 'selftestnumber", containing
  // information about the TYPE of the self-test) is byte swapped with
  // Self-test execution status byte.  These are bytes N, N+1 in the
  // entries.
  for (int i = 0; i < 21; i++)
    swap2((char *)&(data->selftest_struct[i].selftestnumber));

  return;
}

// Reads the Self Test Log (log #6)
int ataReadSelfTestLog (ata_device * device, ata_smart_selftestlog * data,
                        unsigned char fix_firmwarebug)
{

  // get data from device
  if (smartcommandhandler(device, READ_LOG, 0x06, (char *)data)){
    syserror("Error SMART Error Self-Test Log Read failed");
    return -1;
  }

  // compute its checksum, and issue a warning if needed
  if (checksum((unsigned char *)data))
    checksumwarning("SMART Self-Test Log Structure");
  
  // fix firmware bugs in self-test log
  if (fix_firmwarebug == FIX_SAMSUNG)
    fixsamsungselftestlog(data);

  // swap endian order if needed
  if (isbigendian()){
    int i;
    swap2((char*)&(data->revnumber));
    for (i=0; i<21; i++){
      struct ata_smart_selftestlog_struct *x=data->selftest_struct+i;
      swap2((char *)&(x->timestamp));
      swap4((char *)&(x->lbafirstfailure));
    }
  }

  return 0;
}

// Read SMART Extended Self-test Log
bool ataReadExtSelfTestLog(ata_device * device, ata_smart_extselftestlog * log,
                           unsigned nsectors)
{
  if (!ataReadLogExt(device, 0x07, 0x00, 0, log, nsectors))
    return false;

  for (unsigned i = 0; i < nsectors; i++) {
    if (checksum((const unsigned char *)(log + i)))
      checksumwarning("SMART Extended Self-test Log Structure");
  }

  if (isbigendian()) {
    swapx(&log->log_desc_index);
    for (unsigned i = 0; i < nsectors; i++) {
      for (unsigned j = 0; j < 19; j++)
        swapx(&log->log_descs[i].timestamp);
    }
  }
  return true;
}


// Read GP Log page(s)
bool ataReadLogExt(ata_device * device, unsigned char logaddr,
                   unsigned char features, unsigned page,
                   void * data, unsigned nsectors)
{
  ata_cmd_in in;
  in.in_regs.command      = ATA_READ_LOG_EXT;
  in.in_regs.features     = features; // log specific
  in.set_data_in_48bit(data, nsectors);
  in.in_regs.lba_low      = logaddr;
  in.in_regs.lba_mid_16   = page;

  if (!device->ata_pass_through(in)) { // TODO: Debug output
    if (nsectors <= 1) {
      pout("ATA_READ_LOG_EXT (addr=0x%02x:0x%02x, page=%u, n=%u) failed: %s\n",
           logaddr, features, page, nsectors, device->get_errmsg());
      return false;
    }

    // Recurse to retry with single sectors,
    // multi-sector reads may not be supported by ioctl.
    for (unsigned i = 0; i < nsectors; i++) {
      if (!ataReadLogExt(device, logaddr,
                         features, page + i,
                         (char *)data + 512*i, 1))
        return false;
    }
  }

  return true;
}

// Read SMART Log page(s)
bool ataReadSmartLog(ata_device * device, unsigned char logaddr,
                     void * data, unsigned nsectors)
{
  ata_cmd_in in;
  in.in_regs.command  = ATA_SMART_CMD;
  in.in_regs.features = ATA_SMART_READ_LOG_SECTOR;
  in.set_data_in(data, nsectors);
  in.in_regs.lba_high = SMART_CYL_HI;
  in.in_regs.lba_mid  = SMART_CYL_LOW;
  in.in_regs.lba_low  = logaddr;

  if (!device->ata_pass_through(in)) { // TODO: Debug output
    pout("ATA_SMART_READ_LOG failed: %s\n", device->get_errmsg());
    return false;
  }
  return true;
}



// Reads the SMART or GPL Log Directory (log #0)
int ataReadLogDirectory(ata_device * device, ata_smart_log_directory * data, bool gpl)
{
  if (!gpl) { // SMART Log directory
    if (smartcommandhandler(device, READ_LOG, 0x00, (char *)data))
      return -1;
  }
  else { // GP Log directory
    if (!ataReadLogExt(device, 0x00, 0x00, 0, data, 1))
      return -1;
  }

  // swap endian order if needed
  if (isbigendian())
    swapx(&data->logversion);

  return 0;
}


// Reads the selective self-test log (log #9)
int ataReadSelectiveSelfTestLog(ata_device * device, struct ata_selective_self_test_log *data){
  
  // get data from device
  if (smartcommandhandler(device, READ_LOG, 0x09, (char *)data)){
    syserror("Error SMART Read Selective Self-Test Log failed");
    return -1;
  }
   
  // compute its checksum, and issue a warning if needed
  if (checksum((unsigned char *)data))
    checksumwarning("SMART Selective Self-Test Log Structure");
  
  // swap endian order if needed
  if (isbigendian()){
    int i;
    swap2((char *)&(data->logversion));
    for (i=0;i<5;i++){
      swap8((char *)&(data->span[i].start));
      swap8((char *)&(data->span[i].end));
    }
    swap8((char *)&(data->currentlba));
    swap2((char *)&(data->currentspan));
    swap2((char *)&(data->flags));
    swap2((char *)&(data->pendingtime));
  }
  
  if (data->logversion != 1)
    pout("Note: selective self-test log revision number (%d) not 1 implies that no selective self-test has ever been run\n", data->logversion);
  
  return 0;
}

// Writes the selective self-test log (log #9)
int ataWriteSelectiveSelfTestLog(ata_device * device, ata_selective_selftest_args & args,
                                 const ata_smart_values * sv, uint64_t num_sectors)
{
  // Disk size must be known
  if (!num_sectors) {
    pout("Disk size is unknown, unable to check selective self-test spans\n");
    return -1;
  }

  // Read log
  struct ata_selective_self_test_log sstlog, *data=&sstlog;
  unsigned char *ptr=(unsigned char *)data;
  if (ataReadSelectiveSelfTestLog(device, data)) {
    pout("Since Read failed, will not attempt to WRITE Selective Self-test Log\n");
    return -1;
  }
  
  // Set log version
  data->logversion = 1;

  // Host is NOT allowed to write selective self-test log if a selective
  // self-test is in progress.
  if (0<data->currentspan && data->currentspan<6 && ((sv->self_test_exec_status)>>4)==15) {
    pout("Error SMART Selective or other Self-Test in progress.\n");
    return -4;
  }

  // Set start/end values based on old spans for special -t select,... options
  int i;
  for (i = 0; i < args.num_spans; i++) {
    int mode = args.span[i].mode;
    uint64_t start = args.span[i].start;
    uint64_t end   = args.span[i].end;
    if (mode == SEL_CONT) {// redo or next dependig on last test status
      switch (sv->self_test_exec_status >> 4) {
        case 1: case 2: // Aborted/Interrupted by host
          pout("Continue Selective Self-Test: Redo last span\n");
          mode = SEL_REDO;
          break;
        default: // All others
          pout("Continue Selective Self-Test: Start next span\n");
          mode = SEL_NEXT;
          break;
      }
    }
    switch (mode) {
      case SEL_RANGE: // -t select,START-END
        break;
      case SEL_REDO: // -t select,redo... => Redo current
        start = data->span[i].start;
        if (end > 0) { // -t select,redo+SIZE
          end--; end += start; // [oldstart, oldstart+SIZE)
        }
        else // -t select,redo
          end = data->span[i].end; // [oldstart, oldend]
        break;
      case SEL_NEXT: // -t select,next... => Do next
        if (data->span[i].end == 0) {
          start = end = 0; break; // skip empty spans
        }
        start = data->span[i].end + 1;
        if (start >= num_sectors)
          start = 0; // wrap around
        if (end > 0) { // -t select,next+SIZE
          end--; end += start; // (oldend, oldend+SIZE]
        }
        else { // -t select,next
          uint64_t oldsize = data->span[i].end - data->span[i].start + 1;
          end = start + oldsize - 1; // (oldend, oldend+oldsize]
          if (end >= num_sectors) {
            // Adjust size to allow round-robin testing without future size decrease
            uint64_t spans = (num_sectors + oldsize-1) / oldsize;
            uint64_t newsize = (num_sectors + spans-1) / spans;
            uint64_t newstart = num_sectors - newsize, newend = num_sectors - 1;
            pout("Span %d changed from %"PRIu64"-%"PRIu64" (%"PRIu64" sectors)\n",
                 i, start, end, oldsize);
            pout("                 to %"PRIu64"-%"PRIu64" (%"PRIu64" sectors) (%"PRIu64" spans)\n",
                 newstart, newend, newsize, spans);
            start = newstart; end = newend;
          }
        }
        break;
      default:
        pout("ataWriteSelectiveSelfTestLog: Invalid mode %d\n", mode);
        return -1;
    }
    // Range check
    if (start < num_sectors && num_sectors <= end) {
      if (end != ~(uint64_t)0) // -t select,N-max
        pout("Size of self-test span %d decreased according to disk size\n", i);
      end = num_sectors - 1;
    }
    if (!(start <= end && end < num_sectors)) {
      pout("Invalid selective self-test span %d: %"PRIu64"-%"PRIu64" (%"PRIu64" sectors)\n",
        i, start, end, num_sectors);
      return -1;
    }
    // Return the actual mode and range to caller.
    args.span[i].mode  = mode;
    args.span[i].start = start;
    args.span[i].end   = end;
  }

  // Clear spans
  for (i=0; i<5; i++)
    memset(data->span+i, 0, sizeof(struct test_span));
  
  // Set spans for testing 
  for (i = 0; i < args.num_spans; i++){
    data->span[i].start = args.span[i].start;
    data->span[i].end   = args.span[i].end;
  }

  // host must initialize to zero before initiating selective self-test
  data->currentlba=0;
  data->currentspan=0;
  
  // Perform off-line scan after selective test?
  if (args.scan_after_select == 1)
    // NO
    data->flags &= ~SELECTIVE_FLAG_DOSCAN;
  else if (args.scan_after_select == 2)
    // YES
    data->flags |= SELECTIVE_FLAG_DOSCAN;
  
  // Must clear active and pending flags before writing
  data->flags &= ~(SELECTIVE_FLAG_ACTIVE);  
  data->flags &= ~(SELECTIVE_FLAG_PENDING);

  // modify pending time?
  if (args.pending_time)
    data->pendingtime = (unsigned short)(args.pending_time-1);

  // Set checksum to zero, then compute checksum
  data->checksum=0;
  unsigned char cksum=0;
  for (i=0; i<512; i++)
    cksum+=ptr[i];
  cksum=~cksum;
  cksum+=1;
  data->checksum=cksum;

  // swap endian order if needed
  if (isbigendian()){
    swap2((char *)&(data->logversion));
    for (int i=0;i<5;i++){
      swap8((char *)&(data->span[i].start));
      swap8((char *)&(data->span[i].end));
    }
    swap8((char *)&(data->currentlba));
    swap2((char *)&(data->currentspan));
    swap2((char *)&(data->flags));
    swap2((char *)&(data->pendingtime));
  }

  // write new selective self-test log
  if (smartcommandhandler(device, WRITE_LOG, 0x09, (char *)data)){
    syserror("Error Write Selective Self-Test Log failed");
    return -3;
  }

  return 0;
}

// This corrects some quantities that are byte reversed in the SMART
// ATA ERROR LOG.
static void fixsamsungerrorlog(ata_smart_errorlog * data)
{
  // FIXED IN SAMSUNG -25 FIRMWARE???
  // Device error count in bytes 452-3
  swap2((char *)&(data->ata_error_count));
  
  // FIXED IN SAMSUNG -22a FIRMWARE
  // step through 5 error log data structures
  for (int i = 0; i < 5; i++){
    // step through 5 command data structures
    for (int j = 0; j < 5; j++)
      // Command data structure 4-byte millisec timestamp.  These are
      // bytes (N+8, N+9, N+10, N+11).
      swap4((char *)&(data->errorlog_struct[i].commands[j].timestamp));
    // Error data structure two-byte hour life timestamp.  These are
    // bytes (N+28, N+29).
    swap2((char *)&(data->errorlog_struct[i].error_struct.timestamp));
  }
  return;
}

// NEEDED ONLY FOR SAMSUNG -22 (some) -23 AND -24?? FIRMWARE
static void fixsamsungerrorlog2(ata_smart_errorlog * data)
{
  // Device error count in bytes 452-3
  swap2((char *)&(data->ata_error_count));
  return;
}

// Reads the Summary SMART Error Log (log #1). The Comprehensive SMART
// Error Log is #2, and the Extended Comprehensive SMART Error log is
// #3
int ataReadErrorLog (ata_device * device, ata_smart_errorlog *data,
                     unsigned char fix_firmwarebug)
{
  
  // get data from device
  if (smartcommandhandler(device, READ_LOG, 0x01, (char *)data)){
    syserror("Error SMART Error Log Read failed");
    return -1;
  }
  
  // compute its checksum, and issue a warning if needed
  if (checksum((unsigned char *)data))
    checksumwarning("SMART ATA Error Log Structure");
  
  // Some disks have the byte order reversed in some SMART Summary
  // Error log entries
  if (fix_firmwarebug == FIX_SAMSUNG)
    fixsamsungerrorlog(data);
  else if (fix_firmwarebug == FIX_SAMSUNG2)
    fixsamsungerrorlog2(data);

  // swap endian order if needed
  if (isbigendian()){
    int i,j;
    
    // Device error count in bytes 452-3
    swap2((char *)&(data->ata_error_count));
    
    // step through 5 error log data structures
    for (i=0; i<5; i++){
      // step through 5 command data structures
      for (j=0; j<5; j++)
        // Command data structure 4-byte millisec timestamp
        swap4((char *)&(data->errorlog_struct[i].commands[j].timestamp));
      // Error data structure life timestamp
      swap2((char *)&(data->errorlog_struct[i].error_struct.timestamp));
    }
  }
  
  return 0;
}

// Read Extended Comprehensive Error Log
bool ataReadExtErrorLog(ata_device * device, ata_smart_exterrlog * log,
                        unsigned nsectors)
{
  if (!ataReadLogExt(device, 0x03, 0x00, 0, log, nsectors))
    return false;

  for (unsigned i = 0; i < nsectors; i++) {
    if (checksum((const unsigned char *)(log + i)))
      checksumwarning("SMART ATA Extended Comprehensive Error Log Structure");
  }

  if (isbigendian()) {
    swapx(&log->device_error_count);
    swapx(&log->error_log_index);

    for (unsigned i = 0; i < nsectors; i++) {
      for (unsigned j = 0; j < 4; j++)
        swapx(&log->error_logs[i].commands[j].timestamp);
      swapx(&log->error_logs[i].error.timestamp);
    }
  }

  return true;
}


int ataReadSmartThresholds (ata_device * device, struct ata_smart_thresholds_pvt *data){
  
  // get data from device
  if (smartcommandhandler(device, READ_THRESHOLDS, 0, (char *)data)){
    syserror("Error SMART Thresholds Read failed");
    return -1;
  }
  
  // compute its checksum, and issue a warning if needed
  if (checksum((unsigned char *)data))
    checksumwarning("SMART Attribute Thresholds Structure");
  
  // swap endian order if needed
  if (isbigendian())
    swap2((char *)&(data->revnumber));

  return 0;
}

int ataEnableSmart (ata_device * device ){
  if (smartcommandhandler(device, ENABLE, 0, NULL)){
    syserror("Error SMART Enable failed");
    return -1;
  }
  return 0;
}

int ataDisableSmart (ata_device * device ){
  
  if (smartcommandhandler(device, DISABLE, 0, NULL)){
    syserror("Error SMART Disable failed");
    return -1;
  }  
  return 0;
}

int ataEnableAutoSave(ata_device * device){
  if (smartcommandhandler(device, AUTOSAVE, 241, NULL)){
    syserror("Error SMART Enable Auto-save failed");
    return -1;
  }
  return 0;
}

int ataDisableAutoSave(ata_device * device){
  
  if (smartcommandhandler(device, AUTOSAVE, 0, NULL)){
    syserror("Error SMART Disable Auto-save failed");
    return -1;
  }
  return 0;
}

// In *ALL* ATA standards the Enable/Disable AutoOffline command is
// marked "OBSOLETE". It is defined in SFF-8035i Revision 2, and most
// vendors still support it for backwards compatibility. IBM documents
// it for some drives.
int ataEnableAutoOffline (ata_device * device){
  
  /* timer hard coded to 4 hours */  
  if (smartcommandhandler(device, AUTO_OFFLINE, 248, NULL)){
    syserror("Error SMART Enable Automatic Offline failed");
    return -1;
  }
  return 0;
}

// Another Obsolete Command.  See comments directly above, associated
// with the corresponding Enable command.
int ataDisableAutoOffline (ata_device * device){
  
  if (smartcommandhandler(device, AUTO_OFFLINE, 0, NULL)){
    syserror("Error SMART Disable Automatic Offline failed");
    return -1;
  }
  return 0;
}

// If SMART is enabled, supported, and working, then this call is
// guaranteed to return 1, else zero.  Note that it should return 1
// regardless of whether the disk's SMART status is 'healthy' or
// 'failing'.
int ataDoesSmartWork(ata_device * device){
  int retval=smartcommandhandler(device, STATUS, 0, NULL);

  if (-1 == retval)
    return 0;

  return 1;
}

// This function uses a different interface (DRIVE_TASK) than the
// other commands in this file.
int ataSmartStatus2(ata_device * device){
  return smartcommandhandler(device, STATUS_CHECK, 0, NULL);  
}

// This is the way to execute ALL tests: offline, short self-test,
// extended self test, with and without captive mode, etc.
// TODO: Move to ataprint.cpp ?
int ataSmartTest(ata_device * device, int testtype, const ata_selective_selftest_args & selargs,
                 const ata_smart_values * sv, uint64_t num_sectors)
{
  char cmdmsg[128]; const char *type, *captive;
  int errornum, cap, retval, select=0;

  // Boolean, if set, says test is captive
  cap=testtype & CAPTIVE_MASK;

  // Set up strings that describe the type of test
  if (cap)
    captive="captive";
  else
    captive="off-line";
  
  if (testtype==OFFLINE_FULL_SCAN)
    type="off-line";
  else  if (testtype==SHORT_SELF_TEST || testtype==SHORT_CAPTIVE_SELF_TEST)
    type="Short self-test";
  else if (testtype==EXTEND_SELF_TEST || testtype==EXTEND_CAPTIVE_SELF_TEST)
    type="Extended self-test";
  else if (testtype==CONVEYANCE_SELF_TEST || testtype==CONVEYANCE_CAPTIVE_SELF_TEST)
    type="Conveyance self-test";
  else if ((select=(testtype==SELECTIVE_SELF_TEST || testtype==SELECTIVE_CAPTIVE_SELF_TEST)))
    type="Selective self-test";
  else
    type="[Unrecognized] self-test";
  
  // If doing a selective self-test, first use WRITE_LOG to write the
  // selective self-test log.
  ata_selective_selftest_args selargs_io = selargs; // filled with info about actual spans
  if (select && (retval = ataWriteSelectiveSelfTestLog(device, selargs_io, sv, num_sectors))) {
    if (retval==-4)
      pout("Can't start selective self-test without aborting current test: use '-X' option to smartctl.\n");
    return retval;
  }

  //  Print ouf message that we are sending the command to test
  if (testtype==ABORT_SELF_TEST)
    sprintf(cmdmsg,"Abort SMART off-line mode self-test routine");
  else
    sprintf(cmdmsg,"Execute SMART %s routine immediately in %s mode",type,captive);
  pout("Sending command: \"%s\".\n",cmdmsg);

  if (select) {
    int i;
    pout("SPAN         STARTING_LBA           ENDING_LBA\n");
    for (i = 0; i < selargs_io.num_spans; i++)
      pout("   %d %20"PRId64" %20"PRId64"\n", i,
           selargs_io.span[i].start,
           selargs_io.span[i].end);
  }
  
  // Now send the command to test
  errornum=smartcommandhandler(device, IMMEDIATE_OFFLINE, testtype, NULL);
  
  if (errornum && !(cap && errno==EIO)){
    char errormsg[128];
    sprintf(errormsg,"Command \"%s\" failed",cmdmsg); 
    syserror(errormsg);
    pout("\n");
    return -1;
  }
  
  // Since the command succeeded, tell user
  if (testtype==ABORT_SELF_TEST)
    pout("Self-testing aborted!\n");
  else
    pout("Drive command \"%s\" successful.\nTesting has begun.\n",cmdmsg);
  return 0;
}

/* Test Time Functions */
int TestTime(const ata_smart_values *data, int testtype)
{
  switch (testtype){
  case OFFLINE_FULL_SCAN:
    return (int) data->total_time_to_complete_off_line;
  case SHORT_SELF_TEST:
  case SHORT_CAPTIVE_SELF_TEST:
    return (int) data->short_test_completion_time;
  case EXTEND_SELF_TEST:
  case EXTEND_CAPTIVE_SELF_TEST:
    return (int) data->extend_test_completion_time;
  case CONVEYANCE_SELF_TEST:
  case CONVEYANCE_CAPTIVE_SELF_TEST:
    return (int) data->conveyance_test_completion_time;
  default:
    return 0;
  }
}

// This function tells you both about the ATA error log and the
// self-test error log capability (introduced in ATA-5).  The bit is
// poorly documented in the ATA/ATAPI standard.  Starting with ATA-6,
// SMART error logging is also indicated in bit 0 of DEVICE IDENTIFY
// word 84 and 87.  Top two bits must match the pattern 01. BEFORE
// ATA-6 these top two bits still had to match the pattern 01, but the
// remaining bits were reserved (==0).
int isSmartErrorLogCapable (const ata_smart_values * data, const ata_identify_device * identity)
{
  unsigned short word84=identity->command_set_extension;
  unsigned short word87=identity->csf_default;
  int isata6=identity->major_rev_num & (0x01<<6);
  int isata7=identity->major_rev_num & (0x01<<7);

  if ((isata6 || isata7) && (word84>>14) == 0x01 && (word84 & 0x01))
    return 1;
  
  if ((isata6 || isata7) && (word87>>14) == 0x01 && (word87 & 0x01))
    return 1;
  
  // otherwise we'll use the poorly documented capability bit
  return data->errorlog_capability & 0x01;
}

// See previous function.  If the error log exists then the self-test
// log should (must?) also exist.
int isSmartTestLogCapable (const ata_smart_values * data, const ata_identify_device *identity)
{
  unsigned short word84=identity->command_set_extension;
  unsigned short word87=identity->csf_default;
  int isata6=identity->major_rev_num & (0x01<<6);
  int isata7=identity->major_rev_num & (0x01<<7);

  if ((isata6 || isata7) && (word84>>14) == 0x01 && (word84 & 0x02))
    return 1;
  
  if ((isata6 || isata7) && (word87>>14) == 0x01 && (word87 & 0x02))
    return 1;


  // otherwise we'll use the poorly documented capability bit
  return data->errorlog_capability & 0x01;
}


int isGeneralPurposeLoggingCapable(const ata_identify_device *identity)
{
  unsigned short word84=identity->command_set_extension;
  unsigned short word87=identity->csf_default;

  // If bit 14 of word 84 is set to one and bit 15 of word 84 is
  // cleared to zero, the contents of word 84 contains valid support
  // information. If not, support information is not valid in this
  // word.
  if ((word84>>14) == 0x01)
    // If bit 5 of word 84 is set to one, the device supports the
    // General Purpose Logging feature set.
    return (word84 & (0x01 << 5));
  
  // If bit 14 of word 87 is set to one and bit 15 of word 87 is
  // cleared to zero, the contents of words (87:85) contain valid
  // information. If not, information is not valid in these words.  
  if ((word87>>14) == 0x01)
    // If bit 5 of word 87 is set to one, the device supports
    // the General Purpose Logging feature set.
    return (word87 & (0x01 << 5));

  // not capable
  return 0;
}


// SMART self-test capability is also indicated in bit 1 of DEVICE
// IDENTIFY word 87 (if top two bits of word 87 match pattern 01).
// However this was only introduced in ATA-6 (but self-test log was in
// ATA-5).
int isSupportExecuteOfflineImmediate(const ata_smart_values *data)
{
  return data->offline_data_collection_capability & 0x01;
}

// Note in the ATA-5 standard, the following bit is listed as "Vendor
// Specific".  So it may not be reliable. The only use of this that I
// have found is in IBM drives, where it is well-documented.  See for
// example page 170, section 13.32.1.18 of the IBM Travelstar 40GNX
// hard disk drive specifications page 164 Revision 1.1 22 Apr 2002.
int isSupportAutomaticTimer(const ata_smart_values * data)
{
  return data->offline_data_collection_capability & 0x02;
}
int isSupportOfflineAbort(const ata_smart_values *data)
{
  return data->offline_data_collection_capability & 0x04;
}
int isSupportOfflineSurfaceScan(const ata_smart_values * data)
{
   return data->offline_data_collection_capability & 0x08;
}
int isSupportSelfTest (const ata_smart_values * data)
{
   return data->offline_data_collection_capability & 0x10;
}
int isSupportConveyanceSelfTest(const ata_smart_values * data)
{
   return data->offline_data_collection_capability & 0x20;
}
int isSupportSelectiveSelfTest(const ata_smart_values * data)
{
   return data->offline_data_collection_capability & 0x40;
}



// Loop over all valid attributes.  If they are prefailure attributes
// and are at or below the threshold value, then return the ID of the
// first failing attribute found.  Return 0 if all prefailure
// attributes are in bounds.  The spec says "Bit 0
// -Pre-failure/advisory - If the value of this bit equals zero, an
// attribute value less than or equal to its corresponding attribute
// threshold indicates an advisory condition where the usage or age of
// the device has exceeded its intended design life period. If the
// value of this bit equals one, an atribute value less than or equal
// to its corresponding attribute threshold indicates a pre-failure
// condition where imminent loss of data is being predicted."


// onlyfailed=0 : are or were any age or prefailure attributes <= threshold
// onlyfailed=1:  are any prefailure attributes <= threshold now
int ataCheckSmart(const ata_smart_values * data,
                  const ata_smart_thresholds_pvt * thresholds,
                  int onlyfailed)
{
  // loop over all attributes
  for (int i = 0; i < NUMBER_ATA_SMART_ATTRIBUTES; i++){

    // pointers to disk's values and vendor's thresholds
    const ata_smart_attribute * disk = data->vendor_attributes+i;
    const ata_smart_threshold_entry * thre = thresholds->thres_entries+i;
 
    // consider only valid attributes
    if (disk->id && thre->id){
      int failednow,failedever;
      
      failednow =disk->current <= thre->threshold;
      failedever=disk->worst   <= thre->threshold;
      
      if (!onlyfailed && failedever)
        return disk->id;
      
      if (onlyfailed && failednow && ATTRIBUTE_FLAGS_PREFAILURE(disk->flags))
        return disk->id;      
    }
  }
  return 0;
}



// This checks the n'th attribute in the attribute list, NOT the
// attribute with id==n.  If the attribute does not exist, or the
// attribute is > threshold, then returns zero.  If the attribute is
// <= threshold (failing) then we the attribute number if it is a
// prefail attribute.  Else we return minus the attribute number if it
// is a usage attribute.
int ataCheckAttribute(const ata_smart_values * data,
                      const ata_smart_thresholds_pvt * thresholds,
                      int n)
{
  if (n<0 || n>=NUMBER_ATA_SMART_ATTRIBUTES || !data || !thresholds)
    return 0;
  
  // pointers to disk's values and vendor's thresholds
  const ata_smart_attribute * disk = data->vendor_attributes+n;
  const ata_smart_threshold_entry * thre = thresholds->thres_entries+n;

  if (!disk || !thre)
    return 0;
  
  // consider only valid attributes, check for failure
  if (!disk->id || !thre->id || (disk->id != thre->id) || disk->current> thre->threshold)
    return 0;
  
  // We have found a failed attribute.  Return positive or negative? 
  if (ATTRIBUTE_FLAGS_PREFAILURE(disk->flags))
    return disk->id;
  else
    return -1*(disk->id);
}


// Print temperature value and Min/Max value if present
static void ataPrintTemperatureValue(char *out, const unsigned char *raw, const unsigned *word)
{
  out+=sprintf(out, "%u", word[0]);
  if (!word[1] && !word[2])
    return; // No Min/Max

  unsigned lo = ~0, hi = ~0;
  if (!raw[3]) {
    // 00 HH 00 LL 00 TT (IBM)
    hi = word[2]; lo = word[1];
  }
  else if (!word[2]) {
    // 00 00 HH LL 00 TT (Maxtor)
    hi = raw[3]; lo = raw[2];
  }
  if (lo > hi) {
    unsigned t = lo; lo = hi; hi = t;
  }
  if (lo <= word[0] && word[0] <= hi)
    sprintf(out, " (Lifetime Min/Max %u/%u)", lo, hi);
  else
    sprintf(out, " (%u %u %u %u)", raw[5], raw[4], raw[3], raw[2]);
}


// This routine prints the raw value of an attribute as a text string
// into out. It also returns this 48-bit number as a long long.  The
// array defs[] contains non-zero values if particular attributes have
// non-default interpretations.

int64_t ataPrintSmartAttribRawValue(char *out, 
                                    const ata_smart_attribute * attribute,
                                    const unsigned char * defs){
  int64_t rawvalue;
  unsigned word[3];
  int j;
  unsigned char select;
  
  // convert the six individual bytes to a long long (8 byte) integer.
  // This is the value that we'll eventually return.
  rawvalue = 0;
  for (j=0; j<6; j++) {
    // This looks a bit roundabout, but is necessary.  Don't
    // succumb to the temptation to use raw[j]<<(8*j) since under
    // the normal rules this will be promoted to the native type.
    // On a 32 bit machine this might then overflow.
    int64_t temp;
    temp = attribute->raw[j];
    temp <<= 8*j;
    rawvalue |= temp;
  }

  // convert quantities to three two-byte words
  for (j=0; j<3; j++){
    word[j] = attribute->raw[2*j+1];
    word[j] <<= 8;
    word[j] |= attribute->raw[2*j];
  }
  
  // if no data array, Attributes have default interpretations
  if (defs)
    select=defs[attribute->id];
  else
    select=0;

  // Print six one-byte quantities.
  if (select==253){
    for (j=0; j<5; j++)
      out+=sprintf(out, "%d ", attribute->raw[5-j]);
    out+=sprintf(out, "%d ", attribute->raw[0]);
    return rawvalue;
  } 
  
  // Print three two-byte quantities
  if (select==254){
    out+=sprintf(out, "%d %d %d", word[2], word[1], word[0]); 
    return rawvalue;
  } 
  
  // Print one six-byte quantity
  if (select==255){
    out+=sprintf(out, "%"PRIu64, rawvalue);
    return rawvalue;
  }

  // This switch statement is where we handle Raw attributes
  // that are stored in an unusual vendor-specific format,
  switch (attribute->id){
    // Spin-up time
  case 3:
    out+=sprintf(out, "%d", word[0]);
    // if second nonzero then it stores the average spin-up time
    if (word[1])
      out+=sprintf(out, " (Average %d)", word[1]);
    break;
    // reallocated sector count
  case 5:
    out+=sprintf(out, "%u", word[0]);
    if (word[1] || word[2])
      out+=sprintf(out, " (%u, %u)", word[2], word[1]);
    break;
    // Power on time
  case 9:
    if (select==1){
      // minutes
      int64_t temp=word[0]+(word[1]<<16);
      int64_t tmp1=temp/60;
      int64_t tmp2=temp%60;
      out+=sprintf(out, "%"PRIu64"h+%02"PRIu64"m", tmp1, tmp2);
      if (word[2])
        out+=sprintf(out, " (%u)", word[2]);
    }
    else if (select==3){
      // seconds
      int64_t hours=rawvalue/3600;
      int64_t minutes=(rawvalue-3600*hours)/60;
      int64_t seconds=rawvalue%60;
      out+=sprintf(out, "%"PRIu64"h+%02"PRIu64"m+%02"PRIu64"s", hours, minutes, seconds);
    }
    else if (select==4){
      // 30-second counter
      int64_t tmp1=rawvalue/120;
      int64_t tmp2=(rawvalue-120*tmp1)/2;
      out+=sprintf(out, "%"PRIu64"h+%02"PRIu64"m", tmp1, tmp2);
    }
    else
      // hours
      out+=sprintf(out, "%"PRIu64, rawvalue);  //stored in hours
    break;
    // Temperature
  case 190:
    ataPrintTemperatureValue(out, attribute->raw, word);
    break;
   // Load unload cycles
  case 193:
    if (select==1){
      // loadunload
      long load  =attribute->raw[0] + (attribute->raw[1]<<8) + (attribute->raw[2]<<16);
      long unload=attribute->raw[3] + (attribute->raw[4]<<8) + (attribute->raw[5]<<16);
      out+=sprintf(out, "%lu/%lu", load, unload);
    }
    else
      // associated
      out+=sprintf(out, "%"PRIu64, rawvalue);
    break;
    // Temperature
  case 194:
    if (select==1){
      // ten times temperature in Celsius
      int deg=word[0]/10;
      int tenths=word[0]%10;
      out+=sprintf(out, "%d.%d", deg, tenths);
    }
    else if (select==2)
      // unknown attribute
      out+=sprintf(out, "%"PRIu64, rawvalue);
    else
      ataPrintTemperatureValue(out, attribute->raw, word);
    break;
    // reallocated event count
  case 196:
    out+=sprintf(out, "%u", word[0]);
    if (word[1] || word[2])
      out+=sprintf(out, " (%u, %u)", word[2], word[1]);
    break;
  default:
    out+=sprintf(out, "%"PRIu64, rawvalue);
  }
  
  // Return the full value
  return rawvalue;
}


// Note some attribute names appear redundant because different
// manufacturers use different attribute IDs for an attribute with the
// same name.  The variable val should contain a non-zero value if a particular
// attributes has a non-default interpretation.
void ataPrintSmartAttribName(char * out, unsigned char id, const unsigned char * definitions){
  const char *name;
  unsigned char val;

  // If no data array, use default interpretations
  if (definitions)
    val=definitions[id];
  else
    val=0;

  switch (id){
    
  case 1:
    name="Raw_Read_Error_Rate";
    break;
  case 2:
    name="Throughput_Performance";
    break;
  case 3:
    name="Spin_Up_Time";
    break;
  case 4:
    name="Start_Stop_Count";
    break;
  case 5:
    name="Reallocated_Sector_Ct";
    break;
  case 6:
    name="Read_Channel_Margin";
    break;
  case 7:
    name="Seek_Error_Rate";
    break;
  case 8:
    name="Seek_Time_Performance";
    break;
  case 9:
    switch (val) {
    case 1:
      name="Power_On_Minutes";
      break;
    case 2:
      name="Temperature_Celsius";
      break;
    case 3:
      name="Power_On_Seconds";
      break;
    case 4:
      name="Power_On_Half_Minutes";
      break;
    default:
      name="Power_On_Hours";
      break;
    }
    break;
  case 10:
    name="Spin_Retry_Count";
    break;
  case 11:
    name="Calibration_Retry_Count";
    break;
  case 12:
    name="Power_Cycle_Count";
    break;
  case 13:
    name="Read_Soft_Error_Rate";
    break;
  case 178:
    name="Used_Rsvd_Blk_Cnt_Chip";
    break;
  case 179:
    name="Used_Rsvd_Blk_Cnt_Tot";
    break;
  case 180:
    name="Unused_Rsvd_Blk_Cnt_Tot";
    break;
  case 183:
    name="Runtime_Bad_Block";
    break;
  case 187:
    name="Reported_Uncorrect";
    break;
  case 189:
    name="High_Fly_Writes";
    break;
  case 190:
    // Western Digital uses this for temperature.
    // It's identical to Attribute 194 except that it
    // has a failure threshold set to correspond to the
    // max allowed operating temperature of the drive, which 
    // is typically 55C.  So if this attribute has failed
    // in the past, it indicates that the drive temp exceeded
    // 55C sometime in the past.
    name="Airflow_Temperature_Cel";
    break;
  case 191:
    name="G-Sense_Error_Rate";
    break;
  case 192:
    switch (val) {
    case 1:
      // Fujitsu
      name="Emergency_Retract_Cycle_Ct";
      break;
    default:
      name="Power-Off_Retract_Count";
      break;
    }
    break;
  case 193:
    name="Load_Cycle_Count";
    break;
  case 194:
    switch (val){
    case 1:
      // Samsung SV1204H with RK100-13 firmware
      name="Temperature_Celsius_x10";
      break;
    case 2:
      // for disks with no temperature Attribute
      name="Unknown_Attribute";
      break;
    default:
      name="Temperature_Celsius";
      break;
    }
    break;
  case 195:
    // Fujitsu name="ECC_On_The_Fly_Count";
    name="Hardware_ECC_Recovered";
    break;
  case 196:
    name="Reallocated_Event_Count";
    break;
  case 197:
    switch (val) {
    default:
      name="Current_Pending_Sector";
      break;
    case 1:
      // Not reset after sector reallocation
      name="Total_Pending_Sectors";
      break;
    }
    break;
  case 198:
    switch (val){
    default:
      name="Offline_Uncorrectable";
      break;
    case 1:
      // Not reset after sector reallocation
      name="Total_Offl_Uncorrectabl"/*e*/;
      break;
    case 2:
      // Fujitsu
      name="Off-line_Scan_UNC_Sector_Ct";
      break;
    }
    break;
  case 199:
    name="UDMA_CRC_Error_Count";
    break;
  case 200:
    switch (val) {
    case 1:
      // Fujitsu MHS2020AT
      name="Write_Error_Count";
      break;
    default:
      // Western Digital
      name="Multi_Zone_Error_Rate";
      break;
    }
    break;
  case 201:
    switch (val) {
    case 1:
      // Fujitsu
      name="Detected_TA_Count";
      break;
    default:
      name="Soft_Read_Error_Rate";
      break;
    }
    break;
  case 202:
    // Fujitsu
    name="TA_Increase_Count";
    // Maxtor: Data Address Mark Errors
    break;
  case 203:
    // Fujitsu
    name="Run_Out_Cancel";
    // Maxtor: ECC Errors
    break;
  case 204:
    // Fujitsu
    name="Shock_Count_Write_Opern";
    // Maxtor: Soft ECC Correction
    break;
  case 205:
    // Fujitsu
    name="Shock_Rate_Write_Opern";
    // Maxtor: Thermal Aspirates
    break;
  case 206:
    // Fujitsu
    name="Flying_Height";
    break;
  case 207:
    // Maxtor
    name="Spin_High_Current";
    break;
  case 208:
    // Maxtor
    name="Spin_Buzz";
    break;
  case 209:
    // Maxtor
    name="Offline_Seek_Performnce";
    break;
  case 220:
    switch (val) {
    case 1:
      name="Temperature_Celsius";
      break;
    default:
      name="Disk_Shift";
      break;
    }
    break;
  case 221:
    name="G-Sense_Error_Rate";
    break;
  case 222:
    name="Loaded_Hours";
    break;
  case 223:
    name="Load_Retry_Count";
    break;
  case 224:
    name="Load_Friction";
    break;
  case 225:
    name="Load_Cycle_Count";
    break;
  case 226:
    name="Load-in_Time";
    break;
  case 227:
    name="Torq-amp_Count";
    break;
  case 228:
    name="Power-off_Retract_Count";
    break;
  case 230:
    // seen in IBM DTPA-353750
    name="Head_Amplitude";
    break;
  case 231:
    name="Temperature_Celsius";
    break;
  case 240:
    name="Head_Flying_Hours";
    break;
  case 250:
    name="Read_Error_Retry_Rate";
    break;
  default:
    name="Unknown_Attribute";
    break;
  }
  sprintf(out,"%3hu %s",(short int)id,name);
  return;
}

// Returns raw value of Attribute with ID==id. This will be in the
// range 0 to 2^48-1 inclusive.  If the Attribute does not exist,
// return -1.
int64_t ATAReturnAttributeRawValue(unsigned char id, const ata_smart_values * data)
{
  // valid Attribute IDs are in the range 1 to 255 inclusive.
  if (!id || !data)
    return -1;
  
  // loop over Attributes to see if there is one with the desired ID
  for (int i = 0; i < NUMBER_ATA_SMART_ATTRIBUTES; i++) {
    const ata_smart_attribute * ap = data->vendor_attributes + i;
    if (ap->id == id) {
      // we've found the desired Attribute.  Return its value
      int64_t rawvalue=0;
      int j;

      for (j=0; j<6; j++) {
	// This looks a bit roundabout, but is necessary.  Don't
	// succumb to the temptation to use raw[j]<<(8*j) since under
	// the normal rules this will be promoted to the native type.
	// On a 32 bit machine this might then overflow.
	int64_t temp;
	temp = ap->raw[j];
	temp <<= 8*j;
	rawvalue |= temp;
      } // loop over j
      return rawvalue;
    } // found desired Attribute
  } // loop over Attributes
  
  // fall-through: no such Attribute found
  return -1;
}

// Return Temperature Attribute raw value selected according to possible
// non-default interpretations. If the Attribute does not exist, return 0
unsigned char ATAReturnTemperatureValue(const ata_smart_values * data, const unsigned char * defs)
{
  for (int i = 0; i < 3; i++) {
    static const unsigned char ids[3] = {194, 9, 220};
    unsigned char id = ids[i];
    unsigned char select = (defs ? defs[id] : 0);
    int64_t raw; unsigned temp;
    if (!(   (id == 194 && select <= 1)   // ! -v 194,unknown
          || (id == 9 && select == 2)     // -v 9,temp
          || (id == 220 && select == 1))) // -v 220,temp
      continue;
    raw = ATAReturnAttributeRawValue(id, data);
    if (raw < 0)
      continue;
    temp = (unsigned short)raw; // ignore possible min/max values in high words
    if (id == 194 && select == 1) // -v 194,10xCelsius
      temp = (temp+5) / 10;
    if (!(0 < temp && temp <= 255))
      continue;
    return temp;
  }
  // No valid attribute found
  return 0;
}


// Read SCT Status
int ataReadSCTStatus(ata_device * device, ata_sct_status_response * sts)
{
  // read SCT status via SMART log 0xe0
  memset(sts, 0, sizeof(*sts));
  if (smartcommandhandler(device, READ_LOG, 0xe0, (char *)sts)){
    syserror("Error Read SCT Status failed");
    return -1;
  }

  // swap endian order if needed
  if (isbigendian()){
    swapx(&sts->format_version);
    swapx(&sts->sct_version);
    swapx(&sts->sct_spec);
    swapx(&sts->ext_status_code);
    swapx(&sts->action_code);
    swapx(&sts->function_code);
    swapx(&sts->over_limit_count);
    swapx(&sts->under_limit_count);
  }

  // Check format version
  if (!(sts->format_version == 2 || sts->format_version == 3)) {
    pout("Error unknown SCT Status format version %u, should be 2 or 3.\n", sts->format_version);
    return -1;
  }
  return 0;
}

// Read SCT Temperature History Table and Status
int ataReadSCTTempHist(ata_device * device, ata_sct_temperature_history_table * tmh,
                       ata_sct_status_response * sts)
{
  // Check initial status
  if (ataReadSCTStatus(device, sts))
    return -1;

  // Do nothing if other SCT command is executing
  if (sts->ext_status_code == 0xffff) {
    pout("Another SCT command is executing, abort Read Data Table\n"
         "(SCT ext_status_code 0x%04x, action_code=%u, function_code=%u)\n",
      sts->ext_status_code, sts->action_code, sts->function_code);
    return -1;
  }

  ata_sct_data_table_command cmd; memset(&cmd, 0, sizeof(cmd));
  // CAUTION: DO NOT CHANGE THIS VALUE (SOME ACTION CODES MAY ERASE DISK)
  cmd.action_code   = 5; // Data table command
  cmd.function_code = 1; // Read table
  cmd.table_id      = 2; // Temperature History Table

  // write command via SMART log page 0xe0
  if (smartcommandhandler(device, WRITE_LOG, 0xe0, (char *)&cmd)){
    syserror("Error Write SCT Data Table command failed");
    return -1;
  }

  // read SCT data via SMART log page 0xe1
  memset(tmh, 0, sizeof(*tmh));
  if (smartcommandhandler(device, READ_LOG, 0xe1, (char *)tmh)){
    syserror("Error Read SCT Data Table failed");
    return -1;
  }

  // re-read and check SCT status
  if (ataReadSCTStatus(device, sts))
    return -1;

  if (!(sts->ext_status_code == 0 && sts->action_code == 5 && sts->function_code == 1)) {
    pout("Error unexcepted SCT status 0x%04x (action_code=%u, function_code=%u)\n",
      sts->ext_status_code, sts->action_code, sts->function_code);
    return -1;
  }

  // swap endian order if needed
  if (isbigendian()){
    swapx(&tmh->format_version);
    swapx(&tmh->sampling_period);
    swapx(&tmh->interval);
  }

  // Check format version
  if (tmh->format_version != 2) {
    pout("Error unknown SCT Temperature History Format Version (%u), should be 2.\n", tmh->format_version);
    return -1;
  }
  return 0;
}

// Set SCT Temperature Logging Interval
int ataSetSCTTempInterval(ata_device * device, unsigned interval, bool persistent)
{
  // Check initial status
  ata_sct_status_response sts;
  if (ataReadSCTStatus(device, &sts))
    return -1;

  // Do nothing if other SCT command is executing
  if (sts.ext_status_code == 0xffff) {
    pout("Another SCT command is executing, abort Feature Control\n"
         "(SCT ext_status_code 0x%04x, action_code=%u, function_code=%u)\n",
      sts.ext_status_code, sts.action_code, sts.function_code);
    return -1;
  }

  ata_sct_feature_control_command cmd; memset(&cmd, 0, sizeof(cmd));
  // CAUTION: DO NOT CHANGE THIS VALUE (SOME ACTION CODES MAY ERASE DISK)
  cmd.action_code   = 4; // Feature Control command
  cmd.function_code = 1; // Set state
  cmd.feature_code  = 3; // Temperature logging interval
  cmd.state         = interval;
  cmd.option_flags  = (persistent ? 0x01 : 0x00);

  // write command via SMART log page 0xe0
  if (smartcommandhandler(device, WRITE_LOG, 0xe0, (char *)&cmd)){
    syserror("Error Write SCT Feature Control Command failed");
    return -1;
  }

  // re-read and check SCT status
  if (ataReadSCTStatus(device, &sts))
    return -1;

  if (!(sts.ext_status_code == 0 && sts.action_code == 4 && sts.function_code == 1)) {
    pout("Error unexcepted SCT status 0x%04x (action_code=%u, function_code=%u)\n",
      sts.ext_status_code, sts.action_code, sts.function_code);
    return -1;
  }
  return 0;
}

// Print one self-test log entry.
// Returns true if self-test showed an error.
bool ataPrintSmartSelfTestEntry(unsigned testnum, unsigned char test_type,
                                unsigned char test_status,
                                unsigned short timestamp,
                                uint64_t failing_lba,
                                bool print_error_only, bool & print_header)
{
  const char * msgtest;
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
        msgtest = "Vendor offline";
      else
        msgtest = "Reserved offline";
  }

  bool is_error = false;
  const char * msgstat;
  switch (test_status >> 4) {
    case 0x0: msgstat = "Completed without error";       break;
    case 0x1: msgstat = "Aborted by host";               break;
    case 0x2: msgstat = "Interrupted (host reset)";      break;
    case 0x3: msgstat = "Fatal or unknown error";        is_error = true; break;
    case 0x4: msgstat = "Completed: unknown failure";    is_error = true; break;
    case 0x5: msgstat = "Completed: electrical failure"; is_error = true; break;
    case 0x6: msgstat = "Completed: servo/seek failure"; is_error = true; break;
    case 0x7: msgstat = "Completed: read failure";       is_error = true; break;
    case 0x8: msgstat = "Completed: handling damage??";  is_error = true; break;
    case 0xf: msgstat = "Self-test routine in progress"; break;
    default:  msgstat = "Unknown/reserved test status";
  }

  if (!is_error && print_error_only)
    return false;

  // Print header once
  if (print_header) {
    print_header = false;
    pout("Num  Test_Description    Status                  Remaining  LifeTime(hours)  LBA_of_first_error\n");
  }

  char msglba[32];
  if (is_error && failing_lba < 0xffffffffffffULL)
    snprintf(msglba, sizeof(msglba), "%"PRIu64, failing_lba);
  else
    strcpy(msglba, "-");

  pout("#%2u  %-19s %-29s %1d0%%  %8u         %s\n", testnum, msgtest, msgstat,
       test_status & 0x0f, timestamp, msglba);

  return is_error;
}

// Print Smart self-test log, used by smartctl and smartd.
// return value is:
// bottom 8 bits: number of entries found where self-test showed an error
// remaining bits: if nonzero, power on hours of last self-test where error was found
int ataPrintSmartSelfTestlog(const ata_smart_selftestlog * data, bool allentries,
                             unsigned char fix_firmwarebug)
{
  if (allentries)
    pout("SMART Self-test log structure revision number %d\n",(int)data->revnumber);
  if ((data->revnumber!=0x0001) && allentries && fix_firmwarebug != FIX_SAMSUNG)
    pout("Warning: ATA Specification requires self-test log structure revision number = 1\n");
  if (data->mostrecenttest==0){
    if (allentries)
      pout("No self-tests have been logged.  [To run self-tests, use: smartctl -t]\n\n");
    return 0;
  }

  bool noheaderprinted = true;
  int retval=0, hours=0, testno=0;

  // print log
  for (int i = 20; i >= 0; i--) {
    // log is a circular buffer
    int j = (i+data->mostrecenttest)%21;
    const ata_smart_selftestlog_struct * log = data->selftest_struct+j;

    if (nonempty(log, sizeof(*log))) {
      // count entry based on non-empty structures -- needed for
      // Seagate only -- other vendors don't have blank entries 'in
      // the middle'
      testno++;

      // T13/1321D revision 1c: (Data structure Rev #1)

      //The failing LBA shall be the LBA of the uncorrectable sector
      //that caused the test to fail. If the device encountered more
      //than one uncorrectable sector during the test, this field
      //shall indicate the LBA of the first uncorrectable sector
      //encountered. If the test passed or the test failed for some
      //reason other than an uncorrectable sector, the value of this
      //field is undefined.

      // This is true in ALL ATA-5 specs
      uint64_t lba48 = (log->lbafirstfailure < 0xffffffff ? log->lbafirstfailure : 0xffffffffffffULL);

      // Print entry
      bool errorfound = ataPrintSmartSelfTestEntry(testno,
        log->selftestnumber, log->selfteststatus, log->timestamp,
        lba48, !allentries, noheaderprinted);

      // keep track of time of most recent error
      if (errorfound && !hours)
        hours=log->timestamp;
    }
  }
  if (!allentries && retval)
    pout("\n");

  hours = hours << 8;
  return (retval | hours);
}


/////////////////////////////////////////////////////////////////////////////
// Pseudo-device to parse "smartctl -r ataioctl,2 ..." output and simulate
// an ATA device with same behaviour

namespace {

class parsed_ata_device
: public /*implements*/ ata_device_with_command_set
{
public:
  parsed_ata_device(smart_interface * intf, const char * dev_name);

  virtual ~parsed_ata_device() throw();

  virtual bool is_open() const;

  virtual bool open();

  virtual bool close();

  virtual bool ata_identify_is_cached() const;

protected:
  virtual int ata_command_interface(smart_command_set command, int select, char * data);

private:
  // Table of parsed commands, return value, data
  struct parsed_ata_command
  {
    smart_command_set command;
    int select;
    int retval, errval;
    char * data;
  };

  enum { max_num_commands = 32 };
  parsed_ata_command m_command_table[max_num_commands];

  int m_num_commands;
  int m_next_replay_command;
  bool m_replay_out_of_sync;
  bool m_ata_identify_is_cached;
};

static const char * nextline(const char * s, int & lineno)
{
  for (s += strcspn(s, "\r\n"); *s == '\r' || *s == '\n'; s++) {
    if (*s == '\r' && s[1] == '\n')
      s++;
    lineno++;
  }
  return s;
}

static int name2command(const char * s)
{
  for (int i = 0; i < (int)(sizeof(commandstrings)/sizeof(commandstrings[0])); i++) {
    if (!strcmp(s, commandstrings[i]))
      return i;
  }
  return -1;
}

static bool matchcpy(char * dest, size_t size, const char * src, const regmatch_t & srcmatch)
{
  if (srcmatch.rm_so < 0)
    return false;
  size_t n = srcmatch.rm_eo - srcmatch.rm_so;
  if (n >= size)
    n = size-1;
  memcpy(dest, src + srcmatch.rm_so, n);
  dest[n] = 0;
  return true;
}

static inline int matchtoi(const char * src, const regmatch_t & srcmatch, int defval)
{
  if (srcmatch.rm_so < 0)
    return defval;
  return atoi(src + srcmatch.rm_so);
}

parsed_ata_device::parsed_ata_device(smart_interface * intf, const char * dev_name)
: smart_device(intf, dev_name, "ata", ""),
  m_num_commands(0),
  m_next_replay_command(0),
  m_replay_out_of_sync(false),
  m_ata_identify_is_cached(false)
{
  memset(m_command_table, 0, sizeof(m_command_table));
}

parsed_ata_device::~parsed_ata_device() throw()
{
  close();
}

bool parsed_ata_device::is_open() const
{
  return (m_num_commands > 0);
}

// Parse stdin and build command table
bool parsed_ata_device::open()
{
  const char * pathname = get_dev_name();
  if (strcmp(pathname, "-"))
    return set_err(EINVAL);
  pathname = "<stdin>";
  // Fill buffer
  char buffer[64*1024];
  int size = 0;
  while (size < (int)sizeof(buffer)) {
    int nr = fread(buffer, 1, sizeof(buffer), stdin);
    if (nr <= 0)
      break;
    size += nr;
  }
  if (size <= 0)
    return set_err(ENOENT, "%s: Unexpected EOF", pathname);
  if (size >= (int)sizeof(buffer))
    return set_err(EIO, "%s: Buffer overflow", pathname);
  buffer[size] = 0;

  // Regex to match output from "-r ataioctl,2"
  static const char pattern[] = "^"
  "(" // (1
    "REPORT-IOCTL: DeviceF?D?=[^ ]+ Command=([A-Z ]*[A-Z])" // (2)
    "(" // (3
      "( InputParameter=([0-9]+))?" // (4 (5))
    "|"
      "( returned (-?[0-9]+)( errno=([0-9]+)[^\r\n]*)?)" // (6 (7) (8 (9)))
    ")" // )
    "[\r\n]" // EOL match necessary to match optional parts above
  "|"
    "===== \\[([A-Z ]*[A-Z])\\] DATA START " // (10)
  "|"
    "    *(En|Dis)abled status cached by OS, " // (11)
  ")"; // )

  // Compile regex
  regular_expression regex;
  if (!regex.compile(pattern, REG_EXTENDED))
    return set_err(EIO, "invalid regex");

  // Parse buffer
  const char * errmsg = 0;
  int i = -1, state = 0, lineno = 1;
  for (const char * line = buffer; *line; line = nextline(line, lineno)) {
    // Match line
    if (!(line[0] == 'R' || line[0] == '=' || line[0] == ' '))
      continue;
    const int nmatch = 1+11;
    regmatch_t match[nmatch];
    if (!regex.execute(line, nmatch, match))
      continue;

    char cmdname[40];
    if (matchcpy(cmdname, sizeof(cmdname), line, match[2])) { // "REPORT-IOCTL:... Command=%s ..."
      int nc = name2command(cmdname);
      if (nc < 0) {
        errmsg = "Unknown ATA command name"; break;
      }
      if (match[7].rm_so < 0) { // "returned %d"
        // Start of command
        if (!(state == 0 || state == 2)) {
          errmsg = "Missing REPORT-IOCTL result"; break;
        }
        if (++i >= max_num_commands) {
          errmsg = "Too many ATA commands"; break;
        }
        m_command_table[i].command = (smart_command_set)nc;
        m_command_table[i].select = matchtoi(line, match[5], 0); // "InputParameter=%d"
        state = 1;
      }
      else {
        // End of command
        if (!(state == 1 && (int)m_command_table[i].command == nc)) {
          errmsg = "Missing REPORT-IOCTL start"; break;
        }
        m_command_table[i].retval = matchtoi(line, match[7], -1); // "returned %d"
        m_command_table[i].errval = matchtoi(line, match[9], 0); // "errno=%d"
        state = 2;
      }
    }
    else if (matchcpy(cmdname, sizeof(cmdname), line, match[10])) { // "===== [%s] DATA START "
      // Start of sector hexdump
      int nc = name2command(cmdname);
      if (!(state == (nc == WRITE_LOG ? 1 : 2) && (int)m_command_table[i].command == nc)) {
          errmsg = "Unexpected DATA START"; break;
      }
      line = nextline(line, lineno);
      char * data = (char *)malloc(512);
      unsigned j;
      for (j = 0; j < 32; j++) {
        unsigned b[16];
        unsigned u1, u2; int n1 = -1;
        if (!(sscanf(line, "%3u-%3u: "
                        "%2x %2x %2x %2x %2x %2x %2x %2x "
                        "%2x %2x %2x %2x %2x %2x %2x %2x%n",
                     &u1, &u2,
                     b+ 0, b+ 1, b+ 2, b+ 3, b+ 4, b+ 5, b+ 6, b+ 7,
                     b+ 8, b+ 9, b+10, b+11, b+12, b+13, b+14, b+15, &n1) == 18
              && n1 >= 56 && u1 == j*16 && u2 == j*16+15))
          break;
        for (unsigned k = 0; k < 16; k++)
          data[j*16+k] = b[k];
        line = nextline(line, lineno);
      }
      if (j < 32) {
        free(data);
        errmsg = "Incomplete sector hex dump"; break;
      }
      m_command_table[i].data = data;
      if (nc != WRITE_LOG)
        state = 0;
    }
    else if (match[11].rm_so > 0) { // "(En|Dis)abled status cached by OS"
      m_ata_identify_is_cached = true;
    }
  }

  if (!(state == 0 || state == 2))
    errmsg = "Missing REPORT-IOCTL result";

  if (!errmsg && i < 0)
    errmsg = "No information found";

  m_num_commands = i+1;
  m_next_replay_command = 0;
  m_replay_out_of_sync = false;

  if (errmsg) {
    close();
    return set_err(EIO, "%s(%d): Syntax error: %s", pathname, lineno, errmsg);
  }
  return true;
}

// Report warnings and free command table 
bool parsed_ata_device::close()
{
  if (m_replay_out_of_sync)
      pout("REPLAY-IOCTL: Warning: commands replayed out of sync\n");
  else if (m_next_replay_command != 0)
      pout("REPLAY-IOCTL: Warning: %d command(s) not replayed\n", m_num_commands-m_next_replay_command);

  for (int i = 0; i < m_num_commands; i++) {
    if (m_command_table[i].data) {
      free(m_command_table[i].data); m_command_table[i].data = 0;
    }
  }
  m_num_commands = 0;
  m_next_replay_command = 0;
  m_replay_out_of_sync = false;
  return true;
}


bool parsed_ata_device::ata_identify_is_cached() const
{
  return m_ata_identify_is_cached;
}


// Simulate ATA command from command table
int parsed_ata_device::ata_command_interface(smart_command_set command, int select, char * data)
{
  // Find command, try round-robin if out of sync
  int i = m_next_replay_command;
  for (int j = 0; ; j++) {
    if (j >= m_num_commands) {
      pout("REPLAY-IOCTL: Warning: Command not found\n");
      errno = ENOSYS;
      return -1;
    }
    if (m_command_table[i].command == command && m_command_table[i].select == select)
      break;
    if (!m_replay_out_of_sync) {
      m_replay_out_of_sync = true;
      pout("REPLAY-IOCTL: Warning: Command #%d is out of sync\n", i+1);
    }
    if (++i >= m_num_commands)
      i = 0;
  }
  m_next_replay_command = i;
  if (++m_next_replay_command >= m_num_commands)
    m_next_replay_command = 0;

  // Return command data
  switch (command) {
    case IDENTIFY:
    case PIDENTIFY:
    case READ_VALUES:
    case READ_THRESHOLDS:
    case READ_LOG:
      if (m_command_table[i].data)
        memcpy(data, m_command_table[i].data, 512);
      break;
    case WRITE_LOG:
      if (!(m_command_table[i].data && !memcmp(data, m_command_table[i].data, 512)))
        pout("REPLAY-IOCTL: Warning: WRITE LOG data does not match\n");
      break;
    case CHECK_POWER_MODE:
      data[0] = (char)0xff;
    default:
      break;
  }

  if (m_command_table[i].errval)
    errno = m_command_table[i].errval;
  return m_command_table[i].retval;
}

} // namespace

ata_device * get_parsed_ata_device(smart_interface * intf, const char * dev_name)
{
  return new parsed_ata_device(intf, dev_name);
}
