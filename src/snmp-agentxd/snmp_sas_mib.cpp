// snmp_sas_mib.cpp — SAS/SCSI health, error counter, self-test table handlers

#include "snmp_sas_mib.h"
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
static void sas_encode_dt(time_t t, uint8_t out[8]) {
    struct tm *tm = gmtime(&t);
    if (!tm) { memset(out, 0, 8); return; }
    uint16_t y = (uint16_t)(tm->tm_year + 1900);
    out[0] = (uint8_t)(y >> 8); out[1] = (uint8_t)(y & 0xff);
    out[2] = (uint8_t)(tm->tm_mon + 1); out[3] = (uint8_t)tm->tm_mday;
    out[4] = (uint8_t)tm->tm_hour; out[5] = (uint8_t)tm->tm_min;
    out[6] = (uint8_t)tm->tm_sec; out[7] = 0;
}

// ---------------------------------------------------------------------------
// SAS table metadata scalar handlers
// ---------------------------------------------------------------------------

#define SAS_ROW_COUNT_HANDLER(name, cache_field) \
static int name(netsnmp_mib_handler *, netsnmp_handler_registration *, \
                netsnmp_agent_request_info *reqinfo, netsnmp_request_info *requests) { \
    if (reqinfo->mode != MODE_GET) return SNMP_ERR_NOERROR; \
    u_long v = (u_long)g_cache.cache_field.size(); \
    snmp_set_var_typed_value(requests->requestvb, ASN_GAUGE, (u_char*)&v, sizeof(v)); \
    return SNMP_ERR_NOERROR; \
}

#define SAS_LAST_CHANGE_HANDLER(name, ts_field) \
static int name(netsnmp_mib_handler *, netsnmp_handler_registration *, \
                netsnmp_agent_request_info *reqinfo, netsnmp_request_info *requests) { \
    if (reqinfo->mode != MODE_GET) return SNMP_ERR_NOERROR; \
    uint8_t dt[8]; sas_encode_dt(g_cache.ts_field, dt); \
    snmp_set_var_typed_value(requests->requestvb, ASN_OCTET_STR, dt, sizeof(dt)); \
    return SNMP_ERR_NOERROR; \
}

SAS_ROW_COUNT_HANDLER(sas_info_row_count_handler,      sas_info)
SAS_LAST_CHANGE_HANDLER(sas_info_last_change_handler,   ts_sas_info)

SAS_ROW_COUNT_HANDLER(sas_health_row_count_handler,    sas_health)
SAS_LAST_CHANGE_HANDLER(sas_health_last_change_handler, ts_sas_health)

SAS_ROW_COUNT_HANDLER(sas_ec_row_count_handler,        sas_error_counters)
SAS_LAST_CHANGE_HANDLER(sas_ec_last_change_handler,     ts_sas_error_counter)

SAS_ROW_COUNT_HANDLER(sas_st_row_count_handler,        sas_selftests)
SAS_LAST_CHANGE_HANDLER(sas_st_last_change_handler,     ts_sas_selftest)

SAS_ROW_COUNT_HANDLER(sas_bgscan_row_count_handler,    sas_bgscan)
SAS_LAST_CHANGE_HANDLER(sas_bgscan_last_change_handler, ts_sas_bgscan)

static void set_counter64(netsnmp_request_info *req, uint64_t val) {
    struct counter64 c64;
    c64.high = (u_long)(val >> 32);
    c64.low  = (u_long)(val & 0xffffffffUL);
    snmp_set_var_typed_value(req->requestvb, ASN_COUNTER64,
                             (u_char*)&c64, sizeof(c64));
}

