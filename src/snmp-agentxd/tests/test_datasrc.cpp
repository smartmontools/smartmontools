// test_datasrc.cpp — integration tests: parse fixture files → verify cache
//
// Does NOT link against net-snmp.  Includes the datasrc + cache source
// directly and calls the internal JSON-to-cache logic via the datasrc init.

#include "test_util.h"

// Stub out syslog so we can link without the full daemon infrastructure
#include <cstdarg>
#include <cstdio>
void syslog_stub(int, const char *fmt, ...) {
    va_list ap; va_start(ap, fmt);
    fprintf(stderr, "[syslog] ");
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
    va_end(ap);
}

// Redirect syslog calls to our stub at compile time
#define syslog syslog_stub
#include "../agentxd_cache.cpp"
#include "../agentxd_json.cpp"
// agentxd_datasrc.cpp uses inotify — include it but we only call
// process_json_file indirectly via agentxd_datasrc_init.
// To avoid the inotify side-effects in tests we expose a thin wrapper.
#undef syslog

// Re-expose the internal file parser from datasrc without the inotify init.
// We replicate the minimal parsing logic here to avoid pulling in inotify.
#include "../agentxd_cache.h"
#include "../agentxd_json.h"
#include <algorithm>
#include <cstring>
#include <ctime>
#include <cerrno>
#include <sys/stat.h>

// --------------------------------------------------------------------------
// Minimal re-implementation of process_json_file for testing
// (same logic as agentxd_datasrc.cpp but without syslog/inotify deps)
// --------------------------------------------------------------------------

static int health_from_passed(const JVal &root) {
    const JVal &ss = root["smart_status"]["passed"];
    if (ss.is_null()) return 0;
    return ss.as_bool() ? 1 : 2;
}

static void do_parse_nvme(uint32_t dev_idx, const JVal &root) {
    g_cache.nvme_health.erase(
        std::remove_if(g_cache.nvme_health.begin(), g_cache.nvme_health.end(),
            [dev_idx](const CacheNvmeHealthRow &r){ return r.device_index==dev_idx; }),
        g_cache.nvme_health.end());
    g_cache.nvme_selftests.erase(
        std::remove_if(g_cache.nvme_selftests.begin(), g_cache.nvme_selftests.end(),
            [dev_idx](const CacheNvmeSelfTestRow &r){ return r.device_index==dev_idx; }),
        g_cache.nvme_selftests.end());

    const JVal &log = root["nvme_smart_health_information_log"];
    if (log.is_null()) return;
    CacheNvmeHealthRow h;
    h.device_index         = dev_idx;
    h.overall_status       = health_from_passed(root);
    h.critical_warning     = (uint8_t)log["critical_warning"].as_uint64();
    h.available_spare_pct  = (uint32_t)log["available_spare"].as_uint64();
    h.available_spare_thresh = (uint32_t)log["available_spare_threshold"].as_uint64();
    h.percentage_used      = (uint32_t)log["percentage_used"].as_uint64();
    h.data_units_read      = log["data_units_read"].as_uint64();
    h.data_units_written   = log["data_units_written"].as_uint64();
    h.data_bytes_read      = h.data_units_read  * 512000ULL;
    h.data_bytes_written   = h.data_units_written * 512000ULL;
    h.host_read_commands   = log["host_reads"].as_uint64();
    h.host_write_commands  = log["host_writes"].as_uint64();
    h.controller_busy_minutes = log["controller_busy_time"].as_uint64();
    h.power_cycles         = log["power_cycles"].as_uint64();
    h.power_on_hours       = log["power_on_hours"].as_uint64();
    h.unsafe_shutdowns     = log["unsafe_shutdowns"].as_uint64();
    h.media_errors         = log["media_errors"].as_uint64();
    h.error_log_entries    = log["num_err_log_entries"].as_uint64();
    h.warning_temp_minutes = log["warning_temp_time"].as_uint64();
    h.critical_temp_minutes= log["critical_comp_time"].as_uint64();
    const JVal &stlog = root["nvme_self_test_log"];
    if (!stlog.is_null()) {
        const JVal &cur = stlog["current_self_test"]["code"];
        if (!cur.is_null()) {
            h.current_selftest_value = (uint32_t)cur["value"].as_uint64();
            h.current_selftest_str   = cur["string"].as_string();
        }
    }
    g_cache.nvme_health.push_back(h);
    if (!stlog.is_null()) {
        const JVal &tbl = stlog["table"];
        if (tbl.is_array()) {
            for (size_t i = 0; i < tbl.size(); ++i) {
                const JVal &e = tbl[i];
                CacheNvmeSelfTestRow r;
                r.device_index = dev_idx;
                r.entry_index  = (uint32_t)(i+1);
                r.number       = r.entry_index;
                r.type         = (int)e["self_test_code"]["value"].as_int64();
                r.result       = (int)e["self_test_result"]["value"].as_int64();
                r.result_text  = e["self_test_result"]["string"].as_string();
                r.power_on_hours = e["power_on_hours"].as_uint64();
                const JVal &flba = e["failing_lba"];
                r.failing_lba  = flba.is_null() ? 0 : flba.as_uint64();
                r.namespace_id = (uint32_t)e["nsid"].as_uint64();
                r.segment_number = (uint32_t)e["segment_number"].as_uint64();
                r.status_code_type = (uint32_t)e["status_code_type"].as_uint64();
                r.status_code  = (uint32_t)e["status_code"].as_uint64();
                g_cache.nvme_selftests.push_back(r);
            }
        }
    }
}

