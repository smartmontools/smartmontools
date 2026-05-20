#!/bin/bash
# ci/run_integration_test.sh — live SNMP server + client integration test
#
# Can be run directly (no Docker required) or inside the Docker container
# built from ci/Dockerfile.agentxd.
#
# Usage:
#   ci/run_integration_test.sh
#   AGENTXD_BIN=/path/to/binary ci/run_integration_test.sh
#   FIXTURES=/path/to/json/files ci/run_integration_test.sh
#
# Defaults (all overridable via environment variables):
#   AGENTXD_BIN  — searched in common build locations relative to repo root
#   FIXTURES     — src/snmp-agentxd/tests/fixtures (committed test data)
#   OUTPUT       — .tmp/test/
#
# Known fixture values verified (device index is discovered dynamically):
#   NVMe Samsung_SSD_980_PRO_1TB-S6B0NJ0N123456:
#     available_spare_pct = 100
#     percentage_used     = 2
#     power_on_hours      = 1827
#     power_cycles        = 14
#   SCSI SEAGATE_ST4000NM0023_FS-Z1Z0ABCD:
#     grown_defect_count  = 3
#   ATA WDC_WD140EFGX_68B0GN0-SELFTESTS:
#     attr[9] raw_value   = 4321  (Power_On_Hours)
#     selftest[3].lba     = 123456789  (failed entry)

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

# Locate the binary — check env var, then common build locations
if [ -z "${AGENTXD_BIN:-}" ]; then
    for candidate in \
        /build/src/snmp-agentxd/smartmon-snmp-agentxd \
        "$REPO_ROOT/build/src/snmp-agentxd/smartmon-snmp-agentxd" \
        "$REPO_ROOT/src/snmp-agentxd/smartmon-snmp-agentxd"
    do
        if [ -x "$candidate" ]; then
            AGENTXD_BIN="$candidate"
            break
        fi
    done
    AGENTXD_BIN="${AGENTXD_BIN:-}"
fi

FIXTURES="${FIXTURES:-$REPO_ROOT/src/snmp-agentxd/tests/fixtures}"
OUTPUT="${OUTPUT:-$REPO_ROOT/.tmp/test}"

# Use a per-run temp dir for the AgentX socket so no root is needed
AGENTX_RUN_DIR="/tmp/agentx-test-$$"
AGENTX_SOCKET="$AGENTX_RUN_DIR/master"

AGENTXD_CONF="$AGENTX_RUN_DIR/agentxd.conf"
SNMPD_CONF="$AGENTX_RUN_DIR/snmpd.conf"
AGENTXD_PID=""
SNMPD_PID=""

ENT="1.3.6.1.4.1.99999"
COMMUNITY="public"
HOST="127.0.0.1:10161"
WALK="snmpwalk -v2c -c $COMMUNITY -On $HOST"
GET="snmpget -v2c -c $COMMUNITY -On $HOST"

PASS=0
FAIL=0
SKIP=0

# ---------------------------------------------------------------------------
cleanup() {
    [ -n "$AGENTXD_PID" ] && kill "$AGENTXD_PID" 2>/dev/null || true
    [ -n "$SNMPD_PID"   ] && kill "$SNMPD_PID"   2>/dev/null || true
    wait 2>/dev/null || true
    rm -rf "$AGENTX_RUN_DIR"
}
trap cleanup EXIT

mkdir -p "$AGENTX_RUN_DIR"

# ---------------------------------------------------------------------------
# Test helpers
# ---------------------------------------------------------------------------

# check_oid_val LABEL FILE OID_REGEX_SUFFIX TYPE_AND_VALUE
#   OID_REGEX_SUFFIX — ERE suffix after the enterprise OID; use [0-9]+ for device index
#   TYPE_AND_VALUE   — e.g. "Gauge32: 100" or "Counter64: 1827"
check_oid_val() {
    local label="$1" file="$2" oid_sfx="$3" expected="$4"
    local pattern="${ENT//./\\.}${oid_sfx} = ${expected}$"
    if grep -qE "$pattern" "$file"; then
        echo "  PASS: $label"
        PASS=$((PASS + 1))
    else
        echo "  FAIL: $label" >&2
        echo "        expected pattern: $pattern" >&2
        echo "        in file: $file" >&2
        FAIL=$((FAIL + 1))
    fi
}

# check_oid_exact LABEL FILE OID_SUFFIX TYPE_AND_VALUE
#   OID_SUFFIX — literal suffix (no regex); used after exact index discovery
check_oid_exact() {
    local label="$1" file="$2" oid_sfx="$3" expected="$4"
    local ent_escaped="${ENT//./\\.}"
    local sfx_escaped="${oid_sfx//./\\.}"
    local pattern="${ent_escaped}${sfx_escaped} = ${expected}$"
    if grep -qE "$pattern" "$file"; then
        echo "  PASS: $label"
        PASS=$((PASS + 1))
    else
        echo "  FAIL: $label" >&2
        echo "        expected pattern: $pattern" >&2
        echo "        in file: $file" >&2
        FAIL=$((FAIL + 1))
    fi
}

# check_row_count LABEL FILE OID_PREFIX EXPECTED_MIN_ROWS
check_row_count() {
    local label="$1" file="$2" oid_pfx="$3" min_rows="$4"
    local count
    count=$(grep -cE "^\.?${ENT//./\\.}${oid_pfx}" "$file" 2>/dev/null || true)
    if [ "$count" -ge "$min_rows" ]; then
        echo "  PASS: $label ($count rows)"
        PASS=$((PASS + 1))
    else
        echo "  FAIL: $label (found $count rows, need >= $min_rows)" >&2
        FAIL=$((FAIL + 1))
    fi
}

# discover_idx FILE OID_SUFFIX_REGEX VALUE_LITERAL
#   Returns the device index (2nd-from-last OID component) from the line whose
#   OID matches OID_SUFFIX_REGEX AND whose value equals VALUE_LITERAL exactly.
#   Filtering by value prevents picking the wrong device when multiple devices
#   share the same column (e.g. two NVMe drives both have power_on_hours).
discover_idx() {
    local file="$1" sfx_re="$2" val_lit="$3"
    local ent_esc="${ENT//./\\.}"
    grep -E " = ${val_lit}$" "$file" | \
        grep -oE "${ent_esc}${sfx_re}" | head -1 | \
        awk -F'.' '{print $(NF-1)}' || true
}

# ---------------------------------------------------------------------------
# Validate inputs
# ---------------------------------------------------------------------------
echo "=== Integration test: smartmon-snmp-agentxd ==="
echo "  fixtures : $FIXTURES"
echo "  output   : $OUTPUT"
echo "  binary   : $AGENTXD_BIN"

if [ ! -d "$FIXTURES" ] || [ -z "$(ls "$FIXTURES"/*.json 2>/dev/null)" ]; then
    echo "ERROR: No JSON fixture files found in $FIXTURES" >&2
    exit 1
fi
if [ -z "$AGENTXD_BIN" ] || [ ! -x "$AGENTXD_BIN" ]; then
    echo "ERROR: Binary not found. Set AGENTXD_BIN or build first." >&2
    echo "  e.g.: AGENTXD_BIN=/path/to/smartmon-snmp-agentxd $0" >&2
    exit 1
fi

mkdir -p "$OUTPUT"

# ---------------------------------------------------------------------------
# snmpd config — AgentX master, read-only community, localhost only
# ---------------------------------------------------------------------------
cat > "$SNMPD_CONF" <<EOF
master agentx
agentxsocket $AGENTX_SOCKET
rocommunity public 127.0.0.1
agentAddress udp:127.0.0.1:10161
EOF