// ---------------------------------------------------------------------------
// SAS info table
// INDEX { smartmonDeviceIndex, smartmonSasInfoIndex(col 15, NOT-ACCESSIBLE) }
// col 1  = vendor
// col 2  = product
// col 3  = revision
// col 4  = compliance
// col 5  = serialNumber
// col 6  = wwn
// col 7  = scsiModelName
// col 8  = rotationRate
// col 9  = formFactor
// col 10 = logicalBlockSize
// col 11 = physicalBlockSize
// col 12 = userCapacityBytes
// col 13 = powerCycles
// col 14 = powerOnHours
// col 15 = infoIndex (NOT-ACCESSIBLE, index)
// ---------------------------------------------------------------------------

static netsnmp_variable_list *
sas_info_get_next(void **loop_ctx, void **data_ctx,
                  netsnmp_variable_list *put_idx,
                  netsnmp_iterator_info *) {
    size_t idx = (size_t)(uintptr_t)*loop_ctx;
    if (idx >= g_cache.sas_info.size()) return nullptr;
    CacheSasInfoRow &row = g_cache.sas_info[idx];
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
sas_info_get_first(void **loop_ctx, void **data_ctx,
                   netsnmp_variable_list *put_idx,
                   netsnmp_iterator_info *ii) {
    *loop_ctx = 0;
    return sas_info_get_next(loop_ctx, data_ctx, put_idx, ii);
}

static int
sas_info_handler(netsnmp_mib_handler *,
                 netsnmp_handler_registration *,
                 netsnmp_agent_request_info *reqinfo,
                 netsnmp_request_info *requests) {
    if (reqinfo->mode != MODE_GET) return SNMP_ERR_NOERROR;

    for (netsnmp_request_info *req = requests; req; req = req->next) {
        netsnmp_table_request_info *tinfo = netsnmp_extract_table_info(req);
        CacheSasInfoRow *row =
            (CacheSasInfoRow*)netsnmp_extract_iterator_context(req);
        if (!row || !tinfo) continue;

        switch (tinfo->colnum) {
        case 1:  snmp_set_var_typed_value(req->requestvb, ASN_OCTET_STR,
                     (u_char*)row->vendor.c_str(),
                     row->vendor.size()); break;
        case 2:  snmp_set_var_typed_value(req->requestvb, ASN_OCTET_STR,
                     (u_char*)row->product.c_str(),
                     row->product.size()); break;
        case 3:  snmp_set_var_typed_value(req->requestvb, ASN_OCTET_STR,
                     (u_char*)row->revision.c_str(),
                     row->revision.size()); break;
        case 4:  snmp_set_var_typed_value(req->requestvb, ASN_OCTET_STR,
                     (u_char*)row->compliance.c_str(),
                     row->compliance.size()); break;
        case 5:  snmp_set_var_typed_value(req->requestvb, ASN_OCTET_STR,
                     (u_char*)row->serial_number.c_str(),
                     row->serial_number.size()); break;
        case 6:  snmp_set_var_typed_value(req->requestvb, ASN_OCTET_STR,
                     (u_char*)row->wwn.c_str(),
                     row->wwn.size()); break;
        case 7:  snmp_set_var_typed_value(req->requestvb, ASN_OCTET_STR,
                     (u_char*)row->scsi_model_name.c_str(),
                     row->scsi_model_name.size()); break;
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
        case 13: set_counter64(req, row->power_cycles); break;
        case 14: set_counter64(req, row->power_on_hours); break;
        case 15: { u_long v = 1;  // infoIndex
                   snmp_set_var_typed_value(req->requestvb, ASN_GAUGE,
                       (u_char*)&v, sizeof(v)); break; }
        default: netsnmp_set_request_error(reqinfo, req, SNMP_NOSUCHOBJECT);
        }
    }
    return SNMP_ERR_NOERROR;
}

// ---------------------------------------------------------------------------
// SAS background scan table
// INDEX { smartmonDeviceIndex, smartmonSasBgScanIndex(col 6, NOT-ACCESSIBLE) }
// col 1  = bgScanStatus (INTEGER)
// col 2  = bgScanProgressPercent
// col 3  = bgScanScansPerformed
// col 4  = bgScanMediumScansPerformed
// col 5  = bgScanScanResults
// col 6  = bgScanIndex (NOT-ACCESSIBLE, index)
// ---------------------------------------------------------------------------

static netsnmp_variable_list *
sas_bgscan_get_next(void **loop_ctx, void **data_ctx,
                    netsnmp_variable_list *put_idx,
                    netsnmp_iterator_info *) {
    size_t idx = (size_t)(uintptr_t)*loop_ctx;
    if (idx >= g_cache.sas_bgscan.size()) return nullptr;
    CacheSasBgScanRow &row = g_cache.sas_bgscan[idx];
    *loop_ctx = (void*)(uintptr_t)(idx + 1);
    *data_ctx = &row;
    u_long v = (u_long)row.device_index;
    snmp_set_var_typed_value(put_idx, ASN_UNSIGNED, (u_char*)&v, sizeof(v));
    v = 1;  // bgScanIndex = 1 per device
    snmp_set_var_typed_value(put_idx->next_variable, ASN_UNSIGNED,
                             (u_char*)&v, sizeof(v));
    return put_idx;
}

static netsnmp_variable_list *
sas_bgscan_get_first(void **loop_ctx, void **data_ctx,
                     netsnmp_variable_list *put_idx,
                     netsnmp_iterator_info *ii) {
    *loop_ctx = 0;
    return sas_bgscan_get_next(loop_ctx, data_ctx, put_idx, ii);
}

static int
sas_bgscan_handler(netsnmp_mib_handler *,
                   netsnmp_handler_registration *,
                   netsnmp_agent_request_info *reqinfo,
                   netsnmp_request_info *requests) {
    if (reqinfo->mode != MODE_GET) return SNMP_ERR_NOERROR;

    for (netsnmp_request_info *req = requests; req; req = req->next) {
        netsnmp_table_request_info *tinfo = netsnmp_extract_table_info(req);
        CacheSasBgScanRow *row =
            (CacheSasBgScanRow*)netsnmp_extract_iterator_context(req);
        if (!row || !tinfo) continue;

        switch (tinfo->colnum) {
        case 1:  { long v = (long)row->status_value;
                   snmp_set_var_typed_value(req->requestvb, ASN_INTEGER,
                       (u_char*)&v, sizeof(v)); break; }
        case 2:  { u_long v = row->progress_percent;
                   snmp_set_var_typed_value(req->requestvb, ASN_GAUGE,
                       (u_char*)&v, sizeof(v)); break; }
        case 3:  set_counter64(req, row->scans_performed); break;
        case 4:  set_counter64(req, row->medium_scans); break;
        case 5:  snmp_set_var_typed_value(req->requestvb, ASN_OCTET_STR,
                     (u_char*)row->scan_results.c_str(),
                     row->scan_results.size()); break;
        case 6:  { u_long v = 1;  // bgScanIndex
                   snmp_set_var_typed_value(req->requestvb, ASN_GAUGE,
                       (u_char*)&v, sizeof(v)); break; }
        default: netsnmp_set_request_error(reqinfo, req, SNMP_NOSUCHOBJECT);
        }
    }
    return SNMP_ERR_NOERROR;
}

// ---------------------------------------------------------------------------
// SAS health table
// INDEX { smartmonDeviceIndex, smartmonSasHealthIndex(col 6, NOT-ACCESSIBLE) }
// col 1 = overallStatus
// col 2 = grownDefectCount
// col 3 = nonMediumErrorCount (Counter64)
// col 4 = informationalExceptions (TruthValue)
// col 5 = pendingDefectCount
// col 6 = healthIndex (NOT-ACCESSIBLE, index)
// ---------------------------------------------------------------------------

static netsnmp_variable_list *
sas_health_get_next(void **loop_ctx, void **data_ctx,
                    netsnmp_variable_list *put_idx,
                    netsnmp_iterator_info *) {
    size_t idx = (size_t)(uintptr_t)*loop_ctx;
    if (idx >= g_cache.sas_health.size()) return nullptr;
    CacheSasHealthRow &row = g_cache.sas_health[idx];
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
sas_health_get_first(void **loop_ctx, void **data_ctx,
                     netsnmp_variable_list *put_idx,
                     netsnmp_iterator_info *ii) {
    *loop_ctx = 0;
    return sas_health_get_next(loop_ctx, data_ctx, put_idx, ii);
}

static int
sas_health_handler(netsnmp_mib_handler *,
                   netsnmp_handler_registration *,
                   netsnmp_agent_request_info *reqinfo,
                   netsnmp_request_info *requests) {
    if (reqinfo->mode != MODE_GET) return SNMP_ERR_NOERROR;

    for (netsnmp_request_info *req = requests; req; req = req->next) {
        netsnmp_table_request_info *tinfo = netsnmp_extract_table_info(req);
        CacheSasHealthRow *row =
            (CacheSasHealthRow*)netsnmp_extract_iterator_context(req);
        if (!row || !tinfo) continue;

        switch (tinfo->colnum) {
        case 1:  { long v = row->overall_status;
                   snmp_set_var_typed_value(req->requestvb, ASN_INTEGER,
                       (u_char*)&v, sizeof(v)); break; }
        case 2:  { u_long v = row->grown_defect_count;
                   snmp_set_var_typed_value(req->requestvb, ASN_GAUGE,
                       (u_char*)&v, sizeof(v)); break; }
        case 3:  set_counter64(req, row->non_medium_errors); break;
        case 4:  { long v = row->info_exceptions ? 1 : 2;  // TruthValue
                   snmp_set_var_typed_value(req->requestvb, ASN_INTEGER,
                       (u_char*)&v, sizeof(v)); break; }
        case 5:  { u_long v = row->pending_defects;
                   snmp_set_var_typed_value(req->requestvb, ASN_GAUGE,
                       (u_char*)&v, sizeof(v)); break; }
        default: netsnmp_set_request_error(reqinfo, req, SNMP_NOSUCHOBJECT);
        }
    }
    return SNMP_ERR_NOERROR;
}

// ---------------------------------------------------------------------------
// SAS error counter table
// INDEX { smartmonDeviceIndex, smartmonSasErrorDirection(col 1, NOT-ACCESSIBLE) }
// col 1 = direction (NOT-ACCESSIBLE, index)
// col 2 = eccDelayed
// col 3 = eccFast
// col 4 = rereadsRewrites
// col 5 = totalCorrected
// col 6 = algorithmInvocations
// col 7 = bytesProcessed
// col 8 = uncorrected
// ---------------------------------------------------------------------------

static netsnmp_variable_list *
sas_ec_get_next(void **loop_ctx, void **data_ctx,
                netsnmp_variable_list *put_idx,
                netsnmp_iterator_info *) {
    size_t idx = (size_t)(uintptr_t)*loop_ctx;
    if (idx >= g_cache.sas_error_counters.size()) return nullptr;
    CacheSasErrorCounterRow &row = g_cache.sas_error_counters[idx];
    *loop_ctx = (void*)(uintptr_t)(idx + 1);
    *data_ctx = &row;
    u_long v = (u_long)row.device_index;
    snmp_set_var_typed_value(put_idx, ASN_UNSIGNED, (u_char*)&v, sizeof(v));
    long dir = (long)row.direction;
    snmp_set_var_typed_value(put_idx->next_variable, ASN_INTEGER,
                             (u_char*)&dir, sizeof(dir));
    return put_idx;
}

static netsnmp_variable_list *
sas_ec_get_first(void **loop_ctx, void **data_ctx,
                 netsnmp_variable_list *put_idx,
                 netsnmp_iterator_info *ii) {
    *loop_ctx = 0;
    return sas_ec_get_next(loop_ctx, data_ctx, put_idx, ii);
}

static int
sas_ec_handler(netsnmp_mib_handler *,
               netsnmp_handler_registration *,
               netsnmp_agent_request_info *reqinfo,
               netsnmp_request_info *requests) {
    if (reqinfo->mode != MODE_GET) return SNMP_ERR_NOERROR;

    for (netsnmp_request_info *req = requests; req; req = req->next) {
        netsnmp_table_request_info *tinfo = netsnmp_extract_table_info(req);
        CacheSasErrorCounterRow *row =
            (CacheSasErrorCounterRow*)netsnmp_extract_iterator_context(req);
        if (!row || !tinfo) continue;

        switch (tinfo->colnum) {
        case 2:  set_counter64(req, row->ecc_delayed); break;
        case 3:  set_counter64(req, row->ecc_fast); break;
        case 4:  set_counter64(req, row->rereads_rewrites); break;
        case 5:  set_counter64(req, row->total_corrected); break;
        case 6:  set_counter64(req, row->algorithm_invoked); break;
        case 7:  set_counter64(req, row->bytes_processed); break;
        case 8:  set_counter64(req, row->uncorrected); break;
        default: netsnmp_set_request_error(reqinfo, req, SNMP_NOSUCHOBJECT);
        }
    }
    return SNMP_ERR_NOERROR;
}

// ---------------------------------------------------------------------------
// SAS self-test table
// INDEX { smartmonDeviceIndex, smartmonSasSelfTestIndex(col 1, NOT-ACCESSIBLE) }
// col 1 = selfTestIndex (NOT-ACCESSIBLE)
// col 2 = selfTestType
// col 4 = selfTestResult
// col 5 = selfTestResultString
// col 6 = selfTestResultPassed (TruthValue)
// col 7 = selfTestPowerOnHours
// col 8 = selfTestLbaFirstError
// ---------------------------------------------------------------------------

static netsnmp_variable_list *
sas_st_get_next(void **loop_ctx, void **data_ctx,
                netsnmp_variable_list *put_idx,
                netsnmp_iterator_info *) {
    size_t idx = (size_t)(uintptr_t)*loop_ctx;
    if (idx >= g_cache.sas_selftests.size()) return nullptr;
    CacheSasSelfTestRow &row = g_cache.sas_selftests[idx];
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
sas_st_get_first(void **loop_ctx, void **data_ctx,
                 netsnmp_variable_list *put_idx,
                 netsnmp_iterator_info *ii) {
    *loop_ctx = 0;
    return sas_st_get_next(loop_ctx, data_ctx, put_idx, ii);
}

static int
sas_st_handler(netsnmp_mib_handler *,
               netsnmp_handler_registration *,
               netsnmp_agent_request_info *reqinfo,
               netsnmp_request_info *requests) {
    if (reqinfo->mode != MODE_GET) return SNMP_ERR_NOERROR;

    for (netsnmp_request_info *req = requests; req; req = req->next) {
        netsnmp_table_request_info *tinfo = netsnmp_extract_table_info(req);
        CacheSasSelfTestRow *row =
            (CacheSasSelfTestRow*)netsnmp_extract_iterator_context(req);
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
        case 6:  { long v = row->passed ? 1 : 2;
                   snmp_set_var_typed_value(req->requestvb, ASN_INTEGER,
                       (u_char*)&v, sizeof(v)); break; }
        case 7:  set_counter64(req, row->power_on_hours); break;
        case 8:  set_counter64(req, row->lba_first_error); break;
        default: netsnmp_set_request_error(reqinfo, req, SNMP_NOSUCHOBJECT);
        }
    }
    return SNMP_ERR_NOERROR;
}

// ---------------------------------------------------------------------------
// Registration
// ---------------------------------------------------------------------------

void register_sas_mib() {
    // Row-count and last-change scalars for each implemented table
    netsnmp_register_scalar(netsnmp_create_handler_registration(
        "sasInfoTableRowCount",           sas_info_row_count_handler,
        oid_sas_info_row_count,           OID_LEN(oid_sas_info_row_count),           HANDLER_CAN_RONLY));
    netsnmp_register_scalar(netsnmp_create_handler_registration(
        "sasInfoTableLastChange",         sas_info_last_change_handler,
        oid_sas_info_last_change,         OID_LEN(oid_sas_info_last_change),         HANDLER_CAN_RONLY));
    netsnmp_register_scalar(netsnmp_create_handler_registration(
        "sasHealthTableRowCount",         sas_health_row_count_handler,
        oid_sas_health_row_count,         OID_LEN(oid_sas_health_row_count),         HANDLER_CAN_RONLY));
    netsnmp_register_scalar(netsnmp_create_handler_registration(
        "sasHealthTableLastChange",       sas_health_last_change_handler,
        oid_sas_health_last_change,       OID_LEN(oid_sas_health_last_change),       HANDLER_CAN_RONLY));
    netsnmp_register_scalar(netsnmp_create_handler_registration(
        "sasErrorCounterTableRowCount",   sas_ec_row_count_handler,
        oid_sas_error_counter_row_count,  OID_LEN(oid_sas_error_counter_row_count),  HANDLER_CAN_RONLY));
    netsnmp_register_scalar(netsnmp_create_handler_registration(
        "sasErrorCounterTableLastChange", sas_ec_last_change_handler,
        oid_sas_error_counter_last_change,OID_LEN(oid_sas_error_counter_last_change),HANDLER_CAN_RONLY));
    netsnmp_register_scalar(netsnmp_create_handler_registration(
        "sasSelfTestTableRowCount",       sas_st_row_count_handler,
        oid_sas_selftest_row_count,       OID_LEN(oid_sas_selftest_row_count),       HANDLER_CAN_RONLY));
    netsnmp_register_scalar(netsnmp_create_handler_registration(
        "sasSelfTestTableLastChange",     sas_st_last_change_handler,
        oid_sas_selftest_last_change,     OID_LEN(oid_sas_selftest_last_change),     HANDLER_CAN_RONLY));
    netsnmp_register_scalar(netsnmp_create_handler_registration(
        "sasBgScanTableRowCount",         sas_bgscan_row_count_handler,
        oid_sas_bgscan_row_count,         OID_LEN(oid_sas_bgscan_row_count),         HANDLER_CAN_RONLY));
    netsnmp_register_scalar(netsnmp_create_handler_registration(
        "sasBgScanTableLastChange",       sas_bgscan_last_change_handler,
        oid_sas_bgscan_last_change,       OID_LEN(oid_sas_bgscan_last_change),       HANDLER_CAN_RONLY));

    // SAS info table
    {
        netsnmp_handler_registration *reg =
            netsnmp_create_handler_registration(
                "smartmonSasInfoTable", sas_info_handler,
                oid_sas_info_table, OID_LEN(oid_sas_info_table),
                HANDLER_CAN_RONLY);

        netsnmp_table_registration_info *tinfo =
            SNMP_MALLOC_TYPEDEF(netsnmp_table_registration_info);
        netsnmp_table_helper_add_indexes(tinfo, ASN_UNSIGNED, ASN_UNSIGNED, 0);
        tinfo->min_column = 1;
        tinfo->max_column = 15;

        netsnmp_iterator_info *iinfo =
            SNMP_MALLOC_TYPEDEF(netsnmp_iterator_info);
        iinfo->get_first_data_point = sas_info_get_first;
        iinfo->get_next_data_point  = sas_info_get_next;
        iinfo->table_reginfo        = tinfo;

        netsnmp_register_table_iterator(reg, iinfo);
    }

    // SAS background scan table
    {
        netsnmp_handler_registration *reg =
            netsnmp_create_handler_registration(
                "smartmonSasBackgroundScanTable", sas_bgscan_handler,
                oid_sas_bgscan_table, OID_LEN(oid_sas_bgscan_table),
                HANDLER_CAN_RONLY);

        netsnmp_table_registration_info *tinfo =
            SNMP_MALLOC_TYPEDEF(netsnmp_table_registration_info);
        netsnmp_table_helper_add_indexes(tinfo, ASN_UNSIGNED, ASN_UNSIGNED, 0);
        tinfo->min_column = 1;
        tinfo->max_column = 6;

        netsnmp_iterator_info *iinfo =
            SNMP_MALLOC_TYPEDEF(netsnmp_iterator_info);
        iinfo->get_first_data_point = sas_bgscan_get_first;
        iinfo->get_next_data_point  = sas_bgscan_get_next;
        iinfo->table_reginfo        = tinfo;

        netsnmp_register_table_iterator(reg, iinfo);
    }

    // SAS health table
    {
        netsnmp_handler_registration *reg =
            netsnmp_create_handler_registration(
                "smartmonSasHealthTable", sas_health_handler,
                oid_sas_health_table, OID_LEN(oid_sas_health_table),
                HANDLER_CAN_RONLY);

        netsnmp_table_registration_info *tinfo =
            SNMP_MALLOC_TYPEDEF(netsnmp_table_registration_info);
        netsnmp_table_helper_add_indexes(tinfo, ASN_UNSIGNED, ASN_UNSIGNED, 0);
        tinfo->min_column = 1;
        tinfo->max_column = 5;

        netsnmp_iterator_info *iinfo =
            SNMP_MALLOC_TYPEDEF(netsnmp_iterator_info);
        iinfo->get_first_data_point = sas_health_get_first;
        iinfo->get_next_data_point  = sas_health_get_next;
        iinfo->table_reginfo        = tinfo;

        netsnmp_register_table_iterator(reg, iinfo);
    }

    // SAS error counter table (direction is INTEGER index, second column)
    {
        netsnmp_handler_registration *reg =
            netsnmp_create_handler_registration(
                "smartmonSasErrorCounterTable", sas_ec_handler,
                oid_sas_error_counter_table, OID_LEN(oid_sas_error_counter_table),
                HANDLER_CAN_RONLY);

        netsnmp_table_registration_info *tinfo =
            SNMP_MALLOC_TYPEDEF(netsnmp_table_registration_info);
        netsnmp_table_helper_add_indexes(tinfo, ASN_UNSIGNED, ASN_INTEGER, 0);
        tinfo->min_column = 2;
        tinfo->max_column = 8;

        netsnmp_iterator_info *iinfo =
            SNMP_MALLOC_TYPEDEF(netsnmp_iterator_info);
        iinfo->get_first_data_point = sas_ec_get_first;
        iinfo->get_next_data_point  = sas_ec_get_next;
        iinfo->table_reginfo        = tinfo;

        netsnmp_register_table_iterator(reg, iinfo);
    }

    // SAS self-test table
    {
        netsnmp_handler_registration *reg =
            netsnmp_create_handler_registration(
                "smartmonSasSelfTestTable", sas_st_handler,
                oid_sas_selftest_table, OID_LEN(oid_sas_selftest_table),
                HANDLER_CAN_RONLY);

        netsnmp_table_registration_info *tinfo =
            SNMP_MALLOC_TYPEDEF(netsnmp_table_registration_info);
        netsnmp_table_helper_add_indexes(tinfo, ASN_UNSIGNED, ASN_UNSIGNED, 0);
        tinfo->min_column = 2;
        tinfo->max_column = 8;

        netsnmp_iterator_info *iinfo =
            SNMP_MALLOC_TYPEDEF(netsnmp_iterator_info);
        iinfo->get_first_data_point = sas_st_get_first;
        iinfo->get_next_data_point  = sas_st_get_next;
        iinfo->table_reginfo        = tinfo;

        netsnmp_register_table_iterator(reg, iinfo);
    }
}
