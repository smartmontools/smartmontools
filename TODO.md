# Implement new SATA MIB OIDs in agent

MIB revision 202605200100Z added OIDs 19–40 and extended the health entry
to sub-OIDs 27–37.  None of these are implemented in the agent yet.
Four health-entry fields from the previous revision (sub-OIDs 23–26) are
also missing from the cache and handler.

Reference JSON: `.tmp/source/pve2/poll.ata-WDC_WD140EFGX-68B0GN0_81GDJW2V.sat.json`

---

## 1. snmp_oids.h — add OID constants

```
oid_sata_erc_row_count[]           = { SMARTMON_ENT, 4, 1, 19 }
oid_sata_erc_last_change[]         = { SMARTMON_ENT, 4, 1, 20 }
oid_sata_erc_table[]               = { SMARTMON_ENT, 4, 1, 21 }

oid_sata_phy_event_row_count[]     = { SMARTMON_ENT, 4, 1, 22 }
oid_sata_phy_event_last_change[]   = { SMARTMON_ENT, 4, 1, 23 }
oid_sata_phy_event_table[]         = { SMARTMON_ENT, 4, 1, 24 }

oid_sata_selective_row_count[]     = { SMARTMON_ENT, 4, 1, 25 }
oid_sata_selective_last_change[]   = { SMARTMON_ENT, 4, 1, 26 }
oid_sata_selective_table[]         = { SMARTMON_ENT, 4, 1, 27 }
oid_sata_selective_revision[]      = { SMARTMON_ENT, 4, 1, 28 }
oid_sata_selective_flags[]         = { SMARTMON_ENT, 4, 1, 29 }
oid_sata_selective_remainder_scan[]= { SMARTMON_ENT, 4, 1, 30 }
oid_sata_selective_powerup_resume[]= { SMARTMON_ENT, 4, 1, 31 }

oid_sata_logdir_row_count[]        = { SMARTMON_ENT, 4, 1, 32 }
oid_sata_logdir_last_change[]      = { SMARTMON_ENT, 4, 1, 33 }
oid_sata_logdir_table[]            = { SMARTMON_ENT, 4, 1, 34 }
oid_sata_logdir_gp_version[]       = { SMARTMON_ENT, 4, 1, 35 }
oid_sata_logdir_smart_version[]    = { SMARTMON_ENT, 4, 1, 36 }
oid_sata_logdir_smart_multisector[]= { SMARTMON_ENT, 4, 1, 37 }

oid_sata_devstat_row_count[]       = { SMARTMON_ENT, 4, 1, 38 }
oid_sata_devstat_last_change[]     = { SMARTMON_ENT, 4, 1, 39 }
oid_sata_devstat_table[]           = { SMARTMON_ENT, 4, 1, 40 }
```

---

## 2. agentxd_cache.h — extend / add structs

### 2a. Extend CacheSataHealthRow (currently ends at error_log_count)

Add missing fields from previous revision (sub-OIDs 23–26):
```cpp
bool     cap_exec_offline_immediate { false };  // capabilities.exec_offline_immediate_supported
bool     cap_offline_aborted_on_cmd { false };  // capabilities.offline_is_aborted_upon_new_cmd
bool     cap_offline_surface_scan   { false };  // capabilities.offline_surface_scan_supported
bool     cap_attr_autosave          { false };  // capabilities.attribute_autosave_enabled
```