# ---------------------------------------------------------------------------
# Start snmpd
# ---------------------------------------------------------------------------
echo "--- starting snmpd ---"
snmpd -f -C -c "$SNMPD_CONF" -Lo &
SNMPD_PID=$!

for i in $(seq 1 20); do
    [ -S "$AGENTX_SOCKET" ] && break
    sleep 0.5
done
if [ ! -S "$AGENTX_SOCKET" ]; then
    echo "ERROR: snmpd AgentX socket did not appear" >&2
    exit 1
fi
echo "  snmpd ready"

# ---------------------------------------------------------------------------
# agentxd config
# ---------------------------------------------------------------------------
cat > "$AGENTXD_CONF" <<EOF
state_dir     $FIXTURES
agentx_socket $AGENTX_SOCKET
EOF

# ---------------------------------------------------------------------------
# Start agentxd
# ---------------------------------------------------------------------------
echo "--- starting smartmon-snmp-agentxd ---"
"$AGENTXD_BIN" -f -c "$AGENTXD_CONF" &
AGENTXD_PID=$!

# Poll until device count OID returns a real Gauge32 value (AgentX handshake complete)
# snmpwalk exits 0 even on "No Such Object", so use snmpget and check for type keyword.
REGISTERED=0
for i in $(seq 1 40); do
    if $GET "${ENT}.2.1.2.0" 2>/dev/null | grep -q "Gauge32:"; then
        REGISTERED=1
        break
    fi
    sleep 0.5
done
if [ "$REGISTERED" -eq 0 ]; then
    echo "ERROR: timed out waiting for agentxd to register (device count OID never responded)" >&2
    exit 1
fi
echo "  agentxd registered"

# ---------------------------------------------------------------------------
# Collect snmpwalk output
# ---------------------------------------------------------------------------
run_walk() {
    local label="$1" oid="$2"
    local outfile="$OUTPUT/snmpwalk-${label}.txt"
    echo "  snmpwalk $label ($oid)"
    $WALK "$oid" > "$outfile" 2>&1 || true
    echo "    $(wc -l < "$outfile") lines -> snmpwalk-${label}.txt"
}

run_walk "common" "${ENT}.2"
run_walk "nvme"   "${ENT}.3"
run_walk "sata"   "${ENT}.4"
run_walk "sas"    "${ENT}.5"

NVME_OUT="$OUTPUT/snmpwalk-nvme.txt"
SATA_OUT="$OUTPUT/snmpwalk-sata.txt"
SAS_OUT="$OUTPUT/snmpwalk-sas.txt"
COM_OUT="$OUTPUT/snmpwalk-common.txt"

# ---------------------------------------------------------------------------
# Discover device indices dynamically
# Each discovery greps for lines matching both the OID structure AND the unique
# value, then extracts the device index (2nd-from-last OID component).
#   NVMe: power_on_hours = 1827  at .3.1.5.1.15.{D}.1 = Counter64: 1827
#   SAS:  grown_defect   = 3     at .5.1.2.1.2.{D}.1  = Gauge32: 3
#   SATA: attr[9] raw    = 4321  at .4.1.3.1.9.{D}.9  = Counter64: 4321
# ---------------------------------------------------------------------------

# NVMe Samsung 980 PRO — unique power_on_hours value
NVME_IDX=$(discover_idx "$NVME_OUT" \
    "\.3\.1\.5\.1\.15\.[0-9]+\.1" \
    "Counter64: 1827")

# SAS SEAGATE — unique grown_defect_count value
SAS_IDX=$(discover_idx "$SAS_OUT" \
    "\.5\.1\.2\.1\.2\.[0-9]+\.1" \
    "Gauge32: 3")

# SATA SELFTESTS — unique attr[9] raw_value
SATA_IDX=$(discover_idx "$SATA_OUT" \
    "\.4\.1\.3\.1\.9\.[0-9]+\.9" \
    "Counter64: 4321")

echo ""
echo "--- discovered device indices ---"
echo "  NVMe (Samsung 980 PRO):         $NVME_IDX"
echo "  SAS  (SEAGATE ST4000NM0023):    $SAS_IDX"
echo "  SATA (WDC SELFTESTS):           $SATA_IDX"

# Abort if discovery failed — all subsequent tests would trivially fail
for var_name in NVME_IDX SAS_IDX SATA_IDX; do
    val="${!var_name}"
    if [ -z "$val" ] || ! [[ "$val" =~ ^[0-9]+$ ]]; then
        echo "ERROR: could not discover device index for $var_name" >&2
        exit 1
    fi
done

# ---------------------------------------------------------------------------
# --- Value validation ---
# OID structure (all relative to enterprises.99999):
#   Common device table:  .2.1.1.1.{col}.{devIdx}
#   Common device count:  .2.1.2.0
#   NVMe health table:    .3.1.5.1.{col}.{devIdx}.1
#   NVMe selftest table:  .3.1.7.1.{col}.{devIdx}.{entryIdx}
#   SATA attr table:      .4.1.3.1.{col}.{devIdx}.{attrId}
#   SATA selftest table:  .4.1.5.1.{col}.{devIdx}.{entryIdx}
#   SAS health table:     .5.1.2.1.{col}.{devIdx}.1
#   SAS error counter:    .5.1.3.1.{col}.{devIdx}.{dir}
#   SAS selftest table:   .5.1.4.1.{col}.{devIdx}.{entryIdx}
# ---------------------------------------------------------------------------
echo ""
echo "--- value checks ---"

# ===========================================================================
# Common MIB
# ===========================================================================
echo ""
echo "  [Common MIB]"

# deviceCount scalar
check_oid_exact \
    "common: deviceCount >= 3 (scalar)" \
    "$COM_OUT" \
    ".2.1.2.0" \
    "Gauge32: [0-9]+"

# Device table rows exist
check_row_count \
    "common: device table has >= 3 rows" \
    "$COM_OUT" \
    "\.2\.1\.1\.1\.2\." 3

# NVMe device (col3=path, col4=type=5 NVMe)
check_oid_exact \
    "common: NVMe device path = /dev/nvme1 (col 3)" \
    "$COM_OUT" \
    ".2.1.1.1.3.${NVME_IDX}" \
    'STRING: "/dev/nvme1"'

check_oid_exact \
    "common: NVMe device type = 5 (col 4)" \
    "$COM_OUT" \
    ".2.1.1.1.4.${NVME_IDX}" \
    "INTEGER: 5"

check_oid_exact \
    "common: NVMe last poll result = 1 ok (col 6)" \
    "$COM_OUT" \
    ".2.1.1.1.6.${NVME_IDX}" \
    "INTEGER: 1"

check_oid_exact \
    "common: NVMe poll exit status = 0 (col 7)" \
    "$COM_OUT" \
    ".2.1.1.1.7.${NVME_IDX}" \
    "Gauge32: 0"

# SAS device (col3=path, col4=type=3 SCSI)
check_oid_exact \
    "common: SAS device path = /dev/sdi (col 3)" \
    "$COM_OUT" \
    ".2.1.1.1.3.${SAS_IDX}" \
    'STRING: "/dev/sdi"'

check_oid_exact \
    "common: SAS device type = 3 (col 4)" \
    "$COM_OUT" \
    ".2.1.1.1.4.${SAS_IDX}" \
    "INTEGER: 3"

