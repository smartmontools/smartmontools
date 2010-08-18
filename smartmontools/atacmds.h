/*
 * atacmds.h
 *
 * Home page of code is: http://smartmontools.sourceforge.net
 *
 * Copyright (C) 2002-9 Bruce Allen <smartmontools-support@lists.sourceforge.net>
 * Copyright (C) 2008-9 Christian Franke <smartmontools-support@lists.sourceforge.net>
 * Copyright (C) 1999-2000 Michael Cornwell <cornwell@acm.org>
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

#ifndef ATACMDS_H_
#define ATACMDS_H_

#define ATACMDS_H_CVSID "$Id$"

#include "dev_interface.h" // ata_device

// Macro to check expected size of struct at compile time using a
// dummy typedef.  On size mismatch, compiler reports a negative array
// size.  If you see an error message of this form, it means that the
// #pragma pack(1) pragma below is not having the desired effect on
// your compiler.
#define ASSERT_SIZEOF_STRUCT(s, n) \
  typedef char assert_sizeof_struct_##s[(sizeof(struct s) == (n)) ? 1 : -1]

// Add __attribute__((packed)) if compiler supports it
// because some gcc versions (at least ARM) lack support of #pragma pack()
#ifdef HAVE_ATTR_PACKED
#define ATTR_PACKED __attribute__((packed))
#else
#define ATTR_PACKED
#endif

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

// Possible values for fix_firmwarebug.
enum {
  FIX_NOTSPECIFIED = 0,
  FIX_NONE,
  FIX_SAMSUNG,
  FIX_SAMSUNG2,
  FIX_SAMSUNG3
};

// ATA Specification Command Register Values (Commands)
#define ATA_IDENTIFY_DEVICE             0xec
#define ATA_IDENTIFY_PACKET_DEVICE      0xa1
#define ATA_SMART_CMD                   0xb0
#define ATA_CHECK_POWER_MODE            0xe5
// 48-bit commands
#define ATA_READ_LOG_EXT                0x2F

// ATA Specification Feature Register Values (SMART Subcommands).
// Note that some are obsolete as of ATA-7.
#define ATA_SMART_READ_VALUES           0xd0
#define ATA_SMART_READ_THRESHOLDS       0xd1
#define ATA_SMART_AUTOSAVE              0xd2
#define ATA_SMART_SAVE                  0xd3
#define ATA_SMART_IMMEDIATE_OFFLINE     0xd4
#define ATA_SMART_READ_LOG_SECTOR       0xd5
#define ATA_SMART_WRITE_LOG_SECTOR      0xd6
#define ATA_SMART_WRITE_THRESHOLDS      0xd7
#define ATA_SMART_ENABLE                0xd8
#define ATA_SMART_DISABLE               0xd9
#define ATA_SMART_STATUS                0xda
// SFF 8035i Revision 2 Specification Feature Register Value (SMART
// Subcommand)
#define ATA_SMART_AUTO_OFFLINE          0xdb

// Sector Number values for ATA_SMART_IMMEDIATE_OFFLINE Subcommand
#define OFFLINE_FULL_SCAN               0
#define SHORT_SELF_TEST                 1
#define EXTEND_SELF_TEST                2
#define CONVEYANCE_SELF_TEST            3
#define SELECTIVE_SELF_TEST             4
#define ABORT_SELF_TEST                 127
#define SHORT_CAPTIVE_SELF_TEST         129
#define EXTEND_CAPTIVE_SELF_TEST        130
#define CONVEYANCE_CAPTIVE_SELF_TEST    131
#define SELECTIVE_CAPTIVE_SELF_TEST     132
#define CAPTIVE_MASK                    (0x01<<7)

// Maximum allowed number of SMART Attributes
#define NUMBER_ATA_SMART_ATTRIBUTES     30

// Needed parts of the ATA DRIVE IDENTIFY Structure. Those labeled
// word* are NOT used.
#pragma pack(1)
struct ata_identify_device {
  unsigned short words000_009[10];
  unsigned char  serial_no[20];
  unsigned short words020_022[3];
  unsigned char  fw_rev[8];
  unsigned char  model[40];
  unsigned short words047_079[33];
  unsigned short major_rev_num;
  unsigned short minor_rev_num;
  unsigned short command_set_1;
  unsigned short command_set_2;
  unsigned short command_set_extension;
  unsigned short cfs_enable_1;
  unsigned short word086;
  unsigned short csf_default;
  unsigned short words088_255[168];
} ATTR_PACKED;
#pragma pack()
ASSERT_SIZEOF_STRUCT(ata_identify_device, 512);

/* ata_smart_attribute is the vendor specific in SFF-8035 spec */ 
#pragma pack(1)
struct ata_smart_attribute {
  unsigned char id;
  // meaning of flag bits: see MACROS just below
  // WARNING: MISALIGNED!
  unsigned short flags; 
  unsigned char current;
  unsigned char worst;
  unsigned char raw[6];
  unsigned char reserv;
} ATTR_PACKED;
#pragma pack()
ASSERT_SIZEOF_STRUCT(ata_smart_attribute, 12);

