// agentxd_datasrc.cpp — smartd JSON state file watcher and parser

#include "agentxd_datasrc.h"
#include "agentxd_cache.h"
#include "agentxd_json.h"

#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <dirent.h>
#include <string>
#include <syslog.h>
#include <sys/inotify.h>
#include <sys/stat.h>
#include <unistd.h>
#include <unordered_map>
#include <vector>

// ---------------------------------------------------------------------------
// PCI vendor name lookup
// ---------------------------------------------------------------------------

static std::unordered_map<uint32_t, std::string> s_pci_vendors;

static void load_pci_ids()
{
    FILE *f = fopen("/usr/share/misc/pci.ids", "r");
    if (!f) return;

    char line[256];
    while (fgets(line, sizeof(line), f)) {
        // Skip comments and blank lines
        if (line[0] == '#' || line[0] == '\n' || line[0] == '\r')
            continue;
        // Vendor lines: no leading tab, 4 hex digits then two spaces
        if (line[0] != '\t') {
            unsigned int vid = 0;
            char name[220];
            if (sscanf(line, "%04x %219[^\n]", &vid, name) == 2)
                s_pci_vendors[vid] = name;
        }
    }
    fclose(f);
}

static const std::string& pci_vendor_name(uint32_t vid)
{
    static const std::string empty;
    if (s_pci_vendors.empty())
        load_pci_ids();
    auto it = s_pci_vendors.find(vid);
    return it != s_pci_vendors.end() ? it->second : empty;
}

// ---------------------------------------------------------------------------
// Module state
// ---------------------------------------------------------------------------

static int         s_inotify_fd  { -1 };
static int         s_watch_wd    { -1 };
static std::string s_state_dir;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static bool ends_with(const std::string &s, const std::string &suffix) {
    return s.size() >= suffix.size() &&
           s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0;
}

// Extract the device path encoded in a smartd state filename.
// Filename format: <prefix><encoded-device>.<type>.json
// The prefix is the directory prefix supplied via --jsonstate.
// We receive just the basename here.
// Examples:
//   "smartd-json.dev_sda.ata.json"  → "/dev/sda",  "ATA"
//   "smartd-json.dev_nvme0.nvme.json" → "/dev/nvme0", "NVMe"
//
// smartd encodes '/' as '_' (and other specials similarly) in the base name.
// We reverse only the leading '/dev/' segment here, which is all we need for
// the display path stored in CacheDeviceRow.
static bool decode_state_filename(const std::string &basename,
                                  std::string &dev_path,
                                  DeviceProto &proto) {
    // Determine type suffix and strip it
    struct { const char *suffix; DeviceProto proto; } types[] = {
        { ".ata.json",  PROTO_ATA  },
        { ".nvme.json", PROTO_NVME },
        { ".scsi.json", PROTO_SCSI },
        { ".sat.json",  PROTO_SAT  },
        { ".sas.json",  PROTO_SAS  },
    };

    const char *type_suffix = nullptr;
    for (auto &t : types) {
        if (ends_with(basename, t.suffix)) {
            type_suffix = t.suffix;
            proto = t.proto;
            break;
        }
    }
    if (!type_suffix) return false;

    // Everything between the first '.' (after the prefix) and the type suffix
    // is the encoded device name. We don't attempt full decoding — we just
    // expose it as-is. The actual path comes from parsing the JSON.
    (void)basename;
    dev_path = ""; // filled in from JSON "device.name"
    return true;
}

// ---------------------------------------------------------------------------
// Startup validation
// ---------------------------------------------------------------------------

static bool smartd_pid_file_exists() {
    const char *candidates[] = {
        "/run/smartd.pid",
        "/var/run/smartd.pid",
    };
    for (const char *p : candidates) {
        struct stat st;
        if (stat(p, &st) == 0) return true;
    }
    return false;
}

static bool state_dir_has_json(const std::string &dir) {
    DIR *d = opendir(dir.c_str());
    if (!d) return false;
    struct dirent *ent;
    bool found = false;
    while ((ent = readdir(d)) != nullptr) {
        std::string name = ent->d_name;
        if (ends_with(name, ".ata.json") || ends_with(name, ".nvme.json") ||
            ends_with(name, ".scsi.json") || ends_with(name, ".sat.json")  ||
            ends_with(name, ".sas.json")) {
            found = true;
            break;
        }
    }
    closedir(d);
    return found;
}

// ---------------------------------------------------------------------------
// JSON → cache: individual protocol parsers
// ---------------------------------------------------------------------------

static int health_status_from_passed(const JVal &root) {
    // SmartmonHealthStatus: 1=passed, 2=failed, 0=unknown
    const JVal &ss = root["smart_status"];
    if (ss.is_null()) return 0;
    const JVal &passed = ss["passed"];
    if (passed.is_null()) return 0;
    return passed.as_bool() ? 1 : 2;
}

