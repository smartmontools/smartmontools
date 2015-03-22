/*
 * Home page of code is: http://smartmontools.sourceforge.net
 *
 * Copyright (C) 2002-11 Bruce Allen <smartmontools-support@lists.sourceforge.net>
 * Copyright (C) 2000    Michael Cornwell <cornwell@acm.org>
 * Copyright (C) 2008    Oliver Bock <brevilo@users.sourceforge.net>
 * Copyright (C) 2008-15 Christian Franke <smartmontools-support@lists.sourceforge.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * You should have received a copy of the GNU General Public License
 * (for example COPYING); If not, see <http://www.gnu.org/licenses/>.
 *
 * This code was originally developed as a Senior Thesis by Michael Cornwell
 * at the Concurrent Systems Laboratory (now part of the Storage Systems
 * Research Center), Jack Baskin School of Engineering, University of
 * California, Santa Cruz. http://ssrc.soe.ucsc.edu/
 *
 */

#include "config.h"
#include "int64.h"

// unconditionally included files
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>   // umask
#include <signal.h>
#include <fcntl.h>
#include <string.h>
#include <syslog.h>
#include <stdarg.h>
#include <stdlib.h>
#include <errno.h>
#include <time.h>
#include <limits.h>
#include <getopt.h>

#include <stdexcept>
#include <string>
#include <vector>
#include <algorithm> // std::replace()

// conditionally included files
#ifndef _WIN32
#include <sys/wait.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_NETDB_H
#include <netdb.h>
#endif

#ifdef _WIN32
#ifdef _MSC_VER
#pragma warning(disable:4761) // "conversion supplied"
typedef unsigned short mode_t;
typedef int pid_t;
#endif
#include <io.h> // umask()
#include <process.h> // getpid()
#endif // _WIN32

#ifdef __CYGWIN__
#include <io.h> // setmode()
#endif // __CYGWIN__

#ifdef HAVE_LIBCAP_NG
#include <cap-ng.h>
#endif // LIBCAP_NG

// locally included files
#include "atacmds.h"
#include "dev_interface.h"
#include "knowndrives.h"
#include "scsicmds.h"
#include "utility.h"

// This is for solaris, where signal() resets the handler to SIG_DFL
// after the first signal is caught.
#ifdef HAVE_SIGSET
#define SIGNALFN sigset
#else
#define SIGNALFN signal
#endif

#ifdef _WIN32
// fork()/signal()/initd simulation for native Windows
#include "daemon_win32.h" // daemon_main/detach/signal()
#undef SIGNALFN
#define SIGNALFN  daemon_signal
#define strsignal daemon_strsignal
#define sleep     daemon_sleep
// SIGQUIT does not exist, CONTROL-Break signals SIGBREAK.
#define SIGQUIT SIGBREAK
#define SIGQUIT_KEYNAME "CONTROL-Break"
#else  // _WIN32
#define SIGQUIT_KEYNAME "CONTROL-\\"
#endif // _WIN32

#if defined (__SVR4) && defined (__sun)
extern "C" int getdomainname(char *, int); // no declaration in header files!
#endif

const char * smartd_cpp_cvsid = "$Id$"
  CONFIG_H_CVSID;

// smartd exit codes
#define EXIT_BADCMD    1   // command line did not parse
#define EXIT_BADCONF   2   // syntax error in config file
#define EXIT_STARTUP   3   // problem forking daemon
#define EXIT_PID       4   // problem creating pid file
#define EXIT_NOCONF    5   // config file does not exist
#define EXIT_READCONF  6   // config file exists but cannot be read

#define EXIT_NOMEM     8   // out of memory
#define EXIT_BADCODE   10  // internal error - should NEVER happen

#define EXIT_BADDEV    16  // we can't monitor this device
#define EXIT_NODEV     17  // no devices to monitor

#define EXIT_SIGNAL    254 // abort on signal


// command-line: 1=debug mode, 2=print presets
static unsigned char debugmode = 0;

// command-line: how long to sleep between checks
#define CHECKTIME 1800
static int checktime=CHECKTIME;

// command-line: name of PID file (empty for no pid file)
static std::string pid_file;

// command-line: path prefix of persistent state file, empty if no persistence.
static std::string state_path_prefix
#ifdef SMARTMONTOOLS_SAVESTATES
          = SMARTMONTOOLS_SAVESTATES
#endif
                                    ;

// command-line: path prefix of attribute log file, empty if no logs.
static std::string attrlog_path_prefix
#ifdef SMARTMONTOOLS_ATTRIBUTELOG
          = SMARTMONTOOLS_ATTRIBUTELOG
#endif
                                    ;

// configuration file name
static const char * configfile;
// configuration file "name" if read from stdin
static const char * const configfile_stdin = "<stdin>";
// path of alternate configuration file
static std::string configfile_alt;

// warning script file
static std::string warning_script;

// command-line: when should we exit?
static int quit=0;

// command-line; this is the default syslog(3) log facility to use.
static int facility=LOG_DAEMON;

#ifndef _WIN32
// command-line: fork into background?
static bool do_fork=true;
#endif

#ifdef HAVE_LIBCAP_NG
// command-line: enable capabilities?
static bool enable_capabilities = false;
#endif

// TODO: This smartctl only variable is also used in os_win32.cpp
unsigned char failuretest_permissive = 0;

// set to one if we catch a USR1 (check devices now)
static volatile int caughtsigUSR1=0;

#ifdef _WIN32
// set to one if we catch a USR2 (toggle debug mode)
static volatile int caughtsigUSR2=0;
#endif

// set to one if we catch a HUP (reload config file). In debug mode,
// set to two, if we catch INT (also reload config file).
static volatile int caughtsigHUP=0;

// set to signal value if we catch INT, QUIT, or TERM
static volatile int caughtsigEXIT=0;

// This function prints either to stdout or to the syslog as needed.
static void PrintOut(int priority, const char *fmt, ...)
                     __attribute_format_printf(2, 3);

// Attribute monitoring flags.
// See monitor_attr_flags below.
enum {
  MONITOR_IGN_FAILUSE = 0x01,
  MONITOR_IGNORE      = 0x02,
  MONITOR_RAW_PRINT   = 0x04,
  MONITOR_RAW         = 0x08,
  MONITOR_AS_CRIT     = 0x10,
  MONITOR_RAW_AS_CRIT = 0x20,
};

// Array of flags for each attribute.
class attribute_flags
{
public:
  attribute_flags()
    { memset(m_flags, 0, sizeof(m_flags)); }

  bool is_set(int id, unsigned char flag) const
    { return (0 < id && id < (int)sizeof(m_flags) && (m_flags[id] & flag)); }

  void set(int id, unsigned char flags)
    {
      if (0 < id && id < (int)sizeof(m_flags))
        m_flags[id] |= flags;
    }

private:
  unsigned char m_flags[256];
};


/// Configuration data for a device. Read from smartd.conf.
/// Supports copy & assignment and is compatible with STL containers.
struct dev_config
{
  int lineno;                             // Line number of entry in file
  std::string name;                       // Device name (with optional extra info)
  std::string dev_name;                   // Device name (plain, for SMARTD_DEVICE variable)
  std::string dev_type;                   // Device type argument from -d directive, empty if none
  std::string dev_idinfo;                 // Device identify info for warning emails
  std::string state_file;                 // Path of the persistent state file, empty if none
  std::string attrlog_file;               // Path of the persistent attrlog file, empty if none
  bool ignore;                            // Ignore this entry
  bool smartcheck;                        // Check SMART status
  bool usagefailed;                       // Check for failed Usage Attributes
  bool prefail;                           // Track changes in Prefail Attributes
  bool usage;                             // Track changes in Usage Attributes
  bool selftest;                          // Monitor number of selftest errors
  bool errorlog;                          // Monitor number of ATA errors
  bool xerrorlog;                         // Monitor number of ATA errors (Extended Comprehensive error log)
  bool offlinests;                        // Monitor changes in offline data collection status
  bool offlinests_ns;                     // Disable auto standby if in progress
  bool selfteststs;                       // Monitor changes in self-test execution status
  bool selfteststs_ns;                    // Disable auto standby if in progress
  bool permissive;                        // Ignore failed SMART commands
  char autosave;                          // 1=disable, 2=enable Autosave Attributes
  char autoofflinetest;                   // 1=disable, 2=enable Auto Offline Test
  firmwarebug_defs firmwarebugs;          // -F directives from drivedb or smartd.conf
  bool ignorepresets;                     // Ignore database of -v options
  bool showpresets;                       // Show database entry for this device
  bool removable;                         // Device may disappear (not be present)
  char powermode;                         // skip check, if disk in idle or standby mode
  bool powerquiet;                        // skip powermode 'skipping checks' message
  int powerskipmax;                       // how many times can be check skipped
  unsigned char tempdiff;                 // Track Temperature changes >= this limit
  unsigned char tempinfo, tempcrit;       // Track Temperatures >= these limits as LOG_INFO, LOG_CRIT+mail
  regular_expression test_regex;          // Regex for scheduled testing

  // Configuration of email warning messages
  std::string emailcmdline;               // script to execute, empty if no messages
  std::string emailaddress;               // email address, or empty
  unsigned char emailfreq;                // Emails once (1) daily (2) diminishing (3)
  bool emailtest;                         // Send test email?

  // ATA ONLY
  int dev_rpm; // rotation rate, 0 = unknown, 1 = SSD, >1 = HDD
  int set_aam; // disable(-1), enable(1..255->0..254) Automatic Acoustic Management
  int set_apm; // disable(-1), enable(2..255->1..254) Advanced Power Management
  int set_lookahead; // disable(-1), enable(1) read look-ahead
  int set_standby; // set(1..255->0..254) standby timer
  bool set_security_freeze; // Freeze ATA security
  int set_wcache; // disable(-1), enable(1) write cache

  bool sct_erc_set;                       // set SCT ERC to:
  unsigned short sct_erc_readtime;        // ERC read time (deciseconds)
  unsigned short sct_erc_writetime;       // ERC write time (deciseconds)

  unsigned char curr_pending_id;          // ID of current pending sector count, 0 if none
  unsigned char offl_pending_id;          // ID of offline uncorrectable sector count, 0 if none
  bool curr_pending_incr, offl_pending_incr; // True if current/offline pending values increase
  bool curr_pending_set,  offl_pending_set;  // True if '-C', '-U' set in smartd.conf

  attribute_flags monitor_attr_flags;     // MONITOR_* flags for each attribute

  ata_vendor_attr_defs attribute_defs;    // -v options

  dev_config();
};

dev_config::dev_config()
: lineno(0),
  ignore(false),
  smartcheck(false),
  usagefailed(false),
  prefail(false),
  usage(false),
  selftest(false),
  errorlog(false),
  xerrorlog(false),
  offlinests(false),  offlinests_ns(false),
  selfteststs(false), selfteststs_ns(false),
  permissive(false),
  autosave(0),
  autoofflinetest(0),
  ignorepresets(false),
  showpresets(false),
  removable(false),
  powermode(0),
  powerquiet(false),
  powerskipmax(0),
  tempdiff(0),
  tempinfo(0), tempcrit(0),
  emailfreq(0),
  emailtest(false),
  dev_rpm(0),
  set_aam(0), set_apm(0),
  set_lookahead(0),
  set_standby(0),
  set_security_freeze(false),
  set_wcache(0),
  sct_erc_set(false),
  sct_erc_readtime(0), sct_erc_writetime(0),
  curr_pending_id(0), offl_pending_id(0),
  curr_pending_incr(false), offl_pending_incr(false),
  curr_pending_set(false),  offl_pending_set(false)
{
}


// Number of allowed mail message types
static const int SMARTD_NMAIL = 13;
// Type for '-M test' mails (state not persistent)
static const int MAILTYPE_TEST = 0;
// TODO: Add const or enum for all mail types.

struct mailinfo {
  int logged;// number of times an email has been sent
  time_t firstsent;// time first email was sent, as defined by time(2)
  time_t lastsent; // time last email was sent, as defined by time(2)

  mailinfo()
    : logged(0), firstsent(0), lastsent(0) { }
};

/// Persistent state data for a device.
struct persistent_dev_state
{
  unsigned char tempmin, tempmax;         // Min/Max Temperatures

  unsigned char selflogcount;             // total number of self-test errors
  unsigned short selfloghour;             // lifetime hours of last self-test error

  time_t scheduled_test_next_check;       // Time of next check for scheduled self-tests

  uint64_t selective_test_last_start;     // Start LBA of last scheduled selective self-test
  uint64_t selective_test_last_end;       // End LBA of last scheduled selective self-test

  mailinfo maillog[SMARTD_NMAIL];         // log info on when mail sent

  // ATA ONLY
  int ataerrorcount;                      // Total number of ATA errors

  // Persistent part of ata_smart_values:
  struct ata_attribute {
    unsigned char id;
    unsigned char val;
    unsigned char worst; // Byte needed for 'raw64' attribute only.
    uint64_t raw;
    unsigned char resvd;

    ata_attribute() : id(0), val(0), worst(0), raw(0), resvd(0) { }
  };
  ata_attribute ata_attributes[NUMBER_ATA_SMART_ATTRIBUTES];
  
  // SCSI ONLY

  struct scsi_error_counter {
    struct scsiErrorCounter errCounter;
    unsigned char found;
    scsi_error_counter() : found(0) { }
  };
  scsi_error_counter scsi_error_counters[3];

  struct scsi_nonmedium_error {
    struct scsiNonMediumError nme;
    unsigned char found;
    scsi_nonmedium_error() : found(0) { }
  };
  scsi_nonmedium_error scsi_nonmedium_error;

  persistent_dev_state();
};

persistent_dev_state::persistent_dev_state()
: tempmin(0), tempmax(0),
  selflogcount(0),
  selfloghour(0),
  scheduled_test_next_check(0),
  selective_test_last_start(0),
  selective_test_last_end(0),
  ataerrorcount(0)
{
}

/// Non-persistent state data for a device.
struct temp_dev_state
{
  bool must_write;                        // true if persistent part should be written

  bool not_cap_offline;                   // true == not capable of offline testing
  bool not_cap_conveyance;
  bool not_cap_short;
  bool not_cap_long;
  bool not_cap_selective;

  unsigned char temperature;              // last recorded Temperature (in Celsius)
  time_t tempmin_delay;                   // time where Min Temperature tracking will start

  bool powermodefail;                     // true if power mode check failed
  int powerskipcnt;                       // Number of checks skipped due to idle or standby mode

  // SCSI ONLY
  unsigned char SmartPageSupported;       // has log sense IE page (0x2f)
  unsigned char TempPageSupported;        // has log sense temperature page (0xd)
  unsigned char ReadECounterPageSupported;
  unsigned char WriteECounterPageSupported;
  unsigned char VerifyECounterPageSupported;
  unsigned char NonMediumErrorPageSupported;
  unsigned char SuppressReport;           // minimize nuisance reports
  unsigned char modese_len;               // mode sense/select cmd len: 0 (don't
                                          // know yet) 6 or 10
  // ATA ONLY
  uint64_t num_sectors;                   // Number of sectors
  ata_smart_values smartval;              // SMART data
  ata_smart_thresholds_pvt smartthres;    // SMART thresholds
  bool offline_started;                   // true if offline data collection was started
  bool selftest_started;                  // true if self-test was started

  temp_dev_state();
};

temp_dev_state::temp_dev_state()
: must_write(false),
  not_cap_offline(false),
  not_cap_conveyance(false),
  not_cap_short(false),
  not_cap_long(false),
  not_cap_selective(false),
  temperature(0),
  tempmin_delay(0),
  powermodefail(false),
  powerskipcnt(0),
  SmartPageSupported(false),
  TempPageSupported(false),
  ReadECounterPageSupported(false),
  WriteECounterPageSupported(false),
  VerifyECounterPageSupported(false),
  NonMediumErrorPageSupported(false),
  SuppressReport(false),
  modese_len(0),
  num_sectors(0),
  offline_started(false),
  selftest_started(false)
{
  memset(&smartval, 0, sizeof(smartval));
  memset(&smartthres, 0, sizeof(smartthres));
}

/// Runtime state data for a device.
struct dev_state
: public persistent_dev_state,
  public temp_dev_state
{
  void update_persistent_state();
  void update_temp_state();
};

/// Container for configuration info for each device.
typedef std::vector<dev_config> dev_config_vector;

/// Container for state info for each device.
typedef std::vector<dev_state> dev_state_vector;

// Copy ATA attributes to persistent state.
void dev_state::update_persistent_state()
{
  for (int i = 0; i < NUMBER_ATA_SMART_ATTRIBUTES; i++) {
    const ata_smart_attribute & ta = smartval.vendor_attributes[i];
    ata_attribute & pa = ata_attributes[i];
    pa.id = ta.id;
    if (ta.id == 0) {
      pa.val = pa.worst = 0; pa.raw = 0;
      continue;
    }
    pa.val = ta.current;
    pa.worst = ta.worst;
    pa.raw =            ta.raw[0]
           | (          ta.raw[1] <<  8)
           | (          ta.raw[2] << 16)
           | ((uint64_t)ta.raw[3] << 24)
           | ((uint64_t)ta.raw[4] << 32)
           | ((uint64_t)ta.raw[5] << 40);
    pa.resvd = ta.reserv;
  }
}

// Copy ATA from persistent to temp state.
void dev_state::update_temp_state()
{
  for (int i = 0; i < NUMBER_ATA_SMART_ATTRIBUTES; i++) {
    const ata_attribute & pa = ata_attributes[i];
    ata_smart_attribute & ta = smartval.vendor_attributes[i];
    ta.id = pa.id;
    if (pa.id == 0) {
      ta.current = ta.worst = 0;
      memset(ta.raw, 0, sizeof(ta.raw));
      continue;
    }
    ta.current = pa.val;
    ta.worst = pa.worst;
    ta.raw[0] = (unsigned char) pa.raw;
    ta.raw[1] = (unsigned char)(pa.raw >>  8);
    ta.raw[2] = (unsigned char)(pa.raw >> 16);
    ta.raw[3] = (unsigned char)(pa.raw >> 24);
    ta.raw[4] = (unsigned char)(pa.raw >> 32);
    ta.raw[5] = (unsigned char)(pa.raw >> 40);
    ta.reserv = pa.resvd;
  }
}

// Parse a line from a state file.
static bool parse_dev_state_line(const char * line, persistent_dev_state & state)
{
  static const regular_expression regex(
    "^ *"
     "((temperature-min)" // (1 (2)
     "|(temperature-max)" // (3)
     "|(self-test-errors)" // (4)
     "|(self-test-last-err-hour)" // (5)
     "|(scheduled-test-next-check)" // (6)
     "|(selective-test-last-start)" // (7)
     "|(selective-test-last-end)" // (8)
     "|(ata-error-count)"  // (9)
     "|(mail\\.([0-9]+)\\." // (10 (11)
       "((count)" // (12 (13)
       "|(first-sent-time)" // (14)
       "|(last-sent-time)" // (15)
       ")" // 12)
      ")" // 10)
     "|(ata-smart-attribute\\.([0-9]+)\\." // (16 (17)
       "((id)" // (18 (19)
       "|(val)" // (20)
       "|(worst)" // (21)
       "|(raw)" // (22)
       "|(resvd)" // (23)
       ")" // 18)
      ")" // 16)
     ")" // 1)
     " *= *([0-9]+)[ \n]*$", // (24)
    REG_EXTENDED
  );

  const int nmatch = 1+24;
  regmatch_t match[nmatch];
  if (!regex.execute(line, nmatch, match))
    return false;
  if (match[nmatch-1].rm_so < 0)
    return false;

  uint64_t val = strtoull(line + match[nmatch-1].rm_so, (char **)0, 10);

  int m = 1;
  if (match[++m].rm_so >= 0)
    state.tempmin = (unsigned char)val;
  else if (match[++m].rm_so >= 0)
    state.tempmax = (unsigned char)val;
  else if (match[++m].rm_so >= 0)
    state.selflogcount = (unsigned char)val;
  else if (match[++m].rm_so >= 0)
    state.selfloghour = (unsigned short)val;
  else if (match[++m].rm_so >= 0)
    state.scheduled_test_next_check = (time_t)val;
  else if (match[++m].rm_so >= 0)
    state.selective_test_last_start = val;
  else if (match[++m].rm_so >= 0)
    state.selective_test_last_end = val;
  else if (match[++m].rm_so >= 0)
    state.ataerrorcount = (int)val;
  else if (match[m+=2].rm_so >= 0) {
    int i = atoi(line+match[m].rm_so);
    if (!(0 <= i && i < SMARTD_NMAIL))
      return false;
    if (i == MAILTYPE_TEST) // Don't suppress test mails
      return true;
    if (match[m+=2].rm_so >= 0)
      state.maillog[i].logged = (int)val;
    else if (match[++m].rm_so >= 0)
      state.maillog[i].firstsent = (time_t)val;
    else if (match[++m].rm_so >= 0)
      state.maillog[i].lastsent = (time_t)val;
    else
      return false;
  }
  else if (match[m+=5+1].rm_so >= 0) {
    int i = atoi(line+match[m].rm_so);
    if (!(0 <= i && i < NUMBER_ATA_SMART_ATTRIBUTES))
      return false;
    if (match[m+=2].rm_so >= 0)
      state.ata_attributes[i].id = (unsigned char)val;
    else if (match[++m].rm_so >= 0)
      state.ata_attributes[i].val = (unsigned char)val;
    else if (match[++m].rm_so >= 0)
      state.ata_attributes[i].worst = (unsigned char)val;
    else if (match[++m].rm_so >= 0)
      state.ata_attributes[i].raw = val;
    else if (match[++m].rm_so >= 0)
      state.ata_attributes[i].resvd = (unsigned char)val;
    else
      return false;
  }
  else
    return false;
  return true;
}

// Read a state file.
static bool read_dev_state(const char * path, persistent_dev_state & state)
{
  stdio_file f(path, "r");
  if (!f) {
    if (errno != ENOENT)
      pout("Cannot read state file \"%s\"\n", path);
    return false;
  }
#ifdef __CYGWIN__
  setmode(fileno(f), O_TEXT); // Allow files with \r\n
#endif

  persistent_dev_state new_state;
  int good = 0, bad = 0;
  char line[256];
  while (fgets(line, sizeof(line), f)) {
    const char * s = line + strspn(line, " \t");
    if (!*s || *s == '#')
      continue;
    if (!parse_dev_state_line(line, new_state))
      bad++;
    else
      good++;
  }

  if (bad) {
    if (!good) {
      pout("%s: format error\n", path);
      return false;
    }
    pout("%s: %d invalid line(s) ignored\n", path, bad);
  }

  // This sets the values missing in the file to 0.
  state = new_state;
  return true;
}

static void write_dev_state_line(FILE * f, const char * name, uint64_t val)
{
  if (val)
    fprintf(f, "%s = %" PRIu64 "\n", name, val);
}

static void write_dev_state_line(FILE * f, const char * name1, int id, const char * name2, uint64_t val)
{
  if (val)
    fprintf(f, "%s.%d.%s = %" PRIu64 "\n", name1, id, name2, val);
}

// Write a state file
static bool write_dev_state(const char * path, const persistent_dev_state & state)
{
  // Rename old "file" to "file~"
  std::string pathbak = path; pathbak += '~';
  unlink(pathbak.c_str());
  rename(path, pathbak.c_str());

  stdio_file f(path, "w");
  if (!f) {
    pout("Cannot create state file \"%s\"\n", path);
    return false;
  }

  fprintf(f, "# smartd state file\n");
  write_dev_state_line(f, "temperature-min", state.tempmin);
  write_dev_state_line(f, "temperature-max", state.tempmax);
  write_dev_state_line(f, "self-test-errors", state.selflogcount);
  write_dev_state_line(f, "self-test-last-err-hour", state.selfloghour);
  write_dev_state_line(f, "scheduled-test-next-check", state.scheduled_test_next_check);
  write_dev_state_line(f, "selective-test-last-start", state.selective_test_last_start);
  write_dev_state_line(f, "selective-test-last-end", state.selective_test_last_end);

  int i;
  for (i = 0; i < SMARTD_NMAIL; i++) {
    if (i == MAILTYPE_TEST) // Don't suppress test mails
      continue;
    const mailinfo & mi = state.maillog[i];
    if (!mi.logged)
      continue;
    write_dev_state_line(f, "mail", i, "count", mi.logged);
    write_dev_state_line(f, "mail", i, "first-sent-time", mi.firstsent);
    write_dev_state_line(f, "mail", i, "last-sent-time", mi.lastsent);
  }

  // ATA ONLY
  write_dev_state_line(f, "ata-error-count", state.ataerrorcount);

  for (i = 0; i < NUMBER_ATA_SMART_ATTRIBUTES; i++) {
    const persistent_dev_state::ata_attribute & pa = state.ata_attributes[i];
    if (!pa.id)
      continue;
    write_dev_state_line(f, "ata-smart-attribute", i, "id", pa.id);
    write_dev_state_line(f, "ata-smart-attribute", i, "val", pa.val);
    write_dev_state_line(f, "ata-smart-attribute", i, "worst", pa.worst);
    write_dev_state_line(f, "ata-smart-attribute", i, "raw", pa.raw);
    write_dev_state_line(f, "ata-smart-attribute", i, "resvd", pa.resvd);
  }

  return true;
}

