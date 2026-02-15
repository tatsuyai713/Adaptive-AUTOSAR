#!/usr/bin/env bash
set -euo pipefail

AUTOSAR_AP_PREFIX="/opt/autosar_ap"
BUILD_DIR="build-user-apps-opt"
RUN_AFTER_BUILD="OFF"
USER_APP_SOURCE_DIR=""

while [[ $# -gt 0 ]]; do
  case "$1" in
    --prefix)
      AUTOSAR_AP_PREFIX="$2"
      shift 2
      ;;
    --build-dir)
      BUILD_DIR="$2"
      shift 2
      ;;
    --run)
      RUN_AFTER_BUILD="ON"
      shift
      ;;
    --source-dir)
      USER_APP_SOURCE_DIR="$2"
      shift 2
      ;;
    *)
      echo "[ERROR] Unknown argument: $1" >&2
      exit 1
      ;;
  esac
done

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
  echo "        run scripts/build_and_install_autosar_ap.sh first." >&2
  exit 1
fi

if [[ -z "${USER_APP_SOURCE_DIR}" ]]; then
  USER_APP_SOURCE_DIR="${AUTOSAR_AP_PREFIX}/user_apps"
fi

if [[ ! -f "${USER_APP_SOURCE_DIR}/CMakeLists.txt" ]]; then
  echo "[ERROR] User app source directory not found: ${USER_APP_SOURCE_DIR}" >&2
  echo "        install template with build_and_install_autosar_ap.sh or pass --source-dir." >&2
  exit 1
fi

echo "[INFO] Building user apps against installed AUTOSAR AP only"
echo "       autosar_ap_prefix=${AUTOSAR_AP_PREFIX}"
echo "       cmake_config=${CONFIG_PATH}"
echo "       package_dir=${ADAPTIVE_AUTOSAR_AP_DIR}"
echo "       user_app_source_dir=${USER_APP_SOURCE_DIR}"

if [[ "${BUILD_DIR}" = /* ]]; then
  BUILD_DIR_ABS="${BUILD_DIR}"
else
  BUILD_DIR_ABS="$(pwd)/${BUILD_DIR}"
fi

cmake -S "${USER_APP_SOURCE_DIR}" -B "${BUILD_DIR_ABS}" \
  -DCMAKE_BUILD_TYPE=Release \
  -DAUTOSAR_AP_PREFIX="${AUTOSAR_AP_PREFIX}" \
  -DAdaptiveAutosarAP_DIR="${ADAPTIVE_AUTOSAR_AP_DIR}"

cmake --build "${BUILD_DIR_ABS}" -j"$(nproc)"

if [[ "${RUN_AFTER_BUILD}" == "ON" ]]; then
  extra_libdirs=""
  if [[ -d "${AUTOSAR_AP_PREFIX}/lib" ]]; then
    extra_libdirs="${AUTOSAR_AP_PREFIX}/lib"
  fi
  if [[ -d "${AUTOSAR_AP_PREFIX}/lib64" ]]; then
    if [[ -n "${extra_libdirs}" ]]; then
      extra_libdirs="${extra_libdirs}:"
    fi
    extra_libdirs="${extra_libdirs}${AUTOSAR_AP_PREFIX}/lib64"
  fi

  export LD_LIBRARY_PATH="${extra_libdirs}:/opt/vsomeip/lib:/opt/cyclonedds/lib:/opt/iceoryx/lib:${LD_LIBRARY_PATH:-}"

  find_user_binary() {
    local binary_name="$1"
    find "${BUILD_DIR_ABS}" -type f -name "${binary_name}" -perm -111 | head -n 1
  }

  minimal_runtime_bin="$(find_user_binary "autosar_user_minimal_runtime")"
  if [[ -z "${minimal_runtime_bin}" ]]; then
    echo "[ERROR] --run requested but autosar_user_minimal_runtime was not found under ${BUILD_DIR_ABS}" >&2
    exit 1
  fi

  per_phm_bin="$(find_user_binary "autosar_user_per_phm_demo")"
  if [[ -z "${per_phm_bin}" ]]; then
    echo "[ERROR] --run requested but autosar_user_per_phm_demo was not found under ${BUILD_DIR_ABS}" >&2
    exit 1
  fi

  echo "[INFO] Running autosar_user_minimal_runtime (${minimal_runtime_bin})"
  "${minimal_runtime_bin}"
  echo "[INFO] Running autosar_user_per_phm_demo (${per_phm_bin})"
  "${per_phm_bin}"
fi

echo "[OK] User apps built in: ${BUILD_DIR_ABS}"
echo "[INFO] Executables:"
{
  find "${BUILD_DIR_ABS}" -type f -name "autosar_user_*"
  find "${BUILD_DIR_ABS}" -type f -name "ara_*_sample"
} | sort | sed 's/^/       /'