static void parse_nvme(uint32_t dev_idx, const JVal &root) {
    // Remove stale rows
    g_cache.sensors.erase(
        std::remove_if(g_cache.sensors.begin(), g_cache.sensors.end(),
                       [dev_idx](const CacheSensorRow &r) {
                           return r.device_index == dev_idx; }),
        g_cache.sensors.end());
    g_cache.nvme_health.erase(
        std::remove_if(g_cache.nvme_health.begin(), g_cache.nvme_health.end(),
                       [dev_idx](const CacheNvmeHealthRow &r) {
                           return r.device_index == dev_idx; }),
        g_cache.nvme_health.end());
    g_cache.nvme_selftests.erase(
        std::remove_if(g_cache.nvme_selftests.begin(), g_cache.nvme_selftests.end(),
                       [dev_idx](const CacheNvmeSelfTestRow &r) {
                           return r.device_index == dev_idx; }),
        g_cache.nvme_selftests.end());
    g_cache.nvme_controllers.erase(
        std::remove_if(g_cache.nvme_controllers.begin(), g_cache.nvme_controllers.end(),
                       [dev_idx](const CacheNvmeControllerRow &r) {
                           return r.device_index == dev_idx; }),
        g_cache.nvme_controllers.end());
    g_cache.nvme_namespaces.erase(
        std::remove_if(g_cache.nvme_namespaces.begin(), g_cache.nvme_namespaces.end(),
                       [dev_idx](const CacheNvmeNamespaceRow &r) {
                           return r.device_index == dev_idx; }),
        g_cache.nvme_namespaces.end());
    g_cache.nvme_error_log.erase(
        std::remove_if(g_cache.nvme_error_log.begin(), g_cache.nvme_error_log.end(),
                       [dev_idx](const CacheNvmeErrorLogRow &r) {
                           return r.device_index == dev_idx; }),
        g_cache.nvme_error_log.end());
    g_cache.nvme_capabilities.erase(
        std::remove_if(g_cache.nvme_capabilities.begin(), g_cache.nvme_capabilities.end(),
                       [dev_idx](const CacheNvmeCapabilityRow &r) {
                           return r.device_index == dev_idx; }),
        g_cache.nvme_capabilities.end());
    g_cache.nvme_power_states.erase(
        std::remove_if(g_cache.nvme_power_states.begin(), g_cache.nvme_power_states.end(),
                       [dev_idx](const CacheNvmePowerStateRow &r) {
                           return r.device_index == dev_idx; }),
        g_cache.nvme_power_states.end());
    g_cache.nvme_lba_formats.erase(
        std::remove_if(g_cache.nvme_lba_formats.begin(), g_cache.nvme_lba_formats.end(),
                       [dev_idx](const CacheNvmeLbaFormatRow &r) {
                           return r.device_index == dev_idx; }),
        g_cache.nvme_lba_formats.end());

    const JVal &log = root["nvme_smart_health_information_log"];
    if (log.is_null()) return;

    CacheNvmeHealthRow h;
    h.device_index          = dev_idx;
    h.overall_status        = health_status_from_passed(root);
    h.critical_warning      = static_cast<uint8_t>(log["critical_warning"].as_uint64());
    h.available_spare_pct   = static_cast<uint32_t>(log["available_spare"].as_uint64());
    h.available_spare_thresh= static_cast<uint32_t>(log["available_spare_threshold"].as_uint64());
    h.percentage_used       = static_cast<uint32_t>(log["percentage_used"].as_uint64());
    h.data_units_read       = log["data_units_read"].as_uint64();
    h.data_units_written    = log["data_units_written"].as_uint64();
    // bytes = data_units * 512 * 1000 (per NVMe spec one unit = 512000 bytes)
    h.data_bytes_read       = h.data_units_read  * 512000ULL;
    h.data_bytes_written    = h.data_units_written * 512000ULL;
    h.host_read_commands    = log["host_reads"].as_uint64();
    h.host_write_commands   = log["host_writes"].as_uint64();
    h.controller_busy_minutes = log["controller_busy_time"].as_uint64();
    h.power_cycles          = log["power_cycles"].as_uint64();
    h.power_on_hours        = log["power_on_hours"].as_uint64();
    h.unsafe_shutdowns      = log["unsafe_shutdowns"].as_uint64();
    h.media_errors          = log["media_errors"].as_uint64();
    h.error_log_entries     = log["num_err_log_entries"].as_uint64();
    h.warning_temp_minutes  = log["warning_temp_time"].as_uint64();
    h.critical_temp_minutes = log["critical_comp_time"].as_uint64();

    // Current self-test status
    const JVal &stlog = root["nvme_self_test_log"];
    if (!stlog.is_null()) {
        const JVal &cur = stlog["current_self_test"];
        if (!cur.is_null()) {
            const JVal &code = cur["code"];
            h.current_selftest_value = static_cast<uint32_t>(code["value"].as_uint64());
            h.current_selftest_str   = code["string"].as_string();
        }
    }

    g_cache.nvme_health.push_back(h);

    // Sensor rows: composite temperature, available spare, percentage used,
    // then per-sensor temperatures from temperature_sensors[]
    {
        time_t now = time(nullptr);
        uint32_t sidx = 1;

        // Sensor 1: composite temperature
        const JVal &temp_val = log["temperature"];
        if (!temp_val.is_null()) {
            CacheSensorRow sr;
            sr.device_index  = dev_idx;
            sr.sensor_index  = sidx++;
            sr.type          = 3;   // celsius
            sr.name          = "Composite";
            sr.source        = "nvme_smart_health_information_log.temperature";
            sr.scale         = 9;   // units (10^0)
            sr.precision     = 0;
            sr.value         = static_cast<int32_t>(temp_val.as_int64());
            sr.oper_status   = 1;   // ok
            sr.units_display = "Celsius";
            sr.timestamp     = now;
            {
                const JVal &thr = root["nvme_composite_temperature_threshold"];
                if (!thr.is_null()) {
                    const JVal &warn = thr["warning"];
                    if (!warn.is_null()) {
                        sr.has_high_warning = true;
                        sr.high_warning     = static_cast<int32_t>(warn.as_int64());
                    }
                    const JVal &crit = thr["critical"];
                    if (!crit.is_null()) {
                        sr.has_high_critical = true;
                        sr.high_critical     = static_cast<int32_t>(crit.as_int64());
                    }
                }
            }
            g_cache.sensors.push_back(sr);
        }

        // Sensor 2: available spare % (low_warning = spare threshold)
        {
            CacheSensorRow sr;
            sr.device_index    = dev_idx;
            sr.sensor_index    = sidx++;
            sr.type            = 10;  // percent
            sr.name            = "Available Spare";
            sr.source          = "nvme_smart_health_information_log.available_spare";
            sr.scale           = 9;
            sr.precision       = 0;
            sr.value           = static_cast<int32_t>(h.available_spare_pct);
            sr.oper_status     = 1;
            sr.units_display   = "percent";
            sr.timestamp       = now;
            sr.has_low_warning = true;
            sr.low_warning     = static_cast<int32_t>(h.available_spare_thresh);
            g_cache.sensors.push_back(sr);
        }

        // Sensor 3: percentage used
        {
            CacheSensorRow sr;
            sr.device_index  = dev_idx;
            sr.sensor_index  = sidx++;
            sr.type          = 10;  // percent
            sr.name          = "Percentage Used";
            sr.source        = "nvme_smart_health_information_log.percentage_used";
            sr.scale         = 9;
            sr.precision     = 0;
            sr.value         = static_cast<int32_t>(h.percentage_used);
            sr.oper_status   = 1;
            sr.units_display = "percent";
            sr.timestamp     = now;
            g_cache.sensors.push_back(sr);
        }

        // Sensors 10+: individual temperature sensors
        // NVMe spec provides no per-sensor thresholds; apply the composite threshold to all
        const JVal &tsensors = log["temperature_sensors"];
        if (tsensors.is_array()) {
            const JVal &thr = root["nvme_composite_temperature_threshold"];
            for (std::size_t i = 0; i < tsensors.size(); ++i) {
                const JVal &tv = tsensors[i];
                if (tv.is_null()) continue;
                CacheSensorRow sr;
                sr.device_index  = dev_idx;
                sr.sensor_index  = static_cast<uint32_t>(10 + i);
                sr.type          = 3;   // celsius
                char nbuf[32];
                snprintf(nbuf, sizeof(nbuf), "Sensor %zu", i + 1);
                sr.name          = nbuf;
                sr.source        = "nvme_smart_health_information_log.temperature_sensors";
                sr.scale         = 9;
                sr.precision     = 0;
                sr.value         = static_cast<int32_t>(tv.as_int64());
                sr.oper_status   = 1;
                sr.units_display = "Celsius";
                sr.timestamp     = now;
                if (!thr.is_null()) {
                    const JVal &warn = thr["warning"];
                    if (!warn.is_null()) {
                        sr.has_high_warning = true;
                        sr.high_warning     = static_cast<int32_t>(warn.as_int64());
                    }
                    const JVal &crit = thr["critical"];
                    if (!crit.is_null()) {
                        sr.has_high_critical = true;
                        sr.high_critical     = static_cast<int32_t>(crit.as_int64());
                    }
                }
                g_cache.sensors.push_back(sr);
            }
        }
    }

    // Controller info
    {
        CacheNvmeControllerRow ctrl;
        ctrl.device_index      = dev_idx;
        ctrl.model_number      = root["model_name"].as_string();
        ctrl.serial_number     = root["serial_number"].as_string();
        ctrl.firmware_version  = root["firmware_version"].as_string();
        const JVal &pci = root["nvme_pci_vendor"];
        ctrl.pci_vendor_id     = pci.is_null() ? 0 : static_cast<uint32_t>(pci["id"].as_uint64());
        ctrl.pci_subsystem_id  = pci.is_null() ? 0 : static_cast<uint32_t>(pci["subsystem_id"].as_uint64());
        ctrl.pci_vendor_id_text       = pci_vendor_name(ctrl.pci_vendor_id);
        ctrl.pci_subsystem_id_text    = pci_vendor_name(ctrl.pci_subsystem_id);
        ctrl.ieee_oui          = static_cast<uint32_t>(root["nvme_ieee_oui_identifier"].as_uint64());
        ctrl.total_capacity    = root["nvme_total_capacity"].as_uint64();
        ctrl.unallocated_capacity = root["nvme_unallocated_capacity"].as_uint64();
        ctrl.controller_id     = static_cast<uint32_t>(root["nvme_controller_id"].as_uint64());
        const JVal &ver = root["nvme_version"];
        if (!ver.is_null()) {
            ctrl.version_string = ver["string"].as_string();
            ctrl.version_value  = static_cast<uint32_t>(ver["value"].as_uint64());
        }
        ctrl.namespace_count   = static_cast<uint32_t>(root["nvme_number_of_namespaces"].as_uint64());
        ctrl.max_data_transfer_pages = static_cast<uint32_t>(root["nvme_maximum_data_transfer_pages"].as_uint64());
        if (!ctrl.model_number.empty() || ctrl.controller_id != 0)
            g_cache.nvme_controllers.push_back(ctrl);
    }

    // Capability table (1 row per controller)
    {
        const JVal &fw  = root["nvme_firmware_update_capabilities"];
        const JVal &adm = root["nvme_optional_admin_commands"];
        const JVal &nvm = root["nvme_optional_nvm_commands"];
        const JVal &lpa = root["nvme_log_page_attributes"];

        CacheNvmeCapabilityRow cap;
        cap.device_index = dev_idx;
        if (!fw.is_null()) {
            cap.firmware_update_raw     = static_cast<uint32_t>(fw["value"].as_uint64());
            cap.firmware_slot_count     = static_cast<uint32_t>(fw["slots"].as_uint64());
            // activiation_without_reset=true means NO reset required
            cap.firmware_reset_required = !fw["activiation_without_reset"].as_bool();
        }
        if (!adm.is_null()) {
            cap.optional_admin_cmd_raw  = static_cast<uint32_t>(adm["value"].as_uint64());
            std::string t;
            struct { const char *key; const char *label; } adm_bits[] = {
                {"security_send_receive",      "Security Send/Receive"},
                {"format_nvm",                 "Format NVM"},
                {"firmware_download",          "Firmware Download"},
                {"namespace_management",       "Namespace Management"},
                {"self_test",                  "Self-test"},
                {"directives",                 "Directives"},
                {"mi_send_receive",            "MI Send/Receive"},
                {"virtualization_management",  "Virtualization Management"},
                {"doorbell_buffer_config",     "Doorbell Buffer Config"},
                {"get_lba_status",             "Get LBA Status"},
                {"command_and_feature_lockdown","Command and Feature Lockdown"},
            };
            for (auto &b : adm_bits)
                if (adm[b.key].as_bool()) { if (!t.empty()) t += ", "; t += b.label; }
            cap.optional_admin_cmd_text = t;
        }
        if (!nvm.is_null()) {
            cap.optional_nvm_cmd_raw    = static_cast<uint32_t>(nvm["value"].as_uint64());
            std::string t;
            struct { const char *key; const char *label; } nvm_bits[] = {
                {"compare",                        "Compare"},
                {"write_uncorrectable",            "Write Uncorrectable"},
                {"dataset_management",             "Dataset Management"},
                {"write_zeroes",                   "Write Zeroes"},
                {"save_select_feature_nonzero",    "Save/Select Feature Nonzero"},
                {"reservations",                   "Reservations"},
                {"timestamp",                      "Timestamp"},
                {"verify",                         "Verify"},
                {"copy",                           "Copy"},
            };
            for (auto &b : nvm_bits)
                if (nvm[b.key].as_bool()) { if (!t.empty()) t += ", "; t += b.label; }
            cap.optional_nvm_cmd_text = t;
        }
        if (!lpa.is_null()) {
            cap.log_page_attr_raw       = static_cast<uint32_t>(lpa["value"].as_uint64());
            std::string t;
            struct { const char *key; const char *label; } lpa_bits[] = {
                {"smart_health_per_namespace",  "SMART/Health per Namespace"},
                {"commands_effects_log",        "Commands Effects Log"},
                {"extended_get_log_page_cmd",   "Extended Get Log Page"},
                {"telemetry_log",               "Telemetry Log"},
                {"persistent_event_log",        "Persistent Event Log"},
                {"supported_log_pages_log",     "Supported Log Pages Log"},
                {"telemetry_data_area_4",       "Telemetry Data Area 4"},
            };
            for (auto &b : lpa_bits)
                if (lpa[b.key].as_bool()) { if (!t.empty()) t += ", "; t += b.label; }
            cap.log_page_attr_text = t;
        }
        g_cache.nvme_capabilities.push_back(cap);
    }

    // Power state table
    {
        const JVal &ps_arr = root["nvme_power_states"];
        if (ps_arr.is_array()) {
            for (std::size_t i = 0; i < ps_arr.size(); ++i) {
                const JVal &ps = ps_arr[i];
                CacheNvmePowerStateRow r;
                r.device_index          = dev_idx;
                r.state_index           = static_cast<uint32_t>(i);
                r.operational           = !ps["non_operational_state"].as_bool();
                r.read_latency_rank     = static_cast<uint32_t>(ps["relative_read_latency"].as_uint64());
                r.read_throughput_rank  = static_cast<uint32_t>(ps["relative_read_throughput"].as_uint64());
                r.write_latency_rank    = static_cast<uint32_t>(ps["relative_write_latency"].as_uint64());
                r.write_throughput_rank = static_cast<uint32_t>(ps["relative_write_throughput"].as_uint64());
                r.entry_latency_usec    = static_cast<uint32_t>(ps["entry_latency_us"].as_uint64());
                r.exit_latency_usec     = static_cast<uint32_t>(ps["exit_latency_us"].as_uint64());
                const JVal &mp = ps["max_power"];
                if (!mp.is_null()) {
                    uint64_t upw = mp["units_per_watt"].as_uint64();
                    if (upw > 0)
                        r.max_power_mw = static_cast<uint32_t>(mp["value"].as_uint64() * 1000 / upw);
                }
                // active_power and idle_power are optional NVMe fields
                const JVal &ap = ps["active_power"];
                if (!ap.is_null()) {
                    r.has_active_power = true;
                    uint64_t upw = ap["units_per_watt"].as_uint64();
                    r.active_power_mw = (upw > 0)
                        ? static_cast<uint32_t>(ap["value"].as_uint64() * 1000 / upw) : 0;
                }
                const JVal &ip = ps["idle_power"];
                if (!ip.is_null()) {
                    r.has_idle_power = true;
                    uint64_t upw = ip["units_per_watt"].as_uint64();
                    r.idle_power_mw = (upw > 0)
                        ? static_cast<uint32_t>(ip["value"].as_uint64() * 1000 / upw) : 0;
                }
                g_cache.nvme_power_states.push_back(r);
            }
        }
    }

    // LBA format table (per namespace, per format)
    {
        const JVal &ns_arr = root["nvme_namespaces"];
        if (ns_arr.is_array()) {
            for (std::size_t i = 0; i < ns_arr.size(); ++i) {
                const JVal &ns = ns_arr[i];
                uint32_t nsid = static_cast<uint32_t>(ns["id"].as_uint64());
                const JVal &fmts = ns["lba_formats"];
                if (fmts.is_array()) {
                    for (std::size_t j = 0; j < fmts.size(); ++j) {
                        const JVal &f = fmts[j];
                        CacheNvmeLbaFormatRow r;
                        r.device_index  = dev_idx;
                        r.namespace_id  = nsid;
                        r.format_id     = static_cast<uint32_t>(j);
                        r.current       = f["formatted"].as_bool();
                        r.data_size     = static_cast<uint32_t>(f["data_bytes"].as_uint64());
                        r.metadata_size = static_cast<uint32_t>(f["metadata_bytes"].as_uint64());
                        r.rel_perf      = static_cast<uint32_t>(f["relative_performance"].as_uint64());
                        g_cache.nvme_lba_formats.push_back(r);
                    }
                }
            }
        }
    }

    // Namespace table
    {
        const JVal &ns_arr = root["nvme_namespaces"];
        if (ns_arr.is_array()) {
            for (std::size_t i = 0; i < ns_arr.size(); ++i) {
                const JVal &ns = ns_arr[i];
                CacheNvmeNamespaceRow r;
                r.device_index      = dev_idx;
                r.namespace_id      = static_cast<uint32_t>(ns["id"].as_uint64());
                r.size_bytes        = ns["size"]["bytes"].as_uint64();
                r.capacity_bytes    = ns["capacity"]["bytes"].as_uint64();
                r.utilization_bytes = ns["utilization"]["bytes"].as_uint64();
                r.formatted_lba_size= static_cast<uint32_t>(ns["formatted_lba_size"].as_uint64());
                r.size_blocks       = ns["size"]["blocks"].as_uint64();
                r.capacity_blocks   = ns["capacity"]["blocks"].as_uint64();
                r.utilization_blocks= ns["utilization"]["blocks"].as_uint64();
                g_cache.nvme_namespaces.push_back(r);
            }
        }
    }

    // Error log
    {
        const JVal &errlog = root["nvme_error_information_log"];
        if (!errlog.is_null()) {
            const JVal &tbl = errlog["table"];
            if (tbl.is_array()) {
                for (std::size_t i = 0; i < tbl.size(); ++i) {
                    const JVal &e = tbl[i];
                    CacheNvmeErrorLogRow r;
                    r.device_index  = dev_idx;
                    r.entry_index   = static_cast<uint32_t>(i + 1);
                    r.error_count   = e["error_count"].as_uint64();
                    r.sqid          = static_cast<uint32_t>(e["submission_queue_id"].as_uint64());
                    r.command_id    = static_cast<uint32_t>(e["command_id"].as_uint64());
                    const JVal &sf  = e["status_field"];
                    r.status_field  = static_cast<uint32_t>(sf["value"].as_uint64());
                    r.status_code   = static_cast<uint32_t>(sf["status_code"].as_uint64());
                    r.status_code_type = static_cast<uint32_t>(sf["status_code_type"].as_uint64());
                    r.do_not_retry  = sf["do_not_retry"].as_bool();
                    r.phase_tag     = sf["phase_tag"].as_bool();
                    r.status_string = sf["string"].as_string();
                    r.lba           = e["lba"]["value"].as_uint64();
                    r.nsid          = static_cast<uint32_t>(e["nsid"].as_uint64());
                    r.parm_error_location = static_cast<uint32_t>(e["parameter_error_location"].as_uint64());
                    g_cache.nvme_error_log.push_back(r);
                }
            }
        }
    }

    // Self-test log entries
    if (!stlog.is_null()) {
        const JVal &table = stlog["table"];
        if (table.is_array()) {
            for (std::size_t i = 0; i < table.size(); ++i) {
                const JVal &e = table[i];
                CacheNvmeSelfTestRow r;
                r.device_index  = dev_idx;
                r.entry_index   = static_cast<uint32_t>(i + 1);
                r.number        = static_cast<uint32_t>(i + 1);
                r.type          = static_cast<int>(e["self_test_code"]["value"].as_int64());
                r.result        = static_cast<int>(e["self_test_result"]["value"].as_int64());
                r.result_text   = e["self_test_result"]["string"].as_string();
                r.power_on_hours= e["power_on_hours"].as_uint64();
                const JVal &flba= e["failing_lba"];
                // 0xFFFFFFFFFFFFFFFF means no error
                r.failing_lba   = flba.is_null() ? 0 : flba.as_uint64();
                r.namespace_id  = static_cast<uint32_t>(e["nsid"].as_uint64());
                r.segment_number= static_cast<uint32_t>(e["segment_number"].as_uint64());
                r.status_code_type = static_cast<uint32_t>(e["status_code_type"].as_uint64());
                r.status_code   = static_cast<uint32_t>(e["status_code"].as_uint64());
                g_cache.nvme_selftests.push_back(r);
            }
        }
    }

    // Update last-change timestamps
    {
        time_t now_ts = time(nullptr);
        g_cache.ts_device_table       = now_ts;
        g_cache.ts_nvme_controller    = now_ts;
        g_cache.ts_nvme_namespace     = now_ts;
        g_cache.ts_nvme_health        = now_ts;
        g_cache.ts_nvme_selftest      = now_ts;
        g_cache.ts_nvme_error_log     = now_ts;
        g_cache.ts_nvme_capability    = now_ts;
        g_cache.ts_nvme_power_state   = now_ts;
        g_cache.ts_nvme_lba_format    = now_ts;
        g_cache.ts_sensor             = now_ts;
    }
}

