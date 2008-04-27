/*
 * utility.h
 *
 * Home page of code is: http://smartmontools.sourceforge.net
 *
 * Copyright (C) 2002-8 Bruce Allen <smartmontools-support@lists.sourceforge.net>
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

#ifndef UTILITY_H_
#define UTILITY_H_

#define UTILITY_H_CVSID "$Id: utility.h,v 1.54 2008/04/27 16:30:09 chrfranke Exp $\n"

#include <time.h>
#include <sys/types.h> // for regex.h (according to POSIX)
#include <regex.h>

// Make version information string
const char *format_version_info(const char *prog_name);

#ifndef HAVE_WORKING_SNPRINTF
// Substitute by safe replacement functions
#include <stdarg.h>
int safe_snprintf(char *buf, int size, const char *fmt, ...);
int safe_vsnprintf(char *buf, int size, const char *fmt, va_list ap);
#define snprintf  safe_snprintf
#define vsnprintf safe_vsnprintf
#endif

// Utility function prints current date and time and timezone into a
// character buffer of length>=64.  All the fuss is needed to get the
// right timezone info (sigh).
#define DATEANDEPOCHLEN 64
void dateandtimezone(char *buffer);
// Same, but for time defined by epoch tval
void dateandtimezoneepoch(char *buffer, time_t tval);

// utility function for printing out CVS strings
#define CVSMAXLEN 1024
void printone(char *block, const char *cvsid);

// like printf() except that we can control it better. Note --
// although the prototype is given here in utility.h, the function
// itself is defined differently in smartctl and smartd.  So the
// function definition(s) are in smartd.c and in smartctl.c.
#ifndef __GNUC__
#define __attribute__(x)      /* nothing */
#endif
void pout(const char *fmt, ...)  
     __attribute__ ((format (printf, 1, 2)));

// replacement for perror() with redirected output.
void syserror(const char *message);

// Prints a warning message for a failed regular expression compilation from
// regcomp().
void printregexwarning(int errcode, regex_t *compiled);

// A wrapper for regcomp().  Returns zero for success, non-zero otherwise.
int compileregex(regex_t *compiled, const char *pattern, int cflags);

// Function for processing -r option in smartctl and smartd
int split_report_arg(char *s, int *i);
// Function for processing -c option in smartctl and smartd
int split_report_arg2(char *s, int *i);

// Possible values for smartselectivemode
#define SEL_RANGE            0 // MIN-MAX
#define SEL_REDO             1 // redo this
#define SEL_NEXT             2 // do next range
#define SEL_CONT             3 // redo or next depending of last test status
// Function for processing -t selective... option in smartctl
int split_selective_arg(char *s, uint64_t *start, uint64_t *stop, int *mode);


// Guess device type (ata or scsi) based on device name 
// Guessing will now use Controller Type defines below

int guess_device_type(const char * dev_name);

// Create and return the list of devices to probe automatically
// if the DEVICESCAN option is in the smartd config file
int make_device_names (char ***devlist, const char* name);

// Replacement for exit(status)
// (exit is not compatible with C++ destructors)
#define EXIT(status) { throw (int)(status); }

// replacement for calloc() that tracks memory usage
void *Calloc(size_t nmemb, size_t size);

// Utility function to free memory
void *FreeNonZero1(void* address, int size, int whatline, const char* file);

// Typesafe version of above
template <class T>
inline T * FreeNonZero(T * address, int size, int whatline, const char* file)
  { return (T *)FreeNonZero1((void *)address, size, whatline, file); }

// A custom version of strdup() that keeps track of how much memory is
// being allocated. If mustexist is set, it also throws an error if we
// try to duplicate a NULL string.
char *CustomStrDup(const char *ptr, int mustexist, int whatline, const char* file);

// To help with memory checking.  Use when it is known that address is
// NOT null.
void *CheckFree1(void *address, int whatline, const char* file);

// Typesafe version of above
template <class T>
inline T * CheckFree(T * address, int whatline, const char* file)
  { return (T *)CheckFree1((void *)address, whatline, file); }

// This function prints either to stdout or to the syslog as needed

// [From GLIBC Manual: Since the prototype doesn't specify types for
// optional arguments, in a call to a variadic function the default
// argument promotions are performed on the optional argument
// values. This means the objects of type char or short int (whether
// signed or not) are promoted to either int or unsigned int, as
// appropriate.]
void PrintOut(int priority, const char *fmt, ...) __attribute__ ((format(printf, 2, 3)));

// run time, determine byte ordering
int isbigendian();

// This value follows the peripheral device type value as defined in
// SCSI Primary Commands, ANSI INCITS 301:1997.  It is also used in
// the ATA standard for packet devices to define the device type.
const char *packetdevicetype(int type);

int deviceopen(const char *pathname, char *type);

int deviceclose(int fd);

// Optional functions of os_*.c
#ifdef HAVE_GET_OS_VERSION_STR
// Return build host and OS version as static string
const char * get_os_version_str(void);
#endif

// returns 1 if any of the n bytes are nonzero, else zero.
int nonempty(unsigned char *testarea,int n);

// needed to fix glibc bug
void FixGlibcTimeZoneBug();

// convert time in msec to a text string
void MsecToText(unsigned int msec, char *txt);

// Exit codes
#define EXIT_BADCMD    1   // command line did not parse
#define EXIT_BADCONF   2   // syntax error in config file
#define EXIT_STARTUP   3   // problem forking daemon
#define EXIT_PID       4   // problem creating pid file
#define EXIT_NOCONF    5   // config file does not exist
#define EXIT_READCONF  6   // config file exists but cannot be read

#define EXIT_NOMEM     8   // out of memory
#define EXIT_BADCODE   10  // internal error - should NEVER happen

#define EXIT_BADDEV    16  // we can't monitor this device
#define EXIT_NODEV     17  // no devices to monitor

#define EXIT_SIGNAL    254 // abort on signal


// macros to control printing
#define PRINT_ON(control)  {if (control->printing_switchable) control->dont_print=0;}
#define PRINT_OFF(control) {if (control->printing_switchable) control->dont_print=1;}

// possible values for controller_type in extern.h
#define CONTROLLER_UNKNOWN              0x00
#define CONTROLLER_ATA                  0x01
#define CONTROLLER_SCSI                 0x02
#define CONTROLLER_3WARE                0x03  // set by -d option, but converted to one of three types below
#define CONTROLLER_3WARE_678K           0x04  // NOT set by guess_device_type()
#define CONTROLLER_3WARE_9000_CHAR      0x05  // set by guess_device_type()
#define CONTROLLER_3WARE_678K_CHAR      0x06  // set by guess_device_type()
#define CONTROLLER_MARVELL_SATA         0x07  // SATA drives behind Marvell controllers
#define CONTROLLER_SAT         	        0x08  // SATA device behind a SCSI ATA Translation (SAT) layer
#define CONTROLLER_HPT                  0x09  // SATA drives behind HighPoint Raid controllers
#define CONTROLLER_CCISS		0x10  // CCISS controller 
#define CONTROLLER_PARSEDEV             0x11  // "smartctl -r ataioctl,2 ..." output parser pseudo-device
#define CONTROLLER_ATACB		0x12 //ata device behind cypress usb bridge

#endif
