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

#include "config.h"
#include <stdio.h>
#include <unistd.h>
#include <sys/ioctl.h>
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
#include "knowndrives.h"
#include "scsicmds.h"
#include "scsiprint.h"
#include "smartctl.h"
#include "utility.h"

extern const char *atacmdnames_c_cvsid, *atacmds_c_cvsid, *ataprint_c_cvsid, *escalade_c_cvsid, *knowndrives_c_cvsid, *scsicmds_c_cvsid, *scsiprint_c_cvsid, *utility_c_cvsid; 
const char* smartctl_c_cvsid="$Id: smartctl.cpp,v 1.93 2003/10/03 03:51:16 ballen4705 Exp $"
ATACMDS_H_CVSID ATAPRINT_H_CVSID CONFIG_H_CVSID EXTERN_H_CVSID KNOWNDRIVES_H_CVSID SCSICMDS_H_CVSID SCSIPRINT_H_CVSID SMARTCTL_H_CVSID UTILITY_H_CVSID;

// This is a block containing all the "control variables".  We declare
// this globally in this file, and externally in other files.
smartmonctrl *con=NULL;

// Track memory use
long long bytes=0;

void printslogan(){
  pout("smartctl version %s Copyright (C) 2002-3 Bruce Allen\n", VERSION);
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
  printone(out,atacmdnames_c_cvsid);
  pout("%s",out);
  printone(out,atacmds_c_cvsid);
  pout("%s",out);
  printone(out,ataprint_c_cvsid);
  pout("%s",out);
  printone(out,escalade_c_cvsid);
  pout("%s",out);
  printone(out,knowndrives_c_cvsid);
  pout("%s",out);
  printone(out,scsicmds_c_cvsid);
  pout("%s",out);
  printone(out,scsiprint_c_cvsid);
  pout("%s",out);
  printone(out,smartctl_c_cvsid);
  pout("%s",out);
  printone(out,utility_c_cvsid);
  pout("%s",out);
  pout("\nsmartctl build configured on " SMARTMONTOOLS_CONFIGURE_DATE "\n");
  pout("smartctl configure arguments: " SMARTMONTOOLS_CONFIGURE_ARGS "\n");


  return;
}

