// snmp_sata_mib.cpp — SATA attribute and self-test table handlers

#include "snmp_sata_mib.h"
#include "agentxd_cache.h"
#include "snmp_oids.h"

#include <cstring>
#include <ctime>

#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-includes.h>
#include <net-snmp/agent/net-snmp-agent-includes.h>

// ---------------------------------------------------------------------------
// Utility: encode time_t as 8-byte DateAndTime (UTC)
// ---------------------------------------------------------------------------
static void sata_encode_dt(time_t t, uint8_t out[8]) {
    struct tm *tm = gmtime(&t);
    if (!tm) { memset(out, 0, 8); return; }
    uint16_t y = (uint16_t)(tm->tm_year + 1900);
    out[0] = (uint8_t)(y >> 8); out[1] = (uint8_t)(y & 0xff);
    out[2] = (uint8_t)(tm->tm_mon + 1); out[3] = (uint8_t)tm->tm_mday;
    out[4] = (uint8_t)tm->tm_hour; out[5] = (uint8_t)tm->tm_min;
    out[6] = (uint8_t)tm->tm_sec; out[7] = 0;
}

// ---------------------------------------------------------------------------
// SATA table metadata scalar handlers
// ---------------------------------------------------------------------------

#define SATA_ROW_COUNT_HANDLER(name, cache_field) \
static int name(netsnmp_mib_handler *, netsnmp_handler_registration *, \
                netsnmp_agent_request_info *reqinfo, netsnmp_request_info *requests) { \
    if (reqinfo->mode != MODE_GET) return SNMP_ERR_NOERROR; \
    u_long v = (u_long)g_cache.cache_field.size(); \
    snmp_set_var_typed_value(requests->requestvb, ASN_GAUGE, (u_char*)&v, sizeof(v)); \
    return SNMP_ERR_NOERROR; \
}

#define SATA_LAST_CHANGE_HANDLER(name, ts_field) \
static int name(netsnmp_mib_handler *, netsnmp_handler_registration *, \
                netsnmp_agent_request_info *reqinfo, netsnmp_request_info *requests) { \
    if (reqinfo->mode != MODE_GET) return SNMP_ERR_NOERROR; \
    uint8_t dt[8]; sata_encode_dt(g_cache.ts_field, dt); \
    snmp_set_var_typed_value(requests->requestvb, ASN_OCTET_STR, dt, sizeof(dt)); \
    return SNMP_ERR_NOERROR; \
}

SATA_ROW_COUNT_HANDLER(sata_info_row_count_handler,     sata_info)
SATA_LAST_CHANGE_HANDLER(sata_info_last_change_handler,  ts_sata_info)

SATA_ROW_COUNT_HANDLER(sata_health_row_count_handler,   sata_health)
SATA_LAST_CHANGE_HANDLER(sata_health_last_change_handler, ts_sata_health)

SATA_ROW_COUNT_HANDLER(sata_attr_row_count_handler,     sata_attrs)
SATA_LAST_CHANGE_HANDLER(sata_attr_last_change_handler,  ts_sata_attr)

SATA_ROW_COUNT_HANDLER(sata_el_row_count_handler,       sata_error_log)
SATA_LAST_CHANGE_HANDLER(sata_el_last_change_handler,    ts_sata_error_log)

SATA_ROW_COUNT_HANDLER(sata_st_row_count_handler,        sata_selftests)
SATA_LAST_CHANGE_HANDLER(sata_st_last_change_handler,    ts_sata_selftest)

SATA_ROW_COUNT_HANDLER(sata_errcmd_row_count_handler,    sata_error_cmds)
SATA_LAST_CHANGE_HANDLER(sata_errcmd_last_change_handler, ts_sata_error_cmd)

static void set_counter64(netsnmp_request_info *req, uint64_t val) {
    struct counter64 c64;
    c64.high = (u_long)(val >> 32);
    c64.low  = (u_long)(val & 0xffffffffUL);
    snmp_set_var_typed_value(req->requestvb, ASN_COUNTER64,
                             (u_char*)&c64, sizeof(c64));
}

// Convert raw ATA flags byte to ASN.1 BITS byte.
// Raw bit i → ASN.1 bit i → MSBit-first octet position (7-i).
static uint8_t ata_flags_to_bits(uint8_t raw) {
    uint8_t b = 0;
    for (int i = 0; i < 6; i++)
        if (raw & (1 << i)) b |= (uint8_t)(0x80u >> i);
    return b;
}

// ---------------------------------------------------------------------------
// SATA info table
// INDEX { smartmonDeviceIndex, smartmonSataInfoIndex(col 17, NOT-ACCESSIBLE) }
// col 1  = modelFamily
// col 2  = modelName
// col 3  = serialNumber
// col 4  = firmwareVersion
// col 5  = wwn
// col 6  = ataVersionString
// col 7  = versionString (SATA)
// col 8  = rotationRate
// col 9  = formFactor
// col 10 = logicalBlockSize
// col 11 = physicalBlockSize
// col 12 = userCapacityBytes
// col 13 = inSmartctlDatabase (TruthValue)
// col 14 = smartAvailable (TruthValue)
// col 15 = smartEnabled (TruthValue)
// col 16 = trimSupported (TruthValue)
// col 17 = infoIndex (NOT-ACCESSIBLE, index)
// ---------------------------------------------------------------------------

