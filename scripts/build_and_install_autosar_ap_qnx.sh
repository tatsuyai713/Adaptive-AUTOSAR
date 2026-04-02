#!/usr/bin/env bash
set -euo pipefail

# ===========================================================================
# build_and_install_autosar_ap_qnx.sh – Cross-build & install AUTOSAR AP for QNX
# ===========================================================================
# QNX counterpart of build_and_install_autosar_ap.sh.
# Cross-compiles the Adaptive-AUTOSAR runtime using the QNX SDP 8.0 toolchain
# and installs it into the specified prefix (/opt/qnx/autosar-ap by default).
#
# The QNX middleware stack (iceoryx, CycloneDDS, vsomeip) must already be
# installed under /opt/qnx (or wherever --out-root points), or you can pass
# --install-middleware to build them first via install_middleware_stack_qnx.sh.
#
# Usage:
#   ./scripts/build_and_install_autosar_ap_qnx.sh
#   ./scripts/build_and_install_autosar_ap_qnx.sh --arch aarch64le
#   ./scripts/build_and_install_autosar_ap_qnx.sh --install-middleware
# ===========================================================================

SCRIPT_DIR="$(cd "$(dirname "$(readlink -f "${BASH_SOURCE[0]}")")" && pwd)"
REPO_ROOT_DEFAULT="$(cd "${SCRIPT_DIR}/.." && pwd)"
REPO_ROOT="${REPO_ROOT_DEFAULT}"

# ---------------------------------------------------------------------------
# Defaults
# ---------------------------------------------------------------------------
ARCH="aarch64le"
OUT_ROOT="/opt/qnx"
QNX_PATH="qnx800"
QNX_ENV_FILE=""
TOOLCHAIN_FILE="${SCRIPT_DIR}/cmake/qnx_toolchain.cmake"

INSTALL_PREFIX=""  # derived from OUT_ROOT/ARCH below
BUILD_DIR="build-qnx-install-autosar-ap"
BUILD_TYPE="Release"
USE_VSOMEIP="ON"
USE_ICEORYX="ON"
USE_CYCLONEDDS="ON"
BUILD_PLATFORM_APP="ON"
INSTALL_MIDDLEWARE="OFF"
INSTALL_BASE_DEPS="OFF"
SKIP_MIDDLEWARE_SYSTEM_DEPS="OFF"
FORCE_MIDDLEWARE_REINSTALL="OFF"
ENABLE_SHM="AUTO"
VSOMEIP_PREFIX=""
ICEORYX_PREFIX=""
CYCLONEDDS_PREFIX=""
THIRD_PARTY_PREFIX=""

INSTALL_PREFIX_EXPLICIT="OFF"
VSOMEIP_PREFIX_EXPLICIT="OFF"
ICEORYX_PREFIX_EXPLICIT="OFF"
CYCLONEDDS_PREFIX_EXPLICIT="OFF"
THIRD_PARTY_PREFIX_EXPLICIT="OFF"

_nproc="$(nproc 2>/dev/null || echo 4)"
_mem_kb="$(grep -i MemAvailable /proc/meminfo 2>/dev/null | awk '{print $2}' || echo 0)"
_mem_jobs=$(( _mem_kb / 1572864 ))
[[ "${_mem_jobs}" -lt 1 ]] && _mem_jobs=1
JOBS=$(( _nproc < _mem_jobs ? _nproc : _mem_jobs ))
[[ "${JOBS}" -lt 1 ]] && JOBS=1

