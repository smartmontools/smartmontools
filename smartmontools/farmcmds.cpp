/*
 * Copyright (c) 2020-2021 Seagate Technology LLC and/or its Affiliates
 *
 * Licensed under the GNU General Public License v2.0 or later
 * SPDX-Licence-Identifier: GPL-2.0-or-later
 * You may obtain a copy of the License at
 *
 *     https://spdx.org/licenses/GPL-2.0-or-later.html
 * 
 * This program is free software; you can redistribute it and/or modify 
 * it under the terms of the GNU General Public License as published by 
 * the Free Software Foundation; either version 2 of the License, or 
 * (at your option) any later version.
 *
 */

#include "farmcmds.h"

#include "atacmds.h"
#include "scsicmds.h"
#include "smartctl.h"

/////////////////////////////////////////////////////////////////////////////////////////
// Seagate ATA Field Access Reliability Metrics (FARM) log (Log 0xA6)

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
bool ataReadFarmLog(ata_device* device, ataFarmLog& farmLog, unsigned nsectors) {
  // Set up constants for FARM log
  const size_t FARM_PAGE_SIZE = 16384;
  const size_t FARM_ATTRIBUTE_SIZE = 8;
  const size_t FARM_MAX_PAGES = 6;
  const size_t FARM_SECTOR_SIZE = FARM_PAGE_SIZE * FARM_MAX_PAGES / nsectors;
  const unsigned FARM_SECTORS_PER_PAGE = nsectors / FARM_MAX_PAGES;
  const size_t FARM_CURRENT_PAGE_DATA_SIZE[FARM_MAX_PAGES] = {
      sizeof(ataFarmHeader),
      sizeof(ataFarmDriveInformation),
      sizeof(ataFarmWorkloadStatistics),
      sizeof(ataFarmErrorStatistics),
      sizeof(ataFarmEnvironmentStatistics),
      sizeof(ataFarmReliabilityStatistics)};
  memset(&farmLog, 0, sizeof(farmLog));
  unsigned numSectorsToRead = (sizeof(ataFarmHeader) / FARM_SECTOR_SIZE) + 1;
  // Go through each of the six pages of the FARM log
  for (unsigned page = 0; page < FARM_MAX_PAGES; page++) {
    // Reset the buffer
    uint8_t pageBuffer[FARM_PAGE_SIZE] = {};
    // Reset the current FARM log page
    uint64_t currentFarmLogPage[FARM_PAGE_SIZE / FARM_ATTRIBUTE_SIZE] = {};
    // Read the desired quantity of sectors from the current page into the buffer
    bool readSuccessful = ataReadLogExt(device, 0xA6, 0, page * FARM_SECTORS_PER_PAGE, pageBuffer, numSectorsToRead);
    if (!readSuccessful) {
      jerr("Read FARM Log page %u failed\n\n", page);
      return false;
    }
    // Read the page from the buffer, one attribute (8 bytes) at a time
    for (unsigned pageOffset = 0; pageOffset < FARM_CURRENT_PAGE_DATA_SIZE[page]; pageOffset += FARM_ATTRIBUTE_SIZE) {
      uint64_t currentMetric = 0;
      // Read each attribute from the buffer, one byte at a time (combine 8 bytes, little endian)
      for (unsigned byteOffset = 0; byteOffset < FARM_ATTRIBUTE_SIZE - 1; byteOffset++) {
        currentMetric |= (uint64_t)(pageBuffer[pageOffset + byteOffset] | (pageBuffer[pageOffset + byteOffset + 1] << 8)) << (byteOffset * 8);
      }
      // Check the status bit and strip it off if necessary
      if (currentMetric >> 56 == 0xC0) {
        currentMetric &= 0x00FFFFFFFFFFFFFFULL;
      } else {
        currentMetric = 0;
      }
      // Page 0 is the log header, so check the log signature to verify this is a FARM log
      if (page == 0 && pageOffset == 0) {
        if (currentMetric != 0x00004641524D4552) {
          jerr("FARM log header is invalid (log signature=%lu)\n\n", currentMetric);
          return false;
        }
      }
      // Store value in structure for access to log by metric name
      currentFarmLogPage[pageOffset / FARM_ATTRIBUTE_SIZE] = currentMetric;
    }
    // Copies array values to struct for named member access
    // Check number of sectors to read for next page
    switch (page) {
      case 0:
        memcpy(&farmLog.header, currentFarmLogPage, sizeof(farmLog.header));
        numSectorsToRead = (sizeof(ataFarmDriveInformation) / FARM_SECTOR_SIZE) + 1;
        break;
      case 1:
        memcpy(&farmLog.driveInformation, currentFarmLogPage, sizeof(farmLog.driveInformation));
        numSectorsToRead = (sizeof(ataFarmWorkloadStatistics) / FARM_SECTOR_SIZE) + 1;
        break;
      case 2:
        memcpy(&farmLog.workload, currentFarmLogPage, sizeof(farmLog.workload));
        numSectorsToRead = (sizeof(ataFarmErrorStatistics) / FARM_SECTOR_SIZE) + 1;
        break;
      case 3:
        memcpy(&farmLog.error, currentFarmLogPage, sizeof(farmLog.error));
        numSectorsToRead = (sizeof(ataFarmEnvironmentStatistics) / FARM_SECTOR_SIZE) + 1;
        break;
      case 4:
        memcpy(&farmLog.environment, currentFarmLogPage, sizeof(farmLog.environment));
        numSectorsToRead = (sizeof(ataFarmReliabilityStatistics) / FARM_SECTOR_SIZE) + 1;
        break;
      case 5:
        memcpy(&farmLog.reliability, currentFarmLogPage, sizeof(farmLog.reliability));
        break;
    }
  }
  return true;
}

