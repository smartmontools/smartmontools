//  $Id: ataprint.h,v 1.1 2002/10/09 17:56:58 ballen4705 Exp $

/*
 * ataprint.c
 *
 * Copyright (C) 2002 Bruce Allen <ballen@uwm.edu>
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
 */

#ifndef _SMART_PRINT_H_
#define _SMART_PRINT_H_

#include <stdio.h>
#include <stdlib.h>
#include "atacmds.h"


/* Print Formart of Structures for SMART information */


/* Prints ATA Drive Information and S.M.A.R.T. Capability */

void ataPrintDriveInfo (struct hd_driveid);

void ataPrintGeneralSmartValues (struct ata_smart_values);

void ataPrintSmartThresholds (struct ata_smart_thresholds);

void ataPrintSmartErrorlog (struct ata_smart_errorlog);

void PrintSmartAttributes (struct ata_smart_values data);

void PrintSmartAttribWithThres (struct ata_smart_values data,
                                struct ata_smart_thresholds thresholds);

void ataPrintSmartSelfTestlog (struct ata_smart_selftestlog data);

void ataPsuedoCheckSmart (struct ata_smart_values , 
                          struct ata_smart_thresholds );

/* Prints Attribute Name for standard SMART attributes */
/* prints 20 character string */

void ataPrintSmartAttribName (unsigned char id);

void ataPrintMain ( int fd );

#endif
