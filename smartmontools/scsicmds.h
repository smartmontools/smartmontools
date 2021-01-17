/*
 * scsicmds.h
 *
 * Home page of code is: https://www.smartmontools.org
 *
 * Copyright (C) 2002-8 Bruce Allen
 * Copyright (C) 2000 Michael Cornwell <cornwell@acm.org>
 * Copyright (C) 2003-18 Douglas Gilbert <dgilbert@interlog.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 *
 * N.B. What was formerly known as "SMART" are now called "informational
 * exceptions" in recent t10.org drafts (i.e. recent SCSI).
 *
 */


#ifndef SCSICMDS_H_
#define SCSICMDS_H_

#define SCSICMDS_H_CVSID "$Id$\n"

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "static_assert.h"

#define ATTR_PACKED __attribute__((packed))

/* #define SCSI_DEBUG 1 */ /* Comment out to disable command debugging */

/* Following conditional defines just in case OS already has them defined.
 * If they are defined we hope they are defined correctly (for SCSI). */
#ifndef TEST_UNIT_READY
#define TEST_UNIT_READY 0x0
#endif
#ifndef LOG_SELECT
#define LOG_SELECT 0x4c
#endif
#ifndef LOG_SENSE
#define LOG_SENSE 0x4d
#endif
#ifndef MODE_SENSE
#define MODE_SENSE 0x1a
#endif
#ifndef MODE_SENSE_10
#define MODE_SENSE_10 0x5a
#endif
#ifndef MODE_SELECT
#define MODE_SELECT 0x15
#endif
#ifndef MODE_SELECT_10
#define MODE_SELECT_10 0x55
#endif
#ifndef INQUIRY
#define INQUIRY 0x12
#endif
#ifndef REQUEST_SENSE
#define REQUEST_SENSE  0x03
#endif
#ifndef RECEIVE_DIAGNOSTIC
#define RECEIVE_DIAGNOSTIC  0x1c
#endif
#ifndef SEND_DIAGNOSTIC
#define SEND_DIAGNOSTIC  0x1d
#endif
#ifndef READ_DEFECT_10
#define READ_DEFECT_10  0x37
#endif
#ifndef READ_DEFECT_12
#define READ_DEFECT_12  0xb7
#endif
#ifndef START_STOP_UNIT
#define START_STOP_UNIT  0x1b
#endif
#ifndef REPORT_LUNS
#define REPORT_LUNS  0xa0
#endif
#ifndef READ_CAPACITY_10
#define READ_CAPACITY_10  0x25
#endif
#ifndef READ_CAPACITY_16
#define READ_CAPACITY_16  0x9e
#endif
#ifndef SAI_READ_CAPACITY_16    /* service action for READ_CAPACITY_16 */
#define SAI_READ_CAPACITY_16  0x10
#endif

#ifndef SAT_ATA_PASSTHROUGH_12
#define SAT_ATA_PASSTHROUGH_12 0xa1
#endif
#ifndef SAT_ATA_PASSTHROUGH_16
#define SAT_ATA_PASSTHROUGH_16 0x85
#endif


#define DXFER_NONE        0
#define DXFER_FROM_DEVICE 1
#define DXFER_TO_DEVICE   2

struct scsi_cmnd_io
{
    uint8_t * cmnd;     /* [in]: ptr to SCSI command block (cdb) */
    size_t  cmnd_len;   /* [in]: number of bytes in SCSI command */
    int dxfer_dir;      /* [in]: DXFER_NONE, DXFER_FROM_DEVICE, or
                                 DXFER_TO_DEVICE */
    uint8_t * dxferp;   /* [in]: ptr to outgoing or incoming data buffer */
    size_t dxfer_len;   /* [in]: bytes to be transferred to/from dxferp */
    uint8_t * sensep;   /* [in]: ptr to sense buffer, filled when
                                 CHECK CONDITION status occurs */
    size_t max_sense_len; /* [in]: max number of bytes to write to sensep */
    unsigned timeout;   /* [in]: seconds, 0-> default timeout (60 seconds?) */
    size_t resp_sense_len;  /* [out]: sense buffer length written */
    uint8_t scsi_status;  /* [out]: 0->ok, 2->CHECK CONDITION, etc ... */
    int resid;          /* [out]: Number of bytes requested to be transferred
                                  less actual number transferred (0 if not
                                   supported) */
};

struct scsi_sense_disect {
    uint8_t resp_code;
    uint8_t sense_key;
    uint8_t asc;
    uint8_t ascq;
    int progress; /* -1 -> N/A, 0-65535 -> available */
};

/* Useful data from Informational Exception Control mode page (0x1c) */
#define SCSI_IECMP_RAW_LEN 64
struct scsi_iec_mode_page {
    uint8_t requestedCurrent;
    uint8_t gotCurrent;
    uint8_t requestedChangeable;
    uint8_t gotChangeable;
    uint8_t modese_len;   /* 0 (don't know), 6 or 10 */
    uint8_t raw_curr[SCSI_IECMP_RAW_LEN];
    uint8_t raw_chg[SCSI_IECMP_RAW_LEN];
};

/* Carrier for Error counter log pages (e.g. read, write, verify ...) */
struct scsiErrorCounter {
    uint8_t gotPC[7];
    uint8_t gotExtraPC;
    uint64_t counter[8];
};

/* Carrier for Non-medium error log page */
struct scsiNonMediumError {
    uint8_t gotPC0;
    uint8_t gotExtraPC;
    uint64_t counterPC0;
    uint8_t gotTFE_H;
    uint64_t counterTFE_H; /* Track following errors [Hitachi] */
    uint8_t gotPE_H;
    uint64_t counterPE_H;  /* Positioning errors [Hitachi] */
};

struct scsi_readcap_resp {
    uint64_t num_lblocks;       /* Number of Logical Blocks on device */
    uint32_t lb_size;   /* should be available in all non-error cases */
    /* following fields from READ CAPACITY(16) or set to 0 */
    uint8_t prot_type;  /* 0, 1, 2 or 3 protection type, deduced from
                         * READ CAPACITY(16) P_TYPE and PROT_EN fields */
    uint8_t p_i_exp;    /* Protection information Intervals Exponent */
    uint8_t lb_p_pb_exp;/* Logical Blocks per Physical Block Exponent */
    bool lbpme;         /* Logical Block Provisioning Management Enabled */
    bool lbprz;         /* Logical Block Provisioning Read Zeros */
    uint16_t l_a_lba;   /* Lowest Aligned Logical Block Address */
};

