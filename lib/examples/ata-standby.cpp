/*
 * ata-standby.cpp - put ATA/SATA disk in standby mode  (libsmartmon example program)
 *
 * Home page of code is: https://www.smartmontools.org
 *
 * Copyright (C) 2025 Christian Franke
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <smartmon/dev_interface.h>
#include <smartmon/atacmds.h>
#include <smartmon/scsicmds.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>

static int usage(const char * prog, int status)
{
  std::printf("%s\n"
    "Put ATA/SATA disk in standby mode\n\n"
    "Usage: %s [-d TYPE] [-r LEVEL] DEVICE\n\n"
    "    -d TYPE    Specify device type ('-d help' for valid TYPEs)\n"
    "    -r LEVEL   Specify debug level\n"
    "    -h         Print this help\n"
    "    -V         Print version information\n",
    smartmon::format_version_info("ata-standby").c_str(), prog);
    return status;
}

int main(int argc, char **argv)
{
  try {
    smartmon::smart_interface::init();

    const char * type = nullptr;
    int ai;
    for (ai = 1; ai < argc && argv[ai][0] == '-'; ai++) {
      if (!std::strcmp(argv[ai], "-d") && ai + 1 < argc) {
        type = argv[++ai];
        if (!std::strcmp(type, "help")) {
           std::printf("Valid arguments to '-d':\n"
             "%s\n", smartmon::smi()->get_valid_dev_types_str().c_str());
           return 0;
        }
      }
      else if (!std::strcmp(argv[ai], "-r") && ai + 1 < argc) {
        smartmon::ata_debugmode = smartmon::scsi_debugmode = std::atoi(argv[++ai]);
      }
      else if (!std::strcmp(argv[ai], "-h")) {
        return usage(argv[0], 0);
      }
      else if (!std::strcmp(argv[ai], "-V")) {
        std::fputs(smartmon::format_version_info("ata-standby", 3).c_str(), stdout);
        return 0;
      }
      else {
        return usage(argv[0], 1);
      }
    }
    if (ai + 1 != argc)
      return usage(argv[0], 1);

    const char * name = argv[ai];
    std::unique_ptr<smartmon::smart_device> dev( smartmon::smi()->get_smart_device(name, type) );
    if (!dev) {
      std::fprintf(stderr, "%s: get_smart_device() failed: %s\n", name,
        smartmon::smi()->get_errmsg());
      return 1;
    }

    name = dev->get_info_name();
    if (!smartmon::smart_device::autodetect_open(dev)) {
      std::fprintf(stderr, "%s: autodetect_open() failed: %s\n", name, dev->get_errmsg());
      return 1;
    }
    if (!dev->is_ata()) {
      std::fprintf(stderr, "%s: is not an ATA/SATA device\n", name);
      return 1;
    }

    if (!ata_nodata_command(dev->to_ata(), ATA_STANDBY_IMMEDIATE)) {
      std::fprintf(stderr, "%s: ATA command STANDBY IMMEDIATE failed: %s\n", name,
        dev->get_errmsg());
      return 1;
    }
    std::printf("%s: ATA command STANDBY IMMEDIATE succeeded\n", name);
    return 0;
  }
  catch (std::exception & ex) {
    std::fprintf(stderr, "Exception: %s\n", ex.what());
    return 1;
  }
}