static void parse_ata(uint32_t dev_idx, const JVal &root) {
    g_cache.sensors.erase(
        std::remove_if(g_cache.sensors.begin(), g_cache.sensors.end(),
                       [dev_idx](const CacheSensorRow &r) {
                           return r.device_index == dev_idx; }),
        g_cache.sensors.end());
    g_cache.sata_attrs.erase(
        std::remove_if(g_cache.sata_attrs.begin(), g_cache.sata_attrs.end(),
                       [dev_idx](const CacheSataAttrRow &r) {
                           return r.device_index == dev_idx; }),
        g_cache.sata_attrs.end());
    g_cache.sata_selftests.erase(
        std::remove_if(g_cache.sata_selftests.begin(), g_cache.sata_selftests.end(),
                       [dev_idx](const CacheSataSelfTestRow &r) {
                           return r.device_index == dev_idx; }),
        g_cache.sata_selftests.end());
    g_cache.sata_info.erase(
        std::remove_if(g_cache.sata_info.begin(), g_cache.sata_info.end(),
                       [dev_idx](const CacheSataInfoRow &r) {
                           return r.device_index == dev_idx; }),
        g_cache.sata_info.end());
    g_cache.sata_health.erase(
        std::remove_if(g_cache.sata_health.begin(), g_cache.sata_health.end(),
                       [dev_idx](const CacheSataHealthRow &r) {
                           return r.device_index == dev_idx; }),
        g_cache.sata_health.end());
    g_cache.sata_error_log.erase(
        std::remove_if(g_cache.sata_error_log.begin(), g_cache.sata_error_log.end(),
                       [dev_idx](const CacheSataErrorLogRow &r) {
                           return r.device_index == dev_idx; }),
        g_cache.sata_error_log.end());
    g_cache.sata_error_cmds.erase(
        std::remove_if(g_cache.sata_error_cmds.begin(), g_cache.sata_error_cmds.end(),
                       [dev_idx](const CacheSataErrorCmdRow &r) {
                           return r.device_index == dev_idx; }),
        g_cache.sata_error_cmds.end());

    const JVal &attrs = root["ata_smart_attributes"]["table"];
    if (attrs.is_array()) {
        for (std::size_t i = 0; i < attrs.size(); ++i) {
            const JVal &a = attrs[i];
            CacheSataAttrRow r;
            r.device_index = dev_idx;
            r.attr_id      = static_cast<uint32_t>(a["id"].as_uint64());
            r.name         = a["name"].as_string();
            r.value        = static_cast<uint32_t>(a["value"].as_uint64());
            r.worst        = static_cast<uint32_t>(a["worst"].as_uint64());
            r.threshold    = static_cast<uint32_t>(a["thresh"].as_uint64());
            r.when_failed  = a["when_failed"].as_string();
            r.raw_string   = a["raw"]["string"].as_string();
            // Parse the leading decimal from raw_string (matches smartctl display).
            // raw.value is the full 6-byte vendor-encoded integer which packs
            // extra fields (e.g. sub-minute counters, min/max temps) into the
            // upper bytes — not what a monitoring system wants.
            {
                const char *s = r.raw_string.c_str();
                char *endp;
                unsigned long long v = strtoull(s, &endp, 10);
                r.raw_value = (endp > s)
                    ? static_cast<int64_t>(v)
                    : a["raw"]["value"].as_int64();
            }

            const JVal &flags = a["flags"];
            r.flags        = static_cast<uint8_t>(flags["value"].as_uint64());
            // attr_type: 1=prefail, 2=old-age
            r.attr_type    = flags["prefailure"].as_bool() ? 1 : 2;
            // attr_updated: 1=always, 2=offline
            r.attr_updated = flags["updated_online"].as_bool() ? 1 : 2;

            // SmartmonAtaSmartAttrStatus: notRelevant(-1), unknown(0), ok(1),
            // failingNow(2), failedInPast(3). Threshold 0 means no threshold.
            if (r.threshold == 0) {
                r.status = -1;  // notRelevant
            } else if (r.when_failed == "FAILING_NOW") {
                r.status = 2;   // failingNow
            } else if (!r.when_failed.empty() && r.when_failed != "-") {
                r.status = 3;   // failedInPast
            } else {
                r.status = 1;   // ok
            }

            g_cache.sata_attrs.push_back(r);
        }

        // Temperature sensor: prefer attr 194, fall back to attr 190
        const CacheSataAttrRow *temp_attr = nullptr;
        for (const auto &a : g_cache.sata_attrs) {
            if (a.device_index != dev_idx) continue;
            if (a.attr_id == 194) { temp_attr = &a; break; }
            if (a.attr_id == 190 && !temp_attr) temp_attr = &a;
        }
        if (temp_attr) {
            CacheSensorRow sr;
            sr.device_index  = dev_idx;
            sr.sensor_index  = 1;
            sr.type          = 3;   // celsius
            sr.name          = "Temperature";
            sr.source        = (temp_attr->attr_id == 194)
                               ? "ata_smart_attributes.table[id=194].raw"
                               : "ata_smart_attributes.table[id=190].raw";
            sr.scale         = 9;   // units
            sr.precision     = 0;
            sr.value         = static_cast<int32_t>(temp_attr->raw_value);
            sr.oper_status   = 1;
            sr.units_display = "Celsius";
            sr.timestamp     = time(nullptr);
            {
                uint32_t rrate = static_cast<uint32_t>(root["rotation_rate"].as_uint64());
                if (rrate > 0) {
                    // Rotating HDD: use operational thresholds per industry recommendation
                    sr.has_high_warning  = true;  sr.high_warning  = 45;
                    sr.has_high_critical = true;  sr.high_critical = 60;
                    sr.has_low_warning   = true;  sr.low_warning   = 5;
                    sr.has_low_critical  = true;  sr.low_critical  = 1;
                } else {
                    // SSD: use manufacturer thresholds from JSON temperature object
                    const JVal &tmp = root["temperature"];
                    if (!tmp.is_null()) {
                        const JVal &op_max = tmp["op_limit_max"];
                        if (!op_max.is_null()) {
                            sr.has_high_warning = true;
                            sr.high_warning     = static_cast<int32_t>(op_max.as_int64());
                        }
                        const JVal &lim_max = tmp["limit_max"];
                        if (!lim_max.is_null()) {
                            sr.has_high_critical = true;
                            sr.high_critical     = static_cast<int32_t>(lim_max.as_int64());
                        }
                        const JVal &op_min = tmp["op_limit_min"];
                        if (!op_min.is_null()) {
                            sr.has_low_warning = true;
                            sr.low_warning     = static_cast<int32_t>(op_min.as_int64());
                        }
                        const JVal &lim_min = tmp["limit_min"];
                        if (!lim_min.is_null()) {
                            sr.has_low_critical = true;
                            sr.low_critical     = static_cast<int32_t>(lim_min.as_int64());
                        }
                    }
                }
            }
            g_cache.sensors.push_back(sr);
        }
    }

    // Self-test log — smartctl -x reports extended log; fall back to standard
    const JVal &stlog_ext = root["ata_smart_self_test_log"]["extended"]["table"];
    const JVal &stlog_std = root["ata_smart_self_test_log"]["standard"]["table"];
    const JVal &stlog = stlog_ext.is_array() ? stlog_ext : stlog_std;
    if (stlog.is_array()) {
        for (std::size_t i = 0; i < stlog.size(); ++i) {
            const JVal &e = stlog[i];
            CacheSataSelfTestRow r;
            r.device_index   = dev_idx;
            r.entry_index    = static_cast<uint32_t>(i + 1);
            r.type           = static_cast<int>(e["type"]["value"].as_int64());
            // SmartmonAtaSelfTestResult: completedWithoutError(1) = raw 0,
            // raw nibble 0-8 → TC value 1-9; raw 15 (in-progress) → TC 15.
            {
                int raw = static_cast<int>(e["status"]["value"].as_int64());
                if (raw >= 0 && raw <= 8) r.result = raw + 1;
                else if (raw == 15)       r.result = 15;  // inProgress
                else                      r.result = 0;   // unknown
            }
            r.result_str     = e["status"]["string"].as_string();
            r.passed         = e["status"]["passed"].as_bool();
            r.remaining_pct  = static_cast<uint32_t>(
                                  e["status"]["remaining_percent"].as_uint64());
            r.lifetime_hours = e["lifetime_hours"].as_uint64();
            const JVal &lba  = e["lba_of_first_error"];
            r.lba_first_error= lba.is_null() ? 0 : lba.as_uint64();
            g_cache.sata_selftests.push_back(r);
        }
    }

    // SATA info
    {
        CacheSataInfoRow info;
        info.device_index        = dev_idx;
        info.model_family        = root["model_family"].as_string();
        info.model_name          = root["model_name"].as_string();
        info.serial_number       = root["serial_number"].as_string();
        info.firmware_version    = root["firmware_version"].as_string();
        const JVal &wwn = root["wwn"];
        if (!wwn.is_null()) {
            char buf[64];
            snprintf(buf, sizeof(buf), "%llu %07llX %010llX",
                (unsigned long long)wwn["naa"].as_uint64(),
                (unsigned long long)wwn["oui"].as_uint64(),
                (unsigned long long)wwn["id"].as_uint64());
            info.wwn = buf;
        }
        info.ata_version_string  = root["ata_version"]["string"].as_string();
        info.sata_version_string = root["sata_version"]["string"].as_string();
        info.rotation_rate       = static_cast<uint32_t>(root["rotation_rate"].as_uint64());
        info.form_factor         = root["form_factor"]["name"].as_string();
        info.logical_block_size  = static_cast<uint32_t>(root["logical_block_size"].as_uint64());
        info.physical_block_size = static_cast<uint32_t>(root["physical_block_size"].as_uint64());
        info.user_capacity_bytes = root["user_capacity"]["bytes"].as_uint64();
        info.in_smartctl_db      = root["in_smartctl_database"].as_bool();
        const JVal &ss = root["smart_support"];
        info.smart_available     = ss["available"].as_bool();
        info.smart_enabled       = ss["enabled"].as_bool();
        info.trim_supported      = root["trim"]["supported"].as_bool();
        if (!info.model_name.empty() || info.logical_block_size > 0)
            g_cache.sata_info.push_back(info);
    }

    // SATA health
    {
        const JVal &smart = root["ata_smart_data"];
        if (!smart.is_null()) {
            CacheSataHealthRow h;
            h.device_index              = dev_idx;
            h.overall_status            = health_status_from_passed(root);
            const JVal &odc = smart["offline_data_collection"];
            h.offline_status_value      = static_cast<uint32_t>(odc["status"]["value"].as_uint64());
            h.offline_status_string     = odc["status"]["string"].as_string();
            h.offline_completion_secs   = static_cast<uint32_t>(odc["completion_seconds"].as_uint64());
            const JVal &st = smart["self_test"];
            h.selftest_status_value     = static_cast<uint32_t>(st["status"]["value"].as_uint64());
            h.selftest_status_string    = st["status"]["string"].as_string();
            h.polling_short_min         = static_cast<uint32_t>(st["polling_minutes"]["short"].as_uint64());
            h.polling_ext_min           = static_cast<uint32_t>(st["polling_minutes"]["extended"].as_uint64());
            h.polling_conv_min          = static_cast<uint32_t>(st["polling_minutes"]["conveyance"].as_uint64());
            const JVal &cap = smart["capabilities"];
            h.cap_auto_offline          = cap["attribute_autosave_enabled"].as_bool();
            h.cap_selftests             = cap["self_tests_supported"].as_bool();
            h.cap_conveyance            = cap["conveyance_self_test_supported"].as_bool();
            h.cap_selective             = cap["selective_self_test_supported"].as_bool();
            h.cap_error_logging         = cap["error_logging_supported"].as_bool();
            h.cap_gp_logging            = cap["gp_logging_supported"].as_bool();
            const JVal &sct = root["ata_sct_capabilities"];
            h.sct_error_recovery        = sct["error_recovery_control_supported"].as_bool();
            h.sct_feature_control       = sct["feature_control_supported"].as_bool();
            h.sct_data_table            = sct["data_table_supported"].as_bool();
            h.power_cycles              = root["power_cycle_count"].as_uint64();
            h.power_on_hours            = root["power_on_time"]["hours"].as_uint64();
            h.error_log_count           = static_cast<uint32_t>(
                root["ata_smart_error_log"]["extended"]["count"].as_uint64());
            g_cache.sata_health.push_back(h);
        }
    }

    // SATA error log (extended) + error cmd table
    {
        const JVal &errs = root["ata_smart_error_log"]["extended"]["table"];
        if (errs.is_array()) {
            for (std::size_t i = 0; i < errs.size(); ++i) {
                const JVal &e = errs[i];
                uint32_t entry_idx = static_cast<uint32_t>(i + 1);

                CacheSataErrorLogRow r;
                r.device_index      = dev_idx;
                r.entry_index       = entry_idx;
                r.error_number      = static_cast<uint32_t>(e["error_number"].as_uint64());
                r.lifetime_hours    = e["lifetime_hours"].as_uint64();
                r.description       = e["error_description"].as_string();
                const JVal &cr      = e["completion_registers"];
                r.comp_reg_error    = static_cast<uint32_t>(cr["error"].as_uint64());
                r.comp_reg_status   = static_cast<uint32_t>(cr["status"].as_uint64());
                r.lba               = cr["lba"].as_uint64();
                r.reg_command       = static_cast<uint32_t>(cr["command"].as_uint64());
                r.reg_count         = static_cast<uint32_t>(cr["count"].as_uint64());
                r.reg_device        = static_cast<uint32_t>(cr["device"].as_uint64());
                r.reg_feature       = static_cast<uint32_t>(cr["features"].as_uint64());
                r.state_value       = static_cast<uint32_t>(e["device_state"]["value"].as_uint64());
                r.state_string      = e["device_state"]["string"].as_string();
                g_cache.sata_error_log.push_back(r);

                // Error cmd sub-table (previous commands leading to this error)
                const JVal &cmds = e["previous_commands"];
                if (cmds.is_array()) {
                    for (std::size_t j = 0; j < cmds.size(); ++j) {
                        const JVal &c    = cmds[j];
                        const JVal &regs = c["registers"];
                        CacheSataErrorCmdRow cmd;
                        cmd.device_index      = dev_idx;
                        cmd.error_entry_index = entry_idx;
                        cmd.cmd_index         = static_cast<uint32_t>(j + 1);
                        cmd.reg_command       = static_cast<uint32_t>(regs["command"].as_uint64());
                        cmd.reg_count         = static_cast<uint32_t>(regs["count"].as_uint64());
                        cmd.reg_device        = static_cast<uint32_t>(regs["device"].as_uint64());
                        cmd.reg_error         = r.comp_reg_error;
                        cmd.reg_feature       = static_cast<uint32_t>(regs["features"].as_uint64());
                        cmd.reg_lba           = regs["lba"].as_uint64();
                        cmd.reg_status        = r.comp_reg_status;
                        cmd.timestamp_ms      = static_cast<uint32_t>(c["powerup_milliseconds"].as_uint64());
                        cmd.description       = c["command_name"].as_string();
                        g_cache.sata_error_cmds.push_back(cmd);
                    }
                }
            }
        }
    }

    // Update last-change timestamps
    {
        time_t now_ts = time(nullptr);
        g_cache.ts_device_table   = now_ts;
        g_cache.ts_sata_info      = now_ts;
        g_cache.ts_sata_health    = now_ts;
        g_cache.ts_sata_attr      = now_ts;
        g_cache.ts_sata_error_log = now_ts;
        g_cache.ts_sata_error_cmd = now_ts;
        g_cache.ts_sata_selftest  = now_ts;
        g_cache.ts_sensor         = now_ts;
    }
}

