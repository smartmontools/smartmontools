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

void AgentxCache::remove_device(uint32_t idx) {
    devices.erase(
        std::remove_if(devices.begin(), devices.end(),
                       [idx](const CacheDeviceRow &r) { return r.index == idx; }),
        devices.end());
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
    erase_by_device(sas_health,         idx);
    erase_by_device(sas_error_counters, idx);
    erase_by_device(sas_selftests,      idx);
    erase_by_device(sas_info,           idx);
    erase_by_device(sas_bgscan,         idx);
    erase_by_device(sensors,            idx);
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
