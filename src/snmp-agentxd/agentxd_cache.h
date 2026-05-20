// agentxd_cache.h — in-process MIB data cache

#pragma once

#include <ctime>
#include <cstdint>
#include <string>
#include <vector>

// --------------------------------------------------------------------
// Poll result codes (mirrors SmartmonPollResult TC)
// --------------------------------------------------------------------
enum PollResult {
    POLL_UNKNOWN          = 0,
    POLL_OK               = 1,
    POLL_FAILED           = 2,
    POLL_TIMEOUT          = 3,
    POLL_PERMISSION_DENIED = 4,
};

// Protocol type (mirrors SmartmonDeviceType TC)
enum DeviceProto {
    PROTO_UNKNOWN  = 0,
    PROTO_ATA      = 1,
    PROTO_SAT      = 2,
    PROTO_SCSI     = 3,
    PROTO_SAS      = 4,
    PROTO_NVME     = 5,
};

// --------------------------------------------------------------------
// Common device row (smartmonDeviceTable)
// --------------------------------------------------------------------
struct CacheDeviceRow {
    uint32_t    index    { 0 };
    std::string name;
    std::string path;
    DeviceProto proto    { PROTO_UNKNOWN };
    time_t      last_poll_time  { 0 };
    PollResult  poll_result     { POLL_UNKNOWN };
    uint32_t    poll_exit_status { 0 };
    time_t      last_json_mtime { 0 };  // mtime of the JSON state file
};

// --------------------------------------------------------------------
// NVMe health row (smartmonNvmeHealthTable)
// --------------------------------------------------------------------
struct CacheNvmeHealthRow {
    uint32_t device_index;
    int      overall_status          { 0 };  // SmartmonHealthStatus
    uint8_t  critical_warning        { 0 };  // bitmask
    uint32_t available_spare_pct     { 0 };
    uint32_t available_spare_thresh  { 0 };
    uint32_t percentage_used         { 0 };
    uint64_t data_units_read         { 0 };
    uint64_t data_units_written      { 0 };
    uint64_t data_bytes_read         { 0 };
    uint64_t data_bytes_written      { 0 };
    uint64_t host_read_commands      { 0 };
    uint64_t host_write_commands     { 0 };
    uint64_t controller_busy_minutes { 0 };
    uint64_t power_cycles            { 0 };
    uint64_t power_on_hours          { 0 };
    uint64_t unsafe_shutdowns        { 0 };
    uint64_t media_errors            { 0 };
    uint64_t error_log_entries       { 0 };
    uint64_t warning_temp_minutes    { 0 };
    uint64_t critical_temp_minutes   { 0 };
    uint32_t current_selftest_value  { 0 };
    std::string current_selftest_str;
};

// --------------------------------------------------------------------
// NVMe self-test row (smartmonNvmeSelfTestTable)
// --------------------------------------------------------------------
struct CacheNvmeSelfTestRow {
    uint32_t    device_index;
    uint32_t    entry_index   { 0 };
    uint32_t    number        { 0 };
    int         type          { 0 };  // SmartmonNvmeSelfTestType
    int         result        { 0 };  // SmartmonNvmeSelfTestResult
    std::string result_text;
    uint64_t    power_on_hours { 0 };
    uint64_t    failing_lba    { 0 };
    uint32_t    namespace_id   { 0 };
    uint32_t    segment_number { 0 };
    uint32_t    status_code_type { 0 };
    uint32_t    status_code      { 0 };
};

// --------------------------------------------------------------------
// ATA/SATA attribute row (smartmonSataAttrTable)
// --------------------------------------------------------------------
struct CacheSataAttrRow {
    uint32_t    device_index;
    uint32_t    attr_id     { 0 };
    std::string name;
    uint8_t     flags       { 0 };   // BITS bitmask
    int         attr_type   { 0 };   // SmartmonAtaSmartAttrType
    int         attr_updated { 0 };  // SmartmonAtaSmartAttrUpdated
    uint32_t    value       { 0 };
    uint32_t    worst       { 0 };
    uint32_t    threshold   { 0 };
    int64_t     raw_value   { 0 };
    std::string raw_string;
    std::string when_failed;
    int         status      { 0 };   // SmartmonAtaSmartAttrStatus
};

