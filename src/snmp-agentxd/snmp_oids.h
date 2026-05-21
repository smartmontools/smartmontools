// snmp_oids.h — OID arrays for all SMARTMON-* MIBs
//
// Enterprise: 1.3.6.1.4.1.9999.1.1
//   TC MIB:     .1
//   Common MIB: .2   objects=.1  notifications=.3
//   NVMe MIB:   .3   objects=.1  notifications=.2
//   SATA MIB:   .4   objects=.1  notifications=.2
//   SAS MIB:    .5   objects=.1  notifications=.2
//   Sensor MIB: .6   objects=.1  notifications=.2
//
// OID layout — metadata-first, groups of 3 (RowCount, LastChange, Table):

#pragma once

#include <cstring>
#include <ctime>
#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-includes.h>

// Encode time_t as 8-byte DateAndTime (RFC 2579, local time, no UTC offset).
static inline void snmp_encode_date_time(time_t t, uint8_t out[8]) {
    struct tm *tm = localtime(&t);
    if (!tm) { memset(out, 0, 8); return; }
    uint16_t y = (uint16_t)(tm->tm_year + 1900);
    out[0] = (uint8_t)(y >> 8); out[1] = (uint8_t)(y & 0xff);
    out[2] = (uint8_t)(tm->tm_mon + 1); out[3] = (uint8_t)tm->tm_mday;
    out[4] = (uint8_t)tm->tm_hour; out[5] = (uint8_t)tm->tm_min;
    out[6] = (uint8_t)tm->tm_sec; out[7] = 0;
}

// ---------------------------------------------------------------------------
// Enterprise root  (enterprises.9999.1.1)
// ---------------------------------------------------------------------------
#define SMARTMON_ENT  1, 3, 6, 1, 4, 1, 9999, 1, 1

static const oid oid_smartmon_enterprise[] = { SMARTMON_ENT };
#define OID_SMARTMON_ENTERPRISE_LEN (sizeof(oid_smartmon_enterprise) / sizeof(oid_smartmon_enterprise[0]))

// ---------------------------------------------------------------------------
// COMMON MIB (.2.1.X)
// ---------------------------------------------------------------------------
// .2.1.1 = smartmonDeviceTableRowCount
// .2.1.2 = smartmonDeviceTableLastChange
// .2.1.3 = smartmonDeviceTable
// .2.1.4 = smartmonDeviceCountNvme
// .2.1.5 = smartmonDeviceCountAta
// .2.1.6 = smartmonDeviceCountSas
// .2.3.X = notifications

static const oid oid_device_row_count[]      = { SMARTMON_ENT, 2, 1, 1 };
static const oid oid_device_last_change[]    = { SMARTMON_ENT, 2, 1, 2 };
static const oid oid_device_table[]          = { SMARTMON_ENT, 2, 1, 3 };
static const oid oid_device_count_nvme[]     = { SMARTMON_ENT, 2, 1, 4 };
static const oid oid_device_count_ata[]      = { SMARTMON_ENT, 2, 1, 5 };
static const oid oid_device_count_sas[]      = { SMARTMON_ENT, 2, 1, 6 };

// Notifications
static const oid oid_notif_device_discovered[]      = { SMARTMON_ENT, 2, 3, 1 };
static const oid oid_notif_device_removed[]         = { SMARTMON_ENT, 2, 3, 2 };
static const oid oid_notif_device_poll_failed[]     = { SMARTMON_ENT, 2, 3, 3 };

// Device table column OIDs (for trap varbinds)
static const oid oid_device_index[]           = { SMARTMON_ENT, 2, 1, 3, 1, 1 };
static const oid oid_device_name[]            = { SMARTMON_ENT, 2, 1, 3, 1, 2 };
static const oid oid_device_path[]            = { SMARTMON_ENT, 2, 1, 3, 1, 3 };
static const oid oid_device_type[]            = { SMARTMON_ENT, 2, 1, 3, 1, 4 };
static const oid oid_device_last_poll_time[]  = { SMARTMON_ENT, 2, 1, 3, 1, 5 };
static const oid oid_device_last_poll_result[]= { SMARTMON_ENT, 2, 1, 3, 1, 6 };
static const oid oid_device_poll_exit_status[]= { SMARTMON_ENT, 2, 1, 3, 1, 7 };
static const oid oid_device_uris[]            = { SMARTMON_ENT, 2, 1, 3, 1, 9 };

