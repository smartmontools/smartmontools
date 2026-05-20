// snmp_nvme_mib.cpp — NVMe health and self-test table handlers

#include "snmp_nvme_mib.h"
#include "snmp_mib_helpers.h"

#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-includes.h>
#include <net-snmp/agent/net-snmp-agent-includes.h>

// ---------------------------------------------------------------------------
// NVMe table metadata scalar handlers
// ---------------------------------------------------------------------------

TABLE_ROW_COUNT_HANDLER(nvme_ctrl_row_count_handler,    nvme_controllers)
TABLE_LAST_CHANGE_HANDLER(nvme_ctrl_last_change_handler, ts_nvme_controller)

TABLE_ROW_COUNT_HANDLER(nvme_ns_row_count_handler,      nvme_namespaces)
TABLE_LAST_CHANGE_HANDLER(nvme_ns_last_change_handler,   ts_nvme_namespace)

TABLE_ROW_COUNT_HANDLER(nvme_health_row_count_handler,   nvme_health)
TABLE_LAST_CHANGE_HANDLER(nvme_health_last_change_handler, ts_nvme_health)

TABLE_ROW_COUNT_HANDLER(nvme_st_row_count_handler,       nvme_selftests)
TABLE_LAST_CHANGE_HANDLER(nvme_st_last_change_handler,    ts_nvme_selftest)

TABLE_ROW_COUNT_HANDLER(nvme_el_row_count_handler,       nvme_error_log)
TABLE_LAST_CHANGE_HANDLER(nvme_el_last_change_handler,   ts_nvme_error_log)

TABLE_ROW_COUNT_HANDLER(nvme_cap_row_count_handler,      nvme_capabilities)
TABLE_LAST_CHANGE_HANDLER(nvme_cap_last_change_handler,  ts_nvme_capability)

TABLE_ROW_COUNT_HANDLER(nvme_ps_row_count_handler,       nvme_power_states)
TABLE_LAST_CHANGE_HANDLER(nvme_ps_last_change_handler,   ts_nvme_power_state)

TABLE_ROW_COUNT_HANDLER(nvme_lba_row_count_handler,      nvme_lba_formats)
TABLE_LAST_CHANGE_HANDLER(nvme_lba_last_change_handler,  ts_nvme_lba_format)

static void set_bits1(netsnmp_request_info *req, uint8_t raw_bits) {
    // ASN.1 BITS: bit 0 of the named bits → MSB of first octet
    uint8_t b = 0;
    for (int i = 0; i < 8; i++)
        if (raw_bits & (1 << i)) b |= (uint8_t)(0x80u >> i);
    snmp_set_var_typed_value(req->requestvb, ASN_OCTET_STR, &b, 1);
}

// ---------------------------------------------------------------------------
// NVMe health table
// INDEX { smartmonDeviceIndex, smartmonNvmeHealthIndex(col 21) }
// col 1  = overallStatus
// col 2  = criticalWarning (BITS)
// col 4  = availableSpare
// col 5  = availableSpareThreshold
// col 6  = percentageUsed
// col 7  = dataUnitsRead
// col 8  = dataUnitsWritten
// col 9  = dataBytesRead
// col 10 = dataBytesWritten
// col 11 = hostReadCommands
// col 12 = hostWriteCommands
// col 13 = controllerBusyTimeMinutes
// col 14 = powerCycles
// col 15 = powerOnHours
// col 16 = unsafeShutdowns
// col 17 = mediaDataIntegrityErrors
// col 18 = errorInformationLogEntries
// col 19 = warningTemperatureTimeMinutes
// col 20 = criticalTemperatureTimeMinutes
// col 21 = healthIndex (NOT-ACCESSIBLE, index)
// col 22 = currentSelfTestOperationValue
// col 23 = currentSelfTestOperationString
// ---------------------------------------------------------------------------

static netsnmp_variable_list *
nvme_health_get_next(void **loop_ctx, void **data_ctx,
                     netsnmp_variable_list *put_idx,
                     netsnmp_iterator_info *) {
    size_t idx = (size_t)(uintptr_t)*loop_ctx;
    if (idx >= g_cache.nvme_health.size()) return nullptr;
    CacheNvmeHealthRow &row = g_cache.nvme_health[idx];
    *loop_ctx = (void*)(uintptr_t)(idx + 1);
    *data_ctx = &row;
    // Index 1: smartmonDeviceIndex
    u_long v = (u_long)row.device_index;
    snmp_set_var_typed_value(put_idx, ASN_UNSIGNED, (u_char*)&v, sizeof(v));
    // Index 2: smartmonNvmeHealthIndex (always 1 per device)
    v = 1;
    snmp_set_var_typed_value(put_idx->next_variable, ASN_UNSIGNED,
                             (u_char*)&v, sizeof(v));
    return put_idx;
}

