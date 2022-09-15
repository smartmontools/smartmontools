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

#define __STDC_FORMAT_MACROS 1
#include <string>
#include <inttypes.h>
#include "farmprint.h"
#include <initializer_list>
#include "smartctl.h"

/*
 *  Get World Wide Name (WWN) fields from FARM.
 *  Stored in hex format in log pag 1 byte offset 32-47. (Seagate FARM Spec Rev 4.23.1)
 *  
 *  @param  wwn1:  Constant 64-bit integer containing the high 8 bytes of WWN (const uint64_t)
 *  @param  wwn2:  Constant 64-bit integer containing the low 8 bytes of WWN (const uint64_t)
 *  @param  oui:  Reference to OUI field of WWN (unsigned &)
 *  @param  unique_id:  Reference to unique ID field of WWN (uint64_t &)
 *  @return integer containing naa field of WWN
 */
int ata_farm_get_wwn(const uint64_t wwn1, const uint64_t wwn2, unsigned & oui, uint64_t & unique_id) {
  int naa   = ( wwn1 & 0xF000) >> 12;
  oui       = ((wwn1 & 0xFFF) << 12) |
              ((wwn1 & (0xFFF << 20)) >> 20);
  unique_id = ((wwn1 & 0xF0000) << 16) |
              ((wwn2 & 0x0000FFFF) << 16) |
              ((wwn2 & 0xFFFF0000) >> 16);
   return naa;
}

/*
 *  Get the recording type descriptor from FARM.
 *  Stored as bitmask in log pag 1 byte offset 336-343. (Seagate FARM Spec Rev 4.23.1)
 *  
 *  @param  driveRecordingType: Constant 64-bit integer recording type descriptor (const uint64_t)
 *  @return:  Constant reference to string literal (const char *)
 */
const char * ata_farm_get_recording_type(const uint64_t driveRecordingType) {
  switch (driveRecordingType & 0x3) {
    case 0x1: return "SMR";
    case 0x2: return "CMR";
    default : return "UNKNOWN";
  }
}

/*
 *  Get the deveice interface type from FARM log.
 *  Stored as ASCII text in log page 1 byte offset 48-55. (Seagate FARM Spec Rev 4.23.1)
 *  
 *  @param  buffer:  Reference to character buffer (char *)
 *  @param  deviceInterface:  Constant 64-bit integer representing ASCII description of the device interface (const uint64_t)
 *  @return reference to char buffer containing a null-terminated string
 */
char * ata_farm_get_interface(char * buffer, const uint64_t deviceInterface) {
  for (uint8_t i = 0; i < 4; i++){ buffer[i] = (deviceInterface >> ((4 - i - 1) * 8)) & 0xFF; }
  buffer[4] = '\0';
  return buffer;
}

/*
 *  Output the 64-bit integer value of a FARM parameter by head in plain text format
 *  
 *  @param  desc:  Description of the parameter (const char *)
 *  @param  paramArray:  Reference to int64_t array containing paramter values for each head (const int64_t *)
 *  @param  numHeads:  Constant 64-bit integer representing ASCII description of the device interface (const uint64_t)
 */
void ata_farm_by_head_field_to_text(const char * desc, const int64_t * paramArray, const uint64_t numHeads) {
  for (uint8_t hd = 0; hd < numHeads; hd++) { 
     jout("\t\t%s %" PRIu8 ": %" PRIu64 "\n", desc, hd, paramArray[hd]);
  }
}

/*
 *  Add the 64-bit integer value of a FARM parameter by head to json element
 *  
 *  @param  jref:  Reference to a JSON element
 *  @param  buffer:  Reference to character buffer (char *)
 *  @param  desc:  Description of the parameter (const char *)
 *  @param  paramArray:  Reference to int64_t array containing paramter values for each head (const int64_t *)
 *  @param  numHeads:  Constant 64-bit integer representing ASCII description of the device interface (const uint64_t)
 */
void ata_farm_by_head_field_to_json(json::ref jref, char * buffer, const char * desc, const int64_t * paramArray, const uint64_t numHeads) {
  for (uint8_t hd = 0; hd < numHeads; hd++){
     sprintf(buffer, "%s_%" PRIu8, desc, hd);
     jref[buffer] = paramArray[hd];
  }
}

/*
 *  Wrapper function to use ata_format_id_string() for formatting
 *  a string from an 8-byte FARM field.
 *  
 *  @param  buffer:  Constant reference to character buffer (char *)
 *  @param  param1:  Constant 64-bit integer containing the FARM field information (const uint64_t)
 *  @return reference to char buffer containing a null-terminated string
 */
char * ata_farm_format_id_string(char * buffer, const uint64_t param) {
  ata_format_id_string(buffer, (const unsigned char *)&param, sizeof(param));
  return buffer;
}

/*
 *  Overload function to format and concat two 8-byte FARM fields.
 *  
 *  @param  buffer:  Constant reference to character buffer (char *)
 *  @param  param1:  Constant 64-bit integer containing the high 8 bytes of the FARM field (const uint64_t)
 *  @param  param2:  Constant 64-bit integer containing the low 8 bytes of the FARM field (const uint64_t)
 *  @return reference to char buffer containing a null-terminated string
 */
char * ata_farm_format_id_string(char * buffer, const uint64_t param1, const uint64_t param2) {
  ata_format_id_string(buffer, (const unsigned char *)&param1, sizeof(param1));
  ata_format_id_string(&buffer[strlen(buffer)], (const unsigned char *)&param2, sizeof(param2));
  return buffer;
}

/////////////////////////////////////////////////////////////////////////////////////////
// Seagate ATA Field Access Reliability Metrics (FARM) log (Log 0xA6)

/*
 *  Prints parsed FARM log (GP Log 0xA6) data from Seagate
 *  drives already present in ataFarmLogFrame structure
 *  
 *  @param  farmLog:  Constant reference to parsed farm log (const ataFarmLog&)
 */
