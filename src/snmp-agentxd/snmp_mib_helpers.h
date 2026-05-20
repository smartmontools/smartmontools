// snmp_mib_helpers.h — shared helpers for SNMP MIB handler files

#pragma once

#include "agentxd_cache.h"
#include "snmp_oids.h"

#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-includes.h>
#include <net-snmp/agent/net-snmp-agent-includes.h>

// ---------------------------------------------------------------------------
// Scalar handler generators for table metadata
// ---------------------------------------------------------------------------

// Generate a Gauge32 handler returning the size of a cache vector
#define TABLE_ROW_COUNT_HANDLER(fn_name, cache_field) \
static int fn_name(netsnmp_mib_handler *, netsnmp_handler_registration *, \
                   netsnmp_agent_request_info *reqinfo, netsnmp_request_info *requests) { \
    if (reqinfo->mode != MODE_GET) return SNMP_ERR_NOERROR; \
    u_long v = (u_long)g_cache.cache_field.size(); \
    snmp_set_var_typed_value(requests->requestvb, ASN_GAUGE, (u_char*)&v, sizeof(v)); \
    return SNMP_ERR_NOERROR; \
}

// Generate a DateAndTime handler returning a cache timestamp
#define TABLE_LAST_CHANGE_HANDLER(fn_name, ts_field) \
static int fn_name(netsnmp_mib_handler *, netsnmp_handler_registration *, \
                   netsnmp_agent_request_info *reqinfo, netsnmp_request_info *requests) { \
    if (reqinfo->mode != MODE_GET) return SNMP_ERR_NOERROR; \
    uint8_t dt[8]; snmp_encode_date_time(g_cache.ts_field, dt); \
    snmp_set_var_typed_value(requests->requestvb, ASN_OCTET_STR, dt, sizeof(dt)); \
    return SNMP_ERR_NOERROR; \
}

// ---------------------------------------------------------------------------
// Counter64 response helper (replaces identical copies in each MIB file)
// ---------------------------------------------------------------------------
static inline void set_counter64(netsnmp_request_info *req, uint64_t val) {
    struct counter64 c64;
    c64.high = (u_long)(val >> 32);
    c64.low  = (u_long)(val & 0xffffffffUL);
    snmp_set_var_typed_value(req->requestvb, ASN_COUNTER64,
                             (u_char*)&c64, sizeof(c64));
}

// ---------------------------------------------------------------------------
// Generic get_first: resets loop_ctx to 0 and delegates to the registered
// get_next.  Eliminates the trivial per-table get_first boilerplate.
// ---------------------------------------------------------------------------
static inline netsnmp_variable_list *
snmp_table_get_first(void **loop_ctx, void **data_ctx,
                     netsnmp_variable_list *put_idx,
                     netsnmp_iterator_info *ii) {
    *loop_ctx = (void*)(uintptr_t)0;
    return ii->get_next_data_point(loop_ctx, data_ctx, put_idx, ii);
}

// ---------------------------------------------------------------------------
// Read-only table iterator registration helper.
// idx_types: zero-terminated array of ASN_* type codes for the index columns.
// Uses snmp_table_get_first unless get_first is overridden by the caller.
// ---------------------------------------------------------------------------
static inline void
register_table_ronly(const char *name,
                     Netsnmp_Node_Handler *handler,
                     const oid *tbl_oid, size_t tbl_oid_len,
                     Netsnmp_First_Data_Point *get_first,
                     Netsnmp_Next_Data_Point *get_next,
                     int min_col, int max_col,
                     const int *idx_types) {
    netsnmp_handler_registration *reg =
        netsnmp_create_handler_registration(name, handler,
                                            tbl_oid, tbl_oid_len, HANDLER_CAN_RONLY);
    netsnmp_table_registration_info *tinfo =
        SNMP_MALLOC_TYPEDEF(netsnmp_table_registration_info);
    for (const int *t = idx_types; *t; ++t)
        netsnmp_table_helper_add_indexes(tinfo, *t, 0);
    tinfo->min_column = min_col;
    tinfo->max_column = max_col;
    netsnmp_iterator_info *iinfo =
        SNMP_MALLOC_TYPEDEF(netsnmp_iterator_info);
    iinfo->get_first_data_point = get_first;
    iinfo->get_next_data_point  = get_next;
    iinfo->table_reginfo        = tinfo;
    netsnmp_register_table_iterator(reg, iinfo);
}

// Convenience: two ASN_UNSIGNED index columns (the common case)
#define REG_TABLE_UU(name, handler, oid_arr, get_next, min_c, max_c) do { \
    static const int _idx[] = { ASN_UNSIGNED, ASN_UNSIGNED, 0 }; \
    register_table_ronly(name, handler, oid_arr, OID_LEN(oid_arr), \
                         snmp_table_get_first, get_next, min_c, max_c, _idx); \
} while(0)

// Convenience: three ASN_UNSIGNED index columns
#define REG_TABLE_UUU(name, handler, oid_arr, get_next, min_c, max_c) do { \
    static const int _idx[] = { ASN_UNSIGNED, ASN_UNSIGNED, ASN_UNSIGNED, 0 }; \
    register_table_ronly(name, handler, oid_arr, OID_LEN(oid_arr), \
                         snmp_table_get_first, get_next, min_c, max_c, _idx); \
} while(0)

// Convenience: (ASN_UNSIGNED, ASN_INTEGER) index columns
#define REG_TABLE_UI(name, handler, oid_arr, get_next, min_c, max_c) do { \
    static const int _idx[] = { ASN_UNSIGNED, ASN_INTEGER, 0 }; \
    register_table_ronly(name, handler, oid_arr, OID_LEN(oid_arr), \
                         snmp_table_get_first, get_next, min_c, max_c, _idx); \
} while(0)

// Convenience: one ASN_UNSIGNED index column (device table)
#define REG_TABLE_U(name, handler, oid_arr, get_next, min_c, max_c) do { \
    static const int _idx[] = { ASN_UNSIGNED, 0 }; \
    register_table_ronly(name, handler, oid_arr, OID_LEN(oid_arr), \
                         snmp_table_get_first, get_next, min_c, max_c, _idx); \
} while(0)
