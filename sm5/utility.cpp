/*
 * utility.c
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

// THIS FILE IS INTENDED FOR UTILITY ROUTINES THAT ARE APPLICABLE TO
// BOTH SCSI AND ATA DEVICES, AND THAT MAY BE USED IN SMARTD,
// SMARTCTL, OR BOTH.

#include <stdio.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <stdlib.h>
#include <ctype.h>
#include <syslog.h>
#include <stdarg.h>

#if HAVE_CONFIG_H
#include "config.h"
#endif
#if HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif

#include "config.h"
#include "int64.h"
#include "utility.h"

// Any local header files should be represented by a CVSIDX just below.
const char* utility_c_cvsid="$Id: utility.cpp,v 1.50 2004/05/27 15:27:06 card_captor Exp $"
CONFIG_H_CVSID INT64_H_CVSID UTILITY_H_CVSID;

const char * packet_types[] = {
        "Direct-access (disk)",
        "Sequential-access (tape)",
        "Printer",
        "Processor",
        "Write-once (optical disk)",
        "CD/DVD",
        "Scanner",
        "Optical memory (optical disk)",
        "Medium changer",
        "Communications",
        "Graphic arts pre-press (10)",
        "Graphic arts pre-press (11)",
        "Array controller",
        "Enclosure services",
        "Reduced block command (simplified disk)",
        "Optical card reader/writer"
};

// Whenever exit() status is EXIT_BADCODE, please print this message
const char *reportbug="Please report this bug to the Smartmontools developers at " PACKAGE_BUGREPORT ".\n";


// hang on to exit code, so we can make use of more generic 'atexit()'
// functionality and still check our exit code
int exitstatus = 0;

// command-line argument: are we running in debug mode?.
unsigned char debugmode = 0;


// Solaris only: Get site-default timezone. This is called from
// UpdateTimezone() when TZ environment variable is unset at startup.
#if defined (__SVR4) && defined (__sun)
static const char *TIMEZONE_FILE = "/etc/TIMEZONE";

static char *ReadSiteDefaultTimezone(){
  FILE *fp;
  char buf[512], *tz;
  int n;

  tz = NULL;
  fp = fopen(TIMEZONE_FILE, "r");
  if(fp == NULL) return NULL;
  while(fgets(buf, sizeof(buf), fp)) {
    if (strncmp(buf, "TZ=", 3))    // searches last "TZ=" line
      continue;
    n = strlen(buf) - 1;
    if (buf[n] == '\n') buf[n] = 0;
    if (tz) free(tz);
    tz = strdup(buf);
  }
  fclose(fp);
  return tz;
}
#endif

// Make sure that this executable is aware if the user has changed the
// time-zone since the last time we polled devices. The cannonical
// example is a user who starts smartd on a laptop, then flies across
// time-zones with a laptop, and then changes the timezone, WITHOUT
// restarting smartd. This is a work-around for a bug in
// GLIBC. Yuk. See bug number 48184 at http://bugs.debian.org and
// thanks to Ian Redfern for posting a workaround.

// Please refer to the smartd manual page, in the section labeled LOG
// TIMESTAMP TIMEZONE.
void FixGlibcTimeZoneBug(){
#if __GLIBC__  
  if (!getenv("TZ")) {
    putenv("TZ=GMT");
    tzset();
    putenv("TZ");
    tzset();
  }
#elif _WIN32
  if (!getenv("TZ")) {
    putenv("TZ=GMT");
    tzset();
    putenv("TZ=");  // empty value removes TZ, putenv("TZ") does nothing
    tzset();
  }
#elif defined (__SVR4) && defined (__sun)
  // In Solaris, putenv("TZ=") sets null string and invalid timezone.
  // putenv("TZ") does nothing.  With invalid TZ, tzset() do as if
  // TZ=GMT.  With TZ unset, /etc/TIMEZONE will be read only _once_ at
  // first tzset() call.  Conclusion: Unlike glibc, dynamic
  // configuration of timezone can be done only by changing actual
  // value of TZ environment value.
  enum tzstate { NOT_CALLED_YET, USER_TIMEZONE, TRACK_TIMEZONE };
  static enum tzstate state = NOT_CALLED_YET;

  static struct stat prev_stat;
  static char *prev_tz;
  struct stat curr_stat;
  char *curr_tz;

  if(state == NOT_CALLED_YET) {
    if(getenv("TZ")) {
      state = USER_TIMEZONE; // use supplied timezone
    } else {
      state = TRACK_TIMEZONE;
      if(stat(TIMEZONE_FILE, &prev_stat)) {
	state = USER_TIMEZONE;	// no TZ, no timezone file; use GMT forever
      } else {
	prev_tz = ReadSiteDefaultTimezone(); // track timezone file change
	if(prev_tz) putenv(prev_tz);
      }
    }
    tzset();
  } else if(state == TRACK_TIMEZONE) {
    if(stat(TIMEZONE_FILE, &curr_stat) == 0
       && (curr_stat.st_ctime != prev_stat.st_ctime
	    || curr_stat.st_mtime != prev_stat.st_mtime)) {
      // timezone file changed
      curr_tz = ReadSiteDefaultTimezone();
      if(curr_tz) {
	putenv(curr_tz);
	if(prev_tz) free(prev_tz);
	prev_tz = curr_tz; prev_stat = curr_stat; 
      }
    }
    tzset();
  }
#endif
  // OTHER OS/LIBRARY FIXES SHOULD GO HERE, IF DESIRED.  PLEASE TRY TO
  // KEEP THEM INDEPENDENT.
  return;
}

// This value follows the peripheral device type value as defined in
// SCSI Primary Commands, ANSI INCITS 301:1997.  It is also used in
// the ATA standard for packet devices to define the device type.
const char *packetdevicetype(int type){
  if (type<0x10)
    return packet_types[type];
  
  if (type<0x20)
    return "Reserved";
  
  return "Unknown";
}


// Returns 1 if machine is big endian, else zero.  This is a run-time
// rather than a compile-time function.  We could do it at
// compile-time but in principle there are architectures that can run
// with either byte-ordering.
int isbigendian(){
  short i=0x0100;
  char *tmp=(char *)&i;
  return *tmp;
}

// Utility function prints date and time and timezone into a character
// buffer of length>=64.  All the fuss is needed to get the right
// timezone info (sigh).
void dateandtimezoneepoch(char *buffer, time_t tval){
  struct tm *tmval;
  char *timezonename;
  char datebuffer[DATEANDEPOCHLEN];
  int lenm1;

  FixGlibcTimeZoneBug();
  
  // Get the time structure.  We need this to determine if we are in
  // daylight savings time or not.
  tmval=localtime(&tval);
  
  // Convert to an ASCII string, put in datebuffer
  // same as: asctime_r(tmval, datebuffer);
  strncpy(datebuffer, asctime(tmval), DATEANDEPOCHLEN);
  datebuffer[DATEANDEPOCHLEN-1]='\0';
  
  // Remove newline
  lenm1=strlen(datebuffer)-1;
  datebuffer[lenm1>=0?lenm1:0]='\0';
  
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
  snprintf(buffer, DATEANDEPOCHLEN, "%s %s", datebuffer, timezonename);
  
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
  int retVal=0;
  const char delimiters[] = " ,$";

  // make a copy on the heap, go to first token,
  if (!(copy=strdup(cvsid)))
    return 0;

  if (!(filename=strtok(copy, delimiters))){
    free(copy);
    return 0;
  }

  // move to first instance of "Id:"
  while (strcmp(filename,"Id:"))
    if (!(filename=strtok(NULL, delimiters))){
      free(copy);
      return 0;
    }
  
  // get filename, skip "v", get version and date
  if (!(  filename=strtok(NULL, delimiters)  ) ||
      !(           strtok(NULL, delimiters)  ) ||
      !(   version=strtok(NULL, delimiters)  ) ||
      !(      date=strtok(NULL, delimiters)  ) ) {
    free(copy);
    return 0;
  }
  
  sprintf(out,"%-16s revision: %-5s date: %-15s", filename, version, date);
  retVal = (date-copy)+strlen(date);
  free(copy);
  return  retVal;
}

// prints a single set of CVS ids
void printone(char *block, const char *cvsid){
  char strings[CVSMAXLEN];
  const char *here=cvsid;
  int line=1,len=strlen(cvsid)+1;

  // check that the size of the output block is sufficient
  if (len>=CVSMAXLEN) {
    pout("CVSMAXLEN=%d must be at least %d\n",CVSMAXLEN,len+1);
    EXIT(1);
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
    pout("Please inform smartmontools developers at " PACKAGE_BUGREPORT "\n");
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
    char *tailptr;

    *s++ = '\0';
    if (*s == '0' || !isdigit((int)*s))  // The integer part must be positive
      return 1;
    errno = 0;
    *i = (int) strtol(s, &tailptr, 10);
    if (errno || *tailptr != '\0')
      return 1;
  } else {
    // There's no integer part.
    *i = 1;
  }

  return 0;
}

// same as above but sets *i to -1 if missing , argument
int split_report_arg2(char *s, int *i){
  char *tailptr;
  s+=6;

  if (*s=='\0' || !isdigit((int)*s)) { 
    // What's left must be integer
    *i=-1;
    return 1;
  }

  errno = 0;
  *i = (int) strtol(s, &tailptr, 10);
  if (errno || *tailptr != '\0') {
    *i=-1;
    return 1;
  }

  return 0;
}

// Splits an argument to the -t option that is assumed to be of the form
// "selective,%lld-%lld" (prefixes of "0" (for octal) and "0x"/"0X" (for hex)
// are allowed).  The first long long int is assigned to *start and the second
// to *stop.  Returns zero if successful and non-zero otherwise.
int split_selective_arg(char *s, uint64_t *start,
                        uint64_t *stop)
{
  char *tailptr;

  if (!(s = strchr(s, ',')))
    return 1;
  if (!isdigit((int)(*++s)))
    return 1;
  errno = 0;
  // Last argument to strtoull (the base) is 0 meaning that decimal is assumed
  // unless prefixes of "0" (for octal) or "0x"/"0X" (for hex) are used.
  *start = strtoull(s, &tailptr, 0);

  s = tailptr;
  if (errno || *s++ != '-')
    return 1;
  *stop = strtoull(s, &tailptr, 0);
  if (errno || *tailptr != '\0')
    return 1;
  return 0;
}

int64_t bytes = 0;
// Helps debugging.  If the second argument is non-negative, then
// decrement bytes by that amount.  Else decrement bytes by (one plus)
// length of null terminated string.
void *FreeNonZero(void *address, int size, int line, const char* file){
  if (address) {
    if (size<0)
      bytes-=1+strlen(address);
    else
      bytes-=size;
    return CheckFree(address, line, file);
  }
  return NULL;
}

// To help with memory checking.  Use when it is known that address is
// NOT null.
void *CheckFree(void *address, int whatline, const char* file){
  if (address){
    free(address);
    return NULL;
  }
  
  PrintOut(LOG_CRIT, "Internal error in CheckFree() at line %d of file %s\n%s", 
           whatline, file, reportbug);
  EXIT(EXIT_BADCODE);
}

// A custom version of calloc() that tracks memory use
void *Calloc(size_t nmemb, size_t size) { 
  void *ptr=calloc(nmemb, size);
  
  if (ptr)
    bytes+=nmemb*size;

  return ptr;
}

// A custom version of strdup() that keeps track of how much memory is
// being allocated. If mustexist is set, it also throws an error if we
// try to duplicate a NULL string.
char *CustomStrDup(char *ptr, int mustexist, int whatline, const char* file){
  char *tmp;

  // report error if ptr is NULL and mustexist is set
  if (ptr==NULL){
    if (mustexist) {
      PrintOut(LOG_CRIT, "Internal error in CustomStrDup() at line %d of file %s\n%s", 
               whatline, file, reportbug);
      EXIT(EXIT_BADCODE);
    }
    else
      return NULL;
  }

  // make a copy of the string...
  tmp=strdup(ptr);
  
  if (!tmp) {
    PrintOut(LOG_CRIT, "No memory to duplicate string %s\n", ptr);
    EXIT(EXIT_NOMEM);
  }
  
  // and track memory usage
  bytes+=1+strlen(ptr);
  
  return tmp;
}

// Returns nonzero if region of memory contains non-zero entries
int nonempty(unsigned char *testarea,int n){
  int i;
  for (i=0;i<n;i++)
    if (testarea[i])
      return 1;
  return 0;
}


// This routine converts an integer number of milliseconds into a test
// string of the form Xd+Yh+Zm+Ts.msec.  The resulting text string is
// written to the array.
void MsecToText(unsigned int msec, char *txt){
  int start=0;
  unsigned int days, hours, min, sec;

  days       = msec/86400000U;
  msec      -= days*86400000U;

  hours      = msec/3600000U; 
  msec      -= hours*3600000U;

  min        = msec/60000U;
  msec      -= min*60000U;

  sec        = msec/1000U;
  msec      -= sec*1000U;

  if (days) {
    txt += sprintf(txt, "%2dd+", (int)days);
    start=1;
  }

  sprintf(txt, "%02d:%02d:%02d.%03d", (int)hours, (int)min, (int)sec, (int)msec);  
  return;
}



