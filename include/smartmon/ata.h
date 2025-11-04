/*
 * ata.h - ATA constants and types
 *
 * Home page of code is: https://www.smartmontools.org
 *
 * Copyright (C) 1999-2000 Michael Cornwell <cornwell@acm.org>
 * Copyright (C) 2002-2011 Bruce Allen
 * Copyright (C) 2008-2025 Christian Franke
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef SMARTMON_ATA_H
#define SMARTMON_ATA_H

#include <smartmon/smartmon_defs.h>

#include <stdint.h>

namespace smartmon {

// ATA Specification Command Register Values (Commands)
#define ATA_CHECK_POWER_MODE            0xe5
#define ATA_IDENTIFY_DEVICE             0xec
#define ATA_IDENTIFY_PACKET_DEVICE      0xa1
#define ATA_IDLE                        0xe3
#define ATA_SMART_CMD                   0xb0
#define ATA_SECURITY_FREEZE_LOCK        0xf5
#ifndef ATA_SET_FEATURES
#define ATA_SET_FEATURES                0xef
#endif
#define ATA_STANDBY                     0xe2
#define ATA_STANDBY_IMMEDIATE           0xe0

// SET_FEATURES subcommands
#define ATA_DISABLE_AAM                 0xc2
#define ATA_DISABLE_APM                 0x85
#define ATA_DISABLE_WRITE_CACHE         0x82
#define ATA_DISABLE_READ_LOOK_AHEAD     0x55
#define ATA_ENABLE_AAM                  0x42
#define ATA_ENABLE_APM                  0x05
#define ATA_ENABLE_WRITE_CACHE          0x02
#define ATA_ENABLE_READ_LOOK_AHEAD      0xaa
#define ATA_ENABLE_DISABLE_DSN          0x63

// 48-bit commands
#define ATA_READ_LOG_EXT                0x2F
#define ATA_WRITE_LOG_EXT               0x3f

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
} SMARTMON_ATTR_PACKED;
#pragma pack()
SMARTMON_ASSERT_SIZEOF(ata_identify_device, 512);

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
} SMARTMON_ATTR_PACKED;
#pragma pack()
SMARTMON_ASSERT_SIZEOF(ata_smart_attribute, 12);

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

// 6-15: Reserved for future use
#define ATTRIBUTE_FLAGS_OTHER(x) ((x) & 0xffc0)

// Format of data returned by SMART READ DATA
// Table 62 of T13/1699-D (ATA8-ACS) Revision 6a, September 2008
#pragma pack(1)
struct ata_smart_values {
  unsigned short int revnumber;
  struct ata_smart_attribute vendor_attributes [NUMBER_ATA_SMART_ATTRIBUTES];
  unsigned char offline_data_collection_status;
  unsigned char self_test_exec_status;  //IBM # segments for offline collection
  unsigned short int total_time_to_complete_off_line; // IBM different
  unsigned char vendor_specific_366; // Maxtor & IBM current segment pointer
  unsigned char offline_data_collection_capability;
  unsigned short int smart_capability;
  unsigned char errorlog_capability;
  unsigned char vendor_specific_371;  // Maxtor, IBM: self-test failure checkpoint see below!
  unsigned char short_test_completion_time;
  unsigned char extend_test_completion_time_b; // If 0xff, use 16-bit value below
  unsigned char conveyance_test_completion_time;
  unsigned short extend_test_completion_time_w; // e04130r2, added to T13/1699-D Revision 1c, April 2005
  unsigned char reserved_377_385[9];
  unsigned char vendor_specific_386_510[125]; // Maxtor bytes 508-509 Attribute/Threshold Revision #
  unsigned char chksum;
} SMARTMON_ATTR_PACKED;
#pragma pack()
SMARTMON_ASSERT_SIZEOF(ata_smart_values, 512);

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
} SMARTMON_ATTR_PACKED;
#pragma pack()
SMARTMON_ASSERT_SIZEOF(ata_smart_threshold_entry, 12);

/* Format of Read SMART THreshold Command */
/* Compare to ata_smart_values above */
#pragma pack(1)
struct ata_smart_thresholds_pvt {
  unsigned short int revnumber;
  struct ata_smart_threshold_entry thres_entries[NUMBER_ATA_SMART_ATTRIBUTES];
  unsigned char reserved[149];
  unsigned char chksum;
} SMARTMON_ATTR_PACKED;
#pragma pack()
SMARTMON_ASSERT_SIZEOF(ata_smart_thresholds_pvt, 512);

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
} SMARTMON_ATTR_PACKED;
#pragma pack()
SMARTMON_ASSERT_SIZEOF(ata_smart_errorlog_error_struct, 30);

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
} SMARTMON_ATTR_PACKED;
#pragma pack()
SMARTMON_ASSERT_SIZEOF(ata_smart_errorlog_command_struct, 12);

