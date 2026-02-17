#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "${SCRIPT_DIR}/common.sh"

ACTION="build"
if [[ $# -gt 0 ]] && [[ "$1" != --* ]]; then
  ACTION="$1"
  shift
fi

ARCH="$(qnx_default_arch)"
OUT_ROOT="$(qnx_default_out_root)"
JOBS="$(qnx_default_jobs)"
TOOLCHAIN_FILE="$(qnx_resolve_toolchain_file)"
VSOMEIP_TAG="3.4.10"

INSTALL_PREFIX="${OUT_ROOT}/vsomeip"
WORK_DIR="${AUTOSAR_QNX_WORK_ROOT:-${REPO_ROOT}/out/qnx/work}/vsomeip/${ARCH}"
BOOST_PREFIX="${OUT_ROOT}/third_party"

while [[ $# -gt 0 ]]; do
  case "$1" in
    --arch)
      ARCH="$2"
      shift 2
      ;;
    --prefix)
      INSTALL_PREFIX="$2"
      shift 2
      ;;
    --work-dir)
      WORK_DIR="$2"
      shift 2
      ;;
    --boost-prefix)
      BOOST_PREFIX="$2"
      shift 2
      ;;
    --toolchain-file)
      TOOLCHAIN_FILE="$2"
      shift 2
      ;;
    --jobs)
      JOBS="$2"
      shift 2
      ;;
    --tag)
      VSOMEIP_TAG="$2"
      shift 2
      ;;
    *)
      qnx_error "Unknown argument: $1"
      exit 1
      ;;
  esac
done

qnx_setup_env
qnx_require_cmd "cmake"

if [[ "${ACTION}" == "clean" ]]; then
  rm -rf "${WORK_DIR}"
  qnx_info "Cleaned ${WORK_DIR}"
  exit 0
fi

SOURCE_DIR="${WORK_DIR}/vsomeip"
BUILD_DIR="${WORK_DIR}/build-qnx"
BOOST_LIBDIR="${BOOST_PREFIX}/$(qnx_detect_libdir "${BOOST_PREFIX}")"

mkdir -p "${WORK_DIR}"
sudo mkdir -p "${INSTALL_PREFIX}"
sudo chmod 777 "${INSTALL_PREFIX}"

qnx_clone_or_update "https://github.com/COVESA/vsomeip.git" "${VSOMEIP_TAG}" "${SOURCE_DIR}"

cmake -S "${SOURCE_DIR}" -B "${BUILD_DIR}" \
  -DCMAKE_TOOLCHAIN_FILE="${TOOLCHAIN_FILE}" \
  -DQNX_ARCH="${ARCH}" \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX="${INSTALL_PREFIX}" \
  -DBoost_NO_BOOST_CMAKE=ON \
  -DBOOST_ROOT="${BOOST_PREFIX}" \
  -DBOOST_INCLUDEDIR="${BOOST_PREFIX}/include" \
  -DBOOST_LIBRARYDIR="${BOOST_LIBDIR}" \
  -DBUILD_SHARED_LIBS=ON \
  -DENABLE_SIGNAL_HANDLING=0 \
  -DENABLE_MULTIPLE_ROUTING_MANAGERS=1

cmake --build "${BUILD_DIR}" -j"${JOBS}"
if [[ "${ACTION}" == "install" ]]; then
  cmake --install "${BUILD_DIR}"

  mkdir -p "${INSTALL_PREFIX}/etc"
  cat > "${INSTALL_PREFIX}/etc/vsomeip-autosar-qnx.json" <<CFG
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
  "routing": "adaptive_autosar_qnx_routing",
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
fi

qnx_info "vsomeip QNX build complete (action=${ACTION})"
qnx_info "  install_prefix=${INSTALL_PREFIX}"
