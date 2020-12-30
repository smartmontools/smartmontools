/*
 * smartctl.cpp
 *
 * Home page of code is: https://www.smartmontools.org
 *
 * Copyright (C) 2002-11 Bruce Allen
 * Copyright (C) 2008-20 Christian Franke
 * Copyright (C) 2000 Michael Cornwell <cornwell@acm.org>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "config.h"
#define __STDC_FORMAT_MACROS 1 // enable PRI* for C++

#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <sys/types.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdexcept>
#include <getopt.h>

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#if defined(__FreeBSD__)
#include <sys/param.h>
#endif

#include "atacmds.h"
#include "dev_interface.h"
#include "ataprint.h"
#include "knowndrives.h"
#include "scsicmds.h"
#include "scsiprint.h"
#include "nvmeprint.h"
#include "smartctl.h"
#include "utility.h"
#include "svnversion.h"

const char * smartctl_cpp_cvsid = "$Id$"
  CONFIG_H_CVSID SMARTCTL_H_CVSID;

// Globals to control printing
bool printing_is_switchable = false;
bool printing_is_off = false;

// Control JSON output
json jglb;
static bool print_as_json = false;
static json::print_options print_as_json_options;
static bool print_as_json_output = false;
static bool print_as_json_impl = false;
static bool print_as_json_unimpl = false;

static void printslogan()
{
  jout("%s\n", format_version_info("smartctl").c_str());
}

static void UsageSummary()
{
  pout("\nUse smartctl -h to get a usage summary\n\n");
  return;
}

static void js_initialize(int argc, char **argv, bool verbose)
{
  if (jglb.is_enabled())
    return;
  jglb.enable();
  if (verbose)
    jglb.set_verbose();

  // Major.minor version of JSON format
  jglb["json_format_version"][0] = 1;
  jglb["json_format_version"][1] = 0;

  // Smartctl version info
  json::ref jref = jglb["smartctl"];
  int ver[3] = { 0, 0, 0 };
  sscanf(PACKAGE_VERSION, "%d.%d.%d", ver, ver+1, ver+2);
  jref["version"][0] = ver[0];
  jref["version"][1] = ver[1];
  if (ver[2] > 0)
    jref["version"][2] = ver[2];

#ifdef SMARTMONTOOLS_SVN_REV
  jref["svn_revision"] = SMARTMONTOOLS_SVN_REV;
#endif
  jref["platform_info"] = smi()->get_os_version_str();
#ifdef BUILD_INFO
  jref["build_info"] = BUILD_INFO;
#endif

  jref["argv"][0] = "smartctl";
  for (int i = 1; i < argc; i++)
    jref["argv"][i] = argv[i];
}

static std::string getvalidarglist(int opt);

/*  void prints help information for command syntax */
static void Usage()
{
  pout("Usage: smartctl [options] device\n\n");
  pout(
"============================================ SHOW INFORMATION OPTIONS =====\n\n"
"  -h, --help, --usage\n"
"         Display this help and exit\n\n"
"  -V, --version, --copyright, --license\n"
"         Print license, copyright, and version information and exit\n\n"
"  -i, --info\n"
"         Show identity information for device\n\n"
"  --identify[=[w][nvb]]\n"
"         Show words and bits from IDENTIFY DEVICE data                (ATA)\n\n"
"  -g NAME, --get=NAME\n"
"        Get device setting: all, aam, apm, dsn, lookahead, security,\n"
"        wcache, rcache, wcreorder, wcache-sct\n\n"
"  -a, --all\n"
"         Show all SMART information for device\n\n"
"  -x, --xall\n"
"         Show all information for device\n\n"
"  --scan\n"
"         Scan for devices\n\n"
"  --scan-open\n"
"         Scan for devices and try to open each device\n\n"
  );
  pout(
"================================== SMARTCTL RUN-TIME BEHAVIOR OPTIONS =====\n\n"
"  -j, --json[=cgiosuvy]\n"
"         Print output in JSON or YAML format\n\n"
"  -q TYPE, --quietmode=TYPE                                           (ATA)\n"
"         Set smartctl quiet mode to one of: errorsonly, silent, noserial\n\n"
"  -d TYPE, --device=TYPE\n"
"         Specify device type to one of:\n"
"         %s\n\n" // TODO: fold this string
"  -T TYPE, --tolerance=TYPE                                           (ATA)\n"
"         Tolerance: normal, conservative, permissive, verypermissive\n\n"
"  -b TYPE, --badsum=TYPE                                              (ATA)\n"
"         Set action on bad checksum to one of: warn, exit, ignore\n\n"
"  -r TYPE, --report=TYPE\n"
"         Report transactions (see man page)\n\n"
"  -n MODE[,STATUS], --nocheck=MODE[,STATUS]                     (ATA, SCSI)\n"
"         No check if: never, sleep, standby, idle (see man page)\n\n",
  getvalidarglist('d').c_str()); // TODO: Use this function also for other options ?
  pout(
"============================== DEVICE FEATURE ENABLE/DISABLE COMMANDS =====\n\n"
"  -s VALUE, --smart=VALUE\n"
"        Enable/disable SMART on device (on/off)\n\n"
"  -o VALUE, --offlineauto=VALUE                                       (ATA)\n"
"        Enable/disable automatic offline testing on device (on/off)\n\n"
"  -S VALUE, --saveauto=VALUE                                          (ATA)\n"
"        Enable/disable Attribute autosave on device (on/off)\n\n"
"  -s NAME[,VALUE], --set=NAME[,VALUE]\n"
"        Enable/disable/change device setting: aam,[N|off], apm,[N|off],\n"
"        dsn,[on|off], lookahead,[on|off], security-freeze,\n"
"        standby,[N|off|now], wcache,[on|off], rcache,[on|off],\n"
"        wcreorder,[on|off[,p]], wcache-sct,[ata|on|off[,p]]\n\n"
  );
  pout(
"======================================= READ AND DISPLAY DATA OPTIONS =====\n\n"
"  -H, --health\n"
"        Show device SMART health status\n\n"
"  -c, --capabilities                                            (ATA, NVMe)\n"
"        Show device SMART capabilities\n\n"
"  -A, --attributes\n"
"        Show device SMART vendor-specific Attributes and values\n\n"
"  -f FORMAT, --format=FORMAT                                          (ATA)\n"
"        Set output format for attributes: old, brief, hex[,id|val]\n\n"
"  -l TYPE, --log=TYPE\n"
"        Show device log. TYPE: error, selftest, selective, directory[,g|s],\n"
"        xerror[,N][,error], xselftest[,N][,selftest], background,\n"
"        sasphy[,reset], sataphy[,reset], scttemp[sts,hist],\n"
"        scttempint,N[,p], scterc[,N,M], devstat[,N], defects[,N], ssd,\n"
"        gplog,N[,RANGE], smartlog,N[,RANGE], nvmelog,N,SIZE\n\n"
"  -v N,OPTION , --vendorattribute=N,OPTION                            (ATA)\n"
"        Set display OPTION for vendor Attribute N (see man page)\n\n"
"  -F TYPE, --firmwarebug=TYPE                                         (ATA)\n"
"        Use firmware bug workaround:\n"
"        %s, swapid\n\n"
"  -P TYPE, --presets=TYPE                                             (ATA)\n"
"        Drive-specific presets: use, ignore, show, showall\n\n"
"  -B [+]FILE, --drivedb=[+]FILE                                       (ATA)\n"
"        Read and replace [add] drive database from FILE\n"
"        [default is +%s",
    get_valid_firmwarebug_args(),
    get_drivedb_path_add()
  );
#ifdef SMARTMONTOOLS_DRIVEDBDIR
  pout(
                      "\n"
"         and then    %s",
    get_drivedb_path_default()
  );
#endif
  pout(
         "]\n\n"
"============================================ DEVICE SELF-TEST OPTIONS =====\n\n"
"  -t TEST, --test=TEST\n"
"        Run test. TEST: offline, short, long, conveyance, force, vendor,N,\n"
"                        select,M-N, pending,N, afterselect,[on|off]\n\n"
"  -C, --captive\n"
"        Do test in captive mode (along with -t)\n\n"
"  -X, --abort\n"
"        Abort any non-captive test on device\n\n"
);
  std::string examples = smi()->get_app_examples("smartctl");
  if (!examples.empty())
    pout("%s\n", examples.c_str());
}