// MACROS to interpret the flags bits in the previous structure.
// These have not been implemented using bitflags and a union, to make
// it portable across bit/little endian and different platforms.

// 0: Prefailure bit

// From SFF 8035i Revision 2 page 19: Bit 0 (pre-failure/advisory bit)
// - If the value of this bit equals zero, an attribute value less
// than or equal to its corresponding attribute threshold indicates an
// advisory condition where the usage or age of the device has
// exceeded its intended design life period. If the value of this bit
// equals one, an attribute value less than or equal to its
// corresponding attribute threshold indicates a prefailure condition
// where imminent loss of data is being predicted.
#define ATTRIBUTE_FLAGS_PREFAILURE(x) (x & 0x01)

// 1: Online bit 

//  From SFF 8035i Revision 2 page 19: Bit 1 (on-line data collection
// bit) - If the value of this bit equals zero, then the attribute
// value is updated only during off-line data collection
// activities. If the value of this bit equals one, then the attribute
// value is updated during normal operation of the device or during
// both normal operation and off-line testing.
#define ATTRIBUTE_FLAGS_ONLINE(x) (x & 0x02)


// The following are (probably) IBM's, Maxtors and  Quantum's definitions for the
// vendor-specific bits:
// 2: Performance type bit
#define ATTRIBUTE_FLAGS_PERFORMANCE(x) (x & 0x04)

// 3: Errorrate type bit
#define ATTRIBUTE_FLAGS_ERRORRATE(x) (x & 0x08)

// 4: Eventcount bit
#define ATTRIBUTE_FLAGS_EVENTCOUNT(x) (x & 0x10)

// 5: Selfpereserving bit
#define ATTRIBUTE_FLAGS_SELFPRESERVING(x) (x & 0x20)


// Last ten bits are reserved for future use

/* ata_smart_values is format of the read drive Attribute command */
/* see Table 34 of T13/1321D Rev 1 spec (Device SMART data structure) for *some* info */
#pragma pack(1)
struct ata_smart_values {
  unsigned short int revnumber;
  struct ata_smart_attribute vendor_attributes [NUMBER_ATA_SMART_ATTRIBUTES];
  unsigned char offline_data_collection_status;
  unsigned char self_test_exec_status;  //IBM # segments for offline collection
  unsigned short int total_time_to_complete_off_line; // IBM different
  unsigned char vendor_specific_366; // Maxtor & IBM curent segment pointer
  unsigned char offline_data_collection_capability;
  unsigned short int smart_capability;
  unsigned char errorlog_capability;
  unsigned char vendor_specific_371;  // Maxtor, IBM: self-test failure checkpoint see below!
  unsigned char short_test_completion_time;
  unsigned char extend_test_completion_time;
  unsigned char conveyance_test_completion_time;
  unsigned char reserved_375_385[11];
  unsigned char vendor_specific_386_510[125]; // Maxtor bytes 508-509 Attribute/Threshold Revision #
  unsigned char chksum;
} ATTR_PACKED;
#pragma pack()
ASSERT_SIZEOF_STRUCT(ata_smart_values, 512);

