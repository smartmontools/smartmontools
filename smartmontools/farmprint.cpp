/*
 * farmprint.cpp
 *
 * Home page of code is: https://www.smartmontools.org
 *
 * Copyright (C) 2021 - 2023 Seagate Technology LLC and/or its Affiliates
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "config.h"

#define __STDC_FORMAT_MACROS 1
#include <string>
#include <inttypes.h>
#include "farmprint.h"
#include "smartctl.h"

/*
 *  Get the recording type descriptor from FARM.
 *  Stored as bitmask in log pag 1 byte offset 336-343. (Seagate FARM Spec Rev 4.23.1)
 *
 *  @param  driveRecordingType: Constant 64-bit integer recording type descriptor (const uint64_t)
 *  @return:  Constant reference to string literal (const char *)
 */
static const char* farm_get_recording_type(const uint64_t driveRecordingType) {
  switch (driveRecordingType & 0x3) {
    case 0x1:
      return "SMR";
    case 0x2:
      return "CMR";
    default :
      return "UNKNOWN";
  }
}

/*
 *  Get the form factor descriptor from FARM.
 *  Stored as integer in log pag 1 byte offset 336-343. (Seagate FARM Spec Rev 4.23.1)
 *  Consistent with definitions in ACS-3 Table A.32, SBC-4 Table 263.
 *
 *  @param  formFactor: Constant 64-bit integer form factor descriptor (const uint64_t)
 *  @return:  Constant reference to string literal (const char *)
 */
static const char* farm_get_form_factor(const uint64_t formFactor) {
  switch (formFactor & 0xF) {
    case 0x1:
      return "5.25 inches";
    case 0x2:
      return "3.5 inches";
    case 0x3:
      return "2.5 inches";
    case 0x4:
      return "1.8 inches";
    case 0x5:
      return "< 1.8 inches";
    default :
      return 0;
  }
}

/*
 *  Output the 64-bit integer value of a FARM parameter by head in plain text format
 *
 *  @param  desc:  Description of the parameter (const char *)
 *  @param  paramArray:  Reference to int64_t array containing paramter values for each head (const int64_t *)
 *  @param  numHeads:  Constant 64-bit integer representing ASCII description of the device interface (const uint64_t)
 */
