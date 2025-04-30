/*
 * nvmecmds.cpp
 *
 * Home page of code is: https://www.smartmontools.org
 *
 * Copyright (C) 2016-24 Christian Franke
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "config.h"
#include "nvmecmds.h"

const char * nvmecmds_cvsid = "$Id$"
  NVMECMDS_H_CVSID;

#include "dev_interface.h"
#include "atacmds.h" // swapx(), dont_print_serial_number
#include "scsicmds.h" // dStrHex()
#include "utility.h"

#include <errno.h>

using namespace smartmontools;

// Print NVMe debug messages?
unsigned char nvme_debugmode = 0;

// Dump up to 4096 bytes, do not dump trailing zero bytes.
// TODO: Handle this by new unified function in utility.cpp
static void debug_hex_dump(const void * data, unsigned size)
{
  const unsigned char * p = (const unsigned char *)data;
  const unsigned limit = 4096; // sizeof(nvme_id_ctrl)
  unsigned sz = (size <= limit ? size : limit);

  while (sz > 0x10 && !p[sz-1])
    sz--;
  if (sz < size) {
    if (sz & 0x0f)
      sz = (sz & ~0x0f) + 0x10;
    sz += 0x10;
    if (sz > size)
      sz = size;
  }

  dStrHex((const uint8_t *)p, sz, 0);
  if (sz < size)
    pout(" ...\n");
}

// Call NVMe pass-through and print debug info if requested.
static bool nvme_pass_through(nvme_device * device, const nvme_cmd_in & in,
  nvme_cmd_out & out)
{
  if (nvme_debugmode) {
    pout(" [NVMe call: opcode=0x%02x, size=0x%04x, nsid=0x%08x, cdw10=0x%08x",
      in.opcode, in.size, in.nsid, in.cdw10);
    if (in.cdw11 || in.cdw12 || in.cdw13 || in.cdw14 || in.cdw15)
      pout(",\n  cdw1x=0x%08x, 0x%08x, 0x%08x, 0x%08x, 0x%08x",
       in.cdw11, in.cdw12, in.cdw13, in.cdw14, in.cdw15);
    pout("]\n");
  }

  auto start_usec = (nvme_debugmode ? get_timer_usec() : -1);

  bool ok = device->nvme_pass_through(in, out);

  if (start_usec >= 0) {
    auto duration_usec = get_timer_usec() - start_usec;
    if (duration_usec > 0)
      pout(" [Duration: %.6fs]\n", duration_usec / 1000000.0);
  }

  if (dont_print_serial_number && ok && in.opcode == nvme_admin_identify) {
    if (in.cdw10 == 0x01 && in.size >= sizeof(nvme_id_ctrl)) {
      // Identify controller: Invalidate serial number
      nvme_id_ctrl & id_ctrl = *reinterpret_cast<nvme_id_ctrl *>(in.buffer);
      memset(id_ctrl.sn, 'X', sizeof(id_ctrl.sn));
    }
    else if (in.cdw10 == 0x00 && in.size >= sizeof(nvme_id_ns)) {
      // Identify namespace: Invalidate IEEE EUI-64
      nvme_id_ns & id_ns = *reinterpret_cast<nvme_id_ns *>(in.buffer);
      memset(id_ns.eui64, 0x00, sizeof(id_ns.eui64));
    }
  }

  if (nvme_debugmode) {
    if (!ok) {
      pout(" [NVMe call failed: ");
      if (out.status_valid)
        pout("NVMe Status=0x%04x", out.status);
      else
        pout("%s", device->get_errmsg());
    }
    else {
      pout(" [NVMe call succeeded: result=0x%08x", out.result);
      if (nvme_debugmode > 1 && in.direction() == nvme_cmd_in::data_in) {
        pout("\n");
        debug_hex_dump(in.buffer, in.size);
        pout(" ");
      }
    }
    pout("]\n");
  }

  return ok;
}

// Call NVMe pass-through and print debug info if requested.
// Version without output parameters.
static bool nvme_pass_through(nvme_device * device, const nvme_cmd_in & in)
{
  nvme_cmd_out out;
  return nvme_pass_through(device, in, out);
}

// Read NVMe identify info with controller/namespace field CNS.
static bool nvme_read_identify(nvme_device * device, unsigned nsid,
  unsigned char cns, void * data, unsigned size)
{
  memset(data, 0, size);
  nvme_cmd_in in;
  in.set_data_in(nvme_admin_identify, data, size);
  in.nsid = nsid;
  in.cdw10 = cns;

  return nvme_pass_through(device, in);
}

// Read NVMe Identify Controller data structure.
bool nvme_read_id_ctrl(nvme_device * device, nvme_id_ctrl & id_ctrl)
{
  if (!nvme_read_identify(device, 0, 0x01, &id_ctrl, sizeof(id_ctrl)))
    return false;

  if (isbigendian()) {
    swapx(&id_ctrl.vid);
    swapx(&id_ctrl.ssvid);
    swapx(&id_ctrl.cntlid);
    swapx(&id_ctrl.ver);
    swapx(&id_ctrl.oacs);
    swapx(&id_ctrl.wctemp);
    swapx(&id_ctrl.cctemp);
    swapx(&id_ctrl.mtfa);
    swapx(&id_ctrl.hmpre);
    swapx(&id_ctrl.hmmin);
    swapx(&id_ctrl.rpmbs);
    swapx(&id_ctrl.nn);
    swapx(&id_ctrl.oncs);
    swapx(&id_ctrl.fuses);
    swapx(&id_ctrl.awun);
    swapx(&id_ctrl.awupf);
    swapx(&id_ctrl.acwu);
    swapx(&id_ctrl.sgls);
    for (int i = 0; i < 32; i++) {
      swapx(&id_ctrl.psd[i].max_power);
      swapx(&id_ctrl.psd[i].entry_lat);
      swapx(&id_ctrl.psd[i].exit_lat);
      swapx(&id_ctrl.psd[i].idle_power);
      swapx(&id_ctrl.psd[i].active_power);
    }
  }

  return true;
}

// Read NVMe Identify Namespace data structure for namespace NSID.
bool nvme_read_id_ns(nvme_device * device, unsigned nsid, nvme_id_ns & id_ns)
{
  if (!nvme_read_identify(device, nsid, 0x00, &id_ns, sizeof(id_ns)))
    return false;

  if (isbigendian()) {
    swapx(&id_ns.nsze);
    swapx(&id_ns.ncap);
    swapx(&id_ns.nuse);
    swapx(&id_ns.nawun);
    swapx(&id_ns.nawupf);
    swapx(&id_ns.nacwu);
    swapx(&id_ns.nabsn);
    swapx(&id_ns.nabo);
    swapx(&id_ns.nabspf);
    for (int i = 0; i < 16; i++)
      swapx(&id_ns.lbaf[i].ms);
  }

  return true;
}

static bool nvme_read_log_page_1(nvme_device * device, unsigned nsid,
  unsigned char lid, void * data, unsigned size, unsigned offset = 0)
{
  if (!(4 <= size && size <= 0x1000 && !(size % 4) && !(offset % 4)))
    return device->set_err(EINVAL, "Invalid NVMe log size %u or offset %u", size, offset);

  memset(data, 0, size);
  nvme_cmd_in in;
  in.set_data_in(nvme_admin_get_log_page, data, size);
  in.nsid = nsid;
  in.cdw10 = lid | (((size / 4) - 1) << 16);
  in.cdw12 = offset; // LPOL, NVMe 1.2.1

  return nvme_pass_through(device, in);
}

// Read NVMe log page with identifier LID.
unsigned nvme_read_log_page(nvme_device * device, unsigned nsid, unsigned char lid,
  void * data, unsigned size, bool lpo_sup, unsigned offset /* = 0 */)
{
  unsigned n, bs;
  for (n = 0; n < size; n += bs) {
    if (!lpo_sup && offset + n > 0) {
      device->set_err(ENOSYS, "Log Page Offset not supported");
      break;
    }

    // Limit transfer size to one page to avoid problems with
    // limits of NVMe pass-through layer or too low MDTS values.
    bs = size - n;
    if (bs > 0x1000)
      bs = 0x1000;
    if (!nvme_read_log_page_1(device, nsid, lid, (char *)data + n, bs, offset + n))
      break;
  }

  return n;
}