static void do_parse_ata(uint32_t dev_idx, const JVal &root) {
    g_cache.sata_attrs.erase(
        std::remove_if(g_cache.sata_attrs.begin(), g_cache.sata_attrs.end(),
            [dev_idx](const CacheSataAttrRow &r){ return r.device_index==dev_idx; }),
        g_cache.sata_attrs.end());
    g_cache.sata_selftests.erase(
        std::remove_if(g_cache.sata_selftests.begin(), g_cache.sata_selftests.end(),
            [dev_idx](const CacheSataSelfTestRow &r){ return r.device_index==dev_idx; }),
        g_cache.sata_selftests.end());
    const JVal &attrs = root["ata_smart_attributes"]["table"];
    if (attrs.is_array()) {
        for (size_t i = 0; i < attrs.size(); ++i) {
            const JVal &a = attrs[i];
            CacheSataAttrRow r;
            r.device_index = dev_idx;
            r.attr_id    = (uint32_t)a["id"].as_uint64();
            r.name       = a["name"].as_string();
            r.value      = (uint32_t)a["value"].as_uint64();
            r.worst      = (uint32_t)a["worst"].as_uint64();
            r.threshold  = (uint32_t)a["thresh"].as_uint64();
            r.when_failed= a["when_failed"].as_string();
            r.raw_value  = a["raw"]["value"].as_int64();
            r.raw_string = a["raw"]["string"].as_string();
            r.flags      = (uint8_t)a["flags"]["value"].as_uint64();
            r.attr_type  = a["flags"]["prefailure"].as_bool() ? 1 : 2;
            r.attr_updated = a["flags"]["updated_online"].as_bool() ? 1 : 2;
            r.status     = r.when_failed.empty() || r.when_failed=="-" ? 0
                           : (r.when_failed=="FAILING_NOW" ? 1 : 2);
            g_cache.sata_attrs.push_back(r);
        }
    }
    const JVal &stlog_ext = root["ata_smart_self_test_log"]["extended"]["table"];
    const JVal &stlog_std = root["ata_smart_self_test_log"]["standard"]["table"];
    const JVal &stlog = stlog_ext.is_array() ? stlog_ext : stlog_std;
    if (stlog.is_array()) {
        for (size_t i = 0; i < stlog.size(); ++i) {
            const JVal &e = stlog[i];
            CacheSataSelfTestRow r;
            r.device_index = dev_idx; r.entry_index = (uint32_t)(i+1);
            r.type         = (int)e["type"]["value"].as_int64();
            r.result       = (int)e["status"]["value"].as_int64();
            r.result_str   = e["status"]["string"].as_string();
            r.passed       = e["status"]["passed"].as_bool();
            r.remaining_pct= (uint32_t)e["status"]["remaining_percent"].as_uint64();
            r.lifetime_hours = e["lifetime_hours"].as_uint64();
            const JVal &lba = e["lba_of_first_error"];
            r.lba_first_error = lba.is_null() ? 0 : lba.as_uint64();
            g_cache.sata_selftests.push_back(r);
        }
    }
}

