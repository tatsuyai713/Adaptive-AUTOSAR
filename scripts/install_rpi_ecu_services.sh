#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

AUTOSAR_AP_PREFIX="/opt/autosar_ap"
USER_APP_BUILD_DIR="/opt/autosar_ap/user_apps_build"
SYSTEMD_DIR="/etc/systemd/system"
ENV_OUTPUT="/etc/default/autosar-ecu-full-stack"
WRAPPER_OUTPUT="/usr/local/bin/autosar-ecu-full-stack-wrapper.sh"
VSOMEIP_ROUTING_ENV_OUTPUT="/etc/default/autosar-vsomeip-routing"
VSOMEIP_ROUTING_WRAPPER_OUTPUT="/usr/local/bin/autosar-vsomeip-routing-wrapper.sh"
PLATFORM_ENV_OUTPUT="/etc/default/autosar-platform-app"
PLATFORM_WRAPPER_OUTPUT="/usr/local/bin/autosar-platform-app-wrapper.sh"
EXEC_ENV_OUTPUT="/etc/default/autosar-exec-manager"
EXEC_WRAPPER_OUTPUT="/usr/local/bin/autosar-exec-manager-wrapper.sh"
BRINGUP_DIR="/etc/autosar"
BRINGUP_OUTPUT="/etc/autosar/bringup.sh"
STARTUP_OUTPUT="/etc/autosar/startup.sh"
FORCE_OVERWRITE="OFF"
ENABLE_UNITS="OFF"
RUN_SYSTEMCTL="ON"

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
    --vsomeip-routing-env-output)
      VSOMEIP_ROUTING_ENV_OUTPUT="$2"
      shift 2
      ;;
    --vsomeip-routing-wrapper-output)
      VSOMEIP_ROUTING_WRAPPER_OUTPUT="$2"
      shift 2
      ;;
    --exec-env-output)
      EXEC_ENV_OUTPUT="$2"
      shift 2
      ;;
    --exec-wrapper-output)
      EXEC_WRAPPER_OUTPUT="$2"
      shift 2
      ;;
    --platform-env-output)
      PLATFORM_ENV_OUTPUT="$2"
      shift 2
      ;;
    --platform-wrapper-output)
      PLATFORM_WRAPPER_OUTPUT="$2"
      shift 2
      ;;
    --bringup-output)
      BRINGUP_OUTPUT="$2"
      BRINGUP_DIR="$(dirname "${BRINGUP_OUTPUT}")"
      STARTUP_OUTPUT="${BRINGUP_DIR}/startup.sh"
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
    --skip-systemctl)
      RUN_SYSTEMCTL="OFF"
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
VSOMEIP_ROUTING_WRAPPER_TEMPLATE="${ASSET_ROOT}/bin/autosar-vsomeip-routing-wrapper.sh.in"
PLATFORM_WRAPPER_TEMPLATE="${ASSET_ROOT}/bin/autosar-platform-app-wrapper.sh.in"
EXEC_WRAPPER_TEMPLATE="${ASSET_ROOT}/bin/autosar-exec-manager-wrapper.sh.in"
ENV_TEMPLATE="${ASSET_ROOT}/env/autosar-ecu-full-stack.env.in"
VSOMEIP_ROUTING_ENV_TEMPLATE="${ASSET_ROOT}/env/autosar-vsomeip-routing.env.in"
PLATFORM_ENV_TEMPLATE="${ASSET_ROOT}/env/autosar-platform-app.env.in"
EXEC_ENV_TEMPLATE="${ASSET_ROOT}/env/autosar-exec-manager.env.in"
BRINGUP_TEMPLATE="${ASSET_ROOT}/bringup/bringup.sh.in"
ROUDI_SERVICE_TEMPLATE="${ASSET_ROOT}/systemd/autosar-iox-roudi.service"
VSOMEIP_ROUTING_SERVICE_TEMPLATE="${ASSET_ROOT}/systemd/autosar-vsomeip-routing.service"
PLATFORM_SERVICE_TEMPLATE="${ASSET_ROOT}/systemd/autosar-platform-app.service"
ECU_SERVICE_TEMPLATE="${ASSET_ROOT}/systemd/autosar-ecu-full-stack.service"
EXEC_SERVICE_TEMPLATE="${ASSET_ROOT}/systemd/autosar-exec-manager.service"

