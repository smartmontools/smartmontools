/*
 * scsinvme.cpp
 *
 * Home page of code is: https://www.smartmontools.org
 *
 * Copyright (C) 2020 Christian Franke
 * Copyright (C) 2018 Harry Mallon <hjmallon@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 */

#include "config.h"

#include "dev_interface.h"
#include "dev_tunnelled.h"
#include "nvmecmds.h"
#include "scsicmds.h"
#include "sg_unaligned.h"
#include "utility.h"

#include <errno.h>

// SNT (SCSI NVMe Translation) namespace and prefix
namespace snt {

/////////////////////////////////////////////////////////////////////////////
// sntjmicron_device

#define SNT_JMICRON_NVME_SIGNATURE 0x454d564eu // 'NVME' reversed (little endian)
#define SNT_JMICRON_CDB_LEN 12
#define SNT_JMICRON_NVM_CMD_LEN 512

class sntjmicron_device
: public tunnelled_device<
    /*implements*/ nvme_device,
    /*by tunnelling through a*/ scsi_device
  >
{
public:
  sntjmicron_device(smart_interface * intf, scsi_device * scsidev,
                    const char * req_type, unsigned nsid);

  virtual ~sntjmicron_device() throw();

  virtual bool open();

  virtual bool nvme_pass_through(const nvme_cmd_in & in, nvme_cmd_out & out);

private:
  enum {
    proto_nvm_cmd = 0x0, proto_non_data = 0x1, proto_dma_in = 0x2,
    proto_dma_out = 0x3, proto_response = 0xF
  };
};

sntjmicron_device::sntjmicron_device(smart_interface * intf, scsi_device * scsidev,
                                     const char * req_type, unsigned nsid)
: smart_device(intf, scsidev->get_dev_name(), "sntjmicron", req_type),
  tunnelled_device<nvme_device, scsi_device>(scsidev, nsid)
{
  set_info().info_name = strprintf("%s [USB NVMe JMicron]", scsidev->get_info_name());
}

sntjmicron_device::~sntjmicron_device() throw()
{
}

bool sntjmicron_device::open()
{
  // Open USB first
  if (!tunnelled_device<nvme_device, scsi_device>::open())
    return false;

  // No sure how multiple namespaces come up on device so we
  // cannot detect e.g. /dev/sdX is NSID 2.
  // Set to broadcast if not available
  if (!get_nsid()) {
    set_nsid(0xFFFFFFFF);
  }

  return true;
}

// cdb[0]: ATA PASS THROUGH (12) SCSI command opcode byte (0xa1)
// cdb[1]: [ is admin cmd: 1 ] [ protocol : 7 ]
// cdb[2]: reserved
// cdb[3]: parameter list length (23:16)
// cdb[4]: parameter list length (15:08)
// cdb[5]: parameter list length (07:00)
// cdb[6]: reserved
// cdb[7]: reserved
// cdb[8]: reserved
// cdb[9]: reserved
// cdb[10]: reserved
// cdb[11]: CONTROL (?)
bool sntjmicron_device::nvme_pass_through(const nvme_cmd_in & in, nvme_cmd_out & out)
{
  /* Only admin commands used */
  bool admin = true;

  // 1: "NVM Command Set Payload"
  {
    unsigned char cdb[SNT_JMICRON_CDB_LEN] = { 0 };
    cdb[0] = SAT_ATA_PASSTHROUGH_12;
    cdb[1] = (admin ? 0x80 : 0x00) | proto_nvm_cmd;
    sg_put_unaligned_be24(SNT_JMICRON_NVM_CMD_LEN, &cdb[3]);

    unsigned nvm_cmd[SNT_JMICRON_NVM_CMD_LEN / sizeof(unsigned)] = { 0 };
    nvm_cmd[0] = SNT_JMICRON_NVME_SIGNATURE;
    // nvm_cmd[1]: reserved
    nvm_cmd[2] = in.opcode; // More of CDW0 may go in here in future
    nvm_cmd[3] = in.nsid;
    // nvm_cmd[4-5]: reserved
    // nvm_cmd[6-7]: metadata pointer
    // nvm_cmd[8-11]: data ptr (?)
    nvm_cmd[12] = in.cdw10;
    nvm_cmd[13] = in.cdw11;
    nvm_cmd[14] = in.cdw12;
    nvm_cmd[15] = in.cdw13;
    nvm_cmd[16] = in.cdw14;
    nvm_cmd[17] = in.cdw15;
    // nvm_cmd[18-127]: reserved

    if (isbigendian())
      for (unsigned i = 0; i < (SNT_JMICRON_NVM_CMD_LEN / sizeof(uint32_t)); i++)
        swapx(&nvm_cmd[i]);

    scsi_cmnd_io io_nvm;
    memset(&io_nvm, 0, sizeof(io_nvm));

    io_nvm.cmnd = cdb;
    io_nvm.cmnd_len = SNT_JMICRON_CDB_LEN;
    io_nvm.dxfer_dir = DXFER_TO_DEVICE;
    io_nvm.dxferp = (uint8_t *)nvm_cmd;
    io_nvm.dxfer_len = SNT_JMICRON_NVM_CMD_LEN;

    scsi_device * scsidev = get_tunnel_dev();
    if (!scsidev->scsi_pass_through_and_check(&io_nvm,
         "sntjmicron_device::nvme_pass_through:NVM: "))
      return set_err(scsidev->get_err());
  }

  // 2: DMA or Non-Data
  {
    unsigned char cdb[SNT_JMICRON_CDB_LEN] = { 0 };
    cdb[0] = SAT_ATA_PASSTHROUGH_12;

    scsi_cmnd_io io_data;
    memset(&io_data, 0, sizeof(io_data));
    io_data.cmnd = cdb;
    io_data.cmnd_len = SNT_JMICRON_CDB_LEN;

    switch (in.direction()) {
      case nvme_cmd_in::no_data:
        cdb[1] = (admin ? 0x80 : 0x00) | proto_non_data;
        io_data.dxfer_dir = DXFER_NONE;
        break;
      case nvme_cmd_in::data_out:
        cdb[1] = (admin ? 0x80 : 0x00) | proto_dma_out;
        sg_put_unaligned_be24(in.size, &cdb[3]);
        io_data.dxfer_dir = DXFER_TO_DEVICE;
        io_data.dxferp = (uint8_t *)in.buffer;
        io_data.dxfer_len = in.size;
        break;
      case nvme_cmd_in::data_in:
        cdb[1] = (admin ? 0x80 : 0x00) | proto_dma_in;
        sg_put_unaligned_be24(in.size, &cdb[3]);
        io_data.dxfer_dir = DXFER_FROM_DEVICE;
        io_data.dxferp = (uint8_t *)in.buffer;
        io_data.dxfer_len = in.size;
        memset(in.buffer, 0, in.size);
        break;
      case nvme_cmd_in::data_io:
      default:
        return set_err(EINVAL);
    }

    scsi_device * scsidev = get_tunnel_dev();
    if (!scsidev->scsi_pass_through_and_check(&io_data,
         "sntjmicron_device::nvme_pass_through:Data: "))
      return set_err(scsidev->get_err());
  }

  // 3: "Return Response Information"
  {
    unsigned char cdb[SNT_JMICRON_CDB_LEN] = { 0 };
    cdb[0] = SAT_ATA_PASSTHROUGH_12;
    cdb[1] = (admin ? 0x80 : 0x00) | proto_response;
    sg_put_unaligned_be24(SNT_JMICRON_NVM_CMD_LEN, &cdb[3]);

    unsigned nvm_reply[SNT_JMICRON_NVM_CMD_LEN / sizeof(unsigned)] = { 0 };

    scsi_cmnd_io io_reply;
    memset(&io_reply, 0, sizeof(io_reply));

    io_reply.cmnd = cdb;
    io_reply.cmnd_len = SNT_JMICRON_CDB_LEN;
    io_reply.dxfer_dir = DXFER_FROM_DEVICE;
    io_reply.dxferp = (uint8_t *)nvm_reply;
    io_reply.dxfer_len = SNT_JMICRON_NVM_CMD_LEN;

    scsi_device * scsidev = get_tunnel_dev();
    if (!scsidev->scsi_pass_through_and_check(&io_reply,
         "sntjmicron_device::nvme_pass_through:Reply: "))
      return set_err(scsidev->get_err());

    if (isbigendian())
      for (unsigned i = 0; i < (SNT_JMICRON_NVM_CMD_LEN / sizeof(uint32_t)); i++)
        swapx(&nvm_reply[i]);

    if (nvm_reply[0] != SNT_JMICRON_NVME_SIGNATURE)
      return set_err(EIO, "Out of spec JMicron NVMe reply");

    int status = nvm_reply[5] >> 17;

    if (status > 0)
      return set_nvme_err(out, status);

    out.result = nvm_reply[2];
  }

  return true;
}

/////////////////////////////////////////////////////////////////////////////
// sntrealtek_device

class sntrealtek_device
: public tunnelled_device<
    /*implements*/ nvme_device,
    /*by tunnelling through a*/ scsi_device
  >
{
public:
  sntrealtek_device(smart_interface * intf, scsi_device * scsidev,
                    const char * req_type, unsigned nsid);

  virtual ~sntrealtek_device() throw();

  virtual bool nvme_pass_through(const nvme_cmd_in & in, nvme_cmd_out & out);
};

sntrealtek_device::sntrealtek_device(smart_interface * intf, scsi_device * scsidev,
                                     const char * req_type, unsigned nsid)
: smart_device(intf, scsidev->get_dev_name(), "sntrealtek", req_type),
  tunnelled_device<nvme_device, scsi_device>(scsidev, nsid)
{
  set_info().info_name = strprintf("%s [USB NVMe Realtek]", scsidev->get_info_name());
}

sntrealtek_device::~sntrealtek_device() throw()
{
}

bool sntrealtek_device::nvme_pass_through(const nvme_cmd_in & in, nvme_cmd_out & /* out */)
{
  switch (in.opcode) {
    case smartmontools::nvme_admin_identify:
      if (in.cdw10 == 0x0000001) // Identify controller
        break;
      if (in.cdw10 == 0x0000000) { // Identify namespace
        if (in.nsid == 1)
          break;
        return set_err(ENOSYS, "NVMe Identify Namespace 0x%x not supported", in.nsid);
      }
      return set_err(ENOSYS, "NVMe Identify with CDW10=0x%08x not supported", in.cdw10);
    case smartmontools::nvme_admin_get_log_page:
      if (in.nsid == 0xffffffff || !in.nsid)
        break;
      return set_err(ENOSYS, "NVMe Get Log Page with NSID=0x%x not supported", in.nsid);
    default:
      return set_err(ENOSYS, "NVMe admin command 0x%02x not supported", in.opcode);
    break;
  }
  if (in.cdw11 || in.cdw12 || in.cdw13 || in.cdw14 || in.cdw15)
    return set_err(ENOSYS, "Nonzero NVMe command dwords 11-15 not supported");

  uint8_t cdb[16] = {0, };
  cdb[0] = 0xe4;
  sg_put_unaligned_le16(in.size, cdb+1);
  cdb[3] = in.opcode;
  cdb[4] = (uint8_t)in.cdw10;

  scsi_cmnd_io io_hdr; memset(&io_hdr, 0, sizeof(io_hdr));
  io_hdr.cmnd = cdb;
  io_hdr.cmnd_len = sizeof(cdb);
  io_hdr.dxfer_dir = DXFER_FROM_DEVICE;
  io_hdr.dxferp = (uint8_t *)in.buffer;
  io_hdr.dxfer_len = in.size;

  scsi_device * scsidev = get_tunnel_dev();
  if (!scsidev->scsi_pass_through_and_check(&io_hdr, "sntrealtek_device::nvme_pass_through: "))
    return set_err(scsidev->get_err());

  //out.result = ?; // TODO
  return true;
}


} // namespace snt