/*  void prints help information for command syntax */
void Usage (void){
  printf("Usage: smartctl [options] device\n\n");
  printf("============================================ SHOW INFORMATION OPTIONS =====\n\n");
#ifdef HAVE_GETOPT_LONG
  printf(
"  -h, -?, --help, --usage\n"
"         Display this help and exit\n\n"
"  -V, --version, --copyright, --license\n"
"         Print license, copyright, and version information and exit\n\n"
"  -i, --info                                                       \n"
"         Show identity information for device\n\n"
"  -a, --all                                                        \n"
"         Show all SMART information for device\n\n"
  );
#else
  printf(
"  -h, -?    Display this help and exit\n"
"  -V        Print license, copyright, and version information\n"
"  -i        Show identity information for device\n"
"  -a        Show all SMART information for device\n\n"
  );
#endif
  printf("================================== SMARTCTL RUN-TIME BEHAVIOR OPTIONS =====\n\n");
#ifdef HAVE_GETOPT_LONG
  printf(
"  -q TYPE, --quietmode=TYPE                                           (ATA)\n"
"         Set smartctl quiet mode to one of: errorsonly, silent\n\n"
"  -d TYPE, --device=TYPE\n"
"         Specify device type to one of: ata, scsi, 3ware,N\n\n"
"  -T TYPE, --tolerance=TYPE                                           (ATA)\n"
"         Set tolerance to one of: normal, conservative, permissive\n\n"
"  -b TYPE, --badsum=TYPE                                              (ATA)\n"
"         Set action on bad checksum to one of: warn, exit, ignore\n\n"
"  -r TYPE, --report=TYPE\n"
"         Report transactions (see man page)\n\n"
  );
#else
  printf(
"  -q TYPE   Set smartctl quiet mode to one of: errorsonly, silent     (ATA)\n"
"  -d TYPE   Specify device type to one of: ata, scsi, 3ware,N\n"
"  -T TYPE   Set tolerance to one of: normal, conservative, permissive (ATA)\n"
"  -b TYPE   Set action on bad checksum to one of: warn, exit, ignore  (ATA)\n"
"  -r TYPE   Report transactions (see man page)\n\n"
  );
#endif
  printf("============================== DEVICE FEATURE ENABLE/DISABLE COMMANDS =====\n\n");
#ifdef HAVE_GETOPT_LONG
  printf(
"  -s VALUE, --smart=VALUE\n"
"        Enable/disable SMART on device (on/off)\n\n"
"  -o VALUE, --offlineauto=VALUE                                       (ATA)\n"
"        Enable/disable automatic offline testing on device (on/off)\n\n"
"  -S VALUE, --saveauto=VALUE                                          (ATA)\n"
"        Enable/disable Attribute autosave on device (on/off)\n\n"
  );
#else
  printf(
"  -s VALUE  Enable/disable SMART on device (on/off)\n"
"  -o VALUE  Enable/disable device automatic offline testing (on/off)  (ATA)\n"
"  -S VALUE  Enable/disable device Attribute autosave (on/off)         (ATA)\n\n"
  );
#endif
  printf("======================================= READ AND DISPLAY DATA OPTIONS =====\n\n");
#ifdef HAVE_GETOPT_LONG
  printf(
"  -H, --health\n"
"        Show device SMART health status\n\n"
"  -c, --capabilities                                                  (ATA)\n"
"        Show device SMART capabilities\n\n"
"  -A, --attributes                                                    (ATA)\n"
"        Show device SMART vendor-specific Attributes and values\n\n"
"  -l TYPE, --log=TYPE\n"
"        Show device log. Type is one of: error, selftest, directory\n\n"
"  -v N,OPTION , --vendorattribute=N,OPTION                            (ATA)\n"
"        Set display OPTION for vendor Attribute N (see man page)\n\n"
"  -F TYPE, --firmwarebug=TYPE                                         (ATA)\n"
"        Use firmware bug workaround. Type is one of: none, samsung\n\n"
"  -P TYPE, --presets=TYPE                                             (ATA)\n"
"        Drive-specific presets: use, ignore, show, showall\n\n"
  );
#else
  printf(
"  -H        Show device SMART health status\n"
"  -c        Show device SMART capabilities                             (ATA)\n"
"  -A        Show device SMART vendor-specific Attributes and values    (ATA)\n"
"  -l TYPE   Show device log. Type is one of: error, selftest, directory\n"
"  -v N,OPT  Set display OPTion for vendor Attribute N (see man page)   (ATA)\n"
"  -F TYPE   Use firmware bug workaround. Type is one of: none, samsung (ATA)\n"
"  -P TYPE   Drive-specific presets: use, ignore, show, showall         (ATA)\n\n"
  );
#endif
  printf("============================================ DEVICE SELF-TEST OPTIONS =====\n\n");
#ifdef HAVE_GETOPT_LONG
  printf(
"  -t TEST, --test=TEST\n"
#if DEVELOP_SELECTIVE_SELF_TEST
"        Run test.  TEST is: offline, short, long, conveyance, selective,M-N\n\n"
#else
"        Run test.  TEST is: offline, short, long, conveyance\n\n"
#endif

"  -C, --captive\n"
#if DEVELOP_SELECTIVE_SELF_TEST
"        With -t, do test in captive mode (short/long/conveyance/selective)\n\n"
#else
"        With -t, do test in captive mode (short/long/conveyance)\n\n"
#endif

"  -X, --abort\n"
"        Abort any non-captive test on device\n\n"
);
#else
  printf(
#if DEVELOP_SELECTIVE_SELF_TEST
"  -t TEST   Run test.  TEST is: offline, short, long, conveyance, selective,M-N\n"
"  -C        With -t, do test in captive mode (short/long/conveyance/selective)\n"
#else
"  -t TEST   Run test.  TEST is: offline, short, long, conveyance\n"
"  -C        With -t, do test in captive mode (short/long/conveyance)\n"
#endif
"  -X        Abort any non-captive test\n\n"
  );
#endif
  printf("=================================================== SMARTCTL EXAMPLES =====\n\n");
#ifdef HAVE_GETOPT_LONG
  printf(
"  smartctl -a /dev/hda                       (Prints all SMART information)\n\n"
"  smartctl --smart=on --offlineauto=on --saveauto=on /dev/hda\n"
"                                              (Enables SMART on first disk)\n\n"
"  smartctl -t long /dev/hda              (Executes extended disk self-test)\n\n"
"  smartctl --attributes --log=selftest --quietmode=errorsonly /dev/hda\n"
"                                      (Prints Self-Test & Attribute errors)\n"
"  smartctl -a -device=3ware,2 /dev/sda\n"
"          (Prints all SMART info for 3rd ATA disk on 3ware RAID controller)\n"
  );
#else
  printf(
"  smartctl -a /dev/hda                       (Prints all SMART information)\n"
"  smartctl -s on -o on -S on /dev/hda         (Enables SMART on first disk)\n"
"  smartctl -t long /dev/hda              (Executes extended disk self-test)\n"
"  smartctl -A -l selftest -q errorsonly /dev/hda\n"
"                                      (Prints Self-Test & Attribute errors)\n"
"  smartctl -a -d 3ware,2 /dev/sda\n"
"          (Prints all SMART info for 3rd ATA disk on 3ware RAID controller)\n"
  );
#endif
}

