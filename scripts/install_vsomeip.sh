#!/usr/bin/env bash
set -euo pipefail

# Based on lwrcl install flow, adapted for this repository.

VSOMEIP_TAG="3.4.10"
INSTALL_PREFIX="/opt/vsomeip"
BUILD_DIR="${HOME}/build-vsomeip"
_nproc="$(nproc 2>/dev/null || echo 4)"
_mem_kb="$(grep -i MemAvailable /proc/meminfo 2>/dev/null | awk '{print $2}' || echo 0)"
_mem_jobs=$(( _mem_kb / 1572864 ))
[[ "${_mem_jobs}" -lt 1 ]] && _mem_jobs=1
JOBS=$(( _nproc < _mem_jobs ? _nproc : _mem_jobs ))
[[ "${JOBS}" -lt 1 ]] && JOBS=1
SKIP_SYSTEM_DEPS="OFF"
FORCE_REINSTALL="OFF"

while [[ $# -gt 0 ]]; do
  case "$1" in
    --prefix)
      INSTALL_PREFIX="$2"
      shift 2
      ;;
    --tag)
      VSOMEIP_TAG="$2"
      shift 2
      ;;
    --build-dir)
      BUILD_DIR="$2"
      shift 2
      ;;
    --jobs)
      JOBS="$2"
      shift 2
      ;;
    --skip-system-deps)
      SKIP_SYSTEM_DEPS="ON"
      shift
      ;;
    --force)
      FORCE_REINSTALL="ON"
      shift
      ;;
    *)
      echo "[ERROR] Unknown argument: $1" >&2
      exit 1
      ;;
  esac
done

if [[ "${FORCE_REINSTALL}" != "ON" ]] && [[ -f "${INSTALL_PREFIX}/lib/libvsomeip3.so" || -f "${INSTALL_PREFIX}/lib64/libvsomeip3.so" ]]; then
  echo "[INFO] vsomeip already installed at ${INSTALL_PREFIX}. Skipping (use --force to reinstall)."
  exit 0
fi

SUDO=""
if [[ "${EUID}" -ne 0 ]]; then
  if command -v sudo >/dev/null 2>&1; then
    SUDO="sudo"
  else
    echo "[ERROR] Please run as root or install sudo." >&2
    exit 1
  fi
fi

if [[ "${SKIP_SYSTEM_DEPS}" != "ON" ]] && command -v apt-get >/dev/null 2>&1; then
  ${SUDO} apt-get update -qq
  ${SUDO} apt-get install -y --no-install-recommends \
    libboost-system-dev \
    libboost-thread-dev \
    libboost-log-dev \
    libboost-filesystem-dev \
    libboost-program-options-dev
fi

${SUDO} mkdir -p "${INSTALL_PREFIX}"

rm -rf "${BUILD_DIR}"
mkdir -p "${BUILD_DIR}"

git clone --depth 1 --branch "${VSOMEIP_TAG}" https://github.com/COVESA/vsomeip.git "${BUILD_DIR}/vsomeip"

cmake -S "${BUILD_DIR}/vsomeip" -B "${BUILD_DIR}/vsomeip-build" \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX="${INSTALL_PREFIX}" \
  -DBUILD_SHARED_LIBS=ON \
  -DENABLE_SIGNAL_HANDLING=1 \
  -DENABLE_MULTIPLE_ROUTING_MANAGERS=1

cmake --build "${BUILD_DIR}/vsomeip-build" -j"${JOBS}"
${SUDO} cmake --install "${BUILD_DIR}/vsomeip-build"

${SUDO} mkdir -p "${INSTALL_PREFIX}/etc"
${SUDO} tee "${INSTALL_PREFIX}/etc/vsomeip-autosar.json" >/dev/null << 'CFG'
{
  "unicast": "127.0.0.1",
  "logging": {
    "level": "info",
    "console": "true",
    "file": { "enable": "false" },
    "dlt": "false"
  },
  "applications": [],
  "services": [],
  "routing": "adaptive_autosar_routing",
  "service-discovery": {
    "enable": "true",
    "multicast": "224.244.224.245",
    "port": "30490",
    "protocol": "udp",
    "initial_delay_min": "10",
    "initial_delay_max": "100",
    "repetitions_base_delay": "200",
    "repetitions_max": "3",
    "ttl": "3",
    "cyclic_offer_delay": "2000",
    "request_response_delay": "1500"
  }
}
CFG

echo "[OK] Installed vsomeip ${VSOMEIP_TAG} to ${INSTALL_PREFIX}"
