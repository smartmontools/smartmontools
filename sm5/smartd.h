/*
 * smartd.h
 *
 * Home page of code is: http://smartmontools.sourceforge.net
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

#ifndef CVSID7
#define CVSID7 "$Id: smartd.h,v 1.13 2002/10/29 16:59:02 ballen4705 Exp $\n"
#endif

// Configuration file
#define CONFIGFILE "/etc/smartd.conf"
#define MAXLINELEN 128
#define MAXENTRIES 64
#define MAXCONTLINE 511
#define MAXDEVLEN 51

/* how often SMART status is checked, in seconds */
int checktime = 1800;

// number of ATA and SCSI devices being watched
int numatadevices;
int numscsidevices;

#define MAXATADEVICES	12
#define MAXSCSIDEVICES	26

/* Defines for command line options */ 
#define DEBUGMODE 		'X'
#define EMAILNOTIFICATION	'e'
#define PRINTCOPYLEFT           'V'

/* Boolean Values */
#define TRUE 0x01
#define FALSE 0x00

/* Global Variables for command line options */
// These should go into a structure at some point
unsigned char debugmode               = FALSE;
unsigned char emailnotification       = FALSE;
unsigned char printcopyleft           = FALSE;


typedef struct scsidevices_s {
  unsigned char SmartPageSupported;
  unsigned char TempPageSupported;
  unsigned char Temperature;
  char *devicename;
} scsidevices_t;


typedef struct configfile_s {
  // which line was entry in file; what device type and name?
  int lineno;
  char tryata;
  char tryscsi;
  char *name;

  // which tests have been enabled?
  char smartcheck;
  char usagefailed;
  char prefail;
  char usage;
  char selftest;
  char errorlog;

  // store counts of ata and self-test errors
  char selflogcount;
  int  ataerrorcount;
  // following two items point to 32 bytes, in the form of are
  // 32x8=256 single bit flags 

  // valid attribute numbers are from 1 <= x <= 255
  // valid attribute values  are from 1 <= x <= 254
  unsigned char *failatt;
  unsigned char *trackatt;
} cfgfile;


typedef struct atadevices_s {
  struct ata_smart_values *smartval;
  struct ata_smart_thresholds *smartthres;
  cfgfile *cfg;
  char *devicename;
}  atadevices_t;


int ataCheckDevice(atadevices_t *drive);
