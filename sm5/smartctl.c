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
#ifdef HAVE_GETOPT_H
#include <getopt.h>
#endif
#include "smartctl.h"
#include "atacmds.h"
#include "ataprint.h"
#include "scsicmds.h"
#include "scsiprint.h"
#include "extern.h"

extern const char *CVSid1, *CVSid2, *CVSid3, *CVSid4; 
const char* CVSid5="$Id: smartctl.c,v 1.30 2002/12/11 00:11:31 pjwilliams Exp $"
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

const char shortopts[] = {
  S_OPT_HELP,
  S_OPT_ALT_HELP,
  S_OPT_VERSION,
  S_OPT_QUIETMODE,
  ':',
  S_OPT_DEVICE,
  ':',
  S_OPT_TOLERANCE,
  ':',
  S_OPT_BADSUM,
  ':',
  S_OPT_SMART,
  ':',
  S_OPT_OFFLINEAUTO,
  ':',
  S_OPT_SAVEAUTO,
  ':',
  S_OPT_HEALTH,
  S_OPT_CAPABILITIES,
  S_OPT_ATTRIBUTES,
  S_OPT_LOG,
  ':',
  S_OPT_INFO,
  S_OPT_ALL,
  S_OPT_VENDORATTRIBUTE,
  ':',
  S_OPT_TEST,
  ':',
  S_OPT_CAPTIVE,
  S_OPT_ABORT,
  '\0'
};

unsigned char printcopyleft=0,tryata=0,tryscsi=0;