static netsnmp_variable_list *
sata_info_get_next(void **loop_ctx, void **data_ctx,
                   netsnmp_variable_list *put_idx,
                   netsnmp_iterator_info *) {
    size_t idx = (size_t)(uintptr_t)*loop_ctx;
    if (idx >= g_cache.sata_info.size()) return nullptr;
    CacheSataInfoRow &row = g_cache.sata_info[idx];
    *loop_ctx = (void*)(uintptr_t)(idx + 1);
    *data_ctx = &row;
    u_long v = (u_long)row.device_index;
    snmp_set_var_typed_value(put_idx, ASN_UNSIGNED, (u_char*)&v, sizeof(v));
    v = 1;  // infoIndex = 1 per device
    snmp_set_var_typed_value(put_idx->next_variable, ASN_UNSIGNED,
                             (u_char*)&v, sizeof(v));
    return put_idx;
}

static netsnmp_variable_list *
sata_info_get_first(void **loop_ctx, void **data_ctx,
                    netsnmp_variable_list *put_idx,
                    netsnmp_iterator_info *ii) {
    *loop_ctx = 0;
    return sata_info_get_next(loop_ctx, data_ctx, put_idx, ii);
}

static int
sata_info_handler(netsnmp_mib_handler *,
                  netsnmp_handler_registration *,
                  netsnmp_agent_request_info *reqinfo,
                  netsnmp_request_info *requests) {
    if (reqinfo->mode != MODE_GET) return SNMP_ERR_NOERROR;

    for (netsnmp_request_info *req = requests; req; req = req->next) {
        netsnmp_table_request_info *tinfo = netsnmp_extract_table_info(req);
        CacheSataInfoRow *row =
            (CacheSataInfoRow*)netsnmp_extract_iterator_context(req);
        if (!row || !tinfo) continue;

        switch (tinfo->colnum) {
        case 1:  snmp_set_var_typed_value(req->requestvb, ASN_OCTET_STR,
                     (u_char*)row->model_family.c_str(),
                     row->model_family.size()); break;
        case 2:  snmp_set_var_typed_value(req->requestvb, ASN_OCTET_STR,
                     (u_char*)row->model_name.c_str(),
                     row->model_name.size()); break;
        case 3:  snmp_set_var_typed_value(req->requestvb, ASN_OCTET_STR,
                     (u_char*)row->serial_number.c_str(),
                     row->serial_number.size()); break;
        case 4:  snmp_set_var_typed_value(req->requestvb, ASN_OCTET_STR,
                     (u_char*)row->firmware_version.c_str(),
                     row->firmware_version.size()); break;
        case 5:  snmp_set_var_typed_value(req->requestvb, ASN_OCTET_STR,
                     (u_char*)row->wwn.c_str(),
                     row->wwn.size()); break;
        case 6:  snmp_set_var_typed_value(req->requestvb, ASN_OCTET_STR,
                     (u_char*)row->ata_version_string.c_str(),
                     row->ata_version_string.size()); break;
        case 7:  snmp_set_var_typed_value(req->requestvb, ASN_OCTET_STR,
                     (u_char*)row->sata_version_string.c_str(),
                     row->sata_version_string.size()); break;
        case 8:  { u_long v = row->rotation_rate;
                   snmp_set_var_typed_value(req->requestvb, ASN_GAUGE,
                       (u_char*)&v, sizeof(v)); break; }
        case 9:  snmp_set_var_typed_value(req->requestvb, ASN_OCTET_STR,
                     (u_char*)row->form_factor.c_str(),
                     row->form_factor.size()); break;
        case 10: { u_long v = row->logical_block_size;
                   snmp_set_var_typed_value(req->requestvb, ASN_GAUGE,
                       (u_char*)&v, sizeof(v)); break; }
        case 11: { u_long v = row->physical_block_size;
                   snmp_set_var_typed_value(req->requestvb, ASN_GAUGE,
                       (u_char*)&v, sizeof(v)); break; }
        case 12: set_counter64(req, row->user_capacity_bytes); break;
        case 13: { long v = row->in_smartctl_db ? 1 : 2;
                   snmp_set_var_typed_value(req->requestvb, ASN_INTEGER,
                       (u_char*)&v, sizeof(v)); break; }
        case 14: { long v = row->smart_available ? 1 : 2;
                   snmp_set_var_typed_value(req->requestvb, ASN_INTEGER,
                       (u_char*)&v, sizeof(v)); break; }
        case 15: { long v = row->smart_enabled ? 1 : 2;
                   snmp_set_var_typed_value(req->requestvb, ASN_INTEGER,
                       (u_char*)&v, sizeof(v)); break; }
        case 16: { long v = row->trim_supported ? 1 : 2;
                   snmp_set_var_typed_value(req->requestvb, ASN_INTEGER,
                       (u_char*)&v, sizeof(v)); break; }
        case 17: { u_long v = 1;  // infoIndex
                   snmp_set_var_typed_value(req->requestvb, ASN_GAUGE,
                       (u_char*)&v, sizeof(v)); break; }
        default: netsnmp_set_request_error(reqinfo, req, SNMP_NOSUCHOBJECT);
        }
    }
    return SNMP_ERR_NOERROR;
}

// ---------------------------------------------------------------------------
// SATA health table
// INDEX { smartmonDeviceIndex, smartmonSataHealthIndex(col 22, NOT-ACCESSIBLE) }
// col 1  = healthOverallStatus
// col 2  = offlineCollectionStatusValue
// col 3  = offlineCollectionStatusString
// col 4  = offlineCollectionCompletionSecs
// col 5  = selfTestExecutionStatusValue
// col 6  = selfTestExecutionStatusString
// col 7  = selfTestPollingShortMinutes
// col 8  = selfTestPollingExtendedMinutes
// col 9  = selfTestPollingConveyanceMinutes
// col 10 = capabilityAutoOfflineEnabled (TruthValue)
// col 11 = capabilitySelfTestsSupported (TruthValue)
// col 12 = capabilityConveyanceSupported (TruthValue)
// col 13 = capabilitySelectiveSupported (TruthValue)
// col 14 = capabilityErrorLoggingSupported (TruthValue)
// col 15 = capabilityGpLoggingSupported (TruthValue)
// col 16 = sctErrorRecoverySupported (TruthValue)
// col 17 = sctFeatureControlSupported (TruthValue)
// col 18 = sctDataTableSupported (TruthValue)
// col 19 = powerCycles
// col 20 = powerOnHours
// col 21 = errorLogCount
// col 22 = healthIndex (NOT-ACCESSIBLE, index)
// ---------------------------------------------------------------------------