# ---------------------------------------------------------------------------
# Argument parsing
# ---------------------------------------------------------------------------
usage() {
  cat <<EOF
Usage: $(basename "$0") [OPTIONS]

Cross-build and install the Adaptive-AUTOSAR runtime for QNX.

Options:
  --prefix <path>              Install prefix (default: <out-root>/autosar-ap/<arch>)
  --build-dir <name>           Build directory name (default: ${BUILD_DIR})
  --build-type <type>          CMake build type (default: ${BUILD_TYPE})
  --source-dir <path>          Source root (default: repository root)
  --arch <aarch64le|x86_64>    QNX target architecture (default: ${ARCH})
  --out-root <path>            Root install dir for QNX libs (default: ${OUT_ROOT})
  --toolchain-file <path>      QNX CMake toolchain file
  --qnx-path <name>            QNX SDP directory name in HOME (default: ${QNX_PATH})
  --qnx-env <file>             qnxsdp-env.sh path (overrides --qnx-path)
  --jobs <N>                   Parallel build jobs (default: auto)

  --without-vsomeip            Disable vsomeip backend
  --without-iceoryx            Disable iceoryx backend
  --without-cyclonedds         Disable CycloneDDS backend
  --with-platform-app          Build platform applications (default)
  --without-platform-app       Skip platform applications

  --install-middleware          Build & install QNX middleware stack first
  --install-base-deps          Install host build tools before middleware
  --skip-middleware-system-deps Skip apt packages in middleware sub-scripts
  --force-middleware-reinstall  Force reinstall of middleware components
  --enable-shm                 Force CycloneDDS SHM ON
  --disable-shm                Force CycloneDDS SHM OFF

  --vsomeip-prefix <path>      vsomeip install prefix (default: <out-root>/vsomeip)
  --iceoryx-prefix <path>      iceoryx install prefix (default: <out-root>/iceoryx)
  --cyclonedds-prefix <path>   CycloneDDS install prefix (default: <out-root>/cyclonedds)
  --third-party-prefix <path>  Third-party prefix (default: <out-root>/third_party)

  --help                       Show this help
EOF
  exit 0
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --prefix)
      INSTALL_PREFIX="$2"; INSTALL_PREFIX_EXPLICIT="ON"; shift 2;;
    --build-dir)
      BUILD_DIR="$2"; shift 2;;
    --build-type)
      BUILD_TYPE="$2"; shift 2;;
    --source-dir)
      REPO_ROOT="$2"; shift 2;;
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
    --without-vsomeip)
      USE_VSOMEIP="OFF"; shift;;
    --without-iceoryx)
      USE_ICEORYX="OFF"; shift;;
    --without-cyclonedds)
      USE_CYCLONEDDS="OFF"; shift;;
    --with-platform-app)
      BUILD_PLATFORM_APP="ON"; shift;;
    --without-platform-app)
      BUILD_PLATFORM_APP="OFF"; shift;;
    --install-middleware)
      INSTALL_MIDDLEWARE="ON"; shift;;
    --install-base-deps)
      INSTALL_MIDDLEWARE="ON"; INSTALL_BASE_DEPS="ON"; shift;;
    --skip-middleware-system-deps)
      SKIP_MIDDLEWARE_SYSTEM_DEPS="ON"; shift;;
    --force-middleware-reinstall)
      FORCE_MIDDLEWARE_REINSTALL="ON"; shift;;
    --enable-shm)
      ENABLE_SHM="ON"; shift;;
    --disable-shm)
      ENABLE_SHM="OFF"; shift;;
    --vsomeip-prefix)
      VSOMEIP_PREFIX="$2"; VSOMEIP_PREFIX_EXPLICIT="ON"; shift 2;;
    --iceoryx-prefix)
      ICEORYX_PREFIX="$2"; ICEORYX_PREFIX_EXPLICIT="ON"; shift 2;;
    --cyclonedds-prefix)
      CYCLONEDDS_PREFIX="$2"; CYCLONEDDS_PREFIX_EXPLICIT="ON"; shift 2;;
    --third-party-prefix)
      THIRD_PARTY_PREFIX="$2"; THIRD_PARTY_PREFIX_EXPLICIT="ON"; shift 2;;
    --help)
      usage;;
    *)
      echo "[ERROR] Unknown argument: $1" >&2
      exit 1
      ;;
  esac
done

# ---------------------------------------------------------------------------
# Resolve defaults that depend on OUT_ROOT / ARCH
# ---------------------------------------------------------------------------
[[ "${INSTALL_PREFIX_EXPLICIT}" != "ON" ]]      && INSTALL_PREFIX="${OUT_ROOT}/autosar-ap/${ARCH}"
[[ "${VSOMEIP_PREFIX_EXPLICIT}" != "ON" ]]      && VSOMEIP_PREFIX="${OUT_ROOT}/vsomeip"
[[ "${ICEORYX_PREFIX_EXPLICIT}" != "ON" ]]      && ICEORYX_PREFIX="${OUT_ROOT}/iceoryx"
[[ "${CYCLONEDDS_PREFIX_EXPLICIT}" != "ON" ]]   && CYCLONEDDS_PREFIX="${OUT_ROOT}/cyclonedds"
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
# Validate source
# ---------------------------------------------------------------------------
if [[ ! -f "${REPO_ROOT}/CMakeLists.txt" ]]; then
  echo "[ERROR] CMakeLists.txt was not found in source dir: ${REPO_ROOT}" >&2
  echo "        pass the source root by --source-dir <path>." >&2
  exit 1