if [[ ! -f "${WRAPPER_TEMPLATE}" || \
      ! -f "${VSOMEIP_ROUTING_WRAPPER_TEMPLATE}" || \
      ! -f "${PLATFORM_WRAPPER_TEMPLATE}" || \
      ! -f "${EXEC_WRAPPER_TEMPLATE}" || \
      ! -f "${ENV_TEMPLATE}" || \
      ! -f "${VSOMEIP_ROUTING_ENV_TEMPLATE}" || \
      ! -f "${PLATFORM_ENV_TEMPLATE}" || \
      ! -f "${EXEC_ENV_TEMPLATE}" || \
      ! -f "${BRINGUP_TEMPLATE}" || \
      ! -f "${ROUDI_SERVICE_TEMPLATE}" || \
      ! -f "${VSOMEIP_ROUTING_SERVICE_TEMPLATE}" || \
      ! -f "${PLATFORM_SERVICE_TEMPLATE}" || \
      ! -f "${ECU_SERVICE_TEMPLATE}" || \
      ! -f "${EXEC_SERVICE_TEMPLATE}" ]]; then
  echo "[ERROR] Missing one or more deployment template files under ${ASSET_ROOT}" >&2
  exit 1
fi

mkdir -p "$(dirname "${WRAPPER_OUTPUT}")"
mkdir -p "$(dirname "${ENV_OUTPUT}")"
mkdir -p "$(dirname "${VSOMEIP_ROUTING_WRAPPER_OUTPUT}")"
mkdir -p "$(dirname "${VSOMEIP_ROUTING_ENV_OUTPUT}")"
mkdir -p "$(dirname "${PLATFORM_WRAPPER_OUTPUT}")"
mkdir -p "$(dirname "${PLATFORM_ENV_OUTPUT}")"
mkdir -p "$(dirname "${EXEC_WRAPPER_OUTPUT}")"
mkdir -p "$(dirname "${EXEC_ENV_OUTPUT}")"
mkdir -p "${BRINGUP_DIR}"
mkdir -p "${SYSTEMD_DIR}"

sed \
  -e "s|__AUTOSAR_AP_PREFIX__|${AUTOSAR_AP_PREFIX}|g" \
  -e "s|__USER_APPS_BUILD_DIR__|${USER_APP_BUILD_DIR}|g" \
  "${WRAPPER_TEMPLATE}" > "${WRAPPER_OUTPUT}"
chmod 755 "${WRAPPER_OUTPUT}"

sed \
  -e "s|__AUTOSAR_AP_PREFIX__|${AUTOSAR_AP_PREFIX}|g" \
  -e "s|__USER_APPS_BUILD_DIR__|${USER_APP_BUILD_DIR}|g" \
  "${VSOMEIP_ROUTING_WRAPPER_TEMPLATE}" > "${VSOMEIP_ROUTING_WRAPPER_OUTPUT}"
chmod 755 "${VSOMEIP_ROUTING_WRAPPER_OUTPUT}"

sed \
  -e "s|__AUTOSAR_AP_PREFIX__|${AUTOSAR_AP_PREFIX}|g" \
  -e "s|__USER_APPS_BUILD_DIR__|${USER_APP_BUILD_DIR}|g" \
  "${PLATFORM_WRAPPER_TEMPLATE}" > "${PLATFORM_WRAPPER_OUTPUT}"
chmod 755 "${PLATFORM_WRAPPER_OUTPUT}"