Add new fields (sub-OIDs 27–37):
```cpp
uint32_t pending_defects_size       { 0 };  // ata_pending_defects_log.size
uint32_t pending_defects_count      { 0 };  // ata_pending_defects_log.count
uint32_t spare_available_pct        { 0 };  // spare_available.current_percent
uint32_t spare_available_thresh_pct { 0 };  // spare_available.threshold_percent
uint32_t error_log_revision         { 0 };  // ata_smart_error_log.extended.revision
uint32_t error_log_sectors          { 0 };  // ata_smart_error_log.extended.sectors
uint32_t selftest_log_revision      { 0 };  // ata_smart_self_test_log.extended.revision
uint32_t selftest_log_sectors       { 0 };  // ata_smart_self_test_log.extended.sectors
uint32_t selftest_log_count         { 0 };  // ata_smart_self_test_log.extended.count
uint32_t selftest_log_err_total     { 0 };  // ata_smart_self_test_log.extended.error_count_total
uint32_t selftest_log_err_outdated  { 0 };  // ata_smart_self_test_log.extended.error_count_outdated
```

Note: `cap_auto_offline` is currently mapped to `attribute_autosave_enabled` — **wrong**.
Fix: derive from `offline_data_collection.status.value` bit 7 (value & 0x80).

### 2b. New struct CacheSataErcRow
```cpp
struct CacheSataErcRow {
    uint32_t    device_index { 0 };
    uint32_t    erc_index    { 0 };  // 1=read, 2=write
    std::string direction;           // "read" / "write"
    bool        enabled      { false };
    uint32_t    deciseconds  { 0 };
};
```
Source: `ata_sct_erc.read.*` / `ata_sct_erc.write.*`

### 2c. New struct CacheSataPhyEventRow
```cpp
struct CacheSataPhyEventRow {
    uint32_t    device_index { 0 };
    uint32_t    id           { 0 };
    std::string name;
    uint32_t    size         { 0 };
    uint64_t    value        { 0 };
    bool        overflow     { false };
};
```
Source: `sata_phy_event_counters.table[]`

### 2d. New struct CacheSataSelectiveTestRow (+ per-device scalars)
```cpp
struct CacheSataSelectiveTestRow {
    uint32_t    device_index { 0 };
    uint32_t    slot         { 0 };  // 1..5
    uint64_t    lba_min      { 0 };
    uint64_t    lba_max      { 0 };
    uint32_t    status_value { 0 };
    std::string status_string;
};
```
Source: `ata_smart_selective_self_test_log.table[]`

Per-device scalars (store in CacheSataHealthRow or a separate per-device struct):
```
selective_log_revision         // .revision
selective_flags_value          // .flags.value
selective_remainder_scan       // .flags.remainder_scan_enabled
selective_powerup_resume_min   // .power_up_scan_resume_minutes
```

### 2e. New struct CacheSataLogDirRow (+ per-device scalars)
```cpp
struct CacheSataLogDirRow {
    uint32_t    device_index  { 0 };
    uint32_t    address       { 0 };
    std::string name;
    bool        readable      { false };
    bool        writable      { false };
    uint32_t    gp_sectors    { 0 };
    uint32_t    smart_sectors { 0 };
};
```
Source: `ata_log_directory.table[]`

Per-device scalars:
```
logdir_gp_version          // .gp_dir_version
logdir_smart_version       // .smart_dir_version
logdir_smart_multisector   // .smart_dir_multi_sector
```

### 2f. New struct CacheSataDevStatRow
```cpp
struct CacheSataDevStatRow {
    uint32_t    device_index { 0 };
    uint32_t    page_num     { 0 };
    uint32_t    offset       { 0 };
    std::string page_name;
    std::string name;
    uint64_t    value        { 0 };
    uint32_t    flags_value  { 0 };
    bool        valid        { false };
    bool        normalized   { false };
};
```
Source: `ata_device_statistics.pages[].table[]`

### 2g. Add to AgentCache struct

New vectors:
```cpp
std::vector<CacheSataErcRow>            sata_erc;
std::vector<CacheSataPhyEventRow>       sata_phy_events;
std::vector<CacheSataSelectiveTestRow>  sata_selective_tests;
std::vector<CacheSataLogDirRow>         sata_log_dir;
std::vector<CacheSataDevStatRow>        sata_dev_stats;
```

