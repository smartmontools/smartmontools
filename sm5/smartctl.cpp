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
const char* CVSid5="$Id: smartctl.cpp,v 1.32 2003/01/03 17:25:12 ballen4705 Exp $"
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
void Usage (void){
  printf("Usage: smartctl [options] [device]\n");
  printf("\nShow Information Options:\n");
#ifdef HAVE_GETOPT_LONG
  printf("\
  -h, --help, --usage\n\
         Display this help and exit\n\
  -?\n\
         Same as -h\n\
  -V, --version, --copyright, --license\n\
         Print license, copyright, and version information\n\
  -i, --info                                                          (ATA/SCSI)\n\
         Show drive information\n\
  -a, --all                                                           (ATA/SCSI)\n\
         Show all SMART information for device\n\
");
#else
  printf("\
  -h        Display this help and exit\n\
  -?        Same as -h\n\
  -V        Print license, copyright, and version information\n\
  -i        Show drive information                                    (ATA/SCSI)\n\
  -a        Show all SMART information for device                     (ATA/SCSI)\n\
");
#endif
  printf("\n");
  printf("Run-time Behavior Options:\n");
#ifdef HAVE_GETOPT_LONG
  printf("\
  -q TYPE, --quietmode=TYPE                                           (ATA)\n\
         Set the quiet mode to one of: errorsonly, silent\n\
  -d TYPE, --device=TYPE\n\
         Set the device type to one of: ata, scsi\n\
  -T TYPE, --tolerance=TYPE                                           (ATA)\n\
         Set tolerance to one of: normal, conservative, permissive\n\
  -b TYPE, --badsum=TYPE                                              (ATA)\n\
         Set action on bad checksum to one of: warn, exit, ignore\n\
");
#else
  printf("\
  -q TYPE   Set the quiet mode to one of: errorsonly, silent          (ATA)\n\
  -d TYPE   Set the device type to one of: ata, scsi\n\
  -T TYPE   Set tolerance to one of: normal, conservative, permissive (ATA)\n\
  -b TYPE   Set action on bad checksum to one of: warn, exit, ignore  (ATA)\n\
");
#endif
  printf("\n");
  printf("SMART Feature Enable/Disable Commands:\n");
#ifdef HAVE_GETOPT_LONG
  printf("\
  -s VALUE, --smart=VALUE                                             (ATA/SCSI)\n\
        Enable/disable SMART (on/off)\n\
  -o VALUE, --offlineauto=VALUE                                       (ATA)\n\
        Enable/disable automatic offline testing (on/off)\n\
  -S VALUE, --saveauto=VALUE                                          (ATA)\n\
        Enable/disable attribute autosave (on/off)\n\
");
#else
  printf("\
  -s VALUE  Enable/disable SMART (on/off)                             (ATA/SCSI)\n\
  -o VALUE  Enable/disable automatic offline testing (on/off)         (ATA)\n\
  -S VALUE  Enable/disable attribute autosave (on/off)                (ATA)\n\
");
#endif
  printf("\n");
  printf("Read and Display Data Options:\n");
#ifdef HAVE_GETOPT_LONG
  printf("\
  -H, --health                                                        (ATA/SCSI)\n\
        Show SMART health status\n\
  -c, --capabilities                                                  (ATA)\n\
        Show SMART capabilities\n\
  -A, --attributes                                                    (ATA)\n\
        Show SMART vendor-specific attributes and values\n\
  -l TYPE, --log=TYPE                                                 (ATA)\n\
        Show device log. Type is one of: error, selftest\n\
  -v N,OPTION , --vendorattribute=N,OPTION                            (ATA)\n\
        Set vendor specific OPTION for attribute N (see man page)\n\
");
#else
  printf("\
  -H        Show SMART health status                                  (ATA/SCSI)\n\
  -c        Show SMART capabilities                                   (ATA)\n\
  -A        Show SMART vendor-specific attributes and values          (ATA)\n\
  -l TYPE   Show device log. Type is one of: error, selftest          (ATA)\n\
  -v N,OPT  Set vendor specific OPTion for attribute N (see man page) (ATA)\n\
");
#endif
  printf("\n");
  printf("Self-Test Options:\n");
#ifdef HAVE_GETOPT_LONG
  printf("\
  -t TEST, --test=TEST                                                (ATA/SCSI)\n\
        Test immediately.  TEST is one of: offline, short, long\n\
  -C, --captive                                                       (ATA/SCSI)\n\
        With -t, performs test in captive mode (short/long only)\n\
  -X, --abort                                                         (ATA/SCSI)\n\
        Abort any non-captive test\n\
");
#else
  printf("\
  -t TEST   Test immediately.  TEST is one of: offline, short, long   (ATA/SCSI)\n\
  -C        With -t, performs test in captive mode (short/long only)  (ATA/SCSI)\n\
  -X        Abort any non-captive test                                (ATA/SCSI)\n\
");
#endif
  printf("\n");
  printf("Examples:\n");
#ifdef HAVE_GETOPT_LONG
  printf("\
  smartctl -a /dev/hda\n\
                                           (Prints all SMART information)\n\
  smartctl --smart=on --offlineauto=on --saveauto=on /dev/hda\n\
                                           (Enables SMART on first disk)\n\
  smartctl -t long /dev/hda\n\
                                           (Executes extended disk self-test)\n\
  smartctl --attributes --log=selftest --quietmode=errorsonly /dev/hda\n\
                                           (Prints Self-Test & Attribute errors)\n\
");
#else
  printf("\
  smartctl -a /dev/hda\n\
                                           (Prints all SMART information)\n\
  smartctl -s on -o on -S on /dev/hda\n\
                                           (Enables SMART on first disk)\n\
  smartctl -t long /dev/hda\n\
                                           (Executes extended disk self-test)\n\
  smartctl -A -l selftest -q errorsonly /dev/hda\n\
                                           (Prints Self-Test & Attribute errors)\n\
");
#endif
}