void ataPrintFarmLog(const ataFarmLog& farmLog) {
   char buffer[128]; // Generic character buffer

   // Get device information
   char serialNumber[sizeof(farmLog.driveInformation.serialNumber)+sizeof(farmLog.driveInformation.serialNumber2)+1];
   ata_farm_format_id_string(serialNumber, farmLog.driveInformation.serialNumber, farmLog.driveInformation.serialNumber2);

  unsigned oui = 0; uint64_t unique_id = 0; 
  char worldWideName[sizeof(farmLog.driveInformation.worldWideName)+sizeof(farmLog.driveInformation.worldWideName2)+3];
  int naa = ata_farm_get_wwn(farmLog.driveInformation.worldWideName, farmLog.driveInformation.worldWideName2, oui, unique_id);
  sprintf(worldWideName, "%x %06x %09" PRIx64, naa, oui, unique_id);

  char deviceInterface[sizeof(farmLog.driveInformation.deviceInterface)];
  ata_farm_get_interface(deviceInterface, farmLog.driveInformation.deviceInterface);

  char capacityInBytes[64], capacityFormatted[32];
  format_with_thousands_sep(capacityInBytes, sizeof(capacityInBytes), farmLog.driveInformation.deviceCapacity * farmLog.driveInformation.lsecSize);
  format_capacity(capacityFormatted, sizeof(capacityFormatted), farmLog.driveInformation.deviceCapacity * farmLog.driveInformation.lsecSize);

  char bufferInBytes[64], bufferFormatted[32];
  format_with_thousands_sep(bufferInBytes, sizeof(bufferInBytes), farmLog.driveInformation.deviceBufferSize);
  format_capacity(bufferFormatted, sizeof(bufferFormatted), farmLog.driveInformation.deviceBufferSize);

  const char * formFactor = ata_get_form_factor((unsigned short)farmLog.driveInformation.factor);

  char firmwareRev[sizeof(farmLog.driveInformation.firmwareRev)+sizeof(farmLog.driveInformation.firmwareRev2)+1];
  ata_farm_format_id_string(firmwareRev, farmLog.driveInformation.firmwareRev, farmLog.driveInformation.firmwareRev2);

  char modelNumber[sizeof(farmLog.driveInformation.modelNumber)+1];
  for (uint8_t i = 0; i < sizeof(farmLog.driveInformation.modelNumber)/sizeof(farmLog.driveInformation.modelNumber[0]); i++){
     ata_farm_format_id_string(&modelNumber[strlen(modelNumber)], farmLog.driveInformation.modelNumber[i]);
  }

  const char * recordingType = ata_farm_get_recording_type(farmLog.driveInformation.driveRecordingType);

  char dateOfAssembly[sizeof(farmLog.driveInformation.dateOfAssembly)];
  ata_farm_format_id_string(dateOfAssembly, farmLog.driveInformation.dateOfAssembly);

  // Print plain-text
  jout("\nSeagate Field Access Reliability Metrics log (FARM) (GP log 0xA6)\n");
  // Page 0: Log Header
  jout("\tFARM Log Page 0: Log Header\n");
  jout("\t\tFARM Log Version: %" PRIu64 ".%" PRIu64 "\n", farmLog.header.majorRev, farmLog.header.minorRev);
  jout("\t\tPages Supported: %" PRIu64 "\n", farmLog.header.pagesSupported);
  jout("\t\tLog Size: %" PRIu64 "\n", farmLog.header.logSize);
  jout("\t\tPage Size: %" PRIu64 "\n", farmLog.header.pageSize);
  jout("\t\tHeads Supported: %" PRIu64 "\n", farmLog.header.headsSupported);
  jout("\t\tNumber of Copies: %" PRIu64 "\n", farmLog.header.copies);
  jout("\t\tReason for Frame Capture: %" PRIu64 "\n", farmLog.header.frameCapture);

  // Page 1: Drive Information
  jout("\tFARM Log Page 1: Drive Information\n");
  jout("\t\tSerial Number: %s\n", serialNumber);
  jout("\t\tWorld Wide Name: %s\n", worldWideName);
  jout("\t\tDevice Interface: %s\n", deviceInterface);
  jout("\t\tDevice Capacity in Bytes: %s [%s]\n", capacityInBytes, capacityFormatted);
  jout("\t\tPhysical Sector Size: %" PRIu64 " bytes\n", farmLog.driveInformation.psecSize);
  jout("\t\tLogical Sector Size: %" PRIu64 " bytes\n", farmLog.driveInformation.lsecSize);
  jout("\t\tDevice Buffer Size: %s [%s]\n", bufferInBytes, bufferFormatted);
  jout("\t\tNumber of Heads: %" PRIu64 "\n", farmLog.driveInformation.heads);
  jout("\t\tDevice Form Factor: %s\n", formFactor);
  jout("\t\tRotation Rate: %" PRIu64 " rpm\n", farmLog.driveInformation.rotationRate);
  jout("\t\tFirmware Rev: %s\n", firmwareRev);
  jout("\t\tATA Security State (ID Word 128): 0x016%" PRIx64 "\n", farmLog.driveInformation.security);
  jout("\t\tATA Features Supported (ID Word 78): 0x016%" PRIx64 "\n", farmLog.driveInformation.featuresSupported);
  jout("\t\tATA Features Enabled (ID Word 79): 0x%016" PRIx64 "\n", farmLog.driveInformation.featuresEnabled);
  jout("\t\tPower-On Hours: %" PRIu64 "\n", farmLog.driveInformation.poh);
  jout("\t\tSpindle Power-On Hours: %" PRIu64 "\n", farmLog.driveInformation.spoh);
  jout("\t\tHead Flight Hours: %" PRIu64 "\n", farmLog.driveInformation.headFlightHours);
  jout("\t\tHead Load Events: %" PRIu64 "\n", farmLog.driveInformation.headLoadEvents);
  jout("\t\tPower Cycle Count: %" PRIu64 "\n", farmLog.driveInformation.powerCycleCount);
  jout("\t\tHardware Reset Count: %" PRIu64 "\n", farmLog.driveInformation.resetCount);
  jout("\t\tSpin-Up Time: %" PRIu64 " ms\n", farmLog.driveInformation.spinUpTime);
  jout("\t\tTime to ready of the last power cycle: %" PRIu64 " ms\n", farmLog.driveInformation.timeToReady);
  jout("\t\tTime drive is held in staggered spin: %" PRIu64 " ms\n", farmLog.driveInformation.timeHeld);
  jout("\t\tModel Number: %s\n", modelNumber);
  jout("\t\tRecording Type: %s\n", recordingType);
  jout("\t\tMax Number of Available Sectors for Reassignment: %" PRIu64 "\n", farmLog.driveInformation.maxNumberForReasign);
  jout("\t\tAssembly Date (YYWW): %s\n", dateOfAssembly);
  jout("\t\tDepopulation Head Mask: %" PRIx64 "\n", farmLog.driveInformation.depopulationHeadMask);

  // Page 2: Workload Statistics
  jout("\tFARM Log Page 2: Workload Statistics\n");  
  jout("\t\tTotal Number of Read Commands: %" PRIu64 "\n", farmLog.workload.totalReadCommands);
  jout("\t\tTotal Number of Write Commands: %" PRIu64 "\n", farmLog.workload.totalWriteCommands);
  jout("\t\tTotal Number of Random Read Commands: %" PRIu64 "\n", farmLog.workload.totalRandomReads);
  jout("\t\tTotal Number of Random Write Commands: %" PRIu64 "\n", farmLog.workload.totalRandomWrites);
  jout("\t\tTotal Number Of Other Commands: %" PRIu64 "\n", farmLog.workload.totalNumberofOtherCMDS);
  jout("\t\tLogical Sectors Written: %" PRIu64 "\n", farmLog.workload.logicalSecWritten);
  jout("\t\tLogical Sectors Read: %" PRIu64 "\n", farmLog.workload.logicalSecRead);
  jout("\t\tNumber of dither events during current power cycle: %" PRIu64 "\n", farmLog.workload.dither);
  jout("\t\tNumber of times dither was held off during random workloads: %" PRIu64 "\n", farmLog.workload.ditherRandom);
  jout("\t\tNumber of times dither was held off during sequential workloads: %" PRIu64 "\n", farmLog.workload.ditherSequential);
  jout("\t\tNumber of Read commands from 0-3.125%% of LBA space for last 3 SMART Summary Frames: %" PRIu64 "\n", farmLog.workload.readCommandsByRadius1);
  jout("\t\tNumber of Read commands from 3.125-25%% of LBA space for last 3 SMART Summary Frames: %" PRIu64 "\n", farmLog.workload.readCommandsByRadius2);
  jout("\t\tNumber of Read commands from 25-75%% of LBA space for last 3 SMART Summary Frames: %" PRIu64 "\n", farmLog.workload.readCommandsByRadius3);
  jout("\t\tNumber of Read commands from 75-100%% of LBA space for last 3 SMART Summary Frames: %" PRIu64 "\n", farmLog.workload.readCommandsByRadius4);
  jout("\t\tNumber of Write commands from 0-3.125%% of LBA space for last 3 SMART Summary Frames: %" PRIu64 "\n", farmLog.workload.writeCommandsByRadius1);
  jout("\t\tNumber of Write commands from 3.125-25%% of LBA space for last 3 SMART Summary Frames: %" PRIu64 "\n", farmLog.workload.writeCommandsByRadius2);
  jout("\t\tNumber of Write commands from 25-75%% of LBA space for last 3 SMART Summary Frames: %" PRIu64 "\n", farmLog.workload.writeCommandsByRadius3);
  jout("\t\tNumber of Write commands from 75-100%% of LBA space for last 3 SMART Summary Frames: %" PRIu64 "\n", farmLog.workload.writeCommandsByRadius4);

  // Page 3: Error Statistics
  jout("\tFARM Log Page 3: Error Statistics\n");
  jout("\t\tUnrecoverable Read Errors: %" PRIu64 "\n", farmLog.error.totalUnrecoverableReadErrors);
  jout("\t\tUnrecoverable Write Errors: %" PRIu64 "\n", farmLog.error.totalUnrecoverableWriteErrors);
  jout("\t\tNumber of Reallocated Sectors: %" PRIu64 "\n", farmLog.error.totalReallocations);
  jout("\t\tNumber of Read Recovery Attempts: %" PRIu64 "\n", farmLog.error.totalReadRecoveryAttepts);
  jout("\t\tNumber of Mechanical Start Failures: %" PRIu64 "\n", farmLog.error.totalMechanicalStartRetries);
  jout("\t\tNumber of Reallocated Candidate Sectors: %" PRIu64 "\n", farmLog.error.totalReallocationCanidates);
  jout("\t\tNumber of ASR Events: %" PRIu64 "\n", farmLog.error.totalASREvents);
  jout("\t\tNumber of Interface CRC Errors: %" PRIu64 "\n", farmLog.error.totalCRCErrors);
  jout("\t\tSpin Retry Count: %" PRIu64 "\n", farmLog.error.attrSpinRetryCount);
  jout("\t\tSpin Retry Count Normalized: %" PRIu64 "\n", farmLog.error.normalSpinRetryCount);
  jout("\t\tSpin Retry Count Worst: %" PRIu64 "\n", farmLog.error.worstSpinRretryCount);
  jout("\t\tNumber of IOEDC Errors (Raw): %" PRIu64 "\n", farmLog.error.attrIOEDCErrors);
  jout("\t\tCTO Count Total: %" PRIu64 "\n", farmLog.error.attrCTOCount);
  jout("\t\tCTO Count Over 5s: %" PRIu64 "\n", farmLog.error.overFiveSecCTO);
  jout("\t\tCTO Count Over 7.5s: %" PRIu64 "\n", farmLog.error.overSevenSecCTO);

  // Page 3 flash-LED information
  uint8_t index; 
  size_t flash_led_array_size = sizeof(farmLog.error.flashLEDArray) / sizeof(farmLog.error.flashLEDArray[0]);
  jout("\t\tTotal Flash LED (Assert) Events: %" PRIu64 "\n", farmLog.error.totalFlashLED);
  jout("\t\tIndex of the last Flash LED: %" PRIu64 "\n", farmLog.error.indexFlashLED);
  for ( uint8_t i = flash_led_array_size; i > 0; i-- ) {
     index = (i - farmLog.error.indexFlashLED + flash_led_array_size) % flash_led_array_size;
     jout("\t\tFlash LED Event %" PRIu64 ":\n", flash_led_array_size - i);
     jout("\t\t\tEvent Information: 0x%016" PRIx64 "\n", farmLog.error.flashLEDArray[index]);
     jout("\t\t\tTimestamp of Event %" PRIu64 " (hours): %" PRIu64 "\n", flash_led_array_size - i, farmLog.error.universalTimestampFlashLED[index]);
     jout("\t\t\tPower Cycle Event %" PRIu64 ": %" PRIx64 "\n", flash_led_array_size - i, farmLog.error.powerCycleFlashLED[index]);
  }

  // Page 3 unrecoverable errors by-head
  jout("\t\tUncorrectable errors: %" PRIu64 "\n", farmLog.error.uncorrectables);
  jout("\t\tCumulative Lifetime Unrecoverable Read errors due to ERC: %" PRIu64 "\n", farmLog.error.cumulativeUnrecoverableReadERC);
  for (uint8_t hd = 0; hd < farmLog.driveInformation.heads; hd++) {
     jout("\t\tCum Lifetime Unrecoverable by head %" PRIu8 ":\n", hd);
     jout("\t\t\tCumulative Lifetime Unrecoverable Read Repeating: %" PRIu64 "\n", farmLog.error.cumulativeUnrecoverableReadRepeating[hd]);
     jout("\t\t\tCumulative Lifetime Unrecoverable Read Unique: %" PRIu64 "\n", farmLog.error.cumulativeUnrecoverableReadUnique[hd]);
  }

  // Page 4: Environment Statistics
  jout("\tFARM Log Page 4: Environment Statistics\n");
  jout("\t\tCurrent Temperature (Celsius): %" PRIu64 "\n", farmLog.environment.curentTemp);
  jout("\t\tHighest Temperature: %" PRIu64 "\n", farmLog.environment.highestTemp);
  jout("\t\tLowest Temperature: %" PRIu64 "\n", farmLog.environment.lowestTemp);
  jout("\t\tAverage Short Term Temperature: %" PRIu64 "\n", farmLog.environment.averageTemp);
  jout("\t\tAverage Long Term Temperature: %" PRIu64 "\n", farmLog.environment.averageLongTemp);
  jout("\t\tHighest Average Short Term Temperature: %" PRIu64 "\n", farmLog.environment.highestShortTemp);
  jout("\t\tLowest Average Short Term Temperature: %" PRIu64 "\n", farmLog.environment.lowestShortTemp);
  jout("\t\tHighest Average Long Term Temperature: %" PRIu64 "\n", farmLog.environment.highestLongTemp);
  jout("\t\tLowest Average Long Term Temperature: %" PRIu64 "\n", farmLog.environment.lowestLongTemp);
  jout("\t\tTime In Over Temperature (minutes): %" PRIu64 "\n", farmLog.environment.overTempTime);
  jout("\t\tTime In Under Temperature (minutes): %" PRIu64 "\n", farmLog.environment.underTempTime);
  jout("\t\tSpecified Max Operating Temperature: %" PRIu64 "\n", farmLog.environment.maxTemp);
  jout("\t\tSpecified Min Operating Temperature: %" PRIu64 "\n", farmLog.environment.minTemp);
  jout("\t\tCurrent Relative Humidity: %" PRIu64 "\n", farmLog.environment.humidity);
  jout("\t\tCurrent Motor Power: %" PRIu64 "\n", farmLog.environment.currentMotorPower);
  jout("\t\tCurrent 12 volts: %0.3f\n", farmLog.environment.current12v / 1000.0);
  jout("\t\tMinimum 12 volts: %0.3f\n", farmLog.environment.min12v / 1000.0);
  jout("\t\tMaximum 12 volts: %0.3f\n", farmLog.environment.max12v / 1000.0);
  jout("\t\tCurrent 5 volts: %0.3f\n", farmLog.environment.current5v / 1000.0);
  jout("\t\tMinimum 5 volts: %0.3f\n", farmLog.environment.min5v / 1000.0);
  jout("\t\tMaximum 5 volts: %0.3f\n", farmLog.environment.max5v / 1000.0);
  jout("\t\t12V Power Average: %0.3f\n", farmLog.environment.powerAverage12v / 1000.0);
  jout("\t\t12V Power Minimum: %0.3f\n", farmLog.environment.powerMin12v / 1000.0);
  jout("\t\t12V Power Maximum: %0.3f\n", farmLog.environment.powerMax12v / 1000.0);
  jout("\t\t5V Power Average: %0.3f\n", farmLog.environment.powerAverage5v / 1000.0);
  jout("\t\t5V Power Minimum: %0.3f\n", farmLog.environment.powerMin5v / 1000.0);
  jout("\t\t5V Power Maximum: %0.3f\n", farmLog.environment.powerMax5v / 1000.0);

  // Page 5: Reliability Statistics
  jout("\tFARM Log Page 5: Reliability Statistics\n");
  jout("\t\tError Rate (SMART Attribute 1 Raw): 0x%016" PRIx64 "\n", farmLog.reliability.attrErrorRateRaw);
  jout("\t\tError Rate (SMART Attribute 1 Normalized): %" PRIi64 "\n", farmLog.reliability.attrErrorRateNormal);
  jout("\t\tError Rate (SMART Attribute 1 Worst): %" PRIi64 "\n", farmLog.reliability.attrErrorRateWorst);
  jout("\t\tSeek Error Rate (SMART Attr 7 Raw): 0x%016" PRIx64 "\n", farmLog.reliability.attrSeekErrorRateRaw);
  jout("\t\tSeek Error Rate (SMART Attr 7 Normalized): %" PRIi64 "\n", farmLog.reliability.attrSeekErrorRateNormal);
  jout("\t\tSeek Error Rate (SMART Attr 7 Worst): %" PRIi64 "\n", farmLog.reliability.attrSeekErrorRateWorst);
  jout("\t\tHigh Priority Unload Events: %" PRIu64 "\n", farmLog.reliability.attrUnloadEventsRaw);
  jout("\t\tHelium Pressure Threshold Tripped: %" PRIu64 "\n", farmLog.reliability.heliumPresureTrip);
  jout("\t\tLBAs Corrected By Parity Sector: %" PRIi64 "\n", farmLog.reliability.numberLBACorrectedParitySector);

  // Page 5 by-head reliability parameters
  ata_farm_by_head_field_to_text("DVGA Skip Write Detect by Head", farmLog.reliability.DVGASkipWriteDetect, farmLog.driveInformation.heads);
  ata_farm_by_head_field_to_text("RVGA Skip Write Detect by Head", farmLog.reliability.RVGASkipWriteDetect, farmLog.driveInformation.heads);
  ata_farm_by_head_field_to_text("FVGA Skip Write Detect by Head", farmLog.reliability.FVGASkipWriteDetect, farmLog.driveInformation.heads);
  ata_farm_by_head_field_to_text("Skip Write Detect Threshold Exceeded by Head", farmLog.reliability.skipWriteDetectThresExceeded, farmLog.driveInformation.heads);
  ata_farm_by_head_field_to_text("Write Power On (hrs) by Head", farmLog.reliability.writeWorkloadPowerOnTime, farmLog.driveInformation.heads);
  ata_farm_by_head_field_to_text("MR Head Resistance from Head", (int64_t *)farmLog.reliability.mrHeadResistance, farmLog.driveInformation.heads);
  ata_farm_by_head_field_to_text("Second MR Head Resistance by Head", farmLog.reliability.secondMRHeadResistance, farmLog.driveInformation.heads);
  ata_farm_by_head_field_to_text("Number of Reallocated Sectors by Head", farmLog.reliability.reallocatedSectors, farmLog.driveInformation.heads);
  ata_farm_by_head_field_to_text("Number of Reallocation Candidate Sectors by Head", farmLog.reliability.reallocationCandidates, farmLog.driveInformation.heads);

  // Print JSON if --json or -j is specified
  json::ref jref = jglb["seagate_farm_log"];

  // Page 0: Log Header
  json::ref jref0 = jref["page_0_log_header"];
  jref0["farm_log_version"][0] = farmLog.header.majorRev;
  jref0["farm_log_version"][1] = farmLog.header.minorRev;
  jref0["pages_supported"] = farmLog.header.pagesSupported;
  jref0["log_size"] = farmLog.header.logSize;
  jref0["page_size"] = farmLog.header.pageSize;
  jref0["heads_supported"] = farmLog.header.headsSupported;
  jref0["number_of_copies"] = farmLog.header.copies;
  jref0["reason_for_frame_capture"] = farmLog.header.frameCapture;

  // Page 1: Drive Information
  json::ref jref1 = jref["page_1_drive_information"];
  jref1["serial_number"] = serialNumber;
  jref1["world_wide_name"] = worldWideName;
  jref1["device_interface"] = deviceInterface;
  jref1["device_capacity_in_bytes"] = farmLog.driveInformation.deviceCapacity * farmLog.driveInformation.lsecSize;
  jref1["physical_sector_size"] = farmLog.driveInformation.psecSize;
  jref1["logical_sector_size"] = farmLog.driveInformation.lsecSize;
  jref1["device_buffer_size"] = farmLog.driveInformation.deviceBufferSize;
  jref1["number_of_heads"] = farmLog.driveInformation.heads;
  jref1["form_factor"] = formFactor;
  jref1["rotation_rate"] = farmLog.driveInformation.rotationRate;
  jref1["firmware_rev"] = firmwareRev;
  jref1["poh"] = farmLog.driveInformation.poh;
  jref1["spoh"] = farmLog.driveInformation.spoh;
  jref1["head_flight_hours"] = farmLog.driveInformation.headFlightHours;
  jref1["head_load_events"] = farmLog.driveInformation.headLoadEvents;
  jref1["power_cycle_count"] = farmLog.driveInformation.powerCycleCount;
  jref1["reset_count"] = farmLog.driveInformation.resetCount;
  jref1["spin_up_time"] = farmLog.driveInformation.spinUpTime;
  jref1["time_to_ready"] = farmLog.driveInformation.timeToReady;
  jref1["time_held"] = farmLog.driveInformation.timeHeld;
  jref1["drive_recording_type"] = recordingType;
  jref1["max_number_for_reasign"] = farmLog.driveInformation.maxNumberForReasign;
  jref1["date_of_assembly"] = dateOfAssembly;
  jref1["depopulation_head_mask"] = farmLog.driveInformation.depopulationHeadMask;

  // Page 2: Workload Statistics
  json::ref jref2 = jref["page_2_workload_statistics"];
  jref2["total_read_commands"] = farmLog.workload.totalReadCommands;
  jref2["total_write_commands"] = farmLog.workload.totalWriteCommands;
  jref2["total_random_reads"] = farmLog.workload.totalRandomReads;
  jref2["total_random_writes"] = farmLog.workload.totalRandomWrites;
  jref2["total_other_commands"] = farmLog.workload.totalNumberofOtherCMDS;
  jref2["logical_sectors_written"] = farmLog.workload.logicalSecWritten;
  jref2["logical_sectors_read"] = farmLog.workload.logicalSecRead;
  jref2["dither"] = farmLog.workload.dither;
  jref2["dither_random"] = farmLog.workload.ditherRandom;
  jref2["dither_sequential"] = farmLog.workload.ditherSequential;
  jref2["read_commands_by_radius_0_3"] = farmLog.workload.readCommandsByRadius1;
  jref2["read_commands_by_radius_3_25"] = farmLog.workload.readCommandsByRadius2;
  jref2["read_commands_by_radius_25_75"] = farmLog.workload.readCommandsByRadius3;
  jref2["read_commands_by_radius_75_100"] = farmLog.workload.readCommandsByRadius4;
  jref2["write_commands_by_radius_0_3"] = farmLog.workload.writeCommandsByRadius1;
  jref2["write_commands_by_radius_3_25"] = farmLog.workload.writeCommandsByRadius2;
  jref2["write_commands_by_radius_25_75"] = farmLog.workload.writeCommandsByRadius3;
  jref2["write_commands_by_radius_75_100"] = farmLog.workload.writeCommandsByRadius4;

  // Page 3: Error Statistics
  json::ref jref3 = jref["page_3_error_statistics"];
  jref3["number_of_unrecoverable_read_errors"] = farmLog.error.totalUnrecoverableReadErrors;
  jref3["number_of_unrecoverable_write_errors"] = farmLog.error.totalUnrecoverableWriteErrors;
  jref3["number_of_reallocated_sectors"] = farmLog.error.totalReallocations;
  jref3["number_of_read_recovery_attempts"] = farmLog.error.totalReadRecoveryAttepts;
  jref3["number_of_mechanical_start_failures"] = farmLog.error.totalMechanicalStartRetries;
  jref3["number_of_reallocated_candidate_sectors"] = farmLog.error.totalReallocationCanidates;
  jref3["total_asr_events"] = farmLog.error.totalASREvents;
  jref3["total_crc_errors"] = farmLog.error.totalCRCErrors;
  jref3["attr_spin_retry_count"] = farmLog.error.attrSpinRetryCount;
  jref3["normal_spin_retry_count"] = farmLog.error.normalSpinRetryCount;
  jref3["worst_spin_rretry_count"] = farmLog.error.worstSpinRretryCount;
  jref3["number_of_ioedc_errors"] = farmLog.error.attrIOEDCErrors;
  jref3["command_time_out_count_total"] = farmLog.error.attrCTOCount;
  jref3["command_time_out_over_5_seconds_count"] = farmLog.error.overFiveSecCTO;
  jref3["command_time_out_over_7_seconds_count"] = farmLog.error.overSevenSecCTO;
  jref3["total_flash_led"] = farmLog.error.totalFlashLED;
  jref3["index_flash_led"] = farmLog.error.indexFlashLED;
  jref3["uncorrectables"] = farmLog.error.uncorrectables;
  jref3["cumulative_unrecoverable_read_erc"] = farmLog.error.cumulativeUnrecoverableReadERC;
  jref3["total_flash_led_errors"] = farmLog.error.totalFlashLED;

  // Page 3 Flash-LED Information
  for ( uint8_t i = flash_led_array_size; i > 0; i-- ) {
     index = (i - farmLog.error.indexFlashLED + flash_led_array_size) % flash_led_array_size;
     sprintf(buffer, "flash_led_event_%i", index);
     json::ref jref3a = jref3[buffer];
     jref3a["timestamp_of_event"] = farmLog.error.universalTimestampFlashLED[index];
     jref3a["event_information"] = farmLog.error.flashLEDArray[index];
     jref3a["power_cycle_event"] = farmLog.error.powerCycleFlashLED[index];
  }

  // Page 3 by-head parameters
  for (uint8_t hd = 0; hd < farmLog.driveInformation.heads; hd++){
     sprintf(buffer, "cum_lifetime_unrecoverable_by_head_%i", hd);
     json::ref jref3_hd = jref3[buffer];
     jref3_hd["cum_lifetime_unrecoverable_read_repeating"] = farmLog.error.cumulativeUnrecoverableReadRepeating[hd];
     jref3_hd["cum_lifetime_unrecoverable_read_unique"] = farmLog.error.cumulativeUnrecoverableReadUnique[hd];
  }

  // Page 4: Environment Statistics
  json::ref jref4 = jref["page_4_environment_statistics"];
  jref4["curent_temp"] = farmLog.environment.curentTemp;
  jref4["highest_temp"] = farmLog.environment.highestTemp;
  jref4["lowest_temp"] = farmLog.environment.lowestTemp;
  jref4["average_temp"] = farmLog.environment.averageTemp;
  jref4["average_long_temp"] = farmLog.environment.averageLongTemp;
  jref4["highest_short_temp"] = farmLog.environment.highestShortTemp;
  jref4["lowest_short_temp"] = farmLog.environment.lowestShortTemp;
  jref4["highest_long_temp"] = farmLog.environment.highestLongTemp;
  jref4["lowest_long_temp"] = farmLog.environment.lowestLongTemp;
  jref4["over_temp_time"] = farmLog.environment.overTempTime;
  jref4["under_temp_time"] = farmLog.environment.underTempTime;
  jref4["max_temp"] = farmLog.environment.maxTemp;
  jref4["min_temp"] = farmLog.environment.minTemp;
  jref4["humidity"] = farmLog.environment.humidity;
  jref4["current_motor_power"] = farmLog.environment.currentMotorPower;
  jref4["current_12v_in_mv"] = farmLog.environment.current12v;
  jref4["minimum_12v_in_mv"] = farmLog.environment.min12v;
  jref4["maximum_12v_in_mv"] = farmLog.environment.max12v;
  jref4["current_5v_in_mv"] = farmLog.environment.current5v;
  jref4["minimum_5v_in_mv"] = farmLog.environment.min5v;
  jref4["maximum_5v_in_mv"] = farmLog.environment.max5v;
  jref4["average_12v_power"] = farmLog.environment.powerAverage12v;
  jref4["minimum_12v_power"] = farmLog.environment.powerMin12v;
  jref4["maximum_12v_power"] = farmLog.environment.powerMax12v;
  jref4["average_5v_power"] = farmLog.environment.powerAverage5v;
  jref4["minimum_5v_power"] = farmLog.environment.powerMin5v;
  jref4["maximum_5v_power"] = farmLog.environment.powerMax5v;

  // Page 5: Reliability Statistics
  json::ref jref5 = jref["page_5_reliability_statistics"];
  jref5["attr_error_rate_raw"] = farmLog.reliability.attrErrorRateRaw;
  jref5["error_rate_normalized"] = farmLog.reliability.attrErrorRateNormal;
  jref5["error_rate_worst"] = farmLog.reliability.attrErrorRateWorst;
  jref5["attr_seek_error_rate_raw"] = farmLog.reliability.attrSeekErrorRateRaw;
  jref5["seek_error_rate_normalized"] = farmLog.reliability.attrSeekErrorRateNormal;
  jref5["seek_error_rate_worst"] = farmLog.reliability.attrSeekErrorRateWorst;
  jref5["high_priority_unload_events"] = farmLog.reliability.attrUnloadEventsRaw;
  jref5["helium_presure_trip"] = farmLog.reliability.heliumPresureTrip;
  jref5["lbas_corrected_by_parity_sector"] = farmLog.reliability.numberLBACorrectedParitySector;

  // Page 5: Reliability Statistics By Head
  ata_farm_by_head_field_to_json(jref5, buffer, "dvga_skip_write_detect_by_head", farmLog.reliability.DVGASkipWriteDetect, farmLog.driveInformation.heads);
  ata_farm_by_head_field_to_json(jref5, buffer, "rvga_skip_write_detect_by_head", farmLog.reliability.RVGASkipWriteDetect, farmLog.driveInformation.heads);
  ata_farm_by_head_field_to_json(jref5, buffer, "fvga_skip_write_detect_by_head", farmLog.reliability.FVGASkipWriteDetect, farmLog.driveInformation.heads);
  ata_farm_by_head_field_to_json(jref5, buffer, "skip_write_detect_threshold_exceeded_by_head", farmLog.reliability.skipWriteDetectThresExceeded, farmLog.driveInformation.heads);
  ata_farm_by_head_field_to_json(jref5, buffer, "write_workload_power_on_time_by_head", farmLog.reliability.writeWorkloadPowerOnTime, farmLog.driveInformation.heads);
  ata_farm_by_head_field_to_json(jref5, buffer, "mr_head_resistance_from_head", (int64_t *)farmLog.reliability.mrHeadResistance, farmLog.driveInformation.heads);
  ata_farm_by_head_field_to_json(jref5, buffer, "second_mr_head_resistance_by_head", farmLog.reliability.secondMRHeadResistance, farmLog.driveInformation.heads);
  ata_farm_by_head_field_to_json(jref5, buffer, "number_of_reallocated_sectors_by_head", farmLog.reliability.reallocatedSectors, farmLog.driveInformation.heads);
  ata_farm_by_head_field_to_json(jref5, buffer, "number_of_reallocation_candidate_sectors_by_head", farmLog.reliability.reallocationCandidates, farmLog.driveInformation.heads);
}

