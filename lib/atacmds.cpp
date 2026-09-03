/*
 * atacmds.cpp
 * 
 * Home page of code is: https://www.smartmontools.org
 *
 * Copyright (C) 2002-11 Bruce Allen
 * Copyright (C) 2008-26 Christian Franke
 * Copyright (C) 1999-2000 Michael Cornwell <cornwell@acm.org>
 * Copyright (C) 2000 Andre Hedrick <andre@linux-ide.org>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "config.h"
#define __STDC_FORMAT_MACROS 1 // enable PRI* for C++

#include <smartmon/atacmds.h>
#include <smartmon/hexdump.h>
#include <smartmon/knowndrives.h>  // get_default_attr_defs()
#include <smartmon/utility.h>

#include <errno.h>
#include <inttypes.h>

namespace smartmon {

// Print ATA debug messages?
unsigned char ata_debugmode = 0;

// Suppress serial number?
// (also used in scsiprint.cpp)
bool dont_print_serial_number = false;

static lib_ata_hook the_lib_ata_hook;
static lib_ata_hook * current_lib_ata_hook = &the_lib_ata_hook;

lib_ata_hook & lib_ata_hook::get()
{
  return *current_lib_ata_hook;
}

void lib_ata_hook::set(lib_ata_hook & hook)
{
  current_lib_ata_hook = &hook;
}

void lib_ata_hook::reset()
{
  current_lib_ata_hook = &the_lib_ata_hook;
}

void lib_ata_hook::on_checksum_error(const char * datatype)
{
  lib_printf("Warning! %s error: invalid SMART checksum.\n", datatype);
}

// Old function used below to warn users about invalid checksums.
static void checksumwarning(const char * datatype)
{
  lib_ata_hook::get().on_checksum_error(datatype);
}

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
  char fmtname[32+1], attrname[32+1], hddssd[3+1];
  attrname[0] = hddssd[0] = 0;

  if (opt[0] == 'N') {
    // "N,format[,name]"
    if (!(   sscanf(opt, "N,%32[^,]%n,%32[^,]%n", fmtname, &n1, attrname, &n2) >= 1
          && (n1 == len || n2 == len)))
      return false;
  }
  else {
    // "id,format[+][,name[,HDD|SSD]]"
    int n3 = -1;
    if (!(   sscanf(opt, "%d,%32[^,]%n,%32[^,]%n,%3[DHS]%n",
                    &id, fmtname, &n1, attrname, &n2, hddssd, &n3) >= 2
          && 1 <= id && id <= 255
          && (    n1 == len || n2 == len
                  // ",HDD|SSD" for DEFAULT settings only
              || (n3 == len && priority == PRIOR_DEFAULT))))
      return false;
  }

  unsigned flags = 0;
  // For "-v 19[78],increasing" above
  if (fmtname[strlen(fmtname)-1] == '+') {
    fmtname[strlen(fmtname)-1] = 0;
    flags = ATTRFLAG_INCREASING;
  }

  // Split "format[:byteorder]"
  char byteorder[8+1] = "";
  if (strchr(fmtname, ':')) {
    if (priority == PRIOR_DEFAULT)
      // TODO: Allow Byteorder in DEFAULT entry
      return false;
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

  // ",HDD|SSD" suffix for DEFAULT settings
  if (hddssd[0]) {
    if (!strcmp(hddssd, "HDD"))
      flags |= ATTRFLAG_HDD_ONLY;
    else if (!strcmp(hddssd, "SSD"))
      flags |= ATTRFLAG_SSD_ONLY;
    else
      return false;
  }

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
// parse_attribute_def().  The strings are preceded by tabs and followed
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


// Invalidate serial number and WWN and adjust checksum in IDENTIFY data
static void invalidate_serno(ata_identify_device & id)
{
  uint8_t sum = 0;
  unsigned i;
  for (i = 0; i < sizeof(id.serial_no); i++) {
    sum +=  id.serial_no[i];
    sum -= (id.serial_no[i] = 'X');
  }

  uint8_t * b = reinterpret_cast<uint8_t *>(id.wwn);
  for (i = 0; i < sizeof(id.wwn); i++) {
    sum +=  b[i];
    sum -= (b[i] = 0x00);
  }

  if (id.signature == 0xa5)
    id.checksum += sum;
}

static const char * preg(const ata_register & r, char (& buf)[8])
{
  if (!r.is_set())
    //return "n/a ";
    return "....";
  snprintf(buf, sizeof(buf), "0x%02x", r.val());
  return buf;
}

static void print_regs(const char * prefix, const ata_in_regs_48bit & r,
  const char * suffix = "")
{
  char bufs[11][8];
  if (!r.is_48bit_cmd())
    lib_printf("%s FR=%s, SC=%s, LH=%s LM=%s LL=%s, DEV=%s, CMD=%s%s", prefix,
      preg(r.features, bufs[0]), preg(r.sector_count, bufs[1]),
      preg(r.lba_high, bufs[2]), preg(r.lba_mid, bufs[3]), preg(r.lba_low, bufs[4]),
      preg(r.device, bufs[5]), preg(r.command, bufs[6]), suffix);
  else
    lib_printf("%s FR=%s, SC=%s %s, LBA48=%s %s %s, LH=%s LM=%s LL=%s, DEV=%s, CMD=%s%s", prefix,
      preg(r.features, bufs[0]), preg(r.prev.sector_count, bufs[1]), preg(r.sector_count, bufs[2]),
      preg(r.prev.lba_high, bufs[3]), preg(r.prev.lba_mid, bufs[4]), preg(r.prev.lba_low, bufs[5]),
      preg(r.lba_high, bufs[6]), preg(r.lba_mid, bufs[7]), preg(r.lba_low, bufs[8]),
      preg(r.device, bufs[9]), preg(r.command, bufs[10]), suffix);
}

static void print_regs(const char * prefix, const ata_out_regs_48bit & r, bool is_48bit_cmd,
  const char * suffix = "")
{
  char bufs[11][8];
  if (!is_48bit_cmd)
    lib_printf("%sERR=%s, SC=%s, LH=%s, LM=%s, LL=%s, DEV=%s, STS=%s%s", prefix,
      preg(r.error, bufs[0]), preg(r.sector_count, bufs[1]),
      preg(r.lba_high, bufs[2]), preg(r.lba_mid, bufs[3]), preg(r.lba_low, bufs[4]),
      preg(r.device, bufs[5]), preg(r.status, bufs[6]), suffix);
  else
    lib_printf("%sERR=%s, SC=%s %s, LBA48=%s %s %s, LH=%s LM=%s LL=%s, DEV=%s, STS=%s%s", prefix,
      preg(r.error, bufs[0]), preg(r.prev.sector_count, bufs[1]), preg(r.sector_count, bufs[2]),
      preg(r.prev.lba_high, bufs[3]), preg(r.prev.lba_mid, bufs[4]), preg(r.prev.lba_low, bufs[5]),
      preg(r.lba_high, bufs[6]), preg(r.lba_mid, bufs[7]), preg(r.lba_low, bufs[8]),
      preg(r.device, bufs[9]), preg(r.status, bufs[10]), suffix);
}

// Print debug information for ATA pass-through input.
void ata_print_debug_info(const ata_cmd_in & in, const char * devname, bool dump)
{
  lib_printf(" [ATA call: device='%s', command='%s', size=0x%04x\n", devname,
    ata_get_command_name(in.in_regs.command, in.in_regs.features), in.size);

  print_regs("  Input:  ", in.in_regs,
    (in.direction==ata_cmd_in::data_in  ? " IN"  :
     in.direction==ata_cmd_in::data_out ? " OUT" : ""));

  if (in.direction == ata_cmd_in::data_out && dump) {
    lib_printf("\n");
    hexdump_options opts = hexdump_options_canonical;
    opts.prefix = "  ";
    opts.offset_max = 1;
    hexdump([](const char * str){lib_printf("%s", str);}, in.buffer, in.size, opts);
    lib_printf(" ");
  }
  lib_printf("]\n");
}

// Print debug information for ATA pass-through output.
void ata_print_debug_info(const ata_cmd_in & in, const ata_cmd_out & out,
  const smart_device::error_info & err, bool dump, long long duration_usec)
{
  if (duration_usec > 0)
    lib_printf(" [Duration: %.6fs]\n", duration_usec / 1000000.0);

  if (err.no) {
    lib_printf(" [ATA call failed: %s (errno=%d)", err.msg.c_str(), err.no);
    if (out.out_regs.is_set())
      print_regs("\n  Output: ", out.out_regs, in.in_regs.is_48bit_cmd());
  }
  else {
    lib_printf(" [ATA call succeeded");
    if (in.out_needed.is_set() || out.out_regs.is_set())
      print_regs("\n  Output: ", out.out_regs, in.in_regs.is_48bit_cmd());
    if (in.direction == ata_cmd_in::data_in && dump) {
      lib_printf("\n");
      hexdump_options opts = hexdump_options_canonical;
      opts.prefix = "  ";
      opts.offset_max = 1;
      hexdump([](const char * str){lib_printf("%s", str);}, in.buffer, in.size, opts);
      lib_printf(" ");
    }
  }
  lib_printf("]\n");
}

// Call ATA pass-through and print debug info if requested.
bool ata_pass_through(ata_device * device, const ata_cmd_in & in, ata_cmd_out & out)
{
  if (ata_debugmode)
    ata_print_debug_info(in, device->get_info_name(), (ata_debugmode > 1));

  long long start_usec = (ata_debugmode ? get_timer_usec() : -1);

  bool ok = device->ata_pass_through(in, out);

  long long duration_usec = (start_usec >= 0 ? get_timer_usec() - start_usec : -1);

  if (!ok && !(device->get_errno() && *device->get_errmsg()))
    device->set_err(EIO, "Missing error information");

  if (   dont_print_serial_number && ok && in.size >= 512
      && (   in.in_regs.command == ATA_IDENTIFY_DEVICE
          || in.in_regs.command == ATA_IDENTIFY_PACKET_DEVICE))
    // Identify (packet) device: invalidate serial number
    invalidate_serno(*reinterpret_cast<ata_identify_device *>(in.buffer));

  if (ata_debugmode)
    ata_print_debug_info(in, out, (!ok ? device->get_err() : smart_device::error_info{}),
      (ata_debugmode > 1), duration_usec);
  return ok;
}

// Call ATA pass-through and print debug info if requested.
// Variant without output parameters.
bool ata_pass_through(ata_device * device, const ata_cmd_in & in)
{
  ata_cmd_out out;
  return ata_pass_through(device, in, out);
}

// Get capacity and sector sizes from IDENTIFY data
void ata_get_size_info(const ata_identify_device & id, ata_size_info & sizes)
{
  sizes = {};
  // Return if no LBA support
  if (!(id.capabilities_1 & 0x0200))
    return;

  // Determine 48-bit LBA capacity if supported
  uint64_t user_sectors_48 = ((id.command_set_2 & 0xc400) == 0x4400
                              ? uile64_to_uint(id.user_sectors_48) : 0);

  // Return if capacity unknown (ATAPI CD/DVD)
  if (!(id.user_sectors_28 || user_sectors_48))
    return;

  // In some cases, 'user_sectors_48' is limited to 32bit (2TiB - 512B) and
  // the real value is provided in 'user_sectors_ext'.
  uint64_t user_sectors_ext = ((id.additional_support & 0x0004)
                               ? uile64_to_uint(id.user_sectors_ext) : 0);
  if (user_sectors_ext > user_sectors_48)
    user_sectors_48 = user_sectors_ext;

  // Determine sector sizes
  sizes.log_sector_size = sizes.phy_sector_size = 512;

  if ((id.phy_log_sector_size & 0xc000) == 0x4000) {
    // Long Logical/Physical Sectors (LLS/LPS) ?
    if (id.phy_log_sector_size & 0x1000)
      // Logical sector size is specified in 16-bit words
      sizes.log_sector_size = sizes.phy_sector_size = uile32_to_uint(id.log_sector_size) << 1;

    if (id.phy_log_sector_size & 0x2000)
      // Physical sector size is multiple of logical sector size
      sizes.phy_sector_size <<= (id.phy_log_sector_size & 0x0f);

    if ((id.log_sector_align & 0xc000) == 0x4000)
      sizes.log_sector_offset = (id.log_sector_align & 0x3fff) * sizes.log_sector_size;
  }

  // Some early 4KiB LLS disks (Samsung N3U-3) return bogus user_sectors_28 value
  if (user_sectors_48 >= id.user_sectors_28 || (user_sectors_48 && sizes.log_sector_size > 512))
    sizes.sectors = user_sectors_48;
  else
    sizes.sectors = id.user_sectors_28;

  sizes.capacity = sizes.sectors * sizes.log_sector_size;
}

// This function computes the checksum of a single disk sector (512
// bytes).  Returns zero if checksum is OK, nonzero if the checksum is
// incorrect.  The size (512) is correct for all SMART structures.
uint8_t ata_checksum(const void * data)
{
  uint8_t sum = 0;
  for (int i = 0; i < 512; i++)
    sum += reinterpret_cast<const uint8_t *>(data)[i];
  return sum;
}

// returns -1 if command fails or the device is in Sleep mode, else
// value of Sector Count register.  Sector Count result values:
//   00h device is in Standby mode. 
//   80h device is in Idle mode.
//   FFh device is in Active mode or Idle mode.
int ata_check_power_mode(ata_device * device)
{
  ata_cmd_in in{ATA_CHECK_POWER_MODE};
  in.out_needed.sector_count = true;

  ata_cmd_out out;
  if (!ata_pass_through(device, in, out))
    return -1;

  if (!out.out_regs.sector_count.is_set()) {
    if (ata_debugmode)
      lib_printf("CHECK POWER MODE: incomplete response, ATA output registers missing\n");
    device->set_err(ENOSYS);
    return -1;
  }
  return out.out_regs.sector_count;
}

// Issue a no-data ATA command with optional sector count register value
bool ata_nodata_command(ata_device * device, uint8_t command)
{
  return ata_pass_through(device, ata_cmd_in{command});
}

bool ata_nodata_command(ata_device * device, uint8_t command, uint8_t sector_count)
{
  ata_cmd_in in{command};
  in.in_regs.sector_count = sector_count;
  return ata_pass_through(device, in);
}

// Issue SET FEATURES command with optional sector count register value
bool ata_set_features(ata_device * device, uint8_t features)
{
  return ata_pass_through(device, ata_cmd_in{ATA_SET_FEATURES, features});
}

bool ata_set_features(ata_device * device, uint8_t features, uint8_t sector_count)
{
  ata_cmd_in in{ATA_SET_FEATURES, features};
  in.in_regs.sector_count = sector_count;
  return ata_pass_through(device, in);
}

// Reads current Device Identity info (512 bytes) into ID.  Returns 0
// if all OK.  Returns -1 if no ATA Device identity can be
// established.  Returns >0 if Device is ATA Packet Device (not SMART
// capable).  The value of the integer helps identify the type of
// Packet device, which is useful so that the user can connect the
// formal device number with whatever object is inside their computer.
int ata_read_identity(ata_device * device, ata_identify_device & id,
                      bool fix_swapped_id /* = false */)
{
  // See if device responds either to IDENTIFY DEVICE or IDENTIFY
  // PACKET DEVICE
  bool packet = false;
  ata_cmd_in in{ATA_IDENTIFY_DEVICE};
  in.set_data_in(&id, 1);
  if (!ata_pass_through(device, in)) {
    smart_device::error_info err = device->get_err();

    in.in_regs.command = ATA_IDENTIFY_PACKET_DEVICE;
    if (!ata_pass_through(device, in)) {
      device->set_err(err);
      return -1;
    }
    packet = true;
  }

  if (fix_swapped_id)
    // Fix already swapped serial_no, fw_rev and model
    ata_byteswap_id_strings_inplace(id, false /* !all */);

  // If there is a checksum there, validate it
  const uint8_t * rawbyte = reinterpret_cast<const uint8_t *>(&id);
  if (rawbyte[512-2] == 0xa5 && ata_checksum(rawbyte))
    checksumwarning("Drive Identity Structure");

  // Byteswap strings always
  ata_byteswap_id_strings_inplace(id);
  // if machine is big-endian, swap byte order as needed
  ata_if_be_byteswap_inplace(id);
  
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
  if (!packet && id.general_config == 0x848a)
    return 0;

  // If this is a PACKET DEVICE, return device type
  if (id.general_config & 0x8000)
    return 1 + ((id.general_config >> 8) & 0x1f);
  
  // Not a PACKET DEVICE
  return 0;
}