fi

echo "[INFO] Cross-building AUTOSAR AP for QNX"
echo "       source_dir=${REPO_ROOT}"
echo "       install_prefix=${INSTALL_PREFIX}"
echo "       build_dir=${BUILD_DIR}"
echo "       arch=${ARCH}  build_type=${BUILD_TYPE}  jobs=${JOBS}"
echo "       backends: vsomeip=${USE_VSOMEIP} iceoryx=${USE_ICEORYX} cyclonedds=${USE_CYCLONEDDS}"
echo "       build_platform_app=${BUILD_PLATFORM_APP}"
echo "       middleware prefixes: vsomeip=${VSOMEIP_PREFIX} iceoryx=${ICEORYX_PREFIX} cyclonedds=${CYCLONEDDS_PREFIX}"
echo "       toolchain=${TOOLCHAIN_FILE}"

# ---------------------------------------------------------------------------
# Optionally install QNX middleware stack
# ---------------------------------------------------------------------------
if [[ "${INSTALL_MIDDLEWARE}" == "ON" ]]; then
  middleware_args=(
    --arch "${ARCH}"
    --out-root "${OUT_ROOT}"
    --toolchain-file "${TOOLCHAIN_FILE}"
    --jobs "${JOBS}"
    --vsomeip-prefix "${VSOMEIP_PREFIX}"
    --iceoryx-prefix "${ICEORYX_PREFIX}"
    --cyclonedds-prefix "${CYCLONEDDS_PREFIX}"
  )
  if [[ "${USE_VSOMEIP}" == "OFF" ]]; then
    middleware_args+=(--without-vsomeip)
  fi
  if [[ "${USE_ICEORYX}" == "OFF" ]]; then
    middleware_args+=(--without-iceoryx)
  fi
  if [[ "${USE_CYCLONEDDS}" == "OFF" ]]; then
    middleware_args+=(--without-cyclonedds)
  fi
  if [[ "${INSTALL_BASE_DEPS}" == "ON" ]]; then
    middleware_args+=(--install-base-deps)
  fi
  if [[ "${SKIP_MIDDLEWARE_SYSTEM_DEPS}" == "ON" ]]; then
    middleware_args+=(--skip-system-deps)
  fi
  if [[ "${FORCE_MIDDLEWARE_REINSTALL}" == "ON" ]]; then
    middleware_args+=(--force)
  fi
  if [[ "${ENABLE_SHM}" == "ON" ]]; then
    middleware_args+=(--enable-shm)
  elif [[ "${ENABLE_SHM}" == "OFF" ]]; then
    middleware_args+=(--disable-shm)
  fi
  "${SCRIPT_DIR}/install_middleware_stack_qnx.sh" "${middleware_args[@]}"
fi

# ---------------------------------------------------------------------------
# QNX 8.0: pthread is part of libc — create empty stub if needed
# ---------------------------------------------------------------------------
_QNX_ARCH_LIBDIR="${QNX_TARGET}/${ARCH}/lib"
if [[ ! -f "${_QNX_ARCH_LIBDIR}/libpthread.a" ]]; then
  echo "[INFO] Creating empty libpthread.a stub in ${_QNX_ARCH_LIBDIR}"
  _STUB_C="$(mktemp /tmp/empty_pthread_XXXXXX.c)"
  echo "/* pthread is in libc on QNX 8.0 */" > "${_STUB_C}"
  _NTO_ARCH="${ARCH%le}"; _NTO_ARCH="${_NTO_ARCH%be}"
  nto${_NTO_ARCH}-gcc -c "${_STUB_C}" -o "${_STUB_C%.c}.o"
  nto${_NTO_ARCH}-ar rcs "${_QNX_ARCH_LIBDIR}/libpthread.a" "${_STUB_C%.c}.o"
  rm -f "${_STUB_C}" "${_STUB_C%.c}.o"
fi
unset _QNX_ARCH_LIBDIR _STUB_C _NTO_ARCH

# ---------------------------------------------------------------------------
# OpenSSL root detection
# ---------------------------------------------------------------------------
OPENSSL_ROOT_DIR="${QNX_TARGET}/${ARCH}/usr"
if [[ ! -d "${OPENSSL_ROOT_DIR}" ]]; then
  OPENSSL_ROOT_DIR="${QNX_TARGET}/usr"
fi

