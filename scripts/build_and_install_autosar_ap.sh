#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT_DEFAULT="$(cd "${SCRIPT_DIR}/.." && pwd)"
REPO_ROOT="${REPO_ROOT_DEFAULT}"

INSTALL_PREFIX="/opt/autosar_ap"
BUILD_DIR="build-install-autosar-ap"
BUILD_TYPE="Release"
USE_VSOMEIP="ON"
USE_ICEORYX="ON"
USE_CYCLONEDDS="ON"

while [[ $# -gt 0 ]]; do
  case "$1" in
    --prefix)
      INSTALL_PREFIX="$2"
      shift 2
      ;;
    --build-dir)
      BUILD_DIR="$2"
      shift 2
      ;;
    --build-type)
      BUILD_TYPE="$2"
      shift 2
      ;;
    --source-dir)
      REPO_ROOT="$2"
      shift 2
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

if [[ ! -f "${REPO_ROOT}/CMakeLists.txt" ]]; then
  echo "[ERROR] CMakeLists.txt was not found in source dir: ${REPO_ROOT}" >&2
  echo "        pass the source root by --source-dir <path>." >&2
  exit 1
fi

echo "[INFO] Building AUTOSAR AP implementation only"
echo "       source_dir=${REPO_ROOT}"
echo "       install_prefix=${INSTALL_PREFIX}"
echo "       build_dir=${BUILD_DIR}"
echo "       backends: vsomeip=${USE_VSOMEIP} iceoryx=${USE_ICEORYX} cyclonedds=${USE_CYCLONEDDS}"

cmake -S "${REPO_ROOT}" -B "${REPO_ROOT}/${BUILD_DIR}" \
  -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" \
  -DCMAKE_INSTALL_PREFIX="${INSTALL_PREFIX}" \
  -Dbuild_tests=OFF \
  -DARA_COM_ENABLE_ARXML_CODEGEN=OFF \
  -DAUTOSAR_AP_BUILD_PLATFORM_APP=OFF \
  -DAUTOSAR_AP_BUILD_SAMPLES=OFF \
  -DARA_COM_USE_VSOMEIP="${USE_VSOMEIP}" \
  -DARA_COM_USE_ICEORYX="${USE_ICEORYX}" \
  -DARA_COM_USE_CYCLONEDDS="${USE_CYCLONEDDS}"

cmake --build "${REPO_ROOT}/${BUILD_DIR}" -j"$(nproc)"
cmake --install "${REPO_ROOT}/${BUILD_DIR}"

echo "[OK] Installed AUTOSAR AP runtime into: ${INSTALL_PREFIX}"
echo "[INFO] Installed package config:"
echo "       ${INSTALL_PREFIX}/lib/cmake/AdaptiveAutosarAP/AdaptiveAutosarAPConfig.cmake"
