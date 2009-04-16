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

#define ATAPRINT_H_CVSID "$Id: ataprint.h,v 1.40 2009/04/16 21:24:08 chrfranke Exp $\n"

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
// TODO: Move remaining options from con->* to here.
struct ata_print_options
{
  bool sataphy, sataphy_reset;
  bool gp_logdir, smart_logdir;
  unsigned smart_ext_error_log;
  unsigned smart_ext_selftest_log;

  std::vector<ata_log_request> log_requests;

  unsigned char fix_firmwarebug; // FIX_*, see atacmds.h
  bool fix_swapped_id; // Fix swapped ID strings returned by some buggy drivers

  // The i'th entry in this array will modify the printed meaning of
  // the i'th SMART attribute.  The default definitions of the
  // Attributes are obtained by having the array be all zeros.  If
  // attributedefs[i] is nonzero, it means that the i'th attribute has
  // a non-default meaning.  See the ataPrintSmartAttribName and
  // and parse_attribute_def functions.
  unsigned char attributedefs[256];

  bool ignore_presets; // Ignore presets from drive database
  bool show_presets; // Show presets and exit

  ata_print_options()
    : sataphy(false), sataphy_reset(false),
      gp_logdir(false), smart_logdir(false),
      smart_ext_error_log(0),
      smart_ext_selftest_log(0),
      fix_firmwarebug(FIX_NOTSPECIFIED),
      fix_swapped_id(false),
      ignore_presets(false),
      show_presets(false)
    { memset(attributedefs, 0, sizeof(attributedefs)); }
};

int ataPrintMain(ata_device * device, const ata_print_options & options);

#endif