// Seagate Field Access Reliability Metrics log (FARM) PAGE Header (read with SCSI LogSense page 0x3D, sub-page 0x3)
// Page Header
#pragma pack(1)
struct scsiFarmPageHeader {
    uint8_t             pageCode;       // Page Code (0x3D)
	uint8_t             subpageCode;    // Sub-Page Code (0x03)
	uint16_t            pageLength;     // Page Length
} ATTR_PACKED;
#pragma pack()
STATIC_ASSERT(sizeof(scsiFarmPageHeader) == 4);

// Seagate Field Access Reliability Metrics log (FARM) PARAMETER Header (read with SCSI LogSense page 0x3D, sub-page 0x3)
// Parameter Header
#pragma pack(1)
struct scsiFarmParameterHeader {
    uint16_t            parameterCode;       // Page Code (0x3D)
	uint8_t             parameterControl;    // Sub-Page Code (0x03)
	uint8_t             parameterLength;     // Page Length
} ATTR_PACKED;
#pragma pack()
STATIC_ASSERT(sizeof(scsiFarmParameterHeader) == 4);

// Seagate Field Access Reliability Metrics log (FARM) parameter (read with SCSI LogSense page 0x3D, sub-page 0x3)
// Log Header
#pragma pack(1)
struct scsiFarmHeader {
    scsiFarmParameterHeader parameterHeader;        // Parameter Header
    uint64_t                signature;              // Log Signature = 0x00004641524D4552
    uint64_t                majorRev;               // Log Major Revision
    uint64_t                minorRev;               // Log Rinor Revision
    uint64_t                parametersSupported;    // Number of Parameters Supported
    uint64_t                logSize;                // Log Page Size in Bytes
    uint64_t                reserved;               // Reserved
    uint64_t                headsSupported;         // Maximum Drive Heads Supported
    uint64_t                reserved0;              // Reserved
    uint64_t                frameCapture;           // Reason for Frame Capture
} ATTR_PACKED;
#pragma pack()
STATIC_ASSERT(sizeof(scsiFarmHeader) == 76);

// Seagate Field Access Reliability Metrics log (FARM) parameter (read with SCSI LogSense page 0x3D, sub-page 0x3)
// Drive Information
#pragma pack(1)
struct scsiFarmDriveInformation {
    scsiFarmParameterHeader parameterHeader;    // Parameter Header
    uint64_t        pageNumber;                 // Page Number = 1
    uint64_t        copyNumber;                 // Copy Number
    uint64_t        serialNumber;               // Serial Number [0:3]
    uint64_t        serialNumber2;              // Serial Number [4:7]
    uint64_t        worldWideName;              // World Wide Name [0:3]
    uint64_t        worldWideName2;             // World Wide Name [4:7]
    uint64_t        deviceInterface;            // Device Interface
    uint64_t        deviceCapacity;             // 48-bit Device Capacity
    uint64_t        psecSize;                   // Physical Sector Size in Bytes
    uint64_t        lsecSize;                   // Logical Sector Size in Bytes
    uint64_t        deviceBufferSize;           // Device Buffer Size in Bytes
    uint64_t        heads;                      // Number of Heads
    uint64_t        factor;                     // Device Form Factor (ID Word 168)
    uint64_t        rotationRate;               // Rotational Rate of Device (ID Word 217)
    uint64_t        firmwareRev;                // Firmware Revision [0:3]
    uint64_t        firmwareRev2;               // Firmware Revision [4:7]
    uint64_t        reserved;                   // Reserved
    uint64_t        reserved0;                  // Reserved
    uint64_t        reserved1;                  // Reserved
    uint64_t        poh;                        // Power-On Hours
    uint64_t        reserved2;                  // Reserved
    uint64_t        reserved3;                  // Reserved
    uint64_t        reserved4;                  // Reserved
    uint64_t        powerCycleCount;            // Power Cycle Count
    uint64_t        resetCount;                 // Hardware Reset Count
    uint64_t        reserved5;                  // Reserved
    uint64_t        reserved6;                  // Reserved
    uint64_t        reserved7;                  // Reserved
    uint64_t        reserved8;                  // Reserved
    uint64_t        reserved9;                  // Reserved
    uint64_t        dateOfAssembly;             // Date of assembly in ASCII “YYWW” where YY is the year and WW is the calendar week (added 4.2)
} ATTR_PACKED;
#pragma pack()
STATIC_ASSERT(sizeof(scsiFarmDriveInformation) == 252);

// Seagate Field Access Reliability Metrics log (FARM) parameter (read with SCSI LogSense page 0x3D, sub-page 0x3)
// Workload Statistics
#pragma pack(1)
struct scsiFarmWorkloadStatistics {
    scsiFarmParameterHeader parameterHeader;  // Parameter Header
	uint64_t        pageNumber;               // Page Number = 2
	uint64_t        copyNumber;               // Copy Number
	uint64_t        reserved;                 // Reserved
	uint64_t        totalReadCommands;        // Total Number of Read Commands
	uint64_t        totalWriteCommands;       // Total Number of Write Commands
	uint64_t        totalRandomReads;         // Total Number of Random Read Commands
	uint64_t        totalRandomWrites;        // Total Number of Random Write Commands
	uint64_t        totalNumberofOtherCMDS;   // Total Number Of Other Commands
	uint64_t        logicalSecWritten;        // Logical Sectors Written
	uint64_t        logicalSecRead;           // Logical Sectors Read
    uint64_t        readCommandsByRadius1;    // Number of Read Commands from 0-3.125% of LBA space for last 3 SMART Summary Frames (added 4.4)
    uint64_t        readCommandsByRadius2;    // Number of Read Commands from 3.125-25% of LBA space for last 3 SMART Summary Frames (added 4.4)
    uint64_t        readCommandsByRadius3;    // Number of Read Commands from 25-75% of LBA space for last 3 SMART Summary Frames (added 4.4)
    uint64_t        readCommandsByRadius4;    // Number of Read Commands from 75-100% of LBA space for last 3 SMART Summary Frames (added 4.4)
    uint64_t        writeCommandsByRadius1;   // Number of Write Commands from 0-3.125% of LBA space for last 3 SMART Summary Frames (added 4.4)
    uint64_t        writeCommandsByRadius2;   // Number of Write Commands from 3.125-25% of LBA space for last 3 SMART Summary Frames (added 4.4)
    uint64_t        writeCommandsByRadius3;   // Number of Write Commands from 25-75% of LBA space for last 3 SMART Summary Frames (added 4.4)
    uint64_t        writeCommandsByRadius4;   // Number of Write Commands from 75-100% of LBA space for last 3 SMART Summary Frames (added 4.4)
} ATTR_PACKED;
#pragma pack()
STATIC_ASSERT(sizeof(scsiFarmWorkloadStatistics) == 148);

