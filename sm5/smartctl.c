//  $Id: smartctl.c,v 1.10 2002/10/20 19:22:02 ballen4705 Exp $
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
#include "smartctl.h"
#include "atacmds.h"
#include "ataprint.h"
#include "scsicmds.h"
#include "scsiprint.h"

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
int           testcase                = -1;


/*  void Usage (void) 
	prints help information for command syntax */



void Usage ( void){
  printf( "Usage: smartctl -[options] [device]\n\n");
  printf( "Read Only Options:\n");
  printf( "\t\t%c\t\tShow version, copyright and license information\n", PRINTCOPYLEFT);
  printf( "\t\t%c\t\tShow all S.M.A.R.T. Information (ATA and SCSI)\n",  SMARTVERBOSEALL);
  printf( "\t\t%c\t\tShow S.M.A.R.T. Drive Info      (ATA and SCSI)\n",  DRIVEINFO);
  printf( "\t\t%c\t\tShow S.M.A.R.T. Status          (ATA and SCSI)\n",  CHECKSMART);
  printf( "\t\t%c\t\tShow S.M.A.R.T. General Attributes  (ATA Only)\n",  GENERALSMARTVALUES);
  printf( "\t\t%c\t\tShow S.M.A.R.T. Vendor Attributes   (ATA Only)\n",  SMARTVENDORATTRIB);
  printf( "\t\t%c\t\tShow S.M.A.R.T. Drive Error Log     (ATA Only\n",   SMARTERRORLOG);
  printf( "\t\t%c\t\tShow S.M.A.R.T. Drive Self Test Log (ATA Only)\n",  SMARTSELFTESTLOG);
  printf( "\n");
  printf( "Vendor-specific Display Options:\n");
  printf( "\t\t%c\t\tRaw Attribute 009 is minutes        (ATA Only)\n",  SMART009MINUTES);
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
  SMARTAUTOSAVEENABLE,SMARTAUTOSAVEDISABLE,PRINTCOPYLEFT,SMART009MINUTES,'\0'
};

/*      Takes command options and sets features to be run */	
void ParseOpts (int argc, char** argv){
  int optchar;
  extern char *optarg;
  extern int optopt, optind, opterr;
  
  opterr=1;
  while (-1 != (optchar = getopt(argc, argv, opts))) {
    switch (optchar){
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
      Usage();
      exit (-1);	
    }
    
    if ( (smartexeoffimmediate + smartshortselftest +
	  smartextendselftest + smartshortcapselftest +
	  smartextendcapselftest +smartselftestabort ) > 1){
      Usage();
      printf ("\nERROR: smartctl can only run a single test (or abort) at a time.\n\n");
      exit(-1);
    }
  }
}

/* Main Program */

int main (int argc, char **argv){
  int fd;
  char *device;
  
  printf("smartctl version %d.%d-%d Copyright (C) 2002 Bruce Allen\n",RELEASE_MAJOR,RELEASE_MINOR,SMARTMONTOOLS_VERSION);
  printf("Home page of smartctl is %s\n",PROJECTHOME);
  
  // Part input arguments
  ParseOpts (argc,argv);
  
  // Print Copyright/License info if needed
  if (printcopyleft){
    printf("\nsmartctl comes with ABSOLUTELY NO WARRANTY. This\n");
    printf("is free software, and you are welcome to redistribute it\n");
    printf("under the terms of the GNU General Public License Version 2.\n");
    printf("See http://www.gnu.org for further details.\n\n");
    printf("CVS version ID %s\n","$Id: smartctl.c,v 1.10 2002/10/20 19:22:02 ballen4705 Exp $");
    if (argc==2)
      exit(0);
 }

  // Further argument checking
  if ( argc != 3 ){
    Usage();
    exit (-1);
  }
    
  /* open device */
  fd = open ( device=argv[2], O_RDWR );
  
  if ( fd < 0) {
    perror ( "Device open failed");
    exit(-1);
  }
  
  if ( device[5] == 'h')
    ataPrintMain (fd );
  else if (device[5] == 's')
    scsiPrintMain (fd);
  else 
    Usage();
  
  return 0;
}