# SATA device (col3=path, col4=type=1 ATA)
check_oid_exact \
    "common: SATA device path = /dev/sdh (col 3)" \
    "$COM_OUT" \
    ".2.1.1.1.3.${SATA_IDX}" \
    'STRING: "/dev/sdh"'

check_oid_exact \
    "common: SATA device type = 1 (col 4)" \
    "$COM_OUT" \
    ".2.1.1.1.4.${SATA_IDX}" \
    "INTEGER: 1"

# ===========================================================================
# NVMe health table — Samsung 980 PRO (devIdx=$NVME_IDX)
# Columns 1-23 (not all are populated; col 3 is absent / controller-level)
# ===========================================================================
echo ""
echo "  [NVMe health table — Samsung 980 PRO, devIdx=${NVME_IDX}]"

NP=".3.1.5.1"   # NVMe health table prefix relative to enterprise

check_oid_exact "nvme: col1  critWarning = 1 (no warnings)" \
    "$NVME_OUT" "${NP}.1.${NVME_IDX}.1" "INTEGER: 1"

check_oid_exact "nvme: col4  availableSpare = 100%" \
    "$NVME_OUT" "${NP}.4.${NVME_IDX}.1" "Gauge32: 100"

check_oid_exact "nvme: col5  availableSpareThreshold = 10%" \
    "$NVME_OUT" "${NP}.5.${NVME_IDX}.1" "Gauge32: 10"

check_oid_exact "nvme: col6  percentageUsed = 2%" \
    "$NVME_OUT" "${NP}.6.${NVME_IDX}.1" "Gauge32: 2"

check_oid_exact "nvme: col7  dataUnitsRead" \
    "$NVME_OUT" "${NP}.7.${NVME_IDX}.1" "Counter64: 12004521"

check_oid_exact "nvme: col8  dataUnitsWritten" \
    "$NVME_OUT" "${NP}.8.${NVME_IDX}.1" "Counter64: 44110234"

check_oid_exact "nvme: col9  hostReadCommands" \
    "$NVME_OUT" "${NP}.9.${NVME_IDX}.1" "Counter64: 6146314752000"

check_oid_exact "nvme: col10 hostWriteCommands" \
    "$NVME_OUT" "${NP}.10.${NVME_IDX}.1" "Counter64: 22584439808000"

check_oid_exact "nvme: col11 controllerBusyTime" \
    "$NVME_OUT" "${NP}.11.${NVME_IDX}.1" "Counter64: 198443021"

check_oid_exact "nvme: col12 powerCycleCount (unused field)" \
    "$NVME_OUT" "${NP}.12.${NVME_IDX}.1" "Counter64: 612303918"

check_oid_exact "nvme: col13 powerOnHoursRaw = 1832" \
    "$NVME_OUT" "${NP}.13.${NVME_IDX}.1" "Counter64: 1832"

check_oid_exact "nvme: col14 powerCycles = 14" \
    "$NVME_OUT" "${NP}.14.${NVME_IDX}.1" "Counter64: 14"

check_oid_exact "nvme: col15 powerOnHours = 1827" \
    "$NVME_OUT" "${NP}.15.${NVME_IDX}.1" "Counter64: 1827"

check_oid_exact "nvme: col16 unsafeShutdowns = 3" \
    "$NVME_OUT" "${NP}.16.${NVME_IDX}.1" "Counter64: 3"

check_oid_exact "nvme: col17 mediaErrors = 0" \
    "$NVME_OUT" "${NP}.17.${NVME_IDX}.1" "Counter64: 0"

check_oid_exact "nvme: col18 numErrLogEntries = 0" \
    "$NVME_OUT" "${NP}.18.${NVME_IDX}.1" "Counter64: 0"

check_oid_exact "nvme: col19 warnTempTime = 0" \
    "$NVME_OUT" "${NP}.19.${NVME_IDX}.1" "Counter64: 0"

check_oid_exact "nvme: col20 critCompTime = 0" \
    "$NVME_OUT" "${NP}.20.${NVME_IDX}.1" "Counter64: 0"

check_oid_exact "nvme: col22 selfTestStatus = 0 (none in progress)" \
    "$NVME_OUT" "${NP}.22.${NVME_IDX}.1" "Gauge32: 0"

check_oid_exact "nvme: col23 selfTestStatusStr" \
    "$NVME_OUT" "${NP}.23.${NVME_IDX}.1" 'STRING: "No self-test in progress"'

# ===========================================================================
# NVMe self-test table — Samsung 980 PRO
# Columns: 2=testNumber, 3=testType, 4=result, 5=resultStr,
#          6=powerOnHoursAtTest, 7=failingLBA, 8=failingNamespace,
#          9=statusCode, 10=segmentNumber, 11=validDiagnosticInfo
# ===========================================================================
echo ""
echo "  [NVMe selftest table — Samsung 980 PRO, devIdx=${NVME_IDX}]"

NST=".3.1.7.1"   # NVMe selftest table prefix

check_row_count "nvme: selftest table has >= 2 entries" \
    "$NVME_OUT" "\.3\.1\.7\.1\.2\." 2

# Entry 1 (Short, completed without error)
check_oid_exact "nvme: selftest[1] col2 testNumber = 1" \
    "$NVME_OUT" "${NST}.2.${NVME_IDX}.1" "Gauge32: 1"

check_oid_exact "nvme: selftest[1] col3 testType = 1 (Short)" \
    "$NVME_OUT" "${NST}.3.${NVME_IDX}.1" "INTEGER: 1"

check_oid_exact "nvme: selftest[1] col4 result = 0 (no error)" \
    "$NVME_OUT" "${NST}.4.${NVME_IDX}.1" "INTEGER: 0"

check_oid_exact "nvme: selftest[1] col5 resultStr" \
    "$NVME_OUT" "${NST}.5.${NVME_IDX}.1" 'STRING: "Completed without error"'

check_oid_exact "nvme: selftest[1] col6 powerOnHoursAtTest = 1826" \
    "$NVME_OUT" "${NST}.6.${NVME_IDX}.1" "Counter64: 1826"

check_oid_exact "nvme: selftest[1] col8 failingNamespace (no failure = 0xFFFFFFFF)" \
    "$NVME_OUT" "${NST}.8.${NVME_IDX}.1" "Gauge32: 4294967295"

check_oid_exact "nvme: selftest[1] col9 statusCode = 0" \
    "$NVME_OUT" "${NST}.9.${NVME_IDX}.1" "Gauge32: 0"

check_oid_exact "nvme: selftest[1] col10 segmentNumber = 0" \
    "$NVME_OUT" "${NST}.10.${NVME_IDX}.1" "Gauge32: 0"

check_oid_exact "nvme: selftest[1] col11 validDiagnosticInfo = 0" \
    "$NVME_OUT" "${NST}.11.${NVME_IDX}.1" "Gauge32: 0"

# Entry 2 (Extended, completed without error)
check_oid_exact "nvme: selftest[2] col2 testNumber = 2" \
    "$NVME_OUT" "${NST}.2.${NVME_IDX}.2" "Gauge32: 2"

check_oid_exact "nvme: selftest[2] col3 testType = 2 (Extended)" \
    "$NVME_OUT" "${NST}.3.${NVME_IDX}.2" "INTEGER: 2"

check_oid_exact "nvme: selftest[2] col4 result = 0 (no error)" \
    "$NVME_OUT" "${NST}.4.${NVME_IDX}.2" "INTEGER: 0"

check_oid_exact "nvme: selftest[2] col6 powerOnHoursAtTest = 1800" \
    "$NVME_OUT" "${NST}.6.${NVME_IDX}.2" "Counter64: 1800"