// Values for  --long only options, see parse_options()
enum { opt_identify = 1000, opt_scan, opt_scan_open, opt_set, opt_smart };

/* Returns a string containing a formatted list of the valid arguments
   to the option opt or empty on failure. Note 'v' case different */
static std::string getvalidarglist(int opt)
{
  switch (opt) {
  case 'q':
    return "errorsonly, silent, noserial";
  case 'd':
    return smi()->get_valid_dev_types_str() + ", auto, test";
  case 'T':
    return "normal, conservative, permissive, verypermissive";
  case 'b':
    return "warn, exit, ignore";
  case 'B':
    return "[+]<FILE_NAME>";
  case 'r':
    return "ioctl[,N], ataioctl[,N], scsiioctl[,N], nvmeioctl[,N]";
  case opt_smart:
  case 'o':
  case 'S':
    return "on, off";
  case 'l':
    return "error, selftest, selective, directory[,g|s], "
           "xerror[,N][,error], xselftest[,N][,selftest], "
           "background, sasphy[,reset], sataphy[,reset], "
           "scttemp[sts,hist], scttempint,N[,p], "
           "scterc[,N,M], devstat[,N], defects[,N], ssd, "
           "gplog,N[,RANGE], smartlog,N[,RANGE], "
           "nvmelog,N,SIZE";
  case 'P':
    return "use, ignore, show, showall";
  case 't':
    return "offline, short, long, conveyance, force, vendor,N, select,M-N, "
           "pending,N, afterselect,[on|off]";
  case 'F':
    return std::string(get_valid_firmwarebug_args()) + ", swapid";
  case 'n':
    return "never, sleep[,STATUS], standby[,STATUS], idle[,STATUS]";
  case 'f':
    return "old, brief, hex[,id|val]";
  case 'g':
    return "aam, apm, dsn, lookahead, security, wcache, rcache, wcreorder, wcache-sct";
  case opt_set:
    return "aam,[N|off], apm,[N|off], dsn,[on|off], lookahead,[on|off], security-freeze, "
           "standby,[N|off|now], wcache,[on|off], rcache,[on|off], wcreorder,[on|off[,p]], "
           "wcache-sct,[ata|on|off[,p]]";
  case 's':
    return getvalidarglist(opt_smart)+", "+getvalidarglist(opt_set);
  case 'j':
    return "c, g, i, o, s, u, v, y";
  case opt_identify:
    return "n, wn, w, v, wv, wb";
  case 'v':
  default:
    return "";
  }
}

/* Prints the message "=======> VALID ARGUMENTS ARE: <LIST> \n", where
   <LIST> is the list of valid arguments for option opt. */
static void printvalidarglistmessage(int opt)
{
  if (opt=='v'){
    jerr("=======> VALID ARGUMENTS ARE:\n\thelp\n%s\n<=======\n",
         create_vendor_attribute_arg_list().c_str());
  }
  else {
  // getvalidarglist() might produce a multiline or single line string.  We
  // need to figure out which to get the formatting right.
    std::string s = getvalidarglist(opt);
    char separator = strchr(s.c_str(), '\n') ? '\n' : ' ';
    jerr("=======> VALID ARGUMENTS ARE:%c%s%c<=======\n", separator, s.c_str(), separator);
  }

  return;
}

// Checksum error mode
enum checksum_err_mode_t {
  CHECKSUM_ERR_WARN, CHECKSUM_ERR_EXIT, CHECKSUM_ERR_IGNORE
};

static checksum_err_mode_t checksum_err_mode = CHECKSUM_ERR_WARN;

static void scan_devices(const smart_devtype_list & types, bool with_open, char ** argv);