// --------------------------------------------------------------------
// ATA/SATA self-test row (smartmonSataSelfTestTable)
// --------------------------------------------------------------------
struct CacheSataSelfTestRow {
    uint32_t    device_index;
    uint32_t    entry_index  { 0 };
    int         type         { 0 };  // SmartmonAtaSelfTestType
    int         result       { 0 };  // SmartmonAtaSelfTestResult
    std::string result_str;
    bool        passed       { false };
    uint32_t    remaining_pct { 0 };
    uint64_t    lifetime_hours { 0 };
    uint64_t    lba_first_error { 0 };
};

// --------------------------------------------------------------------
// SAS health row (smartmonSasHealthTable)
// --------------------------------------------------------------------
struct CacheSasHealthRow {
    uint32_t device_index;
    int      overall_status     { 0 };
    uint32_t grown_defect_count { 0 };
    uint64_t non_medium_errors  { 0 };
    bool     info_exceptions    { false };
    uint32_t pending_defects    { 0 };
};

// --------------------------------------------------------------------
// SAS error counter row (smartmonSasErrorCounterTable)
// --------------------------------------------------------------------
struct CacheSasErrorCounterRow {
    uint32_t device_index;
    int      direction          { 0 };  // SmartmonScsiErrorDirection
    uint64_t ecc_delayed        { 0 };
    uint64_t ecc_fast           { 0 };
    uint64_t rereads_rewrites   { 0 };
    uint64_t total_corrected    { 0 };
    uint64_t algorithm_invoked  { 0 };
    uint64_t bytes_processed    { 0 };
    uint64_t uncorrected        { 0 };
};

// --------------------------------------------------------------------
// SAS self-test row (smartmonSasSelfTestTable)
// --------------------------------------------------------------------
struct CacheSasSelfTestRow {
    uint32_t    device_index;
    uint32_t    entry_index  { 0 };
    int         type         { 0 };  // SmartmonSasSelfTestType
    int         result       { 0 };  // SmartmonAtaSelfTestResult (reused)
    std::string result_str;
    bool        passed       { false };
    uint64_t    power_on_hours   { 0 };
    uint64_t    lba_first_error  { 0 };
};

// --------------------------------------------------------------------
// NVMe controller row (smartmonNvmeControllerTable)
// --------------------------------------------------------------------
struct CacheNvmeControllerRow {
    uint32_t    device_index;
    std::string model_number;
    std::string serial_number;
    std::string firmware_version;
    uint32_t    pci_vendor_id      { 0 };
    uint32_t    pci_subsystem_id   { 0 };
    std::string pci_vendor_id_text;
    std::string pci_subsystem_id_text;
    uint32_t    ieee_oui           { 0 };
    uint64_t    total_capacity     { 0 };
    uint64_t    unallocated_capacity { 0 };
    uint32_t    controller_id      { 0 };
    std::string version_string;
    uint32_t    version_value      { 0 };
    uint32_t    namespace_count    { 0 };
    uint32_t    max_data_transfer_pages { 0 };  // col 11, nvme_maximum_data_transfer_pages
};

// --------------------------------------------------------------------
// NVMe capability row (smartmonNvmeCapabilityTable)
// INDEX { smartmonDeviceIndex, smartmonNvmeCapabilityIndex }
// col 1  = firmwareUpdateRaw
// col 2  = firmwareSlotCount
// col 3  = firmwareResetRequired (TruthValue)
// col 4  = optionalAdminCommandRaw
// col 5  = optionalNvmCommandRaw
// col 6  = logPageAttributesRaw
// col 7  = optionalAdminCommandText
// col 8  = optionalNvmCommandText
// col 9  = logPageAttributesText
// col 10 = capabilityIndex (NOT-ACCESSIBLE)
// --------------------------------------------------------------------
struct CacheNvmeCapabilityRow {
    uint32_t    device_index;
    uint32_t    firmware_update_raw     { 0 };
    uint32_t    firmware_slot_count     { 0 };
    bool        firmware_reset_required { false };
    uint32_t    optional_admin_cmd_raw  { 0 };
    uint32_t    optional_nvm_cmd_raw    { 0 };
    uint32_t    log_page_attr_raw       { 0 };
    std::string optional_admin_cmd_text;
    std::string optional_nvm_cmd_text;
    std::string log_page_attr_text;
};

