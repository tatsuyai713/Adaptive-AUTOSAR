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

CYCLONEDDS_TAG="0.10.5"
CYCLONEDDS_CXX_TAG="0.10.5"
ENABLE_SHM="OFF"
PATCH_THREADS_FOR_QNX="ON"

INSTALL_PREFIX="${OUT_ROOT}/install/middleware/${ARCH}/cyclonedds"
WORK_DIR="${OUT_ROOT}/work/cyclonedds/${ARCH}"
ICEORYX_PREFIX="${OUT_ROOT}/install/middleware/${ARCH}/iceoryx"

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
    --toolchain-file)
      TOOLCHAIN_FILE="$2"
      shift 2
      ;;
    --jobs)
      JOBS="$2"
      shift 2
      ;;
    --tag)
      CYCLONEDDS_TAG="$2"
      shift 2
      ;;
    --cxx-tag)
      CYCLONEDDS_CXX_TAG="$2"
      shift 2
      ;;
    --enable-shm)
      ENABLE_SHM="ON"
      shift
      ;;
    --disable-shm)
      ENABLE_SHM="OFF"
      shift
      ;;
    --iceoryx-prefix)
      ICEORYX_PREFIX="$2"
      shift 2
      ;;
    --without-qnx-threads-patch)
      PATCH_THREADS_FOR_QNX="OFF"
      shift
      ;;
    *)
      qnx_error "Unknown argument: $1"
      exit 1
      ;;
  esac
done

qnx_setup_env
qnx_require_cmd "cmake"
qnx_require_cmd "sed"

if [[ "${ACTION}" == "clean" ]]; then
  rm -rf "${WORK_DIR}"
  qnx_info "Cleaned ${WORK_DIR}"
  exit 0
fi

SOURCE_DIR="${WORK_DIR}/cyclonedds"
BUILD_DIR="${WORK_DIR}/build-qnx"
SOURCE_CXX_DIR="${WORK_DIR}/cyclonedds-cxx"
BUILD_CXX_DIR="${WORK_DIR}/build-qnx-cxx"

mkdir -p "${WORK_DIR}" "${INSTALL_PREFIX}"

qnx_clone_or_update "https://github.com/eclipse-cyclonedds/cyclonedds.git" "${CYCLONEDDS_TAG}" "${SOURCE_DIR}"

if [[ "${PATCH_THREADS_FOR_QNX}" == "ON" ]]; then
  if [[ -f "${SOURCE_DIR}/src/ddsrt/CMakeLists.txt" ]]; then
    sed -i.bak -E \
      's/^[[:space:]]*(find_package\(Threads REQUIRED\)|target_link_libraries\(ddsrt INTERFACE Threads::Threads\))/# &/' \
      "${SOURCE_DIR}/src/ddsrt/CMakeLists.txt"
  fi
fi

OPENSSL_ROOT_DIR_DEFAULT="${QNX_TARGET}/${ARCH}/usr"
if [[ ! -d "${OPENSSL_ROOT_DIR_DEFAULT}" ]]; then
  OPENSSL_ROOT_DIR_DEFAULT="${QNX_TARGET}/usr"
fi

cmake -S "${SOURCE_DIR}" -B "${BUILD_DIR}" \
  -DCMAKE_TOOLCHAIN_FILE="${TOOLCHAIN_FILE}" \
  -DQNX_ARCH="${ARCH}" \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX="${INSTALL_PREFIX}" \
  -DOPENSSL_ROOT_DIR="${OPENSSL_ROOT_DIR_DEFAULT}" \
  -DOPENSSL_INCLUDE_DIR="${QNX_TARGET}/usr/include" \
  -DBUILD_SHARED_LIBS=ON \
  -DENABLE_SHM="${ENABLE_SHM}" \
  -DENABLE_SOURCE_SPECIFIC_MULTICAST=OFF

cmake --build "${BUILD_DIR}" -j"${JOBS}"
if [[ "${ACTION}" == "install" ]]; then
  cmake --install "${BUILD_DIR}"
fi

qnx_clone_or_update "https://github.com/eclipse-cyclonedds/cyclonedds-cxx.git" "${CYCLONEDDS_CXX_TAG}" "${SOURCE_CXX_DIR}"

cmake -S "${SOURCE_CXX_DIR}" -B "${BUILD_CXX_DIR}" \
  -DCMAKE_TOOLCHAIN_FILE="${TOOLCHAIN_FILE}" \
  -DQNX_ARCH="${ARCH}" \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX="${INSTALL_PREFIX}" \
  -DCMAKE_PREFIX_PATH="${INSTALL_PREFIX}" \
  -DBUILD_IDLCPP_GENERATOR=ON \
  -DCYCLONEDDS_CXX_BLD_IDLCPP=ON \
  -DENABLE_TOPIC_DISCOVERY=OFF \
  -DDDSCXX_NO_STD_OPTIONAL=ON \
  -DBUILD_SHARED_LIBS=ON

cmake --build "${BUILD_CXX_DIR}" -j"${JOBS}"
if [[ "${ACTION}" == "install" ]]; then
  cmake --install "${BUILD_CXX_DIR}"
fi

qnx_info "cyclonedds QNX build complete (action=${ACTION})"
qnx_info "  install_prefix=${INSTALL_PREFIX}"