static netsnmp_variable_list *
sata_health_get_next(void **loop_ctx, void **data_ctx,
                     netsnmp_variable_list *put_idx,
                     netsnmp_iterator_info *) {
    size_t idx = (size_t)(uintptr_t)*loop_ctx;
    if (idx >= g_cache.sata_health.size()) return nullptr;
    CacheSataHealthRow &row = g_cache.sata_health[idx];
    *loop_ctx = (void*)(uintptr_t)(idx + 1);
    *data_ctx = &row;
    u_long v = (u_long)row.device_index;
    snmp_set_var_typed_value(put_idx, ASN_UNSIGNED, (u_char*)&v, sizeof(v));
    v = 1;  // healthIndex = 1 per device
    snmp_set_var_typed_value(put_idx->next_variable, ASN_UNSIGNED,
                             (u_char*)&v, sizeof(v));
    return put_idx;
}

static netsnmp_variable_list *
sata_health_get_first(void **loop_ctx, void **data_ctx,
                      netsnmp_variable_list *put_idx,
                      netsnmp_iterator_info *ii) {
    *loop_ctx = 0;
    return sata_health_get_next(loop_ctx, data_ctx, put_idx, ii);
}

static int
sata_health_handler(netsnmp_mib_handler *,
                    netsnmp_handler_registration *,
                    netsnmp_agent_request_info *reqinfo,
                    netsnmp_request_info *requests) {
    if (reqinfo->mode != MODE_GET) return SNMP_ERR_NOERROR;

    for (netsnmp_request_info *req = requests; req; req = req->next) {
        netsnmp_table_request_info *tinfo = netsnmp_extract_table_info(req);
        CacheSataHealthRow *row =
            (CacheSataHealthRow*)netsnmp_extract_iterator_context(req);
        if (!row || !tinfo) continue;

        switch (tinfo->colnum) {
        case 1:  { long v = row->overall_status;
                   snmp_set_var_typed_value(req->requestvb, ASN_INTEGER,
                       (u_char*)&v, sizeof(v)); break; }
        case 2:  { u_long v = row->offline_status_value;
                   snmp_set_var_typed_value(req->requestvb, ASN_GAUGE,
                       (u_char*)&v, sizeof(v)); break; }
        case 3:  snmp_set_var_typed_value(req->requestvb, ASN_OCTET_STR,
                     (u_char*)row->offline_status_string.c_str(),
                     row->offline_status_string.size()); break;
        case 4:  { u_long v = row->offline_completion_secs;
                   snmp_set_var_typed_value(req->requestvb, ASN_GAUGE,
                       (u_char*)&v, sizeof(v)); break; }
        case 5:  { u_long v = row->selftest_status_value;
                   snmp_set_var_typed_value(req->requestvb, ASN_GAUGE,
                       (u_char*)&v, sizeof(v)); break; }
        case 6:  snmp_set_var_typed_value(req->requestvb, ASN_OCTET_STR,
                     (u_char*)row->selftest_status_string.c_str(),
                     row->selftest_status_string.size()); break;
        case 7:  { u_long v = row->polling_short_min;
                   snmp_set_var_typed_value(req->requestvb, ASN_GAUGE,
                       (u_char*)&v, sizeof(v)); break; }
        case 8:  { u_long v = row->polling_ext_min;
                   snmp_set_var_typed_value(req->requestvb, ASN_GAUGE,
                       (u_char*)&v, sizeof(v)); break; }
        case 9:  { u_long v = row->polling_conv_min;
                   snmp_set_var_typed_value(req->requestvb, ASN_GAUGE,
                       (u_char*)&v, sizeof(v)); break; }
        case 10: { long v = row->cap_auto_offline ? 1 : 2;
                   snmp_set_var_typed_value(req->requestvb, ASN_INTEGER,
                       (u_char*)&v, sizeof(v)); break; }
        case 11: { long v = row->cap_selftests ? 1 : 2;
                   snmp_set_var_typed_value(req->requestvb, ASN_INTEGER,
                       (u_char*)&v, sizeof(v)); break; }
        case 12: { long v = row->cap_conveyance ? 1 : 2;
                   snmp_set_var_typed_value(req->requestvb, ASN_INTEGER,
                       (u_char*)&v, sizeof(v)); break; }
        case 13: { long v = row->cap_selective ? 1 : 2;
                   snmp_set_var_typed_value(req->requestvb, ASN_INTEGER,
                       (u_char*)&v, sizeof(v)); break; }
        case 14: { long v = row->cap_error_logging ? 1 : 2;
                   snmp_set_var_typed_value(req->requestvb, ASN_INTEGER,
                       (u_char*)&v, sizeof(v)); break; }
        case 15: { long v = row->cap_gp_logging ? 1 : 2;
                   snmp_set_var_typed_value(req->requestvb, ASN_INTEGER,
                       (u_char*)&v, sizeof(v)); break; }
        case 16: { long v = row->sct_error_recovery ? 1 : 2;
                   snmp_set_var_typed_value(req->requestvb, ASN_INTEGER,
                       (u_char*)&v, sizeof(v)); break; }
        case 17: { long v = row->sct_feature_control ? 1 : 2;
                   snmp_set_var_typed_value(req->requestvb, ASN_INTEGER,
                       (u_char*)&v, sizeof(v)); break; }
        case 18: { long v = row->sct_data_table ? 1 : 2;
                   snmp_set_var_typed_value(req->requestvb, ASN_INTEGER,
                       (u_char*)&v, sizeof(v)); break; }
        case 19: set_counter64(req, row->power_cycles); break;
        case 20: set_counter64(req, row->power_on_hours); break;
        case 21: { u_long v = row->error_log_count;
                   snmp_set_var_typed_value(req->requestvb, ASN_GAUGE,
                       (u_char*)&v, sizeof(v)); break; }
        case 22: { u_long v = 1;  // healthIndex
                   snmp_set_var_typed_value(req->requestvb, ASN_GAUGE,
                       (u_char*)&v, sizeof(v)); break; }
        default: netsnmp_set_request_error(reqinfo, req, SNMP_NOSUCHOBJECT);
        }
    }
    return SNMP_ERR_NOERROR;
}