sed \
  -e "s|__AUTOSAR_AP_PREFIX__|${AUTOSAR_AP_PREFIX}|g" \
  -e "s|__USER_APPS_BUILD_DIR__|${USER_APP_BUILD_DIR}|g" \
  "${EXEC_WRAPPER_TEMPLATE}" > "${EXEC_WRAPPER_OUTPUT}"
chmod 755 "${EXEC_WRAPPER_OUTPUT}"

if [[ -f "${ENV_OUTPUT}" && "${FORCE_OVERWRITE}" != "ON" ]]; then
  echo "[INFO] Keep existing env file: ${ENV_OUTPUT} (use --force to overwrite)"
else
  sed \
    -e "s|__AUTOSAR_AP_PREFIX__|${AUTOSAR_AP_PREFIX}|g" \
    -e "s|__USER_APPS_BUILD_DIR__|${USER_APP_BUILD_DIR}|g" \
    "${ENV_TEMPLATE}" > "${ENV_OUTPUT}"
  chmod 644 "${ENV_OUTPUT}"
fi

if [[ -f "${VSOMEIP_ROUTING_ENV_OUTPUT}" && "${FORCE_OVERWRITE}" != "ON" ]]; then
  echo "[INFO] Keep existing vsomeip routing env file: ${VSOMEIP_ROUTING_ENV_OUTPUT} (use --force to overwrite)"
else
  sed \
    -e "s|__AUTOSAR_AP_PREFIX__|${AUTOSAR_AP_PREFIX}|g" \
    -e "s|__USER_APPS_BUILD_DIR__|${USER_APP_BUILD_DIR}|g" \
    "${VSOMEIP_ROUTING_ENV_TEMPLATE}" > "${VSOMEIP_ROUTING_ENV_OUTPUT}"
  chmod 644 "${VSOMEIP_ROUTING_ENV_OUTPUT}"
fi

if [[ -f "${PLATFORM_ENV_OUTPUT}" && "${FORCE_OVERWRITE}" != "ON" ]]; then
  echo "[INFO] Keep existing platform env file: ${PLATFORM_ENV_OUTPUT} (use --force to overwrite)"
else
  sed \
    -e "s|__AUTOSAR_AP_PREFIX__|${AUTOSAR_AP_PREFIX}|g" \
    -e "s|__USER_APPS_BUILD_DIR__|${USER_APP_BUILD_DIR}|g" \
    "${PLATFORM_ENV_TEMPLATE}" > "${PLATFORM_ENV_OUTPUT}"
  chmod 644 "${PLATFORM_ENV_OUTPUT}"
fi

if [[ -f "${EXEC_ENV_OUTPUT}" && "${FORCE_OVERWRITE}" != "ON" ]]; then
  echo "[INFO] Keep existing execution-manager env file: ${EXEC_ENV_OUTPUT} (use --force to overwrite)"
else
  sed \
    -e "s|__AUTOSAR_AP_PREFIX__|${AUTOSAR_AP_PREFIX}|g" \
    -e "s|__USER_APPS_BUILD_DIR__|${USER_APP_BUILD_DIR}|g" \
    "${EXEC_ENV_TEMPLATE}" > "${EXEC_ENV_OUTPUT}"
  chmod 644 "${EXEC_ENV_OUTPUT}"
fi

if [[ -f "${BRINGUP_OUTPUT}" && "${FORCE_OVERWRITE}" != "ON" ]]; then
  echo "[INFO] Keep existing bringup script: ${BRINGUP_OUTPUT} (use --force to overwrite)"
else
  sed \
    -e "s|__AUTOSAR_AP_PREFIX__|${AUTOSAR_AP_PREFIX}|g" \
    -e "s|__USER_APPS_BUILD_DIR__|${USER_APP_BUILD_DIR}|g" \
    "${BRINGUP_TEMPLATE}" > "${BRINGUP_OUTPUT}"
  chmod 755 "${BRINGUP_OUTPUT}"
fi

cat > "${STARTUP_OUTPUT}" <<EOF
#!/usr/bin/env bash
set -euo pipefail
exec "${BRINGUP_OUTPUT}" "\$@"
EOF
chmod 755 "${STARTUP_OUTPUT}"

