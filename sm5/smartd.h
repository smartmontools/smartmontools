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

#ifndef SMARTD_H_
#define SMARTD_H_


#ifndef SMARTD_H_CVSID
#define SMARTD_H_CVSID "$Id: smartd.h,v 1.54 2003/10/27 11:11:57 ballen4705 Exp $\n"
#endif

// Configuration file
#define CONFIGFILENAME "smartd.conf"

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

// maximum number of ATA devices to monitor.  Under linux this should
// not exceed 20 (/dev/hda-t).  Check against make_device_names().
#define MAXATADEVICES	20

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


// cfgfile is the main data structure of smartd. It is used in two
// ways.  First, to store a list of devices/options given in the
// configuration smartd.conf or constructed with DEVICESCAN.  And
// second, to point to or provide all persistent storage needed to
// track a device, if registered either as SCSI or ATA.
// 
// After parsing the config file, each valid entry has a cfgfile data
// structure allocated in memory for it.  In parsing the configuration
// file, some storage space may be needed, of indeterminate length,
// for example for the device name.  When this happens, memory should
// be allocated and then pointed to from within the corresponding
// cfgfile structure.

// After parsing the configuration file, each device is then checked
// to see if it can be monitored (this process is called "registering
// the device".  This is done in [scsi|ata]devicescan, which is called
// exactly once, after the configuration file has been parsed and
// cfgfile data structures have been created for each of its entries.
//
// If a device can not be monitored, the memory for its cfgfile data
// structure should be freed by calling rmconfigentry(cfgfile *). In
// this case, we say that this device "was not registered".  All
// memory associated with that particular cfgfile structure is thus
// freed.
// 
// The remaining devices are polled in a timing look, where
// [ata|scsi]CheckDevice looks at each entry in turn.
// 
// If you want to add small amounts of "private" data on a per-device
// basis, just make a new field in cfgfile.  This is guaranteed zero
// on startup (when ata|scsi]scsidevicescan(cfgfile *cfg) is first
// called with a pointer to cfgfile.
// 
// If you need *substantial* persistent data space for a device
// (dozens or hundreds of bytes) please add a pointer field to
// cfgfile.  As before, this is guaranteed NULL when
// ata|scsi]scsidevicescan(cfgfile *cfg) is called. Allocate space for
// it in scsidevicescan or atadevicescan, if needed, and deallocate
// the space in rmconfigentry(cfgfile *cfg). Be sure to make the
// pointer NULL unless it points to an area of the heap that can be
// deallocated with free().  In other words, a non-NULL pointer in
// cfgfile means "this points to data space that should be freed if I
// stop monitoring this device." If you don't need the space anymore,
// please call free() and then SET THE POINTER IN cfgfile TO NULL.
// 
// Note that we allocate one cfgfile structure per device.  This is
// why substantial persisent data storage should only be pointed to
// from within cfgfile, not kept within cfgfile itself - it saves
// memory for those devices that don't need that type of persistent
// data.
// 
// In general, the capabilities of devices should be checked at
// registration time within atadevicescan() and scsidevicescan(), and
// then noted within *cfg.  So if device lacks some capability, this
// should be visible within *cfg after returning from
// [ata|scsi]devicescan.
// 
// Devices are then checked, once per polling interval, within
// ataCheckDevice() and scsiCheckDevice().  These should only check
// the capabilities that devices already are known to have (as noted
// within *cfg).


typedef struct configfile_s {
  // FIRST SET OF ENTRIES CORRESPOND TO WHAT THE USER PUT IN THE
  // CONFIG FILE.  SOME ENTRIES MAY BE MODIFIED WHEN A DEVICE IS
  // REGISTERED AND WE LEARN ITS CAPABILITIES.
  int lineno;                             // Line number of entry in file
  char *name;                             // Device name (+ optional [3ware_disk_XX])
  char tryata;                            // Disk is ATA 
  char tryscsi;                           // Disk is SCSI
  unsigned char escalade;                 // 1 + ATA disk # in 3ware controller
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
  unsigned char fixfirmwarebug;           // Fix firmware bug
  char ignorepresets;                     // Ignore database of -v options
  char showpresets;                       // Show database entry for this device
  char removable;                         // Device may disappear (not be present)
  char *emailcmdline;                     // Program for sending mail (or NULL)
  char *address;                          // Email addresses (or NULL)

  // THE NEXT SET OF ENTRIES TRACK DEVICE STATE AND ARE DYNAMIC
  mailinfo maildata[10];                  // Tracks type/date of email messages sent
  
  // SCSI ONLY
  unsigned char SmartPageSupported;       // has log sense IE page (0x2f)
  unsigned char TempPageSupported;        // has log sense temperature page (0xd)
  unsigned char Temperature;              // last recorded figure (in Celsius)
  unsigned char SuppressReport;           // minimize nuisance reports
  
  // ATA ONLY FROM HERE ON TO THE END
  unsigned char selflogcount;             // Total number of self-test errors
  int ataerrorcount;                      // Total number of ATA errors
  
  // following NMONITOR items each point to 32 bytes, in the form of
  // 32x8=256 single bit flags 
  // valid attribute numbers are from 1 <= x <= 255
  // monitorattflags+0  set: ignore failure for a usage attribute
  // monitorattflats+32 set: don't track attribute
  // monitorattflags+64 set: print raw value when tracking
  // monitorattflags+96 set: track changes in raw value
  unsigned char *monitorattflags;

  // NULL UNLESS (1) STORAGE IS ALLOCATED WHEN CONFIG FILE SCANNED
  // (SET BY USER) or (2) IT IS SET WHEN DRIVE IS AUTOMATICALLY
  // RECOGNIZED IN DATABASE (WHEN DRIVE IS REGISTERED)
  unsigned char *attributedefs;            // -v options, see end of extern.h for def

  // ATA ONLY - SAVE SMART DATA. NULL POINTERS UNLESS NEEDED.  IF
  // NEEDED, ALLOCATED WHEN DEVICE REGISTERED.
  struct ata_smart_values *smartval;       // Pointer to SMART data
  struct ata_smart_thresholds *smartthres; // Pointer to SMART thresholds

} cfgfile;


typedef struct changedattribute_s {
  unsigned char newval;
  unsigned char oldval;
  unsigned char id;
  unsigned char prefail;
  unsigned char sameraw;
} changedattribute_t;

// Declare our own printing functions. Doing this provides error
// messages if the argument number/types don't match the format.
#ifndef __GNUC__
#define __attribute__(x)      /* nothing */
#endif
void PrintOut(int priority,char *fmt, ...) __attribute__ ((format(printf, 2, 3)));

void PrintAndMail(cfgfile *cfg, int which, int priority, char *fmt, ...) __attribute__ ((format(printf, 4, 5)));   

/* Debugging notes: to check for memory allocation/deallocation problems, use:

export LD_PRELOAD=libnjamd.so;
export NJAMD_PROT=strict;           
export NJAMD_CHK_FREE=error;
export NJAMD_DUMP_LEAKS_ON_EXIT=num;
export NJAMD_DUMP_LEAKS_ON_EXIT=3;
export NJAMD_TRACE_LIBS=1

*/

// Number of seconds to allow for registering a SCSI device. If this
// time expires without sucess or failure, then treat it as failure.
// Set to 0 to eliminate this timeout feature from the code
// (equivalent to an infinite timeout interval).
#define SCSITIMEOUT 0

// This is for solaris, where signal() resets the handler to SIG_DFL
// after the first signal is caught.
#ifdef HAVE_SIGSET
#define SIGNALFN sigset
#else
#define SIGNALFN signal
#endif

#endif
