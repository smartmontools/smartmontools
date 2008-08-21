/*
 * ataprint.h
 *
 * Home page of code is: http://smartmontools.sourceforge.net
 *
 * Copyright (C) 2002-8 Bruce Allen <smartmontools-support@lists.sourceforge.net>
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

#define ATAPRINT_H_CVSID "$Id: ataprint.h,v 1.35 2008/08/21 21:20:51 chrfranke Exp $\n"

#include <stdio.h>
#include <stdlib.h>
#include <vector>

/* Prints ATA Drive Information and S.M.A.R.T. Capability */
int ataPrintDriveInfo(struct ata_identify_device *);

void ataPrintGeneralSmartValues(struct ata_smart_values *, struct ata_identify_device *);

void ataPrintSmartThresholds(struct ata_smart_thresholds_pvt *);

// returns number of errors in Errorlog
int  ataPrintSmartErrorlog(struct ata_smart_errorlog *);

void PrintSmartAttributes(struct ata_smart_values *);

void PrintSmartAttribWithThres(struct ata_smart_values *,
                                struct ata_smart_thresholds_pvt *,
                                int onlyfailed);

// returns number of entries that had logged errors
int ataPrintSmartSelfTestlog(struct ata_smart_selftestlog *, int allentries);

void ataPseudoCheckSmart(struct ata_smart_values *, struct ata_smart_thresholds_pvt *);

// Convenience function for formatting strings from ata_identify_device.
void format_ata_string(char *out, const char *in, int n);


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

  std::vector<ata_log_request> log_requests;

  ata_print_options()
    : sataphy(false), sataphy_reset(false),
      gp_logdir(false), smart_logdir(false)
    { }
};

int ataPrintMain(ata_device * device, const ata_print_options & options);

#endif
