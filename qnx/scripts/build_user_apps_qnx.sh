#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "${SCRIPT_DIR}/common.sh"

ACTION="build" # build|install|clean
if [[ $# -gt 0 ]] && [[ "$1" != --* ]]; then
  ACTION="$1"
  shift
fi

ARCH="$(qnx_default_arch)"
OUT_ROOT="$(qnx_default_out_root)"
JOBS="$(qnx_default_jobs)"
TOOLCHAIN_FILE="$(qnx_resolve_toolchain_file)"
BUILD_TYPE="Release"

SOURCE_DIR="${REPO_ROOT}/user_apps"
AUTOSAR_AP_PREFIX="${OUT_ROOT}/autosar_ap/${ARCH}"
BUILD_DIR="${AUTOSAR_QNX_WORK_ROOT:-${REPO_ROOT}/out/qnx/work}/build/user_apps-${ARCH}"
INSTALL_PREFIX="${OUT_ROOT}/user_apps/${ARCH}"

MW_ROOT="${OUT_ROOT}"
THIRD_PARTY_PREFIX="${OUT_ROOT}/third_party"
SOURCE_DIR_EXPLICIT="OFF"
AUTOSAR_AP_PREFIX_EXPLICIT="OFF"
BUILD_DIR_EXPLICIT="OFF"
INSTALL_PREFIX_EXPLICIT="OFF"
MW_ROOT_EXPLICIT="OFF"
THIRD_PARTY_PREFIX_EXPLICIT="OFF"

BUILD_BASIC="ON"
BUILD_COMMUNICATION="ON"
BUILD_FEATURE="ON"

while [[ $# -gt 0 ]]; do
  case "$1" in
    --arch)
      ARCH="$2"
      shift 2
      ;;
    --source-dir)
      SOURCE_DIR="$2"
      SOURCE_DIR_EXPLICIT="ON"
      shift 2
      ;;
    --autosar-prefix)
      AUTOSAR_AP_PREFIX="$2"
      AUTOSAR_AP_PREFIX_EXPLICIT="ON"
      shift 2
      ;;
    --build-dir)
      BUILD_DIR="$2"
      BUILD_DIR_EXPLICIT="ON"
      shift 2
      ;;
    --prefix)
      INSTALL_PREFIX="$2"
      INSTALL_PREFIX_EXPLICIT="ON"
      shift 2
      ;;
    --out-root)
      OUT_ROOT="$2"
      shift 2
      ;;
    --middleware-root)
      MW_ROOT="$2"
      MW_ROOT_EXPLICIT="ON"
      shift 2
      ;;
    --third-party-prefix)
      THIRD_PARTY_PREFIX="$2"
      THIRD_PARTY_PREFIX_EXPLICIT="ON"
      shift 2
      ;;
    --build-type)
      BUILD_TYPE="$2"
      shift 2
      ;;
    --without-basic)
      BUILD_BASIC="OFF"
      shift
      ;;
    --without-communication)
      BUILD_COMMUNICATION="OFF"
      shift
      ;;
    --without-feature)
      BUILD_FEATURE="OFF"
      shift
      ;;
    --jobs)
      JOBS="$2"
      shift 2
      ;;
    *)
      qnx_error "Unknown argument: $1"
      exit 1
      ;;
  esac
done

if [[ "${SOURCE_DIR_EXPLICIT}" != "ON" ]]; then
  SOURCE_DIR="${REPO_ROOT}/user_apps"
fi
if [[ "${AUTOSAR_AP_PREFIX_EXPLICIT}" != "ON" ]]; then
  AUTOSAR_AP_PREFIX="${OUT_ROOT}/autosar_ap/${ARCH}"
fi
if [[ "${BUILD_DIR_EXPLICIT}" != "ON" ]]; then
  BUILD_DIR="${AUTOSAR_QNX_WORK_ROOT:-${REPO_ROOT}/out/qnx/work}/build/user_apps-${ARCH}"
fi
if [[ "${INSTALL_PREFIX_EXPLICIT}" != "ON" ]]; then
  INSTALL_PREFIX="${OUT_ROOT}/user_apps/${ARCH}"
fi
if [[ "${MW_ROOT_EXPLICIT}" != "ON" ]]; then
  MW_ROOT="${OUT_ROOT}"
