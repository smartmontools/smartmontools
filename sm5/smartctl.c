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
#include "extern.h"

extern const char *CVSid1, *CVSid2, *CVSid3, *CVSid4; 
const char* CVSid5="$Id: smartctl.c,v 1.26 2002/11/07 11:00:56 ballen4705 Exp $"
CVSID1 CVSID2 CVSID3 CVSID4 CVSID5 CVSID6;

// This is a block containing all the "control variables".  We declare
// this globally in this file, and externally in other files.
atamainctrl *con=NULL;

void printslogan(){
  pout("smartctl version %d.%d-%d Copyright (C) 2002 Bruce Allen\n",
      (int)RELEASE_MAJOR, (int)RELEASE_MINOR, (int)SMARTMONTOOLS_VERSION);
  pout("Home page is %s\n\n",PROJECTHOME);
  return;
}


void printcopy(){
  char out[CVSMAXLEN];
  pout("smartctl comes with ABSOLUTELY NO WARRANTY. This\n");
  pout("is free software, and you are welcome to redistribute it\n");
  pout("under the terms of the GNU General Public License Version 2.\n");
  pout("See http://www.gnu.org for further details.\n\n");
  pout("CVS version IDs of files used to build this code are:\n");
  printone(out,CVSid1);
  pout("%s",out);
  printone(out,CVSid2);
  pout("%s",out);
  printone(out,CVSid3);
  pout("%s",out);
  printone(out,CVSid4);
  pout("%s",out);
  printone(out,CVSid5);
  pout("%s",out);
  return;
}

/*  void prints help information for command syntax */
void Usage ( void){
  printf("Usage: smartctl -[options] [device]\n");
  printf("\nShow Information Options:\n");
  printf("  %c  Show Version, Copyright and License info\n", PRINTCOPYLEFT);
  printf("  %c  Show SMART Drive Info                   (ATA/SCSI)\n",DRIVEINFO);
  printf("  %c  Show all SMART Information              (ATA/SCSI)\n",SMARTVERBOSEALL);
  printf("\nRun-time Behavior Options:\n");
  printf("  %c  Quiet: only show SMART Drive Errors     (ATA Only)\n",    QUIETMODE);
  printf("  %c  Very Quiet: no display, use Exit Status (ATA Only)\n",    VERYQUIETMODE);  
  printf("  %c  Device is an ATA Device                 (ATA Only)\n",    NOTSCSIDEVICE);
  printf("  %c  Device is a SCSI Device                 (SCSI Only)\n",   NOTATADEVICE);
  printf("  %c  Permissive: continue on Mandatory fails (ATA Only)\n",    PERMISSIVE);
  printf("  %c  Conservative: exit if Optional Cmd fail (ATA Only)\n",    ULTRACONSERVATIVE);  
  printf("  %c  Warning: exit if Struct Checksum bad    (ATA Only)\n",    EXITCHECKSUMERROR);
  printf("\nSMART Feature Enable/Disable Commands:\n");
  printf("  %c  Enable  SMART data collection           (ATA/SCSI)\n",    SMARTENABLE);
  printf("  %c  Disable SMART data collection           (ATA/SCSI)\n",    SMARTDISABLE);
  printf("  %c  Enable  SMART Automatic Offline Test    (ATA Only)\n",    SMARTAUTOOFFLINEENABLE);
  printf("  %c  Disable SMART Automatic Offline Test    (ATA Only)\n",    SMARTAUTOOFFLINEDISABLE);
  printf("  %c  Enable  SMART Attribute Autosave        (ATA Only)\n",    SMARTAUTOSAVEENABLE);
  printf("  %c  Disable SMART Attribute Autosave        (ATA Only)\n",    SMARTAUTOSAVEDISABLE);
  printf("\nRead and Display Data Options:\n");
  printf("  %c  Show SMART Status                       (ATA/SCSI)\n",    CHECKSMART);
  printf("  %c  Show SMART General Attributes           (ATA Only)\n",    GENERALSMARTVALUES);
  printf("  %c  Show SMART Vendor Attributes            (ATA Only)\n",    SMARTVENDORATTRIB);
  printf("  %c  Show SMART Drive Error Log              (ATA Only\n",     SMARTERRORLOG);
  printf("  %c  Show SMART Drive Self Test Log          (ATA Only)\n",    SMARTSELFTESTLOG);
  printf("\nVendor-specific Attribute Display Options:\n");
  printf("  %c  Raw Attribute id=009 stored in minutes  (ATA Only)\n",    SMART009MINUTES);
  printf("\nSelf-Test Options (no more than one):\n");
  printf("  %c  Execute Off-line data collection        (ATA/SCSI)\n",    SMARTEXEOFFIMMEDIATE);
  printf("  %c  Execute Short Self Test                 (ATA/SCSI)\n",    SMARTSHORTSELFTEST );
  printf("  %c  Execute Short Self Test (Captive Mode)  (ATA/SCSI)\n",    SMARTSHORTCAPSELFTEST );
  printf("  %c  Execute Extended Self Test              (ATA/SCSI)\n",    SMARTEXTENDSELFTEST );
  printf("  %c  Execute Extended Self Test (Captive)    (ATA/SCSI)\n",    SMARTEXTENDCAPSELFTEST );
  printf("  %c  Execute Self Test Abort                 (ATA/SCSI)\n",    SMARTSELFTESTABORT );
  printf("\nExamples:\n");
  printf("  smartctl -etf /dev/hda  (Enables SMART on first disk)\n");
  printf("  smartctl -a   /dev/hda  (Prints all SMART information)\n");
  printf("  smartctl -X   /dev/hda  (Executes extended disk self-test)\n");
  printf("  smartctl -qvL /dev/hda  (Prints Self-Test & Attribute errors.)\n");
}