/*      Takes command options and sets features to be run */	
void ParseOpts (int argc, char** argv){
  int optchar;
  int badarg;
  int captive;
  struct {
    int n;
    char *option;
  } vendorattribute;
  extern char *optarg;
  extern int optopt, optind, opterr;
#ifdef HAVE_GETOPT_LONG
  char *arg;
  struct option longopts[] = {
    { L_OPT_HELP,            no_argument,       0, S_OPT_HELP            },
    { L_OPT_USAGE,           no_argument,       0, S_OPT_HELP            },
    { L_OPT_VERSION,         no_argument,       0, S_OPT_VERSION         },
    { L_OPT_COPYRIGHT,       no_argument,       0, S_OPT_VERSION         },
    { L_OPT_LICENSE,         no_argument,       0, S_OPT_VERSION         },
    { L_OPT_QUIETMODE,       required_argument, 0, S_OPT_QUIETMODE       },
    { L_OPT_DEVICE,          required_argument, 0, S_OPT_DEVICE          },
    { L_OPT_TOLERANCE,       required_argument, 0, S_OPT_TOLERANCE       },
    { L_OPT_BADSUM,          required_argument, 0, S_OPT_BADSUM          },
    { L_OPT_SMART,           required_argument, 0, S_OPT_SMART           },
    { L_OPT_OFFLINEAUTO,     required_argument, 0, S_OPT_OFFLINEAUTO     },
    { L_OPT_SAVEAUTO,        required_argument, 0, S_OPT_SAVEAUTO        },
    { L_OPT_HEALTH,          no_argument,       0, S_OPT_HEALTH          },
    { L_OPT_CAPABILITIES,    no_argument,       0, S_OPT_CAPABILITIES    },
    { L_OPT_ATTRIBUTES,      no_argument,       0, S_OPT_ATTRIBUTES      },
    { L_OPT_LOG,             required_argument, 0, S_OPT_LOG             },
    { L_OPT_INFO,            no_argument,       0, S_OPT_INFO            },
    { L_OPT_ALL,             no_argument,       0, S_OPT_ALL             },
    { L_OPT_VENDORATTRIBUTE, required_argument, 0, S_OPT_VENDORATTRIBUTE },
    { L_OPT_TEST,            required_argument, 0, S_OPT_TEST            },
    { L_OPT_CAPTIVE,         no_argument,       0, S_OPT_CAPTIVE         },
    { L_OPT_ABORT,           no_argument,       0, S_OPT_ABORT           },
    { 0,                     0,                 0, 0                     }
  };
#endif
  
  memset(con,0,sizeof(*con));
  con->testcase=-1;
  opterr=optopt=0;
  badarg = captive = FALSE;
#ifdef HAVE_GETOPT_LONG
  while (-1 != (optchar = getopt_long(argc, argv, shortopts, longopts, NULL))) {
#else
  while (-1 != (optchar = getopt(argc, argv, shortopts))) {
#endif
    switch (optchar){
    case S_OPT_VERSION:
      printcopyleft=TRUE;
      break;
    case S_OPT_QUIETMODE:
      if (!strcmp(optarg,"errorsonly")) {
        con->quietmode     = TRUE;
        con->veryquietmode = FALSE;
      } else if (!strcmp(optarg,"silent")) {
        con->veryquietmode = TRUE;
        con->quietmode     = TRUE;
      } else {
        badarg = TRUE;
      }
      break;
    case S_OPT_DEVICE:
      if (!strcmp(optarg,"ata")) {
        tryata  = TRUE;
        tryscsi = FALSE;
      } else if (!strcmp(optarg,"scsi")) {
        tryata  = TRUE;
        tryscsi = FALSE;
      } else {
        badarg = TRUE;
      }
      break;
    case S_OPT_TOLERANCE:
      if (!strcmp(optarg,"normal")) {
        con->conservative = FALSE;
        con->permissive   = FALSE;
      } else if (!strcmp(optarg,"conservative")) {
        con->conservative = TRUE;
        con->permissive   = FALSE;
      } else if (!strcmp(optarg,"permissive")) {
        con->permissive   = TRUE;
        con->conservative = FALSE;
      } else {
        badarg = TRUE;
      }
      break;
    case S_OPT_BADSUM:
      if (!strcmp(optarg,"warn")) {
        con->checksumfail   = FALSE;
        con->checksumignore = FALSE;
      } else if (!strcmp(optarg,"exit")) {
        con->checksumfail   = TRUE;
        con->checksumignore = FALSE;
      } else if (!strcmp(optarg,"ignore")) {
        con->checksumignore = TRUE;
        con->checksumfail   = FALSE;
      } else {
        badarg = TRUE;
      }
      break;
    case S_OPT_SMART:
      if (!strcmp(optarg,"on")) {
        con->smartenable  = TRUE;
        con->smartdisable = FALSE;
      } else if (!strcmp(optarg,"off")) {
        con->smartdisable = TRUE;
        con->smartenable  = FALSE;
      } else {
        badarg = TRUE;
      }
      break;
    case S_OPT_OFFLINEAUTO:
      if (!strcmp(optarg,"on")) {
        con->smartautoofflineenable  = TRUE;
        con->smartautoofflinedisable = FALSE;
      } else if (!strcmp(optarg,"off")) {
        con->smartautoofflinedisable = TRUE;
        con->smartautoofflineenable  = FALSE;
      } else {
        badarg = TRUE;
      }
      break;
    case S_OPT_SAVEAUTO:
      if (!strcmp(optarg,"on")) {
        con->smartautosaveenable  = TRUE;
        con->smartautosavedisable = FALSE;
      } else if (!strcmp(optarg,"off")) {
        con->smartautosavedisable = TRUE;
        con->smartautosaveenable  = FALSE;
      } else {
        badarg = TRUE;
      }
      break;
    case S_OPT_HEALTH:
      con->checksmart = TRUE;		
      break;
    case S_OPT_CAPABILITIES:
      con->generalsmartvalues = TRUE;
      break;
    case S_OPT_ATTRIBUTES:
      con->smartvendorattrib = TRUE;
      break;
    case S_OPT_LOG:
      if (!strcmp(optarg,"error")) {
        con->smarterrorlog = TRUE;
      } else if (!strcmp(optarg,"selftest")) {
        con->smartselftestlog = TRUE;
      } else {
        badarg = TRUE;
      }
      break;
    case S_OPT_INFO:
      con->driveinfo = TRUE;
      break;		
    case S_OPT_ALL:
      con->driveinfo          = TRUE;
      con->checksmart         = TRUE;
      con->generalsmartvalues = TRUE;
      con->smartvendorattrib  = TRUE;
      con->smarterrorlog      = TRUE;
      con->smartselftestlog   = TRUE;
      break;
    case S_OPT_VENDORATTRIBUTE:
      vendorattribute.option = (char *)malloc(strlen(optarg)+1);
      if (sscanf(optarg,"%u,%s",&(vendorattribute.n),vendorattribute.option) != 2) {
        badarg = TRUE;
      }
      if (vendorattribute.n == 9 && !strcmp(vendorattribute.option,"minutes")) {
        con->smart009minutes=TRUE;
      } else {
        // Should handle this better
        badarg = TRUE;
      }
      free(vendorattribute.option);
      break;
    case S_OPT_TEST:
      if (!strcmp(optarg,"offline")) {
        con->smartexeoffimmediate = TRUE;
        con->testcase             = OFFLINE_FULL_SCAN;
      } else if (!strcmp(optarg,"short")) {
        con->smartshortselftest = TRUE;
        con->testcase           = SHORT_SELF_TEST;
      } else if (!strcmp(optarg,"long")) {
        con->smartextendselftest = TRUE;
        con->testcase            = EXTEND_SELF_TEST;
      } else {
        badarg = TRUE;
      }
      break;
    case S_OPT_CAPTIVE:
      captive = TRUE;
      break;
    case S_OPT_ABORT:
      con->smartselftestabort = TRUE;
      con->testcase           = ABORT_SELF_TEST;
      break;
    case S_OPT_HELP:
    case S_OPT_ALT_HELP:
    default:
      con->veryquietmode=FALSE;
      printslogan();
#ifdef HAVE_GETOPT_LONG
      // Point arg to the argument in which this option was found.
      arg = argv[optind-1];
      // Check whether the option is a long option and options that map to -h.
      if (arg[1] == '-' && optchar != S_OPT_HELP) {
        // Iff optopt holds a valid option then argument must be missing.
        if (optopt && (strchr(shortopts, optopt) != NULL)) {
          pout("=======> ARGUMENT REQUIRED FOR OPTION: %s <=======\n\n",arg+2);
        } else {
          pout("=======> UNRECOGNIZED OPTION: %s <=======\n\n",arg+2);
        }
        Usage();
        exit(FAILCMD);
      }
#endif
      if (optopt) {
        // Iff optopt holds a valid option then argument must be missing.
        if (strchr(shortopts, optopt) != NULL){
          pout("=======> ARGUMENT REQUIRED FOR OPTION: %c <=======\n\n",optopt);
        } else {
	  pout("=======> UNRECOGNIZED OPTION: %c <=======\n\n",optopt);
        }
	Usage();
	exit(FAILCMD);
      }
      Usage();
      exit(0);	
    }
    if (badarg) {
        pout("=======> INVALID ARGUMENT: %s <======= \n\n",optarg);
        Usage();
	exit(FAILCMD);
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

  // If captive option was used, change test type if appropriate.
  if (captive && con->smartshortselftest) {
      con->smartshortselftest    = FALSE;
      con->smartshortcapselftest = TRUE;
      con->testcase              = SHORT_CAPTIVE_SELF_TEST;
  } else if (captive && con->smartextendselftest) {
      con->smartextendselftest    = FALSE;
      con->smartextendcapselftest = TRUE;
      con->testcase               = EXTEND_CAPTIVE_SELF_TEST;
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

  // open device - read-only mode is enough to issue needed commands
  fd = open(device=argv[argc-1], O_RDONLY);  
  if (fd<0) {
    char errmsg[256];
    snprintf(errmsg,256,"Smartctl open device: %s failed",argv[argc-1]);
    errmsg[255]='\0';
    syserror(errmsg);
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
    scsiPrintMain (device, fd);
  else {
    pout("Smartctl: specify if this is an ATA or SCSI device with the -%c or -%c options respectively.\n",
	 NOTSCSIDEVICE, NOTATADEVICE);
    Usage();
    return FAILCMD;
  }

  return retval;
}
