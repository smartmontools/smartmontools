# smartmon-snmp-agentxd

An SNMP AgentX subagent (RFC 2741) that exposes SMART drive health data
collected by `smartd` via the SMARTMON-* MIBs.

Supports **NVMe**, **SATA/ATA**, and **SAS/SCSI** drives.

---

## Overview

`smartmon-snmp-agentxd` connects to a running `snmpd` master agent over a
Unix domain socket, registers the SMARTMON-* OID subtrees, and responds to
SNMP GET/GETNEXT/GETBULK requests.  It also sends SNMP v2 traps when drive
health changes or self-tests fail.

Data is read exclusively from JSON state files written by `smartd --jsonstate`.
The agent never invokes `smartctl` directly.

```text
smartd --jsonstate /run/smartmontools/json/
      └── writes *.json  ──>  smartmon-snmp-agentxd  ──>  snmpd  ──>  SNMP manager
```

---

## MIB structure

Enterprise OID: `1.3.6.1.4.1.99999`

| Sub-tree | MIB | Contents |
|----------|-----|----------|
| `.1` | SMARTMON-TC-MIB | Textual conventions |
| `.2` | SMARTMON-COMMON-MIB | Device inventory table, device count scalar |
| `.3` | SMARTMON-NVME-MIB | NVMe health, self-test, controller, namespace, error log |
| `.4` | SMARTMON-SATA-MIB | SATA attributes, self-test, info, health, error log |
| `.5` | SMARTMON-SAS-MIB | SAS health, error counters, self-test, background scan |

MIB files are installed to `/usr/share/snmp/mibs/`.

### Implemented tables

| Table | OID | Status |
|-------|-----|--------|
| `smartmonDeviceTable` | `.2.1.1` | Implemented |
| `smartmonNvmeHealthTable` | `.3.1.5` | Implemented |
| `smartmonNvmeSelfTestTable` | `.3.1.7` | Implemented |
| `smartmonSataAttrTable` | `.4.1.3` | Implemented |
| `smartmonSataSelfTestTable` | `.4.1.5` | Implemented |
| `smartmonSasHealthTable` | `.5.1.2` | Implemented |
| `smartmonSasErrorCounterTable` | `.5.1.3` | Implemented |
| `smartmonSasSelfTestTable` | `.5.1.4` | Implemented |
| `smartmonNvmeControllerTable` | `.3.1.1` | Planned |
| `smartmonNvmeNamespaceTable` | `.3.1.3` | Planned |
| `smartmonSataInfoTable` | `.4.1.1` | Planned |
| `smartmonSataHealthTable` | `.4.1.2` | Planned |
| `smartmonSasInfoTable` | `.5.1.1` | Planned |
| `smartmonSasBackgroundScanTable` | `.5.1.5` | Planned |

---

## Prerequisites

- **`snmpd`** (net-snmp ≥ 5.8) configured as AgentX master
- **`smartd`** configured with `--jsonstate` to write JSON state files
- Read access to the `state_dir` configured for smartd

---

## Installation

### From a source build

```bash
./configure --with-snmp-agentx
make -j$(nproc)
sudo scripts/install-agentxd.sh
```

### Manual install

```bash
sudo install -m 755 build/src/snmp-agentxd/smartmon-snmp-agentxd /usr/sbin/
sudo install -m 644 src/snmp-agentxd/smartmon-snmp-agentxd.conf \
    /etc/smartmontools/snmp-agentxd.conf
sudo install -m 644 doc/SMARTMON-*.mib /usr/share/snmp/mibs/
sudo sed -e 's|@sbindir@|/usr/sbin|' \
         -e 's|@sysconfdir@|/etc|' \
    src/snmp-agentxd/smartmon-snmp-agentxd.service.in \
    > /lib/systemd/system/smartmon-snmp-agentxd.service
sudo systemctl daemon-reload
```

---

## Configuration

### smartd

Configure `smartd` to write JSON state files.  The `-x` flag (extended) is
required for NVMe self-test logs and SAS error counters:

```conf
# /etc/smartd.conf
DEVICESCAN -x --jsonstate /run/smartmontools/json/
```

Restart smartd:
```bash
systemctl restart smartd
ls /run/smartmontools/json/   # JSON files should appear here
```

### snmpd

Add AgentX master support to `/etc/snmp/snmpd.conf`:

```conf
master agentx
agentXSocket /var/agentx/master
rocommunity public 127.0.0.1   # adjust as needed
```

Restart snmpd:
```bash
systemctl restart snmpd
```

### smartmon-snmp-agentxd