// --------------------------------------------------------------------
// NVMe power state row (smartmonNvmePowerStateTable)
// INDEX { smartmonDeviceIndex, smartmonNvmePowerStateIndex }
// col 1  = powerStateIndex (NOT-ACCESSIBLE)
// col 2  = operational (TruthValue)
// col 3  = maxPowerMilliWatts
// col 4  = activePowerMilliWatts (optional, 0=absent)
// col 5  = idlePowerMilliWatts (optional, 0=absent)
// col 6  = readLatencyRank
// col 7  = readThroughputRank
// col 8  = writeLatencyRank
// col 9  = writeThroughputRank
// col 10 = entryLatencyUsec
// col 11 = exitLatencyUsec
// --------------------------------------------------------------------
struct CacheNvmePowerStateRow {
    uint32_t    device_index;
    uint32_t    state_index           { 0 };
    bool        operational           { true };
    uint32_t    max_power_mw          { 0 };
    bool        has_active_power      { false };
    uint32_t    active_power_mw       { 0 };
    bool        has_idle_power        { false };
    uint32_t    idle_power_mw         { 0 };
    uint32_t    read_latency_rank     { 0 };
    uint32_t    read_throughput_rank  { 0 };
    uint32_t    write_latency_rank    { 0 };
    uint32_t    write_throughput_rank { 0 };
    uint32_t    entry_latency_usec    { 0 };
    uint32_t    exit_latency_usec     { 0 };
};

// --------------------------------------------------------------------
// NVMe LBA format row (smartmonNvmeLbaFormatTable)
// INDEX { smartmonDeviceIndex, smartmonNvmeNamespaceId, smartmonNvmeLbaFormatId }
// col 1  = lbaFormatId (NOT-ACCESSIBLE)
// col 2  = lbaFormatCurrent (TruthValue)
// col 3  = lbaFormatDataSizeBytes
// col 4  = lbaFormatMetadataSizeBytes
// col 5  = lbaFormatRelativePerformance
// --------------------------------------------------------------------
struct CacheNvmeLbaFormatRow {
    uint32_t    device_index;
    uint32_t    namespace_id  { 0 };
    uint32_t    format_id     { 0 };
    bool        current       { false };
    uint32_t    data_size     { 0 };
    uint32_t    metadata_size { 0 };
    uint32_t    rel_perf      { 0 };
};

// --------------------------------------------------------------------
// NVMe namespace row (smartmonNvmeNamespaceTable)
// --------------------------------------------------------------------
struct CacheNvmeNamespaceRow {
    uint32_t    device_index;
    uint32_t    namespace_id       { 0 };
    uint64_t    size_bytes         { 0 };
    uint64_t    capacity_bytes     { 0 };
    uint64_t    utilization_bytes  { 0 };
    uint32_t    formatted_lba_size { 0 };
    uint64_t    size_blocks        { 0 };
    uint64_t    capacity_blocks    { 0 };
    uint64_t    utilization_blocks { 0 };
};

// --------------------------------------------------------------------
// NVMe error log row (smartmonNvmeErrorLogTable)
// --------------------------------------------------------------------
struct CacheNvmeErrorLogRow {
    uint32_t    device_index;
    uint32_t    entry_index   { 0 };
    uint64_t    error_count   { 0 };
    uint32_t    sqid          { 0 };
    uint32_t    command_id    { 0 };
    uint32_t    status_field  { 0 };
    uint32_t    parm_error_location { 0 };
    uint64_t    lba           { 0 };
    uint32_t    nsid          { 0 };
    uint32_t    status_code   { 0 };
    uint32_t    status_code_type { 0 };
    bool        do_not_retry  { false };
    bool        phase_tag     { false };
    std::string status_string;
};

// --------------------------------------------------------------------
// SATA info row (smartmonSataInfoTable)
// --------------------------------------------------------------------
struct CacheSataInfoRow {
    uint32_t    device_index;
    std::string model_family;
    std::string model_name;
    std::string serial_number;
    std::string firmware_version;
    std::string wwn;
    std::string ata_version_string;
    std::string sata_version_string;
    uint32_t    rotation_rate    { 0 };
    std::string form_factor;
    uint32_t    logical_block_size  { 0 };
    uint32_t    physical_block_size { 0 };
    uint64_t    user_capacity_bytes { 0 };
    bool        in_smartctl_db   { false };
    bool        smart_available  { false };
    bool        smart_enabled    { false };
    bool        trim_supported   { false };
};