static int
nvme_health_handler(netsnmp_mib_handler *,
                    netsnmp_handler_registration *,
                    netsnmp_agent_request_info *reqinfo,
                    netsnmp_request_info *requests) {
    if (reqinfo->mode != MODE_GET) return SNMP_ERR_NOERROR;

    for (netsnmp_request_info *req = requests; req; req = req->next) {
        netsnmp_table_request_info *tinfo = netsnmp_extract_table_info(req);
        CacheNvmeHealthRow *row =
            (CacheNvmeHealthRow*)netsnmp_extract_iterator_context(req);
        if (!row || !tinfo) continue;

        switch (tinfo->colnum) {
        case 1: { long v = row->overall_status;
                  snmp_set_var_typed_value(req->requestvb, ASN_INTEGER,
                      (u_char*)&v, sizeof(v)); break; }
        case 2:  set_bits1(req, row->critical_warning); break;
        case 4:  { u_long v = row->available_spare_pct;
                   snmp_set_var_typed_value(req->requestvb, ASN_GAUGE,
                       (u_char*)&v, sizeof(v)); break; }
        case 5:  { u_long v = row->available_spare_thresh;
                   snmp_set_var_typed_value(req->requestvb, ASN_GAUGE,
                       (u_char*)&v, sizeof(v)); break; }
        case 6:  { u_long v = row->percentage_used;
                   snmp_set_var_typed_value(req->requestvb, ASN_GAUGE,
                       (u_char*)&v, sizeof(v)); break; }
        case 7:  set_counter64(req, row->data_units_read); break;
        case 8:  set_counter64(req, row->data_units_written); break;
        case 9:  set_counter64(req, row->data_bytes_read); break;
        case 10: set_counter64(req, row->data_bytes_written); break;
        case 11: set_counter64(req, row->host_read_commands); break;
        case 12: set_counter64(req, row->host_write_commands); break;
        case 13: set_counter64(req, row->controller_busy_minutes); break;
        case 14: set_counter64(req, row->power_cycles); break;
        case 15: set_counter64(req, row->power_on_hours); break;
        case 16: set_counter64(req, row->unsafe_shutdowns); break;
        case 17: set_counter64(req, row->media_errors); break;
        case 18: set_counter64(req, row->error_log_entries); break;
        case 19: set_counter64(req, row->warning_temp_minutes); break;
        case 20: set_counter64(req, row->critical_temp_minutes); break;
        case 22: { u_long v = row->current_selftest_value;
                   snmp_set_var_typed_value(req->requestvb, ASN_GAUGE,
                       (u_char*)&v, sizeof(v)); break; }
        case 23: snmp_set_var_typed_value(req->requestvb, ASN_OCTET_STR,
                     (u_char*)row->current_selftest_str.c_str(),
                     row->current_selftest_str.size()); break;
        default: netsnmp_set_request_error(reqinfo, req, SNMP_NOSUCHOBJECT);
        }
    }
    return SNMP_ERR_NOERROR;
}

// ---------------------------------------------------------------------------
// NVMe self-test table
// INDEX { smartmonDeviceIndex, smartmonNvmeSelfTestIndex(col 1) }
// col 1  = selfTestIndex (NOT-ACCESSIBLE)
// col 2  = selfTestNumber
// col 3  = selfTestType
// col 4  = selfTestResult
// col 5  = selfTestResultText
// col 6  = selfTestPowerOnHours
// col 7  = selfTestFailingLba
// col 8  = selfTestNamespaceId
// col 9  = selfTestSegmentNumber
// col 10 = selfTestStatusCodeType
// col 11 = selfTestStatusCode
// ---------------------------------------------------------------------------

static netsnmp_variable_list *
nvme_st_get_next(void **loop_ctx, void **data_ctx,
                 netsnmp_variable_list *put_idx,
                 netsnmp_iterator_info *) {
    size_t idx = (size_t)(uintptr_t)*loop_ctx;
    if (idx >= g_cache.nvme_selftests.size()) return nullptr;
    CacheNvmeSelfTestRow &row = g_cache.nvme_selftests[idx];
    *loop_ctx = (void*)(uintptr_t)(idx + 1);
    *data_ctx = &row;
    u_long v = (u_long)row.device_index;
    snmp_set_var_typed_value(put_idx, ASN_UNSIGNED, (u_char*)&v, sizeof(v));
    v = (u_long)row.entry_index;
    snmp_set_var_typed_value(put_idx->next_variable, ASN_UNSIGNED,
                             (u_char*)&v, sizeof(v));
    return put_idx;
}

static int
nvme_st_handler(netsnmp_mib_handler *,
                netsnmp_handler_registration *,
                netsnmp_agent_request_info *reqinfo,
                netsnmp_request_info *requests) {
    if (reqinfo->mode != MODE_GET) return SNMP_ERR_NOERROR;

    for (netsnmp_request_info *req = requests; req; req = req->next) {
        netsnmp_table_request_info *tinfo = netsnmp_extract_table_info(req);
        CacheNvmeSelfTestRow *row =
            (CacheNvmeSelfTestRow*)netsnmp_extract_iterator_context(req);
        if (!row || !tinfo) continue;

        switch (tinfo->colnum) {
        case 2:  { u_long v = row->number;
                   snmp_set_var_typed_value(req->requestvb, ASN_GAUGE,
                       (u_char*)&v, sizeof(v)); break; }
        case 3:  { long v = row->type;
                   snmp_set_var_typed_value(req->requestvb, ASN_INTEGER,
                       (u_char*)&v, sizeof(v)); break; }
        case 4:  { long v = row->result;
                   snmp_set_var_typed_value(req->requestvb, ASN_INTEGER,
                       (u_char*)&v, sizeof(v)); break; }
        case 5:  snmp_set_var_typed_value(req->requestvb, ASN_OCTET_STR,
                     (u_char*)row->result_text.c_str(),
                     row->result_text.size()); break;
        case 6:  set_counter64(req, row->power_on_hours); break;
        case 7:  set_counter64(req, row->failing_lba); break;
        case 8:  { u_long v = row->namespace_id;
                   snmp_set_var_typed_value(req->requestvb, ASN_GAUGE,
                       (u_char*)&v, sizeof(v)); break; }
        case 9:  { u_long v = row->segment_number;
                   snmp_set_var_typed_value(req->requestvb, ASN_GAUGE,
                       (u_char*)&v, sizeof(v)); break; }
        case 10: { u_long v = row->status_code_type;
                   snmp_set_var_typed_value(req->requestvb, ASN_GAUGE,
                       (u_char*)&v, sizeof(v)); break; }
        case 11: { u_long v = row->status_code;
                   snmp_set_var_typed_value(req->requestvb, ASN_GAUGE,
                       (u_char*)&v, sizeof(v)); break; }
        default: netsnmp_set_request_error(reqinfo, req, SNMP_NOSUCHOBJECT);
        }
    }
    return SNMP_ERR_NOERROR;
}