// ---------------------------------------------------------------------------
// SATA error log table
// INDEX { smartmonDeviceIndex, smartmonSataErrorLogIndex(col 1, NOT-ACCESSIBLE) }
// col 1  = errorIndex (NOT-ACCESSIBLE)
// col 2  = errorNumber
// col 3  = errorLifetimeHours
// col 4  = errorDescription
// col 5  = errorCompRegError
// col 6  = errorCompRegStatus
// col 7  = errorLba
// col 8  = errorRegCommand
// col 9  = errorRegCount
// col 10 = errorRegDevice
// col 11 = errorRegFeature
// col 12 = errorStateValue
// col 13 = errorStateString
// ---------------------------------------------------------------------------

static netsnmp_variable_list *
sata_el_get_next(void **loop_ctx, void **data_ctx,
                 netsnmp_variable_list *put_idx,
                 netsnmp_iterator_info *) {
    size_t idx = (size_t)(uintptr_t)*loop_ctx;
    if (idx >= g_cache.sata_error_log.size()) return nullptr;
    CacheSataErrorLogRow &row = g_cache.sata_error_log[idx];
    *loop_ctx = (void*)(uintptr_t)(idx + 1);
    *data_ctx = &row;
    u_long v = (u_long)row.device_index;
    snmp_set_var_typed_value(put_idx, ASN_UNSIGNED, (u_char*)&v, sizeof(v));
    v = (u_long)row.entry_index;
    snmp_set_var_typed_value(put_idx->next_variable, ASN_UNSIGNED,
                             (u_char*)&v, sizeof(v));
    return put_idx;
}

static netsnmp_variable_list *
sata_el_get_first(void **loop_ctx, void **data_ctx,
                  netsnmp_variable_list *put_idx,
                  netsnmp_iterator_info *ii) {
    *loop_ctx = 0;
    return sata_el_get_next(loop_ctx, data_ctx, put_idx, ii);
}

static int
sata_el_handler(netsnmp_mib_handler *,
                netsnmp_handler_registration *,
                netsnmp_agent_request_info *reqinfo,
                netsnmp_request_info *requests) {
    if (reqinfo->mode != MODE_GET) return SNMP_ERR_NOERROR;

    for (netsnmp_request_info *req = requests; req; req = req->next) {
        netsnmp_table_request_info *tinfo = netsnmp_extract_table_info(req);
        CacheSataErrorLogRow *row =
            (CacheSataErrorLogRow*)netsnmp_extract_iterator_context(req);
        if (!row || !tinfo) continue;

        switch (tinfo->colnum) {
        case 1:  { u_long v = row->entry_index;
                   snmp_set_var_typed_value(req->requestvb, ASN_GAUGE,
                       (u_char*)&v, sizeof(v)); break; }
        case 2:  { u_long v = row->error_number;
                   snmp_set_var_typed_value(req->requestvb, ASN_GAUGE,
                       (u_char*)&v, sizeof(v)); break; }
        case 3:  set_counter64(req, row->lifetime_hours); break;
        case 4:  snmp_set_var_typed_value(req->requestvb, ASN_OCTET_STR,
                     (u_char*)row->description.c_str(),
                     row->description.size()); break;
        case 5:  { u_long v = row->comp_reg_error;
                   snmp_set_var_typed_value(req->requestvb, ASN_GAUGE,
                       (u_char*)&v, sizeof(v)); break; }
        case 6:  { u_long v = row->comp_reg_status;
                   snmp_set_var_typed_value(req->requestvb, ASN_GAUGE,
                       (u_char*)&v, sizeof(v)); break; }
        case 7:  set_counter64(req, row->lba); break;
        case 8:  { u_long v = row->reg_command;
                   snmp_set_var_typed_value(req->requestvb, ASN_GAUGE,
                       (u_char*)&v, sizeof(v)); break; }
        case 9:  { u_long v = row->reg_count;
                   snmp_set_var_typed_value(req->requestvb, ASN_GAUGE,
                       (u_char*)&v, sizeof(v)); break; }
        case 10: { u_long v = row->reg_device;
                   snmp_set_var_typed_value(req->requestvb, ASN_GAUGE,
                       (u_char*)&v, sizeof(v)); break; }
        case 11: { u_long v = row->reg_feature;
                   snmp_set_var_typed_value(req->requestvb, ASN_GAUGE,
                       (u_char*)&v, sizeof(v)); break; }
        case 12: { u_long v = row->state_value;
                   snmp_set_var_typed_value(req->requestvb, ASN_GAUGE,
                       (u_char*)&v, sizeof(v)); break; }
        case 13: snmp_set_var_typed_value(req->requestvb, ASN_OCTET_STR,
                     (u_char*)row->state_string.c_str(),
                     row->state_string.size()); break;
        default: netsnmp_set_request_error(reqinfo, req, SNMP_NOSUCHOBJECT);
        }
    }
    return SNMP_ERR_NOERROR;
}

