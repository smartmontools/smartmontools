/*
 * dev_marvell_nvme.cpp
 *
 * Home page of code is: https://www.smartmontools.org
 *
 * Copyright (C) 2026 Gianluigi Tiesi <sherpya@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

// NVMe SSDs behind a Marvell 88NR2241 NVMe RAID controller.
//
// The controller presents itself as a single NVMe controller. Admin
// commands are tunnelled to a backend SSD with vendor specific admin
// command 0xc2, as done by the vendor tool (mnv_cli):
//   NSID:      backend slot (RC/Slot ID)
//   CDW10:     CDW10 of the tunnelled command
//   CDW14:     opcode of the tunnelled command
//   CDW15:     NSID of the tunnelled command
// Only Identify and Get Log Page are passed through.

#include "config.h"

#include <smartmon/dev_interface.h>
#include "dev_tunnelled.h"
#include <smartmon/nvmecmds.h>
#include <smartmon/utility.h>

#include <errno.h>
#include <stdio.h>
#include <string.h>

namespace smartmon {

const unsigned char marvell_nvme_admin_tunnel = 0xc2;

class marvell_nvme_device
: public tunnelled_device<nvme_device, nvme_device>
{
public:
  marvell_nvme_device(smart_interface * intf, nvme_device * nvmedev,
    const char * req_type, unsigned slot, unsigned nsid);

  virtual bool open() override;

  virtual bool nvme_pass_through(const nvme_cmd_in & in, nvme_cmd_out & out) override;

private:
  unsigned m_slot;
};

marvell_nvme_device::marvell_nvme_device(smart_interface * intf, nvme_device * nvmedev,
  const char * req_type, unsigned slot, unsigned nsid)
: smart_device(intf, nvmedev->get_dev_name(), "marvell-nvme", req_type),
  tunnelled_device<nvme_device, nvme_device>(nvmedev, nsid),
  m_slot(slot)
{
  set_info().info_name = strprintf("%s [Marvell NVMe slot %u]", nvmedev->get_info_name(), slot);
}

bool marvell_nvme_device::open()
{
  if (!tunnelled_device<nvme_device, nvme_device>::open())
    return false;

  // Do not send the vendor command to any other controller
  nvme_device * nvmedev = get_tunnel_dev();
  nvme_id_ctrl id;
  if (!nvme_read_id_ctrl(nvmedev, id)) {
    error_info err = nvmedev->get_err();
    close();
    return set_err(err);
  }

  static const char model[] = "Marvell_NVMe_Controller ";
  if (!(id.vid == 0x1b4b && !memcmp(id.mn, model, sizeof(model) - 1))) {
    close();
    return set_err(ENODEV, "Not a Marvell 88NR2241 NVMe controller");
  }

  return true;
}

bool marvell_nvme_device::nvme_pass_through(const nvme_cmd_in & in, nvme_cmd_out & out)
{
  switch (in.opcode) {
    case nvme_admin_identify:
    case nvme_admin_get_log_page:
      break;
    default:
      return set_err(ENOSYS, "NVMe admin command 0x%02x not supported", in.opcode);
  }
  // CDW14/15 are used by the tunnel, passing of CDW11-13 is unknown
  if (in.cdw11 || in.cdw12 || in.cdw13 || in.cdw14 || in.cdw15)
    return set_err(ENOSYS, "Nonzero NVMe command dwords 11-15 not supported");

  nvme_cmd_in tin;
  tin.set_data_in(marvell_nvme_admin_tunnel, in.buffer, in.size);
  tin.nsid = m_slot;
  tin.cdw10 = in.cdw10;
  tin.cdw14 = in.opcode;
  tin.cdw15 = in.nsid;

  nvme_device * nvmedev = get_tunnel_dev();
  if (!nvmedev->nvme_pass_through(tin, out))
    return set_err(nvmedev->get_err());
  return true;
}

nvme_device * smart_interface::get_marvell_nvme_device(const char * type, nvme_device * nvmedev)
{
  // Take temporary ownership of 'nvmedev' to delete it on error
  nvme_device_auto_ptr nvmedev_holder(nvmedev);

  unsigned slot = ~0U, nsid = nvme_broadcast_nsid;
  int n1 = -1, n2 = -1, len = strlen(type);
  sscanf(type, "marvell-nvme,%u%n,0x%x%n", &slot, &n1, &nsid, &n2);
  if (!((n1 == len || n2 == len) && slot <= 255 && nsid))
    return set_err_np(EINVAL, "Invalid type '%s', use 'marvell-nvme,N[,0xNSID]'", type);

  nvme_device * dev = new marvell_nvme_device(this, nvmedev, type, slot, nsid);
  nvmedev_holder.release();
  return dev;
}

} // namespace smartmon
