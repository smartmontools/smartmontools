#!/bin/bash
# scripts/install-remote.sh — Deploy smartmon-snmp-agentxd to a remote host via SSH
#
# Copies the built binary, support scripts, systemd units, and MIB files
# to a remote machine, then performs all setup (user creation, service
# installation, snmpd config) directly via SSH.
#
# Usage:
#   scripts/install-remote.sh [OPTIONS] user@host
#
# Options:
#   --prefix PREFIX      Installation prefix on remote (default: /usr)
#   --build-dir DIR      Local directory containing the built binary
#   --ssh-key FILE       SSH private key (-i FILE)
#   --port PORT          SSH port (default: 22)
#   --state-dir DIR      JSON state directory on remote
#                        (default: /run/smartmontools/json)
#   --no-collect         Skip installing the smartmon-collect service/timer
#   --dry-run            Print what would be done, without executing
#   -h, --help           Show this help
#
# Requirements:
#   Local:  rsync, ssh (or scp as fallback), built binary
#   Remote: bash, systemctl, useradd, rsync or scp (for file transfer)
#
# Examples:
#   # Deploy to ops@server01 using default build location
#   scripts/install-remote.sh ops@server01
#
#   # Deploy to a non-standard SSH port with a specific key
#   scripts/install-remote.sh --port 2222 --ssh-key ~/.ssh/ops_ed25519 ops@server01
#
#   # Dry run to preview what would happen
#   scripts/install-remote.sh --dry-run ops@server01

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

# Defaults
PREFIX="/usr"
BUILD_DIR=""
SSH_KEY=""
SSH_PORT=22
STATE_DIR="/run/smartmontools/json"
INSTALL_COLLECT=1
DRY_RUN=0
REMOTE=""

# ---------------------------------------------------------------------------
# Parse arguments
# ---------------------------------------------------------------------------
while [ $# -gt 0 ]; do
    case "$1" in
        --prefix)       PREFIX="$2";       shift 2 ;;
        --build-dir)    BUILD_DIR="$2";    shift 2 ;;
        --ssh-key)      SSH_KEY="$2";      shift 2 ;;
        --port)         SSH_PORT="$2";     shift 2 ;;
        --state-dir)    STATE_DIR="$2";    shift 2 ;;
        --no-collect)   INSTALL_COLLECT=0; shift   ;;
        --dry-run)      DRY_RUN=1;         shift   ;;
        -h|--help)
            sed -n '2,/^set -/p' "$0" | grep '^#' | sed 's/^# \{0,1\}//'
            exit 0 ;;
        -*)
            echo "ERROR: Unknown option: $1" >&2
            echo "Usage: $0 [OPTIONS] user@host" >&2
            exit 1 ;;
        *)
            if [ -n "$REMOTE" ]; then
                echo "ERROR: Multiple hosts specified; only one is allowed." >&2
                exit 1
            fi
            REMOTE="$1"
            shift ;;
    esac
done

if [ -z "$REMOTE" ]; then
    echo "ERROR: No remote host specified." >&2
    echo "Usage: $0 [OPTIONS] user@host" >&2
    exit 1
fi

# ---------------------------------------------------------------------------
# Locate the built binary
# ---------------------------------------------------------------------------
if [ -z "$BUILD_DIR" ]; then
    for candidate in \
        "$REPO_ROOT/src/snmp-agentxd/smartmon-snmp-agentxd" \
        "$REPO_ROOT/build/src/snmp-agentxd/smartmon-snmp-agentxd" \
        /build/src/snmp-agentxd/smartmon-snmp-agentxd
    do
        if [ -x "$candidate" ]; then
            BUILD_DIR="$(dirname "$candidate")"
            break
        fi
    done
fi

BINARY="${BUILD_DIR:+$BUILD_DIR/}smartmon-snmp-agentxd"
if [ ! -x "$BINARY" ]; then
    echo "ERROR: Binary not found. Build first or pass --build-dir." >&2
    exit 1
fi

# ---------------------------------------------------------------------------
# SSH / rsync helpers
# ---------------------------------------------------------------------------
SSH_OPTS=(-o StrictHostKeyChecking=accept-new -p "$SSH_PORT")
[ -n "$SSH_KEY" ] && SSH_OPTS+=(-i "$SSH_KEY")