// ---------------------------------------------------------------------------
// NVMe MIB (.3.1.X)
// ---------------------------------------------------------------------------
// .3.1.1  = smartmonNvmeControllerTableRowCount
// .3.1.2  = smartmonNvmeControllerTableLastChange
// .3.1.3  = smartmonNvmeControllerTable
// .3.1.4  = smartmonNvmeNamespaceTableRowCount
// .3.1.5  = smartmonNvmeNamespaceTableLastChange
// .3.1.6  = smartmonNvmeNamespaceTable
// .3.1.7/.8/.9   = PowerState (unimplemented)
// .3.1.10/.11/.12 = LbaFormat (unimplemented)
// .3.1.13 = smartmonNvmeHealthTableRowCount
// .3.1.14 = smartmonNvmeHealthTableLastChange
// .3.1.15 = smartmonNvmeHealthTable
// .3.1.16 = smartmonNvmeSelfTestTableRowCount
// .3.1.17 = smartmonNvmeSelfTestTableLastChange
// .3.1.18 = smartmonNvmeSelfTestTable
// .3.1.19 = smartmonNvmeErrorLogTableRowCount
// .3.1.20 = smartmonNvmeErrorLogTableLastChange
// .3.1.21 = smartmonNvmeErrorLogTable
// .3.1.22/.23/.24 = Capability (unimplemented)

static const oid oid_nvme_controller_row_count[]   = { SMARTMON_ENT, 3, 1, 1 };
static const oid oid_nvme_controller_last_change[] = { SMARTMON_ENT, 3, 1, 2 };
static const oid oid_nvme_controller_table[]       = { SMARTMON_ENT, 3, 1, 3 };
static const oid oid_nvme_namespace_row_count[]    = { SMARTMON_ENT, 3, 1, 4 };
static const oid oid_nvme_namespace_last_change[]  = { SMARTMON_ENT, 3, 1, 5 };
static const oid oid_nvme_namespace_table[]        = { SMARTMON_ENT, 3, 1, 6 };
static const oid oid_nvme_powerstate_row_count[]   = { SMARTMON_ENT, 3, 1, 7 };
static const oid oid_nvme_powerstate_last_change[] = { SMARTMON_ENT, 3, 1, 8 };
static const oid oid_nvme_powerstate_table[]       = { SMARTMON_ENT, 3, 1, 9 };
static const oid oid_nvme_lbafmt_row_count[]       = { SMARTMON_ENT, 3, 1, 10 };
static const oid oid_nvme_lbafmt_last_change[]     = { SMARTMON_ENT, 3, 1, 11 };
static const oid oid_nvme_lbafmt_table[]           = { SMARTMON_ENT, 3, 1, 12 };
static const oid oid_nvme_health_row_count[]       = { SMARTMON_ENT, 3, 1, 13 };
static const oid oid_nvme_health_last_change[]     = { SMARTMON_ENT, 3, 1, 14 };
static const oid oid_nvme_health_table[]           = { SMARTMON_ENT, 3, 1, 15 };
static const oid oid_nvme_selftest_row_count[]     = { SMARTMON_ENT, 3, 1, 16 };
static const oid oid_nvme_selftest_last_change[]   = { SMARTMON_ENT, 3, 1, 17 };
static const oid oid_nvme_selftest_table[]         = { SMARTMON_ENT, 3, 1, 18 };
static const oid oid_nvme_error_log_row_count[]    = { SMARTMON_ENT, 3, 1, 19 };
static const oid oid_nvme_error_log_last_change[]  = { SMARTMON_ENT, 3, 1, 20 };
static const oid oid_nvme_error_log_table[]        = { SMARTMON_ENT, 3, 1, 21 };
static const oid oid_nvme_capability_row_count[]   = { SMARTMON_ENT, 3, 1, 22 };
static const oid oid_nvme_capability_last_change[] = { SMARTMON_ENT, 3, 1, 23 };
static const oid oid_nvme_capability_table[]       = { SMARTMON_ENT, 3, 1, 24 };

