/*
 * ataprint.h
 *
 * Home page of code is: https://www.smartmontools.org
 *
 * Copyright (C) 2002-09 Bruce Allen
 * Copyright (C) 2008-21 Christian Franke
 * Copyright (C) 1999-2000 Michael Cornwell <cornwell@acm.org>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef ATAPRINT_H_
#define ATAPRINT_H_

#define ATAPRINT_H_CVSID "$Id$\n"

#include <vector>

// Request to dump a GP or SMART log
struct ata_log_request
{
  bool gpl = false; // false: SMART, true: GP
  unsigned char logaddr = 0; // Log address
  unsigned page = 0; // First page (sector)
  unsigned nsectors = 0; // # Sectors
};

// Options for ataPrintMain
struct ata_print_options
{
  bool drive_info = false;
  int identify_word_level = -1, identify_bit_level = -1;
  bool smart_check_status = false;
  bool smart_general_values = false;
  bool smart_vendor_attrib = false;
  bool smart_error_log = false;
  bool smart_selftest_log = false;
  bool smart_selective_selftest_log = false;

  bool gp_logdir = false, smart_logdir = false;
  unsigned smart_ext_error_log = 0;
  unsigned smart_ext_selftest_log = 0;
  bool retry_error_log = false, retry_selftest_log = false;

  std::vector<ata_log_request> log_requests;

  bool devstat_all_pages = false, devstat_ssd_page = false;
  std::vector<int> devstat_pages;

  unsigned pending_defects_log = 0;

  bool sct_temp_sts = false, sct_temp_hist = false;
  int sct_erc_get = 0; // get(1), get_power_on(2)
  int sct_erc_set = 0; // set(1), set_power_on(2), mfg_default(3)
  unsigned sct_erc_readtime = 0, sct_erc_writetime = 0;
  bool sataphy = false, sataphy_reset = false;

  bool smart_disable = false, smart_enable = false;
  bool smart_auto_offl_disable = false, smart_auto_offl_enable = false;
  bool smart_auto_save_disable = false, smart_auto_save_enable = false;

  int smart_selftest_type = -1; // OFFLINE_FULL_SCAN, ..., see atacmds.h. -1 for no test
  bool smart_selftest_force = false; // Ignore already running test
  ata_selective_selftest_args smart_selective_args; // Extra args for selective self-test

  unsigned sct_temp_int = 0;
  bool sct_temp_int_pers = false;

  enum { FMT_BRIEF = 0x01, FMT_HEX_ID = 0x02, FMT_HEX_VAL = 0x04 };
  unsigned char output_format = 0; // FMT_* flags

  firmwarebug_defs firmwarebugs; // -F options
  bool fix_swapped_id = false; // Fix swapped ID strings returned by some buggy drivers

  ata_vendor_attr_defs attribute_defs; // -v options

  bool ignore_presets = false; // Ignore presets from drive database
  bool show_presets = false; // Show presets and exit
  unsigned char powermode = 0; // Skip check, if disk in idle or standby mode
  unsigned char powerexit = 0; // exit() code for low power mode
  int powerexit_unsup = -1; // exit() code for unsupported power mode or -1 to ignore

  bool get_set_used = false; // true if any get/set command is used
  bool get_aam = false; // print Automatic Acoustic Management status
  int set_aam = 0; // disable(-1), enable(1..255->0..254) Automatic Acoustic Management
  bool get_apm = false; // print Advanced Power Management status
  int set_apm = 0; // disable(-1), enable(2..255->1..254) Advanced Power Management
  bool get_lookahead = false; // print read look-ahead status
  int set_lookahead = 0; // disable(-1), enable(1) read look-ahead
  int set_standby = 0; // set(1..255->0..254) standby timer
  bool set_standby_now = false; // set drive to standby
  bool get_security = false; // print ATA security status
  bool set_security_freeze = false; // Freeze ATA security
  bool get_wcache = false; // print write cache status
  int set_wcache = 0; // disable(-1), enable(1) write cache
  bool sct_wcache_reorder_get = false; // print write cache reordering status
  int sct_wcache_reorder_set = 0; // disable(-1), enable(1) write cache reordering
  bool sct_wcache_reorder_set_pers = false;
  bool sct_wcache_sct_get = false; // print SCT Feature Control of write cache status
  int sct_wcache_sct_set = 0; // determined by ata set features command(1), force enable(2), force disable(3)
  bool sct_wcache_sct_set_pers = false; // persistent or volatile
  bool get_dsn = false; // print DSN status
  int set_dsn = 0; // disable(02h), enable(01h) DSN
};

int ataPrintMain(ata_device * device, const ata_print_options & options);

#endif
