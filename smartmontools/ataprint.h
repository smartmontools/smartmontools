/*
 * ataprint.h
 *
 * Home page of code is: http://smartmontools.sourceforge.net
 *
 * Copyright (C) 2002-9 Bruce Allen <smartmontools-support@lists.sourceforge.net>
 * Copyright (C) 2008-9 Christian Franke <smartmontools-support@lists.sourceforge.net>
 * Copyright (C) 1999-2000 Michael Cornwell <cornwell@acm.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * You should have received a copy of the GNU General Public License
 * (for example COPYING); if not, write to the Free
 * Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * This code was originally developed as a Senior Thesis by Michael Cornwell
 * at the Concurrent Systems Laboratory (now part of the Storage Systems
 * Research Center), Jack Baskin School of Engineering, University of
 * California, Santa Cruz. http://ssrc.soe.ucsc.edu/
 *
 */

#ifndef ATAPRINT_H_
#define ATAPRINT_H_

#define ATAPRINT_H_CVSID "$Id$\n"

#include <vector>

// Request to dump a GP or SMART log
struct ata_log_request
{
  bool gpl; // false: SMART, true: GP
  unsigned char logaddr; // Log address
  unsigned page; // First page (sector)
  unsigned nsectors; // # Sectors

  ata_log_request()
    : gpl(false), logaddr(0), page(0), nsectors(0)
    { }
};

// Options for ataPrintMain
struct ata_print_options
{
  bool drive_info;
  bool smart_check_status;
  bool smart_general_values;
  bool smart_vendor_attrib;
  bool smart_error_log;
  bool smart_selftest_log;
  bool smart_selective_selftest_log;

  bool gp_logdir, smart_logdir;
  unsigned smart_ext_error_log;
  unsigned smart_ext_selftest_log;
  bool retry_error_log, retry_selftest_log;

  std::vector<ata_log_request> log_requests;

  bool devstat_all_pages, devstat_ssd_page;
  std::vector<int> devstat_pages;

  bool sct_temp_sts, sct_temp_hist;
  bool sct_erc_get;
  bool sct_erc_set;
  unsigned sct_erc_readtime, sct_erc_writetime;
  bool sataphy, sataphy_reset;

  bool smart_disable, smart_enable;
  bool smart_auto_offl_disable, smart_auto_offl_enable;
  bool smart_auto_save_disable, smart_auto_save_enable;

  int smart_selftest_type; // OFFLINE_FULL_SCAN, ..., see atacmds.h. -1 for no test
  ata_selective_selftest_args smart_selective_args; // Extra args for selective self-test

  unsigned sct_temp_int;
  bool sct_temp_int_pers;

  unsigned char output_format; // 0=old, 1=brief
  unsigned char fix_firmwarebug; // FIX_*, see atacmds.h
  bool fix_swapped_id; // Fix swapped ID strings returned by some buggy drivers

  ata_vendor_attr_defs attribute_defs; // -v options

  bool ignore_presets; // Ignore presets from drive database
  bool show_presets; // Show presets and exit
  unsigned char powermode; // Skip check, if disk in idle or standby mode

  ata_print_options()
    : drive_info(false),
      smart_check_status(false),
      smart_general_values(false),
      smart_vendor_attrib(false),
      smart_error_log(false),
      smart_selftest_log(false),
      smart_selective_selftest_log(false),
      gp_logdir(false), smart_logdir(false),
      smart_ext_error_log(0),
      smart_ext_selftest_log(0),
      retry_error_log(false), retry_selftest_log(false),
      devstat_all_pages(false), devstat_ssd_page(false),
      sct_temp_sts(false), sct_temp_hist(false),
      sct_erc_get(false),
      sct_erc_set(false),
      sct_erc_readtime(0), sct_erc_writetime(0),
      sataphy(false), sataphy_reset(false),
      smart_disable(false), smart_enable(false),
      smart_auto_offl_disable(false), smart_auto_offl_enable(false),
      smart_auto_save_disable(false), smart_auto_save_enable(false),
      smart_selftest_type(-1),
      sct_temp_int(0), sct_temp_int_pers(false),
      output_format(0),
      fix_firmwarebug(FIX_NOTSPECIFIED),
      fix_swapped_id(false),
      ignore_presets(false),
      show_presets(false),
      powermode(0)
    { }
};

int ataPrintMain(ata_device * device, const ata_print_options & options);

#endif
