// snmp_sensor_mib.cpp — smartmonSensorTable handler
//
// INDEX { smartmonDeviceIndex, smartmonSensorIndex }
// col 1  = sensorIndex        (NOT-ACCESSIBLE)
// col 2  = sensorType         INTEGER (SmartmonSensorDataType)
// col 3  = sensorName         DisplayString
// col 4  = sensorSource       DisplayString
// col 5  = sensorScale        INTEGER (SmartmonSensorDataScale)
// col 6  = sensorPrecision    Integer32 (SmartmonSensorPrecision)
// col 7  = sensorValue        Integer32 (SmartmonSensorValue)
// col 8  = sensorOperStatus   INTEGER (SmartmonSensorStatus)
// col 9  = sensorUnitsDisplay DisplayString
// col 10 = sensorValueTimestamp DateAndTime
// col 11 = sensorUpdateRate   Unsigned32 (ms)
// col 12 = sensorHighCritical Integer32 (optional)
// col 13 = sensorHighWarning  Integer32 (optional)
// col 14 = sensorLowWarning   Integer32 (optional)
// col 15 = sensorLowCritical  Integer32 (optional)

#include "snmp_sensor_mib.h"
#include "snmp_mib_helpers.h"
#include "agentxd_config.h"

#include <syslog.h>

#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-includes.h>
#include <net-snmp/agent/net-snmp-agent-includes.h>

// ---------------------------------------------------------------------------
// Iterator callbacks
// ---------------------------------------------------------------------------

static netsnmp_variable_list *
sensor_get_next(void **loop_ctx, void **data_ctx,
                netsnmp_variable_list *put_idx,
                netsnmp_iterator_info *) {
    size_t idx = (size_t)(uintptr_t)*loop_ctx;
    if (g_verbosity >= 2)
        syslog(LOG_DEBUG, "sensor_mib: get_next idx=%zu cache=%p sensors.size=%zu",
               idx, (void*)&g_cache, g_cache.sensors.size());
    if (idx >= g_cache.sensors.size()) return nullptr;
    CacheSensorRow &row = g_cache.sensors[idx];
    *loop_ctx = (void*)(uintptr_t)(idx + 1);
    *data_ctx = &row;
    if (g_verbosity >= 2)
        syslog(LOG_DEBUG, "sensor_mib: get_next → dev_idx=%u sensor_idx=%u name='%s'",
               row.device_index, row.sensor_index, row.name.c_str());
    // Index component 1: smartmonDeviceIndex
    u_long v = (u_long)row.device_index;
    snmp_set_var_typed_value(put_idx, ASN_UNSIGNED, (u_char*)&v, sizeof(v));
    // Index component 2: smartmonSensorIndex
    v = (u_long)row.sensor_index;
    snmp_set_var_typed_value(put_idx->next_variable, ASN_UNSIGNED,
                             (u_char*)&v, sizeof(v));
    return put_idx;
}

static netsnmp_variable_list *
sensor_get_first(void **loop_ctx, void **data_ctx,
                 netsnmp_variable_list *put_idx,
                 netsnmp_iterator_info *ii) {
    if (g_verbosity >= 2)
        syslog(LOG_DEBUG, "sensor_mib: get_first cache=%p sensors.size=%zu",
               (void*)&g_cache, g_cache.sensors.size());
    *loop_ctx = 0;
    return sensor_get_next(loop_ctx, data_ctx, put_idx, ii);
}

// ---------------------------------------------------------------------------
// GET handler
// ---------------------------------------------------------------------------