static void parse_scsi(uint32_t dev_idx, const JVal &root) {
    g_cache.sensors.erase(
        std::remove_if(g_cache.sensors.begin(), g_cache.sensors.end(),
                       [dev_idx](const CacheSensorRow &r) {
                           return r.device_index == dev_idx; }),
        g_cache.sensors.end());
    g_cache.sas_health.erase(
        std::remove_if(g_cache.sas_health.begin(), g_cache.sas_health.end(),
                       [dev_idx](const CacheSasHealthRow &r) {
                           return r.device_index == dev_idx; }),
        g_cache.sas_health.end());
    g_cache.sas_error_counters.erase(
        std::remove_if(g_cache.sas_error_counters.begin(), g_cache.sas_error_counters.end(),
                       [dev_idx](const CacheSasErrorCounterRow &r) {
                           return r.device_index == dev_idx; }),
        g_cache.sas_error_counters.end());
    g_cache.sas_selftests.erase(
        std::remove_if(g_cache.sas_selftests.begin(), g_cache.sas_selftests.end(),
                       [dev_idx](const CacheSasSelfTestRow &r) {
                           return r.device_index == dev_idx; }),
        g_cache.sas_selftests.end());
    g_cache.sas_info.erase(
        std::remove_if(g_cache.sas_info.begin(), g_cache.sas_info.end(),
                       [dev_idx](const CacheSasInfoRow &r) {
                           return r.device_index == dev_idx; }),
        g_cache.sas_info.end());
    g_cache.sas_bgscan.erase(
        std::remove_if(g_cache.sas_bgscan.begin(), g_cache.sas_bgscan.end(),
                       [dev_idx](const CacheSasBgScanRow &r) {
                           return r.device_index == dev_idx; }),
        g_cache.sas_bgscan.end());

    CacheSasHealthRow h;
    h.device_index      = dev_idx;
    h.overall_status    = health_status_from_passed(root);
    h.grown_defect_count= static_cast<uint32_t>(root["scsi_grown_defect_list"].as_uint64());
    g_cache.sas_health.push_back(h);

    // Error counter log: read and write directions
    const JVal &ecl = root["scsi_error_counter_log"];
    if (!ecl.is_null()) {
        struct { const char *key; int dir; } dirs[] = {
            { "read",  1 },
            { "write", 2 },
        };
        for (auto &d : dirs) {
            const JVal &dir_obj = ecl[d.key];
            if (dir_obj.is_null()) continue;
            CacheSasErrorCounterRow r;
            r.device_index   = dev_idx;
            r.direction      = d.dir;
            r.ecc_fast       = dir_obj["errors_corrected_by_eccfast"].as_uint64();
            r.ecc_delayed    = dir_obj["errors_corrected_by_eccdelayed"].as_uint64();
            r.rereads_rewrites = dir_obj["errors_corrected_by_rereads_rewrites"].as_uint64();
            r.total_corrected= dir_obj["total_errors_corrected"].as_uint64();
            r.algorithm_invoked = dir_obj["correction_algorithm_invocations"].as_uint64();
            r.uncorrected    = dir_obj["total_uncorrected_errors"].as_uint64();
            // gigabytes_processed may be a string like "47221.194"
            const JVal &gbp  = dir_obj["gigabytes_processed"];
            double gb = gbp.is_string() ? strtod(gbp.as_string().c_str(), nullptr)
                                        : gbp.fval;
            r.bytes_processed= static_cast<uint64_t>(gb * 1e9);
            g_cache.sas_error_counters.push_back(r);
        }
    }

    // Self-test log
    const JVal &stlog = root["scsi_self_test_log"]["extended"]["table"];
    if (stlog.is_array()) {
        for (std::size_t i = 0; i < stlog.size(); ++i) {
            const JVal &e = stlog[i];
            CacheSasSelfTestRow r;
            r.device_index  = dev_idx;
            r.entry_index   = static_cast<uint32_t>(i + 1);
            r.type          = static_cast<int>(e["type"]["value"].as_int64());
            r.result        = static_cast<int>(e["status"]["value"].as_int64());
            r.result_str    = e["status"]["string"].as_string();
            r.passed        = e["status"]["passed"].as_bool();
            r.power_on_hours= e["lifetime_hours"].as_uint64();
            const JVal &lba = e["lba_of_first_error"];
            r.lba_first_error = lba.is_null() ? 0 : lba.as_uint64();
            g_cache.sas_selftests.push_back(r);
        }
    }

    // SAS info
    {
        CacheSasInfoRow info;
        info.device_index        = dev_idx;
        info.vendor              = root["scsi_vendor"].as_string();
        info.product             = root["scsi_product"].as_string();
        info.revision            = root["scsi_revision"].as_string();
        info.compliance          = root["scsi_version"].as_string();
        info.serial_number       = root["serial_number"].as_string();
        const JVal &wwn2 = root["wwn"];
        if (!wwn2.is_null()) {
            char buf[64];
            snprintf(buf, sizeof(buf), "%llu %07llX %010llX",
                (unsigned long long)wwn2["naa"].as_uint64(),
                (unsigned long long)wwn2["oui"].as_uint64(),
                (unsigned long long)wwn2["id"].as_uint64());
            info.wwn = buf;
        }
        info.scsi_model_name     = root["scsi_model_name"].as_string();
        if (info.scsi_model_name.empty())
            info.scsi_model_name = info.vendor + " " + info.product;
        info.rotation_rate       = static_cast<uint32_t>(root["rotation_rate"].as_uint64());
        info.form_factor         = root["form_factor"]["name"].as_string();
        info.logical_block_size  = static_cast<uint32_t>(root["logical_block_size"].as_uint64());
        info.physical_block_size = static_cast<uint32_t>(root["physical_block_size"].as_uint64());
        info.user_capacity_bytes = root["user_capacity"]["bytes"].as_uint64();
        info.power_cycles        = root["power_cycle_count"].as_uint64();
        info.power_on_hours      = root["power_on_time"]["hours"].as_uint64();
        if (!info.vendor.empty() || !info.product.empty() || info.user_capacity_bytes > 0)
            g_cache.sas_info.push_back(info);
    }

    // Background scan
    {
        const JVal &bgs = root["scsi_background_scan"];
        if (!bgs.is_null()) {
            CacheSasBgScanRow r;
            r.device_index      = dev_idx;
            const JVal &st2 = bgs["status"];
            r.status_value      = static_cast<int>(st2["value"].as_int64());
            r.status_string     = st2["string"].as_string();
            r.progress_percent  = static_cast<uint32_t>(bgs["scan_progress"].as_uint64());
            r.medium_scans      = bgs["number_of_background_medium_scans_performed"].as_uint64();
            r.scans_performed   = bgs["number_of_background_pre_scan_scans_performed"].as_uint64();
            g_cache.sas_bgscan.push_back(r);
        }
    }

    // Update last-change timestamps
    {
        time_t now_ts = time(nullptr);
        g_cache.ts_device_table      = now_ts;
        g_cache.ts_sas_info          = now_ts;
        g_cache.ts_sas_health        = now_ts;
        g_cache.ts_sas_error_counter = now_ts;
        g_cache.ts_sas_selftest      = now_ts;
        g_cache.ts_sas_bgscan        = now_ts;
    }
}

