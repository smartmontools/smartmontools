/*
 * utility.h
 *
 * Home page of code is: http://smartmontools.sourceforge.net
 *
 * Copyright (C) 2002-4 Bruce Allen <smartmontools-support@lists.sourceforge.net>
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

#define UTILITY_H_CVSID "$Id: utility.h,v 1.33 2004/04/14 13:27:20 chrfranke Exp $\n"

#include <time.h>
#include <sys/types.h> // for regex.h (according to POSIX)
#include <regex.h>

#include "int64.h"

#if defined(_WIN32) && defined(_MSC_VER)
#define snprintf  _snprintf
#define vsnprintf  _vsnprintf
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
void pout(char *fmt, ...)  
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
// Function for processing -t selective... option in smartctl
int split_selective_arg(char *s, uint64_t *start, uint64_t *stop);

// Guess device type (ata or scsi) based on device name 
#define GUESS_DEVTYPE_ATA       0
#define GUESS_DEVTYPE_SCSI      1
#define GUESS_DEVTYPE_DONT_KNOW 2
int guess_device_type(const char * dev_name);

// Create and return the list of devices to probe automatically
// if the DEVICESCAN option is in the smartd config file
int make_device_names (char ***devlist, const char* name);


#define EXIT(x)  { exitstatus = (x); exit((x)); }

// replacement for calloc() that tracks memory usage
void *Calloc(size_t nmemb, size_t size);

// Utility function to free memory
void *FreeNonZero(void* address, int size, int whatline, const char* file);

// A custom version of strdup() that keeps track of how much memory is
// being allocated. If mustexist is set, it also throws an error if we
// try to duplicate a NULL string.
char *CustomStrDup(char *ptr, int mustexist, int whatline, const char* file);

// To help with memory checking.  Use when it is known that address is
// NOT null.
void *CheckFree(void *address, int whatline, const char* file);

// This function prints either to stdout or to the syslog as needed

// [From GLIBC Manual: Since the prototype doesn't specify types for
// optional arguments, in a call to a variadic function the default
// argument promotions are performed on the optional argument
// values. This means the objects of type char or short int (whether
// signed or not) are promoted to either int or unsigned int, as
// appropriate.]
void PrintOut(int priority,char *fmt, ...);

// run time, determine byte ordering
int isbigendian();

// This value follows the peripheral device type value as defined in
// SCSI Primary Commands, ANSI INCITS 301:1997.  It is also used in
// the ATA standard for packet devices to define the device type.
const char *packetdevicetype(int type);

int deviceopen(const char *pathname, char *type);

int deviceclose(int fd);

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

#endif