// --------------------------------------------------------------------
// SATA health row (smartmonSataHealthTable)
// --------------------------------------------------------------------
struct CacheSataHealthRow {
    uint32_t    device_index;
    int         overall_status   { 0 };
    uint32_t    offline_status_value { 0 };
    std::string offline_status_string;
    uint32_t    offline_completion_secs { 0 };
    uint32_t    selftest_status_value { 0 };
    std::string selftest_status_string;
    uint32_t    polling_short_min { 0 };
    uint32_t    polling_ext_min   { 0 };
    uint32_t    polling_conv_min  { 0 };
    bool        cap_auto_offline  { false };
    bool        cap_selftests     { false };
    bool        cap_conveyance    { false };
    bool        cap_selective     { false };
    bool        cap_error_logging { false };
    bool        cap_gp_logging    { false };
    bool        sct_error_recovery { false };
    bool        sct_feature_control { false };
    bool        sct_data_table    { false };
    uint64_t    power_cycles      { 0 };
    uint64_t    power_on_hours    { 0 };
    uint32_t    error_log_count   { 0 };
};

// --------------------------------------------------------------------
// SATA error log row (smartmonSataErrorLogTable)
// --------------------------------------------------------------------
struct CacheSataErrorLogRow {
    uint32_t    device_index;
    uint32_t    entry_index  { 0 };
    uint32_t    error_number { 0 };
    uint64_t    lifetime_hours { 0 };
    std::string description;
    uint32_t    comp_reg_error  { 0 };
    uint32_t    comp_reg_status { 0 };
    uint64_t    lba             { 0 };
    uint32_t    reg_command     { 0 };
    uint32_t    reg_count       { 0 };
    uint32_t    reg_device      { 0 };
    uint32_t    reg_feature     { 0 };
    uint32_t    state_value     { 0 };
    std::string state_string;
};

// --------------------------------------------------------------------
// SAS info row (smartmonSasInfoTable)
// --------------------------------------------------------------------
struct CacheSasInfoRow {
    uint32_t    device_index;
    std::string vendor;
    std::string product;
    std::string revision;
    std::string compliance;
    std::string serial_number;
    std::string wwn;
    std::string scsi_model_name;
    uint32_t    rotation_rate    { 0 };
    std::string form_factor;
    uint32_t    logical_block_size  { 0 };
    uint32_t    physical_block_size { 0 };
    uint64_t    user_capacity_bytes { 0 };
    uint64_t    power_cycles     { 0 };
    uint64_t    power_on_hours   { 0 };
};

// --------------------------------------------------------------------
// Sensor row (smartmonSensorTable — unified physical sensor)
// SmartmonSensorDataType: celsius=3, percent=10
// SmartmonSensorDataScale: units=9 (10^0)
// --------------------------------------------------------------------
struct CacheSensorRow {
    uint32_t    device_index;
    uint32_t    sensor_index    { 0 };   // stable per-device index
    int         type            { 2 };   // SmartmonSensorDataType (unknown=2)
    std::string name;
    std::string source;                  // JSON field path
    int         scale           { 9 };   // SmartmonSensorDataScale (units=9)
    int         precision       { 0 };   // SmartmonSensorPrecision
    int32_t     value           { 0 };   // SmartmonSensorValue
    int         oper_status     { 1 };   // SmartmonSensorStatus (ok=1)
    std::string units_display;
    time_t      timestamp       { 0 };
    uint32_t    update_rate     { 0 };   // ms, 0=unknown
    bool        has_high_critical { false };
    int32_t     high_critical   { 0 };
    bool        has_high_warning  { false };
    int32_t     high_warning    { 0 };
    bool        has_low_warning   { false };
    int32_t     low_warning     { 0 };
    bool        has_low_critical  { false };
    int32_t     low_critical    { 0 };
};

