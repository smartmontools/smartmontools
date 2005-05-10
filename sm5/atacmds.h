/*
 * atacmds.h
 *
 * Home page of code is: http://smartmontools.sourceforge.net
 *
 * Copyright (C) 2002-5 Bruce Allen <smartmontools-support@lists.sourceforge.net>
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

#define ATACMDS_H_CVSID "$Id: atacmds.h,v 1.80 2005/05/10 19:15:47 chrfranke Exp $\n"

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

// ATA Specification Command Register Values (Commands)
#define ATA_IDENTIFY_DEVICE             0xec                                              
#define ATA_IDENTIFY_PACKET_DEVICE      0xa1
#define ATA_SMART_CMD                   0xb0
#define ATA_CHECK_POWER_MODE            0xe5

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

// Get information from drive
int ataReadHDIdentity(int device, struct ata_identify_device *buf);
int ataCheckPowerMode(int device);

/* Read S.M.A.R.T information from drive */
int ataReadSmartValues(int device,struct ata_smart_values *);
int ataReadSmartThresholds(int device, struct ata_smart_thresholds_pvt *);
int ataReadErrorLog(int device, struct ata_smart_errorlog *);
int ataReadSelfTestLog(int device, struct ata_smart_selftestlog *);
int ataReadSelectiveSelfTestLog(int device, struct ata_selective_self_test_log *data);
int ataSmartStatus(int device);
int ataSetSmartThresholds(int device, struct ata_smart_thresholds_pvt *);
int ataReadLogDirectory(int device, struct ata_smart_log_directory *);  

/* Enable/Disable SMART on device */
int ataEnableSmart ( int device );
int ataDisableSmart (int device );
int ataEnableAutoSave(int device);
int ataDisableAutoSave(int device);

/* Automatic Offline Testing */
int ataEnableAutoOffline ( int device );
int ataDisableAutoOffline (int device );

/* S.M.A.R.T. test commands */
int ataSmartOfflineTest (int device);
int ataSmartExtendSelfTest (int device);
int ataSmartShortSelfTest (int device);
int ataSmartShortCapSelfTest (int device);
int ataSmartExtendCapSelfTest (int device);
int ataSmartSelfTestAbort (int device);

// Returns the latest compatibility of ATA/ATAPI Version the device
// supports. Returns -1 if Version command is not supported
int ataVersionInfo (const char **description, struct ata_identify_device *drive, unsigned short *minor);

// If SMART supported, this is guaranteed to return 1 if SMART is enabled, else 0.
int ataDoesSmartWork(int device);

// returns 1 if SMART supported, 0 if not supported or can't tell
int ataSmartSupport ( struct ata_identify_device *drive);

// Return values:
//  1: SMART enabled
//  0: SMART disabled
// -1: can't tell if SMART is enabled -- try issuing ataDoesSmartWork command to see
int ataIsSmartEnabled(struct ata_identify_device *drive);

/* Check SMART for Threshold failure */
// onlyfailed=0 : are or were any age or prefailure attributes <= threshold
// onlyfailed=1:  are any prefailure attributes <= threshold now
int ataCheckSmart ( struct ata_smart_values *data, struct ata_smart_thresholds_pvt *thresholds, int onlyfailed);

int ataSmartStatus2(int device);

// int isOfflineTestTime ( struct ata_smart_values data)
//  returns S.M.A.R.T. Offline Test Time in seconds
int isOfflineTestTime ( struct ata_smart_values *data);

int isShortSelfTestTime ( struct ata_smart_values *data);

int isExtendedSelfTestTime ( struct ata_smart_values *data);

int isSmartErrorLogCapable(struct ata_smart_values *data, struct ata_identify_device *identity);

int isSmartTestLogCapable(struct ata_smart_values *data, struct ata_identify_device *identity);

int isGeneralPurposeLoggingCapable(struct ata_identify_device *identity);

int isSupportExecuteOfflineImmediate ( struct ata_smart_values *data);

int isSupportAutomaticTimer ( struct ata_smart_values *data);

int isSupportOfflineAbort ( struct ata_smart_values *data);

int isSupportOfflineSurfaceScan ( struct ata_smart_values *data);

int isSupportSelfTest (struct ata_smart_values *data);

int isSupportConveyanceSelfTest(struct ata_smart_values *data);

int isSupportSelectiveSelfTest(struct ata_smart_values *data);

int ataSmartTest(int device, int testtype, struct ata_smart_values *data);

int TestTime(struct ata_smart_values *data,int testtype);

// Prints the raw value (with appropriate formatting) into the
// character string out.
int64_t ataPrintSmartAttribRawValue(char *out, 
                                    struct ata_smart_attribute *attribute,
                                    unsigned char *defs);

// Prints Attribute Name for standard SMART attributes. Writes a
// 30 byte string with attribute name into output
void ataPrintSmartAttribName(char *output, unsigned char id, unsigned char *definitions);

// This checks the n'th attribute in the attribute list, NOT the
// attribute with id==n.  If the attribute does not exist, or the
// attribute is > threshold, then returns zero.  If the attribute is
// <= threshold (failing) then we the attribute number if it is a
// prefail attribute.  Else we return minus the attribute number if it
// is a usage attribute.
int ataCheckAttribute(struct ata_smart_values *data,
                      struct ata_smart_thresholds_pvt *thresholds,
                      int n);

// External handler function, for when a checksum is not correct.  Can
// simply return if no action is desired, or can print error messages
// as needed, or exit.  Is passed a string with the name of the Data
// Structure with the incorrect checksum.
void checksumwarning(const char *string);

// Returns raw value of Attribute with ID==id. This will be in the
// range 0 to 2^48-1 inclusive.  If the Attribute does not exist,
// return -1.
int64_t ATAReturnAttributeRawValue(unsigned char id, struct ata_smart_values *data);


// This are the meanings of the Self-test failure checkpoint byte.
// This is in the self-test log at offset 4 bytes into the self-test
// descriptor and in the SMART READ DATA structure at byte offset
// 371. These codes are not well documented.  The meanings returned by
// this routine are used (at least) by Maxtor and IBM. Returns NULL if
// not recognized.
const char *SelfTestFailureCodeName(unsigned char which);


#define MAX_ATTRIBUTE_NUM 256

extern const char *vendorattributeargs[];

// function to parse pairs like "9,minutes" or "220,temp".  See end of
// extern.h for definition of defs[].  Returns 0 if pair recognized,
// else 1 if there is a problem.  Allocates memory for array if the
// array address is *defs==NULL.
int parse_attribute_def(char *pair, unsigned char **defs);

// Function to return a string containing a list of the arguments in
// vendorattributeargs[].  Returns NULL if the required memory can't
// be allocated.
char *create_vendor_attribute_arg_list(void);


// These are two of the functions that are defined in os_*.c and need
// to be ported to get smartmontools onto another OS.
int ata_command_interface(int device, smart_command_set command, int select, char *data);
int escalade_command_interface(int fd, int escalade_port, int escalade_type, smart_command_set command, int select, char *data);
int marvell_command_interface(int device, smart_command_set command, int select, char *data);
// Optional functions of os_*.c
#ifdef HAVE_ATA_IDENTIFY_IS_CACHED
// Return true if OS caches the ATA identify sector
int ata_identify_is_cached(int fd);
#endif

// This function is exported to give low-level capability
int smartcommandhandler(int device, smart_command_set command, int select, char *data);

// Utility routines.
void swap2(char *location);
void swap4(char *location);
void swap8(char *location);
#endif /* ATACMDS_H_ */