# ===========================================================================
# SATA attribute table — WDC WD140EFGX SELFTESTS (devIdx=$SATA_IDX)
# Columns: 2=name, 3=flags, 4=flagsRaw, 5=prefailure, 6=value,
#          7=worst, 8=threshold, 9=rawValue, 10=rawStr, 11=type, 12=updated
# ===========================================================================
echo ""
echo "  [SATA attr table — WDC SELFTESTS, devIdx=${SATA_IDX}]"

SAT=".4.1.3.1"

# attr id=1: Raw_Read_Error_Rate
check_oid_exact "sata: attr[1] col2  name = Raw_Read_Error_Rate" \
    "$SATA_OUT" "${SAT}.2.${SATA_IDX}.1" 'STRING: "Raw_Read_Error_Rate"'

check_oid_exact "sata: attr[1] col4  prefailure = 1 (true)" \
    "$SATA_OUT" "${SAT}.4.${SATA_IDX}.1" "INTEGER: 1"

check_oid_exact "sata: attr[1] col5  updatedOnline = 1 (true)" \
    "$SATA_OUT" "${SAT}.5.${SATA_IDX}.1" "INTEGER: 1"

check_oid_exact "sata: attr[1] col6  value = 200" \
    "$SATA_OUT" "${SAT}.6.${SATA_IDX}.1" "Gauge32: 200"

check_oid_exact "sata: attr[1] col7  worst = 200" \
    "$SATA_OUT" "${SAT}.7.${SATA_IDX}.1" "Gauge32: 200"

check_oid_exact "sata: attr[1] col8  threshold = 51" \
    "$SATA_OUT" "${SAT}.8.${SATA_IDX}.1" "Gauge32: 51"

check_oid_exact "sata: attr[1] col9  rawValue = 0" \
    "$SATA_OUT" "${SAT}.9.${SATA_IDX}.1" "Counter64: 0"

check_oid_exact "sata: attr[1] col10 rawStr = 0" \
    "$SATA_OUT" "${SAT}.10.${SATA_IDX}.1" 'STRING: "0"'

check_oid_exact "sata: attr[1] col12 whenFailed = 0 (never)" \
    "$SATA_OUT" "${SAT}.12.${SATA_IDX}.1" "INTEGER: 0"

# attr id=9: Power_On_Hours
check_oid_exact "sata: attr[9] col2  name = Power_On_Hours" \
    "$SATA_OUT" "${SAT}.2.${SATA_IDX}.9" 'STRING: "Power_On_Hours"'

check_oid_exact "sata: attr[9] col4  prefailure = 2 (false)" \
    "$SATA_OUT" "${SAT}.4.${SATA_IDX}.9" "INTEGER: 2"

check_oid_exact "sata: attr[9] col5  updatedOnline = 1 (true)" \
    "$SATA_OUT" "${SAT}.5.${SATA_IDX}.9" "INTEGER: 1"

check_oid_exact "sata: attr[9] col6  value = 100" \
    "$SATA_OUT" "${SAT}.6.${SATA_IDX}.9" "Gauge32: 100"

check_oid_exact "sata: attr[9] col7  worst = 100" \
    "$SATA_OUT" "${SAT}.7.${SATA_IDX}.9" "Gauge32: 100"

check_oid_exact "sata: attr[9] col8  threshold = 0" \
    "$SATA_OUT" "${SAT}.8.${SATA_IDX}.9" "Gauge32: 0"

check_oid_exact "sata: attr[9] col9  rawValue = 4321 (Power_On_Hours)" \
    "$SATA_OUT" "${SAT}.9.${SATA_IDX}.9" "Counter64: 4321"

check_oid_exact "sata: attr[9] col10 rawStr = 4321" \
    "$SATA_OUT" "${SAT}.10.${SATA_IDX}.9" 'STRING: "4321"'

check_oid_exact "sata: attr[9] col12 whenFailed = 0 (never)" \
    "$SATA_OUT" "${SAT}.12.${SATA_IDX}.9" "INTEGER: 0"

# ===========================================================================
# SATA self-test table — WDC WD140EFGX SELFTESTS (devIdx=$SATA_IDX)
# Columns: 2=testNumber, 4=errorCode, 5=resultStr, 6=passed,
#          7=remainingPercent, 8=powerOnHoursAtTest, 9=lbaFirstError
# ===========================================================================
echo ""
echo "  [SATA selftest table — WDC SELFTESTS, devIdx=${SATA_IDX}]"

SST=".4.1.5.1"

check_row_count "sata: selftest table has >= 3 entries" \
    "$SATA_OUT" "\.4\.1\.5\.1\.2\." 3

# Entry 1 (Short, passed)
check_oid_exact "sata: selftest[1] col2 testNumber = 1 (Short)" \
    "$SATA_OUT" "${SST}.2.${SATA_IDX}.1" "INTEGER: 1"

check_oid_exact "sata: selftest[1] col4 errorCode = 0" \
    "$SATA_OUT" "${SST}.4.${SATA_IDX}.1" "INTEGER: 0"

check_oid_exact "sata: selftest[1] col5 resultStr" \
    "$SATA_OUT" "${SST}.5.${SATA_IDX}.1" 'STRING: "Completed without error"'

check_oid_exact "sata: selftest[1] col6 passed = 1 (true)" \
    "$SATA_OUT" "${SST}.6.${SATA_IDX}.1" "INTEGER: 1"

check_oid_exact "sata: selftest[1] col7 remaining = 0%" \
    "$SATA_OUT" "${SST}.7.${SATA_IDX}.1" "Gauge32: 0"

check_oid_exact "sata: selftest[1] col8 powerOnHoursAtTest = 4300" \
    "$SATA_OUT" "${SST}.8.${SATA_IDX}.1" "Counter64: 4300"

check_oid_exact "sata: selftest[1] col9 lbaFirstError = 0 (no error)" \
    "$SATA_OUT" "${SST}.9.${SATA_IDX}.1" "Counter64: 0"

# Entry 2 (Extended, passed)
check_oid_exact "sata: selftest[2] col2 testNumber = 2 (Extended)" \
    "$SATA_OUT" "${SST}.2.${SATA_IDX}.2" "INTEGER: 2"

check_oid_exact "sata: selftest[2] col4 errorCode = 0" \
    "$SATA_OUT" "${SST}.4.${SATA_IDX}.2" "INTEGER: 0"

check_oid_exact "sata: selftest[2] col6 passed = 1 (true)" \
    "$SATA_OUT" "${SST}.6.${SATA_IDX}.2" "INTEGER: 1"

check_oid_exact "sata: selftest[2] col8 powerOnHoursAtTest = 4100" \
    "$SATA_OUT" "${SST}.8.${SATA_IDX}.2" "Counter64: 4100"

check_oid_exact "sata: selftest[2] col9 lbaFirstError = 0 (no error)" \
    "$SATA_OUT" "${SST}.9.${SATA_IDX}.2" "Counter64: 0"

# Entry 3 (Short, failed with read failure)
check_oid_exact "sata: selftest[3] col2 testNumber = 1 (Short)" \
    "$SATA_OUT" "${SST}.2.${SATA_IDX}.3" "INTEGER: 1"

check_oid_exact "sata: selftest[3] col4 errorCode = 119" \
    "$SATA_OUT" "${SST}.4.${SATA_IDX}.3" "INTEGER: 119"

check_oid_exact "sata: selftest[3] col5 resultStr = Completed: read failure" \
    "$SATA_OUT" "${SST}.5.${SATA_IDX}.3" 'STRING: "Completed: read failure"'