const char opts[] = { 
  DRIVEINFO, CHECKSMART, SMARTVERBOSEALL, SMARTVENDORATTRIB,
  GENERALSMARTVALUES, SMARTERRORLOG, SMARTSELFTESTLOG, SMARTDISABLE,
  SMARTENABLE, SMARTAUTOOFFLINEENABLE, SMARTAUTOOFFLINEDISABLE,
  SMARTEXEOFFIMMEDIATE, SMARTSHORTSELFTEST, SMARTEXTENDSELFTEST, 
  SMARTSHORTCAPSELFTEST, SMARTEXTENDCAPSELFTEST, SMARTSELFTESTABORT,
  SMARTAUTOSAVEENABLE,SMARTAUTOSAVEDISABLE,PRINTCOPYLEFT,SMART009MINUTES,
  QUIETMODE,VERYQUIETMODE,NOTSCSIDEVICE,NOTATADEVICE,
  EXITCHECKSUMERROR,ULTRACONSERVATIVE,PERMISSIVE,
  'h','?','\0'
};

unsigned char printcopyleft=0,tryata=0,tryscsi=0;

/*      Takes command options and sets features to be run */	
void ParseOpts (int argc, char** argv){
  int optchar;
  extern char *optarg;
  extern int optopt, optind, opterr;
  
  memset(con,0,sizeof(*con));
  con->testcase=-1;
  opterr=optopt=0;
  while (-1 != (optchar = getopt(argc, argv, opts))) {
    switch (optchar){
    case EXITCHECKSUMERROR:
      con->checksumfail=1;
      break;
    case PERMISSIVE:
      con->permissive=1;
      break;
    case ULTRACONSERVATIVE:
      con->conservative=1;
      break;
    case NOTATADEVICE:
      tryata=0;
      tryscsi=1;
      break;
    case NOTSCSIDEVICE:
      tryata=1;
      tryscsi=0;
      break;
    case QUIETMODE:
      con->quietmode=TRUE;
      break;
    case VERYQUIETMODE:
      con->veryquietmode=TRUE;
      break;
    case SMART009MINUTES:
      con->smart009minutes=TRUE;
      break;
    case PRINTCOPYLEFT :
      printcopyleft=TRUE;
      break;
    case DRIVEINFO :
      con->driveinfo  = TRUE;
      break;		
    case CHECKSMART :
      con->checksmart = TRUE;		
      break;
    case SMARTVERBOSEALL :
      con->driveinfo = TRUE;
      con->checksmart = TRUE;
      con->generalsmartvalues = TRUE;
      con->smartvendorattrib = TRUE;
      con->smarterrorlog = TRUE;
      con->smartselftestlog = TRUE;
      break;
    case SMARTVENDORATTRIB :
      con->smartvendorattrib = TRUE;
      break;
    case GENERALSMARTVALUES :
      con->generalsmartvalues = TRUE;
      break;
    case SMARTERRORLOG :
      con->smarterrorlog = TRUE;
      break;
    case SMARTSELFTESTLOG :
      con->smartselftestlog = TRUE;
      break;
    case SMARTDISABLE :
      con->smartdisable = TRUE;
      break;
    case SMARTENABLE :
      con->smartenable   = TRUE;
      break;
    case SMARTAUTOSAVEENABLE:
      con->smartautosaveenable = TRUE;
      break;
    case SMARTAUTOSAVEDISABLE:
      con->smartautosavedisable = TRUE;
      break;
    case SMARTAUTOOFFLINEENABLE: 
      con->smartautoofflineenable = TRUE;
      break;
    case SMARTAUTOOFFLINEDISABLE:
      con->smartautoofflinedisable = TRUE;
      break;
    case SMARTEXEOFFIMMEDIATE:
      con->smartexeoffimmediate = TRUE;
      con->testcase=OFFLINE_FULL_SCAN;
      break;
    case SMARTSHORTSELFTEST :
      con->smartshortselftest = TRUE;
      con->testcase=SHORT_SELF_TEST;
      break;
    case SMARTEXTENDSELFTEST :
      con->smartextendselftest = TRUE;
      con->testcase=EXTEND_SELF_TEST;
      break;
    case SMARTSHORTCAPSELFTEST:
      con->smartshortcapselftest = TRUE;
      con->testcase=SHORT_CAPTIVE_SELF_TEST;
      break;
    case SMARTEXTENDCAPSELFTEST:
      con->smartextendcapselftest = TRUE;
      con->testcase=EXTEND_CAPTIVE_SELF_TEST;
      break;
    case SMARTSELFTESTABORT:
      con->smartselftestabort = TRUE;
      con->testcase=ABORT_SELF_TEST;
      break;
    case 'h':
    case '?':
    default:
      con->veryquietmode=FALSE;
      printslogan();
      if (optopt){
	pout("=======> UNRECOGNIZED OPTION: %c <=======\n\n",(int)optopt);
	Usage();
	exit(FAILCMD);
      }
      Usage();
      exit(0);	
    }
  }
  // Do this here, so results are independent of argument order	
  if (con->quietmode)
    con->veryquietmode=TRUE;
  
  // error message if user has asked for more than one test
  if (1<(con->smartexeoffimmediate+con->smartshortselftest+con->smartextendselftest+
	 con->smartshortcapselftest+con->smartextendcapselftest+con->smartselftestabort)){
    con->veryquietmode=FALSE;
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


// Printing function (controlled by global con->veryquietmode) 

// [From GLIBC Manual: Since the prototype doesn't specify types for
// optional arguments, in a call to a variadic function the default
// argument promotions are performed on the optional argument
// values. This means the objects of type char or short int (whether
// signed or not) are promoted to either int or unsigned int, as
// appropriate.]
void pout(char *fmt, ...){
  va_list ap;

  // initialize variable argument list 
  va_start(ap,fmt);
  if (con->veryquietmode){
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
  atamainctrl control;
  const char *devroot="/dev/";

  // define control block for external functions
  con=&control;

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

  // if necessary, try to guess if this is an ATA or SCSI device
  if (!tryata && !tryscsi) {
    if (!strncmp(device,devroot,strlen(devroot)) && strlen(device)>5){
      if (device[5] == 'h')
	tryata=1;
      if (device[5] == 's')
	tryscsi=1;
    }
    else if (strlen(device)){
	if (device[0] == 'h')
	  tryata=1;
	if (device[0] == 's')
	  tryscsi=1;
      }
  }

  // now call appropriate ATA or SCSI routine
  if (tryata)
    retval=ataPrintMain(fd);
  else if (tryscsi)
    scsiPrintMain (fd);
  else {
    pout("Smartctl: specify if this is an ATA or SCSI device with the -%c or -%c options respectively.\n",
	 NOTSCSIDEVICE, NOTATADEVICE);
    Usage();
    return FAILCMD;
  }

  return retval;
}