/* Maxtor, IBM: self-test failure checkpoint byte meaning:
 00 - write test
 01 - servo basic
 02 - servo random
 03 - G-list scan
 04 - Handling damage
 05 - Read scan
*/

/* Vendor attribute of SMART Threshold (compare to ata_smart_attribute above) */
#pragma pack(1)
struct ata_smart_threshold_entry {
  unsigned char id;
  unsigned char threshold;
  unsigned char reserved[10];
} ATTR_PACKED;
#pragma pack()
ASSERT_SIZEOF_STRUCT(ata_smart_threshold_entry, 12);

/* Format of Read SMART THreshold Command */
/* Compare to ata_smart_values above */
#pragma pack(1)
struct ata_smart_thresholds_pvt {
  unsigned short int revnumber;
  struct ata_smart_threshold_entry thres_entries[NUMBER_ATA_SMART_ATTRIBUTES];
  unsigned char reserved[149];
  unsigned char chksum;
} ATTR_PACKED;
#pragma pack()
ASSERT_SIZEOF_STRUCT(ata_smart_thresholds_pvt, 512);


// Table 42 of T13/1321D Rev 1 spec (Error Data Structure)
#pragma pack(1)
struct ata_smart_errorlog_error_struct {
  unsigned char reserved;
  unsigned char error_register;
  unsigned char sector_count;
  unsigned char sector_number;
  unsigned char cylinder_low;
  unsigned char cylinder_high;
  unsigned char drive_head;
  unsigned char status;
  unsigned char extended_error[19];
  unsigned char state;
  unsigned short timestamp;
} ATTR_PACKED;
#pragma pack()
ASSERT_SIZEOF_STRUCT(ata_smart_errorlog_error_struct, 30);


// Table 41 of T13/1321D Rev 1 spec (Command Data Structure)
#pragma pack(1)
struct ata_smart_errorlog_command_struct {
  unsigned char devicecontrolreg;
  unsigned char featuresreg;
  unsigned char sector_count;
  unsigned char sector_number;
  unsigned char cylinder_low;
  unsigned char cylinder_high;
  unsigned char drive_head;
  unsigned char commandreg;
  unsigned int timestamp;
} ATTR_PACKED;
#pragma pack()
ASSERT_SIZEOF_STRUCT(ata_smart_errorlog_command_struct, 12);

// Table 40 of T13/1321D Rev 1 spec (Error log data structure)
#pragma pack(1)
struct ata_smart_errorlog_struct {
  struct ata_smart_errorlog_command_struct commands[5];
  struct ata_smart_errorlog_error_struct error_struct;
} ATTR_PACKED;
#pragma pack()
ASSERT_SIZEOF_STRUCT(ata_smart_errorlog_struct, 90);

// Table 39 of T13/1321D Rev 1 spec (SMART error log sector)
#pragma pack(1)
struct ata_smart_errorlog {
  unsigned char revnumber;
  unsigned char error_log_pointer;
  struct ata_smart_errorlog_struct errorlog_struct[5];
  unsigned short int ata_error_count;
  unsigned char reserved[57];
  unsigned char checksum;
} ATTR_PACKED;
#pragma pack()
ASSERT_SIZEOF_STRUCT(ata_smart_errorlog, 512);


// Extended Comprehensive SMART Error Log data structures
// See Section A.7 of
//   AT Attachment 8 - ATA/ATAPI Command Set (ATA8-ACS)
//   T13/1699-D Revision 6a (Working Draft), September 6, 2008.

// Command data structure
// Table A.9 of T13/1699-D Revision 6a
#pragma pack(1)
struct ata_smart_exterrlog_command
{
  unsigned char device_control_register;
  unsigned char features_register;
  unsigned char features_register_hi;
  unsigned char count_register;
  unsigned char count_register_hi;
  unsigned char lba_low_register;
  unsigned char lba_low_register_hi;
  unsigned char lba_mid_register;
  unsigned char lba_mid_register_hi;
  unsigned char lba_high_register;
  unsigned char lba_high_register_hi;
  unsigned char device_register;
  unsigned char command_register;

