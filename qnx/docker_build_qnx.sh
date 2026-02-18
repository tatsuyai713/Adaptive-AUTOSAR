#!/usr/bin/env bash
# Quick QNX build helper for Docker container.
#
# This script sets up the QNX SDP environment and invokes the
# appropriate build script. Run inside the Docker container where
# QNX SDP 8.0 is installed at ~/qnx800.
#
# Usage (inside Docker container):
#   cd ~/host_home/repos/Adaptive-AUTOSAR
#   ./qnx/docker_build_qnx.sh [target] [action] [arch]
#
# Examples:
#   ./qnx/docker_build_qnx.sh all                     # Build & install all for aarch64le
#   ./qnx/docker_build_qnx.sh boost install            # Build & install Boost
#   ./qnx/docker_build_qnx.sh cyclonedds build x86_64  # Build CycloneDDS for x86_64
#   ./qnx/docker_build_qnx.sh all clean                # Clean all build artifacts

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

TARGET="${1:-all}"
ACTION="${2:-install}"
ARCH="${3:-aarch64le}"

# Source QNX environment
if [[ -f "${HOME}/qnx800/qnxsdp-env.sh" ]]; then
  echo "[INFO] Sourcing QNX SDK environment from ~/qnx800/qnxsdp-env.sh"
  source "${HOME}/qnx800/qnxsdp-env.sh"
else
  echo "[ERROR] QNX SDK not found at ~/qnx800/qnxsdp-env.sh" >&2
  exit 1
fi

# Source project QNX environment
export AUTOSAR_QNX_ARCH="${ARCH}"
if [[ -f "${SCRIPT_DIR}/env/qnx800.env" ]]; then
  source "${SCRIPT_DIR}/env/qnx800.env"
fi

cd "${REPO_ROOT}"

case "${TARGET}" in
  boost)
    echo "[INFO] Building Boost for QNX ${ARCH}"
    qnx/scripts/build_boost_qnx.sh "${ACTION}"
    ;;
  iceoryx)
    echo "[INFO] Building iceoryx for QNX ${ARCH}"
    qnx/scripts/build_iceoryx_qnx.sh "${ACTION}"
    ;;
  cyclonedds)
    echo "[INFO] Building CycloneDDS for QNX ${ARCH}"
    qnx/scripts/build_cyclonedds_qnx.sh "${ACTION}"
    ;;
  vsomeip)
    echo "[INFO] Building vsomeip for QNX ${ARCH}"
    qnx/scripts/build_vsomeip_qnx.sh "${ACTION}"
    ;;
  libraries)
    echo "[INFO] Building all third-party libraries for QNX ${ARCH}"
    qnx/scripts/build_libraries_qnx.sh all "${ACTION}"
    ;;
  autosar)
    echo "[INFO] Building AUTOSAR AP for QNX ${ARCH}"
    qnx/scripts/build_autosar_ap_qnx.sh "${ACTION}"
    ;;
  all)
    echo "[INFO] Building full stack for QNX ${ARCH}"
    qnx/scripts/build_all_qnx.sh "${ACTION}"
    ;;
  *)
    echo "[ERROR] Unknown target: ${TARGET}" >&2
    echo "Supported targets: boost, iceoryx, cyclonedds, vsomeip, libraries, autosar, all" >&2
    exit 1
    ;;
esac

echo "[OK] QNX build complete: ${TARGET} (${ACTION}, ${ARCH})"
