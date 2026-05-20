// test_datasrc.cpp — integration tests: parse fixture files → verify cache
//
// Includes agentxd_datasrc.cpp directly (with syslog stubbed) so the full
// production parse path is exercised — no parsing logic is duplicated here.

#include "test_util.h"

#include <cstdarg>
#include <cstdio>
#include <string>

// Stub syslog so we can link without the full daemon infrastructure.
// Must use C linkage because agentxd_datasrc.cpp includes <syslog.h> inside
// the #define syslog syslog_stub region, which would produce a linkage conflict
// if syslog_stub were declared with C++ linkage.
extern "C" void syslog_stub(int, const char *fmt, ...) {
    va_list ap; va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
    va_end(ap);
}

#define syslog syslog_stub
int g_verbosity = 0;
#include "../agentxd_cache.cpp"
#include "../agentxd_json.cpp"
#include "../agentxd_datasrc.cpp"
#undef syslog

#include "../agentxd_cache.h"
#include "../agentxd_datasrc.h"
#include "../agentxd_json.h"

// ---------------------------------------------------------------------------
// Load a fixture file and return the device index (0 on failure)
// ---------------------------------------------------------------------------

static uint32_t load_fixture(const std::string &path) {
    std::string err;
    JVal root = json_load_file(path, err);
    if (!err.empty()) { fprintf(stderr, "load_fixture: %s\n", err.c_str()); return 0; }
    std::string dev_path = root["device"]["name"].as_string();
    agentxd_datasrc_load_file(path);
    for (const auto &d : g_cache.devices)
        if (d.path == dev_path) return d.index;
    fprintf(stderr, "load_fixture: device '%s' not found in cache after load\n", dev_path.c_str());
    return 0;
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
    size_t count_before = g_cache.devices.size();
    g_cache.upsert_device("/dev/upsert_test", PROTO_NVME);
    CHECK_EQ(g_cache.devices.size(), count_before);
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