install -m 644 "${ROUDI_SERVICE_TEMPLATE}" "${SYSTEMD_DIR}/autosar-iox-roudi.service"
install -m 644 "${VSOMEIP_ROUTING_SERVICE_TEMPLATE}" "${SYSTEMD_DIR}/autosar-vsomeip-routing.service"
install -m 644 "${PLATFORM_SERVICE_TEMPLATE}" "${SYSTEMD_DIR}/autosar-platform-app.service"
install -m 644 "${ECU_SERVICE_TEMPLATE}" "${SYSTEMD_DIR}/autosar-ecu-full-stack.service"
install -m 644 "${EXEC_SERVICE_TEMPLATE}" "${SYSTEMD_DIR}/autosar-exec-manager.service"

if [[ "${RUN_SYSTEMCTL}" == "ON" ]]; then
  systemctl daemon-reload

  if [[ "${ENABLE_UNITS}" == "ON" ]]; then
    systemctl enable autosar-iox-roudi.service
    if [[ -x "${AUTOSAR_AP_PREFIX}/bin/autosar_vsomeip_routing_manager" ]]; then
      systemctl enable autosar-vsomeip-routing.service
    else
      echo "[WARN] Skip enable autosar-vsomeip-routing.service: routing manager binary not found under ${AUTOSAR_AP_PREFIX}/bin"
    fi
    systemctl enable autosar-platform-app.service
    systemctl enable autosar-ecu-full-stack.service
    systemctl enable autosar-exec-manager.service
  fi
else
  echo "[INFO] --skip-systemctl specified. Skipping systemctl daemon-reload/enable."
fi

echo "[OK] Raspberry Pi ECU services installed."
echo "[INFO] Wrapper: ${WRAPPER_OUTPUT}"
echo "[INFO] vSomeIP routing wrapper: ${VSOMEIP_ROUTING_WRAPPER_OUTPUT}"
echo "[INFO] Platform wrapper: ${PLATFORM_WRAPPER_OUTPUT}"
echo "[INFO] Exec-manager wrapper: ${EXEC_WRAPPER_OUTPUT}"
echo "[INFO] Env file: ${ENV_OUTPUT}"
echo "[INFO] vSomeIP routing env file: ${VSOMEIP_ROUTING_ENV_OUTPUT}"
echo "[INFO] Platform env file: ${PLATFORM_ENV_OUTPUT}"
echo "[INFO] Exec-manager env file: ${EXEC_ENV_OUTPUT}"
echo "[INFO] Bringup script: ${BRINGUP_OUTPUT}"
echo "[INFO] Startup alias script: ${STARTUP_OUTPUT}"
echo "[INFO] Services:"
echo "       ${SYSTEMD_DIR}/autosar-iox-roudi.service"
echo "       ${SYSTEMD_DIR}/autosar-vsomeip-routing.service"
echo "       ${SYSTEMD_DIR}/autosar-platform-app.service"
echo "       ${SYSTEMD_DIR}/autosar-ecu-full-stack.service"
echo "       ${SYSTEMD_DIR}/autosar-exec-manager.service"
echo "[INFO] Next commands:"
echo "       sudo systemctl start autosar-iox-roudi.service"
echo "       sudo systemctl start autosar-vsomeip-routing.service"
echo "       sudo systemctl start autosar-platform-app.service"
echo "       sudo systemctl start autosar-exec-manager.service"
echo "       sudo systemctl start autosar-ecu-full-stack.service"
echo "       sudo systemctl status autosar-vsomeip-routing.service --no-pager"
echo "       sudo systemctl status autosar-platform-app.service --no-pager"
echo "       sudo systemctl status autosar-exec-manager.service --no-pager"
echo "[INFO] If you launch apps through bringup.sh, you can disable the fixed ECU template service:"
echo "       sudo systemctl disable --now autosar-ecu-full-stack.service"