// ---------------------------------------------------------------------------
// NVMe controller table
// INDEX { smartmonDeviceIndex, smartmonNvmeControllerIndex(col 14) }
// col 1  = modelNumber
// col 2  = serialNumber
// col 3  = firmwareVersion
// col 4  = pciVendorId
// col 5  = ieeeOuiIdentifier
// col 6  = totalNvmCapacityBytes
// col 7  = unallocatedNvmCapacityBytes
// col 8  = controllerId
// col 9  = version
// col 10 = namespaceCount
// col 14 = controllerIndex (NOT-ACCESSIBLE, index)
// col 15 = pciVendorSubsystemId
// col 16 = versionValue
// col 17 = pciVendorIdText
// col 18 = pciVendorSubsystemIdText
// ---------------------------------------------------------------------------

static netsnmp_variable_list *
nvme_ctrl_get_next(void **loop_ctx, void **data_ctx,
                   netsnmp_variable_list *put_idx,
                   netsnmp_iterator_info *) {
    size_t idx = (size_t)(uintptr_t)*loop_ctx;
    if (idx >= g_cache.nvme_controllers.size()) return nullptr;
    CacheNvmeControllerRow &row = g_cache.nvme_controllers[idx];
    *loop_ctx = (void*)(uintptr_t)(idx + 1);
    *data_ctx = &row;
    u_long v = (u_long)row.device_index;
    snmp_set_var_typed_value(put_idx, ASN_UNSIGNED, (u_char*)&v, sizeof(v));
    v = 1;  // controllerIndex = 1 per device
    snmp_set_var_typed_value(put_idx->next_variable, ASN_UNSIGNED,
                             (u_char*)&v, sizeof(v));
    return put_idx;
}

static int
nvme_ctrl_handler(netsnmp_mib_handler *,
                  netsnmp_handler_registration *,
                  netsnmp_agent_request_info *reqinfo,
                  netsnmp_request_info *requests) {
    if (reqinfo->mode != MODE_GET) return SNMP_ERR_NOERROR;

    for (netsnmp_request_info *req = requests; req; req = req->next) {
        netsnmp_table_request_info *tinfo = netsnmp_extract_table_info(req);
        CacheNvmeControllerRow *row =
            (CacheNvmeControllerRow*)netsnmp_extract_iterator_context(req);
        if (!row || !tinfo) continue;

        switch (tinfo->colnum) {
        case 1:  snmp_set_var_typed_value(req->requestvb, ASN_OCTET_STR,
                     (u_char*)row->model_number.c_str(),
                     row->model_number.size()); break;
        case 2:  snmp_set_var_typed_value(req->requestvb, ASN_OCTET_STR,
                     (u_char*)row->serial_number.c_str(),
                     row->serial_number.size()); break;
        case 3:  snmp_set_var_typed_value(req->requestvb, ASN_OCTET_STR,
                     (u_char*)row->firmware_version.c_str(),
                     row->firmware_version.size()); break;
        case 4:  { u_long v = row->pci_vendor_id;
                   snmp_set_var_typed_value(req->requestvb, ASN_GAUGE,
                       (u_char*)&v, sizeof(v)); break; }
        case 5:  { u_long v = row->ieee_oui;
                   snmp_set_var_typed_value(req->requestvb, ASN_GAUGE,
                       (u_char*)&v, sizeof(v)); break; }
        case 6:  set_counter64(req, row->total_capacity); break;
        case 7:  set_counter64(req, row->unallocated_capacity); break;
        case 8:  { u_long v = row->controller_id;
                   snmp_set_var_typed_value(req->requestvb, ASN_GAUGE,
                       (u_char*)&v, sizeof(v)); break; }
        case 9:  snmp_set_var_typed_value(req->requestvb, ASN_OCTET_STR,
                     (u_char*)row->version_string.c_str(),
                     row->version_string.size()); break;
        case 10: { u_long v = row->namespace_count;
                   snmp_set_var_typed_value(req->requestvb, ASN_GAUGE,
                       (u_char*)&v, sizeof(v)); break; }
        case 11: { u_long v = row->max_data_transfer_pages;
                   snmp_set_var_typed_value(req->requestvb, ASN_GAUGE,
                       (u_char*)&v, sizeof(v)); break; }
        case 14: { u_long v = 1;  // controllerIndex
                   snmp_set_var_typed_value(req->requestvb, ASN_GAUGE,
                       (u_char*)&v, sizeof(v)); break; }
        case 15: { u_long v = row->pci_subsystem_id;
                   snmp_set_var_typed_value(req->requestvb, ASN_GAUGE,
                       (u_char*)&v, sizeof(v)); break; }
        case 16: { u_long v = row->version_value;
                   snmp_set_var_typed_value(req->requestvb, ASN_GAUGE,
                       (u_char*)&v, sizeof(v)); break; }
        case 17: snmp_set_var_typed_value(req->requestvb, ASN_OCTET_STR,
                     (u_char*)row->pci_vendor_id_text.c_str(),
                     row->pci_vendor_id_text.size()); break;
        case 18: snmp_set_var_typed_value(req->requestvb, ASN_OCTET_STR,
                     (u_char*)row->pci_subsystem_vendor_text.c_str(),
                     row->pci_subsystem_vendor_text.size()); break;
        default: netsnmp_set_request_error(reqinfo, req, SNMP_NOSUCHOBJECT);
        }
    }
    return SNMP_ERR_NOERROR;
}