// Table 40 of T13/1321D Rev 1 spec (Error log data structure)
#pragma pack(1)
struct ata_smart_errorlog_struct {
  struct ata_smart_errorlog_command_struct commands[5];
  struct ata_smart_errorlog_error_struct error_struct;
} SMARTMON_ATTR_PACKED;
#pragma pack()
SMARTMON_ASSERT_SIZEOF(ata_smart_errorlog_struct, 90);

// Table 39 of T13/1321D Rev 1 spec (SMART error log sector)
#pragma pack(1)
struct ata_smart_errorlog {
  unsigned char revnumber;
  unsigned char error_log_pointer;
  struct ata_smart_errorlog_struct errorlog_struct[5];
  unsigned short int ata_error_count;
  unsigned char reserved[57];
  unsigned char checksum;
} SMARTMON_ATTR_PACKED;
#pragma pack()
SMARTMON_ASSERT_SIZEOF(ata_smart_errorlog, 512);

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
} SMARTMON_ATTR_PACKED;
#pragma pack()
SMARTMON_ASSERT_SIZEOF(ata_smart_exterrlog_command, 18);

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
} SMARTMON_ATTR_PACKED;
#pragma pack()
SMARTMON_ASSERT_SIZEOF(ata_smart_exterrlog_error, 34);

// Error log data structure
// Table A.8 of T13/1699-D Revision 6a
#pragma pack(1)
struct ata_smart_exterrlog_error_log
{
  ata_smart_exterrlog_command commands[5];
  ata_smart_exterrlog_error error;
} SMARTMON_ATTR_PACKED;
#pragma pack()
SMARTMON_ASSERT_SIZEOF(ata_smart_exterrlog_error_log, 124);

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
} SMARTMON_ATTR_PACKED;
#pragma pack()
SMARTMON_ASSERT_SIZEOF(ata_smart_exterrlog, 512);

// Table 45 of T13/1321D Rev 1 spec (Self-test log descriptor entry)
#pragma pack(1)
struct ata_smart_selftestlog_struct {
  unsigned char selftestnumber; // Sector number register
  unsigned char selfteststatus;
  unsigned short int timestamp;
  unsigned char selftestfailurecheckpoint;
  unsigned int lbafirstfailure;
  unsigned char vendorspecific[15];
} SMARTMON_ATTR_PACKED;
#pragma pack()
SMARTMON_ASSERT_SIZEOF(ata_smart_selftestlog_struct, 24);

// Table 44 of T13/1321D Rev 1 spec (Self-test log data structure)
#pragma pack(1)
struct ata_smart_selftestlog {
  unsigned short int revnumber;
  struct ata_smart_selftestlog_struct selftest_struct[21];
  unsigned char vendorspecific[2];
  unsigned char mostrecenttest;
  unsigned char reserved[2];
  unsigned char chksum;
} SMARTMON_ATTR_PACKED;
#pragma pack()
SMARTMON_ASSERT_SIZEOF(ata_smart_selftestlog, 512);

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
} SMARTMON_ATTR_PACKED;
#pragma pack()
SMARTMON_ASSERT_SIZEOF(ata_smart_extselftestlog_desc, 26);

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
} SMARTMON_ATTR_PACKED;
#pragma pack()
SMARTMON_ASSERT_SIZEOF(ata_smart_extselftestlog, 512);

// SMART LOG DIRECTORY Table 52 of T13/1532D Vol 1 Rev 1a
#pragma pack(1)
struct ata_smart_log_entry {
  unsigned char numsectors;
  unsigned char reserved;
} SMARTMON_ATTR_PACKED;
#pragma pack()
SMARTMON_ASSERT_SIZEOF(ata_smart_log_entry, 2);