check_oid_exact "sata: selftest[3] col6 passed = 2 (false/failed)" \
    "$SATA_OUT" "${SST}.6.${SATA_IDX}.3" "INTEGER: 2"

check_oid_exact "sata: selftest[3] col7 remaining = 0%" \
    "$SATA_OUT" "${SST}.7.${SATA_IDX}.3" "Gauge32: 0"

check_oid_exact "sata: selftest[3] col8 powerOnHoursAtTest = 3800" \
    "$SATA_OUT" "${SST}.8.${SATA_IDX}.3" "Counter64: 3800"

check_oid_exact "sata: selftest[3] col9 lbaFirstError = 123456789" \
    "$SATA_OUT" "${SST}.9.${SATA_IDX}.3" "Counter64: 123456789"

# ===========================================================================
# SAS health table — SEAGATE ST4000NM0023 (devIdx=$SAS_IDX)
# Columns: 1=index, 2=grownDefectCount, 3=primaryDefectCount,
#          4=selfTestStatus, 5=selfTestPercent
# ===========================================================================
echo ""
echo "  [SAS health table — SEAGATE ST4000NM0023, devIdx=${SAS_IDX}]"

SHP=".5.1.2.1"

check_oid_exact "sas: health col1 index = 1" \
    "$SAS_OUT" "${SHP}.1.${SAS_IDX}.1" "INTEGER: 1"

check_oid_exact "sas: health col2 grownDefectCount = 3" \
    "$SAS_OUT" "${SHP}.2.${SAS_IDX}.1" "Gauge32: 3"

check_oid_exact "sas: health col3 primaryDefectCount = 0" \
    "$SAS_OUT" "${SHP}.3.${SAS_IDX}.1" "Counter64: 0"

check_oid_exact "sas: health col4 selfTestStatus = 2 (no test running)" \
    "$SAS_OUT" "${SHP}.4.${SAS_IDX}.1" "INTEGER: 2"

check_oid_exact "sas: health col5 selfTestPercent = 0" \
    "$SAS_OUT" "${SHP}.5.${SAS_IDX}.1" "Gauge32: 0"

# ===========================================================================
# SAS error counter table — SEAGATE ST4000NM0023 (devIdx=$SAS_IDX)
# Columns: 2=correctedWithDelay, 3=correctedWithRetry, 4=correctedTotal,
#          5=uncorrectedTotal, 6=totalBytesProcessed, 7=totalBytesTransferred,
#          8=errorCount
# Directions: 1=read, 2=write
# ===========================================================================
echo ""
echo "  [SAS error counter table — SEAGATE ST4000NM0023, devIdx=${SAS_IDX}]"

SEC=".5.1.3.1"

check_row_count "sas: error counter table has >= 2 rows (read + write)" \
    "$SAS_OUT" "\.5\.1\.3\.1\.2\." 2

# Direction 1 = read
check_oid_exact "sas: errCount[read] col2 correctedWithDelay = 0" \
    "$SAS_OUT" "${SEC}.2.${SAS_IDX}.1" "Counter64: 0"

check_oid_exact "sas: errCount[read] col3 correctedWithRetry = 0" \
    "$SAS_OUT" "${SEC}.3.${SAS_IDX}.1" "Counter64: 0"

check_oid_exact "sas: errCount[read] col4 correctedTotal = 0" \
    "$SAS_OUT" "${SEC}.4.${SAS_IDX}.1" "Counter64: 0"

check_oid_exact "sas: errCount[read] col5 uncorrectedTotal = 0" \
    "$SAS_OUT" "${SEC}.5.${SAS_IDX}.1" "Counter64: 0"

check_oid_exact "sas: errCount[read] col6 totalBytesProcessed = 18432971" \
    "$SAS_OUT" "${SEC}.6.${SAS_IDX}.1" "Counter64: 18432971"

check_oid_exact "sas: errCount[read] col7 totalBytesTransferred = 47221194000000" \
    "$SAS_OUT" "${SEC}.7.${SAS_IDX}.1" "Counter64: 47221194000000"

check_oid_exact "sas: errCount[read] col8 errorCount = 0" \
    "$SAS_OUT" "${SEC}.8.${SAS_IDX}.1" "Counter64: 0"

# Direction 2 = write
check_oid_exact "sas: errCount[write] col2 correctedWithDelay = 0" \
    "$SAS_OUT" "${SEC}.2.${SAS_IDX}.2" "Counter64: 0"

check_oid_exact "sas: errCount[write] col5 uncorrectedTotal = 0" \
    "$SAS_OUT" "${SEC}.5.${SAS_IDX}.2" "Counter64: 0"

check_oid_exact "sas: errCount[write] col6 totalBytesProcessed = 0" \
    "$SAS_OUT" "${SEC}.6.${SAS_IDX}.2" "Counter64: 0"

check_oid_exact "sas: errCount[write] col7 totalBytesTransferred = 6214377000000" \
    "$SAS_OUT" "${SEC}.7.${SAS_IDX}.2" "Counter64: 6214377000000"

check_oid_exact "sas: errCount[write] col8 errorCount = 0" \
    "$SAS_OUT" "${SEC}.8.${SAS_IDX}.2" "Counter64: 0"

# ===========================================================================
# SAS self-test table — SEAGATE ST4000NM0023 (devIdx=$SAS_IDX)
# Columns: 2=testNumber, 4=errorCode, 5=resultStr, 6=passed,
#          7=powerOnHoursAtTest, 8=lbaFirstError
# ===========================================================================
echo ""
echo "  [SAS selftest table — SEAGATE ST4000NM0023, devIdx=${SAS_IDX}]"

SSST=".5.1.4.1"

check_row_count "sas: selftest table has >= 2 entries" \
    "$SAS_OUT" "\.5\.1\.4\.1\.2\." 2

# Entry 1 (Short, passed)
check_oid_exact "sas: selftest[1] col2 testNumber = 1 (Short)" \
    "$SAS_OUT" "${SSST}.2.${SAS_IDX}.1" "INTEGER: 1"

check_oid_exact "sas: selftest[1] col4 errorCode = 0" \
    "$SAS_OUT" "${SSST}.4.${SAS_IDX}.1" "INTEGER: 0"

check_oid_exact "sas: selftest[1] col5 resultStr = Completed" \
    "$SAS_OUT" "${SSST}.5.${SAS_IDX}.1" 'STRING: "Completed"'

check_oid_exact "sas: selftest[1] col6 passed = 1 (true)" \
    "$SAS_OUT" "${SSST}.6.${SAS_IDX}.1" "INTEGER: 1"

check_oid_exact "sas: selftest[1] col7 powerOnHoursAtTest = 8741" \
    "$SAS_OUT" "${SSST}.7.${SAS_IDX}.1" "Counter64: 8741"

check_oid_exact "sas: selftest[1] col8 lbaFirstError = 0 (no error)" \
    "$SAS_OUT" "${SSST}.8.${SAS_IDX}.1" "Counter64: 0"

# Entry 2 (Extended, passed)
check_oid_exact "sas: selftest[2] col2 testNumber = 2 (Extended)" \
    "$SAS_OUT" "${SSST}.2.${SAS_IDX}.2" "INTEGER: 2"

check_oid_exact "sas: selftest[2] col4 errorCode = 0" \
    "$SAS_OUT" "${SSST}.4.${SAS_IDX}.2" "INTEGER: 0"

check_oid_exact "sas: selftest[2] col5 resultStr = Completed" \
    "$SAS_OUT" "${SSST}.5.${SAS_IDX}.2" 'STRING: "Completed"'

