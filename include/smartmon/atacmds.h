/*
 * atacmds.h
 *
 * Home page of code is: https://www.smartmontools.org
 *
 * Copyright (C) 1999-2000 Michael Cornwell <cornwell@acm.org>
 * Copyright (C) 2002-2011 Bruce Allen
 * Copyright (C) 2008-2025 Christian Franke
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef SMARTMON_ATACMDS_H
#define SMARTMON_ATACMDS_H

#include <smartmon/ata.h>
#include <smartmon/dev_interface.h> // ata_device

namespace smartmon {

typedef enum {
  // returns no data, just succeeds or fails
  ENABLE,
  DISABLE,
  AUTOSAVE,
  IMMEDIATE_OFFLINE,
  AUTO_OFFLINE,
  STATUS,       // just says if SMART is working or not
  STATUS_CHECK, // says if disk's SMART status is healthy, or failing
  // return 512 bytes of data:
  READ_VALUES,
  READ_THRESHOLDS,
  READ_LOG,
  IDENTIFY,
  PIDENTIFY,
  // returns 1 byte of data
  CHECK_POWER_MODE,
  // writes 512 bytes of data:
  WRITE_LOG
} smart_command_set;

/// Class to register an application specific checksum error handler.
class lib_ata_hook
{
public:
  lib_ata_hook() = default;
  virtual ~lib_ata_hook() = default;
  lib_ata_hook(const lib_ata_hook &) = delete;
  void operator=(const lib_ata_hook &) = delete;

  /// Get the current hook.
  static lib_ata_hook & get();

  /// Set the hook.
  static void set(lib_ata_hook & hook);

  /// Reset to default hook.
  static void reset();

  /// Handle an incorrect checksum in an ATA structure: Do nothing, print a
  /// message, or print a message and throw.  The parameter describes the ATA
  /// data structure.
  /// The default implementation prints a warning via lib_printf().
  virtual void on_checksum_error(const char * datatype);
};

// Possible values for span_args.mode
enum {
  SEL_RANGE, // MIN-MAX
  SEL_REDO,  // redo this
  SEL_NEXT,  // do next range
  SEL_CONT   // redo or next depending of last test status
};

// Arguments for selective self-test
struct ata_selective_selftest_args
{
  // Arguments for each span
  struct span_args
  {
    uint64_t start;   // First block
    uint64_t end;     // Last block
    int mode;         // SEL_*, see above

    span_args()
      : start(0), end(0), mode(SEL_RANGE) { }
  };

  span_args span[5];  // Range and mode for 5 spans
  int num_spans;      // Number of spans
  int pending_time;   // One plus time in minutes to wait after powerup before restarting
                      // interrupted offline scan after selective self-test.
  int scan_after_select; // Run offline scan after selective self-test:
                      // 0: don't change,
                      // 1: turn off scan after selective self-test,
                      // 2: turn on scan after selective self-test.

  ata_selective_selftest_args()
    : num_spans(0), pending_time(0), scan_after_select(0) { }
};

// Priority for vendor attribute defs
enum ata_vendor_def_prior
{
  PRIOR_DEFAULT,
  PRIOR_DATABASE,
  PRIOR_USER
};

// Raw attribute value print formats
enum ata_attr_raw_format
{
  RAWFMT_DEFAULT,
  RAWFMT_RAW8,
  RAWFMT_RAW16,
  RAWFMT_RAW48,
  RAWFMT_HEX48,
  RAWFMT_RAW56,
  RAWFMT_HEX56,
  RAWFMT_RAW64,
  RAWFMT_HEX64,
  RAWFMT_RAW16_OPT_RAW16,
  RAWFMT_RAW16_OPT_AVG16,
  RAWFMT_RAW24_OPT_RAW8,
  RAWFMT_RAW24_DIV_RAW24,
  RAWFMT_RAW24_DIV_RAW32,
  RAWFMT_SEC2HOUR,
  RAWFMT_MIN2HOUR,
  RAWFMT_HALFMIN2HOUR,
  RAWFMT_MSEC24_HOUR32,
  RAWFMT_TEMPMINMAX,
  RAWFMT_TEMP10X,
};

// Attribute flags
enum {
  ATTRFLAG_INCREASING  = 0x01, // Value not reset (for reallocated/pending counts)
  ATTRFLAG_NO_NORMVAL  = 0x02, // Normalized value not valid
  ATTRFLAG_NO_WORSTVAL = 0x04, // Worst value not valid
  ATTRFLAG_HDD_ONLY    = 0x08, // DEFAULT setting for HDD only
  ATTRFLAG_SSD_ONLY    = 0x10, // DEFAULT setting for SSD only
};

// Vendor attribute display defs for all attribute ids
class ata_vendor_attr_defs
{
public:
  struct entry
  {
    std::string name; // Attribute name, empty for default
    ata_attr_raw_format raw_format; // Raw value print format
    ata_vendor_def_prior priority; // Setting priority
    unsigned flags; // ATTRFLAG_*
    char byteorder[8+1]; // String [012345rvwz] to define byte order

    entry()
      : raw_format(RAWFMT_DEFAULT),
        priority(PRIOR_DEFAULT),
        flags(0)
      { byteorder[0] = 0; }
  };

  entry & operator[](unsigned char id)
    { return m_defs[id]; }

  const entry & operator[](unsigned char id) const
    { return m_defs[id]; }

private:
  entry m_defs[256];
};


// Possible values for firmwarebugs
enum firmwarebug_t {
  BUG_NONE = 0,
  BUG_NOLOGDIR,
  BUG_SAMSUNG,
  BUG_SAMSUNG2,
  BUG_SAMSUNG3,
  BUG_XERRORLBA
};

// Set of firmware bugs
class firmwarebug_defs
{
public:
  firmwarebug_defs()
    : m_bugs(0) { }

  bool is_set(firmwarebug_t bug) const
    { return !!(m_bugs & (1 << bug)); }

  void set(firmwarebug_t bug)
    { m_bugs |= (1 << bug); }

  void set(firmwarebug_defs bugs)
    { m_bugs |= bugs.m_bugs; }

private:
  unsigned m_bugs;
};


// Print ATA debug messages?
extern unsigned char ata_debugmode;

// Suppress serial number?
extern bool dont_print_serial_number;

// Get information from drive
int ata_read_identity(ata_device * device, ata_identify_device * buf, bool fix_swapped_id,
                      unsigned char * raw_buf = 0);
int ataCheckPowerMode(ata_device * device);

// Issue a no-data ATA command with optional sector count register value
bool ata_nodata_command(ata_device * device, unsigned char command, int sector_count = -1);

// Issue SET FEATURES command with optional sector count register value
bool ata_set_features(ata_device * device, unsigned char features, int sector_count = -1);

/* Read S.M.A.R.T information from drive */
int ataReadSmartValues(ata_device * device,struct ata_smart_values *);
int ataReadSmartThresholds(ata_device * device, struct ata_smart_thresholds_pvt *);
int ataReadErrorLog (ata_device * device, ata_smart_errorlog *data,
                     firmwarebug_defs firmwarebugs);
