/*
 * nvmeprint.cpp
 *
 * Home page of code is: https://www.smartmontools.org
 *
 * Copyright (C) 2016-22 Christian Franke
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "config.h"
#define __STDC_FORMAT_MACROS 1 // enable PRI* for C++

#include "nvmeprint.h"

const char * nvmeprint_cvsid = "$Id$"
  NVMEPRINT_H_CVSID;

#include "utility.h"
#include "dev_interface.h"
#include "nvmecmds.h"
#include "atacmds.h" // dont_print_serial_number
#include "scsicmds.h" // dStrHex()
#include "smartctl.h"
#include "sg_unaligned.h"

#include <inttypes.h>

using namespace smartmontools;

// Return true if 128 bit LE integer is != 0.
static bool le128_is_non_zero(const unsigned char (& val)[16])
{
  for (int i = 0; i < 16; i++) {
    if (val[i])
      return true;
  }
  return false;
}

// Format 128 bit integer for printing.
// Add value with SI prefixes if BYTES_PER_UNIT is specified.
static const char * le128_to_str(char (& str)[64], uint64_t hi, uint64_t lo, unsigned bytes_per_unit)
{
  if (!hi) {
    // Up to 64-bit, print exact value
    format_with_thousands_sep(str, sizeof(str)-16, lo);

    if (lo && bytes_per_unit && lo < 0xffffffffffffffffULL / bytes_per_unit) {
      int i = strlen(str);
      str[i++] = ' '; str[i++] = '[';
      format_capacity(str+i, (int)sizeof(str)-i-1, lo * bytes_per_unit);
      i = strlen(str);
      str[i++] = ']'; str[i] = 0;
    }
  }
  else {
    // More than 64-bit, prepend '~' flag on low precision
    int i = 0;
    // cppcheck-suppress knownConditionTrueFalse
    if (uint128_to_str_precision_bits() < 128)
      str[i++] = '~';
    uint128_hilo_to_str(str + i, (int)sizeof(str) - i, hi, lo);
  }

  return str;
}

// Format 128 bit LE integer for printing.
// Add value with SI prefixes if BYTES_PER_UNIT is specified.
static const char * le128_to_str(char (& str)[64], const unsigned char (& val)[16],
  unsigned bytes_per_unit = 0)
{
  uint64_t hi = val[15];
  for (int i = 15-1; i >= 8; i--) {
    hi <<= 8; hi += val[i];
  }
  uint64_t lo = val[7];
  for (int i =  7-1; i >= 0; i--) {
    lo <<= 8; lo += val[i];
  }
  return le128_to_str(str, hi, lo, bytes_per_unit);
}

// Format capacity specified as 64bit LBA count for printing.
static const char * lbacap_to_str(char (& str)[64], uint64_t lba_cnt, int lba_bits)
{
  return le128_to_str(str, (lba_cnt >> (64 - lba_bits)), (lba_cnt << lba_bits), 1);
}

// Output capacity specified as 64bit LBA count to JSON
static void lbacap_to_js(const json::ref & jref, uint64_t lba_cnt, int lba_bits)
{
  jref["blocks"].set_unsafe_uint64(lba_cnt);
  jref["bytes"].set_unsafe_uint128((lba_cnt >> (64 - lba_bits)), (lba_cnt << lba_bits));
}

// Format a Kelvin temperature value in Celsius.
static const char * kelvin_to_str(char (& str)[64], int k)
{
  if (!k) // unsupported?
    str[0] = '-', str[1] = 0;
  else
    snprintf(str, sizeof(str), "%d Celsius", k - 273);
  return str;
}

static void print_drive_info(const nvme_id_ctrl & id_ctrl, const nvme_id_ns & id_ns,
  unsigned nsid, bool show_all)
{
  char buf[64];
  jout("Model Number:                       %s\n", format_char_array(buf, id_ctrl.mn));
  jglb["model_name"] = buf;
  if (!dont_print_serial_number) {
    jout("Serial Number:                      %s\n", format_char_array(buf, id_ctrl.sn));
    jglb["serial_number"] = buf;
  }

  jout("Firmware Version:                   %s\n", format_char_array(buf, id_ctrl.fr));
  jglb["firmware_version"] = buf;

  // Vendor and Subsystem IDs are usually equal
  if (show_all || id_ctrl.vid != id_ctrl.ssvid) {
    jout("PCI Vendor ID:                      0x%04x\n", id_ctrl.vid);
    jout("PCI Vendor Subsystem ID:            0x%04x\n", id_ctrl.ssvid);
  }
  else {
    jout("PCI Vendor/Subsystem ID:            0x%04x\n", id_ctrl.vid);
  }
  jglb["nvme_pci_vendor"]["id"] = id_ctrl.vid;
  jglb["nvme_pci_vendor"]["subsystem_id"] = id_ctrl.ssvid;

  jout("IEEE OUI Identifier:                0x%02x%02x%02x\n",
       id_ctrl.ieee[2], id_ctrl.ieee[1], id_ctrl.ieee[0]);
  jglb["nvme_ieee_oui_identifier"] = sg_get_unaligned_le(3, id_ctrl.ieee);

  // Capacity info is optional for devices without namespace management
  if (show_all || le128_is_non_zero(id_ctrl.tnvmcap) || le128_is_non_zero(id_ctrl.unvmcap)) {
    jout("Total NVM Capacity:                 %s\n", le128_to_str(buf, id_ctrl.tnvmcap, 1));
    jglb["nvme_total_capacity"].set_unsafe_le128(id_ctrl.tnvmcap);
    jout("Unallocated NVM Capacity:           %s\n", le128_to_str(buf, id_ctrl.unvmcap, 1));
    jglb["nvme_unallocated_capacity"].set_unsafe_le128(id_ctrl.unvmcap);
  }

  jout("Controller ID:                      %d\n", id_ctrl.cntlid);
  jglb["nvme_controller_id"] = id_ctrl.cntlid;

  if (id_ctrl.ver) { // NVMe 1.2
    int i = snprintf(buf, sizeof(buf), "%u.%u", id_ctrl.ver >> 16, (id_ctrl.ver >> 8) & 0xff);
    if (i > 0 && (id_ctrl.ver & 0xff))
      snprintf(buf+i, sizeof(buf)-i, ".%u", id_ctrl.ver & 0xff);
  }
  else
    snprintf(buf, sizeof(buf), "<1.2");
  jout("NVMe Version:                       %s\n", buf);
  jglb["nvme_version"]["string"] = buf;
  jglb["nvme_version"]["value"] = id_ctrl.ver;

  // Print namespace info if available
  jout("Number of Namespaces:               %u\n", id_ctrl.nn);
  jglb["nvme_number_of_namespaces"] = id_ctrl.nn;

  if (nsid && id_ns.nsze) {
    const char * align = &("  "[nsid < 10 ? 0 : (nsid < 100 ? 1 : 2)]);
    int fmt_lba_bits = id_ns.lbaf[id_ns.flbas & 0xf].ds;

    json::ref jrns = jglb["nvme_namespaces"][0];
    jrns["id"] = nsid;

    // Size and Capacity are equal if thin provisioning is not supported
    if (show_all || id_ns.ncap != id_ns.nsze || (id_ns.nsfeat & 0x01)) {
      jout("Namespace %u Size:                 %s%s\n", nsid, align,
           lbacap_to_str(buf, id_ns.nsze, fmt_lba_bits));
      jout("Namespace %u Capacity:             %s%s\n", nsid, align,
           lbacap_to_str(buf, id_ns.ncap, fmt_lba_bits));
    }
    else {
      jout("Namespace %u Size/Capacity:        %s%s\n", nsid, align,
           lbacap_to_str(buf, id_ns.nsze, fmt_lba_bits));
    }
    lbacap_to_js(jrns["size"], id_ns.nsze, fmt_lba_bits);
    lbacap_to_js(jrns["capacity"], id_ns.ncap, fmt_lba_bits);
    lbacap_to_js(jglb["user_capacity"], id_ns.ncap, fmt_lba_bits); // TODO: use nsze?

    // Utilization may be always equal to Capacity if thin provisioning is not supported
    if (show_all || id_ns.nuse != id_ns.ncap || (id_ns.nsfeat & 0x01))
      jout("Namespace %u Utilization:          %s%s\n", nsid, align,
           lbacap_to_str(buf, id_ns.nuse, fmt_lba_bits));
    lbacap_to_js(jrns["utilization"], id_ns.nuse, fmt_lba_bits);

    jout("Namespace %u Formatted LBA Size:   %s%u\n", nsid, align, (1U << fmt_lba_bits));
    jrns["formatted_lba_size"] = (1U << fmt_lba_bits);
    jglb["logical_block_size"] = (1U << fmt_lba_bits);

    if (show_all || nonempty(id_ns.eui64, sizeof(id_ns.eui64))) {
      jout("Namespace %u IEEE EUI-64:          %s%02x%02x%02x %02x%02x%02x%02x%02x\n",
           nsid, align, id_ns.eui64[0], id_ns.eui64[1], id_ns.eui64[2], id_ns.eui64[3],
           id_ns.eui64[4], id_ns.eui64[5], id_ns.eui64[6], id_ns.eui64[7]);
      jrns["eui64"]["oui"]    = sg_get_unaligned_be(3, id_ns.eui64);
      jrns["eui64"]["ext_id"] = sg_get_unaligned_be(5, id_ns.eui64 + 3);
    }
  }

  // SMART/Health Information is mandatory
  jglb["smart_support"] += { {"available", true}, {"enabled", true} };

  jout_startup_datetime("Local Time is:                      ");
}

// Format scaled power value.
static const char * format_power(char (& str)[16], unsigned power, unsigned scale)
{
  switch (scale & 0x3) {
    case 0: // not reported
      str[0] = '-'; str[1] = ' '; str[2] = 0; break;
    case 1: // 0.0001W
      snprintf(str, sizeof(str), "%u.%04uW", power / 10000, power % 10000); break;
    case 2: // 0.01W
      snprintf(str, sizeof(str), "%u.%02uW", power / 100, power % 100); break;
    default: // reserved
      str[0] = '?'; str[1] = 0; break;
  }
  return str;
}

static void print_drive_capabilities(const nvme_id_ctrl & id_ctrl, const nvme_id_ns & id_ns,
  unsigned nsid, bool show_all)
{
  pout("Firmware Updates (0x%02x):            %d Slot%s%s%s\n", id_ctrl.frmw,
       ((id_ctrl.frmw >> 1) & 0x7), (((id_ctrl.frmw >> 1) & 0x7) != 1 ? "s" : ""),
       ((id_ctrl.frmw & 0x01) ? ", Slot 1 R/O" : ""),
       ((id_ctrl.frmw & 0x10) ? ", no Reset required" : ""));

  if (show_all || id_ctrl.oacs)
    pout("Optional Admin Commands (0x%04x):  %s%s%s%s%s%s%s%s%s%s%s%s\n", id_ctrl.oacs,
         (!id_ctrl.oacs ? " -" : ""),
         ((id_ctrl.oacs & 0x0001) ? " Security" : ""),
         ((id_ctrl.oacs & 0x0002) ? " Format" : ""),
         ((id_ctrl.oacs & 0x0004) ? " Frmw_DL" : ""),
         ((id_ctrl.oacs & 0x0008) ? " NS_Mngmt" : ""), // NVMe 1.2
         ((id_ctrl.oacs & 0x0010) ? " Self_Test" : ""), // NVMe 1.3 ...
         ((id_ctrl.oacs & 0x0020) ? " Directvs" : ""),
         ((id_ctrl.oacs & 0x0040) ? " MI_Snd/Rec" : ""),
         ((id_ctrl.oacs & 0x0080) ? " Vrt_Mngmt" : ""),
         ((id_ctrl.oacs & 0x0100) ? " Drbl_Bf_Cfg" : ""),
         ((id_ctrl.oacs & 0x0200) ? " Get_LBA_Sts" : ""), // NVMe 1.4
         ((id_ctrl.oacs & ~0x03ff) ? " *Other*" : ""));

  if (show_all || id_ctrl.oncs)
    pout("Optional NVM Commands (0x%04x):    %s%s%s%s%s%s%s%s%s%s\n", id_ctrl.oncs,
         (!id_ctrl.oncs ? " -" : ""),
         ((id_ctrl.oncs & 0x0001) ? " Comp" : ""),
         ((id_ctrl.oncs & 0x0002) ? " Wr_Unc" : ""),
         ((id_ctrl.oncs & 0x0004) ? " DS_Mngmt" : ""),
         ((id_ctrl.oncs & 0x0008) ? " Wr_Zero" : ""), // NVMe 1.1 ...
         ((id_ctrl.oncs & 0x0010) ? " Sav/Sel_Feat" : ""),
         ((id_ctrl.oncs & 0x0020) ? " Resv" : ""),
         ((id_ctrl.oncs & 0x0040) ? " Timestmp" : ""), // NVMe 1.3
         ((id_ctrl.oncs & 0x0080) ? " Verify" : ""), // NVMe 1.4
         ((id_ctrl.oncs & ~0x00ff) ? " *Other*" : ""));

  if (show_all || id_ctrl.lpa)
    pout("Log Page Attributes (0x%02x):        %s%s%s%s%s%s%s\n", id_ctrl.lpa,
         (!id_ctrl.lpa ? " -" : ""),
         ((id_ctrl.lpa & 0x01) ? " S/H_per_NS" : ""),
         ((id_ctrl.lpa & 0x02) ? " Cmd_Eff_Lg" : ""), // NVMe 1.2
         ((id_ctrl.lpa & 0x04) ? " Ext_Get_Lg" : ""), // NVMe 1.2.1
         ((id_ctrl.lpa & 0x08) ? " Telmtry_Lg" : ""), // NVMe 1.3
         ((id_ctrl.lpa & 0x10) ? " Pers_Ev_Lg" : ""), // NVMe 1.4
         ((id_ctrl.lpa & ~0x001f) ? " *Other*" : ""));

  if (id_ctrl.mdts)
    pout("Maximum Data Transfer Size:         %u Pages\n", (1U << id_ctrl.mdts));
  else if (show_all)
    pout("Maximum Data Transfer Size:         -\n");

  // Temperature thresholds are optional
  char buf[64];
  if (show_all || id_ctrl.wctemp)
    pout("Warning  Comp. Temp. Threshold:     %s\n", kelvin_to_str(buf, id_ctrl.wctemp));
  if (show_all || id_ctrl.cctemp)
    pout("Critical Comp. Temp. Threshold:     %s\n", kelvin_to_str(buf, id_ctrl.cctemp));

  if (nsid && (show_all || id_ns.nsfeat)) {
    const char * align = &("  "[nsid < 10 ? 0 : (nsid < 100 ? 1 : 2)]);
    pout("Namespace %u Features (0x%02x):     %s%s%s%s%s%s%s%s\n", nsid, id_ns.nsfeat, align,
         (!id_ns.nsfeat ? " -" : ""),
         ((id_ns.nsfeat & 0x01) ? " Thin_Prov" : ""),
         ((id_ns.nsfeat & 0x02) ? " NA_Fields" : ""), // NVMe 1.2 ...
         ((id_ns.nsfeat & 0x04) ? " Dea/Unw_Error" : ""),
         ((id_ns.nsfeat & 0x08) ? " No_ID_Reuse" : ""), // NVMe 1.3
         ((id_ns.nsfeat & 0x10) ? " NP_Fields" : ""), // NVMe 1.4
         ((id_ns.nsfeat & ~0x1f) ? " *Other*" : ""));
  }

  // Print Power States
  pout("\nSupported Power States\n");
  pout("St Op     Max   Active     Idle   RL RT WL WT  Ent_Lat  Ex_Lat\n");
  for (int i = 0; i <= id_ctrl.npss /* 1-based */ && i < 32; i++) {
    char p1[16], p2[16], p3[16];
    const nvme_id_power_state & ps = id_ctrl.psd[i];
    pout("%2d %c %9s %8s %8s %3d %2d %2d %2d %8u %7u\n", i,
         ((ps.flags & 0x02) ? '-' : '+'),
         format_power(p1, ps.max_power, ((ps.flags & 0x01) ? 1 : 2)),
         format_power(p2, ps.active_power, ps.active_work_scale),
         format_power(p3, ps.idle_power, ps.idle_scale),
         ps.read_lat & 0x1f, ps.read_tput & 0x1f,
         ps.write_lat & 0x1f, ps.write_tput & 0x1f,
         ps.entry_lat, ps.exit_lat);
  }

  // Print LBA sizes
  if (nsid && id_ns.lbaf[0].ds) {
    pout("\nSupported LBA Sizes (NSID 0x%x)\n", nsid);
    pout("Id Fmt  Data  Metadt  Rel_Perf\n");
    for (int i = 0; i <= id_ns.nlbaf /* 1-based */ && i < 16; i++) {
      const nvme_lbaf & lba = id_ns.lbaf[i];
      pout("%2d %c %7u %7d %9d\n", i, (i == id_ns.flbas ? '+' : '-'),
           (1U << lba.ds), lba.ms, lba.rp);
    }
  }
}