// ---------------------------------------------------------------------------
// NVMe namespace table
// INDEX { smartmonDeviceIndex, smartmonNvmeNamespaceId(col 1) }
// col 1  = namespaceId
// col 2  = namespaceSizeBytes
// col 3  = namespaceCapacityBytes
// col 4  = namespaceUtilizationBytes
// col 5  = namespaceFormattedLbaSizeBytes
// col 6  = namespaceEui64
// col 7  = namespaceNguid
// col 8  = namespaceSizeBlocks
// col 9  = namespaceCapacityBlocks
// col 10 = namespaceUtilizationBlocks
// ---------------------------------------------------------------------------

static netsnmp_variable_list *
nvme_ns_get_next(void **loop_ctx, void **data_ctx,
                 netsnmp_variable_list *put_idx,
                 netsnmp_iterator_info *) {
    size_t idx = (size_t)(uintptr_t)*loop_ctx;
    if (idx >= g_cache.nvme_namespaces.size()) return nullptr;
    CacheNvmeNamespaceRow &row = g_cache.nvme_namespaces[idx];
    *loop_ctx = (void*)(uintptr_t)(idx + 1);
    *data_ctx = &row;
    u_long v = (u_long)row.device_index;
    snmp_set_var_typed_value(put_idx, ASN_UNSIGNED, (u_char*)&v, sizeof(v));
    v = (u_long)row.namespace_id;
    snmp_set_var_typed_value(put_idx->next_variable, ASN_UNSIGNED,
                             (u_char*)&v, sizeof(v));
    return put_idx;
}

static int
nvme_ns_handler(netsnmp_mib_handler *,
                netsnmp_handler_registration *,
                netsnmp_agent_request_info *reqinfo,
                netsnmp_request_info *requests) {
    if (reqinfo->mode != MODE_GET) return SNMP_ERR_NOERROR;

    for (netsnmp_request_info *req = requests; req; req = req->next) {
        netsnmp_table_request_info *tinfo = netsnmp_extract_table_info(req);
        CacheNvmeNamespaceRow *row =
            (CacheNvmeNamespaceRow*)netsnmp_extract_iterator_context(req);
        if (!row || !tinfo) continue;

        switch (tinfo->colnum) {
        case 1:  { u_long v = row->namespace_id;
                   snmp_set_var_typed_value(req->requestvb, ASN_GAUGE,
                       (u_char*)&v, sizeof(v)); break; }
        case 2:  set_counter64(req, row->size_bytes); break;
        case 3:  set_counter64(req, row->capacity_bytes); break;
        case 4:  set_counter64(req, row->utilization_bytes); break;
        case 5:  { u_long v = row->formatted_lba_size;
                   snmp_set_var_typed_value(req->requestvb, ASN_GAUGE,
                       (u_char*)&v, sizeof(v)); break; }
        case 6:  snmp_set_var_typed_value(req->requestvb, ASN_OCTET_STR,
                     (u_char*)"", 0); break;
        case 7:  snmp_set_var_typed_value(req->requestvb, ASN_OCTET_STR,
                     (u_char*)"", 0); break;
        case 8:  set_counter64(req, row->size_blocks); break;
        case 9:  set_counter64(req, row->capacity_blocks); break;
        case 10: set_counter64(req, row->utilization_blocks); break;
        default: netsnmp_set_request_error(reqinfo, req, SNMP_NOSUCHOBJECT);
        }
    }
    return SNMP_ERR_NOERROR;
}

// ---------------------------------------------------------------------------
// NVMe error log table
// INDEX { smartmonDeviceIndex, smartmonNvmeErrorLogIndex(col 1) }
// col 1  = errorLogIndex (NOT-ACCESSIBLE)
// col 2  = errorCount
// col 3  = errorSubmissionQueueId
// col 4  = errorCommandId
// col 5  = errorStatusField
// col 6  = errorParameterErrorLocation
// col 7  = errorLba
// col 8  = errorNamespaceId
// col 9  = errorVendorSpecificInfo
// col 10 = errorStatusCode
// col 11 = errorStatusCodeType
// col 12 = errorDoNotRetry (TruthValue)
// col 13 = errorStatusString
// col 14 = errorPhaseTag (TruthValue)
// ---------------------------------------------------------------------------

static netsnmp_variable_list *
nvme_el_get_next(void **loop_ctx, void **data_ctx,
                 netsnmp_variable_list *put_idx,
                 netsnmp_iterator_info *) {
    size_t idx = (size_t)(uintptr_t)*loop_ctx;
    if (idx >= g_cache.nvme_error_log.size()) return nullptr;
    CacheNvmeErrorLogRow &row = g_cache.nvme_error_log[idx];
    *loop_ctx = (void*)(uintptr_t)(idx + 1);
    *data_ctx = &row;
    u_long v = (u_long)row.device_index;
    snmp_set_var_typed_value(put_idx, ASN_UNSIGNED, (u_char*)&v, sizeof(v));
    v = (u_long)row.entry_index;
    snmp_set_var_typed_value(put_idx->next_variable, ASN_UNSIGNED,
                             (u_char*)&v, sizeof(v));
    return put_idx;
}

