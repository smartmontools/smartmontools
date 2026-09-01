/*
 * atacmds.h
 *
 * Home page of code is: https://www.smartmontools.org
 *
 * Copyright (C) 1999-2000 Michael Cornwell <cornwell@acm.org>
 * Copyright (C) 2002-2011 Bruce Allen
 * Copyright (C) 2008-2026 Christian Franke
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

/// Call ATA pass-through and print debug info if requested.
bool ata_pass_through(ata_device * device, const ata_cmd_in & in, ata_cmd_out & out);

/// Call ATA pass-through and print debug info if requested.
/// Variant without output parameters.
bool ata_pass_through(ata_device * device, const ata_cmd_in & in);

/// Print debug information for ATA pass-through input.
void ata_print_debug_info(const ata_cmd_in & in, const char * devname, bool dump);

/// Print debug information for ATA pass-through output.
void ata_print_debug_info(const ata_cmd_in & in, const ata_cmd_out & out,
                          const smart_device::error_info & err, bool dump,
                          long long duration_usec);

// Get information from drive
int ata_read_identity(ata_device * device, ata_identify_device & id,
                      bool fix_swapped_id = false);

int ata_check_power_mode(ata_device * device);

// Issue a no-data ATA command with optional sector count register value
bool ata_nodata_command(ata_device * device, uint8_t command);
bool ata_nodata_command(ata_device * device, uint8_t command, uint8_t sector_count);

// Issue SET FEATURES command with optional sector count register value
bool ata_set_features(ata_device * device, uint8_t features);
bool ata_set_features(ata_device * device, uint8_t features, uint8_t sector_count);

/* Read S.M.A.R.T information from drive */
bool ata_read_smart_data(ata_device * device, ata_smart_values & data);
bool ata_read_smart_thresholds(ata_device * device, ata_smart_thresholds_pvt & thr);
bool ata_read_smart_error_log (ata_device * device, ata_smart_errorlog & log,
  firmwarebug_defs firmwarebugs);
bool ata_read_smart_self_test_log(ata_device * device, ata_smart_selftestlog & log,
  firmwarebug_defs firmwarebugs);
bool ata_read_smart_selective_self_test_log(ata_device * device,
  ata_selective_self_test_log & log);
bool ata_read_log_directory(ata_device * device, ata_smart_log_directory & log, bool gpl);

// Read GP Log page(s)
bool ata_read_log_ext(ata_device * device, uint8_t logaddr, uint8_t features, uint16_t page,
  void * log, uint16_t nsectors);

// Write GP Log page(s)
bool ata_write_log_ext(ata_device * device, uint8_t logaddr, uint16_t page, const void * log,
  uint16_t nsectors);

// Read SMART Log page(s)
bool ata_read_smart_log(ata_device * device, uint8_t logaddr, void * log, uint8_t nsectors);

/// Write SMART Log page(s).
bool ata_write_smart_log(ata_device * device, uint8_t logaddr, const void * log, uint8_t nsectors);

// Read SMART Extended Comprehensive Error Log
bool ata_read_smart_ext_comp_error_log(ata_device * device, ata_smart_exterrlog * log,
  uint16_t page, uint16_t nsectors, firmwarebug_defs firmwarebugs);

// Read SMART Extended Self-test Log
bool ata_read_smart_ext_self_test_log(ata_device * device, ata_smart_extselftestlog * log,
  uint16_t nsectors);

// Read SCT information
bool ata_read_sct_status(ata_device * device, ata_sct_status_response & sts);
bool ata_read_sct_temperature_history(ata_device * device, ata_sct_temperature_history_table & tmh,
  ata_sct_status_response & sts);
// Set SCT temperature logging interval
bool ata_set_sct_temperature_interval(ata_device * device, uint16_t interval, bool persistent);

// Get/Set SCT Error Recovery Control
bool ata_get_sct_erc_time(ata_device * device, uint16_t type, uint16_t & time_limit,
  bool power_on);
bool ata_set_sct_erc_time(ata_device * device, uint16_t type, uint16_t time_limit,
  bool power_on, bool mfg_default);