// Seagate Field Access Reliability Metrics log (FARM) parameter (read with SCSI LogSense page 0x3D, sub-page 0x3)
// Error Statistics
#pragma pack(1)
struct scsiFarmErrorStatistics {
    scsiFarmParameterHeader parameterHeader;        // Parameter Header
	uint64_t        pageNumber;                     // Page Number = 3
	uint64_t        copyNumber;                     // Copy Number
	uint64_t        totalUnrecoverableReadErrors;   // Number of Unrecoverable Read Errors
	uint64_t        totalUnrecoverableWriteErrors;  // Number of Unrecoverable Write Errors
	uint64_t        reserved;                       // Reserved
	uint64_t        reserved0;                      // Reserved
	uint64_t        totalMechanicalStartRetries;    // Number of Mechanical Start Retries
	uint64_t        reserved1;                      // Reserved
	uint64_t        reserved2;                      // Reserved
	uint64_t        reserved3;                      // Reserved
	uint64_t        reserved4;                      // Reserved
	uint64_t        reserved5;                      // Reserved
	uint64_t        reserved6;                      // Reserved
	uint64_t        reserved7;                      // Reserved
	uint64_t        reserved8;                      // Reserved
	uint64_t        reserved9;                      // Reserved
	uint64_t        reserved10;                     // Reserved
	uint64_t        reserved11;                     // Reserved
	uint64_t        reserved12;                     // Reserved
	uint64_t        reserved13;                     // Reserved
    uint64_t        tripCode;                       // If SMART Trip present the reason code (FRU code)
    uint64_t        invalidDWordCountA;             // Invalid DWord Count (Port A)
    uint64_t        invalidDWordCountB;             // Invalid DWord Count (Port B)                    
    uint64_t        disparityErrorCodeA;            // Disparity Error Count (Port A)        
    uint64_t        disparityErrorCodeB;            // Disparity Error Count (Port A)        
    uint64_t        lossOfDWordSyncA;               // Loss of DWord Sync (Port A)    
    uint64_t        lossOfDWordSyncB;               // Loss of DWord Sync (Port A)    
    uint64_t        phyResetProblemA;               // Phy Reset Problem (Port A)    
    uint64_t        phyResetProblemB;               // Phy Reset Problem (Port A)    
} ATTR_PACKED;
#pragma pack()
STATIC_ASSERT(sizeof(scsiFarmErrorStatistics) == 236);

// Seagate Field Access Reliability Metrics log (FARM) parameter (read with SCSI LogSense page 0x3D, sub-page 0x3)
// Environment Statistics
#pragma pack(1)
struct scsiFarmEnvironmentStatistics {
    scsiFarmParameterHeader parameterHeader;    // Parameter Header
	uint64_t         pageNumber;                // Page Number = 4
	uint64_t         copyNumber;                // Copy Number
	uint64_t         curentTemp;                // Current Temperature in Celsius (Lower 16 bits are a signed integer in units of 0.1C)
	uint64_t         highestTemp;               // Highest Temperature in Celsius (Lower 16 bits are a signed integer in units of 0.1C)
	uint64_t         lowestTemp;                // Lowest Temperature in Celsius (Lower 16 bits are a signed integer in units of 0.1C)  
	uint64_t         reserved;                  // Reserved
	uint64_t         reserved0;                 // Reserved
	uint64_t         reserved1;                 // Reserved
	uint64_t         reserved2;                 // Reserved
	uint64_t         reserved3;                 // Reserved
	uint64_t         reserved4;                 // Reserved
	uint64_t         reserved5;                 // Reserved
	uint64_t         reserved6;                 // Reserved
	uint64_t         maxTemp;                   // Specified Max Operating Temperature in Celsius
	uint64_t         minTemp;                   // Specified Min Operating Temperature in Celsius
	uint64_t         reserved7;                 // Reserved
	uint64_t         reserved8;                 // Reserved
	uint64_t         humidity;                  // Current Relative Humidity (in units of 0.1%)
	uint64_t         reserved9;                 // Reserved
	uint64_t         currentMotorPower;         // Current Motor Power, value from most recent SMART Summary Frame
    uint64_t         powerAverage12v;           // 12V Power Average (mW) - Average of last 3 SMART Summary Frames (added 4.3)
    uint64_t         powerMin12v;               // 12V Power Min (mW) - Lowest of last 3 SMART Summary Frames (added 4.3)
    uint64_t         powerMax12v;               // 12V Power Max (mW) - Highest of last 3 SMART Summary Frames (added 4.3)
    uint64_t         powerAverage5v;            // 5V Power Average (mW) - Average of last 3 SMART Summary Frames (added 4.3)
    uint64_t         powerMin5v;                // 5V Power Min (mW) - Lowest of last 3 SMART Summary Frames (added 4.3)
    uint64_t         powerMax5v;                // 5V Power Max (mW) - Highest of last 3 SMART Summary Frames (added 4.3)
} ATTR_PACKED;
#pragma pack()
STATIC_ASSERT(sizeof(scsiFarmEnvironmentStatistics) == 212);

// Seagate Field Access Reliability Metrics log (FARM) parameter (read with SCSI LogSense page 0x3D, sub-page 0x3)
// Reliability Statistics
#pragma pack(1)
struct scsiFarmReliabilityStatistics {
    scsiFarmParameterHeader parameterHeader;            // Parameter Header
	int64_t         pageNumber;                         // Page Number = 5
	int64_t         copyNumber;                         // Copy Number
	uint64_t        reserved;                           // Reserved
	uint64_t        reserved0;                          // Reserved
	uint64_t        reserved1;                          // Reserved
	uint64_t        reserved2;                          // Reserved
	uint64_t        reserved3;                          // Reserved
	uint64_t        reserved4;                          // Reserved
	uint64_t        reserved5;                          // Reserved
	uint64_t        reserved6;                          // Reserved
	uint64_t        reserved7;                          // Reserved
	uint64_t        reserved8;                          // Reserved
	uint64_t        reserved9;                          // Reserved
	uint64_t        reserved10;                         // Reserved
	uint64_t        reserved11;                         // Reserved
	uint64_t        reserved12;                         // Reserved
	uint64_t        reserved13;                         // Reserved
	uint64_t        reserved14;                         // Reserved
    uint64_t        reserved15;                         // Reserved
	uint64_t        reserved16;                         // Reserved
	uint64_t        reserved17;                         // Reserved
	uint64_t        reserved18;                         // Reserved
	uint64_t        reserved19;                         // Reserved
    uint64_t        reserved20;                         // Reserved
    uint64_t        reserved21;                         // Reserved
	int64_t         heliumPresureTrip;                  // Helium Pressure Threshold Tripped ( 1 - trip, 0 - no trip)
	uint64_t        reserved34;                         // Reserved
	uint64_t        reserved35;                         // Reserved
	uint64_t        reserved36;                         // Reserved
} ATTR_PACKED;
#pragma pack()
STATIC_ASSERT(sizeof(scsiFarmReliabilityStatistics) == 236);

