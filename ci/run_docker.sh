#!/bin/bash
# ci/run_docker.sh — build the agentxd Docker image and run the integration test
#
# Usage (from repo root):
#   ci/run_docker.sh [--no-cache] [--tag TAG]
#
# Mounts:
#   .tmp/source/  → /fixtures  (read-only fixture JSON files)
#   .tmp/test/    → /output    (snmpwalk output written here)
#
# After a successful run, check .tmp/test/ for:
#   snmpwalk-common.txt, snmpwalk-nvme.txt, snmpwalk-sata.txt, snmpwalk-sas.txt
#   integration-test-summary.txt

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
IMAGE_TAG="smartmon-agentxd-test:local"
DOCKERFILE="$REPO_ROOT/ci/Dockerfile.agentxd"
FIXTURES_DIR="$REPO_ROOT/.tmp/source"
OUTPUT_DIR="$REPO_ROOT/.tmp/test"
BUILD_ARGS=()

# ---------------------------------------------------------------------------
# Parse arguments
# ---------------------------------------------------------------------------
while [ $# -gt 0 ]; do
    case "$1" in
        --no-cache)   BUILD_ARGS+=("--no-cache"); shift ;;
        --tag)
            if [ -z "${2-}" ] || [[ "$2" == -* ]]; then
                echo "Error: --tag requires an argument" >&2; exit 1
            fi
            IMAGE_TAG="$2"; shift 2 ;;
        -h|--help)
            echo "Usage: $0 [--no-cache] [--tag IMAGE_TAG]"
            exit 0
            ;;
        *) echo "Unknown argument: $1" >&2; exit 1 ;;
    esac
done

mkdir -p "$OUTPUT_DIR"

# ---------------------------------------------------------------------------
# Build
# ---------------------------------------------------------------------------
echo "=== Building Docker image: $IMAGE_TAG ==="
docker build \
    "${BUILD_ARGS[@]}" \
    -f "$DOCKERFILE" \
    -t "$IMAGE_TAG" \
    "$REPO_ROOT"

# ---------------------------------------------------------------------------
# Run integration test
# If .tmp/source/ has JSON files, mount them to override the committed fixtures.
# Otherwise the image uses src/snmp-agentxd/tests/fixtures/ baked in at build time.
# ---------------------------------------------------------------------------
echo ""
echo "=== Running integration test ==="

DOCKER_OPTS=(--rm --cap-add NET_BIND_SERVICE -v "$OUTPUT_DIR:/output")

if [ -d "$FIXTURES_DIR" ] && [ -n "$(ls "$FIXTURES_DIR"/*.json 2>/dev/null)" ]; then
    echo "  fixtures : $FIXTURES_DIR (host mount)"
    DOCKER_OPTS+=(-v "$FIXTURES_DIR:/fixtures:ro" -e "FIXTURES=/fixtures")
else
    echo "  fixtures : committed test fixtures (inside image)"
fi
echo "  output   : $OUTPUT_DIR"

docker run "${DOCKER_OPTS[@]}" "$IMAGE_TAG"

# ---------------------------------------------------------------------------
# Print summary
# ---------------------------------------------------------------------------
echo ""
echo "=== Output files in $OUTPUT_DIR ==="
ls -lh "$OUTPUT_DIR/" 2>/dev/null || true

SUMMARY="$OUTPUT_DIR/integration-test-summary.txt"
if [ -f "$SUMMARY" ]; then
    echo ""
    echo "--- integration-test-summary.txt (last 20 lines) ---"
    tail -20 "$SUMMARY"
fi
