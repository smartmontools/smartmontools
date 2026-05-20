#!/bin/bash
# scripts/install-agentxd.sh — Deploy smartmon-snmp-agentxd to the local system
#
# Installs the binary, smartmon-collect script, systemd units, config file,
# and MIB files.  Must be run as root (or with sudo).
#
# Usage:
#   sudo scripts/install-agentxd.sh [OPTIONS]
#
# Options:
#   --prefix PREFIX      Installation prefix (default: /usr)
#   --build-dir DIR      Directory containing the built binary
#   --state-dir DIR      JSON state directory (default: /run/smartmontools/json)
#   --no-collect         Skip installing the smartmon-collect service/timer
#   -h, --help           Show this help
#
# After installation the agent can be started with:
#   systemctl enable --now smartmon-collect.timer smartmon-snmp-agentxd
#
# To use smartd --jsonstate instead of smartmon-collect, add to smartd.conf:
#   DEVICESCAN -x -a -s (S/../.././02|L/../../6/03)
# and set state_dir in /etc/smartmontools/snmp-agentxd.conf.
# Note: DEVICESCAN with -x requires smartd >= 7.0.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

PREFIX="/usr"
BUILD_DIR=""
STATE_DIR="/run/smartmontools/json"
INSTALL_COLLECT=1

# ---------------------------------------------------------------------------
# Parse arguments
# ---------------------------------------------------------------------------
while [ $# -gt 0 ]; do
    case "$1" in
        --prefix)      PREFIX="$2";      shift 2 ;;
        --build-dir)   BUILD_DIR="$2";   shift 2 ;;
        --state-dir)   STATE_DIR="$2";   shift 2 ;;
        --no-collect)  INSTALL_COLLECT=0; shift  ;;
        -h|--help)
            sed -n '2,/^set -/p' "$0" | grep '^#' | sed 's/^# \{0,1\}//'
            exit 0 ;;
        *) echo "Unknown argument: $1" >&2; exit 1 ;;
    esac
done

# ---------------------------------------------------------------------------
# Require root
# ---------------------------------------------------------------------------
if [ "$(id -u)" -ne 0 ]; then
    echo "ERROR: This script must be run as root." >&2
    echo "  sudo $0 $*" >&2
    exit 1
fi

# ---------------------------------------------------------------------------
# Locate binary
# ---------------------------------------------------------------------------
if [ -z "$BUILD_DIR" ]; then
    for candidate in \
        "$REPO_ROOT/src/snmp-agentxd/smartmon-snmp-agentxd" \
        "$REPO_ROOT/build/src/snmp-agentxd/smartmon-snmp-agentxd" \
        /build/src/snmp-agentxd/smartmon-snmp-agentxd
    do
        if [ -x "$candidate" ]; then
            BINARY="$candidate"
            BUILD_DIR="$(dirname "$candidate")"
            break
        fi
    done
else
    BINARY="$BUILD_DIR/smartmon-snmp-agentxd"
    [ -x "$BINARY" ] || BINARY="$BUILD_DIR/src/snmp-agentxd/smartmon-snmp-agentxd"
fi

if [ -z "${BINARY:-}" ] || [ ! -x "$BINARY" ]; then
    echo "ERROR: Binary not found. Build first or set --build-dir." >&2
    exit 1
fi

SBINDIR="${PREFIX}/sbin"
SYSCONFDIR="/etc"
UNITDIR="/lib/systemd/system"
MIBDIR="/usr/share/snmp/mibs"
CONFDIR="$SYSCONFDIR/smartmontools"
AGENTXD_SRC="$REPO_ROOT/src/snmp-agentxd"

echo "=== Installing smartmon-snmp-agentxd ==="
echo "  binary      : $BINARY"
echo "  prefix      : $PREFIX"
echo "  state_dir   : $STATE_DIR"
echo "  collect svc : $([ "$INSTALL_COLLECT" -eq 1 ] && echo yes || echo no)"
echo ""

# ---------------------------------------------------------------------------
# Dedicated system user
# ---------------------------------------------------------------------------
echo "--- creating system user ---"
if ! id smartmon &>/dev/null 2>&1; then
    useradd --system --no-create-home --shell /usr/sbin/nologin \
        --comment "smartmon-snmp-agentxd daemon" smartmon
    echo "  created user: smartmon"
else
    echo "  user already exists: smartmon"
fi

# Detect the group snmpd uses for the AgentX socket (distro-dependent).
# Debian/Ubuntu use 'Debian-snmp'; RHEL/Fedora use 'snmp'.
SNMP_GROUP=""
for g in Debian-snmp snmp; do
    if getent group "$g" &>/dev/null; then
        SNMP_GROUP="$g"
        break
    fi
done

# Add smartmon to the snmpd group so it can write to /var/agentx/master.
if [ -n "$SNMP_GROUP" ]; then
    usermod -aG "$SNMP_GROUP" smartmon
    echo "  added smartmon to $SNMP_GROUP group (for AgentX socket access)"