/*      Takes command options and sets features to be run */    
static int parse_options(int argc, char** argv, const char * & type,
  ata_print_options & ataopts, scsi_print_options & scsiopts,
  nvme_print_options & nvmeopts, bool & print_type_only)
{
  // Please update getvalidarglist() if you edit shortopts
  const char *shortopts = "h?Vq:d:T:b:r:s:o:S:HcAl:iaxv:P:t:CXF:n:B:f:g:j";
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
    { "smart",           required_argument, 0, opt_smart },
    { "offlineauto",     required_argument, 0, 'o' },
    { "saveauto",        required_argument, 0, 'S' },
    { "health",          no_argument,       0, 'H' },
    { "capabilities",    no_argument,       0, 'c' },
    { "attributes",      no_argument,       0, 'A' },
    { "log",             required_argument, 0, 'l' },
    { "info",            no_argument,       0, 'i' },
    { "all",             no_argument,       0, 'a' },
    { "xall",            no_argument,       0, 'x' },
    { "vendorattribute", required_argument, 0, 'v' },
    { "presets",         required_argument, 0, 'P' },
    { "test",            required_argument, 0, 't' },
    { "captive",         no_argument,       0, 'C' },
    { "abort",           no_argument,       0, 'X' },
    { "firmwarebug",     required_argument, 0, 'F' },
    { "nocheck",         required_argument, 0, 'n' },
    { "drivedb",         required_argument, 0, 'B' },
    { "format",          required_argument, 0, 'f' },
    { "get",             required_argument, 0, 'g' },
    { "json",            optional_argument, 0, 'j' },
    { "identify",        optional_argument, 0, opt_identify },
    { "set",             required_argument, 0, opt_set },
    { "scan",            no_argument,       0, opt_scan      },
    { "scan-open",       no_argument,       0, opt_scan_open },
    { 0,                 0,                 0, 0   }
  };

  char extraerror[256];
  memset(extraerror, 0, sizeof(extraerror));
  opterr=optopt=0;

  smart_devtype_list scan_types; // multiple -d TYPE options for --scan
  bool use_default_db = true; // set false on '-B FILE'
  bool output_format_set = false; // set true on '-f FORMAT'
  int scan = 0; // set by --scan, --scan-open
  bool badarg = false, captive = false;
  int testcnt = 0; // number of self-tests requested

  int optchar;
  char *arg;

  while ((optchar = getopt_long(argc, argv, shortopts, longopts, 0)) != -1) {

    // Clang analyzer: Workaround for false positive messages
    // 'Dereference of null pointer' and 'Null pointer argument'
    bool optarg_is_set = !!optarg;
    #ifdef __clang_analyzer__
    if (!optarg_is_set) optarg = (char *)"";
    #endif

    switch (optchar){
    case 'V':
      printing_is_off = false;
      pout("%s", format_version_info("smartctl", true /*full*/).c_str());
      return 0;
    case 'q':
      if (!strcmp(optarg,"errorsonly")) {
        printing_is_switchable = true;
        printing_is_off = false;
      } else if (!strcmp(optarg,"silent")) {
        printing_is_switchable = false;
        printing_is_off = true;
      } else if (!strcmp(optarg,"noserial")) {
        dont_print_serial_number = true;
      } else {
        badarg = true;
      }
      break;
    case 'd':
      if (!strcmp(optarg, "test"))
        print_type_only = true;
      else if (!strcmp(optarg, "auto")) {
        type = 0;
        scan_types.clear();
      }
      else {
        type = optarg;
        scan_types.push_back(optarg);
      }
      break;
    case 'T':
      if (!strcmp(optarg,"normal")) {
        failuretest_conservative = false;
        failuretest_permissive   = 0;
      } else if (!strcmp(optarg,"conservative")) {
        failuretest_conservative = true;
      } else if (!strcmp(optarg,"permissive")) {
        if (failuretest_permissive < 0xff)
          failuretest_permissive++;
      } else if (!strcmp(optarg,"verypermissive")) {
        failuretest_permissive = 0xff;
      } else {
        badarg = true;
      }
      break;
    case 'b':
      if (!strcmp(optarg,"warn")) {
        checksum_err_mode = CHECKSUM_ERR_WARN;
      } else if (!strcmp(optarg,"exit")) {
        checksum_err_mode = CHECKSUM_ERR_EXIT;
      } else if (!strcmp(optarg,"ignore")) {
        checksum_err_mode = CHECKSUM_ERR_IGNORE;
      } else {
        badarg = true;
      }
      break;
    case 'r':
      {
        int n1 = -1, n2 = -1, len = strlen(optarg);
        char s[9+1]; unsigned i = 1;
        sscanf(optarg, "%9[a-z]%n,%u%n", s, &n1, &i, &n2);
        if (!((n1 == len || n2 == len) && 1 <= i && i <= 4)) {
          badarg = true;
        } else if (!strcmp(s,"ioctl")) {
          ata_debugmode = scsi_debugmode = nvme_debugmode = i;
        } else if (!strcmp(s,"ataioctl")) {
          ata_debugmode = i;
        } else if (!strcmp(s,"scsiioctl")) {
          scsi_debugmode = i;
        } else if (!strcmp(s,"nvmeioctl")) {
          nvme_debugmode = i;
        } else {
          badarg = true;
        }
      }
      break;

    case 's':
    case opt_smart: // --smart
      if (!strcmp(optarg,"on")) {
        ataopts.smart_enable  = scsiopts.smart_enable  = true;
        ataopts.smart_disable = scsiopts.smart_disable = false;
      } else if (!strcmp(optarg,"off")) {
        ataopts.smart_disable = scsiopts.smart_disable = true;
        ataopts.smart_enable  = scsiopts.smart_enable  = false;
      } else if (optchar == 's') {
        goto case_s_continued; // --set, see below
      } else {
        badarg = true;
      }
      break;

    case 'o':
      if (!strcmp(optarg,"on")) {
        ataopts.smart_auto_offl_enable  = true;
        ataopts.smart_auto_offl_disable = false;
      } else if (!strcmp(optarg,"off")) {
        ataopts.smart_auto_offl_disable = true;
        ataopts.smart_auto_offl_enable  = false;
      } else {
        badarg = true;
      }
      break;
    case 'S':
      if (!strcmp(optarg,"on")) {
        ataopts.smart_auto_save_enable  = scsiopts.smart_auto_save_enable  = true;
        ataopts.smart_auto_save_disable = scsiopts.smart_auto_save_disable = false;
      } else if (!strcmp(optarg,"off")) {
        ataopts.smart_auto_save_disable = scsiopts.smart_auto_save_disable = true;
        ataopts.smart_auto_save_enable  = scsiopts.smart_auto_save_enable  = false;
      } else {
        badarg = true;
      }
      break;
    case 'H':
      ataopts.smart_check_status = scsiopts.smart_check_status = nvmeopts.smart_check_status = true;
      scsiopts.smart_ss_media_log = true;
      break;
    case 'F':
      if (!strcmp(optarg, "swapid"))
        ataopts.fix_swapped_id = true;
      else if (!parse_firmwarebug_def(optarg, ataopts.firmwarebugs))
        badarg = true;
      break;
    case 'c':
      ataopts.smart_general_values = nvmeopts.drive_capabilities = true;
      break;
    case 'A':
      ataopts.smart_vendor_attrib = scsiopts.smart_vendor_attrib = nvmeopts.smart_vendor_attrib = true;
      break;
    case 'l':
      if (str_starts_with(optarg, "error")) {
        int n1 = -1, n2 = -1, len = strlen(optarg);
        unsigned val = ~0;
        sscanf(optarg, "error%n,%u%n", &n1, &val, &n2);
        ataopts.smart_error_log = scsiopts.smart_error_log = true;
        if (n1 == len)
          nvmeopts.error_log_entries = 16;
        else if (n2 == len && val > 0)
          nvmeopts.error_log_entries = val;
        else
          badarg = true;
      } else if (!strcmp(optarg,"selftest")) {
        ataopts.smart_selftest_log = scsiopts.smart_selftest_log = true;
      } else if (!strcmp(optarg, "selective")) {
        ataopts.smart_selective_selftest_log = true;
      } else if (!strcmp(optarg,"directory")) {
        ataopts.smart_logdir = ataopts.gp_logdir = true; // SMART+GPL
      } else if (!strcmp(optarg,"directory,s")) {
        ataopts.smart_logdir = true; // SMART
      } else if (!strcmp(optarg,"directory,g")) {
        ataopts.gp_logdir = true; // GPL
      } else if (!strcmp(optarg,"sasphy")) {
        scsiopts.sasphy = true;
      } else if (!strcmp(optarg,"sasphy,reset")) {
        scsiopts.sasphy = scsiopts.sasphy_reset = true;
      } else if (!strcmp(optarg,"sataphy")) {
        ataopts.sataphy = true;
      } else if (!strcmp(optarg,"sataphy,reset")) {
        ataopts.sataphy = ataopts.sataphy_reset = true;
      } else if (!strcmp(optarg,"background")) {
        scsiopts.smart_background_log = true;
      } else if (!strcmp(optarg,"ssd")) {
        ataopts.devstat_ssd_page = true;
        scsiopts.smart_ss_media_log = true;
      } else if (!strcmp(optarg,"scterc")) {
        ataopts.sct_erc_get = true;
      } else if (!strcmp(optarg,"scttemp")) {
        ataopts.sct_temp_sts = ataopts.sct_temp_hist = true;
      } else if (!strcmp(optarg,"scttempsts")) {
        ataopts.sct_temp_sts = true;
      } else if (!strcmp(optarg,"scttemphist")) {
        ataopts.sct_temp_hist = true;

      } else if (!strncmp(optarg, "scttempint,", sizeof("scstempint,")-1)) {
        unsigned interval = 0; int n1 = -1, n2 = -1, len = strlen(optarg);
        if (!(   sscanf(optarg,"scttempint,%u%n,p%n", &interval, &n1, &n2) == 1
              && 0 < interval && interval <= 0xffff && (n1 == len || n2 == len))) {
            snprintf(extraerror, sizeof(extraerror), "Option -l scttempint,N[,p] must have positive integer N\n");
            badarg = true;
        }
        ataopts.sct_temp_int = interval;
        ataopts.sct_temp_int_pers = (n2 == len);

      } else if (!strncmp(optarg, "devstat", sizeof("devstat")-1)) {
        int n1 = -1, n2 = -1, len = strlen(optarg);
        unsigned val = ~0;
        sscanf(optarg, "devstat%n,%u%n", &n1, &val, &n2);
        if (n1 == len)
          ataopts.devstat_all_pages = true;
        else {
            if (n2 != len) // retry with hex
              sscanf(optarg, "devstat,0x%x%n", &val, &n2);
            if (n2 == len && val <= 0xff)
              ataopts.devstat_pages.push_back(val);
            else
              badarg = true;
        }

      } else if (str_starts_with(optarg, "defects")) {
        int n1 = -1, n2 = -1, len = strlen(optarg);
        unsigned val = ~0;
        sscanf(optarg, "defects%n,%u%n", &n1, &val, &n2);
        if (n1 == len)
          ataopts.pending_defects_log = 31; // Entries of first page
        else if (n2 == len && val <= 0xffff * 32 - 1)
          ataopts.pending_defects_log = val;
        else
          badarg = true;

      } else if (!strncmp(optarg, "xerror", sizeof("xerror")-1)) {
        int n1 = -1, n2 = -1, len = strlen(optarg);
        unsigned val = 8;
        sscanf(optarg, "xerror%n,error%n", &n1, &n2);
        if (!(n1 == len || n2 == len)) {
          n1 = n2 = -1;
          sscanf(optarg, "xerror,%u%n,error%n", &val, &n1, &n2);
        }
        if ((n1 == len || n2 == len) && val > 0) {
          ataopts.smart_ext_error_log = val;
          ataopts.retry_error_log = (n2 == len);
        }
        else
          badarg = true;

      } else if (!strncmp(optarg, "xselftest", sizeof("xselftest")-1)) {
        int n1 = -1, n2 = -1, len = strlen(optarg);
        unsigned val = 25;
        sscanf(optarg, "xselftest%n,selftest%n", &n1, &n2);
        if (!(n1 == len || n2 == len)) {
          n1 = n2 = -1;
          sscanf(optarg, "xselftest,%u%n,selftest%n", &val, &n1, &n2);
        }
        if ((n1 == len || n2 == len) && val > 0) {
          ataopts.smart_ext_selftest_log = val;
          ataopts.retry_selftest_log = (n2 == len);
        }
        else
          badarg = true;

      } else if (!strncmp(optarg, "scterc,", sizeof("scterc,")-1)) {
        unsigned rt = ~0, wt = ~0; int n = -1;
        sscanf(optarg,"scterc,%u,%u%n", &rt, &wt, &n);
        if (n == (int)strlen(optarg) && rt <= 999 && wt <= 999) {
          ataopts.sct_erc_set = true;
          ataopts.sct_erc_readtime = rt;
          ataopts.sct_erc_writetime = wt;
        }
        else {
          snprintf(extraerror, sizeof(extraerror), "Option -l scterc,[READTIME,WRITETIME] syntax error\n");
          badarg = true;
        }
      } else if (   !strncmp(optarg, "gplog,"   , sizeof("gplog,"   )-1)
                 || !strncmp(optarg, "smartlog,", sizeof("smartlog,")-1)) {
        unsigned logaddr = ~0U; unsigned page = 0, nsectors = 1; char sign = 0;
        int n1 = -1, n2 = -1, n3 = -1, len = strlen(optarg);
        sscanf(optarg, "%*[a-z],0x%x%n,%u%n%c%u%n",
               &logaddr, &n1, &page, &n2, &sign, &nsectors, &n3);
        if (len > n2 && n3 == -1 && !strcmp(optarg+n2, "-max")) {
          nsectors = ~0U; sign = '+'; n3 = len;
        }
        bool gpl = (optarg[0] == 'g');
        const char * erropt = (gpl ? "gplog" : "smartlog");
        if (!(   n1 == len || n2 == len
              || (n3 == len && (sign == '+' || sign == '-')))) {
          snprintf(extraerror, sizeof(extraerror), "Option -l %s,ADDR[,FIRST[-LAST|+SIZE]] syntax error\n", erropt);
          badarg = true;
        }
        else if (!(    logaddr <= 0xff && page <= (gpl ? 0xffffU : 0x00ffU)
                   && 0 < nsectors
                   && (nsectors <= (gpl ? 0xffffU : 0xffU) || nsectors == ~0U)
                   && (sign != '-' || page <= nsectors)                       )) {
          snprintf(extraerror, sizeof(extraerror), "Option -l %s,ADDR[,FIRST[-LAST|+SIZE]] parameter out of range\n", erropt);
          badarg = true;
        }
        else {
          ata_log_request req;
          req.gpl = gpl; req.logaddr = logaddr; req.page = page;
          req.nsectors = (sign == '-' ? nsectors-page+1 : nsectors);
          ataopts.log_requests.push_back(req);
        }
      }

      else if (str_starts_with(optarg, "nvmelog,")) {
        int n = -1, len = strlen(optarg);
        unsigned page = 0, size = 0;
        sscanf(optarg, "nvmelog,0x%x,0x%x%n", &page, &size, &n);
        if (n == len && page <= 0xff && 0 < size && size <= 0x4000) {
          nvmeopts.log_page = page; nvmeopts.log_page_size = size;
        }
        else
          badarg = true;
      }

      else {
        badarg = true;
      }
      break;
    case 'i':
      ataopts.drive_info = scsiopts.drive_info = nvmeopts.drive_info = true;
      break;

    case opt_identify:
      ataopts.identify_word_level = ataopts.identify_bit_level = 0;
      if (optarg_is_set) {
        for (int i = 0; optarg[i]; i++) {
          switch (optarg[i]) {
            case 'w': ataopts.identify_word_level = 1; break;
            case 'n': ataopts.identify_bit_level = -1; break;
            case 'v': ataopts.identify_bit_level = 1; break;
            case 'b': ataopts.identify_bit_level = 2; break;
            default: badarg = true;
          }
        }
      }
      break;

    case 'a':
      ataopts.drive_info           = scsiopts.drive_info          = nvmeopts.drive_info          = true;
      ataopts.smart_check_status   = scsiopts.smart_check_status  = nvmeopts.smart_check_status  = true;
      ataopts.smart_general_values =                                nvmeopts.drive_capabilities  = true;
      ataopts.smart_vendor_attrib  = scsiopts.smart_vendor_attrib = nvmeopts.smart_vendor_attrib = true;
      ataopts.smart_error_log      = scsiopts.smart_error_log     = true;
      nvmeopts.error_log_entries   = 16;
      ataopts.smart_selftest_log   = scsiopts.smart_selftest_log  = true;
      ataopts.smart_selective_selftest_log = true;
      /* scsiopts.smart_background_log = true; */
      scsiopts.smart_ss_media_log = true;
      break;
    case 'x':
      ataopts.drive_info           = scsiopts.drive_info          = nvmeopts.drive_info          = true;
      ataopts.smart_check_status   = scsiopts.smart_check_status  = nvmeopts.smart_check_status  = true;
      ataopts.smart_general_values =                                nvmeopts.drive_capabilities  = true;
      ataopts.smart_vendor_attrib  = scsiopts.smart_vendor_attrib = nvmeopts.smart_vendor_attrib = true;
      ataopts.smart_ext_error_log  = 8;
      ataopts.retry_error_log      = true;
      nvmeopts.error_log_entries   = 16;
      ataopts.smart_ext_selftest_log = 25;
      ataopts.retry_selftest_log   = true;
      scsiopts.smart_error_log     = scsiopts.smart_selftest_log    = true;
      ataopts.smart_selective_selftest_log = true;
      ataopts.smart_logdir = ataopts.gp_logdir = true;
      ataopts.sct_temp_sts = ataopts.sct_temp_hist = true;
      ataopts.sct_erc_get = true;
      ataopts.sct_wcache_reorder_get = true;
      ataopts.devstat_all_pages = true;
      ataopts.pending_defects_log = 31;
      ataopts.sataphy = true;
      ataopts.get_set_used = true;
      ataopts.get_aam = ataopts.get_apm = true;
      ataopts.get_security = true;
      ataopts.get_lookahead = ataopts.get_wcache = true;
      ataopts.get_dsn = true;
      scsiopts.get_rcd = scsiopts.get_wce = true;
      scsiopts.smart_background_log = true;
      scsiopts.smart_ss_media_log = true;
      scsiopts.sasphy = true;
      if (!output_format_set)
        ataopts.output_format |= ata_print_options::FMT_BRIEF;
      break;
    case 'v':
      // parse vendor-specific definitions of attributes
      if (!strcmp(optarg,"help")) {
        printing_is_off = false;
        printslogan();
        pout("The valid arguments to -v are:\n\thelp\n%s\n",
             create_vendor_attribute_arg_list().c_str());
        return 0;
      }
      if (!parse_attribute_def(optarg, ataopts.attribute_defs, PRIOR_USER))
        badarg = true;
      break;    
    case 'P':
      if (!strcmp(optarg, "use")) {
        ataopts.ignore_presets = false;
      } else if (!strcmp(optarg, "ignore")) {
        ataopts.ignore_presets = true;
      } else if (!strcmp(optarg, "show")) {
        ataopts.show_presets = true;
      } else if (!strcmp(optarg, "showall")) {
        if (!init_drive_database(use_default_db))
          return FAILCMD;
        if (optind < argc) { // -P showall MODEL [FIRMWARE]
          int cnt = showmatchingpresets(argv[optind], (optind+1<argc ? argv[optind+1] : NULL));
          return (cnt >= 0 ? cnt : 0);
        }
        if (showallpresets())
          return FAILCMD; // report regexp syntax error
        return 0;
      } else {
        badarg = true;
      }
      break;
    case 't':
      if (!strcmp(optarg,"offline")) {
        testcnt++;
        ataopts.smart_selftest_type = OFFLINE_FULL_SCAN;
        scsiopts.smart_default_selftest = true;
      } else if (!strcmp(optarg,"short")) {
        testcnt++;
        ataopts.smart_selftest_type = SHORT_SELF_TEST;
        scsiopts.smart_short_selftest = true;
      } else if (!strcmp(optarg,"long")) {
        testcnt++;
        ataopts.smart_selftest_type = EXTEND_SELF_TEST;
        scsiopts.smart_extend_selftest = true;
      } else if (!strcmp(optarg,"conveyance")) {
        testcnt++;
        ataopts.smart_selftest_type = CONVEYANCE_SELF_TEST;
      } else if (!strcmp(optarg,"force")) {
        ataopts.smart_selftest_force = true;
        scsiopts.smart_selftest_force = true;
      } else if (!strcmp(optarg,"afterselect,on")) {
        // scan remainder of disk after doing selected segment
        ataopts.smart_selective_args.scan_after_select = 2;
      } else if (!strcmp(optarg,"afterselect,off")) {
        // don't scan remainder of disk after doing selected segments
        ataopts.smart_selective_args.scan_after_select = 1;
      } else if (!strncmp(optarg,"pending,",strlen("pending,"))) {
	// parse number of minutes that test should be pending
	int i;
	char *tailptr=NULL;
	errno=0;
	i=(int)strtol(optarg+strlen("pending,"), &tailptr, 10);
	if (errno || *tailptr != '\0') {
          snprintf(extraerror, sizeof(extraerror), "Option -t pending,N requires N to be a non-negative integer\n");
          badarg = true;
	} else if (i<0 || i>65535) {
          snprintf(extraerror, sizeof(extraerror), "Option -t pending,N (N=%d) must have 0 <= N <= 65535\n", i);
          badarg = true;
	} else {
          ataopts.smart_selective_args.pending_time = i+1;
	}
      } else if (!strncmp(optarg,"select",strlen("select"))) {
        if (ataopts.smart_selective_args.num_spans == 0)
          testcnt++;
        // parse range of LBAs to test
        uint64_t start, stop; int mode;
        if (split_selective_arg(optarg, &start, &stop, &mode)) {
          snprintf(extraerror, sizeof(extraerror), "Option -t select,M-N must have non-negative integer M and N\n");
          badarg = true;
        } else {
          if (ataopts.smart_selective_args.num_spans >= 5 || start > stop) {
            if (start > stop) {
              snprintf(extraerror, sizeof(extraerror), "ERROR: Start LBA (%" PRIu64 ") > ending LBA (%" PRId64 ") in argument \"%s\"\n",
                start, stop, optarg);
            } else {
              snprintf(extraerror, sizeof(extraerror),"ERROR: No more than five selective self-test spans may be"
                " defined\n");
            }
            badarg = true;
          }
          ataopts.smart_selective_args.span[ataopts.smart_selective_args.num_spans].start = start;
          ataopts.smart_selective_args.span[ataopts.smart_selective_args.num_spans].end   = stop;
          ataopts.smart_selective_args.span[ataopts.smart_selective_args.num_spans].mode  = mode;
          ataopts.smart_selective_args.num_spans++;
          ataopts.smart_selftest_type = SELECTIVE_SELF_TEST;
        }
      } else if (!strncmp(optarg, "scttempint", sizeof("scstempint")-1)) {
        snprintf(extraerror, sizeof(extraerror), "-t scttempint is no longer supported, use -l scttempint instead\n");
        badarg = true;
      } else if (!strncmp(optarg, "vendor,", sizeof("vendor,")-1)) {
        unsigned subcmd = ~0U; int n = -1;
        if (!(   sscanf(optarg, "%*[a-z],0x%x%n", &subcmd, &n) == 1
              && subcmd <= 0xff && n == (int)strlen(optarg))) {
          snprintf(extraerror, sizeof(extraerror), "Option -t vendor,0xNN syntax error\n");
          badarg = true;
        }
        else
          ataopts.smart_selftest_type = subcmd;
      } else {
        badarg = true;
      }
      break;
    case 'C':
      captive = true;
      break;
    case 'X':
      testcnt++;
      scsiopts.smart_selftest_abort = true;
      ataopts.smart_selftest_type = ABORT_SELF_TEST;
      break;
    case 'n':
      // skip disk check if in low-power mode
      if (!strcmp(optarg, "never")) {
        ataopts.powermode = 1; // do not skip, but print mode
        scsiopts.powermode = 1;
      }
      else {
        int n1 = -1, n2 = -1, len = strlen(optarg);
        char s[7+1]; unsigned i = FAILPOWER;
        sscanf(optarg, "%9[a-z]%n,%u%n", s, &n1, &i, &n2);
        if (!((n1 == len || n2 == len) && i <= 255))
          badarg = true;
        else if (!strcmp(s, "sleep")) {
          ataopts.powermode = 2;
          scsiopts.powermode = 2;
        } else if (!strcmp(s, "standby")) {
          ataopts.powermode = 3;
          scsiopts.powermode = 3;
        } else if (!strcmp(s, "idle")) {
          ataopts.powermode = 4;
          scsiopts.powermode = 4;
        } else
          badarg = true;

        ataopts.powerexit = i;
        scsiopts.powerexit = i;
      }
      break;
    case 'f':
      if (!strcmp(optarg, "old")) {
        ataopts.output_format &= ~ata_print_options::FMT_BRIEF;
        output_format_set = true;
      }
      else if (!strcmp(optarg, "brief")) {
        ataopts.output_format |= ata_print_options::FMT_BRIEF;
        output_format_set = true;
      }
      else if (!strcmp(optarg, "hex"))
        ataopts.output_format |= ata_print_options::FMT_HEX_ID
                              |  ata_print_options::FMT_HEX_VAL;
      else if (!strcmp(optarg, "hex,id"))
        ataopts.output_format |= ata_print_options::FMT_HEX_ID;
      else if (!strcmp(optarg, "hex,val"))
        ataopts.output_format |= ata_print_options::FMT_HEX_VAL;
      else
        badarg = true;
      break;
    case 'B':
      {
        const char * path = optarg;
        if (*path == '+' && path[1])
          path++;
        else
          use_default_db = false;
        if (!read_drive_database(path))
          return FAILCMD;
      }
      break;
    case 'h':
      printing_is_off = false;
      printslogan();
      Usage();
      return 0;

    case 'g':
    case_s_continued: // -s, see above
    case opt_set: // --set
      {
        ataopts.get_set_used = true;
        bool get = (optchar == 'g');
        char name[16+1]; unsigned val;
        int n1 = -1, n2 = -1, n3 = -1, len = strlen(optarg);
        if (sscanf(optarg, "%16[^,=]%n%*[,=]%n%u%n", name, &n1, &n2, &val, &n3) >= 1
            && (n1 == len || (!get && n2 > 0))) {
          bool on  = false;
          bool off = false;
          bool ata = false;
          bool persistent = false;

          if (n2 > 0) {
            int len2 = strlen(optarg + n2);
            char * tmp = strstr(optarg+n2, ",p");
            // handle ",p" in persistent options like: wcache-sct,[ata|on|off],p
            if (tmp && (strlen(tmp) == 2)) {
              persistent = true;
              len2 = strlen(optarg+n2) - 2;

              // the ,p option only works for set of SCT Feature Control command
              if (strcmp(name, "wcache-sct") != 0 &&
                  strcmp(name, "wcreorder") != 0)
                badarg = true;
            }
            on  = !strncmp(optarg+n2, "on", len2);
            off = !strncmp(optarg+n2, "off", len2);
            ata = !strncmp(optarg+n2, "ata", len2);
          }
          if (n3 != len)
            val = ~0U;

          if (get && !strcmp(name, "all")) {
            ataopts.get_aam = ataopts.get_apm = true;
            ataopts.get_security = true;
            ataopts.get_lookahead = ataopts.get_wcache = true;
            ataopts.get_dsn = true;
            scsiopts.get_rcd = scsiopts.get_wce = true;
          }
          else if (!strcmp(name, "aam")) {
            if (get)
              ataopts.get_aam = true;
            else if (off)
              ataopts.set_aam = -1;
            else if (val <= 254)
              ataopts.set_aam = val + 1;
            else {
              snprintf(extraerror, sizeof(extraerror), "Option -s aam,N must have 0 <= N <= 254\n");
              badarg = true;
            }
          }
          else if (!strcmp(name, "apm")) {
            if (get)
              ataopts.get_apm = true;
            else if (off)
              ataopts.set_apm = -1;
            else if (1 <= val && val <= 254)
              ataopts.set_apm = val + 1;
            else {
              snprintf(extraerror, sizeof(extraerror), "Option -s apm,N must have 1 <= N <= 254\n");
              badarg = true;
            }
          }
          else if (!strcmp(name, "lookahead")) {
            if (get) {
              ataopts.get_lookahead = true;
            }
            else if (off)
              ataopts.set_lookahead = -1;
            else if (on)
              ataopts.set_lookahead = 1;
            else
              badarg = true;
          }
          else if (!strcmp(name, "wcreorder")) {
            ataopts.sct_wcache_reorder_set_pers = persistent;
            if (get) {
              ataopts.sct_wcache_reorder_get = true;
            }
            else if (off)
              ataopts.sct_wcache_reorder_set = -1;
            else if (on)
              ataopts.sct_wcache_reorder_set = 1;
            else
              badarg = true;
          }
          else if (!strcmp(name, "wcache-sct")) {
            ataopts.sct_wcache_sct_set_pers = persistent;
            if (get) {
              ataopts.sct_wcache_sct_get = true;
            }
            else if (off)
              ataopts.sct_wcache_sct_set = 3;
            else if (on)
              ataopts.sct_wcache_sct_set = 2;
            else if (ata)
              ataopts.sct_wcache_sct_set = 1;
            else
              badarg = true;
          }
          else if (!strcmp(name, "rcache")) {
            if (get)
              scsiopts.get_rcd = true;
            else if (off)
              scsiopts.set_rcd = -1;
            else if (on)
              scsiopts.set_rcd = 1;
            else
              badarg = true;
          }
          else if (get && !strcmp(name, "security")) {
            ataopts.get_security = true;
          }
          else if (!get && !strcmp(optarg, "security-freeze")) {
            ataopts.set_security_freeze = true;
          }
          else if (!get && !strcmp(optarg, "standby,now")) {
              ataopts.set_standby_now = true;
              scsiopts.set_standby_now = true;
          }
          else if (!get && !strcmp(name, "standby")) {
            if (off) {
              ataopts.set_standby = 0 + 1;
              scsiopts.set_standby = 0 + 1;
            } else if (val <= 255) {
              ataopts.set_standby = val + 1;
              scsiopts.set_standby = val + 1;
            } else {
              snprintf(extraerror, sizeof(extraerror), "Option -s standby,N must have 0 <= N <= 255\n");
              badarg = true;
            }
          }
          else if (!strcmp(name, "wcache")) {
            if (get) {
              ataopts.get_wcache = true;
              scsiopts.get_wce = true;
            }
            else if (off) {
              ataopts.set_wcache = -1;
              scsiopts.set_wce = -1;
            }
            else if (on) {
              ataopts.set_wcache = 1;
              scsiopts.set_wce = 1;
            }
            else
              badarg = true;
          }
          else if (!strcmp(name, "dsn")) {
            if (get) {
              ataopts.get_dsn = true;
            }
            else if (off) {
              ataopts.set_dsn = -1;
            }
            else if (on) {
              ataopts.set_dsn = 1;
            }
            else
              badarg = true;
          }
          else
            badarg = true;
        }
        else
          badarg = true;
      }
      break;

    case opt_scan:
    case opt_scan_open:
      scan = optchar;
      break;

    case 'j':
      {
        print_as_json = true;
        print_as_json_options.pretty = true;
        print_as_json_options.sorted = false;
        print_as_json_options.format = 0;
        print_as_json_output = false;
        print_as_json_impl = print_as_json_unimpl = false;
        bool json_verbose = false;
        if (optarg_is_set) {
          for (int i = 0; optarg[i]; i++) {
            switch (optarg[i]) {
              case 'c': print_as_json_options.pretty = false; break;
              case 'g': print_as_json_options.format = 'g'; break;
              case 'i': print_as_json_impl = true; break;
              case 'o': print_as_json_output = true; break;
              case 's': print_as_json_options.sorted = true; break;
              case 'u': print_as_json_unimpl = true; break;
              case 'v': json_verbose = true; break;
              case 'y': print_as_json_options.format = 'y'; break;
              default: badarg = true;
            }
          }
        }
        js_initialize(argc, argv, json_verbose);
      }
      break;

    case '?':
    default:
      printing_is_off = false;
      printslogan();
      // Point arg to the argument in which this option was found.
      arg = argv[optind-1];
      // Check whether the option is a long option that doesn't map to -h.
      if (arg[1] == '-' && optchar != 'h') {
        // Iff optopt holds a valid option then argument must be missing.
        if (optopt && (optopt >= opt_scan || strchr(shortopts, optopt))) {
          jerr("=======> ARGUMENT REQUIRED FOR OPTION: %s\n", arg+2);
          printvalidarglistmessage(optopt);
        } else
          jerr("=======> UNRECOGNIZED OPTION: %s\n",arg+2);
	if (extraerror[0])
	  pout("=======> %s", extraerror);
        UsageSummary();
        return FAILCMD;
      }
      if (0 < optopt && optopt < '~') {
        // Iff optopt holds a valid option then argument must be
        // missing.  Note (BA) this logic seems to fail using Solaris
        // getopt!
        if (strchr(shortopts, optopt) != NULL) {
          jerr("=======> ARGUMENT REQUIRED FOR OPTION: %c\n", optopt);
          printvalidarglistmessage(optopt);
        } else
          jerr("=======> UNRECOGNIZED OPTION: %c\n",optopt);
	if (extraerror[0])
	  pout("=======> %s", extraerror);
        UsageSummary();
        return FAILCMD;
      }
      Usage();
      return 0;
    } // closes switch statement to process command-line options
    
    // Check to see if option had an unrecognized or incorrect argument.
    if (badarg) {
      printslogan();
      // It would be nice to print the actual option name given by the user
      // here, but we just print the short form.  Please fix this if you know
      // a clean way to do it.
      char optstr[] = { (char)optchar, 0 };
      jerr("=======> INVALID ARGUMENT TO -%s: %s\n",
        (optchar == opt_identify ? "-identify" :
         optchar == opt_set ? "-set" :
         optchar == opt_smart ? "-smart" :
         optchar == 'j' ? "-json" : optstr), optarg);
      printvalidarglistmessage(optchar);
      if (extraerror[0])
	pout("=======> %s", extraerror);
      UsageSummary();
      return FAILCMD;
    }
  }

  // Special handling of --scan, --scanopen
  if (scan) {
    // Read or init drive database to allow USB ID check.
    if (!init_drive_database(use_default_db))
      return FAILCMD;
    scan_devices(scan_types, (scan == opt_scan_open), argv + optind);
    return 0;
  }

  // At this point we have processed all command-line options.  If the
  // print output is switchable, then start with the print output
  // turned off
  if (printing_is_switchable)
    printing_is_off = true;

  // Check for multiple -d TYPE options
  if (scan_types.size() > 1) {
    printing_is_off = false;
    printslogan();
    jerr("ERROR: multiple -d TYPE options are only allowed with --scan\n");
    UsageSummary();
    return FAILCMD;
  }

  // error message if user has asked for more than one test
  if (testcnt > 1) {
    printing_is_off = false;
    printslogan();
    jerr("\nERROR: smartctl can only run a single test type (or abort) at a time.\n");
    UsageSummary();
    return FAILCMD;
  }

  // error message if user has set selective self-test options without
  // asking for a selective self-test
  if (   (ataopts.smart_selective_args.pending_time || ataopts.smart_selective_args.scan_after_select)
      && !ataopts.smart_selective_args.num_spans) {
    printing_is_off = false;
    printslogan();
    if (ataopts.smart_selective_args.pending_time)
      jerr("\nERROR: smartctl -t pending,N must be used with -t select,N-M.\n");
    else
      jerr("\nERROR: smartctl -t afterselect,(on|off) must be used with -t select,N-M.\n");
    UsageSummary();
    return FAILCMD;
  }

  // If captive option was used, change test type if appropriate.
  if (captive)
    switch (ataopts.smart_selftest_type) {
      case SHORT_SELF_TEST:
        ataopts.smart_selftest_type = SHORT_CAPTIVE_SELF_TEST;
        scsiopts.smart_short_selftest     = false;
        scsiopts.smart_short_cap_selftest = true;
        break;
      case EXTEND_SELF_TEST:
        ataopts.smart_selftest_type = EXTEND_CAPTIVE_SELF_TEST;
        scsiopts.smart_extend_selftest     = false;
        scsiopts.smart_extend_cap_selftest = true;
        break;
      case CONVEYANCE_SELF_TEST:
        ataopts.smart_selftest_type = CONVEYANCE_CAPTIVE_SELF_TEST;
        break;
      case SELECTIVE_SELF_TEST:
        ataopts.smart_selftest_type = SELECTIVE_CAPTIVE_SELF_TEST;
        break;
    }

  // From here on, normal operations...
  printslogan();
  
  // Warn if the user has provided no device name
  if (argc-optind<1){
    jerr("ERROR: smartctl requires a device name as the final command-line argument.\n\n");
    UsageSummary();
    return FAILCMD;
  }
  
  // Warn if the user has provided more than one device name
  if (argc-optind>1){
    int i;
    jerr("ERROR: smartctl takes ONE device name as the final command-line argument.\n");
    pout("You have provided %d device names:\n",argc-optind);
    for (i=0; i<argc-optind; i++)
      pout("%s\n",argv[optind+i]);
    UsageSummary();
    return FAILCMD;
  }

  // Read or init drive database
  if (!init_drive_database(use_default_db))
    return FAILCMD;

  // No error, continue in main_worker()
  return -1;
}