int ataReadSelfTestLog(ata_device * device, ata_smart_selftestlog * data,
                       firmwarebug_defs firmwarebugs);
int ataReadSelectiveSelfTestLog(ata_device * device, struct ata_selective_self_test_log *data);
int ataReadLogDirectory(ata_device * device, ata_smart_log_directory *, bool gpl);

// Write GP Log page(s)
bool ataWriteLogExt(ata_device * device, unsigned char logaddr,
                    unsigned page, void * data, unsigned nsectors);

// Read GP Log page(s)
bool ataReadLogExt(ata_device * device, unsigned char logaddr,
                   unsigned char features, unsigned page,
                   void * data, unsigned nsectors);
// Read SMART Log page(s)
bool ataReadSmartLog(ata_device * device, unsigned char logaddr,
                     void * data, unsigned nsectors);
// Read SMART Extended Comprehensive Error Log
bool ataReadExtErrorLog(ata_device * device, ata_smart_exterrlog * log,
                        unsigned page, unsigned nsectors, firmwarebug_defs firmwarebugs);
// Read SMART Extended Self-test Log
bool ataReadExtSelfTestLog(ata_device * device, ata_smart_extselftestlog * log,
                           unsigned nsectors);

// Read SCT information
int ataReadSCTStatus(ata_device * device, ata_sct_status_response * sts);
int ataReadSCTTempHist(ata_device * device, ata_sct_temperature_history_table * tmh,
                       ata_sct_status_response * sts);
// Set SCT temperature logging interval
int ataSetSCTTempInterval(ata_device * device, unsigned interval, bool persistent);

// Get/Set SCT Error Recovery Control
int ataGetSCTErrorRecoveryControltime(ata_device * device, unsigned type, unsigned short & time_limit, bool power_on);
int ataSetSCTErrorRecoveryControltime(ata_device * device, unsigned type, unsigned short time_limit, bool power_on, bool mfg_default);