  unsigned char reserved;
  unsigned int timestamp;
} ATTR_PACKED;
#pragma pack()
ASSERT_SIZEOF_STRUCT(ata_smart_exterrlog_command, 18);

// Error data structure
// Table A.10 T13/1699-D Revision 6a
#pragma pack(1)
struct ata_smart_exterrlog_error
{
  unsigned char device_control_register;
  unsigned char error_register;
  unsigned char count_register;
  unsigned char count_register_hi;
  unsigned char lba_low_register;
  unsigned char lba_low_register_hi;
  unsigned char lba_mid_register;
  unsigned char lba_mid_register_hi;
  unsigned char lba_high_register;
  unsigned char lba_high_register_hi;
  unsigned char device_register;
  unsigned char status_register;

  unsigned char extended_error[19];
  unsigned char state;
  unsigned short timestamp;
} ATTR_PACKED;
#pragma pack()
ASSERT_SIZEOF_STRUCT(ata_smart_exterrlog_error, 34);

// Error log data structure
// Table A.8 of T13/1699-D Revision 6a
#pragma pack(1)
struct ata_smart_exterrlog_error_log
{
  ata_smart_exterrlog_command commands[5];
  ata_smart_exterrlog_error error;
} ATTR_PACKED;
#pragma pack()
ASSERT_SIZEOF_STRUCT(ata_smart_exterrlog_error_log, 124);

// Ext. Comprehensive SMART error log
// Table A.7 of T13/1699-D Revision 6a
#pragma pack(1)
struct ata_smart_exterrlog
{
  unsigned char version;
  unsigned char reserved1;
  unsigned short error_log_index;
  ata_smart_exterrlog_error_log error_logs[4];
  unsigned short device_error_count;
  unsigned char reserved2[9];
  unsigned char checksum;
} ATTR_PACKED;
#pragma pack()
ASSERT_SIZEOF_STRUCT(ata_smart_exterrlog, 512);


// Table 45 of T13/1321D Rev 1 spec (Self-test log descriptor entry)
#pragma pack(1)
struct ata_smart_selftestlog_struct {
  unsigned char selftestnumber; // Sector number register
  unsigned char selfteststatus;
  unsigned short int timestamp;
  unsigned char selftestfailurecheckpoint;
  unsigned int lbafirstfailure;
  unsigned char vendorspecific[15];
} ATTR_PACKED;
#pragma pack()
ASSERT_SIZEOF_STRUCT(ata_smart_selftestlog_struct, 24);

// Table 44 of T13/1321D Rev 1 spec (Self-test log data structure)
#pragma pack(1)
struct ata_smart_selftestlog {
  unsigned short int revnumber;
  struct ata_smart_selftestlog_struct selftest_struct[21];
  unsigned char vendorspecific[2];
  unsigned char mostrecenttest;
  unsigned char reserved[2];
  unsigned char chksum;
} ATTR_PACKED;
#pragma pack()
ASSERT_SIZEOF_STRUCT(ata_smart_selftestlog, 512);

// Extended SMART Self-test log data structures
// See Section A.8 of
//   AT Attachment 8 - ATA/ATAPI Command Set (ATA8-ACS)
//   T13/1699-D Revision 6a (Working Draft), September 6, 2008.

// Extended Self-test log descriptor entry
// Table A.13 of T13/1699-D Revision 6a
#pragma pack(1)
struct ata_smart_extselftestlog_desc
{
  unsigned char self_test_type;
  unsigned char self_test_status;
  unsigned short timestamp;
  unsigned char checkpoint;
  unsigned char failing_lba[6];
  unsigned char vendorspecific[15];
} ATTR_PACKED;
#pragma pack()
ASSERT_SIZEOF_STRUCT(ata_smart_extselftestlog_desc, 26);

// Extended Self-test log data structure
// Table A.12 of T13/1699-D Revision 6a
#pragma pack(1)
struct ata_smart_extselftestlog
{
  unsigned char version;
  unsigned char reserved1;
  unsigned short log_desc_index;
  struct ata_smart_extselftestlog_desc log_descs[19];
  unsigned char vendor_specifc[2];
  unsigned char reserved2[11];
  unsigned char chksum;
} ATTR_PACKED;
#pragma pack()
ASSERT_SIZEOF_STRUCT(ata_smart_extselftestlog, 512);

