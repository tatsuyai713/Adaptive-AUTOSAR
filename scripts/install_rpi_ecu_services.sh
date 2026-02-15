#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

AUTOSAR_AP_PREFIX="/opt/autosar_ap"
USER_APP_BUILD_DIR="/opt/autosar_ap/user_apps_build"
SYSTEMD_DIR="/etc/systemd/system"
ENV_OUTPUT="/etc/default/autosar-ecu-full-stack"
WRAPPER_OUTPUT="/usr/local/bin/autosar-ecu-full-stack-wrapper.sh"
FORCE_OVERWRITE="OFF"
ENABLE_UNITS="OFF"

while [[ $# -gt 0 ]]; do
  case "$1" in
    --prefix)
      AUTOSAR_AP_PREFIX="$2"
      shift 2
      ;;
    --user-app-build-dir)
      USER_APP_BUILD_DIR="$2"
      shift 2
      ;;
    --systemd-dir)
      SYSTEMD_DIR="$2"
      shift 2
      ;;
    --env-output)
      ENV_OUTPUT="$2"
      shift 2
      ;;
    --wrapper-output)
      WRAPPER_OUTPUT="$2"
      shift 2
      ;;
    --force)
      FORCE_OVERWRITE="ON"
      shift
      ;;
    --enable)
      ENABLE_UNITS="ON"
      shift
      ;;
    *)
      echo "[ERROR] Unknown argument: $1" >&2
      exit 1
      ;;
  esac
done

if [[ "${EUID}" -ne 0 ]]; then
  echo "[ERROR] This script must run as root (use sudo)." >&2
  exit 1
fi

ASSET_ROOT=""
if [[ -d "${AUTOSAR_AP_PREFIX}/deployment/rpi_ecu" ]]; then
  ASSET_ROOT="${AUTOSAR_AP_PREFIX}/deployment/rpi_ecu"
elif [[ -d "${REPO_ROOT}/deployment/rpi_ecu" ]]; then
  ASSET_ROOT="${REPO_ROOT}/deployment/rpi_ecu"
else
  echo "[ERROR] deployment/rpi_ecu assets were not found." >&2
  echo "        expected at ${AUTOSAR_AP_PREFIX}/deployment/rpi_ecu or ${REPO_ROOT}/deployment/rpi_ecu" >&2
  exit 1
fi

WRAPPER_TEMPLATE="${ASSET_ROOT}/bin/autosar-ecu-full-stack-wrapper.sh.in"
ENV_TEMPLATE="${ASSET_ROOT}/env/autosar-ecu-full-stack.env.in"
ROUDI_SERVICE_TEMPLATE="${ASSET_ROOT}/systemd/autosar-iox-roudi.service"
ECU_SERVICE_TEMPLATE="${ASSET_ROOT}/systemd/autosar-ecu-full-stack.service"

if [[ ! -f "${WRAPPER_TEMPLATE}" || ! -f "${ENV_TEMPLATE}" || ! -f "${ROUDI_SERVICE_TEMPLATE}" || ! -f "${ECU_SERVICE_TEMPLATE}" ]]; then
  echo "[ERROR] Missing one or more deployment template files under ${ASSET_ROOT}" >&2
  exit 1
fi

mkdir -p "$(dirname "${WRAPPER_OUTPUT}")"
mkdir -p "$(dirname "${ENV_OUTPUT}")"
mkdir -p "${SYSTEMD_DIR}"

sed \
  -e "s|__AUTOSAR_AP_PREFIX__|${AUTOSAR_AP_PREFIX}|g" \
  -e "s|__USER_APPS_BUILD_DIR__|${USER_APP_BUILD_DIR}|g" \
  "${WRAPPER_TEMPLATE}" > "${WRAPPER_OUTPUT}"
chmod 755 "${WRAPPER_OUTPUT}"

if [[ -f "${ENV_OUTPUT}" && "${FORCE_OVERWRITE}" != "ON" ]]; then
  echo "[INFO] Keep existing env file: ${ENV_OUTPUT} (use --force to overwrite)"
else
  sed \
    -e "s|__AUTOSAR_AP_PREFIX__|${AUTOSAR_AP_PREFIX}|g" \
    -e "s|__USER_APPS_BUILD_DIR__|${USER_APP_BUILD_DIR}|g" \
    "${ENV_TEMPLATE}" > "${ENV_OUTPUT}"
  chmod 644 "${ENV_OUTPUT}"
fi

install -m 644 "${ROUDI_SERVICE_TEMPLATE}" "${SYSTEMD_DIR}/autosar-iox-roudi.service"
install -m 644 "${ECU_SERVICE_TEMPLATE}" "${SYSTEMD_DIR}/autosar-ecu-full-stack.service"

systemctl daemon-reload

if [[ "${ENABLE_UNITS}" == "ON" ]]; then
  systemctl enable autosar-iox-roudi.service
  systemctl enable autosar-ecu-full-stack.service
fi

echo "[OK] Raspberry Pi ECU services installed."
echo "[INFO] Wrapper: ${WRAPPER_OUTPUT}"
echo "[INFO] Env file: ${ENV_OUTPUT}"
echo "[INFO] Services:"
echo "       ${SYSTEMD_DIR}/autosar-iox-roudi.service"
echo "       ${SYSTEMD_DIR}/autosar-ecu-full-stack.service"
echo "[INFO] Next commands:"
echo "       sudo systemctl start autosar-iox-roudi.service"
echo "       sudo systemctl start autosar-ecu-full-stack.service"
echo "       sudo systemctl status autosar-ecu-full-stack.service --no-pager"