// Write to the attrlog file
static bool write_dev_attrlog(const char * path, const dev_state & state)
{
  stdio_file f(path, "a");
  if (!f) {
    pout("Cannot create attribute log file \"%s\"\n", path);
    return false;
  }

  
  time_t now = time(0);
  struct tm * tms = gmtime(&now);
  fprintf(f, "%d-%02d-%02d %02d:%02d:%02d;",
             1900+tms->tm_year, 1+tms->tm_mon, tms->tm_mday,
             tms->tm_hour, tms->tm_min, tms->tm_sec);
  // ATA ONLY
  for (int i = 0; i < NUMBER_ATA_SMART_ATTRIBUTES; i++) {
    const persistent_dev_state::ata_attribute & pa = state.ata_attributes[i];
    if (!pa.id)
      continue;
    fprintf(f, "\t%d;%d;%" PRIu64 ";", pa.id, pa.val, pa.raw);
  }
  // SCSI ONLY
  const struct scsiErrorCounter * ecp;
  const char * pageNames[3] = {"read", "write", "verify"};
  for (int k = 0; k < 3; ++k) {
    if ( !state.scsi_error_counters[k].found ) continue;
    ecp = &state.scsi_error_counters[k].errCounter;
     fprintf(f, "\t%s-corr-by-ecc-fast;%" PRIu64 ";"
       "\t%s-corr-by-ecc-delayed;%" PRIu64 ";"
       "\t%s-corr-by-retry;%" PRIu64 ";"
       "\t%s-total-err-corrected;%" PRIu64 ";"
       "\t%s-corr-algorithm-invocations;%" PRIu64 ";"
       "\t%s-gb-processed;%.3f;"
       "\t%s-total-unc-errors;%" PRIu64 ";",
       pageNames[k], ecp->counter[0],
       pageNames[k], ecp->counter[1],
       pageNames[k], ecp->counter[2],
       pageNames[k], ecp->counter[3],
       pageNames[k], ecp->counter[4],
       pageNames[k], (ecp->counter[5] / 1000000000.0),
       pageNames[k], ecp->counter[6]);
  }
  if(state.scsi_nonmedium_error.found && state.scsi_nonmedium_error.nme.gotPC0) {
    fprintf(f, "\tnon-medium-errors;%" PRIu64 ";", state.scsi_nonmedium_error.nme.counterPC0);
  }
  // write SCSI current temperature if it is monitored
  if(state.TempPageSupported && state.temperature)
     fprintf(f, "\ttemperature;%d;", state.temperature);
  // end of line
  fprintf(f, "\n");
  return true;
}

// Write all state files. If write_always is false, don't write
// unless must_write is set.
static void write_all_dev_states(const dev_config_vector & configs,
                                 dev_state_vector & states,
                                 bool write_always = true)
{
  for (unsigned i = 0; i < states.size(); i++) {
    const dev_config & cfg = configs.at(i);
    if (cfg.state_file.empty())
      continue;
    dev_state & state = states[i];
    if (!write_always && !state.must_write)
      continue;
    if (!write_dev_state(cfg.state_file.c_str(), state))
      continue;
    state.must_write = false;
    if (write_always || debugmode)
      PrintOut(LOG_INFO, "Device: %s, state written to %s\n",
               cfg.name.c_str(), cfg.state_file.c_str());
  }
}

// Write to all attrlog files
static void write_all_dev_attrlogs(const dev_config_vector & configs,
                                   dev_state_vector & states)
{
  for (unsigned i = 0; i < states.size(); i++) {
    const dev_config & cfg = configs.at(i);
    if (cfg.attrlog_file.empty())
      continue;
    dev_state & state = states[i];
    write_dev_attrlog(cfg.attrlog_file.c_str(), state);
  }
}

// remove the PID file
static void RemovePidFile()
{
  if (!pid_file.empty()) {
    if (unlink(pid_file.c_str()))
      PrintOut(LOG_CRIT,"Can't unlink PID file %s (%s).\n", 
               pid_file.c_str(), strerror(errno));
    pid_file.clear();
  }
  return;
}

extern "C" { // signal handlers require C-linkage

//  Note if we catch a SIGUSR1
static void USR1handler(int sig)
{
  if (SIGUSR1==sig)
    caughtsigUSR1=1;
  return;
}

#ifdef _WIN32
//  Note if we catch a SIGUSR2
static void USR2handler(int sig)
{
  if (SIGUSR2==sig)
    caughtsigUSR2=1;
  return;
}
#endif

// Note if we catch a HUP (or INT in debug mode)
static void HUPhandler(int sig)
{
  if (sig==SIGHUP)
    caughtsigHUP=1;
  else
    caughtsigHUP=2;
  return;
}

// signal handler for TERM, QUIT, and INT (if not in debug mode)
static void sighandler(int sig)
{
  if (!caughtsigEXIT)
    caughtsigEXIT=sig;
  return;
}

} // extern "C"

// Cleanup, print Goodbye message and remove pidfile
static int Goodbye(int status)
{
  // delete PID file, if one was created
  RemovePidFile();

  // if we are exiting because of a code bug, tell user
  if (status==EXIT_BADCODE)
        PrintOut(LOG_CRIT, "Please inform " PACKAGE_BUGREPORT ", including output of smartd -V.\n");

  // and this should be the final output from smartd before it exits
  PrintOut(status?LOG_CRIT:LOG_INFO, "smartd is exiting (exit status %d)\n", status);

  return status;
}

// a replacement for setenv() which is not available on all platforms.
// Note that the string passed to putenv must not be freed or made
// invalid, since a pointer to it is kept by putenv(). This means that
// it must either be a static buffer or allocated off the heap. The
// string can be freed if the environment variable is redefined via
// another call to putenv(). There is no portable way to unset a variable
// with putenv(). So we manage the buffer in a static object.
// Using setenv() if available is not considered because some
// implementations may produce memory leaks.

class env_buffer
{
public:
  env_buffer()
    : m_buf((char *)0) { }

  void set(const char * name, const char * value);

private:
  char * m_buf;

  env_buffer(const env_buffer &);
  void operator=(const env_buffer &);
};

void env_buffer::set(const char * name, const char * value)
{
  int size = strlen(name) + 1 + strlen(value) + 1;
  char * newbuf = new char[size];
  snprintf(newbuf, size, "%s=%s", name, value);

  if (putenv(newbuf))
    throw std::runtime_error("putenv() failed");

  // This assumes that the same NAME is passed on each call
  delete [] m_buf;
  m_buf = newbuf;
}

#define EBUFLEN 1024

static void MailWarning(const dev_config & cfg, dev_state & state, int which, const char *fmt, ...)
                        __attribute_format_printf(4, 5);

// If either address or executable path is non-null then send and log
// a warning email, or execute executable
static void MailWarning(const dev_config & cfg, dev_state & state, int which, const char *fmt, ...)
{
  static const char * const whichfail[] = {
    "EmailTest",                  // 0
    "Health",                     // 1
    "Usage",                      // 2
    "SelfTest",                   // 3
    "ErrorCount",                 // 4
    "FailedHealthCheck",          // 5
    "FailedReadSmartData",        // 6
    "FailedReadSmartErrorLog",    // 7
    "FailedReadSmartSelfTestLog", // 8
    "FailedOpenDevice",           // 9
    "CurrentPendingSector",       // 10
    "OfflineUncorrectableSector", // 11
    "Temperature"                 // 12
  };
  
  // See if user wants us to send mail
  if (cfg.emailaddress.empty() && cfg.emailcmdline.empty())
    return;

  std::string address = cfg.emailaddress;
  const char * executable = cfg.emailcmdline.c_str();

  // which type of mail are we sending?
  mailinfo * mail=(state.maillog)+which;

  // checks for sanity
  if (cfg.emailfreq<1 || cfg.emailfreq>3) {
    PrintOut(LOG_CRIT,"internal error in MailWarning(): cfg.mailwarn->emailfreq=%d\n",cfg.emailfreq);
    return;
  }
  if (which<0 || which>=SMARTD_NMAIL || sizeof(whichfail)!=SMARTD_NMAIL*sizeof(char *)) {
    PrintOut(LOG_CRIT,"Contact " PACKAGE_BUGREPORT "; internal error in MailWarning(): which=%d, size=%d\n",
             which, (int)sizeof(whichfail));
    return;
  }

  // Return if a single warning mail has been sent.
  if ((cfg.emailfreq==1) && mail->logged)
    return;

  // Return if this is an email test and one has already been sent.
  if (which == 0 && mail->logged)
    return;
  
  // To decide if to send mail, we need to know what time it is.
  time_t epoch = time(0);

  // Return if less than one day has gone by
  const int day = 24*3600;
  if (cfg.emailfreq==2 && mail->logged && epoch<(mail->lastsent+day))
    return;

  // Return if less than 2^(logged-1) days have gone by
  if (cfg.emailfreq==3 && mail->logged) {
    int days = 0x01 << (mail->logged - 1);
    days*=day;
    if  (epoch<(mail->lastsent+days))
      return;
  }

#ifdef HAVE_LIBCAP_NG
  if (enable_capabilities) {
    PrintOut(LOG_ERR, "Sending a mail was supressed. "
             "Mails can't be send when capabilites are enabled\n");
    return;
  }
#endif

  // record the time of this mail message, and the first mail message
  if (!mail->logged)
    mail->firstsent=epoch;
  mail->lastsent=epoch;

  // print warning string into message
  char message[256];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(message, sizeof(message), fmt, ap);
  va_end(ap);

  // replace commas by spaces to separate recipients
  std::replace(address.begin(), address.end(), ',', ' ');

  // Export information in environment variables that will be useful
  // for user scripts
  static env_buffer env[12];
  env[0].set("SMARTD_MAILER", executable);
  env[1].set("SMARTD_MESSAGE", message);
  char dates[DATEANDEPOCHLEN];
  snprintf(dates, sizeof(dates), "%d", mail->logged);
  env[2].set("SMARTD_PREVCNT", dates);
  dateandtimezoneepoch(dates, mail->firstsent);
  env[3].set("SMARTD_TFIRST", dates);
  snprintf(dates, DATEANDEPOCHLEN,"%d", (int)mail->firstsent);
  env[4].set("SMARTD_TFIRSTEPOCH", dates);
  env[5].set("SMARTD_FAILTYPE", whichfail[which]);
  env[6].set("SMARTD_ADDRESS", address.c_str());
  env[7].set("SMARTD_DEVICESTRING", cfg.name.c_str());

  // Allow 'smartctl ... -d $SMARTD_DEVICETYPE $SMARTD_DEVICE'
  env[8].set("SMARTD_DEVICETYPE",
             (!cfg.dev_type.empty() ? cfg.dev_type.c_str() : "auto"));
  env[9].set("SMARTD_DEVICE", cfg.dev_name.c_str());

  env[10].set("SMARTD_DEVICEINFO", cfg.dev_idinfo.c_str());
  dates[0] = 0;
  if (which) switch (cfg.emailfreq) {
    case 2: dates[0] = '1'; dates[1] = 0; break;
    case 3: snprintf(dates, sizeof(dates), "%d", (0x01)<<mail->logged);
  }
  env[11].set("SMARTD_NEXTDAYS", dates);

  // now construct a command to send this as EMAIL
  char command[2048];
  if (!*executable)
    executable = "<mail>";
  const char * newadd = (!address.empty()? address.c_str() : "<nomailer>");
  const char * newwarn = (which? "Warning via" : "Test of");

#ifndef _WIN32
  snprintf(command, sizeof(command), "%s 2>&1", warning_script.c_str());
  
  // tell SYSLOG what we are about to do...
  PrintOut(LOG_INFO,"%s %s to %s ...\n",
           which?"Sending warning via":"Executing test of", executable, newadd);
  
  // issue the command to send mail or to run the user's executable
  errno=0;
  FILE * pfp;
  if (!(pfp=popen(command, "r")))
    // failed to popen() mail process
    PrintOut(LOG_CRIT,"%s %s to %s: failed (fork or pipe failed, or no memory) %s\n", 
	     newwarn,  executable, newadd, errno?strerror(errno):"");
  else {
    // pipe suceeded!
    int len, status;
    char buffer[EBUFLEN];

    // if unexpected output on stdout/stderr, null terminate, print, and flush
    if ((len=fread(buffer, 1, EBUFLEN, pfp))) {
      int count=0;
      int newlen = len<EBUFLEN ? len : EBUFLEN-1;
      buffer[newlen]='\0';
      PrintOut(LOG_CRIT,"%s %s to %s produced unexpected output (%s%d bytes) to STDOUT/STDERR: \n%s\n", 
	       newwarn, executable, newadd, len!=newlen?"here truncated to ":"", newlen, buffer);
      
      // flush pipe if needed
      while (fread(buffer, 1, EBUFLEN, pfp) && count<EBUFLEN)
	count++;

      // tell user that pipe was flushed, or that something is really wrong
      if (count && count<EBUFLEN)
	PrintOut(LOG_CRIT,"%s %s to %s: flushed remaining STDOUT/STDERR\n", 
		 newwarn, executable, newadd);
      else if (count)
	PrintOut(LOG_CRIT,"%s %s to %s: more than 1 MB STDOUT/STDERR flushed, breaking pipe\n", 
		 newwarn, executable, newadd);
    }
    
    // if something went wrong with mail process, print warning
    errno=0;
    if (-1==(status=pclose(pfp)))
      PrintOut(LOG_CRIT,"%s %s to %s: pclose(3) failed %s\n", newwarn, executable, newadd,
	       errno?strerror(errno):"");
    else {
      // mail process apparently succeeded. Check and report exit status
      int status8;

      if (WIFEXITED(status)) {
	// exited 'normally' (but perhaps with nonzero status)
	status8=WEXITSTATUS(status);
	
	if (status8>128)  
	  PrintOut(LOG_CRIT,"%s %s to %s: failed (32-bit/8-bit exit status: %d/%d) perhaps caught signal %d [%s]\n", 
		   newwarn, executable, newadd, status, status8, status8-128, strsignal(status8-128));
	else if (status8)  
	  PrintOut(LOG_CRIT,"%s %s to %s: failed (32-bit/8-bit exit status: %d/%d)\n", 
		   newwarn, executable, newadd, status, status8);
	else
	  PrintOut(LOG_INFO,"%s %s to %s: successful\n", newwarn, executable, newadd);
      }
      
      if (WIFSIGNALED(status))
	PrintOut(LOG_INFO,"%s %s to %s: exited because of uncaught signal %d [%s]\n", 
		 newwarn, executable, newadd, WTERMSIG(status), strsignal(WTERMSIG(status)));
      
      // this branch is probably not possible. If subprocess is
      // stopped then pclose() should not return.
      if (WIFSTOPPED(status)) 
      	PrintOut(LOG_CRIT,"%s %s to %s: process STOPPED because it caught signal %d [%s]\n",
		 newwarn, executable, newadd, WSTOPSIG(status), strsignal(WSTOPSIG(status)));
      
    }
  }
  
#else // _WIN32
  {
    snprintf(command, sizeof(command), "cmd /c \"%s\"", warning_script.c_str());

    char stdoutbuf[800]; // < buffer in syslog_win32::vsyslog()
    int rc;
    // run command
    PrintOut(LOG_INFO,"%s %s to %s ...\n",
             (which?"Sending warning via":"Executing test of"), executable, newadd);
    rc = daemon_spawn(command, "", 0, stdoutbuf, sizeof(stdoutbuf));
    if (rc >= 0 && stdoutbuf[0])
      PrintOut(LOG_CRIT,"%s %s to %s produced unexpected output (%d bytes) to STDOUT/STDERR:\n%s\n",
        newwarn, executable, newadd, (int)strlen(stdoutbuf), stdoutbuf);
    if (rc != 0)
      PrintOut(LOG_CRIT,"%s %s to %s: failed, exit status %d\n",
        newwarn, executable, newadd, rc);
    else
      PrintOut(LOG_INFO,"%s %s to %s: successful\n", newwarn, executable, newadd);
  }

#endif // _WIN32

  // increment mail sent counter
  mail->logged++;
}

static void reset_warning_mail(const dev_config & cfg, dev_state & state, int which, const char *fmt, ...)
                               __attribute_format_printf(4, 5);

static void reset_warning_mail(const dev_config & cfg, dev_state & state, int which, const char *fmt, ...)
{
  if (!(0 <= which && which < SMARTD_NMAIL))
    return;

  // Return if no mail sent yet
  mailinfo & mi = state.maillog[which];
  if (!mi.logged)
    return;

  // Format & print message
  char msg[256];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(msg, sizeof(msg), fmt, ap);
  va_end(ap);

  PrintOut(LOG_INFO, "Device: %s, %s, warning condition reset after %d email%s\n", cfg.name.c_str(),
           msg, mi.logged, (mi.logged==1 ? "" : "s"));

  // Clear mail counter and timestamps
  mi = mailinfo();
  state.must_write = true;
}

#ifndef _WIN32

// Output multiple lines via separate syslog(3) calls.
static void vsyslog_lines(int priority, const char * fmt, va_list ap)
{
  char buf[512+EBUFLEN]; // enough space for exec cmd output in MailWarning()
  vsnprintf(buf, sizeof(buf), fmt, ap);

  for (char * p = buf, * q; p && *p; p = q) {
    if ((q = strchr(p, '\n')))
      *q++ = 0;
    if (*p)
      syslog(priority, "%s\n", p);
  }
}

#else  // _WIN32
// os_win32/syslog_win32.cpp supports multiple lines.
#define vsyslog_lines vsyslog
#endif // _WIN32

// Printing function for watching ataprint commands, or losing them
// [From GLIBC Manual: Since the prototype doesn't specify types for
// optional arguments, in a call to a variadic function the default
// argument promotions are performed on the optional argument
// values. This means the objects of type char or short int (whether
// signed or not) are promoted to either int or unsigned int, as
// appropriate.]
void pout(const char *fmt, ...){
  va_list ap;

  // get the correct time in syslog()
  FixGlibcTimeZoneBug();
  // initialize variable argument list 
  va_start(ap,fmt);
  // in debugmode==1 mode we will print the output from the ataprint.o functions!
  if (debugmode && debugmode != 2) {
    FILE * f = stdout;
#ifdef _WIN32
    if (facility == LOG_LOCAL1) // logging to stdout
      f = stderr;
#endif
    vfprintf(f, fmt, ap);
    fflush(f);
  }
  // in debugmode==2 mode we print output from knowndrives.o functions
  else if (debugmode==2 || ata_debugmode || scsi_debugmode) {
    openlog("smartd", LOG_PID, facility);
    vsyslog_lines(LOG_INFO, fmt, ap);
    closelog();
  }
  va_end(ap);
  return;
}

// This function prints either to stdout or to the syslog as needed.
static void PrintOut(int priority, const char *fmt, ...){
  va_list ap;
  
  // get the correct time in syslog()
  FixGlibcTimeZoneBug();
  // initialize variable argument list 
  va_start(ap,fmt);
  if (debugmode) {
    FILE * f = stdout;
#ifdef _WIN32
    if (facility == LOG_LOCAL1) // logging to stdout
      f = stderr;
#endif
    vfprintf(f, fmt, ap);
    fflush(f);
  }
  else {
    openlog("smartd", LOG_PID, facility);
    vsyslog_lines(priority, fmt, ap);
    closelog();
  }
  va_end(ap);
  return;
}

// Used to warn users about invalid checksums. Called from atacmds.cpp.
void checksumwarning(const char * string)
{
  pout("Warning! %s error: invalid SMART checksum.\n", string);
}

#ifndef _WIN32

// Wait for the pid file to show up, this makes sure a calling program knows
// that the daemon is really up and running and has a pid to kill it
static bool WaitForPidFile()
{
    int waited, max_wait = 10;
    struct stat stat_buf;

    if (pid_file.empty() || debugmode)
    	return true;

    for(waited = 0; waited < max_wait; ++waited) {
	if (!stat(pid_file.c_str(), &stat_buf)) {
		return true;
	} else
		sleep(1);
    }
    return false;
}

#endif // _WIN32

// Forks new process, closes ALL file descriptors, redirects stdin,
// stdout, and stderr.  Not quite daemon().  See
// http://www.linuxjournal.com/article/2335
// for a good description of why we do things this way.
static void DaemonInit()
{
#ifndef _WIN32
  pid_t pid;
  int i;  

  // flush all buffered streams.  Else we might get two copies of open
  // streams since both parent and child get copies of the buffers.
  fflush(NULL);

  if (do_fork) {
    if ((pid=fork()) < 0) {
      // unable to fork!
      PrintOut(LOG_CRIT,"smartd unable to fork daemon process!\n");
      EXIT(EXIT_STARTUP);
    }
    else if (pid) {
      // we are the parent process, wait for pid file, then exit cleanly
      if(!WaitForPidFile()) {
        PrintOut(LOG_CRIT,"PID file %s didn't show up!\n", pid_file.c_str());
     	EXIT(EXIT_STARTUP);
      } else
        EXIT(0);
    }
  
    // from here on, we are the child process.
    setsid();

    // Fork one more time to avoid any possibility of having terminals
    if ((pid=fork()) < 0) {
      // unable to fork!
      PrintOut(LOG_CRIT,"smartd unable to fork daemon process!\n");
      EXIT(EXIT_STARTUP);
    }
    else if (pid)
      // we are the parent process -- exit cleanly
      EXIT(0);

    // Now we are the child's child...
  }

  // close any open file descriptors
  for (i=getdtablesize();i>=0;--i)
    close(i);
  
#define NO_warn_unused_result(cmd) { if (cmd) {} ; }

  // redirect any IO attempts to /dev/null for stdin
  i=open("/dev/null",O_RDWR);
  if (i>=0) {
    // stdout
    NO_warn_unused_result(dup(i));
    // stderr
    NO_warn_unused_result(dup(i));
  };
  umask(0022);
  NO_warn_unused_result(chdir("/"));

  if (do_fork)
    PrintOut(LOG_INFO, "smartd has fork()ed into background mode. New PID=%d.\n", (int)getpid());

#else // _WIN32

  // No fork() on native Win32
  // Detach this process from console
  fflush(NULL);
  if (daemon_detach("smartd")) {
    PrintOut(LOG_CRIT,"smartd unable to detach from console!\n");
    EXIT(EXIT_STARTUP);
  }
  // stdin/out/err now closed if not redirected

#endif // _WIN32
  return;
}

// create a PID file containing the current process id
static void WritePidFile()
{
  if (!pid_file.empty()) {
    pid_t pid = getpid();
    mode_t old_umask;
#ifndef __CYGWIN__
    old_umask = umask(0077); // rwx------
#else
    // Cygwin: smartd service runs on system account, ensure PID file can be read by admins
    old_umask = umask(0033); // rwxr--r--
#endif

    stdio_file f(pid_file.c_str(), "w");
    umask(old_umask);
    if (!(f && fprintf(f, "%d\n", (int)pid) > 0 && f.close())) {
      PrintOut(LOG_CRIT, "unable to write PID file %s - exiting.\n", pid_file.c_str());
      EXIT(EXIT_PID);
    }
    PrintOut(LOG_INFO, "file %s written containing PID %d\n", pid_file.c_str(), (int)pid);
  }
}

// Prints header identifying version of code and home
static void PrintHead()
{
  PrintOut(LOG_INFO, "%s\n", format_version_info("smartd").c_str());
}

