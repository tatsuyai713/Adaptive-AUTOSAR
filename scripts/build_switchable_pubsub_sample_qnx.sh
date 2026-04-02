#!/usr/bin/env bash
set -euo pipefail

# ===========================================================================
# build_switchable_pubsub_sample_qnx.sh – Cross-build switchable pub/sub for QNX
# ===========================================================================
# QNX counterpart of build_switchable_pubsub_sample.sh.
# Cross-compiles the switchable pub/sub sample user application using the
# QNX SDP 8.0 toolchain against an already-installed AUTOSAR AP runtime.
#
# Note: smoke tests (--run-smoke) are not supported because the QNX binaries
# cannot execute on the host.
#
# Usage:
#   ./scripts/build_switchable_pubsub_sample_qnx.sh
#   ./scripts/build_switchable_pubsub_sample_qnx.sh --arch aarch64le
#   ./scripts/build_switchable_pubsub_sample_qnx.sh --prefix /opt/qnx/autosar-ap/aarch64le
# ===========================================================================

SCRIPT_DIR="$(cd "$(dirname "$(readlink -f "${BASH_SOURCE[0]}")")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
SWITCHABLE_APP_DIR="${REPO_ROOT}/user_apps/src/apps/communication/switchable_pubsub"

ARCH="aarch64le"
OUT_ROOT="/opt/qnx"
QNX_PATH="qnx800"
QNX_ENV_FILE=""
TOOLCHAIN_FILE="${SCRIPT_DIR}/cmake/qnx_toolchain.cmake"

AUTOSAR_AP_PREFIX=""
CYCLONEDDS_PREFIX=""
ICEORYX_PREFIX=""
THIRD_PARTY_PREFIX=""
BUILD_DIR="${REPO_ROOT}/build-qnx-switchable-pubsub-sample"
CLEAN="OFF"

AUTOSAR_AP_PREFIX_EXPLICIT="OFF"
CYCLONEDDS_PREFIX_EXPLICIT="OFF"
ICEORYX_PREFIX_EXPLICIT="OFF"
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

Cross-build the switchable pub/sub sample application for QNX.

Options:
  --prefix <path>              Installed AUTOSAR AP prefix (default: <out-root>/autosar-ap/<arch>)
  --cyclonedds-prefix <path>   CycloneDDS install prefix (default: <out-root>/cyclonedds)
  --iceoryx-prefix <path>      iceoryx install prefix (default: <out-root>/iceoryx)
  --third-party-prefix <path>  Third-party prefix (default: <out-root>/third_party)
  --build-dir <path>           Build directory (default: ${BUILD_DIR})
  --arch <aarch64le|x86_64>    QNX target architecture (default: ${ARCH})
  --out-root <path>            Root install dir for QNX libs (default: ${OUT_ROOT})
  --toolchain-file <path>      QNX CMake toolchain file
  --qnx-path <name>            QNX SDP directory name in HOME (default: ${QNX_PATH})
  --qnx-env <file>             qnxsdp-env.sh path (overrides --qnx-path)
  --jobs <N>                   Parallel build jobs (default: auto)
  --clean                      Remove build directory before building

  --help                       Show this help
EOF
  exit 0
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --prefix)
      AUTOSAR_AP_PREFIX="$2"; AUTOSAR_AP_PREFIX_EXPLICIT="ON"; shift 2;;
    --cyclonedds-prefix)
      CYCLONEDDS_PREFIX="$2"; CYCLONEDDS_PREFIX_EXPLICIT="ON"; shift 2;;
    --iceoryx-prefix)
      ICEORYX_PREFIX="$2"; ICEORYX_PREFIX_EXPLICIT="ON"; shift 2;;
    --third-party-prefix)
      THIRD_PARTY_PREFIX="$2"; THIRD_PARTY_PREFIX_EXPLICIT="ON"; shift 2;;
    --build-dir)
      BUILD_DIR="$2"; shift 2;;
    --arch)
      ARCH="$2"; shift 2;;
    --out-root)
      OUT_ROOT="$2"; shift 2;;
    --toolchain-file)
      TOOLCHAIN_FILE="$2"; shift 2;;
    --qnx-path)
      QNX_PATH="$2"; shift 2;;
    --qnx-env)
      QNX_ENV_FILE="$2"; shift 2;;
    --jobs)
      JOBS="$2"; shift 2;;
    --clean)
      CLEAN="ON"; shift;;
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
[[ "${AUTOSAR_AP_PREFIX_EXPLICIT}" != "ON" ]]   && AUTOSAR_AP_PREFIX="${OUT_ROOT}/autosar-ap/${ARCH}"
[[ "${CYCLONEDDS_PREFIX_EXPLICIT}" != "ON" ]]   && CYCLONEDDS_PREFIX="${OUT_ROOT}/cyclonedds"
[[ "${ICEORYX_PREFIX_EXPLICIT}" != "ON" ]]      && ICEORYX_PREFIX="${OUT_ROOT}/iceoryx"
[[ "${THIRD_PARTY_PREFIX_EXPLICIT}" != "ON" ]]  && THIRD_PARTY_PREFIX="${OUT_ROOT}/third_party"

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
# Clean
# ---------------------------------------------------------------------------
if [[ "${CLEAN}" == "ON" ]]; then
  rm -rf "${BUILD_DIR}"
fi

