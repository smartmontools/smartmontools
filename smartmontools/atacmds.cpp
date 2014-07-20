/*
 * atacmds.cpp
 * 
 * Home page of code is: http://smartmontools.sourceforge.net
 *
 * Copyright (C) 2002-11 Bruce Allen <smartmontools-support@lists.sourceforge.net>
 * Copyright (C) 2008-14 Christian Franke <smartmontools-support@lists.sourceforge.net>
 * Copyright (C) 1999-2000 Michael Cornwell <cornwell@acm.org>
 * Copyright (C) 2000 Andre Hedrick <andre@linux-ide.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * You should have received a copy of the GNU General Public License
 * (for example COPYING); If not, see <http://www.gnu.org/licenses/>.
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
#include "utility.h"
#include "dev_ata_cmd_set.h" // for parsed_ata_device

const char * atacmds_cpp_cvsid = "$Id$"
                                 ATACMDS_H_CVSID;

// Print ATA debug messages?
unsigned char ata_debugmode = 0;

// Suppress serial number?
// (also used in scsiprint.cpp)
bool dont_print_serial_number = false;


#define SMART_CYL_LOW  0x4F
#define SMART_CYL_HI   0xC2

// SMART RETURN STATUS yields SMART_CYL_HI,SMART_CYL_LOW to indicate drive
// is healthy and SRET_STATUS_HI_EXCEEDED,SRET_STATUS_MID_EXCEEDED to
// indicate that a threshhold exceeded condition has been detected.
// Those values (byte pairs) are placed in ATA register "LBA 23:8".
#define SRET_STATUS_HI_EXCEEDED 0x2C
#define SRET_STATUS_MID_EXCEEDED 0xF4


// Get ID and increase flag of current pending or offline
// uncorrectable attribute.
unsigned char get_unc_attr_id(bool offline, const ata_vendor_attr_defs & defs,
                              bool & increase)
{
  unsigned char id = (!offline ? 197 : 198);
  const ata_vendor_attr_defs::entry & def = defs[id];
  if (def.flags & ATTRFLAG_INCREASING)
    increase = true; // '-v 19[78],increasing' option
  else if (def.name.empty() || (id == 198 && def.name == "Offline_Scan_UNC_SectCt"))
    increase = false; // no or '-v 198,offlinescanuncsectorct' option
  else
    id = 0; // other '-v 19[78],...' option
  return id;
}

#if 0 // TODO: never used
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
#endif


// Table of raw print format names
struct format_name_entry
{
  const char * name;
  ata_attr_raw_format format;
};

const format_name_entry format_names[] = {
  {"raw8"           , RAWFMT_RAW8},
  {"raw16"          , RAWFMT_RAW16},
  {"raw48"          , RAWFMT_RAW48},
  {"hex48"          , RAWFMT_HEX48},
  {"raw56"          , RAWFMT_RAW56},
  {"hex56"          , RAWFMT_HEX56},
  {"raw64"          , RAWFMT_RAW64},
  {"hex64"          , RAWFMT_HEX64},
  {"raw16(raw16)"   , RAWFMT_RAW16_OPT_RAW16},
  {"raw16(avg16)"   , RAWFMT_RAW16_OPT_AVG16},
  {"raw24(raw8)"    , RAWFMT_RAW24_OPT_RAW8},
  {"raw24/raw24"    , RAWFMT_RAW24_DIV_RAW24},
  {"raw24/raw32"    , RAWFMT_RAW24_DIV_RAW32},
  {"sec2hour"       , RAWFMT_SEC2HOUR},
  {"min2hour"       , RAWFMT_MIN2HOUR},
  {"halfmin2hour"   , RAWFMT_HALFMIN2HOUR},
  {"msec24hour32"   , RAWFMT_MSEC24_HOUR32},
  {"tempminmax"     , RAWFMT_TEMPMINMAX},
  {"temp10x"        , RAWFMT_TEMP10X},
};

const unsigned num_format_names = sizeof(format_names)/sizeof(format_names[0]);

// Table to map old to new '-v' option arguments
const char * const map_old_vendor_opts[][2] = {
  {  "9,halfminutes"              , "9,halfmin2hour,Power_On_Half_Minutes"},
  {  "9,minutes"                  , "9,min2hour,Power_On_Minutes"},
  {  "9,seconds"                  , "9,sec2hour,Power_On_Seconds"},
  {  "9,temp"                     , "9,tempminmax,Temperature_Celsius"},
  {"192,emergencyretractcyclect"  , "192,raw48,Emerg_Retract_Cycle_Ct"},
  {"193,loadunload"               , "193,raw24/raw24"},
  {"194,10xCelsius"               , "194,temp10x,Temperature_Celsius_x10"},
  {"194,unknown"                  , "194,raw48,Unknown_Attribute"},
  {"197,increasing"               , "197,raw48+,Total_Pending_Sectors"}, // '+' sets flag
  {"198,offlinescanuncsectorct"   , "198,raw48,Offline_Scan_UNC_SectCt"}, // see also get_unc_attr_id() above
  {"198,increasing"               , "198,raw48+,Total_Offl_Uncorrectabl"}, // '+' sets flag
  {"200,writeerrorcount"          , "200,raw48,Write_Error_Count"},
  {"201,detectedtacount"          , "201,raw48,Detected_TA_Count"},
  {"220,temp"                     , "220,tempminmax,Temperature_Celsius"},
};

const unsigned num_old_vendor_opts = sizeof(map_old_vendor_opts)/sizeof(map_old_vendor_opts[0]);

// Parse vendor attribute display def (-v option).
// Return false on error.
bool parse_attribute_def(const char * opt, ata_vendor_attr_defs & defs,
                         ata_vendor_def_prior priority)
{
  // Map old -> new options
  unsigned i;
  for (i = 0; i < num_old_vendor_opts; i++) {
    if (!strcmp(opt, map_old_vendor_opts[i][0])) {
      opt = map_old_vendor_opts[i][1];
      break;
    }
  }

  // Parse option
  int len = strlen(opt);
  int id = 0, n1 = -1, n2 = -1;
  char fmtname[32+1], attrname[32+1];
  if (opt[0] == 'N') {
    // "N,format[,name]"
    if (!(   sscanf(opt, "N,%32[^,]%n,%32[^,]%n", fmtname, &n1, attrname, &n2) >= 1
          && (n1 == len || n2 == len)))
      return false;
  }
  else {
    // "id,format[+][,name]"
    if (!(   sscanf(opt, "%d,%32[^,]%n,%32[^,]%n", &id, fmtname, &n1, attrname, &n2) >= 2
          && 1 <= id && id <= 255 && (n1 == len || n2 == len)))
      return false;
  }
  if (n1 == len)
    attrname[0] = 0;

  unsigned flags = 0;
  // For "-v 19[78],increasing" above
  if (fmtname[strlen(fmtname)-1] == '+') {
    fmtname[strlen(fmtname)-1] = 0;
    flags = ATTRFLAG_INCREASING;
  }

  // Split "format[:byteorder]"
  char byteorder[8+1] = "";
  if (strchr(fmtname, ':')) {
    n1 = n2 = -1;
    if (!(   sscanf(fmtname, "%*[^:]%n:%8[012345rvwz]%n", &n1, byteorder, &n2) >= 1
          && n2 == (int)strlen(fmtname)))
      return false;
    fmtname[n1] = 0;
    if (strchr(byteorder, 'v'))
      flags |= (ATTRFLAG_NO_NORMVAL|ATTRFLAG_NO_WORSTVAL);
    if (strchr(byteorder, 'w'))
      flags |= ATTRFLAG_NO_WORSTVAL;
  }

  // Find format name
  for (i = 0; ; i++) {
    if (i >= num_format_names)
      return false; // Not found
    if (!strcmp(fmtname, format_names[i].name))
      break;
  }
  ata_attr_raw_format format = format_names[i].format;

  // 64-bit formats use the normalized and worst value bytes.
  if (!*byteorder && (format == RAWFMT_RAW64 || format == RAWFMT_HEX64))
    flags |= (ATTRFLAG_NO_NORMVAL|ATTRFLAG_NO_WORSTVAL);

  if (!id) {
    // "N,format" -> set format for all entries
    for (i = 0; i < MAX_ATTRIBUTE_NUM; i++) {
      if (defs[i].priority >= priority)
        continue;
      if (attrname[0])
        defs[i].name = attrname;
      defs[i].priority = priority;
      defs[i].raw_format = format;
      defs[i].flags = flags;
      snprintf(defs[i].byteorder, sizeof(defs[i].byteorder), "%s", byteorder);
    }
  }
  else if (defs[id].priority <= priority) {
    // "id,format[,name]"
    if (attrname[0])
      defs[id].name = attrname;
    defs[id].raw_format = format;
    defs[id].priority = priority;
    defs[id].flags = flags;
    snprintf(defs[id].byteorder, sizeof(defs[id].byteorder), "%s", byteorder);
  }

  return true;
}


// Return a multiline string containing a list of valid arguments for
// parse_attribute_def().  The strings are preceeded by tabs and followed
// (except for the last) by newlines.
std::string create_vendor_attribute_arg_list()
{
  std::string s;
  unsigned i;
  for (i = 0; i < num_format_names; i++)
    s += strprintf("%s\tN,%s[:012345rvwz][,ATTR_NAME]",
      (i>0 ? "\n" : ""), format_names[i].name);
  for (i = 0; i < num_old_vendor_opts; i++)
    s += strprintf("\n\t%s", map_old_vendor_opts[i][0]);
  return s;
}


// Parse firmwarebug def (-F option).
// Return false on error.
bool parse_firmwarebug_def(const char * opt, firmwarebug_defs & firmwarebugs)
{
    if (!strcmp(opt, "none"))
      firmwarebugs.set(BUG_NONE);
    else if (!strcmp(opt, "nologdir"))
      firmwarebugs.set(BUG_NOLOGDIR);
    else if (!strcmp(opt, "samsung"))
      firmwarebugs.set(BUG_SAMSUNG);
    else if (!strcmp(opt, "samsung2"))
      firmwarebugs.set(BUG_SAMSUNG2);
    else if (!strcmp(opt, "samsung3"))
      firmwarebugs.set(BUG_SAMSUNG3);
    else if (!strcmp(opt, "xerrorlba"))
      firmwarebugs.set(BUG_XERRORLBA);
    else
      return false;
    return true;
}

// Return a string of valid argument words for parse_firmwarebug_def()
const char * get_valid_firmwarebug_args()
{
  return "none, nologdir, samsung, samsung2, samsung3, xerrorlba";
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

// Invalidate serial number and WWN and adjust checksum in IDENTIFY data
static void invalidate_serno(ata_identify_device * id)
{
  unsigned char sum = 0;
  unsigned i;
  for (i = 0; i < sizeof(id->serial_no); i++) {
    sum += id->serial_no[i]; sum -= id->serial_no[i] = 'X';
  }
  unsigned char * b = (unsigned char *)id;
  for (i = 2*108; i < 2*112; i++) { // words108-111: WWN
    sum += b[i]; sum -= b[i] = 0x00;
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


static const char * preg(const ata_register & r, char (& buf)[8])
{
  if (!r.is_set())
    //return "n/a ";
    return "....";
  snprintf(buf, sizeof(buf), "0x%02x", r.val());
  return buf;
}

static void print_regs(const char * prefix, const ata_in_regs & r, const char * suffix = "\n")
{
  char bufs[7][8];
  pout("%s FR=%s, SC=%s, LL=%s, LM=%s, LH=%s, DEV=%s, CMD=%s%s", prefix,
    preg(r.features, bufs[0]), preg(r.sector_count, bufs[1]), preg(r.lba_low, bufs[2]),
    preg(r.lba_mid, bufs[3]), preg(r.lba_high, bufs[4]), preg(r.device, bufs[5]),
    preg(r.command, bufs[6]), suffix);
}

static void print_regs(const char * prefix, const ata_out_regs & r, const char * suffix = "\n")
{
  char bufs[7][8];
  pout("%sERR=%s, SC=%s, LL=%s, LM=%s, LH=%s, DEV=%s, STS=%s%s", prefix,
    preg(r.error, bufs[0]), preg(r.sector_count, bufs[1]), preg(r.lba_low, bufs[2]),
    preg(r.lba_mid, bufs[3]), preg(r.lba_high, bufs[4]), preg(r.device, bufs[5]),
    preg(r.status, bufs[6]), suffix);
}

static void prettyprint(const unsigned char *p, const char *name){
  pout("\n===== [%s] DATA START (BASE-16) =====\n", name);
  for (int i=0; i<512; i+=16, p+=16)
#define P(n) (' ' <= p[n] && p[n] <= '~' ? (int)p[n] : '.')
    // print complete line to avoid slow tty output and extra lines in syslog.
    pout("%03d-%03d: %02x %02x %02x %02x %02x %02x %02x %02x "
                    "%02x %02x %02x %02x %02x %02x %02x %02x"
                    " |%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c|"
         "%c",
         i, i+16-1,
         p[ 0], p[ 1], p[ 2], p[ 3], p[ 4], p[ 5], p[ 6], p[ 7],
         p[ 8], p[ 9], p[10], p[11], p[12], p[13], p[14], p[15], 
         P( 0), P( 1), P( 2), P( 3), P( 4), P( 5), P( 6), P( 7),
         P( 8), P( 9), P(10), P(11), P(12), P(13), P(14), P(15),
         '\n');
#undef P
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
  if (ata_debugmode) {
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
  if (ata_debugmode > 1 && sendsdata)
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
        return -1;
    }

    if (ata_debugmode)
      print_regs(" Input:  ", in.in_regs,
        (in.direction==ata_cmd_in::data_in ? " IN\n":
         in.direction==ata_cmd_in::data_out ? " OUT\n":"\n"));

    ata_cmd_out out;

    int64_t start_usec = -1;
    if (ata_debugmode)
      start_usec = smi()->get_timer_usec();

    bool ok = device->ata_pass_through(in, out);

    if (start_usec >= 0) {
      int64_t duration_usec = smi()->get_timer_usec() - start_usec;
      if (duration_usec >= 500)
        pout(" [Duration: %.3fs]\n", duration_usec / 1000000.0);
    }

    if (ata_debugmode && out.out_regs.is_set())
      print_regs(" Output: ", out.out_regs);

    if (ok) switch (command) {
      default:
        retval = 0;
        break;
      case CHECK_POWER_MODE:
        if (out.out_regs.sector_count.is_set()) {
          data[0] = out.out_regs.sector_count;
          retval = 0;
        }
        else {
          pout("CHECK POWER MODE: incomplete response, ATA output registers missing\n");
          device->set_err(ENOSYS);
          retval = -1;
        }
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
          if (ata_debugmode)
            pout("SMART STATUS RETURN: half healthy response sequence, "
                 "probable SAT/USB truncation\n");
          } else if (out.out_regs.lba_mid == SRET_STATUS_MID_EXCEEDED) {
          retval = 1;
          if (ata_debugmode)
            pout("SMART STATUS RETURN: half unhealthy response sequence, "
                 "probable SAT/USB truncation\n");
        }
        else if (!out.out_regs.is_set()) {
          device->set_err(ENOSYS, "Incomplete response, ATA output registers missing");
          retval = -1;
        }
        else {
          // We haven't gotten output that makes sense; print out some debugging info
          pout("SMART Status command failed\n");
          pout("Please get assistance from %s\n", PACKAGE_HOMEPAGE);
          pout("Register values returned from SMART Status command are:\n");
          print_regs(" ", out.out_regs);
          device->set_err(ENOSYS, "Invalid ATA output register values");
          retval = -1;
        }
        break;
    }
  }

  // If requested, invalidate serial number before any printing is done
  if ((command == IDENTIFY || command == PIDENTIFY) && !retval && dont_print_serial_number)
    invalidate_serno((ata_identify_device *)data);

  // If reporting is enabled, say what output was produced by the command
  if (ata_debugmode) {
    if (device->get_errno())
      pout("REPORT-IOCTL: Device=%s Command=%s returned %d errno=%d [%s]\n",
           device->get_dev_name(), commandstrings[command], retval,
           device->get_errno(), device->get_errmsg());
    else
      pout("REPORT-IOCTL: Device=%s Command=%s returned %d\n",
           device->get_dev_name(), commandstrings[command], retval);
    
    // if requested, pretty-print the output data structure
    if (ata_debugmode > 1 && getsdata) {
      if (command==CHECK_POWER_MODE)
	pout("Sector Count Register (BASE-16): %02x\n", (unsigned char)(*data));
      else
	prettyprint((unsigned char *)data, commandstrings[command]);
    }
  }

  return retval;
}

// Get capacity and sector sizes from IDENTIFY data
void ata_get_size_info(const ata_identify_device * id, ata_size_info & sizes)
{
  sizes.sectors = sizes.capacity = 0;
  sizes.log_sector_size = sizes.phy_sector_size = 0;
  sizes.log_sector_offset = 0;

  // Return if no LBA support
  if (!(id->words047_079[49-47] & 0x0200))
    return;

  // Determine 28-bit LBA capacity
  unsigned lba28 = (unsigned)id->words047_079[61-47] << 16
                 | (unsigned)id->words047_079[60-47]      ;

  // Determine 48-bit LBA capacity if supported
  uint64_t lba48 = 0;
  if ((id->command_set_2 & 0xc400) == 0x4400)
    lba48 = (uint64_t)id->words088_255[103-88] << 48
          | (uint64_t)id->words088_255[102-88] << 32
          | (uint64_t)id->words088_255[101-88] << 16
          | (uint64_t)id->words088_255[100-88]      ;

  // Return if capacity unknown (ATAPI CD/DVD)
  if (!(lba28 || lba48))
    return;

  // Determine sector sizes
  sizes.log_sector_size = sizes.phy_sector_size = 512;

  unsigned short word106 = id->words088_255[106-88];
  if ((word106 & 0xc000) == 0x4000) {
    // Long Logical/Physical Sectors (LLS/LPS) ?
    if (word106 & 0x1000)
      // Logical sector size is specified in 16-bit words
      sizes.log_sector_size = sizes.phy_sector_size =
        ((id->words088_255[118-88] << 16) | id->words088_255[117-88]) << 1;

    if (word106 & 0x2000)
      // Physical sector size is multiple of logical sector size
      sizes.phy_sector_size <<= (word106 & 0x0f);

    unsigned short word209 = id->words088_255[209-88];
    if ((word209 & 0xc000) == 0x4000)
      sizes.log_sector_offset = (word209 & 0x3fff) * sizes.log_sector_size;
  }

  // Some early 4KiB LLS disks (Samsung N3U-3) return bogus lba28 value
  if (lba48 >= lba28 || (lba48 && sizes.log_sector_size > 512))
    sizes.sectors = lba48;
  else
    sizes.sectors = lba28;

  sizes.capacity = sizes.sectors * sizes.log_sector_size;
}

// This function computes the checksum of a single disk sector (512
// bytes).  Returns zero if checksum is OK, nonzero if the checksum is
// incorrect.  The size (512) is correct for all SMART structures.
unsigned char checksum(const void * data)
{
  unsigned char sum = 0;
  for (int i = 0; i < 512; i++)
    sum += ((const unsigned char *)data)[i];
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
void ata_format_id_string(char * out, const unsigned char * in, int n)
{
  bool must_swap = true;
#ifdef __NetBSD__
  /* NetBSD kernel delivers IDENTIFY data in host byte order (but all else is LE) */
  // TODO: Handle NetBSD case in os_netbsd.cpp
  if (isbigendian())
    must_swap = !must_swap;
