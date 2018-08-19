/*
 * nvmeprint.h
 *
 * Home page of code is: http://www.smartmontools.org
 *
 * Copyright (C) 2016 Christian Franke
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef NVMEPRINT_H
#define NVMEPRINT_H

#define NVMEPRINT_H_CVSID "$Id: nvmeprint.h 4760 2018-08-19 18:45:53Z chrfranke $"

#include "nvmecmds.h"

// options for nvmePrintMain
struct nvme_print_options
{
  bool drive_info;
  bool drive_capabilities;
  bool smart_check_status;
  bool smart_vendor_attrib;
  unsigned error_log_entries;
  unsigned char log_page;
  unsigned log_page_size;

  nvme_print_options()
    : drive_info(false),
      drive_capabilities(false),
      smart_check_status(false),
      smart_vendor_attrib(false),
      error_log_entries(0),
      log_page(0),
      log_page_size(0)
    { }
};

int nvmePrintMain(nvme_device * device, const nvme_print_options & options);

#endif // NVMEPRINT_H