check_oid_exact "sas: selftest[2] col6 passed = 1 (true)" \
    "$SAS_OUT" "${SSST}.6.${SAS_IDX}.2" "INTEGER: 1"

check_oid_exact "sas: selftest[2] col7 powerOnHoursAtTest = 8568" \
    "$SAS_OUT" "${SSST}.7.${SAS_IDX}.2" "Counter64: 8568"

check_oid_exact "sas: selftest[2] col8 lbaFirstError = 0 (no error)" \
    "$SAS_OUT" "${SSST}.8.${SAS_IDX}.2" "Counter64: 0"

# ===========================================================================
# NVMe controller table — Samsung 980 PRO (devIdx=$NVME_IDX)
# OID: .3.1.1.1.{col}.{devIdx}.1
# col 1=modelNumber  col 2=serialNumber  col 3=firmwareVersion
# col 4=pciVendorId  col 5=ieeeOui       col 6=totalCapacityBytes
# col 7=unallocatedCapacityBytes  col 8=controllerId
# col 9=version  col 10=namespaceCount   col 15=pciSubsystemId
# col 16=versionValue
# ===========================================================================
echo ""
echo "  [NVMe controller table — Samsung 980 PRO, devIdx=${NVME_IDX}]"

NCP=".3.1.1.1"

check_oid_exact "nvme: ctrl col1 modelNumber" \
    "$NVME_OUT" "${NCP}.1.${NVME_IDX}.1" 'STRING: "Samsung SSD 980 PRO 1TB"'

check_oid_exact "nvme: ctrl col2 serialNumber" \
    "$NVME_OUT" "${NCP}.2.${NVME_IDX}.1" 'STRING: "S6B0NJ0N123456"'

check_oid_exact "nvme: ctrl col3 firmwareVersion" \
    "$NVME_OUT" "${NCP}.3.${NVME_IDX}.1" 'STRING: "5B2QGXA7"'

check_oid_exact "nvme: ctrl col4 pciVendorId = 5387 (Samsung)" \
    "$NVME_OUT" "${NCP}.4.${NVME_IDX}.1" "Gauge32: 5387"

check_oid_exact "nvme: ctrl col6 totalCapacityBytes = 1000204886016" \
    "$NVME_OUT" "${NCP}.6.${NVME_IDX}.1" "Counter64: 1000204886016"

check_oid_exact "nvme: ctrl col7 unallocatedCapacityBytes = 0" \
    "$NVME_OUT" "${NCP}.7.${NVME_IDX}.1" "Counter64: 0"

check_oid_exact "nvme: ctrl col8 controllerId = 1" \
    "$NVME_OUT" "${NCP}.8.${NVME_IDX}.1" "Gauge32: 1"

check_oid_exact "nvme: ctrl col9 version = 1.3" \
    "$NVME_OUT" "${NCP}.9.${NVME_IDX}.1" 'STRING: "1.3"'

check_oid_exact "nvme: ctrl col10 namespaceCount = 1" \
    "$NVME_OUT" "${NCP}.10.${NVME_IDX}.1" "Gauge32: 1"

check_oid_exact "nvme: ctrl col15 pciSubsystemId = 2" \
    "$NVME_OUT" "${NCP}.15.${NVME_IDX}.1" "Gauge32: 2"

check_oid_exact "nvme: ctrl col16 versionValue = 66304" \
    "$NVME_OUT" "${NCP}.16.${NVME_IDX}.1" "Gauge32: 66304"

# ===========================================================================
# NVMe namespace table — Samsung 980 PRO (devIdx=$NVME_IDX, nsId=1)
# OID: .3.1.3.1.{col}.{devIdx}.{nsId}
# col 1=namespaceId  col 2=sizeBytes  col 3=capacityBytes
# col 4=utilizationBytes  col 5=formattedLbaSize
# col 8=sizeBlocks  col 9=capacityBlocks  col 10=utilizationBlocks
# ===========================================================================
echo ""
echo "  [NVMe namespace table — Samsung 980 PRO ns1, devIdx=${NVME_IDX}]"

NNP=".3.1.3.1"

check_oid_exact "nvme: ns[1] col1 namespaceId = 1" \
    "$NVME_OUT" "${NNP}.1.${NVME_IDX}.1" "Gauge32: 1"

check_oid_exact "nvme: ns[1] col2 sizeBytes = 1000204886016" \
    "$NVME_OUT" "${NNP}.2.${NVME_IDX}.1" "Counter64: 1000204886016"

check_oid_exact "nvme: ns[1] col3 capacityBytes = 1000204886016" \
    "$NVME_OUT" "${NNP}.3.${NVME_IDX}.1" "Counter64: 1000204886016"

check_oid_exact "nvme: ns[1] col4 utilizationBytes = 998400000000" \
    "$NVME_OUT" "${NNP}.4.${NVME_IDX}.1" "Counter64: 998400000000"

check_oid_exact "nvme: ns[1] col5 formattedLbaSize = 512" \
    "$NVME_OUT" "${NNP}.5.${NVME_IDX}.1" "Gauge32: 512"

check_oid_exact "nvme: ns[1] col8 sizeBlocks = 1953525168" \
    "$NVME_OUT" "${NNP}.8.${NVME_IDX}.1" "Counter64: 1953525168"

check_oid_exact "nvme: ns[1] col10 utilizationBlocks = 1950000000" \
    "$NVME_OUT" "${NNP}.10.${NVME_IDX}.1" "Counter64: 1950000000"

# ===========================================================================
# NVMe error log table — Samsung 980 PRO (devIdx=$NVME_IDX, entry 1)
# OID: .3.1.8.1.{col}.{devIdx}.{entryIdx}
# col 2=errorCount  col 3=sqid  col 4=commandId  col 5=statusField
# col 8=nsid  col 10=statusCode  col 11=statusCodeType
# col 12=doNotRetry  col 13=statusString  col 14=phaseTag
# ===========================================================================
echo ""
echo "  [NVMe error log table — Samsung 980 PRO, devIdx=${NVME_IDX}]"

NEL=".3.1.8.1"

check_oid_exact "nvme: errlog[1] col2 errorCount = 7" \
    "$NVME_OUT" "${NEL}.2.${NVME_IDX}.1" "Counter64: 7"

check_oid_exact "nvme: errlog[1] col3 sqid = 0" \
    "$NVME_OUT" "${NEL}.3.${NVME_IDX}.1" "Gauge32: 0"

check_oid_exact "nvme: errlog[1] col4 commandId = 3" \
    "$NVME_OUT" "${NEL}.4.${NVME_IDX}.1" "Gauge32: 3"

check_oid_exact "nvme: errlog[1] col5 statusField = 5" \
    "$NVME_OUT" "${NEL}.5.${NVME_IDX}.1" "Gauge32: 5"

check_oid_exact "nvme: errlog[1] col8 nsid = 1" \
    "$NVME_OUT" "${NEL}.8.${NVME_IDX}.1" "Gauge32: 1"

check_oid_exact "nvme: errlog[1] col10 statusCode = 5" \
    "$NVME_OUT" "${NEL}.10.${NVME_IDX}.1" "Gauge32: 5"

check_oid_exact "nvme: errlog[1] col11 statusCodeType = 0" \
    "$NVME_OUT" "${NEL}.11.${NVME_IDX}.1" "Gauge32: 0"

check_oid_exact "nvme: errlog[1] col12 doNotRetry = 2 (false)" \
    "$NVME_OUT" "${NEL}.12.${NVME_IDX}.1" "INTEGER: 2"

