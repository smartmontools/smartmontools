/*
 * nvmeprint.h
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

#ifndef NVMEPRINT_H
#define NVMEPRINT_H

#define NVMEPRINT_H_CVSID "$Id$"

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