// prints help info for configuration file Directives
static void Directives()
{
  PrintOut(LOG_INFO,
           "Configuration file (%s) Directives (after device name):\n"
           "  -d TYPE Set the device type: auto, ignore, removable,\n"
           "          %s\n"
           "  -T TYPE Set the tolerance to one of: normal, permissive\n"
           "  -o VAL  Enable/disable automatic offline tests (on/off)\n"
           "  -S VAL  Enable/disable attribute autosave (on/off)\n"
           "  -n MODE No check if: never, sleep[,N][,q], standby[,N][,q], idle[,N][,q]\n"
           "  -H      Monitor SMART Health Status, report if failed\n"
           "  -s REG  Do Self-Test at time(s) given by regular expression REG\n"
           "  -l TYPE Monitor SMART log or self-test status:\n"
           "          error, selftest, xerror, offlinests[,ns], selfteststs[,ns]\n"
           "  -l scterc,R,W  Set SCT Error Recovery Control\n"
           "  -e      Change device setting: aam,[N|off], apm,[N|off], lookahead,[on|off],\n"
           "          security-freeze, standby,[N|off], wcache,[on|off]\n"
           "  -f      Monitor 'Usage' Attributes, report failures\n"
           "  -m ADD  Send email warning to address ADD\n"
           "  -M TYPE Modify email warning behavior (see man page)\n"
           "  -p      Report changes in 'Prefailure' Attributes\n"
           "  -u      Report changes in 'Usage' Attributes\n"
           "  -t      Equivalent to -p and -u Directives\n"
           "  -r ID   Also report Raw values of Attribute ID with -p, -u or -t\n"
           "  -R ID   Track changes in Attribute ID Raw value with -p, -u or -t\n"
           "  -i ID   Ignore Attribute ID for -f Directive\n"
           "  -I ID   Ignore Attribute ID for -p, -u or -t Directive\n"
           "  -C ID[+] Monitor [increases of] Current Pending Sectors in Attribute ID\n"
           "  -U ID[+] Monitor [increases of] Offline Uncorrectable Sectors in Attribute ID\n"
           "  -W D,I,C Monitor Temperature D)ifference, I)nformal limit, C)ritical limit\n"
           "  -v N,ST Modifies labeling of Attribute N (see man page)  \n"
           "  -P TYPE Drive-specific presets: use, ignore, show, showall\n"
           "  -a      Default: -H -f -t -l error -l selftest -l selfteststs -C 197 -U 198\n"
           "  -F TYPE Use firmware bug workaround:\n"
           "          %s\n"
           "   #      Comment: text after a hash sign is ignored\n"
           "   \\      Line continuation character\n"
           "Attribute ID is a decimal integer 1 <= ID <= 255\n"
           "Use ID = 0 to turn off -C and/or -U Directives\n"
           "Example: /dev/sda -a\n",
           configfile,
           smi()->get_valid_dev_types_str().c_str(),
           get_valid_firmwarebug_args());
}

/* Returns a pointer to a static string containing a formatted list of the valid
   arguments to the option opt or NULL on failure. */
static const char *GetValidArgList(char opt)
{
  switch (opt) {
  case 'A':
  case 's':
    return "<PATH_PREFIX>";
  case 'c':
    return "<FILE_NAME>, -";
  case 'l':
    return "daemon, local0, local1, local2, local3, local4, local5, local6, local7";
  case 'q':
    return "nodev, errors, nodevstartup, never, onecheck, showtests";
  case 'r':
    return "ioctl[,N], ataioctl[,N], scsiioctl[,N]";
  case 'B':
  case 'p':
  case 'w':
    return "<FILE_NAME>";
  case 'i':
    return "<INTEGER_SECONDS>";
  default:
    return NULL;
  }
}

/* prints help information for command syntax */
static void Usage()
{
  PrintOut(LOG_INFO,"Usage: smartd [options]\n\n");
  PrintOut(LOG_INFO,"  -A PREFIX, --attributelog=PREFIX\n");
  PrintOut(LOG_INFO,"        Log ATA attribute information to {PREFIX}MODEL-SERIAL.ata.csv\n");
#ifdef SMARTMONTOOLS_ATTRIBUTELOG
  PrintOut(LOG_INFO,"        [default is " SMARTMONTOOLS_ATTRIBUTELOG "MODEL-SERIAL.ata.csv]\n");
#endif
  PrintOut(LOG_INFO,"\n");
  PrintOut(LOG_INFO,"  -B [+]FILE, --drivedb=[+]FILE\n");
  PrintOut(LOG_INFO,"        Read and replace [add] drive database from FILE\n");
  PrintOut(LOG_INFO,"        [default is +%s", get_drivedb_path_add());
#ifdef SMARTMONTOOLS_DRIVEDBDIR
  PrintOut(LOG_INFO,"\n");
  PrintOut(LOG_INFO,"         and then    %s", get_drivedb_path_default());
#endif
  PrintOut(LOG_INFO,"]\n\n");
  PrintOut(LOG_INFO,"  -c NAME|-, --configfile=NAME|-\n");
  PrintOut(LOG_INFO,"        Read configuration file NAME or stdin\n");
  PrintOut(LOG_INFO,"        [default is %s]\n\n", configfile);
#ifdef HAVE_LIBCAP_NG
  PrintOut(LOG_INFO,"  -C, --capabilities\n");
  PrintOut(LOG_INFO,"        Drop unneeded Linux process capabilities.\n"
                    "        Warning: Mail notification does not work when used.\n\n");
#endif
  PrintOut(LOG_INFO,"  -d, --debug\n");
  PrintOut(LOG_INFO,"        Start smartd in debug mode\n\n");
  PrintOut(LOG_INFO,"  -D, --showdirectives\n");
  PrintOut(LOG_INFO,"        Print the configuration file Directives and exit\n\n");
  PrintOut(LOG_INFO,"  -h, --help, --usage\n");
  PrintOut(LOG_INFO,"        Display this help and exit\n\n");
  PrintOut(LOG_INFO,"  -i N, --interval=N\n");
  PrintOut(LOG_INFO,"        Set interval between disk checks to N seconds, where N >= 10\n\n");
  PrintOut(LOG_INFO,"  -l local[0-7], --logfacility=local[0-7]\n");
#ifndef _WIN32
  PrintOut(LOG_INFO,"        Use syslog facility local0 - local7 or daemon [default]\n\n");
#else
  PrintOut(LOG_INFO,"        Log to \"./smartd.log\", stdout, stderr [default is event log]\n\n");
#endif
#ifndef _WIN32
  PrintOut(LOG_INFO,"  -n, --no-fork\n");
  PrintOut(LOG_INFO,"        Do not fork into background\n\n");
#endif  // _WIN32
  PrintOut(LOG_INFO,"  -p NAME, --pidfile=NAME\n");
  PrintOut(LOG_INFO,"        Write PID file NAME\n\n");
  PrintOut(LOG_INFO,"  -q WHEN, --quit=WHEN\n");
  PrintOut(LOG_INFO,"        Quit on one of: %s\n\n", GetValidArgList('q'));
  PrintOut(LOG_INFO,"  -r, --report=TYPE\n");
  PrintOut(LOG_INFO,"        Report transactions for one of: %s\n\n", GetValidArgList('r'));
  PrintOut(LOG_INFO,"  -s PREFIX, --savestates=PREFIX\n");
  PrintOut(LOG_INFO,"        Save disk states to {PREFIX}MODEL-SERIAL.TYPE.state\n");
#ifdef SMARTMONTOOLS_SAVESTATES
  PrintOut(LOG_INFO,"        [default is " SMARTMONTOOLS_SAVESTATES "MODEL-SERIAL.TYPE.state]\n");
#endif
  PrintOut(LOG_INFO,"\n");
  PrintOut(LOG_INFO,"  -w NAME, --warnexec=NAME\n");
  PrintOut(LOG_INFO,"        Run executable NAME on warnings\n");
#ifndef _WIN32
  PrintOut(LOG_INFO,"        [default is " SMARTMONTOOLS_SMARTDSCRIPTDIR "/smartd_warning.sh]\n\n");
#else
  PrintOut(LOG_INFO,"        [default is %s/smartd_warning.cmd]\n\n", get_exe_dir().c_str());
#endif
#ifdef _WIN32
  PrintOut(LOG_INFO,"  --service\n");
  PrintOut(LOG_INFO,"        Running as windows service (see man page), install with:\n");
  PrintOut(LOG_INFO,"          smartd install [options]\n");
  PrintOut(LOG_INFO,"        Remove service with:\n");
  PrintOut(LOG_INFO,"          smartd remove\n\n");
#endif // _WIN32
  PrintOut(LOG_INFO,"  -V, --version, --license, --copyright\n");
  PrintOut(LOG_INFO,"        Print License, Copyright, and version information\n");
}

static int CloseDevice(smart_device * device, const char * name)
{
  if (!device->close()){
    PrintOut(LOG_INFO,"Device: %s, %s, close() failed\n", name, device->get_errmsg());
    return 1;
  }
  // device sucessfully closed
  return 0;
}

// return true if a char is not allowed in a state file name
static bool not_allowed_in_filename(char c)
{
  return !(   ('0' <= c && c <= '9')
           || ('A' <= c && c <= 'Z')
           || ('a' <= c && c <= 'z'));
}

// Read error count from Summary or Extended Comprehensive SMART error log
// Return -1 on error
static int read_ata_error_count(ata_device * device, const char * name,
                                firmwarebug_defs firmwarebugs, bool extended)
{
  if (!extended) {
    ata_smart_errorlog log;
    if (ataReadErrorLog(device, &log, firmwarebugs)){
      PrintOut(LOG_INFO,"Device: %s, Read Summary SMART Error Log failed\n",name);
      return -1;
    }
    return (log.error_log_pointer ? log.ata_error_count : 0);
  }
  else {
    ata_smart_exterrlog logx;
    if (!ataReadExtErrorLog(device, &logx, 1 /*first sector only*/, firmwarebugs)) {
      PrintOut(LOG_INFO,"Device: %s, Read Extended Comprehensive SMART Error Log failed\n",name);
      return -1;
    }
    // Some disks use the reserved byte as index, see ataprint.cpp.
    return (logx.error_log_index || logx.reserved1 ? logx.device_error_count : 0);
  }
}

// returns <0 if problem.  Otherwise, bottom 8 bits are the self test
// error count, and top bits are the power-on hours of the last error.
static int SelfTestErrorCount(ata_device * device, const char * name,
                              firmwarebug_defs firmwarebugs)
{
  struct ata_smart_selftestlog log;

  if (ataReadSelfTestLog(device, &log, firmwarebugs)){
    PrintOut(LOG_INFO,"Device: %s, Read SMART Self Test Log Failed\n",name);
    return -1;
  }
  
  // return current number of self-test errors
  return ataPrintSmartSelfTestlog(&log, false, firmwarebugs);
}

#define SELFTEST_ERRORCOUNT(x) (x & 0xff)
#define SELFTEST_ERRORHOURS(x) ((x >> 8) & 0xffff)

// Check offline data collection status
static inline bool is_offl_coll_in_progress(unsigned char status)
{
  return ((status & 0x7f) == 0x03);
}

// Check self-test execution status
static inline bool is_self_test_in_progress(unsigned char status)
{
  return ((status >> 4) == 0xf);
}

// Log offline data collection status
static void log_offline_data_coll_status(const char * name, unsigned char status)
{
  const char * msg;
  switch (status & 0x7f) {
    case 0x00: msg = "was never started"; break;
    case 0x02: msg = "was completed without error"; break;
    case 0x03: msg = "is in progress"; break;
    case 0x04: msg = "was suspended by an interrupting command from host"; break;
    case 0x05: msg = "was aborted by an interrupting command from host"; break;
    case 0x06: msg = "was aborted by the device with a fatal error"; break;
    default:   msg = 0;
  }

  if (msg)
    PrintOut(((status & 0x7f) == 0x06 ? LOG_CRIT : LOG_INFO),
             "Device: %s, offline data collection %s%s\n", name, msg,
             ((status & 0x80) ? " (auto:on)" : ""));
  else
    PrintOut(LOG_INFO, "Device: %s, unknown offline data collection status 0x%02x\n",
             name, status);
}

// Log self-test execution status
static void log_self_test_exec_status(const char * name, unsigned char status)
{
  const char * msg;
  switch (status >> 4) {
    case 0x0: msg = "completed without error"; break;
    case 0x1: msg = "was aborted by the host"; break;
    case 0x2: msg = "was interrupted by the host with a reset"; break;
    case 0x3: msg = "could not complete due to a fatal or unknown error"; break;
    case 0x4: msg = "completed with error (unknown test element)"; break;
    case 0x5: msg = "completed with error (electrical test element)"; break;
    case 0x6: msg = "completed with error (servo/seek test element)"; break;
    case 0x7: msg = "completed with error (read test element)"; break;
    case 0x8: msg = "completed with error (handling damage?)"; break;
    default:  msg = 0;
  }

  if (msg)
    PrintOut(((status >> 4) >= 0x4 ? LOG_CRIT : LOG_INFO),
             "Device: %s, previous self-test %s\n", name, msg);
  else if ((status >> 4) == 0xf)
    PrintOut(LOG_INFO, "Device: %s, self-test in progress, %u0%% remaining\n",
             name, status & 0x0f);
  else
    PrintOut(LOG_INFO, "Device: %s, unknown self-test status 0x%02x\n",
             name, status);
}

// Check pending sector count id (-C, -U directives).
static bool check_pending_id(const dev_config & cfg, const dev_state & state,
                             unsigned char id, const char * msg)
{
  // Check attribute index
  int i = ata_find_attr_index(id, state.smartval);
  if (i < 0) {
    PrintOut(LOG_INFO, "Device: %s, can't monitor %s count - no Attribute %d\n",
             cfg.name.c_str(), msg, id);
    return false;
  }

  // Check value
  uint64_t rawval = ata_get_attr_raw_value(state.smartval.vendor_attributes[i],
    cfg.attribute_defs);
  if (rawval >= (state.num_sectors ? state.num_sectors : 0xffffffffULL)) {
    PrintOut(LOG_INFO, "Device: %s, ignoring %s count - bogus Attribute %d value %" PRIu64 " (0x%" PRIx64 ")\n",
             cfg.name.c_str(), msg, id, rawval, rawval);
    return false;
  }

  return true;
}

// Called by ATA/SCSIDeviceScan() after successful device check
static void finish_device_scan(dev_config & cfg, dev_state & state)
{
  // Set cfg.emailfreq if user hasn't set it
  if ((!cfg.emailaddress.empty() || !cfg.emailcmdline.empty()) && !cfg.emailfreq) {
    // Avoid that emails are suppressed forever due to state persistence
    if (cfg.state_file.empty())
      cfg.emailfreq = 1; // '-M once'
    else
      cfg.emailfreq = 2; // '-M daily'
  }

  // Start self-test regex check now if time was not read from state file
  if (!cfg.test_regex.empty() && !state.scheduled_test_next_check)
    state.scheduled_test_next_check = time(0);
}

// Common function to format result message for ATA setting
static void format_set_result_msg(std::string & msg, const char * name, bool ok,
                                  int set_option = 0, bool has_value = false)
{
  if (!msg.empty())
    msg += ", ";
  msg += name;
  if (!ok)
    msg += ":--";
  else if (set_option < 0)
    msg += ":off";
  else if (has_value)
    msg += strprintf(":%d", set_option-1);
  else if (set_option > 0)
    msg += ":on";
}


// TODO: Add '-F swapid' directive
const bool fix_swapped_id = false;

