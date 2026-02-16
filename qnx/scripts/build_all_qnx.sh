#!/usr/bin/env bash
set -euo pipefail

# One-shot QNX build pipeline:
# 1) middleware libraries
# 2) Adaptive-AUTOSAR runtime
# 3) user_apps

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "${SCRIPT_DIR}/common.sh"

ACTION="install" # build|install|clean
if [[ $# -gt 0 ]] && [[ "$1" != --* ]]; then
  ACTION="$1"
  shift
fi

ARCH="$(qnx_default_arch)"
OUT_ROOT="$(qnx_default_out_root)"
JOBS="$(qnx_default_jobs)"
ENABLE_SHM="OFF"

BUILD_LIBS="ON"
BUILD_RUNTIME="ON"
BUILD_USER_APPS="ON"

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
    --jobs)
      JOBS="$2"
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
    --without-libraries)
      BUILD_LIBS="OFF"
      shift
      ;;
    --without-runtime)
      BUILD_RUNTIME="OFF"
      shift
      ;;
    --without-user-apps)
      BUILD_USER_APPS="OFF"
      shift
      ;;
    *)
      qnx_error "Unknown argument: $1"
      exit 1
      ;;
  esac
done

qnx_setup_env

if [[ "${BUILD_LIBS}" == "ON" ]]; then
  shm_opt="--disable-shm"
  if [[ "${ENABLE_SHM}" == "ON" ]]; then
    shm_opt="--enable-shm"
  fi
  "${SCRIPT_DIR}/build_libraries_qnx.sh" all "${ACTION}" \
    --arch "${ARCH}" \
    --out-root "${OUT_ROOT}" \
    --jobs "${JOBS}" \
    "${shm_opt}"
fi

if [[ "${BUILD_RUNTIME}" == "ON" ]]; then
  "${SCRIPT_DIR}/build_autosar_ap_qnx.sh" "${ACTION}" \
    --arch "${ARCH}" \
    --out-root "${OUT_ROOT}" \
    --jobs "${JOBS}"
fi

if [[ "${BUILD_USER_APPS}" == "ON" ]]; then
  "${SCRIPT_DIR}/build_user_apps_qnx.sh" "${ACTION}" \
    --arch "${ARCH}" \
    --out-root "${OUT_ROOT}" \
    --jobs "${JOBS}"
fi

qnx_info "QNX full build pipeline complete."