New timestamps:
```cpp
time_t  ts_sata_erc            { 0 };
time_t  ts_sata_phy_event      { 0 };
time_t  ts_sata_selective_test { 0 };
time_t  ts_sata_log_dir        { 0 };
time_t  ts_sata_dev_stat       { 0 };
```

---

## 3. agentxd_datasrc.cpp — parse new JSON fields

### 3a. Fix cap_auto_offline (bug)
Currently: `h.cap_auto_offline = cap["attribute_autosave_enabled"].as_bool();`
Fix: `h.cap_auto_offline = (ata_smart_data["offline_data_collection"]["status"]["value"].as_int() & 0x80) != 0;`

### 3b. Parse health entries 23–26
```cpp
h.cap_exec_offline_immediate = cap["exec_offline_immediate_supported"].as_bool();
h.cap_offline_aborted_on_cmd = cap["offline_is_aborted_upon_new_cmd"].as_bool();
h.cap_offline_surface_scan   = cap["offline_surface_scan_supported"].as_bool();
h.cap_attr_autosave          = cap["attribute_autosave_enabled"].as_bool();
```

### 3c. Parse health entries 27–37
```cpp
// ata_pending_defects_log
h.pending_defects_size  = j["ata_pending_defects_log"]["size"].as_uint32();
h.pending_defects_count = j["ata_pending_defects_log"]["count"].as_uint32();

// spare_available
h.spare_available_pct        = j["spare_available"]["current_percent"].as_uint32();
h.spare_available_thresh_pct = j["spare_available"]["threshold_percent"].as_uint32();

// ata_smart_error_log.extended
h.error_log_revision = j["ata_smart_error_log"]["extended"]["revision"].as_uint32();
h.error_log_sectors  = j["ata_smart_error_log"]["extended"]["sectors"].as_uint32();

// ata_smart_self_test_log.extended
auto &stl = j["ata_smart_self_test_log"]["extended"];
h.selftest_log_revision    = stl["revision"].as_uint32();
h.selftest_log_sectors     = stl["sectors"].as_uint32();
h.selftest_log_count       = stl["count"].as_uint32();
h.selftest_log_err_total   = stl["error_count_total"].as_uint32();
h.selftest_log_err_outdated= stl["error_count_outdated"].as_uint32();
```

### 3d. Parse ata_sct_erc → sata_erc
```cpp
for (auto [key, idx] : {{"read", 1u}, {"write", 2u}}) {
    auto &e = j["ata_sct_erc"][key];
    if (!e.exists()) continue;
    CacheSataErcRow r;
    r.device_index = dev_idx;
    r.erc_index    = idx;
    r.direction    = key;
    r.enabled      = e["enabled"].as_bool();
    r.deciseconds  = e["deciseconds"].as_uint32();
    g_cache.sata_erc.push_back(r);
}
```

### 3e. Parse sata_phy_event_counters → sata_phy_events
```cpp
for (auto &entry : j["sata_phy_event_counters"]["table"].as_array()) {
    CacheSataPhyEventRow r;
    r.device_index = dev_idx;
    r.id           = entry["id"].as_uint32();
    r.name         = entry["name"].as_string();
    r.size         = entry["size"].as_uint32();
    r.value        = entry["value"].as_uint64();
    r.overflow     = entry["overflow"].as_bool();
    g_cache.sata_phy_events.push_back(r);
}
```

