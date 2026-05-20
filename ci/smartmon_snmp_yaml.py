#!/usr/bin/env python3
"""
Convert SMARTMON-* SNMP walk output into readable YAML grouped by device.

Input example:
  snmpwalk -v2c -c public HOST SMARTMON-COMMON-MIB SMARTMON-NVME-MIB SMARTMON-SATA-MIB SMARTMON-SENSOR-MIB > pve2.snmp

Usage:
  ./smartmon_snmp_yaml.py pve2.snmp
  ./smartmon_snmp_yaml.py pve2.snmp -o pve2.yaml
  ./smartmon_snmp_yaml.py pve2.snmp --all-attrs
  ./smartmon_snmp_yaml.py pve2.snmp --device 5
  ./smartmon_snmp_yaml.py pve2.snmp --raw-values

Design:
  - Groups data by smartmonDeviceIndex.
  - Keeps repeated SNMP tables as YAML lists of rows.
  - Avoids one gigantic flat OID dump. Flat dumps are where readability goes to die.
"""
from __future__ import annotations

import argparse
import re
import sys
from collections import defaultdict
from typing import Any, Dict, Iterable, List, Tuple

LINE_RE = re.compile(
    r"^(?P<mib>[A-Za-z0-9_-]+)::(?P<object>[A-Za-z0-9_-]+)"
    r"(?:\.(?P<index>[0-9.]+))?\s*=\s*(?P<type>[^:]+):\s*(?P<value>.*)$"
)

IMPORTANT_ATTR_IDS = {
    5, 9, 10, 12, 18, 22, 187, 188, 190, 192, 193, 194,
    196, 197, 198, 199, 200, 241, 242,
}

# Order the output so humans do not need a machete.
DEVICE_ORDER = [
    "index", "path", "type", "physical_index", "name", "poll",
    "identity", "capacity", "health", "temperature", "lifetime",
    "nvme", "sata", "sas", "sensors", "errors", "self_tests",
]


def parse_index(s: str | None) -> Tuple[int, ...]:
    if not s:
        return ()
    return tuple(int(x) for x in s.split("."))


def clean_value(v: str) -> str:
    v = v.strip()
    if len(v) >= 2 and v[0] == '"' and v[-1] == '"':
        v = v[1:-1]
    return re.sub(r"\s+", " ", v)


def enum_name(v: str) -> str:
    m = re.match(r"([A-Za-z_][A-Za-z0-9_-]*)\(-?\d+\)$", v.strip())
    return m.group(1) if m else v.strip()


def value_number(v: str) -> int | float | str:
    """Return first numeric value when obvious, otherwise original string."""
    s = clean_value(v)
    if re.fullmatch(r"-?\d+", s):
        return int(s)
    if re.fullmatch(r"-?\d+\.\d+", s):
        return float(s)
    m = re.match(r"^(-?\d+)(?:\s+[A-Za-z%].*)?$", s)
    if m:
        return int(m.group(1))
    m = re.match(r"^(-?\d+\.\d+)(?:\s+[A-Za-z%].*)?$", s)
    if m:
        return float(m.group(1))
    return s


def parse_file(path: str) -> Dict[str, Dict[Tuple[int, ...], str]]:
    data: Dict[str, Dict[Tuple[int, ...], str]] = defaultdict(dict)
    with open(path, "r", encoding="utf-8", errors="replace") as f:
        for line in f:
            line = line.rstrip("\n")
            if not line or line.startswith("#"):
                continue
            m = LINE_RE.match(line)
            if not m:
                continue
            obj = m.group("object")
            idx = parse_index(m.group("index"))
            data[obj][idx] = clean_value(m.group("value"))
    return data


def get(data: Dict[str, Dict[Tuple[int, ...], str]], obj: str, *idx: int, default: Any = None) -> Any:
    return data.get(obj, {}).get(tuple(idx), default)