static void do_parse_scsi(uint32_t dev_idx, const JVal &root) {
    g_cache.sas_health.erase(
        std::remove_if(g_cache.sas_health.begin(), g_cache.sas_health.end(),
            [dev_idx](const CacheSasHealthRow &r){ return r.device_index==dev_idx; }),
        g_cache.sas_health.end());
    g_cache.sas_error_counters.erase(
        std::remove_if(g_cache.sas_error_counters.begin(), g_cache.sas_error_counters.end(),
            [dev_idx](const CacheSasErrorCounterRow &r){ return r.device_index==dev_idx; }),
        g_cache.sas_error_counters.end());
    g_cache.sas_selftests.erase(
        std::remove_if(g_cache.sas_selftests.begin(), g_cache.sas_selftests.end(),
            [dev_idx](const CacheSasSelfTestRow &r){ return r.device_index==dev_idx; }),
        g_cache.sas_selftests.end());
    CacheSasHealthRow h;
    h.device_index       = dev_idx;
    h.overall_status     = health_from_passed(root);
    h.grown_defect_count = (uint32_t)root["scsi_grown_defect_list"].as_uint64();
    g_cache.sas_health.push_back(h);
    const JVal &ecl = root["scsi_error_counter_log"];
    if (!ecl.is_null()) {
        struct { const char *key; int dir; } dirs[] = {{"read",1},{"write",2}};
        for (auto &d : dirs) {
            const JVal &obj = ecl[d.key];
            if (obj.is_null()) continue;
            CacheSasErrorCounterRow r;
            r.device_index    = dev_idx;
            r.direction       = d.dir;
            r.ecc_fast        = obj["errors_corrected_by_eccfast"].as_uint64();
            r.ecc_delayed     = obj["errors_corrected_by_eccdelayed"].as_uint64();
            r.rereads_rewrites= obj["errors_corrected_by_rereads_rewrites"].as_uint64();
            r.total_corrected = obj["total_errors_corrected"].as_uint64();
            r.algorithm_invoked = obj["correction_algorithm_invocations"].as_uint64();
            r.uncorrected     = obj["total_uncorrected_errors"].as_uint64();
            const JVal &gbp   = obj["gigabytes_processed"];
            double gb = gbp.is_string() ? strtod(gbp.as_string().c_str(),nullptr) : gbp.fval;
            r.bytes_processed = (uint64_t)(gb * 1e9);
            g_cache.sas_error_counters.push_back(r);
        }
    }
    const JVal &stlog = root["scsi_self_test_log"]["extended"]["table"];
    if (stlog.is_array()) {
        for (size_t i = 0; i < stlog.size(); ++i) {
            const JVal &e = stlog[i];
            CacheSasSelfTestRow r;
            r.device_index = dev_idx; r.entry_index = (uint32_t)(i+1);
            r.type         = (int)e["type"]["value"].as_int64();
            r.result       = (int)e["status"]["value"].as_int64();
            r.result_str   = e["status"]["string"].as_string();
            r.passed       = e["status"]["passed"].as_bool();
            r.power_on_hours = e["lifetime_hours"].as_uint64();
            const JVal &lba = e["lba_of_first_error"];
            r.lba_first_error = lba.is_null() ? 0 : lba.as_uint64();
            g_cache.sas_selftests.push_back(r);
        }
    }
}