// scan to see what ata devices there are, and if they support SMART
static int ATADeviceScan(dev_config & cfg, dev_state & state, ata_device * atadev)
{
  int supported=0;
  struct ata_identify_device drive;
  const char *name = cfg.name.c_str();
  int retid;

  // Device must be open

  // Get drive identity structure
  if ((retid = ata_read_identity(atadev, &drive, fix_swapped_id))) {
    if (retid<0)
      // Unable to read Identity structure
      PrintOut(LOG_INFO,"Device: %s, not ATA, no IDENTIFY DEVICE Structure\n",name);
    else
      PrintOut(LOG_INFO,"Device: %s, packet devices [this device %s] not SMART capable\n",
               name, packetdevicetype(retid-1));
    CloseDevice(atadev, name);
    return 2; 
  }

  // Get drive identity, size and rotation rate (HDD/SSD)
  char model[40+1], serial[20+1], firmware[8+1];
  ata_format_id_string(model, drive.model, sizeof(model)-1);
  ata_format_id_string(serial, drive.serial_no, sizeof(serial)-1);
  ata_format_id_string(firmware, drive.fw_rev, sizeof(firmware)-1);

  ata_size_info sizes;
  ata_get_size_info(&drive, sizes);
  state.num_sectors = sizes.sectors;
  cfg.dev_rpm = ata_get_rotation_rate(&drive);

  char wwn[30]; wwn[0] = 0;
  unsigned oui = 0; uint64_t unique_id = 0;
  int naa = ata_get_wwn(&drive, oui, unique_id);
  if (naa >= 0)
    snprintf(wwn, sizeof(wwn), "WWN:%x-%06x-%09" PRIx64 ", ", naa, oui, unique_id);

  // Format device id string for warning emails
  char cap[32];
  cfg.dev_idinfo = strprintf("%s, S/N:%s, %sFW:%s, %s", model, serial, wwn, firmware,
                     format_capacity(cap, sizeof(cap), sizes.capacity, "."));

  PrintOut(LOG_INFO, "Device: %s, %s\n", name, cfg.dev_idinfo.c_str());

  // Show if device in database, and use preset vendor attribute
  // options unless user has requested otherwise.
  if (cfg.ignorepresets)
    PrintOut(LOG_INFO, "Device: %s, smartd database not searched (Directive: -P ignore).\n", name);
  else {
    // Apply vendor specific presets, print warning if present
    const drive_settings * dbentry = lookup_drive_apply_presets(
      &drive, cfg.attribute_defs, cfg.firmwarebugs);
    if (!dbentry)
      PrintOut(LOG_INFO, "Device: %s, not found in smartd database.\n", name);
    else {
      PrintOut(LOG_INFO, "Device: %s, found in smartd database%s%s\n",
        name, (*dbentry->modelfamily ? ": " : "."), (*dbentry->modelfamily ? dbentry->modelfamily : ""));
      if (*dbentry->warningmsg)
        PrintOut(LOG_CRIT, "Device: %s, WARNING: %s\n", name, dbentry->warningmsg);
    }
  }

  // Set default '-C 197[+]' if no '-C ID' is specified.
  if (!cfg.curr_pending_set)
    cfg.curr_pending_id = get_unc_attr_id(false, cfg.attribute_defs, cfg.curr_pending_incr);
  // Set default '-U 198[+]' if no '-U ID' is specified.
  if (!cfg.offl_pending_set)
    cfg.offl_pending_id = get_unc_attr_id(true, cfg.attribute_defs, cfg.offl_pending_incr);

  // If requested, show which presets would be used for this drive
  if (cfg.showpresets) {
    int savedebugmode=debugmode;
    PrintOut(LOG_INFO, "Device %s: presets are:\n", name);
    if (!debugmode)
      debugmode=2;
    show_presets(&drive);
    debugmode=savedebugmode;
  }

  // see if drive supports SMART
  supported=ataSmartSupport(&drive);
  if (supported!=1) {
    if (supported==0)
      // drive does NOT support SMART
      PrintOut(LOG_INFO,"Device: %s, lacks SMART capability\n",name);
    else
      // can't tell if drive supports SMART
      PrintOut(LOG_INFO,"Device: %s, ATA IDENTIFY DEVICE words 82-83 don't specify if SMART capable.\n",name);
  
    // should we proceed anyway?
    if (cfg.permissive) {
      PrintOut(LOG_INFO,"Device: %s, proceeding since '-T permissive' Directive given.\n",name);
    }
    else {
      PrintOut(LOG_INFO,"Device: %s, to proceed anyway, use '-T permissive' Directive.\n",name);
      CloseDevice(atadev, name);
      return 2;
    }
  }
  
  if (ataEnableSmart(atadev)) {
    // Enable SMART command has failed
    PrintOut(LOG_INFO,"Device: %s, could not enable SMART capability\n",name);
    CloseDevice(atadev, name);
    return 2; 
  }
  
  // disable device attribute autosave...
  if (cfg.autosave==1) {
    if (ataDisableAutoSave(atadev))
      PrintOut(LOG_INFO,"Device: %s, could not disable SMART Attribute Autosave.\n",name);
    else
      PrintOut(LOG_INFO,"Device: %s, disabled SMART Attribute Autosave.\n",name);
  }

  // or enable device attribute autosave
  if (cfg.autosave==2) {
    if (ataEnableAutoSave(atadev))
      PrintOut(LOG_INFO,"Device: %s, could not enable SMART Attribute Autosave.\n",name);
    else
      PrintOut(LOG_INFO,"Device: %s, enabled SMART Attribute Autosave.\n",name);
  }

  // capability check: SMART status
  if (cfg.smartcheck && ataSmartStatus2(atadev) == -1) {
    PrintOut(LOG_INFO,"Device: %s, not capable of SMART Health Status check\n",name);
    cfg.smartcheck = false;
  }
  
  // capability check: Read smart values and thresholds.  Note that
  // smart values are ALSO needed even if we ONLY want to know if the
  // device is self-test log or error-log capable!  After ATA-5, this
  // information was ALSO reproduced in the IDENTIFY DEVICE response,
  // but sadly not for ATA-5.  Sigh.

  // do we need to get SMART data?
  bool smart_val_ok = false;
  if (   cfg.autoofflinetest || cfg.selftest
      || cfg.errorlog        || cfg.xerrorlog
      || cfg.offlinests      || cfg.selfteststs
      || cfg.usagefailed     || cfg.prefail  || cfg.usage
      || cfg.tempdiff        || cfg.tempinfo || cfg.tempcrit
      || cfg.curr_pending_id || cfg.offl_pending_id         ) {

    if (ataReadSmartValues(atadev, &state.smartval)) {
      PrintOut(LOG_INFO, "Device: %s, Read SMART Values failed\n", name);
      cfg.usagefailed = cfg.prefail = cfg.usage = false;
      cfg.tempdiff = cfg.tempinfo = cfg.tempcrit = 0;
      cfg.curr_pending_id = cfg.offl_pending_id = 0;
    }
    else {
      smart_val_ok = true;
      if (ataReadSmartThresholds(atadev, &state.smartthres)) {
        PrintOut(LOG_INFO, "Device: %s, Read SMART Thresholds failed%s\n",
                 name, (cfg.usagefailed ? ", ignoring -f Directive" : ""));
        cfg.usagefailed = false;
        // Let ata_get_attr_state() return ATTRSTATE_NO_THRESHOLD:
        memset(&state.smartthres, 0, sizeof(state.smartthres));
      }
    }

    // see if the necessary Attribute is there to monitor offline or
    // current pending sectors or temperature
    if (   cfg.curr_pending_id
        && !check_pending_id(cfg, state, cfg.curr_pending_id,
              "Current_Pending_Sector"))
      cfg.curr_pending_id = 0;

    if (   cfg.offl_pending_id
        && !check_pending_id(cfg, state, cfg.offl_pending_id,
              "Offline_Uncorrectable"))
      cfg.offl_pending_id = 0;

    if (   (cfg.tempdiff || cfg.tempinfo || cfg.tempcrit)
        && !ata_return_temperature_value(&state.smartval, cfg.attribute_defs)) {
      PrintOut(LOG_INFO, "Device: %s, can't monitor Temperature, ignoring -W %d,%d,%d\n",
               name, cfg.tempdiff, cfg.tempinfo, cfg.tempcrit);
      cfg.tempdiff = cfg.tempinfo = cfg.tempcrit = 0;
    }

    // Report ignored '-r' or '-R' directives
    for (int id = 1; id <= 255; id++) {
      if (cfg.monitor_attr_flags.is_set(id, MONITOR_RAW_PRINT)) {
        char opt = (!cfg.monitor_attr_flags.is_set(id, MONITOR_RAW) ? 'r' : 'R');
        const char * excl = (cfg.monitor_attr_flags.is_set(id,
          (opt == 'r' ? MONITOR_AS_CRIT : MONITOR_RAW_AS_CRIT)) ? "!" : "");

        int idx = ata_find_attr_index(id, state.smartval);
        if (idx < 0)
          PrintOut(LOG_INFO,"Device: %s, no Attribute %d, ignoring -%c %d%s\n", name, id, opt, id, excl);
        else {
          bool prefail = !!ATTRIBUTE_FLAGS_PREFAILURE(state.smartval.vendor_attributes[idx].flags);
          if (!((prefail && cfg.prefail) || (!prefail && cfg.usage)))
            PrintOut(LOG_INFO,"Device: %s, not monitoring %s Attributes, ignoring -%c %d%s\n", name,
                     (prefail ? "Prefailure" : "Usage"), opt, id, excl);
        }
      }
    }
  }
  
  // enable/disable automatic on-line testing
  if (cfg.autoofflinetest) {
    // is this an enable or disable request?
    const char *what=(cfg.autoofflinetest==1)?"disable":"enable";
    if (!smart_val_ok)
      PrintOut(LOG_INFO,"Device: %s, could not %s SMART Automatic Offline Testing.\n",name, what);
    else {
      // if command appears unsupported, issue a warning...
      if (!isSupportAutomaticTimer(&state.smartval))
        PrintOut(LOG_INFO,"Device: %s, SMART Automatic Offline Testing unsupported...\n",name);
      // ... but then try anyway
      if ((cfg.autoofflinetest==1)?ataDisableAutoOffline(atadev):ataEnableAutoOffline(atadev))
        PrintOut(LOG_INFO,"Device: %s, %s SMART Automatic Offline Testing failed.\n", name, what);
      else
        PrintOut(LOG_INFO,"Device: %s, %sd SMART Automatic Offline Testing.\n", name, what);
    }
  }

  // Read log directories if required for capability check
  ata_smart_log_directory smart_logdir, gp_logdir;
  bool smart_logdir_ok = false, gp_logdir_ok = false;

  if (   isGeneralPurposeLoggingCapable(&drive)
      && (cfg.errorlog || cfg.selftest)
      && !cfg.firmwarebugs.is_set(BUG_NOLOGDIR)) {
      if (!ataReadLogDirectory(atadev, &smart_logdir, false))
        smart_logdir_ok = true;
  }

  if (cfg.xerrorlog && !cfg.firmwarebugs.is_set(BUG_NOLOGDIR)) {
    if (!ataReadLogDirectory(atadev, &gp_logdir, true))
      gp_logdir_ok = true;
  }

  // capability check: self-test-log
  state.selflogcount = 0; state.selfloghour = 0;
  if (cfg.selftest) {
    int retval;
    if (!(   cfg.permissive
          || ( smart_logdir_ok && smart_logdir.entry[0x06-1].numsectors)
          || (!smart_logdir_ok && smart_val_ok && isSmartTestLogCapable(&state.smartval, &drive)))) {
      PrintOut(LOG_INFO, "Device: %s, no SMART Self-test Log, ignoring -l selftest (override with -T permissive)\n", name);
      cfg.selftest = false;
    }
    else if ((retval = SelfTestErrorCount(atadev, name, cfg.firmwarebugs)) < 0) {
      PrintOut(LOG_INFO, "Device: %s, no SMART Self-test Log, ignoring -l selftest\n", name);
      cfg.selftest = false;
    }
    else {
      state.selflogcount=SELFTEST_ERRORCOUNT(retval);
      state.selfloghour =SELFTEST_ERRORHOURS(retval);
    }
  }
  
  // capability check: ATA error log
  state.ataerrorcount = 0;
  if (cfg.errorlog) {
    int errcnt1;
    if (!(   cfg.permissive
          || ( smart_logdir_ok && smart_logdir.entry[0x01-1].numsectors)
          || (!smart_logdir_ok && smart_val_ok && isSmartErrorLogCapable(&state.smartval, &drive)))) {
      PrintOut(LOG_INFO, "Device: %s, no SMART Error Log, ignoring -l error (override with -T permissive)\n", name);
      cfg.errorlog = false;
    }
    else if ((errcnt1 = read_ata_error_count(atadev, name, cfg.firmwarebugs, false)) < 0) {
      PrintOut(LOG_INFO, "Device: %s, no SMART Error Log, ignoring -l error\n", name);
      cfg.errorlog = false;
    }
    else
      state.ataerrorcount = errcnt1;
  }

  if (cfg.xerrorlog) {
    int errcnt2;
    if (!(   cfg.permissive || cfg.firmwarebugs.is_set(BUG_NOLOGDIR)
          || (gp_logdir_ok && gp_logdir.entry[0x03-1].numsectors)   )) {
      PrintOut(LOG_INFO, "Device: %s, no Extended Comprehensive SMART Error Log, ignoring -l xerror (override with -T permissive)\n",
               name);
      cfg.xerrorlog = false;
    }
    else if ((errcnt2 = read_ata_error_count(atadev, name, cfg.firmwarebugs, true)) < 0) {
      PrintOut(LOG_INFO, "Device: %s, no Extended Comprehensive SMART Error Log, ignoring -l xerror\n", name);
      cfg.xerrorlog = false;
    }
    else if (cfg.errorlog && state.ataerrorcount != errcnt2) {
      PrintOut(LOG_INFO, "Device: %s, SMART Error Logs report different error counts: %d != %d\n",
               name, state.ataerrorcount, errcnt2);
      // Record max error count
      if (errcnt2 > state.ataerrorcount)
        state.ataerrorcount = errcnt2;
    }
    else
      state.ataerrorcount = errcnt2;
  }

  // capability check: self-test and offline data collection status
  if (cfg.offlinests || cfg.selfteststs) {
    if (!(cfg.permissive || (smart_val_ok && state.smartval.offline_data_collection_capability))) {
      if (cfg.offlinests)
        PrintOut(LOG_INFO, "Device: %s, no SMART Offline Data Collection capability, ignoring -l offlinests (override with -T permissive)\n", name);
      if (cfg.selfteststs)
        PrintOut(LOG_INFO, "Device: %s, no SMART Self-test capability, ignoring -l selfteststs (override with -T permissive)\n", name);
      cfg.offlinests = cfg.selfteststs = false;
    }
  }

  // capabilities check -- does it support powermode?
  if (cfg.powermode) {
    int powermode = ataCheckPowerMode(atadev);
    
    if (-1 == powermode) {
      PrintOut(LOG_CRIT, "Device: %s, no ATA CHECK POWER STATUS support, ignoring -n Directive\n", name);
      cfg.powermode=0;
    } 
    else if (powermode!=0 && powermode!=0x80 && powermode!=0xff) {
      PrintOut(LOG_CRIT, "Device: %s, CHECK POWER STATUS returned %d, not ATA compliant, ignoring -n Directive\n",
	       name, powermode);
      cfg.powermode=0;
    }
  }

  // Apply ATA settings
  std::string msg;

  if (cfg.set_aam)
    format_set_result_msg(msg, "AAM", (cfg.set_aam > 0 ?
      ata_set_features(atadev, ATA_ENABLE_AAM, cfg.set_aam-1) :
      ata_set_features(atadev, ATA_DISABLE_AAM)), cfg.set_aam, true);

  if (cfg.set_apm)
    format_set_result_msg(msg, "APM", (cfg.set_apm > 0 ?
      ata_set_features(atadev, ATA_ENABLE_APM, cfg.set_apm-1) :
      ata_set_features(atadev, ATA_DISABLE_APM)), cfg.set_apm, true);

  if (cfg.set_lookahead)
    format_set_result_msg(msg, "Rd-ahead", ata_set_features(atadev,
      (cfg.set_lookahead > 0 ? ATA_ENABLE_READ_LOOK_AHEAD : ATA_DISABLE_READ_LOOK_AHEAD)),
      cfg.set_lookahead);

  if (cfg.set_wcache)
    format_set_result_msg(msg, "Wr-cache", ata_set_features(atadev,
      (cfg.set_wcache > 0? ATA_ENABLE_WRITE_CACHE : ATA_DISABLE_WRITE_CACHE)), cfg.set_wcache);

  if (cfg.set_security_freeze)
    format_set_result_msg(msg, "Security freeze",
      ata_nodata_command(atadev, ATA_SECURITY_FREEZE_LOCK));

  if (cfg.set_standby)
    format_set_result_msg(msg, "Standby",
      ata_nodata_command(atadev, ATA_IDLE, cfg.set_standby-1), cfg.set_standby, true);

  // Report as one log entry
  if (!msg.empty())
    PrintOut(LOG_INFO, "Device: %s, ATA settings applied: %s\n", name, msg.c_str());

  // set SCT Error Recovery Control if requested
  if (cfg.sct_erc_set) {
    if (!isSCTErrorRecoveryControlCapable(&drive))
      PrintOut(LOG_INFO, "Device: %s, no SCT Error Recovery Control support, ignoring -l scterc\n",
               name);
    else if (   ataSetSCTErrorRecoveryControltime(atadev, 1, cfg.sct_erc_readtime )
             || ataSetSCTErrorRecoveryControltime(atadev, 2, cfg.sct_erc_writetime))
      PrintOut(LOG_INFO, "Device: %s, set of SCT Error Recovery Control failed\n", name);
    else
      PrintOut(LOG_INFO, "Device: %s, SCT Error Recovery Control set to: Read: %u, Write: %u\n",
               name, cfg.sct_erc_readtime, cfg.sct_erc_writetime);
  }

  // If no tests available or selected, return
  if (!(   cfg.smartcheck  || cfg.selftest
        || cfg.errorlog    || cfg.xerrorlog
        || cfg.offlinests  || cfg.selfteststs
        || cfg.usagefailed || cfg.prefail  || cfg.usage
        || cfg.tempdiff    || cfg.tempinfo || cfg.tempcrit)) {
    CloseDevice(atadev, name);
    return 3;
  }
  
  // tell user we are registering device
  PrintOut(LOG_INFO,"Device: %s, is SMART capable. Adding to \"monitor\" list.\n",name);
  
  // close file descriptor
  CloseDevice(atadev, name);

  if (!state_path_prefix.empty() || !attrlog_path_prefix.empty()) {
    // Build file name for state file
    std::replace_if(model, model+strlen(model), not_allowed_in_filename, '_');
    std::replace_if(serial, serial+strlen(serial), not_allowed_in_filename, '_');
    if (!state_path_prefix.empty()) {
      cfg.state_file = strprintf("%s%s-%s.ata.state", state_path_prefix.c_str(), model, serial);
      // Read previous state
      if (read_dev_state(cfg.state_file.c_str(), state)) {
        PrintOut(LOG_INFO, "Device: %s, state read from %s\n", name, cfg.state_file.c_str());
        // Copy ATA attribute values to temp state
        state.update_temp_state();
      }
    }
    if (!attrlog_path_prefix.empty())
      cfg.attrlog_file = strprintf("%s%s-%s.ata.csv", attrlog_path_prefix.c_str(), model, serial);
  }

  finish_device_scan(cfg, state);

  return 0;
}

// on success, return 0. On failure, return >0.  Never return <0,
// please.
static int SCSIDeviceScan(dev_config & cfg, dev_state & state, scsi_device * scsidev)
{
  int k, err, req_len, avail_len, version, len;
  const char *device = cfg.name.c_str();
  struct scsi_iec_mode_page iec;
  UINT8  tBuf[64];
  UINT8  inqBuf[96];
  UINT8  vpdBuf[252];
  char lu_id[64], serial[256], vendor[40], model[40];

  // Device must be open
  memset(inqBuf, 0, 96);
  req_len = 36;
  if ((err = scsiStdInquiry(scsidev, inqBuf, req_len))) {
    /* Marvell controllers fail on a 36 bytes StdInquiry, but 64 suffices */
    req_len = 64;
    if ((err = scsiStdInquiry(scsidev, inqBuf, req_len))) {
      PrintOut(LOG_INFO, "Device: %s, Both 36 and 64 byte INQUIRY failed; "
	       "skip device\n", device);
      return 2;
    }
  }
  version = (inqBuf[2] & 0x7f);	/* Accept old ISO/IEC 9316:1995 variants */

  avail_len = inqBuf[4] + 5;
  len = (avail_len < req_len) ? avail_len : req_len;
  if (len < 36) {
    PrintOut(LOG_INFO, "Device: %s, INQUIRY response less than 36 bytes; "
	     "skip device\n", device);
    return 2;
  }

  int pdt = inqBuf[0] & 0x1f;

  if (! ((0 == pdt) || (4 == pdt) || (5 == pdt) || (7 == pdt) ||
         (0xe == pdt))) {
    PrintOut(LOG_INFO, "Device: %s, not a disk like device [PDT=0x%x], "
             "skip\n", device, pdt);
    return 2;
  }

  if (supported_vpd_pages_p) {
    delete supported_vpd_pages_p;
    supported_vpd_pages_p = NULL;
  }
  supported_vpd_pages_p = new supported_vpd_pages(scsidev);

  lu_id[0] = '\0';
  if ((version >= 0x3) && (version < 0x8)) {
    /* SPC to SPC-5 */
    if (0 == scsiInquiryVpd(scsidev, SCSI_VPD_DEVICE_IDENTIFICATION,
			    vpdBuf, sizeof(vpdBuf))) {
      len = vpdBuf[3];
      scsi_decode_lu_dev_id(vpdBuf + 4, len, lu_id, sizeof(lu_id), NULL);
    }
  }
  serial[0] = '\0';
  if (0 == scsiInquiryVpd(scsidev, SCSI_VPD_UNIT_SERIAL_NUMBER,
			  vpdBuf, sizeof(vpdBuf))) {
  	  len = vpdBuf[3];
  	  vpdBuf[4 + len] = '\0';
  	  scsi_format_id_string(serial, (const unsigned char *)&vpdBuf[4], len);
  }

  unsigned int lb_size;
  char si_str[64];
  uint64_t capacity = scsiGetSize(scsidev, &lb_size, NULL);

  if (capacity)
    format_capacity(si_str, sizeof(si_str), capacity);
  else
    si_str[0] = '\0';

  // Format device id string for warning emails
  cfg.dev_idinfo = strprintf("[%.8s %.16s %.4s]%s%s%s%s%s%s",
                     (char *)&inqBuf[8], (char *)&inqBuf[16], (char *)&inqBuf[32],
                     (lu_id[0] ? ", lu id: " : ""), (lu_id[0] ? lu_id : ""),
                     (serial[0] ? ", S/N: " : ""), (serial[0] ? serial : ""),
                     (si_str[0] ? ", " : ""), (si_str[0] ? si_str : ""));
  
  // format "model" string
  scsi_format_id_string(vendor, (const unsigned char *)&inqBuf[8], 8);
  scsi_format_id_string(model, (const unsigned char *)&inqBuf[16], 16);
  PrintOut(LOG_INFO, "Device: %s, %s\n", device, cfg.dev_idinfo.c_str());

  // check that device is ready for commands. IE stores its stuff on
  // the media.
  if ((err = scsiTestUnitReady(scsidev))) {
    if (SIMPLE_ERR_NOT_READY == err)
      PrintOut(LOG_INFO, "Device: %s, NOT READY (e.g. spun down); skip device\n", device);
    else if (SIMPLE_ERR_NO_MEDIUM == err)
      PrintOut(LOG_INFO, "Device: %s, NO MEDIUM present; skip device\n", device);
    else if (SIMPLE_ERR_BECOMING_READY == err)
      PrintOut(LOG_INFO, "Device: %s, BECOMING (but not yet) READY; skip device\n", device);
    else
      PrintOut(LOG_CRIT, "Device: %s, failed Test Unit Ready [err=%d]\n", device, err);
    CloseDevice(scsidev, device);
    return 2; 
  }
  
  // Badly-conforming USB storage devices may fail this check.
  // The response to the following IE mode page fetch (current and
  // changeable values) is carefully examined. It has been found
  // that various USB devices that malform the response will lock up
  // if asked for a log page (e.g. temperature) so it is best to
  // bail out now.
  if (!(err = scsiFetchIECmpage(scsidev, &iec, state.modese_len)))
    state.modese_len = iec.modese_len;
  else if (SIMPLE_ERR_BAD_FIELD == err)
    ;  /* continue since it is reasonable not to support IE mpage */
  else { /* any other error (including malformed response) unreasonable */
    PrintOut(LOG_INFO, 
             "Device: %s, Bad IEC (SMART) mode page, err=%d, skip device\n", 
             device, err);
    CloseDevice(scsidev, device);
    return 3;
  }
  
  // N.B. The following is passive (i.e. it doesn't attempt to turn on
  // smart if it is off). This may change to be the same as the ATA side.
  if (!scsi_IsExceptionControlEnabled(&iec)) {
    PrintOut(LOG_INFO, "Device: %s, IE (SMART) not enabled, skip device\n"
	               "Try 'smartctl -s on %s' to turn on SMART features\n", 
                        device, device);
    CloseDevice(scsidev, device);
    return 3;
  }
  
  // Flag that certain log pages are supported (information may be
  // available from other sources).
  if (0 == scsiLogSense(scsidev, SUPPORTED_LPAGES, 0, tBuf, sizeof(tBuf), 0)) {
    for (k = 4; k < tBuf[3] + LOGPAGEHDRSIZE; ++k) {
      switch (tBuf[k]) { 
      case TEMPERATURE_LPAGE:
        state.TempPageSupported = 1;
        break;
      case IE_LPAGE:
        state.SmartPageSupported = 1;
        break;
      case READ_ERROR_COUNTER_LPAGE:
        state.ReadECounterPageSupported = 1;
        break;
      case WRITE_ERROR_COUNTER_LPAGE:
        state.WriteECounterPageSupported = 1;
        break;
      case VERIFY_ERROR_COUNTER_LPAGE:
        state.VerifyECounterPageSupported = 1;
        break;
      case NON_MEDIUM_ERROR_LPAGE:
        state.NonMediumErrorPageSupported = 1;
        break;
      default:
        break;
      }
    }   
  }
  
  // Check if scsiCheckIE() is going to work
  {
    UINT8 asc = 0;
    UINT8 ascq = 0;
    UINT8 currenttemp = 0;
    UINT8 triptemp = 0;
    
    if (scsiCheckIE(scsidev, state.SmartPageSupported, state.TempPageSupported,
                    &asc, &ascq, &currenttemp, &triptemp)) {
      PrintOut(LOG_INFO, "Device: %s, unexpectedly failed to read SMART values\n", device);
      state.SuppressReport = 1;
      if (cfg.tempdiff || cfg.tempinfo || cfg.tempcrit) {
        PrintOut(LOG_INFO, "Device: %s, can't monitor Temperature, ignoring -W %d,%d,%d\n",
                 device, cfg.tempdiff, cfg.tempinfo, cfg.tempcrit);
        cfg.tempdiff = cfg.tempinfo = cfg.tempcrit = 0;
      }
    }
  }
  
  // capability check: self-test-log
  if (cfg.selftest){
    int retval = scsiCountFailedSelfTests(scsidev, 0);
    if (retval<0) {
      // no self-test log, turn off monitoring
      PrintOut(LOG_INFO, "Device: %s, does not support SMART Self-Test Log.\n", device);
      cfg.selftest = false;
      state.selflogcount = 0;
      state.selfloghour = 0;
    }
    else {
      // register starting values to watch for changes
      state.selflogcount=SELFTEST_ERRORCOUNT(retval);
      state.selfloghour =SELFTEST_ERRORHOURS(retval);
    }
  }
  
  // disable autosave (set GLTSD bit)
  if (cfg.autosave==1){
    if (scsiSetControlGLTSD(scsidev, 1, state.modese_len))
      PrintOut(LOG_INFO,"Device: %s, could not disable autosave (set GLTSD bit).\n",device);
    else
      PrintOut(LOG_INFO,"Device: %s, disabled autosave (set GLTSD bit).\n",device);
  }

  // or enable autosave (clear GLTSD bit)
  if (cfg.autosave==2){
    if (scsiSetControlGLTSD(scsidev, 0, state.modese_len))
      PrintOut(LOG_INFO,"Device: %s, could not enable autosave (clear GLTSD bit).\n",device);
    else
      PrintOut(LOG_INFO,"Device: %s, enabled autosave (cleared GLTSD bit).\n",device);
  }
  
  // tell user we are registering device
  PrintOut(LOG_INFO, "Device: %s, is SMART capable. Adding to \"monitor\" list.\n", device);

  // Make sure that init_standby_check() ignores SCSI devices
  cfg.offlinests_ns = cfg.selfteststs_ns = false;

  // close file descriptor
  CloseDevice(scsidev, device);

  if (!state_path_prefix.empty() || !attrlog_path_prefix.empty()) {
    // Build file name for state file
    std::replace_if(model, model+strlen(model), not_allowed_in_filename, '_');
    std::replace_if(serial, serial+strlen(serial), not_allowed_in_filename, '_');
    if (!state_path_prefix.empty()) {
      cfg.state_file = strprintf("%s%s-%s-%s.scsi.state", state_path_prefix.c_str(), vendor, model, serial);
      // Read previous state
      if (read_dev_state(cfg.state_file.c_str(), state)) {
        PrintOut(LOG_INFO, "Device: %s, state read from %s\n", device, cfg.state_file.c_str());
        // Copy ATA attribute values to temp state
        state.update_temp_state();
      }
    }
    if (!attrlog_path_prefix.empty())
      cfg.attrlog_file = strprintf("%s%s-%s-%s.scsi.csv", attrlog_path_prefix.c_str(), vendor, model, serial);
  }

  finish_device_scan(cfg, state);

  return 0;
}

// If the self-test log has got more self-test errors (or more recent
// self-test errors) recorded, then notify user.
static void CheckSelfTestLogs(const dev_config & cfg, dev_state & state, int newi)
{
  const char * name = cfg.name.c_str();

  if (newi<0)
    // command failed
    MailWarning(cfg, state, 8, "Device: %s, Read SMART Self-Test Log Failed", name);
  else {
    reset_warning_mail(cfg, state, 8, "Read SMART Self-Test Log worked again");

    // old and new error counts
    int oldc=state.selflogcount;
    int newc=SELFTEST_ERRORCOUNT(newi);
    
    // old and new error timestamps in hours
    int oldh=state.selfloghour;
    int newh=SELFTEST_ERRORHOURS(newi);
    
    if (oldc<newc) {
      // increase in error count
      PrintOut(LOG_CRIT, "Device: %s, Self-Test Log error count increased from %d to %d\n",
               name, oldc, newc);
      MailWarning(cfg, state, 3, "Device: %s, Self-Test Log error count increased from %d to %d",
                   name, oldc, newc);
      state.must_write = true;
    }
    else if (newc > 0 && oldh != newh) {
      // more recent error
      // a 'more recent' error might actually be a smaller hour number,
      // if the hour number has wrapped.
      // There's still a bug here.  You might just happen to run a new test
      // exactly 32768 hours after the previous failure, and have run exactly
      // 20 tests between the two, in which case smartd will miss the
      // new failure.
      PrintOut(LOG_CRIT, "Device: %s, new Self-Test Log error at hour timestamp %d\n",
               name, newh);
      MailWarning(cfg, state, 3, "Device: %s, new Self-Test Log error at hour timestamp %d",
                   name, newh);
      state.must_write = true;
    }

    // Print info if error entries have disappeared
    // or newer successful successful extended self-test exits
    if (oldc > newc) {
      PrintOut(LOG_INFO, "Device: %s, Self-Test Log error count decreased from %d to %d\n",
               name, oldc, newc);
      if (newc == 0)
        reset_warning_mail(cfg, state, 3, "Self-Test Log does no longer report errors");
    }

    // Needed since self-test error count may DECREASE.  Hour might
    // also have changed.
    state.selflogcount= newc;
    state.selfloghour = newh;
  }
  return;
}

// Test types, ordered by priority.
static const char test_type_chars[] = "LncrSCO";
static const unsigned num_test_types = sizeof(test_type_chars)-1;

// returns test type if time to do test of type testtype,
// 0 if not time to do test.
static char next_scheduled_test(const dev_config & cfg, dev_state & state, bool scsi, time_t usetime = 0)
{
  // check that self-testing has been requested
  if (cfg.test_regex.empty())
    return 0;

  // Exit if drive not capable of any test
  if ( state.not_cap_long && state.not_cap_short &&
      (scsi || (state.not_cap_conveyance && state.not_cap_offline)))
    return 0;

  // since we are about to call localtime(), be sure glibc is informed
  // of any timezone changes we make.
  if (!usetime)
    FixGlibcTimeZoneBug();
  
  // Is it time for next check?
  time_t now = (!usetime ? time(0) : usetime);
  if (now < state.scheduled_test_next_check)
    return 0;

  // Limit time check interval to 90 days
  if (state.scheduled_test_next_check + (3600L*24*90) < now)
    state.scheduled_test_next_check = now - (3600L*24*90);

  // Check interval [state.scheduled_test_next_check, now] for scheduled tests
  char testtype = 0;
  time_t testtime = 0; int testhour = 0;
  int maxtest = num_test_types-1;

  for (time_t t = state.scheduled_test_next_check; ; ) {
    struct tm * tms = localtime(&t);
    // tm_wday is 0 (Sunday) to 6 (Saturday).  We use 1 (Monday) to 7 (Sunday).
    int weekday = (tms->tm_wday ? tms->tm_wday : 7);
    for (int i = 0; i <= maxtest; i++) {
      // Skip if drive not capable of this test
      switch (test_type_chars[i]) {
        case 'L': if (state.not_cap_long)       continue; break;
        case 'S': if (state.not_cap_short)      continue; break;
        case 'C': if (scsi || state.not_cap_conveyance) continue; break;
        case 'O': if (scsi || state.not_cap_offline)    continue; break;
        case 'c': case 'n':
        case 'r': if (scsi || state.not_cap_selective)  continue; break;
        default: continue;
      }
      // Try match of "T/MM/DD/d/HH"
      char pattern[16];
      snprintf(pattern, sizeof(pattern), "%c/%02d/%02d/%1d/%02d",
        test_type_chars[i], tms->tm_mon+1, tms->tm_mday, weekday, tms->tm_hour);
      if (cfg.test_regex.full_match(pattern)) {
        // Test found
        testtype = pattern[0];
        testtime = t; testhour = tms->tm_hour;
        // Limit further matches to higher priority self-tests
        maxtest = i-1;
        break;
      }
    }
    // Exit if no tests left or current time reached
    if (maxtest < 0)
      break;
    if (t >= now)
      break;
    // Check next hour
    if ((t += 3600) > now)
      t = now;
  }
  
  // Do next check not before next hour.
  struct tm * tmnow = localtime(&now);
  state.scheduled_test_next_check = now + (3600 - tmnow->tm_min*60 - tmnow->tm_sec);

  if (testtype) {
    state.must_write = true;
    // Tell user if an old test was found.
    if (!usetime && !(testhour == tmnow->tm_hour && testtime + 3600 > now)) {
      char datebuf[DATEANDEPOCHLEN]; dateandtimezoneepoch(datebuf, testtime);
      PrintOut(LOG_INFO, "Device: %s, old test of type %c not run at %s, starting now.\n",
        cfg.name.c_str(), testtype, datebuf);
    }
  }

  return testtype;
}