static void print_critical_warning(unsigned char w)
{
  jout("SMART overall-health self-assessment test result: %s\n",
       (!w ? "PASSED" : "FAILED!"));
  jglb["smart_status"]["passed"] = !w;

  json::ref jref = jglb["smart_status"]["nvme"];
  jref["value"] = w;

  if (w) {
   if (w & 0x01)
     jout("- available spare has fallen below threshold\n");
   jref["spare_below_threshold"] = !!(w & 0x01);
   if (w & 0x02)
     jout("- temperature is above or below threshold\n");
   jref["temperature_above_or_below_threshold"] = !!(w & 0x02);
   if (w & 0x04)
     jout("- NVM subsystem reliability has been degraded\n");
   jref["reliability_degraded"] = !!(w & 0x04);
   if (w & 0x08)
     jout("- media has been placed in read only mode\n");
   jref["media_read_only"] = !!(w & 0x08);
   if (w & 0x10)
     jout("- volatile memory backup device has failed\n");
   jref["volatile_memory_backup_failed"] = !!(w & 0x10);
   if (w & 0x20)
     jout("- persistent memory region has become read-only or unreliable\n");
   jref["persistent_memory_region_unreliable"] = !!(w & 0x20);
   if (w & ~0x3f)
     jout("- unknown critical warning(s) (0x%02x)\n", w & ~0x3f);
   jref["other"] = w & ~0x3f;
  }

  jout("\n");
}