// ---------------------------------------------------------------------------
// Logging helpers
// ---------------------------------------------------------------------------

static void log_device_loaded(uint32_t dev_idx, DeviceProto proto,
                               const std::string &dev_path) {
    char buf[320];

    if (proto == PROTO_NVME) {
        for (const auto &h : g_cache.nvme_health) {
            if (h.device_index != dev_idx) continue;
            std::string model;
            for (const auto &c : g_cache.nvme_controllers)
                if (c.device_index == dev_idx) { model = c.model_number; break; }
            snprintf(buf, sizeof(buf),
                "loaded %s [NVMe] model=\"%s\" spare=%u%% used=%u%% "
                "poh=%lluh media_err=%llu",
                dev_path.c_str(), model.c_str(),
                h.available_spare_pct, h.percentage_used,
                (unsigned long long)h.power_on_hours,
                (unsigned long long)h.media_errors);
            syslog(LOG_INFO, "%s", buf);
            return;
        }
    } else if (proto == PROTO_ATA || proto == PROTO_SAT) {
        for (const auto &h : g_cache.sata_health) {
            if (h.device_index != dev_idx) continue;
            std::string model;
            for (const auto &i : g_cache.sata_info)
                if (i.device_index == dev_idx) { model = i.model_name; break; }
            size_t n_attrs = 0;
            for (const auto &a : g_cache.sata_attrs)
                if (a.device_index == dev_idx) ++n_attrs;
            const char *health_str = (h.overall_status == 1) ? "passed"
                                   : (h.overall_status == 2) ? "FAILED" : "unknown";
            snprintf(buf, sizeof(buf),
                "loaded %s [%s] model=\"%s\" health=%s poh=%lluh attrs=%zu",
                dev_path.c_str(), (proto == PROTO_SAT) ? "SAT" : "ATA",
                model.c_str(), health_str,
                (unsigned long long)h.power_on_hours, n_attrs);
            syslog(LOG_INFO, "%s", buf);
            return;
        }
    } else if (proto == PROTO_SCSI || proto == PROTO_SAS) {
        for (const auto &h : g_cache.sas_health) {
            if (h.device_index != dev_idx) continue;
            const char *health_str = (h.overall_status == 1) ? "passed"
                                   : (h.overall_status == 2) ? "FAILED" : "unknown";
            snprintf(buf, sizeof(buf),
                "loaded %s [%s] health=%s grown_defects=%u",
                dev_path.c_str(), (proto == PROTO_SAS) ? "SAS" : "SCSI",
                health_str, h.grown_defect_count);
            syslog(LOG_INFO, "%s", buf);
            return;
        }
    }
    syslog(LOG_INFO, "loaded %s [proto=%d] (no health data)", dev_path.c_str(), (int)proto);
}