run_ssh() {
    if [ "$DRY_RUN" -eq 1 ]; then
        echo "[DRY-RUN] ssh ${SSH_OPTS[*]} $REMOTE: $*"
    else
        ssh "${SSH_OPTS[@]}" "$REMOTE" "$@"
    fi
}

# Copies a list of local files to a remote directory (requires remote sudo).
# rsync is preferred for efficiency; falls back to scp.
copy_to_remote() {
    local remote_dir="$1"
    shift
    local files=("$@")

    if [ "$DRY_RUN" -eq 1 ]; then
        echo "[DRY-RUN] copy ${files[*]} → $REMOTE:$remote_dir"
        return 0
    fi

    # Use a temp dir on the remote that the SSH user can write without sudo
    local tmp
    tmp=$(run_ssh "mktemp -d /tmp/smartmon-deploy.XXXXXX")

    if command -v rsync &>/dev/null; then
        rsync -az -e "ssh ${SSH_OPTS[*]}" "${files[@]}" "$REMOTE:$tmp/" \
            || { run_ssh "rm -rf '$tmp'" 2>/dev/null || true; return 1; }
    else
        scp "${SSH_OPTS[@]/#-p/-P}" "${files[@]}" "$REMOTE:$tmp/" \
            || { run_ssh "rm -rf '$tmp'" 2>/dev/null || true; return 1; }
    fi

    # Move from the writable temp dir into the final destination as root
    local names=()
    for f in "${files[@]}"; do names+=("$(basename "$f")"); done
    run_ssh "sudo mkdir -p '$remote_dir' && sudo mv ${names[*]/#/$tmp/} '$remote_dir/'"
    run_ssh "rm -rf '$tmp'"
}

# ---------------------------------------------------------------------------
# Pre-flight: verify the binary links against the system net-snmp.
# If it resolves to /usr/local, the binary was built against a custom
# net-snmp and will fail on a clean target.  Abort with a clear message.
# ---------------------------------------------------------------------------
NON_SYSTEM_LIBS=$(ldd "$BINARY" 2>/dev/null | awk '{print $3}' | grep -E '^/usr/local' || true)
if [ -n "$NON_SYSTEM_LIBS" ]; then
    echo "" >&2
    echo "ERROR: binary links against non-system net-snmp libraries:" >&2
    echo "$NON_SYSTEM_LIBS" >&2
    echo "" >&2
    echo "Rebuild the binary against the system net-snmp:" >&2
    echo "  apt-get install libsnmp-dev" >&2
    echo "  ./configure && make" >&2
    echo "Then re-run this script." >&2
    exit 1
fi

# ---------------------------------------------------------------------------
# Assemble the list of files to deploy
# ---------------------------------------------------------------------------
AGENTXD_SRC="$REPO_ROOT/src/snmp-agentxd"

BINARIES=("$BINARY")
[ "$INSTALL_COLLECT" -eq 1 ] && BINARIES+=("$AGENTXD_SRC/smartmon-collect")

SERVICE_FILES=("$AGENTXD_SRC/smartmon-snmp-agentxd.service.in")
[ "$INSTALL_COLLECT" -eq 1 ] && SERVICE_FILES+=(
    "$AGENTXD_SRC/smartmon-collect.service"
    "$AGENTXD_SRC/smartmon-collect.timer"
)

MIB_FILES=("$REPO_ROOT"/doc/SMARTMON-*.mib)

echo "=== Remote deployment: smartmon-snmp-agentxd ==="
echo "  target       : $REMOTE"
echo "  prefix       : $PREFIX"
echo "  state_dir    : $STATE_DIR"
echo "  ssh port     : $SSH_PORT"
echo "  binary       : $BINARY"
echo "  install poll : $([ "$INSTALL_COLLECT" -eq 1 ] && echo yes || echo no)"
[ "$DRY_RUN" -eq 1 ] && echo "  *** DRY RUN — no changes will be made ***"
echo ""

