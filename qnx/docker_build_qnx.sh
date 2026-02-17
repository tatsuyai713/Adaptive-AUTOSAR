#!/usr/bin/env bash
# Quick QNX build helper for Docker container (linuxserver-kde-tatsuyai)
#
# Usage (inside Docker container):
#   cd ~/host_home/repos/Adaptive-AUTOSAR
#   ./qnx/docker_build_qnx.sh [target] [arch]
#
# Examples:
#   ./qnx/docker_build_qnx.sh cyclonedds           # Build CycloneDDS for aarch64le
#   ./qnx/docker_build_qnx.sh iceoryx aarch64le    # Build iceoryx for ARM64
#   ./qnx/docker_build_qnx.sh all x86_64           # Build all for x86_64

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

TARGET="${1:-all}"
ARCH="${2:-aarch64le}"

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
  cyclonedds)
    echo "[INFO] Building CycloneDDS for QNX ${ARCH}"
    qnx/scripts/build_cyclonedds_qnx.sh build
    ;;
  iceoryx)
    echo "[INFO] Building iceoryx for QNX ${ARCH}"
    qnx/scripts/build_iceoryx_qnx.sh build
    ;;
  vsomeip)
    echo "[INFO] Building vsomeip for QNX ${ARCH}"
    qnx/scripts/build_vsomeip_qnx.sh build
    ;;
  boost)
    echo "[INFO] Building Boost for QNX ${ARCH}"
    qnx/scripts/build_boost_qnx.sh build
    ;;
  libraries)
    echo "[INFO] Building third-party libraries for QNX ${ARCH}"
    qnx/scripts/build_libraries_qnx.sh build
    ;;
  autosar)
    echo "[INFO] Building AUTOSAR AP for QNX ${ARCH}"
    qnx/scripts/build_autosar_ap_qnx.sh build
    ;;
  all)
    echo "[INFO] Building full stack for QNX ${ARCH}"
    qnx/scripts/build_all_qnx.sh build
    ;;
  *)
    echo "[ERROR] Unknown target: ${TARGET}" >&2
    echo "Supported targets: cyclonedds, iceoryx, vsomeip, boost, libraries, autosar, all" >&2
    exit 1
    ;;
esac

echo "[OK] QNX build complete: ${TARGET} (${ARCH})"