static void log_cache_summary() {
    size_t n_total = g_cache.devices.size();
    size_t n_nvme = 0, n_ata = 0, n_sas = 0;
    for (const auto &d : g_cache.devices) {
        switch (d.proto) {
        case PROTO_NVME:                  ++n_nvme; break;
        case PROTO_ATA: case PROTO_SAT:   ++n_ata;  break;
        case PROTO_SCSI: case PROTO_SAS:  ++n_sas;  break;
        default: break;
        }
    }
    syslog(LOG_NOTICE,
        "cache: %zu device(s) — %zu NVMe, %zu ATA/SAT, %zu SAS/SCSI",
        n_total, n_nvme, n_ata, n_sas);
}

// ---------------------------------------------------------------------------
// Parse one JSON state file into the cache
// ---------------------------------------------------------------------------

static void process_json_file(const std::string &filepath) {
    struct stat st;
    if (stat(filepath.c_str(), &st) != 0) {
        syslog(LOG_WARNING, "stat(%s): %s", filepath.c_str(), strerror(errno));
        return;
    }
    time_t mtime = st.st_mtime;

    std::string err;
    JVal root = json_load_file(filepath, err);
    if (!err.empty()) {
        syslog(LOG_WARNING, "%s: JSON parse error: %s", filepath.c_str(), err.c_str());
        return;
    }

    // Device path and protocol come from the JSON itself
    std::string dev_path = root["device"]["name"].as_string();
    std::string protocol = root["device"]["protocol"].as_string();

    if (dev_path.empty()) {
        syslog(LOG_WARNING, "%s: missing device.name", filepath.c_str());
        return;
    }

    // Map protocol string to enum
    DeviceProto proto = PROTO_UNKNOWN;
    if (protocol == "ATA")  proto = PROTO_ATA;
    else if (protocol == "NVMe") proto = PROTO_NVME;
    else if (protocol == "SCSI") proto = PROTO_SCSI;
    else if (protocol == "SAT")  proto = PROTO_SAT;
    else if (protocol == "SAS")  proto = PROTO_SAS;

    uint32_t dev_idx = g_cache.upsert_device(dev_path, proto);

    // Update device row metadata
    for (auto &row : g_cache.devices) {
        if (row.index != dev_idx) continue;
        row.name           = root["device_info"].as_string();
        row.last_poll_time = root["local_time"]["time_t"].as_int64();
        row.last_json_mtime= mtime;
        row.poll_result    = POLL_OK;
        break;
    }

    // Parse protocol-specific data
    if (proto == PROTO_NVME)
        parse_nvme(dev_idx, root);
    else if (proto == PROTO_ATA || proto == PROTO_SAT)
        parse_ata(dev_idx, root);
    else if (proto == PROTO_SCSI || proto == PROTO_SAS)
        parse_scsi(dev_idx, root);

    log_device_loaded(dev_idx, proto, dev_path);
}

