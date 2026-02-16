#!/usr/bin/env bash
set -euo pipefail

# QNX host dependency preparation script.
# - Does not use apt/yum.
# - Verifies required host commands.
# - Optionally builds host tools from source.

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "${SCRIPT_DIR}/common.sh"

CHECK_ONLY="OFF"
BUILD_HOST_TOOLS="ON"
OUT_ROOT="$(qnx_default_out_root)"
HOST_TOOLS_PREFIX="${OUT_ROOT}/host-tools"
HOST_TOOLS_WORK_DIR="${OUT_ROOT}/work/host-tools"
JOBS="$(qnx_default_jobs)"
HOST_TOOLS_PREFIX_EXPLICIT="OFF"
HOST_TOOLS_WORK_DIR_EXPLICIT="OFF"

while [[ $# -gt 0 ]]; do
  case "$1" in
    --check-only)
      CHECK_ONLY="ON"
      shift
      ;;
    --without-host-tools)
      BUILD_HOST_TOOLS="OFF"
      shift
      ;;
    --with-host-tools)
      BUILD_HOST_TOOLS="ON"
      shift
      ;;
    --out-root)
      OUT_ROOT="$2"
      shift 2
      ;;
    --host-tools-prefix)
      HOST_TOOLS_PREFIX="$2"
      HOST_TOOLS_PREFIX_EXPLICIT="ON"
      shift 2
      ;;
    --host-tools-work-dir)
      HOST_TOOLS_WORK_DIR="$2"
      HOST_TOOLS_WORK_DIR_EXPLICIT="ON"
      shift 2
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

if [[ "${HOST_TOOLS_PREFIX_EXPLICIT}" != "ON" ]]; then
  HOST_TOOLS_PREFIX="${OUT_ROOT}/host-tools"
fi
if [[ "${HOST_TOOLS_WORK_DIR_EXPLICIT}" != "ON" ]]; then
  HOST_TOOLS_WORK_DIR="${OUT_ROOT}/work/host-tools"
fi

qnx_info "Check QNX host build prerequisites"

for cmd in git curl tar make perl python3 gcc g++ cmake; do
  qnx_require_cmd "${cmd}"
done

qnx_info "Required host commands are available."

# Validate cross-compile toolchain environment as early as possible.
qnx_setup_env

if [[ "${CHECK_ONLY}" == "ON" ]]; then
  qnx_info "check-only mode complete."
  exit 0
fi

if [[ "${BUILD_HOST_TOOLS}" == "ON" ]]; then
  "${SCRIPT_DIR}/build_host_tools.sh" \
    --prefix "${HOST_TOOLS_PREFIX}" \
    --work-dir "${HOST_TOOLS_WORK_DIR}" \
    --jobs "${JOBS}"
fi

qnx_info "Dependency preparation complete."
qnx_info "If host tools were built, add this to your shell:"
echo "export PATH=${HOST_TOOLS_PREFIX}/bin:\$PATH"
