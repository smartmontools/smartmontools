/*
 * Home page of code is: http://smartmontools.sourceforge.net
 *
 * Copyright (C) 2002-9 Bruce Allen <smartmontools-support@lists.sourceforge.net>
 * Copyright (C) 2000   Michael Cornwell <cornwell@acm.org>
 * Copyright (C) 2008   Oliver Bock <brevilo@users.sourceforge.net>
 * Copyright (C) 2008-9 Christian Franke <smartmontools-support@lists.sourceforge.net>
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

#ifndef _GNU_SOURCE
// TODO: Why is this define necessary?
#define _GNU_SOURCE
#endif

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

// see which system files to conditionally include
#include "config.h"

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
// From <windows.h>:
// BOOL WINAPI FreeConsole(void);
extern "C" int __stdcall FreeConsole(void);
#include <io.h> // setmode()
#endif // __CYGWIN__

// locally included files
#include "int64.h"
#include "atacmds.h"
#include "dev_interface.h"
#include "extern.h"
#include "knowndrives.h"
#include "scsicmds.h"
#include "scsiata.h"
#include "utility.h"

// This is for solaris, where signal() resets the handler to SIG_DFL
// after the first signal is caught.
#ifdef HAVE_SIGSET
#define SIGNALFN sigset
#else
#define SIGNALFN signal
#endif

#ifdef _WIN32
#include "hostname_win32.h" // gethost/domainname()
#define HAVE_GETHOSTNAME   1
#define HAVE_GETDOMAINNAME 1
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
#ifdef __CYGWIN__
// 2x CONTROL-C simulates missing SIGQUIT via keyboard
#define SIGQUIT_KEYNAME "2x CONTROL-C"
#else // __CYGWIN__
#define SIGQUIT_KEYNAME "CONTROL-\\"
#endif // __CYGWIN__
#endif // _WIN32

#if defined (__SVR4) && defined (__sun)
extern "C" int getdomainname(char *, int); // no declaration in header files!
#endif

#define ARGUSED(x) ((void)(x))

// These are CVS identification information for *.cpp and *.h files
extern const char *atacmdnames_c_cvsid, *atacmds_c_cvsid,
                  *knowndrives_c_cvsid, *os_XXXX_c_cvsid, *scsicmds_c_cvsid, *utility_c_cvsid;

#ifdef _HAVE_CCISS
extern const char *cciss_c_cvsid;
#endif
#ifdef NEED_SOLARIS_ATA_CODE
extern const char *os_solaris_ata_s_cvsid;
#endif
#ifdef _WIN32
extern const char *daemon_win32_c_cvsid, *hostname_win32_c_cvsid, *syslog_win32_c_cvsid;
#endif
const char *smartd_c_cvsid="$Id: smartd.cpp,v 1.450 2009/07/10 20:14:40 chrfranke Exp $"
ATACMDS_H_CVSID CONFIG_H_CVSID
#ifdef DAEMON_WIN32_H_CVSID
DAEMON_WIN32_H_CVSID
#endif
EXTERN_H_CVSID INT64_H_CVSID
#ifdef HOSTNAME_WIN32_H_CVSID
HOSTNAME_WIN32_H_CVSID
#endif
KNOWNDRIVES_H_CVSID SCSICMDS_H_CVSID
#ifdef SYSLOG_H_CVSID
SYSLOG_H_CVSID
#endif
UTILITY_H_CVSID;

extern const char *reportbug;

// GNU copyleft statement.  Needed for GPL purposes.
const char *copyleftstring="smartd comes with ABSOLUTELY NO WARRANTY. This is\n"
                           "free software, and you are welcome to redistribute it\n"
                           "under the terms of the GNU General Public License\n"
                           "Version 2. See http://www.gnu.org for further details.\n\n";

extern unsigned char debugmode;

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

// configuration file name
#define CONFIGFILENAME "smartd.conf"

#ifndef _WIN32
static const char *configfile = SMARTMONTOOLS_SYSCONFDIR "/" CONFIGFILENAME ;
#else
static const char *configfile = "./" CONFIGFILENAME ;
#endif
// configuration file "name" if read from stdin
static const char * const configfile_stdin = "<stdin>";
// path of alternate configuration file
static std::string configfile_alt;

// command-line: when should we exit?
static int quit=0;

// command-line; this is the default syslog(3) log facility to use.
static int facility=LOG_DAEMON;

#ifndef _WIN32
// command-line: fork into background?
static bool do_fork=true;
#endif

// used for control of printing, passing arguments to atacmds.c
smartmonctrl *con=NULL;

// set to one if we catch a USR1 (check devices now)
volatile int caughtsigUSR1=0;

#ifdef _WIN32
// set to one if we catch a USR2 (toggle debug mode)
volatile int caughtsigUSR2=0;
#endif

// set to one if we catch a HUP (reload config file). In debug mode,
// set to two, if we catch INT (also reload config file).
volatile int caughtsigHUP=0;

// set to signal value if we catch INT, QUIT, or TERM
volatile int caughtsigEXIT=0;

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
    { return (0 < id && id < NUMBER_ATA_SMART_ATTRIBUTES
              && (m_flags[id] & flag)                   ); }

  void set(int id, unsigned char flags)
    {
      if (0 < id && id < NUMBER_ATA_SMART_ATTRIBUTES)
        m_flags[id] |= flags;
    }

private:
  unsigned char m_flags[NUMBER_ATA_SMART_ATTRIBUTES];
};


/// Configuration data for a device. Read from smartd.conf.
/// Supports copy & assignment and is compatible with STL containers.
struct dev_config
{
  int lineno;                             // Line number of entry in file
  std::string name;                       // Device name
  std::string dev_type;                   // Device type argument from -d directive, empty if none
  std::string state_file;                 // Path of the persistent state file, empty if none
  bool smartcheck;                        // Check SMART status
  bool usagefailed;                       // Check for failed Usage Attributes
  bool prefail;                           // Track changes in Prefail Attributes
  bool usage;                             // Track changes in Usage Attributes
  bool selftest;                          // Monitor number of selftest errors
  bool errorlog;                          // Monitor number of ATA errors
  bool permissive;                        // Ignore failed SMART commands
  char autosave;                          // 1=disable, 2=enable Autosave Attributes
  char autoofflinetest;                   // 1=disable, 2=enable Auto Offline Test
  unsigned char fix_firmwarebug;          // FIX_*, see atacmds.h
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
  unsigned char curr_pending_id;          // ID of current pending sector count, 0 if none
  unsigned char offl_pending_id;          // ID of offline uncorrectable sector count, 0 if none
  bool curr_pending_incr, offl_pending_incr; // True if current/offline pending values increase
  bool curr_pending_set,  offl_pending_set;  // True if '-C', '-U' set in smartd.conf

  attribute_flags monitor_attr_flags;     // MONITOR_* flags for each attribute

  // TODO: Encapsulate, add get/set functions
  unsigned char attributedefs[256];       // -v options, see end of extern.h for def

  dev_config();
};

dev_config::dev_config()
: lineno(0),
  smartcheck(false),
  usagefailed(false),
  prefail(false),
  usage(false),
  selftest(false),
  errorlog(false),
  permissive(false),
  autosave(0),
  autoofflinetest(0),
  fix_firmwarebug(FIX_NOTSPECIFIED),
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
  curr_pending_id(0), offl_pending_id(0),
  curr_pending_incr(false), offl_pending_incr(false),
  curr_pending_set(false),  offl_pending_set(false)
{
  memset(attributedefs, 0, sizeof(attributedefs));
}


// Number of allowed mail message types
const int SMARTD_NMAIL = 13;
// Type for '-M test' mails (state not persistent)
const int MAILTYPE_TEST = 0;
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

  mailinfo maillog[SMARTD_NMAIL];         // log info on when mail sent

  // ATA ONLY
  int ataerrorcount;                      // Total number of ATA errors

  // Persistent part of ata_smart_values:
  struct ata_attribute {
    unsigned char id;
    unsigned char val;
    uint64_t raw;

    ata_attribute() : id(0), val(0), raw(0) { }
  };
  ata_attribute ata_attributes[NUMBER_ATA_SMART_ATTRIBUTES];

  persistent_dev_state();
};

persistent_dev_state::persistent_dev_state()
: tempmin(0), tempmax(0),
  selflogcount(0),
  selfloghour(0),
  scheduled_test_next_check(0),
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
  unsigned char SuppressReport;           // minimize nuisance reports
  unsigned char modese_len;               // mode sense/select cmd len: 0 (don't
                                          // know yet) 6 or 10

  // ATA ONLY
  uint64_t num_sectors;                   // Number of sectors (for selective self-test only)
  ata_smart_values smartval;              // SMART data
  ata_smart_thresholds_pvt smartthres;    // SMART thresholds

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
  SuppressReport(false),
  modese_len(0),
  num_sectors(0)
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
      pa.val = 0; pa.raw = 0;
      continue;
    }
    pa.val = ta.current;
    pa.raw =            ta.raw[0]
           | (          ta.raw[1] <<  8)
           | (          ta.raw[2] << 16)
           | ((uint64_t)ta.raw[3] << 24)
           | ((uint64_t)ta.raw[4] << 32)
           | ((uint64_t)ta.raw[5] << 40);
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
      ta.current = 0; memset(ta.raw, 0, sizeof(ta.raw));
      continue;
    }
    ta.current = pa.val;
    ta.raw[0] = (unsigned char) pa.raw;
    ta.raw[1] = (unsigned char)(pa.raw >>  8);
    ta.raw[2] = (unsigned char)(pa.raw >> 16);
    ta.raw[3] = (unsigned char)(pa.raw >> 24);
    ta.raw[4] = (unsigned char)(pa.raw >> 32);
    ta.raw[5] = (unsigned char)(pa.raw >> 40);
  }
}

// Parse a line from a state file.
static bool parse_dev_state_line(const char * line, persistent_dev_state & state)
{
  static regular_expression regex(
    "^ *"
     "((temperature-min)" // (1 (2)
     "|(temperature-max)" // (3)
     "|(self-test-errors)" // (4)
     "|(self-test-last-err-hour)" // (5)
     "|(scheduled-test-next-check)" // (6)
     "|(ata-error-count)"  // (7)
     "|(mail\\.([0-9]+)\\." // (8 (9)
       "((count)" // (10 (11)
       "|(first-sent-time)" // (12)
       "|(last-sent-time)" // (13)
       ")" // 14)
      ")" // 8)
     "|(ata-smart-attribute\\.([0-9]+)\\." // (14 (15)
       "((id)" // (16)
       "|(val)" // (17)
       "|(raw)" // (18)
       ")" // 19)
      ")" // 14)
     ")" // 1)
     " *= *([0-9]+)[ \n]*$", // (20)
    REG_EXTENDED
  );
  if (regex.empty())
    throw std::logic_error("parse_dev_state_line: invalid regex");

  const int nmatch = 1+20;
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
      state.ata_attributes[i].raw = val;
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

  int good = 0, bad = 0;
  char line[256];
  while (fgets(line, sizeof(line), f)) {
    const char * s = line + strspn(line, " \t");
    if (!*s || *s == '#')
      continue;
    if (!parse_dev_state_line(line, state))
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
  return true;
}

static void write_dev_state_line(FILE * f, const char * name, uint64_t val)
{
  if (val)
    fprintf(f, "%s = %"PRIu64"\n", name, val);
}

static void write_dev_state_line(FILE * f, const char * name1, int id, const char * name2, uint64_t val)
{
  if (val)
    fprintf(f, "%s.%d.%s = %"PRIu64"\n", name1, id, name2, val);
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
    write_dev_state_line(f, "ata-smart-attribute", i, "raw", pa.raw);
  }

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


void PrintOneCVS(const char *a_cvs_id){
  char out[CVSMAXLEN];
  printone(out,a_cvs_id);
  PrintOut(LOG_INFO,"%s",out);
  return;
}

// prints CVS identity information for the executable
void PrintCVS(void){
  const char *configargs=strlen(SMARTMONTOOLS_CONFIGURE_ARGS)?SMARTMONTOOLS_CONFIGURE_ARGS:"[no arguments given]";

  PrintOut(LOG_INFO,"%s", (char *)copyleftstring);
  PrintOut(LOG_INFO,"CVS version IDs of files used to build this code are:\n");
  PrintOneCVS(atacmdnames_c_cvsid);
  PrintOneCVS(atacmds_c_cvsid);
#ifdef _HAVE_CCISS
  PrintOneCVS(cciss_c_cvsid);
#endif
#ifdef _WIN32
  PrintOneCVS(daemon_win32_c_cvsid);
  PrintOneCVS(hostname_win32_c_cvsid);
#endif
  PrintOneCVS(knowndrives_c_cvsid);
  PrintOneCVS(os_XXXX_c_cvsid);
#ifdef NEED_SOLARIS_ATA_CODE
  PrintOneCVS( os_solaris_ata_s_cvsid);
#endif
  PrintOneCVS(scsicmds_c_cvsid);
  PrintOneCVS(smartd_c_cvsid);
#ifdef _WIN32
  PrintOneCVS(syslog_win32_c_cvsid);
#endif
  PrintOneCVS(utility_c_cvsid);
  PrintOut(LOG_INFO, "\nsmartmontools release " PACKAGE_VERSION " dated " SMARTMONTOOLS_RELEASE_DATE " at " SMARTMONTOOLS_RELEASE_TIME "\n");
  PrintOut(LOG_INFO, "smartmontools build host: " SMARTMONTOOLS_BUILD_HOST "\n");
  PrintOut(LOG_INFO, "smartmontools build configured: " SMARTMONTOOLS_CONFIGURE_DATE "\n");
  PrintOut(LOG_INFO, "smartd compile dated " __DATE__ " at "__TIME__ "\n");
  PrintOut(LOG_INFO, "smartmontools configure arguments: %s\n", configargs);
  return;
}

// remove the PID file
void RemovePidFile(){
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
void USR1handler(int sig){
  if (SIGUSR1==sig)
    caughtsigUSR1=1;
  return;
}

#ifdef _WIN32
//  Note if we catch a SIGUSR2
void USR2handler(int sig){
  if (SIGUSR2==sig)
    caughtsigUSR2=1;
  return;
}
#endif

// Note if we catch a HUP (or INT in debug mode)
void HUPhandler(int sig){
  if (sig==SIGHUP)
    caughtsigHUP=1;
  else
    caughtsigHUP=2;
  return;
}

// signal handler for TERM, QUIT, and INT (if not in debug mode)
void sighandler(int sig){
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

#define ENVLENGTH 1024

// a replacement for setenv() which is not available on all platforms.
// Note that the string passed to putenv must not be freed or made
// invalid, since a pointer to it is kept by putenv(). This means that
// it must either be a static buffer or allocated off the heap. The
// string can be freed if the environment variable is redefined or
// deleted via another call to putenv(). So we keep these on the stack
// as long as the popen() call is underway.
int exportenv(char* stackspace, const char *name, const char *value){
  snprintf(stackspace,ENVLENGTH, "%s=%s", name, value);
  return putenv(stackspace);
}

char* dnsdomain(const char* hostname) {
  char *p = NULL;
#ifdef HAVE_GETADDRINFO
  static char canon_name[NI_MAXHOST];
  struct addrinfo *info = NULL;
  struct addrinfo hints;
  int err;

  memset(&hints, 0, sizeof(hints));
  hints.ai_flags = AI_CANONNAME;
  if ((err = getaddrinfo(hostname, NULL, &hints, &info)) || (!info)) {
    PrintOut(LOG_CRIT, "Error retrieving getaddrinfo(%s): %s\n", hostname, gai_strerror(err));
    return NULL;
  }
  if (info->ai_canonname) {
    strncpy(canon_name, info->ai_canonname, sizeof(canon_name));
    canon_name[NI_MAXHOST - 1] = '\0';
    p = canon_name;
    if ((p = strchr(canon_name, '.')))
      p++;
  }
  freeaddrinfo(info);
#elif HAVE_GETHOSTBYNAME
  struct hostent *hp;
  if ((hp = gethostbyname(hostname))) {
    // Does this work if gethostbyname() returns an IPv6 name in
    // colon/dot notation?  [BA]
    if ((p = strchr(hp->h_name, '.')))
      p++; // skip "."
  }
#else
  ARGUSED(hostname);
#endif
  return p;
}

#define EBUFLEN 1024

static void MailWarning(const dev_config & cfg, dev_state & state, int which, const char *fmt, ...)
                        __attribute__ ((format (printf, 4, 5)));

// If either address or executable path is non-null then send and log
// a warning email, or execute executable
static void MailWarning(const dev_config & cfg, dev_state & state, int which, const char *fmt, ...){
  char command[2048], message[256], hostname[256], domainname[256], additional[256],fullmessage[1024];
  char original[256], further[256], nisdomain[256], subject[256],dates[DATEANDEPOCHLEN];
  char environ_strings[11][ENVLENGTH];
  time_t epoch;
  va_list ap;
  const int day=24*3600;
  int days=0;
  const char * const whichfail[]={
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
  
  const char *unknown="[Unknown]";

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
  epoch=time(NULL);

  // Return if less than one day has gone by
  if (cfg.emailfreq==2 && mail->logged && epoch<(mail->lastsent+day))
    return;

  // Return if less than 2^(logged-1) days have gone by
  if (cfg.emailfreq==3 && mail->logged) {
    days=0x01<<(mail->logged-1);
    days*=day;
    if  (epoch<(mail->lastsent+days))
      return;
  }

  // record the time of this mail message, and the first mail message
  if (!mail->logged)
    mail->firstsent=epoch;
  mail->lastsent=epoch;
  
  // get system host & domain names (not null terminated if length=MAX) 
#ifdef HAVE_GETHOSTNAME
  if (gethostname(hostname, 256))
    strcpy(hostname, unknown);
  else {
    char *p=NULL;
    hostname[255]='\0';
    p = dnsdomain(hostname);
    if (p && *p) {
      strncpy(domainname, p, 255);
      domainname[255]='\0';
    } else
      strcpy(domainname, unknown);
  }
#else
  strcpy(hostname, unknown);
  strcpy(domainname, unknown);
#endif
  
#ifdef HAVE_GETDOMAINNAME
  if (getdomainname(nisdomain, 256))
    strcpy(nisdomain, unknown);
  else
    nisdomain[255]='\0';
#else
  strcpy(nisdomain, unknown);
#endif
  
  // print warning string into message
  va_start(ap, fmt);
  vsnprintf(message, 256, fmt, ap);
  va_end(ap);

  // appropriate message about further information
  additional[0]=original[0]=further[0]='\0';
  if (which) {
    sprintf(further,"You can also use the smartctl utility for further investigation.\n");

    switch (cfg.emailfreq) {
    case 1:
      sprintf(additional,"No additional email messages about this problem will be sent.\n");
      break;
    case 2:
      sprintf(additional,"Another email message will be sent in 24 hours if the problem persists.\n");
      break;
    case 3:
      sprintf(additional,"Another email message will be sent in %d days if the problem persists\n",
              (0x01)<<mail->logged);
      break;
    }
    if (cfg.emailfreq>1 && mail->logged) {
      dateandtimezoneepoch(dates, mail->firstsent);
      sprintf(original,"The original email about this issue was sent at %s\n", dates);
    }
  }
  
  snprintf(subject, 256,"SMART error (%s) detected on host: %s", whichfail[which], hostname);

  // If the user has set cfg.emailcmdline, use that as mailer, else "mail" or "mailx".
  if (!*executable)
#ifdef DEFAULT_MAILER
    executable = DEFAULT_MAILER ;
#else
#ifndef _WIN32
    executable = "mail";
#else
    executable = "blat"; // http://blat.sourceforge.net/
#endif
#endif

#ifndef _WIN32 // blat mailer needs comma
  // replace commas by spaces to separate recipients
  std::replace(address.begin(), address.end(), ',', ' ');
#endif
  // Export information in environment variables that will be useful
  // for user scripts
  exportenv(environ_strings[0], "SMARTD_MAILER", executable);
  exportenv(environ_strings[1], "SMARTD_MESSAGE", message);
  exportenv(environ_strings[2], "SMARTD_SUBJECT", subject);
  dateandtimezoneepoch(dates, mail->firstsent);
  exportenv(environ_strings[3], "SMARTD_TFIRST", dates);
  snprintf(dates, DATEANDEPOCHLEN,"%d", (int)mail->firstsent);
  exportenv(environ_strings[4], "SMARTD_TFIRSTEPOCH", dates);
  exportenv(environ_strings[5], "SMARTD_FAILTYPE", whichfail[which]);
  if (!address.empty())
    exportenv(environ_strings[6], "SMARTD_ADDRESS", address.c_str());
  exportenv(environ_strings[7], "SMARTD_DEVICESTRING", cfg.name.c_str());

  exportenv(environ_strings[8], "SMARTD_DEVICETYPE", cfg.dev_type.c_str());
  exportenv(environ_strings[9], "SMARTD_DEVICE", cfg.name.c_str());

  snprintf(fullmessage, 1024,
             "This email was generated by the smartd daemon running on:\n\n"
             "   host name: %s\n"
             "  DNS domain: %s\n"
             "  NIS domain: %s\n\n"
             "The following warning/error was logged by the smartd daemon:\n\n"
             "%s\n\n"
             "For details see host's SYSLOG (default: /var/log/messages).\n\n"
             "%s%s%s",
	     hostname, domainname, nisdomain, message, further, original, additional);
  exportenv(environ_strings[10], "SMARTD_FULLMESSAGE", fullmessage);

  // now construct a command to send this as EMAIL
#ifndef _WIN32
  if (!address.empty())
    snprintf(command, 2048, 
             "$SMARTD_MAILER -s '%s' %s 2>&1 << \"ENDMAIL\"\n"
	     "%sENDMAIL\n", subject, address.c_str(), fullmessage);
  else
    snprintf(command, 2048, "%s 2>&1", executable);
  
  // tell SYSLOG what we are about to do...
  const char * newadd = (!address.empty()? address.c_str() : "<nomailer>");
  const char * newwarn = (which? "Warning via" : "Test of");

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

  // No "here-documents" on Windows, so must use separate commandline and stdin
  char stdinbuf[1024];
  command[0] = stdinbuf[0] = 0;
  int boxtype = -1, boxmsgoffs = 0;
  const char * newadd = "<nomailer>";
  if (!address.empty()) {
    // address "[sys]msgbox ..." => show warning (also) as [system modal ]messagebox
    char addr1[9+1+13] = ""; int n1 = -1, n2 = -1;
    if (sscanf(address.c_str(), "%9[a-z]%n,%n", addr1, &n1, &n2) == 1 && (n1 == (int)address.size() || n2 > 0)) {
      if (!strcmp(addr1, "msgbox"))
        boxtype = 0;
      else if (!strcmp(addr1, "sysmsgbox"))
        boxtype = 1;
      if (boxtype >= 0)
        address.erase(0, (n2 > n1 ? n2 : n1));
    }

    if (!address.empty()) {
      // Use "blat" parameter syntax (TODO: configure via -M for other mailers)
      snprintf(command, sizeof(command),
               "%s - -q -subject \"%s\" -to \"%s\"",
               executable, subject, address.c_str());
      newadd = address.c_str();
    }

#ifdef _MSC_VER
    _set_printf_count_output(1); // "%n" disabled by default
#endif
    // Message for mail [0...] and messagebox [boxmsgoffs...]
    snprintf(stdinbuf, sizeof(stdinbuf),
             "This email was generated by the smartd daemon running on:\n\n"
             "   host name: %s\n"
             "  DNS domain: %s\n"
//           "  NIS domain: %s\n"
             "\n%n"
             "The following warning/error was logged by the smartd daemon:\n\n"
             "%s\n\n"
             "For details see the event log or log file of smartd.\n\n"
             "%s%s%s"
             "\n",
             hostname, /*domainname, */ nisdomain, &boxmsgoffs, message, further, original, additional);
  }
  else
    snprintf(command, sizeof(command), "%s", executable);

  const char * newwarn = (which ? "Warning via" : "Test of");
  if (boxtype >= 0) {
    // show message box
    daemon_messagebox(boxtype, subject, stdinbuf+boxmsgoffs);
    PrintOut(LOG_INFO,"%s message box\n", newwarn);
  }
  if (command[0]) {
    char stdoutbuf[800]; // < buffer in syslog_win32::vsyslog()
    int rc;
    // run command
    PrintOut(LOG_INFO,"%s %s to %s ...\n",
             (which?"Sending warning via":"Executing test of"), executable, newadd);
    rc = daemon_spawn(command, stdinbuf, strlen(stdinbuf), stdoutbuf, sizeof(stdoutbuf));
    if (rc >= 0 && stdoutbuf[0])
      PrintOut(LOG_CRIT,"%s %s to %s produced unexpected output (%d bytes) to STDOUT/STDERR:\n%s\n",
        newwarn, executable, newadd, strlen(stdoutbuf), stdoutbuf);
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
  // in debug==1 mode we will print the output from the ataprint.o functions!
  if (debugmode && debugmode!=2)
#ifdef _WIN32
   if (facility == LOG_LOCAL1) // logging to stdout
    vfprintf(stderr,fmt,ap);
   else   
#endif
    vprintf(fmt,ap);
  // in debug==2 mode we print output from knowndrives.o functions
  else if (debugmode==2 || con->reportataioctl || con->reportscsiioctl /*|| con->controller_port???*/) {
    openlog("smartd", LOG_PID, facility);
    vsyslog(LOG_INFO, fmt, ap);
    closelog();
  }
  va_end(ap);
  fflush(NULL);
  return;
}

