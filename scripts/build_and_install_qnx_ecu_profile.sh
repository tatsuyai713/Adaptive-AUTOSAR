#!/usr/bin/env bash
set -euo pipefail

# ===========================================================================
# build_and_install_qnx_ecu_profile.sh – Full QNX ECU profile build & install
# ===========================================================================
# QNX counterpart of build_and_install_rpi_ecu_profile.sh.
# Orchestrates the full QNX cross-build pipeline:
#   1. Build and install the AUTOSAR AP runtime for QNX
#   2. Build user applications against the installed runtime
#   3. Copy deployment and configuration assets
#
# Usage:
#   ./scripts/build_and_install_qnx_ecu_profile.sh
#   ./scripts/build_and_install_qnx_ecu_profile.sh --arch aarch64le --install-middleware
# ===========================================================================

SCRIPT_DIR="$(cd "$(dirname "$(readlink -f "${BASH_SOURCE[0]}")")" && pwd)"
REPO_ROOT_DEFAULT="$(cd "${SCRIPT_DIR}/.." && pwd)"

SOURCE_DIR="${REPO_ROOT_DEFAULT}"
ARCH="aarch64le"
OUT_ROOT="/opt/qnx"
QNX_PATH="qnx800"
QNX_ENV_FILE=""
TOOLCHAIN_FILE="${SCRIPT_DIR}/cmake/qnx_toolchain.cmake"

INSTALL_PREFIX=""
RUNTIME_BUILD_DIR="build-qnx-ecu-autosar-ap"
USER_APP_BUILD_DIR=""
BUILD_TYPE="Release"
BUILD_USER_APPS="ON"
INSTALL_DEPLOYMENT_ASSETS="ON"
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
USER_APP_BUILD_DIR_EXPLICIT="OFF"
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

usage() {
  cat <<EOF
Usage: $(basename "$0") [OPTIONS]

Cross-build the full QNX ECU profile (runtime + user apps + deployment).

Options:
  --source-dir <path>          Source root (default: repository root)
  --prefix <path>              Install prefix (default: <out-root>/autosar-ap/<arch>)
  --runtime-build-dir <name>   Runtime build directory (default: ${RUNTIME_BUILD_DIR})
  --user-app-build-dir <path>  User app build directory (default: <prefix>/user_apps_build)
  --build-type <type>          CMake build type (default: ${BUILD_TYPE})
  --arch <aarch64le|x86_64>    QNX target architecture (default: ${ARCH})
  --out-root <path>            Root install dir for QNX libs (default: ${OUT_ROOT})
  --toolchain-file <path>      QNX CMake toolchain file
  --qnx-path <name>            QNX SDP directory name in HOME (default: ${QNX_PATH})
  --qnx-env <file>             qnxsdp-env.sh path (overrides --qnx-path)
  --jobs <N>                   Parallel build jobs (default: auto)

  --skip-user-app-build        Skip user application build
  --skip-deployment-assets     Skip copying deployment/configuration assets

  --without-vsomeip            Disable vsomeip backend
  --without-iceoryx            Disable iceoryx backend
  --without-cyclonedds         Disable CycloneDDS backend
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
    --source-dir)
      SOURCE_DIR="$2"; shift 2;;
    --prefix)
      INSTALL_PREFIX="$2"; INSTALL_PREFIX_EXPLICIT="ON"; shift 2;;
    --runtime-build-dir)
      RUNTIME_BUILD_DIR="$2"; shift 2;;
    --user-app-build-dir)
      USER_APP_BUILD_DIR="$2"; USER_APP_BUILD_DIR_EXPLICIT="ON"; shift 2;;
    --build-type)
      BUILD_TYPE="$2"; shift 2;;
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
    --skip-user-app-build)
      BUILD_USER_APPS="OFF"; shift;;
    --skip-deployment-assets)
      INSTALL_DEPLOYMENT_ASSETS="OFF"; shift;;
    --without-vsomeip)
      USE_VSOMEIP="OFF"; shift;;
    --without-iceoryx)
      USE_ICEORYX="OFF"; shift;;
    --without-cyclonedds)
      USE_CYCLONEDDS="OFF"; shift;;
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
# Resolve defaults
# ---------------------------------------------------------------------------
[[ "${INSTALL_PREFIX_EXPLICIT}" != "ON" ]]       && INSTALL_PREFIX="${OUT_ROOT}/autosar-ap/${ARCH}"
[[ "${USER_APP_BUILD_DIR_EXPLICIT}" != "ON" ]]   && USER_APP_BUILD_DIR="${INSTALL_PREFIX}/user_apps_build"
[[ "${VSOMEIP_PREFIX_EXPLICIT}" != "ON" ]]       && VSOMEIP_PREFIX="${OUT_ROOT}/vsomeip"
[[ "${ICEORYX_PREFIX_EXPLICIT}" != "ON" ]]       && ICEORYX_PREFIX="${OUT_ROOT}/iceoryx"
[[ "${CYCLONEDDS_PREFIX_EXPLICIT}" != "ON" ]]    && CYCLONEDDS_PREFIX="${OUT_ROOT}/cyclonedds"
[[ "${THIRD_PARTY_PREFIX_EXPLICIT}" != "ON" ]]   && THIRD_PARTY_PREFIX="${OUT_ROOT}/third_party"

if [[ ! -f "${SOURCE_DIR}/CMakeLists.txt" ]]; then
  echo "[ERROR] CMakeLists.txt was not found in source dir: ${SOURCE_DIR}" >&2
  exit 1