/* Enable/Disable SMART on device */
int ataEnableSmart (ata_device * device);
int ataDisableSmart (ata_device * device);
int ataEnableAutoSave(ata_device * device);
int ataDisableAutoSave(ata_device * device);

/* Automatic Offline Testing */
int ataEnableAutoOffline (ata_device * device);
int ataDisableAutoOffline (ata_device * device);

/* S.M.A.R.T. test commands */
int ataSmartTest(ata_device * device, int testtype, bool force,
                 const ata_selective_selftest_args & args,
                 const ata_smart_values * sv, uint64_t num_sectors);

int ataWriteSelectiveSelfTestLog(ata_device * device, ata_selective_selftest_args & args,
                                 const ata_smart_values * sv, uint64_t num_sectors,
                                 const ata_selective_selftest_args * prev_spans = 0);

// Get World Wide Name (WWN) fields.
// Return NAA field or -1 if WWN is unsupported.
int ata_get_wwn(const ata_identify_device * id, unsigned & oui, uint64_t & unique_id);

// Get nominal media rotation rate.
// Returns: 0 = not reported, 1 = SSD, >1 = HDD rpm, < 0 = -(Unknown value)
int ata_get_rotation_rate(const ata_identify_device * id);

// If SMART supported, this is guaranteed to return 1 if SMART is enabled, else 0.
int ataDoesSmartWork(ata_device * device);

// returns 1 if SMART supported, 0 if not supported or can't tell
int ataSmartSupport(const ata_identify_device * drive);

// Return values:
//  1: Write Cache Reordering enabled
//  2: Write Cache Reordering disabled
// -1: error
int ataGetSetSCTWriteCacheReordering(ata_device * device, bool enable, bool persistent, bool set);

// Return values:
// 1: Write cache controlled by ATA Set Features command
// 2: Force enable write cache
// 3: Force disable write cache
int ataGetSetSCTWriteCache(ata_device * device, unsigned short state, bool persistent, bool set);

// Return values:
//  1: SMART enabled
//  0: SMART disabled
// -1: can't tell if SMART is enabled -- try issuing ataDoesSmartWork command to see
int ataIsSmartEnabled(const ata_identify_device * drive);

int ataSmartStatus2(ata_device * device);

bool isSmartErrorLogCapable(const ata_smart_values * data, const ata_identify_device * identity);

bool isSmartTestLogCapable(const ata_smart_values * data, const ata_identify_device * identity);

bool isGeneralPurposeLoggingCapable(const ata_identify_device * identity);

// SMART self-test capability is also indicated in bit 1 of DEVICE
// IDENTIFY word 87 (if top two bits of word 87 match pattern 01).
// However this was only introduced in ATA-6 (but self-test log was in
// ATA-5).
inline bool isSupportExecuteOfflineImmediate(const ata_smart_values *data)
  { return !!(data->offline_data_collection_capability & 0x01); }

// TODO: Remove uses of this check.  Bit 1 is vendor specific since ATA-4.
// Automatic timer support was only documented for very old IBM drives
// (for example IBM Travelstar 40GNX).
inline bool isSupportAutomaticTimer(const ata_smart_values * data)
  { return !!(data->offline_data_collection_capability & 0x02); }

inline bool isSupportOfflineAbort(const ata_smart_values *data)
  { return !!(data->offline_data_collection_capability & 0x04); }

inline bool isSupportOfflineSurfaceScan(const ata_smart_values * data)
  { return !!(data->offline_data_collection_capability & 0x08); }

inline bool isSupportSelfTest(const ata_smart_values * data)
  { return !!(data->offline_data_collection_capability & 0x10); }

inline bool isSupportConveyanceSelfTest(const ata_smart_values * data)
  { return !!(data->offline_data_collection_capability & 0x20); }

inline bool isSupportSelectiveSelfTest(const ata_smart_values * data)
  { return !!(data->offline_data_collection_capability & 0x40); }

inline bool isSCTCapable(const ata_identify_device *drive)
  { return !!(drive->words088_255[206-88] & 0x01); } // 0x01 = SCT support

inline bool isSCTErrorRecoveryControlCapable(const ata_identify_device *drive)
  { return ((drive->words088_255[206-88] & 0x09) == 0x09); } // 0x08 = SCT Error Recovery Control support

