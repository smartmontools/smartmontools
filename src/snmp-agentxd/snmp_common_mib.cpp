// snmp_common_mib.cpp — smartmonDeviceTable + metadata scalar handlers

#include "snmp_common_mib.h"
#include "snmp_mib_helpers.h"

#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-includes.h>
#include <net-snmp/agent/net-snmp-agent-includes.h>

// ---------------------------------------------------------------------------
// Device table iterator
// ---------------------------------------------------------------------------

#define COL_DEV_NAME        2
#define COL_DEV_PATH        3
#define COL_DEV_TYPE        4
#define COL_DEV_LAST_POLL   5
#define COL_DEV_POLL_RESULT 6
#define COL_DEV_EXIT_STATUS 7
#define COL_DEV_PHYS_INDEX  8
#define COL_DEV_URIS        9

static netsnmp_variable_list *
device_get_next(void **loop_ctx, void **data_ctx,
                netsnmp_variable_list *put_idx,
                netsnmp_iterator_info *) {
    size_t idx = (size_t)(uintptr_t)*loop_ctx;
    if (idx >= g_cache.devices.size()) return nullptr;
    CacheDeviceRow &row = g_cache.devices[idx];
    *loop_ctx = (void*)(uintptr_t)(idx + 1);
    *data_ctx = &row;
    u_long uval = (u_long)row.index;
    snmp_set_var_typed_value(put_idx, ASN_UNSIGNED,
                             (u_char*)&uval, sizeof(uval));
    return put_idx;
}

static int
device_table_handler(netsnmp_mib_handler *,
                     netsnmp_handler_registration *,
                     netsnmp_agent_request_info *reqinfo,
                     netsnmp_request_info *requests) {
    if (reqinfo->mode != MODE_GET) return SNMP_ERR_NOERROR;

    for (netsnmp_request_info *req = requests; req; req = req->next) {
        netsnmp_table_request_info *tinfo = netsnmp_extract_table_info(req);
        CacheDeviceRow *row =
            (CacheDeviceRow*)netsnmp_extract_iterator_context(req);
        if (!row || !tinfo) continue;

        switch (tinfo->colnum) {
        case COL_DEV_NAME:
            snmp_set_var_typed_value(req->requestvb, ASN_OCTET_STR,
                (u_char*)row->name.c_str(), row->name.size());
            break;
        case COL_DEV_PATH:
            snmp_set_var_typed_value(req->requestvb, ASN_OCTET_STR,
                (u_char*)row->path.c_str(), row->path.size());
            break;
        case COL_DEV_TYPE: {
            long v = (long)row->proto;
            snmp_set_var_typed_value(req->requestvb, ASN_INTEGER,
                                     (u_char*)&v, sizeof(v));
            break;
        }
        case COL_DEV_LAST_POLL: {
            uint8_t dt[8];
            snmp_encode_date_time(row->last_poll_time, dt);
            snmp_set_var_typed_value(req->requestvb, ASN_OCTET_STR, dt, 8);
            break;
        }
        case COL_DEV_POLL_RESULT: {
            long v = (long)row->poll_result;
            snmp_set_var_typed_value(req->requestvb, ASN_INTEGER,
                                     (u_char*)&v, sizeof(v));
            break;
        }
        case COL_DEV_EXIT_STATUS: {
            u_long v = (u_long)row->poll_exit_status;
            snmp_set_var_typed_value(req->requestvb, ASN_GAUGE,
                                     (u_char*)&v, sizeof(v));
            break;
        }
        case COL_DEV_PHYS_INDEX: {
            long v = 0;
            snmp_set_var_typed_value(req->requestvb, ASN_INTEGER,
                                     (u_char*)&v, sizeof(v));
            break;
        }
        case COL_DEV_URIS:
            snmp_set_var_typed_value(req->requestvb, ASN_OCTET_STR,
                (u_char*)row->uris.c_str(), row->uris.size());
            break;
        default:
            netsnmp_set_request_error(reqinfo, req, SNMP_NOSUCHOBJECT);
        }
    }
    return SNMP_ERR_NOERROR;
}

