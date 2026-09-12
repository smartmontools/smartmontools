#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Opt-in, read-only Darwin USB SMART/capture/release validation (not run by CI).

Run as root from a logged-in Terminal session. Requires an unmounted, single-LUN
USB test disk. Every invocation re-resolves its USB identity; diskN is not reused.
"""

import argparse
import hashlib
import json
import os
from pathlib import Path
import plistlib
import subprocess
import time


def walk(node):
    yield node
    for child in node.get("IORegistryEntryChildren", []):
        yield from walk(child)


def plist_command(args):
    return plistlib.loads(subprocess.check_output(args, timeout=10))


def snapshot(args):
    roots = plist_command(["/usr/sbin/ioreg", "-a", "-r", "-c", "IOUSBHostDevice", "-l"])
    candidates = {}
    for root in roots:
        for node in walk(root):
            if ("bDeviceClass" in node and node.get("idVendor") == args.vendor
                    and node.get("idProduct") == args.product
                    and node.get("USB Serial Number") == args.usb_serial):
                candidates[node["IORegistryEntryID"]] = node
    if len(candidates) != 1:
        raise RuntimeError("expected exactly one matching USB bridge")
    device = next(iter(candidates.values()))
    interfaces = [n for n in device.get("IORegistryEntryChildren", [])
                  if "bInterfaceNumber" in n]
    if (len(interfaces) != 1 or interfaces[0].get("bInterfaceClass") != 8
            or interfaces[0].get("bInterfaceSubClass") != 6
            or interfaces[0].get("bInterfaceProtocol") not in (0x50, 0x62)):
        raise RuntimeError("system storage interface is not ready or is ambiguous")
    media = [n for n in walk(device) if n.get("IOObjectClass") == "IOMedia"]
    whole = [n for n in media if n.get("Whole")]
    if len(whole) != 1 or not whole[0].get("BSD Name"):
        raise RuntimeError("whole disk has not reappeared")
    disk_info = None
    for entry in media:
        if not entry.get("BSD Name"):
            continue
        info = plist_command(["/usr/sbin/diskutil", "info", "-plist", "/dev/" + entry["BSD Name"]])
        if info.get("MountPoint"):
            raise RuntimeError("hardware test requires all volumes to be unmounted")
        if entry is whole[0]:
            disk_info = info
    if (not disk_info or disk_info.get("Internal") is not False
            or disk_info.get("WholeDisk") is not True
            or disk_info.get("BusProtocol") != "USB"
            or disk_info.get("IOKitSize") != args.capacity):
        raise RuntimeError("external whole disk identity/capacity mismatch")
    return device, disk_info, interfaces[0]["bInterfaceProtocol"]


def identity(device):
    return tuple(device.get(key) for key in
                 ("locationID", "idVendor", "idProduct", "bcdDevice", "USB Serial Number"))


def wait_released(args, before, deadline, require_new_id=True):
    interval = 0.02
    last_error = ""
    while time.monotonic() < deadline:
        try:
            after = snapshot(args)
            if identity(after[0]) != identity(before):
                raise RuntimeError("bridge identity changed during release")
            if not require_new_id or after[0]["IORegistryEntryID"] != before["IORegistryEntryID"]:
                return after
        except (RuntimeError, KeyError, subprocess.SubprocessError) as error:
            last_error = str(error)
        time.sleep(min(interval, max(0, deadline - time.monotonic())))
        interval = min(interval * 2, 0.5)
    raise RuntimeError("same disk did not return after capture: " + last_error)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--smartctl", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True, help="new directory for evidence")
    parser.add_argument("--vendor", type=lambda s: int(s, 0), required=True)
    parser.add_argument("--product", type=lambda s: int(s, 0), required=True)
    parser.add_argument("--usb-serial", required=True)
    parser.add_argument("--model", required=True, help="expected NVMe Identify model")
    parser.add_argument("--drive-serial", required=True)
    parser.add_argument("--capacity", type=int, required=True)
    parser.add_argument("--type", default="sntjmicron", choices=("sntjmicron", "sntasmedia", "sntrealtek"))
    parser.add_argument("--autodetect", action="store_true",
                        help="test plain /dev/diskN without -d for system-protocol runs")
    parser.add_argument("--rounds", type=int, default=3)
    args = parser.parse_args()
    if os.geteuid() != 0:
        parser.error("run as root from the logged-in user's session")
    if args.rounds < 1:
        parser.error("rounds must be positive")
    args.smartctl = args.smartctl.resolve(strict=True)
    args.output.mkdir(parents=True, exist_ok=False)
    summary = {"binary": str(args.smartctl),
               "sha256": hashlib.sha256(args.smartctl.read_bytes()).hexdigest(), "runs": []}
    baseline = None
    for number in range(1, args.rounds + 1):
        for mode in ("system", "uasp", "bot"):
            label = f"{number:02d}-{mode}"
            before, disk, system_protocol = snapshot(args)
            if baseline is None:
                baseline = identity(before)
            if identity(before) != baseline:
                raise RuntimeError("bridge identity changed between runs")
            (args.output / (label + "-before.plist")).write_bytes(plistlib.dumps([before, disk]))
            dev_type = args.type + ("" if mode == "system" else "+usb," + mode)
            command = [str(args.smartctl), "-a", "--json=o", "-r", "ioctl,1"]
            if mode == "system" and args.autodetect:
                command += ["/dev/" + disk["DeviceIdentifier"]]
            else:
                command += ["-d", dev_type, "usbraw:" + str(before["IORegistryEntryID"])]
            started = time.monotonic()
            print(label + ": " + " ".join(command), flush=True)
            result = None
            try:
                result = subprocess.run(command, capture_output=True, text=True, timeout=90)
                (args.output / (label + ".json")).write_text(result.stdout)
                (args.output / (label + ".stderr")).write_text(result.stderr)
            finally:
                released = time.monotonic()
                after, after_disk, _ = wait_released(args, before, released + 30,
                    require_new_id=result is not None and result.returncode == 0)
                (args.output / (label + "-after.plist")).write_bytes(plistlib.dumps([after, after_disk]))
            payload = json.loads(result.stdout)
            system_name = "UASP" if system_protocol == 0x62 else "BOT"
            selected = system_name if mode == "system" else mode.upper()
            diagnostic = f"USB transport: system={system_name}, selected={selected}, selection={'system' if mode == 'system' else 'explicit'}, fallback=disabled"
            error_log = payload.get("nvme_error_information_log", {})
            self_test_supported = payload.get("nvme_optional_admin_commands", {}).get("self_test", False)
            checks = {
                "exit_zero": result.returncode == 0 and payload["smartctl"]["exit_status"] == 0,
                "model": payload.get("model_name") == args.model,
                "drive_serial": payload.get("serial_number") == args.drive_serial,
                "capacity": payload.get("nvme_total_capacity") == args.capacity,
                "smart_read": "nvme_smart_health_information_log" in payload,
                "error_log_read": error_log.get("read", 0) > 0
                    and error_log.get("read") == min(16, error_log.get("size", 0)),
                "self_test_log_read": not self_test_supported or "nvme_self_test_log" in payload,
                "selected_protocol": diagnostic in payload["smartctl"].get("output", []),
            }
            run = {"case": label, "checks": checks, "command": command,
                   "registry_before": before["IORegistryEntryID"], "registry_after": after["IORegistryEntryID"],
                   "seconds": time.monotonic() - started,
                   "release_seconds": time.monotonic() - released,
                   "smart": payload.get("nvme_smart_health_information_log")}
            summary["runs"].append(run)
            (args.output / "summary.json").write_text(json.dumps(summary, indent=2) + "\n")
            print(f"{label}: checks={checks}, released as {after_disk['DeviceIdentifier']}", flush=True)
            if not all(checks.values()):
                raise RuntimeError("hardware acceptance failed; inspect saved evidence")
    print(f"PASS: {len(summary['runs'])} SMART reads and device releases", flush=True)


if __name__ == "__main__":
    main()