static int
sensor_handler(netsnmp_mib_handler *,
               netsnmp_handler_registration *,
               netsnmp_agent_request_info *reqinfo,
               netsnmp_request_info *requests) {
    if (reqinfo->mode != MODE_GET) return SNMP_ERR_NOERROR;

    for (netsnmp_request_info *req = requests; req; req = req->next) {
        netsnmp_table_request_info *tinfo = netsnmp_extract_table_info(req);
        CacheSensorRow *row =
            (CacheSensorRow*)netsnmp_extract_iterator_context(req);
        if (!row || !tinfo) continue;

        switch (tinfo->colnum) {
        case 2: { long v = row->type;
                  snmp_set_var_typed_value(req->requestvb, ASN_INTEGER,
                      (u_char*)&v, sizeof(v)); break; }
        case 3: snmp_set_var_typed_value(req->requestvb, ASN_OCTET_STR,
                    (u_char*)row->name.c_str(), row->name.size()); break;
        case 4: snmp_set_var_typed_value(req->requestvb, ASN_OCTET_STR,
                    (u_char*)row->source.c_str(), row->source.size()); break;
        case 5: { long v = row->scale;
                  snmp_set_var_typed_value(req->requestvb, ASN_INTEGER,
                      (u_char*)&v, sizeof(v)); break; }
        case 6: { long v = row->precision;
                  snmp_set_var_typed_value(req->requestvb, ASN_INTEGER,
                      (u_char*)&v, sizeof(v)); break; }
        case 7: { long v = row->value;
                  snmp_set_var_typed_value(req->requestvb, ASN_INTEGER,
                      (u_char*)&v, sizeof(v)); break; }
        case 8: { long v = row->oper_status;
                  snmp_set_var_typed_value(req->requestvb, ASN_INTEGER,
                      (u_char*)&v, sizeof(v)); break; }
        case 9: snmp_set_var_typed_value(req->requestvb, ASN_OCTET_STR,
                    (u_char*)row->units_display.c_str(),
                    row->units_display.size()); break;
        case 10: { uint8_t dt[8];
                   snmp_encode_date_time(row->timestamp, dt);
                   snmp_set_var_typed_value(req->requestvb, ASN_OCTET_STR,
                       dt, sizeof(dt)); break; }
        case 11: { u_long v = (u_long)row->update_rate;
                   snmp_set_var_typed_value(req->requestvb, ASN_UNSIGNED,
                       (u_char*)&v, sizeof(v)); break; }
        case 12:
            if (row->has_high_critical) {
                long v = row->high_critical;
                snmp_set_var_typed_value(req->requestvb, ASN_INTEGER,
                    (u_char*)&v, sizeof(v));
            } else {
                netsnmp_set_request_error(reqinfo, req, SNMP_NOSUCHINSTANCE);
            }
            break;
        case 13:
            if (row->has_high_warning) {
                long v = row->high_warning;
                snmp_set_var_typed_value(req->requestvb, ASN_INTEGER,
                    (u_char*)&v, sizeof(v));
            } else {
                netsnmp_set_request_error(reqinfo, req, SNMP_NOSUCHINSTANCE);
            }
            break;
        case 14:
            if (row->has_low_warning) {
                long v = row->low_warning;
                snmp_set_var_typed_value(req->requestvb, ASN_INTEGER,
                    (u_char*)&v, sizeof(v));
            } else {
                netsnmp_set_request_error(reqinfo, req, SNMP_NOSUCHINSTANCE);
            }
            break;
        case 15:
            if (row->has_low_critical) {
                long v = row->low_critical;
                snmp_set_var_typed_value(req->requestvb, ASN_INTEGER,
                    (u_char*)&v, sizeof(v));
            } else {
                netsnmp_set_request_error(reqinfo, req, SNMP_NOSUCHINSTANCE);
            }
            break;
        default: netsnmp_set_request_error(reqinfo, req, SNMP_NOSUCHOBJECT);
        }
    }
    return SNMP_ERR_NOERROR;
}

// ---------------------------------------------------------------------------
// Sensor table metadata scalar handlers
// ---------------------------------------------------------------------------

static int
sensor_row_count_handler(netsnmp_mib_handler *,
                         netsnmp_handler_registration *,
                         netsnmp_agent_request_info *reqinfo,
                         netsnmp_request_info *requests) {
    if (reqinfo->mode != MODE_GET) return SNMP_ERR_NOERROR;
    u_long v = (u_long)g_cache.sensors.size();
    if (g_verbosity >= 2)
        syslog(LOG_DEBUG, "sensor_mib: row_count_handler cache=%p sensors.size=%lu ts_sensor=%ld",
               (void*)&g_cache, v, (long)g_cache.ts_sensor);
    snmp_set_var_typed_value(requests->requestvb, ASN_GAUGE, (u_char*)&v, sizeof(v));
    return SNMP_ERR_NOERROR;
}

static int
sensor_last_change_handler(netsnmp_mib_handler *,
                            netsnmp_handler_registration *,
                            netsnmp_agent_request_info *reqinfo,
                            netsnmp_request_info *requests) {
    if (reqinfo->mode != MODE_GET) return SNMP_ERR_NOERROR;
    uint8_t dt[8];
    snmp_encode_date_time(g_cache.ts_sensor, dt);
    snmp_set_var_typed_value(requests->requestvb, ASN_OCTET_STR, dt, sizeof(dt));
    return SNMP_ERR_NOERROR;
}

// ---------------------------------------------------------------------------
// Registration
// ---------------------------------------------------------------------------

void register_sensor_mib() {
    if (g_verbosity >= 1)
        syslog(LOG_DEBUG, "sensor_mib: register_sensor_mib called cache=%p sensors.size=%zu",
               (void*)&g_cache, g_cache.sensors.size());
    netsnmp_register_scalar(netsnmp_create_handler_registration(
        "smartmonSensorTableRowCount",  sensor_row_count_handler,
        oid_sensor_row_count,  OID_LEN(oid_sensor_row_count),  HANDLER_CAN_RONLY));
    netsnmp_register_scalar(netsnmp_create_handler_registration(
        "smartmonSensorTableLastChange", sensor_last_change_handler,
        oid_sensor_last_change, OID_LEN(oid_sensor_last_change), HANDLER_CAN_RONLY));

    // smartmonSensorTable — sensor_get_first kept for -vv debug logging
    {
        static const int idx[] = { ASN_UNSIGNED, ASN_UNSIGNED, 0 };
        register_table_ronly("smartmonSensorTable", sensor_handler,
                             oid_sensor_table, OID_LEN(oid_sensor_table),
                             sensor_get_first, sensor_get_next, 2, 15, idx);
    }
}
