/*
 * ata.h - ATA constants and types
 *
 * Home page of code is: https://www.smartmontools.org
 *
 * Copyright (C) 1999-2000 Michael Cornwell <cornwell@acm.org>
 * Copyright (C) 2002-2011 Bruce Allen
 * Copyright (C) 2008-2026 Christian Franke
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef SMARTMON_ATA_H
#define SMARTMON_ATA_H

#include <smartmon/byteorder.h>

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

// LBA_MID/HIGH (CYLINDER_LOW/HIGH, LBA 23:8) register values for ATA_SMART_CMD.
enum {
  // Required as input for each ATA_SMART_CMD.
  // Also returned by SMART_STATUS to indicate a "good" status.
  ATA_SMART_CMD_LBA_MID               = 0x4f,
  ATA_SMART_CMD_LBA_HIGH              = 0xc2,
  // Returned by SMART_STATUS to indicate a "failed" status.
  ATA_SMART_FAILED_LBA_MID            = 0xf4,
  ATA_SMART_FAILED_LBA_HIGH           = 0x2c
};

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

// IDENTIFY DEVICE data
// Table 57 of T13/BSR INCITS 574 (ACS-6) Revision 13, April 17, 2025
struct ata_identify_device {
  uint16_t  general_config;           // 0: general configuration
  uint16_t  obsolete_001;
  uint16_t  specific_config;          // 2: specific configuration
  uint16_t  obsolete_003_006[4];
  uint16_t  reserved_007_008_cfa[2];
  uint16_t  obsolete_009;
  uint8_t   serial_no[20];            // 10-19: serial number
  uint16_t  obsolete_020_022[3];
  uint8_t   fw_rev[8];                // 23-26: firmware revision
  uint8_t   model[40];                // 27-46: model "number"
  uint16_t  rd_wr_multi_support;      // 47: READ/WRITE MULTIPLE support [OBS-ACS-4]
  uint16_t  tc_feature_set_options;   // 48: trusted computing feature set options
  uint16_t  capabilities_1;           // 49: capabilities
  uint16_t  capabilities_2;           // 50: capabilities
  uint16_t  obsolete_051_052[2];
  uint16_t  field_validity;           // 53: field validity / free-fall control
  uint16_t  obsolete_054_058[5];
  uint16_t  sanitize_rd_wr_multi;     // 59: sanitize device - READ/WRITE MULTIPLE support
  uint32_t  user_sectors_28;          // 60-61: user addressable logical sectors for 28-bit commands
  uint16_t  obsolete_062;
  uint16_t  dma_multi_modes;          // 63: multiword DMA modes
  uint16_t  pio_modes;                // 64: PIO modes
  uint16_t  dma_multi_cycle_min_ns;   // 65: minimum multiword DMA cycle time per word in ns
  uint16_t  dma_multi_cycle_rec_ns;   // 66: recommended multiword DMA cycle time in ns
  uint16_t  pio_cycle_no_fl_min_ns;   // 67: Minimum PIO cycle time without flow control in ns
  uint16_t  pio_cycle_iordy_min_ns;   // 68: Minimum PIO cycle time with IORDY flow control in ns
  uint16_t  additional_support;       // 69: additional support
  uint16_t  reserved_070;
  uint16_t  reserved_071_074_atapi[4];
  uint16_t  queue_depth;              // 75: queue depth
  uint16_t  sata_capabilities_1;      // 76: Serial ATA capabilities
  uint16_t  sata_capabilities_2;      // 77: Serial ATA additional capabilities
  uint16_t  sata_features_supported;  // 78: Serial ATA features supported
  uint16_t  sata_features_enabled;    // 79: Serial ATA features enabled
  uint16_t  major_rev_num;            // 80: major version number
  uint16_t  minor_rev_num;            // 81: minor version number
  uint16_t  command_set_1;            // 82: commands and feature sets supported
  uint16_t  command_set_2;            // 83: commands and feature sets supported
  uint16_t  command_set_3;            // 84: commands and feature sets supported
  uint16_t  cfs_enabled_1;            // 85: commands and feature sets supported or enabled
  uint16_t  cfs_enabled_2;            // 86: commands and feature sets supported or enabled
  uint16_t  cfs_enabled_3;            // 87: commands and feature sets supported or enabled
  uint16_t  udma_modes;               // 88: Ultra DMA modes
  uint16_t  sec_erase_unit_time;      // 89: SECURITY ERASE UNIT time
  uint16_t  sec_enh_erase_unit_time;  // 90: ENHANCED SECURITY ERASE UNIT tim
  uint16_t  apm_level;                // 91: current APM level
  uint16_t  master_password_id;       // 92: master password identifier
  uint16_t  pata_hw_reset_result;     // 93: hardware reset result (PATA)
  uint16_t  aam_level;                // 94: AAM level [OBS-ACS-2]
  uint16_t  strm_min_req_size;        // 95: stream minimum request size
  uint16_t  strm_trnfr_time_dma;      // 96: streaming transfer time - DMA
  uint16_t  strm_acc_latency;         // 97: streaming access latency - DMA and PIO
  uint32_t  strm_perf_granularity;    // 98-99: streaming performance granularity
  uile64_t  user_sectors_48;          // 100-103: user addressable logical sectors for 48-bit commands
  uint16_t  strm_trnfr_time_pio;      // 104: streaming transfer time - PIO
  uint16_t  ds_mgmt_range_max_blks;   // 105: max blocks of LBA range entries per DS MANAGEMENT cmd
  uint16_t  phy_log_sector_size;      // 106: physical sector size / logical sector size
  uint16_t  iso7779_seek_delay;       // 107: inter-seek delay for ISO 7779 acoustic testing
  uint16_t  wwn[4];                   // 108-111: world wide name
  uint16_t  reserved_112_115[4];
  uint16_t  reserved_116_tlc;
  uile32_t  log_sector_size;          // 117-118: logical sector_size
  uint16_t  command_set_4;            // 119: commands and feature sets supported
  uint16_t  cfs_enabled_4;            // 120: commands and feature sets supported or enabled
  uint16_t  reserved_121_124[4];
  uint16_t  reserved_125_126_atapi[2];
  uint16_t  rm_media_status;          // 127: removable media status notification [OBS-8]
  uint16_t  security_status;          // 128: security status
  uint16_t  vendor_129_159[31];       // 129-159: vendor specific
  uint16_t  cfa_power_mode;           // 160: CFA power mode
  uint16_t  reserved_161_167_cfa[7];
  uint16_t  form_factor;              // 168: nominal form factor
  uint16_t  dataset_management;       // 169: DATA SET MANAGEMENT command support
  uint8_t   add_product_id[8];        // 170-173: additional product identifier
  uint16_t  reserved_174_175[2];
  uint8_t   media_serial_no[60];      // 176-205: current media serial number
  uint16_t  sct_capabilities;         // 206: SCT command transport capabilities
  uint16_t  reserved_207_208[2];
  uint16_t  log_sector_align;         // 209: alignment of logical sectors
  uint32_t  wr_rd_vr_count_mode_3;    // 210-211 write-read-verify sector count mode 3
  uint32_t  wr_rd_vr_count_mode_2;    // 212-213 write-read-verify sector count mode 2
  uint16_t  nv_cache_capabilities;    // 214: non volatile cache capabilities [OBS-ACS-3]
  uile32_t  nv_cache_size;            // 215-216: non volatile size in logical blocks [OBS-ACS-3]
  uint16_t  rotation_rate;            // 217: nominal rotation rate
  uint16_t  reserved_218;
  uint16_t  nv_cache_options;         // 219: non volatile cache options [OBS-ACS-3]
  uint16_t  write_read_verify_mode;   // 220: write-read-verify mode
  uint16_t  reserved_221;
  uint16_t  transport_maj_version;    // 222: transport major version number
  uint16_t  transport_min_version;    // 223: transport minor version number
  uint16_t  reserved_224_229[6];
  uile64_t  user_sectors_ext;         // 230-233: extended number of user addressable logical sectors
  uint16_t  dl_mcode_3_min_blocks;    // 234: minimum blocks per DOWNLOAD MICROCODE mode 3 command
  uint16_t  dl_mcode_3_max_blocks;    // 235: maximum blocks per DOWNLOAD MICROCODE mode 3 command
  uint16_t  reserved_236_254[19];
  uint8_t   signature;                // 255 LSB: 0xa5 if checksum is set
  uint8_t   checksum;                 // 255 MSB: checksum
};
SMARTMON_ASSERT_SIZEOF(ata_identify_device, 512);

/* ata_smart_attribute is the vendor specific in SFF-8035 spec */
struct ata_smart_attribute {
  uint8_t   id;
  // meaning of flag bits: see MACROS just below
  uile16_t  flags;
  uint8_t   current;
  uint8_t   worst;
  uint8_t   raw[6];
  uint8_t   reserv;
};
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
struct ata_smart_values {
  uint16_t  revnumber;
  ata_smart_attribute vendor_attributes[NUMBER_ATA_SMART_ATTRIBUTES];
  uint8_t   offline_data_collection_status;
  uint8_t   self_test_exec_status;  //IBM # segments for offline collection
  uint16_t  total_time_to_complete_off_line; // IBM different
  uint8_t   vendor_specific_366; // Maxtor & IBM current segment pointer
  uint8_t   offline_data_collection_capability;
  uint16_t  smart_capability;
  uint8_t   errorlog_capability;
  uint8_t   vendor_specific_371;  // Maxtor, IBM: self-test failure checkpoint see below!
  uint8_t   short_test_completion_time;
  uint8_t   extend_test_completion_time_b; // If 0xff, use 16-bit value below
  uint8_t   conveyance_test_completion_time;
  uile16_t  extend_test_completion_time_w; // e04130r2, added to T13/1699-D Revision 1c, April 2005
  uint8_t   reserved_377_385[9];
  uint8_t   vendor_specific_386_510[125]; // Maxtor bytes 508-509 Attribute/Threshold Revision #
  uint8_t   chksum;
};
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
struct ata_smart_threshold_entry {
  uint8_t   id;
  uint8_t   threshold;
  uint8_t   reserved[10];
};
SMARTMON_ASSERT_SIZEOF(ata_smart_threshold_entry, 12);

