#!/usr/bin/env bash
set -euo pipefail

# ===========================================================================
# build_user_apps_from_opt_qnx.sh – Cross-build user apps for QNX
# ===========================================================================
# QNX counterpart of build_user_apps_from_opt.sh.
# Cross-compiles user applications for QNX against an already-installed
# AUTOSAR AP runtime.
#
# Note: --run is not supported because the QNX binaries cannot execute on
# the host.
#
# Usage:
#   ./scripts/build_user_apps_from_opt_qnx.sh
#   ./scripts/build_user_apps_from_opt_qnx.sh --arch aarch64le
#   ./scripts/build_user_apps_from_opt_qnx.sh --prefix /opt/qnx/autosar-ap/aarch64le
# ===========================================================================

SCRIPT_DIR="$(cd "$(dirname "$(readlink -f "${BASH_SOURCE[0]}")")" && pwd)"

ARCH="aarch64le"
OUT_ROOT="/opt/qnx"
QNX_PATH="qnx800"
QNX_ENV_FILE=""
TOOLCHAIN_FILE="${SCRIPT_DIR}/cmake/qnx_toolchain.cmake"

AUTOSAR_AP_PREFIX=""
BUILD_DIR="build-qnx-user-apps-opt"
USER_APP_SOURCE_DIR=""
THIRD_PARTY_PREFIX=""

AUTOSAR_AP_PREFIX_EXPLICIT="OFF"
THIRD_PARTY_PREFIX_EXPLICIT="OFF"

_nproc="$(nproc 2>/dev/null || echo 4)"
_mem_kb="$(grep -i MemAvailable /proc/meminfo 2>/dev/null | awk '{print $2}' || echo 0)"
_mem_jobs=$(( _mem_kb / 1572864 ))
[[ "${_mem_jobs}" -lt 1 ]] && _mem_jobs=1
JOBS=$(( _nproc < _mem_jobs ? _nproc : _mem_jobs ))
[[ "${JOBS}" -lt 1 ]] && JOBS=1

usage() {
  cat <<EOF
Usage: $(basename "$0") [OPTIONS]

Cross-build user applications for QNX against an installed AUTOSAR AP runtime.

Options:
  --prefix <path>              Installed AUTOSAR AP prefix (default: <out-root>/autosar-ap/<arch>)
  --build-dir <path>           Build directory (default: ${BUILD_DIR})
  --source-dir <path>          User app source directory (default: <prefix>/user_apps)
  --arch <aarch64le|x86_64>    QNX target architecture (default: ${ARCH})
  --out-root <path>            Root install dir for QNX libs (default: ${OUT_ROOT})
  --toolchain-file <path>      QNX CMake toolchain file
  --third-party-prefix <path>  Third-party prefix (default: <out-root>/third_party)
  --qnx-path <name>            QNX SDP directory name in HOME (default: ${QNX_PATH})
  --qnx-env <file>             qnxsdp-env.sh path (overrides --qnx-path)
  --jobs <N>                   Parallel build jobs (default: auto)

  --help                       Show this help
EOF
  exit 0
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --prefix)
      AUTOSAR_AP_PREFIX="$2"; AUTOSAR_AP_PREFIX_EXPLICIT="ON"; shift 2;;
    --build-dir)
      BUILD_DIR="$2"; shift 2;;
    --source-dir)
      USER_APP_SOURCE_DIR="$2"; shift 2;;
    --arch)
      ARCH="$2"; shift 2;;
    --out-root)
      OUT_ROOT="$2"; shift 2;;
    --toolchain-file)
      TOOLCHAIN_FILE="$2"; shift 2;;
    --third-party-prefix)
      THIRD_PARTY_PREFIX="$2"; THIRD_PARTY_PREFIX_EXPLICIT="ON"; shift 2;;
    --qnx-path)
      QNX_PATH="$2"; shift 2;;
    --qnx-env)
      QNX_ENV_FILE="$2"; shift 2;;
    --jobs)
      JOBS="$2"; shift 2;;
    --help)
      usage;;
    *)
      echo "[ERROR] Unknown argument: $1" >&2
      exit 1
      ;;
  esac
done

# ---------------------------------------------------------------------------
# Resolve defaults
# ---------------------------------------------------------------------------
[[ "${AUTOSAR_AP_PREFIX_EXPLICIT}" != "ON" ]]  && AUTOSAR_AP_PREFIX="${OUT_ROOT}/autosar-ap/${ARCH}"
[[ "${THIRD_PARTY_PREFIX_EXPLICIT}" != "ON" ]] && THIRD_PARTY_PREFIX="${OUT_ROOT}/third_party"

if [[ -z "${USER_APP_SOURCE_DIR}" ]]; then
  USER_APP_SOURCE_DIR="${AUTOSAR_AP_PREFIX}/user_apps"
fi

# ---------------------------------------------------------------------------
# Setup QNX environment
# ---------------------------------------------------------------------------
if [[ -z "${QNX_HOST:-}" || -z "${QNX_TARGET:-}" ]]; then
  if [[ -z "${QNX_ENV_FILE}" ]]; then
    QNX_ENV_FILE="${HOME}/${QNX_PATH}/qnxsdp-env.sh"
  fi
  if [[ ! -f "${QNX_ENV_FILE}" ]]; then
    echo "[ERROR] QNX SDP environment script not found: ${QNX_ENV_FILE}" >&2
    echo "        Set --qnx-env or --qnx-path, or source qnxsdp-env.sh before running." >&2
    exit 1
  fi
  # shellcheck disable=SC1090
  _SAVED_SCRIPT_DIR="${SCRIPT_DIR}"; source "${QNX_ENV_FILE}"; SCRIPT_DIR="${_SAVED_SCRIPT_DIR}"