// SMART LOG DIRECTORY Table 52 of T13/1532D Vol 1 Rev 1a
#pragma pack(1)
struct ata_smart_log_entry {
  unsigned char numsectors;
  unsigned char reserved;
} ATTR_PACKED;
#pragma pack()
ASSERT_SIZEOF_STRUCT(ata_smart_log_entry, 2);

#pragma pack(1)
struct ata_smart_log_directory {
  unsigned short int logversion;
  struct ata_smart_log_entry entry[255];
} ATTR_PACKED;
#pragma pack()
ASSERT_SIZEOF_STRUCT(ata_smart_log_directory, 512);

// SMART SELECTIVE SELF-TEST LOG Table 61 of T13/1532D Volume 1
// Revision 3
#pragma pack(1)
struct test_span {
  uint64_t start;
  uint64_t end;
} ATTR_PACKED;
#pragma pack()
ASSERT_SIZEOF_STRUCT(test_span, 16);

#pragma pack(1)
struct ata_selective_self_test_log {
  unsigned short     logversion;
  struct test_span   span[5];
  unsigned char      reserved1[337-82+1];
  unsigned char      vendor_specific1[491-338+1];
  uint64_t           currentlba;
  unsigned short     currentspan;
  unsigned short     flags;
  unsigned char      vendor_specific2[507-504+1];
  unsigned short     pendingtime;
  unsigned char      reserved2;
  unsigned char      checksum;
} ATTR_PACKED;
#pragma pack()
ASSERT_SIZEOF_STRUCT(ata_selective_self_test_log, 512);

#define SELECTIVE_FLAG_DOSCAN  (0x0002)
#define SELECTIVE_FLAG_PENDING (0x0008)
#define SELECTIVE_FLAG_ACTIVE  (0x0010)


// SCT (SMART Command Transport) data structures
// See Sections 8.2 and 8.3 of:
//   AT Attachment 8 - ATA/ATAPI Command Set (ATA8-ACS)
//   T13/1699-D Revision 3f (Working Draft), December 11, 2006.

// SCT Status response (read with SMART_READ_LOG page 0xe0)
// Table 60 of T13/1699-D Revision 3f 
#pragma pack(1)
struct ata_sct_status_response
{
  unsigned short format_version;    // 0-1: Status response format version number (2, 3)
  unsigned short sct_version;       // 2-3: Vendor specific version number
  unsigned short sct_spec;          // 4-5: SCT level supported (1)
  unsigned int status_flags;        // 6-9: Status flags (Bit 0: Segment initialized, Bits 1-31: reserved)
  unsigned char device_state;       // 10: Device State (0-5)
  unsigned char bytes011_013[3];    // 11-13: reserved
  unsigned short ext_status_code;   // 14-15: Status of last SCT command (0xffff if executing)
  unsigned short action_code;       // 16-17: Action code of last SCT command
  unsigned short function_code;     // 18-19: Function code of last SCT command
  unsigned char bytes020_039[20];   // 20-39: reserved
  uint64_t lba_current;             // 40-47: LBA of SCT command executing in background
  unsigned char bytes048_199[152];  // 48-199: reserved
  signed char hda_temp;             // 200: Current temperature in Celsius (0x80 = invalid)
  signed char min_temp;             // 201: Minimum temperature this power cycle
  signed char max_temp;             // 202: Maximum temperature this power cycle
  signed char life_min_temp;        // 203: Minimum lifetime temperature
  signed char life_max_temp;        // 204: Maximum lifetime temperature
  unsigned char byte205;            // 205: reserved (T13/e06152r0-2: Average lifetime temperature)
  unsigned int over_limit_count;    // 206-209: # intervals since last reset with temperature > Max Op Limit
  unsigned int under_limit_count;   // 210-213: # intervals since last reset with temperature < Min Op Limit
  unsigned char bytes214_479[266];  // 214-479: reserved
  unsigned char vendor_specific[32];// 480-511: vendor specific
} ATTR_PACKED;
#pragma pack()
ASSERT_SIZEOF_STRUCT(ata_sct_status_response, 512);