/* Format of Read SMART THreshold Command */
/* Compare to ata_smart_values above */
struct ata_smart_thresholds_pvt {
  uint16_t  revnumber;
  ata_smart_threshold_entry thres_entries[NUMBER_ATA_SMART_ATTRIBUTES];
  uint8_t   reserved[149];
  uint8_t   chksum;
};
SMARTMON_ASSERT_SIZEOF(ata_smart_thresholds_pvt, 512);

// Table 42 of T13/1321D Rev 1 spec (Error Data Structure)
struct ata_smart_errorlog_error_struct {
  uint8_t   reserved;
  uint8_t   error_register;
  uint8_t   sector_count;
  uint8_t   sector_number;
  uint8_t   cylinder_low;
  uint8_t   cylinder_high;
  uint8_t   drive_head;
  uint8_t   status;
  uint8_t   extended_error[19];
  uint8_t   state;
  uint16_t  timestamp;
};
SMARTMON_ASSERT_SIZEOF(ata_smart_errorlog_error_struct, 30);

// Table 41 of T13/1321D Rev 1 spec (Command Data Structure)
struct ata_smart_errorlog_command_struct {
  uint8_t   devicecontrolreg;
  uint8_t   featuresreg;
  uint8_t   sector_count;
  uint8_t   sector_number;
  uint8_t   cylinder_low;
  uint8_t   cylinder_high;
  uint8_t   drive_head;
  uint8_t   commandreg;
  uile32_t  timestamp;
};
SMARTMON_ASSERT_SIZEOF(ata_smart_errorlog_command_struct, 12);

