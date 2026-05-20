// test_json.cpp — unit tests for agentxd_json parser

#include "test_util.h"
#include "../agentxd_json.h"

#include <climits>
#include <cstdio>

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static JVal parse(const char *text) {
    std::string err;
    JVal v = json_parse(text, err);
    if (!err.empty()) {
        fprintf(stderr, "parse error: %s\n", err.c_str());
    }
    return v;
}

static bool parse_fails(const char *text) {
    std::string err;
    JVal v = json_parse(text, err);
    return !err.empty();
}

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

static void test_literals() {
    SECTION("literals");
    JVal v;

    v = parse("null");
    CHECK(v.is_null());

    v = parse("true");
    CHECK(v.is_bool());
    CHECK(v.as_bool() == true);

    v = parse("false");
    CHECK(v.is_bool());
    CHECK(v.as_bool() == false);
}

static void test_numbers() {
    SECTION("numbers");
    JVal v;

    v = parse("0");
    CHECK(v.is_int());
    CHECK_EQ(v.as_int64(), 0LL);

    v = parse("42");
    CHECK(v.is_int());
    CHECK_EQ(v.as_int64(), 42LL);

    v = parse("-17");
    CHECK(v.is_int());
    CHECK_EQ(v.as_int64(), -17LL);

    // Large unsigned
    v = parse("18446744073709551615");
    CHECK(v.is_uint() || v.is_int());
    CHECK_EQ(v.as_uint64(), 18446744073709551615ULL);

    // Float
    v = parse("3.14");
    CHECK(v.type == JVal::J_FLOAT);

    v = parse("1e10");
    CHECK(v.type == JVal::J_FLOAT);

    // Negative stays int
    v = parse("-1");
    CHECK_EQ(v.as_int64(), -1LL);
}

static void test_strings() {
    SECTION("strings");
    JVal v;

    v = parse("\"hello\"");
    CHECK(v.is_string());
    CHECK_STR(v.as_string(), "hello");

    v = parse("\"tab\\there\"");
    CHECK_STR(v.as_string(), "tab\there");

    v = parse("\"new\\nline\"");
    CHECK_STR(v.as_string(), "new\nline");

    v = parse("\"quote\\\"inside\"");
    CHECK_STR(v.as_string(), "quote\"inside");

    v = parse("\"slash\\/path\"");
    CHECK_STR(v.as_string(), "slash/path");

    // Unicode escape (ASCII range)
    v = parse("\"\\u0041\"");
    CHECK_STR(v.as_string(), "A");

    // Empty string
    v = parse("\"\"");
    CHECK(v.is_string());
    CHECK_STR(v.as_string(), "");
}

static void test_arrays() {
    SECTION("arrays");
    JVal v;

    v = parse("[]");
    CHECK(v.is_array());
    CHECK_EQ(v.size(), 0u);

    v = parse("[1, 2, 3]");
    CHECK(v.is_array());
    CHECK_EQ(v.size(), 3u);
    CHECK_EQ(v[0].as_int64(), 1LL);
    CHECK_EQ(v[1].as_int64(), 2LL);
    CHECK_EQ(v[2].as_int64(), 3LL);

    // Out-of-bounds returns null
    CHECK(v[99].is_null());

    // Nested
    v = parse("[[1,2],[3,4]]");
    CHECK_EQ(v.size(), 2u);
    CHECK_EQ(v[0][1].as_int64(), 2LL);
    CHECK_EQ(v[1][0].as_int64(), 3LL);

    // Mixed types
    v = parse("[null, true, \"hi\", 7]");
    CHECK(v[0].is_null());
    CHECK(v[1].as_bool() == true);
    CHECK_STR(v[2].as_string(), "hi");
    CHECK_EQ(v[3].as_int64(), 7LL);
}

static void test_objects() {
    SECTION("objects");
    JVal v;

    v = parse("{}");
    CHECK(v.is_object());
    CHECK(!v.has("x"));
    CHECK(v["x"].is_null());

    v = parse("{\"a\": 1, \"b\": \"two\", \"c\": true}");
    CHECK(v.is_object());
    CHECK(v.has("a"));
    CHECK_EQ(v["a"].as_int64(), 1LL);
    CHECK_STR(v["b"].as_string(), "two");
    CHECK(v["c"].as_bool() == true);
    CHECK(!v.has("d"));

    // Nested object
    v = parse("{\"outer\": {\"inner\": 42}}");
    CHECK_EQ(v["outer"]["inner"].as_int64(), 42LL);
}

static void test_chained_access() {
    SECTION("chained access");
    JVal v = parse("{\"device\": {\"name\": \"/dev/nvme0\", \"protocol\": \"NVMe\"}}");
    CHECK_STR(v["device"]["name"].as_string(), "/dev/nvme0");
    CHECK_STR(v["device"]["protocol"].as_string(), "NVMe");
    // Missing key chain returns null, not a crash
    CHECK(v["device"]["missing"]["deep"].is_null());
}

