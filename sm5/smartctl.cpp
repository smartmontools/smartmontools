//  $Id: smartctl.cpp,v 1.6 2002/10/11 12:15:50 ballen4705 Exp $
/*
 * smartctl.c
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


/*  void Usage (void) 
	prints help information for command syntax */



void Usage ( void){
  printf( "usage: smartctl -[options] [device]\n");
  printf( "Read Only Options:\n");
  printf( "\t\t%c\t\tPrint Copyright and License information\n", 
	  PRINTCOPYLEFT);
  printf( "\t\t%c\t\tShow All S.M.A.R.T. Information (ATA and SCSI)\n", 
	  SMARTVERBOSEALL);
  printf( "\t\t%c\t\tShow General S.M.A.R.T. Attributes (ATA Only)\n",
	  GENERALSMARTVALUES);
  printf( "\t\t%c\t\tShow Vendor S.M.A.R.T. Attributes (ATA Only)\n", 
	  SMARTVENDORATTRIB);
  printf( "\t\t%c\t\tShow S.M.A.R.T. Drive Error Log (ATA Only\n", 
	  SMARTERRORLOG);
  printf( "\t\t%c\t\tShow S.M.A.R.T. Drive Self Test Log (ATA Only)\n", 
	  SMARTSELFTESTLOG);
  printf( "\t\t%c\t\tShow S.M.A.R.T. Drive Info (ATA and SCSI)\n", DRIVEINFO);
  printf( "\t\t%c\t\tCheck S.M.A.R.T. Status (ATA and SCSI)\n", CHECKSMART);
  printf( "\n");
  printf( "Enable/Disable Options:\n");
  printf( "\t\t%c\t\tEnable S.M.A.R.T. data collection (ATA and SCSI)\n", 
	  SMARTENABLE);
  printf( "\t\t%c\t\tDisable S.M.A.R.T.data collection (ATA and SCSI)\n", 
	  SMARTDISABLE);
  printf( "\t\t%c\t\tEnable S.M.A.R.T. Automatic Offline Test (ATA Only)\n", 
	  SMARTAUTOOFFLINEENABLE);
  printf( "\t\t%c\t\tDisable S.M.A.R.T. Automatic Offline Test (ATA Only)\n", 
	  SMARTAUTOOFFLINEDISABLE);
  printf( "\t\t%c\t\tEnable S.M.A.R.T. Attribute Autosave (ATA Only)\n", 
	  SMARTAUTOSAVEENABLE);
  printf( "\t\t%c\t\tDisable S.M.A.R.T. Attribute Autosave (ATA Only)\n", 
	  SMARTAUTOSAVEDISABLE);
  printf( "\n");
  printf( "Test Options:\n");
  printf( "\t\t%c\t\tExecute Off-line data collection (ATA Only)\n",
	  SMARTEXEOFFIMMEDIATE);
  printf( "\t\t%c\t\tExecute Short Self Test (ATA Only)\n", 
	  SMARTSHORTSELFTEST );
  printf( "\t\t%c\t\tExecute Short Self Test (Captive Mode) (ATA Only)\n",
	  SMARTSHORTCAPSELFTEST );
  printf( "\t\t%c\t\tExecute Extended Self Test (ATA Only)\n", 
	  SMARTEXTENDSELFTEST );
  printf( "\t\t%c\t\tExecute Extended Self Test (Captive Mode) (ATA Only)\n", 
	  SMARTEXTENDCAPSELFTEST );
  printf( "\t\t%c\t\tExecute Self Test Abort (ATA Only)\n\n", 
	  SMARTSELFTESTABORT );
  printf( "Examples:\n");
  printf("\tsmartctl -etf /dev/hda   (Enables S.M.A.R.T. on first disk)\n");
  printf("\tsmartctl -a   /dev/hda   (Prints all S.M.A.R.T. information)\n");
  printf("\tsmartctl -X   /dev/hda   (Executes extended disk self-test)\n");
  printf("Please see the man pages or the web site for further information.\n");

}

const char opts[] = { 
  DRIVEINFO, CHECKSMART, SMARTVERBOSEALL, SMARTVENDORATTRIB,
  GENERALSMARTVALUES, SMARTERRORLOG, SMARTSELFTESTLOG, SMARTDISABLE,
  SMARTENABLE, SMARTAUTOOFFLINEENABLE, SMARTAUTOOFFLINEDISABLE,
  SMARTEXEOFFIMMEDIATE, SMARTSHORTSELFTEST, SMARTEXTENDSELFTEST, 
  SMARTSHORTCAPSELFTEST, SMARTEXTENDCAPSELFTEST, SMARTSELFTESTABORT,
  SMARTAUTOSAVEENABLE,SMARTAUTOSAVEDISABLE,PRINTCOPYLEFT,'\0'
};

/*      Takes command options and sets features to be run */	
void ParseOpts (int argc, char** argv){
  int optchar;
  extern char *optarg;
  extern int optopt, optind, opterr;
  
  opterr=1;
  while (-1 != (optchar = getopt(argc, argv, opts))) {
    switch (optchar){
    case PRINTCOPYLEFT :
      printcopyleft=TRUE;
      break;
    case DRIVEINFO :
      driveinfo  = TRUE;
      break;		
    case CHECKSMART :
      driveinfo  = TRUE;
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
      break;
    case SMARTSHORTSELFTEST :
      smartshortselftest = TRUE;
      break;
    case SMARTEXTENDSELFTEST :
      smartextendselftest = TRUE;
      break;
    case SMARTSHORTCAPSELFTEST:
      smartshortcapselftest = TRUE;
      break;
    case SMARTEXTENDCAPSELFTEST:
      smartextendcapselftest = TRUE;
      break;
    case SMARTSELFTESTABORT:
      smartselftestabort = TRUE;
      break;
    default:
      Usage();
      exit (-1);	
    }
    
    if ( (smartexeoffimmediate + smartshortselftest +
	  smartextendselftest + smartshortcapselftest +
	  smartextendcapselftest ) > 1){
      Usage();
      printf ("\n ERROR: smartctl can only run a single test at a time \n");
      exit(-1);
    }
  }
}

/* Main Program */

int main (int argc, char **argv){
  int fd;
  char *device;
  
  printf("smartctl version %d.%d-%d Copyright (C) 2002 Bruce Allen\n",RELEASE_MAJOR,RELEASE_MINOR,SMARTMONTOOLS_VERSION);
  printf("Home page of project is %s\n\n",PROJECTHOME);
  
  // Part input arguments
  ParseOpts (argc,argv);
  
  // Print Copyright/License info if needed
  if (printcopyleft){
    printf("smartctl comes with ABSOLUTELY NO WARRANTY. This\n");
    printf("is free software, and you are welcome to redistribute it\n");
    printf("under the terms of the GNU General Public License Version 2.\n");
    printf("See http://www.gnu.org for further details.\n\n");
    printf("CVS version ID %s\n","$Id: smartctl.cpp,v 1.6 2002/10/11 12:15:50 ballen4705 Exp $");
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
