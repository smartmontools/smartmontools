// agentxd_notify.h — SNMP v2 trap (NOTIFICATION) wrappers

#pragma once

#include <cstdint>

// Send smartmonDeviceHealthChanged notification.
// new_status: SmartmonHealthStatus value (1=passed, 2=failed, 0=unknown)
void notify_device_health_changed(uint32_t dev_idx, int new_status);

// Send smartmonDevicePollingFailed notification.
// poll_result: SmartmonPollResult value
void notify_device_polling_failed(uint32_t dev_idx, int poll_result);

// Send *SelfTestFailed notification appropriate for the device protocol.
// type_str: human-readable self-test type, e.g. "Short" or "Extended"
void notify_selftest_failed(uint32_t dev_idx, const char *type_str,
                            int result_code);
