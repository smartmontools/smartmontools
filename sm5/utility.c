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
#include "utility.h"

// Any local header files should be represented by a CVSIDX just below.
const char* utility_c_cvsid="$Id: utility.c,v 1.6 2003/04/13 16:05:23 pjwilliams Exp $" UTILITY_H_CVSID;


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
    fprintf(stderr,"CVSMAXLEN=%d must be at least %d\n",CVSMAXLEN,len+1);
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
