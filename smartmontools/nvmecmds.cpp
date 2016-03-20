/*
 * nvmecmds.cpp
 *
 * Home page of code is: http://www.smartmontools.org
 *
 * Copyright (C) 2016 Christian Franke
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

#include "config.h"
#include "nvmecmds.h"

const char * nvmecmds_cvsid = "$Id$"
  NVMECMDS_H_CVSID;

#include "dev_interface.h"
#include "atacmds.h" // swapx(), ASSERT_*()
#include "scsicmds.h" // dStrHex()
#include "utility.h"

using namespace smartmontools;

// Check nvme_* struct sizes
ASSERT_SIZEOF_STRUCT(nvme_id_ctrl, 4096);
ASSERT_SIZEOF_STRUCT(nvme_error_log_page, 64);
ASSERT_SIZEOF_STRUCT(nvme_smart_log, 512);


// Print NVMe debug messages?
unsigned char nvme_debugmode = 0;

// Call NVMe pass-through and print debug info if requested.
static bool nvme_pass_through(nvme_device * device, const nvme_cmd_in & in,
  nvme_cmd_out & out)
{
  int64_t start_usec = -1;

  if (nvme_debugmode) {
    pout(" [NVMe call: opcode=0x%02x, size=0x%04x, nsid=0x%08x, cdw10=0x%08x",
      in.opcode, in.size, in.nsid, in.cdw10);
    if (in.cdw11 || in.cdw12 || in.cdw13 || in.cdw14 || in.cdw15)
      pout(",\n  cdw1x=0x%08x, 0x%08x, 0x%08x, 0x%08x, 0x%08x",
       in.cdw11, in.cdw12, in.cdw13, in.cdw14, in.cdw15);
    pout("]\n");

    start_usec = smi()->get_timer_usec();
  }

  bool ok = device->nvme_pass_through(in, out);

  if (nvme_debugmode) {
    if (start_usec >= 0) {
      int64_t duration_usec = smi()->get_timer_usec() - start_usec;
      if (duration_usec >= 500)
        pout("  [Duration: %.3fs]\n", duration_usec / 1000000.0);
    }

    if (!ok) {
      pout(" [NVMe call failed: ");
      if (out.status_valid)
        pout("NVMe Status=%04x", out.status);
      else
        pout("%s", device->get_errmsg());
    }
    else {
      pout(" [NVMe call succeeded: result=%08x", out.result);
      if (nvme_debugmode > 1 && in.direction() == nvme_cmd_in::data_in) {
        pout("\n");
        const unsigned limit = 512;
        dStrHex((const unsigned char *)in.buffer,
                (in.size <= limit ? in.size : limit), 0);
        if (in.size > limit)
          pout(" ...\n");
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
static bool nvme_read_identify(nvme_device * device, unsigned char cns, void * data, unsigned size)
{
  memset(data, 0, size);
  nvme_cmd_in in;
  in.set_data_in(nvme_admin_identify, data, size);
  in.nsid = device->get_nsid();
  in.cdw10 = cns;

  return nvme_pass_through(device, in);
}

// Read NVMe Identify Controller data structure.
bool nvme_read_id_ctrl(nvme_device * device, nvme_id_ctrl & id_ctrl)
{
  if (!nvme_read_identify(device, 0x1, &id_ctrl, sizeof(id_ctrl)))
    return false;

  if (isbigendian()) {
    swapx(&id_ctrl.vid);
    swapx(&id_ctrl.ssvid);
    swapx(&id_ctrl.cntlid);
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

// Read NVMe log page with identifier LID.
static bool nvme_read_log_page(nvme_device * device, unsigned char lid,
  void * data, unsigned size)
{
  if (!(size / 4))
    throw std::logic_error("nvme_read_log_page(): invalid size");

  memset(data, 0, size);
  nvme_cmd_in in;
  in.set_data_in(nvme_admin_get_log_page, data, size);
  in.nsid = device->get_nsid();
  in.cdw10 = lid | (((size / 4) - 1) << 16);

  return nvme_pass_through(device, in);
}

// Read NVMe Error Information Log.
bool nvme_read_error_log(nvme_device * device, nvme_error_log_page * error_log, unsigned num_entries)
{
  if (!nvme_read_log_page(device, 0x01, error_log, num_entries * sizeof(*error_log)))
    return false;

  if (isbigendian()) {
    for (unsigned i = 0; i < num_entries; i++) {
      swapx(&error_log[i].error_count);
      swapx(&error_log[i].sqid);
      swapx(&error_log[i].cmdid);
      swapx(&error_log[i].status_field);
      swapx(&error_log[i].parm_error_location);
      swapx(&error_log[i].lba);
      swapx(&error_log[i].nsid);
    }
  }

  return true;
}

// Read NVMe SMART/Health Information log.
bool nvme_read_smart_log(nvme_device * device, nvme_smart_log & smart_log)
{
  if (!nvme_read_log_page(device, 0x02, &smart_log, sizeof(smart_log)))
    return false;

  if (isbigendian()) {
    swapx(&smart_log.warning_temp_time);
    swapx(&smart_log.critical_comp_time);
    for (int i = 0; i < 8; i++)
      swapx(&smart_log.temp_sensor[i]);
  }

  return true;
}