/* Enable/Disable SMART on device */
bool ata_enable_smart(ata_device * device, bool enable = true);
bool ata_enable_smart_auto_save(ata_device * device, bool enable = true);

/* Automatic Offline Testing */
bool ata_enable_smart_auto_offline(ata_device * device, bool enable = true);

/* S.M.A.R.T. test commands */
bool ata_smart_self_test(ata_device * device, uint8_t testtype);

/// Read/write selective self-test log to prepare a selective self-test.
/// Return 1 on success, 0 if a test is already running or  -1 on error.
int ata_prepare_selective_self_test(ata_device * device, ata_selective_selftest_args & args,
  const ata_smart_values & sv, uint64_t num_sectors,
  const ata_selective_selftest_args * prev_spans = nullptr);

// Get World Wide Name (WWN) fields.
// Return NAA field or -1 if WWN is unsupported.
int ata_get_wwn(const ata_identify_device & id, uint32_t & oui, uint64_t & unique_id);

// Get nominal media rotation rate.
// Returns: 0 = not reported, 1 = SSD, >1 = HDD rpm, < 0 = -(Unknown value)
int ata_get_rotation_rate(const ata_identify_device & id);

// If SMART supported, this is guaranteed to return 1 if SMART is enabled, else 0.
bool ata_is_smart_status_working(ata_device * device);

// returns 1 if SMART supported, 0 if not supported or can't tell
int ata_is_smart_supported(const ata_identify_device & id);

// Return values:
//  1: Write Cache Reordering enabled
//  2: Write Cache Reordering disabled
// -1: error
int ata_get_set_sct_write_cache_reordering(ata_device * device, bool enable, bool persistent,
  bool set);

// Return values:
// 1: Write cache controlled by ATA Set Features command
// 2: Force enable write cache
// 3: Force disable write cache
int ata_get_set_sct_write_cache(ata_device * device, uint16_t state, bool persistent, bool set);

// Return values:
//  1: SMART enabled
//  0: SMART disabled
// -1: can't tell if SMART is enabled -- try issuing ataDoesSmartWork command to see
int ata_is_smart_enabled(const ata_identify_device & id);

/// Issue SMART STATUS command and check the result.
/// Return 0 if "good" status, 1 if "failed" status and -1 on error.
int ata_get_smart_status(ata_device * device);

// Get reference to modify word N from `ata_identify_device.words*[]` arrays.
// Does not compile for the other fields.
template <int N>
static inline uint16_t & ata_set_id_word(ata_identify_device & id)
{
  SMARTMON_STATIC_ASSERT(   (  0 <= N && N <=   9) || ( 20 <= N && N <=  22)
                         || ( 47 <= N && N <=  59) || ( 62 <= N && N <=  79)
                         || ( 88 <= N && N <=  99) || (104 <= N && N <= 169)
                         || (174 <= N && N <= 229) || (234 <= N && N <= 255));
  if (N < 20)
    return id.words000_009[N];
  else if (N < 47)
    return id.words020_022[N -  20];
  else if (N < 62)
    return id.words047_059[N -  47];
  else if (N < 88)
    return id.words062_079[N -  62];
  else if (N < 104)
    return id.words088_099[N -  88];
  else if (N < 174)
    return id.words104_169[N - 104];
  else if (N < 234)
    return id.words174_229[N - 174];
  else
    return id.words234_255[N - 234];
}

// Get const reference to word N from `ata_identify_device.words*[]` arrays.
template <int N>
static inline const uint16_t & ata_get_id_word(const ata_identify_device & id)
{
  return ata_set_id_word<N>(const_cast<ata_identify_device &>(id));
}

bool ata_is_smart_error_log_capable(const ata_smart_values & data, const ata_identify_device & id);

bool ata_is_smart_self_test_log_capable(const ata_smart_values & data, const ata_identify_device & id);

bool ata_is_gp_log_capable(const ata_identify_device & id);

// SMART self-test capability is also indicated in bit 1 of DEVICE
// IDENTIFY word 87 (if top two bits of word 87 match pattern 01).
// However this was only introduced in ATA-6 (but self-test log was in
// ATA-5).
static inline bool ata_is_offline_immediate_capable(const ata_smart_values & data)
  { return !!(data.offline_data_collection_capability & 0x01); }