/////////////////////////////////////////////////////////////////////////////////////////
// Seagate SCSI Field Access Reliability Metrics (FARM) log (Log page 0x3D, sub-page 0x3)

/*
 *  Reads vendor-specific FARM log (SCSI log page 0x3D, sub-page 0x3) data from Seagate
 *  drives and parses data into FARM log structures
 *  Returns parsed structure as defined in scsicmds.h
 *  
 *  @param  device: Pointer to instantiated device object (scsi_device*)
 *  @param  farmLog:  Reference to parsed data in structure(s) with named members (scsiFarmLog&)
 *  @return true if read successful, false otherwise (bool)
 */
bool scsiReadFarmLog(scsi_device* device, scsiFarmLog& farmLog) {
  const uint32_t LOG_RESP_LONG_LEN = ((62 * 256) + 252);
  const uint32_t GBUF_SIZE = 65532;
  uint8_t gBuf[GBUF_SIZE];
  const size_t FARM_ATTRIBUTE_SIZE = 8;
  memset(&farmLog, 0, sizeof(farmLog));
  if (0 != scsiLogSense(device, SEAGATE_FARM_LPAGE, SEAGATE_FARM_CURRENT_L_SPAGE, gBuf, LOG_RESP_LONG_LEN, 0)) {
    jerr("Read FARM Log page failed\n\n");
    return false;
  }
  bool isParameterHeader = true;
  // Log page header
  farmLog.pageHeader.pageCode = gBuf[0];
  farmLog.pageHeader.subpageCode = gBuf[1];
  farmLog.pageHeader.pageLength = gBuf[2] << 8 | gBuf[3];
  // Get rest of log
  // Holds data for each SCSI parameter
  u_int64_t currentParameter[sizeof(gBuf) / FARM_ATTRIBUTE_SIZE] = {};
  // Track index of current metric within each parameter
  unsigned currentMetricIndex = 0;
  // Track offset (in struct) of current SCSI parameter
  unsigned currentParameterOffset = 0;
  // Track offset in struct
  scsiFarmParameterHeader currentParameterHeader;
  for (unsigned pageOffset = sizeof(scsiFarmPageHeader); pageOffset < (farmLog.pageHeader.pageLength + sizeof(scsiFarmPageHeader)); pageOffset += FARM_ATTRIBUTE_SIZE) {
    // First get parameter header
    if (isParameterHeader) {
      // Clear old header
      currentParameterHeader = scsiFarmParameterHeader();
      // Set parameter values
      currentParameterHeader.parameterCode = (gBuf[pageOffset] << 8) | gBuf[pageOffset + 1];
      currentParameterHeader.parameterControl = gBuf[pageOffset + 2];
      currentParameterHeader.parameterLength = gBuf[pageOffset + 3];
      // Add offset to struct based on current parameter code
      currentParameterOffset = sizeof(scsiFarmPageHeader);
      if (currentParameterHeader.parameterCode >= 0x1) {
        currentParameterOffset += sizeof(scsiFarmHeader);
      }
      if (currentParameterHeader.parameterCode >= 0x2) {
        currentParameterOffset += sizeof(scsiFarmDriveInformation);
      }
      if (currentParameterHeader.parameterCode >= 0x3) {
        currentParameterOffset += sizeof(scsiFarmWorkloadStatistics);
      }
      if (currentParameterHeader.parameterCode >= 0x4) {
        currentParameterOffset += sizeof(scsiFarmErrorStatistics);
      }
      if (currentParameterHeader.parameterCode >= 0x5) {
        currentParameterOffset += sizeof(scsiFarmEnvironmentStatistics);
      }
      if (currentParameterHeader.parameterCode >= 0x6) {
        currentParameterOffset += sizeof(scsiFarmReliabilityStatistics);
      }
      if (currentParameterHeader.parameterCode >= 0x7) {
        currentParameterOffset += sizeof(scsiFarmDriveInformation2);
      }
      if (currentParameterHeader.parameterCode >= 0x8) {
        currentParameterOffset += sizeof(scsiFarmEnvironmentStatistics2);
      }
      // Skip "By Head" sections that are not present
      if (currentParameterHeader.parameterCode >= 0x11 && currentParameterHeader.parameterCode <= 0x29) {
        currentParameterOffset += sizeof(scsiFarmByHead) * (currentParameterHeader.parameterCode - 0x10);
      } else if (currentParameterHeader.parameterCode >= 0x30 && currentParameterHeader.parameterCode <= 0x35) {
        currentParameterOffset += sizeof(scsiFarmByHead) * ((0x2A - 0x10) + (currentParameterHeader.parameterCode - 0x30));
      } else if (currentParameterHeader.parameterCode >= 0x40 && currentParameterHeader.parameterCode <= 0x4E) {
        currentParameterOffset += sizeof(scsiFarmByHead) * ((0x2A - 0x10) + (0x36 - 0x30) + (currentParameterHeader.parameterCode - 0x40));
      } else if (currentParameterHeader.parameterCode >= 0x50) {
        currentParameterOffset += sizeof(scsiFarmByHead) * ((0x2A - 0x10) + (0x36 - 0x30) + (0x4F - 0x40));
      }
      if (currentParameterHeader.parameterCode >= 0x51) {
        currentParameterOffset += sizeof(scsiFarmByActuator);
      }
      if (currentParameterHeader.parameterCode >= 0x52) {
        currentParameterOffset += sizeof(scsiFarmByActuatorFLED);
      }
      if (currentParameterHeader.parameterCode >= 0x53) {
        currentParameterOffset += sizeof(scsiFarmByActuatorReallocation);
      }
      if (currentParameterHeader.parameterCode >= 0x61) {
        currentParameterOffset += sizeof(scsiFarmByActuator);
      }
      if (currentParameterHeader.parameterCode >= 0x62) {
        currentParameterOffset += sizeof(scsiFarmByActuatorFLED);
      }
      if (currentParameterHeader.parameterCode >= 0x63) {
        currentParameterOffset += sizeof(scsiFarmByActuatorReallocation);
      }
      if (currentParameterHeader.parameterCode >= 0x71) {
        currentParameterOffset += sizeof(scsiFarmByActuator);
      }
      if (currentParameterHeader.parameterCode >= 0x72) {
        currentParameterOffset += sizeof(scsiFarmByActuatorFLED);
      }
      if (currentParameterHeader.parameterCode >= 0x73) {
        currentParameterOffset += sizeof(scsiFarmByActuatorReallocation);
      }
      if (currentParameterHeader.parameterCode >= 0x81) {
        currentParameterOffset += sizeof(scsiFarmByActuator);
      }
      if (currentParameterHeader.parameterCode >= 0x82) {
        currentParameterOffset += sizeof(scsiFarmByActuatorFLED);
      }
      if (currentParameterHeader.parameterCode >= 0x83) {
        currentParameterOffset += sizeof(scsiFarmByActuatorReallocation);
      }
      // Copy parameter header to struct
      memcpy((char*) &farmLog + currentParameterOffset, &currentParameterHeader, sizeof(scsiFarmParameterHeader));
      // Fix offset
      pageOffset += sizeof(scsiFarmParameterHeader);
      // No longer setting header
      isParameterHeader = false;
      currentMetricIndex = 0;
    }
    uint64_t currentMetric = 0;
    // Read each attribute from the buffer, one byte at a time (combine 8 bytes, big endian)
    for (unsigned byteOffset = 0; byteOffset < FARM_ATTRIBUTE_SIZE; byteOffset++) {
      currentMetric |= (uint64_t)(gBuf[pageOffset + byteOffset]) << ((7 - byteOffset) * 8);
    }
    // Check the status bit and strip it off if necessary
    if (currentMetric >> 56 == 0xC0) {
      currentMetric &= 0x00FFFFFFFFFFFFFFULL;
      // Parameter 0 is the log header, so check the log signature to verify this is a FARM log
      if (pageOffset == sizeof(scsiFarmPageHeader)) {
        if (currentMetric != 0x00004641524D4552) {
          jerr("FARM log header is invalid (log signature=%lu)\n\n", currentMetric);
          return false;
        }
      }
      // If a parameter header is reached, set the values
    } else if (currentParameterHeader.parameterLength <= currentMetricIndex * FARM_ATTRIBUTE_SIZE) {
      // Apply header for NEXT parameter
      isParameterHeader = true;
      pageOffset -= FARM_ATTRIBUTE_SIZE;
      // Copy data for CURRENT parameter to struct (skip parameter header which has already been assigned)
      memcpy((char*) &farmLog + currentParameterOffset + sizeof(scsiFarmParameterHeader), currentParameter, currentParameterHeader.parameterLength);
      continue;
    } else {
      currentMetric = 0;
    }
    currentParameter[currentMetricIndex] = currentMetric;
    currentMetricIndex++;
  }
  return true;
}