inline bool isSCTFeatureControlCapable(const ata_identify_device *drive)
  { return ((drive->words088_255[206-88] & 0x11) == 0x11); } // 0x10 = SCT Feature Control support

inline bool isSCTDataTableCapable(const ata_identify_device *drive)
  { return ((drive->words088_255[206-88] & 0x21) == 0x21); } // 0x20 = SCT Data Table support

int TestTime(const ata_smart_values * data, int testtype);

// Attribute state
enum ata_attr_state
{
  ATTRSTATE_NON_EXISTING,   // No such Attribute
  ATTRSTATE_NO_NORMVAL,     // Normalized value not valid
  ATTRSTATE_NO_THRESHOLD,   // Unknown or no threshold
  ATTRSTATE_OK,             // Never failed
  ATTRSTATE_FAILED_PAST,    // Failed in the past
  ATTRSTATE_FAILED_NOW      // Failed now
};

// Get attribute state
ata_attr_state ata_get_attr_state(const ata_smart_attribute & attr,
                                  int attridx,
                                  const ata_smart_threshold_entry * thresholds,
                                  const ata_vendor_attr_defs & defs,
                                  unsigned char * threshval = 0);

// Get attribute raw value.
uint64_t ata_get_attr_raw_value(const ata_smart_attribute & attr,
                                const ata_vendor_attr_defs & defs);

// Format attribute raw value.
std::string ata_format_attr_raw_value(const ata_smart_attribute & attr,
                                      const ata_vendor_attr_defs & defs);

// Get attribute name
std::string ata_get_smart_attr_name(unsigned char id,
                                    const ata_vendor_attr_defs & defs,
                                    int rpm = 0);

// Find attribute index for attribute id, -1 if not found.
int ata_find_attr_index(unsigned char id, const ata_smart_values & smartval);

// Return Temperature Attribute raw value selected according to possible
// non-default interpretations. If the Attribute does not exist, return 0
unsigned char ata_return_temperature_value(const ata_smart_values * data, const ata_vendor_attr_defs & defs);


#define MAX_ATTRIBUTE_NUM 256

// Parse vendor attribute display def (-v option).
// Return false on error.
bool parse_attribute_def(const char * opt, ata_vendor_attr_defs & defs,
                         ata_vendor_def_prior priority);

// Get ID and increase flag of current pending or offline
// uncorrectable attribute.
unsigned char get_unc_attr_id(bool offline, const ata_vendor_attr_defs & defs,
                              bool & increase);

// Return a multiline string containing a list of valid arguments for
// parse_attribute_def().
std::string create_vendor_attribute_arg_list();

// Parse firmwarebug def (-F option).
// Return false on error.
bool parse_firmwarebug_def(const char * opt, firmwarebug_defs & firmwarebugs);

// Return a string of valid argument words for parse_firmwarebug_def()
const char * get_valid_firmwarebug_args();


// These are two of the functions that are defined in os_*.c and need
// to be ported to get smartmontools onto another OS.
// Moved to C++ interface
//int ata_command_interface(int device, smart_command_set command, int select, char *data);
//int escalade_command_interface(int fd, int escalade_port, int escalade_type, smart_command_set command, int select, char *data);
//int marvell_command_interface(int device, smart_command_set command, int select, char *data);
//int highpoint_command_interface(int device, smart_command_set command, int select, char *data);
//int areca_command_interface(int fd, int disknum, smart_command_set command, int select, char *data);


// This function is exported to give low-level capability
int smartcommandhandler(ata_device * device, smart_command_set command, int select, char *data);

// Get capacity and sector sizes from IDENTIFY data
struct ata_size_info
{
  uint64_t sectors;
  uint64_t capacity;
  unsigned log_sector_size;
  unsigned phy_sector_size;
  unsigned log_sector_offset;
};

void ata_get_size_info(const ata_identify_device * id, ata_size_info & sizes);

// Convenience function for formatting strings from ata_identify_device.
void ata_format_id_string(char * out, const unsigned char * in, int n);

// Utility routines.
unsigned char checksum(const void * data);

// Returns the name of the command (and possibly sub-command) with the given
// command code and feature register values.
const char * look_up_ata_command(unsigned char c_code, unsigned char f_reg);

// Return pseudo-device to parse "smartctl -r ataioctl,2 ..." output
// and simulate an ATA device with same behaviour
ata_device * get_parsed_ata_device(smart_interface * intf, const char * dev_name);

} // namespace smartmon

#endif // SMARTMON_ATACMDS_H