// This function prints either to stdout or to the syslog as needed.
// This function is also used by utility.cpp to report LOG_CRIT errors.
void PrintOut(int priority, const char *fmt, ...){
  va_list ap;
  
  // get the correct time in syslog()
  FixGlibcTimeZoneBug();
  // initialize variable argument list 
  va_start(ap,fmt);
  if (debugmode) 
#ifdef _WIN32
   if (facility == LOG_LOCAL1) // logging to stdout
    vfprintf(stderr,fmt,ap);
   else   
#endif
    vprintf(fmt,ap);
  else {
    openlog("smartd", LOG_PID, facility);
    vsyslog(priority,fmt,ap);
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

// Wait for the pid file to show up, this makes sure a calling program knows
// that the daemon is really up and running and has a pid to kill it
bool WaitForPidFile()
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


// Forks new process, closes ALL file descriptors, redirects stdin,
// stdout, and stderr.  Not quite daemon().  See
// http://www.linuxjournal.com/article/2335
// for a good description of why we do things this way.
void DaemonInit(){
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
  
#ifdef __CYGWIN__
  // Cygwin's setsid() does not detach the process from Windows console
  FreeConsole();
#endif // __CYGWIN__

#define NO_warn_unused_result(cmd) { if (cmd) {} ; }

  // redirect any IO attempts to /dev/null for stdin
  i=open("/dev/null",O_RDWR);
  if (i>=0) {
    // stdout
    NO_warn_unused_result(dup(i));
    // stderr
    NO_warn_unused_result(dup(i));
  };
  umask(0);
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
void PrintHead(){
  PrintOut(LOG_INFO, "%s\n", format_version_info("smartd"));
}

// prints help info for configuration file Directives
void Directives() {
  PrintOut(LOG_INFO,
           "Configuration file (%s) Directives (after device name):\n"
           "  -d TYPE Set the device type: %s\n"
           "  -T TYPE Set the tolerance to one of: normal, permissive\n"
           "  -o VAL  Enable/disable automatic offline tests (on/off)\n"
           "  -S VAL  Enable/disable attribute autosave (on/off)\n"
           "  -n MODE No check if: never, sleep[,N][,q], standby[,N][,q], idle[,N][,q]\n"
           "  -H      Monitor SMART Health Status, report if failed\n"
           "  -s REG  Do Self-Test at time(s) given by regular expression REG\n"
           "  -l TYPE Monitor SMART log.  Type is one of: error, selftest\n"
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
           "  -a      Default: -H -f -t -l error -l selftest -C 197 -U 198\n"
           "  -F TYPE Firmware bug workaround: none, samsung, samsung2, samsung3\n"
           "   #      Comment: text after a hash sign is ignored\n"
           "   \\      Line continuation character\n"
           "Attribute ID is a decimal integer 1 <= ID <= 255\n"
	   "Use ID = 0 to turn off -C and/or -U Directives\n"
           "Example: /dev/hda -a\n", 
           configfile, smi()->get_valid_dev_types_str());
  return;
}

/* Returns a pointer to a static string containing a formatted list of the valid
   arguments to the option opt or NULL on failure. */
const char *GetValidArgList(char opt) {
  switch (opt) {
  case 'c':
    return "<FILE_NAME>, -";
  case 's':
    return "valid_regular_expression";
  case 'l':
    return "daemon, local0, local1, local2, local3, local4, local5, local6, local7";
  case 'q':
    return "nodev, errors, nodevstartup, never, onecheck, showtests";
  case 'r':
    return "ioctl[,N], ataioctl[,N], scsiioctl[,N]";
  case 'p':
    return "<FILE_NAME>";
  case 'i':
    return "<INTEGER_SECONDS>";
  default:
    return NULL;
  }
}

/* prints help information for command syntax */
void Usage (void){
  PrintOut(LOG_INFO,"Usage: smartd [options]\n\n");
  PrintOut(LOG_INFO,"  -c NAME|-, --configfile=NAME|-\n");
  PrintOut(LOG_INFO,"        Read configuration file NAME or stdin [default is %s]\n\n", configfile);
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
  PrintOut(LOG_INFO,"        [default is "SMARTMONTOOLS_SAVESTATES"MODEL-SERIAL.TYPE.state]\n");
#endif
  PrintOut(LOG_INFO,"\n");
  PrintOut(LOG_INFO,"  -B [+]FILE, --drivedb=[+]FILE\n");
  PrintOut(LOG_INFO,"        Read and replace [add] drive database from FILE\n");
#ifdef SMARTMONTOOLS_DRIVEDBDIR
  PrintOut(LOG_INFO,"        [default is "SMARTMONTOOLS_DRIVEDBDIR"/drivedb.h]\n");
#endif
  PrintOut(LOG_INFO,"\n");
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

// returns <0 on failure
static int ATAErrorCount(ata_device * device, const char * name,
                         unsigned char fix_firmwarebug)
{
  struct ata_smart_errorlog log;
  
  if (ataReadErrorLog(device, &log, fix_firmwarebug)){
    PrintOut(LOG_INFO,"Device: %s, Read SMART Error Log Failed\n",name);
    return -1;
  }
  
  // return current number of ATA errors
  return log.error_log_pointer?log.ata_error_count:0;
}

// returns <0 if problem.  Otherwise, bottom 8 bits are the self test
// error count, and top bits are the power-on hours of the last error.
static int SelfTestErrorCount(ata_device * device, const char * name,
                              unsigned char fix_firmwarebug)
{
  struct ata_smart_selftestlog log;

  if (ataReadSelfTestLog(device, &log, fix_firmwarebug)){
    PrintOut(LOG_INFO,"Device: %s, Read SMART Self Test Log Failed\n",name);
    return -1;
  }
  
  // return current number of self-test errors
  return ataPrintSmartSelfTestlog(&log, false, fix_firmwarebug);
}

#define SELFTEST_ERRORCOUNT(x) (x & 0xff)
#define SELFTEST_ERRORHOURS(x) ((x >> 8) & 0xffff)

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
  if ((retid=ataReadHDIdentity (atadev, &drive))){
    if (retid<0)
      // Unable to read Identity structure
      PrintOut(LOG_INFO,"Device: %s, not ATA, no IDENTIFY DEVICE Structure\n",name);
    else
      PrintOut(LOG_INFO,"Device: %s, packet devices [this device %s] not SMART capable\n",
               name, packetdevicetype(retid-1));
    CloseDevice(atadev, name);
    return 2; 
  }
  // Store drive size (for selective self-test only)
  state.num_sectors = get_num_sectors(&drive);

  // Show if device in database, and use preset vendor attribute
  // options unless user has requested otherwise.
  if (cfg.ignorepresets)
    PrintOut(LOG_INFO, "Device: %s, smartd database not searched (Directive: -P ignore).\n", name);
  else {
    // do whatever applypresets decides to do.
    if (!apply_presets(&drive, cfg.attributedefs, cfg.fix_firmwarebug, fix_swapped_id))
      PrintOut(LOG_INFO, "Device: %s, not found in smartd database.\n", name);
    else
      PrintOut(LOG_INFO, "Device: %s, found in smartd database.\n", name);
  }

  // Set default '-C 197[+]' if no '-C ID' is specified.
  if (!cfg.curr_pending_set)
    cfg.curr_pending_id = get_unc_attr_id(false, cfg.attributedefs, cfg.curr_pending_incr);
  // Set default '-U 198[+]' if no '-U ID' is specified.
  if (!cfg.offl_pending_set)
    cfg.offl_pending_id = get_unc_attr_id(true, cfg.attributedefs, cfg.offl_pending_incr);

  // If requested, show which presets would be used for this drive
  if (cfg.showpresets) {
    int savedebugmode=debugmode;
    PrintOut(LOG_INFO, "Device %s: presets are:\n", name);
    if (!debugmode)
      debugmode=2;
    show_presets(&drive, false);
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

  // do we need to retain SMART data after returning from this routine?
  bool retainsmartdata = (cfg.usagefailed || cfg.prefail || cfg.usage || cfg.tempdiff || cfg.tempinfo || cfg.tempcrit);
  
  // do we need to get SMART data?
  bool smart_val_ok = false;
  if (   retainsmartdata || cfg.autoofflinetest || cfg.selftest || cfg.errorlog
      || cfg.curr_pending_id || cfg.offl_pending_id                            ) {

    if (ataReadSmartValues(atadev, &state.smartval) ||
        ataReadSmartThresholds (atadev, &state.smartthres)) {
      PrintOut(LOG_INFO,"Device: %s, Read SMART Values and/or Thresholds Failed\n",name);
      retainsmartdata = cfg.usagefailed = cfg.prefail = cfg.usage = false;
      cfg.tempdiff = cfg.tempinfo = cfg.tempcrit = 0;
      cfg.curr_pending_id = cfg.offl_pending_id = 0;
    }
    else
      smart_val_ok = true;

    // see if the necessary Attribute is there to monitor offline or
    // current pending sectors or temperature
    if (cfg.curr_pending_id && ATAReturnAttributeRawValue(cfg.curr_pending_id, &state.smartval) < 0) {
      PrintOut(LOG_INFO,"Device: %s, can't monitor Current Pending Sector count - no Attribute %d\n",
               name, cfg.curr_pending_id);
      cfg.curr_pending_id = 0;
    }
    
    if (cfg.offl_pending_id && ATAReturnAttributeRawValue(cfg.offl_pending_id, &state.smartval) < 0) {
      PrintOut(LOG_INFO,"Device: %s, can't monitor Offline Uncorrectable Sector count - no Attribute %d\n",
               name, cfg.offl_pending_id);
      cfg.offl_pending_id = 0;
    }

    if (   (cfg.tempdiff || cfg.tempinfo || cfg.tempcrit)
        && !ATAReturnTemperatureValue(&state.smartval, cfg.attributedefs)) {
      PrintOut(LOG_CRIT, "Device: %s, can't monitor Temperature, ignoring -W Directive\n", name);
      cfg.tempdiff = cfg.tempinfo = cfg.tempcrit = 0;
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
  
  // capability check: self-test-log
  if (cfg.selftest) {
    int retval;
    
    // start with service disabled, and re-enable it if all works OK
    cfg.selftest = false;
    state.selflogcount = 0;
    state.selfloghour = 0;

    if (!smart_val_ok)
      PrintOut(LOG_INFO, "Device: %s, no SMART Self-Test log (SMART READ DATA failed); disabling -l selftest\n", name);
    else if (!cfg.permissive && !isSmartTestLogCapable(&state.smartval, &drive))
      PrintOut(LOG_INFO, "Device: %s, appears to lack SMART Self-Test log; disabling -l selftest (override with -T permissive Directive)\n", name);
    else if ((retval = SelfTestErrorCount(atadev, name, cfg.fix_firmwarebug)) < 0)
      PrintOut(LOG_INFO, "Device: %s, no SMART Self-Test log; remove -l selftest Directive from smartd.conf\n", name);
    else {
      cfg.selftest = true;
      state.selflogcount=SELFTEST_ERRORCOUNT(retval);
      state.selfloghour =SELFTEST_ERRORHOURS(retval);
    }
  }
  
  // capability check: ATA error log
  if (cfg.errorlog) {
    int val;

    // start with service disabled, and re-enable it if all works OK
    cfg.errorlog = false;
    state.ataerrorcount=0;

    if (!smart_val_ok)
      PrintOut(LOG_INFO, "Device: %s, no SMART Error log (SMART READ DATA failed); disabling -l error\n", name);
    else if (!cfg.permissive && !isSmartErrorLogCapable(&state.smartval, &drive))
      PrintOut(LOG_INFO, "Device: %s, appears to lack SMART Error log; disabling -l error (override with -T permissive Directive)\n", name);
    else if ((val = ATAErrorCount(atadev, name, cfg.fix_firmwarebug)) < 0)
      PrintOut(LOG_INFO, "Device: %s, no SMART Error log; remove -l error Directive from smartd.conf\n", name);
    else {
        cfg.errorlog = true;
        state.ataerrorcount=val;
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

  // If no tests available or selected, return
  if (!(cfg.errorlog    || cfg.selftest || cfg.smartcheck ||
        cfg.usagefailed || cfg.prefail  || cfg.usage      ||
        cfg.tempdiff    || cfg.tempinfo || cfg.tempcrit     )) {
    CloseDevice(atadev, name);
    return 3;
  }
  
  // tell user we are registering device
  PrintOut(LOG_INFO,"Device: %s, is SMART capable. Adding to \"monitor\" list.\n",name);
  
  // record number of device, type of device, increment device count
  if (cfg.dev_type.empty())
    cfg.dev_type = "ata";
  
  // close file descriptor
  CloseDevice(atadev, name);

  if (!state_path_prefix.empty()) {
    // Build file name for state file
    char model[40+1], serial[20+1];
    format_ata_string(model, drive.model, sizeof(model)-1, fix_swapped_id);
    format_ata_string(serial, drive.serial_no, sizeof(serial)-1, fix_swapped_id);
    std::replace_if(model, model+strlen(model), not_allowed_in_filename, '_');
    std::replace_if(serial, serial+strlen(serial), not_allowed_in_filename, '_');
    cfg.state_file = strprintf("%s%s-%s.ata.state", state_path_prefix.c_str(), model, serial);

    // Read previous state
    if (read_dev_state(cfg.state_file.c_str(), state)) {
      PrintOut(LOG_INFO, "Device: %s, state read from %s\n", name, cfg.state_file.c_str());
      // Copy ATA attribute values to temp state
      state.update_temp_state();
    }
  }

  // Start self-test regex check now if time was not read from state file
  if (!cfg.test_regex.empty() && !state.scheduled_test_next_check)
    state.scheduled_test_next_check = time(0);

  return 0;
}

// on success, return 0. On failure, return >0.  Never return <0,
// please.
static int SCSIDeviceScan(dev_config & cfg, dev_state & state, scsi_device * scsidev)
{
  int k, err;
  const char *device = cfg.name.c_str();
  struct scsi_iec_mode_page iec;
  UINT8  tBuf[64];

  // Device must be open

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
      default:
        break;
      }
    }   
  }
  
  // record type of device
  if (cfg.dev_type.empty())
    cfg.dev_type = "scsi";
  
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
        PrintOut(LOG_CRIT, "Device: %s, can't monitor Temperature, ignoring -W Directive\n", device);
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

  // TODO: Build file name for state file
  if (!state_path_prefix.empty()) {
    PrintOut(LOG_INFO, "Device: %s, persistence not yet supported for SCSI; ignoring -s option.\n", device);
  }

  // close file descriptor
  CloseDevice(scsidev, device);

  // Start self-test regex check now if time was not read from state file
  if (!cfg.test_regex.empty() && !state.scheduled_test_next_check)
    state.scheduled_test_next_check = time(0);

  return 0;
}


struct changedattribute_t {
  unsigned char newval;
  unsigned char oldval;
  unsigned char id;
  unsigned char prefail;
  unsigned char sameraw;
};

// We compare old and new values of the n'th attribute.  Note that n
// is NOT the attribute ID number.. If (Normalized & Raw) equal,
// then return 0, else nonzero.
static int ATACompareValues(changedattribute_t *delta,
                            struct ata_smart_values *newv,
                            struct ata_smart_values *oldv,
                            struct ata_smart_thresholds_pvt *thresholds,
                            int n, const char * name)
{
  struct ata_smart_attribute *now,*was;
  struct ata_smart_threshold_entry *thre;
  unsigned char oldval,newval;
  int sameraw;

  // check that attribute number in range, and no null pointers
  if (n<0 || n>=NUMBER_ATA_SMART_ATTRIBUTES || !newv || !oldv || !thresholds)
    return 0;
  
  // pointers to disk's values and vendor's thresholds
  now=newv->vendor_attributes+n;
  was=oldv->vendor_attributes+n;
  thre=thresholds->thres_entries+n;

  // consider only valid attributes
  if (!now->id || !was->id || !thre->id)
    return 0;
  
  
  // issue warning if they don't have the same ID in all structures:
  if ( (now->id != was->id) || (now->id != thre->id) ){
    PrintOut(LOG_INFO,"Device: %s, same Attribute has different ID numbers: %d = %d = %d\n",
             name, (int)now->id, (int)was->id, (int)thre->id);
    return 0;
  }

  // new and old values of Normalized Attributes
  newval=now->current;
  oldval=was->current;

  // See if the RAW values are unchanged (ie, the same)
  if (memcmp(now->raw, was->raw, 6))
    sameraw=0;
  else
    sameraw=1;
  
  // if any values out of the allowed range, or if the values haven't
  // changed, return 0
  if (!newval || !oldval || newval>0xfe || oldval>0xfe || (oldval==newval && sameraw))
    return 0;
  
  // values have changed.  Construct output and return
  delta->newval=newval;
  delta->oldval=oldval;
  delta->id=now->id;
  delta->prefail=ATTRIBUTE_FLAGS_PREFAILURE(now->flags);
  delta->sameraw=sameraw;

  return 1;
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
    } else if (oldh!=newh) {
      // more recent error
      // a 'more recent' error might actually be a smaller hour number,
      // if the hour number has wrapped.
      // There's still a bug here.  You might just happen to run a new test
      // exactly 32768 hours after the previous failure, and have run exactly
      // 20 tests between the two, in which case smartd will miss the
      // new failure.
      PrintOut(LOG_CRIT, "Device: %s, new Self-Test Log error at hour timestamp %d\n",
               name, newh);
      MailWarning(cfg, state, 3, "Device: %s, new Self-Test Log error at hour timestamp %d\n",
                   name, newh);
      state.must_write = true;
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
const unsigned num_test_types = sizeof(test_type_chars)-1;

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
    if (cfg.fix_firmwarebug == FIX_SAMSUNG3 && data.self_test_exec_status == 0xf0) {
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
    ata_selective_selftest_args selargs;
    selargs.num_spans = 1;
    selargs.span[0].mode = mode;
    if (ataWriteSelectiveSelfTestLog(device, selargs, &data, state.num_sectors)) {
      PrintOut(LOG_CRIT, "Device: %s, prepare %sTest failed\n", name, testname);
      return 1;
    }
    uint64_t start = selargs.span[0].start, end = selargs.span[0].end;
    PrintOut(LOG_INFO, "Device: %s, %s test span at LBA %"PRIu64" - %"PRIu64" (%"PRIu64" sectors, %u%% - %u%% of disk).\n",
      name, (selargs.span[0].mode == SEL_NEXT ? "next" : "redo"),
      start, end, end - start + 1,
      (unsigned)((100 * start + state.num_sectors/2) / state.num_sectors),
      (unsigned)((100 * end   + state.num_sectors/2) / state.num_sectors));
  }

  // execute the test, and return status
  int retval = smartcommandhandler(device, IMMEDIATE_OFFLINE, dotest, NULL);
  if (retval) {
    PrintOut(LOG_CRIT, "Device: %s, execute %sTest failed.\n", name, testname);
    return retval;
  }

  if (testtype != 'O')
    // Log next self-test execution status
    state.smartval.self_test_exec_status = 0xff;

  PrintOut(LOG_INFO, "Device: %s, starting scheduled %sTest.\n", name, testname);
  return 0;
}

// Check pending sector count attribute values (-C, -U directives).
static void check_pending(const dev_config & cfg, dev_state & state,
                          unsigned char id, bool increase_only,
                          const ata_smart_values & smartval,
                          int mailtype, const char * msg)
{
  // No report if no sectors pending.
  int64_t rawval = ATAReturnAttributeRawValue(id, &smartval);
  if (rawval <= 0)
    return;

  // If attribute is not reset, report only sector count increases.
  int64_t prev_rawval = ATAReturnAttributeRawValue(id, &state.smartval);
  if (!(!increase_only || prev_rawval < rawval))
    return;

  // Format message.
  std::string s = strprintf("Device: %s, %"PRId64" %s", cfg.name.c_str(), rawval, msg);
  if (prev_rawval > 0 && rawval != prev_rawval)
    s += strprintf(" (changed %+"PRId64")", rawval - prev_rawval);

  PrintOut(LOG_CRIT, "%s\n", s.c_str());
  MailWarning(cfg, state, mailtype, "%s\n", s.c_str());
  state.must_write = true;
}

// Format Temperature value
static const char * fmt_temp(unsigned char x, char * buf)
{
  if (!x) // unset
    strcpy(buf, "??");
  else
    sprintf(buf, "%u", x);
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
    MailWarning(cfg, state, 12, "Device: %s, Temperature %d Celsius reached critical limit of %u Celsius (Min/Max %s%s/%u%s)\n",
      cfg.name.c_str(), currtemp, cfg.tempcrit, fmt_temp(state.tempmin, buf), minchg, state.tempmax, maxchg);
  }
  else if (cfg.tempinfo && currtemp >= cfg.tempinfo) {
    PrintOut(LOG_INFO, "Device: %s, Temperature %u Celsius reached limit of %u Celsius (Min/Max %s%s/%u%s)\n",
      cfg.name.c_str(), currtemp, cfg.tempinfo, fmt_temp(state.tempmin, buf), minchg, state.tempmax, maxchg);
  }
}

static int ATACheckDevice(const dev_config & cfg, dev_state & state, ata_device * atadev, bool allow_selftests)
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
  } else if (debugmode)
    PrintOut(LOG_INFO,"Device: %s, opened ATA device\n", name);

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
      || cfg.tempdiff || cfg.tempinfo || cfg.tempcrit || cfg.selftest) {
    struct ata_smart_values     curval;
    struct ata_smart_thresholds_pvt * thresh = &state.smartthres;

    // Read current attribute values. *drive contains old values and thresholds
    if (ataReadSmartValues(atadev, &curval)){
      PrintOut(LOG_CRIT, "Device: %s, failed to read SMART Attribute Data\n", name);
      MailWarning(cfg, state, 6, "Device: %s, failed to read SMART Attribute Data", name);
      state.must_write = true;
    }
    else {
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
        CheckTemperature(cfg, state, ATAReturnTemperatureValue(&curval, cfg.attributedefs), 0);

      if (cfg.usagefailed || cfg.prefail || cfg.usage) {

	// look for failed usage attributes, or track usage or prefail attributes
        for (int i = 0; i < NUMBER_ATA_SMART_ATTRIBUTES; i++) {

	  // This block looks for usage attributes that have failed.
	  // Prefail attributes that have failed are returned with a
	  // positive sign. No failure returns 0. Usage attributes<0.
          int att;
	  if (cfg.usagefailed && ((att=ataCheckAttribute(&curval, thresh, i))<0)){
	    
	    // are we ignoring failures of this attribute?
	    att *= -1;
            if (!cfg.monitor_attr_flags.is_set(att, MONITOR_IGN_FAILUSE)) {
	      char attname[64], *loc=attname;
	      
	      // get attribute name & skip white space
	      ataPrintSmartAttribName(loc, att, cfg.attributedefs);
	      while (*loc && *loc==' ') loc++;
	      
	      // warning message
	      PrintOut(LOG_CRIT, "Device: %s, Failed SMART usage Attribute: %s.\n", name, loc);
	      MailWarning(cfg, state, 2, "Device: %s, Failed SMART usage Attribute: %s.", name, loc);
              state.must_write = true;
	    }
	  }
	  
	  // This block tracks usage or prefailure attributes to see if
	  // they are changing.  It also looks for changes in RAW values
	  // if this has been requested by user.
          changedattribute_t delta;
	  if ((cfg.usage || cfg.prefail) && ATACompareValues(&delta, &curval, &state.smartval, thresh, i, name)){

            // Continue if we're not tracking this type of attribute
            if (!(   ( delta.prefail && cfg.prefail)
                  || (!delta.prefail && cfg.usage   )))
              continue;

            // Continue if '-I ID' was specified
            unsigned char id = delta.id;
            if (cfg.monitor_attr_flags.is_set(id, MONITOR_IGNORE))
              continue;

	    // if the only change is the raw value, and we're not
	    // tracking raw value, then continue loop over attributes
            if (   !delta.sameraw && delta.newval == delta.oldval
                && !cfg.monitor_attr_flags.is_set(id, MONITOR_RAW))
	      continue;

            // get attribute name, skip spaces
            char attname[64], *loc = attname;
            ataPrintSmartAttribName(loc, id, cfg.attributedefs);
            while (*loc && *loc==' ')
              loc++;

            // has the user asked for us to print raw values?
            char newrawstring[64], oldrawstring[64];
            if (cfg.monitor_attr_flags.is_set(id, MONITOR_RAW_PRINT)) {
              // get raw values (as a string) and add to printout
              char rawstring[64];
              ataPrintSmartAttribRawValue(rawstring, curval.vendor_attributes+i, cfg.attributedefs);
              sprintf(newrawstring, " [Raw %s]", rawstring);
              ataPrintSmartAttribRawValue(rawstring, state.smartval.vendor_attributes+i, cfg.attributedefs);
              sprintf(oldrawstring, " [Raw %s]", rawstring);
            }
            else
              newrawstring[0]=oldrawstring[0]='\0';

            // Format message
            std::string msg = strprintf("Device: %s, SMART %s Attribute: %s changed from %d%s to %d%s",
                                        name, (delta.prefail ? "Prefailure" : "Usage"), loc,
                                        delta.oldval, oldrawstring, delta.newval, newrawstring);

            // Report this change as critical ?
            if (   (delta.newval != delta.oldval && cfg.monitor_attr_flags.is_set(id, MONITOR_AS_CRIT))
                || (!delta.sameraw               && cfg.monitor_attr_flags.is_set(id, MONITOR_RAW_AS_CRIT))) {
              PrintOut(LOG_CRIT, "%s\n", msg.c_str());
              MailWarning(cfg, state, 2, "%s", msg.c_str());
            }
            else {
              PrintOut(LOG_INFO, "%s\n", msg.c_str());
            }
            state.must_write = true;
	  } // endof block tracking usage or prefailure
	} // end of loop over attributes
	
        if (cfg.selftest) {
          // Log changes of self-test executions status
          if (   curval.self_test_exec_status != state.smartval.self_test_exec_status
              || (!allow_selftests && curval.self_test_exec_status != 0x00)          )
            log_self_test_exec_status(name, curval.self_test_exec_status);
        }

	// Save the new values into *drive for the next time around
	state.smartval = curval;
      }
    }
  }
  
  // check if number of selftest errors has increased (note: may also DECREASE)
  if (cfg.selftest)
    CheckSelfTestLogs(cfg, state, SelfTestErrorCount(atadev, name, cfg.fix_firmwarebug));

  // check if number of ATA errors has increased
  if (cfg.errorlog) {

    int newc, oldc= state.ataerrorcount;

    // new number of errors
    newc = ATAErrorCount(atadev, name, cfg.fix_firmwarebug);

    // did command fail?
    if (newc<0)
      // lack of PrintOut here is INTENTIONAL
      MailWarning(cfg, state, 7, "Device: %s, Read SMART Error Log Failed", name);

    // has error count increased?
    if (newc>oldc){
      PrintOut(LOG_CRIT, "Device: %s, ATA error count increased from %d to %d\n",
               name, oldc, newc);
      MailWarning(cfg, state, 4, "Device: %s, ATA error count increased from %d to %d",
                   name, oldc, newc);
      state.must_write = true;
    }
    
    // this last line is probably not needed, count always increases
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
        } else if (debugmode)
            PrintOut(LOG_INFO,"Device: %s, non-SMART asc,ascq: %d,%d\n",
                     name, (int)asc, (int)ascq);  
    } else if (debugmode)
        PrintOut(LOG_INFO,"Device: %s, SMART health: passed\n", name);  

    // check temperature limits
    if (cfg.tempdiff || cfg.tempinfo || cfg.tempcrit)
      CheckTemperature(cfg, state, currenttemp, triptemp);

    // check if number of selftest errors has increased (note: may also DECREASE)
    if (cfg.selftest)
      CheckSelfTestLogs(cfg, state, scsiCountFailedSelfTests(scsidev, 0));
    
    if (allow_selftests && !cfg.test_regex.empty()) {
      char testtype = next_scheduled_test(cfg, state, true/*scsi*/);
      if (testtype)
        DoSCSISelfTest(cfg, state, scsidev, testtype);
    }
    CloseDevice(scsidev, name);
    return 0;
}