# ---------------------------------------------------------------------------
# Validate inputs
# ---------------------------------------------------------------------------
if [[ ! -f "${SWITCHABLE_APP_DIR}/CMakeLists.txt" ]]; then
  echo "[ERROR] switchable user-app CMakeLists not found: ${SWITCHABLE_APP_DIR}/CMakeLists.txt" >&2
  exit 1
fi

AP_CMAKE_DIR=""
for candidate_libdir in lib lib64; do
  candidate="${AUTOSAR_AP_PREFIX}/${candidate_libdir}/cmake/AdaptiveAutosarAP/AdaptiveAutosarAPConfig.cmake"
  if [[ -f "${candidate}" ]]; then
    AP_CMAKE_DIR="${AUTOSAR_AP_PREFIX}/${candidate_libdir}/cmake/AdaptiveAutosarAP"
    break
  fi
done

if [[ -z "${AP_CMAKE_DIR}" ]]; then
  echo "[ERROR] AdaptiveAutosarAPConfig.cmake not found under ${AUTOSAR_AP_PREFIX}/{lib,lib64}/cmake/AdaptiveAutosarAP" >&2
  echo "        Run build_and_install_autosar_ap_qnx.sh first." >&2
  exit 1
fi

mkdir -p "${BUILD_DIR}"

# ---------------------------------------------------------------------------
# Host library paths for idlc (IDL compiler runs on the host, not QNX target)
# ---------------------------------------------------------------------------
# When cross-compiling, CYCLONEDDS_PREFIX points to QNX-arch libraries.
# The idlc compiler needs *host*-arch CycloneDDS/iceoryx shared libraries
# (especially libcycloneddsidlcxx.so) to generate C++ types from IDL files.
CYCLONEDDS_HOST_PREFIX="${CYCLONEDDS_HOST_PREFIX:-/opt/cyclonedds}"
ICEORYX_HOST_PREFIX="${ICEORYX_HOST_PREFIX:-/opt/iceoryx}"
export LD_LIBRARY_PATH="${CYCLONEDDS_HOST_PREFIX}/lib:${ICEORYX_HOST_PREFIX}/lib:${LD_LIBRARY_PATH:-}"

# ---------------------------------------------------------------------------
# Configure & build
# ---------------------------------------------------------------------------
CMAKE_PREFIX_PATH_VALUE="${AUTOSAR_AP_PREFIX};${CYCLONEDDS_PREFIX};${ICEORYX_PREFIX};${OUT_ROOT}/vsomeip;${THIRD_PARTY_PREFIX}"

echo "[INFO] Configure switchable pub/sub user app (QNX ${ARCH})"
echo "       app_dir=${SWITCHABLE_APP_DIR}"
echo "       build_dir=${BUILD_DIR}"
echo "       autosar_prefix=${AUTOSAR_AP_PREFIX}"
echo "       cyclonedds_prefix=${CYCLONEDDS_PREFIX}"
echo "       iceoryx_prefix=${ICEORYX_PREFIX}"
echo "       toolchain=${TOOLCHAIN_FILE}"
echo "       jobs=${JOBS}"

cmake -S "${SWITCHABLE_APP_DIR}" -B "${BUILD_DIR}" \
  -DCMAKE_TOOLCHAIN_FILE="${TOOLCHAIN_FILE}" \
  -DQNX_ARCH="${ARCH}" \
  -DCMAKE_BUILD_TYPE=Release \
  -DAUTOSAR_AP_PREFIX="${AUTOSAR_AP_PREFIX}" \
  -DCYCLONEDDS_PREFIX="${CYCLONEDDS_PREFIX}" \
  -DICEORYX_PREFIX="${ICEORYX_PREFIX}" \
  -DAdaptiveAutosarAP_DIR="${AP_CMAKE_DIR}" \
  -DCMAKE_PREFIX_PATH="${CMAKE_PREFIX_PATH_VALUE}"

cmake --build "${BUILD_DIR}" -j"${JOBS}"

echo "[OK] Build completed (QNX ${ARCH})"
echo "[INFO] Generated artifacts:"
for artifact in \
  "${BUILD_DIR}/generated/switchable_topic_mapping_dds.yaml" \
  "${BUILD_DIR}/generated/switchable_topic_mapping_vsomeip.yaml" \
  "${BUILD_DIR}/generated/switchable_topic_mapping_iceoryx.yaml" \
  "${BUILD_DIR}/generated/switchable_manifest_dds.yaml" \
  "${BUILD_DIR}/generated/switchable_manifest_vsomeip.yaml" \
  "${BUILD_DIR}/generated/switchable_manifest_iceoryx.yaml" \
  "${BUILD_DIR}/generated/switchable_manifest_dds.arxml" \
  "${BUILD_DIR}/generated/switchable_manifest_vsomeip.arxml" \
  "${BUILD_DIR}/generated/switchable_manifest_iceoryx.arxml" \
  "${BUILD_DIR}/generated/switchable_proxy_skeleton.hpp" \
  "${BUILD_DIR}/generated/switchable_binding.hpp"; do
  if [[ -f "${artifact}" ]]; then
    echo "       ${artifact}"
  else
    echo "[WARN] missing generated artifact: ${artifact}" >&2
  fi
done

echo "[INFO] Built binaries:"
for binary in \
  "${BUILD_DIR}/autosar_switchable_pubsub_pub" \
  "${BUILD_DIR}/autosar_switchable_pubsub_sub"; do
  if [[ -f "${binary}" ]]; then
    echo "       ${binary}"
  else
    echo "[WARN] missing executable: ${binary}" >&2
  fi
done

echo "[INFO] To run on a QNX target, deploy the binaries and generated configs to the device."