else
    echo "  WARNING: no snmpd group found (Debian-snmp / snmp); add smartmon manually" >&2
fi

# ---------------------------------------------------------------------------
# State directory (writable by root/collect, readable by smartmon user)
# ---------------------------------------------------------------------------
echo "--- creating state directory ---"
install -d -m 750 -o root -g smartmon "$STATE_DIR"
echo "  $STATE_DIR (mode 750, root:smartmon)"

# ---------------------------------------------------------------------------
# Binaries
# ---------------------------------------------------------------------------
echo "--- installing binaries ---"
install -d "$SBINDIR"
install -m 755 "$BINARY" "$SBINDIR/smartmon-snmp-agentxd"
echo "  $SBINDIR/smartmon-snmp-agentxd"

if [ "$INSTALL_COLLECT" -eq 1 ]; then
    COLLECT_SCRIPT="$AGENTXD_SRC/smartmon-collect"
    if [ -f "$COLLECT_SCRIPT" ]; then
        install -m 755 "$COLLECT_SCRIPT" "$SBINDIR/smartmon-collect"
        echo "  $SBINDIR/smartmon-collect"
    else
        echo "  WARNING: smartmon-collect not found at $COLLECT_SCRIPT" >&2
        INSTALL_COLLECT=0
    fi
fi

# ---------------------------------------------------------------------------
# Runtime library dependencies
# The binary must be built against the system net-snmp (libsnmp-dev),
# which links to the distro-provided libnetsnmp.so.* in /usr/lib.
# Here we just ensure the runtime package is installed.
# ---------------------------------------------------------------------------
echo "--- checking runtime libraries ---"
install_runtime_snmp() {
    if command -v apt-get &>/dev/null; then
        # libsnmp40 is the runtime package on Debian/Ubuntu (bookworm/noble)
        # libsnmp40t64 is used on Ubuntu 24.04+ with the t64 ABI transition
        apt-get install -y --no-install-recommends \
            libsnmp40 snmp 2>/dev/null \
        || apt-get install -y --no-install-recommends \
            libsnmp40t64 snmp 2>/dev/null \
        || true
    elif command -v dnf &>/dev/null; then
        dnf install -y net-snmp-libs 2>/dev/null || true
    elif command -v yum &>/dev/null; then
        yum install -y net-snmp-libs 2>/dev/null || true
    fi
}

if ldd "$BINARY" 2>/dev/null | grep -q "not found"; then
    echo "  installing net-snmp runtime package..."
    install_runtime_snmp
    if ldd "$BINARY" 2>/dev/null | grep -q "not found"; then
        echo ""
        echo "ERROR: missing runtime libraries after package install:" >&2
        ldd "$BINARY" 2>/dev/null | grep "not found" >&2
        echo "" >&2
        echo "The binary must be built against the system net-snmp (libsnmp-dev)." >&2
        echo "Rebuild with:  ./configure && make" >&2
        echo "Ensure libsnmp-dev is installed before building." >&2
        exit 1
    fi
fi
echo "  runtime libraries OK"

# ---------------------------------------------------------------------------
# Man page (if built)
# ---------------------------------------------------------------------------
for candidate in \
    "$REPO_ROOT/doc/smartmon-snmp-agentxd.8" \
    "$(dirname "$BINARY")/../../doc/smartmon-snmp-agentxd.8"
do
    if [ -f "$candidate" ]; then
        MANDIR="${PREFIX}/share/man/man8"
        install -d "$MANDIR"
        install -m 644 "$candidate" "$MANDIR/smartmon-snmp-agentxd.8"
        echo "  $MANDIR/smartmon-snmp-agentxd.8"
        break
    fi
done

# ---------------------------------------------------------------------------
# Config file (do not overwrite existing)
# ---------------------------------------------------------------------------
echo "--- installing config ---"
install -d "$CONFDIR"
CONF_DEST="$CONFDIR/snmp-agentxd.conf"
if [ -f "$CONF_DEST" ]; then
    echo "  $CONF_DEST already exists — not overwriting"
else
    cat > "$CONF_DEST" <<EOF
# smartmon-snmp-agentxd configuration
# See: man smartmon-snmp-agentxd

# Directory containing JSON state files written by smartmon-collect
# (or by smartd --jsonstate)
state_dir       $STATE_DIR

# AgentX socket — must match snmpd's agentxsocket setting
agentx_socket   /var/agentx/master

# How long (seconds) before a device entry is considered stale
# cache_timeout  300
EOF
    chmod 640 "$CONF_DEST"
    chown root:smartmon "$CONF_DEST"
    echo "  $CONF_DEST (new)"
fi

# ---------------------------------------------------------------------------
# MIB files
# ---------------------------------------------------------------------------
echo "--- installing MIB files ---"
install -d "$MIBDIR"
for mib in "$REPO_ROOT"/doc/SMARTMON-*.mib; do
    [ -f "$mib" ] || continue
    install -m 644 "$mib" "$MIBDIR/"
    echo "  $MIBDIR/$(basename "$mib")"