// ---------------------------------------------------------------------------
// SATA attribute table
// INDEX { smartmonDeviceIndex, smartmonSataAttrId(col 1, NOT-ACCESSIBLE) }
// col 2  = attrName
// col 3  = attrFlags (BITS)
// col 4  = attrType
// col 5  = attrUpdated
// col 6  = attrValue
// col 7  = attrWorst
// col 8  = attrThreshold
// col 9  = attrRawValue (Counter64)
// col 10 = attrRawString
// col 11 = attrWhenFailed
// col 12 = attrStatus
// ---------------------------------------------------------------------------

static netsnmp_variable_list *
sata_attr_get_next(void **loop_ctx, void **data_ctx,
                   netsnmp_variable_list *put_idx,
                   netsnmp_iterator_info *) {
    size_t idx = (size_t)(uintptr_t)*loop_ctx;
    if (idx >= g_cache.sata_attrs.size()) return nullptr;
    CacheSataAttrRow &row = g_cache.sata_attrs[idx];
    *loop_ctx = (void*)(uintptr_t)(idx + 1);
    *data_ctx = &row;
    u_long v = (u_long)row.device_index;
    snmp_set_var_typed_value(put_idx, ASN_UNSIGNED, (u_char*)&v, sizeof(v));
    v = (u_long)row.attr_id;
    snmp_set_var_typed_value(put_idx->next_variable, ASN_UNSIGNED,
                             (u_char*)&v, sizeof(v));
    return put_idx;
}

static netsnmp_variable_list *
sata_attr_get_first(void **loop_ctx, void **data_ctx,
                    netsnmp_variable_list *put_idx,
                    netsnmp_iterator_info *ii) {
    *loop_ctx = 0;
    return sata_attr_get_next(loop_ctx, data_ctx, put_idx, ii);
}

static int
sata_attr_handler(netsnmp_mib_handler *,
                  netsnmp_handler_registration *,
                  netsnmp_agent_request_info *reqinfo,
                  netsnmp_request_info *requests) {
    if (reqinfo->mode != MODE_GET) return SNMP_ERR_NOERROR;

    for (netsnmp_request_info *req = requests; req; req = req->next) {
        netsnmp_table_request_info *tinfo = netsnmp_extract_table_info(req);
        CacheSataAttrRow *row =
            (CacheSataAttrRow*)netsnmp_extract_iterator_context(req);
        if (!row || !tinfo) continue;

        switch (tinfo->colnum) {
        case 2:  snmp_set_var_typed_value(req->requestvb, ASN_OCTET_STR,
                     (u_char*)row->name.c_str(), row->name.size()); break;
        case 3:  { uint8_t b = ata_flags_to_bits(row->flags);
                   snmp_set_var_typed_value(req->requestvb, ASN_OCTET_STR,
                       &b, 1); break; }
        case 4:  { long v = row->attr_type;
                   snmp_set_var_typed_value(req->requestvb, ASN_INTEGER,
                       (u_char*)&v, sizeof(v)); break; }
        case 5:  { long v = row->attr_updated;
                   snmp_set_var_typed_value(req->requestvb, ASN_INTEGER,
                       (u_char*)&v, sizeof(v)); break; }
        case 6:  { u_long v = row->value;
                   snmp_set_var_typed_value(req->requestvb, ASN_GAUGE,
                       (u_char*)&v, sizeof(v)); break; }
        case 7:  { u_long v = row->worst;
                   snmp_set_var_typed_value(req->requestvb, ASN_GAUGE,
                       (u_char*)&v, sizeof(v)); break; }
        case 8:  { u_long v = row->threshold;
                   snmp_set_var_typed_value(req->requestvb, ASN_GAUGE,
                       (u_char*)&v, sizeof(v)); break; }
        case 9:  set_counter64(req, (uint64_t)row->raw_value); break;
        case 10: snmp_set_var_typed_value(req->requestvb, ASN_OCTET_STR,
                     (u_char*)row->raw_string.c_str(),
                     row->raw_string.size()); break;
        case 11: snmp_set_var_typed_value(req->requestvb, ASN_OCTET_STR,
                     (u_char*)row->when_failed.c_str(),
                     row->when_failed.size()); break;
        case 12: { long v = row->status;
                   snmp_set_var_typed_value(req->requestvb, ASN_INTEGER,
                       (u_char*)&v, sizeof(v)); break; }
        default: netsnmp_set_request_error(reqinfo, req, SNMP_NOSUCHOBJECT);
        }
    }
    return SNMP_ERR_NOERROR;
}

// ---------------------------------------------------------------------------
// SATA self-test table
// INDEX { smartmonDeviceIndex, smartmonSataSelfTestIndex(col 1, NOT-ACCESSIBLE) }
// col 2  = selfTestType
// col 4  = selfTestResult
// col 5  = selfTestResultString
// col 6  = selfTestResultPassed (TruthValue)
// col 7  = selfTestRemainingPct
// col 8  = selfTestLifetimeHours
// col 9  = selfTestLbaFirstError
// ---------------------------------------------------------------------------

