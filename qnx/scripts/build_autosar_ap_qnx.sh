#!/usr/bin/env bash
set -euo pipefail

# QNX cross-build script for Adaptive-AUTOSAR runtime.
# Usage:
#   ./qnx/scripts/build_autosar_ap_qnx.sh install
#   ./qnx/scripts/build_autosar_ap_qnx.sh clean

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
SOURCE_DIR="${REPO_ROOT}"

INSTALL_PREFIX="${OUT_ROOT}/install/autosar_ap/${ARCH}"
BUILD_DIR="${OUT_ROOT}/build/autosar_ap-${ARCH}"

THIRD_PARTY_PREFIX="${OUT_ROOT}/install/third_party/${ARCH}"
MW_ROOT="${OUT_ROOT}/install/middleware/${ARCH}"
VSOMEIP_PREFIX="${MW_ROOT}/vsomeip"
ICEORYX_PREFIX="${MW_ROOT}/iceoryx"
CYCLONEDDS_PREFIX="${MW_ROOT}/cyclonedds"
INSTALL_PREFIX_EXPLICIT="OFF"
BUILD_DIR_EXPLICIT="OFF"
THIRD_PARTY_PREFIX_EXPLICIT="OFF"
MW_ROOT_EXPLICIT="OFF"
VSOMEIP_PREFIX_EXPLICIT="OFF"
ICEORYX_PREFIX_EXPLICIT="OFF"
CYCLONEDDS_PREFIX_EXPLICIT="OFF"

USE_VSOMEIP="ON"
USE_ICEORYX="ON"
USE_CYCLONEDDS="ON"
BUILD_PLATFORM_APP="ON"
BUILD_SAMPLES="OFF"
BUILD_TESTS="OFF"
ENABLE_ARXML_CODEGEN="OFF"

while [[ $# -gt 0 ]]; do
  case "$1" in
    --arch)
      ARCH="$2"
      shift 2
      ;;
    --source-dir)
      SOURCE_DIR="$2"
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
    --build-type)
      BUILD_TYPE="$2"
      shift 2
      ;;
    --third-party-prefix)
      THIRD_PARTY_PREFIX="$2"
      THIRD_PARTY_PREFIX_EXPLICIT="ON"
      shift 2
      ;;
    --middleware-root)
      MW_ROOT="$2"
      MW_ROOT_EXPLICIT="ON"
      shift 2
      ;;
    --vsomeip-prefix)
      VSOMEIP_PREFIX="$2"
      VSOMEIP_PREFIX_EXPLICIT="ON"
      shift 2
      ;;
    --iceoryx-prefix)
      ICEORYX_PREFIX="$2"
      ICEORYX_PREFIX_EXPLICIT="ON"
      shift 2
      ;;
    --cyclonedds-prefix)
      CYCLONEDDS_PREFIX="$2"
      CYCLONEDDS_PREFIX_EXPLICIT="ON"
      shift 2
      ;;
    --without-vsomeip)
      USE_VSOMEIP="OFF"
      shift
      ;;
    --without-iceoryx)
      USE_ICEORYX="OFF"
      shift
      ;;
    --without-cyclonedds)
      USE_CYCLONEDDS="OFF"
      shift
      ;;
    --with-samples)
      BUILD_SAMPLES="ON"
      shift
      ;;
    --with-tests)
      BUILD_TESTS="ON"
      shift
      ;;
    --with-arxml-codegen)
      ENABLE_ARXML_CODEGEN="ON"
      shift
      ;;
    --without-platform-app)
      BUILD_PLATFORM_APP="OFF"
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

if [[ "${INSTALL_PREFIX_EXPLICIT}" != "ON" ]]; then
  INSTALL_PREFIX="${OUT_ROOT}/install/autosar_ap/${ARCH}"
fi
if [[ "${BUILD_DIR_EXPLICIT}" != "ON" ]]; then
  BUILD_DIR="${OUT_ROOT}/build/autosar_ap-${ARCH}"
fi
if [[ "${THIRD_PARTY_PREFIX_EXPLICIT}" != "ON" ]]; then
  THIRD_PARTY_PREFIX="${OUT_ROOT}/install/third_party/${ARCH}"