// Printing functions

__attribute_format_printf(3, 0)
static void vjpout(bool is_js_impl, const char * msg_severity,
                   const char *fmt, va_list ap)
{
  if (!print_as_json) {
    // Print out directly
    vprintf(fmt, ap);
    fflush(stdout);
  }
  else {
    // Add lines to JSON output
    static char buf[1024];
    static char * bufnext = buf;
    vsnprintf(bufnext, sizeof(buf) - (bufnext - buf), fmt, ap);
    for (char * p = buf, *q; ; p = q) {
      if (!(q = strchr(p, '\n'))) {
        // Keep remaining line for next call
        for (bufnext = buf; *p; bufnext++, p++)
          *bufnext = *p;
        break;
      }
      *q++ = 0; // '\n' -> '\0'

      static int lineno = 0;
      lineno++;
      if (print_as_json_output) {
        // Collect full output in array
        static int outindex = 0;
        jglb["smartctl"]["output"][outindex++] = p;
      }
      if (!*p)
        continue; // Skip empty line

      if (msg_severity) {
        // Collect non-empty messages in array
        static int errindex = 0;
        json::ref jref = jglb["smartctl"]["messages"][errindex++];
        jref["string"] = p;
        jref["severity"] = msg_severity;
      }

      if (   ( is_js_impl && print_as_json_impl  )
          || (!is_js_impl && print_as_json_unimpl)) {
        // Add (un)implemented non-empty lines to global object
        jglb[strprintf("smartctl_%04d_%c", lineno,
                     (is_js_impl ? 'i' : 'u')).c_str()] = p;
      }
    }
  }
}

