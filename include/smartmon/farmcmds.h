/*
 * farmcmds.h
 *
 * Home page of code is: https://www.smartmontools.org
 *
 * Copyright (C) 2021 - 2023 Seagate Technology LLC and/or its Affiliates
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef FARMCMDS_H
#define FARMCMDS_H

// Add __attribute__((packed)) if compiler supports it
// because some gcc versions (at least ARM) lack support of #pragma pack()
#ifdef HAVE_ATTR_PACKED_FARM
#define ATTR_PACKED_FARM __attribute__((packed))
#else
#define ATTR_PACKED_FARM
#endif

#include <stdint.h>

#include "atacmds.h"
#include "dev_interface.h"
#include "knowndrives.h"
#include "static_assert.h"

///////////////////////////////////////////////////////////////////////////////////
// Seagate ATA Field Access Reliability Metrics log (FARM) structures (GP Log 0xA6)

// Seagate ATA Field Access Reliability Metrics log (FARM) page 0 (read with ATA_READ_LOG_EXT address 0xA6, page 0)
// Log Header
struct ataFarmHeader {
  uint64_t signature;       // Log Signature = 0x00004641524D4552
  uint64_t majorRev;        // Log Major Revision
  uint64_t minorRev;        // Log Rinor Revision
  uint64_t pagesSupported;  // Number of Pages Supported
  uint64_t logSize;         // Log Size in Bytes
  uint64_t pageSize;        // Page Size in Bytes
  uint64_t headsSupported;  // Maximum Drive Heads Supported
  uint64_t copies;          // Number of Historical Copies
  uint64_t frameCapture;    // Reason for Frame Capture
};
STATIC_ASSERT(sizeof(ataFarmHeader)== 72);

// Seagate ATA Field Access Reliability Metrics log (FARM) page 1 (read with ATA_READ_LOG_EXT address 0xA6, page 1)
// Drive Information
struct ataFarmDriveInformation {
  uint64_t pageNumber;            // Page Number = 1
  uint64_t copyNumber;            // Copy Number
  uint64_t serialNumber;          // Serial Number [0:3]
  uint64_t serialNumber2;         // Serial Number [4:7]
  uint64_t worldWideName;         // World Wide Name [0:3]
  uint64_t worldWideName2;        // World Wide Name [4:7]
  uint64_t deviceInterface;       // Device Interface
  uint64_t deviceCapacity;        // 48-bit Device Capacity
  uint64_t psecSize;              // Physical Sector Size in Bytes
  uint64_t lsecSize;              // Logical Sector Size in Bytes
  uint64_t deviceBufferSize;      // Device Buffer Size in Bytes
  uint64_t heads;                 // Number of Heads
  uint64_t factor;                // Device Form Factor (ID Word 168)
  uint64_t rotationRate;          // Rotational Rate of Device (ID Word 217)
  uint64_t firmwareRev;           // Firmware Revision [0:3]
  uint64_t firmwareRev2;          // Firmware Revision [4:7]
  uint64_t security;              // ATA Security State (ID Word 128)
  uint64_t featuresSupported;     // ATA Features Supported (ID Word 78)
  uint64_t featuresEnabled;       // ATA Features Enabled (ID Word 79)
  uint64_t poh;                   // Power-On Hours
  uint64_t spoh;                  // Spindle Power-On Hours
  uint64_t headFlightHours;       // Head Flight Hours
  uint64_t headLoadEvents;        // Head Load Events
  uint64_t powerCycleCount;       // Power Cycle Count
  uint64_t resetCount;            // Hardware Reset Count
  uint64_t spinUpTime;            // SMART Spin-Up Time in milliseconds
  uint64_t reserved;              // Reserved
  uint64_t reserved0;             // Reserved
  uint64_t reserved1;             // Reserved
  uint64_t reserved2;             // Reserved
  uint64_t timeToReady;           // Time to ready of the last power cycle
  uint64_t timeHeld;              // Time drive is held in staggered spin during the last power on sequence
  uint64_t modelNumber[10];       // Lower 32 Model Number (added 2.14)
  uint64_t driveRecordingType;    // 0 for SMR and 1 for CMR (added 2.15)
  uint64_t depopped;              // Has the drive been depopped  1 = depopped and 0 = not depopped (added 2.15)
  uint64_t maxNumberForReasign;   // Max Number of Available Sectors for Reassignment. Value in disc sectors (added 3.3)
  uint64_t dateOfAssembly;        // Date of assembly in ASCII YYWW where YY is the year and WW is the calendar week (added 4.2)
  uint64_t depopulationHeadMask;  // Depopulation Head Mask
};
STATIC_ASSERT(sizeof(ataFarmDriveInformation)== 376);

// Seagate ATA Field Access Reliability Metrics log (FARM) page 2 (read with ATA_READ_LOG_EXT address 0xA6, page 2)
// Workload Statistics
struct ataFarmWorkloadStatistics {
  uint64_t pageNumber;              // Page Number = 2
  uint64_t copyNumber;              // Copy Number
  uint64_t reserved;                // Reserved
  uint64_t totalReadCommands;       // Total Number of Read Commands
  uint64_t totalWriteCommands;      // Total Number of Write Commands
  uint64_t totalRandomReads;        // Total Number of Random Read Commands
  uint64_t totalRandomWrites;       // Total Number of Random Write Commands
  uint64_t totalNumberofOtherCMDS;  // Total Number Of Other Commands
  uint64_t logicalSecWritten;       // Logical Sectors Written
  uint64_t logicalSecRead;          // Logical Sectors Read
  uint64_t dither;                  // Number of dither events during current power cycle (added 3.4)
  uint64_t ditherRandom;            // Number of times dither was held off during random workloads during current power cycle (added 3.4)
  uint64_t ditherSequential;        // Number of times dither was held off during sequential workloads during current power cycle (added 3.4)
  uint64_t readCommandsByRadius1;   // Number of Read Commands from 0-3.125% of LBA space for last 3 SMART Summary Frames (added 4.4)
  uint64_t readCommandsByRadius2;   // Number of Read Commands from 3.125-25% of LBA space for last 3 SMART Summary Frames (added 4.4)
  uint64_t readCommandsByRadius3;   // Number of Read Commands from 25-75% of LBA space for last 3 SMART Summary Frames (added 4.4)
  uint64_t readCommandsByRadius4;   // Number of Read Commands from 75-100% of LBA space for last 3 SMART Summary Frames (added 4.4)
  uint64_t writeCommandsByRadius1;  // Number of Write Commands from 0-3.125% of LBA space for last 3 SMART Summary Frames (added 4.4)
  uint64_t writeCommandsByRadius2;  // Number of Write Commands from 3.125-25% of LBA space for last 3 SMART Summary Frames (added 4.4)
  uint64_t writeCommandsByRadius3;  // Number of Write Commands from 25-75% of LBA space for last 3 SMART Summary Frames (added 4.4)
  uint64_t writeCommandsByRadius4;  // Number of Write Commands from 75-100% of LBA space for last 3 SMART Summary Frames (added 4.4)
};
STATIC_ASSERT(sizeof(ataFarmWorkloadStatistics)== 168);

// Seagate ATA Field Access Reliability Metrics log (FARM) page 3 (read with ATA_READ_LOG_EXT address 0xA6, page 3)
// Error Statistics
struct ataFarmErrorStatistics {
  uint64_t pageNumber;                                // Page Number = 3
  uint64_t copyNumber;                                // Copy Number
  uint64_t totalUnrecoverableReadErrors;              // Number of Unrecoverable Read Errors
  uint64_t totalUnrecoverableWriteErrors;             // Number of Unrecoverable Write Errors
  uint64_t totalReallocations;                        // Number of Re-Allocated Sectors
  uint64_t totalReadRecoveryAttepts;                  // Number of Read Recovery Attempts
  uint64_t totalMechanicalStartRetries;               // Number of Mechanical Start Retries
  uint64_t totalReallocationCanidates;                // Number of Re-Allocated Candidate Sectors
  uint64_t totalASREvents;                            // Number of ASR Events
  uint64_t totalCRCErrors;                            // Number of Interface CRC Errors
  uint64_t attrSpinRetryCount;                        // Spin Retry Count (Most recent value from array at byte 401 of attribute sector)
  uint64_t normalSpinRetryCount;                      // Spin Retry Count (SMART Attribute 10 Normalized)
  uint64_t worstSpinRretryCount;                      // Spin Retry Count (SMART Attribute 10 Worst Ever)
  uint64_t attrIOEDCErrors;                           // Number of IOEDC Errors (SMART Attribute 184 Raw)
  uint64_t attrCTOCount;                              // CTO Count Total (SMART Attribute 188 Raw[0..1])
  uint64_t overFiveSecCTO;                            // CTO Count Over 5s (SMART Attribute 188 Raw[2..3])
  uint64_t overSevenSecCTO;                           // CTO Count Over 7.5s (SMART Attribute 188 Raw[4..5])
  uint64_t totalFlashLED;                             // Total Flash LED (Assert) Events
  uint64_t indexFlashLED;                             // Index of last entry in Flash LED Info array below, in case the array wraps
  uint64_t uncorrectables;                            // Uncorrectable errors (SMART Attribute 187 Raw)
  uint64_t reserved;                                  // Reserved
  uint64_t flashLEDArray[8];                          // Info on the last 8 Flash LED (assert) events wrapping array (added 2.7)
  uint64_t reserved0[8];                              // Reserved
  uint64_t reserved1[2];                              // Reserved
  uint64_t reserved2[15];                             // Reserved
  uint64_t universalTimestampFlashLED[8];             // Universal Timestamp (us) of last 8 Flash LED (assert) Events, wrapping array
  uint64_t powerCycleFlashLED[8];                     // Power Cycle of the last 8 Flash LED (assert) Events, wrapping array
  uint64_t cumulativeUnrecoverableReadERC;            // Cumulative Lifetime Unrecoverable Read errors due to Error Recovery Control (e.g. ERC timeout)
  uint64_t cumulativeUnrecoverableReadRepeating[24];  // Cumulative Lifetime Unrecoverable Read Repeating by head
  uint64_t cumulativeUnrecoverableReadUnique[24];     // Cumulative Lifetime Unrecoverable Read Unique by head
};
STATIC_ASSERT(sizeof(ataFarmErrorStatistics)== 952);

// Seagate ATA Field Access Reliability Metrics log (FARM) page 4 (read with ATA_READ_LOG_EXT address 0xa6, page 4)
// Environment Statistics
struct ataFarmEnvironmentStatistics {
  uint64_t pageNumber;         // Page Number = 4
  uint64_t copyNumber;         // Copy Number
  uint64_t curentTemp;         // Current Temperature in Celsius
  uint64_t highestTemp;        // Highest Temperature in Celsius
  uint64_t lowestTemp;         // Lowest Temperature in Celsius
  uint64_t averageTemp;        // Average Short-Term Temperature in Celsius
  uint64_t averageLongTemp;    // Average Long-Term Temperature in Celsius
  uint64_t highestShortTemp;   // Highest Average Short-Term Temperature in Celsius
  uint64_t lowestShortTemp;    // Lowest Average Short-Term Temperature in Celsius
  uint64_t highestLongTemp;    // Highest Average Long-Term Temperature in Celsius
  uint64_t lowestLongTemp;     // Lowest Average Long-Term Temperature in Celsius
  uint64_t overTempTime;       // Time In Over Temperature in Minutes
  uint64_t underTempTime;      // Time In Under Temperature in Minutes
  uint64_t maxTemp;            // Specified Max Operating Temperature in Celsius
  uint64_t minTemp;            // Specified Min Operating Temperature in Celsius
  uint64_t reserved;           // Reserved
  uint64_t reserved0;          // Reserved
  uint64_t humidity;           // Current Relative Humidity (in units of 0.1%)
  uint64_t reserved1;          // Reserved
  uint64_t currentMotorPower;  // Current Motor Power, value from most recent SMART Summary Frame
  uint64_t current12v;         // Current 12V input in mV (added 3.7)
  uint64_t min12v;             // Minimum 12V input from last 3 SMART Summary Frames in mV (added 3.7)
  uint64_t max12v;             // Maximum 12V input from last 3 SMART Summary Frames in mV (added 3.7)
  uint64_t current5v;          // Current 5V input in mV (added 3.7)
  uint64_t min5v;              // Minimum 5V input from last 3 SMART Summary Frames in mV (added 3.7)
  uint64_t max5v;              // Maximum 5V input from last 3 SMART Summary Frames in mV (added 3.7)
  uint64_t powerAverage12v;    // 12V Power Average (mW) - Average of last 3 SMART Summary Frames (added 4.3)
  uint64_t powerMin12v;        // 12V Power Min (mW) - Lowest of last 3 SMART Summary Frames (added 4.3)
  uint64_t powerMax12v;        // 12V Power Max (mW) - Highest of last 3 SMART Summary Frames (added 4.3)
  uint64_t powerAverage5v;     // 5V Power Average (mW) - Average of last 3 SMART Summary Frames (added 4.3)
  uint64_t powerMin5v;         // 5V Power Min (mW) - Lowest of last 3 SMART Summary Frames (added 4.3)
  uint64_t powerMax5v;         // 5V Power Max (mW) - Highest of last 3 SMART Summary Frames (added 4.3)
};
STATIC_ASSERT(sizeof(ataFarmEnvironmentStatistics)== 256);

// Seagate ATA Field Access Reliability Metrics log (FARM) page 5 (read with ATA_READ_LOG_EXT address 0xA6, page 5)
// Reliability Statistics
struct ataFarmReliabilityStatistics {
  int64_t pageNumber;                        // Page Number = 5
  int64_t copyNumber;                        // Copy Number
  uint64_t reserved;                         // Reserved
  uint64_t reserved0;                        // Reserved
  uint64_t reserved1[24];                    // Reserved
  uint64_t reserved2[24];                    // Reserved
  uint64_t reserved3;                        // Reserved
  uint64_t reserved4;                        // Reserved
  uint64_t reserved5;                        // Reserved
  uint64_t reserved6;                        // Reserved
  uint64_t reserved7;                        // Reserved
  uint64_t reserved8;                        // Reserved
  uint64_t reserved9;                        // Reserved
  uint64_t reserved10;                       // Reserved
  uint64_t reserved11;                       // Reserved
  uint64_t reserved12;                       // Reserved
  uint64_t reserved13;                       // Reserved
  uint64_t reserved14[24];                   // Reserved
  uint64_t reserved15;                       // Reserved
  int64_t DVGASkipWriteDetect[24];           // [24] DVGA Skip Write Detect by Head
  int64_t RVGASkipWriteDetect[24];           // [24] RVGA Skip Write Detect by Head
  int64_t FVGASkipWriteDetect[24];           // [24] FVGA Skip Write Detect by Head
  int64_t skipWriteDetectThresExceeded[24];  // [24] Skip Write Detect Threshold Exceeded Count by Head
  int64_t attrErrorRateRaw;                  // Error Rate Raw
  int64_t attrErrorRateNormal;               // Error Rate Normalized
  int64_t attrErrorRateWorst;                // Error Rate Worst
  int64_t attrSeekErrorRateRaw;              // Seek Error Rate Raw
  int64_t attrSeekErrorRateNormal;           // Seek Error Rate Normalized
  int64_t attrSeekErrorRateWorst;            // Seek Error Rate Worst
  int64_t attrUnloadEventsRaw;               // High Priority Unload Events
  uint64_t reserved16;                       // Reserved
  uint64_t reserved17[24];                   // Reserved
  uint64_t reserved18[24];                   // Reserved
  uint64_t reserved19[24];                   // Reserved
  uint64_t mrHeadResistance[24];             // MR Head Resistance from most recent SMART Summary Frame by Head
  uint64_t reserved21[24];                   // Reserved
  uint64_t reserved22[24];                   // Reserved
  uint64_t reserved23[24];                   // Reserved
  uint64_t reserved24[24][3];                // Reserved
  uint64_t reserved25[24][3];                // Reserved
  uint64_t reserved26[24];                   // Reserved
  uint64_t reserved27[24];                   // Reserved
  int64_t reserved28[24];                    // Reserved
  int64_t reserved29[24][3];                 // Reserved
  uint64_t reserved30;                       // Reserved
  int64_t reallocatedSectors[24];            // [24] Number of Reallocated Sectors per Head
  int64_t reallocationCandidates[24];        // [24] Number of Reallocation Candidate Sectors per Head
  int64_t heliumPresureTrip;                 // Helium Pressure Threshold Tripped ( 1 - trip, 0 - no trip)
  uint64_t reserved31[24];                   // Reserved
  uint64_t reserved32[24];                   // Reserved
  uint64_t reserved33[24];                   // Reserved
  int64_t writeWorkloadPowerOnTime[24];      // [24] Write Workload Power-on Time in Seconds, value from most recent SMART Summary Frame by Head
  uint64_t reserved34;                       // Reserved
  uint64_t reserved35;                       // Reserved
  uint64_t reserved36;                       // Reserved
  uint64_t reserved37[24];                   // Reserved
  int64_t secondMRHeadResistance[24];        // [24] Second Head, MR Head Resistance from most recent SMART Summary Frame by Head
  uint64_t reserved38[24];                   // Reserved
  uint64_t reserved39[24];                   // Reserved
  uint64_t reserved40[24][3];                // Reserved
  uint64_t reserved41[24][3];                // Reserved
  uint64_t reserved42[24][3];                // Reserved
  int64_t numberLBACorrectedParitySector;    // Number of LBAs Corrected by Parity Sector
};
STATIC_ASSERT(sizeof(ataFarmReliabilityStatistics)== 8880);

// Seagate ATA Field Access Reliability Metrics log (FARM) all pages
struct ataFarmLog {
  ataFarmHeader header;                      // Log Header page
  ataFarmDriveInformation driveInformation;  // Drive Information page
  ataFarmWorkloadStatistics workload;        // Workload Statistics page
  ataFarmErrorStatistics error;              // Error Statistics page
  ataFarmEnvironmentStatistics environment;  // Environment Statistics page
  ataFarmReliabilityStatistics reliability;  // Reliability Statistics page
};
STATIC_ASSERT(sizeof(ataFarmLog)== 72 + 376 + 168 + 952 + 256 + 8880);

/////////////////////////////////////////////////////////////////////////////////////////////////////////
// Seagate SCSI Field Access Reliability Metrics log (FARM) structures (SCSI Log Page 0x3D, sub-page 0x3)

// Seagate SCSI Field Access Reliability Metrics log (FARM) PAGE Header (read with SCSI LogSense page 0x3D, sub-page 0x3)
// Page Header
struct scsiFarmPageHeader {
  uint8_t pageCode;     // Page Code (0x3D)
  uint8_t subpageCode;  // Sub-Page Code (0x03)
  uint16_t pageLength;  // Page Length
};
STATIC_ASSERT(sizeof(scsiFarmPageHeader)== 4);

// Seagate SCSI Field Access Reliability Metrics log (FARM) PARAMETER Header (read with SCSI LogSense page 0x3D, sub-page 0x3)
// Parameter Header
struct scsiFarmParameterHeader {
  uint16_t parameterCode;    // Page Code (0x3D)
  uint8_t parameterControl;  // Sub-Page Code (0x03)
  uint8_t parameterLength;   // Page Length
};
STATIC_ASSERT(sizeof(scsiFarmParameterHeader)== 4);

// Seagate SCSI Field Access Reliability Metrics log (FARM) parameter (read with SCSI LogSense page 0x3D, sub-page 0x3)
// Log Header
#pragma pack(1)
struct scsiFarmHeader {
  scsiFarmParameterHeader parameterHeader;  // Parameter Header
  uint64_t signature;                       // Log Signature = 0x00004641524D4552
  uint64_t majorRev;                        // Log Major Revision
  uint64_t minorRev;                        // Log Rinor Revision
  uint64_t parametersSupported;             // Number of Parameters Supported
  uint64_t logSize;                         // Log Page Size in Bytes
  uint64_t reserved;                        // Reserved
  uint64_t headsSupported;                  // Maximum Drive Heads Supported
  uint64_t reserved0;                       // Reserved
  uint64_t frameCapture;                    // Reason for Frame Capture
} ATTR_PACKED_FARM;
#pragma pack()
STATIC_ASSERT(sizeof(scsiFarmHeader)== 76);

// Seagate SCSI Field Access Reliability Metrics log (FARM) parameter (read with SCSI LogSense page 0x3D, sub-page 0x3)
// Drive Information
#pragma pack(1)
struct scsiFarmDriveInformation {
  scsiFarmParameterHeader parameterHeader;  // Parameter Header
  uint64_t pageNumber;                      // Page Number = 1
  uint64_t copyNumber;                      // Copy Number
  uint64_t serialNumber;                    // Serial Number [0:3]
  uint64_t serialNumber2;                   // Serial Number [4:7]
  uint64_t worldWideName;                   // World Wide Name [0:3]
  uint64_t worldWideName2;                  // World Wide Name [4:7]
  uint64_t deviceInterface;                 // Device Interface
  uint64_t deviceCapacity;                  // 48-bit Device Capacity
  uint64_t psecSize;                        // Physical Sector Size in Bytes
  uint64_t lsecSize;                        // Logical Sector Size in Bytes
  uint64_t deviceBufferSize;                // Device Buffer Size in Bytes
  uint64_t heads;                           // Number of Heads
  uint64_t factor;                          // Device Form Factor (ID Word 168)
  uint64_t rotationRate;                    // Rotational Rate of Device (ID Word 217)
  uint64_t firmwareRev;                     // Firmware Revision [0:3]
  uint64_t firmwareRev2;                    // Firmware Revision [4:7]
  uint64_t reserved;                        // Reserved
  uint64_t reserved0;                       // Reserved
  uint64_t reserved1;                       // Reserved
  uint64_t poh;                             // Power-On Hours
  uint64_t reserved2;                       // Reserved
  uint64_t reserved3;                       // Reserved
  uint64_t reserved4;                       // Reserved
  uint64_t powerCycleCount;                 // Power Cycle Count
  uint64_t resetCount;                      // Hardware Reset Count
  uint64_t reserved5;                       // Reserved
  uint64_t reserved6;                       // Reserved
  uint64_t reserved7;                       // Reserved
  uint64_t reserved8;                       // Reserved
  uint64_t reserved9;                       // Reserved
  uint64_t dateOfAssembly;                  // Date of assembly in ASCII YYWW where YY is the year and WW is the calendar week (added 4.2)
} ATTR_PACKED_FARM;
#pragma pack()
STATIC_ASSERT(sizeof(scsiFarmDriveInformation)== 252);

// Seagate SCSI Field Access Reliability Metrics log (FARM) parameter (read with SCSI LogSense page 0x3D, sub-page 0x3)
// Workload Statistics
#pragma pack(1)
struct scsiFarmWorkloadStatistics {
  scsiFarmParameterHeader parameterHeader;  // Parameter Header
  uint64_t pageNumber;                      // Page Number = 2
  uint64_t copyNumber;                      // Copy Number
  uint64_t reserved;                        // Reserved
  uint64_t totalReadCommands;               // Total Number of Read Commands
  uint64_t totalWriteCommands;              // Total Number of Write Commands
  uint64_t totalRandomReads;                // Total Number of Random Read Commands
  uint64_t totalRandomWrites;               // Total Number of Random Write Commands
  uint64_t totalNumberofOtherCMDS;          // Total Number Of Other Commands
  uint64_t logicalSecWritten;               // Logical Sectors Written
  uint64_t logicalSecRead;                  // Logical Sectors Read
  uint64_t readCommandsByRadius1;           // Number of Read Commands from 0-3.125% of LBA space for last 3 SMART Summary Frames (added 4.4)
  uint64_t readCommandsByRadius2;           // Number of Read Commands from 3.125-25% of LBA space for last 3 SMART Summary Frames (added 4.4)
  uint64_t readCommandsByRadius3;           // Number of Read Commands from 25-75% of LBA space for last 3 SMART Summary Frames (added 4.4)
  uint64_t readCommandsByRadius4;           // Number of Read Commands from 75-100% of LBA space for last 3 SMART Summary Frames (added 4.4)
  uint64_t writeCommandsByRadius1;          // Number of Write Commands from 0-3.125% of LBA space for last 3 SMART Summary Frames (added 4.4)
  uint64_t writeCommandsByRadius2;          // Number of Write Commands from 3.125-25% of LBA space for last 3 SMART Summary Frames (added 4.4)
  uint64_t writeCommandsByRadius3;          // Number of Write Commands from 25-75% of LBA space for last 3 SMART Summary Frames (added 4.4)
  uint64_t writeCommandsByRadius4;          // Number of Write Commands from 75-100% of LBA space for last 3 SMART Summary Frames (added 4.4)
} ATTR_PACKED_FARM;
#pragma pack()
STATIC_ASSERT(sizeof(scsiFarmWorkloadStatistics)== 148);

// Seagate SCSI Field Access Reliability Metrics log (FARM) parameter (read with SCSI LogSense page 0x3D, sub-page 0x3)
// Error Statistics
#pragma pack(1)
struct scsiFarmErrorStatistics {
  scsiFarmParameterHeader parameterHeader;  // Parameter Header
  uint64_t pageNumber;                      // Page Number = 3
  uint64_t copyNumber;                      // Copy Number
  uint64_t totalUnrecoverableReadErrors;    // Number of Unrecoverable Read Errors
  uint64_t totalUnrecoverableWriteErrors;   // Number of Unrecoverable Write Errors
  uint64_t reserved;                        // Reserved
  uint64_t reserved0;                       // Reserved
  uint64_t totalMechanicalStartRetries;     // Number of Mechanical Start Retries
  uint64_t reserved1;                       // Reserved
  uint64_t reserved2;                       // Reserved
  uint64_t reserved3;                       // Reserved
  uint64_t reserved4;                       // Reserved
  uint64_t reserved5;                       // Reserved
  uint64_t reserved6;                       // Reserved
  uint64_t reserved7;                       // Reserved
  uint64_t reserved8;                       // Reserved
  uint64_t reserved9;                       // Reserved
  uint64_t reserved10;                      // Reserved
  uint64_t reserved11;                      // Reserved
  uint64_t reserved12;                      // Reserved
  uint64_t reserved13;                      // Reserved
  uint64_t tripCode;                        // If SMART Trip present the reason code (FRU code)
  uint64_t invalidDWordCountA;              // Invalid DWord Count (Port A)
  uint64_t invalidDWordCountB;              // Invalid DWord Count (Port B)
  uint64_t disparityErrorCodeA;             // Disparity Error Count (Port A)
  uint64_t disparityErrorCodeB;             // Disparity Error Count (Port A)
  uint64_t lossOfDWordSyncA;                // Loss of DWord Sync (Port A)
  uint64_t lossOfDWordSyncB;                // Loss of DWord Sync (Port A)
  uint64_t phyResetProblemA;                // Phy Reset Problem (Port A)
  uint64_t phyResetProblemB;                // Phy Reset Problem (Port A)
} ATTR_PACKED_FARM;
#pragma pack()
STATIC_ASSERT(sizeof(scsiFarmErrorStatistics)== 236);

// Seagate SCSI Field Access Reliability Metrics log (FARM) parameter (read with SCSI LogSense page 0x3D, sub-page 0x3)
// Environment Statistics
#pragma pack(1)
struct scsiFarmEnvironmentStatistics {
  scsiFarmParameterHeader parameterHeader;  // Parameter Header
  uint64_t pageNumber;                      // Page Number = 4
  uint64_t copyNumber;                      // Copy Number
  uint64_t curentTemp;                      // Current Temperature in Celsius (Lower 16 bits are a signed integer in units of 0.1C)
  uint64_t highestTemp;                     // Highest Temperature in Celsius (Lower 16 bits are a signed integer in units of 0.1C)
  uint64_t lowestTemp;                      // Lowest Temperature in Celsius (Lower 16 bits are a signed integer in units of 0.1C)
  uint64_t reserved;                        // Reserved
  uint64_t reserved0;                       // Reserved
  uint64_t reserved1;                       // Reserved
  uint64_t reserved2;                       // Reserved
  uint64_t reserved3;                       // Reserved
  uint64_t reserved4;                       // Reserved
  uint64_t reserved5;                       // Reserved
  uint64_t reserved6;                       // Reserved
  uint64_t maxTemp;                         // Specified Max Operating Temperature in Celsius
  uint64_t minTemp;                         // Specified Min Operating Temperature in Celsius
  uint64_t reserved7;                       // Reserved
  uint64_t reserved8;                       // Reserved
  uint64_t humidity;                        // Current Relative Humidity (in units of 0.1%)
  uint64_t reserved9;                       // Reserved
  uint64_t currentMotorPower;               // Current Motor Power, value from most recent SMART Summary Frame
  uint64_t powerAverage12v;                 // 12V Power Average (mW) - Average of last 3 SMART Summary Frames (added 4.3)
  uint64_t powerMin12v;                     // 12V Power Min (mW) - Lowest of last 3 SMART Summary Frames (added 4.3)
  uint64_t powerMax12v;                     // 12V Power Max (mW) - Highest of last 3 SMART Summary Frames (added 4.3)
  uint64_t powerAverage5v;                  // 5V Power Average (mW) - Average of last 3 SMART Summary Frames (added 4.3)
  uint64_t powerMin5v;                      // 5V Power Min (mW) - Lowest of last 3 SMART Summary Frames (added 4.3)
  uint64_t powerMax5v;                      // 5V Power Max (mW) - Highest of last 3 SMART Summary Frames (added 4.3)
} ATTR_PACKED_FARM;
#pragma pack()
STATIC_ASSERT(sizeof(scsiFarmEnvironmentStatistics)== 212);

// Seagate SCSI Field Access Reliability Metrics log (FARM) parameter (read with SCSI LogSense page 0x3D, sub-page 0x3)
// Reliability Statistics
#pragma pack(1)
struct scsiFarmReliabilityStatistics {
  scsiFarmParameterHeader parameterHeader;  // Parameter Header
  int64_t pageNumber;                       // Page Number = 5
  int64_t copyNumber;                       // Copy Number
  uint64_t reserved;                        // Reserved
  uint64_t reserved0;                       // Reserved
  uint64_t reserved1;                       // Reserved
  uint64_t reserved2;                       // Reserved
  uint64_t reserved3;                       // Reserved
  uint64_t reserved4;                       // Reserved
  uint64_t reserved5;                       // Reserved
  uint64_t reserved6;                       // Reserved
  uint64_t reserved7;                       // Reserved
  uint64_t reserved8;                       // Reserved
  uint64_t reserved9;                       // Reserved
  uint64_t reserved10;                      // Reserved
  uint64_t reserved11;                      // Reserved
  uint64_t reserved12;                      // Reserved
  uint64_t reserved13;                      // Reserved
  uint64_t reserved14;                      // Reserved
  uint64_t reserved15;                      // Reserved
  uint64_t reserved16;                      // Reserved
  uint64_t reserved17;                      // Reserved
  uint64_t reserved18;                      // Reserved
  uint64_t reserved19;                      // Reserved
  uint64_t reserved20;                      // Reserved
  uint64_t reserved21;                      // Reserved
  int64_t heliumPresureTrip;                // Helium Pressure Threshold Tripped ( 1 - trip, 0 - no trip)
  uint64_t reserved34;                      // Reserved
  uint64_t reserved35;                      // Reserved
  uint64_t reserved36;                      // Reserved
} ATTR_PACKED_FARM;
#pragma pack()
STATIC_ASSERT(sizeof(scsiFarmReliabilityStatistics)== 236);

// Seagate SCSI Field Access Reliability Metrics log (FARM) parameter (read with SCSI LogSense page 0x3D, sub-page 0x3)
// Drive Information Continued
#pragma pack(1)
struct scsiFarmDriveInformation2 {
  scsiFarmParameterHeader parameterHeader;  // Parameter Header
  uint64_t pageNumber;                      // Page Number = 6
  uint64_t copyNumber;                      // Copy Number
  uint64_t depopulationHeadMask;            // Depopulation Head Mask
  uint64_t productID;                       // Product ID [0:3]
  uint64_t productID2;                      // Product ID [4:7]
  uint64_t productID3;                      // Product ID [8:11]
  uint64_t productID4;                      // Product ID [12:15]
  uint64_t driveRecordingType;              // Drive Recording Type - 0 for SMR and 1 for CMR
  uint64_t dpopped;                         // Is drive currently depopped. 1 = depopped, 0 = not depopped
  uint64_t maxNumberForReasign;             // Max Number of Available Sectors for Re-Assignment. Value in disc sectors
  uint64_t timeToReady;                     // Time to Ready of the last power cycle in milliseconds
  uint64_t timeHeld;                        // Time the drive is held in staggered spin in milliseconds
  uint64_t lastServoSpinUpTime;             // The last servo spin up time in milliseconds
} ATTR_PACKED_FARM;
#pragma pack()
STATIC_ASSERT(sizeof(scsiFarmDriveInformation2)== 108);

// Seagate SCSI Field Access Reliability Metrics log (FARM) parameter (read with SCSI LogSense page 0x3D, sub-page 0x3)
// Environment Statistics Continued
#pragma pack(1)
struct scsiFarmEnvironmentStatistics2 {
  scsiFarmParameterHeader parameterHeader;  // Parameter Header
  uint64_t pageNumber;                      // Page Number = 7
  uint64_t copyNumber;                      // Copy Number
  uint64_t current12v;                      // Current 12V input in mV
  uint64_t min12v;                          // Minimum 12V input from last 3 SMART Summary Frames in mV
  uint64_t max12v;                          // Maximum 12V input from last 3 SMART Summary Frames in mV
  uint64_t current5v;                       // Current 5V input in mV
  uint64_t min5v;                           // Minimum 5V input from last 3 SMART Summary Frames in mV
  uint64_t max5v;                           // Maximum 5V input from last 3 SMART Summary Frames in mV
} ATTR_PACKED_FARM;
#pragma pack()
STATIC_ASSERT(sizeof(scsiFarmEnvironmentStatistics2)== 68);

// Seagate SCSI Field Access Reliability Metrics log (FARM) parameter (read with SCSI LogSense page 0x3D, sub-page 0x3)
// "By Head" Parameters
#pragma pack(1)
struct scsiFarmByHead {
  scsiFarmParameterHeader parameterHeader;  // Parameter Header
  uint64_t headValue[20];                   // [16] Head Information
} ATTR_PACKED_FARM;
#pragma pack()
STATIC_ASSERT(sizeof(scsiFarmByHead)==(4 +(20 * 8)));

// Seagate SCSI Field Access Reliability Metrics log (FARM) parameter (read with SCSI LogSense page 0x3D, sub-page 0x3)
// "By Actuator" Parameters
#pragma pack(1)
struct scsiFarmByActuator {
  scsiFarmParameterHeader parameterHeader;      // Parameter Header
  uint64_t pageNumber;                          // Page Number
  uint64_t copyNumber;                          // Copy Number
  uint64_t actuatorID;                          // Actuator ID
  uint64_t headLoadEvents;                      // Head Load Events
  uint64_t reserved;                            // Reserved
  uint64_t reserved0;                           // Reserved
  uint64_t timelastIDDTest;                     // Timestamp of last IDD test
  uint64_t subcommandlastIDDTest;               // Sub-Command of last IDD test
  uint64_t numberGListReclam;                   // Number of G-list reclamations
  uint64_t servoStatus;                         // Servo Status (follows standard DST error code definitions)
  uint64_t numberSlippedSectorsBeforeIDD;       // Number of Slipped Sectors Before IDD Scan
  uint64_t numberSlippedSectorsAfterIDD;        // Number of Slipped Sectors After IDD Scan
  uint64_t numberResidentReallocatedBeforeIDD;  // Number of Resident Reallocated Sectors Before IDD Scan
  uint64_t numberResidentReallocatedAfterIDD;   // Number of Resident Reallocated Sectors After IDD Scan
  uint64_t numberScrubbedSectorsBeforeIDD;      // Number of Successfully Scrubbed Sectors Before IDD Scan
  uint64_t numberScrubbedSectorsAfterIDD;       // Number of Successfully Scrubbed Sectors After IDD Scan
  uint64_t dosScansPerformed;                   // Number of DOS Scans Performed
  uint64_t lbasCorrectedISP;                    // Number of LBAs Corrected by Intermediate Super Parity
  uint64_t numberValidParitySectors;            // Number of Valid Parity Sectors
  uint64_t reserved1;                           // Reserved
  uint64_t reserved2;                           // Reserved
  uint64_t reserved3;                           // Reserved
  uint64_t numberLBACorrectedParitySector;      // Number of LBAs Corrected by Parity Sector
} ATTR_PACKED_FARM;
#pragma pack()
STATIC_ASSERT(sizeof(scsiFarmByActuator)== 188);

// Seagate SCSI Field Access Reliability Metrics log (FARM) parameter (read with SCSI LogSense page 0x3D, sub-page 0x3)
// "By Actuator" Parameters for Flash LED Information
#pragma pack(1)
struct scsiFarmByActuatorFLED {
  scsiFarmParameterHeader parameterHeader;  // Parameter Header
  uint64_t pageNumber;                      // Page Number
  uint64_t copyNumber;                      // Copy Number
  uint64_t actuatorID;                      // Actuator ID
  uint64_t totalFlashLED;                   // Total Flash LED (Assert) Events
  uint64_t indexFlashLED;                   // Index of last entry in Flash LED Info array below, in case the array wraps
  uint64_t flashLEDArray[8];                // Info on the last 8 Flash LED (assert) events wrapping array
  uint64_t universalTimestampFlashLED[8];   // Universal Timestamp (us) of last 8 Flash LED (assert) Events, wrapping array
  uint64_t powerCycleFlashLED[8];           // Power Cycle of the last 8 Flash LED (assert) Events, wrapping array
} ATTR_PACKED_FARM;
#pragma pack()
STATIC_ASSERT(sizeof(scsiFarmByActuatorFLED)== 236);

// Seagate SCSI Field Access Reliability Metrics log (FARM) parameter (read with SCSI LogSense page 0x3D, sub-page 0x3)
// "By Actuator" Parameters for Reallocation Information
#pragma pack(1)
struct scsiFarmByActuatorReallocation {
  scsiFarmParameterHeader parameterHeader;  // Parameter Header
  uint64_t pageNumber;                      // Page Number
  uint64_t copyNumber;                      // Copy Number
  uint64_t actuatorID;                      // Actuator ID
  uint64_t totalReallocations;              // Number of Re-Allocated Sectors
  uint64_t totalReallocationCanidates;      // Number of Re-Allocated Candidate Sectors
  uint64_t reserved[15];                    // Reserved
} ATTR_PACKED_FARM;
#pragma pack()
STATIC_ASSERT(sizeof(scsiFarmByActuatorReallocation)== 164);

// Seagate SCSI Field Access Reliability Metrics log (FARM) all parameters
struct scsiFarmLog {
  scsiFarmPageHeader pageHeader;                         // Head for whole log page
  scsiFarmHeader header;                                 // Log Header parameter
  scsiFarmDriveInformation driveInformation;             // Drive Information parameter
  scsiFarmWorkloadStatistics workload;                   // Workload Statistics parameter
  scsiFarmErrorStatistics error;                         // Error Statistics parameter
  scsiFarmEnvironmentStatistics environment;             // Environment Statistics parameter
  scsiFarmReliabilityStatistics reliability;             // Reliability Statistics parameter
  scsiFarmDriveInformation2 driveInformation2;           // Drive Information parameter continued
  scsiFarmEnvironmentStatistics2 environment2;           // Environment Statistics parameter continued
  scsiFarmByHead reserved;                               // Reserved
  scsiFarmByHead reserved0;                              // Reserved
  scsiFarmByHead reserved1;                              // Reserved
  scsiFarmByHead reserved2;                              // Reserved
  scsiFarmByHead reserved3;                              // Reserved
  scsiFarmByHead reserved4;                              // Reserved
  scsiFarmByHead reserved5;                              // Reserved
  scsiFarmByHead reserved6;                              // Reserved
  scsiFarmByHead reserved7;                              // Reserved
  scsiFarmByHead reserved8;                              // Reserved
  scsiFarmByHead mrHeadResistance;                       // MR Head Resistance from most recent SMART Summary Frame by Head
  scsiFarmByHead reserved9;                              // Reserved
  scsiFarmByHead reserved10;                             // Reserved
  scsiFarmByHead reserved11;                             // Reserved
  scsiFarmByHead reserved12;                             // Reserved
  scsiFarmByHead reserved13;                             // Reserved
  scsiFarmByHead reserved14;                             // Reserved
  scsiFarmByHead totalReallocations;                     // Number of Reallocated Sectors
  scsiFarmByHead totalReallocationCanidates;             // Number of Reallocation Candidate Sectors
  scsiFarmByHead reserved15;                             // Reserved
  scsiFarmByHead reserved16;                             // Reserved
  scsiFarmByHead reserved17;                             // Reserved
  scsiFarmByHead writeWorkloadPowerOnTime;               // Write Workload Power-on Time in Seconds, value from most recent SMART Frame by Head
  scsiFarmByHead reserved18;                             // Reserved
  scsiFarmByHead cumulativeUnrecoverableReadRepeat;      // Cumulative Lifetime Unrecoverable Read Repeat by head
  scsiFarmByHead cumulativeUnrecoverableReadUnique;      // Cumulative Lifetime Unrecoverable Read Unique by head
  scsiFarmByHead reserved19;                             // Reserved
  scsiFarmByHead reserved20;                             // Reserved
  scsiFarmByHead reserved21;                             // Reserved
  scsiFarmByHead reserved22;                             // Reserved
  scsiFarmByHead reserved23;                             // Reserved
  scsiFarmByHead reserved24;                             // Reserved
  scsiFarmByHead reserved25;                             // Reserved
  scsiFarmByHead reserved26;                             // Reserved
  scsiFarmByHead reserved27;                             // Reserved
  scsiFarmByHead secondMRHeadResistance;                 // Second Head MR Head Resistance from most recent SMART Summary Frame by Head
  scsiFarmByHead reserved28;                             // Reserved
  scsiFarmByHead reserved29;                             // Reserved
  scsiFarmByHead reserved30;                             // Reserved
  scsiFarmByHead reserved31;                             // Reserved
  scsiFarmByHead reserved32;                             // Reserved
  scsiFarmByHead reserved33;                             // Reserved
  scsiFarmByHead reserved34;                             // Reserved
  scsiFarmByHead reserved35;                             // Reserved
  scsiFarmByHead reserved36;                             // Reserved
  scsiFarmByHead reserved37;                             // Reserved
  scsiFarmByHead reserved38;                             // Reserved
  scsiFarmByActuator actuator0;                          // Actuator 0 parameters
  scsiFarmByActuatorFLED actuatorFLED0;                  // Actuator 0 FLED Information parameters
  scsiFarmByActuatorReallocation actuatorReallocation0;  // Actuator 0 Reallocation parameters
  scsiFarmByActuator actuator1;                          // Actuator 1 parameters
  scsiFarmByActuatorFLED actuatorFLED1;                  // Actuator 1 FLED Information parameters
  scsiFarmByActuatorReallocation actuatorReallocation1;  // Actuator 1 Reallocation parameters
  scsiFarmByActuator actuator2;                          // Actuator 2 parameters
  scsiFarmByActuatorFLED actuatorFLED2;                  // Actuator 2 FLED Information parameters
  scsiFarmByActuatorReallocation actuatorReallocation2;  // Actuator 2 Reallocation parameters
  scsiFarmByActuator actuator3;                          // Actuator 3 parameters
  scsiFarmByActuatorFLED actuatorFLED3;                  // Actuator 3 FLED Information parameters
  scsiFarmByActuatorReallocation actuatorReallocation3;  // Actuator 3 Reallocation parameters
};
STATIC_ASSERT(sizeof(scsiFarmLog)== 4 + 76 + 252 + 148 + 236 + 212 + 236 + 108 + 68 +(47 *((8 * 20)+ 4))+ 188 * 4 + 236 * 4 + 164 * 4);

/*
 *  Determines whether the current drive is an ATA Seagate drive
 *
 *  @param  drive:  Pointer to drive struct containing ATA device information (*ata_identify_device)
 *  @param  dbentry:  Pointer to struct containing drive database entries (see drivedb.h) (drive_settings*)
 *  @return True if the drive is a Seagate drive, false otherwise (bool)
 */