static int
nvme_el_handler(netsnmp_mib_handler *,
                netsnmp_handler_registration *,
                netsnmp_agent_request_info *reqinfo,
                netsnmp_request_info *requests) {
    if (reqinfo->mode != MODE_GET) return SNMP_ERR_NOERROR;

    for (netsnmp_request_info *req = requests; req; req = req->next) {
        netsnmp_table_request_info *tinfo = netsnmp_extract_table_info(req);
        CacheNvmeErrorLogRow *row =
            (CacheNvmeErrorLogRow*)netsnmp_extract_iterator_context(req);
        if (!row || !tinfo) continue;

        switch (tinfo->colnum) {
        case 1:  { u_long v = row->entry_index;
                   snmp_set_var_typed_value(req->requestvb, ASN_GAUGE,
                       (u_char*)&v, sizeof(v)); break; }
        case 2:  set_counter64(req, row->error_count); break;
        case 3:  { u_long v = row->sqid;
                   snmp_set_var_typed_value(req->requestvb, ASN_GAUGE,
                       (u_char*)&v, sizeof(v)); break; }
        case 4:  { u_long v = row->command_id;
                   snmp_set_var_typed_value(req->requestvb, ASN_GAUGE,
                       (u_char*)&v, sizeof(v)); break; }
        case 5:  { u_long v = row->status_field;
                   snmp_set_var_typed_value(req->requestvb, ASN_GAUGE,
                       (u_char*)&v, sizeof(v)); break; }
        case 6:  { u_long v = row->parm_error_location;
                   snmp_set_var_typed_value(req->requestvb, ASN_GAUGE,
                       (u_char*)&v, sizeof(v)); break; }
        case 7:  set_counter64(req, row->lba); break;
        case 8:  { u_long v = row->nsid;
                   snmp_set_var_typed_value(req->requestvb, ASN_GAUGE,
                       (u_char*)&v, sizeof(v)); break; }
        case 9:  { u_long v = 0;
                   snmp_set_var_typed_value(req->requestvb, ASN_GAUGE,
                       (u_char*)&v, sizeof(v)); break; }
        case 10: { u_long v = row->status_code;
                   snmp_set_var_typed_value(req->requestvb, ASN_GAUGE,
                       (u_char*)&v, sizeof(v)); break; }
        case 11: { u_long v = row->status_code_type;
                   snmp_set_var_typed_value(req->requestvb, ASN_GAUGE,
                       (u_char*)&v, sizeof(v)); break; }
        case 12: { long v = row->do_not_retry ? 1 : 2;
                   snmp_set_var_typed_value(req->requestvb, ASN_INTEGER,
                       (u_char*)&v, sizeof(v)); break; }
        case 13: snmp_set_var_typed_value(req->requestvb, ASN_OCTET_STR,
                     (u_char*)row->status_string.c_str(),
                     row->status_string.size()); break;
        case 14: { long v = row->phase_tag ? 1 : 2;
                   snmp_set_var_typed_value(req->requestvb, ASN_INTEGER,
                       (u_char*)&v, sizeof(v)); break; }
        default: netsnmp_set_request_error(reqinfo, req, SNMP_NOSUCHOBJECT);
        }
    }
    return SNMP_ERR_NOERROR;
}

// ---------------------------------------------------------------------------
// NVMe capability table
// INDEX { smartmonDeviceIndex, smartmonNvmeCapabilityIndex(col 10) }
// col 1  = firmwareUpdateRaw
// col 2  = firmwareSlotCount
// col 3  = firmwareResetRequired (TruthValue)
// col 4  = optionalAdminCommandRaw
// col 5  = optionalNvmCommandRaw
// col 6  = logPageAttributesRaw
// col 7  = optionalAdminCommandText
// col 8  = optionalNvmCommandText
// col 9  = logPageAttributesText
// col 10 = capabilityIndex (NOT-ACCESSIBLE, always 1)
// ---------------------------------------------------------------------------

static netsnmp_variable_list *
nvme_cap_get_next(void **loop_ctx, void **data_ctx,
                  netsnmp_variable_list *put_idx,
                  netsnmp_iterator_info *) {
    size_t idx = (size_t)(uintptr_t)*loop_ctx;
    if (idx >= g_cache.nvme_capabilities.size()) return nullptr;
    CacheNvmeCapabilityRow &row = g_cache.nvme_capabilities[idx];
    *loop_ctx = (void*)(uintptr_t)(idx + 1);
    *data_ctx = &row;
    u_long v = (u_long)row.device_index;
    snmp_set_var_typed_value(put_idx, ASN_UNSIGNED, (u_char*)&v, sizeof(v));
    v = 1;  // capabilityIndex = 1 per device
    snmp_set_var_typed_value(put_idx->next_variable, ASN_UNSIGNED,
                             (u_char*)&v, sizeof(v));
    return put_idx;
}