### 3f. Parse ata_smart_selective_self_test_log → sata_selective_tests + scalars
```cpp
auto &ssl = j["ata_smart_selective_self_test_log"];
// table rows
uint32_t slot = 1;
for (auto &entry : ssl["table"].as_array()) {
    CacheSataSelectiveTestRow r;
    r.device_index  = dev_idx;
    r.slot          = slot++;
    r.lba_min       = entry["lba_min"].as_uint64();
    r.lba_max       = entry["lba_max"].as_uint64();
    r.status_value  = entry["status"]["value"].as_uint32();
    r.status_string = entry["status"]["string"].as_string();
    g_cache.sata_selective_tests.push_back(r);
}
// per-device scalars → store in CacheSataHealthRow (or separate struct)
h.selective_log_revision       = ssl["revision"].as_uint32();
h.selective_flags_value        = ssl["flags"]["value"].as_uint32();
h.selective_remainder_scan     = ssl["flags"]["remainder_scan_enabled"].as_bool();
h.selective_powerup_resume_min = ssl["power_up_scan_resume_minutes"].as_uint32();
```

### 3g. Parse ata_log_directory → sata_log_dir + scalars
```cpp
auto &ld = j["ata_log_directory"];
// per-device scalars → store in CacheSataHealthRow (or separate struct)
h.logdir_gp_version        = ld["gp_dir_version"].as_uint32();
h.logdir_smart_version     = ld["smart_dir_version"].as_uint32();
h.logdir_smart_multisector = ld["smart_dir_multi_sector"].as_bool();
// table rows
for (auto &entry : ld["table"].as_array()) {
    CacheSataLogDirRow r;
    r.device_index  = dev_idx;
    r.address       = entry["address"].as_uint32();
    r.name          = entry["name"].as_string();
    r.readable      = entry["read"].as_bool();
    r.writable      = entry["write"].as_bool();
    r.gp_sectors    = entry["gp_sectors"].as_uint32();     // missing = 0
    r.smart_sectors = entry["smart_sectors"].as_uint32();  // missing = 0
    g_cache.sata_log_dir.push_back(r);
}
```

### 3h. Parse ata_device_statistics → sata_dev_stats
```cpp
for (auto &page : j["ata_device_statistics"]["pages"].as_array()) {
    std::string page_name = page["name"].as_string();
    uint32_t    page_num  = page["number"].as_uint32();
    for (auto &entry : page["table"].as_array()) {
        CacheSataDevStatRow r;
        r.device_index = dev_idx;
        r.page_num     = page_num;
        r.offset       = entry["offset"].as_uint32();
        r.page_name    = page_name;
        r.name         = entry["name"].as_string();
        r.value        = entry["value"].as_uint64();
        r.flags_value  = entry["flags"]["value"].as_uint32();
        r.valid        = entry["flags"]["valid"].as_bool();
        r.normalized   = entry["flags"]["normalized"].as_bool();
        g_cache.sata_dev_stats.push_back(r);
    }
}
```

### 3i. Update timestamp block
Add alongside existing ts_sata_* assignments:
```cpp
g_cache.ts_sata_erc            = now_ts;
g_cache.ts_sata_phy_event      = now_ts;
g_cache.ts_sata_selective_test = now_ts;
g_cache.ts_sata_log_dir        = now_ts;
g_cache.ts_sata_dev_stat       = now_ts;
```

Also clear new vectors in `clear_device_data()` / `clear()`.

---

## 4. snmp_sata_mib.cpp — extend handlers + register new tables

### 4a. Extend sata_health_handler (currently col 1..22)

Change `REG_TABLE_UU(..., 1, 22)` → `REG_TABLE_UU(..., 1, 37)`.