def get_enum(data: Dict[str, Dict[Tuple[int, ...], str]], obj: str, *idx: int, default: Any = None) -> Any:
    v = get(data, obj, *idx, default=default)
    return enum_name(v) if isinstance(v, str) else v


def get_num(data: Dict[str, Dict[Tuple[int, ...], str]], obj: str, *idx: int, default: Any = None) -> Any:
    v = get(data, obj, *idx, default=default)
    return value_number(v) if isinstance(v, str) else v


def compact(d: Dict[str, Any]) -> Dict[str, Any]:
    return {k: v for k, v in d.items() if v not in (None, "", [], {})}


def ordered(d: Dict[str, Any], keys: Iterable[str]) -> Dict[str, Any]:
    out = {}
    for k in keys:
        if k in d:
            out[k] = d[k]
    for k, v in d.items():
        if k not in out:
            out[k] = v
    return out


def device_ids(data: Dict[str, Dict[Tuple[int, ...], str]]) -> List[int]:
    ids = {idx[0] for obj in ("smartmonDevicePath", "smartmonDeviceType") for idx in data.get(obj, {}) if len(idx) == 1}
    return sorted(ids)


def sensor_rows(data: Dict[str, Dict[Tuple[int, ...], str]], dev: int) -> List[Dict[str, Any]]:
    rows = []
    sensor_indexes = sorted(idx[1] for idx in data.get("smartmonSensorName", {}) if len(idx) == 2 and idx[0] == dev)
    for sidx in sensor_indexes:
        unit = get(data, "smartmonSensorUnitsDisplay", dev, sidx)
        row = compact({
            "sensor": sidx,
            "name": get(data, "smartmonSensorName", dev, sidx),
            "value": get_num(data, "smartmonSensorValue", dev, sidx),
            "unit": unit,
            "type": get_enum(data, "smartmonSensorType", dev, sidx),
            "status": get_enum(data, "smartmonSensorOperStatus", dev, sidx),
            "source": get(data, "smartmonSensorSource", dev, sidx),
            "timestamp": get(data, "smartmonSensorValueTimestamp", dev, sidx),
        })
        rows.append(row)
    return rows


def sata_attr_rows(data: Dict[str, Dict[Tuple[int, ...], str]], dev: int, all_attrs: bool) -> List[Dict[str, Any]]:
    attrs = sorted(idx[1] for idx in data.get("smartmonSataAttrName", {}) if len(idx) == 2 and idx[0] == dev)
    rows = []
    for attr_id in attrs:
        if not all_attrs and attr_id not in IMPORTANT_ATTR_IDS:
            continue
        rows.append(compact({
            "id": attr_id,
            "name": get(data, "smartmonSataAttrName", dev, attr_id),
            "value": get_num(data, "smartmonSataAttrValue", dev, attr_id),
            "worst": get_num(data, "smartmonSataAttrWorst", dev, attr_id),
            "threshold": get_num(data, "smartmonSataAttrThreshold", dev, attr_id),
            "raw": get(data, "smartmonSataAttrRawString", dev, attr_id) or get_num(data, "smartmonSataAttrRawValue", dev, attr_id),
            "status": get_enum(data, "smartmonSataAttrStatus", dev, attr_id),
            "type": get_enum(data, "smartmonSataAttrType", dev, attr_id),
            "updated": get_enum(data, "smartmonSataAttrUpdated", dev, attr_id),
            "when_failed": get(data, "smartmonSataAttrWhenFailed", dev, attr_id),
        }))
    return rows


def sata_selftest_rows(data: Dict[str, Dict[Tuple[int, ...], str]], dev: int, limit: int) -> List[Dict[str, Any]]:
    nums = sorted(idx[1] for idx in data.get("smartmonSataSelfTestType", {}) if len(idx) == 2 and idx[0] == dev)
    rows = []
    for n in nums[:limit]:
        rows.append(compact({
            "number": n,
            "type": get_enum(data, "smartmonSataSelfTestType", dev, n),
            "result": get_enum(data, "smartmonSataSelfTestResult", dev, n),
            "result_text": get(data, "smartmonSataSelfTestResultString", dev, n),
            "passed": get_enum(data, "smartmonSataSelfTestResultPassed", dev, n),
            "remaining_percent": get_num(data, "smartmonSataSelfTestRemainingPct", dev, n),
            "lifetime_hours": get_num(data, "smartmonSataSelfTestLifetimeHours", dev, n),
            "lba_first_error": get_num(data, "smartmonSataSelfTestLbaFirstError", dev, n),
        }))
    return rows