bool ataIsSeagate(const ata_identify_device& drive, const drive_settings* dbentry);

/*
 *  Reads vendor-specific FARM log (GP Log 0xA6) data from Seagate
 *  drives and parses data into FARM log structures
 *  Returns parsed structure as defined in atacmds.h
 *
 *  @param  device:   Pointer to instantiated device object (ata_device*)
 *  @param  farmLog:  Reference to parsed data in structure(s) with named members (ataFarmLog&)
 *  @param  nsectors: Number of 512-byte sectors in this log (unsigned int)
 *  @return true if read successful, false otherwise (bool)
 */
bool ataReadFarmLog(ata_device* device, ataFarmLog& farmLog, unsigned nsectors);

/*
 *  Determines whether the current drive is a SCSI Seagate drive
 *
 *  @param  scsi_vendor:  Text of SCSI vendor field (char*)
 *  @return True if the drive is a Seagate drive, false otherwise (bool)
 */
bool scsiIsSeagate(char* scsi_vendor);

/*
 *  Reads vendor-specific FARM log (SCSI log page 0x3D, sub-page 0x3) data from Seagate
 *  drives and parses data into FARM log structures
 *  Returns parsed structure as defined in scsicmds.h
 *
 *  @param  device: Pointer to instantiated device object (scsi_device*)
 *  @param  farmLog:  Reference to parsed data in structure(s) with named members (scsiFarmLog&)
 *  @return true if read successful, false otherwise (bool)
 */
bool scsiReadFarmLog(scsi_device* device, scsiFarmLog& farmLog);

#endif