// ---------------------------------------------------------------------------
// Initial directory scan
// ---------------------------------------------------------------------------

static void scan_state_dir() {
    DIR *d = opendir(s_state_dir.c_str());
    if (!d) {
        syslog(LOG_ERR, "opendir(%s): %s", s_state_dir.c_str(), strerror(errno));
        return;
    }
    struct dirent *ent;
    while ((ent = readdir(d)) != nullptr) {
        std::string name = ent->d_name;
        if (name == "." || name == "..") continue;
        std::string dev_path;
        DeviceProto proto;
        if (!decode_state_filename(name, dev_path, proto)) continue;
        process_json_file(s_state_dir + "/" + name);
    }
    closedir(d);
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

bool agentxd_datasrc_init(const std::string &state_dir) {
    s_state_dir = state_dir;
    // Ensure trailing slash is absent for consistent path concatenation
    while (!s_state_dir.empty() && s_state_dir.back() == '/')
        s_state_dir.pop_back();

    // Validate that the directory exists
    struct stat st;
    if (stat(s_state_dir.c_str(), &st) != 0 || !S_ISDIR(st.st_mode)) {
        syslog(LOG_ERR, "state_dir '%s' does not exist or is not a directory",
               s_state_dir.c_str());
        return false;
    }

    // Detect smartd running without --jsonstate
    if (smartd_pid_file_exists() && !state_dir_has_json(s_state_dir)) {
        syslog(LOG_ERR,
               "smartd appears to be running but '%s' contains no JSON state files. "
               "Configure smartd with '--jsonstate %s/' in smartd.conf and restart smartd.",
               s_state_dir.c_str(), s_state_dir.c_str());
        return false;
    }

    // Set up inotify
    s_inotify_fd = inotify_init1(IN_NONBLOCK | IN_CLOEXEC);
    if (s_inotify_fd < 0) {
        syslog(LOG_ERR, "inotify_init1: %s", strerror(errno));
        return false;
    }

    s_watch_wd = inotify_add_watch(s_inotify_fd, s_state_dir.c_str(),
                                   IN_CLOSE_WRITE | IN_MOVED_TO);
    if (s_watch_wd < 0) {
        syslog(LOG_ERR, "inotify_add_watch(%s): %s",
               s_state_dir.c_str(), strerror(errno));
        close(s_inotify_fd);
        s_inotify_fd = -1;
        return false;
    }

    // Load current state from all existing JSON files
    scan_state_dir();
    log_cache_summary();
    return true;
}

void agentxd_datasrc_handle_events() {
    if (s_inotify_fd < 0) return;

    // inotify events can be batched; read all available
    char buf[4096] __attribute__((aligned(__alignof__(struct inotify_event))));
    for (;;) {
        ssize_t n = read(s_inotify_fd, buf, sizeof(buf));
        if (n <= 0) break; // EAGAIN when no more events

        const char *p = buf;
        while (p < buf + n) {
            const struct inotify_event *ev =
                reinterpret_cast<const struct inotify_event *>(p);
            p += sizeof(struct inotify_event) + ev->len;

            if (ev->len == 0) continue;
            std::string name = ev->name;

            std::string dev_path;
            DeviceProto proto;
            if (!decode_state_filename(name, dev_path, proto)) continue;

            process_json_file(s_state_dir + "/" + name);
        }
    }
}

int agentxd_datasrc_fd() {
    return s_inotify_fd;
}

void agentxd_datasrc_check_staleness(unsigned cache_timeout) {
    time_t now = time(nullptr);
    time_t stale_threshold = static_cast<time_t>(cache_timeout) * 2;

    for (const auto &dev : g_cache.devices) {
        if (dev.last_json_mtime == 0) continue;
        time_t age = now - dev.last_json_mtime;
        if (age > stale_threshold) {
            syslog(LOG_WARNING,
                   "device %s: JSON state file not updated for %ld seconds "
                   "(threshold %u s). Is smartd still running with --jsonstate?",
                   dev.path.c_str(), (long)age, cache_timeout * 2);
        }
    }
}

void agentxd_datasrc_shutdown() {
    if (s_watch_wd >= 0 && s_inotify_fd >= 0)
        inotify_rm_watch(s_inotify_fd, s_watch_wd);
    if (s_inotify_fd >= 0) {
        close(s_inotify_fd);
        s_inotify_fd = -1;
    }
    s_watch_wd = -1;
}