// ---------------------------------------------------------------------------
// Per-type count scalars
// ---------------------------------------------------------------------------

static int
device_count_nvme_handler(netsnmp_mib_handler *,
                           netsnmp_handler_registration *,
                           netsnmp_agent_request_info *reqinfo,
                           netsnmp_request_info *requests) {
    if (reqinfo->mode != MODE_GET) return SNMP_ERR_NOERROR;
    u_long count = 0;
    for (const auto &d : g_cache.devices)
        if (d.proto == PROTO_NVME) ++count;
    snmp_set_var_typed_value(requests->requestvb, ASN_GAUGE,
                             (u_char*)&count, sizeof(count));
    return SNMP_ERR_NOERROR;
}

static int
device_count_ata_handler(netsnmp_mib_handler *,
                          netsnmp_handler_registration *,
                          netsnmp_agent_request_info *reqinfo,
                          netsnmp_request_info *requests) {
    if (reqinfo->mode != MODE_GET) return SNMP_ERR_NOERROR;
    u_long count = 0;
    for (const auto &d : g_cache.devices)
        if (d.proto == PROTO_ATA || d.proto == PROTO_SAT) ++count;
    snmp_set_var_typed_value(requests->requestvb, ASN_GAUGE,
                             (u_char*)&count, sizeof(count));
    return SNMP_ERR_NOERROR;
}

static int
device_count_sas_handler(netsnmp_mib_handler *,
                          netsnmp_handler_registration *,
                          netsnmp_agent_request_info *reqinfo,
                          netsnmp_request_info *requests) {
    if (reqinfo->mode != MODE_GET) return SNMP_ERR_NOERROR;
    u_long count = 0;
    for (const auto &d : g_cache.devices)
        if (d.proto == PROTO_SCSI || d.proto == PROTO_SAS) ++count;
    snmp_set_var_typed_value(requests->requestvb, ASN_GAUGE,
                             (u_char*)&count, sizeof(count));
    return SNMP_ERR_NOERROR;
}

// ---------------------------------------------------------------------------
// Device table metadata scalar handlers
// ---------------------------------------------------------------------------

TABLE_ROW_COUNT_HANDLER(device_row_count_handler,    devices)
TABLE_LAST_CHANGE_HANDLER(device_last_change_handler, ts_device_table)

// ---------------------------------------------------------------------------
// Registration
// ---------------------------------------------------------------------------

void register_common_mib() {
    // Device table metadata scalars
    netsnmp_register_scalar(netsnmp_create_handler_registration(
        "smartmonDeviceTableRowCount",  device_row_count_handler,
        oid_device_row_count,  OID_LEN(oid_device_row_count),  HANDLER_CAN_RONLY));
    netsnmp_register_scalar(netsnmp_create_handler_registration(
        "smartmonDeviceTableLastChange", device_last_change_handler,
        oid_device_last_change, OID_LEN(oid_device_last_change), HANDLER_CAN_RONLY));

    // Per-type count scalars
    netsnmp_register_scalar(netsnmp_create_handler_registration(
        "smartmonDeviceCountNvme", device_count_nvme_handler,
        oid_device_count_nvme, OID_LEN(oid_device_count_nvme), HANDLER_CAN_RONLY));
    netsnmp_register_scalar(netsnmp_create_handler_registration(
        "smartmonDeviceCountAta",  device_count_ata_handler,
        oid_device_count_ata,  OID_LEN(oid_device_count_ata),  HANDLER_CAN_RONLY));
    netsnmp_register_scalar(netsnmp_create_handler_registration(
        "smartmonDeviceCountSas",  device_count_sas_handler,
        oid_device_count_sas,  OID_LEN(oid_device_count_sas),  HANDLER_CAN_RONLY));

    // Device table (single ASN_UNSIGNED index: smartmonDeviceIndex)
    REG_TABLE_U("smartmonDeviceTable", device_table_handler, oid_device_table,
                device_get_next, COL_DEV_NAME, COL_DEV_URIS);
}