// Seagate Field Access Reliability Metrics log (FARM) parameter (read with SCSI LogSense page 0x3D, sub-page 0x3)
// Drive Information Continued
#pragma pack(1)
struct scsiFarmDriveInformation2 {
    scsiFarmParameterHeader parameterHeader;    // Parameter Header
    uint64_t        pageNumber;                 // Page Number = 6
    uint64_t        copyNumber;                 // Copy Number
    uint64_t        depopulationHeadMask;       // Depopulation Head Mask
    uint64_t        productID;                  // Product ID [0:3]
    uint64_t        productID2;                 // Product ID [4:7]
    uint64_t        productID3;                 // Product ID [8:11]
    uint64_t        productID4;                 // Product ID [12:15]
    uint64_t        driveRecordingType;         // Drive Recording Type - 0 for SMR and 1 for CMR
    uint64_t        dpopped;                    // Is drive currently depopped – 1 = depopped, 0 = not depopped 
    uint64_t        maxNumberForReasign;        // Max Number of Available Sectors for Re-Assignment – Value in disc sectors 
    uint64_t        timeToReady;                // Time to Ready of the last power cycle in milliseconds  
    uint64_t        timeHeld;                   // Time the drive is held in staggered spin in milliseconds 
    uint64_t        lastServoSpinUpTime;        // The last servo spin up time in milliseconds 
} ATTR_PACKED;
#pragma pack()
STATIC_ASSERT(sizeof(scsiFarmDriveInformation2) == 108);

// Seagate Field Access Reliability Metrics log (FARM) parameter (read with SCSI LogSense page 0x3D, sub-page 0x3)
// Environment Statistics Continued
#pragma pack(1)
struct scsiFarmEnvironmentStatistics2 {
    scsiFarmParameterHeader parameterHeader;    // Parameter Header
	uint64_t         pageNumber;                // Page Number = 7
	uint64_t         copyNumber;                // Copy Number
	uint64_t         current12v;                // Current 12V input in mV
    uint64_t         min12v;                    // Minimum 12V input from last 3 SMART Summary Frames in mV
    uint64_t         max12v;                    // Maximum 12V input from last 3 SMART Summary Frames in mV
    uint64_t         current5v;                 // Current 5V input in mV
    uint64_t         min5v;                     // Minimum 5V input from last 3 SMART Summary Frames in mV
    uint64_t         max5v;                     // Maximum 5V input from last 3 SMART Summary Frames in mV
} ATTR_PACKED;
#pragma pack()
STATIC_ASSERT(sizeof(scsiFarmEnvironmentStatistics2) == 68);

// Seagate Field Access Reliability Metrics log (FARM) parameter (read with SCSI LogSense page 0x3D, sub-page 0x3)
// "By Head" Parameters
#pragma pack(1)
struct scsiFarmByHead {
    scsiFarmParameterHeader parameterHeader;    // Parameter Header
	uint64_t                headValue[16];      // [16] Head Information
} ATTR_PACKED;
#pragma pack()
STATIC_ASSERT(sizeof(scsiFarmByHead) == (4 + (16 * 8)));

// Seagate Field Access Reliability Metrics log (FARM) parameter (read with SCSI LogSense page 0x3D, sub-page 0x3)
// "By Actuator" Parameters
#pragma pack(1)
struct scsiFarmByActuator {
    scsiFarmParameterHeader parameterHeader;                        // Parameter Header
	uint64_t                pageNumber;                             // Page Number
	uint64_t                copyNumber;                             // Copy Number  
    uint64_t                actuatorID;                             // Actuator ID 
    uint64_t                headLoadEvents;                         // Head Load Events 
    uint64_t                reserved;                               // Reserved 
    uint64_t                reserved0;                              // Reserved 
    uint64_t                timelastIDDTest;                        // Timestamp of last IDD test 
    uint64_t                subcommandlastIDDTest;                  // Sub-Command of last IDD test 
    uint64_t                numberGListReclam;                      // Number of G-list reclamations 
    uint64_t                servoStatus;                            // Servo Status (follows standard DST error code definitions) 
    uint64_t                numberSlippedSectorsBeforeIDD;          // Number of Slipped Sectors Before IDD Scan 
    uint64_t                numberSlippedSectorsAfterIDD;           // Number of Slipped Sectors After IDD Scan 
    uint64_t                numberResidentReallocatedBeforeIDD;     // Number of Resident Reallocated Sectors Before IDD Scan 
    uint64_t                numberResidentReallocatedAfterIDD;      // Number of Resident Reallocated Sectors After IDD Scan 
    uint64_t                numberScrubbedSectorsBeforeIDD;         // Number of Successfully Scrubbed Sectors Before IDD Scan 
    uint64_t                numberScrubbedSectorsAfterIDD;          // Number of Successfully Scrubbed Sectors After IDD Scan 
    uint64_t                dosScansPerformed;                      // Number of DOS Scans Performed 
    uint64_t                lbasCorrectedISP;                       // Number of LBAs Corrected by ISP 
    uint64_t                numberValidParitySectors;               // Number of Valid Parity Sectors 
    uint64_t                reserved1;                              // Reserved 
    uint64_t                reserved2;                              // Reserved 
    uint64_t                reserved3;                              // Reserved 
    uint64_t                numberLBACorrectedParitySector;         // Number of LBAs Corrected by Parity Sector 
} ATTR_PACKED;
#pragma pack()
STATIC_ASSERT(sizeof(scsiFarmByActuator) == (188));