def sata_error_rows(data: Dict[str, Dict[Tuple[int, ...], str]], dev: int) -> List[Dict[str, Any]]:
    nums = sorted(idx[1] for idx in data.get("smartmonSataErrorDescription", {}) if len(idx) == 2 and idx[0] == dev)
    rows = []
    for n in nums:
        rows.append(compact({
            "number": get_num(data, "smartmonSataErrorNumber", dev, n) or n,
            "description": get(data, "smartmonSataErrorDescription", dev, n),
            "lifetime_hours": get_num(data, "smartmonSataErrorLifetimeHours", dev, n),
            "state": get(data, "smartmonSataErrorStateString", dev, n),
            "lba": get_num(data, "smartmonSataErrorLba", dev, n),
            "command": get(data, "smartmonSataErrorRegCommand", dev, n),
            "status": get(data, "smartmonSataErrorCompRegStatus", dev, n),
            "error": get(data, "smartmonSataErrorCompRegError", dev, n),
        }))
    return rows


def nvme_selftest_rows(data: Dict[str, Dict[Tuple[int, ...], str]], dev: int, limit: int) -> List[Dict[str, Any]]:
    nums = sorted(idx[1] for idx in data.get("smartmonNvmeSelfTestType", {}) if len(idx) == 2 and idx[0] == dev)
    rows = []
    for n in nums[:limit]:
        rows.append(compact({
            "number": get_num(data, "smartmonNvmeSelfTestNumber", dev, n) or n,
            "type": get_enum(data, "smartmonNvmeSelfTestType", dev, n),
            "result": get_enum(data, "smartmonNvmeSelfTestResult", dev, n),
            "result_text": get(data, "smartmonNvmeSelfTestResultText", dev, n),
            "power_on_hours": get_num(data, "smartmonNvmeSelfTestPowerOnHours", dev, n),
            "failing_lba": get_num(data, "smartmonNvmeSelfTestFailingLba", dev, n),
            "namespace_id": get_num(data, "smartmonNvmeSelfTestNamespaceId", dev, n),
            "segment": get_num(data, "smartmonNvmeSelfTestSegmentNumber", dev, n),
        }))
    return rows


def nvme_error_rows(data: Dict[str, Dict[Tuple[int, ...], str]], dev: int) -> List[Dict[str, Any]]:
    nums = sorted(idx[1] for idx in data.get("smartmonNvmeErrorStatusString", {}) if len(idx) == 2 and idx[0] == dev)
    rows = []
    for n in nums:
        rows.append(compact({
            "index": n,
            "count": get_num(data, "smartmonNvmeErrorCount", dev, n),
            "status": get(data, "smartmonNvmeErrorStatusString", dev, n),
            "lba": get_num(data, "smartmonNvmeErrorLba", dev, n),
            "namespace_id": get_num(data, "smartmonNvmeErrorNamespaceId", dev, n),
            "command_id": get_num(data, "smartmonNvmeErrorCommandId", dev, n),
            "submission_queue_id": get_num(data, "smartmonNvmeErrorSubmissionQueueId", dev, n),
            "do_not_retry": get_enum(data, "smartmonNvmeErrorDoNotRetry", dev, n),
        }))
    return rows