static netsnmp_variable_list *
sata_st_get_next(void **loop_ctx, void **data_ctx,
                 netsnmp_variable_list *put_idx,
                 netsnmp_iterator_info *) {
    size_t idx = (size_t)(uintptr_t)*loop_ctx;
    if (idx >= g_cache.sata_selftests.size()) return nullptr;
    CacheSataSelfTestRow &row = g_cache.sata_selftests[idx];
    *loop_ctx = (void*)(uintptr_t)(idx + 1);
    *data_ctx = &row;
    u_long v = (u_long)row.device_index;
    snmp_set_var_typed_value(put_idx, ASN_UNSIGNED, (u_char*)&v, sizeof(v));
    v = (u_long)row.entry_index;
    snmp_set_var_typed_value(put_idx->next_variable, ASN_UNSIGNED,
                             (u_char*)&v, sizeof(v));
    return put_idx;
}

static netsnmp_variable_list *
sata_st_get_first(void **loop_ctx, void **data_ctx,
                  netsnmp_variable_list *put_idx,
                  netsnmp_iterator_info *ii) {
    *loop_ctx = 0;
    return sata_st_get_next(loop_ctx, data_ctx, put_idx, ii);
}

static int
sata_st_handler(netsnmp_mib_handler *,
                netsnmp_handler_registration *,
                netsnmp_agent_request_info *reqinfo,
                netsnmp_request_info *requests) {
    if (reqinfo->mode != MODE_GET) return SNMP_ERR_NOERROR;

    for (netsnmp_request_info *req = requests; req; req = req->next) {
        netsnmp_table_request_info *tinfo = netsnmp_extract_table_info(req);
        CacheSataSelfTestRow *row =
            (CacheSataSelfTestRow*)netsnmp_extract_iterator_context(req);
        if (!row || !tinfo) continue;

        switch (tinfo->colnum) {
        case 2:  { long v = row->type;
                   snmp_set_var_typed_value(req->requestvb, ASN_INTEGER,
                       (u_char*)&v, sizeof(v)); break; }
        case 4:  { long v = row->result;
                   snmp_set_var_typed_value(req->requestvb, ASN_INTEGER,
                       (u_char*)&v, sizeof(v)); break; }
        case 5:  snmp_set_var_typed_value(req->requestvb, ASN_OCTET_STR,
                     (u_char*)row->result_str.c_str(),
                     row->result_str.size()); break;
        case 6:  { long v = row->passed ? 1 : 2;  // TruthValue
                   snmp_set_var_typed_value(req->requestvb, ASN_INTEGER,
                       (u_char*)&v, sizeof(v)); break; }
        case 7:  { u_long v = row->remaining_pct;
                   snmp_set_var_typed_value(req->requestvb, ASN_GAUGE,
                       (u_char*)&v, sizeof(v)); break; }
        case 8:  set_counter64(req, row->lifetime_hours); break;
        case 9:  set_counter64(req, row->lba_first_error); break;
        default: netsnmp_set_request_error(reqinfo, req, SNMP_NOSUCHOBJECT);
        }
    }
    return SNMP_ERR_NOERROR;
}

// ---------------------------------------------------------------------------
// SATA error command table
// INDEX { smartmonDeviceIndex, smartmonSataErrorLogIndex, smartmonSataErrorCmdIndex(col 1) }
// col 1  = errorCmdIndex (NOT-ACCESSIBLE)
// col 2  = regCommand
// col 3  = regCount
// col 4  = regDevice
// col 5  = regError   (from parent error entry completion_registers.error)
// col 6  = regFeature
// col 7  = regLba     (CounterBasedGauge64)
// col 8  = regStatus  (from parent error entry completion_registers.status)
// col 9  = timestamp  (powerup_milliseconds)
// col 10 = description
// ---------------------------------------------------------------------------

static netsnmp_variable_list *
sata_errcmd_get_next(void **loop_ctx, void **data_ctx,
                     netsnmp_variable_list *put_idx,
                     netsnmp_iterator_info *) {
    size_t idx = (size_t)(uintptr_t)*loop_ctx;
    if (idx >= g_cache.sata_error_cmds.size()) return nullptr;
    CacheSataErrorCmdRow &row = g_cache.sata_error_cmds[idx];
    *loop_ctx = (void*)(uintptr_t)(idx + 1);
    *data_ctx = &row;
    u_long v = (u_long)row.device_index;
    snmp_set_var_typed_value(put_idx, ASN_UNSIGNED, (u_char*)&v, sizeof(v));
    v = (u_long)row.error_entry_index;
    snmp_set_var_typed_value(put_idx->next_variable, ASN_UNSIGNED,
                             (u_char*)&v, sizeof(v));
    v = (u_long)row.cmd_index;
    snmp_set_var_typed_value(put_idx->next_variable->next_variable, ASN_UNSIGNED,
                             (u_char*)&v, sizeof(v));
    return put_idx;
}

static netsnmp_variable_list *
sata_errcmd_get_first(void **loop_ctx, void **data_ctx,
                      netsnmp_variable_list *put_idx,
                      netsnmp_iterator_info *ii) {
    *loop_ctx = 0;
    return sata_errcmd_get_next(loop_ctx, data_ctx, put_idx, ii);
}