// Checks the SMART status of all ATA and SCSI devices
static void CheckDevicesOnce(const dev_config_vector & configs, dev_state_vector & states,
                             smart_device_list & devices, bool allow_selftests)
{
  for (unsigned i = 0; i < configs.size(); i++) {
    const dev_config & cfg = configs.at(i);
    dev_state & state = states.at(i);
    smart_device * dev = devices.at(i);
    if (dev->is_ata())
      ATACheckDevice(cfg, state, dev->to_ata(), allow_selftests);
    else if (dev->is_scsi())
      SCSICheckDevice(cfg, state, dev->to_scsi(), allow_selftests);
  }
}

// Set if Initialize() was called
static bool is_initialized = false;

// Does initialization right after fork to daemon mode
void Initialize(time_t *wakeuptime){

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
  while (timenow<wakeuptime && !caughtsigUSR1 && !caughtsigHUP && !caughtsigEXIT){
    
    // protect user again system clock being adjusted backwards
    if (wakeuptime>timenow+checktime){
      PrintOut(LOG_CRIT, "System clock time adjusted to the past. Resetting next wakeup time.\n");
      wakeuptime=timenow+checktime;
    }
    
    // Exit sleep when time interval has expired or a signal is received
    sleep(wakeuptime-timenow);

#ifdef _WIN32
    // toggle debug mode?
    if (caughtsigUSR2) {
      ToggleDebugMode();
      caughtsigUSR2 = 0;
    }
#endif

    timenow=time(NULL);
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
void printoutvaliddirectiveargs(int priority, char d) {

  switch (d) {
  case 'n':
    PrintOut(priority, "never[,N][,q], sleep[,N][,q], standby[,N][,q], idle[,N][,q]");
    break;
  case 's':
    PrintOut(priority, "valid_regular_expression");
    break;
  case 'd':
    PrintOut(priority, "%s", smi()->get_valid_dev_types_str());
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
    PrintOut(priority, "none, samsung, samsung2, samsung3");
    break;
  }
}

// exits with an error message, or returns integer value of token
int GetInteger(const char *arg, const char *name, const char *token, int lineno, const char *configfile,
               int min, int max, char * suffix = 0)
{
  // make sure argument is there
  if (!arg) {
    PrintOut(LOG_CRIT,"File %s line %d (drive %s): Directive: %s takes integer argument from %d to %d.\n",
             configfile, lineno, name, token, min, max);
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
             configfile, lineno, name, token, arg, min, max);
    return -1;
  }

  // all is well; return value
  return val;
}


// Get 1-3 small integer(s) for '-W' directive
int Get3Integers(const char *arg, const char *name, const char *token, int lineno, const char *configfile,
                 unsigned char * val1, unsigned char * val2, unsigned char * val3){
  unsigned v1 = 0, v2 = 0, v3 = 0;
  int n1 = -1, n2 = -1, n3 = -1, len;
  if (!arg) {
    PrintOut(LOG_CRIT,"File %s line %d (drive %s): Directive: %s takes 1-3 integer argument(s) from 0 to 255.\n",
             configfile, lineno, name, token);
    return -1;
  }

  len = strlen(arg);
  if (!(   sscanf(arg, "%u%n,%u%n,%u%n", &v1, &n1, &v2, &n2, &v3, &n3) >= 1
        && (n1 == len || n2 == len || n3 == len) && v1 <= 255 && v2 <= 255 && v3 <= 255)) {
    PrintOut(LOG_CRIT,"File %s line %d (drive %s): Directive: %s has argument: %s; needs 1-3 integer(s) from 0 to 255.\n",
             configfile, lineno, name, token, arg);
    return -1;
  }
  *val1 = (unsigned char)v1; *val2 = (unsigned char)v2; *val3 = (unsigned char)v3;
  return 0;
}


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
    } else if (!strcmp(arg, "removable")) {
      cfg.removable = true;
    } else {
      cfg.dev_type = arg;
    }
    break;
  case 'F':
    // fix firmware bug
    if ((arg = strtok(NULL, delim)) == NULL) {
      missingarg = 1;
    } else if (!strcmp(arg, "none")) {
      cfg.fix_firmwarebug = FIX_NONE;
    } else if (!strcmp(arg, "samsung")) {
      cfg.fix_firmwarebug = FIX_SAMSUNG;
    } else if (!strcmp(arg, "samsung2")) {
      cfg.fix_firmwarebug = FIX_SAMSUNG2;
    } else if (!strcmp(arg, "samsung3")) {
      cfg.fix_firmwarebug = FIX_SAMSUNG3;
    } else {
      badarg = 1;
    }
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
      if (!(arg = strtok(NULL, delim))) {
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
    } else if (parse_attribute_def(arg, cfg.attributedefs)) {
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
//  0: found comment or blank line
// -1: found SCANDIRECTIVE line
// -2: found an error
//
// Note: this routine modifies *line from the caller!
static int ParseConfigLine(dev_config_vector & conf_entries, int entry, int lineno, /*const*/ char * line)
{
  char *token=NULL;
  char *name=NULL;
  const char *delim = " \n\t";
  int devscan=0;

  // get first token: device name. If a comment, skip line
  if (!(name=strtok(line,delim)) || *name=='#') {
    return 0;
  }

  // Have we detected the SCANDIRECTIVE directive?
  if (!strcmp(SCANDIRECTIVE,name)){
    devscan=1;
    if (entry) {
      PrintOut(LOG_INFO,"Scan Directive %s (line %d) must be the first entry in %s\n",name, lineno, configfile);
      return -2;
    }
  }
  
  // We've got a legit entry, make space to store it
  conf_entries.push_back( dev_config() );
  dev_config & cfg = conf_entries.back();

  cfg.name = name;

  // Store line number, and by default check for both device types.
  cfg.lineno=lineno;

  // parse tokens one at a time from the file.
  while ((token=strtok(NULL,delim))){
    int retval=ParseToken(token,cfg);
    
    if (retval==0)
      // No tokens left:
      break;
    
    if (retval>0) {
      // Parsed token  
#if (0)
      PrintOut(LOG_INFO,"Parsed token %s\n",token);
#endif
      continue;
    }
    
    if (retval<0) {
      // error found on the line
      return -2;
    }
  }
  
  // If NO monitoring directives are set, then set all of them.
  if (!(cfg.smartcheck || cfg.usagefailed || cfg.prefail  ||
        cfg.usage      || cfg.selftest    || cfg.errorlog ||
       cfg.tempdiff   || cfg.tempinfo    || cfg.tempcrit   )) {
    
    PrintOut(LOG_INFO,"Drive: %s, implied '-a' Directive on line %d of file %s\n",
             cfg.name.c_str(), cfg.lineno, configfile);
    
    cfg.smartcheck = true;
    cfg.usagefailed = true;
    cfg.prefail = true;
    cfg.usage = true;
    cfg.selftest = true;
    cfg.errorlog = true;
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
    // From here on the sign of <nomailer> is address.empty() and !cfg.emailcmdline.empty()
    cfg.emailaddress.clear();
  }

  // set cfg.emailfreq to 1 (once) if user hasn't set it
  if ((!cfg.emailaddress.empty() || !cfg.emailcmdline.empty()) && !cfg.emailfreq)
    cfg.emailfreq = 1;

  if (devscan)
    return -1;
  else
    return 1;
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
// SCANDIRECTIVE found      ==> conf_entries[0].lineno != 0
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

  // No configuration file found -- use fake one
  int entry = 0;
  if (!f) {
    char fakeconfig[] = SCANDIRECTIVE" -a"; // TODO: Remove this hack, build cfg_entry.

    if (ParseConfigLine(conf_entries, entry, 0, fakeconfig) != -1)
      throw std::logic_error("Internal error parsing "SCANDIRECTIVE);
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
        scandevice = ParseConfigLine(conf_entries, entry, contlineno, fullline);
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
    strcpy(fullline+cont,line);
    cont+=len;

    // is this a continuation line.  If so, replace \ by space and look at next line
    if ( (lastslash=strrchr(line,'\\')) && !strtok(lastslash+1," \n\t")){
      *(fullline+(cont-len)+(lastslash-line))=' ';
      continue;
    }

    // Not a continuation line. Parse it
    scandevice = ParseConfigLine(conf_entries, entry, contlineno, fullline);

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


// Prints copyright, license and version information
void PrintCopyleft(void){
  debugmode=1;
  PrintHead();
  PrintCVS();
  return;
}

/* Prints the message "=======> VALID ARGUMENTS ARE: <LIST>  <=======\n", where
   <LIST> is the list of valid arguments for option opt. */
void PrintValidArgs(char opt) {
  const char *s;

  PrintOut(LOG_CRIT, "=======> VALID ARGUMENTS ARE: ");
  if (!(s = GetValidArgList(opt)))
    PrintOut(LOG_CRIT, "Error constructing argument list for option %c", opt);
  else
    PrintOut(LOG_CRIT, "%s", (char *)s);
  PrintOut(LOG_CRIT, " <=======\n");
}

// Return true if absolute path name
static bool is_abs_path(const char * path)
{
  if (*path == '/')
    return true;
#if defined(_WIN32) || defined(__CYGWIN__)
  if (*path == '\\')
    return true;
  int n = -1;
  sscanf(path, "%*1[A-Za-z]:%*1[/\\]%n", &n);
  if (n > 0)
    return true;
#endif
  return false;
}

// Parses input line, prints usage message and
// version/license/copyright messages
void ParseOpts(int argc, char **argv){
  int optchar;
  char *tailptr;
  long lchecktime;
  // Please update GetValidArgList() if you edit shortopts
  const char *shortopts = "c:l:q:dDni:p:r:s:B:Vh?";
  char *arg;
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
#endif
    { "pidfile",        required_argument, 0, 'p' },
    { "report",         required_argument, 0, 'r' },
    { "savestates",     required_argument, 0, 's' },
    { "drivedb",        required_argument, 0, 'B' },
#if defined(_WIN32) || defined(__CYGWIN__)
    { "service",        no_argument,       0, 'n' },
#endif
    { "version",        no_argument,       0, 'V' },
    { "license",        no_argument,       0, 'V' },
    { "copyright",      no_argument,       0, 'V' },
    { "help",           no_argument,       0, 'h' },
    { "usage",          no_argument,       0, 'h' },
    { 0,                0,                 0, 0   }
  };

  opterr=optopt=0;
  bool badarg = false;
  bool no_defaultdb = false; // set true on '-B FILE'

  // Parse input options.  This horrible construction is so that emacs
  // indents properly.  Sorry.
  while (-1 != (optchar = 
                getopt_long(argc, argv, shortopts, longopts, NULL)
                )) {
    
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
          con->reportataioctl  = con->reportscsiioctl = i;
        } else if (!strcmp(s,"ataioctl")) {
          con->reportataioctl = i;
        } else if (!strcmp(s,"scsiioctl")) {
          con->reportscsiioctl = i;
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
    case 'V':
      // print version and CVS info
      PrintCopyleft();
      EXIT(0);
      break;
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

  // absolute path is required due to chdir('/') after fork().
  if (!state_path_prefix.empty() && !debugmode && !is_abs_path(state_path_prefix.c_str())) {
    debugmode=1;
    PrintHead();
    PrintOut(LOG_CRIT, "=======> INVALID CHOICE OF OPTIONS: -s <======= \n\n");
    PrintOut(LOG_CRIT, "Error: relative path %s is only allowed in debug (-d) mode\n\n",
      state_path_prefix.c_str());
    EXIT(EXIT_BADCMD);
  }

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

  // loop over entries to create
  for (unsigned i = 0; i < devlist.size(); i++) {
    // Move device pointer
    smart_device * dev = devlist.release(i);
    scanned_devs.push_back(dev);

    // Copy configuration, update device and type name
    conf_entries.push_back(base_cfg);
    dev_config & cfg = conf_entries.back();
    cfg.name = dev->get_info().info_name;
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
// If we found no configuration file, or it contained SCANDIRECTIVE,
// then *scanning is set to 1, else 0.
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
  else if (conf_entries.size() == 1) {
    // we found a SCANDIRECTIVE or there was no configuration file so
    // scan.  Configuration file's first entry contains all options
    // that were set
    dev_config first = conf_entries.front();
    conf_entries.clear();

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
    PrintOut(LOG_CRIT,"Configuration file %s parsed but has no entries (like /dev/hda)\n",configfile);
  
  return conf_entries.size();
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
  for (unsigned i = 0; i < conf_entries.size(); i++){

    dev_config cfg = conf_entries[i];

    // get device of appropriate type
    // TODO: exception handling
    smart_device * dev = 0;
    bool scanning = false;

    // Device may already be detected during devicescan
    if (i < scanned_devs.size()) {
      dev = scanned_devs.release(i);
      if (dev)
        scanning = true;
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
    dev = dev->autodetect_open();

    // Report if type has changed
    if (/* ent->dev_type && */ oldinfo.dev_type != dev->get_dev_type())
      PrintOut(LOG_INFO,"Device: %s, type changed from '%s' to '%s'\n",
        cfg.name.c_str(), oldinfo.dev_type.c_str(), dev->get_dev_type());

    if (!dev->is_open()) {
      // For linux+devfs, a nonexistent device gives a strange error
      // message.  This makes the error message a bit more sensible.
      // If no debug and scanning - don't print errors
      if (debugmode || !scanning)
        PrintOut(LOG_INFO, "Device: %s, open() failed: %s\n", dev->get_info_name(), dev->get_errmsg());
      delete dev;
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
        delete dev; dev = 0;
      }
      else {
        // move onto the list of ata devices
        configs.push_back(cfg);
        states.push_back(state);
        devices.push_back(dev);
      }
    }
    
    // or register SCSI devices
    else if (dev->is_scsi()){
      if (SCSIDeviceScan(cfg, state, dev->to_scsi())) {
        CanNotRegister(cfg.name.c_str(), "SCSI", cfg.lineno, scanning);
        delete dev; dev = 0;
      }
      else {
        // move onto the list of scsi devices
        configs.push_back(cfg);
        states.push_back(state);
        devices.push_back(dev);
      }
    }
    
    // if device is explictly listed and we can't register it, then
    // exit unless the user has specified that the device is removable
    if (!dev && !scanning) {
      if (cfg.removable || quit==2)
        PrintOut(LOG_INFO, "Device %s not available\n", cfg.name.c_str());
      else {
        PrintOut(LOG_CRIT, "Unable to register device %s (no Directive -d removable). Exiting.\n", cfg.name.c_str());
        EXIT(EXIT_BADDEV);
      }
    }
  }
}


// Main program without exception handling
int main_worker(int argc, char **argv)
{
  // Initialize interface
  smart_interface::init();
  if (!smi())
    return 1;

  // external control variables for ATA disks
  smartmonctrl control;

  // is it our first pass through?
  bool firstpass = true;

  // next time to wake up
  time_t wakeuptime;

  // for simplicity, null all global communications variables/lists
  con=&control;
  memset(con,        0,sizeof(control));

  // parse input and print header and usage info if needed
  ParseOpts(argc,argv);
  
  // do we mute printing from ataprint commands?
  con->printing_switchable = false;
  con->dont_print = !debugmode;
  
  // Configuration for each device
  dev_config_vector configs;
  // Device states
  dev_state_vector states;
  // Devices to monitor
  smart_device_list devices;

  bool write_states_always = true;

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
#ifdef __CYGWIN__
        // Workaround for missing SIGQUIT via keyboard on Cygwin
        if (caughtsigHUP==2) {
          // Simulate SIGQUIT if another SIGINT arrives soon
          caughtsigHUP=0;
          sleep(1);
          if (caughtsigHUP==2) {
            caughtsigEXIT=SIGQUIT;
            continue;
          }
          caughtsigHUP=2;
        }
#endif
        // Write state files
        if (!state_path_prefix.empty())
          write_all_dev_states(configs, states);

        PrintOut(LOG_INFO,
                 caughtsigHUP==1?
                 "Signal HUP - rereading configuration file %s\n":
                 "\a\nSignal INT - rereading configuration file %s ("SIGQUIT_KEYNAME" quits)\n\n",
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
          int status = (entries==-3 ? EXIT_READCONF : entries==-2 ? EXIT_NOCONF : EXIT_BADCONF);
          EXIT(status);
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
        EXIT(EXIT_NODEV);
      }

      if (quit==4) {
        // user has asked to print test schedule
        PrintTestSchedule(configs, states, devices);
        EXIT(0);
      }
      
      // reset signal
      caughtsigHUP=0;

      // Always write state files after (re)configuration
      write_states_always = true;
    }

    // check all devices once,
    // self tests are not started in first pass unless '-q onecheck' is specified
    CheckDevicesOnce(configs, states, devices, (!firstpass || quit==3));

     // Write state files
    if (!state_path_prefix.empty())
      write_all_dev_states(configs, states, write_states_always);
    write_states_always = false;

    // user has asked us to exit after first check
    if (quit==3) {
      PrintOut(LOG_INFO,"Started with '-q onecheck' option. All devices sucessfully checked once.\n"
               "smartd is exiting (exit status 0)\n");
      EXIT(0);
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
    "Analysis and Reporting Technology System (S.M.A.R.T.) "
    "built into ATA and SCSI Hard Drives. "
    PACKAGE_HOMEPAGE
  };
  // daemon_main() handles daemon and service specific commands
  // and starts smartd_main() direct, from a new process,
  // or via service control manager
  return daemon_main("smartd", &svc_opts , smartd_main, argc, argv);
}
#endif
