/*
 * smartctl.c
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


#include <stdio.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/hdreg.h>
#include <sys/fcntl.h>
#include <sys/types.h>
#include <string.h>
#include <stdarg.h>
#include "smartctl.h"
#include "atacmds.h"
#include "ataprint.h"
#include "scsicmds.h"
#include "scsiprint.h"

extern const char *CVSid1, *CVSid2, *CVSid4, *CVSid5; 
const char* CVSid6="$Id: smartctl.c,v 1.15 2002/10/23 12:24:24 ballen4705 Exp $\n"
"\t" CVSID1 "\t" CVSID2 "\t" CVSID4 "\t" CVSID5 "\t" CVSID6 ;

unsigned char driveinfo               = FALSE;
unsigned char checksmart              = FALSE;
unsigned char smartvendorattrib       = FALSE;
unsigned char generalsmartvalues      = FALSE;
unsigned char smartselftestlog        = FALSE;
unsigned char smarterrorlog           = FALSE;
unsigned char smartdisable            = FALSE;
unsigned char smartenable             = FALSE;
unsigned char smartstatus             = FALSE;
unsigned char smartexeoffimmediate    = FALSE;
unsigned char smartshortselftest      = FALSE;
unsigned char smartextendselftest     = FALSE;
unsigned char smartshortcapselftest   = FALSE;
unsigned char smartextendcapselftest  = FALSE;
unsigned char smartselftestabort      = FALSE;
unsigned char smartautoofflineenable  = FALSE;
unsigned char smartautoofflinedisable = FALSE;
unsigned char smartautosaveenable     = FALSE;
unsigned char smartautosavedisable    = FALSE;
unsigned char printcopyleft           = FALSE;
unsigned char smart009minutes         = FALSE;
unsigned char quietmode               = FALSE;
unsigned char veryquietmode           = FALSE;
int           testcase                = -1;


void printslogan(){
  pout("smartctl version %d.%d-%d Copyright (C) 2002 Bruce Allen\n",RELEASE_MAJOR,RELEASE_MINOR,SMARTMONTOOLS_VERSION);
  pout("Home page of smartctl is %s\n\n",PROJECTHOME);
  return;
}



int massagecvs(char *out,const char *in){
  char filename[128], version[128], date[128];
  int i=0;
  const char *savein=in;

  // skip to I of $Id:
  while (*in !='\0' && *in!='I')
    in++;
  
  // skip to start of filename
  if (!*in)
    return 0;
  in+=4;

  // copy filename
  i=0;
  while (i<100 && *in!=',' && *in)
    filename[i++]=*in++;
  filename[i]='\0';
  if (!*in)
    return 0;

  // skip ,v and space
  in+=3;

  i=0;
  // copy version number
  while (i<100 && *in!=' ' && *in)
    version[i++]=*in++;
  version[i]='\0';
  if (!*in)
    return 0;

  // skip space
  in++;
  // copy date
  i=0;
  while (i<100 && *in!=' ' && *in)
    date[i++]=*in++;
  date[i]='\0';

  sprintf(out,"%-13s revision: %-6s date: %-15s", filename, version, date);
  return in-savein;
}

// prints a single set of CVS ids
void printone(const char *cvsid){
  char strings[512];
  const char *here;
  int len,line=1;
  here=cvsid;
  while ((len=massagecvs(strings,here))){
    switch (line++){
    case 1:
      pout("Module:");
      break;
    default:
      pout("  uses:");
    } 
    pout(" %s\n",strings);
    here+=len;
  }
  return;
}

void printcopy(){
  pout("smartctl comes with ABSOLUTELY NO WARRANTY. This\n");
  pout("is free software, and you are welcome to redistribute it\n");
  pout("under the terms of the GNU General Public License Version 2.\n");
  pout("See http://www.gnu.org for further details.\n\n");
  pout("CVS version IDs of files used to build this code are:\n");
  printone(CVSid6);
  printone(CVSid1);
  printone(CVSid2);
  printone(CVSid4);
  printone(CVSid5);
  return;
}

/*  void prints help information for command syntax */
void Usage ( void){
  printf( "Usage: smartctl -[options] [device]\n\n");
  printf( "Read Only Options:\n");
  printf( "\t\t%c\t\tShow version, copyright and license information\n", PRINTCOPYLEFT);
  printf( "\t\t%c\t\tShow all S.M.A.R.T. Information         (ATA and SCSI)\n",  SMARTVERBOSEALL);
  printf( "\t\t%c\t\tShow S.M.A.R.T. Drive Info              (ATA and SCSI)\n",  DRIVEINFO);
  printf( "\t\t%c\t\tShow S.M.A.R.T. Status                  (ATA and SCSI)\n",  CHECKSMART);
  printf( "\t\t%c\t\tShow S.M.A.R.T. General Attributes      (ATA Only)\n",  GENERALSMARTVALUES);
  printf( "\t\t%c\t\tShow S.M.A.R.T. Vendor Attributes       (ATA Only)\n",  SMARTVENDORATTRIB);
  printf( "\t\t%c\t\tShow S.M.A.R.T. Drive Error Log         (ATA Only\n",   SMARTERRORLOG);
  printf( "\t\t%c\t\tShow S.M.A.R.T. Drive Self Test Log     (ATA Only)\n",  SMARTSELFTESTLOG);
  printf( "\t\t%c\t\tQuiet: only show SMART drive errors     (ATA Only)\n",  QUIETMODE);
  printf( "\t\t%c\t\tVery Quiet: no display, use exit status (ATA Only)\n",  VERYQUIETMODE);
  printf( "\n");
  printf( "Vendor-specific Display Options:\n");
  printf( "\t\t%c\t\tRaw Attribute 009 is minutes            (ATA Only)\n",  SMART009MINUTES);
  printf( "\n");
  printf( "Enable/Disable Options:\n");
  printf( "\t\t%c\t\tEnable  S.M.A.R.T. data collection    (ATA and SCSI)\n",SMARTENABLE);
  printf( "\t\t%c\t\tDisable S.M.A.R.T. data collection    (ATA and SCSI)\n",SMARTDISABLE);
  printf( "\t\t%c\t\tEnable  S.M.A.R.T. Automatic Offline Test (ATA Only)\n",SMARTAUTOOFFLINEENABLE);
  printf( "\t\t%c\t\tDisable S.M.A.R.T. Automatic Offline Test (ATA Only)\n",SMARTAUTOOFFLINEDISABLE);
  printf( "\t\t%c\t\tEnable  S.M.A.R.T. Attribute Autosave     (ATA Only)\n",SMARTAUTOSAVEENABLE);
  printf( "\t\t%c\t\tDisable S.M.A.R.T. Attribute Autosave     (ATA Only)\n",SMARTAUTOSAVEDISABLE);
  printf( "\n");
  printf( "Test Options:\n");
  printf( "\t\t%c\t\tExecute Off-line data collection (ATA Only)\n",          SMARTEXEOFFIMMEDIATE);
  printf( "\t\t%c\t\tExecute Short Self Test (ATA Only)\n",                   SMARTSHORTSELFTEST );
  printf( "\t\t%c\t\tExecute Short Self Test (Captive Mode) (ATA Only)\n",    SMARTSHORTCAPSELFTEST );
  printf( "\t\t%c\t\tExecute Extended Self Test (ATA Only)\n",                SMARTEXTENDSELFTEST );
  printf( "\t\t%c\t\tExecute Extended Self Test (Captive Mode) (ATA Only)\n", SMARTEXTENDCAPSELFTEST );
  printf( "\t\t%c\t\tExecute Self Test Abort (ATA Only)\n",                 SMARTSELFTESTABORT );
  printf( "Examples:\n");
  printf("\tsmartctl -etf /dev/hda   (Enables S.M.A.R.T. on first disk)\n");
  printf("\tsmartctl -a   /dev/hda   (Prints all S.M.A.R.T. information)\n");
  printf("\tsmartctl -X   /dev/hda   (Executes extended disk self-test)\n\n");
  printf("Please see the man pages or %s for further information.\n",PROJECTHOME);
}

