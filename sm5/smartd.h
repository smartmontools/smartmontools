//  $Id: smartd.h,v 1.4 2002/10/14 15:26:32 ballen4705 Exp $
/*
 * smartd.h
 *
 * Copyright (C) 2002 Bruce Allen <smartmontools-support@lists.sourceforge.net>
 * Copyright (C) 2000 Michael Cornwell <cornwell@acm.org>
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

/* Defines for command line options */ 
#define DEBUGMODE 		'X'
#define EMAILNOTIFICATION	'e'
#define PRINTCOPYLEFT           'p'

/* Boolean Values */
#define TRUE 0x01
#define FALSE 0x00

#define MAXATADEVICES	12
#define MAXSCSIDEVICES	26

/* Global Variables for command line options */
unsigned char debugmode               = FALSE;
unsigned char emailnotification       = FALSE;
unsigned char printcopyleft           = FALSE;

/* Number of ata device to scan */
int numatadevices;
int numscsidevices;


/* how often SMART is checks in seconds */
int checktime = 1800;

typedef struct atadevices_s {
	int fd;
	char devicename[14];
	int selftest;
	struct hd_driveid drive;
	struct ata_smart_values smartval;
	struct ata_smart_thresholds smartthres;
}  atadevices_t;

typedef struct scsidevices_s {
	int fd;
	char devicename[14];
	unsigned char SmartPageSupported;
	unsigned char TempPageSupported;
	unsigned char Temperature;
} scsidevices_t;
