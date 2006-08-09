/*
 * utility.cpp
 *
 * Home page of code is: http://smartmontools.sourceforge.net
 *
 * Copyright (C) 2002-6 Bruce Allen <smartmontools-support@lists.sourceforge.net>
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
#include <sys/stat.h>
#ifdef _WIN32
#include <mbstring.h> // _mbsinc()
#endif

#include "config.h"
#include "int64.h"
#include "utility.h"

// Any local header files should be represented by a CVSIDX just below.
const char* utility_c_cvsid="$Id: utility.cpp,v 1.62 2006/08/09 20:40:20 chrfranke Exp $"
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

#ifdef _WIN32
// Fix strings in tzname[] to avoid long names with non-ascii characters.
// If TZ is not set, tzset() in the MSVC runtime sets tzname[] to the
// national language timezone names returned by GetTimezoneInformation().
static char * fixtzname(char * dest, int destsize, const char * src)
{
  int i = 0, j = 0;
  while (src[i] && j < destsize-1) {
    int i2 = (const char *)_mbsinc((const unsigned char *)src+i) - src;
    if (i2 > i+1)
      i = i2; // Ignore multibyte chars
    else {
      if ('A' <= src[i] && src[i] <= 'Z')
        dest[j++] = src[i]; // "Pacific Standard Time" => "PST"
      i++;
    }
  }
  if (j < 2)
    j = 0;
  dest[j] = 0;
  return dest;
}
#endif // _WIN32

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
#ifdef _WIN32
  char tzfixbuf[6+1];
#endif

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

#ifdef _WIN32
  // Fix long non-ascii timezone names
  if (!getenv("TZ"))
    timezonename=fixtzname(tzfixbuf, sizeof(tzfixbuf), timezonename);
#endif
  
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

  if (!(filename=strtok(copy, delimiters)))
    goto endmassage;

  // move to first instance of "Id:"
  while (strcmp(filename,"Id:"))
    if (!(filename=strtok(NULL, delimiters)))
      goto endmassage;
  
  // get filename, skip "v", get version and date
  if (!(  filename=strtok(NULL, delimiters)  ) ||
      !(           strtok(NULL, delimiters)  ) ||
      !(   version=strtok(NULL, delimiters)  ) ||
      !(      date=strtok(NULL, delimiters)  ) )
    goto endmassage;
  
  sprintf(out,"%-16s revision: %-5s date: %-15s", filename, version, date);
  retVal = (date-copy)+strlen(date);
  
 endmassage:
  free(copy);
  return retVal;
}

// prints a single set of CVS ids
void printone(char *block, const char *cvsid){
  char strings[CVSMAXLEN];
  const char *here=cvsid;
  int bi=0, len=strlen(cvsid)+1;

  // check that the size of the output block is sufficient
  if (len>=CVSMAXLEN) {
    pout("CVSMAXLEN=%d must be at least %d\n",CVSMAXLEN,len+1);
    EXIT(1);
  }

  // loop through the different strings
  while (bi<CVSMAXLEN && (len=massagecvs(strings,here))){
    bi+=snprintf(block+bi,CVSMAXLEN-bi,"%s %s\n",(bi==0?"Module:":"  uses:"),strings);
    here+=len;
  }
  return;
}


// A replacement for perror() that sends output to our choice of
// printing. If errno not set then just print message.
void syserror(const char *message){
  
  if (errno) {
    // Get the correct system error message:
    const char *errormessage=strerror(errno);
    
    // Check that caller has handed a sensible string, and provide
    // appropriate output. See perrror(3) man page to understand better.
    if (message && *message)
      pout("%s: %s\n",message, errormessage);
    else
      pout("%s\n",errormessage);
  }
  else if (message && *message)
    pout("%s\n",message);
  
  return;
}

// Prints a warning message for a failed regular expression compilation from
// regcomp().
void printregexwarning(int errcode, regex_t *compiled){
  size_t length = regerror(errcode, compiled, NULL, 0);
  char *buffer = (char*)malloc(length);
  if (!buffer){
    pout("Out of memory in printregexwarning()\n");
    return;
  }
  regerror(errcode, compiled, buffer, length);
  pout("%s\n", buffer);
  free(buffer);
  return;
}

// POSIX extended regular expressions interpret unmatched ')' ordinary:
// "The close-parenthesis shall be considered special in this context
//  only if matched with a preceding open-parenthesis."
//
// Actual '(...)' nesting errors remain undetected on strict POSIX
// implementations (glibc) but an error is reported on others (Cygwin).
// 
// The check below is rather incomplete because it does not handle
// e.g. '\)' '[)]'.
// But it should work for the regex subset used in drive database.
static int check_regex_nesting(const char * pattern)
{
  int level = 0, i;
  for (i = 0; pattern[i] && level >= 0; i++) {
    switch (pattern[i]) {
      case '(': level++; break;
      case ')': level--; break;
    }
  }
  return level;
}

// A wrapper for regcomp().  Returns zero for success, non-zero otherwise.
int compileregex(regex_t *compiled, const char *pattern, int cflags)
{ 
  int errorcode;

  if (   (errorcode = regcomp(compiled, pattern, cflags))
      || check_regex_nesting(pattern) < 0                ) {
    pout("Internal error: unable to compile regular expression \"%s\" ", pattern);
    if (errorcode)
      printregexwarning(errorcode, compiled);
    else
      pout("Unmatched ')'\n");
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

#ifndef HAVE_STRTOULL
// Replacement for missing strtoull() (Linux with libc < 6, MSVC 6.0)
// Functionality reduced to split_selective_arg()'s requirements.

static uint64_t strtoull(const char * p, char * * endp, int base)
{
  uint64_t result, maxres;
  int i = 0;
  char c = p[i++];
  // assume base == 0
  if (c == '0') {
    if (p[i] == 'x' || p[i] == 'X') {
      base = 16; i++;
    }
    else
      base = 8;
    c = p[i++];
  }
  else
    base = 10;

  result = 0;
  maxres = ~(uint64_t)0 / (unsigned)base;
  for (;;) {
    unsigned digit;
    if ('0' <= c && c <= '9')
      digit = c - '0';
    else if ('A' <= c && c <= 'Z')
      digit = c - 'A' + 10;
    else if ('a' <= c && c <= 'z')
      digit = c - 'a' + 10;
    else
      break;
    if (digit >= (unsigned)base)
      break;
    if (!(   result < maxres
          || (result == maxres && digit <= ~(uint64_t)0 % (unsigned)base))) {
      result = ~(uint64_t)0; errno = ERANGE; // return on overflow
      break;
    }
    result = result * (unsigned)base + digit;
    c = p[i++];
  }
  *endp = (char *)p + i - 1;
  return result;
}
#endif // HAVE_STRTOLL

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
void *FreeNonZero1(void *address, int size, int line, const char* file){
  if (address) {
    if (size<0)
      bytes-=1+strlen((char*)address);
    else
      bytes-=size;
    return CheckFree1(address, line, file);
  }
  return NULL;
}

// To help with memory checking.  Use when it is known that address is
// NOT null.
void *CheckFree1(void *address, int whatline, const char* file){
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
    PrintOut(LOG_CRIT, "No memory to duplicate string %s at line %d of file %s\n", ptr, whatline, file);
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


#ifndef HAVE_WORKING_SNPRINTF
// Some versions of (v)snprintf() don't append null char on overflow (MSVCRT.DLL),
// and/or return -1 on overflow (old Linux).
// Below are sane replacements substituted by #define in utility.h.

#undef vsnprintf
#if defined(_WIN32) && defined(_MSC_VER)
#define vsnprintf _vsnprintf
#endif

int safe_vsnprintf(char *buf, int size, const char *fmt, va_list ap)
{
  int i;
  if (size <= 0)
    return 0;
  i = vsnprintf(buf, size, fmt, ap);
  if (0 <= i && i < size)
    return i;
  buf[size-1] = 0;
  return strlen(buf); // Note: cannot detect for overflow, not necessary here.
}

int safe_snprintf(char *buf, int size, const char *fmt, ...)
{
  int i; va_list ap;
  va_start(ap, fmt);
  i = safe_vsnprintf(buf, size, fmt, ap);
  va_end(ap);
  return i;
}

#endif
