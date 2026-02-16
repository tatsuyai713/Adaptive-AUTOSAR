#!/usr/bin/env bash
set -euo pipefail

# QNX cross-build entrypoint for middleware libraries.
# Style is aligned with lwrcl's "build_libraries_qnx.sh <target> <action>".
#
# Usage examples:
#   ./qnx/scripts/build_libraries_qnx.sh all install
#   ./qnx/scripts/build_libraries_qnx.sh cyclonedds build --arch aarch64le
#   ./qnx/scripts/build_libraries_qnx.sh vsomeip clean

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "${SCRIPT_DIR}/common.sh"

TARGET="all"   # all|boost|iceoryx|cyclonedds|vsomeip
ACTION="build" # build|install|clean

if [[ $# -gt 0 ]] && [[ "$1" != --* ]]; then
  TARGET="$1"
  shift
fi
if [[ $# -gt 0 ]] && [[ "$1" != --* ]]; then
  ACTION="$1"
  shift
fi

ARCH="$(qnx_default_arch)"
OUT_ROOT="$(qnx_default_out_root)"
JOBS="$(qnx_default_jobs)"
ENABLE_SHM="OFF"

THIRD_PARTY_PREFIX="${OUT_ROOT}/install/third_party/${ARCH}"
MW_ROOT="${OUT_ROOT}/install/middleware/${ARCH}"
BOOST_PREFIX="${THIRD_PARTY_PREFIX}"
ICEORYX_PREFIX="${MW_ROOT}/iceoryx"
CYCLONEDDS_PREFIX="${MW_ROOT}/cyclonedds"
VSOMEIP_PREFIX="${MW_ROOT}/vsomeip"
THIRD_PARTY_PREFIX_EXPLICIT="OFF"
MW_ROOT_EXPLICIT="OFF"
BOOST_PREFIX_EXPLICIT="OFF"
ICEORYX_PREFIX_EXPLICIT="OFF"
CYCLONEDDS_PREFIX_EXPLICIT="OFF"
VSOMEIP_PREFIX_EXPLICIT="OFF"

while [[ $# -gt 0 ]]; do
  case "$1" in
    --arch)
      ARCH="$2"
      shift 2
      ;;
    --out-root)
      OUT_ROOT="$2"
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
    --boost-prefix)
      BOOST_PREFIX="$2"
      BOOST_PREFIX_EXPLICIT="ON"
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
    --vsomeip-prefix)
      VSOMEIP_PREFIX="$2"
      VSOMEIP_PREFIX_EXPLICIT="ON"
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

if [[ "${THIRD_PARTY_PREFIX_EXPLICIT}" != "ON" ]]; then
  THIRD_PARTY_PREFIX="${OUT_ROOT}/install/third_party/${ARCH}"
fi
if [[ "${MW_ROOT_EXPLICIT}" != "ON" ]]; then
  MW_ROOT="${OUT_ROOT}/install/middleware/${ARCH}"
fi
if [[ "${BOOST_PREFIX_EXPLICIT}" != "ON" ]]; then
  BOOST_PREFIX="${THIRD_PARTY_PREFIX}"
fi
if [[ "${ICEORYX_PREFIX_EXPLICIT}" != "ON" ]]; then
  ICEORYX_PREFIX="${MW_ROOT}/iceoryx"
fi
if [[ "${CYCLONEDDS_PREFIX_EXPLICIT}" != "ON" ]]; then
  CYCLONEDDS_PREFIX="${MW_ROOT}/cyclonedds"
fi
if [[ "${VSOMEIP_PREFIX_EXPLICIT}" != "ON" ]]; then
  VSOMEIP_PREFIX="${MW_ROOT}/vsomeip"
fi

qnx_setup_env

qnx_info "Build QNX libraries"
qnx_info "  target=${TARGET} action=${ACTION} arch=${ARCH} jobs=${JOBS}"
qnx_info "  third_party_prefix=${THIRD_PARTY_PREFIX}"
qnx_info "  middleware_root=${MW_ROOT}"

run_boost() {
  "${SCRIPT_DIR}/build_boost_qnx.sh" "${ACTION}" \
    --arch "${ARCH}" \
    --prefix "${BOOST_PREFIX}" \
    --work-dir "${OUT_ROOT}/work/boost/${ARCH}" \
    --jobs "${JOBS}"
}

run_iceoryx() {
  "${SCRIPT_DIR}/build_iceoryx_qnx.sh" "${ACTION}" \
    --arch "${ARCH}" \
    --prefix "${ICEORYX_PREFIX}" \
    --work-dir "${OUT_ROOT}/work/iceoryx/${ARCH}" \
    --third-party-prefix "${THIRD_PARTY_PREFIX}" \
    --jobs "${JOBS}"
}

run_cyclonedds() {
  local shm_opt="--disable-shm"
  if [[ "${ENABLE_SHM}" == "ON" ]]; then
    shm_opt="--enable-shm"
  fi

  "${SCRIPT_DIR}/build_cyclonedds_qnx.sh" "${ACTION}" \
    --arch "${ARCH}" \
    --prefix "${CYCLONEDDS_PREFIX}" \
    --work-dir "${OUT_ROOT}/work/cyclonedds/${ARCH}" \
    --iceoryx-prefix "${ICEORYX_PREFIX}" \
    "${shm_opt}" \
    --jobs "${JOBS}"
}

run_vsomeip() {
  "${SCRIPT_DIR}/build_vsomeip_qnx.sh" "${ACTION}" \
    --arch "${ARCH}" \
    --prefix "${VSOMEIP_PREFIX}" \
    --work-dir "${OUT_ROOT}/work/vsomeip/${ARCH}" \
    --boost-prefix "${BOOST_PREFIX}" \
    --jobs "${JOBS}"
}

case "${TARGET}" in
  all)
    run_boost
    run_iceoryx
    run_cyclonedds
    run_vsomeip
    ;;
  boost)
    run_boost
    ;;
  iceoryx)
    run_iceoryx
    ;;
  cyclonedds)
    run_cyclonedds
    ;;
  vsomeip)
    run_boost
    run_vsomeip
    ;;
  *)
    qnx_error "Unsupported target: ${TARGET} (all|boost|iceoryx|cyclonedds|vsomeip)"
    exit 1
    ;;
esac

qnx_info "QNX middleware library workflow complete."