Add cases 23–37 in the switch:
```cpp
case 23: { long v = row->cap_exec_offline_immediate ? 1 : 2; ... }
case 24: { long v = row->cap_offline_aborted_on_cmd ? 1 : 2; ... }
case 25: { long v = row->cap_offline_surface_scan   ? 1 : 2; ... }
case 26: { long v = row->cap_attr_autosave          ? 1 : 2; ... }
case 27: snmp_set_var_typed_value(var, ASN_UNSIGNED, &row->pending_defects_size,  sizeof(uint32_t)); break;
case 28: snmp_set_var_typed_value(var, ASN_UNSIGNED, &row->pending_defects_count, sizeof(uint32_t)); break;
case 29: snmp_set_var_typed_value(var, ASN_UNSIGNED, &row->spare_available_pct,        sizeof(uint32_t)); break;
case 30: snmp_set_var_typed_value(var, ASN_UNSIGNED, &row->spare_available_thresh_pct, sizeof(uint32_t)); break;
case 31: snmp_set_var_typed_value(var, ASN_UNSIGNED, &row->error_log_revision,  sizeof(uint32_t)); break;
case 32: snmp_set_var_typed_value(var, ASN_UNSIGNED, &row->error_log_sectors,   sizeof(uint32_t)); break;
case 33: snmp_set_var_typed_value(var, ASN_UNSIGNED, &row->selftest_log_revision,     sizeof(uint32_t)); break;
case 34: snmp_set_var_typed_value(var, ASN_UNSIGNED, &row->selftest_log_sectors,      sizeof(uint32_t)); break;
case 35: snmp_set_var_typed_value(var, ASN_UNSIGNED, &row->selftest_log_count,        sizeof(uint32_t)); break;
case 36: snmp_set_var_typed_value(var, ASN_UNSIGNED, &row->selftest_log_err_total,    sizeof(uint32_t)); break;
case 37: snmp_set_var_typed_value(var, ASN_UNSIGNED, &row->selftest_log_err_outdated, sizeof(uint32_t)); break;
```

### 4b. Add ERC table (index UU: device_index, erc_index; cols 2..4)

Follow existing pattern: `get_next`, `handler`, row/last-change macros, `REG_TABLE_UU(..., 1, 4)`.

### 4c. Add PHY event table (index UU: device_index, id; cols 2..5)

### 4d. Add selective self-test table (index UU: device_index, slot; cols 2..5)
Plus four scalar handlers for OIDs 28–31 (need per-device lookup by device_index
from sata_selective_tests or health row).

### 4e. Add log directory table (index UU: device_index, address; cols 2..6)
Plus three scalar handlers for OIDs 35–37.

### 4f. Add device statistics table (index UUU: device_index, page_num, offset; cols 3..8)
Use `REG_TABLE_UUU(...)` like the error-cmd table.

### 4g. Register all new handlers in snmp_sata_mib_register()
Add row-count, last-change, and table registrations for OIDs 19–40.
Update `smartmonSataTableMetaGroup` handler list to match.

---

## 5. Tests

- Fixture `WDC_WD140EFGX_68B0GN0-81GDJW2V.ata.json` already in tests/fixtures —
  it contains erc, phy_events, selective_tests, log_dir, dev_stats, spare_available.
- Add assertions in `test_datasrc.cpp` for each new table:
  - sata_erc: 2 rows (read + write), enabled=true, deciseconds=70
  - sata_phy_events: 12 rows, id 1..13 (no id 12)
  - sata_selective_tests: 5 rows, all lba_min=lba_max=0, status="Not_testing"
  - sata_log_dir: verify a few known addresses (0, 1, 4, 17)
  - sata_dev_stats: check General Statistics page (1), Logical Sectors Written etc.
  - health row: spare_available_pct=100, pending_defects_count=0,
    error_log_revision=1, selftest_log_count=19

---

## Implementation order (suggested)

1. `snmp_oids.h` — OID constants (no deps)
2. `agentxd_cache.h` — structs + vectors + timestamps
3. `agentxd_datasrc.cpp` — JSON parsing (fix bug + all new fields)
4. `snmp_sata_mib.cpp` — extend health handler (entries 23–37)
5. `snmp_sata_mib.cpp` — ERC table (smallest new table)
6. `snmp_sata_mib.cpp` — PHY event table
7. `snmp_sata_mib.cpp` — selective self-test table + scalars
8. `snmp_sata_mib.cpp` — log directory table + scalars
9. `snmp_sata_mib.cpp` — device statistics table
10. `test_datasrc.cpp` — test assertions for all new tables