// Get World Wide Name (WWN) fields.
// Return NAA field or -1 if WWN is unsupported.
// Table 34 of T13/1699-D Revision 6a (ATA8-ACS), September 6, 2008.
// (WWN was introduced in ATA/ATAPI-7 and is mandatory since ATA8-ACS Revision 3b)
int ata_get_wwn(const ata_identify_device & id, uint32_t & oui, uint64_t & unique_id)
{
  // Don't use id.command_set_3 to be compatible with some older ATA-7 disks
  if ((id.cfs_enabled_3 & 0xc100) != 0x4100)
    return -1; // word not valid or WWN support bit 8 not set

  oui = (uint32_t)(id.wwn[0] & 0x0fff) << 12 | id.wwn[1] >> 4;
  unique_id = (uint64_t)(id.wwn[1] & 0x000f) << 32
            | (uint64_t)id.wwn[2] << 16 | id.wwn[3];
  return id.wwn[0] >> 12;
}

// Get nominal media rotation rate.
// Returns: 0 = not reported, 1 = SSD, >1 = HDD rpm, < 0 = -(Unknown value)
int ata_get_rotation_rate(const ata_identify_device & id)
{
  // Table 37 of T13/1699-D (ATA8-ACS) Revision 6a, September 6, 2008
  // Table A.31 of T13/2161-D (ACS-3) Revision 3b, August 25, 2012
  if (id.rotation_rate == 0x0000 || id.rotation_rate == 0xffff)
    return 0;
  else if (id.rotation_rate == 0x0001)
    return 1;
  else if (id.rotation_rate > 0x0400)
    return id.rotation_rate;
  else
    return -(int)id.rotation_rate;
}