static void farm_print_by_head_to_text(const char* desc, const int64_t* paramArray, const uint64_t numHeads) {
  for (uint8_t hd = 0; hd < (uint8_t)numHeads; hd++) {
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
static void farm_print_by_head_to_json(const json::ref & jref, char (& buffer)[128], const char* desc,
                                       const int64_t* paramArray, const uint64_t numHeads) {
  for (uint8_t hd = 0; hd < (uint8_t)numHeads; hd++) {
    snprintf(buffer, sizeof(buffer), "%s_%" PRIu8, desc, hd);
    jref[buffer] = paramArray[hd];
  }
}

/*
 *  Swap adjacent 16-bit words of an unsigned 64-bit integer.
 *
 *  @param  param:  Constant reference to an unsigned 64-bit integer (const uint64_t *)
 *  @return result
 */
static uint64_t farm_byte_swap(const uint64_t param) {
  const uint64_t even_bytes = 0x0000FFFF0000FFFF;
  const uint64_t odd_bytes  = 0xFFFF0000FFFF0000;
  return ((param & even_bytes) << 16) | ((param & odd_bytes) >> 16);
}

/*
 *  Formats an unsigned 64-bit integer into a big-endian null-terminated ascii string.
 *
 *  @param  buffer:  Constant reference to character buffer (char *)
 *  @param  param:  Constant 64-bit integer containing ASCII FARM field information (const uint64_t)
 *  @return reference to the char buffer containing a null-terminated string
 */
static char* farm_format_id_string(char* buffer, const uint64_t param) {
  uint8_t val;
  uint8_t j = 0;
  size_t str_size = sizeof(param) / sizeof(buffer[0]);

  for (uint8_t i = 0; i < str_size; i++) {
    val = (param >> ((str_size - i - 1) * 8)) & 0xFF;
    if (32 <= val && val < 127) {
      buffer[j] = val;
      j++;
    }
  }
  buffer[j] = '\0';
  return buffer;
}

/*
 *  Overload function to format and concat two 8-byte FARM fields.
 *
 *  @param  buffer:  Constant reference to character buffer (char *)
 *  @param  param1:  Constant 64-bit integer containing the low 8 bytes of the FARM field (const uint64_t)
 *  @param  param2:  Constant 64-bit integer containing the high 8 bytes of the FARM field (const uint64_t)
 *  @return reference to char buffer containing a null-terminated string
 */
static char* farm_format_id_string(char* buffer, const uint64_t param1, const uint64_t param2) {
  farm_format_id_string(buffer, param2);
  farm_format_id_string(&buffer[strlen(buffer)], param1);
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
  // Request feedback on FARM output on big-endian systems
  if (isbigendian()) {
    jinf("FARM support was not tested on Big Endian platforms by the developers.\n"
         "Please report success/failure to " PACKAGE_BUGREPORT "\n\n");
  }

  char buffer[128]; // Generic character buffer

  // Get device information
  char serialNumber[sizeof(farmLog.driveInformation.serialNumber) + sizeof(farmLog.driveInformation.serialNumber2) + 1];
  farm_format_id_string(serialNumber, farm_byte_swap(farmLog.driveInformation.serialNumber2), farm_byte_swap(farmLog.driveInformation.serialNumber));

  char worldWideName[64];
  snprintf(worldWideName, sizeof(worldWideName), "0x%" PRIx64 "%" PRIx64, farm_byte_swap(farmLog.driveInformation.worldWideName),
           farm_byte_swap(farmLog.driveInformation.worldWideName2));

  char deviceInterface[sizeof(farmLog.driveInformation.deviceInterface)];
  farm_format_id_string(deviceInterface, farmLog.driveInformation.deviceInterface);

  const char* formFactor = farm_get_form_factor(farmLog.driveInformation.factor);

  char firmwareRev[sizeof(farmLog.driveInformation.firmwareRev) + sizeof(farmLog.driveInformation.firmwareRev2) + 1];
  farm_format_id_string(firmwareRev, farm_byte_swap(farmLog.driveInformation.firmwareRev2), farm_byte_swap(farmLog.driveInformation.firmwareRev));

  char modelNumber[sizeof(farmLog.driveInformation.modelNumber) + 1];
  for (uint8_t i = 0; i < sizeof(farmLog.driveInformation.modelNumber) / sizeof(farmLog.driveInformation.modelNumber[0]); i++) {
    farm_format_id_string(&modelNumber[strlen(modelNumber)], farm_byte_swap(farmLog.driveInformation.modelNumber[i]));
  }

  const char* recordingType = farm_get_recording_type(farmLog.driveInformation.driveRecordingType);

  char dateOfAssembly[sizeof(farmLog.driveInformation.dateOfAssembly)];
  farm_format_id_string(dateOfAssembly, farm_byte_swap(farmLog.driveInformation.dateOfAssembly));

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
  jout("\t\tDevice Capacity in Sectors: %" PRIu64 "\n", farmLog.driveInformation.deviceCapacity);
  jout("\t\tPhysical Sector Size: %" PRIu64 "\n", farmLog.driveInformation.psecSize);
  jout("\t\tLogical Sector Size: %" PRIu64 "\n", farmLog.driveInformation.lsecSize);
  jout("\t\tDevice Buffer Size: %" PRIu64 "\n", farmLog.driveInformation.deviceBufferSize);
  jout("\t\tNumber of Heads: %" PRIu64 "\n", farmLog.driveInformation.heads);
  jout("\t\tDevice Form Factor: %s\n", formFactor);
  jout("\t\tRotation Rate: %" PRIu64 " rpm\n", farmLog.driveInformation.rotationRate);
  jout("\t\tFirmware Rev: %s\n", firmwareRev);
  jout("\t\tATA Security State (ID Word 128): 0x016%" PRIx64 "\n", farmLog.driveInformation.security);
  jout("\t\tATA Features Supported (ID Word 78): 0x016%" PRIx64 "\n", farmLog.driveInformation.featuresSupported);
  jout("\t\tATA Features Enabled (ID Word 79): 0x%016" PRIx64 "\n", farmLog.driveInformation.featuresEnabled);
  jout("\t\tPower on Hours: %" PRIu64 "\n", farmLog.driveInformation.poh);
  jout("\t\tSpindle Power on Hours: %" PRIu64 "\n", farmLog.driveInformation.spoh);
  jout("\t\tHead Flight Hours: %" PRIu64 "\n", farmLog.driveInformation.headFlightHours);
  jout("\t\tHead Load Events: %" PRIu64 "\n", farmLog.driveInformation.headLoadEvents);
  jout("\t\tPower Cycle Count: %" PRIu64 "\n", farmLog.driveInformation.powerCycleCount);
  jout("\t\tHardware Reset Count: %" PRIu64 "\n", farmLog.driveInformation.resetCount);
  jout("\t\tSpin-up Time: %" PRIu64 " ms\n", farmLog.driveInformation.spinUpTime);
  jout("\t\tTime to ready of the last power cycle: %" PRIu64 " ms\n", farmLog.driveInformation.timeToReady);
  jout("\t\tTime drive is held in staggered spin: %" PRIu64 " ms\n", farmLog.driveInformation.timeHeld);
  jout("\t\tModel Number: %s\n", modelNumber);
  jout("\t\tDrive Recording Type: %s\n", recordingType);
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
  size_t flash_led_size = sizeof(farmLog.error.flashLEDArray) / sizeof(farmLog.error.flashLEDArray[0]);
  jout("\t\tTotal Flash LED (Assert) Events: %" PRIu64 "\n", farmLog.error.totalFlashLED);
  jout("\t\tIndex of the last Flash LED: %" PRIu64 "\n", farmLog.error.indexFlashLED);
  for (uint8_t i = flash_led_size; i > 0; i--) {
    index = (i - farmLog.error.indexFlashLED + flash_led_size) % flash_led_size;
    jout("\t\tFlash LED Event %" PRIuMAX ":\n", static_cast<uintmax_t>(flash_led_size - i));
    jout("\t\t\tEvent Information: 0x%016" PRIx64 "\n", farmLog.error.flashLEDArray[index]);
    jout("\t\t\tTimestamp of Event %" PRIuMAX " (hours): %" PRIu64 "\n", static_cast<uintmax_t>(flash_led_size - i), farmLog.error.universalTimestampFlashLED[index]);
    jout("\t\t\tPower Cycle Event %" PRIuMAX ": %" PRIx64 "\n", static_cast<uintmax_t>(flash_led_size - i), farmLog.error.powerCycleFlashLED[index]);
  }

  // Page 3 unrecoverable errors by-head
  jout("\t\tUncorrectable errors: %" PRIu64 "\n", farmLog.error.uncorrectables);
  jout("\t\tCumulative Lifetime Unrecoverable Read errors due to ERC: %" PRIu64 "\n", farmLog.error.cumulativeUnrecoverableReadERC);
  for (uint8_t hd = 0; hd < (uint8_t)farmLog.driveInformation.heads; hd++) {
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
  farm_print_by_head_to_text("DVGA Skip Write Detect by Head", farmLog.reliability.DVGASkipWriteDetect, farmLog.driveInformation.heads);
  farm_print_by_head_to_text("RVGA Skip Write Detect by Head", farmLog.reliability.RVGASkipWriteDetect, farmLog.driveInformation.heads);
  farm_print_by_head_to_text("FVGA Skip Write Detect by Head", farmLog.reliability.FVGASkipWriteDetect, farmLog.driveInformation.heads);
  farm_print_by_head_to_text("Skip Write Detect Threshold Exceeded by Head", farmLog.reliability.skipWriteDetectThresExceeded, farmLog.driveInformation.heads);
  farm_print_by_head_to_text("Write Power On (hrs) by Head", farmLog.reliability.writeWorkloadPowerOnTime, farmLog.driveInformation.heads);
  farm_print_by_head_to_text("MR Head Resistance from Head", (int64_t*)farmLog.reliability.mrHeadResistance, farmLog.driveInformation.heads);
  farm_print_by_head_to_text("Second MR Head Resistance by Head", farmLog.reliability.secondMRHeadResistance, farmLog.driveInformation.heads);
  farm_print_by_head_to_text("Number of Reallocated Sectors by Head", farmLog.reliability.reallocatedSectors, farmLog.driveInformation.heads);
  farm_print_by_head_to_text("Number of Reallocation Candidate Sectors by Head", farmLog.reliability.reallocationCandidates, farmLog.driveInformation.heads);

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
  jref1["device_capacity_in_sectors"] = farmLog.driveInformation.deviceCapacity;
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
  for (uint8_t i = flash_led_size; i > 0; i--) {
    index = (i - farmLog.error.indexFlashLED + flash_led_size) % flash_led_size;
    snprintf(buffer, sizeof(buffer), "flash_led_event_%i", index);
    json::ref jref3a = jref3[buffer];
    jref3a["timestamp_of_event"] = farmLog.error.universalTimestampFlashLED[index];
    jref3a["event_information"] = farmLog.error.flashLEDArray[index];
    jref3a["power_cycle_event"] = farmLog.error.powerCycleFlashLED[index];
  }

  // Page 3 by-head parameters
  for (uint8_t hd = 0; hd < (uint8_t)farmLog.driveInformation.heads; hd++) {
    snprintf(buffer, sizeof(buffer), "cum_lifetime_unrecoverable_by_head_%i", hd);
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
  farm_print_by_head_to_json(jref5, buffer, "dvga_skip_write_detect_by_head", farmLog.reliability.DVGASkipWriteDetect, farmLog.driveInformation.heads);
  farm_print_by_head_to_json(jref5, buffer, "rvga_skip_write_detect_by_head", farmLog.reliability.RVGASkipWriteDetect, farmLog.driveInformation.heads);
  farm_print_by_head_to_json(jref5, buffer, "fvga_skip_write_detect_by_head", farmLog.reliability.FVGASkipWriteDetect, farmLog.driveInformation.heads);
  farm_print_by_head_to_json(jref5, buffer, "skip_write_detect_threshold_exceeded_by_head", farmLog.reliability.skipWriteDetectThresExceeded, farmLog.driveInformation.heads);
  farm_print_by_head_to_json(jref5, buffer, "write_workload_power_on_time_by_head", farmLog.reliability.writeWorkloadPowerOnTime, farmLog.driveInformation.heads);
  farm_print_by_head_to_json(jref5, buffer, "mr_head_resistance_from_head", (int64_t*)farmLog.reliability.mrHeadResistance, farmLog.driveInformation.heads);
  farm_print_by_head_to_json(jref5, buffer, "second_mr_head_resistance_by_head", farmLog.reliability.secondMRHeadResistance, farmLog.driveInformation.heads);
  farm_print_by_head_to_json(jref5, buffer, "number_of_reallocated_sectors_by_head", farmLog.reliability.reallocatedSectors, farmLog.driveInformation.heads);
  farm_print_by_head_to_json(jref5, buffer, "number_of_reallocation_candidate_sectors_by_head", farmLog.reliability.reallocationCandidates, farmLog.driveInformation.heads);
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
  // Request feedback on FARM output on big-endian systems
  if (isbigendian()) {
    jinf("FARM support was not tested on Big Endian platforms by the developers.\n"
         "Please report success/failure to " PACKAGE_BUGREPORT "\n\n");
  }

  // Get device information
  char serialNumber[sizeof(farmLog.driveInformation.serialNumber) + sizeof(farmLog.driveInformation.serialNumber2) + 1];
  farm_format_id_string(serialNumber, farmLog.driveInformation.serialNumber, farmLog.driveInformation.serialNumber2);

  char worldWideName[64];
  snprintf(worldWideName, sizeof(worldWideName), "0x%" PRIx64 "%" PRIx64, farmLog.driveInformation.worldWideName2,
           farmLog.driveInformation.worldWideName);

  char firmwareRev[sizeof(farmLog.driveInformation.firmwareRev) + sizeof(farmLog.driveInformation.firmwareRev2) + 1];
  farm_format_id_string(firmwareRev, farmLog.driveInformation.firmwareRev, farmLog.driveInformation.firmwareRev2);

  char deviceInterface[sizeof(farmLog.driveInformation.deviceInterface) + 1];
  farm_format_id_string(deviceInterface, farmLog.driveInformation.deviceInterface);

  char dateOfAssembly[sizeof(farmLog.driveInformation.dateOfAssembly) + 1];
  farm_format_id_string(dateOfAssembly, farmLog.driveInformation.dateOfAssembly);

  const char* formFactor = farm_get_form_factor(farmLog.driveInformation.factor);

  const char* recordingType = farm_get_recording_type(farmLog.driveInformation2.driveRecordingType);

  char productID[sizeof(farmLog.driveInformation2.productID) * 4 + 1];
  farm_format_id_string(productID, farmLog.driveInformation2.productID);
  farm_format_id_string(&productID[strlen(productID)], farmLog.driveInformation2.productID2);
  farm_format_id_string(&productID[strlen(productID)], farmLog.driveInformation2.productID3);
  farm_format_id_string(&productID[strlen(productID)], farmLog.driveInformation2.productID4);

  // Print plain-text
  jout("\nSeagate Field Access Reliability Metrics log (FARM) (SCSI Log page 0x3D, sub-page 0x3)\n");

  // Parameter 0: Log Header
  jout("\tFARM Log Parameter 0: Log Header\n");
  jout("\t\tFARM Log Version: %" PRIu64 ".%" PRIu64 "\n", farmLog.header.majorRev, farmLog.header.minorRev);
  jout("\t\tPages Supported: %" PRIu64 "\n", farmLog.header.parametersSupported);
  jout("\t\tLog Size: %" PRIu64 "\n", farmLog.header.logSize);
  jout("\t\tHeads Supported: %" PRIu64 "\n", farmLog.header.headsSupported);
  jout("\t\tReason for Frame Capture: %" PRIu64 "\n", farmLog.header.frameCapture);

  // Parameter 1: Drive Information
  jout("\tFARM Log Parameter 1: Drive Information\n");
  jout("\t\tSerial Number: %s\n", serialNumber);
  jout("\t\tWorld Wide Name: %s\n", worldWideName);
  jout("\t\tFirmware Rev: %s\n", firmwareRev);
  jout("\t\tDevice Interface: %s\n", deviceInterface);
  jout("\t\tDevice Capacity in Sectors: %" PRIu64 "\n", farmLog.driveInformation.deviceCapacity);
  jout("\t\tReason for Frame Capture: %" PRIu64 "\n", farmLog.driveInformation.psecSize);
  jout("\t\tLogical Sector Size: %" PRIu64 "\n", farmLog.driveInformation.lsecSize);
  jout("\t\tDevice Buffer Size: %" PRIu64 "\n", farmLog.driveInformation.deviceBufferSize);
  jout("\t\tNumber of heads: %" PRIu64 "\n", farmLog.driveInformation.heads);
  jout("\t\tDevice form factor: %s\n", formFactor);
  jout("\t\tRotation Rate: %" PRIu64 "\n", farmLog.driveInformation.rotationRate);
  jout("\t\tPower on Hour: %" PRIu64 "\n", farmLog.driveInformation.poh);
  jout("\t\tPower Cycle count: %" PRIu64 "\n", farmLog.driveInformation.powerCycleCount);
  jout("\t\tHardware Reset count: %" PRIu64 "\n", farmLog.driveInformation.resetCount);
  jout("\t\tDate of Assembled: %s\n", dateOfAssembly);

  // Parameter 2: Workload Statistics
  jout("\tFARM Log Parameter 2: Workload Statistics\n");
  jout("\t\tTotal Number of Read Commands: %" PRIu64 "\n", farmLog.workload.totalReadCommands);
  jout("\t\tTotal Number of Write Commands: %" PRIu64 "\n", farmLog.workload.totalWriteCommands);
  jout("\t\tTotal Number of Random Read Cmds: %" PRIu64 "\n", farmLog.workload.totalRandomReads);
  jout("\t\tTotal Number of Random Write Cmds: %" PRIu64 "\n", farmLog.workload.totalRandomWrites);
  jout("\t\tTotal Number of Other Commands: %" PRIu64 "\n", farmLog.workload.totalNumberofOtherCMDS);
  jout("\t\tLogical Sectors Written: %" PRIu64 "\n", farmLog.workload.logicalSecWritten);
  jout("\t\tLogical Sectors Read: %" PRIu64 "\n", farmLog.workload.logicalSecRead);
  jout("\t\tNumber of Read commands from 0-3.125%% of LBA space: %" PRIu64 "\n", farmLog.workload.readCommandsByRadius1);
  jout("\t\tNumber of Read commands from 3.125-25%% of LBA space: %" PRIu64 "\n", farmLog.workload.readCommandsByRadius2);
  jout("\t\tNumber of Read commands from 25-50%% of LBA space: %" PRIu64 "\n", farmLog.workload.readCommandsByRadius3);
  jout("\t\tNumber of Read commands from 50-100%% of LBA space: %" PRIu64 "\n", farmLog.workload.readCommandsByRadius4);
  jout("\t\tNumber of Write commands from 0-3.125%% of LBA space: %" PRIu64 "\n", farmLog.workload.writeCommandsByRadius1);
  jout("\t\tNumber of Write commands from 3.125-25%% of LBA space: %" PRIu64 "\n", farmLog.workload.writeCommandsByRadius2);
  jout("\t\tNumber of Write commands from 25-50%% of LBA space: %" PRIu64 "\n", farmLog.workload.writeCommandsByRadius3);
  jout("\t\tNumber of Write commands from 50-100%% of LBA space: %" PRIu64 "\n", farmLog.workload.writeCommandsByRadius4);

  // Parameter 3: Error Statistics
  jout("\tFARM Log Parameter 3: Error Statistics\n");
  jout("\t\tUnrecoverable Read Errors: %" PRIu64 "\n", farmLog.error.totalUnrecoverableReadErrors);
  jout("\t\tUnrecoverable Write Errors: %" PRIu64 "\n", farmLog.error.totalUnrecoverableWriteErrors);
  jout("\t\tNumber of Mechanical Start Failures: %" PRIu64 "\n", farmLog.error.totalMechanicalStartRetries);
  jout("\t\tFRU code if smart trip from most recent SMART Frame: %" PRIu64 "\n", farmLog.error.tripCode);
  jout("\t\tInvalid DWord Count Port A: %" PRIu64 "\n", farmLog.error.invalidDWordCountA);
  jout("\t\tInvalid DWord Count Port B: %" PRIu64 "\n", farmLog.error.invalidDWordCountB);
  jout("\t\tDisparity Error Count Port A: %" PRIu64 "\n", farmLog.error.disparityErrorCodeA);
  jout("\t\tDisparity Error Count Port B: %" PRIu64 "\n", farmLog.error.disparityErrorCodeB);
  jout("\t\tLoss Of DWord Sync Port A: %" PRIu64 "\n", farmLog.error.lossOfDWordSyncA);
  jout("\t\tLoss Of DWord Sync Port B: %" PRIu64 "\n", farmLog.error.lossOfDWordSyncB);
  jout("\t\tPhy Reset Problem Port A: %" PRIu64 "\n", farmLog.error.phyResetProblemA);
  jout("\t\tPhy Reset Problem Port B: %" PRIu64 "\n", farmLog.error.phyResetProblemB);

  // Parameter 4: Environment Statistics
  jout("\tFARM Log Parameter 4: Environment Statistics\n");
  jout("\t\tCurrent Temperature (Celsius): %" PRIu64 "\n", farmLog.environment.curentTemp);
  jout("\t\tHighest Temperature: %" PRIu64 "\n", farmLog.environment.highestTemp);
  jout("\t\tLowest Temperature: %" PRIu64 "\n", farmLog.environment.lowestTemp);
  jout("\t\tSpecified Max Operating Temperature: %" PRIu64 "\n", farmLog.environment.maxTemp);
  jout("\t\tSpecified Min Operating Temperature: %" PRIu64 "\n", farmLog.environment.minTemp);
  jout("\t\tCurrent Relative Humidity: %" PRIu64 "\n", farmLog.environment.humidity);
  jout("\t\tCurrent Motor Power: %" PRIu64 "\n", farmLog.environment.currentMotorPower);
  jout("\t\t12V Power Average: %" PRIu64 "\n", farmLog.environment.powerAverage12v);
  jout("\t\t12V Power Minimum: %" PRIu64 "\n", farmLog.environment.powerMin12v);
  jout("\t\t12V Power Maximum: %" PRIu64 "\n", farmLog.environment.powerMax12v);
  jout("\t\t5V Power Average: %" PRIu64 "\n", farmLog.environment.powerAverage5v);
  jout("\t\t5V Power Minimum: %" PRIu64 "\n", farmLog.environment.powerMin5v);
  jout("\t\t5V Power Maximum: %" PRIu64 "\n", farmLog.environment.powerMax5v);

  // Parameter 5: Reliability Statistics
  jout("\tFARM Log Parameter 5: Reliability Statistics\n");
  jout("\t\tHelium Pressure Threshold Tripped: %" PRIi64 "\n", farmLog.reliability.heliumPresureTrip);

  // Parameter 6: Drive Information Continued
  jout("\tFARM Log Parameter 6: Drive Information Continued\n");
  jout("\t\tDepopulation Head Mask: %" PRIu64 "\n", farmLog.driveInformation2.depopulationHeadMask);
  jout("\t\tProduct ID: %s\n", productID);
  jout("\t\tDrive Recording Type: %s\n", recordingType);
  jout("\t\tHas Drive been Depopped: %" PRIu64 "\n", farmLog.driveInformation2.dpopped);
  jout("\t\tMax Number of Available Sectors for Reassignment: %" PRIu64 "\n", farmLog.driveInformation2.maxNumberForReasign);
  jout("\t\tTime to ready of the last power cycle (sec): %" PRIu64 "\n", farmLog.driveInformation2.timeToReady);
  jout("\t\tTime drive is held in staggered spin (sec): %" PRIu64 "\n", farmLog.driveInformation2.timeHeld);
  jout("\t\tLast Servo Spin up Time (sec): %" PRIu64 "\n", farmLog.driveInformation2.lastServoSpinUpTime);

  // Parameter 7: Environment Information Continued
  jout("\tFARM Log Parameter 7: Environment Information Continued\n");
  jout("\t\tCurrent 12 volts: %" PRIu64 "\n", farmLog.environment2.current12v);
  jout("\t\tMinimum 12 volts: %" PRIu64 "\n", farmLog.environment2.min12v);
  jout("\t\tMaximum 12 volts: %" PRIu64 "\n", farmLog.environment2.max12v);
  jout("\t\tCurrent 5 volts: %" PRIu64 "\n", farmLog.environment2.current5v);
  jout("\t\tMinimum 5 volts: %" PRIu64 "\n", farmLog.environment2.min5v);
  jout("\t\tMaximum 5 volts: %" PRIu64 "\n", farmLog.environment2.max5v);

  // "By Head" Parameters
  jout("\tFARM Log \"By Head\" Information\n");
  farm_print_by_head_to_text("MR Head Resistance", (int64_t*)farmLog.mrHeadResistance.headValue, farmLog.driveInformation.heads);
  farm_print_by_head_to_text("Number of Reallocated Sectors", (int64_t*)farmLog.totalReallocations.headValue, farmLog.driveInformation.heads);
  farm_print_by_head_to_text("Number of Reallocation Candidate Sectors", (int64_t*)farmLog.totalReallocationCanidates.headValue, farmLog.driveInformation.heads);
  farm_print_by_head_to_text("Write Power On (hrs)", (int64_t*)farmLog.writeWorkloadPowerOnTime.headValue, farmLog.driveInformation.heads);
  farm_print_by_head_to_text("Cum Lifetime Unrecoverable Read Repeating", (int64_t*)farmLog.cumulativeUnrecoverableReadRepeat.headValue, farmLog.driveInformation.heads);
  farm_print_by_head_to_text("Cum Lifetime Unrecoverable Read Unique", (int64_t*)farmLog.cumulativeUnrecoverableReadUnique.headValue, farmLog.driveInformation.heads);
  farm_print_by_head_to_text("Second MR Head Resistance", (int64_t*)farmLog.secondMRHeadResistance.headValue, farmLog.driveInformation.heads);

  // "By Actuator" Parameters
  const scsiFarmByActuator actrefs[] = {
    farmLog.actuator0, farmLog.actuator1, farmLog.actuator2, farmLog.actuator3
  };
  for (uint8_t i = 0; i < sizeof(actrefs) / sizeof(actrefs[0]); i++) {
    jout("\tFARM Log Actuator Information 0x%" PRIx64 "\n", actrefs[i].actuatorID);
    jout("\t\tHead Load Events: %" PRIu64 "\n", actrefs[i].headLoadEvents);
    jout("\t\tTimeStamp of last IDD test: %" PRIu64 "\n", actrefs[i].timelastIDDTest);
    jout("\t\tSub-Command of Last IDD Test: %" PRIu64 "\n", actrefs[i].subcommandlastIDDTest);
    jout("\t\tNumber of Reallocated Sector Reclamations: %" PRIu64 "\n", actrefs[i].numberGListReclam);
    jout("\t\tServo Status: %" PRIu64 "\n", actrefs[i].servoStatus);
    jout("\t\tNumber of Slipped Sectors Before IDD Scan: %" PRIu64 "\n", actrefs[i].numberSlippedSectorsBeforeIDD);
    jout("\t\tNumber of Slipped Sectors Before IDD Scan: %" PRIu64 "\n", actrefs[i].numberSlippedSectorsAfterIDD);
    jout("\t\tNumber of Resident Reallocated Sectors Before IDD Scan: %" PRIu64 "\n", actrefs[i].numberResidentReallocatedBeforeIDD);
    jout("\t\tNumber of Resident Reallocated Sectors Before IDD Scan: %" PRIu64 "\n", actrefs[i].numberResidentReallocatedAfterIDD);
    jout("\t\tSuccessfully Scrubbed Sectors Before IDD Scan: %" PRIu64 "\n", actrefs[i].numberScrubbedSectorsBeforeIDD);
    jout("\t\tSuccessfully Scrubbed Sectors Before IDD Scan: %" PRIu64 "\n", actrefs[i].numberScrubbedSectorsAfterIDD);
    jout("\t\tNumber of DOS Scans Performed: %" PRIu64 "\n", actrefs[i].dosScansPerformed);
    jout("\t\tNumber of LBAs Corrected by ISP: %" PRIu64 "\n", actrefs[i].lbasCorrectedISP);
    jout("\t\tNumber of Valid Parity Sectors: %" PRIu64 "\n", actrefs[i].numberValidParitySectors);
    jout("\t\tNumber of LBAs Corrected by Parity Sector: %" PRIu64 "\n", actrefs[i].numberLBACorrectedParitySector);
  }

  // "By Actuator" Flash LED Information
  uint8_t index;
  size_t flash_led_size;
  const scsiFarmByActuatorFLED fledrefs[] = {
    farmLog.actuatorFLED0, farmLog.actuatorFLED1, farmLog.actuatorFLED2, farmLog.actuatorFLED3
  };
  for (uint8_t i = 0; i < sizeof(fledrefs) / sizeof(fledrefs[0]); i++) {
    jout("\tFARM Log Actuator 0x%" PRIx64 " Flash LED Information\n", fledrefs[i].actuatorID);
    jout("\t\tTotal Flash LED Events: %" PRIu64 "\n", fledrefs[i].totalFlashLED);
    jout("\t\tIndex of Last Flash LED: %" PRIu64 "\n", fledrefs[i].indexFlashLED);

    flash_led_size = sizeof(fledrefs[i].flashLEDArray) / sizeof(fledrefs[i].flashLEDArray[0]);
    for (uint8_t j = flash_led_size; j > 0; j--) {
      index = (j - fledrefs[i].indexFlashLED + flash_led_size) % flash_led_size;
      jout("\t\tEvent %" PRIuMAX ":\n", static_cast<uintmax_t>(flash_led_size - j));
      jout("\t\t\tEvent Information: 0x%016" PRIx64 "\n", fledrefs[i].flashLEDArray[index]);
      jout("\t\t\tTimestamp of Event %" PRIuMAX " (hours): %" PRIu64 "\n", static_cast<uintmax_t>(flash_led_size - j), fledrefs[i].universalTimestampFlashLED[index]);
      jout("\t\t\tPower Cycle Event %" PRIuMAX ": %" PRIx64 "\n", static_cast<uintmax_t>(flash_led_size - j), fledrefs[i].powerCycleFlashLED[index]);
    }
  }

  // "By Actuator" Reallocation Information
  const scsiFarmByActuatorReallocation ararefs[] = {
    farmLog.actuatorReallocation0, farmLog.actuatorReallocation1, farmLog.actuatorReallocation2, farmLog.actuatorReallocation3
  };
  for (uint8_t i = 0; i < sizeof(ararefs) / sizeof(ararefs[0]); i++) {
    jout("\tFARM Log Actuator 0x%" PRIx64 " Reallocation\n", ararefs[i].actuatorID);
    jout("\t\tNumber of Reallocated Sectors: %" PRIu64 "\n", ararefs[i].totalReallocations);
    jout("\t\tNumber of Reallocated Candidate Sectors: %" PRIu64 "\n", ararefs[i].totalReallocationCanidates);
  }

  // Print JSON if --json or -j is specified
  json::ref jref = jglb["seagate_farm_log"];

  // Parameter 0: Log Header
  json::ref jref0 = jref["log_header"];
  jref0["farm_log_version"] = farmLog.header.minorRev;
  jref0["pages_supported"] = farmLog.header.parametersSupported;
  jref0["log_size"] = farmLog.header.logSize;
  jref0["heads_supported"] = farmLog.header.headsSupported;
  jref0["reason_for_frame_capture"] = farmLog.header.frameCapture;

  // Parameter 1: Drive Information
  json::ref jref1 = jref["drive_information"];
  jref1["serial_number"] = serialNumber;
  jref1["world_wide_name"] = worldWideName;
  jref1["firmware_rev"] = firmwareRev;
  jref1["device_interface"] = deviceInterface;
  jref1["device_capacity_in_sectors"] = farmLog.driveInformation.deviceCapacity;
  jref1["reason_for_frame_capture"] = farmLog.driveInformation.psecSize;
  jref1["logical_sector_size"] = farmLog.driveInformation.lsecSize;
  jref1["device_buffer_size"] = farmLog.driveInformation.deviceBufferSize;
  jref1["number_of_heads"] = farmLog.driveInformation.heads;
  jref1["device_form_factor"] = formFactor;
  jref1["rotation_rate"] = farmLog.driveInformation.rotationRate;
  jref1["power_on_hour"] = farmLog.driveInformation.poh;
  jref1["power_cycle_count"] = farmLog.driveInformation.powerCycleCount;
  jref1["hardware_reset_count"] = farmLog.driveInformation.resetCount;
  jref1["date_of_assembled"] = dateOfAssembly;

  // Parameter 2: Workload Statistics
  json::ref jref2 = jref["workload_statistics"];
  jref2["total_number_of_read_commands"] = farmLog.workload.totalReadCommands;
  jref2["total_number_of_write_commands"] = farmLog.workload.totalWriteCommands;
  jref2["total_number_of_random_read_cmds"] = farmLog.workload.totalRandomReads;
  jref2["total_number_of_random_write_cmds"] = farmLog.workload.totalRandomWrites;
  jref2["total_number_of_other_commands"] = farmLog.workload.totalNumberofOtherCMDS;
  jref2["logical_sectors_written"] = farmLog.workload.logicalSecWritten;
  jref2["logical_sectors_read"] = farmLog.workload.logicalSecRead;
  jref2["number_of_read_commands_from_0_to_3_percent_of_lba_space"] = farmLog.workload.readCommandsByRadius1;
  jref2["number_of_read_commands_from_3_to_25_percent_of_lba_space"] = farmLog.workload.readCommandsByRadius2;
  jref2["number_of_read_commands_from_25_to_50_percent_of_lba_space"] = farmLog.workload.readCommandsByRadius3;
  jref2["number_of_read_commands_from_50_to_100_percent_of_lba_space"] = farmLog.workload.readCommandsByRadius4;
  jref2["number_of_write_commands_from_0_to_3_percent_of_lba_space"] = farmLog.workload.writeCommandsByRadius1;
  jref2["number_of_write_commands_from_3_to_25_percent_of_lba_space"] = farmLog.workload.writeCommandsByRadius2;
  jref2["number_of_write_commands_from_25_to_50_percent_of_lba_space"] = farmLog.workload.writeCommandsByRadius3;
  jref2["number_of_write_commands_from_50_to_100_percent_of_lba_space"] = farmLog.workload.writeCommandsByRadius4;

  // Parameter 3: Error Statistics
  json::ref jref3 = jref["error_statistics"];
  jref3["unrecoverable_read_errors"] = farmLog.error.totalUnrecoverableReadErrors;
  jref3["unrecoverable_write_errors"] = farmLog.error.totalUnrecoverableWriteErrors;
  jref3["number_of_mechanical_start_failures"] = farmLog.error.totalMechanicalStartRetries;
  jref3["fru_code_if_smart_trip_from_most_recent_smart_frame"] = farmLog.error.tripCode;
  jref3["invalid_dword_count_port_a"] = farmLog.error.invalidDWordCountA;
  jref3["invalid_dword_count_port_b"] = farmLog.error.invalidDWordCountB;
  jref3["disparity_error_count_port_a"] = farmLog.error.disparityErrorCodeA;
  jref3["disparity_error_count_port_b"] = farmLog.error.disparityErrorCodeB;
  jref3["loss_of_dword_sync_port_a"] = farmLog.error.lossOfDWordSyncA;
  jref3["loss_of_dword_sync_port_b"] = farmLog.error.lossOfDWordSyncB;
  jref3["phy_reset_problem_port_a"] = farmLog.error.phyResetProblemA;
  jref3["phy_reset_problem_port_b"] = farmLog.error.phyResetProblemB;

  // Parameter 4: Environment Statistics
  json::ref jref4 = jref["environment_statistics"];
  jref4["current_temperature_(celsius)"] = farmLog.environment.curentTemp;
  jref4["highest_temperature"] = farmLog.environment.highestTemp;
  jref4["lowest_temperature"] = farmLog.environment.lowestTemp;
  jref4["specified_max_operating_temperature"] = farmLog.environment.maxTemp;
  jref4["specified_min_operating_temperature"] = farmLog.environment.minTemp;
  jref4["current_relative_humidity"] = farmLog.environment.humidity;
  jref4["current_motor_power"] = farmLog.environment.currentMotorPower;
  jref4["12v_power_average"] = farmLog.environment.powerAverage12v;
  jref4["12v_power_minimum"] = farmLog.environment.powerMin12v;
  jref4["12v_power_maximum"] = farmLog.environment.powerMax12v;
  jref4["5v_power_average"] = farmLog.environment.powerAverage5v;
  jref4["5v_power_minimum"] = farmLog.environment.powerMin5v;
  jref4["5v_power_maximum"] = farmLog.environment.powerMax5v;

  // Parameter 5: Reliability Statistics
  json::ref jref5 = jref["reliability_statistics"];
//jref5["number_of_raw_operations"] = farmLog.reliability.xxxxxx;
//jref5["cumulative_lifetime_ecc_due_to_erc"] = farmLog.reliability.xxxxxx;
  jref5["helium_pressure_threshold_tripped"] = farmLog.reliability.heliumPresureTrip;

  // Parameter 6: Drive Information Continued
  json::ref jref6 = jref["drive_information_continued"];
  jref6["depopulation_head_mask"] = farmLog.driveInformation2.depopulationHeadMask;
  jref6["product_id"] = productID;
  jref6["drive_recording_type"] = recordingType;
  jref6["has_drive_been_depopped"] = farmLog.driveInformation2.dpopped;
  jref6["max_number_of_available_sectors_for_reassignment"] = farmLog.driveInformation2.maxNumberForReasign;
  jref6["time_to_ready_of_the_last_power_cycle_(sec)"] = farmLog.driveInformation2.timeToReady;
  jref6["time_drive_is_held_in_staggered_spin_(sec)"] = farmLog.driveInformation2.timeHeld;
  jref6["last_servo_spin_up_time_(sec)"] = farmLog.driveInformation2.lastServoSpinUpTime;

  // Parameter 7: Environment Information Continued
  json::ref jref7 = jref["environment_information_continued"];
  jref7["current_12_volts"] = farmLog.environment2.current12v;
  jref7["minimum_12_volts"] = farmLog.environment2.min12v;
  jref7["maximum_12_volts"] = farmLog.environment2.max12v;
  jref7["current_5_volts"] = farmLog.environment2.current5v;
  jref7["minimum_5_volts"] = farmLog.environment2.min5v;
  jref7["maximum_5_volts"] = farmLog.environment2.max5v;

  // "By Head" Parameters
  char buffer[128]; // Generic character buffer
  json::ref jrefh = jref["head_information"];
  farm_print_by_head_to_json(jrefh, buffer, "mr_head_resistance", (int64_t*)farmLog.mrHeadResistance.headValue, farmLog.driveInformation.heads);
  farm_print_by_head_to_json(jrefh, buffer, "number_of_reallocated_sectors", (int64_t*)farmLog.totalReallocations.headValue, farmLog.driveInformation.heads);
  farm_print_by_head_to_json(jrefh, buffer, "number_of_reallocation_candidate_sectors", (int64_t*)farmLog.totalReallocationCanidates.headValue, farmLog.driveInformation.heads);
  farm_print_by_head_to_json(jrefh, buffer, "write_power_on_(hrs)", (int64_t*)farmLog.writeWorkloadPowerOnTime.headValue, farmLog.driveInformation.heads);
  farm_print_by_head_to_json(jrefh, buffer, "cum_lifetime_unrecoverable_read_repeating", (int64_t*)farmLog.cumulativeUnrecoverableReadRepeat.headValue, farmLog.driveInformation.heads);
  farm_print_by_head_to_json(jrefh, buffer, "cum_lifetime_unrecoverable_read_unique", (int64_t*)farmLog.cumulativeUnrecoverableReadUnique.headValue, farmLog.driveInformation.heads);
  farm_print_by_head_to_json(jrefh, buffer, "second_mr_head_resistance", (int64_t*)farmLog.secondMRHeadResistance.headValue, farmLog.driveInformation.heads);

  // "By Actuator" Parameters
  for (unsigned i = 0; i < sizeof(actrefs) / sizeof(actrefs[0]); i++) {
    snprintf(buffer, sizeof(buffer), "actuator_information_%" PRIx64, actrefs[i].actuatorID);
    json::ref jrefa = jref[buffer];
    jrefa["head_load_events"] = actrefs[i].headLoadEvents;
    jrefa["timestamp_of_last_idd_test"] = actrefs[i].timelastIDDTest;
    jrefa["sub-command_of_last_idd_test"] = actrefs[i].subcommandlastIDDTest;
    jrefa["number_of_reallocated_sector_reclamations"] = actrefs[i].numberGListReclam;
    jrefa["servo_status"] = actrefs[i].servoStatus;
    jrefa["number_of_slipped_sectors_before_idd_scan"] = actrefs[i].numberSlippedSectorsBeforeIDD;
    jrefa["number_of_slipped_sectors_before_idd_scan"] = actrefs[i].numberSlippedSectorsAfterIDD;
    jrefa["number_of_resident_reallocated_sectors_before_idd_scan"] = actrefs[i].numberResidentReallocatedBeforeIDD;
    jrefa["number_of_resident_reallocated_sectors_before_idd_scan"] = actrefs[i].numberResidentReallocatedAfterIDD;
    jrefa["successfully_scrubbed_sectors_before_idd_scan"] = actrefs[i].numberScrubbedSectorsBeforeIDD;
    jrefa["successfully_scrubbed_sectors_before_idd_scan"] = actrefs[i].numberScrubbedSectorsAfterIDD;
    jrefa["number_of_dos_scans_performed"] = actrefs[i].dosScansPerformed;
    jrefa["number_of_lbas_corrected_by_isp"] = actrefs[i].lbasCorrectedISP;
    jrefa["number_of_valid_parity_sectors"] = actrefs[i].numberValidParitySectors;
    jrefa["number_of_lbas_corrected_by_parity_sector"] = actrefs[i].numberLBACorrectedParitySector;
  }

  // "By Actuator" Flash LED Information
  for (unsigned i = 0; i < sizeof(fledrefs) / sizeof(fledrefs[0]); i++) {
    snprintf(buffer, sizeof(buffer), "actuator_flash_led_information_%" PRIx64, fledrefs[i].actuatorID);
    json::ref jrefa = jref[buffer];
    jrefa["total_flash_led_events"] = fledrefs[i].totalFlashLED;
    jrefa["index_of_last_flash_led"] = fledrefs[i].indexFlashLED;

    snprintf(buffer, sizeof(buffer), "event_%" PRIx64, fledrefs[i].actuatorID);
    flash_led_size = sizeof(fledrefs[i].flashLEDArray) / sizeof(fledrefs[i].flashLEDArray[0]);
    for (uint8_t j = flash_led_size; j > 0; j--) {
      index = (j - fledrefs[i].indexFlashLED + flash_led_size) % flash_led_size;
      jrefa[buffer]["event_information"] = fledrefs[i].flashLEDArray[index];
      jrefa[buffer]["timestamp_of_event"] = fledrefs[i].universalTimestampFlashLED[index];
      jrefa[buffer]["power_cycle_event"] = fledrefs[i].powerCycleFlashLED[index];
    }
  }

  // "By Actuator" Reallocation Information
  for (unsigned i = 0; i < sizeof(ararefs) / sizeof(ararefs[0]); i++) {
    snprintf(buffer, sizeof(buffer), "actuator_reallocation_information_%" PRIx64, ararefs[i].actuatorID);
    json::ref jrefa = jref[buffer];
    jrefa["number_of_reallocated_sectors"] = ararefs[i].totalReallocations;
    jrefa["number_of_reallocated_candidate_sectors"] = ararefs[i].totalReallocationCanidates;
  }
}