// Print a list of future tests.
static void PrintTestSchedule(const dev_config_vector & configs, dev_state_vector & states, const smart_device_list & devices)
{
  unsigned numdev = configs.size();
  if (!numdev)
    return;
  std::vector<int> testcnts(numdev * num_test_types, 0);

  PrintOut(LOG_INFO, "\nNext scheduled self tests (at most 5 of each type per device):\n");

  // FixGlibcTimeZoneBug(); // done in PrintOut()
  time_t now = time(0);
  char datenow[DATEANDEPOCHLEN], date[DATEANDEPOCHLEN];
  dateandtimezoneepoch(datenow, now);

  long seconds;
  for (seconds=checktime; seconds<3600L*24*90; seconds+=checktime) {
    // Check for each device whether a test will be run
    time_t testtime = now + seconds;
    for (unsigned i = 0; i < numdev; i++) {
      const dev_config & cfg = configs.at(i);
      dev_state & state = states.at(i);
      const char * p;
      char testtype = next_scheduled_test(cfg, state, devices.at(i)->is_scsi(), testtime);
      if (testtype && (p = strchr(test_type_chars, testtype))) {
        unsigned t = (p - test_type_chars);
        // Report at most 5 tests of each type
        if (++testcnts[i*num_test_types + t] <= 5) {
          dateandtimezoneepoch(date, testtime);
          PrintOut(LOG_INFO, "Device: %s, will do test %d of type %c at %s\n", cfg.name.c_str(),
            testcnts[i*num_test_types + t], testtype, date);
        }
      }
    }
  }

  // Report totals
  dateandtimezoneepoch(date, now+seconds);
  PrintOut(LOG_INFO, "\nTotals [%s - %s]:\n", datenow, date);
  for (unsigned i = 0; i < numdev; i++) {
    const dev_config & cfg = configs.at(i);
    bool scsi = devices.at(i)->is_scsi();
    for (unsigned t = 0; t < num_test_types; t++) {
      int cnt = testcnts[i*num_test_types + t];
      if (cnt == 0 && !strchr((scsi ? "LS" : "LSCO"), test_type_chars[t]))
        continue;
      PrintOut(LOG_INFO, "Device: %s, will do %3d test%s of type %c\n", cfg.name.c_str(),
        cnt, (cnt==1?"":"s"), test_type_chars[t]);
    }
  }

}

// Return zero on success, nonzero on failure. Perform offline (background)
// short or long (extended) self test on given scsi device.
static int DoSCSISelfTest(const dev_config & cfg, dev_state & state, scsi_device * device, char testtype)
{
  int retval = 0;
  const char *testname = 0;
  const char *name = cfg.name.c_str();
  int inProgress;

  if (scsiSelfTestInProgress(device, &inProgress)) {
    PrintOut(LOG_CRIT, "Device: %s, does not support Self-Tests\n", name);
    state.not_cap_short = state.not_cap_long = true;
    return 1;
  }

  if (1 == inProgress) {
    PrintOut(LOG_INFO, "Device: %s, skip since Self-Test already in "
             "progress.\n", name);
    return 1;
  }

  switch (testtype) {
  case 'S':
    testname = "Short Self";
    retval = scsiSmartShortSelfTest(device);
    break;
  case 'L':
    testname = "Long Self";
    retval = scsiSmartExtendSelfTest(device);
    break;
  }
  // If we can't do the test, exit
  if (NULL == testname) {
    PrintOut(LOG_CRIT, "Device: %s, not capable of %c Self-Test\n", name, 
             testtype);
    return 1;
  }
  if (retval) {
    if ((SIMPLE_ERR_BAD_OPCODE == retval) || 
        (SIMPLE_ERR_BAD_FIELD == retval)) {
      PrintOut(LOG_CRIT, "Device: %s, not capable of %s-Test\n", name, 
               testname);
      if ('L'==testtype)
        state.not_cap_long = true;
      else
        state.not_cap_short = true;
     
      return 1;
    }
    PrintOut(LOG_CRIT, "Device: %s, execute %s-Test failed (err: %d)\n", name, 
             testname, retval);
    return 1;
  }
  
  PrintOut(LOG_INFO, "Device: %s, starting scheduled %s-Test.\n", name, testname);
  
  return 0;
}

// Do an offline immediate or self-test.  Return zero on success,
// nonzero on failure.
static int DoATASelfTest(const dev_config & cfg, dev_state & state, ata_device * device, char testtype)
{
  const char *name = cfg.name.c_str();

  // Read current smart data and check status/capability
  struct ata_smart_values data;
  if (ataReadSmartValues(device, &data) || !(data.offline_data_collection_capability)) {
    PrintOut(LOG_CRIT, "Device: %s, not capable of Offline or Self-Testing.\n", name);
    return 1;
  }
  
  // Check for capability to do the test
  int dotest = -1, mode = 0;
  const char *testname = 0;
  switch (testtype) {
  case 'O':
    testname="Offline Immediate ";
    if (isSupportExecuteOfflineImmediate(&data))
      dotest=OFFLINE_FULL_SCAN;
    else
      state.not_cap_offline = true;
    break;
  case 'C':
    testname="Conveyance Self-";
    if (isSupportConveyanceSelfTest(&data))
      dotest=CONVEYANCE_SELF_TEST;
    else
      state.not_cap_conveyance = true;
    break;
  case 'S':
    testname="Short Self-";
    if (isSupportSelfTest(&data))
      dotest=SHORT_SELF_TEST;
    else
      state.not_cap_short = true;
    break;
  case 'L':
    testname="Long Self-";
    if (isSupportSelfTest(&data))
      dotest=EXTEND_SELF_TEST;
    else
      state.not_cap_long = true;
    break;

  case 'c': case 'n': case 'r':
    testname = "Selective Self-";
    if (isSupportSelectiveSelfTest(&data)) {
      dotest = SELECTIVE_SELF_TEST;
      switch (testtype) {
        case 'c': mode = SEL_CONT; break;
        case 'n': mode = SEL_NEXT; break;
        case 'r': mode = SEL_REDO; break;
      }
    }
    else
      state.not_cap_selective = true;
    break;
  }
  
  // If we can't do the test, exit
  if (dotest<0) {
    PrintOut(LOG_CRIT, "Device: %s, not capable of %sTest\n", name, testname);
    return 1;
  }
  
  // If currently running a self-test, do not interrupt it to start another.
  if (15==(data.self_test_exec_status >> 4)) {
    if (cfg.firmwarebugs.is_set(BUG_SAMSUNG3) && data.self_test_exec_status == 0xf0) {
      PrintOut(LOG_INFO, "Device: %s, will not skip scheduled %sTest "
               "despite unclear Self-Test byte (SAMSUNG Firmware bug).\n", name, testname);
    } else {
      PrintOut(LOG_INFO, "Device: %s, skip scheduled %sTest; %1d0%% remaining of current Self-Test.\n",
               name, testname, (int)(data.self_test_exec_status & 0x0f));
      return 1;
    }
  }

  if (dotest == SELECTIVE_SELF_TEST) {
    // Set test span
    ata_selective_selftest_args selargs, prev_args;
    selargs.num_spans = 1;
    selargs.span[0].mode = mode;
    prev_args.num_spans = 1;
    prev_args.span[0].start = state.selective_test_last_start;
    prev_args.span[0].end   = state.selective_test_last_end;
    if (ataWriteSelectiveSelfTestLog(device, selargs, &data, state.num_sectors, &prev_args)) {
      PrintOut(LOG_CRIT, "Device: %s, prepare %sTest failed\n", name, testname);
      return 1;
    }
    uint64_t start = selargs.span[0].start, end = selargs.span[0].end;
    PrintOut(LOG_INFO, "Device: %s, %s test span at LBA %" PRIu64 " - %" PRIu64 " (%" PRIu64 " sectors, %u%% - %u%% of disk).\n",
      name, (selargs.span[0].mode == SEL_NEXT ? "next" : "redo"),
      start, end, end - start + 1,
      (unsigned)((100 * start + state.num_sectors/2) / state.num_sectors),
      (unsigned)((100 * end   + state.num_sectors/2) / state.num_sectors));
    state.selective_test_last_start = start;
    state.selective_test_last_end = end;
  }

  // execute the test, and return status
  int retval = smartcommandhandler(device, IMMEDIATE_OFFLINE, dotest, NULL);
  if (retval) {
    PrintOut(LOG_CRIT, "Device: %s, execute %sTest failed.\n", name, testname);
    return retval;
  }

  // Report recent test start to do_disable_standby_check()
  // and force log of next test status
  if (testtype == 'O')
    state.offline_started = true;
  else
    state.selftest_started = true;

  PrintOut(LOG_INFO, "Device: %s, starting scheduled %sTest.\n", name, testname);
  return 0;
}

// Check pending sector count attribute values (-C, -U directives).
static void check_pending(const dev_config & cfg, dev_state & state,
                          unsigned char id, bool increase_only,
                          const ata_smart_values & smartval,
                          int mailtype, const char * msg)
{
  // Find attribute index
  int i = ata_find_attr_index(id, smartval);
  if (!(i >= 0 && ata_find_attr_index(id, state.smartval) == i))
    return;

  // No report if no sectors pending.
  uint64_t rawval = ata_get_attr_raw_value(smartval.vendor_attributes[i], cfg.attribute_defs);
  if (rawval == 0) {
    reset_warning_mail(cfg, state, mailtype, "No more %s", msg);
    return;
  }

  // If attribute is not reset, report only sector count increases.
  uint64_t prev_rawval = ata_get_attr_raw_value(state.smartval.vendor_attributes[i], cfg.attribute_defs);
  if (!(!increase_only || prev_rawval < rawval))
    return;

  // Format message.
  std::string s = strprintf("Device: %s, %" PRId64 " %s", cfg.name.c_str(), rawval, msg);
  if (prev_rawval > 0 && rawval != prev_rawval)
    s += strprintf(" (changed %+" PRId64 ")", rawval - prev_rawval);

  PrintOut(LOG_CRIT, "%s\n", s.c_str());
  MailWarning(cfg, state, mailtype, "%s", s.c_str());
  state.must_write = true;
}

// Format Temperature value
static const char * fmt_temp(unsigned char x, char (& buf)[20])
{
  if (!x) // unset
    return "??";
  snprintf(buf, sizeof(buf), "%u", x);
  return buf;
}

// Check Temperature limits
static void CheckTemperature(const dev_config & cfg, dev_state & state, unsigned char currtemp, unsigned char triptemp)
{
  if (!(0 < currtemp && currtemp < 255)) {
    PrintOut(LOG_INFO, "Device: %s, failed to read Temperature\n", cfg.name.c_str());
    return;
  }

  // Update Max Temperature
  const char * minchg = "", * maxchg = "";
  if (currtemp > state.tempmax) {
    if (state.tempmax)
      maxchg = "!";
    state.tempmax = currtemp;
    state.must_write = true;
  }

  char buf[20];
  if (!state.temperature) {
    // First check
    if (!state.tempmin || currtemp < state.tempmin)
        // Delay Min Temperature update by ~ 30 minutes.
        state.tempmin_delay = time(0) + CHECKTIME - 60;
    PrintOut(LOG_INFO, "Device: %s, initial Temperature is %d Celsius (Min/Max %s/%u%s)\n",
      cfg.name.c_str(), (int)currtemp, fmt_temp(state.tempmin, buf), state.tempmax, maxchg);
    if (triptemp)
      PrintOut(LOG_INFO, "    [trip Temperature is %d Celsius]\n", (int)triptemp);
    state.temperature = currtemp;
  }
  else {
    if (state.tempmin_delay) {
      // End Min Temperature update delay if ...
      if (   (state.tempmin && currtemp > state.tempmin) // current temp exceeds recorded min,
          || (state.tempmin_delay <= time(0))) {         // or delay time is over.
        state.tempmin_delay = 0;
        if (!state.tempmin)
          state.tempmin = 255;
      }
    }

    // Update Min Temperature
    if (!state.tempmin_delay && currtemp < state.tempmin) {
      state.tempmin = currtemp;
      state.must_write = true;
      if (currtemp != state.temperature)
        minchg = "!";
    }

    // Track changes
    if (cfg.tempdiff && (*minchg || *maxchg || abs((int)currtemp - (int)state.temperature) >= cfg.tempdiff)) {
      PrintOut(LOG_INFO, "Device: %s, Temperature changed %+d Celsius to %u Celsius (Min/Max %s%s/%u%s)\n",
        cfg.name.c_str(), (int)currtemp-(int)state.temperature, currtemp, fmt_temp(state.tempmin, buf), minchg, state.tempmax, maxchg);
      state.temperature = currtemp;
    }
  }

  // Check limits
  if (cfg.tempcrit && currtemp >= cfg.tempcrit) {
    PrintOut(LOG_CRIT, "Device: %s, Temperature %u Celsius reached critical limit of %u Celsius (Min/Max %s%s/%u%s)\n",
      cfg.name.c_str(), currtemp, cfg.tempcrit, fmt_temp(state.tempmin, buf), minchg, state.tempmax, maxchg);
    MailWarning(cfg, state, 12, "Device: %s, Temperature %d Celsius reached critical limit of %u Celsius (Min/Max %s%s/%u%s)",
      cfg.name.c_str(), currtemp, cfg.tempcrit, fmt_temp(state.tempmin, buf), minchg, state.tempmax, maxchg);
  }
  else if (cfg.tempinfo && currtemp >= cfg.tempinfo) {
    PrintOut(LOG_INFO, "Device: %s, Temperature %u Celsius reached limit of %u Celsius (Min/Max %s%s/%u%s)\n",
      cfg.name.c_str(), currtemp, cfg.tempinfo, fmt_temp(state.tempmin, buf), minchg, state.tempmax, maxchg);
  }
  else if (cfg.tempcrit) {
    unsigned char limit = (cfg.tempinfo ? cfg.tempinfo : cfg.tempcrit-5);
    if (currtemp < limit)
      reset_warning_mail(cfg, state, 12, "Temperature %u Celsius dropped below %u Celsius", currtemp, limit);
  }
}

// Check normalized and raw attribute values.
static void check_attribute(const dev_config & cfg, dev_state & state,
                            const ata_smart_attribute & attr,
                            const ata_smart_attribute & prev,
                            int attridx,
                            const ata_smart_threshold_entry * thresholds)
{
  // Check attribute and threshold
  ata_attr_state attrstate = ata_get_attr_state(attr, attridx, thresholds, cfg.attribute_defs);
  if (attrstate == ATTRSTATE_NON_EXISTING)
    return;

  // If requested, check for usage attributes that have failed.
  if (   cfg.usagefailed && attrstate == ATTRSTATE_FAILED_NOW
      && !cfg.monitor_attr_flags.is_set(attr.id, MONITOR_IGN_FAILUSE)) {
    std::string attrname = ata_get_smart_attr_name(attr.id, cfg.attribute_defs, cfg.dev_rpm);
    PrintOut(LOG_CRIT, "Device: %s, Failed SMART usage Attribute: %d %s.\n", cfg.name.c_str(), attr.id, attrname.c_str());
    MailWarning(cfg, state, 2, "Device: %s, Failed SMART usage Attribute: %d %s.", cfg.name.c_str(), attr.id, attrname.c_str());
    state.must_write = true;
  }

  // Return if we're not tracking this type of attribute
  bool prefail = !!ATTRIBUTE_FLAGS_PREFAILURE(attr.flags);
  if (!(   ( prefail && cfg.prefail)
        || (!prefail && cfg.usage  )))
    return;

  // Return if '-I ID' was specified
  if (cfg.monitor_attr_flags.is_set(attr.id, MONITOR_IGNORE))
    return;

  // Issue warning if they don't have the same ID in all structures.
  if (attr.id != prev.id) {
    PrintOut(LOG_INFO,"Device: %s, same Attribute has different ID numbers: %d = %d\n",
             cfg.name.c_str(), attr.id, prev.id);
    return;
  }

  // Compare normalized values if valid.
  bool valchanged = false;
  if (attrstate > ATTRSTATE_NO_NORMVAL) {
    if (attr.current != prev.current)
      valchanged = true;
  }

  // Compare raw values if requested.
  bool rawchanged = false;
  if (cfg.monitor_attr_flags.is_set(attr.id, MONITOR_RAW)) {
    if (   ata_get_attr_raw_value(attr, cfg.attribute_defs)
        != ata_get_attr_raw_value(prev, cfg.attribute_defs))
      rawchanged = true;
  }

  // Return if no change
  if (!(valchanged || rawchanged))
    return;

  // Format value strings
  std::string currstr, prevstr;
  if (attrstate == ATTRSTATE_NO_NORMVAL) {
    // Print raw values only
    currstr = strprintf("%s (Raw)",
      ata_format_attr_raw_value(attr, cfg.attribute_defs).c_str());
    prevstr = strprintf("%s (Raw)",
      ata_format_attr_raw_value(prev, cfg.attribute_defs).c_str());
  }
  else if (cfg.monitor_attr_flags.is_set(attr.id, MONITOR_RAW_PRINT)) {
    // Print normalized and raw values
    currstr = strprintf("%d [Raw %s]", attr.current,
      ata_format_attr_raw_value(attr, cfg.attribute_defs).c_str());
    prevstr = strprintf("%d [Raw %s]", prev.current,
      ata_format_attr_raw_value(prev, cfg.attribute_defs).c_str());
  }
  else {
    // Print normalized values only
    currstr = strprintf("%d", attr.current);
    prevstr = strprintf("%d", prev.current);
  }

  // Format message
  std::string msg = strprintf("Device: %s, SMART %s Attribute: %d %s changed from %s to %s",
                              cfg.name.c_str(), (prefail ? "Prefailure" : "Usage"), attr.id,
                              ata_get_smart_attr_name(attr.id, cfg.attribute_defs, cfg.dev_rpm).c_str(),
                              prevstr.c_str(), currstr.c_str());

  // Report this change as critical ?
  if (   (valchanged && cfg.monitor_attr_flags.is_set(attr.id, MONITOR_AS_CRIT))
      || (rawchanged && cfg.monitor_attr_flags.is_set(attr.id, MONITOR_RAW_AS_CRIT))) {
    PrintOut(LOG_CRIT, "%s\n", msg.c_str());
    MailWarning(cfg, state, 2, "%s", msg.c_str());
  }
  else {
    PrintOut(LOG_INFO, "%s\n", msg.c_str());
  }
  state.must_write = true;
}


static int ATACheckDevice(const dev_config & cfg, dev_state & state, ata_device * atadev,
                          bool firstpass, bool allow_selftests)
{
  const char * name = cfg.name.c_str();

  // If user has asked, test the email warning system
  if (cfg.emailtest)
    MailWarning(cfg, state, 0, "TEST EMAIL from smartd for device: %s", name);

  // if we can't open device, fail gracefully rather than hard --
  // perhaps the next time around we'll be able to open it.  ATAPI
  // cd/dvd devices will hang awaiting media if O_NONBLOCK is not
  // given (see linux cdrom driver).
  if (!atadev->open()) {
    PrintOut(LOG_INFO, "Device: %s, open() failed: %s\n", name, atadev->get_errmsg());
    MailWarning(cfg, state, 9, "Device: %s, unable to open device", name);
    return 1;
  }
  if (debugmode)
    PrintOut(LOG_INFO,"Device: %s, opened ATA device\n", name);
  reset_warning_mail(cfg, state, 9, "open device worked again");

  // user may have requested (with the -n Directive) to leave the disk
  // alone if it is in idle or sleeping mode.  In this case check the
  // power mode and exit without check if needed
  if (cfg.powermode && !state.powermodefail) {
    int dontcheck=0, powermode=ataCheckPowerMode(atadev);
    const char * mode = 0;
    if (0 <= powermode && powermode < 0xff) {
      // wait for possible spin up and check again
      int powermode2;
      sleep(5);
      powermode2 = ataCheckPowerMode(atadev);
      if (powermode2 > powermode)
        PrintOut(LOG_INFO, "Device: %s, CHECK POWER STATUS spins up disk (0x%02x -> 0x%02x)\n", name, powermode, powermode2);
      powermode = powermode2;
    }
        
    switch (powermode){
    case -1:
      // SLEEP
      mode="SLEEP";
      if (cfg.powermode>=1)
        dontcheck=1;
      break;
    case 0:
      // STANDBY
      mode="STANDBY";
      if (cfg.powermode>=2)
        dontcheck=1;
      break;
    case 0x80:
      // IDLE
      mode="IDLE";
      if (cfg.powermode>=3)
        dontcheck=1;
      break;
    case 0xff:
      // ACTIVE/IDLE
      mode="ACTIVE or IDLE";
      break;
    default:
      // UNKNOWN
      PrintOut(LOG_CRIT, "Device: %s, CHECK POWER STATUS returned %d, not ATA compliant, ignoring -n Directive\n",
        name, powermode);
      state.powermodefail = true;
      break;
    }

    // if we are going to skip a check, return now
    if (dontcheck){
      // skip at most powerskipmax checks
      if (!cfg.powerskipmax || state.powerskipcnt<cfg.powerskipmax) {
        CloseDevice(atadev, name);
        if (!state.powerskipcnt && !cfg.powerquiet) // report first only and avoid waking up system disk
          PrintOut(LOG_INFO, "Device: %s, is in %s mode, suspending checks\n", name, mode);
        state.powerskipcnt++;
        return 0;
      }
      else {
        PrintOut(LOG_INFO, "Device: %s, %s mode ignored due to reached limit of skipped checks (%d check%s skipped)\n",
          name, mode, state.powerskipcnt, (state.powerskipcnt==1?"":"s"));
      }
      state.powerskipcnt = 0;
      state.tempmin_delay = time(0) + CHECKTIME - 60; // Delay Min Temperature update
    }
    else if (state.powerskipcnt) {
      PrintOut(LOG_INFO, "Device: %s, is back in %s mode, resuming checks (%d check%s skipped)\n",
        name, mode, state.powerskipcnt, (state.powerskipcnt==1?"":"s"));
      state.powerskipcnt = 0;
      state.tempmin_delay = time(0) + CHECKTIME - 60; // Delay Min Temperature update
    }
  }

  // check smart status
  if (cfg.smartcheck) {
    int status=ataSmartStatus2(atadev);
    if (status==-1){
      PrintOut(LOG_INFO,"Device: %s, not capable of SMART self-check\n",name);
      MailWarning(cfg, state, 5, "Device: %s, not capable of SMART self-check", name);
      state.must_write = true;
    }
    else if (status==1){
      PrintOut(LOG_CRIT, "Device: %s, FAILED SMART self-check. BACK UP DATA NOW!\n", name);
      MailWarning(cfg, state, 1, "Device: %s, FAILED SMART self-check. BACK UP DATA NOW!", name);
      state.must_write = true;
    }
  }
  
  // Check everything that depends upon SMART Data (eg, Attribute values)
  if (   cfg.usagefailed || cfg.prefail || cfg.usage
      || cfg.curr_pending_id || cfg.offl_pending_id
      || cfg.tempdiff || cfg.tempinfo || cfg.tempcrit
      || cfg.selftest ||  cfg.offlinests || cfg.selfteststs) {

    // Read current attribute values.
    ata_smart_values curval;
    if (ataReadSmartValues(atadev, &curval)){
      PrintOut(LOG_CRIT, "Device: %s, failed to read SMART Attribute Data\n", name);
      MailWarning(cfg, state, 6, "Device: %s, failed to read SMART Attribute Data", name);
      state.must_write = true;
    }
    else {
      reset_warning_mail(cfg, state, 6, "read SMART Attribute Data worked again");

      // look for current or offline pending sectors
      if (cfg.curr_pending_id)
        check_pending(cfg, state, cfg.curr_pending_id, cfg.curr_pending_incr, curval, 10,
                      (!cfg.curr_pending_incr ? "Currently unreadable (pending) sectors"
                                              : "Total unreadable (pending) sectors"    ));

      if (cfg.offl_pending_id)
        check_pending(cfg, state, cfg.offl_pending_id, cfg.offl_pending_incr, curval, 11,
                      (!cfg.offl_pending_incr ? "Offline uncorrectable sectors"
                                              : "Total offline uncorrectable sectors"));

      // check temperature limits
      if (cfg.tempdiff || cfg.tempinfo || cfg.tempcrit)
        CheckTemperature(cfg, state, ata_return_temperature_value(&curval, cfg.attribute_defs), 0);

      // look for failed usage attributes, or track usage or prefail attributes
      if (cfg.usagefailed || cfg.prefail || cfg.usage) {
        for (int i = 0; i < NUMBER_ATA_SMART_ATTRIBUTES; i++) {
          check_attribute(cfg, state,
                          curval.vendor_attributes[i],
                          state.smartval.vendor_attributes[i],
                          i, state.smartthres.thres_entries);
        }
      }

      // Log changes of offline data collection status
      if (cfg.offlinests) {
        if (   curval.offline_data_collection_status
                != state.smartval.offline_data_collection_status
            || state.offline_started // test was started in previous call
            || (firstpass && (debugmode || (curval.offline_data_collection_status & 0x7d))))
          log_offline_data_coll_status(name, curval.offline_data_collection_status);
      }

      // Log changes of self-test execution status
      if (cfg.selfteststs) {
        if (   curval.self_test_exec_status != state.smartval.self_test_exec_status
            || state.selftest_started // test was started in previous call
            || (firstpass && (debugmode || curval.self_test_exec_status != 0x00)))
          log_self_test_exec_status(name, curval.self_test_exec_status);
      }

      // Save the new values for the next time around
      state.smartval = curval;
    }
  }
  state.offline_started = state.selftest_started = false;
  
  // check if number of selftest errors has increased (note: may also DECREASE)
  if (cfg.selftest)
    CheckSelfTestLogs(cfg, state, SelfTestErrorCount(atadev, name, cfg.firmwarebugs));

  // check if number of ATA errors has increased
  if (cfg.errorlog || cfg.xerrorlog) {

    int errcnt1 = -1, errcnt2 = -1;
    if (cfg.errorlog)
      errcnt1 = read_ata_error_count(atadev, name, cfg.firmwarebugs, false);
    if (cfg.xerrorlog)
      errcnt2 = read_ata_error_count(atadev, name, cfg.firmwarebugs, true);

    // new number of errors is max of both logs
    int newc = (errcnt1 >= errcnt2 ? errcnt1 : errcnt2);

    // did command fail?
    if (newc<0)
      // lack of PrintOut here is INTENTIONAL
      MailWarning(cfg, state, 7, "Device: %s, Read SMART Error Log Failed", name);

    // has error count increased?
    int oldc = state.ataerrorcount;
    if (newc>oldc){
      PrintOut(LOG_CRIT, "Device: %s, ATA error count increased from %d to %d\n",
               name, oldc, newc);
      MailWarning(cfg, state, 4, "Device: %s, ATA error count increased from %d to %d",
                   name, oldc, newc);
      state.must_write = true;
    }

    if (newc>=0)
      state.ataerrorcount=newc;
  }

  // if the user has asked, and device is capable (or we're not yet
  // sure) check whether a self test should be done now.
  if (allow_selftests && !cfg.test_regex.empty()) {
    char testtype = next_scheduled_test(cfg, state, false/*!scsi*/);
    if (testtype)
      DoATASelfTest(cfg, state, atadev, testtype);
  }

  // Don't leave device open -- the OS/user may want to access it
  // before the next smartd cycle!
  CloseDevice(atadev, name);

  // Copy ATA attribute values to persistent state
  state.update_persistent_state();

  return 0;
}