fi

if [[ -z "${QNX_HOST:-}" || -z "${QNX_TARGET:-}" ]]; then
  echo "[ERROR] QNX_HOST/QNX_TARGET are not set after sourcing environment." >&2
  exit 1
fi

export PATH="${QNX_HOST}/usr/bin:${PATH}"

if ! command -v qcc >/dev/null 2>&1 || ! command -v q++ >/dev/null 2>&1; then
  echo "[ERROR] qcc/q++ not found in PATH." >&2
  exit 1
fi

if [[ ! -f "${TOOLCHAIN_FILE}" ]]; then
  echo "[ERROR] Toolchain file not found: ${TOOLCHAIN_FILE}" >&2
  exit 1
fi

# ---------------------------------------------------------------------------
# Locate installed AUTOSAR AP package
# ---------------------------------------------------------------------------
CONFIG_PATH=""
ADAPTIVE_AUTOSAR_AP_DIR=""
for candidate_libdir in lib lib64; do
  candidate="${AUTOSAR_AP_PREFIX}/${candidate_libdir}/cmake/AdaptiveAutosarAP/AdaptiveAutosarAPConfig.cmake"
  if [[ -f "${candidate}" ]]; then
    CONFIG_PATH="${candidate}"
    ADAPTIVE_AUTOSAR_AP_DIR="${AUTOSAR_AP_PREFIX}/${candidate_libdir}/cmake/AdaptiveAutosarAP"
    break
  fi
done

if [[ -z "${CONFIG_PATH}" ]]; then
  echo "[ERROR] Installed AUTOSAR AP package was not found." >&2
  echo "        expected one of:" >&2
  echo "          ${AUTOSAR_AP_PREFIX}/lib/cmake/AdaptiveAutosarAP/AdaptiveAutosarAPConfig.cmake" >&2
  echo "          ${AUTOSAR_AP_PREFIX}/lib64/cmake/AdaptiveAutosarAP/AdaptiveAutosarAPConfig.cmake" >&2
  echo "        run scripts/build_and_install_autosar_ap_qnx.sh first." >&2
  exit 1
fi

if [[ ! -f "${USER_APP_SOURCE_DIR}/CMakeLists.txt" ]]; then
  echo "[ERROR] User app source directory not found: ${USER_APP_SOURCE_DIR}" >&2
  echo "        install template with build_and_install_autosar_ap_qnx.sh or pass --source-dir." >&2
  exit 1
fi

# ---------------------------------------------------------------------------
# Configure & build
# ---------------------------------------------------------------------------
CMAKE_PREFIX_PATH_VALUE="${AUTOSAR_AP_PREFIX};${OUT_ROOT}/cyclonedds;${OUT_ROOT}/iceoryx;${OUT_ROOT}/vsomeip;${THIRD_PARTY_PREFIX}"

# Resolve iceoryx versioned include directory for CycloneDDS SHM headers.
ICEORYX_PREFIX="${OUT_ROOT}/iceoryx"
ICEORYX_VERSIONED_INCLUDE=""
for _d in "${ICEORYX_PREFIX}/include/iceoryx"/*/; do
  if [[ -d "${_d}iceoryx_binding_c" ]]; then
    ICEORYX_VERSIONED_INCLUDE="${_d%/}"
    break
  fi
done

EXTRA_CXX_FLAGS=""
if [[ -n "${ICEORYX_VERSIONED_INCLUDE}" ]]; then
  EXTRA_CXX_FLAGS="-I${ICEORYX_VERSIONED_INCLUDE}"
fi

echo "[INFO] Building user apps against installed AUTOSAR AP (QNX ${ARCH})"
echo "       autosar_ap_prefix=${AUTOSAR_AP_PREFIX}"
echo "       cmake_config=${CONFIG_PATH}"
echo "       package_dir=${ADAPTIVE_AUTOSAR_AP_DIR}"
echo "       user_app_source_dir=${USER_APP_SOURCE_DIR}"
echo "       toolchain=${TOOLCHAIN_FILE}"
echo "       jobs=${JOBS}"

if [[ "${BUILD_DIR}" = /* ]]; then
  BUILD_DIR_ABS="${BUILD_DIR}"
else
  BUILD_DIR_ABS="$(pwd)/${BUILD_DIR}"
fi

cmake -S "${USER_APP_SOURCE_DIR}" -B "${BUILD_DIR_ABS}" \
  -DCMAKE_TOOLCHAIN_FILE="${TOOLCHAIN_FILE}" \
  -DQNX_ARCH="${ARCH}" \
  -DCMAKE_BUILD_TYPE=Release \
  -DAUTOSAR_AP_PREFIX="${AUTOSAR_AP_PREFIX}" \
  -DAdaptiveAutosarAP_DIR="${ADAPTIVE_AUTOSAR_AP_DIR}" \
  -DCMAKE_PREFIX_PATH="${CMAKE_PREFIX_PATH_VALUE}" \
  ${EXTRA_CXX_FLAGS:+-DCMAKE_CXX_FLAGS="${EXTRA_CXX_FLAGS}"}

cmake --build "${BUILD_DIR_ABS}" -j"${JOBS}"

echo "[OK] User apps built for QNX ${ARCH} in: ${BUILD_DIR_ABS}"
echo "[INFO] Executables:"
{
  find "${BUILD_DIR_ABS}" -type f -name "autosar_user_*" 2>/dev/null
  find "${BUILD_DIR_ABS}" -type f -name "ara_*_sample" 2>/dev/null
} | sort | sed 's/^/       /'
