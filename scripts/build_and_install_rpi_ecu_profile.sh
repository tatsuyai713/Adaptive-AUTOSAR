#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT_DEFAULT="$(cd "${SCRIPT_DIR}/.." && pwd)"

SOURCE_DIR="${REPO_ROOT_DEFAULT}"
INSTALL_PREFIX="/opt/autosar_ap"
RUNTIME_BUILD_DIR="build-rpi-autosar-ap"
USER_APP_BUILD_DIR="/opt/autosar_ap/user_apps_build"
BUILD_TYPE="Release"
BUILD_USER_APPS="ON"
INSTALL_DEPLOYMENT_ASSETS="ON"
RUN_VERIFY="OFF"
USE_VSOMEIP="ON"
USE_ICEORYX="ON"
USE_CYCLONEDDS="ON"

while [[ $# -gt 0 ]]; do
  case "$1" in
    --source-dir)
      SOURCE_DIR="$2"
      shift 2
      ;;
    --prefix)
      INSTALL_PREFIX="$2"
      shift 2
      ;;
    --runtime-build-dir)
      RUNTIME_BUILD_DIR="$2"
      shift 2
      ;;
    --user-app-build-dir)
      USER_APP_BUILD_DIR="$2"
      shift 2
      ;;
    --build-type)
      BUILD_TYPE="$2"
      shift 2
      ;;
    --skip-user-app-build)
      BUILD_USER_APPS="OFF"
      shift
      ;;
    --skip-deployment-assets)
      INSTALL_DEPLOYMENT_ASSETS="OFF"
      shift
      ;;
    --run-verify)
      RUN_VERIFY="ON"
      shift
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
    *)
      echo "[ERROR] Unknown argument: $1" >&2
      exit 1
      ;;
  esac
done

if [[ ! -f "${SOURCE_DIR}/CMakeLists.txt" ]]; then
  echo "[ERROR] CMakeLists.txt was not found in source dir: ${SOURCE_DIR}" >&2
  exit 1
fi

echo "[INFO] Raspberry Pi ECU profile build started"
echo "       source_dir=${SOURCE_DIR}"
echo "       install_prefix=${INSTALL_PREFIX}"
echo "       runtime_build_dir=${RUNTIME_BUILD_DIR}"
echo "       user_app_build_dir=${USER_APP_BUILD_DIR}"
echo "       backends: vsomeip=${USE_VSOMEIP} iceoryx=${USE_ICEORYX} cyclonedds=${USE_CYCLONEDDS}"
echo "       build_user_apps=${BUILD_USER_APPS} install_deployment_assets=${INSTALL_DEPLOYMENT_ASSETS} run_verify=${RUN_VERIFY}"

runtime_args=(
  --source-dir "${SOURCE_DIR}"
  --prefix "${INSTALL_PREFIX}"
  --build-dir "${RUNTIME_BUILD_DIR}"
  --build-type "${BUILD_TYPE}"
)

if [[ "${USE_VSOMEIP}" == "OFF" ]]; then
  runtime_args+=(--without-vsomeip)
fi
if [[ "${USE_ICEORYX}" == "OFF" ]]; then
  runtime_args+=(--without-iceoryx)
fi
if [[ "${USE_CYCLONEDDS}" == "OFF" ]]; then
  runtime_args+=(--without-cyclonedds)
fi

"${SCRIPT_DIR}/build_and_install_autosar_ap.sh" "${runtime_args[@]}"

if [[ "${BUILD_USER_APPS}" == "ON" ]]; then
  "${SCRIPT_DIR}/build_user_apps_from_opt.sh" \
    --prefix "${INSTALL_PREFIX}" \
    --source-dir "${INSTALL_PREFIX}/user_apps" \
    --build-dir "${USER_APP_BUILD_DIR}"
fi

if [[ "${INSTALL_DEPLOYMENT_ASSETS}" == "ON" ]]; then
  mkdir -p "${INSTALL_PREFIX}/deployment"
  cp -a "${SOURCE_DIR}/deployment/rpi_ecu" "${INSTALL_PREFIX}/deployment/"

  mkdir -p "${INSTALL_PREFIX}/configuration"
  if [[ -f "${SOURCE_DIR}/configuration/vsomeip-pubsub-sample.json" ]]; then
    cp -f \
      "${SOURCE_DIR}/configuration/vsomeip-pubsub-sample.json" \
      "${INSTALL_PREFIX}/configuration/"
  fi
fi

if [[ "${RUN_VERIFY}" == "ON" ]]; then
  "${SCRIPT_DIR}/verify_rpi_ecu_profile.sh" \
    --prefix "${INSTALL_PREFIX}" \
    --user-app-build-dir "${USER_APP_BUILD_DIR}" \
    --can-backend mock
fi

echo "[OK] Raspberry Pi ECU profile build/install completed."
echo "[INFO] Next step:"
echo "       sudo ./scripts/install_rpi_ecu_services.sh --prefix ${INSTALL_PREFIX} --user-app-build-dir ${USER_APP_BUILD_DIR}"
echo "       ./scripts/verify_rpi_ecu_profile.sh --prefix ${INSTALL_PREFIX} --user-app-build-dir ${USER_APP_BUILD_DIR} --can-backend mock"