fi
if [[ "${THIRD_PARTY_PREFIX_EXPLICIT}" != "ON" ]]; then
  THIRD_PARTY_PREFIX="${OUT_ROOT}/third_party"
fi

qnx_setup_env
qnx_require_cmd "cmake"

if [[ ! -f "${SOURCE_DIR}/CMakeLists.txt" ]]; then
  qnx_die "user_apps CMakeLists.txt not found: ${SOURCE_DIR}/CMakeLists.txt"
fi

if [[ "${ACTION}" == "clean" ]]; then
  rm -rf "${BUILD_DIR}"
  qnx_info "Cleaned ${BUILD_DIR}"
  exit 0
fi

mkdir -p "${BUILD_DIR}"
mkdir -p "${INSTALL_PREFIX}"
chmod 777 "${INSTALL_PREFIX}"

CMAKE_PREFIX_PATH_VALUE="${AUTOSAR_AP_PREFIX};${MW_ROOT}/cyclonedds;${MW_ROOT}/iceoryx;${MW_ROOT}/vsomeip;${THIRD_PARTY_PREFIX}"

# iceoryx installs headers into a versioned subdir (include/iceoryx/v<X>/),
# so we must add that path explicitly so CycloneDDS's SHM transport header
# can #include "iceoryx_binding_c/chunk.h" etc.
ICEORYX_PREFIX="${MW_ROOT}/iceoryx"
ICEORYX_VERSIONED_INCLUDE=""
for _d in "${ICEORYX_PREFIX}/include/iceoryx"/*/; do
  if [[ -d "${_d}iceoryx_binding_c" ]]; then
    ICEORYX_VERSIONED_INCLUDE="${_d%/}"
    break
  fi
done

# CycloneDDS DDSCXX include: user_apps/CMakeLists.txt has hardcoded HINTS for
# /opt/cyclonedds (Linux path). Override to the QNX install via cmake variables.
CYCLONEDDS_QNX_PREFIX="${MW_ROOT}/cyclonedds"
USER_APPS_CYCLONEDDS_DDSCXX_INCLUDE_DIR="${CYCLONEDDS_QNX_PREFIX}/include/ddscxx"
USER_APPS_CYCLONEDDS_INCLUDE_DIR="${CYCLONEDDS_QNX_PREFIX}/include"

EXTRA_CXX_FLAGS=""
if [[ -n "${ICEORYX_VERSIONED_INCLUDE}" ]]; then
  EXTRA_CXX_FLAGS="-I${ICEORYX_VERSIONED_INCLUDE}"
  qnx_info "Adding iceoryx versioned include: ${ICEORYX_VERSIONED_INCLUDE}"
fi

cmake -S "${SOURCE_DIR}" -B "${BUILD_DIR}" \
  -DCMAKE_TOOLCHAIN_FILE="${TOOLCHAIN_FILE}" \
  -DQNX_ARCH="${ARCH}" \
  -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" \
  -DCMAKE_INSTALL_PREFIX="${INSTALL_PREFIX}" \
  -DAUTOSAR_AP_PREFIX="${AUTOSAR_AP_PREFIX}" \
  -DCMAKE_PREFIX_PATH="${CMAKE_PREFIX_PATH_VALUE}" \
  -DCMAKE_CXX_FLAGS="${EXTRA_CXX_FLAGS}" \
  -DUSER_APPS_CYCLONEDDS_DDSCXX_INCLUDE_DIR="${USER_APPS_CYCLONEDDS_DDSCXX_INCLUDE_DIR}" \
  -DUSER_APPS_CYCLONEDDS_INCLUDE_DIR="${USER_APPS_CYCLONEDDS_INCLUDE_DIR}" \
  -DUSER_APPS_BUILD_APPS_BASIC="${BUILD_BASIC}" \
  -DUSER_APPS_BUILD_APPS_COMMUNICATION="${BUILD_COMMUNICATION}" \
  -DUSER_APPS_BUILD_APPS_FEATURE="${BUILD_FEATURE}"

cmake --build "${BUILD_DIR}" -j"${JOBS}"
if [[ "${ACTION}" == "install" ]]; then
  cmake --install "${BUILD_DIR}"
fi

qnx_info "user_apps QNX build complete (action=${ACTION})"