// Table 40 of T13/1321D Rev 1 spec (Error log data structure)
struct ata_smart_errorlog_struct {
  ata_smart_errorlog_command_struct commands[5];
  ata_smart_errorlog_error_struct error_struct;
};
SMARTMON_ASSERT_SIZEOF(ata_smart_errorlog_struct, 90);

// Table 39 of T13/1321D Rev 1 spec (SMART error log sector)
struct ata_smart_errorlog {
  uint8_t   revnumber;
  uint8_t   error_log_pointer;
  ata_smart_errorlog_struct errorlog_struct[5];
  uint16_t  ata_error_count;
  uint8_t   reserved[57];
  uint8_t   checksum;
};
SMARTMON_ASSERT_SIZEOF(ata_smart_errorlog, 512);

// Extended Comprehensive SMART Error Log data structures
// See Section A.7 of
//   AT Attachment 8 - ATA/ATAPI Command Set (ATA8-ACS)
//   T13/1699-D Revision 6a (Working Draft), September 6, 2008.

// Command data structure
// Table A.9 of T13/1699-D Revision 6a
struct ata_smart_exterrlog_command
{
  uint8_t   device_control_register;
  uint8_t   features_register;
  uint8_t   features_register_hi;
  uint8_t   count_register;
  uint8_t   count_register_hi;
  uint8_t   lba_low_register;
  uint8_t   lba_low_register_hi;
  uint8_t   lba_mid_register;
  uint8_t   lba_mid_register_hi;
  uint8_t   lba_high_register;
  uint8_t   lba_high_register_hi;
  uint8_t   device_register;
  uint8_t   command_register;

