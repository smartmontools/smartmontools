/*
 * smartctl.c
 *
 * Home page of code is: http://smartmontools.sourceforge.net
 *
 * Copyright (C) 2002-4 Bruce Allen <smartmontools-support@lists.sourceforge.net>
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
#include <errno.h>
#include <stdio.h>
#include <sys/types.h>
#include <string.h>
#include <stdarg.h>
#ifdef HAVE_GETOPT_LONG
#include <getopt.h>
#endif
#include "atacmds.h"
#include "ataprint.h"
#include "extern.h"
#include "int64.h"
#include "knowndrives.h"
#include "scsicmds.h"
#include "scsiprint.h"
#include "smartctl.h"
#include "utility.h"

#ifdef NEED_SOLARIS_ATA_CODE
extern const char *os_solaris_ata_s_cvsid;
#endif
#if defined(_WIN32) && defined(_MSC_VER)
extern const char *int64_vc6_c_cvsid;
#endif
extern const char *atacmdnames_c_cvsid, *atacmds_c_cvsid, *ataprint_c_cvsid, *knowndrives_c_cvsid, *os_XXXX_c_cvsid, *scsicmds_c_cvsid, *scsiprint_c_cvsid, *utility_c_cvsid;
const char* smartctl_c_cvsid="$Id: smartctl.c,v 1.126 2004/07/09 12:38:04 ballen4705 Exp $"
ATACMDS_H_CVSID ATAPRINT_H_CVSID CONFIG_H_CVSID EXTERN_H_CVSID INT64_H_CVSID KNOWNDRIVES_H_CVSID SCSICMDS_H_CVSID SCSIPRINT_H_CVSID SMARTCTL_H_CVSID UTILITY_H_CVSID;

// This is a block containing all the "control variables".  We declare
// this globally in this file, and externally in other files.
smartmonctrl *con=NULL;

// to hold onto exit code for atexit routine
extern int exitstatus;

// Track memory use
extern int64_t bytes;

void printslogan(){
  pout("smartctl version %s Copyright (C) 2002-4 Bruce Allen\n", PACKAGE_VERSION);
  pout("Home page is " PACKAGE_HOMEPAGE "\n\n");
  return;
}

void printcopy(){
  char out[CVSMAXLEN];
  char *configargs=strlen(SMARTMONTOOLS_CONFIGURE_ARGS)?SMARTMONTOOLS_CONFIGURE_ARGS:"[no arguments given]";

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
 #if defined(_WIN32) && defined(_MSC_VER)
  printone(out,int64_vc6_c_cvsid);
  pout("%s",out);
#endif
 printone(out,knowndrives_c_cvsid);
  pout("%s",out);
  printone(out,os_XXXX_c_cvsid);
  pout("%s",out);
#ifdef NEED_SOLARIS_ATA_CODE
  printone(out, os_solaris_ata_s_cvsid);
  pout("%s",out);
#endif
  printone(out,scsicmds_c_cvsid);
  pout("%s",out);
  printone(out,scsiprint_c_cvsid);
  pout("%s",out);
  printone(out,smartctl_c_cvsid);
  pout("%s",out);
  printone(out,utility_c_cvsid);
  pout("%s",out);
  pout("\nsmartmontools release " PACKAGE_VERSION " dated " SMARTMONTOOLS_RELEASE_DATE " at " SMARTMONTOOLS_RELEASE_TIME "\n");
  pout("smartmontools build host: " SMARTMONTOOLS_BUILD_HOST "\n");
  pout("smartmontools build configured: " SMARTMONTOOLS_CONFIGURE_DATE "\n");
  pout("smartctl compile dated " __DATE__ " at "__TIME__ "\n");
  pout("smartmontools configure arguments: %s\n", configargs);
  return;
}

void UsageSummary(){
  pout("\nUse smartctl -h to get a usage summary\n\n");
  return;
}

/*  void prints help information for command syntax */
void Usage (void){
  printf("Usage: smartctl [options] device\n\n");
  printf("============================================ SHOW INFORMATION OPTIONS =====\n\n");
#ifdef HAVE_GETOPT_LONG
  printf(
"  -h, --help, --usage\n"
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
"  -h        Display this help and exit\n"
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
"         Tolerance: normal, conservative, permissive, verypermissive\n\n"
"  -b TYPE, --badsum=TYPE                                              (ATA)\n"
"         Set action on bad checksum to one of: warn, exit, ignore\n\n"
"  -r TYPE, --report=TYPE\n"
"         Report transactions (see man page)\n\n"
  );
#else
  printf(
"  -q TYPE   Set smartctl quiet mode to one of: errorsonly, silent     (ATA)\n"
"  -d TYPE   Specify device type to one of: ata, scsi, 3ware,N\n"
"  -T TYPE   Tolerance: normal, conservative,permissive,verypermissive (ATA\n"
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
"  -A, --attributes                                                         \n"
"        Show device SMART vendor-specific Attributes and values\n\n"
"  -l TYPE, --log=TYPE\n"
"        Show device log. TYPE: error, selftest, selective, directory\n\n"
"  -v N,OPTION , --vendorattribute=N,OPTION                            (ATA)\n"
"        Set display OPTION for vendor Attribute N (see man page)\n\n"
"  -F TYPE, --firmwarebug=TYPE                                         (ATA)\n"
"        Use firmware bug workaround: none, samsung, samsung2\n\n"
"  -P TYPE, --presets=TYPE                                             (ATA)\n"
"        Drive-specific presets: use, ignore, show, showall\n\n"
  );
#else
  printf(
"  -H        Show device SMART health status\n"
"  -c        Show device SMART capabilities                             (ATA)\n"
"  -A        Show device SMART vendor-specific Attributes and values    (ATA)\n"
"  -l TYPE   Show device log. TYPE: error,selftest,selective,directory\n"
"  -v N,OPT  Set display OPTion for vendor Attribute N (see man page)   (ATA)\n"
"  -F TYPE   Use firmware bug workaround: none, samsung, samsung2       (ATA)\n"
"  -P TYPE   Drive-specific presets: use, ignore, show, showall         (ATA)\n\n"
  );
#endif
  printf("============================================ DEVICE SELF-TEST OPTIONS =====\n\n");
#ifdef HAVE_GETOPT_LONG
  printf(
"  -t TEST, --test=TEST\n"
"        Run test.  TEST is: offline short long conveyance select,M-N pending,N afterselect,on afterselect,off\n\n"
"  -C, --captive\n"
"        Do test in captive mode (along with -t)\n\n"
"  -X, --abort\n"
"        Abort any non-captive test on device\n\n"
);
#else
  printf(
"  -t TEST   Run test.  TEST is: offline short long conveyance select,M-N pending,N afterselect,on afterselect,off\n"
"  -C        Do test in captive mode (along with -t)\n"
"  -X        Abort any non-captive test\n\n"
  );
#endif
  print_smartctl_examples();
  return;
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
    return "normal, conservative, permissive, verypermissive";
  case 'b':
    return "warn, exit, ignore";
  case 'r':
    return "ioctl[,N], ataioctl[,N], scsiioctl[,N]";
  case 's':
  case 'o':
  case 'S':
    return "on, off";
  case 'l':
    return "error, selftest, selective, directory";
  case 'P':
    return "use, ignore, show, showall";
  case 't':
    return "offline, short, long, conveyance, select,M-N, pending,N, afterselect,on, afterselect,off";
  case 'F':
    return "none, samsung, samsung2";
  case 'v':
  default:
    return NULL;
  }
}