// Default: print to stdout
// --json: ignore
// --json=o: append to "output" array
// --json=u: add "smartctl_NNNN_u" element(s)
void pout(const char *fmt, ...)
{
  if (printing_is_off)
    return;
  if (print_as_json && !(print_as_json_output
      || print_as_json_impl || print_as_json_unimpl))
    return;

  va_list ap;
  va_start(ap, fmt);
  vjpout(false, 0, fmt, ap);
  va_end(ap);
}

// Default: Print to stdout
// --json: ignore
// --json=o: append to "output" array
// --json=i: add "smartctl_NNNN_i" element(s)
void jout(const char *fmt, ...)
{
  if (printing_is_off)
    return;
  if (print_as_json && !(print_as_json_output
      || print_as_json_impl || print_as_json_unimpl))
    return;

  va_list ap;
  va_start(ap, fmt);
  vjpout(true, 0, fmt, ap);
  va_end(ap);
}

// Default: print to stdout
// --json: append to "messages"
// --json=o: append to "output" array
// --json=i: add "smartctl_NNNN_i" element(s)
void jinf(const char *fmt, ...)
{
  if (printing_is_off)
    return;

  va_list ap;
  va_start(ap, fmt);
  vjpout(true, "information", fmt, ap);
  va_end(ap);
}