// Read NVMe Error Information Log.
unsigned nvme_read_error_log(nvme_device * device, nvme_error_log_page * error_log,
  unsigned num_entries, bool lpo_sup)
{
  unsigned n = nvme_read_log_page(device, nvme_broadcast_nsid, 0x01, error_log,
                                  num_entries * sizeof(*error_log), lpo_sup);

  unsigned read_entries = n / sizeof(*error_log);
  if (isbigendian()) {
    for (unsigned i = 0; i < read_entries; i++) {
      swapx(&error_log[i].error_count);
      swapx(&error_log[i].sqid);
      swapx(&error_log[i].cmdid);
      swapx(&error_log[i].status_field);
      swapx(&error_log[i].parm_error_location);
      swapx(&error_log[i].lba);
      swapx(&error_log[i].nsid);
    }
  }

  return read_entries;
}

// Read NVMe SMART/Health Information log.
bool nvme_read_smart_log(nvme_device * device, uint32_t nsid, nvme_smart_log & smart_log)
{
  if (!nvme_read_log_page_1(device, nsid, 0x02, &smart_log, sizeof(smart_log)))
    return false;

  if (isbigendian()) {
    swapx(&smart_log.warning_temp_time);
    swapx(&smart_log.critical_comp_time);
    for (int i = 0; i < 8; i++)
      swapx(&smart_log.temp_sensor[i]);
  }

  return true;
}