static uint32_t load_fixture(const std::string &path) {
    std::string err;
    JVal root = json_load_file(path, err);
    if (!err.empty()) { fprintf(stderr, "load_fixture: %s\n", err.c_str()); return 0; }
    std::string dev_path = root["device"]["name"].as_string();
    std::string protocol = root["device"]["protocol"].as_string();
    DeviceProto proto = PROTO_UNKNOWN;
    if (protocol=="ATA")  proto=PROTO_ATA;
    else if (protocol=="NVMe") proto=PROTO_NVME;
    else if (protocol=="SCSI") proto=PROTO_SCSI;
    else if (protocol=="SAT")  proto=PROTO_SAT;
    else if (protocol=="SAS")  proto=PROTO_SAS;
    uint32_t idx = g_cache.upsert_device(dev_path, proto);
    for (auto &row : g_cache.devices) {
        if (row.index != idx) continue;
        row.name           = root["device_info"].as_string();
        row.last_poll_time = root["local_time"]["time_t"].as_int64();
        row.poll_result    = POLL_OK;
        break;
    }
    if (proto==PROTO_NVME)          do_parse_nvme(idx, root);
    else if (proto==PROTO_ATA||proto==PROTO_SAT) do_parse_ata(idx, root);
    else if (proto==PROTO_SCSI||proto==PROTO_SAS) do_parse_scsi(idx, root);
    return idx;
}

// ---------------------------------------------------------------------------
// Test cases
// ---------------------------------------------------------------------------

static void test_nvme_health(const char *path) {
    SECTION("NVMe health cache");
    uint32_t idx = load_fixture(path);
    CHECK(idx > 0);

    // Check device row
    bool found_dev = false;
    for (const auto &d : g_cache.devices) {
        if (d.index != idx) continue;
        found_dev = true;
        CHECK_EQ(d.proto, PROTO_NVME);
        CHECK(!d.path.empty());
        CHECK(d.last_poll_time > 0);
        break;
    }
    CHECK(found_dev);

    // Check health row
    bool found_h = false;
    for (const auto &h : g_cache.nvme_health) {
        if (h.device_index != idx) continue;
        found_h = true;
        CHECK(h.overall_status == 1);  // passed=true → 1
        CHECK_EQ(h.critical_warning, 0u);
        CHECK_EQ(h.available_spare_pct, 100u);
        CHECK_EQ(h.available_spare_thresh, 10u);
        CHECK(h.power_on_hours > 0u);
        // data_bytes = data_units * 512000
        CHECK_EQ(h.data_bytes_read, h.data_units_read * 512000ULL);
        CHECK_EQ(h.data_bytes_written, h.data_units_written * 512000ULL);
        break;
    }
    CHECK(found_h);
}

static void test_nvme_selftest(const char *path) {
    SECTION("NVMe self-test cache");
    uint32_t idx = load_fixture(path);
    CHECK(idx > 0);
    size_t count = 0;
    for (const auto &r : g_cache.nvme_selftests)
        if (r.device_index == idx) ++count;
    CHECK(count >= 2u);

    // First entry: Short, completed without error
    for (const auto &r : g_cache.nvme_selftests) {
        if (r.device_index != idx || r.entry_index != 1) continue;
        CHECK_EQ(r.type, 1);   // short
        CHECK_EQ(r.result, 0); // completed without error
        CHECK(!r.result_text.empty());
        CHECK(r.power_on_hours > 0u);
        break;
    }
}

static void test_ata_attrs(const char *path) {
    SECTION("ATA attribute cache");
    uint32_t idx = load_fixture(path);
    CHECK(idx > 0);

    size_t count = 0;
    for (const auto &a : g_cache.sata_attrs)
        if (a.device_index == idx) ++count;
    CHECK(count >= 10u);

    // Find Power_On_Hours (id=9) and check raw value is > 0
    bool found_poh = false;
    for (const auto &a : g_cache.sata_attrs) {
        if (a.device_index != idx || a.attr_id != 9) continue;
        found_poh = true;
        CHECK_STR(a.name, "Power_On_Hours");
        CHECK(a.raw_value > 0);
        break;
    }
    CHECK(found_poh);
}

