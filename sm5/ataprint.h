/*
 * ataprint.h
 *
 * Home page of code is: http://smartmontools.sourceforge.net
 *
 * Copyright (C) 2002-6 Bruce Allen <smartmontools-support@lists.sourceforge.net>
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

#define ATAPRINT_H_CVSID "$Id: ataprint.h,v 1.28 2006/04/12 14:54:28 ballen4705 Exp $\n"

#include <stdio.h>
#include <stdlib.h>

/* Prints ATA Drive Information and S.M.A.R.T. Capability */
void ataPrintDriveInfo(struct ata_identify_device *);

void ataPrintGeneralSmartValues(struct ata_smart_values *, struct ata_identify_device *);

void ataPrintSmartThresholds(struct ata_smart_thresholds_pvt *);

// returns number of errors in Errorlog
int  ataPrintSmartErrorlog(struct ata_smart_errorlog *);

int ataPrintLogDirectory(struct ata_smart_log_directory *);

void PrintSmartAttributes(struct ata_smart_values *);

void PrintSmartAttribWithThres(struct ata_smart_values *,
                                struct ata_smart_thresholds_pvt *,
                                int onlyfailed);

// returns number of entries that had logged errors
int ataPrintSmartSelfTestlog(struct ata_smart_selftestlog *, int allentries);

void ataPseudoCheckSmart(struct ata_smart_values *, struct ata_smart_thresholds_pvt *);

// Convenience function for formatting strings from ata_identify_device.
void formatdriveidstring(char *out, const char *in, int n);

int ataPrintMain(int fd);

#endif