void jwrn(const char *fmt, ...)
{
  if (printing_is_off)
    return;

  va_list ap;
  va_start(ap, fmt);
  vjpout(true, "warning", fmt, ap);
  va_end(ap);
}

void jerr(const char *fmt, ...)
{
  if (printing_is_off)
    return;

  va_list ap;
  va_start(ap, fmt);
  vjpout(true, "error", fmt, ap);
  va_end(ap);
}

// Globals to set failuretest() policy
bool failuretest_conservative = false;
unsigned char failuretest_permissive = 0;

// Compares failure type to policy in effect, and either exits or
// simply returns to the calling routine.
// Used in ataprint.cpp and scsiprint.cpp.
void failuretest(failure_type type, int returnvalue)
{
  // If this is an error in an "optional" SMART command
  if (type == OPTIONAL_CMD) {
    if (!failuretest_conservative)
      return;
    pout("An optional SMART command failed: exiting. Remove '-T conservative' option to continue.\n");
    throw int(returnvalue);
  }

  // If this is an error in a "mandatory" SMART command
  if (type == MANDATORY_CMD) {
    if (failuretest_permissive--)
      return;
    pout("A mandatory SMART command failed: exiting. To continue, add one or more '-T permissive' options.\n");
    throw int(returnvalue);
  }

  throw std::logic_error("failuretest: Unknown type");
}

