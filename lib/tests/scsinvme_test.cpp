/*
 * SNT response completeness with a simulated SCSI transport. No device I/O.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "config.h"
#include <algorithm>
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <vector>
#include <smartmon/dev_interface.h>
#include <smartmon/nvmecmds.h>
#include <smartmon/scsicmds.h>
#include <smartmon/sg_unaligned.h>

using namespace smartmon;

#define CHECK(condition) do { if (!(condition)) { \
  std::fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #condition); \
  std::exit(1); } } while (0)

class test_interface : public smart_interface
{
public:
  using smart_interface::get_snt_device;
private:
  ata_device * get_ata_device(const char *, const char *) override { return nullptr; }
  scsi_device * get_scsi_device(const char *, const char *) override { return nullptr; }
  smart_device * autodetect_smart_device(const char *) override { return nullptr; }
};

static uint8_t payload_byte(uint64_t offset)
{
  return (uint8_t)(offset ^ (offset >> 8) ^ (offset >> 16) ^ (offset >> 32));
}

class test_scsi_device : public scsi_device
{
public:
  explicit test_scsi_device(smart_interface * intf)
    : smart_device(intf, "simulated", "scsi", "scsi") { }
  bool is_open() const override { return true; }
  bool open() override { return true; }
  bool close() override { return true; }
  unsigned limit = UINT_MAX;
  int residue = INT_MIN;
  unsigned fail_call = 0;
  bool realtek = false;
  std::vector<uint64_t> offsets;
  std::vector<unsigned> sizes;
  bool scsi_pass_through(scsi_cmnd_io * io) override
  {
    CHECK(io->cmnd_len == 16 && io->cmnd[0] == (realtek ? 0xe4 : 0xe6));
    CHECK(io->dxfer_dir == DXFER_FROM_DEVICE);
    uint64_t offset = realtek ? 0 : sg_get_unaligned_be64(io->cmnd + 8);
    if (realtek) {
      CHECK(sg_get_unaligned_le16(io->cmnd + 1) == io->dxfer_len);
      CHECK(io->cmnd[3] == nvme_admin_identify || io->cmnd[3] == nvme_admin_get_log_page);
    }
    offsets.push_back(offset);
    sizes.push_back(io->dxfer_len);
    if (fail_call == sizes.size())
      return set_err(EIO, "simulated continuation error");
    if (!realtek && io->cmnd[1] == nvme_admin_get_log_page)
      CHECK((sg_get_unaligned_be16(io->cmnd + 6) + 1u) * 4 == io->dxfer_len);
    unsigned count = (unsigned)std::min((size_t)limit, io->dxfer_len);
    for (unsigned i = 0; i < count; ++i)
      ((uint8_t *)io->dxferp)[i] = payload_byte(offset + i);
    io->resid = (residue == INT_MIN ? (int)(io->dxfer_len - count) : residue);
    return true;
  }
};

int main()
{
  test_interface intf;
  auto * scsi = new test_scsi_device(&intf);
  smart_device_auto_ptr device(intf.get_snt_device("sntasmedia", scsi));
  CHECK(device && device->is_nvme());
  std::vector<uint8_t> buffer;
  auto execute = [&](unsigned size, uint64_t offset = 0,
                     unsigned opcode = nvme_admin_get_log_page) {
    scsi->offsets.clear();
    scsi->sizes.clear();
    buffer.assign(size, 0xa5);
    nvme_cmd_in in;
    in.set_data_in(opcode, buffer.data(), size);
    in.nsid = nvme_broadcast_nsid;
    in.cdw10 = (opcode == nvme_admin_identify ? 1 : 6 | ((size / 4 - 1) << 16));
    in.cdw12 = (uint32_t)offset;
    in.cdw13 = (uint32_t)(offset >> 32);
    nvme_cmd_out out;
    return device->to_nvme()->nvme_pass_through(in, out);
  };
  auto check_payload = [&](uint64_t offset = 0) {
    for (size_t i = 0; i < buffer.size(); ++i)
      CHECK(buffer[i] == payload_byte(offset + i));
  };

  CHECK(execute(4096, 0, nvme_admin_identify) && scsi->sizes.size() == 1);
  check_payload();
  CHECK(execute(512) && scsi->sizes.size() == 1);
  check_payload();
  scsi->limit = 512;
  CHECK(execute(1024));
  CHECK(scsi->offsets == std::vector<uint64_t>({0, 512}));
  CHECK(scsi->sizes == std::vector<unsigned>({1024, 512}));
  check_payload();
  CHECK(execute(564) && scsi->sizes == std::vector<unsigned>({564, 52}));
  check_payload();
  CHECK(execute(4096) && scsi->sizes.size() == 8);
  check_payload();
  CHECK(execute(1024, 0xffffff00ULL));
  CHECK(scsi->offsets == std::vector<uint64_t>({0xffffff00ULL, 0x100000100ULL}));
  check_payload(0xffffff00ULL);
  CHECK(!execute(1024, UINT64_MAX - 255) && scsi->sizes.size() == 1);
  CHECK(!execute(4096, 0, nvme_admin_identify) && scsi->sizes.size() == 1);
  scsi->limit = 0;
  CHECK(!execute(1024) && scsi->sizes.size() == 1);
  scsi->limit = 511;
  CHECK(!execute(1024) && scsi->sizes.size() == 1);
  for (int invalid : {-1, 1025}) {
    scsi->residue = invalid;
    CHECK(!execute(1024) && scsi->sizes.size() == 1);
  }
  scsi->residue = INT_MIN;
  scsi->limit = 512;
  scsi->fail_call = 2;
  CHECK(!execute(1024) && scsi->sizes.size() == 2);
  CHECK(std::string(device->get_errmsg()) == "simulated continuation error");

  auto * realtek = new test_scsi_device(&intf);
  realtek->realtek = true;
  smart_device_auto_ptr rtl_device(intf.get_snt_device("sntrealtek", realtek));
  nvme_device * rtl = rtl_device->to_nvme();
  CHECK(rtl->get_log_page_size_limit() == 512);
  CHECK(device->to_nvme()->get_log_page_size_limit() == 4096);
  nvme_id_ctrl controller;
  CHECK(nvme_read_id_ctrl(rtl, controller));
  nvme_smart_log health;
  CHECK(nvme_read_smart_log(rtl, nvme_broadcast_nsid, health));
  realtek->sizes.clear();
  nvme_error_log_page errors[16];
  CHECK(nvme_read_error_log(rtl, errors, 16, true) == 8);
  CHECK(realtek->sizes == std::vector<unsigned>{512});
  CHECK(std::string(rtl->get_errmsg()) == "Nonzero NVMe command dwords 11-15 not supported");
  CHECK(nvme_read_error_log(rtl, errors, 8, false) == 8);
  nvme_self_test_log selftest;
  realtek->sizes.clear();
  CHECK(!nvme_read_self_test_log(rtl, nvme_broadcast_nsid, selftest));
  CHECK(realtek->sizes.empty());
  CHECK(std::string(rtl->get_errmsg()).find("limited to 512 bytes") != std::string::npos);
  // Short or invalid SCSI residue must not manufacture Identify/SMART data.
  for (int residue : {1, 512, -1, 4097}) {
    realtek->residue = residue;
    CHECK(!nvme_read_id_ctrl(rtl, controller));
    CHECK(!nvme_read_smart_log(rtl, nvme_broadcast_nsid, health));
  }
  realtek->residue = INT_MIN;
  realtek->limit = 0;
  CHECK(!nvme_read_smart_log(rtl, nvme_broadcast_nsid, health));
  return 0;
}
