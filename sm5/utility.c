/*
 * utility.c
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

// THIS FILE IS INTENDED FOR UTILITY ROUTINES THAT ARE APPLICABLE TO
// BOTH SCSI AND ATA DEVICES, AND THAT MAY BE USED IN SMARTD,
// SMARTCTL, OR BOTH.

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <stdlib.h>
#include <ctype.h>
#include "utility.h"

// Any local header files should be represented by a CVSIDX just below.
const char* utility_c_cvsid="$Id: utility.c,v 1.11 2003/05/06 22:58:00 guidog Exp $" UTILITY_H_CVSID;


// Utility function prints date and time and timezone into a character
// buffer of length>=64.  All the fuss is needed to get the right
// timezone info (sigh).
void dateandtimezoneepoch(char *buffer, time_t tval){
  struct tm *tmval;
  char *timezonename;
  char datebuffer[64];
  
  // Get the time structure.  We need this to determine if we are in
  // daylight savings time or not.
  tmval=localtime(&tval);
  
  // Convert to an ASCII string, put in datebuffer
  asctime_r(tmval, datebuffer);
  
  // Remove newline
  datebuffer[strlen(datebuffer)-1]='\0';
  
  // correct timezone name
  if (tmval->tm_isdst==0)
    // standard time zone
    timezonename=tzname[0];
  else if (tmval->tm_isdst>0)
    // daylight savings in effect
    timezonename=tzname[1];
  else
    // unable to determine if daylight savings in effect
    timezonename="";
  
  // Finally put the information into the buffer as needed.
  snprintf(buffer, 64, "%s %s", datebuffer, timezonename);
  
  return;
}

// Date and timezone gets printed into string pointed to by buffer
void dateandtimezone(char *buffer){
  
  // Get the epoch (time in seconds since Jan 1 1970)
  time_t tval=time(NULL);
  
  dateandtimezoneepoch(buffer, tval);
  return;
}

// These are two utility functions for printing CVS IDs. Massagecvs()
// returns distance that it has moved ahead in the input string
int massagecvs(char *out, const char *cvsid){
  char *copy,*filename,*date,*version;
  const char delimiters[] = " ,$";

  // make a copy on stack, go to first token,
  if (!(copy=strdup(cvsid)) || !(filename=strtok(copy, delimiters))) 
    return 0;

  // move to first instance of "Id:"
  while (strcmp(filename,"Id:"))
    if (!(filename=strtok(NULL, delimiters)))
      return 0;

  // get filename, skip "v", get version and date
  if (!(  filename=strtok(NULL, delimiters)  ) ||
      !(           strtok(NULL, delimiters)  ) ||
      !(   version=strtok(NULL, delimiters)  ) ||
      !(      date=strtok(NULL, delimiters)  ) )
    return 0;

   sprintf(out,"%-13s revision: %-6s date: %-15s", filename, version, date);
   free(copy);
   return  (date-copy)+strlen(date);
}

// prints a single set of CVS ids
void printone(char *block, const char *cvsid){
  char strings[CVSMAXLEN];
  const char *here=cvsid;
  int line=1,len=strlen(cvsid)+1;

  // check that the size of the output block is sufficient
  if (len>=CVSMAXLEN) {
    pout("CVSMAXLEN=%d must be at least %d\n",CVSMAXLEN,len+1);
    exit(1);
  }

  // loop through the different strings
  while ((len=massagecvs(strings,here))){
    switch (line++){
    case 1:
      block+=snprintf(block,CVSMAXLEN,"Module:");
      break;
    default:
      block+=snprintf(block,CVSMAXLEN,"  uses:");
    } 
    block+=snprintf(block,CVSMAXLEN," %s\n",strings);
    here+=len;
  }
  return;
}


// A replacement for perror() that sends output to our choice of
// printing.
void syserror(const char *message){
  const char *errormessage;
  
  // Get the correct system error message:
  errormessage=strerror(errno);

  // Check that caller has handed a sensible string, and provide
  // appropriate output. See perrror(3) man page to understand better.
    if (message && *message)
      pout("%s: %s\n",message, errormessage);
    else
      pout("%s\n",errormessage);
	
    return;
}

// Prints a warning message for a failed regular expression compilation from
// regcomp().
void printregexwarning(int errcode, regex_t *compiled){
  size_t length = regerror(errcode, compiled, NULL, 0);
  char *buffer = malloc(length);
  if (!buffer){
    pout("Out of memory in printregexwarning()\n");
    return;
  }
  regerror(errcode, compiled, buffer, length);
  pout("%s\n", buffer);
  free(buffer);
  return;
}

// A wrapper for regcomp().  Returns zero for success, non-zero otherwise.
int compileregex(regex_t *compiled, const char *pattern, int cflags)
{ 
  int errorcode;

  if ((errorcode = regcomp(compiled, pattern, cflags))) {
    pout("Internal error: unable to compile regular expression %s", pattern);
    printregexwarning(errorcode, compiled);
    pout("Please inform smartmontools developers\n");
    return 1;
  }
  return 0;
}

// Splits an argument to the -r option into a name part and an (optional) 
// positive integer part.  s is a pointer to a string containing the
// argument.  After the call, s will point to the name part and *i the
// integer part if there is one or 1 otherwise.  Note that the string s may
// be changed by this function.  Returns zero if successful and non-zero
// otherwise.
int split_report_arg(char *s, int *i)
{
  if ((s = strchr(s, ','))) {
    // Looks like there's a name part and an integer part.
    *s++ = '\0';
    if (*s == '0' || !isdigit(*s))  // The integer part must be positive
      return 1;
    errno = 0;
    *i = atoi(s);
    if (errno)
      return 1;
  } else {
    // There's no integer part.
    *i = 1;
  }

  return 0;
}

// Guess device type (ata or scsi) based on device name (Linux
// specific) SCSI device name in linux can be sd, sr, scd, st, nst,
// osst, nosst and sg.
static const char * lin_dev_prefix = "/dev/";
static const char * lin_dev_ata_disk_plus = "h";
static const char * lin_dev_ata_devfs_disk_plus = "ide/";
static const char * lin_dev_scsi_devfs_disk_plus = "scsi/";
static const char * lin_dev_scsi_disk_plus = "s";
static const char * lin_dev_scsi_tape1 = "ns";
static const char * lin_dev_scsi_tape2 = "os";
static const char * lin_dev_scsi_tape3 = "nos";

int guess_linux_device_type(const char * dev_name) {
  int len;
  int dev_prefix_len = strlen(lin_dev_prefix);
  
  // if dev_name null, or string length zero
  if (!dev_name || !(len = strlen(dev_name)))
    return GUESS_DEVTYPE_DONT_KNOW;
  
  // Remove the leading /dev/... if it's there
  if (!strncmp(lin_dev_prefix, dev_name, dev_prefix_len)) {
    if (len <= dev_prefix_len)
      // if nothing else in the string, unrecognized
      return GUESS_DEVTYPE_DONT_KNOW;
    // else advance pointer to following characters
    dev_name += dev_prefix_len;
  }
  
  // form /dev/h* or h*
  if (!strncmp(lin_dev_ata_disk_plus, dev_name,
	       strlen(lin_dev_ata_disk_plus)))
    return GUESS_DEVTYPE_ATA;
  
  // form /dev/ide/* or ide/*
  if (!strncmp(lin_dev_ata_devfs_disk_plus, dev_name,
	       strlen(lin_dev_ata_devfs_disk_plus)))
    return GUESS_DEVTYPE_ATA;

  // form /dev/s* or s*
  if (!strncmp(lin_dev_scsi_disk_plus, dev_name,
	       strlen(lin_dev_scsi_disk_plus)))
    return GUESS_DEVTYPE_SCSI;

  // form /dev/scsi/* or scsi/*
  if (!strncmp(lin_dev_scsi_devfs_disk_plus, dev_name,
	       strlen(lin_dev_scsi_devfs_disk_plus)))
    return GUESS_DEVTYPE_SCSI;
  
  // form /dev/ns* or ns*
  if (!strncmp(lin_dev_scsi_tape1, dev_name,
	       strlen(lin_dev_scsi_tape1)))
    return GUESS_DEVTYPE_SCSI;
  
  // form /dev/os* or os*
  if (!strncmp(lin_dev_scsi_tape2, dev_name,
	       strlen(lin_dev_scsi_tape2)))
    return GUESS_DEVTYPE_SCSI;
  
  // form /dev/nos* or nos*
  if (!strncmp(lin_dev_scsi_tape3, dev_name,
	       strlen(lin_dev_scsi_tape3)))
    return GUESS_DEVTYPE_SCSI;
  
  // we failed to recognize any of the forms
  return GUESS_DEVTYPE_DONT_KNOW;
}