static int SCSICheckDevice(const dev_config & cfg, dev_state & state, scsi_device * scsidev, bool allow_selftests)
{
    UINT8 asc, ascq;
    UINT8 currenttemp;
    UINT8 triptemp;
    UINT8  tBuf[252];
    const char * name = cfg.name.c_str();
    const char *cp;

    // If the user has asked for it, test the email warning system
    if (cfg.emailtest)
      MailWarning(cfg, state, 0, "TEST EMAIL from smartd for device: %s", name);

    // if we can't open device, fail gracefully rather than hard --
    // perhaps the next time around we'll be able to open it
    if (!scsidev->open()) {
      PrintOut(LOG_INFO, "Device: %s, open() failed: %s\n", name, scsidev->get_errmsg());
      MailWarning(cfg, state, 9, "Device: %s, unable to open device", name);
      return 1;
    } else if (debugmode)
        PrintOut(LOG_INFO,"Device: %s, opened SCSI device\n", name);
    reset_warning_mail(cfg, state, 9, "open device worked again");
    currenttemp = 0;
    asc = 0;
    ascq = 0;
    if (!state.SuppressReport) {
        if (scsiCheckIE(scsidev, state.SmartPageSupported, state.TempPageSupported,
                        &asc, &ascq, &currenttemp, &triptemp)) {
            PrintOut(LOG_INFO, "Device: %s, failed to read SMART values\n",
                      name);
            MailWarning(cfg, state, 6, "Device: %s, failed to read SMART values", name);
            state.SuppressReport = 1;
        }
    }
    if (asc > 0) {
        cp = scsiGetIEString(asc, ascq);
        if (cp) {
            PrintOut(LOG_CRIT, "Device: %s, SMART Failure: %s\n", name, cp);
            MailWarning(cfg, state, 1,"Device: %s, SMART Failure: %s", name, cp);
        } else if (asc == 4 && ascq == 9) {
            PrintOut(LOG_INFO,"Device: %s, self-test in progress\n", name);  
        } else if (debugmode)
            PrintOut(LOG_INFO,"Device: %s, non-SMART asc,ascq: %d,%d\n",
                     name, (int)asc, (int)ascq);  
    } else if (debugmode)
        PrintOut(LOG_INFO,"Device: %s, SMART health: passed\n", name);  

    // check temperature limits
    if (cfg.tempdiff || cfg.tempinfo || cfg.tempcrit || !cfg.attrlog_file.empty())
      CheckTemperature(cfg, state, currenttemp, triptemp);

    // check if number of selftest errors has increased (note: may also DECREASE)
    if (cfg.selftest)
      CheckSelfTestLogs(cfg, state, scsiCountFailedSelfTests(scsidev, 0));
    
    if (allow_selftests && !cfg.test_regex.empty()) {
      char testtype = next_scheduled_test(cfg, state, true/*scsi*/);
      if (testtype)
        DoSCSISelfTest(cfg, state, scsidev, testtype);
    }
    if (!cfg.attrlog_file.empty()){
      // saving error counters to state
      if (state.ReadECounterPageSupported && (0 == scsiLogSense(scsidev,
          READ_ERROR_COUNTER_LPAGE, 0, tBuf, sizeof(tBuf), 0))) {
          scsiDecodeErrCounterPage(tBuf, &state.scsi_error_counters[0].errCounter);
          state.scsi_error_counters[0].found=1;
      }
      if (state.WriteECounterPageSupported && (0 == scsiLogSense(scsidev,
          WRITE_ERROR_COUNTER_LPAGE, 0, tBuf, sizeof(tBuf), 0))) {
          scsiDecodeErrCounterPage(tBuf, &state.scsi_error_counters[1].errCounter);
          state.scsi_error_counters[1].found=1;
      }
      if (state.VerifyECounterPageSupported && (0 == scsiLogSense(scsidev,
          VERIFY_ERROR_COUNTER_LPAGE, 0, tBuf, sizeof(tBuf), 0))) {
          scsiDecodeErrCounterPage(tBuf, &state.scsi_error_counters[2].errCounter);
          state.scsi_error_counters[2].found=1;
      }
      if (state.NonMediumErrorPageSupported && (0 == scsiLogSense(scsidev,
          NON_MEDIUM_ERROR_LPAGE, 0, tBuf, sizeof(tBuf), 0))) {
          scsiDecodeNonMediumErrPage(tBuf, &state.scsi_nonmedium_error.nme);
          state.scsi_nonmedium_error.found=1;
      }
    }
    CloseDevice(scsidev, name);
    return 0;
}

// 0=not used, 1=not disabled, 2=disable rejected by OS, 3=disabled
static int standby_disable_state = 0;

static void init_disable_standby_check(dev_config_vector & configs)
{
  // Check for '-l offlinests,ns' or '-l selfteststs,ns' directives
  bool sts1 = false, sts2 = false;
  for (unsigned i = 0; i < configs.size() && !(sts1 || sts2); i++) {
    const dev_config & cfg = configs.at(i);
    if (cfg.offlinests_ns)
      sts1 = true;
    if (cfg.selfteststs_ns)
      sts2 = true;
  }

  // Check for support of disable auto standby
  // Reenable standby if smartd.conf was reread
  if (sts1 || sts2 || standby_disable_state == 3) {
   if (!smi()->disable_system_auto_standby(false)) {
      if (standby_disable_state == 3)
        PrintOut(LOG_CRIT, "System auto standby enable failed: %s\n", smi()->get_errmsg());
      if (sts1 || sts2) {
        PrintOut(LOG_INFO, "Disable auto standby not supported, ignoring ',ns' from %s%s%s\n",
          (sts1 ? "-l offlinests,ns" : ""), (sts1 && sts2 ? " and " : ""), (sts2 ? "-l selfteststs,ns" : ""));
        sts1 = sts2 = false;
      }
    }
  }

  standby_disable_state = (sts1 || sts2 ? 1 : 0);
}

static void do_disable_standby_check(const dev_config_vector & configs, const dev_state_vector & states)
{
  if (!standby_disable_state)
    return;

  // Check for just started or still running self-tests
  bool running = false;
  for (unsigned i = 0; i < configs.size() && !running; i++) {
    const dev_config & cfg = configs.at(i); const dev_state & state = states.at(i);

    if (   (   cfg.offlinests_ns
            && (state.offline_started ||
                is_offl_coll_in_progress(state.smartval.offline_data_collection_status)))
        || (   cfg.selfteststs_ns
            && (state.selftest_started ||
                is_self_test_in_progress(state.smartval.self_test_exec_status)))         )
      running = true;
    // state.offline/selftest_started will be reset after next logging of test status
  }

  // Disable/enable auto standby and log state changes
  if (!running) {
    if (standby_disable_state != 1) {
      if (!smi()->disable_system_auto_standby(false))
        PrintOut(LOG_CRIT, "Self-test(s) completed, system auto standby enable failed: %s\n",
                 smi()->get_errmsg());
      else
        PrintOut(LOG_INFO, "Self-test(s) completed, system auto standby enabled\n");
      standby_disable_state = 1;
    }
  }
  else if (!smi()->disable_system_auto_standby(true)) {
    if (standby_disable_state != 2) {
      PrintOut(LOG_INFO, "Self-test(s) in progress, system auto standby disable rejected: %s\n",
               smi()->get_errmsg());
      standby_disable_state = 2;
    }
  }
  else {
    if (standby_disable_state != 3) {
      PrintOut(LOG_INFO, "Self-test(s) in progress, system auto standby disabled\n");
      standby_disable_state = 3;
    }
  }
}

// Checks the SMART status of all ATA and SCSI devices
static void CheckDevicesOnce(const dev_config_vector & configs, dev_state_vector & states,
                             smart_device_list & devices, bool firstpass, bool allow_selftests)
{
  for (unsigned i = 0; i < configs.size(); i++) {
    const dev_config & cfg = configs.at(i);
    dev_state & state = states.at(i);
    smart_device * dev = devices.at(i);
    if (dev->is_ata())
      ATACheckDevice(cfg, state, dev->to_ata(), firstpass, allow_selftests);
    else if (dev->is_scsi())
      SCSICheckDevice(cfg, state, dev->to_scsi(), allow_selftests);
  }

  do_disable_standby_check(configs, states);
}

// Set if Initialize() was called
static bool is_initialized = false;

// Does initialization right after fork to daemon mode
static void Initialize(time_t *wakeuptime)
{
  // Call Goodbye() on exit
  is_initialized = true;
  
  // write PID file
  if (!debugmode)
    WritePidFile();
  
  // install signal handlers.  On Solaris, can't use signal() because
  // it resets the handler to SIG_DFL after each call.  So use sigset()
  // instead.  So SIGNALFN()==signal() or SIGNALFN()==sigset().
  
  // normal and abnormal exit
  if (SIGNALFN(SIGTERM, sighandler)==SIG_IGN)
    SIGNALFN(SIGTERM, SIG_IGN);
  if (SIGNALFN(SIGQUIT, sighandler)==SIG_IGN)
    SIGNALFN(SIGQUIT, SIG_IGN);
  
  // in debug mode, <CONTROL-C> ==> HUP
  if (SIGNALFN(SIGINT, debugmode?HUPhandler:sighandler)==SIG_IGN)
    SIGNALFN(SIGINT, SIG_IGN);
  
  // Catch HUP and USR1
  if (SIGNALFN(SIGHUP, HUPhandler)==SIG_IGN)
    SIGNALFN(SIGHUP, SIG_IGN);
  if (SIGNALFN(SIGUSR1, USR1handler)==SIG_IGN)
    SIGNALFN(SIGUSR1, SIG_IGN);
#ifdef _WIN32
  if (SIGNALFN(SIGUSR2, USR2handler)==SIG_IGN)
    SIGNALFN(SIGUSR2, SIG_IGN);
#endif

  // initialize wakeup time to CURRENT time
  *wakeuptime=time(NULL);
  
  return;
}

#ifdef _WIN32
// Toggle debug mode implemented for native windows only
// (there is no easy way to reopen tty on *nix)
static void ToggleDebugMode()
{
  if (!debugmode) {
    PrintOut(LOG_INFO,"Signal USR2 - enabling debug mode\n");
    if (!daemon_enable_console("smartd [Debug]")) {
      debugmode = 1;
      daemon_signal(SIGINT, HUPhandler);
      PrintOut(LOG_INFO,"smartd debug mode enabled, PID=%d\n", getpid());
    }
    else
      PrintOut(LOG_INFO,"enable console failed\n");
  }
  else if (debugmode == 1) {
    daemon_disable_console();
    debugmode = 0;
    daemon_signal(SIGINT, sighandler);
    PrintOut(LOG_INFO,"Signal USR2 - debug mode disabled\n");
  }
  else
    PrintOut(LOG_INFO,"Signal USR2 - debug mode %d not changed\n", debugmode);
}
#endif

static time_t dosleep(time_t wakeuptime, bool & sigwakeup)
{
  // If past wake-up-time, compute next wake-up-time
  time_t timenow=time(NULL);
  while (wakeuptime<=timenow){
    int intervals=1+(timenow-wakeuptime)/checktime;
    wakeuptime+=intervals*checktime;
  }
  
  // sleep until we catch SIGUSR1 or have completed sleeping
  int addtime = 0;
  while (timenow < wakeuptime+addtime && !caughtsigUSR1 && !caughtsigHUP && !caughtsigEXIT) {
    
    // protect user again system clock being adjusted backwards
    if (wakeuptime>timenow+checktime){
      PrintOut(LOG_CRIT, "System clock time adjusted to the past. Resetting next wakeup time.\n");
      wakeuptime=timenow+checktime;
    }
    
    // Exit sleep when time interval has expired or a signal is received
    sleep(wakeuptime+addtime-timenow);

#ifdef _WIN32
    // toggle debug mode?
    if (caughtsigUSR2) {
      ToggleDebugMode();
      caughtsigUSR2 = 0;
    }
#endif

    timenow=time(NULL);

    // Actual sleep time too long?
    if (!addtime && timenow > wakeuptime+60) {
      if (debugmode)
        PrintOut(LOG_INFO, "Sleep time was %d seconds too long, assuming wakeup from standby mode.\n",
          (int)(timenow-wakeuptime));
      // Wait another 20 seconds to avoid I/O errors during disk spin-up
      addtime = timenow-wakeuptime+20;
      // Use next wake-up-time if close
      int nextcheck = checktime - addtime % checktime;
      if (nextcheck <= 20)
        addtime += nextcheck;
    }
  }
 
  // if we caught a SIGUSR1 then print message and clear signal
  if (caughtsigUSR1){
    PrintOut(LOG_INFO,"Signal USR1 - checking devices now rather than in %d seconds.\n",
             wakeuptime-timenow>0?(int)(wakeuptime-timenow):0);
    caughtsigUSR1=0;
    sigwakeup = true;
  }
  
  // return adjusted wakeuptime
  return wakeuptime;
}

// Print out a list of valid arguments for the Directive d
static void printoutvaliddirectiveargs(int priority, char d)
{
  switch (d) {
  case 'n':
    PrintOut(priority, "never[,N][,q], sleep[,N][,q], standby[,N][,q], idle[,N][,q]");
    break;
  case 's':
    PrintOut(priority, "valid_regular_expression");
    break;
  case 'd':
    PrintOut(priority, "%s", smi()->get_valid_dev_types_str().c_str());
    break;
  case 'T':
    PrintOut(priority, "normal, permissive");
    break;
  case 'o':
  case 'S':
    PrintOut(priority, "on, off");
    break;
  case 'l':
    PrintOut(priority, "error, selftest");
    break;
  case 'M':
    PrintOut(priority, "\"once\", \"daily\", \"diminishing\", \"test\", \"exec\"");
    break;
  case 'v':
    PrintOut(priority, "\n%s\n", create_vendor_attribute_arg_list().c_str());
    break;
  case 'P':
    PrintOut(priority, "use, ignore, show, showall");
    break;
  case 'F':
    PrintOut(priority, "%s", get_valid_firmwarebug_args());
    break;
  case 'e':
    PrintOut(priority, "aam,[N|off], apm,[N|off], lookahead,[on|off], "
                       "security-freeze, standby,[N|off], wcache,[on|off]");
    break;
  }
}

// exits with an error message, or returns integer value of token
static int GetInteger(const char *arg, const char *name, const char *token, int lineno, const char *cfgfile,
               int min, int max, char * suffix = 0)
{
  // make sure argument is there
  if (!arg) {
    PrintOut(LOG_CRIT,"File %s line %d (drive %s): Directive: %s takes integer argument from %d to %d.\n",
             cfgfile, lineno, name, token, min, max);
    return -1;
  }
  
  // get argument value (base 10), check that it's integer, and in-range
  char *endptr;
  int val = strtol(arg,&endptr,10);

  // optional suffix present?
  if (suffix) {
    if (!strcmp(endptr, suffix))
      endptr += strlen(suffix);
    else
      *suffix = 0;
  }

  if (!(!*endptr && min <= val && val <= max)) {
    PrintOut(LOG_CRIT,"File %s line %d (drive %s): Directive: %s has argument: %s; needs integer from %d to %d.\n",
             cfgfile, lineno, name, token, arg, min, max);
    return -1;
  }

  // all is well; return value
  return val;
}


// Get 1-3 small integer(s) for '-W' directive
static int Get3Integers(const char *arg, const char *name, const char *token, int lineno, const char *cfgfile,
                 unsigned char *val1, unsigned char *val2, unsigned char *val3)
{
  unsigned v1 = 0, v2 = 0, v3 = 0;
  int n1 = -1, n2 = -1, n3 = -1, len;
  if (!arg) {
    PrintOut(LOG_CRIT,"File %s line %d (drive %s): Directive: %s takes 1-3 integer argument(s) from 0 to 255.\n",
             cfgfile, lineno, name, token);
    return -1;
  }

  len = strlen(arg);
  if (!(   sscanf(arg, "%u%n,%u%n,%u%n", &v1, &n1, &v2, &n2, &v3, &n3) >= 1
        && (n1 == len || n2 == len || n3 == len) && v1 <= 255 && v2 <= 255 && v3 <= 255)) {
    PrintOut(LOG_CRIT,"File %s line %d (drive %s): Directive: %s has argument: %s; needs 1-3 integer(s) from 0 to 255.\n",
             cfgfile, lineno, name, token, arg);
    return -1;
  }
  *val1 = (unsigned char)v1; *val2 = (unsigned char)v2; *val3 = (unsigned char)v3;
  return 0;
}


#ifdef _WIN32

// Concatenate strtok() results if quoted with "..."
static const char * strtok_dequote(const char * delimiters)
{
  const char * t = strtok(0, delimiters);
  if (!t || t[0] != '"')
    return t;

  static std::string token;
  token = t+1;
  for (;;) {
    t = strtok(0, delimiters);
    if (!t || !*t)
      return "\"";
    token += ' ';
    int len = strlen(t);
    if (t[len-1] == '"') {
      token += std::string(t, len-1);
      break;
    }
    token += t;
  }
  return token.c_str();
}

#endif // _WIN32