// Read NVMe Self-test Log.
bool nvme_read_self_test_log(nvme_device * device, uint32_t nsid,
  smartmontools::nvme_self_test_log & self_test_log)
{
  if (!nvme_read_log_page_1(device, nsid, 0x06, &self_test_log, sizeof(self_test_log)))
    return false;

  if (isbigendian()) {
    for (int i = 0; i < 20; i++)
      swapx(&self_test_log.results[i].nsid);
  }

  return true;
}

// Start Self-test
bool nvme_self_test(nvme_device * device, uint8_t stc, uint32_t nsid)
{
  nvme_cmd_in in;
  in.opcode = nvme_admin_dev_self_test;
  in.nsid = nsid;
  in.cdw10 = stc;
  return nvme_pass_through(device, in);
}

// Return flagged error message for NVMe status SCT/SC fields or nullptr if unknown.
// If message starts with '-', the status indicates an invalid command (EINVAL).
static const char * nvme_status_to_flagged_str(uint16_t status)
{
  // Section 3.3.3.2.1 of NVM Express Base Specification Revision 2.0c, October 4, 2022
  uint8_t sc = (uint8_t)status;
  switch ((status >> 8) & 0x7) {
    case 0x0: // Generic Command Status
      if (sc < 0x80) switch (sc) {
        case 0x00: return "Successful Completion";
        case 0x01: return "-Invalid Command Opcode";
        case 0x02: return "-Invalid Field in Command";
        case 0x03: return "Command ID Conflict";
        case 0x04: return "Data Transfer Error";
        case 0x05: return "Commands Aborted due to Power Loss Notification";
        case 0x06: return "Internal Error";
        case 0x07: return "Command Abort Requested";
        case 0x08: return "Command Aborted due to SQ Deletion";
        case 0x09: return "Command Aborted due to Failed Fused Command";
        case 0x0a: return "Command Aborted due to Missing Fused Command";
        case 0x0b: return "-Invalid Namespace or Format";
        case 0x0c: return "Command Sequence Error";
        case 0x0d: return "-Invalid SGL Segment Descriptor";
        case 0x0e: return "-Invalid Number of SGL Descriptors";
        case 0x0f: return "-Data SGL Length Invalid";
        case 0x10: return "-Metadata SGL Length Invalid";
        case 0x11: return "-SGL Descriptor Type Invalid";
        case 0x12: return "-Invalid Use of Controller Memory Buffer";
        case 0x13: return "-PRP Offset Invalid";
        case 0x14: return "Atomic Write Unit Exceeded";
        case 0x15: return "Operation Denied";
        case 0x16: return "-SGL Offset Invalid";
        case 0x18: return "Host Identifier Inconsistent Format";
        case 0x19: return "Keep Alive Timer Expired";
        case 0x1a: return "-Keep Alive Timeout Invalid";
        case 0x1b: return "Command Aborted due to Preempt and Abort";
        case 0x1c: return "Sanitize Failed";
        case 0x1d: return "Sanitize In Progress";
        case 0x1e: return "SGL Data Block Granularity Invalid";
        case 0x1f: return "Command Not Supported for Queue in CMB";
        case 0x20: return "Namespace is Write Protected";
        case 0x21: return "Command Interrupted";
        case 0x22: return "Transient Transport Error";
        case 0x23: return "Command Prohibited by Command and Feature Lockdown";
        case 0x24: return "Admin Command Media Not Ready";
        //   0x25-0x7f: Reserved
      }
      else switch (sc) {
        //   0x80-0xbf: I/O Command Set Specific
        case 0x80: return "LBA Out of Range";
        case 0x81: return "Capacity Exceeded";
        case 0x82: return "Namespace Not Ready";
        case 0x83: return "Reservation Conflict";
        case 0x84: return "Format In Progress";
        case 0x85: return "-Invalid Value Size";
        case 0x86: return "-Invalid Key Size";
        case 0x87: return "KV Key Does Not Exist";
        case 0x88: return "Unrecovered Error";
        case 0x89: return "Key Exists";
        //   0x90-0xbf: Reserved
        //   0xc0-0xff: Vendor Specific
      }
      break;

    case 0x1: // Command Specific Status
      if (sc < 0x80) switch (sc) {
        case 0x00: return "-Completion Queue Invalid";
        case 0x01: return "-Invalid Queue Identifier";
        case 0x02: return "-Invalid Queue Size";
        case 0x03: return "Abort Command Limit Exceeded";
        case 0x04: return "Abort Command Is Missing";
        case 0x05: return "Asynchronous Event Request Limit Exceeded";
        case 0x06: return "-Invalid Firmware Slot";
        case 0x07: return "-Invalid Firmware Image";
        case 0x08: return "-Invalid Interrupt Vector";
        case 0x09: return "-Invalid Log Page";
        case 0x0a: return "-Invalid Format";
        case 0x0b: return "Firmware Activation Requires Conventional Reset";
        case 0x0c: return "-Invalid Queue Deletion";
        case 0x0d: return "-Feature Identifier Not Saveable";
        case 0x0e: return "-Feature Not Changeable";
        case 0x0f: return "-Feature Not Namespace Specific";
        case 0x10: return "Firmware Activation Requires NVM Subsystem Reset";
        case 0x11: return "Firmware Activation Requires Controller Level Reset";
        case 0x12: return "Firmware Activation Requires Maximum Time Violation";
        case 0x13: return "Firmware Activation Prohibited";
        case 0x14: return "Overlapping Range";
        case 0x15: return "Namespace Insufficient Capacity";
        case 0x16: return "-Namespace Identifier Unavailable";
        case 0x18: return "Namespace Already Attached";
        case 0x19: return "Namespace Is Private";
        case 0x1a: return "Namespace Not Attached";
        case 0x1b: return "Thin Provisioning Not Supported";
        case 0x1c: return "-Controller List Invalid";
        case 0x1d: return "Device Self-test In Progress";
        case 0x1e: return "Boot Partition Write Prohibited";
        case 0x1f: return "Invalid Controller Identifier";
        case 0x20: return "-Invalid Secondary Controller State";
        case 0x21: return "-Invalid Number of Controller Resources";
        case 0x22: return "-Invalid Resource Identifier";
        case 0x23: return "Sanitize Prohibited While Persistent Memory Region is Enabled";
        case 0x24: return "-ANA Group Identifier Invalid";
        case 0x25: return "ANA Attach Failed";
        case 0x26: return "Insufficient Capacity";
        case 0x27: return "Namespace Attachment Limit Exceeded";
        case 0x28: return "Prohibition of Command Execution Not Supported";
        case 0x29: return "I/O Command Set Not Supported";
        case 0x2a: return "I/O Command Set Not Enabled";
        case 0x2b: return "I/O Command Set Combination Rejected";
        case 0x2c: return "-Invalid I/O Command Set";
        case 0x2d: return "-Identifier Unavailable";
        //   0x2e-0x6f: Reserved
        //   0x70-0x7f: Directive Specific
      }
      else if (sc < 0xb8) switch (sc) {
        //   0x80-0xbf: I/O Command Set Specific (overlap with Fabrics Command Set)
        case 0x80: return "-Conflicting Attributes";
        case 0x81: return "-Invalid Protection Information";
        case 0x82: return "Attempted Write to Read Only Range";
        case 0x83: return "Command Size Limit Exceeded";
        //   0x84-0xb7: Reserved
      }
      else switch (sc) {
        case 0xb8: return "Zoned Boundary Error";
        case 0xb9: return "Zone Is Full";
        case 0xba: return "Zone Is Read Only";
        case 0xbb: return "Zone Is Offline";
        case 0xbc: return "Zone Invalid Write";
        case 0xbd: return "Too Many Active Zones";
        case 0xbe: return "Too Many Open Zones";
        case 0xbf: return "Invalid Zone State Transition";
        //   0xc0-0xff: Vendor Specific
      }
      break;

    case 0x2: // Media and Data Integrity Errors
      switch (sc) {
        //   0x00-0x7f: Reserved
        case 0x80: return "Write Fault";
        case 0x81: return "Unrecovered Read Error";
        case 0x82: return "End-to-end Guard Check Error";
        case 0x83: return "End-to-end Application Tag Check Error";
        case 0x84: return "End-to-end Reference Tag Check Error";
        case 0x85: return "Compare Failure";
        case 0x86: return "Access Denied";
        case 0x87: return "Deallocated or Unwritten Logical Block";
        case 0x88: return "End-to-End Storage Tag Check Error";
        //   0x89-0xbf: Reserved
        //   0xc0-0xff: Vendor Specific
      }
      break;

    case 0x3: // Path Related Status
      switch (sc) {
        case 0x00: return "Internal Path Error";
        case 0x01: return "Asymmetric Access Persistent Loss";
        case 0x02: return "Asymmetric Access Inaccessible";
        case 0x03: return "Asymmetric Access Transition";
        //   0x04-0x5f: Reserved
        //   0x60-0x6f: Controller Detected Pathing Errors
        case 0x60: return "Controller Pathing Error";
        //   0x61-0x6f: Reserved
        //   0x70-0x7f: Host Detected Pathing Errors
        case 0x70: return "Host Pathing Error";
        case 0x71: return "Command Aborted By Host";
        //   0x72-0x7f: Reserved
        //   0x80-0xbf: I/O Command Set Specific
        //   0xc0-0xff: Vendor Specific
      }
      break;

    //   0x4-0x6: Reserved
    //   0x7: Vendor Specific
  }
  return nullptr;
}