// returns 1 if SMART supported, 0 if SMART unsupported, -1 if can't tell
int ata_is_smart_supported(const ata_identify_device & id)
{
  uint16_t word82 = id.command_set_1;
  uint16_t word83 = id.command_set_2;
  
  // check if words 82/83 contain valid info
  if ((word83>>14) == 0x01)
    // return value of SMART support bit 
    return word82 & 0x0001;
  
  // since we can're rely on word 82, we don't know if SMART supported
  return -1;
}

// returns 1 if SMART enabled, 0 if SMART disabled, -1 if can't tell
int ata_is_smart_enabled(const ata_identify_device & id)
{
  uint16_t word85 = id.cfs_enabled_1;
  uint16_t word87 = id.cfs_enabled_3;
  
  // check if words 85/86/87 contain valid info
  if ((word87>>14) == 0x01)
    // return value of SMART enabled bit
    return word85 & 0x0001;
  
  // Since we can't rely word85, we don't know if SMART is enabled.
  return -1;
}

// Reads SMART attributes into DATA
bool ata_read_smart_data(ata_device * device, ata_smart_values & data)
{
  ata_cmd_in in{ATA_SMART_CMD, ATA_SMART_READ_VALUES};
  in.set_data_in(&data, 1);
  if (!ata_pass_through(device, in))
    return false;

  // compute checksum
  if (ata_checksum(&data))
    checksumwarning("SMART Attribute Data Structure");
  
  // swap endian order if needed
  ata_if_be_byteswap_inplace(data);
  return true;
}

// This corrects some quantities that are byte reversed in the SMART
// SELF TEST LOG
static void fix_samsung_selftest_log(ata_smart_selftestlog & log)
{
  // bytes 508/509 (numbered from 0) swapped (swap of self-test index
  // with one byte of reserved.
  std::swap(log.mostrecenttest, log.reserved[0]);

  // LBA low register (here called 'selftestnumber", containing
  // information about the TYPE of the self-test) is byte swapped with
  // Self-test execution status byte.  These are bytes N, N+1 in the
  // entries.
  for (int i = 0; i < 21; i++)
    std::swap(log.selftest_struct[i].selftestnumber, log.selftest_struct[i].selfteststatus);
}

// Reads the Self Test Log (log #6)
bool ata_read_smart_self_test_log(ata_device * device, ata_smart_selftestlog & log,
  firmwarebug_defs firmwarebugs)
{
  // get data from device
  if (!ata_read_smart_log(device, 0x06, &log, 1))
    return false;

  // compute its checksum, and issue a warning if needed
  if (ata_checksum(&log))
    checksumwarning("SMART Self-Test Log Structure");
  
  // fix firmware bugs in self-test log
  if (firmwarebugs.is_set(BUG_SAMSUNG))
    fix_samsung_selftest_log(log);

  // swap endian order if needed
  ata_if_be_byteswap_inplace(log);
  return true;
}