const char opts[] = { 
  DRIVEINFO, CHECKSMART, SMARTVERBOSEALL, SMARTVENDORATTRIB,
  GENERALSMARTVALUES, SMARTERRORLOG, SMARTSELFTESTLOG, SMARTDISABLE,
  SMARTENABLE, SMARTAUTOOFFLINEENABLE, SMARTAUTOOFFLINEDISABLE,
  SMARTEXEOFFIMMEDIATE, SMARTSHORTSELFTEST, SMARTEXTENDSELFTEST, 
  SMARTSHORTCAPSELFTEST, SMARTEXTENDCAPSELFTEST, SMARTSELFTESTABORT,
  SMARTAUTOSAVEENABLE,SMARTAUTOSAVEDISABLE,PRINTCOPYLEFT,SMART009MINUTES,QUIETMODE,VERYQUIETMODE,'\0'
};

/*      Takes command options and sets features to be run */	
void ParseOpts (int argc, char** argv){
  int optchar;
  extern char *optarg;
  extern int optopt, optind, opterr;
  
  opterr=1;
  while (-1 != (optchar = getopt(argc, argv, opts))) {
    switch (optchar){
    case QUIETMODE:
      quietmode=TRUE;
      veryquietmode=TRUE;
      break;
    case VERYQUIETMODE:
      quietmode=FALSE;
      veryquietmode=TRUE;
      break;
    case SMART009MINUTES:
      smart009minutes=TRUE;
      break;
    case PRINTCOPYLEFT :
      printcopyleft=TRUE;
      break;
    case DRIVEINFO :
      driveinfo  = TRUE;
      break;		
    case CHECKSMART :
      checksmart = TRUE;		
      break;
    case SMARTVERBOSEALL :
      driveinfo = TRUE;
      checksmart = TRUE;
      generalsmartvalues = TRUE;
      smartvendorattrib = TRUE;
      smarterrorlog = TRUE;
      smartselftestlog = TRUE;
      break;
    case SMARTVENDORATTRIB :
      smartvendorattrib = TRUE;
      break;
    case GENERALSMARTVALUES :
      generalsmartvalues = TRUE;
      break;
    case SMARTERRORLOG :
      smarterrorlog = TRUE;
      break;
    case SMARTSELFTESTLOG :
      smartselftestlog = TRUE;
      break;
    case SMARTDISABLE :
      smartdisable = TRUE;
      break;
    case SMARTENABLE :
      smartenable   = TRUE;
      break;
    case SMARTAUTOSAVEENABLE:
      smartautosaveenable = TRUE;
      break;
    case SMARTAUTOSAVEDISABLE:
      smartautosavedisable = TRUE;
      break;
    case SMARTAUTOOFFLINEENABLE: 
      smartautoofflineenable = TRUE;
      break;
    case SMARTAUTOOFFLINEDISABLE:
      smartautoofflinedisable = TRUE;
      break;
    case SMARTEXEOFFIMMEDIATE:
      smartexeoffimmediate = TRUE;
      testcase=OFFLINE_FULL_SCAN;
      break;
    case SMARTSHORTSELFTEST :
      smartshortselftest = TRUE;
      testcase=SHORT_SELF_TEST;
      break;
    case SMARTEXTENDSELFTEST :
      smartextendselftest = TRUE;
      testcase=EXTEND_SELF_TEST;
      break;
    case SMARTSHORTCAPSELFTEST:
      smartshortcapselftest = TRUE;
      testcase=SHORT_CAPTIVE_SELF_TEST;
      break;
    case SMARTEXTENDCAPSELFTEST:
      smartextendcapselftest = TRUE;
      testcase=EXTEND_CAPTIVE_SELF_TEST;
      break;
    case SMARTSELFTESTABORT:
      smartselftestabort = TRUE;
      testcase=ABORT_SELF_TEST;
      break;
    default:
      veryquietmode=FALSE;
      printslogan();
      Usage();
      exit(FAILCMD);	
    }
  }
  // error message if user has asked for more than one test
  if (1<(smartexeoffimmediate+smartshortselftest+smartextendselftest+
	 smartshortcapselftest+smartextendcapselftest+smartselftestabort)){
    veryquietmode=FALSE;
    printslogan();
    Usage();
    printf ("\nERROR: smartctl can only run a single test (or abort) at a time.\n\n");
    exit(FAILCMD);
  }

  // From here on, normal operations...
  printslogan();
  
  // Print Copyright/License info if needed
  if (printcopyleft){
    printcopy();
    if (argc==2)
      exit(0);
  }   
}


// Printing function (controlled by global veryquietmode)
void pout(char *fmt, ...){
  va_list ap;

  // initialize variable argument list 
  va_start(ap,fmt);
  if (veryquietmode){
    va_end(ap);
    return;
  }

  // print out
  vprintf(fmt,ap);
  va_end(ap);
  return;
}


/* Main Program */
int main (int argc, char **argv){
  int fd,retval=0;
  char *device;
  
  // Part input arguments
  ParseOpts(argc,argv);
    
  // Further argument checking
  if (argc != 3){
    Usage();
    return FAILCMD;
  }
  
  // open device - read-only mode is enough to issue needed commands
  fd = open(device=argv[2], O_RDONLY);
  
  if (fd<0) {
    perror("Smartctl device open failed");
    return FAILDEV;
  }
  
  if (device[5] == 'h')
    retval=ataPrintMain(fd);
  else if (device[5] == 's')
    scsiPrintMain (fd);
  else {
    Usage();
    return FAILCMD;
  }

  return retval;
}