static int
nvme_cap_handler(netsnmp_mib_handler *,
                 netsnmp_handler_registration *,
                 netsnmp_agent_request_info *reqinfo,
                 netsnmp_request_info *requests) {
    if (reqinfo->mode != MODE_GET) return SNMP_ERR_NOERROR;

    for (netsnmp_request_info *req = requests; req; req = req->next) {
        netsnmp_table_request_info *tinfo = netsnmp_extract_table_info(req);
        CacheNvmeCapabilityRow *row =
            (CacheNvmeCapabilityRow*)netsnmp_extract_iterator_context(req);
        if (!row || !tinfo) continue;

        switch (tinfo->colnum) {
        case 1:  { u_long v = row->firmware_update_raw;
                   snmp_set_var_typed_value(req->requestvb, ASN_GAUGE,
                       (u_char*)&v, sizeof(v)); break; }
        case 2:  { u_long v = row->firmware_slot_count;
                   snmp_set_var_typed_value(req->requestvb, ASN_GAUGE,
                       (u_char*)&v, sizeof(v)); break; }
        case 3:  { long v = row->firmware_reset_required ? 1 : 2;
                   snmp_set_var_typed_value(req->requestvb, ASN_INTEGER,
                       (u_char*)&v, sizeof(v)); break; }
        case 4:  { u_long v = row->optional_admin_cmd_raw;
                   snmp_set_var_typed_value(req->requestvb, ASN_GAUGE,
                       (u_char*)&v, sizeof(v)); break; }
        case 5:  { u_long v = row->optional_nvm_cmd_raw;
                   snmp_set_var_typed_value(req->requestvb, ASN_GAUGE,
                       (u_char*)&v, sizeof(v)); break; }
        case 6:  { u_long v = row->log_page_attr_raw;
                   snmp_set_var_typed_value(req->requestvb, ASN_GAUGE,
                       (u_char*)&v, sizeof(v)); break; }
        case 7:  snmp_set_var_typed_value(req->requestvb, ASN_OCTET_STR,
                     (u_char*)row->optional_admin_cmd_text.c_str(),
                     row->optional_admin_cmd_text.size()); break;
        case 8:  snmp_set_var_typed_value(req->requestvb, ASN_OCTET_STR,
                     (u_char*)row->optional_nvm_cmd_text.c_str(),
                     row->optional_nvm_cmd_text.size()); break;
        case 9:  snmp_set_var_typed_value(req->requestvb, ASN_OCTET_STR,
                     (u_char*)row->log_page_attr_text.c_str(),
                     row->log_page_attr_text.size()); break;
        default: netsnmp_set_request_error(reqinfo, req, SNMP_NOSUCHOBJECT);
        }
    }
    return SNMP_ERR_NOERROR;
}

// ---------------------------------------------------------------------------
// NVMe power state table
// INDEX { smartmonDeviceIndex, smartmonNvmePowerStateIndex(col 1) }
// col 1  = powerStateIndex (NOT-ACCESSIBLE)
// col 2  = operational (TruthValue)
// col 3  = maxPowerMilliWatts
// col 4  = activePowerMilliWatts (NOSUCHINSTANCE if absent)
// col 5  = idlePowerMilliWatts   (NOSUCHINSTANCE if absent)
// col 6  = readLatencyRank
// col 7  = readThroughputRank
// col 8  = writeLatencyRank
// col 9  = writeThroughputRank
// col 10 = entryLatencyUsec
// col 11 = exitLatencyUsec
// ---------------------------------------------------------------------------

static netsnmp_variable_list *
nvme_ps_get_next(void **loop_ctx, void **data_ctx,
                 netsnmp_variable_list *put_idx,
                 netsnmp_iterator_info *) {
    size_t idx = (size_t)(uintptr_t)*loop_ctx;
    if (idx >= g_cache.nvme_power_states.size()) return nullptr;
    CacheNvmePowerStateRow &row = g_cache.nvme_power_states[idx];
    *loop_ctx = (void*)(uintptr_t)(idx + 1);
    *data_ctx = &row;
    u_long v = (u_long)row.device_index;
    snmp_set_var_typed_value(put_idx, ASN_UNSIGNED, (u_char*)&v, sizeof(v));
    v = (u_long)row.state_index;
    snmp_set_var_typed_value(put_idx->next_variable, ASN_UNSIGNED,
                             (u_char*)&v, sizeof(v));
    return put_idx;
}

static int
nvme_ps_handler(netsnmp_mib_handler *,
                netsnmp_handler_registration *,
                netsnmp_agent_request_info *reqinfo,
                netsnmp_request_info *requests) {
    if (reqinfo->mode != MODE_GET) return SNMP_ERR_NOERROR;

    for (netsnmp_request_info *req = requests; req; req = req->next) {
        netsnmp_table_request_info *tinfo = netsnmp_extract_table_info(req);
        CacheNvmePowerStateRow *row =
            (CacheNvmePowerStateRow*)netsnmp_extract_iterator_context(req);
        if (!row || !tinfo) continue;

        switch (tinfo->colnum) {
        case 2:  { long v = row->operational ? 1 : 2;
                   snmp_set_var_typed_value(req->requestvb, ASN_INTEGER,
                       (u_char*)&v, sizeof(v)); break; }
        case 3:  { u_long v = row->max_power_mw;
                   snmp_set_var_typed_value(req->requestvb, ASN_GAUGE,
                       (u_char*)&v, sizeof(v)); break; }
        case 4:  if (!row->has_active_power) {
                     netsnmp_set_request_error(reqinfo, req, SNMP_NOSUCHINSTANCE);
                 } else {
                     u_long v = row->active_power_mw;
                     snmp_set_var_typed_value(req->requestvb, ASN_GAUGE,
                         (u_char*)&v, sizeof(v));
                 } break;
        case 5:  if (!row->has_idle_power) {
                     netsnmp_set_request_error(reqinfo, req, SNMP_NOSUCHINSTANCE);
                 } else {
                     u_long v = row->idle_power_mw;
                     snmp_set_var_typed_value(req->requestvb, ASN_GAUGE,
                         (u_char*)&v, sizeof(v));
                 } break;
        case 6:  { u_long v = row->read_latency_rank;
                   snmp_set_var_typed_value(req->requestvb, ASN_GAUGE,
                       (u_char*)&v, sizeof(v)); break; }
        case 7:  { u_long v = row->read_throughput_rank;
                   snmp_set_var_typed_value(req->requestvb, ASN_GAUGE,
                       (u_char*)&v, sizeof(v)); break; }
        case 8:  { u_long v = row->write_latency_rank;
                   snmp_set_var_typed_value(req->requestvb, ASN_GAUGE,
                       (u_char*)&v, sizeof(v)); break; }
        case 9:  { u_long v = row->write_throughput_rank;
                   snmp_set_var_typed_value(req->requestvb, ASN_GAUGE,
                       (u_char*)&v, sizeof(v)); break; }
        case 10: { u_long v = row->entry_latency_usec;
                   snmp_set_var_typed_value(req->requestvb, ASN_GAUGE,
                       (u_char*)&v, sizeof(v)); break; }
        case 11: { u_long v = row->exit_latency_usec;
                   snmp_set_var_typed_value(req->requestvb, ASN_GAUGE,
                       (u_char*)&v, sizeof(v)); break; }
        default: netsnmp_set_request_error(reqinfo, req, SNMP_NOSUCHOBJECT);
        }
    }
    return SNMP_ERR_NOERROR;
}