/* Returns a pointer to a static string containing a formatted list of the valid
   arguments to the option opt or NULL on failure. Note 'v' case different */
const char *getvalidarglist(char opt) {
  switch (opt) {
  case 'q':
    return "errorsonly, silent";
  case 'd':
    return "ata, scsi, 3ware,N";
  case 'T':
    return "normal, conservative, permissive";
  case 'b':
    return "warn, exit, ignore";
  case 'r':
    return "ioctl[,N], ataioctl[,N], scsiioctl[,N]";
  case 's':
  case 'o':
  case 'S':
    return "on, off";
  case 'l':
    return "error, selftest, directory";
  case 'P':
    return "use, ignore, show, showall";
  case 't':
#if DEVELOP_SELECTIVE_SELF_TEST
    return "offline, short, long, conveyance, selective,M-N";
#else
    return "offline, short, long, conveyance";
#endif
  case 'F':
    return "none, samsung";
  case 'v':
  default:
    return NULL;
  }
}

/* Prints the message "=======> VALID ARGUMENTS ARE: <LIST>  <=======\n", where
   <LIST> is the list of valid arguments for option opt. */
void printvalidarglistmessage(char opt) {
  char *s;
  
  if (opt=='v')
    s=create_vendor_attribute_arg_list();
  else
    s=(char *)getvalidarglist(opt);
  
  if (!s) {
    pout("Error whilst constructing argument list for option %c", opt);
    return;
  }
 
  if (opt=='v'){
    pout("=======> VALID ARGUMENTS ARE:\n\thelp\n%s\n<=======\n", s);
    free(s);
  }
  else {
  // getvalidarglist() might produce a multiline or single line string.  We
  // need to figure out which to get the formatting right.
    char separator = strchr(s, '\n') ? '\n' : ' ';
    pout("=======> VALID ARGUMENTS ARE:%c%s%c<=======\n", separator, (char *)s, separator);
  }

  return;
}


unsigned char tryata=0,tryscsi=0;