fi

echo "[INFO] QNX ECU profile build started"
echo "       source_dir=${SOURCE_DIR}"
echo "       install_prefix=${INSTALL_PREFIX}"
echo "       arch=${ARCH}  build_type=${BUILD_TYPE}  jobs=${JOBS}"
echo "       runtime_build_dir=${RUNTIME_BUILD_DIR}"
echo "       user_app_build_dir=${USER_APP_BUILD_DIR}"
echo "       backends: vsomeip=${USE_VSOMEIP} iceoryx=${USE_ICEORYX} cyclonedds=${USE_CYCLONEDDS}"
echo "       build_platform_app=${BUILD_PLATFORM_APP}"
echo "       build_user_apps=${BUILD_USER_APPS} install_deployment_assets=${INSTALL_DEPLOYMENT_ASSETS}"

# ---------------------------------------------------------------------------
# Step 1: Build & install AUTOSAR AP runtime for QNX
# ---------------------------------------------------------------------------
runtime_args=(
  --source-dir "${SOURCE_DIR}"
  --prefix "${INSTALL_PREFIX}"
  --build-dir "${RUNTIME_BUILD_DIR}"
  --build-type "${BUILD_TYPE}"
  --arch "${ARCH}"
  --out-root "${OUT_ROOT}"
  --toolchain-file "${TOOLCHAIN_FILE}"
  --jobs "${JOBS}"
  --vsomeip-prefix "${VSOMEIP_PREFIX}"
  --iceoryx-prefix "${ICEORYX_PREFIX}"
  --cyclonedds-prefix "${CYCLONEDDS_PREFIX}"
  --third-party-prefix "${THIRD_PARTY_PREFIX}"
)

if [[ -n "${QNX_ENV_FILE}" ]]; then
  runtime_args+=(--qnx-env "${QNX_ENV_FILE}")
else
  runtime_args+=(--qnx-path "${QNX_PATH}")
fi

if [[ "${USE_VSOMEIP}" == "OFF" ]]; then
  runtime_args+=(--without-vsomeip)
fi
if [[ "${USE_ICEORYX}" == "OFF" ]]; then
  runtime_args+=(--without-iceoryx)
fi
if [[ "${USE_CYCLONEDDS}" == "OFF" ]]; then
  runtime_args+=(--without-cyclonedds)
fi
if [[ "${BUILD_PLATFORM_APP}" == "ON" ]]; then
  runtime_args+=(--with-platform-app)
else
  runtime_args+=(--without-platform-app)
fi
if [[ "${INSTALL_MIDDLEWARE}" == "ON" ]]; then
  runtime_args+=(--install-middleware)
fi
if [[ "${INSTALL_BASE_DEPS}" == "ON" ]]; then
  runtime_args+=(--install-base-deps)
fi
if [[ "${SKIP_MIDDLEWARE_SYSTEM_DEPS}" == "ON" ]]; then
  runtime_args+=(--skip-middleware-system-deps)
fi
if [[ "${FORCE_MIDDLEWARE_REINSTALL}" == "ON" ]]; then
  runtime_args+=(--force-middleware-reinstall)
fi
if [[ "${ENABLE_SHM}" == "ON" ]]; then
  runtime_args+=(--enable-shm)
elif [[ "${ENABLE_SHM}" == "OFF" ]]; then
  runtime_args+=(--disable-shm)
fi

"${SCRIPT_DIR}/build_and_install_autosar_ap_qnx.sh" "${runtime_args[@]}"

# ---------------------------------------------------------------------------
# Step 2: Build user apps for QNX
# ---------------------------------------------------------------------------
if [[ "${BUILD_USER_APPS}" == "ON" ]]; then
  "${SCRIPT_DIR}/build_user_apps_from_opt_qnx.sh" \
    --prefix "${INSTALL_PREFIX}" \
    --source-dir "${INSTALL_PREFIX}/user_apps" \
    --build-dir "${USER_APP_BUILD_DIR}" \
    --arch "${ARCH}" \
    --out-root "${OUT_ROOT}" \
    --toolchain-file "${TOOLCHAIN_FILE}" \
    --jobs "${JOBS}" \
    ${QNX_ENV_FILE:+--qnx-env "${QNX_ENV_FILE}"} \
    ${QNX_ENV_FILE:---qnx-path "${QNX_PATH}"}
fi

# ---------------------------------------------------------------------------
# Step 3: Copy deployment & configuration assets
# ---------------------------------------------------------------------------
if [[ "${INSTALL_DEPLOYMENT_ASSETS}" == "ON" ]]; then
  mkdir -p "${INSTALL_PREFIX}/deployment"
  if [[ -d "${SOURCE_DIR}/deployment" ]]; then
    cp -a "${SOURCE_DIR}/deployment/." "${INSTALL_PREFIX}/deployment/"
  fi

  mkdir -p "${INSTALL_PREFIX}/configuration"
  if [[ -d "${SOURCE_DIR}/configuration" ]]; then
    cp -a "${SOURCE_DIR}/configuration/." "${INSTALL_PREFIX}/configuration/"
  fi
fi

echo "[OK] QNX ECU profile build/install completed."
echo "[INFO] Install prefix: ${INSTALL_PREFIX}"
echo "[INFO] To deploy to a QNX target, use:"
echo "       ./qnx/scripts/deploy_to_qnx_target.sh --arch ${ARCH} --out-root ${OUT_ROOT}"
