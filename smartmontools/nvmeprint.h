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
  bool smart_check_status;
  bool smart_vendor_attrib;

  nvme_print_options()
    : drive_info(false),
      smart_check_status(false),
      smart_vendor_attrib(false)
    { }
};

int nvmePrintMain(nvme_device * device, const nvme_print_options & options);

#endif // NVMEPRINT_H