// Used to warn users about invalid checksums. Called from atacmds.cpp.
// Action to be taken may be altered by the user.
void checksumwarning(const char * string)
{
  // user has asked us to ignore checksum errors
  if (checksum_err_mode == CHECKSUM_ERR_IGNORE)
    return;

  pout("Warning! %s error: invalid SMART checksum.\n", string);

  // user has asked us to fail on checksum errors
  if (checksum_err_mode == CHECKSUM_ERR_EXIT)
    throw int(FAILSMART);
}

// Return info string about device protocol
static const char * get_protocol_info(const smart_device * dev)
{
  switch (   (int)dev->is_ata()
          | ((int)dev->is_scsi() << 1)
          | ((int)dev->is_nvme() << 2)) {
    case 0x1: return "ATA";
    case 0x2: return "SCSI";
    case 0x3: return "ATA+SCSI";
    case 0x4: return "NVMe";
    default:  return "Unknown";
  }
}

// Add JSON device info
static void js_device_info(const json::ref & jref, const smart_device * dev)
{
  jref["name"] = dev->get_dev_name();
  jref["info_name"] = dev->get_info_name();
  jref["type"] = dev->get_dev_type();
  jref["protocol"] = get_protocol_info(dev);
}

// Device scan
// smartctl [-d type] --scan[-open] -- [PATTERN] [smartd directive ...]
void scan_devices(const smart_devtype_list & types, bool with_open, char ** argv)
{
  bool dont_print = !(ata_debugmode || scsi_debugmode || nvme_debugmode);

  const char * pattern = 0;
  int ai = 0;
  if (argv[ai] && argv[ai][0] != '-')
    pattern = argv[ai++];

  smart_device_list devlist;
  printing_is_off = dont_print;
  bool ok = smi()->scan_smart_devices(devlist, types, pattern);
  printing_is_off = false;

  if (!ok) {
    pout("# scan_smart_devices: %s\n", smi()->get_errmsg());
    return;
  }

  for (unsigned i = 0; i < devlist.size(); i++) {
    smart_device_auto_ptr dev( devlist.release(i) );
    json::ref jref = jglb["devices"][i];

    if (with_open) {
      printing_is_off = dont_print;
      dev.replace ( dev->autodetect_open() );
      printing_is_off = false;
    }

    js_device_info(jref, dev.get());

    if (with_open && !dev->is_open()) {
      jout("# %s -d %s # %s, %s device open failed: %s\n", dev->get_dev_name(),
           dev->get_dev_type(), dev->get_info_name(),
           get_protocol_info(dev.get()), dev->get_errmsg());
      jref["open_error"] = dev->get_errmsg();
      continue;
    }

    jout("%s -d %s", dev->get_dev_name(), dev->get_dev_type());
    if (!argv[ai])
      jout(" # %s, %s device\n", dev->get_info_name(), get_protocol_info(dev.get()));
    else {
      for (int j = ai; argv[j]; j++)
        jout(" %s", argv[j]);
      jout("\n");
    }

    if (dev->is_open())
      dev->close();
  }
}