// Seagate Field Access Reliability Metrics log (FARM) parameter (read with SCSI LogSense page 0x3D, sub-page 0x3)
// "By Actuator" Parameters for Flash LED Information
#pragma pack(1)
struct scsiFarmByActuatorFLED {
    scsiFarmParameterHeader parameterHeader;                        // Parameter Header
	uint64_t                pageNumber;                             // Page Number
	uint64_t                copyNumber;                             // Copy Number  
    uint64_t                actuatorID;                             // Actuator ID 
    uint64_t                totalFlashLED;                          // Total Flash LED (Assert) Events
    uint64_t                indexFlashLED;                          // Index of last entry in Flash LED Info array below, in case the array wraps
    uint64_t                flashLEDArray[8];                       // Info on the last 8 Flash LED (assert) events wrapping array
    uint64_t                universalTimestampFlashLED[8];          // Universal Timestamp (us) of last 8 Flash LED (assert) Events, wrapping array
    uint64_t                powerCycleFlashLED[8];                  // Power Cycle of the last 8 Flash LED (assert) Events, wrapping array
} ATTR_PACKED;
#pragma pack()
STATIC_ASSERT(sizeof(scsiFarmByActuatorFLED) == (236));

// Seagate Field Access Reliability Metrics log (FARM) parameter (read with SCSI LogSense page 0x3D, sub-page 0x3)
// "By Actuator" Parameters for Reallocation Information
#pragma pack(1)
struct scsiFarmByActuatorReallocation {
    scsiFarmParameterHeader parameterHeader;                        // Parameter Header
	uint64_t                pageNumber;                             // Page Number
	uint64_t                copyNumber;                             // Copy Number  
    uint64_t                actuatorID;                             // Actuator ID 
    uint64_t                totalReallocations;                     // Number of Re-Allocated Sectors
    uint64_t                totalReallocationCanidates;             // Number of Re-Allocated Candidate Sectors 
    uint64_t                reserved[15];                           // Reserved
} ATTR_PACKED;
#pragma pack()
STATIC_ASSERT(sizeof(scsiFarmByActuatorReallocation) == (164));

// Seagate Field Access Reliability Metrics log (FARM) all parameters
#pragma pack(1)
struct scsiFarmLog {
    scsiFarmPageHeader                      pageHeader;                             // Head for whole log page
    scsiFarmHeader                          header;                                 // Log Header parameter
	scsiFarmDriveInformation                driveInformation;                       // Drive Information parameter
	scsiFarmWorkloadStatistics              workload;                               // Workload Statistics parameter
	scsiFarmErrorStatistics                 error;                                  // Error Statistics parameter
	scsiFarmEnvironmentStatistics           environment;                            // Environment Statistics parameter 
	scsiFarmReliabilityStatistics           reliability;                            // Reliability Statistics parameter
    scsiFarmDriveInformation2               driveInformation2;                      // Drive Information parameter continued
    scsiFarmEnvironmentStatistics2          environment2;                           // Environment Statistics parameter continued
    scsiFarmByHead                          reserved;                               // Reserved
    scsiFarmByHead                          reserved0;                              // Reserved
    scsiFarmByHead                          reserved1;                              // Reserved
    scsiFarmByHead                          reserved2;                              // Reserved
    scsiFarmByHead                          reserved3;                              // Reserved
    scsiFarmByHead                          reserved4;                              // Reserved
    scsiFarmByHead                          reserved5;                              // Reserved
    scsiFarmByHead                          reserved6;                              // Reserved
    scsiFarmByHead                          reserved7;                              // Reserved
    scsiFarmByHead                          reserved8;                              // Reserved
    scsiFarmByHead                          mrHeadResistance;                       // MR Head Resistance from most recent SMART Summary Frame by Head
    scsiFarmByHead                          reserved9;                              // Reserved
    scsiFarmByHead                          reserved10;                             // Reserved
    scsiFarmByHead                          reserved11;                             // Reserved
    scsiFarmByHead                          reserved12;                             // Reserved
    scsiFarmByHead                          reserved13;                             // Reserved
    scsiFarmByHead                          reserved14;                             // Reserved
    scsiFarmByHead                          totalReallocations;                     // Number of Reallocated Sectors
    scsiFarmByHead                          totalReallocationCanidates;             // Number of Reallocation Candidate Sectors
    scsiFarmByHead                          reserved15;                             // Reserved
    scsiFarmByHead                          reserved16;                             // Reserved
    scsiFarmByHead                          reserved17;                             // Reserved
    scsiFarmByHead                          writeWorkloadPowerOnTime;               // Write Workload Power-on Time in Seconds, value from most recent SMART Frame by Head
    scsiFarmByHead                          reserved18;                             // Reserved
    scsiFarmByHead                          cumulativeUnrecoverableReadRepeat;      // Cumulative Lifetime Unrecoverable Read Repeat by head
    scsiFarmByHead                          cumulativeUnrecoverableReadUnique;      // Cumulative Lifetime Unrecoverable Read Unique by head
    scsiFarmByHead                          reserved19;                             // Reserved
    scsiFarmByHead                          reserved20;                             // Reserved
    scsiFarmByHead                          reserved21;                             // Reserved
    scsiFarmByHead                          reserved22;                             // Reserved
    scsiFarmByHead                          reserved23;                             // Reserved
    scsiFarmByHead                          reserved24;                             // Reserved
    scsiFarmByHead                          reserved25;                             // Reserved
    scsiFarmByHead                          reserved26;                             // Reserved
    scsiFarmByHead                          reserved27;                             // Reserved
    scsiFarmByHead                          secondMRHeadResistance;                 // Second Head MR Head Resistance from most recent SMART Summary Frame by Head
    scsiFarmByHead                          reserved28;                             // Reserved
    scsiFarmByHead                          reserved29;                             // Reserved
    scsiFarmByHead                          reserved30;                             // Reserved
    scsiFarmByHead                          reserved31;                             // Reserved
    scsiFarmByHead                          reserved32;                             // Reserved
    scsiFarmByHead                          reserved33;                             // Reserved
    scsiFarmByHead                          reserved34;                             // Reserved
    scsiFarmByHead                          reserved35;                             // Reserved
    scsiFarmByHead                          reserved36;                             // Reserved
    scsiFarmByHead                          reserved37;                             // Reserved
    scsiFarmByHead                          reserved38;                             // Reserved
    scsiFarmByActuator                      actuator0;                              // Actuator 0 parameters
    scsiFarmByActuatorFLED                  actuatorFLED0;                          // Actuator 0 FLED Information parameters
    scsiFarmByActuatorReallocation          actuatorReallocation0;                  // Actuator 0 Reallocation parameters
    scsiFarmByActuator                      actuator1;                              // Actuator 1 parameters
    scsiFarmByActuatorFLED                  actuatorFLED1;                          // Actuator 1 FLED Information parameters
    scsiFarmByActuatorReallocation          actuatorReallocation1;                  // Actuator 1 Reallocation parameters
    scsiFarmByActuator                      actuator2;                              // Actuator 2 parameters
    scsiFarmByActuatorFLED                  actuatorFLED2;                          // Actuator 2 FLED Information parameters
    scsiFarmByActuatorReallocation          actuatorReallocation2;                  // Actuator 2 Reallocation parameters
    scsiFarmByActuator                      actuator3;                              // Actuator 3 parameters
    scsiFarmByActuatorFLED                  actuatorFLED3;                          // Actuator 3 FLED Information parameters
    scsiFarmByActuatorReallocation          actuatorReallocation3;                  // Actuator 3 Reallocation parameters
} ATTR_PACKED;
#pragma pack()
STATIC_ASSERT(sizeof(scsiFarmLog) == 4 + 76 + 252 + 148 + 236 + 212 + 236 + 108 + 68 + (47 * ((8 * 16) + 4)) + 188 * 4 + 236 * 4 + 164 * 4);