using namespace snt;

nvme_device * smart_interface::get_snt_device(const char * type, scsi_device * scsidev)
{
  if (!scsidev)
    throw std::logic_error("smart_interface: get_snt_device() called with scsidev=0");

  // Take temporary ownership of 'scsidev' to delete it on error
  scsi_device_auto_ptr scsidev_holder(scsidev);
  nvme_device * sntdev = 0;

  // TODO: Remove this and adjust drivedb entry accordingly when no longer EXPERIMENTAL
  if (!strcmp(type, "sntjmicron#please_try")) {
    set_err(EINVAL, "USB to NVMe bridge [please try '-d sntjmicron' and report result to: "
            PACKAGE_BUGREPORT "]");
    return 0;
  }

  if (!strncmp(type, "sntjmicron", 10)) {
    int n1 = -1, n2 = -1, len = strlen(type);
    unsigned nsid = 0; // invalid namespace id -> use default
    sscanf(type, "sntjmicron%n,0x%x%n", &n1, &nsid, &n2);
    if (!(n1 == len || n2 == len)) {
      set_err(EINVAL, "Invalid NVMe namespace id in '%s'", type);
      return 0;
    }
    sntdev = new sntjmicron_device(this, scsidev, type, nsid);
  }

  else if (!strcmp(type, "sntrealtek")) {
    // No namespace supported
    sntdev = new sntrealtek_device(this, scsidev, type, 0xffffffff);
  }

  else {
    set_err(EINVAL, "Unknown SNT device type '%s'", type);
    return 0;
  }

  // 'scsidev' is now owned by 'sntdev'
  scsidev_holder.release();
  return sntdev;
}
