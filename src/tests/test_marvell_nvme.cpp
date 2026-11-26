/*
 * tests/test_marvell_nvme.cpp
 *
 * Home page of code is: https://www.smartmontools.org
 *
 * Copyright (C) 2026 Gianluigi Tiesi <sherpya@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

// Unit test of '-d marvell-nvme' with a simulated controller.

#include "config.h"

#include <smartmon/dev_interface.h>
#include <smartmon/nvmecmds.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

using namespace smartmon;

#define CHECK(x) check((x), #x, __LINE__)

static void check(bool ok, const char * expr, int line)
{
  if (ok)
    return;
  printf("test_marvell_nvme.cpp:%d: Check failed: %s\n", line, expr);
  exit(1);
}

// Simulated Marvell 88NR2241 controller with backend slots 0 and 1
class sim_controller
: public nvme_device
{
public:
  explicit sim_controller(smart_interface * intf)
    : smart_device(intf, "/dev/sim", "nvme", "nvme"),
      nvme_device(nvme_broadcast_nsid)
    { }

  virtual bool is_open() const override
    { return opened; }

  virtual bool open() override
    { opened = true; return true; }

  virtual bool close() override
    { opened = false; return true; }

  virtual bool nvme_pass_through(const nvme_cmd_in & in, nvme_cmd_out & out) override;

  bool opened = false;
  bool marvell = true;  // Frontend reports Marvell VID and model
  int tunnel_cmds = 0;  // Number of tunnel commands received
  nvme_cmd_in last;     // Last tunnel command
};

bool sim_controller::nvme_pass_through(const nvme_cmd_in & in, nvme_cmd_out & out)
{
  CHECK(opened);
  auto data = static_cast<unsigned char *>(in.buffer);
  memset(data, 0, in.size);

  if (in.opcode == nvme_admin_identify) {
    // Identify Controller of frontend
    CHECK(in.nsid == 0 && in.cdw10 == 0x01 && in.size == 4096);
    const char * model = (marvell ? "Marvell_NVMe_Controller" : "Other_NVMe_Controller");
    data[0] = 0x4b; data[1] = 0x1b;
    memset(data + 24, ' ', 40);
    memcpy(data + 24, model, strlen(model));
    return true;
  }

  CHECK(in.opcode == 0xc2);
  tunnel_cmds++;
  last = in;
  if (in.nsid > 1)
    return set_nvme_err(out, 0x0b); // Empty slot
  data[0] = 0x5a; data[in.size - 1] = 0xa5;
  out.result = 0x1234;
  return true;
}

class test_interface
: public smart_interface
{
public:
  sim_controller * sim = nullptr; // Last created controller

protected:
  virtual ata_device * get_ata_device(const char *, const char *) override
    { return nullptr; }

  virtual scsi_device * get_scsi_device(const char *, const char *) override
    { return nullptr; }

  virtual nvme_device * get_nvme_device(const char *, const char *, unsigned) override
    { return (sim = new sim_controller(this)); }

  virtual smart_device * autodetect_smart_device(const char *) override
    { return nullptr; }
};

static void test_types(test_interface & intf)
{
  static const char * const valid[] = {
    "marvell-nvme,0", "marvell-nvme,255", "marvell-nvme,1,0x1", "marvell-nvme,1,0xffffffff"
  };
  for (const char * type : valid) {
    smart_device_auto_ptr dev(intf.get_smart_device("/dev/sim", type));
    CHECK(dev && dev->is_nvme());
  }

  static const char * const invalid[] = {
    "marvell-nvme", "marvell-nvme,", "marvell-nvme,x", "marvell-nvme,-1",
    "marvell-nvme,256", "marvell-nvme,0,", "marvell-nvme,0,1", "marvell-nvme,0,0x0",
    "marvell-nvme,0,0x1,", "marvell-nvme0"
  };
  for (const char * type : invalid) {
    smart_device_auto_ptr dev(intf.get_smart_device("/dev/sim", type));
    CHECK(!dev && intf.get_errno() == EINVAL);
  }
}

static void test_frontend(test_interface & intf)
{
  smart_device_auto_ptr dev(intf.get_smart_device("/dev/sim", "marvell-nvme,0"));
  intf.sim->marvell = false;
  CHECK(!dev->open() && dev->get_errno() == ENODEV);
  CHECK(!intf.sim->is_open() && !intf.sim->tunnel_cmds);
}

static void test_tunnel(test_interface & intf)
{
  smart_device_auto_ptr dev(intf.get_smart_device("/dev/sim", "marvell-nvme,1,0x1"));
  const sim_controller * sim = intf.sim;
  nvme_device * nvmedev = dev->to_nvme();
  CHECK(nvmedev->get_nsid() == 1);
  CHECK(dev->open());

  nvme_id_ctrl id;
  CHECK(nvme_read_id_ctrl(nvmedev, id) && id.vid == 0x005a);
  const nvme_cmd_in & last = sim->last;
  CHECK(sim->tunnel_cmds == 1 && last.nsid == 1 && last.size == 4096);
  CHECK(last.cdw10 == 0x01 && last.cdw14 == nvme_admin_identify && last.cdw15 == 0);

  nvme_smart_log smart;
  CHECK(nvme_read_smart_log(nvmedev, nvme_broadcast_nsid, smart));
  CHECK(sim->tunnel_cmds == 2 && last.nsid == 1 && last.size == 512);
  CHECK(last.cdw10 == 0x007f0002 && last.cdw14 == nvme_admin_get_log_page);
  CHECK(last.cdw15 == nvme_broadcast_nsid);
  CHECK(!last.cdw11 && !last.cdw12 && !last.cdw13);

  // Other commands and unsupported fields are rejected
  unsigned char buf[512];
  nvme_cmd_in in;
  nvme_cmd_out out;
  in.set_data_in(0x0a, buf, sizeof(buf)); // Get Features
  CHECK(!nvmedev->nvme_pass_through(in, out) && nvmedev->get_errno() == ENOSYS);
  in.set_data_in(nvme_admin_get_log_page, buf, sizeof(buf));
  in.cdw10 = 0x007f0002; in.cdw12 = 512; // Log page offset
  CHECK(!nvmedev->nvme_pass_through(in, out) && nvmedev->get_errno() == ENOSYS);
  CHECK(sim->tunnel_cmds == 2);

  // NVMe status of empty slot is returned
  smart_device_auto_ptr dev2(intf.get_smart_device("/dev/sim", "marvell-nvme,2"));
  CHECK(dev2->open());
  in.cdw12 = 0;
  CHECK(!dev2->to_nvme()->nvme_pass_through(in, out));
  CHECK(out.status_valid && out.status == 0x0b);
}

int main()
{
  test_interface intf;
  int objects = smart_device::get_num_objects();

  test_types(intf);
  test_frontend(intf);
  test_tunnel(intf);
  CHECK(smart_device::get_num_objects() == objects);

  printf("test_marvell_nvme: OK\n");
  return 0;
}