/* Prints the message "=======> VALID ARGUMENTS ARE: <LIST> \n", where
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
  char extraerror[256];
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
  
  memset(extraerror, 0, sizeof(extraerror));
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
      con->dont_print=FALSE;
      printslogan();
      printcopy();
      exit(0);
      break;
    case 'q':
      if (!strcmp(optarg,"errorsonly")) {
        con->printing_switchable     = TRUE;
        con->dont_print = FALSE;
      } else if (!strcmp(optarg,"silent")) {
        con->printing_switchable     = FALSE;
        con->dont_print = TRUE;
      } else {
        badarg = TRUE;
      }
      break;
    case 'd':
      if (!strcmp(optarg,"ata")) {
        tryata  = TRUE;
        tryscsi = FALSE;
        con->escalade_port = 0;
	con->escalade_type = THREE_WARE_NONE;
      } else if (!strcmp(optarg,"scsi")) {
        tryata  = FALSE;
        tryscsi = TRUE;
        con->escalade_port = 0;
	con->escalade_type = THREE_WARE_NONE;
      } else {
        // look for RAID-type device
        int i;
        char *s;
        
        // make a copy of the string to mess with
        if (!(s = strdup(optarg))) {
          con->dont_print = FALSE;
          pout("No memory for argument of -d. Exiting...\n");
          exit(FAILCMD);
        } else if (strncmp(s,"3ware,",6)) {
          badarg = TRUE;
        } else if (split_report_arg2(s, &i)) {
          sprintf(extraerror, "Option -d 3ware,N requires N to be a non-negative integer\n");
          badarg = TRUE;
        } else if (i<0 || i>15) {
          sprintf(extraerror, "Option -d 3ware,N (N=%d) must have 0 <= N <= 15\n", i);
          badarg = TRUE;
        } else {
	  // NOTE: escalade_port == disk number + 1
          con->escalade_port = i+1;
          tryata  = TRUE;
          tryscsi = FALSE;
        }
        free(s);
      }         
      break;
    case 'T':
      if (!strcmp(optarg,"normal")) {
        con->conservative = FALSE;
        con->permissive   = 0;
      } else if (!strcmp(optarg,"conservative")) {
        con->conservative = TRUE;
      } else if (!strcmp(optarg,"permissive")) {
        if (con->permissive<0xff)
          con->permissive++;
      } else if (!strcmp(optarg,"verypermissive")) {
        con->permissive=0xff;
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
          con->dont_print = FALSE;
          pout("Can't allocate memory to copy argument to -r option"
               " - exiting\n");
          EXIT(FAILCMD);
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
      } else if (!strcmp(optarg,"samsung2")) {
        con->fixfirmwarebug = FIX_SAMSUNG2;
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
      } else if (!strcmp(optarg, "selective")) {
	con->selectivetestlog = TRUE;
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
      con->selectivetestlog   = TRUE;
      break;
    case 'v':
      // parse vendor-specific definitions of attributes
      if (!strcmp(optarg,"help")) {
        char *s;
        con->dont_print=FALSE;
        printslogan();
        if (!(s = create_vendor_attribute_arg_list())) {
          pout("Insufficient memory to construct argument list\n");
          EXIT(FAILCMD);
        }
        pout("The valid arguments to -v are:\n\thelp\n%s\n", s);
        free(s);
        EXIT(0);
      }
      charp=con->attributedefs;
      if (!charp){
        pout("Fatal internal error in ParseOpts()\n");
        EXIT(FAILCMD);
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
        EXIT(0);
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
      } else if (!strcmp(optarg,"afterselect,on")) {
	// scan remainder of disk after doing selected segments
	con->scanafterselect=2;
      } else if (!strcmp(optarg,"afterselect,off")) {
	// don't scan remainder of disk after doing selected segments
	con->scanafterselect=1;
      } else if (!strncmp(optarg,"pending,",strlen("pending,"))) {
	// parse number of minutes that test should be pending
	int i;
	char *tailptr=NULL;
	errno=0;
	i=(int)strtol(optarg+strlen("pending,"), &tailptr, 10);
	if (errno || *tailptr != '\0') {
	  sprintf(extraerror, "Option -t pending,N requires N to be a non-negative integer\n");
	  badarg = TRUE;
	} else if (i<0 || i>65535) {
	  sprintf(extraerror, "Option -t pending,N (N=%d) must have 0 <= N <= 65535\n", i);
	  badarg = TRUE;
	} else {
	  con->pendingtime=i+1;
	}
      } else if (!strncmp(optarg,"select",strlen("select"))) {
	// parse range of LBAs to test
	uint64_t start, stop;

        if (split_selective_arg(optarg, &start, &stop)) {
	  sprintf(extraerror, "Option -t select,M-N must have non-negative integer M and N\n");
          badarg = TRUE;
        } else {
          if (con->smartselectivenumspans >= 5 || start > stop) {
            if (start > stop) {
              sprintf(extraerror, "ERROR: Start LBA (%"PRIu64") > ending LBA (%"PRId64") in argument \"%s\"\n",
                start, stop, optarg);
            } else {
              sprintf(extraerror,"ERROR: No more than five selective self-test spans may be"
                " defined\n");
            }
	    badarg = TRUE;
          }
          con->smartselectivespan[con->smartselectivenumspans][0] = start;
          con->smartselectivespan[con->smartselectivenumspans][1] = stop;
          con->smartselectivenumspans++;
          con->testcase            = SELECTIVE_SELF_TEST;
        }
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
      con->dont_print=FALSE;
      printslogan();
      Usage();
      EXIT(0);  
      break;
    case '?':
    default:
      con->dont_print=FALSE;
      printslogan();
#ifdef HAVE_GETOPT_LONG
      // Point arg to the argument in which this option was found.
      arg = argv[optind-1];
      // Check whether the option is a long option that doesn't map to -h.
      if (arg[1] == '-' && optchar != 'h') {
        // Iff optopt holds a valid option then argument must be missing.
        if (optopt && (strchr(shortopts, optopt) != NULL)) {
          pout("=======> ARGUMENT REQUIRED FOR OPTION: %s\n", arg+2);
          printvalidarglistmessage(optopt);
        } else
          pout("=======> UNRECOGNIZED OPTION: %s\n",arg+2);
	if (extraerror[0])
	  pout("=======> %s", extraerror);
        UsageSummary();
        EXIT(FAILCMD);
      }
#endif
      if (optopt) {
        // Iff optopt holds a valid option then argument must be
        // missing.  Note (BA) this logic seems to fail using Solaris
        // getopt!
        if (strchr(shortopts, optopt) != NULL) {
          pout("=======> ARGUMENT REQUIRED FOR OPTION: %c\n", optopt);
          printvalidarglistmessage(optopt);
        } else
          pout("=======> UNRECOGNIZED OPTION: %c\n",optopt);
	if (extraerror[0])
	  pout("=======> %s", extraerror);
        UsageSummary();
        EXIT(FAILCMD);
      }
      Usage();
      EXIT(0);  
    } // closes switch statement to process command-line options
    
    // Check to see if option had an unrecognized or incorrect argument.
    if (badarg) {
      printslogan();
      // It would be nice to print the actual option name given by the user
      // here, but we just print the short form.  Please fix this if you know
      // a clean way to do it.
      pout("=======> INVALID ARGUMENT TO -%c: %s\n", optchar, optarg);
      printvalidarglistmessage(optchar);
      if (extraerror[0])
	pout("=======> %s", extraerror);
      UsageSummary();
      EXIT(FAILCMD);
    }
  }
  // At this point we have processed all command-line options.  If the
  // print output is switchable, then start with the print output
  // turned off
  if (con->printing_switchable)
    con->dont_print=TRUE;

  // error message if user has asked for more than one test
  if (1<(con->smartexeoffimmediate+con->smartshortselftest+con->smartextendselftest+
         con->smartshortcapselftest+con->smartextendcapselftest+con->smartselftestabort + (con->smartselectivenumspans>0?1:0))){
    con->dont_print=FALSE;
    printslogan();
    pout("\nERROR: smartctl can only run a single test type (or abort) at a time.\n");
    UsageSummary();
    EXIT(FAILCMD);
  }

  // error message if user has set selective self-test options without
  // asking for a selective self-test
  if ((con->pendingtime || con->scanafterselect) && !con->smartselectivenumspans){
    con->dont_print=FALSE;
    printslogan();
    if (con->pendingtime)
      pout("\nERROR: smartctl -t pending,N must be used with -t select,N-M.\n");
    else
      pout("\nERROR: smartctl -t afterselect,(on|off) must be used with -t select,N-M.\n");
    UsageSummary();
    EXIT(FAILCMD);
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
  else if (captive && con->smartselectiveselftest) {
    con->smartselectiveselftest    = FALSE;
    con->smartselectivecapselftest = TRUE;
    con->testcase                  = SELECTIVE_CAPTIVE_SELF_TEST;
  }
 
  // From here on, normal operations...
  printslogan();
  
  // Warn if the user has provided no device name
  if (argc-optind<1){
    pout("ERROR: smartctl requires a device name as the final command-line argument.\n\n");
    UsageSummary();
    EXIT(FAILCMD);
  }
  
  // Warn if the user has provided more than one device name
  if (argc-optind>1){
    int i;
    pout("ERROR: smartctl takes ONE device name as the final command-line argument.\n");
    pout("You have provided %d device names:\n",argc-optind);
    for (i=0; i<argc-optind; i++)
      pout("%s\n",argv[optind+i]);
    UsageSummary();
    EXIT(FAILCMD);
  }  
}

// Printing function (controlled by global con->dont_print) 
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
  if (con->dont_print){
    va_end(ap);
    return;
  }

  // print out
  vprintf(fmt,ap);
  va_end(ap);
  fflush(stdout);
  return;
}

// This function is used by utility.c to report LOG_CRIT errors.
// The smartctl version prints to stdout instead of syslog().
void PrintOut(int priority, char *fmt, ...) {
  va_list ap;

  // avoid warning message about unused variable from gcc -W: just
  // change value of local copy.
  priority=0;

  va_start(ap,fmt);
  vprintf(fmt,ap);
  va_end(ap);
  return;
}


/* Main Program */
int main (int argc, char **argv){
  int fd,retval=0;
  char *device;
  smartmonctrl control;
  int dev_type;
  char *mode=NULL;

  // define control block for external functions
  con=&control;

  // Part input arguments
  ParseOpts(argc,argv);

  device = argv[argc-1];
  
  if (!tryata && !tryscsi) {
    // user has not specified device type, so guess
    dev_type = guess_device_type(device);
    if (GUESS_DEVTYPE_SCSI == dev_type)
      tryscsi = 1;
    else if (GUESS_DEVTYPE_ATA == dev_type)
      tryata = 1;
    else {
      pout("Smartctl: please specify if this is an ATA or SCSI device with the -d option.\n");
      UsageSummary();
      return FAILCMD;
    }    
  }
  
  if (con->escalade_port) {
    dev_type = guess_device_type(device);
    tryata = 1;
    tryscsi = 0;

    if (GUESS_DEVTYPE_3WARE_9000_CHAR == dev_type)
      con->escalade_type=THREE_WARE_9000_CHAR;
    else if (GUESS_DEVTYPE_3WARE_678K_CHAR == dev_type)
      con->escalade_type=THREE_WARE_678K_CHAR;
    else
      con->escalade_type=THREE_WARE_678K;
  }

  // set up mode for open() call.  SCSI case is:
  mode="SCSI";

  if (tryata)
    mode="ATA";
    
  // open device - SCSI devices are opened (O_RDWR | O_NONBLOCK) so the
  // scsi generic device can be used (needs write permission for MODE 
  // SELECT command) plus O_NONBLOCK to stop open hanging if media not
  // present (e.g. with st).  Opening is retried O_RDONLY if read-only
  // media prevents opening O_RDWR (it cannot happen for scsi generic
  // devices, but it can for the others).
  fd = deviceopen(device, mode);
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
    retval = scsiPrintMain(fd);
  else {
    pout("Smartctl: specify if this is an ATA or SCSI device with the -d option.\n");
    UsageSummary();
    return FAILCMD;
  }
  
  return retval;
}
