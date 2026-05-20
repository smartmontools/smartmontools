// agentxd_notify.cpp — SNMP v2 trap sender

#include "agentxd_notify.h"
#include "agentxd_cache.h"
#include "snmp_oids.h"

#include <cstring>
#include <vector>

#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-includes.h>
#include <net-snmp/agent/net-snmp-agent-includes.h>

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

// Build the snmpTrapOID.0 varbind that starts every v2 trap, plus the
// enterprise OID in snmpTrapEnterprise.0.  Returns the head of the list.
static netsnmp_variable_list *
make_trap_header(const oid *trap_oid, size_t trap_oid_len) {
    netsnmp_variable_list *vars = nullptr;
    // snmpTrapOID.0
    oid snmptrapoid[] = { 1, 3, 6, 1, 6, 3, 1, 1, 4, 1, 0 };
    size_t snmptrapoid_len = sizeof(snmptrapoid) / sizeof(snmptrapoid[0]);
    snmp_varlist_add_variable(&vars,
        snmptrapoid, snmptrapoid_len,
        ASN_OBJECT_ID,
        (u_char*)trap_oid, trap_oid_len * sizeof(oid));
    return vars;
}

static void append_uint32(netsnmp_variable_list **vars,
                          const oid *col_oid, size_t col_len,
                          uint32_t instance_idx,
                          u_char asn_type, u_long value) {
    std::vector<oid> inst(col_len + 1);
    memcpy(inst.data(), col_oid, col_len * sizeof(oid));
    inst[col_len] = (oid)instance_idx;
    snmp_varlist_add_variable(vars, inst.data(), inst.size(),
                              asn_type, (u_char*)&value, sizeof(value));
}

static void append_string(netsnmp_variable_list **vars,
                          const oid *col_oid, size_t col_len,
                          uint32_t instance_idx,
                          const char *str) {
    std::vector<oid> inst(col_len + 1);
    memcpy(inst.data(), col_oid, col_len * sizeof(oid));
    inst[col_len] = (oid)instance_idx;
    snmp_varlist_add_variable(vars, inst.data(), inst.size(),
                              ASN_OCTET_STR,
                              (u_char*)str, strlen(str));
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void notify_device_health_changed(uint32_t dev_idx, int new_status) {
    const oid *notif_oid = oid_notif_nvme_health_changed;
    size_t notif_len     = OID_LEN(oid_notif_nvme_health_changed);
    const CacheDeviceRow *dev = g_cache.find_device(dev_idx);
    if (dev) {
        if (dev->proto == PROTO_ATA || dev->proto == PROTO_SAT) {
            notif_oid = oid_notif_sata_health_degraded;
            notif_len = OID_LEN(oid_notif_sata_health_degraded);
        } else if (dev->proto == PROTO_SCSI || dev->proto == PROTO_SAS) {
            notif_oid = oid_notif_sas_health_changed;
            notif_len = OID_LEN(oid_notif_sas_health_changed);
        }
    }
    netsnmp_variable_list *vars = make_trap_header(notif_oid, notif_len);

    if (dev) {
        append_string(&vars, oid_device_path, OID_LEN(oid_device_path),
                      dev_idx, dev->path.c_str());
    }
    // smartmonDeviceLastPollResult.dev_idx
    append_uint32(&vars, oid_device_last_poll_result,
                  OID_LEN(oid_device_last_poll_result),
                  dev_idx, ASN_INTEGER, (u_long)new_status);

    send_v2trap(vars);
    snmp_free_varbind(vars);
}

void notify_device_polling_failed(uint32_t dev_idx, int poll_result) {
    netsnmp_variable_list *vars =
        make_trap_header(oid_notif_device_poll_failed,
                         OID_LEN(oid_notif_device_poll_failed));

    const CacheDeviceRow *dev = g_cache.find_device(dev_idx);
    if (dev) {
        append_string(&vars, oid_device_name, OID_LEN(oid_device_name),
                      dev_idx, dev->name.c_str());
        append_string(&vars, oid_device_path, OID_LEN(oid_device_path),
                      dev_idx, dev->path.c_str());
    }
    append_uint32(&vars, oid_device_last_poll_result,
                  OID_LEN(oid_device_last_poll_result),
                  dev_idx, ASN_INTEGER, (u_long)poll_result);

    send_v2trap(vars);
    snmp_free_varbind(vars);
}

void notify_selftest_failed(uint32_t dev_idx, const char *type_str,
                            int result_code) {
    // Determine protocol from cache to choose the right notification OID
    const oid *notif_oid = oid_notif_sata_selftest_failed;
    size_t notif_len     = OID_LEN(oid_notif_sata_selftest_failed);

    for (const auto &d : g_cache.devices) {
        if (d.index != dev_idx) continue;
        if (d.proto == PROTO_NVME) {
            notif_oid = oid_notif_nvme_selftest_failed;
            notif_len = OID_LEN(oid_notif_nvme_selftest_failed);
        } else if (d.proto == PROTO_SCSI || d.proto == PROTO_SAS) {
            notif_oid = oid_notif_sas_selftest_failed;
            notif_len = OID_LEN(oid_notif_sas_selftest_failed);
        }
        break;
    }

    netsnmp_variable_list *vars = make_trap_header(notif_oid, notif_len);

    // Include device path and result code as varbinds
    const CacheDeviceRow *dev = g_cache.find_device(dev_idx);

    if (dev) {
        append_string(&vars, oid_device_path, OID_LEN(oid_device_path),
                      dev_idx, dev->path.c_str());
    }
    if (type_str && *type_str) {
        // Use device_name OID as a convenient DisplayString slot for type
        append_string(&vars, oid_device_name, OID_LEN(oid_device_name),
                      dev_idx, type_str);
    }
    append_uint32(&vars, oid_device_last_poll_result,
                  OID_LEN(oid_device_last_poll_result),
                  dev_idx, ASN_INTEGER, (u_long)result_code);

    send_v2trap(vars);
    snmp_free_varbind(vars);
}