// ---------------------------------------------------------------------------
// NVMe LBA format table
// INDEX { smartmonDeviceIndex, smartmonNvmeNamespaceId, smartmonNvmeLbaFormatId(col 1) }
// col 1  = lbaFormatId (NOT-ACCESSIBLE)
// col 2  = lbaFormatCurrent (TruthValue)
// col 3  = lbaFormatDataSizeBytes
// col 4  = lbaFormatMetadataSizeBytes
// col 5  = lbaFormatRelativePerformance
// ---------------------------------------------------------------------------

static netsnmp_variable_list *
nvme_lba_get_next(void **loop_ctx, void **data_ctx,
                  netsnmp_variable_list *put_idx,
                  netsnmp_iterator_info *) {
    size_t idx = (size_t)(uintptr_t)*loop_ctx;
    if (idx >= g_cache.nvme_lba_formats.size()) return nullptr;
    CacheNvmeLbaFormatRow &row = g_cache.nvme_lba_formats[idx];
    *loop_ctx = (void*)(uintptr_t)(idx + 1);
    *data_ctx = &row;
    u_long v = (u_long)row.device_index;
    snmp_set_var_typed_value(put_idx, ASN_UNSIGNED, (u_char*)&v, sizeof(v));
    v = (u_long)row.namespace_id;
    snmp_set_var_typed_value(put_idx->next_variable, ASN_UNSIGNED,
                             (u_char*)&v, sizeof(v));
    v = (u_long)row.format_id;
    snmp_set_var_typed_value(put_idx->next_variable->next_variable, ASN_UNSIGNED,
                             (u_char*)&v, sizeof(v));
    return put_idx;
}

static int
nvme_lba_handler(netsnmp_mib_handler *,
                 netsnmp_handler_registration *,
                 netsnmp_agent_request_info *reqinfo,
                 netsnmp_request_info *requests) {
    if (reqinfo->mode != MODE_GET) return SNMP_ERR_NOERROR;

    for (netsnmp_request_info *req = requests; req; req = req->next) {
        netsnmp_table_request_info *tinfo = netsnmp_extract_table_info(req);
        CacheNvmeLbaFormatRow *row =
            (CacheNvmeLbaFormatRow*)netsnmp_extract_iterator_context(req);
        if (!row || !tinfo) continue;

        switch (tinfo->colnum) {
        case 2:  { long v = row->current ? 1 : 2;
                   snmp_set_var_typed_value(req->requestvb, ASN_INTEGER,
                       (u_char*)&v, sizeof(v)); break; }
        case 3:  { u_long v = row->data_size;
                   snmp_set_var_typed_value(req->requestvb, ASN_GAUGE,
                       (u_char*)&v, sizeof(v)); break; }
        case 4:  { u_long v = row->metadata_size;
                   snmp_set_var_typed_value(req->requestvb, ASN_GAUGE,
                       (u_char*)&v, sizeof(v)); break; }
        case 5:  { u_long v = row->rel_perf;
                   snmp_set_var_typed_value(req->requestvb, ASN_GAUGE,
                       (u_char*)&v, sizeof(v)); break; }
        default: netsnmp_set_request_error(reqinfo, req, SNMP_NOSUCHOBJECT);
        }
    }
    return SNMP_ERR_NOERROR;
}

// ---------------------------------------------------------------------------
// Registration
// ---------------------------------------------------------------------------

