/*
 * nvmecmds.cpp
 *
 * Home page of code is: https://www.smartmontools.org
 *
 * Copyright (C) 2016-21 Christian Franke
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

  if (   dont_print_serial_number && ok
      && in.opcode == nvme_admin_identify && in.cdw10 == 0x01) {
        // Invalidate serial number
        nvme_id_ctrl & id_ctrl = *reinterpret_cast<nvme_id_ctrl *>(in.buffer);
        memset(id_ctrl.sn, 'X', sizeof(id_ctrl.sn));
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
  unsigned n = nvme_read_log_page(device, 0xffffffff, 0x01, error_log,
                                  num_entries * sizeof(*error_log), lpo_sup);

  if (isbigendian()) {
    for (unsigned i = 0; i < n; i++) {
      swapx(&error_log[i].error_count);
      swapx(&error_log[i].sqid);
      swapx(&error_log[i].cmdid);
      swapx(&error_log[i].status_field);
      swapx(&error_log[i].parm_error_location);
      swapx(&error_log[i].lba);
      swapx(&error_log[i].nsid);
    }
  }

  return n / sizeof(*error_log);
}

// Read NVMe SMART/Health Information log.
bool nvme_read_smart_log(nvme_device * device, nvme_smart_log & smart_log)
{
  if (!nvme_read_log_page_1(device, 0xffffffff, 0x02, &smart_log, sizeof(smart_log)))
    return false;

  if (isbigendian()) {
    swapx(&smart_log.warning_temp_time);
    swapx(&smart_log.critical_comp_time);
    for (int i = 0; i < 8; i++)
      swapx(&smart_log.temp_sensor[i]);
  }

  return true;
}