check_oid_exact "nvme: errlog[1] col13 statusString" \
    "$NVME_OUT" "${NEL}.13.${NVME_IDX}.1" 'STRING: "Invalid Field in Command"'

check_oid_exact "nvme: errlog[1] col14 phaseTag = 2 (false)" \
    "$NVME_OUT" "${NEL}.14.${NVME_IDX}.1" "INTEGER: 2"

# ===========================================================================
# SATA info table — WDC WD140EFGX SELFTESTS (devIdx=$SATA_IDX)
# OID: .4.1.1.1.{col}.{devIdx}.1
# col 1=modelFamily  col 2=modelName  col 3=serialNumber
# col 4=firmwareVersion  col 8=rotationRate  col 9=formFactor
# col 10=logicalBlockSize  col 11=physicalBlockSize
# col 12=userCapacityBytes  col 13=inSmartctlDb  col 14=smartAvailable
# col 15=smartEnabled  col 16=trimSupported
# ===========================================================================
echo ""
echo "  [SATA info table — WDC SELFTESTS, devIdx=${SATA_IDX}]"

SIF=".4.1.1.1"

check_oid_exact "sata: info col1 modelFamily" \
    "$SATA_OUT" "${SIF}.1.${SATA_IDX}.1" 'STRING: "Western Digital Red"'

check_oid_exact "sata: info col2 modelName" \
    "$SATA_OUT" "${SIF}.2.${SATA_IDX}.1" 'STRING: "WDC WD140EFGX-68B0GN0"'

check_oid_exact "sata: info col3 serialNumber" \
    "$SATA_OUT" "${SIF}.3.${SATA_IDX}.1" 'STRING: "WD140SELFTESTS"'

check_oid_exact "sata: info col4 firmwareVersion" \
    "$SATA_OUT" "${SIF}.4.${SATA_IDX}.1" 'STRING: "MGNTN10"'

check_oid_exact "sata: info col8 rotationRate = 5400" \
    "$SATA_OUT" "${SIF}.8.${SATA_IDX}.1" "Gauge32: 5400"

check_oid_exact "sata: info col9 formFactor = 3.5 inches" \
    "$SATA_OUT" "${SIF}.9.${SATA_IDX}.1" 'STRING: "3.5 inches"'

check_oid_exact "sata: info col10 logicalBlockSize = 4096" \
    "$SATA_OUT" "${SIF}.10.${SATA_IDX}.1" "Gauge32: 4096"

check_oid_exact "sata: info col12 userCapacityBytes = 14000519643136" \
    "$SATA_OUT" "${SIF}.12.${SATA_IDX}.1" "Counter64: 14000519643136"

check_oid_exact "sata: info col13 inSmartctlDb = 1 (true)" \
    "$SATA_OUT" "${SIF}.13.${SATA_IDX}.1" "INTEGER: 1"

check_oid_exact "sata: info col14 smartAvailable = 1 (true)" \
    "$SATA_OUT" "${SIF}.14.${SATA_IDX}.1" "INTEGER: 1"

check_oid_exact "sata: info col15 smartEnabled = 1 (true)" \
    "$SATA_OUT" "${SIF}.15.${SATA_IDX}.1" "INTEGER: 1"

check_oid_exact "sata: info col16 trimSupported = 2 (false)" \
    "$SATA_OUT" "${SIF}.16.${SATA_IDX}.1" "INTEGER: 2"

# ===========================================================================
# SATA health table — WDC WD140EFGX SELFTESTS (devIdx=$SATA_IDX)
# OID: .4.1.2.1.{col}.{devIdx}.1
# col 1=overallStatus  col 2=offlineStatusValue  col 3=offlineStatusString
# col 4=offlineCompletionSecs  col 5=selftestStatusValue
# col 7=pollingShortMin  col 8=pollingExtMin  col 9=pollingConvMin
# col 10=capAutoOffline  col 11=capSelftests  col 15=capGpLogging
# col 16=sctErrRecovery  col 17=sctFeatureControl  col 18=sctDataTable
# col 19=powerCycles  col 20=powerOnHours  col 21=errorLogCount
# ===========================================================================
echo ""
echo "  [SATA health table — WDC SELFTESTS, devIdx=${SATA_IDX}]"

SHL=".4.1.2.1"

check_oid_exact "sata: health col1 overallStatus = 1 (passed)" \
    "$SATA_OUT" "${SHL}.1.${SATA_IDX}.1" "INTEGER: 1"

check_oid_exact "sata: health col2 offlineStatusValue = 2" \
    "$SATA_OUT" "${SHL}.2.${SATA_IDX}.1" "Gauge32: 2"

check_oid_exact "sata: health col5 selftestStatusValue = 0" \
    "$SATA_OUT" "${SHL}.5.${SATA_IDX}.1" "Gauge32: 0"

check_oid_exact "sata: health col7 pollingShortMin = 2" \
    "$SATA_OUT" "${SHL}.7.${SATA_IDX}.1" "Gauge32: 2"

check_oid_exact "sata: health col8 pollingExtMin = 255" \
    "$SATA_OUT" "${SHL}.8.${SATA_IDX}.1" "Gauge32: 255"

check_oid_exact "sata: health col9 pollingConvMin = 5" \
    "$SATA_OUT" "${SHL}.9.${SATA_IDX}.1" "Gauge32: 5"

check_oid_exact "sata: health col10 capAutoOffline = 2 (false)" \
    "$SATA_OUT" "${SHL}.10.${SATA_IDX}.1" "INTEGER: 2"

check_oid_exact "sata: health col11 capSelftests = 1 (true)" \
    "$SATA_OUT" "${SHL}.11.${SATA_IDX}.1" "INTEGER: 1"

check_oid_exact "sata: health col17 sctFeatureControl = 1 (true)" \
    "$SATA_OUT" "${SHL}.17.${SATA_IDX}.1" "INTEGER: 1"

check_oid_exact "sata: health col19 powerCycles = 42" \
    "$SATA_OUT" "${SHL}.19.${SATA_IDX}.1" "Counter64: 42"

check_oid_exact "sata: health col20 powerOnHours = 4321" \
    "$SATA_OUT" "${SHL}.20.${SATA_IDX}.1" "Counter64: 4321"

check_oid_exact "sata: health col21 errorLogCount = 1" \
    "$SATA_OUT" "${SHL}.21.${SATA_IDX}.1" "Gauge32: 1"

# ===========================================================================
# SATA error log table — WDC WD140EFGX SELFTESTS (devIdx=$SATA_IDX, entry 1)
# OID: .4.1.4.1.{col}.{devIdx}.{entryIdx}
# col 2=errorNumber  col 3=errorLifetimeHours  col 4=errorDescription
# col 5=compRegError  col 6=compRegStatus  col 7=errorLba
# col 8=regCommand  col 9=regCount  col 12=stateValue  col 13=stateString
# ===========================================================================
echo ""
echo "  [SATA error log table — WDC SELFTESTS, devIdx=${SATA_IDX}]"

SEL_P=".4.1.4.1"

check_oid_exact "sata: errlog[1] col2 errorNumber = 1" \
    "$SATA_OUT" "${SEL_P}.2.${SATA_IDX}.1" "Gauge32: 1"

check_oid_exact "sata: errlog[1] col3 lifetimeHours = 4100" \
    "$SATA_OUT" "${SEL_P}.3.${SATA_IDX}.1" "Counter64: 4100"