def build_device(data: Dict[str, Dict[Tuple[int, ...], str]], dev: int, all_attrs: bool, selftest_limit: int) -> Dict[str, Any]:
    dtype = get_enum(data, "smartmonDeviceType", dev)
    base = {
        "index": dev,
        "path": get(data, "smartmonDevicePath", dev),
        "type": dtype,
        "physical_index": get_num(data, "smartmonDevicePhysicalIndex", dev),
        "name": get(data, "smartmonDeviceName", dev),
        "poll": compact({
            "time": get(data, "smartmonDeviceLastPollTime", dev),
            "result": get_enum(data, "smartmonDeviceLastPollResult", dev),
            "exit_status": get_num(data, "smartmonDeviceLastPollExitStatus", dev),
        }),
    }

    sensors = sensor_rows(data, dev)
    if sensors:
        base["sensors"] = sensors
        # expose a compact temperature list near the top as well
        temps = [r for r in sensors if r.get("type") == "celsius"]
        if temps:
            base["temperature"] = [{"name": r.get("name"), "value": r.get("value"), "unit": r.get("unit")} for r in temps]

    if dtype == "nvme":
        base["identity"] = compact({
            "model": get(data, "smartmonNvmeModelNumber", dev, 1),
            "serial": get(data, "smartmonNvmeSerialNumber", dev, 1),
            "firmware": get(data, "smartmonNvmeFirmwareVersion", dev, 1),
            "version": get(data, "smartmonNvmeVersion", dev, 1),
            "pci_vendor_id": get(data, "smartmonNvmePciVendorIdText", dev, 1) or get(data, "smartmonNvmePciVendorId", dev, 1),
            "pci_subsystem_vendor_id": get(data, "smartmonNvmePciVendorSubsystemIdText", dev, 1) or get(data, "smartmonNvmePciVendorSubsystemId", dev, 1),
        })
        base["capacity"] = compact({
            "total_nvm_bytes": get_num(data, "smartmonNvmeTotalNvmCapacityBytes", dev, 1),
            "unallocated_nvm_bytes": get_num(data, "smartmonNvmeUnallocatedNvmCapacityBytes", dev, 1),
            "namespace_count": get_num(data, "smartmonNvmeNamespaceCount", dev, 1),
            "namespaces": [compact({
                "id": get_num(data, "smartmonNvmeNamespaceId", dev, ns),
                "size_bytes": get_num(data, "smartmonNvmeNamespaceSizeBytes", dev, ns),
                "capacity_bytes": get_num(data, "smartmonNvmeNamespaceCapacityBytes", dev, ns),
                "utilization_bytes": get_num(data, "smartmonNvmeNamespaceUtilizationBytes", dev, ns),
                "formatted_lba_size_bytes": get_num(data, "smartmonNvmeNamespaceFormattedLbaSizeBytes", dev, ns),
            }) for ns in sorted(idx[1] for idx in data.get("smartmonNvmeNamespaceId", {}) if len(idx) == 2 and idx[0] == dev)],
        })
        base["health"] = compact({
            "overall_status": get_enum(data, "smartmonNvmeHealthOverallStatus", dev, 1),
            "critical_warning": get_num(data, "smartmonNvmeCriticalWarning", dev, 1),
            "available_spare_percent": get_num(data, "smartmonNvmeAvailableSparePercent", dev, 1),
            "available_spare_threshold_percent": get_num(data, "smartmonNvmeAvailableSpareThresholdPercent", dev, 1),
            "percentage_used": get_num(data, "smartmonNvmePercentageUsed", dev, 1),
            "media_data_integrity_errors": get_num(data, "smartmonNvmeMediaDataIntegrityErrors", dev, 1),
            "error_information_log_entries": get_num(data, "smartmonNvmeErrorInformationLogEntries", dev, 1),
            "warning_temperature_time_minutes": get_num(data, "smartmonNvmeWarningTemperatureTimeMinutes", dev, 1),
            "critical_temperature_time_minutes": get_num(data, "smartmonNvmeCriticalTemperatureTimeMinutes", dev, 1),
        })
        base["lifetime"] = compact({
            "power_on_hours": get_num(data, "smartmonNvmePowerOnHours", dev, 1),
            "power_cycles": get_num(data, "smartmonNvmePowerCycles", dev, 1),
            "unsafe_shutdowns": get_num(data, "smartmonNvmeUnsafeShutdowns", dev, 1),
            "controller_busy_time_minutes": get_num(data, "smartmonNvmeControllerBusyTimeMinutes", dev, 1),
            "host_read_commands": get_num(data, "smartmonNvmeHostReadCommands", dev, 1),
            "host_write_commands": get_num(data, "smartmonNvmeHostWriteCommands", dev, 1),
            "data_units_read": get_num(data, "smartmonNvmeDataUnitsRead", dev, 1),
            "data_units_written": get_num(data, "smartmonNvmeDataUnitsWritten", dev, 1),
            "data_bytes_read": get_num(data, "smartmonNvmeDataBytesRead", dev, 1),
            "data_bytes_written": get_num(data, "smartmonNvmeDataBytesWritten", dev, 1),
        })
        errors = nvme_error_rows(data, dev)
        if errors:
            base["errors"] = errors
        tests = nvme_selftest_rows(data, dev, selftest_limit)
        if tests:
            base["self_tests"] = tests

    elif dtype == "ata":
        base["identity"] = compact({
            "model": get(data, "smartmonSataModelName", dev),
            "model_family": get(data, "smartmonSataModelFamily", dev),
            "serial": get(data, "smartmonSataSerialNumber", dev),
            "firmware": get(data, "smartmonSataFirmwareVersion", dev),
            "wwn": get(data, "smartmonSataWwn", dev),
            "ata_version": get(data, "smartmonSataAtaVersionString", dev),
            "sata_version": get(data, "smartmonSataVersionString", dev),
            "form_factor": get(data, "smartmonSataFormFactor", dev),
            "rotation_rate": get(data, "smartmonSataRotationRate", dev),
            "in_smartctl_database": get_enum(data, "smartmonSataInSmartctlDatabase", dev),
        })
        base["capacity"] = compact({
            "user_capacity_bytes": get_num(data, "smartmonSataUserCapacityBytes", dev),
            "logical_block_size": get_num(data, "smartmonSataLogicalBlockSize", dev),
            "physical_block_size": get_num(data, "smartmonSataPhysicalBlockSize", dev),
            "trim_supported": get_enum(data, "smartmonSataTrimSupported", dev),
        })
        base["health"] = compact({
            "overall_status": get_enum(data, "smartmonSataHealthOverallStatus", dev),
            "smart_available": get_enum(data, "smartmonSataSmartAvailable", dev),
            "smart_enabled": get_enum(data, "smartmonSataSmartEnabled", dev),
            "offline_collection_status": get(data, "smartmonSataOfflineCollectionStatusString", dev),
            "self_test_execution_status": get(data, "smartmonSataSelfTestExecutionStatusString", dev),
            "error_log_count": get_num(data, "smartmonSataErrorLogCount", dev),
        })
        base["lifetime"] = compact({
            "power_on_hours": get_num(data, "smartmonSataPowerOnHours", dev),
            "power_cycles": get_num(data, "smartmonSataPowerCycles", dev),
        })
        base["sata"] = compact({
            "capabilities": compact({
                "self_tests_supported": get_enum(data, "smartmonSataCapabilitySelfTestsSupported", dev),
                "conveyance_supported": get_enum(data, "smartmonSataCapabilityConveyanceSupported", dev),
                "selective_supported": get_enum(data, "smartmonSataCapabilitySelectiveSupported", dev),
                "error_logging_supported": get_enum(data, "smartmonSataCapabilityErrorLoggingSupported", dev),
                "gp_logging_supported": get_enum(data, "smartmonSataCapabilityGpLoggingSupported", dev),
                "auto_offline_enabled": get_enum(data, "smartmonSataCapabilityAutoOfflineEnabled", dev),
            }),
            "self_test_polling_minutes": compact({
                "short": get_num(data, "smartmonSataSelfTestPollingShortMinutes", dev),
                "extended": get_num(data, "smartmonSataSelfTestPollingExtendedMinutes", dev),
                "conveyance": get_num(data, "smartmonSataSelfTestPollingConveyanceMinutes", dev),
            }),
            "attributes": sata_attr_rows(data, dev, all_attrs),
        })
        errors = sata_error_rows(data, dev)
        if errors:
            base["errors"] = errors
        tests = sata_selftest_rows(data, dev, selftest_limit)
        if tests:
            base["self_tests"] = tests

    return ordered(compact(base), DEVICE_ORDER)