// SCT Error Recovery Control command (send with SMART_WRITE_LOG page 0xe0)
// Table 88 of T13/1699-D Revision 6a
#pragma pack(1)
struct ata_sct_error_recovery_control_command
{
  unsigned short action_code;       // 3 = Error Recovery Control
  unsigned short function_code;     // 1 = Set, 2 = Return
  unsigned short selection_code;    // 1 = Read Timer, 2 = Write Timer
  unsigned short time_limit;        // If set: Recovery time limit in 100ms units
  unsigned short words004_255[252]; // reserved
} ATTR_PACKED;
#pragma pack()
ASSERT_SIZEOF_STRUCT(ata_sct_error_recovery_control_command, 512);

// SCT Feature Control command (send with SMART_WRITE_LOG page 0xe0)
// Table 72 of T13/1699-D Revision 3f
#pragma pack(1)
struct ata_sct_feature_control_command
{
  unsigned short action_code;       // 4 = Feature Control
  unsigned short function_code;     // 1 = Set, 2 = Return, 3 = Return options
  unsigned short feature_code;      // 3 = Temperature logging interval
  unsigned short state;             // Interval
  unsigned short option_flags;      // Bit 0: persistent, Bits 1-15: reserved
  unsigned short words005_255[251]; // reserved
} ATTR_PACKED;
#pragma pack()
ASSERT_SIZEOF_STRUCT(ata_sct_feature_control_command, 512);

// SCT Data Table command (send with SMART_WRITE_LOG page 0xe0)
// Table 73 of T13/1699-D Revision 3f 
#pragma pack(1)
struct ata_sct_data_table_command
{
  unsigned short action_code;       // 5 = Data Table
  unsigned short function_code;     // 1 = Read Table
  unsigned short table_id;          // 2 = Temperature History
  unsigned short words003_255[253]; // reserved
} ATTR_PACKED;
#pragma pack()
ASSERT_SIZEOF_STRUCT(ata_sct_data_table_command, 512);

// SCT Temperature History Table (read with SMART_READ_LOG page 0xe1)
// Table 75 of T13/1699-D Revision 3f 
#pragma pack(1)
struct ata_sct_temperature_history_table
{
  unsigned short format_version;    // 0-1: Data table format version number (2)
  unsigned short sampling_period;   // 2-3: Temperature sampling period in minutes
  unsigned short interval;          // 4-5: Timer interval between history entries
  signed char max_op_limit;         // 6: Maximum recommended continuous operating temperature
  signed char over_limit;           // 7: Maximum temperature limit
  signed char min_op_limit;         // 8: Minimum recommended continuous operating limit
  signed char under_limit;          // 9: Minimum temperature limit
  unsigned char bytes010_029[20];   // 10-29: reserved
  unsigned short cb_size;           // 30-31: Number of history entries (range 128-478)
  unsigned short cb_index;          // 32-33: Index of last updated entry (zero-based)
  signed char cb[478];              // 34-(34+cb_size-1): Circular buffer of temperature values
} ATTR_PACKED;
#pragma pack()
ASSERT_SIZEOF_STRUCT(ata_sct_temperature_history_table, 512);

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
  RAWFMT_RAW64,
  RAWFMT_HEX64,
  RAWFMT_RAW16_OPT_RAW16,
  RAWFMT_RAW16_OPT_AVG16,
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
  ATTRFLAG_INCREASING = 0x01,   // Value not reset (for reallocated/pending counts)
  ATTRFLAG_NO_NORMVAL = 0x02,   // Normalized value not valid
  ATTRFLAG_NO_WORSTVAL = 0x04   // Worst value not valid
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


// Get information from drive
int ataReadHDIdentity(ata_device * device, struct ata_identify_device *buf);
int ataCheckPowerMode(ata_device * device);