static void print_smart_log(const nvme_smart_log & smart_log,
  const nvme_id_ctrl & id_ctrl, bool show_all)
{
  json::ref jref = jglb["nvme_smart_health_information_log"];
  char buf[64];
  jout("SMART/Health Information (NVMe Log 0x02)\n");
  jout("Critical Warning:                   0x%02x\n", smart_log.critical_warning);
  jref["critical_warning"] = smart_log.critical_warning;

  int k = sg_get_unaligned_le16(smart_log.temperature);
  jout("Temperature:                        %s\n", kelvin_to_str(buf, k));
  if (k) {
    jref["temperature"] = k - 273;
    jglb["temperature"]["current"] = k - 273;
  }

  jout("Available Spare:                    %u%%\n", smart_log.avail_spare);
  jref["available_spare"] = smart_log.avail_spare;
  jout("Available Spare Threshold:          %u%%\n", smart_log.spare_thresh);
  jref["available_spare_threshold"] = smart_log.spare_thresh;
  jout("Percentage Used:                    %u%%\n", smart_log.percent_used);
  jref["percentage_used"] = smart_log.percent_used;
  jout("Data Units Read:                    %s\n", le128_to_str(buf, smart_log.data_units_read, 1000*512));
  jref["data_units_read"].set_unsafe_le128(smart_log.data_units_read);
  jout("Data Units Written:                 %s\n", le128_to_str(buf, smart_log.data_units_written, 1000*512));
  jref["data_units_written"].set_unsafe_le128(smart_log.data_units_written);
  jout("Host Read Commands:                 %s\n", le128_to_str(buf, smart_log.host_reads));
  jref["host_reads"].set_unsafe_le128(smart_log.host_reads);
  jout("Host Write Commands:                %s\n", le128_to_str(buf, smart_log.host_writes));
  jref["host_writes"].set_unsafe_le128(smart_log.host_writes);
  jout("Controller Busy Time:               %s\n", le128_to_str(buf, smart_log.ctrl_busy_time));
  jref["controller_busy_time"].set_unsafe_le128(smart_log.ctrl_busy_time);
  jout("Power Cycles:                       %s\n", le128_to_str(buf, smart_log.power_cycles));
  jref["power_cycles"].set_unsafe_le128(smart_log.power_cycles);
  jglb["power_cycle_count"].set_if_safe_le128(smart_log.power_cycles);
  jout("Power On Hours:                     %s\n", le128_to_str(buf, smart_log.power_on_hours));
  jref["power_on_hours"].set_unsafe_le128(smart_log.power_on_hours);
  jglb["power_on_time"]["hours"].set_if_safe_le128(smart_log.power_on_hours);
  jout("Unsafe Shutdowns:                   %s\n", le128_to_str(buf, smart_log.unsafe_shutdowns));
  jref["unsafe_shutdowns"].set_unsafe_le128(smart_log.unsafe_shutdowns);
  jout("Media and Data Integrity Errors:    %s\n", le128_to_str(buf, smart_log.media_errors));
  jref["media_errors"].set_unsafe_le128(smart_log.media_errors);
  jout("Error Information Log Entries:      %s\n", le128_to_str(buf, smart_log.num_err_log_entries));
  jref["num_err_log_entries"].set_unsafe_le128(smart_log.num_err_log_entries);

  // Temperature thresholds are optional
  if (show_all || id_ctrl.wctemp || smart_log.warning_temp_time) {
    jout("Warning  Comp. Temperature Time:    %d\n", smart_log.warning_temp_time);
    jref["warning_temp_time"] = smart_log.warning_temp_time;
  }
  if (show_all || id_ctrl.cctemp || smart_log.critical_comp_time) {
    jout("Critical Comp. Temperature Time:    %d\n", smart_log.critical_comp_time);
    jref["critical_comp_time"] = smart_log.critical_comp_time;
  }

  // Temperature sensors are optional
  for (int i = 0; i < 8; i++) {
    k = smart_log.temp_sensor[i];
    if (show_all || k) {
      jout("Temperature Sensor %d:               %s\n", i + 1,
           kelvin_to_str(buf, k));
      if (k)
        jref["temperature_sensors"][i] = k - 273;
    }
  }
  if (show_all || smart_log.thm_temp1_trans_count)
    pout("Thermal Temp. 1 Transition Count:   %d\n", smart_log.thm_temp1_trans_count);
  if (show_all || smart_log.thm_temp2_trans_count)
    pout("Thermal Temp. 2 Transition Count:   %d\n", smart_log.thm_temp2_trans_count);
  if (show_all || smart_log.thm_temp1_total_time)
    pout("Thermal Temp. 1 Total Time:         %d\n", smart_log.thm_temp1_total_time);
  if (show_all || smart_log.thm_temp2_total_time)
    pout("Thermal Temp. 2 Total Time:         %d\n", smart_log.thm_temp2_total_time);
  pout("\n");
}