def build_document(data: Dict[str, Dict[Tuple[int, ...], str]], args: argparse.Namespace) -> Dict[str, Any]:
    ids = device_ids(data)
    if args.device:
        wanted = set(args.device)
        ids = [i for i in ids if i in wanted]

    return compact({
        "smartmon_snmp": compact({
            "source": args.input,
            "device_count": get_num(data, "smartmonDeviceTableRowCount", 0),
            "last_change": get(data, "smartmonDeviceTableLastChange", 0),
            "notes": "Repeated SNMP tables are represented as YAML lists under each device.",
        }),
        "devices": [build_device(data, dev, args.all_attrs, args.selftests) for dev in ids],
    })


def yaml_scalar(v: Any) -> str:
    if v is None:
        return "null"
    if isinstance(v, bool):
        return "true" if v else "false"
    if isinstance(v, (int, float)) and not isinstance(v, bool):
        return str(v)
    s = str(v)
    if s == "":
        return "''"
    # Safe enough unquoted scalars. Quote the rest to avoid YAML footguns.
    if re.fullmatch(r"[A-Za-z0-9_./:+@%=-]+", s) and s.lower() not in {"null", "true", "false", "yes", "no", "on", "off"}:
        return s
    return "'" + s.replace("'", "''") + "'"


def dump_yaml(obj: Any, indent: int = 0) -> str:
    sp = " " * indent
    if isinstance(obj, dict):
        lines = []
        for k, v in obj.items():
            if isinstance(v, (dict, list)):
                lines.append(f"{sp}{k}:")
                lines.append(dump_yaml(v, indent + 2))
            else:
                lines.append(f"{sp}{k}: {yaml_scalar(v)}")
        return "\n".join(lines)
    if isinstance(obj, list):
        lines = []
        if not obj:
            return sp + "[]"
        for item in obj:
            if isinstance(item, dict):
                if not item:
                    lines.append(f"{sp}- {{}}")
                    continue
                first = True
                for k, v in item.items():
                    prefix = "- " if first else "  "
                    if isinstance(v, (dict, list)):
                        lines.append(f"{sp}{prefix}{k}:")
                        lines.append(dump_yaml(v, indent + 4))
                    else:
                        lines.append(f"{sp}{prefix}{k}: {yaml_scalar(v)}")
                    first = False
            elif isinstance(item, list):
                lines.append(f"{sp}-")
                lines.append(dump_yaml(item, indent + 2))
            else:
                lines.append(f"{sp}- {yaml_scalar(item)}")
        return "\n".join(lines)
    return sp + yaml_scalar(obj)


def main() -> int:
    ap = argparse.ArgumentParser(description="Convert SMARTMON SNMP walk output to readable YAML grouped by device.")
    ap.add_argument("input", help="SNMP walk text file")
    ap.add_argument("-o", "--output", help="Write YAML to file instead of stdout")
    ap.add_argument("--all-attrs", action="store_true", help="Include all SATA SMART attributes, not only the important/noisy-filtered subset")
    ap.add_argument("--selftests", type=int, default=8, help="Maximum self-test rows per device, default: 8")
    ap.add_argument("--device", type=int, action="append", help="Only include this device index; may be repeated")
    args = ap.parse_args()

    data = parse_file(args.input)
    doc = build_document(data, args)
    text = dump_yaml(doc) + "\n"

    if args.output:
        with open(args.output, "w", encoding="utf-8") as f:
            f.write(text)
    else:
        sys.stdout.write(text)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