# ---------------------------------------------------------------------------
# Pre-flight: verify we can reach the remote
# ---------------------------------------------------------------------------
if [ "$DRY_RUN" -eq 0 ]; then
    echo "--- checking remote connectivity ---"
    if ! run_ssh "true" 2>/dev/null; then
        echo "ERROR: Cannot connect to $REMOTE" >&2
        exit 1
    fi
    echo "  connected"
fi

# ---------------------------------------------------------------------------
# Copy files to remote
# ---------------------------------------------------------------------------
echo "--- copying files ---"

SBINDIR="$PREFIX/sbin"
SYSTEMD_DIR="/etc/systemd/system"
MIB_DIR="/usr/share/snmp/mibs"

copy_to_remote "$SBINDIR" "${BINARIES[@]}"
copy_to_remote "$SYSTEMD_DIR" "${SERVICE_FILES[@]}"
[ ${#MIB_FILES[@]} -gt 0 ] && copy_to_remote "$MIB_DIR" "${MIB_FILES[@]}"

# ---------------------------------------------------------------------------
# Remote configuration and service setup
# ---------------------------------------------------------------------------
echo "--- configuring remote ---"

run_ssh bash -s << REMOTE_SCRIPT
set -euo pipefail

SBINDIR="$SBINDIR"
SYSCONFDIR="/etc"
STATE_DIR="$STATE_DIR"
INSTALL_COLLECT="$INSTALL_COLLECT"
SYSTEMD_DIR="$SYSTEMD_DIR"

# ---- Install net-snmp runtime package ------------------------------------
if command -v apt-get &>/dev/null 2>/dev/null; then
    sudo apt-get install -y --no-install-recommends libsnmp40 snmp 2>/dev/null \
    || sudo apt-get install -y --no-install-recommends libsnmp40t64 snmp 2>/dev/null \
    || true
elif command -v dnf &>/dev/null 2>/dev/null; then
    sudo dnf install -y net-snmp-libs 2>/dev/null || true
elif command -v yum &>/dev/null 2>/dev/null; then
    sudo yum install -y net-snmp-libs 2>/dev/null || true
fi

# ---- Create dedicated system user ----------------------------------------
if ! id smartmon &>/dev/null 2>&1; then
    sudo useradd --system --no-create-home --shell /usr/sbin/nologin \
        --comment "smartmon-snmp-agentxd daemon" smartmon
    echo "  created user: smartmon"
else
    echo "  user already exists: smartmon"
fi

# Detect the group snmpd uses for the AgentX socket (distro-dependent).
# Debian/Ubuntu use 'Debian-snmp'; RHEL/Fedora use 'snmp'.
SNMP_GROUP=""
for g in Debian-snmp snmp; do
    if getent group "\$g" &>/dev/null; then
        SNMP_GROUP="\$g"
        break
    fi
done

# Add smartmon to the snmpd group so it can write to /var/agentx/master.
if [ -n "\$SNMP_GROUP" ]; then
    sudo usermod -aG "\$SNMP_GROUP" smartmon
    echo "  added smartmon to \$SNMP_GROUP group (for AgentX socket access)"
else
    echo "  WARNING: no snmpd group found (Debian-snmp / snmp); add smartmon manually" >&2
fi

# ---- Create and secure the state directory --------------------------------
sudo mkdir -p "\$STATE_DIR"
sudo chown root:smartmon "\$STATE_DIR"
# Owner (root) can write; group (smartmon) can read; others: nothing
sudo chmod 750 "\$STATE_DIR"
echo "  state dir: \$STATE_DIR (mode 750, root:smartmon)"

# ---- Substitute @variables@ in the service file --------------------------
AGENTXD_SVC="\$SYSTEMD_DIR/smartmon-snmp-agentxd.service"
if [ -f "\$AGENTXD_SVC.in" ]; then
    sudo sed \
        -e "s|@sbindir@|\$SBINDIR|g" \
        -e "s|@sysconfdir@|\$SYSCONFDIR|g" \
        "\$AGENTXD_SVC.in" | sudo tee "\$AGENTXD_SVC" > /dev/null
    sudo rm -f "\$AGENTXD_SVC.in"
fi

# Patch state dir into the collect service if different from default
if [ "\$INSTALL_COLLECT" = "1" ]; then
    COLLECT_SVC="\$SYSTEMD_DIR/smartmon-collect.service"
    if [ -f "\$COLLECT_SVC" ] && [ "\$STATE_DIR" != "/run/smartmontools/json" ]; then
        sudo sed -i "s|/run/smartmontools/json|\$STATE_DIR|g" "\$COLLECT_SVC"
    fi
fi

# ---- Default config file --------------------------------------------------
CONF_FILE="\$SYSCONFDIR/smartmontools/snmp-agentxd.conf"
sudo mkdir -p "\$(dirname "\$CONF_FILE")"
if [ ! -f "\$CONF_FILE" ]; then
    sudo tee "\$CONF_FILE" > /dev/null <<EOF
# smartmon-snmp-agentxd configuration
# See: man smartmon-snmp-agentxd

# Directory containing JSON state files written by smartmon-collect
# (or by smartd --jsonstate)
state_dir       \$STATE_DIR

# AgentX socket — must match snmpd's agentxsocket setting
agentx_socket   /var/agentx/master

# How long (seconds) before a device entry is considered stale
# cache_timeout  300
EOF
    echo "  created default config: \$CONF_FILE"
else
    echo "  config already exists (not overwritten): \$CONF_FILE"
fi

# ---- Harden snmpd.conf ----------------------------------------------------
SNMPD_CONF="\$(find /etc -name snmpd.conf 2>/dev/null | head -1)"
if [ -n "\$SNMPD_CONF" ]; then
    NEED_SNMPD_RESTART=0

    AGENTX_GROUP="\${SNMP_GROUP:-snmp}"
    if ! grep -qE "^[[:space:]]*master[[:space:]]+agentx" "\$SNMPD_CONF"; then
        printf '\n# Added by install-remote.sh\nmaster agentx\nagentxsocket /var/agentx/master\nagentxperms 0660 0550 root %s\n' "\$AGENTX_GROUP" | sudo tee -a "\$SNMPD_CONF" > /dev/null
        echo "  \$SNMPD_CONF: added master agentx + agentxperms (group: \$AGENTX_GROUP)"
        NEED_SNMPD_RESTART=1
    elif ! grep -qE "^[[:space:]]*agentxperms[[:space:]]" "\$SNMPD_CONF"; then
        printf '\nagentxperms 0660 0550 root %s\n' "\$AGENTX_GROUP" | sudo tee -a "\$SNMPD_CONF" > /dev/null
        echo "  \$SNMPD_CONF: added agentxperms 0660 0550 root \$AGENTX_GROUP"
        NEED_SNMPD_RESTART=1
    else
        echo "  \$SNMPD_CONF: AgentX already configured"
    fi

    [ "\$NEED_SNMPD_RESTART" -eq 1 ] && sudo systemctl restart snmpd && echo "  restarted snmpd"

    # Fix /var/agentx/ directory: snmpd may leave it drwx------ (root:root).
    # Subagents need execute permission on the directory to connect to the socket.
    AGENTX_DIR="/var/agentx"
    if [ -d "\$AGENTX_DIR" ]; then
        sudo chown "root:\$AGENTX_GROUP" "\$AGENTX_DIR"
        sudo chmod 750 "\$AGENTX_DIR"
        echo "  \$AGENTX_DIR: set root:\$AGENTX_GROUP 750"
    fi
fi

# ---- Reload and enable services ------------------------------------------
sudo systemctl daemon-reload

if [ "\$INSTALL_COLLECT" = "1" ]; then
    sudo systemctl enable smartmon-collect.timer
    sudo systemctl start  smartmon-collect.timer
    # Run once immediately so data is available before the first timer tick
    sudo systemctl start  smartmon-collect.service
    echo "  enabled and started: smartmon-collect.timer"
fi

sudo systemctl enable smartmon-snmp-agentxd.service
sudo systemctl restart smartmon-snmp-agentxd.service
echo "  enabled and (re)started: smartmon-snmp-agentxd.service"

echo ""
echo "  Deployment complete.  Verify with:"
echo "    snmpwalk -v2c -c public localhost 1.3.6.1.4.1.99999.2"
REMOTE_SCRIPT

echo ""
echo "=== Deployment finished ==="
