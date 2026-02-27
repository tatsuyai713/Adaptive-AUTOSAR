#!/usr/bin/env bash
set -euo pipefail

# ===========================================================================
# install_middleware_stack_qnx.sh – QNX middleware stack installer
# ===========================================================================
# Installs iceoryx, CycloneDDS (+SHM), and vsomeip (+CDR) for QNX.
# Build order: [boost] -> iceoryx -> cyclonedds -> vsomeip
#
# Usage:
#   ./scripts/install_middleware_stack_qnx.sh
#   ./scripts/install_middleware_stack_qnx.sh --arch aarch64le --enable-shm
#   ./scripts/install_middleware_stack_qnx.sh --without-vsomeip
# ===========================================================================

SCRIPT_DIR="$(cd "$(dirname "$(readlink -f "${BASH_SOURCE[0]}")")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

# ---------------------------------------------------------------------------
# Defaults
# ---------------------------------------------------------------------------
ARCH="aarch64le"
OUT_ROOT="/opt/qnx"
QNX_PATH="qnx800"
QNX_ENV_FILE=""
TOOLCHAIN_FILE="${SCRIPT_DIR}/cmake/qnx_toolchain.cmake"

USE_VSOMEIP="ON"
USE_ICEORYX="ON"
USE_CYCLONEDDS="ON"
USE_BOOST="AUTO"        # AUTO | ON | OFF  — ON forces boost build; AUTO builds only if vsomeip is ON and boost not found
ENABLE_SHM="AUTO"       # AUTO | ON | OFF
INSTALL_BASE_DEPS="OFF"
SKIP_SYSTEM_DEPS="OFF"
FORCE_REINSTALL="OFF"

_nproc="$(nproc 2>/dev/null || echo 4)"
_mem_kb="$(grep -i MemAvailable /proc/meminfo 2>/dev/null | awk '{print $2}' || echo 0)"
_mem_jobs=$(( _mem_kb / 1572864 ))
[[ "${_mem_jobs}" -lt 1 ]] && _mem_jobs=1
JOBS=$(( _nproc < _mem_jobs ? _nproc : _mem_jobs ))
[[ "${JOBS}" -lt 1 ]] && JOBS=1

ICEORYX_PREFIX_EXPLICIT="OFF"
CYCLONEDDS_PREFIX_EXPLICIT="OFF"
VSOMEIP_PREFIX_EXPLICIT="OFF"
BOOST_PREFIX_EXPLICIT="OFF"

