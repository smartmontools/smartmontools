/*
 * utility.c
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

// THIS FILE IS INTENDED FOR UTILITY ROUTINES THAT ARE APPLICABLE TO
// BOTH SCSI AND ATA DEVICES, AND THAT MAY BE USED IN SMARTD,
// SMARTCTL, OR BOTH.

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include "utility.h"

// Any local header files should be represented by a CVSIDX just below.
const char* utility_c_cvsid="$Id: utility.c,v 1.1 2003/01/16 15:28:58 ballen4705 Exp $" UTILITY_H_CVSID;

// Utility function prints date and time and timezone into a character
// buffer of length>=64.  All the fuss is needed to get the right
// timezone info (sigh).
void dateandtimezone(char *buffer){
  time_t tval;
  struct tm *tmval;
  char *timezonename;
  char datebuffer[64];
  
  // First get the time in seconds since Jan 1 1970
  tval=time(NULL);
  
  // Now get the time structure.  We need this to determine if we
  // are in daylight savings time or not.
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