/* SCSI Peripheral types (of interest) */
#define SCSI_PT_DIRECT_ACCESS           0x0
#define SCSI_PT_SEQUENTIAL_ACCESS       0x1
#define SCSI_PT_CDROM                   0x5
#define SCSI_PT_MEDIUM_CHANGER          0x8
#define SCSI_PT_ENCLOSURE               0xd
#define SCSI_PT_HOST_MANAGED            0x14

/* Transport protocol identifiers or just Protocol identifiers */
#define SCSI_TPROTO_FCP 0
#define SCSI_TPROTO_SPI 1
#define SCSI_TPROTO_SSA 2
#define SCSI_TPROTO_1394 3
#define SCSI_TPROTO_SRP 4            /* SCSI over RDMA */
#define SCSI_TPROTO_ISCSI 5
#define SCSI_TPROTO_SAS 6
#define SCSI_TPROTO_ADT 7
#define SCSI_TPROTO_ATA 8
#define SCSI_TPROTO_UAS 9            /* USB attached SCSI */
#define SCSI_TPROTO_SOP 0xa          /* SCSI over PCIe */
#define SCSI_TPROTO_PCIE 0xb         /* includes NVMe */
#define SCSI_TPROTO_NONE 0xf


/* SCSI Log Pages retrieved by LOG SENSE. 0x0 to 0x3f, 0x30 to 0x3e vendor */
#define SUPPORTED_LPAGES                    0x00
#define BUFFER_OVERRUN_LPAGE                0x01
#define WRITE_ERROR_COUNTER_LPAGE           0x02
#define READ_ERROR_COUNTER_LPAGE            0x03
#define READ_REVERSE_ERROR_COUNTER_LPAGE    0x04
#define VERIFY_ERROR_COUNTER_LPAGE          0x05
#define NON_MEDIUM_ERROR_LPAGE              0x06
#define LAST_N_ERROR_EVENTS_LPAGE           0x07
#define FORMAT_STATUS_LPAGE                 0x08
#define LAST_N_DEFERRED_LPAGE               0x0b   /* or async events */
#define LB_PROV_LPAGE                       0x0c   /* SBC-3 */
#define TEMPERATURE_LPAGE                   0x0d
#define STARTSTOP_CYCLE_COUNTER_LPAGE       0x0e
#define APPLICATION_CLIENT_LPAGE            0x0f
#define SELFTEST_RESULTS_LPAGE              0x10
#define SS_MEDIA_LPAGE                      0x11   /* SBC-3 */
#define BACKGROUND_RESULTS_LPAGE            0x15   /* SBC-3 */
#define ATA_PT_RESULTS_LPAGE                0x16   /* SAT */
#define NONVOL_CACHE_LPAGE                  0x17   /* SBC-3 */
#define PROTOCOL_SPECIFIC_LPAGE             0x18
#define GEN_STATS_PERF_LPAGE                0x19
#define POWER_COND_TRANS_LPAGE              0x1a
#define IE_LPAGE                            0x2f

/* SCSI Log subpages (8 bits), added spc4r05 2006, standardized SPC-4 2015 */
#define NO_SUBPAGE_L_SPAGE              0x0     /* 0x0-0x3f,0x0 */
#define LAST_N_INQ_DAT_L_SPAGE          0x1     /* 0xb,0x1 */
#define LAST_N_MODE_PG_L_SPAGE          0x2     /* 0xb,0x2 */
#define ENVIRO_REP_L_SPAGE              0x1     /* 0xd,0x1 */
#define ENVIRO_LIMITS_L_SPAGE           0x2     /* 0xd,0x2 */
#define UTILIZATION_L_SPAGE             0x1     /* 0xe,0x1 */
#define ZB_DEV_STATS_L_SPAGE            0x1     /* 0x14,0x1 */
#define PEND_DEFECTS_L_SPAGE            0x1     /* 0x15,0x1 */
#define BACKGROUND_OP_L_SPAGE           0x2     /* 0x15,0x2 */
#define LPS_MISALIGN_L_SPAGE            0x3     /* 0x15,0x3 */
#define SUPP_SPAGE_L_SPAGE              0xff    /* 0x0,0xff pages+subpages */

/* Seagate vendor specific log pages. */
#define SEAGATE_CACHE_LPAGE                 0x37
#define SEAGATE_FARM_LPAGE                  0x3d
#define SEAGATE_FACTORY_LPAGE               0x3e

/* Seagate vendor specific log sub-pages. */
#define SEAGATE_FARM_CURRENT_L_SPAGE        0x3     /* 0x3d,0x3 */

/* Log page response lengths */
#define LOG_RESP_SELF_TEST_LEN 0x194

/* See the SSC-2 document at www.t10.org . Earlier note: From IBM
Documentation, see http://www.storage.ibm.com/techsup/hddtech/prodspecs.htm */
#define TAPE_ALERTS_LPAGE                        0x2e

/* ANSI SCSI-3 Mode Pages */
#define VENDOR_UNIQUE_PAGE                       0x00
#define READ_WRITE_ERROR_RECOVERY_PAGE           0x01
#define DISCONNECT_RECONNECT_PAGE                0x02
#define FORMAT_DEVICE_PAGE                       0x03
#define RIGID_DISK_DRIVE_GEOMETRY_PAGE           0x04
#define FLEXIBLE_DISK_PAGE                       0x05
#define VERIFY_ERROR_RECOVERY_PAGE               0x07
#define CACHING_PAGE                             0x08
#define PERIPHERAL_DEVICE_PAGE                   0x09
#define XOR_CONTROL_MODE_PAGE                    0x10
#define CONTROL_MODE_PAGE                        0x0a
#define MEDIUM_TYPES_SUPPORTED_PAGE              0x0b
#define NOTCH_PAGE                               0x0c
#define CD_DEVICE_PAGE                           0x0d
#define CD_AUDIO_CONTROL_PAGE                    0x0e
#define DATA_COMPRESSION_PAGE                    0x0f
#define ENCLOSURE_SERVICES_MANAGEMENT_PAGE       0x14
#define PROTOCOL_SPECIFIC_LUN_PAGE               0x18
#define PROTOCOL_SPECIFIC_PORT_PAGE              0x19
#define POWER_CONDITION_PAGE                     0x1a
#define INFORMATIONAL_EXCEPTIONS_CONTROL_PAGE    0x1c
#define FAULT_FAILURE_REPORTING_PAGE             0x1c