// NVMe health scalar for trap varbinds (column 1 in health table)
static const oid oid_nvme_health_status[]          = { SMARTMON_ENT, 3, 1, 15, 1, 1 };

// NVMe notifications
static const oid oid_notif_nvme_selftest_failed[]  = { SMARTMON_ENT, 3, 2, 1 };
static const oid oid_notif_nvme_health_changed[]   = { SMARTMON_ENT, 3, 2, 2 };

// ---------------------------------------------------------------------------
// SATA MIB (.4.1.X)
// ---------------------------------------------------------------------------
// .4.1.1  = smartmonSataInfoTableRowCount
// .4.1.2  = smartmonSataInfoTableLastChange
// .4.1.3  = smartmonSataInfoTable
// .4.1.4  = smartmonSataHealthTableRowCount
// .4.1.5  = smartmonSataHealthTableLastChange
// .4.1.6  = smartmonSataHealthTable
// .4.1.7  = smartmonSataAttrTableRowCount
// .4.1.8  = smartmonSataAttrTableLastChange
// .4.1.9  = smartmonSataAttrTable
// .4.1.10 = smartmonSataErrorLogTableRowCount
// .4.1.11 = smartmonSataErrorLogTableLastChange
// .4.1.12 = smartmonSataErrorLogTable
// .4.1.13/.14/.15 = errorCmdTable (unimplemented)
// .4.1.16 = smartmonSataSelfTestTableRowCount
// .4.1.17 = smartmonSataSelfTestTableLastChange
// .4.1.18 = smartmonSataSelfTestTable

static const oid oid_sata_info_row_count[]         = { SMARTMON_ENT, 4, 1, 1 };
static const oid oid_sata_info_last_change[]       = { SMARTMON_ENT, 4, 1, 2 };
static const oid oid_sata_info_table[]             = { SMARTMON_ENT, 4, 1, 3 };
static const oid oid_sata_health_row_count[]       = { SMARTMON_ENT, 4, 1, 4 };
static const oid oid_sata_health_last_change[]     = { SMARTMON_ENT, 4, 1, 5 };
static const oid oid_sata_health_table[]           = { SMARTMON_ENT, 4, 1, 6 };
static const oid oid_sata_attr_row_count[]         = { SMARTMON_ENT, 4, 1, 7 };
static const oid oid_sata_attr_last_change[]       = { SMARTMON_ENT, 4, 1, 8 };
static const oid oid_sata_attr_table[]             = { SMARTMON_ENT, 4, 1, 9 };
static const oid oid_sata_error_log_row_count[]    = { SMARTMON_ENT, 4, 1, 10 };
static const oid oid_sata_error_log_last_change[]  = { SMARTMON_ENT, 4, 1, 11 };
static const oid oid_sata_error_log_table[]        = { SMARTMON_ENT, 4, 1, 12 };
static const oid oid_sata_error_cmd_row_count[]    = { SMARTMON_ENT, 4, 1, 13 };
static const oid oid_sata_error_cmd_last_change[]  = { SMARTMON_ENT, 4, 1, 14 };
static const oid oid_sata_error_cmd_table[]        = { SMARTMON_ENT, 4, 1, 15 };
static const oid oid_sata_selftest_row_count[]     = { SMARTMON_ENT, 4, 1, 16 };
static const oid oid_sata_selftest_last_change[]   = { SMARTMON_ENT, 4, 1, 17 };
static const oid oid_sata_selftest_table[]         = { SMARTMON_ENT, 4, 1, 18 };

static const oid oid_sata_erc_row_count[]          = { SMARTMON_ENT, 4, 1, 19 };
static const oid oid_sata_erc_last_change[]        = { SMARTMON_ENT, 4, 1, 20 };
static const oid oid_sata_erc_table[]              = { SMARTMON_ENT, 4, 1, 21 };