  uint8_t   reserved;
  uile32_t  timestamp;
};
SMARTMON_ASSERT_SIZEOF(ata_smart_exterrlog_command, 18);

// Error data structure
// Table A.10 T13/1699-D Revision 6a
struct ata_smart_exterrlog_error
{
  uint8_t   device_control_register;
  uint8_t   error_register;
  uint8_t   count_register;
  uint8_t   count_register_hi;
  uint8_t   lba_low_register;
  uint8_t   lba_low_register_hi;
  uint8_t   lba_mid_register;
  uint8_t   lba_mid_register_hi;
  uint8_t   lba_high_register;
  uint8_t   lba_high_register_hi;
  uint8_t   device_register;
  uint8_t   status_register;
  uint8_t   extended_error[19];
  uint8_t   state;
  uint16_t  timestamp;
};
SMARTMON_ASSERT_SIZEOF(ata_smart_exterrlog_error, 34);

// Error log data structure
// Table A.8 of T13/1699-D Revision 6a
struct ata_smart_exterrlog_error_log
{
  ata_smart_exterrlog_command commands[5];
  ata_smart_exterrlog_error error;
};
SMARTMON_ASSERT_SIZEOF(ata_smart_exterrlog_error_log, 124);

// Ext. Comprehensive SMART error log
// Table A.7 of T13/1699-D Revision 6a
struct ata_smart_exterrlog
{
  uint8_t   version;
  uint8_t   reserved1;
  uint16_t  error_log_index;
  ata_smart_exterrlog_error_log error_logs[4];
  uint16_t  device_error_count;
  uint8_t   reserved2[9];
  uint8_t   checksum;
};
SMARTMON_ASSERT_SIZEOF(ata_smart_exterrlog, 512);

// Table 45 of T13/1321D Rev 1 spec (Self-test log descriptor entry)
struct ata_smart_selftestlog_struct {
  uint8_t   selftestnumber;
  uint8_t   selfteststatus;
  uint16_t  timestamp;
  uint8_t   selftestfailurecheckpoint;
  uile32_t  lbafirstfailure;
  uint8_t   vendorspecific[15];
};
SMARTMON_ASSERT_SIZEOF(ata_smart_selftestlog_struct, 24);