// Main program without exception handling
static int main_worker(int argc, char **argv)
{
  // Throw if runtime environment does not match compile time test.
  check_config();

  // Initialize interface
  smart_interface::init();
  if (!smi())
    return 1;

  // Parse input arguments
  const char * type = 0;
  ata_print_options ataopts;
  scsi_print_options scsiopts;
  nvme_print_options nvmeopts;
  bool print_type_only = false;
  {
    int status = parse_options(argc, argv, type, ataopts, scsiopts, nvmeopts, print_type_only);
    if (status >= 0)
      return status;
  }

  const char * name = argv[argc-1];

  smart_device_auto_ptr dev;
  if (!strcmp(name,"-")) {
    // Parse "smartctl -r ataioctl,2 ..." output from stdin
    if (type || print_type_only) {
      pout("-d option is not allowed in conjunction with device name \"-\".\n");
      UsageSummary();
      return FAILCMD;
    }
    dev = get_parsed_ata_device(smi(), name);
  }
  else
    // get device of appropriate type
    dev = smi()->get_smart_device(name, type);

  if (!dev) {
    jerr("%s: %s\n", name, smi()->get_errmsg());
    if (type)
      printvalidarglistmessage('d');
    else
      pout("Please specify device type with the -d option.\n");
    UsageSummary();
    return FAILCMD;
  }

  if (print_type_only)
    // Report result of first autodetection
    pout("%s: Device of type '%s' [%s] detected\n",
         dev->get_info_name(), dev->get_dev_type(), get_protocol_info(dev.get()));

  if (dev->is_ata() && ataopts.powermode>=2 && dev->is_powered_down()) {
    jinf("Device is in STANDBY (OS) mode, exit(%d)\n", ataopts.powerexit);
    return ataopts.powerexit;
  }

  // Open device
  {
    // Save old info
    smart_device::device_info oldinfo = dev->get_info();

    // Open with autodetect support, may return 'better' device
    dev.replace( dev->autodetect_open() );

    // Report if type has changed
    if (   (ata_debugmode || scsi_debugmode || nvme_debugmode || print_type_only)
        && oldinfo.dev_type != dev->get_dev_type()                               )
      pout("%s: Device open changed type from '%s' to '%s'\n",
        dev->get_info_name(), oldinfo.dev_type.c_str(), dev->get_dev_type());
  }
  if (!dev->is_open()) {
    jerr("Smartctl open device: %s failed: %s\n", dev->get_info_name(), dev->get_errmsg());
    return FAILDEV;
  }

  // Add JSON info similar to --scan output
  js_device_info(jglb["device"], dev.get());

  // now call appropriate ATA or SCSI routine
  int retval = 0;
  if (print_type_only)
    jout("%s: Device of type '%s' [%s] opened\n",
         dev->get_info_name(), dev->get_dev_type(), get_protocol_info(dev.get()));
  else if (dev->is_ata())
    retval = ataPrintMain(dev->to_ata(), ataopts);
  else if (dev->is_scsi())
    retval = scsiPrintMain(dev->to_scsi(), scsiopts);
  else if (dev->is_nvme())
    retval = nvmePrintMain(dev->to_nvme(), nvmeopts);
  else
    // we should never fall into this branch!
    pout("%s: Neither ATA, SCSI nor NVMe device\n", dev->get_info_name());

  dev->close();
  return retval;
}


// Main program
int main(int argc, char **argv)
{
  int status;
  bool badcode = false;

  try {
    try {
      // Do the real work ...
      status = main_worker(argc, argv);
    }
    catch (int ex) {
      // Exit status from checksumwarning() and failuretest() arrives here
      status = ex;
    }
    // Print JSON if enabled
    if (jglb.has_uint128_output())
      jglb["smartctl"]["uint128_precision_bits"] = uint128_to_str_precision_bits();
    jglb["smartctl"]["exit_status"] = status;
    jglb.print(stdout, print_as_json_options);
  }
  catch (const std::bad_alloc & /*ex*/) {
    // Memory allocation failed (also thrown by std::operator new)
    printf("Smartctl: Out of memory\n");
    status = FAILCMD;
  }
  catch (const std::exception & ex) {
    // Other fatal errors
    printf("Smartctl: Exception: %s\n", ex.what());
    badcode = true;
    status = FAILCMD;
  }

  // Check for remaining device objects
  if (smart_device::get_num_objects() != 0) {
    printf("Smartctl: Internal Error: %d device object(s) left at exit.\n",
           smart_device::get_num_objects());
    badcode = true;
    status = FAILCMD;
  }

  if (badcode)
     printf("Please inform " PACKAGE_BUGREPORT ", including output of smartctl -V.\n");

  return status;
}