// --------------------------------------------------------------------
// SATA error command row (smartmonSataErrorCmdTable)
// INDEX { smartmonDeviceIndex, smartmonSataErrorIndex, smartmonSataErrorCmdIndex }
// col 1  = errorCmdIndex (NOT-ACCESSIBLE)
// col 2  = regCommand
// col 3  = regCount
// col 4  = regDevice
// col 5  = regError   (from parent error entry completion_registers.error)
// col 6  = regFeature (registers.features)
// col 7  = regLba     (registers.lba, CounterBasedGauge64)
// col 8  = regStatus  (from parent error entry completion_registers.status)
// col 9  = timestamp  (powerup_milliseconds)
// col 10 = description (command_name)
// --------------------------------------------------------------------
struct CacheSataErrorCmdRow {
    uint32_t    device_index;
    uint32_t    error_entry_index { 0 };   // smartmonSataErrorIndex
    uint32_t    cmd_index         { 0 };   // smartmonSataErrorCmdIndex (1-based)
    uint32_t    reg_command       { 0 };
    uint32_t    reg_count         { 0 };
    uint32_t    reg_device        { 0 };
    uint32_t    reg_error         { 0 };   // from parent completion_registers.error
    uint32_t    reg_feature       { 0 };
    uint64_t    reg_lba           { 0 };
    uint32_t    reg_status        { 0 };   // from parent completion_registers.status
    uint32_t    timestamp_ms      { 0 };   // powerup_milliseconds
    std::string description;
};

// --------------------------------------------------------------------
// SAS background scan row (smartmonSasBackgroundScanTable)
// --------------------------------------------------------------------
struct CacheSasBgScanRow {
    uint32_t    device_index;
    int         status_value     { 0 };
    std::string status_string;
    uint32_t    progress_percent { 0 };
    uint64_t    scans_performed  { 0 };
    uint64_t    medium_scans     { 0 };
    std::string scan_results;
};

// --------------------------------------------------------------------
// Main cache — one global instance, owned by agentxd_cache.cpp
// All access serialised by the single-threaded AgentX select loop.
// --------------------------------------------------------------------
struct AgentxCache {
    uint32_t next_device_index { 1 };

    std::vector<CacheDeviceRow>          devices;
    std::vector<CacheNvmeHealthRow>      nvme_health;
    std::vector<CacheNvmeSelfTestRow>    nvme_selftests;
    std::vector<CacheNvmeControllerRow>  nvme_controllers;
    std::vector<CacheNvmeNamespaceRow>   nvme_namespaces;
    std::vector<CacheNvmeErrorLogRow>    nvme_error_log;
    std::vector<CacheNvmeCapabilityRow>  nvme_capabilities;
    std::vector<CacheNvmePowerStateRow>  nvme_power_states;
    std::vector<CacheNvmeLbaFormatRow>   nvme_lba_formats;
    std::vector<CacheSataAttrRow>        sata_attrs;
    std::vector<CacheSataSelfTestRow>    sata_selftests;
    std::vector<CacheSataInfoRow>        sata_info;
    std::vector<CacheSataHealthRow>      sata_health;
    std::vector<CacheSataErrorLogRow>    sata_error_log;
    std::vector<CacheSataErrorCmdRow>    sata_error_cmds;
    std::vector<CacheSasHealthRow>       sas_health;
    std::vector<CacheSasErrorCounterRow> sas_error_counters;
    std::vector<CacheSasSelfTestRow>     sas_selftests;
    std::vector<CacheSasInfoRow>         sas_info;
    std::vector<CacheSasBgScanRow>       sas_bgscan;
    std::vector<CacheSensorRow>          sensors;

    // Table metadata — updated by datasrc whenever a table's rows change
    time_t  ts_device_table       { 0 };
    time_t  ts_nvme_controller    { 0 };
    time_t  ts_nvme_namespace     { 0 };
    time_t  ts_nvme_health        { 0 };
    time_t  ts_nvme_selftest      { 0 };
    time_t  ts_nvme_error_log     { 0 };
    time_t  ts_nvme_capability    { 0 };
    time_t  ts_nvme_power_state   { 0 };
    time_t  ts_nvme_lba_format    { 0 };
    time_t  ts_sata_info          { 0 };
    time_t  ts_sata_health        { 0 };
    time_t  ts_sata_attr          { 0 };
    time_t  ts_sata_error_log     { 0 };
    time_t  ts_sata_error_cmd     { 0 };
    time_t  ts_sata_selftest      { 0 };
    time_t  ts_sas_info           { 0 };
    time_t  ts_sas_health         { 0 };
    time_t  ts_sas_error_counter  { 0 };
    time_t  ts_sas_selftest       { 0 };
    time_t  ts_sas_bgscan         { 0 };
    time_t  ts_sensor             { 0 };

    // Remove all rows belonging to a device index
    void remove_device(uint32_t device_index);

    // Find or create a device row, return its index
    uint32_t upsert_device(const std::string &path, DeviceProto proto);
};

extern AgentxCache g_cache;