static void print_error_log(const nvme_error_log_page * error_log,
  unsigned read_entries, unsigned max_entries)
{
  pout("Error Information (NVMe Log 0x01, %u of %u entries)\n",
       read_entries, max_entries);

  // Search last valid entry
  unsigned valid_entries = read_entries;
  while (valid_entries && !error_log[valid_entries-1].error_count)
    valid_entries--;

  if (!valid_entries) {
    pout("No Errors Logged\n\n");
    return;
  }

  pout("Num   ErrCount  SQId   CmdId  Status  PELoc          LBA  NSID    VS\n");
  int unused = 0;
  for (unsigned i = 0; i < valid_entries; i++) {
    const nvme_error_log_page & e = error_log[i];
    if (!e.error_count) {
      // unused or invalid entry
      unused++;
      continue;
    }
    if (unused) {
      pout("  - [%d unused entr%s]\n", unused, (unused == 1 ? "y" : "ies"));
      unused = 0;
    }

    char sq[16] = "-", cm[16] = "-", st[16] = "-", pe[16] = "-";
    char lb[32] = "-", ns[16] = "-", vs[8] = "-";
    if (e.sqid != 0xffff)
      snprintf(sq, sizeof(sq), "%d", e.sqid);
    if (e.cmdid != 0xffff)
      snprintf(cm, sizeof(cm), "0x%04x", e.cmdid);
    if (e.status_field != 0xffff)
      snprintf(st, sizeof(st), "0x%04x", e.status_field);
    if (e.parm_error_location != 0xffff)
      snprintf(pe, sizeof(pe), "0x%03x", e.parm_error_location);
    if (e.lba != 0xffffffffffffffffULL)
      snprintf(lb, sizeof(lb), "%" PRIu64, e.lba);
    if (e.nsid != 0xffffffffU)
      snprintf(ns, sizeof(ns), "%u", e.nsid);
    if (e.vs != 0x00)
      snprintf(vs, sizeof(vs), "0x%02x", e.vs);

    pout("%3u %10" PRIu64 " %5s %7s %7s %6s %12s %5s %5s\n",
         i, e.error_count, sq, cm, st, pe, lb, ns, vs);
  }

  if (valid_entries == read_entries && read_entries < max_entries)
    pout("... (%u entries not read)\n", max_entries - read_entries);
  pout("\n");
}