/////////////////////////////////////////////////////////////////////////////////////////
// Seagate SCSI Field Access Reliability Metrics (FARM) log (Log page 0x3D, sub-page 0x3)

/*
 *  Prints parsed FARM log (SCSI log page 0x3D, sub-page 0x3) data from Seagate
 *  drives already present in scsiFarmLog structure
 *  
 *  @param  farmLog:  Pointer to parsed farm log (const scsiFarmLog&)
 */
void scsiPrintFarmLog(const scsiFarmLog& farmLog) {
  size_t n = farmLog.driveInformation.heads;
  // Print plain-text
  jout("\nSeagate Field Access Reliability Metrics log (FARM) (SCSI Log page 0x3D, sub-page 0x3)\n");
  // Parameter 0: Log Header
  jout("\tFARM Log Parameter 0: Log Header\n");
  jout("\t\tFARM Log Version: %" PRIu64 ".%" PRIu64 "\n", farmLog.header.majorRev, farmLog.header.minorRev);
  // Parameter 1: Drive Information
  jout("\tFARM Log Parameter 1: Drive Information\n");
  jout("\n");
  // Parameter 2/6: Workload Statistics
  jout("\tFARM Log Parameter 2/6: Workload Statistics\n");
  jout("\n");
  // Parameter 3: Error Statistics
  jout("\tFARM Log Parameter 3: Error Statistics\n");
  jout("\t\tNumber of Unrecoverable Read Errors: %" PRIu64 "\n", farmLog.error.totalUnrecoverableReadErrors);
  jout("\t\tNumber of Unrecoverable Write Errors: %" PRIu64 "\n", farmLog.error.totalUnrecoverableWriteErrors);
  jout("\t\tNumber of Mechanical Start Failures: %" PRIu64 "\n", farmLog.error.totalMechanicalStartRetries);
  // Parameter 4/7: Environment Statistics
  jout("\tFARM Log Parameter 4/7: Environment Statistics\n");
  jout("\t\tCurrent 12V Input (mV): %" PRIu64 "\n", farmLog.environment2.current12v);
  jout("\t\tMinimum 12V input from last 3 SMART Summary Frames (mV): %" PRIu64 "\n", farmLog.environment2.min12v);
  jout("\t\tMaximum 12V input from last 3 SMART Summary Frames (mV): %" PRIu64 "\n", farmLog.environment2.max12v);
  jout("\t\tCurrent 5V Input (mV): %" PRIu64 "\n", farmLog.environment2.current5v);
  jout("\t\tMinimum 5V input from last 3 SMART Summary Frames (mV): %" PRIu64 "\n", farmLog.environment2.min5v);
  jout("\t\tMaximum 5V input from last 3 SMART Summary Frames (mV): %" PRIu64 "\n", farmLog.environment2.max5v);
  jout("\t\t12V Power Average (mW): %" PRIu64 "\n", farmLog.environment.powerAverage12v);
  // Parameter 5: Reliability Statistics
  jout("\tFARM Log Parameter 5: Reliability Statistics\n");
  jout("\n");
  // "By Head" Parameters
  jout("\tFARM Log \"By Head\" Parameters\n");
  jout("\t\tNumber of Reallocated Sectors:\n");
  for (unsigned i = 0; i < n; i++) {
    jout("\t\t\tHead %i: %" PRIu64 "\n", i, farmLog.totalReallocations.headValue[i]);
  }
  jout("\t\tNumber of Reallocated Candidate Sectors:\n");
  for (unsigned i = 0; i < n; i++) {
    jout("\t\t\tHead %i: %" PRIu64 "\n", i, farmLog.totalReallocationCanidates.headValue[i]);
  }
  // "By Actuator" Parameters
  jout("\tFARM Log \"By Actuator\" Parameters\n");
  const scsiFarmByActuator actrefs[] = {
    farmLog.actuator0, farmLog.actuator1, farmLog.actuator2, farmLog.actuator3
  };
  for (unsigned i = 0; i < sizeof(actrefs) /sizeof(actrefs[0]); i++) {
    const scsiFarmByActuator& ar = actrefs[i];
    jout("\t\tActuator %u:\n", i);
    jout("\t\t\tHead Load Events: %" PRIu64 "\n", ar.headLoadEvents);
    jout("\t\t\tLBAs Corrected By Intermediate Super Parity: %" PRIu64 "\n", ar.lbasCorrectedISP);
    jout("\t\t\tLBAs Corrected By Parity Sector: %" PRIu64 "\n", ar.numberLBACorrectedParitySector);
  }
  // Print JSON if --json or -j is specified
  json::ref jref = jglb["seagate_farm_log"];
  // Parameter 0: Log Header
  json::ref jref0 = jref["parameter_0_log_header"];
  jref0["farm_log_version"][0] = farmLog.header.majorRev;
  jref0["farm_log_version"][1] = farmLog.header.minorRev;
  // Parameter 1: Drive Information
  // json::ref jref1 = jref["parameter_1_drive_information"];
  // Parameter 2/6: Workload Statistics
  // json::ref jref2 = jref["parameter_2_or_6_workload_statistics"];
  // Parameter 3: Error Statistics
  json::ref jref3 = jref["parameter_3_error_statistics"];
  jref3["number_of_unrecoverable_read_errors"] = farmLog.error.totalUnrecoverableReadErrors;
  jref3["number_of_unrecoverable_write_errors"] = farmLog.error.totalUnrecoverableWriteErrors;
  jref3["number_of_mechanical_start_failures"] = farmLog.error.totalMechanicalStartRetries;
  // Parameter 4/7: Environment Statistics
  json::ref jref4 = jref["parameter_4_or_7_environment_statistics"];
  jref4["current_12_volt_input_in_mv"] = farmLog.environment2.current12v;
  jref4["minimum_12_volt_input_in_mv"] = farmLog.environment2.min12v;
  jref4["maximum_12_volt_input_in_mv"] = farmLog.environment2.max12v;
  jref4["current_5_volt_input_in_mv"] = farmLog.environment2.current5v;
  jref4["minimum_5_volt_input_in_mv"] = farmLog.environment2.min5v;
  jref4["maximum_5_volt_input_in_mv"] = farmLog.environment2.max5v;
  jref4["twelve_volt_power_average_in_mw"] = farmLog.environment.powerAverage12v;
  // Parameter 5: Reliability Statistics
  // json::ref jref5 = jref["parameter_5_reliability_statistics"];
  // "By Head" Parameters
  json::ref jrefh = jref["by_head_parameters"];
  json::ref jrefh0 = jrefh["number_of_reallocated_sectors"];
  for (unsigned i = 0; i < n; i++) {
    char h[15];
    sprintf(h, "head_%i", i);
    jrefh0[h] = farmLog.totalReallocations.headValue[i];
  }
  json::ref jrefh1 = jrefh["number_of_reallocated_candidate_sectors"];
  for (unsigned i = 0; i < n; i++) {
    char h[15];
    sprintf(h, "head_%i", i);
    jrefh1[h] = farmLog.totalReallocationCanidates.headValue[i];
  }
  // "By Actuator" Parameters
  json::ref jrefa = jref["by_actuator_parameters"];
  for (unsigned i = 0; i < sizeof(actrefs) /sizeof(actrefs[0]); i++) {
    const scsiFarmByActuator& ar = actrefs[i];
    char a[15];
    sprintf(a, "actuator_%i", i);
    jrefa[a]["head_load_events"] = ar.headLoadEvents;
    jrefa[a]["lbas_corrected_by_intermediate_super_parity"] = ar.lbasCorrectedISP;
    jrefa[a]["lbas_corrected_by_parity_sector"] = ar.numberLBACorrectedParitySector;
  }
}