/* Background control mode subpage is [0x1c,0x1] */
#define BACKGROUND_CONTROL_M_SUBPAGE             0x1   /* SBC-2 */

#define ALL_MODE_PAGES                           0x3f

/* Mode page control field */
#define MPAGE_CONTROL_CURRENT               0
#define MPAGE_CONTROL_CHANGEABLE            1
#define MPAGE_CONTROL_DEFAULT               2
#define MPAGE_CONTROL_SAVED                 3

/* SCSI Vital Product Data (VPD) pages */
#define SCSI_VPD_SUPPORTED_VPD_PAGES    0x0
#define SCSI_VPD_UNIT_SERIAL_NUMBER     0x80
#define SCSI_VPD_DEVICE_IDENTIFICATION  0x83
#define SCSI_VPD_EXTENDED_INQUIRY_DATA  0x86
#define SCSI_VPD_ATA_INFORMATION        0x89
#define SCSI_VPD_POWER_CONDITION        0x8a
#define SCSI_VPD_POWER_CONSUMPTION      0x8d
#define SCSI_VPD_BLOCK_LIMITS           0xb0
#define SCSI_VPD_BLOCK_DEVICE_CHARACTERISTICS   0xb1
#define SCSI_VPD_LOGICAL_BLOCK_PROVISIONING     0xb2

/* defines for useful SCSI Status codes */
#define SCSI_STATUS_CHECK_CONDITION     0x2

/* defines for useful Sense Key codes */
#define SCSI_SK_NO_SENSE                0x0
#define SCSI_SK_RECOVERED_ERR           0x1
#define SCSI_SK_NOT_READY               0x2
#define SCSI_SK_MEDIUM_ERROR            0x3
#define SCSI_SK_HARDWARE_ERROR          0x4
#define SCSI_SK_ILLEGAL_REQUEST         0x5
#define SCSI_SK_UNIT_ATTENTION          0x6
#define SCSI_SK_ABORTED_COMMAND         0xb

/* defines for useful Additional Sense Codes (ASCs) */
#define SCSI_ASC_NOT_READY              0x4     /* more info in ASCQ code */
#define SCSI_ASC_NO_MEDIUM              0x3a    /* more info in ASCQ code */
#define SCSI_ASC_UNKNOWN_OPCODE         0x20
#define SCSI_ASC_INVALID_FIELD          0x24
#define SCSI_ASC_UNKNOWN_PARAM          0x26
#define SCSI_ASC_WARNING                0xb
#define SCSI_ASC_IMPENDING_FAILURE      0x5d

#define SCSI_ASCQ_ATA_PASS_THROUGH      0x1d

/* Simplified error code (negative values as per errno) */
#define SIMPLE_NO_ERROR                 0
#define SIMPLE_ERR_NOT_READY            1
#define SIMPLE_ERR_BAD_OPCODE           2
#define SIMPLE_ERR_BAD_FIELD            3       /* in cbd */
#define SIMPLE_ERR_BAD_PARAM            4       /* in data */
#define SIMPLE_ERR_BAD_RESP             5       /* response fails sanity */
#define SIMPLE_ERR_NO_MEDIUM            6       /* no medium present */
#define SIMPLE_ERR_BECOMING_READY       7       /* device will be ready soon */
#define SIMPLE_ERR_TRY_AGAIN            8       /* some warning, try again */
#define SIMPLE_ERR_MEDIUM_HARDWARE      9       /* medium or hardware error */
#define SIMPLE_ERR_UNKNOWN              10      /* unknown sense value */
#define SIMPLE_ERR_ABORTED_COMMAND      11      /* probably transport error */


/* defines for functioncode parameter in SENDDIAGNOSTIC function */
#define SCSI_DIAG_NO_SELF_TEST          0x00
#define SCSI_DIAG_DEF_SELF_TEST         0xff
#define SCSI_DIAG_BG_SHORT_SELF_TEST    0x01
#define SCSI_DIAG_BG_EXTENDED_SELF_TEST 0x02
#define SCSI_DIAG_FG_SHORT_SELF_TEST    0x05
#define SCSI_DIAG_FG_EXTENDED_SELF_TEST 0x06
#define SCSI_DIAG_ABORT_SELF_TEST       0x04


/* SCSI command timeout values (units are seconds) */
#define SCSI_TIMEOUT_DEFAULT    60  // should be longer than the spin up time
                                    // of a disk in JBOD.

#define SCSI_TIMEOUT_SELF_TEST  (5 * 60 * 60)   /* allow max 5 hours for */
                                            /* extended foreground self test */



#define LOGPAGEHDRSIZE  4

class scsi_device;

// Set of supported SCSI VPD pages. Constructor fetches Supported VPD pages
// VPD page and remembers the response for later queries.
class supported_vpd_pages
{
public:
    explicit supported_vpd_pages(scsi_device * device);

    bool is_supported(int vpd_page_num) const;

private:
    int num_valid;      /* 0 or less for invalid */
    unsigned char pages[256];
};

extern supported_vpd_pages * supported_vpd_pages_p;

/* This is a heuristic that takes into account the command bytes and length
 * to decide whether the presented unstructured sequence of bytes could be
 * a SCSI command. If so it returns true otherwise false. Vendor specific
 * SCSI commands (i.e. opcodes from 0xc0 to 0xff), if presented, are assumed
 * to follow SCSI conventions (i.e. length of 6, 10, 12 or 16 bytes). The
 * only SCSI commands considered above 16 bytes of length are the Variable
 * Length Commands (opcode 0x7f) and the XCDB wrapped commands (opcode 0x7e).
 * Both have an inbuilt length field which can be cross checked with clen.
 * No NVMe commands (64 bytes long plus some extra added by some OSes) have
 * opcodes 0x7e or 0x7f yet. ATA is register based but SATA has FIS
 * structures that are sent across the wire. The FIS register structure is
 * used to move a command from a SATA host to device, but the ATA 'command'
 * is not the first byte. So it is harder to say what will happen if a
 * FIS structure is presented as a SCSI command, hopefully there is a low
 * probability this function will yield true in that case. */