static const oid oid_sata_phy_event_row_count[]    = { SMARTMON_ENT, 4, 1, 22 };
static const oid oid_sata_phy_event_last_change[]  = { SMARTMON_ENT, 4, 1, 23 };
static const oid oid_sata_phy_event_table[]        = { SMARTMON_ENT, 4, 1, 24 };

static const oid oid_sata_selective_row_count[]    = { SMARTMON_ENT, 4, 1, 25 };
static const oid oid_sata_selective_last_change[]  = { SMARTMON_ENT, 4, 1, 26 };
static const oid oid_sata_selective_table[]        = { SMARTMON_ENT, 4, 1, 27 };
static const oid oid_sata_selective_revision[]     = { SMARTMON_ENT, 4, 1, 28 };
static const oid oid_sata_selective_flags[]        = { SMARTMON_ENT, 4, 1, 29 };
static const oid oid_sata_selective_remainder_scan[]={ SMARTMON_ENT, 4, 1, 30 };
static const oid oid_sata_selective_powerup_resume[]={ SMARTMON_ENT, 4, 1, 31 };

static const oid oid_sata_logdir_row_count[]       = { SMARTMON_ENT, 4, 1, 32 };
static const oid oid_sata_logdir_last_change[]     = { SMARTMON_ENT, 4, 1, 33 };
static const oid oid_sata_logdir_table[]           = { SMARTMON_ENT, 4, 1, 34 };
static const oid oid_sata_logdir_gp_version[]      = { SMARTMON_ENT, 4, 1, 35 };
static const oid oid_sata_logdir_smart_version[]   = { SMARTMON_ENT, 4, 1, 36 };
static const oid oid_sata_logdir_smart_multisector[]={ SMARTMON_ENT, 4, 1, 37 };

static const oid oid_sata_devstat_row_count[]      = { SMARTMON_ENT, 4, 1, 38 };
static const oid oid_sata_devstat_last_change[]    = { SMARTMON_ENT, 4, 1, 39 };
static const oid oid_sata_devstat_table[]          = { SMARTMON_ENT, 4, 1, 40 };

static const oid oid_sata_pending_def_row_count[]  = { SMARTMON_ENT, 4, 1, 41 };
static const oid oid_sata_pending_def_last_change[]= { SMARTMON_ENT, 4, 1, 42 };
static const oid oid_sata_pending_def_table[]      = { SMARTMON_ENT, 4, 1, 43 };

// SATA notifications
static const oid oid_notif_sata_selftest_failed[]    = { SMARTMON_ENT, 4, 2, 1 };
static const oid oid_notif_sata_health_degraded[]    = { SMARTMON_ENT, 4, 2, 2 };
static const oid oid_notif_sata_attr_threshold_met[] = { SMARTMON_ENT, 4, 2, 3 };

// ---------------------------------------------------------------------------
// SAS MIB (.5.1.X)
// ---------------------------------------------------------------------------
// .5.1.1  = smartmonSasInfoTableRowCount
// .5.1.2  = smartmonSasInfoTableLastChange
// .5.1.3  = smartmonSasInfoTable
// .5.1.4  = smartmonSasHealthTableRowCount
// .5.1.5  = smartmonSasHealthTableLastChange
// .5.1.6  = smartmonSasHealthTable
// .5.1.7  = smartmonSasErrorCounterTableRowCount
// .5.1.8  = smartmonSasErrorCounterTableLastChange
// .5.1.9  = smartmonSasErrorCounterTable
// .5.1.10 = smartmonSasSelfTestTableRowCount
// .5.1.11 = smartmonSasSelfTestTableLastChange
// .5.1.12 = smartmonSasSelfTestTable
// .5.1.13 = smartmonSasBackgroundScanTableRowCount
// .5.1.14 = smartmonSasBackgroundScanTableLastChange
// .5.1.15 = smartmonSasBackgroundScanTable