/*      Takes command options and sets features to be run */	
void ParseOpts (int argc, char** argv){
  int optchar;
  int badarg;
  int captive;
  unsigned char *charp;
  extern char *optarg;
  extern int optopt, optind, opterr;
  // Please update getvalidarglist() if you edit shortopts
  const char *shortopts = "h?Vq:d:T:b:r:s:o:S:HcAl:iav:P:t:CXF:";
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
    { "presets",         required_argument, 0, 'P' },
    { "test",            required_argument, 0, 't' },
    { "captive",         no_argument,       0, 'C' },
    { "abort",           no_argument,       0, 'X' },
    { "firmwarebug",     required_argument, 0, 'F' },
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
	con->escalade = 0;
      } else if (!strcmp(optarg,"scsi")) {
	tryata  = FALSE;
	tryscsi = TRUE;
	con->escalade = 0;
      } else {
	// look for RAID-type device
	int i;
        char *s;
	
	// make a copy of the string to mess with
	if (!(s = strdup(optarg))) {
          con->veryquietmode = FALSE;
          pout("No memory for argument of -d. Exiting...\n");
          exit(FAILCMD);
        } else if (strncmp(s,"3ware,",6)) {
	  badarg = TRUE;
	} else if (split_report_arg2(s, &i)) {
	  pout("Option -d 3ware,N requires N to be a non-negative integer\n");
	  badarg = TRUE;
        } else if (i<0 || i>15) {
	  pout("Option -d 3ware,N (N=%d) must have 0 <= N <= 15\n", i);
	  badarg = TRUE;
	} else {
	  // NOTE: escalade = disk number + 1
          con->escalade = i+1;
	  tryata  = TRUE;
	  tryscsi = FALSE;
	}
        free(s);
      } 	
      break;
    case 'T':
      if (!strcmp(optarg,"normal")) {
        con->conservative = FALSE;
        con->permissive   = FALSE;
      } else if (!strcmp(optarg,"conservative")) {
        con->conservative = TRUE;
      } else if (!strcmp(optarg,"permissive")) {
        con->permissive++;
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
      {
        int i;
        char *s;

        // split_report_arg() may modify its first argument string, so use a
        // copy of optarg in case we want optarg for an error message.
        if (!(s = strdup(optarg))) {
          con->veryquietmode = FALSE;
          pout("Can't allocate memory to copy argument to -r option"
               " - exiting\n");
          exit(FAILCMD);
        }
        if (split_report_arg(s, &i)) {
          badarg = TRUE;
        } else if (!strcmp(s,"ioctl")) {
          con->reportataioctl  = con->reportscsiioctl = i;
        } else if (!strcmp(s,"ataioctl")) {
          con->reportataioctl = i;
        } else if (!strcmp(s,"scsiioctl")) {
          con->reportscsiioctl = i;
        } else {
          badarg = TRUE;
        }
        free(s);
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
      if (!strcmp(optarg,"none")) {
        con->fixfirmwarebug = FIX_NONE;
      } else if (!strcmp(optarg,"samsung")) {
        con->fixfirmwarebug = FIX_SAMSUNG;
      } else {
        badarg = TRUE;
      }
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
      charp=con->attributedefs;
      if (!charp){
	pout("Fatal internal error in ParseOpts()\n");
	exit(FAILCMD);
      }
      if (parse_attribute_def(optarg, &charp))
	badarg = TRUE;
      break;    
    case 'P':
      if (!strcmp(optarg, "use")) {
        con->ignorepresets = FALSE;
      } else if (!strcmp(optarg, "ignore")) {
        con->ignorepresets = TRUE;
      } else if (!strcmp(optarg, "show")) {
        con->showpresets = TRUE;
      } else if (!strcmp(optarg, "showall")) {
        showallpresets();
        exit(0);
      } else {
        badarg = TRUE;
      }
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
      } else if (!strcmp(optarg,"conveyance")) {
        con->smartconveyanceselftest = TRUE;
        con->testcase            = CONVEYANCE_SELF_TEST;
#if DEVELOP_SELECTIVE_SELF_TEST
      } else if (!strncmp(optarg,"selective",strlen("selective"))) {
        unsigned long long start, stop;

        if (split_selective_arg(optarg, &start, &stop)) {
          badarg = TRUE;
        } else {
          if (con->smartselectivenumspans >= 5 || start > stop) {
            con->veryquietmode=FALSE;
            printslogan();
            if (start > stop) {
              pout("ERROR: Start LBA > ending LBA in argument \"%s\"\n",
                optarg);
            } else {
              pout("ERROR: No more than five selective self-test spans may be"
                " defined\n");
            }
            pout("\nUse smartctl -h to get a usage summary\n\n");
            exit(FAILCMD);
          }
          con->smartselectivespan[con->smartselectivenumspans][0] = start;
          con->smartselectivespan[con->smartselectivenumspans][1] = stop;
          con->smartselectivenumspans++;
          con->testcase            = SELECTIVE_SELF_TEST;
        }
#endif
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
  else if (captive && con->smartconveyanceselftest) {
    con->smartconveyanceselftest    = FALSE;
    con->smartconveyancecapselftest = TRUE;
    con->testcase                   = CONVEYANCE_CAPTIVE_SELF_TEST;
  }
#if DEVELOP_SELECTIVE_SELF_TEST
  else if (captive && con->smartselectiveselftest) {
    con->smartselectiveselftest    = FALSE;
    con->smartselectivecapselftest = TRUE;
    con->testcase                  = SELECTIVE_CAPTIVE_SELF_TEST;
  }
#endif 
 
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
  int dev_type, flags;

  // define control block for external functions
  con=&control;

  // Part input arguments
  ParseOpts(argc,argv);

  device = argv[argc-1];

  if (!tryata && !tryscsi) {
    // user has not specified device type, so guess
    dev_type = guess_linux_device_type(device);
    if (GUESS_DEVTYPE_SCSI == dev_type) {
      tryscsi = 1;
    } else if (GUESS_DEVTYPE_ATA == dev_type)
      tryata = 1;
    else {
      pout("Smartctl: please specify if this is an ATA or SCSI device with the -d option.\n");
      Usage();
      return FAILCMD;
    }    
  }
  
  // set up flags for open() call.  SCSI case is:
  flags = O_RDWR | O_NONBLOCK;

  if (tryata)
    flags = O_RDONLY | O_NONBLOCK;
    
  // open device - SCSI devices are opened (O_RDWR | O_NONBLOCK) so the
  // scsi generci device can be used (needs write permission for MODE 
  // SELECT command) plus O_NONBLOCK to stop open hanging if media not
  // present (e.g. with st). 
  fd = open(device, flags);
  if (fd<0) {
    char errmsg[256];
    snprintf(errmsg,256,"Smartctl open device: %s failed",argv[argc-1]);
    errmsg[255]='\0';
    syserror(errmsg);
    return FAILDEV;
  }

  // now call appropriate ATA or SCSI routine
  if (tryata)
    retval = ataPrintMain(fd);
  else if (tryscsi)
    retval = scsiPrintMain(device, fd);
  else {
    pout("Smartctl: specify if this is an ATA or SCSI device with the -d option.\n");
    Usage();
    return FAILCMD;
  }
  
  return retval;
}