/* Print an appropriate error message for option opt when given an invalid argument, optarg */
void printbadargmessage(int opt, const char *optarg) {
  const char **ps;

  pout("=======> INVALID ARGUMENT OF -%c: %s <======= \n", opt, optarg);
  pout("=======> VALID ARGUMENTS ARE: ");
  switch (opt) {
  case 'q':
     pout("errorsonly, silent");
     break;
  case 'd':
     pout("ata, scsi");
     break;
  case 'T':
     pout("normal, conservative, permissive");
     break;
  case 'b':
     pout("warn, exit, ignore");
     break;
  case 's':
  case 'o':
  case 'S':
     pout("on, off");
     break;
  case 'l':
     pout("error, selftest");
     break;
  case 'v':
     // Print all strings in vendorattributeargs separated by commas.  The
     // strings themselves contain commas, so surrounding double quotes are used
     // for clarity.
     for (ps = vendorattributeargs; *ps != NULL; ps++)
       pout("\"%s\"%s", *ps, *(ps+1) ? ", " : "");
     break;
  case 't':
     pout("offline, short, long");
     break;
  }
  pout(" <======= \n\n");
}

unsigned char printcopyleft=0,tryata=0,tryscsi=0;

/*      Takes command options and sets features to be run */	
void ParseOpts (int argc, char** argv){
  int optchar;
  int badarg;
  int captive;
  extern char *optarg;
  extern int optopt, optind, opterr;
  const char *shortopts = "h?Vq:d:T:b:s:o:S:HcAl:iav:t:CX";
#ifdef HAVE_GETOPT_LONG
  char *arg;
  struct option longopts[] = {
    { "help",            no_argument,       0, 'h' },
    { "usage",           no_argument,       0, 'h' },
    { "version",         no_argument,       0, 'V' },
    { "copyright",       no_argument,       0, 'V' },
    { "license",         no_argument,       0, 'V' },
    { "quietmode",       required_argument, 0, 'q' },
    { "device",          required_argument, 0, 'd' },
    { "tolerance",       required_argument, 0, 'T' },
    { "badsum",          required_argument, 0, 'b' },
    { "smart",           required_argument, 0, 's' },
    { "offlineauto",     required_argument, 0, 'o' },
    { "saveauto",        required_argument, 0, 'S' },
    { "health",          no_argument,       0, 'H' },
    { "capabilities",    no_argument,       0, 'c' },
    { "attributes",      no_argument,       0, 'A' },
    { "log",             required_argument, 0, 'l' },
    { "info",            no_argument,       0, 'i' },
    { "all",             no_argument,       0, 'a' },
    { "vendorattribute", required_argument, 0, 'v' },
    { "test",            required_argument, 0, 't' },
    { "captive",         no_argument,       0, 'C' },
    { "abort",           no_argument,       0, 'X' },
    { 0,                 0,                 0, 0   }
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
    case 'V':
      printcopyleft=TRUE;
      break;
    case 'q':
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
    case 'd':
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
    case 'T':
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
    case 'b':
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
    case 's':
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
    case 'o':
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
    case 'S':
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
    case 'H':
      con->checksmart = TRUE;		
      break;
    case 'c':
      con->generalsmartvalues = TRUE;
      break;
    case 'A':
      con->smartvendorattrib = TRUE;
      break;
    case 'l':
      if (!strcmp(optarg,"error")) {
        con->smarterrorlog = TRUE;
      } else if (!strcmp(optarg,"selftest")) {
        con->smartselftestlog = TRUE;
      } else {
        badarg = TRUE;
      }
      break;
    case 'i':
      con->driveinfo = TRUE;
      break;		
    case 'a':
      con->driveinfo          = TRUE;
      con->checksmart         = TRUE;
      con->generalsmartvalues = TRUE;
      con->smartvendorattrib  = TRUE;
      con->smarterrorlog      = TRUE;
      con->smartselftestlog   = TRUE;
      break;
    case 'v':
      // parse vendor-specific definitions of attributes
      if (parse_attribute_def(optarg, con->attributedefs))
	badarg = TRUE;
      break;    
    case 't':
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
    case 'C':
      captive = TRUE;
      break;
    case 'X':
      con->smartselftestabort = TRUE;
      con->testcase           = ABORT_SELF_TEST;
      break;
    case 'h':
    case '?':
    default:
      con->veryquietmode=FALSE;
      printslogan();
#ifdef HAVE_GETOPT_LONG
      // Point arg to the argument in which this option was found.
      arg = argv[optind-1];
      // Check whether the option is a long option that doesn't map to -h.
      if (arg[1] == '-' && optchar != 'h') {
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
        printbadargmessage(optchar, optarg);
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
    pout("Smartctl: specify if this is an ATA or SCSI device with the -d option.\n");
    Usage();
    return FAILCMD;
  }

  return retval;
}
