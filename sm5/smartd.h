/*
 * smartd.h
 *
 * Home page of code is: http://smartmontools.sourceforge.net
 *
 * Copyright (C) 2002-3 Bruce Allen <smartmontools-support@lists.sourceforge.net>
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

#ifndef SMARTD_H_CVSID
#define SMARTD_H_CVSID "$Id: smartd.h,v 1.42 2003/08/14 23:35:50 ballen4705 Exp $\n"
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

// Number of monitoring flags per Attribute and offsets.  See
// monitorattflags below.
#define NMONITOR 4
#define MONITOR_FAILUSE   0
#define MONITOR_IGNORE    1
#define MONITOR_RAWPRINT  2
#define MONITOR_RAW       3

// Exit codes
#define EXIT_BADCMD    1   // command line did not parse
#define EXIT_BADCONF   2   // problem reading/parsing config file
#define EXIT_STARTUP   3   // problem forking daemon
#define EXIT_PID       4   // problem creating pid file

#define EXIT_NOMEM     8   // out of memory
#define EXIT_CCONST    9   // we hit a compile time constant
#define EXIT_BADCODE   10  // internal error - should NEVER happen

#define EXIT_BADDEV    16  // we can't monitor this device
#define EXIT_NODEV     17  // no devices to monitor

#define EXIT_SIGNAL    254 // abort on signal

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

// Used two ways.  First, to store a list of devices/options given in
// the configuration smartd.conf or constructed with DEVICESCAN.  Also
// contains all persistent storage needed to track device, if
// registered (either as SCSI or ATA).  After parsing the config file,
// the devices are checked to see if they can be monitored.  If so,
// then we keep a list of pointers to the entries in this table, and
// scan those once per polling interval.

typedef struct configfile_s {
  // FIRST SET OF ENTRIES CORRESPOND TO WHAT THE USER PUT IN THE CONFIG FILE
  int lineno;                             // Line number of entry in file
  char tryata;                            // Disk is ATA 
  char tryscsi;                           // Disk is SCSI
  unsigned char escalade;                 // 1 + ATA disk # in 3ware controller
  char *name;                             // Device name (+ optional [3ware_disk_XX])

  char smartcheck;                        // Check SMART status
  char usagefailed;                       // Check for failed Usage Attributes
  char prefail;                           // Track changes in Prefail Attributes
  char usage;                             // Track changes in Usage Attributes
  char selftest;                          // Monitor number of selftest errors
  char errorlog;                          // Monitor number of ATA errors

  char permissive;                        // Ignore failed SMART commands
  char autosave;                          // 1=disable, 2=enable Autosave Attributes
  char autoofflinetest;                   // 1=disable, 2=enable Auto Offline Test
  unsigned char emailfreq;                // Emails once (1) daily (2) diminishing (3)
  unsigned char emailtest;                // Send test email?

  // THE FOLLOWING TWO POINTERS ARE NULL UNLESS SET BY THE USER.
  // STORAGE IS ALLOCATED WHEN CONFIG FILE SCANNED
  char *emailcmdline;                     // Execute this program for sending mail
  char *address;                          // Email addresses


  // THE FOLLOWING POINTER IS NULL UNLESS SET BY USER: STORAGE IS
  // ALLOCATED WHEN CONFIG FILE SCANNED

  // following NMONITOR items each point to 32 bytes, in the form of
  // 32x8=256 single bit flags 
  // valid attribute numbers are from 1 <= x <= 255
  // monitorattflags+0  set: ignore failure for a usage attribute
  // monitorattflats+32 set: don't track attribute
  // monitorattflags+64 set: print raw value when tracking
  // monitorattflags+96 set: track changes in raw value
  unsigned char *monitorattflags;

  // THE FOLLOWING POINTER IS NULL UNLESS (1) IT IS SET BY USER:
  // STORAGE IS ALLOCATED WHEN CONFIG FILE SCANNED, or (2) IT IS SET
  // WHEN DRIVE IS AUTOMATICALLY RECOGNIZED IN DATABASE (WHEN DRIVE IS
  // REGISTERED)

  unsigned char *attributedefs;            // -v options, see end of extern.h for def
  unsigned char fixfirmwarebug;            // Fix firmware bug
  char ignorepresets;                      // Ignore database of -v options
  char showpresets;                        // Show database entry for this device
  char removable;                          // Device may disappear (not be present)

  // NEXT SET OF ENTRIES ARE DYNAMIC DATA THAT WE TRACK IF DEVICE IS
  // REGISTERED AND THEN MONITORED.

  mailinfo maildata[10];                   // Tracks type/date of email messages sent
  
  // ATA ONLY:
  unsigned char selflogcount;              // Total number of self-test errors
  int  ataerrorcount;                      // Total number of ATA errors

  // NULL POINTERS UNLESS NEEDED.  IF NEEDED, ALLOCATED WHEN DEVICE
  // REGISTERED.
  struct ata_smart_values *smartval;       // Pointer to SMART data
  struct ata_smart_thresholds *smartthres; // Pointer to SMART thresholds

  // SCSI ONLY:
  unsigned char SmartPageSupported;
  unsigned char TempPageSupported;
  unsigned char Temperature;
} cfgfile;


typedef struct changedattribute_s {
  unsigned char newval;
  unsigned char oldval;
  unsigned char id;
  unsigned char prefail;
  unsigned char sameraw;
} changedattribute_t;

// Declare our own printing functions...
void printout(int priority,char *fmt, ...) __attribute__ ((format(printf, 2, 3)));
void printandmail(cfgfile *cfg, int which, int priority, char *fmt, ...) __attribute__ ((format(printf, 4, 5)));   

int ataCheckDevice(cfgfile *cfg);