# ---------------------------------------------------------------------------
# Configure & build
# ---------------------------------------------------------------------------
# CycloneDDS must appear before vsomeip in CMAKE_PREFIX_PATH because the
# vsomeip install ships a CDR-only stub CycloneDDSConfig.cmake that lacks
# the real dds/dds.h headers.
CMAKE_PREFIX_PATH_VALUE="${THIRD_PARTY_PREFIX};${CYCLONEDDS_PREFIX};${CYCLONEDDS_PREFIX}/lib/cmake;${ICEORYX_PREFIX};${ICEORYX_PREFIX}/lib/cmake;${VSOMEIP_PREFIX}"

cmake -S "${REPO_ROOT}" -B "${REPO_ROOT}/${BUILD_DIR}" \
  -DCMAKE_TOOLCHAIN_FILE="${TOOLCHAIN_FILE}" \
  -DQNX_ARCH="${ARCH}" \
  -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" \
  -DCMAKE_INSTALL_PREFIX="${INSTALL_PREFIX}" \
  -DCMAKE_PREFIX_PATH="${CMAKE_PREFIX_PATH_VALUE}" \
  -DCycloneDDS_DIR="${CYCLONEDDS_PREFIX}/lib/cmake/CycloneDDS" \
  -DCycloneDDS-CXX_DIR="${CYCLONEDDS_PREFIX}/lib/cmake/CycloneDDS-CXX" \
  -DOPENSSL_ROOT_DIR="${OPENSSL_ROOT_DIR}" \
  -DOPENSSL_INCLUDE_DIR="${QNX_TARGET}/usr/include" \
  -Dbuild_tests=OFF \
  -DARA_COM_ENABLE_ARXML_CODEGEN=OFF \
  -DAUTOSAR_AP_BUILD_PLATFORM_APP="${BUILD_PLATFORM_APP}" \
  -DAUTOSAR_AP_BUILD_SAMPLES=OFF \
  -DARA_COM_USE_VSOMEIP="${USE_VSOMEIP}" \
  -DARA_COM_USE_ICEORYX="${USE_ICEORYX}" \
  -DARA_COM_USE_CYCLONEDDS="${USE_CYCLONEDDS}" \
  -DVSOMEIP_PREFIX="${VSOMEIP_PREFIX}" \
  -DICEORYX_PREFIX="${ICEORYX_PREFIX}" \
  -DCYCLONEDDS_PREFIX="${CYCLONEDDS_PREFIX}"

# Apply QNX-specific source patches to FetchContent dependencies.
ASYNC_BSD_SRC="${REPO_ROOT}/${BUILD_DIR}/_deps/async-bsd-socket-lib-src"
QNX_PATCHES_DIR="${REPO_ROOT}/qnx/patches"
if [[ -d "${ASYNC_BSD_SRC}" && -f "${QNX_PATCHES_DIR}/apply_async_bsd_socket_qnx_support.py" ]]; then
  echo "[INFO] Applying QNX poll() patch to async-bsd-socket-lib"
  python3 "${QNX_PATCHES_DIR}/apply_async_bsd_socket_qnx_support.py" "${ASYNC_BSD_SRC}"
fi

OBD_EMULATOR_SRC="${REPO_ROOT}/${BUILD_DIR}/_deps/obd-ii-emulator-src"
if [[ -d "${OBD_EMULATOR_SRC}" && -f "${QNX_PATCHES_DIR}/apply_obd_emulator_qnx_support.py" ]]; then
  echo "[INFO] Applying QNX uint*_t patch to OBD-II-Emulator"
  python3 "${QNX_PATCHES_DIR}/apply_obd_emulator_qnx_support.py" "${OBD_EMULATOR_SRC}"
fi

cmake --build "${REPO_ROOT}/${BUILD_DIR}" -j"${JOBS}"

# Create install prefix if it doesn't exist.
if [[ ! -d "${INSTALL_PREFIX}" ]]; then
  if mkdir -p "${INSTALL_PREFIX}" 2>/dev/null; then
    : # created without sudo
  else
    echo "[INFO] Creating ${INSTALL_PREFIX} with sudo"
    sudo mkdir -p "${INSTALL_PREFIX}"
    sudo chmod 777 "${INSTALL_PREFIX}"
  fi
fi

cmake --install "${REPO_ROOT}/${BUILD_DIR}"

echo "[OK] Installed AUTOSAR AP (QNX ${ARCH}) into: ${INSTALL_PREFIX}"
echo "[INFO] Installed package config:"
echo "       ${INSTALL_PREFIX}/lib/cmake/AdaptiveAutosarAP/AdaptiveAutosarAPConfig.cmake"
