/*
 * ataprint.h
 *
 * Home page of code is: http://smartmontools.sourceforge.net
 *
 * Copyright (C) 2002 Bruce Allen <smartmontools-support@lists.sourceforge.net>
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

#ifndef _SMART_PRINT_H_
#define _SMART_PRINT_H_

#ifndef CVSID2
#define CVSID2 "$Id: ataprint.h,v 1.13 2002/10/28 23:46:59 ballen4705 Exp $\n"
#endif

#include <stdio.h>
#include <stdlib.h>

// MACROS to control printing behavior
#define QUIETON(control)  {if (control->quietmode) control->veryquietmode=0;}
#define QUIETOFF(control) {if (control->quietmode && !control->veryquietmode) control->veryquietmode=1;}




/* Prints ATA Drive Information and S.M.A.R.T. Capability */
void ataPrintDriveInfo(struct hd_driveid);

void ataPrintGeneralSmartValues(struct ata_smart_values);

void ataPrintSmartThresholds(struct ata_smart_thresholds);

void ataPrintSmartErrorlog(struct ata_smart_errorlog);

void PrintSmartAttributes(struct ata_smart_values data);

void PrintSmartAttribWithThres(struct ata_smart_values data,
                                struct ata_smart_thresholds thresholds,
				int onlyfailed);

// returns number of entries that had logged errors
int ataPrintSmartSelfTestlog(struct ata_smart_selftestlog data, int allentries);

void ataPseudoCheckSmart(struct ata_smart_values, struct ata_smart_thresholds );



int ataPrintMain(int fd);

#endif