void register_nvme_mib() {
    // Row-count and last-change scalars for each implemented table
    netsnmp_register_scalar(netsnmp_create_handler_registration(
        "nvmeControllerTableRowCount",   nvme_ctrl_row_count_handler,
        oid_nvme_controller_row_count,   OID_LEN(oid_nvme_controller_row_count),   HANDLER_CAN_RONLY));
    netsnmp_register_scalar(netsnmp_create_handler_registration(
        "nvmeControllerTableLastChange", nvme_ctrl_last_change_handler,
        oid_nvme_controller_last_change, OID_LEN(oid_nvme_controller_last_change), HANDLER_CAN_RONLY));
    netsnmp_register_scalar(netsnmp_create_handler_registration(
        "nvmeNamespaceTableRowCount",    nvme_ns_row_count_handler,
        oid_nvme_namespace_row_count,    OID_LEN(oid_nvme_namespace_row_count),    HANDLER_CAN_RONLY));
    netsnmp_register_scalar(netsnmp_create_handler_registration(
        "nvmeNamespaceTableLastChange",  nvme_ns_last_change_handler,
        oid_nvme_namespace_last_change,  OID_LEN(oid_nvme_namespace_last_change),  HANDLER_CAN_RONLY));
    netsnmp_register_scalar(netsnmp_create_handler_registration(
        "nvmeHealthTableRowCount",       nvme_health_row_count_handler,
        oid_nvme_health_row_count,       OID_LEN(oid_nvme_health_row_count),       HANDLER_CAN_RONLY));
    netsnmp_register_scalar(netsnmp_create_handler_registration(
        "nvmeHealthTableLastChange",     nvme_health_last_change_handler,
        oid_nvme_health_last_change,     OID_LEN(oid_nvme_health_last_change),     HANDLER_CAN_RONLY));
    netsnmp_register_scalar(netsnmp_create_handler_registration(
        "nvmeSelfTestTableRowCount",     nvme_st_row_count_handler,
        oid_nvme_selftest_row_count,     OID_LEN(oid_nvme_selftest_row_count),     HANDLER_CAN_RONLY));
    netsnmp_register_scalar(netsnmp_create_handler_registration(
        "nvmeSelfTestTableLastChange",   nvme_st_last_change_handler,
        oid_nvme_selftest_last_change,   OID_LEN(oid_nvme_selftest_last_change),   HANDLER_CAN_RONLY));
    netsnmp_register_scalar(netsnmp_create_handler_registration(
        "nvmeErrorLogTableRowCount",     nvme_el_row_count_handler,
        oid_nvme_error_log_row_count,    OID_LEN(oid_nvme_error_log_row_count),    HANDLER_CAN_RONLY));
    netsnmp_register_scalar(netsnmp_create_handler_registration(
        "nvmeErrorLogTableLastChange",   nvme_el_last_change_handler,
        oid_nvme_error_log_last_change,  OID_LEN(oid_nvme_error_log_last_change),  HANDLER_CAN_RONLY));

    // NVMe table iterator registrations
    REG_TABLE_UU("smartmonNvmeHealthTable",     nvme_health_handler, oid_nvme_health_table,       nvme_health_get_next,  1, 23);
    REG_TABLE_UU("smartmonNvmeSelfTestTable",   nvme_st_handler,     oid_nvme_selftest_table,     nvme_st_get_next,      2, 11);
    REG_TABLE_UU("smartmonNvmeControllerTable", nvme_ctrl_handler,   oid_nvme_controller_table,   nvme_ctrl_get_next,    1, 18);
    REG_TABLE_UU("smartmonNvmeNamespaceTable",  nvme_ns_handler,     oid_nvme_namespace_table,    nvme_ns_get_next,      1, 10);

    // NVMe capability table metadata scalars
    netsnmp_register_scalar(netsnmp_create_handler_registration(
        "nvmeCapabilityTableRowCount",    nvme_cap_row_count_handler,
        oid_nvme_capability_row_count,    OID_LEN(oid_nvme_capability_row_count),    HANDLER_CAN_RONLY));
    netsnmp_register_scalar(netsnmp_create_handler_registration(
        "nvmeCapabilityTableLastChange",  nvme_cap_last_change_handler,
        oid_nvme_capability_last_change,  OID_LEN(oid_nvme_capability_last_change),  HANDLER_CAN_RONLY));

    REG_TABLE_UU("smartmonNvmeCapabilityTable", nvme_cap_handler, oid_nvme_capability_table, nvme_cap_get_next, 1, 9);

    // NVMe power state table metadata scalars
    netsnmp_register_scalar(netsnmp_create_handler_registration(
        "nvmePowerStateTableRowCount",    nvme_ps_row_count_handler,
        oid_nvme_powerstate_row_count,    OID_LEN(oid_nvme_powerstate_row_count),    HANDLER_CAN_RONLY));
    netsnmp_register_scalar(netsnmp_create_handler_registration(
        "nvmePowerStateTableLastChange",  nvme_ps_last_change_handler,
        oid_nvme_powerstate_last_change,  OID_LEN(oid_nvme_powerstate_last_change),  HANDLER_CAN_RONLY));

    REG_TABLE_UU("smartmonNvmePowerStateTable", nvme_ps_handler, oid_nvme_powerstate_table, nvme_ps_get_next, 2, 11);

    // NVMe LBA format table metadata scalars
    netsnmp_register_scalar(netsnmp_create_handler_registration(
        "nvmeLbaFormatTableRowCount",     nvme_lba_row_count_handler,
        oid_nvme_lbafmt_row_count,        OID_LEN(oid_nvme_lbafmt_row_count),        HANDLER_CAN_RONLY));
    netsnmp_register_scalar(netsnmp_create_handler_registration(
        "nvmeLbaFormatTableLastChange",   nvme_lba_last_change_handler,
        oid_nvme_lbafmt_last_change,      OID_LEN(oid_nvme_lbafmt_last_change),      HANDLER_CAN_RONLY));

    // NVMe LBA format table (3 index columns: deviceIndex + namespaceId + formatId)
    REG_TABLE_UUU("smartmonNvmeLbaFormatTable", nvme_lba_handler, oid_nvme_lbafmt_table, nvme_lba_get_next, 2, 5);

    REG_TABLE_UU("smartmonNvmeErrorLogTable",   nvme_el_handler,  oid_nvme_error_log_table, nvme_el_get_next, 1, 14);
}