fi
if [[ "${MW_ROOT_EXPLICIT}" != "ON" ]]; then
  MW_ROOT="${OUT_ROOT}/install/middleware/${ARCH}"
fi
if [[ "${VSOMEIP_PREFIX_EXPLICIT}" != "ON" ]]; then
  VSOMEIP_PREFIX="${MW_ROOT}/vsomeip"
fi
if [[ "${ICEORYX_PREFIX_EXPLICIT}" != "ON" ]]; then
  ICEORYX_PREFIX="${MW_ROOT}/iceoryx"
fi
if [[ "${CYCLONEDDS_PREFIX_EXPLICIT}" != "ON" ]]; then
  CYCLONEDDS_PREFIX="${MW_ROOT}/cyclonedds"
fi

qnx_setup_env
qnx_require_cmd "cmake"

if [[ ! -f "${SOURCE_DIR}/CMakeLists.txt" ]]; then
  qnx_die "CMakeLists.txt not found under source dir: ${SOURCE_DIR}"
fi

if [[ "${ACTION}" == "clean" ]]; then
  rm -rf "${BUILD_DIR}"
  qnx_info "Cleaned ${BUILD_DIR}"
  exit 0
fi

OPENSSL_ROOT_DIR="${QNX_TARGET}/${ARCH}/usr"
if [[ ! -d "${OPENSSL_ROOT_DIR}" ]]; then
  OPENSSL_ROOT_DIR="${QNX_TARGET}/usr"
fi

CMAKE_PREFIX_PATH_VALUE="${THIRD_PARTY_PREFIX};${VSOMEIP_PREFIX};${ICEORYX_PREFIX};${CYCLONEDDS_PREFIX};${ICEORYX_PREFIX}/lib/cmake;${CYCLONEDDS_PREFIX}/lib/cmake"

qnx_info "Build Adaptive-AUTOSAR for QNX"
qnx_info "  action=${ACTION} arch=${ARCH} build_type=${BUILD_TYPE}"
qnx_info "  source_dir=${SOURCE_DIR}"
qnx_info "  build_dir=${BUILD_DIR}"
qnx_info "  install_prefix=${INSTALL_PREFIX}"
qnx_info "  backends: vsomeip=${USE_VSOMEIP} iceoryx=${USE_ICEORYX} cyclonedds=${USE_CYCLONEDDS}"

cmake -S "${SOURCE_DIR}" -B "${BUILD_DIR}" \
  -DCMAKE_TOOLCHAIN_FILE="${TOOLCHAIN_FILE}" \
  -DQNX_ARCH="${ARCH}" \
  -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" \
  -DCMAKE_INSTALL_PREFIX="${INSTALL_PREFIX}" \
  -DCMAKE_PREFIX_PATH="${CMAKE_PREFIX_PATH_VALUE}" \
  -DOPENSSL_ROOT_DIR="${OPENSSL_ROOT_DIR}" \
  -DOPENSSL_INCLUDE_DIR="${QNX_TARGET}/usr/include" \
  -Dbuild_tests="${BUILD_TESTS}" \
  -DARA_COM_ENABLE_ARXML_CODEGEN="${ENABLE_ARXML_CODEGEN}" \
  -DAUTOSAR_AP_BUILD_PLATFORM_APP="${BUILD_PLATFORM_APP}" \
  -DAUTOSAR_AP_BUILD_SAMPLES="${BUILD_SAMPLES}" \
  -DARA_COM_USE_VSOMEIP="${USE_VSOMEIP}" \
  -DARA_COM_USE_ICEORYX="${USE_ICEORYX}" \
  -DARA_COM_USE_CYCLONEDDS="${USE_CYCLONEDDS}" \
  -DVSOMEIP_PREFIX="${VSOMEIP_PREFIX}" \
  -DICEORYX_PREFIX="${ICEORYX_PREFIX}" \
  -DCYCLONEDDS_PREFIX="${CYCLONEDDS_PREFIX}"

cmake --build "${BUILD_DIR}" -j"${JOBS}"
if [[ "${ACTION}" == "install" ]]; then
  cmake --install "${BUILD_DIR}"
fi

qnx_info "Adaptive-AUTOSAR QNX build complete (action=${ACTION})"