bool is_scsi_cdb(const uint8_t * cdbp, int clen);

// Print SCSI debug messages?
extern unsigned char scsi_debugmode;

void scsi_do_sense_disect(const struct scsi_cmnd_io * in,
                          struct scsi_sense_disect * out);

int scsiSimpleSenseFilter(const struct scsi_sense_disect * sinfo);

const char * scsiErrString(int scsiErr);

int scsi_vpd_dev_id_iter(const unsigned char * initial_desig_desc,
                         int page_len, int * off, int m_assoc,
                         int m_desig_type, int m_code_set);

int scsi_decode_lu_dev_id(const unsigned char * b, int blen, char * s,
                          int slen, int * transport);


/* STANDARD SCSI Commands  */
int scsiTestUnitReady(scsi_device * device);

int scsiStdInquiry(scsi_device * device, uint8_t *pBuf, int bufLen);

int scsiInquiryVpd(scsi_device * device, int vpd_page, uint8_t *pBuf,
                   int bufLen);

int scsiLogSense(scsi_device * device, int pagenum, int subpagenum,
                 uint8_t *pBuf, int bufLen, int known_resp_len);

int scsiLogSelect(scsi_device * device, int pcr, int sp, int pc, int pagenum,
                  int subpagenum, uint8_t *pBuf, int bufLen);

int scsiModeSense(scsi_device * device, int pagenum, int subpagenum, int pc,
                  uint8_t *pBuf, int bufLen);

int scsiModeSelect(scsi_device * device, int sp, uint8_t *pBuf, int bufLen);

int scsiModeSense10(scsi_device * device, int pagenum, int subpagenum, int pc,
                    uint8_t *pBuf, int bufLen);

int scsiModeSelect10(scsi_device * device, int sp, uint8_t *pBuf, int bufLen);

int scsiModePageOffset(const uint8_t * resp, int len, int modese_len);

int scsiRequestSense(scsi_device * device,
                     struct scsi_sense_disect * sense_info);

int scsiSendDiagnostic(scsi_device * device, int functioncode, uint8_t *pBuf,
                       int bufLen);

int scsiReadDefect10(scsi_device * device, int req_plist, int req_glist,
                     int dl_format, uint8_t *pBuf, int bufLen);

int scsiReadDefect12(scsi_device * device, int req_plist, int req_glist,
                     int dl_format, int addrDescIndex, uint8_t *pBuf,
                     int bufLen);

int scsiReadCapacity10(scsi_device * device, unsigned int * last_lbp,
                       unsigned int * lb_sizep);

int scsiReadCapacity16(scsi_device * device, uint8_t *pBuf, int bufLen);

/* SMART specific commands */
int scsiCheckIE(scsi_device * device, int hasIELogPage, int hasTempLogPage,
                uint8_t *asc, uint8_t *ascq, uint8_t *currenttemp,
                uint8_t *triptemp);

int scsiFetchIECmpage(scsi_device * device, struct scsi_iec_mode_page *iecp,
                      int modese_len);
int scsi_IsExceptionControlEnabled(const struct scsi_iec_mode_page *iecp);
int scsi_IsWarningEnabled(const struct scsi_iec_mode_page *iecp);
int scsiSetExceptionControlAndWarning(scsi_device * device, int enabled,
                            const struct scsi_iec_mode_page *iecp);
void scsiDecodeErrCounterPage(unsigned char * resp,
                              struct scsiErrorCounter *ecp);
void scsiDecodeNonMediumErrPage(unsigned char * resp,
                                struct scsiNonMediumError *nmep);
int scsiFetchExtendedSelfTestTime(scsi_device * device, int * durationSec,
                                  int modese_len);
int scsiCountFailedSelfTests(scsi_device * device, int noisy);
int scsiSelfTestInProgress(scsi_device * device, int * inProgress);
int scsiFetchControlGLTSD(scsi_device * device, int modese_len, int current);
int scsiSetControlGLTSD(scsi_device * device, int enabled, int modese_len);
int scsiFetchTransportProtocol(scsi_device * device, int modese_len);
int scsiGetRPM(scsi_device * device, int modese_len, int * form_factorp,
               int * haw_zbcp);
int scsiGetSetCache(scsi_device * device,  int modese_len, short int * wce,
                    short int * rcd);
uint64_t scsiGetSize(scsi_device * device, bool avoid_rcap16,
                     struct scsi_readcap_resp * srrp);

/* T10 Standard IE Additional Sense Code strings taken from t10.org */
const char* scsiGetIEString(uint8_t asc, uint8_t ascq);
int scsiGetTemp(scsi_device * device, uint8_t *currenttemp, uint8_t *triptemp);


int scsiSmartDefaultSelfTest(scsi_device * device);
int scsiSmartShortSelfTest(scsi_device * device);
int scsiSmartExtendSelfTest(scsi_device * device);
int scsiSmartShortCapSelfTest(scsi_device * device);
int scsiSmartExtendCapSelfTest(scsi_device * device);
int scsiSmartSelfTestAbort(scsi_device * device);

const char * scsiTapeAlertsTapeDevice(unsigned short code);
const char * scsiTapeAlertsChangerDevice(unsigned short code);

const char * scsi_get_opcode_name(uint8_t opcode);
void scsi_format_id_string(char * out, const uint8_t * in, int n);

void dStrHex(const uint8_t * up, int len, int no_ascii);

/* Attempt to find the first SCSI sense data descriptor that matches the
   given 'desc_type'. If found return pointer to start of sense data
   descriptor; otherwise (including fixed format sense data) returns NULL. */
const unsigned char * sg_scsi_sense_desc_find(const unsigned char * sensep,
                                              int sense_len, int desc_type);


/* SCSI command transmission interface function declaration. Its
 * definition is target OS specific (see os_<OS>.c file).
 * Returns 0 if SCSI command successfully launched and response
 * received. Even when 0 is returned the caller should check
 * scsi_cmnd_io::scsi_status for SCSI defined errors and warnings
 * (e.g. CHECK CONDITION). If the SCSI command could not be issued
 * (e.g. device not present or not a SCSI device) or some other problem
 * arises (e.g. timeout) then returns a negative errno value. */
// Moved to C++ interface
//int do_scsi_cmnd_io(int dev_fd, struct scsi_cmnd_io * iop, int report);



#endif