#endif

  char tmp[65];
  n = n > 64 ? 64 : n;
  if (!must_swap)
    strncpy(tmp, (const char *)in, n);
  else
    swapbytes(tmp, (const char *)in, n);
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

// Issue a no-data ATA command with optional sector count register value
bool ata_nodata_command(ata_device * device, unsigned char command,
                        int sector_count /* = -1 */)
{
  ata_cmd_in in;
  in.in_regs.command = command;
  if (sector_count >= 0)
    in.in_regs.sector_count = sector_count;

  return device->ata_pass_through(in);
}

// Issue SET FEATURES command with optional sector count register value
bool ata_set_features(ata_device * device, unsigned char features,
                      int sector_count /* = -1 */)
{
  ata_cmd_in in;
  in.in_regs.command = ATA_SET_FEATURES;
  in.in_regs.features = features;
  if (sector_count >= 0)
    in.in_regs.sector_count = sector_count;

  return device->ata_pass_through(in);
}

// Reads current Device Identity info (512 bytes) into buf.  Returns 0
// if all OK.  Returns -1 if no ATA Device identity can be
// established.  Returns >0 if Device is ATA Packet Device (not SMART
// capable).  The value of the integer helps identify the type of
// Packet device, which is useful so that the user can connect the
// formal device number with whatever object is inside their computer.
int ata_read_identity(ata_device * device, ata_identify_device * buf, bool fix_swapped_id,
                      unsigned char * raw_buf /* = 0 */)
{
  unsigned short *rawshort=(unsigned short *)buf;
  unsigned char  *rawbyte =(unsigned char  *)buf;

  // See if device responds either to IDENTIFY DEVICE or IDENTIFY
  // PACKET DEVICE
  bool packet = false;
  if ((smartcommandhandler(device, IDENTIFY, 0, (char *)buf))){
    if (smartcommandhandler(device, PIDENTIFY, 0, (char *)buf)){
      return -1; 
    }
    packet = true;
  }

  unsigned i;
  if (fix_swapped_id) {
    // Swap ID strings
    for (i = 0; i < sizeof(buf->serial_no)-1; i += 2)
      swap2((char *)(buf->serial_no+i));
    for (i = 0; i < sizeof(buf->fw_rev)-1; i += 2)
      swap2((char *)(buf->fw_rev+i));
    for (i = 0; i < sizeof(buf->model)-1; i += 2)
      swap2((char *)(buf->model+i));
  }

  // If requested, save raw data before endianness adjustments
  if (raw_buf)
    memcpy(raw_buf, buf, sizeof(*buf));

#ifndef __NetBSD__
  // if machine is big-endian, swap byte order as needed
  // NetBSD kernel delivers IDENTIFY data in host byte order
  // TODO: Handle NetBSD case in os_netbsd.cpp
  if (isbigendian()){
    
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

  // AT Attachment 8 - ATA/ATAPI Command Set (ATA8-ACS)
  // T13/1699-D Revision 6a (Final Draft), September 6, 2008.
  // Sections 7.16.7 and 7.17.6:
  //
  // Word 0 of IDENTIFY DEVICE data:
  // Bit 15 = 0 : ATA device
  //
  // Word 0 of IDENTIFY PACKET DEVICE data:
  // Bits 15:14 = 10b : ATAPI device
  // Bits 15:14 = 11b : Reserved
  // Bits 12:8        : Device type (SPC-4, e.g 0x05 = CD/DVD)

  // CF+ and CompactFlash Specification Revision 4.0, May 24, 2006.
  // Section 6.2.1.6:
  //
  // Word 0 of IDENTIFY DEVICE data:
  // 848Ah = Signature for CompactFlash Storage Card
  // 044Ah = Alternate value turns on ATA device while preserving all retired bits
  // 0040h = Alternate value turns on ATA device while zeroing all retired bits

  // Assume ATA if IDENTIFY DEVICE returns CompactFlash Signature
  if (!packet && rawbyte[1] == 0x84 && rawbyte[0] == 0x8a)
    return 0;

  // If this is a PACKET DEVICE, return device type
  if (rawbyte[1] & 0x80)
    return 1+(rawbyte[1] & 0x1f);
  
  // Not a PACKET DEVICE
  return 0;
}

// Get World Wide Name (WWN) fields.
// Return NAA field or -1 if WWN is unsupported.
// Table 34 of T13/1699-D Revision 6a (ATA8-ACS), September 6, 2008.
// (WWN was introduced in ATA/ATAPI-7 and is mandatory since ATA8-ACS Revision 3b)
int ata_get_wwn(const ata_identify_device * id, unsigned & oui, uint64_t & unique_id)
{
  // Don't use word 84 to be compatible with some older ATA-7 disks
  unsigned short word087 = id->csf_default;
  if ((word087 & 0xc100) != 0x4100)
    return -1; // word not valid or WWN support bit 8 not set

  unsigned short word108 = id->words088_255[108-88];
  unsigned short word109 = id->words088_255[109-88];
  unsigned short word110 = id->words088_255[110-88];
  unsigned short word111 = id->words088_255[111-88];

  oui = ((word108 & 0x0fff) << 12) | (word109 >> 4);
  unique_id = ((uint64_t)(word109 & 0xf) << 32)
            | (unsigned)((word110 << 16) | word111);
  return (word108 >> 12);
}

// Get nominal media rotation rate.
// Returns: 0 = not reported, 1 = SSD, >1 = HDD rpm, < 0 = -(Unknown value)
int ata_get_rotation_rate(const ata_identify_device * id)
{
  // Table 37 of T13/1699-D (ATA8-ACS) Revision 6a, September 6, 2008
  // Table A.31 of T13/2161-D (ACS-3) Revision 3b, August 25, 2012
  unsigned short word217 = id->words088_255[217-88];
  if (word217 == 0x0000 || word217 == 0xffff)
    return 0;
  else if (word217 == 0x0001)
    return 1;
  else if (word217 > 0x0400)
    return word217;
  else
    return -(int)word217;
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
    return -1;
  }

  // compute checksum
  if (checksum(data))
    checksumwarning("SMART Attribute Data Structure");
  
  // swap endian order if needed
  if (isbigendian()){
    int i;
    swap2((char *)&(data->revnumber));
    swap2((char *)&(data->total_time_to_complete_off_line));
    swap2((char *)&(data->smart_capability));
    swapx(&data->extend_test_completion_time_w);
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
                        firmwarebug_defs firmwarebugs)
{

  // get data from device
  if (smartcommandhandler(device, READ_LOG, 0x06, (char *)data)){
    return -1;
  }

  // compute its checksum, and issue a warning if needed
  if (checksum(data))
    checksumwarning("SMART Self-Test Log Structure");
  
  // fix firmware bugs in self-test log
  if (firmwarebugs.is_set(BUG_SAMSUNG))
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

// Print checksum warning for multi sector log
static void check_multi_sector_sum(const void * data, unsigned nsectors, const char * msg)
{
  unsigned errs = 0;
  for (unsigned i = 0; i < nsectors; i++) {
    if (checksum((const unsigned char *)data + i*512))
      errs++;
  }
  if (errs > 0) {
    if (nsectors == 1)
      checksumwarning(msg);
    else
      checksumwarning(strprintf("%s (%u/%u)", msg, errs, nsectors).c_str());
  }
}

// Read SMART Extended Self-test Log
bool ataReadExtSelfTestLog(ata_device * device, ata_smart_extselftestlog * log,
                           unsigned nsectors)
{
  if (!ataReadLogExt(device, 0x07, 0x00, 0, log, nsectors))
    return false;

  check_multi_sector_sum(log, nsectors, "SMART Extended Self-test Log Structure");

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
    return -1;
  }
   
  // compute its checksum, and issue a warning if needed
  if (checksum(data))
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
  
  return 0;
}

// Writes the selective self-test log (log #9)
int ataWriteSelectiveSelfTestLog(ata_device * device, ata_selective_selftest_args & args,
                                 const ata_smart_values * sv, uint64_t num_sectors,
                                 const ata_selective_selftest_args * prev_args)
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
    pout("SMART Read Selective Self-test Log failed: %s\n", device->get_errmsg());
    pout("Since Read failed, will not attempt to WRITE Selective Self-test Log\n");
    return -1;
  }
  
  // Set log version
  data->logversion = 1;

  // Host is NOT allowed to write selective self-test log if a selective
  // self-test is in progress.
  if (0<data->currentspan && data->currentspan<6 && ((sv->self_test_exec_status)>>4)==15) {
    pout("SMART Selective or other Self-test in progress\n");
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

    if (   (mode == SEL_REDO || mode == SEL_NEXT)
        && prev_args && i < prev_args->num_spans
        && !data->span[i].start && !data->span[i].end) {
      // Some drives do not preserve the selective self-test log accross
      // power-cyles.  If old span on drive is cleared use span provided
      // by caller.  This is used by smartd (first span only).
      data->span[i].start = prev_args->span[i].start;
      data->span[i].end   = prev_args->span[i].end;
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
            pout("Span %d changed from %" PRIu64 "-%" PRIu64 " (%" PRIu64 " sectors)\n",
                 i, start, end, oldsize);
            pout("                 to %" PRIu64 "-%" PRIu64 " (%" PRIu64 " sectors) (%" PRIu64 " spans)\n",
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
      pout("Invalid selective self-test span %d: %" PRIu64 "-%" PRIu64 " (%" PRIu64 " sectors)\n",
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
    for (int b = 0; b < 5; b++) {
      swap8((char *)&(data->span[b].start));
      swap8((char *)&(data->span[b].end));
    }
    swap8((char *)&(data->currentlba));
    swap2((char *)&(data->currentspan));
    swap2((char *)&(data->flags));
    swap2((char *)&(data->pendingtime));
  }

  // write new selective self-test log
  if (smartcommandhandler(device, WRITE_LOG, 0x09, (char *)data)){
    pout("Write Selective Self-test Log failed: %s\n", device->get_errmsg());
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
                     firmwarebug_defs firmwarebugs)
{
  
  // get data from device
  if (smartcommandhandler(device, READ_LOG, 0x01, (char *)data)){
    return -1;
  }
  
  // compute its checksum, and issue a warning if needed
  if (checksum(data))
    checksumwarning("SMART ATA Error Log Structure");
  
  // Some disks have the byte order reversed in some SMART Summary
  // Error log entries
  if (firmwarebugs.is_set(BUG_SAMSUNG))
    fixsamsungerrorlog(data);
  else if (firmwarebugs.is_set(BUG_SAMSUNG2))
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


// Fix LBA byte ordering of Extended Comprehensive Error Log
// if little endian instead of ATA register ordering is provided
template <class T>
static inline void fix_exterrlog_lba_cmd(T & cmd)
{
  T org = cmd;
  cmd.lba_mid_register_hi = org.lba_high_register;
  cmd.lba_low_register_hi = org.lba_mid_register_hi;
  cmd.lba_high_register   = org.lba_mid_register;
  cmd.lba_mid_register    = org.lba_low_register_hi;
}

static void fix_exterrlog_lba(ata_smart_exterrlog * log, unsigned nsectors)
{
   for (unsigned i = 0; i < nsectors; i++) {
     for (int ei = 0; ei < 4; ei++) {
       ata_smart_exterrlog_error_log & entry = log[i].error_logs[ei];
       fix_exterrlog_lba_cmd(entry.error);
       for (int ci = 0; ci < 5; ci++)
         fix_exterrlog_lba_cmd(entry.commands[ci]);
     }
   }
}

// Read Extended Comprehensive Error Log
bool ataReadExtErrorLog(ata_device * device, ata_smart_exterrlog * log,
                        unsigned nsectors, firmwarebug_defs firmwarebugs)
{
  if (!ataReadLogExt(device, 0x03, 0x00, 0, log, nsectors))
    return false;

  check_multi_sector_sum(log, nsectors, "SMART Extended Comprehensive Error Log Structure");

  if (isbigendian()) {
    swapx(&log->device_error_count);
    swapx(&log->error_log_index);
    for (unsigned i = 0; i < nsectors; i++) {
      for (unsigned j = 0; j < 4; j++) {
        for (unsigned k = 0; k < 5; k++)
           swapx(&log[i].error_logs[j].commands[k].timestamp);
        swapx(&log[i].error_logs[j].error.timestamp);
      }
    }
  }

  if (firmwarebugs.is_set(BUG_XERRORLBA))
    fix_exterrlog_lba(log, nsectors);

  return true;
}


int ataReadSmartThresholds (ata_device * device, struct ata_smart_thresholds_pvt *data){
  
  // get data from device
  if (smartcommandhandler(device, READ_THRESHOLDS, 0, (char *)data)){
    return -1;
  }
  
  // compute its checksum, and issue a warning if needed
  if (checksum(data))
    checksumwarning("SMART Attribute Thresholds Structure");
  
  // swap endian order if needed
  if (isbigendian())
    swap2((char *)&(data->revnumber));

  return 0;
}

int ataEnableSmart (ata_device * device ){
  if (smartcommandhandler(device, ENABLE, 0, NULL)){
    return -1;
  }
  return 0;
}

int ataDisableSmart (ata_device * device ){
  
  if (smartcommandhandler(device, DISABLE, 0, NULL)){
    return -1;
  }  
  return 0;
}

int ataEnableAutoSave(ata_device * device){
  if (smartcommandhandler(device, AUTOSAVE, 241, NULL)){
    return -1;
  }
  return 0;
}

int ataDisableAutoSave(ata_device * device){
  
  if (smartcommandhandler(device, AUTOSAVE, 0, NULL)){
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
    return -1;
  }
  return 0;
}

// Another Obsolete Command.  See comments directly above, associated
// with the corresponding Enable command.
int ataDisableAutoOffline (ata_device * device){
  
  if (smartcommandhandler(device, AUTO_OFFLINE, 0, NULL)){
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
int ataSmartTest(ata_device * device, int testtype, bool force,
                 const ata_selective_selftest_args & selargs,
                 const ata_smart_values * sv, uint64_t num_sectors)
{
  char cmdmsg[128]; const char *type, *captive;
  int cap, retval, select=0;

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
    type = 0;

  // Check whether another test is already running
  if (type && (sv->self_test_exec_status >> 4) == 0xf) {
    if (!force) {
      pout("Can't start self-test without aborting current test (%d0%% remaining),\n"
           "%srun 'smartctl -X' to abort test.\n",
           sv->self_test_exec_status & 0x0f,
           (!select ? "add '-t force' option to override, or " : ""));
      return -1;
    }
  }
  else
    force = false;

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
    snprintf(cmdmsg, sizeof(cmdmsg), "Abort SMART off-line mode self-test routine");
  else if (!type)
    snprintf(cmdmsg, sizeof(cmdmsg), "SMART EXECUTE OFF-LINE IMMEDIATE subcommand 0x%02x", testtype);
  else
    snprintf(cmdmsg, sizeof(cmdmsg), "Execute SMART %s routine immediately in %s mode", type, captive);
  pout("Sending command: \"%s\".\n",cmdmsg);

  if (select) {
    int i;
    pout("SPAN         STARTING_LBA           ENDING_LBA\n");
    for (i = 0; i < selargs_io.num_spans; i++)
      pout("   %d %20" PRId64 " %20" PRId64 "\n", i,
           selargs_io.span[i].start,
           selargs_io.span[i].end);
  }
  
  // Now send the command to test
  if (smartcommandhandler(device, IMMEDIATE_OFFLINE, testtype, NULL)) {
    if (!(cap && device->get_errno() == EIO)) {
      pout("Command \"%s\" failed: %s\n", cmdmsg, device->get_errmsg());
      return -1;
    }
  }
  
  // Since the command succeeded, tell user
  if (testtype==ABORT_SELF_TEST)
    pout("Self-testing aborted!\n");
  else {
    pout("Drive command \"%s\" successful.\n", cmdmsg);
    if (type)
      pout("Testing has begun%s.\n", (force ? " (previous test aborted)" : ""));
  }
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
    if (data->extend_test_completion_time_b == 0xff
        && data->extend_test_completion_time_w != 0x0000
        && data->extend_test_completion_time_w != 0xffff)
      return data->extend_test_completion_time_w; // ATA-8
    else
      return data->extend_test_completion_time_b;
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

// Get attribute state
ata_attr_state ata_get_attr_state(const ata_smart_attribute & attr,
                                  int attridx,
                                  const ata_smart_threshold_entry * thresholds,
                                  const ata_vendor_attr_defs & defs,
                                  unsigned char * threshval /* = 0 */)
{
  if (!attr.id)
    return ATTRSTATE_NON_EXISTING;

  // Normalized values (current,worst,threshold) not valid
  // if specified by '-v' option.
  // (Some SSD disks uses these bytes to store raw value).
  if (defs[attr.id].flags & ATTRFLAG_NO_NORMVAL)
    return ATTRSTATE_NO_NORMVAL;

  // Normally threshold is at same index as attribute
  int i = attridx;
  if (thresholds[i].id != attr.id) {
    // Find threshold id in table
    for (i = 0; thresholds[i].id != attr.id; ) {
      if (++i >= NUMBER_ATA_SMART_ATTRIBUTES)
        // Threshold id missing or thresholds cannot be read
        return ATTRSTATE_NO_THRESHOLD;
    }
  }
  unsigned char threshold = thresholds[i].threshold;

  // Return threshold if requested
  if (threshval)
    *threshval = threshold;

  // Don't report a failed attribute if its threshold is 0.
  // ATA-3 (X3T13/2008D Revision 7b) declares 0x00 as the "always passing"
  // threshold (Later ATA versions declare all thresholds as "obsolete").
  // In practice, threshold value 0 is often used for usage attributes.
  if (!threshold)
    return ATTRSTATE_OK;

  // Failed now if current value is below threshold
  if (attr.current <= threshold)
    return ATTRSTATE_FAILED_NOW;

  // Failed in the past if worst value is below threshold
  if (!(defs[attr.id].flags & ATTRFLAG_NO_WORSTVAL) && attr.worst <= threshold)
    return ATTRSTATE_FAILED_PAST;

  return ATTRSTATE_OK;
}

// Get default raw value print format
static ata_attr_raw_format get_default_raw_format(unsigned char id)
{
  switch (id) {
  case 3:   // Spin-up time
    return RAWFMT_RAW16_OPT_AVG16;

  case 5:   // Reallocated sector count
  case 196: // Reallocated event count
    return RAWFMT_RAW16_OPT_RAW16;

  case 9:  // Power on hours
    return RAWFMT_RAW24_OPT_RAW8;

  case 190: // Temperature
  case 194:
    return RAWFMT_TEMPMINMAX;

  default:
    return RAWFMT_RAW48;
  }
}

// Get attribute raw value.
uint64_t ata_get_attr_raw_value(const ata_smart_attribute & attr,
                                const ata_vendor_attr_defs & defs)
{
  const ata_vendor_attr_defs::entry & def = defs[attr.id];

  // Use default byteorder if not specified
  const char * byteorder = def.byteorder;
  if (!*byteorder) {
    switch (def.raw_format) {
      case RAWFMT_RAW64:
      case RAWFMT_HEX64:
        byteorder = "543210wv"; break;
      case RAWFMT_RAW56:
      case RAWFMT_HEX56:
      case RAWFMT_RAW24_DIV_RAW32:
      case RAWFMT_MSEC24_HOUR32:
        byteorder = "r543210"; break;
      default:
        byteorder = "543210"; break;
    }
  }

  // Build 64-bit value from selected bytes
  uint64_t rawvalue = 0;
  for (int i = 0; byteorder[i]; i++) {
    unsigned char b;
    switch (byteorder[i]) {
      case '0': b = attr.raw[0];  break;
      case '1': b = attr.raw[1];  break;
      case '2': b = attr.raw[2];  break;
      case '3': b = attr.raw[3];  break;
      case '4': b = attr.raw[4];  break;
      case '5': b = attr.raw[5];  break;
      case 'r': b = attr.reserv;  break;
      case 'v': b = attr.current; break;
      case 'w': b = attr.worst;   break;
      default : b = 0;            break;
    }
    rawvalue <<= 8; rawvalue |= b;
  }

  return rawvalue;
}

// Helper functions for RAWFMT_TEMPMINMAX
static inline int check_temp_word(unsigned word)
{
  if (word <= 0x7f)
    return 0x11; // >= 0, signed byte or word
  if (word <= 0xff)
    return 0x01; // < 0, signed byte
  if (0xff80 <= word)
    return 0x10; // < 0, signed word
  return 0x00;
}

static bool check_temp_range(int t, unsigned char ut1, unsigned char ut2,
                             int & lo, int & hi)
{
  int t1 = (signed char)ut1, t2 = (signed char)ut2;
  if (t1 > t2) {
    int tx = t1; t1 = t2; t2 = tx;
  }

  if (   -60 <= t1 && t1 <= t && t <= t2 && t2 <= 120
      && !(t1 == -1 && t2 <= 0)                      ) {
    lo = t1; hi = t2;
    return true;
  }
  return false;
}

// Format attribute raw value.
std::string ata_format_attr_raw_value(const ata_smart_attribute & attr,
                                      const ata_vendor_attr_defs & defs)
{
  // Get 48 bit or 64 bit raw value
  uint64_t rawvalue = ata_get_attr_raw_value(attr, defs);

  // Split into bytes and words
  unsigned char raw[6];
  raw[0] = (unsigned char) rawvalue;
  raw[1] = (unsigned char)(rawvalue >>  8);
  raw[2] = (unsigned char)(rawvalue >> 16);
  raw[3] = (unsigned char)(rawvalue >> 24);
  raw[4] = (unsigned char)(rawvalue >> 32);
  raw[5] = (unsigned char)(rawvalue >> 40);
  unsigned word[3];
  word[0] = raw[0] | (raw[1] << 8);
  word[1] = raw[2] | (raw[3] << 8);
  word[2] = raw[4] | (raw[5] << 8);

  // Get print format
  ata_attr_raw_format format = defs[attr.id].raw_format;
  if (format == RAWFMT_DEFAULT)
    format = get_default_raw_format(attr.id);

  // Print
  std::string s;
  switch (format) {
  case RAWFMT_RAW8:
    s = strprintf("%d %d %d %d %d %d",
      raw[5], raw[4], raw[3], raw[2], raw[1], raw[0]);
    break;

  case RAWFMT_RAW16:
    s = strprintf("%u %u %u", word[2], word[1], word[0]);
    break;

  case RAWFMT_RAW48:
  case RAWFMT_RAW56:
  case RAWFMT_RAW64:
    s = strprintf("%" PRIu64, rawvalue);
    break;

  case RAWFMT_HEX48:
    s = strprintf("0x%012" PRIx64, rawvalue);
    break;

  case RAWFMT_HEX56:
    s = strprintf("0x%014" PRIx64, rawvalue);
    break;

  case RAWFMT_HEX64:
    s = strprintf("0x%016" PRIx64, rawvalue);
    break;

  case RAWFMT_RAW16_OPT_RAW16:
    s = strprintf("%u", word[0]);
    if (word[1] || word[2])
      s += strprintf(" (%u %u)", word[2], word[1]);
    break;

  case RAWFMT_RAW16_OPT_AVG16:
    s = strprintf("%u", word[0]);
    if (word[1])
      s += strprintf(" (Average %u)", word[1]);
    break;

  case RAWFMT_RAW24_OPT_RAW8:
    s = strprintf("%u", (unsigned)(rawvalue & 0x00ffffffULL));
    if (raw[3] || raw[4] || raw[5])
      s += strprintf(" (%d %d %d)", raw[5], raw[4], raw[3]);
    break;

  case RAWFMT_RAW24_DIV_RAW24:
    s = strprintf("%u/%u",
      (unsigned)(rawvalue >> 24), (unsigned)(rawvalue & 0x00ffffffULL));
    break;

  case RAWFMT_RAW24_DIV_RAW32:
    s = strprintf("%u/%u",
      (unsigned)(rawvalue >> 32), (unsigned)(rawvalue & 0xffffffffULL));
    break;

  case RAWFMT_MIN2HOUR:
    {
      // minutes
      int64_t temp = word[0]+(word[1]<<16);
      int64_t tmp1 = temp/60;
      int64_t tmp2 = temp%60;
      s = strprintf("%" PRIu64 "h+%02" PRIu64 "m", tmp1, tmp2);
      if (word[2])
        s += strprintf(" (%u)", word[2]);
    }
    break;

  case RAWFMT_SEC2HOUR:
    {
      // seconds
      int64_t hours = rawvalue/3600;
      int64_t minutes = (rawvalue-3600*hours)/60;
      int64_t seconds = rawvalue%60;
      s = strprintf("%" PRIu64 "h+%02" PRIu64 "m+%02" PRIu64 "s", hours, minutes, seconds);
    }
    break;

  case RAWFMT_HALFMIN2HOUR:
    {
      // 30-second counter
      int64_t hours = rawvalue/120;
      int64_t minutes = (rawvalue-120*hours)/2;
      s += strprintf("%" PRIu64 "h+%02" PRIu64 "m", hours, minutes);
    }
    break;

  case RAWFMT_MSEC24_HOUR32:
    {
      // hours + milliseconds
      unsigned hours = (unsigned)(rawvalue & 0xffffffffULL);
      unsigned milliseconds = (unsigned)(rawvalue >> 32);
      unsigned seconds = milliseconds / 1000;
      s = strprintf("%uh+%02um+%02u.%03us",
        hours, seconds / 60, seconds % 60, milliseconds % 1000);
    }
    break;

  case RAWFMT_TEMPMINMAX:
    // Temperature
    {
      // Search for possible min/max values
      // [5][4][3][2][1][0] raw[]
      // [ 2 ] [ 1 ] [ 0 ]  word[]
      // xx HH xx LL xx TT (Hitachi/HGST)
      // xx LL xx HH xx TT (Kingston SSDs)
      // 00 00 HH LL xx TT (Maxtor, Samsung, Seagate, Toshiba)
      // 00 00 00 HH LL TT (WDC)
      // CC CC HH LL xx TT (WDC, CCCC=over temperature count)
      // (xx = 00/ff, possibly sign extension of lower byte)

      int t = (signed char)raw[0];
      int lo = 0, hi = 0;

      int tformat;
      int ctw0 = check_temp_word(word[0]);
      if (!word[2]) {
        if (!word[1] && ctw0)
          // 00 00 00 00 xx TT
          tformat = 0;
        else if (ctw0 && check_temp_range(t, raw[2], raw[3], lo, hi))
          // 00 00 HL LH xx TT
          tformat = 1;
        else if (!raw[3] && check_temp_range(t, raw[1], raw[2], lo, hi))
          // 00 00 00 HL LH TT
          tformat = 2;
        else
          tformat = -1;
      }
      else if (ctw0) {
        if (   (ctw0 & check_temp_word(word[1]) & check_temp_word(word[2])) != 0x00
            && check_temp_range(t, raw[2], raw[4], lo, hi)                         )
          // xx HL xx LH xx TT
          tformat = 3;
        else if (   word[2] < 0x7fff
                 && check_temp_range(t, raw[2], raw[3], lo, hi)
                 && hi >= 40                                   )
          // CC CC HL LH xx TT
          tformat = 4;
        else
          tformat = -2;
      }
      else
        tformat = -3;

      switch (tformat) {
        case 0:
          s = strprintf("%d", t);
          break;
        case 1: case 2: case 3:
          s = strprintf("%d (Min/Max %d/%d)", t, lo, hi);
          break;
        case 4:
          s = strprintf("%d (Min/Max %d/%d #%d)", t, lo, hi, word[2]);
          break;
        default:
          s = strprintf("%d (%d %d %d %d %d)", raw[0], raw[5], raw[4], raw[3], raw[2], raw[1]);
          break;
      }
    }
    break;

  case RAWFMT_TEMP10X:
    // ten times temperature in Celsius
    s = strprintf("%d.%d", word[0]/10, word[0]%10);
    break;

  default:
    s = "?"; // Should not happen
    break;
  }

  return s;
}

// Attribute names shouldn't be longer than 23 chars, otherwise they break the
// output of smartctl.
static const char * get_default_attr_name(unsigned char id, int rpm)
{
  bool hdd = (rpm > 1), ssd = (rpm == 1);

  static const char Unknown_HDD_Attribute[] = "Unknown_HDD_Attribute";
  static const char Unknown_SSD_Attribute[] = "Unknown_SSD_Attribute";

  switch (id) {
  case 1:
    return "Raw_Read_Error_Rate";
  case 2:
    return "Throughput_Performance";
  case 3:
    return "Spin_Up_Time";
  case 4:
    return "Start_Stop_Count";
  case 5:
    return "Reallocated_Sector_Ct";
  case 6:
    if (ssd) return Unknown_SSD_Attribute;
    return "Read_Channel_Margin";
  case 7:
    if (ssd) return Unknown_SSD_Attribute;
    return "Seek_Error_Rate";
  case 8:
    if (ssd) return Unknown_SSD_Attribute;
    return "Seek_Time_Performance";
  case 9:
    return "Power_On_Hours";
  case 10:
    if (ssd) return Unknown_SSD_Attribute;
    return "Spin_Retry_Count";
  case 11:
    if (ssd) return Unknown_SSD_Attribute;
    return "Calibration_Retry_Count";
  case 12:
    return "Power_Cycle_Count";
  case 13:
    return "Read_Soft_Error_Rate";
  case 175:
    if (hdd) return Unknown_HDD_Attribute;
    return "Program_Fail_Count_Chip";
  case 176:
    if (hdd) return Unknown_HDD_Attribute;
    return "Erase_Fail_Count_Chip";
  case 177:
    if (hdd) return Unknown_HDD_Attribute;
    return "Wear_Leveling_Count";
  case 178:
    if (hdd) return Unknown_HDD_Attribute;
    return "Used_Rsvd_Blk_Cnt_Chip";
  case 179:
    if (hdd) return Unknown_HDD_Attribute;
    return "Used_Rsvd_Blk_Cnt_Tot";
  case 180:
    if (hdd) return Unknown_HDD_Attribute;
    return "Unused_Rsvd_Blk_Cnt_Tot";
  case 181:
    return "Program_Fail_Cnt_Total";
  case 182:
    if (hdd) return Unknown_HDD_Attribute;
    return "Erase_Fail_Count_Total";
  case 183:
    return "Runtime_Bad_Block";
  case 184:
    return "End-to-End_Error";
  case 187:
    return "Reported_Uncorrect";
  case 188:
    return "Command_Timeout";
  case 189:
    if (ssd) return Unknown_SSD_Attribute;
    return "High_Fly_Writes";
  case 190:
    // Western Digital uses this for temperature.
    // It's identical to Attribute 194 except that it
    // has a failure threshold set to correspond to the
    // max allowed operating temperature of the drive, which 
    // is typically 55C.  So if this attribute has failed
    // in the past, it indicates that the drive temp exceeded
    // 55C sometime in the past.
    return "Airflow_Temperature_Cel";
  case 191:
    if (ssd) return Unknown_SSD_Attribute;
    return "G-Sense_Error_Rate";
  case 192:
    return "Power-Off_Retract_Count";
  case 193:
    if (ssd) return Unknown_SSD_Attribute;
    return "Load_Cycle_Count";
  case 194:
    return "Temperature_Celsius";
  case 195:
    // Fujitsu: "ECC_On_The_Fly_Count";
    return "Hardware_ECC_Recovered";
  case 196:
    return "Reallocated_Event_Count";
  case 197:
    return "Current_Pending_Sector";
  case 198:
    return "Offline_Uncorrectable";
  case 199:
    return "UDMA_CRC_Error_Count";
  case 200:
    if (ssd) return Unknown_SSD_Attribute;
    // Western Digital
    return "Multi_Zone_Error_Rate";
  case 201:
    if (ssd) return Unknown_SSD_Attribute;
    return "Soft_Read_Error_Rate";
  case 202:
    if (ssd) return Unknown_SSD_Attribute;
    // Fujitsu: "TA_Increase_Count"
    return "Data_Address_Mark_Errs";
  case 203:
    // Fujitsu
    return "Run_Out_Cancel";
    // Maxtor: ECC Errors
  case 204:
    // Fujitsu: "Shock_Count_Write_Opern"
    return "Soft_ECC_Correction";
  case 205:
    // Fujitsu: "Shock_Rate_Write_Opern"
    return "Thermal_Asperity_Rate";
  case 206:
    // Fujitsu
    if (ssd) return Unknown_SSD_Attribute;
    return "Flying_Height";
  case 207:
    // Maxtor
    if (ssd) return Unknown_SSD_Attribute;
    return "Spin_High_Current";
  case 208:
    // Maxtor
    if (ssd) return Unknown_SSD_Attribute;
    return "Spin_Buzz";
  case 209:
    // Maxtor
    if (ssd) return Unknown_SSD_Attribute;
    return "Offline_Seek_Performnce";
  case 220:
    if (ssd) return Unknown_SSD_Attribute;
    return "Disk_Shift";
  case 221:
    if (ssd) return Unknown_SSD_Attribute;
    return "G-Sense_Error_Rate";
  case 222:
    if (ssd) return Unknown_SSD_Attribute;
    return "Loaded_Hours";
  case 223:
    if (ssd) return Unknown_SSD_Attribute;
    return "Load_Retry_Count";
  case 224:
    if (ssd) return Unknown_SSD_Attribute;
    return "Load_Friction";
  case 225:
    if (ssd) return Unknown_SSD_Attribute;
    return "Load_Cycle_Count";
  case 226:
    if (ssd) return Unknown_SSD_Attribute;
    return "Load-in_Time";
  case 227:
    if (ssd) return Unknown_SSD_Attribute;
    return "Torq-amp_Count";
  case 228:
    return "Power-off_Retract_Count";
  case 230:
    // seen in IBM DTPA-353750
    if (ssd) return Unknown_SSD_Attribute;
    return "Head_Amplitude";
  case 231:
    return "Temperature_Celsius";
  case 232:
    // seen in Intel X25-E SSD
    return "Available_Reservd_Space";
  case 233:
    // seen in Intel X25-E SSD
    if (hdd) return Unknown_HDD_Attribute;
    return "Media_Wearout_Indicator";
  case 240:
    if (ssd) return Unknown_SSD_Attribute;
    return "Head_Flying_Hours";
  case 241:
    return "Total_LBAs_Written";
  case 242:
    return "Total_LBAs_Read";
  case 250:
    return "Read_Error_Retry_Rate";
  case 254:
    if (ssd) return Unknown_SSD_Attribute;
    return "Free_Fall_Sensor";
  default:
    return "Unknown_Attribute";
  }
}

// Get attribute name
std::string ata_get_smart_attr_name(unsigned char id, const ata_vendor_attr_defs & defs,
                                    int rpm /* = 0 */)
{
  if (!defs[id].name.empty())
    return defs[id].name;
  else
    return get_default_attr_name(id, rpm);
}

// Find attribute index for attribute id, -1 if not found.
int ata_find_attr_index(unsigned char id, const ata_smart_values & smartval)
{
  if (!id)
    return -1;
  for (int i = 0; i < NUMBER_ATA_SMART_ATTRIBUTES; i++) {
    if (smartval.vendor_attributes[i].id == id)
      return i;
  }
  return -1;
}

// Return Temperature Attribute raw value selected according to possible
// non-default interpretations. If the Attribute does not exist, return 0
unsigned char ata_return_temperature_value(const ata_smart_values * data, const ata_vendor_attr_defs & defs)
{
  for (int i = 0; i < 4; i++) {
    static const unsigned char ids[4] = {194, 190, 9, 220};
    unsigned char id = ids[i];
    const ata_attr_raw_format format = defs[id].raw_format;
    if (!(   ((id == 194 || id == 190) && format == RAWFMT_DEFAULT)
          || format == RAWFMT_TEMPMINMAX || format == RAWFMT_TEMP10X))
      continue;
    int idx = ata_find_attr_index(id, *data);
    if (idx < 0)
      continue;
    uint64_t raw = ata_get_attr_raw_value(data->vendor_attributes[idx], defs);
    unsigned temp;
    // ignore possible min/max values in high words
    if (format == RAWFMT_TEMP10X) // -v N,temp10x
      temp = ((unsigned short)raw + 5) / 10;
    else
      temp = (unsigned char)raw;
    if (!(0 < temp && temp < 128))
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
    pout("Read SCT Status failed: %s\n", device->get_errmsg());
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
    pout("Unknown SCT Status format version %u, should be 2 or 3.\n", sts->format_version);
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

  // swap endian order if needed
  if (isbigendian()) {
    swapx(&cmd.action_code);
    swapx(&cmd.function_code);
    swapx(&cmd.table_id);
  }

  // write command via SMART log page 0xe0
  if (smartcommandhandler(device, WRITE_LOG, 0xe0, (char *)&cmd)){
    pout("Write SCT Data Table failed: %s\n", device->get_errmsg());
    return -1;
  }

  // read SCT data via SMART log page 0xe1
  memset(tmh, 0, sizeof(*tmh));
  if (smartcommandhandler(device, READ_LOG, 0xe1, (char *)tmh)){
    pout("Read SCT Data Table failed: %s\n", device->get_errmsg());
    return -1;
  }

  // re-read and check SCT status
  if (ataReadSCTStatus(device, sts))
    return -1;

  if (!(sts->ext_status_code == 0 && sts->action_code == 5 && sts->function_code == 1)) {
    pout("Unexpected SCT status 0x%04x (action_code=%u, function_code=%u)\n",
      sts->ext_status_code, sts->action_code, sts->function_code);
    return -1;
  }

  // swap endian order if needed
  if (isbigendian()){
    swapx(&tmh->format_version);
    swapx(&tmh->sampling_period);
    swapx(&tmh->interval);
    swapx(&tmh->cb_index);
    swapx(&tmh->cb_size);
  }
  return 0;
}

// Get/Set Write Cache Reordering
int ataGetSetSCTWriteCacheReordering(ata_device * device, bool enable, bool persistent, bool set)
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
  cmd.function_code  = (set ? 1 : 2); // 1=Set, 2=Get
  cmd.feature_code  = 2; //  Enable/Disable Write Cache Reordering 
  cmd.state         = (enable ? 1 : 2); // 1 enable, 2 disable
  cmd.option_flags  = (persistent ? 0x01 : 0x00);

  // swap endian order if needed
  if (isbigendian()) {
    swapx(&cmd.action_code);
    swapx(&cmd.function_code);
    swapx(&cmd.feature_code);
    swapx(&cmd.state);
    swapx(&cmd.option_flags);
  }

  // write command via SMART log page 0xe0
  // TODO: Debug output
  ata_cmd_in in;
  in.in_regs.command = ATA_SMART_CMD;
  in.in_regs.lba_high = SMART_CYL_HI; in.in_regs.lba_mid = SMART_CYL_LOW;
  in.in_regs.features = ATA_SMART_WRITE_LOG_SECTOR;
  in.in_regs.lba_low = 0xe0;
  in.set_data_out(&cmd, 1);

  if (!set)
    // Time limit returned in ATA registers
    in.out_needed.sector_count = in.out_needed.lba_low = true;

  ata_cmd_out out;
  if (!device->ata_pass_through(in, out)) {
    pout("Write SCT (%cet) Feature Control Command failed: %s\n",
      (!set ? 'G' : 'S'), device->get_errmsg());
    return -1;
  }
  int state = out.out_regs.sector_count | (out.out_regs.lba_low << 8);

  // re-read and check SCT status
  if (ataReadSCTStatus(device, &sts))
    return -1;

  if (!(sts.ext_status_code == 0 && sts.action_code == 4 && sts.function_code == (set ? 1 : 2))) {
    pout("Unexpected SCT status 0x%04x (action_code=%u, function_code=%u)\n",
      sts.ext_status_code, sts.action_code, sts.function_code);
    return -1;
  }
  return state;
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

  // swap endian order if needed
  if (isbigendian()) {
    swapx(&cmd.action_code);
    swapx(&cmd.function_code);
    swapx(&cmd.feature_code);
    swapx(&cmd.state);
    swapx(&cmd.option_flags);
  }

  // write command via SMART log page 0xe0
  if (smartcommandhandler(device, WRITE_LOG, 0xe0, (char *)&cmd)){
    pout("Write SCT Feature Control Command failed: %s\n", device->get_errmsg());
    return -1;
  }

  // re-read and check SCT status
  if (ataReadSCTStatus(device, &sts))
    return -1;

  if (!(sts.ext_status_code == 0 && sts.action_code == 4 && sts.function_code == 1)) {
    pout("Unexpected SCT status 0x%04x (action_code=%u, function_code=%u)\n",
      sts.ext_status_code, sts.action_code, sts.function_code);
    return -1;
  }
  return 0;
}

// Get/Set SCT Error Recovery Control
static int ataGetSetSCTErrorRecoveryControltime(ata_device * device, unsigned type,
                                                bool set, unsigned short & time_limit)
{
  // Check initial status
  ata_sct_status_response sts;
  if (ataReadSCTStatus(device, &sts))
    return -1;

  // Do nothing if other SCT command is executing
  if (sts.ext_status_code == 0xffff) {
    pout("Another SCT command is executing, abort Error Recovery Control\n"
         "(SCT ext_status_code 0x%04x, action_code=%u, function_code=%u)\n",
      sts.ext_status_code, sts.action_code, sts.function_code);
    return -1;
  }

  ata_sct_error_recovery_control_command cmd; memset(&cmd, 0, sizeof(cmd));
  // CAUTION: DO NOT CHANGE THIS VALUE (SOME ACTION CODES MAY ERASE DISK)
  cmd.action_code    = 3; // Error Recovery Control command
  cmd.function_code  = (set ? 1 : 2); // 1=Set timer, 2=Get timer
  cmd.selection_code = type; // 1=Read timer, 2=Write timer
  if (set)
    cmd.time_limit   = time_limit;

  // swap endian order if needed
  if (isbigendian()) {
    swapx(&cmd.action_code);
    swapx(&cmd.function_code);
    swapx(&cmd.selection_code);
    swapx(&cmd.time_limit);
  }

  // write command via SMART log page 0xe0
  // TODO: Debug output
  ata_cmd_in in;
  in.in_regs.command = ATA_SMART_CMD;
  in.in_regs.lba_high = SMART_CYL_HI; in.in_regs.lba_mid = SMART_CYL_LOW;
  in.in_regs.features = ATA_SMART_WRITE_LOG_SECTOR;
  in.in_regs.lba_low = 0xe0;
  in.set_data_out(&cmd, 1);

  if (!set)
    // Time limit returned in ATA registers
    in.out_needed.sector_count = in.out_needed.lba_low = true;

  ata_cmd_out out;
  if (!device->ata_pass_through(in, out)) {
    pout("Write SCT (%cet) Error Recovery Control Command failed: %s\n",
      (!set ? 'G' : 'S'), device->get_errmsg());
    return -1;
  }

  // re-read and check SCT status
  if (ataReadSCTStatus(device, &sts))
    return -1;

  if (!(sts.ext_status_code == 0 && sts.action_code == 3 && sts.function_code == (set ? 1 : 2))) {
    pout("Unexpected SCT status 0x%04x (action_code=%u, function_code=%u)\n",
      sts.ext_status_code, sts.action_code, sts.function_code);
    return -1;
  }

  if (!set) {
    // Check whether registers are properly returned by ioctl()
    if (!(out.out_regs.sector_count.is_set() && out.out_regs.lba_low.is_set())) {
      // TODO: Output register support should be checked within each ata_pass_through()
      // implementation before command is issued.
      pout("SMART WRITE LOG does not return COUNT and LBA_LOW register\n");
      return -1;
    }
    if (   out.out_regs.sector_count == in.in_regs.sector_count
        && out.out_regs.lba_low      == in.in_regs.lba_low     ) {
      // 0xe001 (5734.5s) - this is most likely a broken ATA pass-through implementation
      pout("SMART WRITE LOG returns COUNT and LBA_LOW register unchanged\n");
      return -1;
    }

    // Return value to caller
    time_limit = out.out_regs.sector_count | (out.out_regs.lba_low << 8);
  }

  return 0;
}

// Get SCT Error Recovery Control
int ataGetSCTErrorRecoveryControltime(ata_device * device, unsigned type, unsigned short & time_limit)
{
  return ataGetSetSCTErrorRecoveryControltime(device, type, false/*get*/, time_limit);
}

// Set SCT Error Recovery Control
int ataSetSCTErrorRecoveryControltime(ata_device * device, unsigned type, unsigned short time_limit)
{
  return ataGetSetSCTErrorRecoveryControltime(device, type, true/*set*/, time_limit);
}


// Print one self-test log entry.
// Returns:
// -1: self-test failed
//  1: extended self-test completed without error
//  0: otherwise
int ataPrintSmartSelfTestEntry(unsigned testnum, unsigned char test_type,
                               unsigned char test_status,
                               unsigned short timestamp,
                               uint64_t failing_lba,
                               bool print_error_only, bool & print_header)
{
  // Check status and type for return value
  int retval = 0;
  switch (test_status >> 4) {
    case 0x0:
      if ((test_type & 0x0f) == 0x02)
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
    pout("Num  Test_Description    Status                  Remaining  LifeTime(hours)  LBA_of_first_error\n");
  }

  char msglba[32];
  if (retval < 0 && failing_lba < 0xffffffffffffULL)
    snprintf(msglba, sizeof(msglba), "%" PRIu64, failing_lba);
  else {
    msglba[0] = '-'; msglba[1] = 0;
  }

  pout("#%2u  %-19s %-29s %1d0%%  %8u         %s\n", testnum,
       msgtest.c_str(), msgstat.c_str(), test_status & 0x0f, timestamp, msglba);

  return retval;
}

// Print Smart self-test log, used by smartctl and smartd.
// return value is:
// bottom 8 bits: number of entries found where self-test showed an error
// remaining bits: if nonzero, power on hours of last self-test where error was found
int ataPrintSmartSelfTestlog(const ata_smart_selftestlog * data, bool allentries,
                             firmwarebug_defs firmwarebugs)
{
  if (allentries)
    pout("SMART Self-test log structure revision number %d\n",(int)data->revnumber);
  if (data->revnumber != 0x0001 && allentries && !firmwarebugs.is_set(BUG_SAMSUNG))
    pout("Warning: ATA Specification requires self-test log structure revision number = 1\n");
  if (data->mostrecenttest==0){
    if (allentries)
      pout("No self-tests have been logged.  [To run self-tests, use: smartctl -t]\n");
    return 0;
  }

  bool noheaderprinted = true;
  int errcnt = 0, hours = 0, igncnt = 0;
  int testno = 0, ext_ok_testno = -1;

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
      int state = ataPrintSmartSelfTestEntry(testno,
        log->selftestnumber, log->selfteststatus,
        log->timestamp, lba48, !allentries, noheaderprinted);

      if (state < 0) {
        // Self-test showed an error
        if (ext_ok_testno < 0) {
          errcnt++;

          // keep track of time of most recent error
          if (!hours)
            hours = log->timestamp;
        }
        else
          // Newer successful extended self-test exits
          igncnt++;
      }
      else if (state > 0 && ext_ok_testno < 0) {
        // Latest successful extended self-test
        ext_ok_testno = testno;
      }
    }
  }

  if (igncnt)
    pout("%d of %d failed self-tests are outdated by newer successful extended offline self-test #%2d\n",
      igncnt, igncnt+errcnt, ext_ok_testno);

  if (!allentries && !noheaderprinted)
    pout("\n");

  return ((hours << 8) | errcnt);
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
  const regular_expression regex(pattern, REG_EXTENDED);

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
