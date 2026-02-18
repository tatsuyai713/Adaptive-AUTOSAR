#!/usr/bin/env bash
set -euo pipefail

# QNX vsomeip cross-build script.
#
# Uses upstream COVESA/vsomeip and applies a QNX support patch via
# qnx/patches/apply_vsomeip_qnx_support.py (based on the vsomeip-qnx
# reference fork by lixiaolia).
#
# Key QNX adaptations applied by the patcher:
#   - Adds -lsocket to OS_CXX_FLAGS
#   - Disables DLT / systemd
#   - Uses static Boost libraries
#   - Removes rt library dependency
#
# Usage:
#   ./qnx/scripts/build_vsomeip_qnx.sh install --arch aarch64le
#   ./qnx/scripts/build_vsomeip_qnx.sh clean

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
VSOMEIP_TAG="3.6.1"

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
qnx_require_cmd "python3"

if [[ "${ACTION}" == "clean" ]]; then
  rm -rf "${WORK_DIR}"
  qnx_info "Cleaned ${WORK_DIR}"
  exit 0
fi

SOURCE_DIR="${WORK_DIR}/vsomeip"
BUILD_DIR="${WORK_DIR}/build-qnx"
BOOST_LIBDIR="${BOOST_PREFIX}/lib"

mkdir -p "${WORK_DIR}"
mkdir -p "${INSTALL_PREFIX}"
chmod 777 "${INSTALL_PREFIX}"

qnx_info "Build vsomeip for QNX"
qnx_info "  tag=${VSOMEIP_TAG} arch=${ARCH}"
qnx_info "  prefix=${INSTALL_PREFIX} boost=${BOOST_PREFIX}"

qnx_clone_or_update "https://github.com/COVESA/vsomeip.git" "${VSOMEIP_TAG}" "${SOURCE_DIR}"

# Apply QNX support patch (idempotent)
python3 "${QNX_ROOT}/patches/apply_vsomeip_qnx_support.py" "${SOURCE_DIR}/CMakeLists.txt"

cmake -S "${SOURCE_DIR}" -B "${BUILD_DIR}" \
  -DCMAKE_TOOLCHAIN_FILE="${TOOLCHAIN_FILE}" \
  -DQNX_ARCH="${ARCH}" \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX="${INSTALL_PREFIX}" \
  -DBoost_FOUND=TRUE \
  -DBoost_DIR="${BOOST_PREFIX}" \
  -DBoost_INCLUDE_DIR="${BOOST_PREFIX}/include" \
  -DBoost_LIBRARY_DIR="${BOOST_LIBDIR}" \
  -DBOOST_LIBRARYDIR="${BOOST_LIBDIR}" \
  -DCONFIG_DIR="${SOURCE_DIR}" \
  -DBUILD_SHARED_LIBS=OFF \
  -DENABLE_SIGNAL_HANDLING=OFF \
  -DDISABLE_DLT=ON \
  -DCMAKE_EXE_LINKER_FLAGS="-lsocket" \
  -DCMAKE_CXX_FLAGS="-DSA_RESTART=0x0040"

cmake --build "${BUILD_DIR}" -j"${JOBS}"

if [[ "${ACTION}" == "install" ]]; then
  cmake --install "${BUILD_DIR}"
fi

# Install default QNX config
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

qnx_info "vsomeip QNX build complete (action=${ACTION})"
qnx_info "  install_prefix=${INSTALL_PREFIX}"