// This function returns 1 if it has correctly parsed one token (and
// any arguments), else zero if no tokens remain.  It returns -1 if an
// error was encountered.
static int ParseToken(char * token, dev_config & cfg)
{
  char sym;
  const char * name = cfg.name.c_str();
  int lineno=cfg.lineno;
  const char *delim = " \n\t";
  int badarg = 0;
  int missingarg = 0;
  const char *arg = 0;

  // is the rest of the line a comment
  if (*token=='#')
    return 1;
  
  // is the token not recognized?
  if (*token!='-' || strlen(token)!=2) {
    PrintOut(LOG_CRIT,"File %s line %d (drive %s): unknown Directive: %s\n",
             configfile, lineno, name, token);
    PrintOut(LOG_CRIT, "Run smartd -D to print a list of valid Directives.\n");
    return -1;
  }
  
  // token we will be parsing:
  sym=token[1];

  // parse the token and swallow its argument
  int val;
  char plus[] = "+", excl[] = "!";

  switch (sym) {
  case 'C':
    // monitor current pending sector count (default 197)
    if ((val = GetInteger(arg=strtok(NULL,delim), name, token, lineno, configfile, 0, 255, plus)) < 0)
      return -1;
    cfg.curr_pending_id = (unsigned char)val;
    cfg.curr_pending_incr = (*plus == '+');
    cfg.curr_pending_set = true;
    break;
  case 'U':
    // monitor offline uncorrectable sectors (default 198)
    if ((val = GetInteger(arg=strtok(NULL,delim), name, token, lineno, configfile, 0, 255, plus)) < 0)
      return -1;
    cfg.offl_pending_id = (unsigned char)val;
    cfg.offl_pending_incr = (*plus == '+');
    cfg.offl_pending_set = true;
    break;
  case 'T':
    // Set tolerance level for SMART command failures
    if ((arg = strtok(NULL, delim)) == NULL) {
      missingarg = 1;
    } else if (!strcmp(arg, "normal")) {
      // Normal mode: exit on failure of a mandatory S.M.A.R.T. command, but
      // not on failure of an optional S.M.A.R.T. command.
      // This is the default so we don't need to actually do anything here.
      cfg.permissive = false;
    } else if (!strcmp(arg, "permissive")) {
      // Permissive mode; ignore errors from Mandatory SMART commands
      cfg.permissive = true;
    } else {
      badarg = 1;
    }
    break;
  case 'd':
    // specify the device type
    if ((arg = strtok(NULL, delim)) == NULL) {
      missingarg = 1;
    } else if (!strcmp(arg, "ignore")) {
      cfg.ignore = true;
    } else if (!strcmp(arg, "removable")) {
      cfg.removable = true;
    } else if (!strcmp(arg, "auto")) {
      cfg.dev_type = "";
    } else {
      cfg.dev_type = arg;
    }
    break;
  case 'F':
    // fix firmware bug
    if (!(arg = strtok(0, delim)))
      missingarg = 1;
    else if (!parse_firmwarebug_def(arg, cfg.firmwarebugs))
      badarg = 1;
    break;
  case 'H':
    // check SMART status
    cfg.smartcheck = true;
    break;
  case 'f':
    // check for failure of usage attributes
    cfg.usagefailed = true;
    break;
  case 't':
    // track changes in all vendor attributes
    cfg.prefail = true;
    cfg.usage = true;
    break;
  case 'p':
    // track changes in prefail vendor attributes
    cfg.prefail = true;
    break;
  case 'u':
    //  track changes in usage vendor attributes
    cfg.usage = true;
    break;
  case 'l':
    // track changes in SMART logs
    if ((arg = strtok(NULL, delim)) == NULL) {
      missingarg = 1;
    } else if (!strcmp(arg, "selftest")) {
      // track changes in self-test log
      cfg.selftest = true;
    } else if (!strcmp(arg, "error")) {
      // track changes in ATA error log
      cfg.errorlog = true;
    } else if (!strcmp(arg, "xerror")) {
      // track changes in Extended Comprehensive SMART error log
      cfg.xerrorlog = true;
    } else if (!strcmp(arg, "offlinests")) {
      // track changes in offline data collection status
      cfg.offlinests = true;
    } else if (!strcmp(arg, "offlinests,ns")) {
      // track changes in offline data collection status, disable auto standby
      cfg.offlinests = cfg.offlinests_ns = true;
    } else if (!strcmp(arg, "selfteststs")) {
      // track changes in self-test execution status
      cfg.selfteststs = true;
    } else if (!strcmp(arg, "selfteststs,ns")) {
      // track changes in self-test execution status, disable auto standby
      cfg.selfteststs = cfg.selfteststs_ns = true;
    } else if (!strncmp(arg, "scterc,", sizeof("scterc,")-1)) {
        // set SCT Error Recovery Control
        unsigned rt = ~0, wt = ~0; int nc = -1;
        sscanf(arg,"scterc,%u,%u%n", &rt, &wt, &nc);
        if (nc == (int)strlen(arg) && rt <= 999 && wt <= 999) {
          cfg.sct_erc_set = true;
          cfg.sct_erc_readtime = rt;
          cfg.sct_erc_writetime = wt;
        }
        else
          badarg = 1;
    } else {
      badarg = 1;
    }
    break;
  case 'a':
    // monitor everything
    cfg.smartcheck = true;
    cfg.prefail = true;
    cfg.usagefailed = true;
    cfg.usage = true;
    cfg.selftest = true;
    cfg.errorlog = true;
    cfg.selfteststs = true;
    break;
  case 'o':
    // automatic offline testing enable/disable
    if ((arg = strtok(NULL, delim)) == NULL) {
      missingarg = 1;
    } else if (!strcmp(arg, "on")) {
      cfg.autoofflinetest = 2;
    } else if (!strcmp(arg, "off")) {
      cfg.autoofflinetest = 1;
    } else {
      badarg = 1;
    }
    break;
  case 'n':
    // skip disk check if in idle or standby mode
    if (!(arg = strtok(NULL, delim)))
      missingarg = 1;
    else {
      char *endptr = NULL;
      char *next = strchr(const_cast<char*>(arg), ',');

      cfg.powerquiet = false;
      cfg.powerskipmax = 0;

      if (next!=NULL) *next='\0';
      if (!strcmp(arg, "never"))
        cfg.powermode = 0;
      else if (!strcmp(arg, "sleep"))
        cfg.powermode = 1;
      else if (!strcmp(arg, "standby"))
        cfg.powermode = 2;
      else if (!strcmp(arg, "idle"))
        cfg.powermode = 3;
      else
        badarg = 1;

      // if optional arguments are present
      if (!badarg && next!=NULL) {
        next++;
        cfg.powerskipmax = strtol(next, &endptr, 10);
        if (endptr == next)
          cfg.powerskipmax = 0;
        else {
          next = endptr + (*endptr != '\0');
          if (cfg.powerskipmax <= 0)
            badarg = 1;
        }
        if (*next != '\0') {
          if (!strcmp("q", next))
            cfg.powerquiet = true;
          else {
            badarg = 1;
          }
        }
      }
    }
    break;
  case 'S':
    // automatic attribute autosave enable/disable
    if ((arg = strtok(NULL, delim)) == NULL) {
      missingarg = 1;
    } else if (!strcmp(arg, "on")) {
      cfg.autosave = 2;
    } else if (!strcmp(arg, "off")) {
      cfg.autosave = 1;
    } else {
      badarg = 1;
    }
    break;
  case 's':
    // warn user, and delete any previously given -s REGEXP Directives
    if (!cfg.test_regex.empty()){
      PrintOut(LOG_INFO, "File %s line %d (drive %s): ignoring previous Test Directive -s %s\n",
               configfile, lineno, name, cfg.test_regex.get_pattern());
      cfg.test_regex = regular_expression();
    }
    // check for missing argument
    if (!(arg = strtok(NULL, delim))) {
      missingarg = 1;
    }
    // Compile regex
    else {
      if (!cfg.test_regex.compile(arg, REG_EXTENDED)) {
        // not a valid regular expression!
        PrintOut(LOG_CRIT, "File %s line %d (drive %s): -s argument \"%s\" is INVALID extended regular expression. %s.\n",
                 configfile, lineno, name, arg, cfg.test_regex.get_errmsg());
        return -1;
      }
    }
    // Do a bit of sanity checking and warn user if we think that
    // their regexp is "strange". User probably confused about shell
    // glob(3) syntax versus regular expression syntax regexp(7).
    if (arg[(val = strspn(arg, "0123456789/.-+*|()?^$[]SLCOcnr"))])
      PrintOut(LOG_INFO,  "File %s line %d (drive %s): warning, character %d (%c) looks odd in extended regular expression %s\n",
               configfile, lineno, name, val+1, arg[val], arg);
    break;
  case 'm':
    // send email to address that follows
    if (!(arg = strtok(NULL,delim)))
      missingarg = 1;
    else {
      if (!cfg.emailaddress.empty())
        PrintOut(LOG_INFO, "File %s line %d (drive %s): ignoring previous Address Directive -m %s\n",
                 configfile, lineno, name, cfg.emailaddress.c_str());
#ifdef _WIN32
      if (   !strcmp(arg, "msgbox")          || !strcmp(arg, "sysmsgbox")
          || str_starts_with(arg, "msgbox,") || str_starts_with(arg, "sysmsgbox,")) {
        cfg.emailaddress = "console";
        const char * arg2 = strchr(arg, ',');
        if (arg2)
          cfg.emailaddress += arg2;
        PrintOut(LOG_INFO, "File %s line %d (drive %s): Deprecated -m %s changed to -m %s\n",
                 configfile, lineno, name, arg, cfg.emailaddress.c_str());
      }
      else
#endif
      cfg.emailaddress = arg;
    }
    break;
  case 'M':
    // email warning options
    if (!(arg = strtok(NULL, delim)))
      missingarg = 1;
    else if (!strcmp(arg, "once"))
      cfg.emailfreq = 1;
    else if (!strcmp(arg, "daily"))
      cfg.emailfreq = 2;
    else if (!strcmp(arg, "diminishing"))
      cfg.emailfreq = 3;
    else if (!strcmp(arg, "test"))
      cfg.emailtest = 1;
    else if (!strcmp(arg, "exec")) {
      // Get the next argument (the command line)
#ifdef _WIN32
      // Allow "/path name/with spaces/..." on Windows
      arg = strtok_dequote(delim);
      if (arg && arg[0] == '"') {
        PrintOut(LOG_CRIT, "File %s line %d (drive %s): Directive %s 'exec' argument: missing closing quote\n",
                 configfile, lineno, name, token);
        return -1;
      }
#else
      arg = strtok(0, delim);
#endif
      if (!arg) {
        PrintOut(LOG_CRIT, "File %s line %d (drive %s): Directive %s 'exec' argument must be followed by executable path.\n",
                 configfile, lineno, name, token);
        return -1;
      }
      // Free the last cmd line given if any, and copy new one
      if (!cfg.emailcmdline.empty())
        PrintOut(LOG_INFO, "File %s line %d (drive %s): ignoring previous mail Directive -M exec %s\n",
                 configfile, lineno, name, cfg.emailcmdline.c_str());
      cfg.emailcmdline = arg;
    } 
    else
      badarg = 1;
    break;
  case 'i':
    // ignore failure of usage attribute
    if ((val=GetInteger(arg=strtok(NULL,delim), name, token, lineno, configfile, 1, 255))<0)
      return -1;
    cfg.monitor_attr_flags.set(val, MONITOR_IGN_FAILUSE);
    break;
  case 'I':
    // ignore attribute for tracking purposes
    if ((val=GetInteger(arg=strtok(NULL,delim), name, token, lineno, configfile, 1, 255))<0)
      return -1;
    cfg.monitor_attr_flags.set(val, MONITOR_IGNORE);
    break;
  case 'r':
    // print raw value when tracking
    if ((val = GetInteger(arg=strtok(NULL,delim), name, token, lineno, configfile, 1, 255, excl)) < 0)
      return -1;
    cfg.monitor_attr_flags.set(val, MONITOR_RAW_PRINT);
    if (*excl == '!') // attribute change is critical
      cfg.monitor_attr_flags.set(val, MONITOR_AS_CRIT);
    break;
  case 'R':
    // track changes in raw value (forces printing of raw value)
    if ((val = GetInteger(arg=strtok(NULL,delim), name, token, lineno, configfile, 1, 255, excl)) < 0)
      return -1;
    cfg.monitor_attr_flags.set(val, MONITOR_RAW_PRINT|MONITOR_RAW);
    if (*excl == '!') // raw value change is critical
      cfg.monitor_attr_flags.set(val, MONITOR_RAW_AS_CRIT);
    break;
  case 'W':
    // track Temperature
    if ((val=Get3Integers(arg=strtok(NULL,delim), name, token, lineno, configfile,
                          &cfg.tempdiff, &cfg.tempinfo, &cfg.tempcrit))<0)
      return -1;
    break;
  case 'v':
    // non-default vendor-specific attribute meaning
    if (!(arg=strtok(NULL,delim))) {
      missingarg = 1;
    } else if (!parse_attribute_def(arg, cfg.attribute_defs, PRIOR_USER)) {
      badarg = 1;
    }
    break;
  case 'P':
    // Define use of drive-specific presets.
    if (!(arg = strtok(NULL, delim))) {
      missingarg = 1;
    } else if (!strcmp(arg, "use")) {
      cfg.ignorepresets = false;
    } else if (!strcmp(arg, "ignore")) {
      cfg.ignorepresets = true;
    } else if (!strcmp(arg, "show")) {
      cfg.showpresets = true;
    } else if (!strcmp(arg, "showall")) {
      showallpresets();
    } else {
      badarg = 1;
    }
    break;

  case 'e':
    // Various ATA settings
    if (!(arg = strtok(NULL, delim))) {
      missingarg = true;
    }
    else {
      char arg2[16+1]; unsigned val;
      int n1 = -1, n2 = -1, n3 = -1, len = strlen(arg);
      if (sscanf(arg, "%16[^,=]%n%*[,=]%n%u%n", arg2, &n1, &n2, &val, &n3) >= 1
          && (n1 == len || n2 > 0)) {
        bool on  = (n2 > 0 && !strcmp(arg+n2, "on"));
        bool off = (n2 > 0 && !strcmp(arg+n2, "off"));
        if (n3 != len)
          val = ~0U;

        if (!strcmp(arg2, "aam")) {
          if (off)
            cfg.set_aam = -1;
          else if (val <= 254)
            cfg.set_aam = val + 1;
          else
            badarg = true;
        }
        else if (!strcmp(arg2, "apm")) {
          if (off)
            cfg.set_apm = -1;
          else if (1 <= val && val <= 254)
            cfg.set_apm = val + 1;
          else
            badarg = true;
        }
        else if (!strcmp(arg2, "lookahead")) {
          if (off)
            cfg.set_lookahead = -1;
          else if (on)
            cfg.set_lookahead = 1;
          else
            badarg = true;
        }
        else if (!strcmp(arg, "security-freeze")) {
          cfg.set_security_freeze = true;
        }
        else if (!strcmp(arg2, "standby")) {
          if (off)
            cfg.set_standby = 0 + 1;
          else if (val <= 255)
            cfg.set_standby = val + 1;
          else
            badarg = true;
        }
        else if (!strcmp(arg2, "wcache")) {
          if (off)
            cfg.set_wcache = -1;
          else if (on)
            cfg.set_wcache = 1;
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

  default:
    // Directive not recognized
    PrintOut(LOG_CRIT,"File %s line %d (drive %s): unknown Directive: %s\n",
             configfile, lineno, name, token);
    Directives();
    return -1;
  }
  if (missingarg) {
    PrintOut(LOG_CRIT, "File %s line %d (drive %s): Missing argument to %s Directive\n",
             configfile, lineno, name, token);
  }
  if (badarg) {
    PrintOut(LOG_CRIT, "File %s line %d (drive %s): Invalid argument to %s Directive: %s\n",
             configfile, lineno, name, token, arg);
  }
  if (missingarg || badarg) {
    PrintOut(LOG_CRIT, "Valid arguments to %s Directive are: ", token);
    printoutvaliddirectiveargs(LOG_CRIT, sym);
    PrintOut(LOG_CRIT, "\n");
    return -1;
  }

  return 1;
}

// Scan directive for configuration file
#define SCANDIRECTIVE "DEVICESCAN"

// This is the routine that adds things to the conf_entries list.
//
// Return values are:
//  1: parsed a normal line
//  0: found DEFAULT setting or comment or blank line
// -1: found SCANDIRECTIVE line
// -2: found an error
//
// Note: this routine modifies *line from the caller!
static int ParseConfigLine(dev_config_vector & conf_entries, dev_config & default_conf, int lineno, /*const*/ char * line)
{
  const char *delim = " \n\t";

  // get first token: device name. If a comment, skip line
  const char * name = strtok(line, delim);
  if (!name || *name == '#')
    return 0;

  // Check device name for DEFAULT or DEVICESCAN
  int retval;
  if (!strcmp("DEFAULT", name)) {
    retval = 0;
    // Restart with empty defaults
    default_conf = dev_config();
  }
  else {
    retval = (!strcmp(SCANDIRECTIVE, name) ? -1 : 1);
    // Init new entry with current defaults
    conf_entries.push_back(default_conf);
  }
  dev_config & cfg = (retval ? conf_entries.back() : default_conf);

  cfg.name = name; // Later replaced by dev->get_info().info_name
  cfg.dev_name = name; // If DEVICESCAN later replaced by get->dev_info().dev_name
  cfg.lineno = lineno;

  // parse tokens one at a time from the file.
  while (char * token = strtok(0, delim)) {
    int rc = ParseToken(token, cfg);
    if (rc < 0)
      // error found on the line
      return -2;

    if (rc == 0)
      // No tokens left
      break;

    // PrintOut(LOG_INFO,"Parsed token %s\n",token);
  }

  // Don't perform checks below for DEFAULT entries
  if (retval == 0)
    return retval;

  // If NO monitoring directives are set, then set all of them.
  if (!(   cfg.smartcheck  || cfg.selftest
        || cfg.errorlog    || cfg.xerrorlog
        || cfg.offlinests  || cfg.selfteststs
        || cfg.usagefailed || cfg.prefail  || cfg.usage
        || cfg.tempdiff    || cfg.tempinfo || cfg.tempcrit)) {
    
    PrintOut(LOG_INFO,"Drive: %s, implied '-a' Directive on line %d of file %s\n",
             cfg.name.c_str(), cfg.lineno, configfile);
    
    cfg.smartcheck = true;
    cfg.usagefailed = true;
    cfg.prefail = true;
    cfg.usage = true;
    cfg.selftest = true;
    cfg.errorlog = true;
    cfg.selfteststs = true;
  }
  
  // additional sanity check. Has user set -M options without -m?
  if (cfg.emailaddress.empty() && (!cfg.emailcmdline.empty() || cfg.emailfreq || cfg.emailtest)){
    PrintOut(LOG_CRIT,"Drive: %s, -M Directive(s) on line %d of file %s need -m ADDRESS Directive\n",
             cfg.name.c_str(), cfg.lineno, configfile);
    return -2;
  }
  
  // has the user has set <nomailer>?
  if (cfg.emailaddress == "<nomailer>") {
    // check that -M exec is also set
    if (cfg.emailcmdline.empty()){
      PrintOut(LOG_CRIT,"Drive: %s, -m <nomailer> Directive on line %d of file %s needs -M exec Directive\n",
               cfg.name.c_str(), cfg.lineno, configfile);
      return -2;
    }
    // From here on the sign of <nomailer> is cfg.emailaddress.empty() and !cfg.emailcmdline.empty()
    cfg.emailaddress.clear();
  }

  return retval;
}

// Parses a configuration file.  Return values are:
//  N=>0: found N entries
// -1:    syntax error in config file
// -2:    config file does not exist
// -3:    config file exists but cannot be read
//
// In the case where the return value is 0, there are three
// possiblities:
// Empty configuration file ==> conf_entries.empty()
// No configuration file    ==> conf_entries[0].lineno == 0
// SCANDIRECTIVE found      ==> conf_entries.back().lineno != 0 (size >= 1)
static int ParseConfigFile(dev_config_vector & conf_entries)
{
  // maximum line length in configuration file
  const int MAXLINELEN = 256;
  // maximum length of a continued line in configuration file
  const int MAXCONTLINE = 1023;

  stdio_file f;
  // Open config file, if it exists and is not <stdin>
  if (!(configfile == configfile_stdin)) { // pointer comparison ok here
    if (!f.open(configfile,"r") && (errno!=ENOENT || !configfile_alt.empty())) {
      // file exists but we can't read it or it should exist due to '-c' option
      int ret = (errno!=ENOENT ? -3 : -2);
      PrintOut(LOG_CRIT,"%s: Unable to open configuration file %s\n",
               strerror(errno),configfile);
      return ret;
    }
  }
  else // read from stdin ('-c -' option)
    f.open(stdin);

  // Start with empty defaults
  dev_config default_conf;

  // No configuration file found -- use fake one
  int entry = 0;
  if (!f) {
    char fakeconfig[] = SCANDIRECTIVE " -a"; // TODO: Remove this hack, build cfg_entry.

    if (ParseConfigLine(conf_entries, default_conf, 0, fakeconfig) != -1)
      throw std::logic_error("Internal error parsing " SCANDIRECTIVE);
    return 0;
  }

#ifdef __CYGWIN__
  setmode(fileno(f), O_TEXT); // Allow files with \r\n
#endif

  // configuration file exists
  PrintOut(LOG_INFO,"Opened configuration file %s\n",configfile);

  // parse config file line by line
  int lineno = 1, cont = 0, contlineno = 0;
  char line[MAXLINELEN+2];
  char fullline[MAXCONTLINE+1];

  for (;;) {
    int len=0,scandevice;
    char *lastslash;
    char *comment;
    char *code;

    // make debugging simpler
    memset(line,0,sizeof(line));

    // get a line
    code=fgets(line, MAXLINELEN+2, f);
    
    // are we at the end of the file?
    if (!code){
      if (cont) {
        scandevice = ParseConfigLine(conf_entries, default_conf, contlineno, fullline);
        // See if we found a SCANDIRECTIVE directive
        if (scandevice==-1)
          return 0;
        // did we find a syntax error
        if (scandevice==-2)
          return -1;
        // the final line is part of a continuation line
        cont=0;
        entry+=scandevice;
      }
      break;
    }

    // input file line number
    contlineno++;
    
    // See if line is too long
    len=strlen(line);
    if (len>MAXLINELEN){
      const char *warn;
      if (line[len-1]=='\n')
        warn="(including newline!) ";
      else
        warn="";
      PrintOut(LOG_CRIT,"Error: line %d of file %s %sis more than MAXLINELEN=%d characters.\n",
               (int)contlineno,configfile,warn,(int)MAXLINELEN);
      return -1;
    }

    // Ignore anything after comment symbol
    if ((comment=strchr(line,'#'))){
      *comment='\0';
      len=strlen(line);
    }

    // is the total line (made of all continuation lines) too long?
    if (cont+len>MAXCONTLINE){
      PrintOut(LOG_CRIT,"Error: continued line %d (actual line %d) of file %s is more than MAXCONTLINE=%d characters.\n",
               lineno, (int)contlineno, configfile, (int)MAXCONTLINE);
      return -1;
    }
    
    // copy string so far into fullline, and increment length
    snprintf(fullline+cont, sizeof(fullline)-cont, "%s" ,line);
    cont+=len;

    // is this a continuation line.  If so, replace \ by space and look at next line
    if ( (lastslash=strrchr(line,'\\')) && !strtok(lastslash+1," \n\t")){
      *(fullline+(cont-len)+(lastslash-line))=' ';
      continue;
    }

    // Not a continuation line. Parse it
    scandevice = ParseConfigLine(conf_entries, default_conf, contlineno, fullline);

    // did we find a scandevice directive?
    if (scandevice==-1)
      return 0;
    // did we find a syntax error
    if (scandevice==-2)
      return -1;

    entry+=scandevice;
    lineno++;
    cont=0;
  }

  // note -- may be zero if syntax of file OK, but no valid entries!
  return entry;
}

/* Prints the message "=======> VALID ARGUMENTS ARE: <LIST>  <=======\n", where
   <LIST> is the list of valid arguments for option opt. */
static void PrintValidArgs(char opt)
{
  const char *s;

  PrintOut(LOG_CRIT, "=======> VALID ARGUMENTS ARE: ");
  if (!(s = GetValidArgList(opt)))
    PrintOut(LOG_CRIT, "Error constructing argument list for option %c", opt);
  else
    PrintOut(LOG_CRIT, "%s", (char *)s);
  PrintOut(LOG_CRIT, " <=======\n");
}

#ifndef _WIN32
// Report error and exit if specified path is not absolute.
static void check_abs_path(char option, const std::string & path)
{
  if (path.empty() || path[0] == '/')
    return;

  debugmode = 1;
  PrintHead();
  PrintOut(LOG_CRIT, "=======> INVALID ARGUMENT TO -%c: %s <=======\n\n", option, path.c_str());
  PrintOut(LOG_CRIT, "Error: relative path names are not allowed\n\n");
  EXIT(EXIT_BADCMD);
}
#endif // !_WIN32

// Parses input line, prints usage message and
// version/license/copyright messages
static void ParseOpts(int argc, char **argv)
{
  // Init default path names
#ifndef _WIN32
  configfile = SMARTMONTOOLS_SYSCONFDIR "/smartd.conf";
  warning_script = SMARTMONTOOLS_SMARTDSCRIPTDIR "/smartd_warning.sh";
#else
  std::string exedir = get_exe_dir();
  static std::string configfile_str = exedir + "/smartd.conf";
  configfile = configfile_str.c_str();
  warning_script = exedir + "/smartd_warning.cmd";
#endif

  // Please update GetValidArgList() if you edit shortopts
  static const char shortopts[] = "c:l:q:dDni:p:r:s:A:B:w:Vh?"
#ifdef HAVE_LIBCAP_NG
                                                          "C"
#endif
                                                             ;
  // Please update GetValidArgList() if you edit longopts
  struct option longopts[] = {
    { "configfile",     required_argument, 0, 'c' },
    { "logfacility",    required_argument, 0, 'l' },
    { "quit",           required_argument, 0, 'q' },
    { "debug",          no_argument,       0, 'd' },
    { "showdirectives", no_argument,       0, 'D' },
    { "interval",       required_argument, 0, 'i' },
#ifndef _WIN32
    { "no-fork",        no_argument,       0, 'n' },
#else
    { "service",        no_argument,       0, 'n' },
#endif
    { "pidfile",        required_argument, 0, 'p' },
    { "report",         required_argument, 0, 'r' },
    { "savestates",     required_argument, 0, 's' },
    { "attributelog",   required_argument, 0, 'A' },
    { "drivedb",        required_argument, 0, 'B' },
    { "warnexec",       required_argument, 0, 'w' },
    { "version",        no_argument,       0, 'V' },
    { "license",        no_argument,       0, 'V' },
    { "copyright",      no_argument,       0, 'V' },
    { "help",           no_argument,       0, 'h' },
    { "usage",          no_argument,       0, 'h' },
#ifdef HAVE_LIBCAP_NG
    { "capabilities",   no_argument,       0, 'C' },
#endif
    { 0,                0,                 0, 0   }
  };

  opterr=optopt=0;
  bool badarg = false;
  bool no_defaultdb = false; // set true on '-B FILE'

  // Parse input options.
  int optchar;
  while ((optchar = getopt_long(argc, argv, shortopts, longopts, NULL)) != -1) {
    char *arg;
    char *tailptr;
    long lchecktime;

    switch(optchar) {
    case 'q':
      // when to quit
      if (!(strcmp(optarg,"nodev"))) {
        quit=0;
      } else if (!(strcmp(optarg,"nodevstartup"))) {
        quit=1;
      } else if (!(strcmp(optarg,"never"))) {
        quit=2;
      } else if (!(strcmp(optarg,"onecheck"))) {
        quit=3;
        debugmode=1;
      } else if (!(strcmp(optarg,"showtests"))) {
        quit=4;
        debugmode=1;
      } else if (!(strcmp(optarg,"errors"))) {
        quit=5;
      } else {
        badarg = true;
      }
      break;
    case 'l':
      // set the log facility level
      if (!strcmp(optarg, "daemon"))
        facility=LOG_DAEMON;
      else if (!strcmp(optarg, "local0"))
        facility=LOG_LOCAL0;
      else if (!strcmp(optarg, "local1"))
        facility=LOG_LOCAL1;
      else if (!strcmp(optarg, "local2"))
        facility=LOG_LOCAL2;
      else if (!strcmp(optarg, "local3"))
        facility=LOG_LOCAL3;
      else if (!strcmp(optarg, "local4"))
        facility=LOG_LOCAL4;
      else if (!strcmp(optarg, "local5"))
        facility=LOG_LOCAL5;
      else if (!strcmp(optarg, "local6"))
        facility=LOG_LOCAL6;
      else if (!strcmp(optarg, "local7"))
        facility=LOG_LOCAL7;
      else
        badarg = true;
      break;
    case 'd':
      // enable debug mode
      debugmode = 1;
      break;
    case 'n':
      // don't fork()
#ifndef _WIN32 // On Windows, --service is already handled by daemon_main()
      do_fork = false;
#endif
      break;
    case 'D':
      // print summary of all valid directives
      debugmode = 1;
      Directives();
      EXIT(0);
      break;
    case 'i':
      // Period (time interval) for checking
      // strtol will set errno in the event of overflow, so we'll check it.
      errno = 0;
      lchecktime = strtol(optarg, &tailptr, 10);
      if (*tailptr != '\0' || lchecktime < 10 || lchecktime > INT_MAX || errno) {
        debugmode=1;
        PrintHead();
        PrintOut(LOG_CRIT, "======> INVALID INTERVAL: %s <=======\n", optarg);
        PrintOut(LOG_CRIT, "======> INTERVAL MUST BE INTEGER BETWEEN %d AND %d <=======\n", 10, INT_MAX);
        PrintOut(LOG_CRIT, "\nUse smartd -h to get a usage summary\n\n");
        EXIT(EXIT_BADCMD);
      }
      checktime = (int)lchecktime;
      break;
    case 'r':
      // report IOCTL transactions
      {
        int i;
        char *s;

        // split_report_arg() may modify its first argument string, so use a
        // copy of optarg in case we want optarg for an error message.
        if (!(s = strdup(optarg))) {
          PrintOut(LOG_CRIT, "No memory to process -r option - exiting\n");
          EXIT(EXIT_NOMEM);
        }
        if (split_report_arg(s, &i)) {
          badarg = true;
        } else if (i<1 || i>3) {
          debugmode=1;
          PrintHead();
          PrintOut(LOG_CRIT, "======> INVALID REPORT LEVEL: %s <=======\n", optarg);
          PrintOut(LOG_CRIT, "======> LEVEL MUST BE INTEGER BETWEEN 1 AND 3<=======\n");
          EXIT(EXIT_BADCMD);
        } else if (!strcmp(s,"ioctl")) {
          ata_debugmode = scsi_debugmode = i;
        } else if (!strcmp(s,"ataioctl")) {
          ata_debugmode = i;
        } else if (!strcmp(s,"scsiioctl")) {
          scsi_debugmode = i;
        } else {
          badarg = true;
        }
        free(s);  // TODO: use std::string
      }
      break;
    case 'c':
      // alternate configuration file
      if (strcmp(optarg,"-"))
        configfile = (configfile_alt = optarg).c_str();
      else // read from stdin
        configfile=configfile_stdin;
      break;
    case 'p':
      // output file with PID number
      pid_file = optarg;
      break;
    case 's':
      // path prefix of persistent state file
      state_path_prefix = optarg;
      break;
    case 'A':
      // path prefix of attribute log file
      attrlog_path_prefix = optarg;
      break;
    case 'B':
      {
        const char * path = optarg;
        if (*path == '+' && path[1])
          path++;
        else
          no_defaultdb = true;
        unsigned char savedebug = debugmode; debugmode = 1;
        if (!read_drive_database(path))
          EXIT(EXIT_BADCMD);
        debugmode = savedebug;
      }
      break;
    case 'w':
      warning_script = optarg;
      break;
    case 'V':
      // print version and CVS info
      debugmode = 1;
      PrintOut(LOG_INFO, "%s", format_version_info("smartd", true /*full*/).c_str());
      EXIT(0);
      break;
#ifdef HAVE_LIBCAP_NG
    case 'C':
      // enable capabilities
      enable_capabilities = true;
      break;
#endif
    case 'h':
      // help: print summary of command-line options
      debugmode=1;
      PrintHead();
      Usage();
      EXIT(0);
      break;
    case '?':
    default:
      // unrecognized option
      debugmode=1;
      PrintHead();
      // Point arg to the argument in which this option was found.
      arg = argv[optind-1];
      // Check whether the option is a long option that doesn't map to -h.
      if (arg[1] == '-' && optchar != 'h') {
        // Iff optopt holds a valid option then argument must be missing.
        if (optopt && (strchr(shortopts, optopt) != NULL)) {
          PrintOut(LOG_CRIT, "=======> ARGUMENT REQUIRED FOR OPTION: %s <=======\n",arg+2);
          PrintValidArgs(optopt);
        } else {
          PrintOut(LOG_CRIT, "=======> UNRECOGNIZED OPTION: %s <=======\n\n",arg+2);
        }
        PrintOut(LOG_CRIT, "\nUse smartd --help to get a usage summary\n\n");
        EXIT(EXIT_BADCMD);
      }
      if (optopt) {
        // Iff optopt holds a valid option then argument must be missing.
        if (strchr(shortopts, optopt) != NULL){
          PrintOut(LOG_CRIT, "=======> ARGUMENT REQUIRED FOR OPTION: %c <=======\n",optopt);
          PrintValidArgs(optopt);
        } else {
          PrintOut(LOG_CRIT, "=======> UNRECOGNIZED OPTION: %c <=======\n\n",optopt);
        }
        PrintOut(LOG_CRIT, "\nUse smartd -h to get a usage summary\n\n");
        EXIT(EXIT_BADCMD);
      }
      Usage();
      EXIT(0);
    }

    // Check to see if option had an unrecognized or incorrect argument.
    if (badarg) {
      debugmode=1;
      PrintHead();
      // It would be nice to print the actual option name given by the user
      // here, but we just print the short form.  Please fix this if you know
      // a clean way to do it.
      PrintOut(LOG_CRIT, "=======> INVALID ARGUMENT TO -%c: %s <======= \n", optchar, optarg);
      PrintValidArgs(optchar);
      PrintOut(LOG_CRIT, "\nUse smartd -h to get a usage summary\n\n");
      EXIT(EXIT_BADCMD);
    }
  }

  // non-option arguments are not allowed
  if (argc > optind) {
    debugmode=1;
    PrintHead();
    PrintOut(LOG_CRIT, "=======> UNRECOGNIZED ARGUMENT: %s <=======\n\n", argv[optind]);
    PrintOut(LOG_CRIT, "\nUse smartd -h to get a usage summary\n\n");
    EXIT(EXIT_BADCMD);
  }

  // no pidfile in debug mode
  if (debugmode && !pid_file.empty()) {
    debugmode=1;
    PrintHead();
    PrintOut(LOG_CRIT, "=======> INVALID CHOICE OF OPTIONS: -d and -p <======= \n\n");
    PrintOut(LOG_CRIT, "Error: pid file %s not written in debug (-d) mode\n\n", pid_file.c_str());
    EXIT(EXIT_BADCMD);
  }

#ifndef _WIN32
  if (!debugmode) {
    // absolute path names are required due to chdir('/') after fork().
    check_abs_path('p', pid_file);
    check_abs_path('s', state_path_prefix);
    check_abs_path('A', attrlog_path_prefix);
  }
#endif

  // Read or init drive database
  if (!no_defaultdb) {
    unsigned char savedebug = debugmode; debugmode = 1;
    if (!read_default_drive_databases())
        EXIT(EXIT_BADCMD);
    debugmode = savedebug;
  }

  // print header
  PrintHead();
}

// Function we call if no configuration file was found or if the
// SCANDIRECTIVE Directive was found.  It makes entries for device
// names returned by scan_smart_devices() in os_OSNAME.cpp
static int MakeConfigEntries(const dev_config & base_cfg,
  dev_config_vector & conf_entries, smart_device_list & scanned_devs, const char * type)
{
  // make list of devices
  smart_device_list devlist;
  if (!smi()->scan_smart_devices(devlist, (*type ? type : 0)))
    PrintOut(LOG_CRIT,"Problem creating device name scan list\n");
  
  // if no devices, or error constructing list, return
  if (devlist.size() <= 0)
    return 0;

  // add empty device slots for existing config entries
  while (scanned_devs.size() < conf_entries.size())
    scanned_devs.push_back((smart_device *)0);

  // loop over entries to create
  for (unsigned i = 0; i < devlist.size(); i++) {
    // Move device pointer
    smart_device * dev = devlist.release(i);
    scanned_devs.push_back(dev);

    // Copy configuration, update device and type name
    conf_entries.push_back(base_cfg);
    dev_config & cfg = conf_entries.back();
    cfg.name = dev->get_info().info_name;
    cfg.dev_name = dev->get_info().dev_name;
    cfg.dev_type = type;
  }
  
  return devlist.size();
}
 
static void CanNotRegister(const char *name, const char *type, int line, bool scandirective)
{
  if (!debugmode && scandirective)
    return;
  if (line)
    PrintOut(scandirective?LOG_INFO:LOG_CRIT,
             "Unable to register %s device %s at line %d of file %s\n",
             type, name, line, configfile);
  else
    PrintOut(LOG_INFO,"Unable to register %s device %s\n",
             type, name);
  return;
}

// Returns negative value (see ParseConfigFile()) if config file
// had errors, else number of entries which may be zero or positive. 
static int ReadOrMakeConfigEntries(dev_config_vector & conf_entries, smart_device_list & scanned_devs)
{
  // parse configuration file configfile (normally /etc/smartd.conf)  
  int entries = ParseConfigFile(conf_entries);

  if (entries < 0) {
    // There was an error reading the configuration file.
    conf_entries.clear();
    if (entries == -1)
      PrintOut(LOG_CRIT, "Configuration file %s has fatal syntax errors.\n", configfile);
    return entries;
  }

  // no error parsing config file.
  if (entries) {
    // we did not find a SCANDIRECTIVE and did find valid entries
    PrintOut(LOG_INFO, "Configuration file %s parsed.\n", configfile);
  }
  else if (!conf_entries.empty()) {
    // we found a SCANDIRECTIVE or there was no configuration file so
    // scan.  Configuration file's last entry contains all options
    // that were set
    dev_config first = conf_entries.back();
    conf_entries.pop_back();

    if (first.lineno)
      PrintOut(LOG_INFO,"Configuration file %s was parsed, found %s, scanning devices\n", configfile, SCANDIRECTIVE);
    else
      PrintOut(LOG_INFO,"No configuration file %s found, scanning devices\n", configfile);
    
    // make config list of devices to search for
    MakeConfigEntries(first, conf_entries, scanned_devs, first.dev_type.c_str());

    // warn user if scan table found no devices
    if (conf_entries.empty())
      PrintOut(LOG_CRIT,"In the system's table of devices NO devices found to scan\n");
  } 
  else
    PrintOut(LOG_CRIT, "Configuration file %s parsed but has no entries\n", configfile);
  
  return conf_entries.size();
}

// Return true if TYPE contains a RAID drive number
static bool is_raid_type(const char * type)
{
  if (str_starts_with(type, "sat,"))
    return false;
  int i;
  if (sscanf(type, "%*[^,],%d", &i) != 1)
    return false;
  return true;
}

// Return true if DEV is already in DEVICES[0..NUMDEVS) or IGNORED[*]
static bool is_duplicate_device(const smart_device * dev,
                                const smart_device_list & devices, unsigned numdevs,
                                const dev_config_vector & ignored)
{
  const smart_device::device_info & info1 = dev->get_info();
  bool is_raid1 = is_raid_type(info1.dev_type.c_str());

  for (unsigned i = 0; i < numdevs; i++) {
    const smart_device::device_info & info2 = devices.at(i)->get_info();
    // -d TYPE options must match if RAID drive number is specified
    if (   info1.dev_name == info2.dev_name
        && (   info1.dev_type == info2.dev_type
            || !is_raid1 || !is_raid_type(info2.dev_type.c_str())))
      return true;
  }

  for (unsigned i = 0; i < ignored.size(); i++) {
    const dev_config & cfg2 = ignored.at(i);
    if (   info1.dev_name == cfg2.dev_name
        && (   info1.dev_type == cfg2.dev_type
            || !is_raid1 || !is_raid_type(cfg2.dev_type.c_str())))
      return true;
  }
  return false;
}

// This function tries devices from conf_entries.  Each one that can be
// registered is moved onto the [ata|scsi]devices lists and removed
// from the conf_entries list.
static void RegisterDevices(const dev_config_vector & conf_entries, smart_device_list & scanned_devs,
                            dev_config_vector & configs, dev_state_vector & states, smart_device_list & devices)
{
  // start by clearing lists/memory of ALL existing devices
  configs.clear();
  devices.clear();
  states.clear();

  // Register entries
  dev_config_vector ignored_entries;
  unsigned numnoscan = 0;
  for (unsigned i = 0; i < conf_entries.size(); i++){

    dev_config cfg = conf_entries[i];

    if (cfg.ignore) {
      // Store for is_duplicate_device() check and ignore
      PrintOut(LOG_INFO, "Device: %s%s%s%s, ignored\n", cfg.name.c_str(),
               (!cfg.dev_type.empty() ? " [" : ""),
               cfg.dev_type.c_str(),
               (!cfg.dev_type.empty() ? "]" : ""));
      ignored_entries.push_back(cfg);
      continue;
    }

    // get device of appropriate type
    smart_device_auto_ptr dev;
    bool scanning = false;

    // Device may already be detected during devicescan
    if (i < scanned_devs.size()) {
      dev = scanned_devs.release(i);
      if (dev) {
        // Check for a preceding non-DEVICESCAN entry for the same device
        if (  (numnoscan || !ignored_entries.empty())
            && is_duplicate_device(dev.get(), devices, numnoscan, ignored_entries)) {
          PrintOut(LOG_INFO, "Device: %s, duplicate, ignored\n", dev->get_info_name());
          continue;
        }
        scanning = true;
      }
    }

    if (!dev) {
      dev = smi()->get_smart_device(cfg.name.c_str(), cfg.dev_type.c_str());
      if (!dev) {
        if (cfg.dev_type.empty())
          PrintOut(LOG_INFO,"Device: %s, unable to autodetect device type\n", cfg.name.c_str());
        else
          PrintOut(LOG_INFO,"Device: %s, unsupported device type '%s'\n", cfg.name.c_str(), cfg.dev_type.c_str());
        continue;
      }
    }

    // Save old info
    smart_device::device_info oldinfo = dev->get_info();

    // Open with autodetect support, may return 'better' device
    dev.replace( dev->autodetect_open() );

    // Report if type has changed
    if (oldinfo.dev_type != dev->get_dev_type())
      PrintOut(LOG_INFO,"Device: %s, type changed from '%s' to '%s'\n",
        cfg.name.c_str(), oldinfo.dev_type.c_str(), dev->get_dev_type());

    if (!dev->is_open()) {
      // For linux+devfs, a nonexistent device gives a strange error
      // message.  This makes the error message a bit more sensible.
      // If no debug and scanning - don't print errors
      if (debugmode || !scanning)
        PrintOut(LOG_INFO, "Device: %s, open() failed: %s\n", dev->get_info_name(), dev->get_errmsg());
      continue;
    }

    // Update informal name
    cfg.name = dev->get_info().info_name;
    PrintOut(LOG_INFO, "Device: %s, opened\n", cfg.name.c_str());

    // Prepare initial state
    dev_state state;

    // register ATA devices
    if (dev->is_ata()){
      if (ATADeviceScan(cfg, state, dev->to_ata())) {
        CanNotRegister(cfg.name.c_str(), "ATA", cfg.lineno, scanning);
        dev.reset();
      }
    }
    // or register SCSI devices
    else if (dev->is_scsi()){
      if (SCSIDeviceScan(cfg, state, dev->to_scsi())) {
        CanNotRegister(cfg.name.c_str(), "SCSI", cfg.lineno, scanning);
        dev.reset();
      }
    }
    else {
      PrintOut(LOG_INFO, "Device: %s, neither ATA nor SCSI device\n", cfg.name.c_str());
      dev.reset();
    }

    if (dev) {
      // move onto the list of devices
      configs.push_back(cfg);
      states.push_back(state);
      devices.push_back(dev);
      if (!scanning)
        numnoscan = devices.size();
    }
    // if device is explictly listed and we can't register it, then
    // exit unless the user has specified that the device is removable
    else if (!scanning) {
      if (cfg.removable || quit==2)
        PrintOut(LOG_INFO, "Device %s not available\n", cfg.name.c_str());
      else {
        PrintOut(LOG_CRIT, "Unable to register device %s (no Directive -d removable). Exiting.\n", cfg.name.c_str());
        EXIT(EXIT_BADDEV);
      }
    }
  }

  init_disable_standby_check(configs);
}


// Main program without exception handling
static int main_worker(int argc, char **argv)
{
  // Initialize interface
  smart_interface::init();
  if (!smi())
    return 1;

  // is it our first pass through?
  bool firstpass = true;

  // next time to wake up
  time_t wakeuptime = 0;

  // parse input and print header and usage info if needed
  ParseOpts(argc,argv);
  
  // Configuration for each device
  dev_config_vector configs;
  // Device states
  dev_state_vector states;
  // Devices to monitor
  smart_device_list devices;

  bool write_states_always = true;

#ifdef HAVE_LIBCAP_NG
  // Drop capabilities
  if (enable_capabilities) {
    capng_clear(CAPNG_SELECT_BOTH);
    capng_updatev(CAPNG_ADD, (capng_type_t)(CAPNG_EFFECTIVE|CAPNG_PERMITTED),
                  CAP_SYS_ADMIN, CAP_MKNOD, CAP_SYS_RAWIO, -1);
    capng_apply(CAPNG_SELECT_BOTH);
  }
#endif

  // the main loop of the code
  for (;;) {

    // are we exiting from a signal?
    if (caughtsigEXIT) {
      // are we exiting with SIGTERM?
      int isterm=(caughtsigEXIT==SIGTERM);
      int isquit=(caughtsigEXIT==SIGQUIT);
      int isok=debugmode?isterm || isquit:isterm;
      
      PrintOut(isok?LOG_INFO:LOG_CRIT, "smartd received signal %d: %s\n",
               caughtsigEXIT, strsignal(caughtsigEXIT));

      if (!isok)
        return EXIT_SIGNAL;

      // Write state files
      if (!state_path_prefix.empty())
        write_all_dev_states(configs, states);

      return 0;
    }

    // Should we (re)read the config file?
    if (firstpass || caughtsigHUP){
      if (!firstpass) {
        // Write state files
        if (!state_path_prefix.empty())
          write_all_dev_states(configs, states);

        PrintOut(LOG_INFO,
                 caughtsigHUP==1?
                 "Signal HUP - rereading configuration file %s\n":
                 "\a\nSignal INT - rereading configuration file %s (" SIGQUIT_KEYNAME " quits)\n\n",
                 configfile);
      }

      {
        dev_config_vector conf_entries; // Entries read from smartd.conf
        smart_device_list scanned_devs; // Devices found during scan
        // (re)reads config file, makes >=0 entries
        int entries = ReadOrMakeConfigEntries(conf_entries, scanned_devs);

        if (entries>=0) {
          // checks devices, then moves onto ata/scsi list or deallocates.
          RegisterDevices(conf_entries, scanned_devs, configs, states, devices);
          if (!(configs.size() == devices.size() && configs.size() == states.size()))
            throw std::logic_error("Invalid result from RegisterDevices");
        }
        else if (quit==2 || ((quit==0 || quit==1) && !firstpass)) {
          // user has asked to continue on error in configuration file
          if (!firstpass)
            PrintOut(LOG_INFO,"Reusing previous configuration\n");
        }
        else {
          // exit with configuration file error status
          return (entries==-3 ? EXIT_READCONF : entries==-2 ? EXIT_NOCONF : EXIT_BADCONF);
        }
      }

      // Log number of devices we are monitoring...
      if (devices.size() > 0 || quit==2 || (quit==1 && !firstpass)) {
        int numata = 0;
        for (unsigned i = 0; i < devices.size(); i++) {
          if (devices.at(i)->is_ata())
            numata++;
        }
        PrintOut(LOG_INFO,"Monitoring %d ATA and %d SCSI devices\n",
                 numata, devices.size() - numata);
      }
      else {
        PrintOut(LOG_INFO,"Unable to monitor any SMART enabled devices. Try debug (-d) option. Exiting...\n");
        return EXIT_NODEV;
      }

      if (quit==4) {
        // user has asked to print test schedule
        PrintTestSchedule(configs, states, devices);
        return 0;
      }

#ifdef HAVE_LIBCAP_NG
      if (enable_capabilities) {
        for (unsigned i = 0; i < configs.size(); i++) {
          if (!configs[i].emailaddress.empty() || !configs[i].emailcmdline.empty()) {
            PrintOut(LOG_WARNING, "Mail can't be enabled together with --capabilities. All mail will be suppressed.\n");
            break;
          }
        }
      }
#endif

      // reset signal
      caughtsigHUP=0;

      // Always write state files after (re)configuration
      write_states_always = true;
    }

    // check all devices once,
    // self tests are not started in first pass unless '-q onecheck' is specified
    CheckDevicesOnce(configs, states, devices, firstpass, (!firstpass || quit==3));

     // Write state files
    if (!state_path_prefix.empty())
      write_all_dev_states(configs, states, write_states_always);
    write_states_always = false;

    // Write attribute logs
    if (!attrlog_path_prefix.empty())
      write_all_dev_attrlogs(configs, states);

    // user has asked us to exit after first check
    if (quit==3) {
      PrintOut(LOG_INFO,"Started with '-q onecheck' option. All devices sucessfully checked once.\n"
               "smartd is exiting (exit status 0)\n");
      return 0;
    }
    
    // fork into background if needed
    if (firstpass && !debugmode) {
      DaemonInit();
    }

    // set exit and signal handlers, write PID file, set wake-up time
    if (firstpass){
      Initialize(&wakeuptime);
      firstpass = false;
    }
    
    // sleep until next check time, or a signal arrives
    wakeuptime = dosleep(wakeuptime, write_states_always);
  }
}


#ifndef _WIN32
// Main program
int main(int argc, char **argv)
#else
// Windows: internal main function started direct or by service control manager
static int smartd_main(int argc, char **argv)
#endif
{
  int status;
  try {
    // Do the real work ...
    status = main_worker(argc, argv);
  }
  catch (int ex) {
    // EXIT(status) arrives here
    status = ex;
  }
  catch (const std::bad_alloc & /*ex*/) {
    // Memory allocation failed (also thrown by std::operator new)
    PrintOut(LOG_CRIT, "Smartd: Out of memory\n");
    status = EXIT_NOMEM;
  }
  catch (const std::exception & ex) {
    // Other fatal errors
    PrintOut(LOG_CRIT, "Smartd: Exception: %s\n", ex.what());
    status = EXIT_BADCODE;
  }

  if (is_initialized)
    status = Goodbye(status);

#ifdef _WIN32
  daemon_winsvc_exitcode = status;
#endif
  return status;
}


#ifdef _WIN32
// Main function for Windows
int main(int argc, char **argv){
  // Options for smartd windows service
  static const daemon_winsvc_options svc_opts = {
    "--service", // cmd_opt
    "smartd", "SmartD Service", // servicename, displayname
    // description
    "Controls and monitors storage devices using the Self-Monitoring, "
    "Analysis and Reporting Technology System (SMART) built into "
    "ATA/SATA and SCSI/SAS hard drives and solid-state drives. "
    "www.smartmontools.org"
  };
  // daemon_main() handles daemon and service specific commands
  // and starts smartd_main() direct, from a new process,
  // or via service control manager
  return daemon_main("smartd", &svc_opts , smartd_main, argc, argv);
}
#endif