// Return errno for NVMe status SCT/SC fields: 0, EINVAL or EIO.
int nvme_status_to_errno(uint16_t status)
{
  if (!nvme_status_is_error(status))
    return 0;
  const char * s = nvme_status_to_flagged_str(status);
  if (s && *s == '-')
    return EINVAL;
  return EIO;
}

// Return error message for NVMe status SCT/SC fields or nullptr if unknown.
const char * nvme_status_to_str(uint16_t status)
{
  const char * s = nvme_status_to_flagged_str(status);
  return (s && *s == '-' ? s + 1 : s);
}

// Return error message for NVMe status SCT/SC fields or explanatory message if unknown.
const char * nvme_status_to_info_str(char * buf, size_t bufsize, uint16_t status)
{
  const char * s = nvme_status_to_str(status);
  if (s)
    return s;

  uint8_t sct = (status >> 8) & 0x7, sc = (uint8_t)status;
  const char * pfx = (sc >= 0xc0 ? "Vendor Specific " : "Unknown ");
  switch (sct) {
    case 0x0: s = "Generic Command Status"; break;
    case 0x1: s = "Command Specific Status"; break;
    case 0x2: s = "Media and Data Integrity Error"; break;
    case 0x3: s = "Path Related Status"; break;
    case 0x7: s = "Vendor Specific Status"; pfx = ""; break;
  }
  if (s)
    snprintf(buf, bufsize, "%s%s 0x%02x", pfx, s, sc);
  else
    snprintf(buf, bufsize, "Unknown Status 0x%x/0x%02x", sct, sc);
  return buf;
}