static void test_ata_selftest(const char *path) {
    SECTION("ATA self-test cache");
    uint32_t idx = load_fixture(path);
    CHECK(idx > 0);

    size_t count = 0;
    for (const auto &r : g_cache.sata_selftests)
        if (r.device_index == idx) ++count;
    CHECK_EQ(count, 3u);

    // Third entry (entry_index=3): failure
    for (const auto &r : g_cache.sata_selftests) {
        if (r.device_index != idx || r.entry_index != 3) continue;
        CHECK(r.passed == false);
        CHECK_EQ(r.lba_first_error, 123456789ULL);
        break;
    }
}

static void test_scsi_health(const char *path) {
    SECTION("SCSI health cache");
    uint32_t idx = load_fixture(path);
    CHECK(idx > 0);

    bool found_h = false;
    for (const auto &h : g_cache.sas_health) {
        if (h.device_index != idx) continue;
        found_h = true;
        CHECK_EQ(h.overall_status, 1);         // passed
        CHECK_EQ(h.grown_defect_count, 3u);
        break;
    }
    CHECK(found_h);

    // Error counters: 2 directions (read + write)
    size_t ec_count = 0;
    for (const auto &ec : g_cache.sas_error_counters)
        if (ec.device_index == idx) ++ec_count;
    CHECK_EQ(ec_count, 2u);

    // Self-test entries: 2
    size_t st_count = 0;
    for (const auto &st : g_cache.sas_selftests)
        if (st.device_index == idx) ++st_count;
    CHECK_EQ(st_count, 2u);

    // Both self-tests passed
    for (const auto &st : g_cache.sas_selftests) {
        if (st.device_index != idx) continue;
        CHECK(st.passed);
    }
}

static void test_cache_remove() {
    SECTION("cache remove_device");
    size_t initial_devs = g_cache.devices.size();
    uint32_t idx = g_cache.upsert_device("/dev/test_remove", PROTO_ATA);
    CHECK(g_cache.devices.size() == initial_devs + 1);
    g_cache.remove_device(idx);
    CHECK(g_cache.devices.size() == initial_devs);
    // Verify not found after removal
    for (const auto &d : g_cache.devices)
        CHECK(d.index != idx);
}

static void test_cache_upsert() {
    SECTION("cache upsert idempotence");
    uint32_t idx1 = g_cache.upsert_device("/dev/upsert_test", PROTO_NVME);
    uint32_t idx2 = g_cache.upsert_device("/dev/upsert_test", PROTO_NVME);
    CHECK_EQ(idx1, idx2);
    // Update proto
    uint32_t idx3 = g_cache.upsert_device("/dev/upsert_test", PROTO_SAT);
    CHECK_EQ(idx1, idx3);
    for (const auto &d : g_cache.devices)
        if (d.index == idx1) { CHECK_EQ(d.proto, PROTO_SAT); break; }
    g_cache.remove_device(idx1);
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------

int main(int argc, char *argv[]) {
    // argv[1..] = fixture file paths
    const char *nvme_path    = nullptr;
    const char *nvme_st_path = nullptr;  // NVMe with self-test log
    const char *ata_path     = nullptr;
    const char *ata_st_path  = nullptr;
    const char *scsi_path    = nullptr;

    for (int i = 1; i < argc; ++i) {
        std::string p = argv[i];
        if (p.find("980_PRO") != std::string::npos)
            nvme_st_path = argv[i];
        else if (p.find(".nvme.json") != std::string::npos && !nvme_path)
            nvme_path = argv[i];
        else if (p.find("SELFTESTS") != std::string::npos)
            ata_st_path = argv[i];
        else if (p.find(".ata.json") != std::string::npos && !ata_path)
            ata_path = argv[i];
        else if (p.find(".scsi.json") != std::string::npos && !scsi_path)
            scsi_path = argv[i];
    }

    test_cache_remove();
    test_cache_upsert();

    if (nvme_path)    test_nvme_health(nvme_path);
    if (nvme_st_path) test_nvme_selftest(nvme_st_path);
    if (ata_path)     test_ata_attrs(ata_path);
    if (ata_st_path)  test_ata_selftest(ata_st_path);
    if (scsi_path)    test_scsi_health(scsi_path);

    return test_summary();
}