Edit `/etc/smartmontools/snmp-agentxd.conf`:

```conf
# Directory where smartd writes --jsonstate files (required)
state_dir       /run/smartmontools/json/

# AgentX master socket — must match agentXSocket in snmpd.conf
agentx_socket   /var/agentx/master

# Cache timeout in seconds (default: 300)
cache_timeout   300
```

---

## Starting the service

```bash
systemctl enable --now smartmon-snmp-agentxd
systemctl status smartmon-snmp-agentxd
```

---

## Verifying

```bash
# List all monitored devices
snmpwalk -v2c -c public localhost 1.3.6.1.4.1.99999.2

# NVMe health
snmpwalk -v2c -c public localhost 1.3.6.1.4.1.99999.3

# SATA attributes
snmpwalk -v2c -c public localhost 1.3.6.1.4.1.99999.4

# SAS health and error counters
snmpwalk -v2c -c public localhost 1.3.6.1.4.1.99999.5

# Human-readable output (requires MIBs in /usr/share/snmp/mibs/):
snmpwalk -v2c -c public -m ALL localhost \
    SMARTMON-COMMON-MIB::smartmonDeviceTable
```

---

## Command-line options

| Option | Description |
|--------|-------------|
| `-c FILE` | Path to config file (default: `/etc/smartmontools/snmp-agentxd.conf`) |
| `-f` | Run in foreground (do not daemonise; useful for debugging) |
| `-h` | Print usage and exit |

---

## Building and testing

### Build

```bash
./autogen.sh --force
mkdir build && cd build
../configure --with-snmp-agentx --disable-static CXXFLAGS="-O2 -Wall"
make -j$(nproc)
```

### Unit tests

```bash
cd src/snmp-agentxd/tests
make test
```

### Integration test (live SNMP)

Requires `snmpd` and the built binary:

```bash
# Auto-detects binary in build/
ci/run_integration_test.sh

# Or specify explicitly:
AGENTXD_BIN=build/src/snmp-agentxd/smartmon-snmp-agentxd \
    ci/run_integration_test.sh
```

The integration test:
1. Starts `snmpd` on `127.0.0.1:10161` with a temp AgentX socket (no root needed)
2. Starts `smartmon-snmp-agentxd` against fixture JSON files
3. Runs `snmpwalk` over all MIB subtrees
4. Validates 113 specific OID values across all device types and table columns

### Docker (full build + integration test)

```bash
ci/run_docker.sh
```

This builds using `ghcr.io/smartmontools/docker-build:master` as the base and
runs the full integration test suite inside a container.

---

## Notifications (SNMP traps)

The agent sends v2 traps to the snmpd master for:

| Trap | OID | Trigger |
|------|-----|---------|
| `smartmonDeviceHealthChanged` | `.2.3.1` | Drive overall health status changes |
| `smartmonDevicePollingFailed` | `.2.3.2` | smartd JSON file cannot be read |
| `smartmonDeviceAdded` | `.2.3.3` | New device discovered |
| `smartmonNvmeSelfTestFailed` | `.3.2.1` | NVMe self-test result is non-zero |
| `smartmonNvmeHealthChanged` | `.3.2.2` | NVMe health status changes |
| `smartmonSataSelfTestFailed` | `.4.2.1` | SATA self-test result is failed |
| `smartmonSataHealthDegraded` | `.4.2.2` | SATA overall health degrades |
| `smartmonSataAttrThresholdMet` | `.4.2.3` | SATA attribute crosses threshold |
| `smartmonSasSelfTestFailed` | `.5.2.1` | SAS self-test result is failed |
| `smartmonSasHealthChanged` | `.5.2.2` | SAS health status changes |

---

## Troubleshooting

**Agent exits immediately:**
Check syslog for config errors:
```bash
journalctl -u smartmon-snmp-agentxd -n 50
```
Common causes: `state_dir` not set, no JSON files in `state_dir`, or
`agentx_socket` path does not exist (snmpd not running).

**snmpwalk returns "No Such Object":**
The agent may not have registered yet.  Check:
```bash
snmpget -v2c -c public localhost 1.3.6.1.4.1.99999.2.1.2.0
```
Should return `Gauge32: N` (number of devices).

**Empty NVMe self-test or SAS error counter tables:**
Ensure smartd is using `-x` (extended monitoring), not just `-a`.

**snmpwalk shows numeric OIDs instead of names:**
Install MIBs to `/usr/share/snmp/mibs/` and use `-m ALL`:
```bash
export MIBS=ALL
snmpwalk -v2c -c public localhost 1.3.6.1.4.1.99999
```
