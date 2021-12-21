/*
 * scsiprint.h
 *
 * Home page of code is: https://www.smartmontools.org
 *
 * Copyright (C) 2002-9 Bruce Allen
 * Copyright (C) 2000 Michael Cornwell <cornwell@acm.org>
 *
 * Additional SCSI work:
 * Copyright (C) 2003-18 Douglas Gilbert <dgilbert@interlog.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */


#ifndef SCSI_PRINT_H_
#define SCSI_PRINT_H_

#define SCSIPRINT_H_CVSID "$Id$\n"

// Options for scsiPrintMain
struct scsi_print_options
{
  bool drive_info = false;
  bool smart_check_status = false;
  bool smart_vendor_attrib = false;
  bool smart_error_log = false;
  bool smart_selftest_log = false;
  bool smart_background_log = false;
  bool smart_ss_media_log = false;

  bool smart_disable = false, smart_enable = false;
  bool smart_auto_save_disable = false, smart_auto_save_enable = false;

  bool smart_default_selftest = false;
  bool smart_short_selftest = false, smart_short_cap_selftest = false;
  bool smart_extend_selftest = false, smart_extend_cap_selftest = false;
  bool smart_selftest_abort = false;
  bool smart_selftest_force = false; // Ignore already running test

  bool smart_env_rep = false;

  bool sasphy = false, sasphy_reset = false;
  
  bool get_wce = false, get_rcd = false;
  short int set_wce = 0, set_rcd = 0;  // disable(-1), enable(1) cache

  unsigned char powermode = 0;  // Enhancement Skip check, if disk in idle or standby mode
  unsigned char powerexit = 0;  // exit() code for low power mode

  int set_standby = 0;          // set(1..255->0..254) standby timer
  bool set_standby_now = false; // set drive to standby
  bool set_active = false;      // set drive to active
};

int scsiPrintMain(scsi_device * device, const scsi_print_options & options);

#endif