/* Read S.M.A.R.T information from drive */
int ataReadSmartValues(ata_device * device,struct ata_smart_values *);
int ataReadSmartThresholds(ata_device * device, struct ata_smart_thresholds_pvt *);
int ataReadErrorLog (ata_device * device, ata_smart_errorlog *data,
                     unsigned char fix_firmwarebug);
int ataReadSelfTestLog(ata_device * device, ata_smart_selftestlog * data,
                       unsigned char fix_firmwarebug);
int ataReadSelectiveSelfTestLog(ata_device * device, struct ata_selective_self_test_log *data);
int ataSetSmartThresholds(ata_device * device, struct ata_smart_thresholds_pvt *);
int ataReadLogDirectory(ata_device * device, ata_smart_log_directory *, bool gpl);

// Read GP Log page(s)
bool ataReadLogExt(ata_device * device, unsigned char logaddr,
                   unsigned char features, unsigned page,
                   void * data, unsigned nsectors);
// Read SMART Log page(s)
bool ataReadSmartLog(ata_device * device, unsigned char logaddr,
                     void * data, unsigned nsectors);
// Read SMART Extended Comprehensive Error Log
bool ataReadExtErrorLog(ata_device * device, ata_smart_exterrlog * log,
                        unsigned nsectors);
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
int ataGetSCTErrorRecoveryControltime(ata_device * device, unsigned type, unsigned short & time_limit);
int ataSetSCTErrorRecoveryControltime(ata_device * device, unsigned type, unsigned short time_limit);


/* Enable/Disable SMART on device */
int ataEnableSmart (ata_device * device);
int ataDisableSmart (ata_device * device);
int ataEnableAutoSave(ata_device * device);
int ataDisableAutoSave(ata_device * device);

/* Automatic Offline Testing */
int ataEnableAutoOffline (ata_device * device);
int ataDisableAutoOffline (ata_device * device);

/* S.M.A.R.T. test commands */
int ataSmartOfflineTest (ata_device * device);
int ataSmartExtendSelfTest (ata_device * device);
int ataSmartShortSelfTest (ata_device * device);
int ataSmartShortCapSelfTest (ata_device * device);
int ataSmartExtendCapSelfTest (ata_device * device);
int ataSmartSelfTestAbort (ata_device * device);
int ataWriteSelectiveSelfTestLog(ata_device * device, ata_selective_selftest_args & args,
                                 const ata_smart_values * sv, uint64_t num_sectors);

// Returns the latest compatibility of ATA/ATAPI Version the device
// supports. Returns -1 if Version command is not supported
int ataVersionInfo(const char ** description, const ata_identify_device * drive, unsigned short * minor);

// If SMART supported, this is guaranteed to return 1 if SMART is enabled, else 0.
int ataDoesSmartWork(ata_device * device);

// returns 1 if SMART supported, 0 if not supported or can't tell
int ataSmartSupport(const ata_identify_device * drive);

// Return values:
//  1: SMART enabled
//  0: SMART disabled
// -1: can't tell if SMART is enabled -- try issuing ataDoesSmartWork command to see
int ataIsSmartEnabled(const ata_identify_device * drive);

int ataSmartStatus2(ata_device * device);

int isSmartErrorLogCapable(const ata_smart_values * data, const ata_identify_device * identity);

int isSmartTestLogCapable(const ata_smart_values * data, const ata_identify_device * identity);

int isGeneralPurposeLoggingCapable(const ata_identify_device * identity);

int isSupportExecuteOfflineImmediate(const ata_smart_values * data);

int isSupportAutomaticTimer(const ata_smart_values * data);

int isSupportOfflineAbort(const ata_smart_values * data);

int isSupportOfflineSurfaceScan(const ata_smart_values * data);

int isSupportSelfTest(const ata_smart_values * data);

int isSupportConveyanceSelfTest(const ata_smart_values * data);

int isSupportSelectiveSelfTest(const ata_smart_values * data);

inline bool isSCTCapable(const ata_identify_device *drive)
  { return !!(drive->words088_255[206-88] & 0x01); } // 0x01 = SCT support

