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
#define CVSID7 "$Id: smartd.h,v 1.24 2002/11/23 17:06:45 pjwilliams Exp $\n"
#endif

// Configuration file
#define CONFIGFILE "/etc/smartd.conf"

// Scan directive for configuration file
#define SCANDIRECTIVE "DEVICESCAN"

// maximum line length in configuration file
#define MAXLINELEN 128

// maximum number of device entries in configuration file.
#define MAXENTRIES 64

// maximum length of a continued line in configuration file
#define MAXCONTLINE 1023

// default for how often SMART status is checked, in seconds
#define CHECKTIME 1800

// maximum number of ATA devices to monitor
#define MAXATADEVICES	12

// maximum number of SCSI devices to monitor
#define MAXSCSIDEVICES	26

/* Boolean Values */
#define TRUE 0x01
#define FALSE 0x00


// If user has requested email warning messages, then this structure
// stores the information about them.
typedef struct mailinfo {
  // number of times an email has been sent
  int logged;
  // time last email was sent, as defined by man 2 time
  time_t lastsent;
  // time problem initially logged
  time_t firstsent;
} mailinfo;

// Used to store a list of devices and options that were in the
// configuration file.
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
  // Should we ignore missing capabilities/SMART errors
  char permissive;
  // Disable (1) or Enable (2) device attribute autosave
  char autosave;
  // Disable (1) or Enable (2) autmatic offline testing
  char autoofflinetest;
  // mailing information for four of the previous error types plus mailtest
  mailinfo maildata[5];
  char emailopt;
  // address to send email to
  char *address;
  // counts of ata and self-test errors.  Perhaps ought to be in the
  // atadevices_t structure.
  unsigned char selflogcount;
  int  ataerrorcount;
  // following two items point to 32 bytes, in the form of
  // 32x8=256 single bit flags 
  // valid attribute numbers are from 1 <= x <= 255
  // valid attribute values  are from 1 <= x <= 254
  unsigned char *failatt;
  unsigned char *trackatt;
} cfgfile;


// Used to store list of ATA devices to monitor.
typedef struct atadevices_s {
  struct ata_smart_values *smartval;
  struct ata_smart_thresholds *smartthres;
  cfgfile *cfg;
  char *devicename;
}  atadevices_t;

// used to store a list of SCSI devices to monitor.  Devicename points
// to a malloced name string.
typedef struct scsidevices_s {
  unsigned char SmartPageSupported;
  unsigned char TempPageSupported;
  unsigned char Temperature;
  char *devicename;
  cfgfile *cfg;
} scsidevices_t;


// Declare our own printing functions...
void printout(int priority,char *fmt, ...) __attribute__ ((format(printf, 2, 3)));
void printandmail(cfgfile *cfg, int which, int priority, char *fmt, ...) __attribute__ ((format(printf, 4, 5)));   

int ataCheckDevice(atadevices_t *drive);