static int
sata_errcmd_handler(netsnmp_mib_handler *,
                    netsnmp_handler_registration *,
                    netsnmp_agent_request_info *reqinfo,
                    netsnmp_request_info *requests) {
    if (reqinfo->mode != MODE_GET) return SNMP_ERR_NOERROR;

    for (netsnmp_request_info *req = requests; req; req = req->next) {
        netsnmp_table_request_info *tinfo = netsnmp_extract_table_info(req);
        CacheSataErrorCmdRow *row =
            (CacheSataErrorCmdRow*)netsnmp_extract_iterator_context(req);
        if (!row || !tinfo) continue;

        switch (tinfo->colnum) {
        case 2:  { u_long v = row->reg_command;
                   snmp_set_var_typed_value(req->requestvb, ASN_GAUGE,
                       (u_char*)&v, sizeof(v)); break; }
        case 3:  { u_long v = row->reg_count;
                   snmp_set_var_typed_value(req->requestvb, ASN_GAUGE,
                       (u_char*)&v, sizeof(v)); break; }
        case 4:  { u_long v = row->reg_device;
                   snmp_set_var_typed_value(req->requestvb, ASN_GAUGE,
                       (u_char*)&v, sizeof(v)); break; }
        case 5:  { u_long v = row->reg_error;
                   snmp_set_var_typed_value(req->requestvb, ASN_GAUGE,
                       (u_char*)&v, sizeof(v)); break; }
        case 6:  { u_long v = row->reg_feature;
                   snmp_set_var_typed_value(req->requestvb, ASN_GAUGE,
                       (u_char*)&v, sizeof(v)); break; }
        case 7:  set_counter64(req, row->reg_lba); break;
        case 8:  { u_long v = row->reg_status;
                   snmp_set_var_typed_value(req->requestvb, ASN_GAUGE,
                       (u_char*)&v, sizeof(v)); break; }
        case 9:  { u_long v = row->timestamp_ms;
                   snmp_set_var_typed_value(req->requestvb, ASN_GAUGE,
                       (u_char*)&v, sizeof(v)); break; }
        case 10: snmp_set_var_typed_value(req->requestvb, ASN_OCTET_STR,
                     (u_char*)row->description.c_str(),
                     row->description.size()); break;
        default: netsnmp_set_request_error(reqinfo, req, SNMP_NOSUCHOBJECT);
        }
    }
    return SNMP_ERR_NOERROR;
}

// ---------------------------------------------------------------------------
// Registration
// ---------------------------------------------------------------------------