static const oid oid_sas_info_row_count[]           = { SMARTMON_ENT, 5, 1, 1 };
static const oid oid_sas_info_last_change[]         = { SMARTMON_ENT, 5, 1, 2 };
static const oid oid_sas_info_table[]               = { SMARTMON_ENT, 5, 1, 3 };
static const oid oid_sas_health_row_count[]         = { SMARTMON_ENT, 5, 1, 4 };
static const oid oid_sas_health_last_change[]       = { SMARTMON_ENT, 5, 1, 5 };
static const oid oid_sas_health_table[]             = { SMARTMON_ENT, 5, 1, 6 };
static const oid oid_sas_error_counter_row_count[]  = { SMARTMON_ENT, 5, 1, 7 };
static const oid oid_sas_error_counter_last_change[]= { SMARTMON_ENT, 5, 1, 8 };
static const oid oid_sas_error_counter_table[]      = { SMARTMON_ENT, 5, 1, 9 };
static const oid oid_sas_selftest_row_count[]       = { SMARTMON_ENT, 5, 1, 10 };
static const oid oid_sas_selftest_last_change[]     = { SMARTMON_ENT, 5, 1, 11 };
static const oid oid_sas_selftest_table[]           = { SMARTMON_ENT, 5, 1, 12 };
static const oid oid_sas_bgscan_row_count[]         = { SMARTMON_ENT, 5, 1, 13 };
static const oid oid_sas_bgscan_last_change[]       = { SMARTMON_ENT, 5, 1, 14 };
static const oid oid_sas_bgscan_table[]             = { SMARTMON_ENT, 5, 1, 15 };

// SAS notifications
static const oid oid_notif_sas_selftest_failed[] = { SMARTMON_ENT, 5, 2, 1 };
static const oid oid_notif_sas_health_changed[]  = { SMARTMON_ENT, 5, 2, 2 };

// ---------------------------------------------------------------------------
// SENSOR MIB (.6.1.X)
// ---------------------------------------------------------------------------
// .6.1.1 = smartmonSensorTableRowCount
// .6.1.2 = smartmonSensorTableLastChange
// .6.1.3 = smartmonSensorTable

static const oid oid_sensor_row_count[]    = { SMARTMON_ENT, 6, 1, 1 };
static const oid oid_sensor_last_change[]  = { SMARTMON_ENT, 6, 1, 2 };
static const oid oid_sensor_table[]        = { SMARTMON_ENT, 6, 1, 3 };

// Sensor column OIDs (used in SNMP_NOSUCHINSTANCE & trap varbinds)
static const oid oid_sensor_type[]         = { SMARTMON_ENT, 6, 1, 3, 1, 2 };
static const oid oid_sensor_name[]         = { SMARTMON_ENT, 6, 1, 3, 1, 3 };
static const oid oid_sensor_value[]        = { SMARTMON_ENT, 6, 1, 3, 1, 7 };
static const oid oid_sensor_oper_status[]  = { SMARTMON_ENT, 6, 1, 3, 1, 8 };
static const oid oid_sensor_units[]        = { SMARTMON_ENT, 6, 1, 3, 1, 9 };
static const oid oid_sensor_high_critical[]= { SMARTMON_ENT, 6, 1, 3, 1, 12 };
static const oid oid_sensor_high_warning[] = { SMARTMON_ENT, 6, 1, 3, 1, 13 };
static const oid oid_sensor_low_warning[]  = { SMARTMON_ENT, 6, 1, 3, 1, 14 };
static const oid oid_sensor_low_critical[] = { SMARTMON_ENT, 6, 1, 3, 1, 15 };

// Sensor notifications
static const oid oid_notif_sensor_high_critical[] = { SMARTMON_ENT, 6, 2, 1 };
static const oid oid_notif_sensor_high_warning[]  = { SMARTMON_ENT, 6, 2, 2 };
static const oid oid_notif_sensor_low_warning[]   = { SMARTMON_ENT, 6, 2, 3 };
static const oid oid_notif_sensor_low_critical[]  = { SMARTMON_ENT, 6, 2, 4 };

// ---------------------------------------------------------------------------
// Convenience macro
// ---------------------------------------------------------------------------
#define OID_LEN(a) (sizeof(a) / sizeof((a)[0]))
