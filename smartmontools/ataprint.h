/*
 * ataprint.h
 *
 * Home page of code is: http://www.smartmontools.org
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
  int identify_word_level, identify_bit_level;
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

  unsigned pending_defects_log;

  bool sct_temp_sts, sct_temp_hist;
  int sct_erc_get; // get(1), get_power_on(2)
  int sct_erc_set; // set(1), set_power_on(2), mfg_default(3)
  unsigned sct_erc_readtime, sct_erc_writetime;
  bool sataphy, sataphy_reset;

  bool smart_disable, smart_enable;
  bool smart_auto_offl_disable, smart_auto_offl_enable;
  bool smart_auto_save_disable, smart_auto_save_enable;

  int smart_selftest_type; // OFFLINE_FULL_SCAN, ..., see atacmds.h. -1 for no test
  bool smart_selftest_force; // Ignore already running test
  ata_selective_selftest_args smart_selective_args; // Extra args for selective self-test

  unsigned sct_temp_int;
  bool sct_temp_int_pers;

  enum { FMT_BRIEF = 0x01, FMT_HEX_ID = 0x02, FMT_HEX_VAL = 0x04 };
  unsigned char output_format; // FMT_* flags

  firmwarebug_defs firmwarebugs; // -F options
  bool fix_swapped_id; // Fix swapped ID strings returned by some buggy drivers

  ata_vendor_attr_defs attribute_defs; // -v options

  bool ignore_presets; // Ignore presets from drive database
  bool show_presets; // Show presets and exit
  unsigned char powermode; // Skip check, if disk in idle or standby mode
  unsigned char powerexit; // exit() code for low power mode

  bool get_set_used; // true if any get/set command is used
  bool get_aam; // print Automatic Acoustic Management status
  int set_aam; // disable(-1), enable(1..255->0..254) Automatic Acoustic Management
  bool get_apm; // print Advanced Power Management status
  int set_apm; // disable(-1), enable(2..255->1..254) Advanced Power Management
  bool get_lookahead; // print read look-ahead status
  int set_lookahead; // disable(-1), enable(1) read look-ahead
  int set_standby; // set(1..255->0..254) standby timer
  bool set_standby_now; // set drive to standby
  bool get_security; // print ATA security status
  bool set_security_freeze; // Freeze ATA security
  bool get_wcache; // print write cache status
  int set_wcache; // disable(-1), enable(1) write cache
  bool sct_wcache_reorder_get; // print write cache reordering status
  int sct_wcache_reorder_set; // disable(-1), enable(1) write cache reordering
  bool sct_wcache_reorder_set_pers;
  bool sct_wcache_sct_get; // print SCT Feature Control of write cache status
  int sct_wcache_sct_set; // determined by ata set features command(1), force enable(2), force disable(3)
  bool sct_wcache_sct_set_pers; // persistent or volatile
  bool get_dsn; // print DSN status
  int set_dsn; // disable(02h), enable(01h) DSN

  ata_print_options()
    : drive_info(false),
      identify_word_level(-1), identify_bit_level(-1),
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
      pending_defects_log(0),
      sct_temp_sts(false), sct_temp_hist(false),
      sct_erc_get(0), sct_erc_set(0),
      sct_erc_readtime(0), sct_erc_writetime(0),
      sataphy(false), sataphy_reset(false),
      smart_disable(false), smart_enable(false),
      smart_auto_offl_disable(false), smart_auto_offl_enable(false),
      smart_auto_save_disable(false), smart_auto_save_enable(false),
      smart_selftest_type(-1), smart_selftest_force(false),
      sct_temp_int(0), sct_temp_int_pers(false),
      output_format(0),
      fix_swapped_id(false),
      ignore_presets(false),
      show_presets(false),
      powermode(0), powerexit(0),
      get_set_used(false),
      get_aam(false), set_aam(0),
      get_apm(false), set_apm(0),
      get_lookahead(false), set_lookahead(0),
      set_standby(0), set_standby_now(false),
      get_security(false), set_security_freeze(false),
      get_wcache(false), set_wcache(0),
      sct_wcache_reorder_get(false), sct_wcache_reorder_set(0),
      sct_wcache_reorder_set_pers(false),
      sct_wcache_sct_get(false), sct_wcache_sct_set(0),
      sct_wcache_sct_set_pers(false),
      get_dsn(false), set_dsn(0)
    { }
};

int ataPrintMain(ata_device * device, const ata_print_options & options);

#endif