# ---------------------------------------------------------------------------
# Argument parsing
# ---------------------------------------------------------------------------
usage() {
  cat <<EOF
Usage: $(basename "$0") [OPTIONS]

Options:
  --arch <aarch64le|x86_64>    QNX target architecture (default: aarch64le)
  --out-root <path>             Root install dir for all libs (default: /opt/qnx)
  --iceoryx-prefix <path>       iceoryx install prefix (default: <out-root>/iceoryx)
  --cyclonedds-prefix <path>    CycloneDDS install prefix (default: <out-root>/cyclonedds)
  --vsomeip-prefix <path>       vsomeip install prefix (default: <out-root>/vsomeip)
  --boost-prefix <path>         Boost install prefix (default: <out-root>/third_party)
  --toolchain-file <path>       QNX CMake toolchain file (default: scripts/cmake/qnx_toolchain.cmake)
  --qnx-path <name>             QNX SDP directory name in HOME (default: qnx800)
  --qnx-env <file>              qnxsdp-env.sh path (overrides --qnx-path)
  --jobs <N>                    Parallel build jobs (default: auto)

  --without-iceoryx             Skip iceoryx install
  --without-cyclonedds          Skip CycloneDDS install
  --without-vsomeip             Skip vsomeip install
  --with-boost                  Force Boost build even if already present
  --without-boost               Skip Boost build (vsomeip will fail if Boost is absent)

  --enable-shm                  Force CycloneDDS SHM support ON
  --disable-shm                 Disable CycloneDDS SHM support

  --install-base-deps           Install host build tools via install_dependency.sh
  --skip-system-deps            Skip apt package installation in each sub-script
  --force                       Force reinstall of all components

  --help                        Show this help
EOF
  exit 0
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --arch)
      ARCH="$2"; shift 2;;
    --out-root)
      OUT_ROOT="$2"; shift 2;;
    --iceoryx-prefix)
      ICEORYX_PREFIX="$2"; ICEORYX_PREFIX_EXPLICIT="ON"; shift 2;;
    --cyclonedds-prefix)
      CYCLONEDDS_PREFIX="$2"; CYCLONEDDS_PREFIX_EXPLICIT="ON"; shift 2;;
    --vsomeip-prefix)
      VSOMEIP_PREFIX="$2"; VSOMEIP_PREFIX_EXPLICIT="ON"; shift 2;;
    --boost-prefix)
      BOOST_PREFIX="$2"; BOOST_PREFIX_EXPLICIT="ON"; shift 2;;
    --toolchain-file)
      TOOLCHAIN_FILE="$2"; shift 2;;
    --qnx-path)
      QNX_PATH="$2"; shift 2;;
    --qnx-env)
      QNX_ENV_FILE="$2"; shift 2;;
    --jobs)
      JOBS="$2"; shift 2;;
    --without-iceoryx)
      USE_ICEORYX="OFF"; shift;;
    --without-cyclonedds)
      USE_CYCLONEDDS="OFF"; shift;;
    --without-vsomeip)
      USE_VSOMEIP="OFF"; shift;;
    --with-boost)
      USE_BOOST="ON"; shift;;
    --without-boost)
      USE_BOOST="OFF"; shift;;
    --enable-shm)
      ENABLE_SHM="ON"; shift;;
    --disable-shm)
      ENABLE_SHM="OFF"; shift;;
    --install-base-deps)
      INSTALL_BASE_DEPS="ON"; shift;;
    --skip-system-deps)
      SKIP_SYSTEM_DEPS="ON"; shift;;
    --force)
      FORCE_REINSTALL="ON"; shift;;
    --help)
      usage;;
    *)
      echo "[ERROR] Unknown argument: $1" >&2
      exit 1
      ;;
  esac
done

# Resolve prefixes that depend on OUT_ROOT (only if not set explicitly)
[[ "${ICEORYX_PREFIX_EXPLICIT}" != "ON" ]]    && ICEORYX_PREFIX="${OUT_ROOT}/iceoryx"
[[ "${CYCLONEDDS_PREFIX_EXPLICIT}" != "ON" ]] && CYCLONEDDS_PREFIX="${OUT_ROOT}/cyclonedds"
[[ "${VSOMEIP_PREFIX_EXPLICIT}" != "ON" ]]    && VSOMEIP_PREFIX="${OUT_ROOT}/vsomeip"
[[ "${BOOST_PREFIX_EXPLICIT}" != "ON" ]]      && BOOST_PREFIX="${OUT_ROOT}/third_party"

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
# Optional: install host build tools
# ---------------------------------------------------------------------------
if [[ "${INSTALL_BASE_DEPS}" == "ON" ]]; then
  "${SCRIPT_DIR}/install_dependency.sh"
fi

# ---------------------------------------------------------------------------
# Print configuration
# ---------------------------------------------------------------------------
echo "[INFO] QNX middleware stack install"
echo "       arch=${ARCH}  out_root=${OUT_ROOT}  jobs=${JOBS}"
echo "       iceoryx=${USE_ICEORYX} (${ICEORYX_PREFIX})"
echo "       cyclonedds=${USE_CYCLONEDDS} (${CYCLONEDDS_PREFIX})  shm=${ENABLE_SHM}"
echo "       vsomeip=${USE_VSOMEIP} (${VSOMEIP_PREFIX})"
echo "       boost prefix=${BOOST_PREFIX}  use_boost=${USE_BOOST}"
echo "       toolchain=${TOOLCHAIN_FILE}"

# ---------------------------------------------------------------------------
# Common argument arrays
# ---------------------------------------------------------------------------
common_args=(
  --arch            "${ARCH}"
  --toolchain-file  "${TOOLCHAIN_FILE}"
  --jobs            "${JOBS}"
)
if [[ "${SKIP_SYSTEM_DEPS}" == "ON" ]]; then
  common_args+=(--skip-system-deps)