static void test_errors() {
    SECTION("error cases");
    CHECK(parse_fails(""));
    CHECK(parse_fails("{"));
    CHECK(parse_fails("[1,2,"));
    CHECK(parse_fails("\"unterminated"));
    CHECK(parse_fails("{\"key\": }"));
    CHECK(parse_fails("truee"));
    CHECK(parse_fails("nul"));
}

static void test_nvme_fixture(const char *path) {
    SECTION("NVMe fixture");
    std::string err;
    JVal root = json_load_file(path, err);
    CHECK(err.empty());
    if (!err.empty()) { fprintf(stderr, "  %s\n", err.c_str()); return; }

    CHECK_STR(root["device"]["protocol"].as_string(), "NVMe");
    CHECK(!root["device"]["name"].as_string().empty());

    const JVal &log = root["nvme_smart_health_information_log"];
    CHECK(!log.is_null());
    // available_spare should be 0-100
    uint64_t spare = log["available_spare"].as_uint64();
    CHECK(spare <= 100u);
    // power_on_hours should be > 0 for a used drive
    CHECK(log["power_on_hours"].as_uint64() > 0u);
}

static void test_ata_fixture(const char *path) {
    SECTION("ATA fixture");
    std::string err;
    JVal root = json_load_file(path, err);
    CHECK(err.empty());
    if (!err.empty()) { fprintf(stderr, "  %s\n", err.c_str()); return; }

    CHECK_STR(root["device"]["protocol"].as_string(), "ATA");

    const JVal &attrs = root["ata_smart_attributes"]["table"];
    CHECK(attrs.is_array());
    CHECK(attrs.size() > 0u);

    // Each attribute must have id, name, value, worst, thresh
    for (size_t i = 0; i < attrs.size(); ++i) {
        const JVal &a = attrs[i];
        CHECK(a["id"].as_uint64() >= 1u && a["id"].as_uint64() <= 255u);
        CHECK(!a["name"].as_string().empty());
        CHECK(a.has("value"));
        CHECK(a.has("worst"));
        CHECK(a.has("thresh"));
    }
}

static void test_ata_selftest_fixture(const char *path) {
    SECTION("ATA self-test fixture");
    std::string err;
    JVal root = json_load_file(path, err);
    CHECK(err.empty());
    if (!err.empty()) return;

    const JVal &stlog = root["ata_smart_self_test_log"]["standard"]["table"];
    CHECK(stlog.is_array());
    CHECK(stlog.size() >= 2u);

    // Third entry should be a failure
    if (stlog.size() >= 3) {
        const JVal &e = stlog[2];
        CHECK(e["status"]["passed"].as_bool() == false);
        CHECK(e["lba_of_first_error"].as_uint64() == 123456789u);
    }
}

static void test_scsi_fixture(const char *path) {
    SECTION("SCSI fixture");
    std::string err;
    JVal root = json_load_file(path, err);
    CHECK(err.empty());
    if (!err.empty()) { fprintf(stderr, "  %s\n", err.c_str()); return; }

    CHECK_STR(root["device"]["protocol"].as_string(), "SCSI");

    // grown_defect_list
    CHECK_EQ(root["scsi_grown_defect_list"].as_uint64(), 3u);

    // error counter log
    const JVal &ecl = root["scsi_error_counter_log"];
    CHECK(!ecl.is_null());
    CHECK(!ecl["read"].is_null());
    CHECK(!ecl["write"].is_null());

    // Self-test log
    const JVal &stlog = root["scsi_self_test_log"]["extended"]["table"];
    CHECK(stlog.is_array());
    CHECK_EQ(stlog.size(), 2u);
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------

int main(int argc, char *argv[]) {
    // argv[1..] = paths to fixture files
    const char *nvme_path  = nullptr;
    const char *ata_path   = nullptr;
    const char *ata_st_path = nullptr;
    const char *scsi_path  = nullptr;

    for (int i = 1; i < argc; ++i) {
        std::string p = argv[i];
        if (p.find(".nvme.json") != std::string::npos && !nvme_path)
            nvme_path = argv[i];
        else if (p.find("SELFTESTS") != std::string::npos)
            ata_st_path = argv[i];
        else if (p.find(".ata.json") != std::string::npos && !ata_path)
            ata_path = argv[i];
        else if (p.find(".scsi.json") != std::string::npos && !scsi_path)
            scsi_path = argv[i];
    }

    test_literals();
    test_numbers();
    test_strings();
    test_arrays();
    test_objects();
    test_chained_access();
    test_errors();

    if (nvme_path)   test_nvme_fixture(nvme_path);
    if (ata_path)    test_ata_fixture(ata_path);
    if (ata_st_path) test_ata_selftest_fixture(ata_st_path);
    if (scsi_path)   test_scsi_fixture(scsi_path);

    return test_summary();
}
