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
#include <inttypes.h>
#include "farmprint.h"
#include <initializer_list>
#include "smartctl.h"

/////////////////////////////////////////////////////////////////////////////////////////
// Seagate ATA Field Access Reliability Metrics (FARM) log (Log 0xA6)

/*
 *  Prints parsed FARM log (GP Log 0xA6) data from Seagate
 *  drives already present in ataFarmLogFrame structure
 *  
 *  @param  farmLog:  Constant reference to parsed farm log (const ataFarmLog&)
 */
void ataPrintFarmLog(const ataFarmLog& farmLog) {
  // Print plain-text
  jout("\nSeagate Field Access Reliability Metrics log (FARM) (GP log 0xA6)\n");
  // Page 0: Log Header
  jout("\tFARM Log Page 0: Log Header\n");
  jout("\t\tFARM Log Version: %" PRIu64 ".%" PRIu64 "\n", farmLog.header.majorRev, farmLog.header.minorRev);
  // Page 1: Drive Information
  jout("\tFARM Log Page 1: Drive Information\n");
  jout("\t\tHead Load Events: %" PRIu64 "\n", farmLog.driveInformation.headLoadEvents);
  // Page 2: Workload Statistics
  jout("\tFARM Log Page 2: Workload Statistics\n");
  jout("\n");
  // Page 3: Error Statistics
  jout("\tFARM Log Page 3: Error Statistics\n");
  jout("\t\tNumber of Unrecoverable Read Errors: %" PRIu64 "\n", farmLog.error.totalUnrecoverableReadErrors);
  jout("\t\tNumber of Unrecoverable Write Errors: %" PRIu64 "\n", farmLog.error.totalUnrecoverableWriteErrors);
  jout("\t\tNumber of Reallocated Sectors: %" PRIu64 "\n", farmLog.error.totalReallocations);
  jout("\t\tNumber of Reallocated Candidate Sectors: %" PRIu64 "\n", farmLog.error.totalReallocationCanidates);
  jout("\t\tNumber of Read Recovery Attempts: %" PRIu64 "\n", farmLog.error.totalReadRecoveryAttepts);
  jout("\t\tNumber of Mechanical Start Failures: %" PRIu64 "\n", farmLog.error.totalMechanicalStartRetries);
  jout("\t\tNumber of IOEDC Errors: %" PRIu64 "\n", farmLog.error.attrIOEDCErrors);
  jout("\t\tCommand Time-Out Count Total: %" PRIu64 "\n", farmLog.error.attrCTOCount);
  jout("\t\tCommand Time-Out Over 5 Seconds Count: %" PRIu64 "\n", farmLog.error.overFiveSecCTO);
  jout("\t\tCommand Time-Out Over 7 Seconds Count: %" PRIu64 "\n", farmLog.error.overSevenSecCTO);
  // Page 4: Environment Statistics
  jout("\tFARM Log Page 4: Environment Statistics\n");
  jout("\t\tCurrent 12V Input (mV): %" PRIu64 "\n", farmLog.environment.current12v);
  jout("\t\tMinimum 12V input from last 3 SMART Summary Frames (mV): %" PRIu64 "\n", farmLog.environment.min12v);
  jout("\t\tMaximum 12V input from last 3 SMART Summary Frames (mV): %" PRIu64 "\n", farmLog.environment.max12v);
  jout("\t\tCurrent 5V Input (mV): %" PRIu64 "\n", farmLog.environment.current5v);
  jout("\t\tMinimum 5V input from last 3 SMART Summary Frames (mV): %" PRIu64 "\n", farmLog.environment.min5v);
  jout("\t\tMaximum 5V input from last 3 SMART Summary Frames (mV): %" PRIu64 "\n", farmLog.environment.max5v);
  jout("\t\t12V Power Average (mW): %" PRIu64 "\n", farmLog.environment.powerAverage12v);
  // Page 5: Reliability Statistics
  jout("\tFARM Log Page 5: Reliability Statistics\n");
  jout("\t\tError Rate (Normalized): %" PRIi64 "\n", farmLog.reliability.attrErrorRateNormal);
  jout("\t\tError Rate (Worst): %" PRIi64 "\n", farmLog.reliability.attrErrorRateWorst);
  jout("\t\tSeek Error Rate (Normalized): %" PRIi64 "\n", farmLog.reliability.attrSeekErrorRateNormal);
  jout("\t\tSeek Error Rate (Worst): %" PRIi64 "\n", farmLog.reliability.attrSeekErrorRateWorst);
  jout("\t\tHigh Priority Unload Events: %" PRIi64 "\n", farmLog.reliability.attrUnloadEventsRaw);
  jout("\t\tLBAs Corrected By Parity Sector: %" PRIi64 "\n", farmLog.reliability.numberLBACorrectedParitySector);
  // Print JSON if --json or -j is specified
  json::ref jref = jglb["seagate_farm_log"];
  // Page 0: Log Header
  json::ref jref0 = jref["page_0_log_header"];
  jref0["farm_log_version"][0] = farmLog.header.majorRev;
  jref0["farm_log_version"][1] = farmLog.header.minorRev;
  // Page 1: Drive Information
  json::ref jref1 = jref["page_1_drive_information"];
  jref1["head_load_events"] = farmLog.driveInformation.headLoadEvents;
  // Page 2: Workload Statistics
  json::ref jref2 = jref["page_2_workload_statistics"];
  // Page 3: Error Statistics
  json::ref jref3 = jref["page_3_error_statistics"];
  jref3["number_of_unrecoverable_read_errors"] = farmLog.error.totalUnrecoverableReadErrors;
  jref3["number_of_unrecoverable_write_errors"] = farmLog.error.totalUnrecoverableWriteErrors;
  jref3["number_of_reallocated_sectors"] = farmLog.error.totalReallocations;
  jref3["number_of_reallocated_candidate_sectors"] = farmLog.error.totalReallocationCanidates;
  jref3["number_of_read_recovery_attempts"] = farmLog.error.totalReadRecoveryAttepts;
  jref3["number_of_mechanical_start_failures"] = farmLog.error.totalMechanicalStartRetries;
  jref3["number_of_ioedc_errors"] = farmLog.error.attrIOEDCErrors;
  jref3["command_time_out_count_total"] = farmLog.error.attrCTOCount;
  jref3["command_time_out_over_5_seconds_count"] = farmLog.error.overFiveSecCTO;
  jref3["command_time_out_over_7_seconds_count"] = farmLog.error.overSevenSecCTO;
  // Page 4: Environment Statistics
  json::ref jref4 = jref["page_4_environment_statistics"];
  jref4["current_12_volt_input_in_mv"] = farmLog.environment.current12v;
  jref4["minimum_12_volt_input_in_mv"] = farmLog.environment.min12v;
  jref4["maximum_12_volt_input_in_mv"] = farmLog.environment.max12v;
  jref4["current_5_volt_input_in_mv"] = farmLog.environment.current5v;
  jref4["minimum_5_volt_input_in_mv"] = farmLog.environment.min5v;
  jref4["maximum_5_volt_input_in_mv"] = farmLog.environment.max5v;
  jref4["twelve_volt_power_average_in_mw"] = farmLog.environment.powerAverage12v;
  // Page 5: Reliability Statistics
  json::ref jref5 = jref["page_5_reliability_statistics"];
  jref5["error_rate_normalized"] = farmLog.reliability.attrErrorRateNormal;
  jref5["error_rate_worst"] = farmLog.reliability.attrErrorRateWorst;
  jref5["seek_error_rate_normalized"] = farmLog.reliability.attrSeekErrorRateNormal;
  jref5["seek_error_rate_worst"] = farmLog.reliability.attrSeekErrorRateWorst;
  jref5["lbas_corrected_by_parity_sector"] = farmLog.reliability.numberLBACorrectedParitySector;
  jref5["high_priority_unload_events"] = farmLog.reliability.attrUnloadEventsRaw;
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
  // "By Actuator" Parameters
  jout("\tFARM Log \"By Actuator\" Parameters\n");
  jout("\t\tHead Load Events:\n");
  jout("\t\t\tActuator 0: %" PRIu64 "\n", farmLog.actuator0.headLoadEvents);
  jout("\t\t\tActuator 1: %" PRIu64 "\n", farmLog.actuator1.headLoadEvents);
  jout("\t\t\tActuator 2: %" PRIu64 "\n", farmLog.actuator2.headLoadEvents);
  jout("\t\t\tActuator 3: %" PRIu64 "\n", farmLog.actuator3.headLoadEvents);
  jout("\t\tLBAs Corrected By Intermediate Super Parity:\n");
  jout("\t\t\tActuator 0: %" PRIu64 "\n", farmLog.actuator0.lbasCorrectedISP);
  jout("\t\t\tActuator 1: %" PRIu64 "\n", farmLog.actuator1.lbasCorrectedISP);
  jout("\t\t\tActuator 2: %" PRIu64 "\n", farmLog.actuator2.lbasCorrectedISP);
  jout("\t\t\tActuator 3: %" PRIu64 "\n", farmLog.actuator3.lbasCorrectedISP);
  jout("\t\tLBAs Corrected By Parity Sector:\n");
  jout("\t\t\tActuator 0: %" PRIu64 "\n", farmLog.actuator0.numberLBACorrectedParitySector);
  jout("\t\t\tActuator 1: %" PRIu64 "\n", farmLog.actuator1.numberLBACorrectedParitySector);
  jout("\t\t\tActuator 2: %" PRIu64 "\n", farmLog.actuator2.numberLBACorrectedParitySector);
  jout("\t\t\tActuator 3: %" PRIu64 "\n", farmLog.actuator3.numberLBACorrectedParitySector);
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
  // "By Actuator" Parameters
  json::ref jrefa = jref["by_actuator_parameters"];
  json::ref jrefa0 = jrefa["head_load_events"];
  jrefa0["actuator_0"] = farmLog.actuator0.headLoadEvents;
  jrefa0["actuator_1"] = farmLog.actuator1.headLoadEvents;
  jrefa0["actuator_2"] = farmLog.actuator2.headLoadEvents;
  jrefa0["actuator_3"] = farmLog.actuator3.headLoadEvents;
  json::ref jrefa1 = jrefa["lbas_corrected_by_intermediate_super_parity"];
  jrefa1["actuator_0"] = farmLog.actuator0.lbasCorrectedISP;
  jrefa1["actuator_1"] = farmLog.actuator1.lbasCorrectedISP;
  jrefa1["actuator_2"] = farmLog.actuator2.lbasCorrectedISP;
  jrefa1["actuator_3"] = farmLog.actuator3.lbasCorrectedISP;
  json::ref jrefa2 = jrefa["lbas_corrected_by_parity_sector"];
  jrefa2["actuator_0"] = farmLog.actuator0.numberLBACorrectedParitySector;
  jrefa2["actuator_1"] = farmLog.actuator1.numberLBACorrectedParitySector;
  jrefa2["actuator_2"] = farmLog.actuator2.numberLBACorrectedParitySector;
  jrefa2["actuator_3"] = farmLog.actuator3.numberLBACorrectedParitySector;
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
}