static void print_self_test_log(const nvme_self_test_log & self_test_log)
{
  pout("Self-test Log (NVMe Log 0x06)\n");

  const char * s; char buf[32];
  switch (self_test_log.current_operation & 0xf) {
    case 0x0: s = "No self-test in progress"; break;
    case 0x1: s = "Short self-test in progress"; break;
    case 0x2: s = "Extended self-test in progress"; break;
    case 0xe: s = "Vendor specific self-test in progress"; break;
    default:  snprintf(buf, sizeof(buf), "Unknown status (0x%x)",
                       self_test_log.current_operation & 0xf);
              s = buf; break;
  }
  pout("Self-test status: %s", s);
  if (self_test_log.current_operation & 0xf)
    pout(" (%d%% completed)", self_test_log.current_completion & 0x7f);
  pout("\n");

  int cnt = 0;
  for (unsigned i = 0; i < 20; i++) {
    const nvme_self_test_result & r = self_test_log.results[i];
    uint8_t op = r.self_test_status >> 4;
    uint8_t res = r.self_test_status & 0xf;
    if (!op || res == 0xf)
      continue; // unused entry

    const char * t; char buf2[32];
    switch (op) {
      case 0x1: t = "Short"; break;
      case 0x2: t = "Extended"; break;
      case 0xe: t = "Vendor specific"; break;
      default:  snprintf(buf2, sizeof(buf2), "Unknown (0x%x)", op);
                t = buf2; break;
    }

    switch (res) {
      case 0x0: s = "Completed without error"; break;
      case 0x1: s = "Aborted: Self-test command"; break;
      case 0x2: s = "Aborted: Controller Reset"; break;
      case 0x3: s = "Aborted: Namespace removed"; break;
      case 0x4: s = "Aborted: Format NVM command"; break;
      case 0x5: s = "Fatal or unknown test error"; break;
      case 0x6: s = "Completed: unknown failed segment"; break;
      case 0x7: s = "Completed: failed segments"; break;
      case 0x8: s = "Aborted: unknown reason"; break;
      case 0x9: s = "Aborted: sanitize operation"; break;
      default:  snprintf(buf, sizeof(buf), "Unknown result (0x%x)", res);
                s = buf; break;
    }

    char ns[16] = "-", lb[32] = "-", st[8] = "-", sc[8] = "-";
    if (r.valid & 0x01) {
      if (r.nsid == 0xffffffff)
        ns[0] = '*', ns[1] = 0;
      else
        snprintf(ns, sizeof(ns), "%u", r.nsid);
    }
    if (r.valid & 0x02)
      snprintf(lb, sizeof(lb), "%" PRIu64, sg_get_unaligned_le64(r.lba));
    if (r.valid & 0x04)
      snprintf(st, sizeof(st), "0x%x", r.status_code_type);
    if (r.valid & 0x08)
      snprintf(sc, sizeof(sc), "0x%02x", r.status_code);

    if (++cnt == 1)
      pout("Num  Test_Description  Status                       Power_on_Hours  Failing_LBA  NSID SCT Code\n");
    pout("%2u   %-17s %-33s %9" PRIu64 " %12s %5s %3s %4s\n",
         i, t, s, sg_get_unaligned_le64(r.power_on_hours), lb, ns, st, sc);
  }

  if (!cnt)
    pout("No Self-tests Logged\n");
  pout("\n");
}