fi
if [[ "${FORCE_REINSTALL}" == "ON" ]]; then
  common_args+=(--force)
fi

# Resolve QNX env passthrough (the sub-scripts will re-source if needed,
# but since we already exported QNX_HOST/QNX_TARGET they will detect them)

# ---------------------------------------------------------------------------
# Step 1: Boost (required for vsomeip)
# ---------------------------------------------------------------------------
run_boost() {
  local boost_build_script="${REPO_ROOT}/qnx/scripts/build_boost_qnx.sh"
  if [[ ! -f "${boost_build_script}" ]]; then
    echo "[ERROR] Boost build script not found: ${boost_build_script}" >&2
    exit 1
  fi
  "${boost_build_script}" install \
    --arch   "${ARCH}" \
    --prefix "${BOOST_PREFIX}" \
    --jobs   "${JOBS}"
}

boost_needed="OFF"
if [[ "${USE_VSOMEIP}" == "ON" ]]; then
  boost_needed="ON"
fi

if [[ "${USE_BOOST}" == "ON" ]]; then
  echo "[INFO] Building Boost for QNX..."
  run_boost
elif [[ "${USE_BOOST}" == "AUTO" && "${boost_needed}" == "ON" ]]; then
  # Build boost only if not already present
  if [[ ! -d "${BOOST_PREFIX}/include/boost" ]]; then
    echo "[INFO] Boost not found at ${BOOST_PREFIX}/include/boost – building Boost for QNX..."
    run_boost
  else
    echo "[INFO] Boost already found at ${BOOST_PREFIX}/include/boost – skipping."
  fi
fi

# ---------------------------------------------------------------------------
# Step 2: iceoryx
# ---------------------------------------------------------------------------
if [[ "${USE_ICEORYX}" == "ON" ]]; then
  echo "[INFO] Installing iceoryx for QNX..."
  "${SCRIPT_DIR}/install_iceoryx_qnx.sh" install \
    --prefix "${ICEORYX_PREFIX}" \
    "${common_args[@]}"
fi

# ---------------------------------------------------------------------------
# Step 3: CycloneDDS
# ---------------------------------------------------------------------------
if [[ "${USE_CYCLONEDDS}" == "ON" ]]; then
  echo "[INFO] Installing CycloneDDS for QNX..."

  cyclonedds_args=(
    --prefix          "${CYCLONEDDS_PREFIX}"
    --iceoryx-prefix  "${ICEORYX_PREFIX}"
    "${common_args[@]}"
  )

  # SHM mode
  if [[ "${ENABLE_SHM}" == "ON" ]]; then
    cyclonedds_args+=(--enable-shm)
  elif [[ "${ENABLE_SHM}" == "OFF" ]]; then
    cyclonedds_args+=(--disable-shm)
  elif [[ "${USE_ICEORYX}" == "OFF" ]]; then
    # iceoryx was skipped, so SHM cannot be used
    cyclonedds_args+=(--disable-shm --skip-iceoryx)
  fi
  # AUTO: install_cyclonedds_qnx.sh will auto-detect iceoryx

  "${SCRIPT_DIR}/install_cyclonedds_qnx.sh" install \
    "${cyclonedds_args[@]}"
fi

# ---------------------------------------------------------------------------
# Step 4: vsomeip + CDR
# ---------------------------------------------------------------------------
if [[ "${USE_VSOMEIP}" == "ON" ]]; then
  echo "[INFO] Installing vsomeip + CDR for QNX..."
  "${SCRIPT_DIR}/install_vsomeip_qnx.sh" install \
    --prefix        "${VSOMEIP_PREFIX}" \
    --boost-prefix  "${BOOST_PREFIX}" \
    "${common_args[@]}"
fi

echo "[OK] QNX middleware stack installation complete."
echo "     iceoryx    : ${ICEORYX_PREFIX}"
echo "     cyclonedds : ${CYCLONEDDS_PREFIX}"
echo "     vsomeip    : ${VSOMEIP_PREFIX}"