done

# ---------------------------------------------------------------------------
# systemd units
# ---------------------------------------------------------------------------
echo "--- installing systemd units ---"
install -d "$UNITDIR"

# agentxd service (substitute @variables@)
UNIT_SRC="$AGENTXD_SRC/smartmon-snmp-agentxd.service.in"
UNIT_DEST="$UNITDIR/smartmon-snmp-agentxd.service"
if [ -f "$UNIT_SRC" ]; then
    sed \
        -e "s|@sbindir@|${SBINDIR}|g" \
        -e "s|@sysconfdir@|${SYSCONFDIR}|g" \
        "$UNIT_SRC" > "$UNIT_DEST"
    # Patch the state_dir path in the ReadOnlyPaths line
    sed -i "s|/run/smartmontools/json|${STATE_DIR}|g" "$UNIT_DEST"
    chmod 644 "$UNIT_DEST"
    echo "  $UNIT_DEST"
fi

# collect service + timer
if [ "$INSTALL_COLLECT" -eq 1 ]; then
    for unit in smartmon-collect.service smartmon-collect.timer; do
        src="$AGENTXD_SRC/$unit"
        [ -f "$src" ] || continue
        dest="$UNITDIR/$unit"
        install -m 644 "$src" "$dest"
        # Patch state_dir if non-default
        [ "$STATE_DIR" != "/run/smartmontools/json" ] && \
            sed -i "s|/run/smartmontools/json|${STATE_DIR}|g" "$dest"
        echo "  $dest"
    done
fi

systemctl daemon-reload

# ---------------------------------------------------------------------------
# snmpd AgentX configuration
# ---------------------------------------------------------------------------
SNMPD_CONF=""
for candidate in /etc/snmp/snmpd.conf /etc/snmpd/snmpd.conf; do
    [ -f "$candidate" ] && SNMPD_CONF="$candidate" && break
done
if [ -n "$SNMPD_CONF" ]; then
    NEED_SNMPD_RESTART=0

    AGENTX_GROUP="${SNMP_GROUP:-snmp}"
    if ! grep -qE "^[[:space:]]*master[[:space:]]+agentx" "$SNMPD_CONF"; then
        printf '\n# Added by install-agentxd.sh\nmaster agentx\nagentxsocket /var/agentx/master\nagentxperms 0660 0550 root %s\n' "$AGENTX_GROUP" >> "$SNMPD_CONF"
        echo "  $SNMPD_CONF: added master agentx + agentxperms (group: $AGENTX_GROUP)"
        NEED_SNMPD_RESTART=1
    elif ! grep -qE "^[[:space:]]*agentxperms[[:space:]]" "$SNMPD_CONF"; then
        printf '\nagentxperms 0660 0550 root %s\n' "$AGENTX_GROUP" >> "$SNMPD_CONF"
        echo "  $SNMPD_CONF: added agentxperms 0660 0550 root $AGENTX_GROUP"
        NEED_SNMPD_RESTART=1
    else
        echo "  $SNMPD_CONF: AgentX already configured"
    fi

    [ "$NEED_SNMPD_RESTART" -eq 1 ] && systemctl restart snmpd && echo "  restarted snmpd"

    # Fix /var/agentx/ directory: snmpd may leave it drwx------ (root:root).
    # Subagents need execute permission on the directory to connect to the socket.
    AGENTX_DIR="/var/agentx"
    if [ -d "$AGENTX_DIR" ]; then
        chown "root:$AGENTX_GROUP" "$AGENTX_DIR"
        chmod 750 "$AGENTX_DIR"
        echo "  $AGENTX_DIR: set root:$AGENTX_GROUP 750"
    fi
fi

# ---------------------------------------------------------------------------
# Done
# ---------------------------------------------------------------------------
echo ""
echo "=== Installation complete ==="
echo ""
if [ "$INSTALL_COLLECT" -eq 1 ]; then
    echo "To use smartmon-collect (recommended — no smartd --jsonstate needed):"
    echo "  systemctl enable --now smartmon-collect.timer"
    echo "  systemctl enable --now smartmon-snmp-agentxd"
    echo ""
    echo "To use smartd --jsonstate instead:"
    echo "  Add to /etc/smartd.conf:  DEVICESCAN -x -a (requires smartd >= 7.0)"
    echo "  Set state_dir in $CONF_DEST to match --jsonstate path"
    echo "  Then: systemctl enable --now smartmon-snmp-agentxd"
else
    echo "  systemctl enable --now smartmon-snmp-agentxd"
fi
echo ""
echo "Verify:"
echo '  snmpwalk -v2c -c public localhost 1.3.6.1.4.1.99999.2'
echo '  snmpwalk -v2c -c public -m ALL localhost SMARTMON-COMMON-MIB::smartmonDeviceTable'
echo ""
echo "MIBs installed to $MIBDIR"