int nvmePrintMain(nvme_device * device, const nvme_print_options & options)
{
  if (!(   options.drive_info || options.drive_capabilities
        || options.smart_check_status || options.smart_vendor_attrib
        || options.smart_selftest_log || options.error_log_entries
        || options.log_page_size || options.smart_selftest_type     )) {
    pout("NVMe device successfully opened\n\n"
         "Use 'smartctl -a' (or '-x') to print SMART (and more) information\n\n");
    return 0;
  }

  // Show unset optional values only if debugging is enabled
  bool show_all = (nvme_debugmode > 0);

  // Read Identify Controller always
  nvme_id_ctrl id_ctrl;
  if (!nvme_read_id_ctrl(device, id_ctrl)) {
    jerr("Read NVMe Identify Controller failed: %s\n", device->get_errmsg());
    return FAILID;
  }

  // Print Identify Controller/Namespace info
  if (options.drive_info || options.drive_capabilities) {
    pout("=== START OF INFORMATION SECTION ===\n");
    nvme_id_ns id_ns; memset(&id_ns, 0, sizeof(id_ns));

    unsigned nsid = device->get_nsid();
    if (nsid == 0xffffffffU) {
      // Broadcast namespace
      if (id_ctrl.nn == 1) {
        // No namespace management, get size from single namespace
        nsid = 1;
        if (!nvme_read_id_ns(device, nsid, id_ns))
          nsid = 0;
      }
    }
    else {
        // Identify current namespace
        if (!nvme_read_id_ns(device, nsid, id_ns)) {
          jerr("Read NVMe Identify Namespace 0x%x failed: %s\n", nsid, device->get_errmsg());
          return FAILID;
        }
    }

    if (options.drive_info)
      print_drive_info(id_ctrl, id_ns, nsid, show_all);
    if (options.drive_capabilities)
      print_drive_capabilities(id_ctrl, id_ns, nsid, show_all);
    pout("\n");
  }

  if (   options.smart_check_status || options.smart_vendor_attrib
      || options.error_log_entries)
    pout("=== START OF SMART DATA SECTION ===\n");

  // Print SMART Status and SMART/Health Information
  int retval = 0;
  if (options.smart_check_status || options.smart_vendor_attrib) {
    nvme_smart_log smart_log;
    if (!nvme_read_smart_log(device, smart_log)) {
      jerr("Read NVMe SMART/Health Information failed: %s\n\n", device->get_errmsg());
      return FAILSMART;
    }

    if (options.smart_check_status) {
      print_critical_warning(smart_log.critical_warning);
      if (smart_log.critical_warning)
        retval |= FAILSTATUS;
    }

    if (options.smart_vendor_attrib) {
      print_smart_log(smart_log, id_ctrl, show_all);
    }
  }

  // Check for Log Page Offset support
  bool lpo_sup = !!(id_ctrl.lpa & 0x04);

  // Print Error Information Log
  if (options.error_log_entries) {
    unsigned max_entries = id_ctrl.elpe + 1; // 0's based value
    unsigned want_entries = options.error_log_entries;
    if (want_entries > max_entries)
      want_entries = max_entries;
    raw_buffer error_log_buf(want_entries * sizeof(nvme_error_log_page));
    nvme_error_log_page * error_log =
      reinterpret_cast<nvme_error_log_page *>(error_log_buf.data());

    unsigned read_entries = nvme_read_error_log(device, error_log, want_entries, lpo_sup);
    if (!read_entries) {
      jerr("Read %u entries from Error Information Log failed: %s\n\n",
           want_entries, device->get_errmsg());
      return retval | FAILSMART;
    }
    if (read_entries < want_entries)
      jerr("Read Error Information Log failed, %u entries missing: %s\n",
           want_entries - read_entries, device->get_errmsg());

    print_error_log(error_log, read_entries, max_entries);
  }

  // Check for self-test support
  bool self_test_sup = !!(id_ctrl.oacs & 0x0010);
  unsigned self_test_nsid = device->get_nsid(); // TODO: Support NSID=0 to test controller

  // Read and print Self-test log, check for running test
  int self_test_completion = -1;
  if (options.smart_selftest_log || options.smart_selftest_type) {
    if (!self_test_sup)
      pout("Self-tests not supported\n\n");
    else {
      nvme_self_test_log self_test_log;
      if (!nvme_read_self_test_log(device, self_test_nsid, self_test_log)) {
        jerr("Read Self-test Log failed: %s\n\n", device->get_errmsg());
        return retval | FAILSMART;
      }

      if (options.smart_selftest_log)
        print_self_test_log(self_test_log);

      if (self_test_log.current_operation & 0xf)
        self_test_completion = self_test_log.current_completion & 0x7f;
    }
  }

  // Dump log page
  if (options.log_page_size) {
    // Align size to dword boundary
    unsigned size = ((options.log_page_size + 4-1) / 4) * 4;
    raw_buffer log_buf(size);

    unsigned nsid;
    switch (options.log_page) {
    case 1:
    case 2:
    case 3:
      nsid = 0xffffffff;
      break;
    default:
      nsid = device->get_nsid();
      break;
    }
    unsigned read_bytes = nvme_read_log_page(device, nsid, options.log_page, log_buf.data(),
                                             size, lpo_sup);
    if (!read_bytes) {
      jerr("Read NVMe Log 0x%02x failed: %s\n\n", options.log_page, device->get_errmsg());
      return retval | FAILSMART;
    }
    if (read_bytes < size)
      jerr("Read NVMe Log 0x%02x failed, 0x%x bytes missing: %s\n",
           options.log_page, size - read_bytes, device->get_errmsg());

    pout("NVMe Log 0x%02x (0x%04x bytes)\n", options.log_page, read_bytes);
    dStrHex(log_buf.data(), read_bytes, 0);
    pout("\n");
  }

  // Start self-test
  if (self_test_sup && options.smart_selftest_type) {
    bool self_test_abort = (options.smart_selftest_type == 0xf);
    if (!self_test_abort && self_test_completion >= 0) {
      pout("Can't start self-test without aborting current test (%2d%% completed)\n"
           "Use smartctl -X to abort test\n", self_test_completion);
      retval |= FAILSMART;
    }
    else {
      if (!nvme_self_test(device, options.smart_selftest_type, self_test_nsid)) {
        jerr("NVMe Self-test cmd with type=0x%x, nsid=0x%x failed: %s\n\n",
             options.smart_selftest_type, self_test_nsid, device->get_errmsg());
        return retval | FAILSMART;
      }

      if (!self_test_abort)
        pout("Self-test has begun\n"
             "Use smartctl -X to abort test\n");
      else
        pout("Self-test aborted!\n");
    }
  }

  return retval;
}
