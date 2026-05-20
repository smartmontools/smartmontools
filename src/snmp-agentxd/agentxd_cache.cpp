// agentxd_cache.cpp — AgentxCache implementation

#include "agentxd_cache.h"

#include <algorithm>

AgentxCache g_cache;

template<typename Vec>
static void erase_by_device(Vec &vec, uint32_t idx) {
    vec.erase(
        std::remove_if(vec.begin(), vec.end(),
                       [idx](const typename Vec::value_type &row) {
                           return row.device_index == idx;
                       }),
        vec.end());
}

void AgentxCache::clear_device_data(uint32_t idx) {
    erase_by_device(nvme_health,        idx);
    erase_by_device(nvme_selftests,     idx);
    erase_by_device(nvme_controllers,   idx);
    erase_by_device(nvme_namespaces,    idx);
    erase_by_device(nvme_error_log,     idx);
    erase_by_device(nvme_capabilities,  idx);
    erase_by_device(nvme_power_states,  idx);
    erase_by_device(nvme_lba_formats,   idx);
    erase_by_device(sata_attrs,         idx);
    erase_by_device(sata_selftests,     idx);
    erase_by_device(sata_info,          idx);
    erase_by_device(sata_health,        idx);
    erase_by_device(sata_error_log,     idx);
    erase_by_device(sata_error_cmds,    idx);
    erase_by_device(sata_erc,           idx);
    erase_by_device(sata_phy_events,    idx);
    erase_by_device(sata_selective_tests, idx);
    erase_by_device(sata_log_dir,       idx);
    erase_by_device(sata_dev_stats,     idx);
    erase_by_device(sas_health,         idx);
    erase_by_device(sas_error_counters, idx);
    erase_by_device(sas_selftests,      idx);
    erase_by_device(sas_info,           idx);
    erase_by_device(sas_bgscan,         idx);
    erase_by_device(sensors,            idx);
}

void AgentxCache::remove_device(uint32_t idx) {
    devices.erase(
        std::remove_if(devices.begin(), devices.end(),
                       [idx](const CacheDeviceRow &r) { return r.index == idx; }),
        devices.end());
    clear_device_data(idx);
}

void AgentxCache::clear() {
    devices.clear();
    nvme_health.clear();        nvme_selftests.clear();
    nvme_controllers.clear();   nvme_namespaces.clear();
    nvme_error_log.clear();     nvme_capabilities.clear();
    nvme_power_states.clear();  nvme_lba_formats.clear();
    sata_attrs.clear();         sata_selftests.clear();
    sata_info.clear();          sata_health.clear();
    sata_error_log.clear();     sata_error_cmds.clear();
    sata_erc.clear();           sata_phy_events.clear();
    sata_selective_tests.clear(); sata_log_dir.clear();
    sata_dev_stats.clear();
    sas_health.clear();         sas_error_counters.clear();
    sas_selftests.clear();      sas_info.clear();
    sas_bgscan.clear();         sensors.clear();
    ts_device_table = ts_nvme_controller = ts_nvme_namespace  = 0;
    ts_nvme_health  = ts_nvme_selftest   = ts_nvme_error_log  = 0;
    ts_nvme_capability = ts_nvme_power_state = ts_nvme_lba_format = 0;
    ts_sata_info    = ts_sata_health     = ts_sata_attr        = 0;
    ts_sata_error_log = ts_sata_error_cmd = ts_sata_selftest   = 0;
    ts_sata_erc = ts_sata_phy_event = ts_sata_selective_test  = 0;
    ts_sata_log_dir = ts_sata_dev_stat                        = 0;
    ts_sas_info     = ts_sas_health      = ts_sas_error_counter = 0;
    ts_sas_selftest = ts_sas_bgscan      = ts_sensor            = 0;
}

const CacheDeviceRow *AgentxCache::find_device(uint32_t idx) const {
    for (const auto &d : devices)
        if (d.index == idx) return &d;
    return nullptr;
}

uint32_t AgentxCache::upsert_device(const std::string &path, DeviceProto proto) {
    for (auto &row : devices) {
        if (row.path == path) {
            row.proto = proto;
            return row.index;
        }
    }
    CacheDeviceRow row;
    row.index = next_device_index++;
    row.path  = path;
    row.proto = proto;
    devices.push_back(row);
    return row.index;
}