#pragma pack(1)
struct ata_smart_log_directory {
  unsigned short int logversion;
  struct ata_smart_log_entry entry[255];
} SMARTMON_ATTR_PACKED;
#pragma pack()
SMARTMON_ASSERT_SIZEOF(ata_smart_log_directory, 512);

// SMART SELECTIVE SELF-TEST LOG Table 61 of T13/1532D Volume 1
// Revision 3
#pragma pack(1)
struct test_span {
  uint64_t start;
  uint64_t end;
} SMARTMON_ATTR_PACKED;
#pragma pack()
SMARTMON_ASSERT_SIZEOF(test_span, 16);

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
} SMARTMON_ATTR_PACKED;
#pragma pack()
SMARTMON_ASSERT_SIZEOF(ata_selective_self_test_log, 512);

#define SELECTIVE_FLAG_DOSCAN  (0x0002)
#define SELECTIVE_FLAG_PENDING (0x0008)
#define SELECTIVE_FLAG_ACTIVE  (0x0010)

// SCT (SMART Command Transport) data structures
// See Sections 8.2 and 8.3 of:
//   AT Attachment 8 - ATA/ATAPI Command Set (ATA8-ACS)
//   T13/1699-D Revision 3f (Working Draft), December 11, 2006.

// SCT Status response (read with SMART_READ_LOG page 0xe0)
// Table 194 of T13/BSR INCITS 529 (ACS-4) Revision 20, October 26, 2017
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
  signed char max_op_limit;         // 205: Specified maximum operating temperature (ACS-4)
  unsigned int over_limit_count;    // 206-209: # intervals since last reset with temperature > Max Op Limit
  unsigned int under_limit_count;   // 210-213: # intervals since last reset with temperature < Min Op Limit
  unsigned short smart_status;      // 214-215: LBA(32:8) of SMART RETURN STATUS (0, 0x2cf4, 0xc24f) (ACS-4)
  unsigned short min_erc_time;      // 216-217: Minimum supported value for ERC (ACS-4)
  unsigned char bytes216_479[479-218+1]; // 218-479: reserved
  unsigned char vendor_specific[32];// 480-511: vendor specific
} SMARTMON_ATTR_PACKED;
#pragma pack()
SMARTMON_ASSERT_SIZEOF(ata_sct_status_response, 512);

// SCT Error Recovery Control command (send with SMART_WRITE_LOG page 0xe0)
// Table 88 of T13/1699-D Revision 6a
#pragma pack(1)
struct ata_sct_error_recovery_control_command
{
  unsigned short action_code;       // 3 = Error Recovery Control
  unsigned short function_code;     // 1 = Set Current, 2 = Return Current, 3 = Set Power-on, 4 = Return Power-on, 5 = Restore Default
  unsigned short selection_code;    // 1 = Read Timer, 2 = Write Timer
  unsigned short time_limit;        // If set: Recovery time limit in 100ms units
  unsigned short words004_255[252]; // reserved
} SMARTMON_ATTR_PACKED;
#pragma pack()
SMARTMON_ASSERT_SIZEOF(ata_sct_error_recovery_control_command, 512);

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
} SMARTMON_ATTR_PACKED;
#pragma pack()
SMARTMON_ASSERT_SIZEOF(ata_sct_feature_control_command, 512);

// SCT Data Table command (send with SMART_WRITE_LOG page 0xe0)
// Table 73 of T13/1699-D Revision 3f
#pragma pack(1)
struct ata_sct_data_table_command
{
  unsigned short action_code;       // 5 = Data Table
  unsigned short function_code;     // 1 = Read Table
  unsigned short table_id;          // 2 = Temperature History
  unsigned short words003_255[253]; // reserved
} SMARTMON_ATTR_PACKED;
#pragma pack()
SMARTMON_ASSERT_SIZEOF(ata_sct_data_table_command, 512);

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
} SMARTMON_ATTR_PACKED;
#pragma pack()
SMARTMON_ASSERT_SIZEOF(ata_sct_temperature_history_table, 512);

} // namespace smartmon

#endif // SMARTMON_ATA_H
