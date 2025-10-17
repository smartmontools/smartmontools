/*
 * lsdisk.cpp - list disk identify information (libsmartmon example program)
 *
 * Home page of code is: https://www.smartmontools.org
 *
 * Copyright (C) 2025 Christian Franke
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <smartmon/dev_interface.h>
#include <smartmon/atacmds.h>
#include <smartmon/nvmecmds.h>
#include <smartmon/scsicmds.h>
#include <smartmon/utility.h>
#include <smartmon/sg_unaligned.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>

static void usage(const char * prog)
{
  std::printf("%s\n"
    "Usage: %s [-d TYPE] [-r LEVEL] [DEVICE...]\n\n"
    "    -d TYPE    Specify device type ('-d help' for valid TYPEs)\n"
    "    -r LEVEL   Specify debug level\n"
    "    -h         Print this help\n"
    "    -V         Print version information\n",
    format_version_info("lsdisk").c_str(), prog);
}

static int dev_error(const smart_device * dev, const char * msg)
{
  std::fprintf(stderr, "%s: %s: %s\n", dev->get_info_name(), msg, dev->get_errmsg());
  return 1;
}

static int identify(ata_device * dev)
{
  ata_identify_device id{};
  int atapi = ata_read_identity(dev->to_ata(), &id, false, nullptr);
  if (atapi < 0)
    return dev_error(dev, "ata_read_identity() failed");

  ata_size_info sizes;
  ata_get_size_info(&id, sizes);
  char cap[32]{};
  if (sizes.capacity)
    format_capacity(cap, sizeof(cap), sizes.capacity);

  char md[40 + 1], fw[8 + 1], sn[20 + 1];
  ata_format_id_string(md, id.model, sizeof(md) - 1);
  ata_format_id_string(fw, id.fw_rev, sizeof(fw) - 1);
  ata_format_id_string(sn, id.serial_no, sizeof(sn) - 1);
  std::printf("%s -d %s [ATA%s]: \"%s\", FW:\"%s\", S/N:\"%s\"%s%s\n",
    dev->get_info_name(), dev->get_dev_type(),
    (!atapi ? "" : "PI"), md, fw, sn, (cap[0] ? ", " : ""), cap);
  return 0;
}

static int identify(scsi_device * dev)
{
  char inq[36]{};
  if (scsiStdInquiry(dev, (uint8_t *)inq, sizeof(inq)))
    return dev_error(dev, "scsiStdInquiry() failed");

  char cap[32]{};
  uint64_t sz = scsiGetSize(dev, false, nullptr);
  if (sz)
    format_capacity(cap, sizeof(cap), sz);

  char vn[8 + 1], pr[16 + 1], rv[4 + 1];
  std::printf("%s -d %s [SCSI]: \"%s\" \"%s\", Rev:\"%s\"%s%s\n",
    dev->get_info_name(), dev->get_dev_type(),
    format_char_array(vn, inq +  8,  8),
    format_char_array(pr, inq + 16, 16),
    format_char_array(rv, inq + 32,  4),
    (cap[0] ? ", " : ""), cap);
  return 0;
}

static int identify(nvme_device * dev)
{
  smartmontools::nvme_id_ctrl id_ctrl{};
  if (!nvme_read_id_ctrl(dev, id_ctrl))
    return dev_error(dev, "nvme_read_id_ctrl() failed");

  char ns[32]{};
  uint32_t nsid = dev->get_nsid();
  if (nsid != nvme_broadcast_nsid)
    std::snprintf(ns, sizeof(ns), ", NS:%u", (unsigned)nsid);

  char cap[32]{};
  uint64_t tnvmcap = sg_get_unaligned_le64(id_ctrl.tnvmcap);
  if (tnvmcap)
    format_capacity(cap, sizeof(cap), tnvmcap);

  char mn[40 + 1], fr[8 + 1], sn[20 + 1];
  std::printf("%s -d %s [NVMe]: \"%s\", FW:\"%s\", S/N:\"%s\"%s%s%s\n",
    dev->get_info_name(), dev->get_dev_type(),
    format_char_array(mn, id_ctrl.mn),
    format_char_array(fr, id_ctrl.fr),
    format_char_array(sn, id_ctrl.sn),
    ns, (cap[0] ? ", " : ""), cap);
  return 0;
}

static int identify(std::unique_ptr<smart_device> & dev)
{
  if (!smart_device::autodetect_open(dev))
    return dev_error(dev.get(), "autodetect_open() failed");

  int status = 0, cnt = 0;
  if (dev->is_ata())
    status |= identify(dev->to_ata()), cnt++;
  if (dev->is_scsi())
    status |= identify(dev->to_scsi()), cnt++;
  if (dev->is_nvme())
    status |= identify(dev->to_nvme()), cnt++;
  if (!cnt) {
    std::fprintf(stderr, "%s: unknown device type '%s'\n",
      dev->get_info_name(), dev->get_dev_type());
    status = 1;
  }
  dev->close();
  return status;
}

int main(int argc, char **argv)
{
  try {
    smart_interface::init();

    const char * type = nullptr;
    int ai;
    for (ai = 1; ai < argc && argv[ai][0] == '-'; ai++) {
      if (!std::strcmp(argv[ai], "-d") && ai + 1 < argc) {
        type = argv[++ai];
        if (!std::strcmp(type, "help")) {
           std::printf("Valid arguments to '-d':\n"
             "%s\n", smi()->get_valid_dev_types_str().c_str());
           return 0;
        }
      }
      else if (!std::strcmp(argv[ai], "-r") && ai + 1 < argc) {
        ata_debugmode = scsi_debugmode = nvme_debugmode = std::atoi(argv[++ai]);
      }
      else if (!std::strcmp(argv[ai], "-h")) {
        usage(argv[0]);
        return 0;
      }
      else if (!std::strcmp(argv[ai], "-V")) {
        std::fputs(format_version_info("lsdisk", 3).c_str(), stdout);
        return 0;
      }
      else {
        usage(argv[0]);
        return 1;
      }
    }

    int status = 0;
    if (ai < argc) {
      do {
        const char * name = argv[ai];
        std::unique_ptr<smart_device> dev( smi()->get_smart_device(name, type) );
        if (!dev) {
          std::fprintf(stderr, "%s: get_smart_device() failed: %s\n", name, smi()->get_errmsg());
          status |= 1;
          continue;
        }

        status |= identify(dev);
      } while (++ai < argc);
    }
    else {
      smart_device_list devs{};
      if (!smi()->scan_smart_devices(devs, type)) {
        std::fprintf(stderr, "scan_smart_devices() failed: %s", smi()->get_errmsg());
        return 1;
      }

      for (unsigned i = 0; i < devs.size(); i++) {
        std::unique_ptr<smart_device> dev( devs.release(i) );
        status |= identify(dev);
      }
    }
    return status;
  }
  catch (std::exception & ex) {
    std::fprintf(stderr, "Exception: %s\n", ex.what());
    return 1;
  }
}