// TODO: Remove uses of this check.  Bit 1 is vendor specific since ATA-4.
// Automatic timer support was only documented for very old IBM drives
// (for example IBM Travelstar 40GNX).
static inline bool ata_is_automatic_timer_capable(const ata_smart_values & data)
  { return !!(data.offline_data_collection_capability & 0x02); }

static inline bool ata_is_offline_abort_capable(const ata_smart_values & data)
  { return !!(data.offline_data_collection_capability & 0x04); }

static inline bool ata_is_offline_surface_scan_capable(const ata_smart_values & data)
  { return !!(data.offline_data_collection_capability & 0x08); }

static inline bool ata_is_self_test_capable(const ata_smart_values & data)
  { return !!(data.offline_data_collection_capability & 0x10); }

static inline bool ata_is_conveyance_self_test_capable(const ata_smart_values & data)
  { return !!(data.offline_data_collection_capability & 0x20); }

static inline bool ata_is_selective_self_test_capable(const ata_smart_values & data)
  { return !!(data.offline_data_collection_capability & 0x40); }

static inline bool ata_is_sct_capable(const ata_identify_device & id)
  { return !!(ata_get_id_word<206>(id) & 0x01); } // 0x01 = SCT support

static inline bool ata_is_sct_erc_capable(const ata_identify_device & id)
  { return ((ata_get_id_word<206>(id) & 0x09) == 0x09); } // 0x08 = SCT Error Recovery Control support

static inline bool ata_is_sct_feature_control_capable(const ata_identify_device & id)
  { return ((ata_get_id_word<206>(id) & 0x11) == 0x11); } // 0x10 = SCT Feature Control support

static inline bool ata_is_sct_data_table_capable(const ata_identify_device & id)
  { return ((ata_get_id_word<206>(id) & 0x21) == 0x21); } // 0x20 = SCT Data Table support

/// Return estimated time (minimum polling interval in minutes) for a self-test of type TESTTYPE.
int ata_get_smart_self_test_minutes(const ata_smart_values & data, uint8_t testtype);

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

/// Calculate or check the checksum of 512 byte ATA sector.
/// Returns 0 if correct.
uint8_t ata_checksum(const void * data);

// Returns the name of the command (and possibly sub-command) with the given
// command code and feature register values.
const char * ata_get_command_name(uint8_t command, uint8_t features);

// Byteswap strings in identify_device data.
void ata_byteswap_id_strings_inplace(ata_identify_device & id, bool all = true);

// Byteswap all aligned integers on Big Endian platforms, do nothing otherwise.
void ata_if_be_byteswap_inplace(ata_identify_device & id);
void ata_if_be_byteswap_inplace(ata_smart_values & val);
void ata_if_be_byteswap_inplace(ata_smart_thresholds_pvt & thr);
void ata_if_be_byteswap_inplace(ata_smart_log_directory & log);
void ata_if_be_byteswap_inplace(ata_smart_errorlog & log);
void ata_if_be_byteswap_inplace(ata_smart_selftestlog & log);
void ata_if_be_byteswap_inplace(ata_smart_exterrlog * log, unsigned num_sectors);
void ata_if_be_byteswap_inplace(ata_smart_extselftestlog * log, unsigned num_sectors);
void ata_if_be_byteswap_inplace(ata_selective_self_test_log & log);
void ata_if_be_byteswap_inplace(ata_sct_status_response & sts);
void ata_if_be_byteswap_inplace(ata_sct_data_table_command & cmd);
void ata_if_be_byteswap_inplace(ata_sct_temperature_history_table & tmh);
void ata_if_be_byteswap_inplace(ata_sct_feature_control_command & cmd);
void ata_if_be_byteswap_inplace(ata_sct_error_recovery_control_command & cmd);

// Return pseudo-device to parse "smartctl -r ataioctl,2 ..." output
// and simulate an ATA device with same behaviour
ata_device * get_parsed_ata_device(smart_interface * intf, const char * dev_name);

} // namespace smartmon

#endif // SMARTMON_ATACMDS_H