// Print checksum warning for multi sector log
static void check_multi_sector_sum(const void * data, unsigned nsectors, const char * msg)
{
  unsigned errs = 0;
  for (unsigned i = 0; i < nsectors; i++) {
    if (ata_checksum(reinterpret_cast<const uint8_t *>(data) + i * 512))
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
bool ata_read_smart_ext_self_test_log(ata_device * device, ata_smart_extselftestlog * log,
  uint16_t nsectors)
{
  if (!ata_read_log_ext(device, 0x07, 0x00, 0, log, nsectors))
    return false;

  check_multi_sector_sum(log, nsectors, "SMART Extended Self-test Log Structure");

  ata_if_be_byteswap_inplace(log, nsectors);
  return true;
}

// Write GP Log page(s)
bool ata_write_log_ext(ata_device * device, uint8_t logaddr, uint16_t page, const void * log,
  uint16_t nsectors)
{
  ata_cmd_in in{ATA_WRITE_LOG_EXT};
  in.in_regs.lba_low    = logaddr;
  in.in_regs.lba_mid_16 = page;
  in.set_data_out(log, nsectors); // TODO: only supports 8-bit nsectors

  if (!ata_pass_through(device, in)) {
    if (nsectors <= 1) {
      lib_printf("ATA_WRITE_LOG_EXT (addr=0x%02x, page=%u, n=%u) failed: %s\n",
           logaddr, page, nsectors, device->get_errmsg());
      return false;
    }

    // Recurse to retry with single sectors,
    // multi-sector reads may not be supported by ioctl.
    for (unsigned i = 0; i < nsectors; i++) {
      if (!ata_write_log_ext(device, logaddr, page + i,
                             reinterpret_cast<const uint8_t *>(log) + 512 * i, 1))
        return false;
    }
  }

  return true;
}

// Read GP Log page(s)
bool ata_read_log_ext(ata_device * device, uint8_t logaddr, uint8_t features, uint16_t page,
  void * log, uint16_t nsectors)
{
  ata_cmd_in in{ATA_READ_LOG_EXT, features};
  in.in_regs.lba_low    = logaddr;
  in.in_regs.lba_mid_16 = page;
  in.set_data_in_48bit(log, nsectors);
  if (!ata_pass_through(device, in)) {
    if (nsectors <= 1) {
      lib_printf("ATA_READ_LOG_EXT (addr=0x%02x:0x%02x, page=%u, n=%u) failed: %s\n",
           logaddr, features, page, nsectors, device->get_errmsg());
      return false;
    }

    // Recurse to retry with single sectors,
    // multi-sector reads may not be supported by ioctl.
    for (unsigned i = 0; i < nsectors; i++) {
      if (!ata_read_log_ext(device, logaddr, features, page + i,
                            reinterpret_cast<uint8_t *>(log) + 512*i, 1))
        return false;
    }
  }

  return true;
}

// Read SMART Log page(s)
bool ata_read_smart_log(ata_device * device, uint8_t logaddr, void * log, uint8_t nsectors)
{
  ata_cmd_in in{ATA_SMART_CMD, ATA_SMART_READ_LOG_SECTOR};
  in.in_regs.lba_low = logaddr;
  in.set_data_in(log, nsectors);
  return ata_pass_through(device, in);
}

// Write SMART Log page(s).
bool ata_write_smart_log(ata_device * device, uint8_t logaddr, const void * log, uint8_t nsectors)
{
  ata_cmd_in in{ATA_SMART_CMD, ATA_SMART_WRITE_LOG_SECTOR};
  in.in_regs.lba_low = logaddr;
  in.set_data_out(log, nsectors);
  return ata_pass_through(device, in);
}

// Reads the SMART or GPL Log Directory (log #0)
bool ata_read_log_directory(ata_device * device, ata_smart_log_directory & log, bool gpl)
{
  if (!gpl) { // SMART Log directory
    if (!ata_read_smart_log(device, 0x00, &log, 1))
      return false;
  }
  else { // GP Log directory
    if (!ata_read_log_ext(device, 0x00, 0x00, 0, &log, 1))
      return false;
  }

  // swap endian order if needed
  ata_if_be_byteswap_inplace(log);
  return true;
}

// Reads the selective self-test log (log #9)
bool ata_read_smart_selective_self_test_log(ata_device * device,
  ata_selective_self_test_log & log)
{
  // get data from device
  if (!ata_read_smart_log(device, 0x09, &log, 1))
    return false;
   
  // compute its checksum, and issue a warning if needed
  if (ata_checksum(&log))
    checksumwarning("SMART Selective Self-Test Log Structure");
  
  // swap endian order if needed
  ata_if_be_byteswap_inplace(log);
  return true;
}

// Read/write selective self-test log to prepare a selective self-test.
// Return 1 on success, 0 if a test is already running or  -1 on error.
int ata_prepare_selective_self_test(ata_device * device, ata_selective_selftest_args & args,
  const ata_smart_values & sv, uint64_t num_sectors,
  const ata_selective_selftest_args * prev_args /* = nullptr */)
{
  // Disk size must be known
  if (!num_sectors) {
    lib_printf("Disk size is unknown, unable to check selective self-test spans\n");
    return -1;
  }

  // Read log
  struct ata_selective_self_test_log sstlog, *data=&sstlog;
  unsigned char *ptr=(unsigned char *)data;
  if (!ata_read_smart_selective_self_test_log(device, sstlog)) {
    lib_printf("SMART Read Selective Self-test Log failed: %s\n", device->get_errmsg());
    lib_printf("Since Read failed, will not attempt to WRITE Selective Self-test Log\n");
    return -1;
  }
  
  // Set log version
  data->logversion = 1;

  // Host is NOT allowed to write selective self-test log if a selective
  // self-test is in progress.
  if (   0 < data->currentspan && data->currentspan < 6
      && (sv.self_test_exec_status >> 4) == 0xf) {
    lib_printf("SMART Selective or other Self-test in progress\n");
    return 0;
  }

  // Set start/end values based on old spans for special -t select,... options
  int i;
  for (i = 0; i < args.num_spans; i++) {
    int mode = args.span[i].mode;
    uint64_t start = args.span[i].start;
    uint64_t end   = args.span[i].end;
    if (mode == SEL_CONT) {// redo or next depending on last test status
      switch (sv.self_test_exec_status >> 4) {
        case 1: case 2: // Aborted/Interrupted by host
          lib_printf("Continue Selective Self-Test: Redo last span\n");
          mode = SEL_REDO;
          break;
        default: // All others
          lib_printf("Continue Selective Self-Test: Start next span\n");
          mode = SEL_NEXT;
          break;
      }
    }

    if (   (mode == SEL_REDO || mode == SEL_NEXT)
        && prev_args && i < prev_args->num_spans
        && !uile64_to_uint(data->span[i].start)
        && !uile64_to_uint(data->span[i].end)   ) {
      // Some drives do not preserve the selective self-test log across
      // power-cyles.  If old span on drive is cleared use span provided
      // by caller.  This is used by smartd (first span only).
      data->span[i].start = uint_to_uile64(prev_args->span[i].start);
      data->span[i].end   = uint_to_uile64(prev_args->span[i].end);
    }

    switch (mode) {
      case SEL_RANGE: // -t select,START-END
        break;
      case SEL_REDO: // -t select,redo... => Redo current
        start = uile64_to_uint(data->span[i].start);
        if (end > 0) { // -t select,redo+SIZE
          end--; end += start; // [oldstart, oldstart+SIZE)
        }
        else // -t select,redo
          end = uile64_to_uint(data->span[i].end); // [oldstart, oldend]
        break;
      case SEL_NEXT: // -t select,next... => Do next
        if (uile64_to_uint(data->span[i].end) == 0) {
          start = end = 0; break; // skip empty spans
        }
        start = uile64_to_uint(data->span[i].end) + 1;
        if (start >= num_sectors)
          start = 0; // wrap around
        if (end > 0) { // -t select,next+SIZE
          end--; end += start; // (oldend, oldend+SIZE]
        }
        else { // -t select,next
          uint64_t oldsize =   uile64_to_uint(data->span[i].end)
                             - uile64_to_uint(data->span[i].start) + 1;
          end = start + oldsize - 1; // (oldend, oldend+oldsize]
          if (end >= num_sectors) {
            // Adjust size to allow round-robin testing without future size decrease
            uint64_t spans = (num_sectors + oldsize-1) / oldsize;
            uint64_t newsize = (num_sectors + spans-1) / spans;
            uint64_t newstart = num_sectors - newsize, newend = num_sectors - 1;
            lib_printf("Span %d changed from %" PRIu64 "-%" PRIu64 " (%" PRIu64 " sectors)\n",
                 i, start, end, oldsize);
            lib_printf("                 to %" PRIu64 "-%" PRIu64 " (%" PRIu64 " sectors) (%" PRIu64 " spans)\n",
                 newstart, newend, newsize, spans);
            start = newstart; end = newend;
          }
        }
        break;
      default:
        lib_printf("ataWriteSelectiveSelfTestLog: Invalid mode %d\n", mode);
        return -1;
    }
    // Range check
    if (start < num_sectors && num_sectors <= end) {
      if (end != ~(uint64_t)0) // -t select,N-max
        lib_printf("Size of self-test span %d decreased according to disk size\n", i);
      end = num_sectors - 1;
    }
    if (!(start <= end && end < num_sectors)) {
      lib_printf("Invalid selective self-test span %d: %" PRIu64 "-%" PRIu64 " (%" PRIu64 " sectors)\n",
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
    data->span[i].start = uint_to_uile64(args.span[i].start);
    data->span[i].end   = uint_to_uile64(args.span[i].end);
  }

  // host must initialize to zero before initiating selective self-test
  data->currentlba = {};
  data->currentspan = 0;
  
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
  ata_if_be_byteswap_inplace(*data);

  // write new selective self-test log
  if (!ata_write_smart_log(device, 0x09, data, 1)) {
    lib_printf("Write Selective Self-test Log failed: %s\n", device->get_errmsg());
    return -1;
  }
  return 1;
}

// This corrects some quantities that are byte reversed in the SMART
// ATA ERROR LOG.
static void fix_samsung_error_log(ata_smart_errorlog & log)
{
  // FIXED IN SAMSUNG -25 FIRMWARE???
  // Device error count in bytes 452-3
  byteswap_inplace(log.ata_error_count);
  
  // FIXED IN SAMSUNG -22a FIRMWARE
  // step through 5 error log data structures
  for (int i = 0; i < 5; i++){
    // Error data structure two-byte hour life timestamp.  These are
    // bytes (N+28, N+29).
    byteswap_inplace(log.errorlog_struct[i].error_struct.timestamp);
  }
}

// NEEDED ONLY FOR SAMSUNG -22 (some) -23 AND -24?? FIRMWARE
static void fix_samsung_error_log_2(ata_smart_errorlog & log)
{
  // Device error count in bytes 452-3
  byteswap_inplace(log.ata_error_count);
}

// Reads the Summary SMART Error Log (log #1). The Comprehensive SMART
// Error Log is #2, and the Extended Comprehensive SMART Error log is
// #3
bool ata_read_smart_error_log(ata_device * device, ata_smart_errorlog & log,
  firmwarebug_defs firmwarebugs)
{
  // get data from device
  if (!ata_read_smart_log(device, 0x01, &log, 1))
    return false;
  
  // compute its checksum, and issue a warning if needed
  if (ata_checksum(&log))
    checksumwarning("SMART ATA Error Log Structure");
  
  // Some disks have the byte order reversed in some SMART Summary
  // Error log entries
  if (firmwarebugs.is_set(BUG_SAMSUNG))
    fix_samsung_error_log(log);
  else if (firmwarebugs.is_set(BUG_SAMSUNG2))
    fix_samsung_error_log_2(log);

  // swap endian order if needed
  ata_if_be_byteswap_inplace(log);
  return true;
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
bool ata_read_smart_ext_comp_error_log(ata_device * device, ata_smart_exterrlog * log,
  uint16_t page, uint16_t nsectors, firmwarebug_defs firmwarebugs)
{
  if (!ata_read_log_ext(device, 0x03, 0x00, page, log, nsectors))
    return false;

  check_multi_sector_sum(log, nsectors, "SMART Extended Comprehensive Error Log Structure");

  ata_if_be_byteswap_inplace(log, nsectors);

  if (firmwarebugs.is_set(BUG_XERRORLBA))
    fix_exterrlog_lba(log, nsectors);
  return true;
}

bool ata_read_smart_thresholds(ata_device * device, ata_smart_thresholds_pvt & thr)
{
  ata_cmd_in in{ATA_SMART_CMD, ATA_SMART_READ_THRESHOLDS};
  in.set_data_in(&thr, 1);
  if (!ata_pass_through(device, in))
    return false;
  
  if (ata_checksum(&thr))
    checksumwarning("SMART Attribute Thresholds Structure");
  
  // swap endian order if needed
  ata_if_be_byteswap_inplace(thr);
  return true;
}

bool ata_enable_smart(ata_device * device, bool enable /* = true */)
{
  return ata_pass_through(device,
    ata_cmd_in{ATA_SMART_CMD, (uint8_t)(enable ? ATA_SMART_ENABLE : ATA_SMART_DISABLE)});
}

bool ata_enable_smart_auto_save(ata_device * device, bool enable /* = true */)
{
  ata_cmd_in in{ATA_SMART_CMD, ATA_SMART_AUTOSAVE};
  in.in_regs.sector_count = (enable ? 0xf1 : 0x00); // Caution: Non-DATA command!
  return ata_pass_through(device, in);
}

// In *ALL* ATA standards the Enable/Disable AutoOffline command is
// marked "OBSOLETE". It is defined in SFF-8035i Revision 2, and most
// vendors still support it for backwards compatibility. IBM documents
// it for some drives.
// Timer is hardcoded to 4 hours.
bool ata_enable_smart_auto_offline(ata_device * device, bool enable /* = true */)
{
  ata_cmd_in in{ATA_SMART_CMD, ATA_SMART_AUTO_OFFLINE};
  in.in_regs.sector_count = (enable ? 0xf8 : 0x00); // Caution: Non-DATA command!
  return ata_pass_through(device, in);
}

// If SMART is enabled, supported, and working, then this call is
// guaranteed to return 1, else zero.  Note that it should return 1
// regardless of whether the disk's SMART status is 'healthy' or
// 'failing'.
bool ata_is_smart_status_working(ata_device * device)
{
  return ata_pass_through(device, ata_cmd_in{ATA_SMART_CMD, ATA_SMART_STATUS});
}

// Issue SMART STATUS command and check the result.
// Return 0 if "good" status, 1 if "failed" status and -1 on error.
int ata_get_smart_status(ata_device * device)
{
  ata_cmd_in in{ATA_SMART_CMD, ATA_SMART_STATUS};
  in.out_needed.lba_high = in.out_needed.lba_mid = true; // Status returned here

  ata_cmd_out out;
  if (!ata_pass_through(device, in, out))
    return -1;

  // Cyl low and Cyl high unchanged means "Good SMART status"
  if (out.out_regs.lba_high == ATA_SMART_CMD_LBA_HIGH && out.out_regs.lba_mid == ATA_SMART_CMD_LBA_MID)
    return 0;
  // These values mean "Bad SMART status"
  if (   out.out_regs.lba_high == ATA_SMART_FAILED_LBA_HIGH
      && out.out_regs.lba_mid == ATA_SMART_FAILED_LBA_MID)
    return 1;
  if (out.out_regs.lba_mid == ATA_SMART_CMD_LBA_MID) {
    if (ata_debugmode)
      lib_printf("SMART STATUS RETURN: half healthy response sequence, "
                 "probable SAT/USB truncation\n");
    return 0;
  }
  if (out.out_regs.lba_mid == ATA_SMART_FAILED_LBA_MID) {
    if (ata_debugmode)
      lib_printf("SMART STATUS RETURN: half unhealthy response sequence, "
                 "probable SAT/USB truncation\n");
    return 1;
  }
  if (!out.out_regs.is_set()) {
    device->set_err(ENOSYS, "Incomplete response, ATA output registers missing");
    return -1;
  }
  device->set_err(ENOSYS, "Invalid ATA output register values: LBA_HI=0x%02x, LBA_MID=0x%02x",
                  out.out_regs.lba_high.val(), out.out_regs.lba_mid.val());
  return -1;
}

bool ata_smart_self_test(ata_device * device, uint8_t testtype)
{
  ata_cmd_in in{ATA_SMART_CMD, ATA_SMART_IMMEDIATE_OFFLINE};
  in.in_regs.lba_low = testtype;
  return ata_pass_through(device, in);
}

// Return estimated time (minimum polling interval in minutes) for a self-test of type TESTTYPE.
int ata_get_smart_self_test_minutes(const ata_smart_values & data, uint8_t testtype)
{
  switch (testtype){
  case OFFLINE_FULL_SCAN:
    return data.total_time_to_complete_off_line;
  case SHORT_SELF_TEST:
  case SHORT_CAPTIVE_SELF_TEST:
    return data.short_test_completion_time;
  case EXTEND_SELF_TEST:
  case EXTEND_CAPTIVE_SELF_TEST:
    {
      uint16_t extend_test_completion_time_w = uile16_to_uint(data.extend_test_completion_time_w);
      if (data.extend_test_completion_time_b == 0xff
          && extend_test_completion_time_w != 0x0000
          && extend_test_completion_time_w != 0xffff)
        return extend_test_completion_time_w; // ATA-8
      else
        return data.extend_test_completion_time_b;
    }
  case CONVEYANCE_SELF_TEST:
  case CONVEYANCE_CAPTIVE_SELF_TEST:
    return data.conveyance_test_completion_time;
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
bool ata_is_smart_error_log_capable(const ata_smart_values & data, const ata_identify_device & id)
{
  uint16_t word84 = id.command_set_3;
  uint16_t word87 = id.cfs_enabled_3;
  int isata6 = id.major_rev_num & (0x01 << 6);
  int isata7 = id.major_rev_num & (0x01 << 7);

  if ((isata6 || isata7) && (word84>>14) == 0x01 && (word84 & 0x01))
    return true;

  if ((isata6 || isata7) && (word87>>14) == 0x01 && (word87 & 0x01))
    return true;

  // otherwise we'll use the poorly documented capability bit
  return !!(data.errorlog_capability & 0x01);
}

// See previous function.  If the error log exists then the self-test
// log should (must?) also exist.
bool ata_is_smart_self_test_log_capable(const ata_smart_values & data, const ata_identify_device & id)
{
  uint16_t word84 = id.command_set_3;
  uint16_t word87 = id.cfs_enabled_3;
  int isata6 = id.major_rev_num & (0x01 << 6);
  int isata7 = id.major_rev_num & (0x01 << 7);

  if ((isata6 || isata7) && (word84>>14) == 0x01 && (word84 & 0x02))
    return true;

  if ((isata6 || isata7) && (word87>>14) == 0x01 && (word87 & 0x02))
    return true;

  // otherwise we'll use the poorly documented capability bit
  return !!(data.errorlog_capability & 0x01);
}

bool ata_is_gp_log_capable(const ata_identify_device & id)
{
  uint16_t word84 = id.command_set_3;
  uint16_t word87 = id.cfs_enabled_3;

  // If bit 14 of word 84 is set to one and bit 15 of word 84 is
  // cleared to zero, the contents of word 84 contains valid support
  // information. If not, support information is not valid in this
  // word.
  if ((word84>>14) == 0x01)
    // If bit 5 of word 84 is set to one, the device supports the
    // General Purpose Logging feature set.
    return !!(word84 & (0x01 << 5));
  
  // If bit 14 of word 87 is set to one and bit 15 of word 87 is
  // cleared to zero, the contents of words (87:85) contain valid
  // information. If not, information is not valid in these words.  
  if ((word87>>14) == 0x01)
    // If bit 5 of word 87 is set to one, the device supports
    // the General Purpose Logging feature set.
    return !!(word87 & (0x01 << 5));

  // not capable
  return false;
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

// Get attribute raw value.
uint64_t ata_get_attr_raw_value(const ata_smart_attribute & attr,
                                const ata_vendor_attr_defs & defs)
{
  const ata_vendor_attr_defs::entry & def = defs[attr.id];
  // TODO: Allow Byteorder in DEFAULT entry

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
  if (format == RAWFMT_DEFAULT) {
     // Get format from DEFAULT entry
     format = get_default_attr_defs()[attr.id].raw_format;
     if (format == RAWFMT_DEFAULT)
       // Unknown Attribute
       format = RAWFMT_RAW48;
  }

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

// Get attribute name
std::string ata_get_smart_attr_name(unsigned char id, const ata_vendor_attr_defs & defs,
                                    int rpm /* = 0 */)
{
  if (!defs[id].name.empty())
    return defs[id].name;
  else {
     const ata_vendor_attr_defs::entry & def = get_default_attr_defs()[id];
     if (def.name.empty())
       return "Unknown_Attribute";
     else if ((def.flags & ATTRFLAG_HDD_ONLY) && rpm == 1)
       return "Unknown_SSD_Attribute";
     else if ((def.flags & ATTRFLAG_SSD_ONLY) && rpm > 1)
       return "Unknown_HDD_Attribute";
     else
       return def.name;
  }
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
bool ata_read_sct_status(ata_device * device, ata_sct_status_response & sts)
{
  // read SCT status via SMART log 0xe0
  sts = {};
  if (!ata_read_smart_log(device, 0xe0, &sts, 1)) {
    lib_printf("Read SCT Status failed: %s\n", device->get_errmsg());
    return false;
  }

  // swap endian order if needed
  ata_if_be_byteswap_inplace(sts);

  // Check format version
  if (!(sts.format_version == 2 || sts.format_version == 3)) {
    lib_printf("Unknown SCT Status format version %u, should be 2 or 3.\n", sts.format_version);
    return false;
  }
  return true;
}

// Read SCT Temperature History Table
bool ata_read_sct_temperature_history(ata_device * device, ata_sct_temperature_history_table & tmh,
  ata_sct_status_response & sts)
{
  // Initial SCT status must be provided by caller

  // Do nothing if other SCT command is executing
  if (sts.ext_status_code == 0xffff) {
    lib_printf("Another SCT command is executing, abort Read Data Table\n"
               "(SCT ext_status_code 0x%04x, action_code=%u, function_code=%u)\n",
      sts.ext_status_code, sts.action_code, sts.function_code);
    return false;
  }

  ata_sct_data_table_command cmd; memset(&cmd, 0, sizeof(cmd));
  // CAUTION: DO NOT CHANGE THIS VALUE (SOME ACTION CODES MAY ERASE DISK)
  cmd.action_code   = 5; // Data table command
  cmd.function_code = 1; // Read table
  cmd.table_id      = 2; // Temperature History Table

  // swap endian order if needed
  ata_if_be_byteswap_inplace(cmd);

  // write command via SMART log page 0xe0
  if (!ata_write_smart_log(device, 0xe0, &cmd, 1)) {
    lib_printf("Write SCT Data Table failed: %s\n", device->get_errmsg());
    return false;
  }

  // read SCT data via SMART log page 0xe1
  tmh = {};
  if (!ata_read_smart_log(device, 0xe1, &tmh, 1)) {
    lib_printf("Read SCT Data Table failed: %s\n", device->get_errmsg());
    return false;
  }

  // re-read and check SCT status
  if (!ata_read_sct_status(device, sts))
    return false;

  if (!(sts.ext_status_code == 0 && sts.action_code == 5 && sts.function_code == 1)) {
    lib_printf("Unexpected SCT status 0x%04x (action_code=%u, function_code=%u)\n",
      sts.ext_status_code, sts.action_code, sts.function_code);
    return false;
  }

  // swap endian order if needed
  ata_if_be_byteswap_inplace(tmh);
  return true;
}

// Common function for Get/Set SCT Feature Control:
// Write Cache, Write Cache Reordering, etc.
static int ata_get_set_sct_feature_control(ata_device * device, uint16_t feature_code,
  uint16_t state, bool persistent, bool set)
{
  // Check initial status
  ata_sct_status_response sts;
  if (!ata_read_sct_status(device, sts))
    return -1;

  // Do nothing if other SCT command is executing
  if (sts.ext_status_code == 0xffff) {
    lib_printf("Another SCT command is executing, abort Feature Control\n"
               "(SCT ext_status_code 0x%04x, action_code=%u, function_code=%u)\n",
      sts.ext_status_code, sts.action_code, sts.function_code);
    return -1;
  }

  ata_sct_feature_control_command cmd; memset(&cmd, 0, sizeof(cmd));
  // CAUTION: DO NOT CHANGE THIS VALUE (SOME ACTION CODES MAY ERASE DISK)
  cmd.action_code   = 4; // Feature Control command
  cmd.function_code  = (set ? 1 : 2); // 1=Set, 2=Get
  cmd.feature_code  = feature_code;
  cmd.state         = state;
  cmd.option_flags  = (persistent ? 0x01 : 0x00);

  // swap endian order if needed
  ata_if_be_byteswap_inplace(cmd);

  // write command via SMART log page 0xe0
  ata_cmd_in in{ATA_SMART_CMD, ATA_SMART_WRITE_LOG_SECTOR};
  in.in_regs.lba_low = 0xe0;
  in.set_data_out(&cmd, 1);

  if (!set)
    // Time limit returned in ATA registers
    in.out_needed.sector_count = in.out_needed.lba_low = true;

  ata_cmd_out out;
  if (!ata_pass_through(device, in, out)) {
    lib_printf("Write SCT (%cet) Feature Control Command failed: %s\n",
      (!set ? 'G' : 'S'), device->get_errmsg());
    return -1;
  }
  state = out.out_regs.sector_count | (out.out_regs.lba_low << 8);

  // re-read and check SCT status
  if (!ata_read_sct_status(device, sts))
    return -1;

  if (!(sts.ext_status_code == 0 && sts.action_code == 4 && sts.function_code == (set ? 1 : 2))) {
    lib_printf("Unexpected SCT status 0x%04x (action_code=%u, function_code=%u)\n",
      sts.ext_status_code, sts.action_code, sts.function_code);
    return -1;
  }
  return state;
}

// Get/Set Write Cache Reordering
int ata_get_set_sct_write_cache_reordering(ata_device * device, bool enable, bool persistent,
  bool set)
{
  return ata_get_set_sct_feature_control(device, 2 /* Enable/Disable Write Cache Reordering */,
                                         (enable ? 1 : 2), persistent, set);
}

// Get/Set Write Cache (force enable, force disable)
int ata_get_set_sct_write_cache(ata_device * device, uint16_t state, bool persistent, bool set)
{
  return ata_get_set_sct_feature_control(device, 1 /* Enable/Disable Write Cache */,
                                         state, persistent, set);
}

// Set SCT Temperature Logging Interval
bool ata_set_sct_temperature_interval(ata_device * device, uint16_t interval, bool persistent)
{
  // Check initial status
  ata_sct_status_response sts;
  if (!ata_read_sct_status(device, sts))
    return false;

  // Do nothing if other SCT command is executing
  if (sts.ext_status_code == 0xffff) {
    lib_printf("Another SCT command is executing, abort Feature Control\n"
               "(SCT ext_status_code 0x%04x, action_code=%u, function_code=%u)\n",
      sts.ext_status_code, sts.action_code, sts.function_code);
    return false;
  }

  ata_sct_feature_control_command cmd; memset(&cmd, 0, sizeof(cmd));
  // CAUTION: DO NOT CHANGE THIS VALUE (SOME ACTION CODES MAY ERASE DISK)
  cmd.action_code   = 4; // Feature Control command
  cmd.function_code = 1; // Set state
  cmd.feature_code  = 3; // Temperature logging interval
  cmd.state         = interval;
  cmd.option_flags  = (persistent ? 0x01 : 0x00);

  // swap endian order if needed
  ata_if_be_byteswap_inplace(cmd);

  // write command via SMART log page 0xe0
  if (!ata_write_smart_log(device, 0xe0, &cmd, 1)){
    lib_printf("Write SCT Feature Control Command failed: %s\n", device->get_errmsg());
    return false;
  }

  // re-read and check SCT status
  if (!ata_read_sct_status(device, sts))
    return false;

  if (!(sts.ext_status_code == 0 && sts.action_code == 4 && sts.function_code == 1)) {
    lib_printf("Unexpected SCT status 0x%04x (action_code=%u, function_code=%u)\n",
      sts.ext_status_code, sts.action_code, sts.function_code);
    return false;
  }
  return true;
}

// Get/Set SCT Error Recovery Control
static bool ata_get_set_sct_erc_time(ata_device * device, uint16_t type, bool set,
  uint16_t & time_limit, bool power_on, bool mfg_default)
{
  // Check initial status
  ata_sct_status_response sts;
  if (!ata_read_sct_status(device, sts))
    return false;

  // Do nothing if other SCT command is executing
  if (sts.ext_status_code == 0xffff) {
    lib_printf("Another SCT command is executing, abort Error Recovery Control\n"
               "(SCT ext_status_code 0x%04x, action_code=%u, function_code=%u)\n",
      sts.ext_status_code, sts.action_code, sts.function_code);
    return false;
  }

  ata_sct_error_recovery_control_command cmd; memset(&cmd, 0, sizeof(cmd));
  // CAUTION: DO NOT CHANGE THIS VALUE (SOME ACTION CODES MAY ERASE DISK)
  cmd.action_code    = 3; // Error Recovery Control command

  // 1=Set timer, 2=Get timer, 3=Set Power-on timer, 4=Get Power-on timer, 5=Restore mfg default
  if (mfg_default) {
    cmd.function_code = 5;
  } else if (power_on) {
    cmd.function_code = (set ? 3 : 4);
  } else {
    cmd.function_code = (set ? 1 : 2);
  }
  unsigned short saved_function_code = cmd.function_code;

  cmd.selection_code = type; // 1=Read timer, 2=Write timer
  if (set)
    cmd.time_limit   = time_limit;

  // swap endian order if needed
  ata_if_be_byteswap_inplace(cmd);

  // write command via SMART log page 0xe0
  ata_cmd_in in{ATA_SMART_CMD, ATA_SMART_WRITE_LOG_SECTOR};
  in.in_regs.lba_low = 0xe0;
  in.set_data_out(&cmd, 1);

  if (!set)
    // Time limit returned in ATA registers
    in.out_needed.sector_count = in.out_needed.lba_low = true;

  ata_cmd_out out;
  if (!ata_pass_through(device, in, out)) {
    lib_printf("Write SCT (%cet) Error Recovery Control Command failed: %s\n",
      (!set ? 'G' : 'S'), device->get_errmsg());
    return false;
  }

  // re-read and check SCT status
  if (!ata_read_sct_status(device, sts))
    return false;

  if (!(sts.ext_status_code == 0 && sts.action_code == 3 && sts.function_code == saved_function_code)) {
    lib_printf("Unexpected SCT status 0x%04x (action_code=%u, function_code=%u)\n",
      sts.ext_status_code, sts.action_code, sts.function_code);
    return false;
  }

  if (!set) {
    // Check whether registers are properly returned by ioctl()
    if (!(out.out_regs.sector_count.is_set() && out.out_regs.lba_low.is_set())) {
      // TODO: Output register support should be checked within each ata_pass_through()
      // implementation before command is issued.
      lib_printf("SMART WRITE LOG does not return COUNT and LBA_LOW register\n");
      return false;
    }
    if (   out.out_regs.sector_count == in.in_regs.sector_count
        && out.out_regs.lba_low      == in.in_regs.lba_low     ) {
      // 0xe001 (5734.5s) - this is most likely a broken ATA pass-through implementation
      lib_printf("SMART WRITE LOG returns COUNT and LBA_LOW register unchanged\n");
      return false;
    }

    // Return value to caller
    time_limit = out.out_regs.sector_count | (out.out_regs.lba_low << 8);
  }

  return true;
}

// Get SCT Error Recovery Control
bool ata_get_sct_erc_time(ata_device * device, uint16_t type, uint16_t & time_limit,
  bool power_on)
{
  return ata_get_set_sct_erc_time(device, type, false/*get*/, time_limit, power_on, false);
}

// Set SCT Error Recovery Control
bool ata_set_sct_erc_time(ata_device * device, uint16_t type, uint16_t time_limit,
  bool power_on, bool mfg_default)
{
  return ata_get_set_sct_erc_time(device, type, true/*set*/, time_limit, power_on, mfg_default);
}

// Byteswap strings in identify_device data.
void ata_byteswap_id_strings_inplace(ata_identify_device & id, bool all /* = true */)
{
  byteswap_array_16_inplace(id.serial_no);
  byteswap_array_16_inplace(id.fw_rev);
  byteswap_array_16_inplace(id.model);
  if (!all)
    return;
  byteswap_array_16_inplace(id.add_product_id);
  byteswap_array_16_inplace(id.media_serial_no);
}

// Byteswap all aligned integers on Big Endian platforms, otherwise do nothing.
void ata_if_be_byteswap_inplace(ata_identify_device & id)
{
  if /*constexpr*/(!byteorder_is_big_endian)
    return;

  byteswap_inplace(id.general_config);
  byteswap_inplace(id.obsolete_001);
  byteswap_inplace(id.specific_config);
  byteswap_array_inplace(id.obsolete_003_006);
  byteswap_array_inplace(id.reserved_007_008_cfa);
  byteswap_inplace(id.obsolete_009);
  // serial_no: ata_byteswap_id_strings_inplace()
  byteswap_array_inplace(id.obsolete_020_022);
  // fw_rev:    ata_byteswap_id_strings_inplace()
  // model:     ata_byteswap_id_strings_inplace()
  byteswap_inplace(id.rd_wr_multi_support);
  byteswap_inplace(id.tc_feature_set_options);
  byteswap_inplace(id.capabilities_1);
  byteswap_inplace(id.capabilities_2);
  byteswap_array_inplace(id.obsolete_051_052);
  byteswap_inplace(id.field_validity);
  byteswap_array_inplace(id.obsolete_054_058);
  byteswap_inplace(id.sanitize_rd_wr_multi);
  byteswap_inplace(id.user_sectors_28);
  byteswap_inplace(id.obsolete_062);
  byteswap_inplace(id.dma_multi_modes);
  byteswap_inplace(id.pio_modes);
  byteswap_inplace(id.dma_multi_cycle_min_ns);
  byteswap_inplace(id.dma_multi_cycle_rec_ns);
  byteswap_inplace(id.pio_cycle_no_fl_min_ns);
  byteswap_inplace(id.pio_cycle_iordy_min_ns);
  byteswap_inplace(id.additional_support);
  byteswap_inplace(id.reserved_070);
  byteswap_array_inplace(id.reserved_071_074_atapi);
  byteswap_inplace(id.queue_depth);
  byteswap_inplace(id.sata_capabilities_1);
  byteswap_inplace(id.sata_capabilities_2);
  byteswap_inplace(id.sata_features_supported);
  byteswap_inplace(id.sata_features_enabled);
  byteswap_inplace(id.minor_rev_num);
  byteswap_inplace(id.major_rev_num);
  byteswap_inplace(id.command_set_1);
  byteswap_inplace(id.command_set_2);
  byteswap_inplace(id.command_set_3);
  byteswap_inplace(id.cfs_enabled_1);
  byteswap_inplace(id.cfs_enabled_2);
  byteswap_inplace(id.cfs_enabled_3);
  byteswap_inplace(id.udma_modes);
  byteswap_inplace(id.sec_erase_unit_time);
  byteswap_inplace(id.sec_enh_erase_unit_time);
  byteswap_inplace(id.apm_level);
  byteswap_inplace(id.master_password_id);
  byteswap_inplace(id.pata_hw_reset_result);
  byteswap_inplace(id.aam_level);
  byteswap_inplace(id.strm_min_req_size);
  byteswap_inplace(id.strm_trnfr_time_dma);
  byteswap_inplace(id.strm_acc_latency);
  byteswap_inplace(id.strm_perf_granularity);
  byteswap_inplace(id.strm_trnfr_time_pio);
  byteswap_inplace(id.ds_mgmt_range_max_blks);
  byteswap_inplace(id.phy_log_sector_size);
  byteswap_inplace(id.iso7779_seek_delay);
  byteswap_array_inplace(id.wwn);
  byteswap_array_inplace(id.reserved_112_115);
  byteswap_inplace(id.reserved_116_tlc);
  byteswap_inplace(id.command_set_4);
  byteswap_inplace(id.cfs_enabled_4);
  byteswap_array_inplace(id.reserved_121_124);
  byteswap_array_inplace(id.reserved_125_126_atapi);
  byteswap_inplace(id.rm_media_status);
  byteswap_inplace(id.security_status);
  byteswap_array_inplace(id.vendor_129_159);
  byteswap_inplace(id.cfa_power_mode);
  byteswap_array_inplace(id.reserved_161_167_cfa);
  byteswap_inplace(id.form_factor);
  byteswap_inplace(id.dataset_management);
  // add_product_id: ata_byteswap_id_strings_inplace()
  byteswap_array_inplace(id.reserved_174_175);
  byteswap_inplace(id.sct_capabilities);
  byteswap_array_inplace(id.reserved_207_208);
  byteswap_inplace(id.log_sector_align);
  byteswap_inplace(id.wr_rd_vr_count_mode_3);
  byteswap_inplace(id.wr_rd_vr_count_mode_2);
  byteswap_inplace(id.nv_cache_capabilities);
  byteswap_inplace(id.rotation_rate);
  byteswap_inplace(id.reserved_218);
  byteswap_inplace(id.nv_cache_options);
  byteswap_inplace(id.write_read_verify_mode);
  byteswap_inplace(id.reserved_221);
  byteswap_inplace(id.transport_maj_version);
  byteswap_inplace(id.transport_min_version);
  byteswap_inplace(id.dl_mcode_3_min_blocks);
  byteswap_inplace(id.dl_mcode_3_max_blocks);
  byteswap_array_inplace(id.reserved_224_229);
  byteswap_array_inplace(id.reserved_236_254);
}

void ata_if_be_byteswap_inplace(ata_smart_values & val)
{
  if /*constexpr*/(!byteorder_is_big_endian)
    return;

  byteswap_inplace(val.revnumber);
  byteswap_inplace(val.total_time_to_complete_off_line);
  byteswap_inplace(val.smart_capability);
}

void ata_if_be_byteswap_inplace(ata_smart_thresholds_pvt & thr)
{
  if /*constexpr*/(!byteorder_is_big_endian)
    return;

  byteswap_inplace(thr.revnumber);
}

void ata_if_be_byteswap_inplace(ata_smart_log_directory & log)
{
  if /*constexpr*/(!byteorder_is_big_endian)
    return;

  byteswap_inplace(log.logversion);
}

void ata_if_be_byteswap_inplace(ata_smart_errorlog & log)
{
  if /*constexpr*/(!byteorder_is_big_endian)
    return;

  byteswap_inplace(log.ata_error_count);
  for (int i = 0; i < 5; i++)
    byteswap_inplace(log.errorlog_struct[i].error_struct.timestamp);
}

void ata_if_be_byteswap_inplace(ata_smart_selftestlog & log)
{
  if /*constexpr*/(!byteorder_is_big_endian)
    return;

  byteswap_inplace(log.revnumber);
  for (int i = 0; i < 21; i++)
    byteswap_inplace(log.selftest_struct[i].timestamp);
}


void ata_if_be_byteswap_inplace(ata_smart_exterrlog * log, unsigned num_sectors)
{
  if /*constexpr*/(!byteorder_is_big_endian)
    return;

  byteswap_inplace(log->device_error_count);
  byteswap_inplace(log->error_log_index);
  for (unsigned i = 0; i < num_sectors; i++) {
    for (unsigned j = 0; j < 4; j++)
      byteswap_inplace(log[i].error_logs[j].error.timestamp);
  }
}

void ata_if_be_byteswap_inplace(ata_smart_extselftestlog * log, unsigned num_sectors)
{
  if /*constexpr*/(!byteorder_is_big_endian)
    return;

  for (unsigned i = 0; i < num_sectors; i++) {
    byteswap_inplace(log[i].log_desc_index);
    for (unsigned j = 0; j < 19; j++)
      byteswap_inplace(log[i].log_descs[j].timestamp);
  }
}

void ata_if_be_byteswap_inplace(ata_selective_self_test_log & log)
{
  if /*constexpr*/(!byteorder_is_big_endian)
    return;

  byteswap_inplace(log.logversion);
  byteswap_inplace(log.currentspan);
  byteswap_inplace(log.flags);
  byteswap_inplace(log.pendingtime);
}

void ata_if_be_byteswap_inplace(ata_sct_status_response & sts)
{
  if /*constexpr*/(!byteorder_is_big_endian)
    return;

  byteswap_inplace(sts.format_version);
  byteswap_inplace(sts.sct_version);
  byteswap_inplace(sts.sct_spec);
  byteswap_inplace(sts.ext_status_code);
  byteswap_inplace(sts.action_code);
  byteswap_inplace(sts.function_code);
  byteswap_inplace(sts.smart_status);
  byteswap_inplace(sts.min_erc_time);
}

void ata_if_be_byteswap_inplace(ata_sct_data_table_command & cmd)
{
  if /*constexpr*/(!byteorder_is_big_endian)
    return;

  byteswap_inplace(cmd.action_code);
  byteswap_inplace(cmd.function_code);
  byteswap_inplace(cmd.table_id);
}

void ata_if_be_byteswap_inplace(ata_sct_temperature_history_table & tmh)
{
  if /*constexpr*/(!byteorder_is_big_endian)
    return;

  byteswap_inplace(tmh.format_version);
  byteswap_inplace(tmh.sampling_period);
  byteswap_inplace(tmh.interval);
  byteswap_inplace(tmh.cb_index);
  byteswap_inplace(tmh.cb_size);
}

void ata_if_be_byteswap_inplace(ata_sct_feature_control_command & cmd)
{
  if /*constexpr*/(!byteorder_is_big_endian)
    return;

  byteswap_inplace(cmd.action_code);
  byteswap_inplace(cmd.function_code);
  byteswap_inplace(cmd.feature_code);
  byteswap_inplace(cmd.state);
  byteswap_inplace(cmd.option_flags);
}

void ata_if_be_byteswap_inplace(ata_sct_error_recovery_control_command & cmd)
{
  if /*constexpr*/(!byteorder_is_big_endian)
    return;

  byteswap_inplace(cmd.action_code);
  byteswap_inplace(cmd.function_code);
  byteswap_inplace(cmd.selection_code);
  byteswap_inplace(cmd.time_limit);
}

} // namespace smartmon
