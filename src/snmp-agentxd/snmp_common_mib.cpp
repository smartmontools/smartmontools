// snmp_common_mib.cpp — smartmonDeviceTable + metadata scalar handlers

#include "snmp_common_mib.h"
#include "agentxd_cache.h"
#include "snmp_oids.h"

#include <cstring>
#include <ctime>

#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-includes.h>
#include <net-snmp/agent/net-snmp-agent-includes.h>

// ---------------------------------------------------------------------------
// Utility: encode time_t as 8-byte DateAndTime (local time)
// ---------------------------------------------------------------------------
static void encode_date_time(time_t t, uint8_t out[8]) {
    struct tm *tm = localtime(&t);
    if (!tm) { memset(out, 0, 8); return; }
    uint16_t year = (uint16_t)(tm->tm_year + 1900);
    out[0] = (uint8_t)(year >> 8);
    out[1] = (uint8_t)(year & 0xff);
    out[2] = (uint8_t)(tm->tm_mon + 1);
    out[3] = (uint8_t)tm->tm_mday;
    out[4] = (uint8_t)tm->tm_hour;
    out[5] = (uint8_t)tm->tm_min;
    out[6] = (uint8_t)tm->tm_sec;
    out[7] = 0;
}

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

static netsnmp_variable_list *
device_get_first(void **loop_ctx, void **data_ctx,
                 netsnmp_variable_list *put_idx,
                 netsnmp_iterator_info *ii) {
    *loop_ctx = 0;
    return device_get_next(loop_ctx, data_ctx, put_idx, ii);
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
            encode_date_time(row->last_poll_time, dt);
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
        default:
            netsnmp_set_request_error(reqinfo, req, SNMP_NOSUCHOBJECT);
        }
    }
    return SNMP_ERR_NOERROR;
}

// ---------------------------------------------------------------------------
// Device table row-count scalar (smartmonDeviceTableRowCount)
// ---------------------------------------------------------------------------

static int
device_row_count_handler(netsnmp_mib_handler *,
                         netsnmp_handler_registration *,
                         netsnmp_agent_request_info *reqinfo,
                         netsnmp_request_info *requests) {
    if (reqinfo->mode != MODE_GET) return SNMP_ERR_NOERROR;
    u_long count = (u_long)g_cache.devices.size();
    snmp_set_var_typed_value(requests->requestvb, ASN_GAUGE,
                             (u_char*)&count, sizeof(count));
    return SNMP_ERR_NOERROR;
}

// ---------------------------------------------------------------------------
// Device table last-change scalar (smartmonDeviceTableLastChange)
// ---------------------------------------------------------------------------

static int
device_last_change_handler(netsnmp_mib_handler *,
                            netsnmp_handler_registration *,
                            netsnmp_agent_request_info *reqinfo,
                            netsnmp_request_info *requests) {
    if (reqinfo->mode != MODE_GET) return SNMP_ERR_NOERROR;
    uint8_t dt[8];
    encode_date_time(g_cache.ts_device_table, dt);
    snmp_set_var_typed_value(requests->requestvb, ASN_OCTET_STR, dt, sizeof(dt));
    return SNMP_ERR_NOERROR;
}

// ---------------------------------------------------------------------------
// Registration
// ---------------------------------------------------------------------------

void register_common_mib() {
    // Device table row-count scalar
    {
        netsnmp_handler_registration *reg =
            netsnmp_create_handler_registration(
                "smartmonDeviceTableRowCount", device_row_count_handler,
                oid_device_row_count, OID_LEN(oid_device_row_count),
                HANDLER_CAN_RONLY);
        netsnmp_register_scalar(reg);
    }

    // Device table last-change scalar
    {
        netsnmp_handler_registration *reg =
            netsnmp_create_handler_registration(
                "smartmonDeviceTableLastChange", device_last_change_handler,
                oid_device_last_change, OID_LEN(oid_device_last_change),
                HANDLER_CAN_RONLY);
        netsnmp_register_scalar(reg);
    }

    // Device table
    {
        netsnmp_handler_registration *reg =
            netsnmp_create_handler_registration(
                "smartmonDeviceTable", device_table_handler,
                oid_device_table, OID_LEN(oid_device_table),
                HANDLER_CAN_RONLY);

        netsnmp_table_registration_info *tinfo =
            SNMP_MALLOC_TYPEDEF(netsnmp_table_registration_info);
        netsnmp_table_helper_add_indexes(tinfo, ASN_UNSIGNED, 0);
        tinfo->min_column = COL_DEV_NAME;
        tinfo->max_column = COL_DEV_PHYS_INDEX;

        netsnmp_iterator_info *iinfo =
            SNMP_MALLOC_TYPEDEF(netsnmp_iterator_info);
        iinfo->get_first_data_point = device_get_first;
        iinfo->get_next_data_point  = device_get_next;
        iinfo->table_reginfo        = tinfo;

        netsnmp_register_table_iterator(reg, iinfo);
    }
}
