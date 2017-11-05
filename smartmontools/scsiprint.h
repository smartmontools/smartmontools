/*
 * scsiprint.h
 *
 * Home page of code is: http://www.smartmontools.org
 *
 * Copyright (C) 2002-9 Bruce Allen
 * Copyright (C) 2000 Michael Cornwell <cornwell@acm.org>
 *
 * Additional SCSI work:
 * Copyright (C) 2003-13 Douglas Gilbert <dgilbert@interlog.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * You should have received a copy of the GNU General Public License
 * (for example COPYING); if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * This code was originally developed as a Senior Thesis by Michael Cornwell
 * at the Concurrent Systems Laboratory (now part of the Storage Systems
 * Research Center), Jack Baskin School of Engineering, University of
 * California, Santa Cruz. http://ssrc.soe.ucsc.edu/
 *
 */


#ifndef SCSI_PRINT_H_
#define SCSI_PRINT_H_

#define SCSIPRINT_H_CVSID "$Id$\n"

// Options for scsiPrintMain
struct scsi_print_options
{
  bool drive_info;
  bool smart_check_status;
  bool smart_vendor_attrib;
  bool smart_error_log;
  bool smart_selftest_log;
  bool smart_background_log;
  bool smart_ss_media_log;

  bool smart_disable, smart_enable;
  bool smart_auto_save_disable, smart_auto_save_enable;

  bool smart_default_selftest;
  bool smart_short_selftest, smart_short_cap_selftest;
  bool smart_extend_selftest, smart_extend_cap_selftest;
  bool smart_selftest_abort;
  bool smart_selftest_force; // Ignore already running test

  bool sasphy, sasphy_reset;
  
  bool get_wce, get_rcd;
  short int set_wce, set_rcd;  // disable(-1), enable(1) cache

  scsi_print_options()
    : drive_info(false),
      smart_check_status(false),
      smart_vendor_attrib(false),
      smart_error_log(false),
      smart_selftest_log(false),
      smart_background_log(false),
      smart_ss_media_log(false),
      smart_disable(false), smart_enable(false),
      smart_auto_save_disable(false), smart_auto_save_enable(false),
      smart_default_selftest(false),
      smart_short_selftest(false), smart_short_cap_selftest(false),
      smart_extend_selftest(false), smart_extend_cap_selftest(false),
      smart_selftest_abort(false),
      smart_selftest_force(false),
      sasphy(false), sasphy_reset(false),
      get_wce(false), get_rcd(false),
      set_wce(0), set_rcd(0)
    { }
};

int scsiPrintMain(scsi_device * device, const scsi_print_options & options);

#endif