// Table 44 of T13/1321D Rev 1 spec (Self-test log data structure)
struct ata_smart_selftestlog {
  uint16_t  revnumber;
  ata_smart_selftestlog_struct selftest_struct[21];
  uint8_t   vendorspecific[2];
  uint8_t   mostrecenttest;
  uint8_t   reserved[2];
  uint8_t   chksum;
};
SMARTMON_ASSERT_SIZEOF(ata_smart_selftestlog, 512);

// Extended SMART Self-test log data structures
// See Section A.8 of
//   AT Attachment 8 - ATA/ATAPI Command Set (ATA8-ACS)
//   T13/1699-D Revision 6a (Working Draft), September 6, 2008.

// Extended Self-test log descriptor entry
// Table A.13 of T13/1699-D Revision 6a
struct ata_smart_extselftestlog_desc
{
  uint8_t   self_test_type;
  uint8_t   self_test_status;
  uint16_t  timestamp;
  uint8_t   checkpoint;
  uint8_t   failing_lba[6];
  uint8_t   vendorspecific[15];
};
SMARTMON_ASSERT_SIZEOF(ata_smart_extselftestlog_desc, 26);

// Extended Self-test log data structure
// Table A.12 of T13/1699-D Revision 6a
struct ata_smart_extselftestlog
{
  uint8_t   version;
  uint8_t   reserved1;
  uint16_t  log_desc_index;
  ata_smart_extselftestlog_desc log_descs[19];
  uint8_t   vendor_specifc[2];
  uint8_t   reserved2[11];
  uint8_t   chksum;
};
SMARTMON_ASSERT_SIZEOF(ata_smart_extselftestlog, 512);

// SMART LOG DIRECTORY Table 52 of T13/1532D Vol 1 Rev 1a
struct ata_smart_log_entry {
  uint8_t   numsectors;
  uint8_t   reserved;
};
SMARTMON_ASSERT_SIZEOF(ata_smart_log_entry, 2);

struct ata_smart_log_directory {
  uint16_t  logversion;
  ata_smart_log_entry entry[255];
};
SMARTMON_ASSERT_SIZEOF(ata_smart_log_directory, 512);

// SMART SELECTIVE SELF-TEST LOG Table 61 of T13/1532D Volume 1
// Revision 3
struct test_span {
  uile64_t  start;
  uile64_t  end;
};
SMARTMON_ASSERT_SIZEOF(test_span, 16);

struct ata_selective_self_test_log {
  uint16_t  logversion;
  test_span span[5];
  uint8_t   reserved1[337-82+1];
  uint8_t   vendor_specific1[491-338+1];
  uile64_t  currentlba;
  uint16_t  currentspan;
  uint16_t  flags;
  uint8_t   vendor_specific2[507-504+1];
  uint16_t  pendingtime;
  uint8_t   reserved2;
  uint8_t   checksum;
};
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
struct ata_sct_status_response
{
  uint16_t  format_version;     // 0-1: Status response format version number (2, 3)
  uint16_t  sct_version;        // 2-3: Vendor specific version number
  uint16_t  sct_spec;           // 4-5: SCT level supported (1)
  uile32_t  status_flags;       // 6-9: Status flags (Bit 0: Segment initialized, Bits 1-31: reserved)
  uint8_t   device_state;       // 10: Device State (0-5)
  uint8_t   bytes011_013[3];    // 11-13: reserved
  uint16_t  ext_status_code;    // 14-15: Status of last SCT command (0xffff if executing)
  uint16_t  action_code;        // 16-17: Action code of last SCT command
  uint16_t  function_code;      // 18-19: Function code of last SCT command
  uint8_t   bytes020_039[20];   // 20-39: reserved
  uint64_t  lba_current;        // 40-47: LBA of SCT command executing in background
  uint8_t   bytes048_199[152];  // 48-199: reserved
  int8_t    hda_temp;           // 200: Current temperature in Celsius (0x80 = invalid)
  int8_t    min_temp;           // 201: Minimum temperature this power cycle
  int8_t    max_temp;           // 202: Maximum temperature this power cycle
  int8_t    life_min_temp;      // 203: Minimum lifetime temperature
  int8_t    life_max_temp;      // 204: Maximum lifetime temperature
  int8_t    max_op_limit;       // 205: Specified maximum operating temperature (ACS-4)
  uile32_t  over_limit_count;   // 206-209: # intervals since last reset with temperature > Max Op Limit
  uile32_t  under_limit_count;  // 210-213: # intervals since last reset with temperature < Min Op Limit
  uint16_t  smart_status;       // 214-215: LBA(32:8) of SMART RETURN STATUS (0, 0x2cf4, 0xc24f) (ACS-4)
  uint16_t  min_erc_time;       // 216-217: Minimum supported value for ERC (ACS-4)
  uint8_t   bytes216_479[479-218+1]; // 218-479: reserved
  uint8_t   vendor_specific[32]; // 480-511: vendor specific
};
SMARTMON_ASSERT_SIZEOF(ata_sct_status_response, 512);

