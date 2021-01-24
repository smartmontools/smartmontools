/*
 * nvmeprint.h
 *
 * Home page of code is: https://www.smartmontools.org
 *
 * Copyright (C) 2016-21 Christian Franke
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef NVMEPRINT_H
#define NVMEPRINT_H

#define NVMEPRINT_H_CVSID "$Id$"

#include "nvmecmds.h"

// options for nvmePrintMain
struct nvme_print_options
{
  bool drive_info = false;
  bool drive_capabilities = false;
  bool smart_check_status = false;
  bool smart_vendor_attrib = false;
  unsigned error_log_entries = 0;
  unsigned char log_page = 0;
  unsigned log_page_size = 0;
};

int nvmePrintMain(nvme_device * device, const nvme_print_options & options);

#endif // NVMEPRINT_H