void register_sata_mib() {
    // Row-count and last-change scalars for each implemented table
    netsnmp_register_scalar(netsnmp_create_handler_registration(
        "sataInfoTableRowCount",        sata_info_row_count_handler,
        oid_sata_info_row_count,        OID_LEN(oid_sata_info_row_count),        HANDLER_CAN_RONLY));
    netsnmp_register_scalar(netsnmp_create_handler_registration(
        "sataInfoTableLastChange",      sata_info_last_change_handler,
        oid_sata_info_last_change,      OID_LEN(oid_sata_info_last_change),      HANDLER_CAN_RONLY));
    netsnmp_register_scalar(netsnmp_create_handler_registration(
        "sataHealthTableRowCount",      sata_health_row_count_handler,
        oid_sata_health_row_count,      OID_LEN(oid_sata_health_row_count),      HANDLER_CAN_RONLY));
    netsnmp_register_scalar(netsnmp_create_handler_registration(
        "sataHealthTableLastChange",    sata_health_last_change_handler,
        oid_sata_health_last_change,    OID_LEN(oid_sata_health_last_change),    HANDLER_CAN_RONLY));
    netsnmp_register_scalar(netsnmp_create_handler_registration(
        "sataAttrTableRowCount",        sata_attr_row_count_handler,
        oid_sata_attr_row_count,        OID_LEN(oid_sata_attr_row_count),        HANDLER_CAN_RONLY));
    netsnmp_register_scalar(netsnmp_create_handler_registration(
        "sataAttrTableLastChange",      sata_attr_last_change_handler,
        oid_sata_attr_last_change,      OID_LEN(oid_sata_attr_last_change),      HANDLER_CAN_RONLY));
    netsnmp_register_scalar(netsnmp_create_handler_registration(
        "sataErrorLogTableRowCount",    sata_el_row_count_handler,
        oid_sata_error_log_row_count,   OID_LEN(oid_sata_error_log_row_count),   HANDLER_CAN_RONLY));
    netsnmp_register_scalar(netsnmp_create_handler_registration(
        "sataErrorLogTableLastChange",  sata_el_last_change_handler,
        oid_sata_error_log_last_change, OID_LEN(oid_sata_error_log_last_change), HANDLER_CAN_RONLY));
    netsnmp_register_scalar(netsnmp_create_handler_registration(
        "sataSelfTestTableRowCount",    sata_st_row_count_handler,
        oid_sata_selftest_row_count,    OID_LEN(oid_sata_selftest_row_count),    HANDLER_CAN_RONLY));
    netsnmp_register_scalar(netsnmp_create_handler_registration(
        "sataSelfTestTableLastChange",  sata_st_last_change_handler,
        oid_sata_selftest_last_change,  OID_LEN(oid_sata_selftest_last_change),  HANDLER_CAN_RONLY));

    // SATA info table
    {
        netsnmp_handler_registration *reg =
            netsnmp_create_handler_registration(
                "smartmonSataInfoTable", sata_info_handler,
                oid_sata_info_table, OID_LEN(oid_sata_info_table),
                HANDLER_CAN_RONLY);

        netsnmp_table_registration_info *tinfo =
            SNMP_MALLOC_TYPEDEF(netsnmp_table_registration_info);
        netsnmp_table_helper_add_indexes(tinfo, ASN_UNSIGNED, ASN_UNSIGNED, 0);
        tinfo->min_column = 1;
        tinfo->max_column = 17;

        netsnmp_iterator_info *iinfo =
            SNMP_MALLOC_TYPEDEF(netsnmp_iterator_info);
        iinfo->get_first_data_point = sata_info_get_first;
        iinfo->get_next_data_point  = sata_info_get_next;
        iinfo->table_reginfo        = tinfo;

        netsnmp_register_table_iterator(reg, iinfo);
    }

    // SATA health table
    {
        netsnmp_handler_registration *reg =
            netsnmp_create_handler_registration(
                "smartmonSataHealthTable", sata_health_handler,
                oid_sata_health_table, OID_LEN(oid_sata_health_table),
                HANDLER_CAN_RONLY);

        netsnmp_table_registration_info *tinfo =
            SNMP_MALLOC_TYPEDEF(netsnmp_table_registration_info);
        netsnmp_table_helper_add_indexes(tinfo, ASN_UNSIGNED, ASN_UNSIGNED, 0);
        tinfo->min_column = 1;
        tinfo->max_column = 22;

        netsnmp_iterator_info *iinfo =
            SNMP_MALLOC_TYPEDEF(netsnmp_iterator_info);
        iinfo->get_first_data_point = sata_health_get_first;
        iinfo->get_next_data_point  = sata_health_get_next;
        iinfo->table_reginfo        = tinfo;

        netsnmp_register_table_iterator(reg, iinfo);
    }

    // SATA error log table
    {
        netsnmp_handler_registration *reg =
            netsnmp_create_handler_registration(
                "smartmonSataErrorLogTable", sata_el_handler,
                oid_sata_error_log_table, OID_LEN(oid_sata_error_log_table),
                HANDLER_CAN_RONLY);

        netsnmp_table_registration_info *tinfo =
            SNMP_MALLOC_TYPEDEF(netsnmp_table_registration_info);
        netsnmp_table_helper_add_indexes(tinfo, ASN_UNSIGNED, ASN_UNSIGNED, 0);
        tinfo->min_column = 1;
        tinfo->max_column = 13;

        netsnmp_iterator_info *iinfo =
            SNMP_MALLOC_TYPEDEF(netsnmp_iterator_info);
        iinfo->get_first_data_point = sata_el_get_first;
        iinfo->get_next_data_point  = sata_el_get_next;
        iinfo->table_reginfo        = tinfo;

        netsnmp_register_table_iterator(reg, iinfo);
    }

    // SATA attribute table
    {
        netsnmp_handler_registration *reg =
            netsnmp_create_handler_registration(
                "smartmonSataAttrTable", sata_attr_handler,
                oid_sata_attr_table, OID_LEN(oid_sata_attr_table),
                HANDLER_CAN_RONLY);

        netsnmp_table_registration_info *tinfo =
            SNMP_MALLOC_TYPEDEF(netsnmp_table_registration_info);
        netsnmp_table_helper_add_indexes(tinfo, ASN_UNSIGNED, ASN_UNSIGNED, 0);
        tinfo->min_column = 2;
        tinfo->max_column = 12;

        netsnmp_iterator_info *iinfo =
            SNMP_MALLOC_TYPEDEF(netsnmp_iterator_info);
        iinfo->get_first_data_point = sata_attr_get_first;
        iinfo->get_next_data_point  = sata_attr_get_next;
        iinfo->table_reginfo        = tinfo;

        netsnmp_register_table_iterator(reg, iinfo);
    }

    // SATA error cmd table metadata scalars
    netsnmp_register_scalar(netsnmp_create_handler_registration(
        "sataErrorCmdTableRowCount",    sata_errcmd_row_count_handler,
        oid_sata_error_cmd_row_count,   OID_LEN(oid_sata_error_cmd_row_count),   HANDLER_CAN_RONLY));
    netsnmp_register_scalar(netsnmp_create_handler_registration(
        "sataErrorCmdTableLastChange",  sata_errcmd_last_change_handler,
        oid_sata_error_cmd_last_change, OID_LEN(oid_sata_error_cmd_last_change), HANDLER_CAN_RONLY));

    // SATA error cmd table (3 index columns: deviceIndex + errorLogIndex + cmdIndex)
    {
        netsnmp_handler_registration *reg =
            netsnmp_create_handler_registration(
                "smartmonSataErrorCmdTable", sata_errcmd_handler,
                oid_sata_error_cmd_table, OID_LEN(oid_sata_error_cmd_table),
                HANDLER_CAN_RONLY);

        netsnmp_table_registration_info *tinfo =
            SNMP_MALLOC_TYPEDEF(netsnmp_table_registration_info);
        netsnmp_table_helper_add_indexes(tinfo,
            ASN_UNSIGNED, ASN_UNSIGNED, ASN_UNSIGNED, 0);
        tinfo->min_column = 2;
        tinfo->max_column = 10;

        netsnmp_iterator_info *iinfo =
            SNMP_MALLOC_TYPEDEF(netsnmp_iterator_info);
        iinfo->get_first_data_point = sata_errcmd_get_first;
        iinfo->get_next_data_point  = sata_errcmd_get_next;
        iinfo->table_reginfo        = tinfo;

        netsnmp_register_table_iterator(reg, iinfo);
    }

    // SATA self-test table
    {
        netsnmp_handler_registration *reg =
            netsnmp_create_handler_registration(
                "smartmonSataSelfTestTable", sata_st_handler,
                oid_sata_selftest_table, OID_LEN(oid_sata_selftest_table),
                HANDLER_CAN_RONLY);

        netsnmp_table_registration_info *tinfo =
            SNMP_MALLOC_TYPEDEF(netsnmp_table_registration_info);
        netsnmp_table_helper_add_indexes(tinfo, ASN_UNSIGNED, ASN_UNSIGNED, 0);
        tinfo->min_column = 2;
        tinfo->max_column = 9;

        netsnmp_iterator_info *iinfo =
            SNMP_MALLOC_TYPEDEF(netsnmp_iterator_info);
        iinfo->get_first_data_point = sata_st_get_first;
        iinfo->get_next_data_point  = sata_st_get_next;
        iinfo->table_reginfo        = tinfo;

        netsnmp_register_table_iterator(reg, iinfo);
    }
}