inline bool isSCTErrorRecoveryControlCapable(const ata_identify_device *drive)
  { return ((drive->words088_255[206-88] & 0x09) == 0x09); } // 0x08 = SCT Error Recovery Control support

inline bool isSCTFeatureControlCapable(const ata_identify_device *drive)
  { return ((drive->words088_255[206-88] & 0x11) == 0x11); } // 0x10 = SCT Feature Control support

inline bool isSCTDataTableCapable(const ata_identify_device *drive)
  { return ((drive->words088_255[206-88] & 0x21) == 0x21); } // 0x20 = SCT Data Table support

int ataSmartTest(ata_device * device, int testtype, const ata_selective_selftest_args & args,
                 const ata_smart_values * sv, uint64_t num_sectors);

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
                                    const ata_vendor_attr_defs & defs);

// External handler function, for when a checksum is not correct.  Can
// simply return if no action is desired, or can print error messages
// as needed, or exit.  Is passed a string with the name of the Data
// Structure with the incorrect checksum.
void checksumwarning(const char *string);

// Find attribute index for attribute id, -1 if not found.
int ata_find_attr_index(unsigned char id, const ata_smart_values & smartval);

// Return Temperature Attribute raw value selected according to possible
// non-default interpretations. If the Attribute does not exist, return 0
unsigned char ata_return_temperature_value(const ata_smart_values * data, const ata_vendor_attr_defs & defs);


// This are the meanings of the Self-test failure checkpoint byte.
// This is in the self-test log at offset 4 bytes into the self-test
// descriptor and in the SMART READ DATA structure at byte offset
// 371. These codes are not well documented.  The meanings returned by
// this routine are used (at least) by Maxtor and IBM. Returns NULL if
// not recognized.
const char *SelfTestFailureCodeName(unsigned char which);


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


// These are two of the functions that are defined in os_*.c and need
// to be ported to get smartmontools onto another OS.
// Moved to C++ interface
//int ata_command_interface(int device, smart_command_set command, int select, char *data);
//int escalade_command_interface(int fd, int escalade_port, int escalade_type, smart_command_set command, int select, char *data);
//int marvell_command_interface(int device, smart_command_set command, int select, char *data);
//int highpoint_command_interface(int device, smart_command_set command, int select, char *data);
//int areca_command_interface(int fd, int disknum, smart_command_set command, int select, char *data);


// Optional functions of os_*.c
#ifdef HAVE_ATA_IDENTIFY_IS_CACHED
// Return true if OS caches the ATA identify sector
//int ata_identify_is_cached(int fd);
#endif

// This function is exported to give low-level capability
int smartcommandhandler(ata_device * device, smart_command_set command, int select, char *data);

// Print one self-test log entry.
bool ataPrintSmartSelfTestEntry(unsigned testnum, unsigned char test_type,
                                unsigned char test_status,
                                unsigned short timestamp,
                                uint64_t failing_lba,
                                bool print_error_only, bool & print_header);

// Print Smart self-test log, used by smartctl and smartd.
int ataPrintSmartSelfTestlog(const ata_smart_selftestlog * data, bool allentries,
                             unsigned char fix_firmwarebug);

// Get number of sectors from IDENTIFY sector.
uint64_t get_num_sectors(const ata_identify_device * drive);

// Convenience function for formatting strings from ata_identify_device.
void format_ata_string(char * out, const char * in, int n, bool fix_swap);
inline void format_ata_string(char * out, const unsigned char * in, int n, bool fix_swap)
  { format_ata_string(out, (const char *)in, n, fix_swap); }

// Utility routines.
unsigned char checksum(const void * data);

void swap2(char *location);
void swap4(char *location);
void swap8(char *location);
// Typesafe variants using overloading
inline void swapx(unsigned short * p)
  { swap2((char*)p); }
inline void swapx(unsigned int * p)
  { swap4((char*)p); }
inline void swapx(uint64_t * p)
  { swap8((char*)p); }

// Return pseudo-device to parse "smartctl -r ataioctl,2 ..." output
// and simulate an ATA device with same behaviour
ata_device * get_parsed_ata_device(smart_interface * intf, const char * dev_name);

#endif /* ATACMDS_H_ */