// SCT Error Recovery Control command (send with SMART_WRITE_LOG page 0xe0)
// Table 88 of T13/1699-D Revision 6a
struct ata_sct_error_recovery_control_command
{
  uint16_t  action_code;        // 3 = Error Recovery Control
  uint16_t  function_code;      // 1 = Set Current, 2 = Return Current, 3 = Set Power-on, 4 = Return Power-on, 5 = Restore Default
  uint16_t  selection_code;     // 1 = Read Timer, 2 = Write Timer
  uint16_t  time_limit;         // If set: Recovery time limit in 100ms units
  uint16_t  words004_255[252];  // reserved
};
SMARTMON_ASSERT_SIZEOF(ata_sct_error_recovery_control_command, 512);

// SCT Feature Control command (send with SMART_WRITE_LOG page 0xe0)
// Table 72 of T13/1699-D Revision 3f
struct ata_sct_feature_control_command
{
  uint16_t  action_code;        // 4 = Feature Control
  uint16_t  function_code;      // 1 = Set, 2 = Return, 3 = Return options
  uint16_t  feature_code;       // 3 = Temperature logging interval
  uint16_t  state;              // Interval
  uint16_t  option_flags;       // Bit 0: persistent, Bits 1-15: reserved
  uint16_t  words005_255[251];  // reserved
};
SMARTMON_ASSERT_SIZEOF(ata_sct_feature_control_command, 512);

// SCT Data Table command (send with SMART_WRITE_LOG page 0xe0)
// Table 73 of T13/1699-D Revision 3f
struct ata_sct_data_table_command
{
  uint16_t  action_code;        // 5 = Data Table
  uint16_t  function_code;      // 1 = Read Table
  uint16_t  table_id;           // 2 = Temperature History
  uint16_t  words003_255[253];  // reserved
};
SMARTMON_ASSERT_SIZEOF(ata_sct_data_table_command, 512);

// SCT Temperature History Table (read with SMART_READ_LOG page 0xe1)
// Table 75 of T13/1699-D Revision 3f
struct ata_sct_temperature_history_table
{
  uint16_t  format_version;     // 0-1: Data table format version number (2)
  uint16_t  sampling_period;    // 2-3: Temperature sampling period in minutes
  uint16_t  interval;           // 4-5: Timer interval between history entries
  int8_t    max_op_limit;       // 6: Maximum recommended continuous operating temperature
  int8_t    over_limit;         // 7: Maximum temperature limit
  int8_t    min_op_limit;       // 8: Minimum recommended continuous operating limit
  int8_t    under_limit;        // 9: Minimum temperature limit
  uint8_t   bytes010_029[20];   // 10-29: reserved
  uint16_t  cb_size;            // 30-31: Number of history entries (range 128-478)
  uint16_t  cb_index;           // 32-33: Index of last updated entry (zero-based)
  int8_t    cb[478];            // 34-(34+cb_size-1): Circular buffer of temperature values
};
SMARTMON_ASSERT_SIZEOF(ata_sct_temperature_history_table, 512);

} // namespace smartmon

#endif // SMARTMON_ATA_H