check_oid_exact "sata: errlog[1] col4 description" \
    "$SATA_OUT" "${SEL_P}.4.${SATA_IDX}.1" 'STRING: "Error: ICRC, ABRT at LBA = 0x0000abcd"'

check_oid_exact "sata: errlog[1] col5 compRegError = 4" \
    "$SATA_OUT" "${SEL_P}.5.${SATA_IDX}.1" "Gauge32: 4"

check_oid_exact "sata: errlog[1] col7 lba = 43981" \
    "$SATA_OUT" "${SEL_P}.7.${SATA_IDX}.1" "Counter64: 43981"

check_oid_exact "sata: errlog[1] col12 stateValue = 15" \
    "$SATA_OUT" "${SEL_P}.12.${SATA_IDX}.1" "Gauge32: 15"

check_oid_exact "sata: errlog[1] col13 stateString = standby" \
    "$SATA_OUT" "${SEL_P}.13.${SATA_IDX}.1" 'STRING: "standby"'

# ===========================================================================
# SAS info table — SEAGATE ST4000NM0023 (devIdx=$SAS_IDX)
# OID: .5.1.1.1.{col}.{devIdx}.1
# col 1=vendor  col 2=product  col 3=revision  col 4=compliance
# col 5=serialNumber  col 7=scsiModelName  col 8=rotationRate
# col 9=formFactor  col 10=logicalBlockSize  col 11=physicalBlockSize
# col 12=userCapacityBytes  col 13=powerCycles  col 14=powerOnHours
# ===========================================================================
echo ""
echo "  [SAS info table — SEAGATE ST4000NM0023, devIdx=${SAS_IDX}]"

SAI=".5.1.1.1"

check_oid_exact "sas: info col1 vendor = SEAGATE" \
    "$SAS_OUT" "${SAI}.1.${SAS_IDX}.1" 'STRING: "SEAGATE"'

check_oid_exact "sas: info col2 product = ST4000NM0023" \
    "$SAS_OUT" "${SAI}.2.${SAS_IDX}.1" 'STRING: "ST4000NM0023"'

check_oid_exact "sas: info col3 revision = 0004" \
    "$SAS_OUT" "${SAI}.3.${SAS_IDX}.1" 'STRING: "0004"'

check_oid_exact "sas: info col4 compliance = SPC-4" \
    "$SAS_OUT" "${SAI}.4.${SAS_IDX}.1" 'STRING: "SPC-4"'

check_oid_exact "sas: info col5 serialNumber = Z1Z0ABCD" \
    "$SAS_OUT" "${SAI}.5.${SAS_IDX}.1" 'STRING: "Z1Z0ABCD"'

check_oid_exact "sas: info col7 scsiModelName = SEAGATE ST4000NM0023" \
    "$SAS_OUT" "${SAI}.7.${SAS_IDX}.1" 'STRING: "SEAGATE ST4000NM0023"'

check_oid_exact "sas: info col8 rotationRate = 7200" \
    "$SAS_OUT" "${SAI}.8.${SAS_IDX}.1" "Gauge32: 7200"

check_oid_exact "sas: info col9 formFactor = 3.5 inches" \
    "$SAS_OUT" "${SAI}.9.${SAS_IDX}.1" 'STRING: "3.5 inches"'

check_oid_exact "sas: info col10 logicalBlockSize = 512" \
    "$SAS_OUT" "${SAI}.10.${SAS_IDX}.1" "Gauge32: 512"

check_oid_exact "sas: info col12 userCapacityBytes = 4000787030016" \
    "$SAS_OUT" "${SAI}.12.${SAS_IDX}.1" "Counter64: 4000787030016"

check_oid_exact "sas: info col13 powerCycles = 15" \
    "$SAS_OUT" "${SAI}.13.${SAS_IDX}.1" "Counter64: 15"

check_oid_exact "sas: info col14 powerOnHours = 8741" \
    "$SAS_OUT" "${SAI}.14.${SAS_IDX}.1" "Counter64: 8741"

# ===========================================================================
# SAS background scan table — SEAGATE ST4000NM0023 (devIdx=$SAS_IDX)
# OID: .5.1.5.1.{col}.{devIdx}.1
# col 1=bgScanStatus  col 2=bgScanProgressPercent
# col 3=bgScanScansPerformed  col 4=bgScanMediumScansPerformed
# ===========================================================================
echo ""
echo "  [SAS background scan table — SEAGATE ST4000NM0023, devIdx=${SAS_IDX}]"

SBG=".5.1.5.1"

check_oid_exact "sas: bgscan col1 status = 4 (Idle)" \
    "$SAS_OUT" "${SBG}.1.${SAS_IDX}.1" "INTEGER: 4"

check_oid_exact "sas: bgscan col2 progressPercent = 0" \
    "$SAS_OUT" "${SBG}.2.${SAS_IDX}.1" "Gauge32: 0"

check_oid_exact "sas: bgscan col3 scansPerformed = 3" \
    "$SAS_OUT" "${SBG}.3.${SAS_IDX}.1" "Counter64: 3"

check_oid_exact "sas: bgscan col4 mediumScansPerformed = 12" \
    "$SAS_OUT" "${SBG}.4.${SAS_IDX}.1" "Counter64: 12"

# ===========================================================================
# Still-planned tables (not yet implemented) — mark as skipped
# ===========================================================================
echo ""
echo "  [Planned tables — expected empty (not yet implemented)]"

# skip_not_implemented LABEL
#   Records one SKIP and prints a "SKIP" line.  Used for tables that are
#   defined in the MIB but not yet implemented in the agent.
skip_not_implemented() {
    local label="$1"
    echo "  SKIP: $label (not yet implemented)"
    SKIP=$((SKIP + 1))
}

# NVMe still-planned tables
skip_not_implemented "nvme: NvmePowerStateTable (.3.1.4)"
skip_not_implemented "nvme: NvmeLbaFormatTable (.3.1.6)"
skip_not_implemented "nvme: NvmeCapabilityTable (.3.1.9)"

# SATA still-planned tables
skip_not_implemented "sata: SataErrorCmdTable (.4.1.6)"

# Sensor MIB — not yet implemented at all
run_walk "sensor" "${ENT}.6"
SENSOR_OUT="$OUTPUT/snmpwalk-sensor.txt"
skip_not_implemented "sensor: SensorTable (.6.1)"

# ---------------------------------------------------------------------------
# Summary
# ---------------------------------------------------------------------------
{
    echo "Integration test completed: $(date -u)"
    echo "Binary: $AGENTXD_BIN"
    echo "Fixtures: $(ls "$FIXTURES"/*.json | wc -l) JSON files"
    echo "Results: $PASS passed, $SKIP skipped, $FAIL failed"
    echo ""
    echo "Device indices discovered:"
    echo "  NVMe (Samsung 980 PRO):      $NVME_IDX"
    echo "  SAS  (SEAGATE ST4000NM0023): $SAS_IDX"
    echo "  SATA (WDC SELFTESTS):        $SATA_IDX"
    echo ""
    for label in common nvme sata sas sensor; do
        f="$OUTPUT/snmpwalk-${label}.txt"
        echo "--- snmpwalk-${label}.txt ---"
        cat "$f" 2>/dev/null || echo "(missing)"
        echo ""
    done
} > "$OUTPUT/integration-test-summary.txt"

echo ""
echo "Results: $PASS passed, $SKIP skipped, $FAIL failed"

if [ "$FAIL" -ne 0 ]; then
    echo "=== Integration test FAILED ==="
    exit 1
fi

echo "=== Integration test PASSED ==="
