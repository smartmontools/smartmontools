/*
 * smartctl.c
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
#include "atacmds.h"
#include "ataprint.h"
#include "extern.h"
#include "scsicmds.h"
#include "scsiprint.h"
#include "smartctl.h"
#include "utility.h"

extern const char *atacmds_c_cvsid, *ataprint_c_cvsid, *scsicmds_c_cvsid, *scsiprint_c_cvsid, *utility_c_cvsid; 
const char* smartctl_c_cvsid="$Id: smartctl.c,v 1.58 2003/04/05 05:32:31 ballen4705 Exp $"
ATACMDS_H_CVSID ATAPRINT_H_CVSID EXTERN_H_CVSID SCSICMDS_H_CVSID SCSIPRINT_H_CVSID SMARTCTL_H_CVSID UTILITY_H_CVSID;

// This is a block containing all the "control variables".  We declare
// this globally in this file, and externally in other files.
smartmonctrl *con=NULL;


void printslogan(){
  pout("smartctl version %d.%d-%d Copyright (C) 2002-3 Bruce Allen\n",
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
  printone(out,atacmds_c_cvsid);
  pout("%s",out);
  printone(out,ataprint_c_cvsid);
  pout("%s",out);
  printone(out,scsicmds_c_cvsid);
  pout("%s",out);
  printone(out,scsiprint_c_cvsid);
  pout("%s",out);
  printone(out,smartctl_c_cvsid);
  pout("%s",out);
  printone(out,utility_c_cvsid);
  pout("%s",out);
  return;
}

/*  void prints help information for command syntax */
void Usage (void){
  printf("Usage: smartctl [options] [device]\n");
  printf("\n==============  SHOW INFORMATION OPTIONS  =================================\n");
#ifdef HAVE_GETOPT_LONG
  printf("\
  -h, -?, --help, --usage\n\
         Display this help and exit\n\
  -V, --version, --copyright, --license\n\
         Print license, copyright, and version information and exit\n\
  -i, --info                                                       \n\
         Show identity information for device\n\
  -a, --all                                                        \n\
         Show all SMART information for device\n\
");
#else
  printf("\
  -h, -?    Display this help and exit\n\
  -V        Print license, copyright, and version information\n\
  -i        Show identity information for device\n\
  -a        Show all SMART information for device                   \n\
");
#endif
  printf("==============  SMARTCTL RUN-TIME BEHAVIOR OPTIONS  =======================\n");
#ifdef HAVE_GETOPT_LONG
  printf("\
  -q TYPE, --quietmode=TYPE                                           (ATA)\n\
         Set smartctl quiet mode to one of: errorsonly, silent\n\
  -d TYPE, --device=TYPE\n\
         Specify device type to one of: ata, scsi\n\
  -T TYPE, --tolerance=TYPE                                           (ATA)\n\
         Set tolerance to one of: normal, conservative, permissive\n\
  -b TYPE, --badsum=TYPE                                              (ATA)\n\
         Set action on bad checksum to one of: warn, exit, ignore\n\
  -r TYPE, --report=TYPE
         Report transactions for one of: ioctl, ataioctl, scsiioctl\n\
");
#else
  printf("\
  -q TYPE   Set smartctl quiet mode to one of: errorsonly, silent     (ATA)\n\
  -d TYPE   Specify device type to one of: ata, scsi\n\
  -T TYPE   Set tolerance to one of: normal, conservative, permissive (ATA)\n\
  -b TYPE   Set action on bad checksum to one of: warn, exit, ignore  (ATA)\n\
  -r TYPE   Report transactions for one of: ioctl, ataioctl, scsiioctl\n\
");
#endif
  printf("==============  DEVICE FEATURE ENABLE/DISABLE COMMANDS  ===================\n");
#ifdef HAVE_GETOPT_LONG
  printf("\
  -s VALUE, --smart=VALUE\n\
        Enable/disable SMART on device (on/off)\n\
  -o VALUE, --offlineauto=VALUE                                       (ATA)\n\
        Enable/disable automatic offline testing on device (on/off)\n\
  -S VALUE, --saveauto=VALUE                                          (ATA)\n\
        Enable/disable Attribute autosave on device (on/off)\n\
");
#else
  printf("\
  -s VALUE  Enable/disable SMART on device (on/off)\n\
  -o VALUE  Enable/disable device automatic offline testing (on/off)  (ATA)\n\
  -S VALUE  Enable/disable device Attribute autosave (on/off)         (ATA)\n\
");
#endif
  printf("==============  READ AND DISPLAY DATA OPTIONS  ============================\n");
#ifdef HAVE_GETOPT_LONG
  printf("\
  -H, --health\n\
        Show device SMART health status\n\
  -c, --capabilities                                                  (ATA)\n\
        Show device SMART capabilities\n\
  -A, --attributes                                                    (ATA)\n\
        Show device SMART vendor-specific Attributes and values\n\
  -l TYPE, --log=TYPE\n\
        Show device log. Type is one of: error (ATA), selftest\n\
  -v N,OPTION , --vendorattribute=N,OPTION                            (ATA)\n\
        Set display OPTION for vendor Attribute N (see man page)\n\
");
#else
  printf("\
  -H        Show device SMART health status\n\
  -c        Show device SMART capabilities                            (ATA)\n\
  -A        Show device SMART vendor-specific Attributes and values   (ATA)\n\
  -l TYPE   Show device log. Type is one of: error (ATA), selftest\n\
  -v N,OPT  Set display OPTion for vendor Attribute N (see man page)  (ATA)\n\
");
#endif
  printf("==============  DEVICE SELF-TEST OPTIONS  =================================\n");
#ifdef HAVE_GETOPT_LONG
  printf("\
  -t TEST, --test=TEST\n\
        Run test on device.  TEST is one of: offline, short, long\n\
  -C, --captive\n\
        With -t, performs test in captive mode (short/long only)\n\
  -X, --abort\n\
        Abort any non-captive test on device\n\
");
#else
  printf("\
  -t TEST   Run test on device.  TEST is one of: offline, short, long   \n\
  -C        With -t, performs test in captive mode (short/long only)  \n\
  -X        Abort any non-captive test                                \n\
");
#endif
  printf("==============  SMARTCTL EXAMPLES  ========================================\n");
#ifdef HAVE_GETOPT_LONG
  printf("\
  smartctl -a /dev/hda                 (Prints all SMART information)\n\
  smartctl --smart=on --offlineauto=on --saveauto=on /dev/hda\n\
                                       (Enables SMART on first disk)\n\
  smartctl -t long /dev/hda            (Executes extended disk self-test)\n\
  smartctl --attributes --log=selftest --quietmode=errorsonly /dev/hda\n\
                                       (Prints Self-Test & Attribute errors)\n\
");
#else
  printf("\
  smartctl -a /dev/hda                 (Prints all SMART information)\n\
  smartctl -s on -o on -S on /dev/hda  (Enables SMART on first disk)\n\
  smartctl -t long /dev/hda            (Executes extended disk self-test)\n\
  smartctl -A -l selftest -q errorsonly /dev/hda\n\
                                       (Prints Self-Test & Attribute errors)\n\
");
#endif
}

/* Returns a pointer to a static string containing a formatted list of the valid
   arguments to the option opt or NULL on failure. */
const char *getvalidarglist(char opt) {
  static char *v_list = NULL;
  char *s;

  switch (opt) {
  case 'q':
    return "errorsonly, silent";
  case 'd':
    return "ata, scsi";
  case 'T':
    return "normal, conservative, permissive";
  case 'b':
    return "warn, exit, ignore";
  case 'r':
    return "ioctl, ataioctl, scsiioctl";
  case 's':
  case 'o':
  case 'S':
    return "on, off";
  case 'l':
    return "error, selftest";
  case 'v':
    if (v_list) 
      return v_list;
    if (!(s = create_vendor_attribute_arg_list()))
      return NULL;
    // Allocate space for tab + "help" + newline + s + terminating 0
    v_list = (char *)malloc(7+strlen(s));
    sprintf(v_list, "\thelp\n%s", s);
    free(s);
    return v_list;
  case 't':
    return "offline, short, long";
  default:
    return NULL;
  }
}

/* Prints the message "=======> VALID ARGUMENTS ARE: <LIST>  <=======\n", where
   <LIST> is the list of valid arguments for option opt. */
void printvalidarglistmessage(char opt) {
  const char *s;
  char separator;

  if (!(s = getvalidarglist(opt))) {
    pout("Error whilst constructing argument list for option %c", opt);
    return;
  }

  // getvalidarglist() might produce a multiline or single line string.  We
  // need to figure out which to get the formatting right.
  separator = strchr(s, '\n') ? '\n' : ' ';

  pout("=======> VALID ARGUMENTS ARE:%c%s%c<=======\n", separator, (char *)s,
    separator);
}

unsigned char tryata=0,tryscsi=0;

/*      Takes command options and sets features to be run */	
void ParseOpts (int argc, char** argv){
  int optchar;
  int badarg;
  int captive;
  extern char *optarg;
  extern int optopt, optind, opterr;
  // Please update getvalidarglist() if you edit shortopts
  const char *shortopts = "h?Vq:d:T:b:r:s:o:S:HcAl:iav:t:CXF";
#ifdef HAVE_GETOPT_LONG
  char *arg;
  // Please update getvalidarglist() if you edit longopts
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
    { "report",          required_argument, 0, 'r' },
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
    { "fixbug",          no_argument,       0, 'F' },
    { 0,                 0,                 0, 0   }
  };
#endif
  
  memset(con,0,sizeof(*con));
  con->testcase=-1;
  opterr=optopt=0;
  badarg = captive = FALSE;
  
  // This miserable construction is needed to get emacs to do proper indenting. Sorry!
  while (-1 != (optchar = 
#ifdef HAVE_GETOPT_LONG
		getopt_long(argc, argv, shortopts, longopts, NULL)
#else
		getopt(argc, argv, shortopts)
#endif
		)){
    switch (optchar){
    case 'V':
      con->veryquietmode=FALSE;
      printslogan();
      printcopy();
      exit(0);
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
        tryata  = FALSE;
        tryscsi = TRUE;
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
    case 'r':
      if (!strcmp(optarg,"ioctl")) {
        con->reportataioctl++;
        con->reportscsiioctl++;
      } else if (!strcmp(optarg,"ataioctl")) {
        con->reportataioctl++;
      } else if (!strcmp(optarg,"scsiioctl")) {
        con->reportscsiioctl++;
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
    case 'F':
      con->fixbuginerrorlog = TRUE;
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
      } else if (!strcmp(optarg,"directory")) {
        con->smartlogdirectory = TRUE;
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
      if (!strcmp(optarg,"help")) {
        char *s;
        con->veryquietmode=FALSE;
        printslogan();
        if (!(s = create_vendor_attribute_arg_list())) {
          pout("Insufficient memory to construct argument list\n");
          exit(FAILCMD);
        }
        pout("The valid arguments to -v are:\n\thelp\n%s\n", s);
        free(s);
        exit(0);
      }
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
          pout("=======> ARGUMENT REQUIRED FOR OPTION: %s <=======\n", arg+2);
          printvalidarglistmessage(optopt);
	} else
	  pout("=======> UNRECOGNIZED OPTION: %s <=======\n",arg+2);
	pout("\nUse smartctl --help to get a usage summary\n\n");
	exit(FAILCMD);
      }
#endif
      if (optopt) {
        // Iff optopt holds a valid option then argument must be missing.
        if (strchr(shortopts, optopt) != NULL) {
          pout("=======> ARGUMENT REQUIRED FOR OPTION: %c <=======\n", optopt);
          printvalidarglistmessage(optopt);
        } else
	  pout("=======> UNRECOGNIZED OPTION: %c <=======\n",optopt);
	pout("\nUse smartctl -h to get a usage summary\n\n");
	exit(FAILCMD);
      }
      Usage();
      exit(0);	
    } // closes switch statement to process command-line options
    
    // Check to see if option had an unrecognized or incorrect argument.
    if (badarg) {
      printslogan();
      // It would be nice to print the actual option name given by the user
      // here, but we just print the short form.  Please fix this if you know
      // a clean way to do it.
      pout("=======> INVALID ARGUMENT TO -%c: %s <======= \n", optchar, optarg);
      printvalidarglistmessage(optchar);
      pout("\nUse smartctl -h to get a usage summary\n\n");
      exit(FAILCMD);
    }
  }
  // At this point we have processed all command-line options.

  // Do this here, so results are independent of argument order	
  if (con->quietmode)
    con->veryquietmode=TRUE;
  
  // error message if user has asked for more than one test
  if (1<(con->smartexeoffimmediate+con->smartshortselftest+con->smartextendselftest+
	 con->smartshortcapselftest+con->smartextendcapselftest+con->smartselftestabort)){
    con->veryquietmode=FALSE;
    printslogan();
    pout("\nERROR: smartctl can only run a single test (or abort) at a time.\n");
    pout("Use smartctl -h to get a usage summary\n\n");
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
  
  // Warn if the user has provided no device name
  if (argc-optind<1){
    pout("ERROR: smartctl requires a device name as the final command-line argument.\n\n");
    pout("Use smartctl -h to get a usage summary\n\n");
    exit(FAILCMD);
  }
  
  // Warn if the user has provided more than one device name
  if (argc-optind>1){
    int i;
    pout("ERROR: smartctl takes ONE device name as the final command-line argument.\n");
    pout("You have provided %d device names:\n",argc-optind);
    for (i=0; i<argc-optind; i++)
      pout("%s\n",argv[optind+i]);
    pout("Use smartctl -h to get a usage summary\n\n");
    exit(FAILCMD);
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
  fflush(stdout);
  return;
}


/* Main Program */
int main (int argc, char **argv){
  int fd,retval=0;
  char *device;
  smartmonctrl control;
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
