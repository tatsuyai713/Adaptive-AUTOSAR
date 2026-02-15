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

TIME_SYNC_ENV_OUTPUT="/etc/default/autosar-time-sync"
TIME_SYNC_WRAPPER_OUTPUT="/usr/local/bin/autosar-time-sync-wrapper.sh"

PERSISTENCY_ENV_OUTPUT="/etc/default/autosar-persistency-guard"
PERSISTENCY_WRAPPER_OUTPUT="/usr/local/bin/autosar-persistency-guard-wrapper.sh"

IAM_POLICY_ENV_OUTPUT="/etc/default/autosar-iam-policy"
IAM_POLICY_WRAPPER_OUTPUT="/usr/local/bin/autosar-iam-policy-wrapper.sh"
IAM_POLICY_OUTPUT="/etc/autosar/iam_policy.csv"

CAN_MANAGER_ENV_OUTPUT="/etc/default/autosar-can-manager"
CAN_MANAGER_WRAPPER_OUTPUT="/usr/local/bin/autosar-can-manager-wrapper.sh"

WATCHDOG_ENV_OUTPUT="/etc/default/autosar-watchdog"
WATCHDOG_WRAPPER_OUTPUT="/usr/local/bin/autosar-watchdog-wrapper.sh"

USER_APP_MONITOR_ENV_OUTPUT="/etc/default/autosar-user-app-monitor"
USER_APP_MONITOR_WRAPPER_OUTPUT="/usr/local/bin/autosar-user-app-monitor-wrapper.sh"

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
    --platform-env-output)
      PLATFORM_ENV_OUTPUT="$2"
      shift 2
      ;;
    --platform-wrapper-output)
      PLATFORM_WRAPPER_OUTPUT="$2"
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
    --time-sync-env-output)
      TIME_SYNC_ENV_OUTPUT="$2"
      shift 2
      ;;
    --time-sync-wrapper-output)
      TIME_SYNC_WRAPPER_OUTPUT="$2"
      shift 2
      ;;
    --persistency-env-output)
      PERSISTENCY_ENV_OUTPUT="$2"
      shift 2
      ;;
    --persistency-wrapper-output)
      PERSISTENCY_WRAPPER_OUTPUT="$2"
      shift 2
      ;;
    --iam-policy-env-output)
      IAM_POLICY_ENV_OUTPUT="$2"
      shift 2
      ;;
    --iam-policy-wrapper-output)
      IAM_POLICY_WRAPPER_OUTPUT="$2"
      shift 2
      ;;
    --iam-policy-output)
      IAM_POLICY_OUTPUT="$2"
      shift 2
      ;;
    --can-manager-env-output)
      CAN_MANAGER_ENV_OUTPUT="$2"
      shift 2
      ;;
    --can-manager-wrapper-output)
      CAN_MANAGER_WRAPPER_OUTPUT="$2"
      shift 2
      ;;
    --watchdog-env-output)
      WATCHDOG_ENV_OUTPUT="$2"
      shift 2
      ;;
    --watchdog-wrapper-output)
      WATCHDOG_WRAPPER_OUTPUT="$2"
      shift 2
      ;;
    --user-app-monitor-env-output)
      USER_APP_MONITOR_ENV_OUTPUT="$2"
      shift 2
      ;;
    --user-app-monitor-wrapper-output)
      USER_APP_MONITOR_WRAPPER_OUTPUT="$2"
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

find_asset_root() {
  local candidate=""
  for candidate in \
    "${AUTOSAR_AP_PREFIX}/deployment/rpi_ecu" \
    "${REPO_ROOT}/deployment/rpi_ecu"; do
    if [[ ! -d "${candidate}" ]]; then
      continue
    fi

    if [[ -f "${candidate}/bin/autosar-ecu-full-stack-wrapper.sh.in" && \
          -f "${candidate}/bin/autosar-vsomeip-routing-wrapper.sh.in" && \
          -f "${candidate}/bin/autosar-user-app-monitor-wrapper.sh.in" && \
          -f "${candidate}/env/autosar-ecu-full-stack.env.in" && \
          -f "${candidate}/env/autosar-user-app-monitor.env.in" && \
          -f "${candidate}/bringup/bringup.sh.in" && \
          -f "${candidate}/config/iam_policy.csv.in" && \
          -f "${candidate}/systemd/autosar-iox-roudi.service" && \
          -f "${candidate}/systemd/autosar-watchdog.service" && \
          -f "${candidate}/systemd/autosar-user-app-monitor.service" ]]; then
      echo "${candidate}"
      return 0
    fi
  done

  return 1
}

ASSET_ROOT="$(find_asset_root || true)"
if [[ -z "${ASSET_ROOT}" ]]; then
  echo "[ERROR] deployment/rpi_ecu assets were not found or are incomplete." >&2
  echo "        checked: ${AUTOSAR_AP_PREFIX}/deployment/rpi_ecu and ${REPO_ROOT}/deployment/rpi_ecu" >&2
  exit 1
fi

WRAPPER_TEMPLATE="${ASSET_ROOT}/bin/autosar-ecu-full-stack-wrapper.sh.in"
VSOMEIP_ROUTING_WRAPPER_TEMPLATE="${ASSET_ROOT}/bin/autosar-vsomeip-routing-wrapper.sh.in"
PLATFORM_WRAPPER_TEMPLATE="${ASSET_ROOT}/bin/autosar-platform-app-wrapper.sh.in"
EXEC_WRAPPER_TEMPLATE="${ASSET_ROOT}/bin/autosar-exec-manager-wrapper.sh.in"
TIME_SYNC_WRAPPER_TEMPLATE="${ASSET_ROOT}/bin/autosar-time-sync-wrapper.sh.in"
PERSISTENCY_WRAPPER_TEMPLATE="${ASSET_ROOT}/bin/autosar-persistency-guard-wrapper.sh.in"
IAM_POLICY_WRAPPER_TEMPLATE="${ASSET_ROOT}/bin/autosar-iam-policy-wrapper.sh.in"
CAN_MANAGER_WRAPPER_TEMPLATE="${ASSET_ROOT}/bin/autosar-can-manager-wrapper.sh.in"
WATCHDOG_WRAPPER_TEMPLATE="${ASSET_ROOT}/bin/autosar-watchdog-wrapper.sh.in"
USER_APP_MONITOR_WRAPPER_TEMPLATE="${ASSET_ROOT}/bin/autosar-user-app-monitor-wrapper.sh.in"

ENV_TEMPLATE="${ASSET_ROOT}/env/autosar-ecu-full-stack.env.in"
VSOMEIP_ROUTING_ENV_TEMPLATE="${ASSET_ROOT}/env/autosar-vsomeip-routing.env.in"
PLATFORM_ENV_TEMPLATE="${ASSET_ROOT}/env/autosar-platform-app.env.in"
EXEC_ENV_TEMPLATE="${ASSET_ROOT}/env/autosar-exec-manager.env.in"
TIME_SYNC_ENV_TEMPLATE="${ASSET_ROOT}/env/autosar-time-sync.env.in"
PERSISTENCY_ENV_TEMPLATE="${ASSET_ROOT}/env/autosar-persistency-guard.env.in"
IAM_POLICY_ENV_TEMPLATE="${ASSET_ROOT}/env/autosar-iam-policy.env.in"
CAN_MANAGER_ENV_TEMPLATE="${ASSET_ROOT}/env/autosar-can-manager.env.in"
WATCHDOG_ENV_TEMPLATE="${ASSET_ROOT}/env/autosar-watchdog.env.in"
USER_APP_MONITOR_ENV_TEMPLATE="${ASSET_ROOT}/env/autosar-user-app-monitor.env.in"

IAM_POLICY_TEMPLATE="${ASSET_ROOT}/config/iam_policy.csv.in"
BRINGUP_TEMPLATE="${ASSET_ROOT}/bringup/bringup.sh.in"

ROUDI_SERVICE_TEMPLATE="${ASSET_ROOT}/systemd/autosar-iox-roudi.service"
VSOMEIP_ROUTING_SERVICE_TEMPLATE="${ASSET_ROOT}/systemd/autosar-vsomeip-routing.service"
PLATFORM_SERVICE_TEMPLATE="${ASSET_ROOT}/systemd/autosar-platform-app.service"
ECU_SERVICE_TEMPLATE="${ASSET_ROOT}/systemd/autosar-ecu-full-stack.service"
EXEC_SERVICE_TEMPLATE="${ASSET_ROOT}/systemd/autosar-exec-manager.service"
TIME_SYNC_SERVICE_TEMPLATE="${ASSET_ROOT}/systemd/autosar-time-sync.service"
PERSISTENCY_SERVICE_TEMPLATE="${ASSET_ROOT}/systemd/autosar-persistency-guard.service"
IAM_POLICY_SERVICE_TEMPLATE="${ASSET_ROOT}/systemd/autosar-iam-policy.service"
CAN_MANAGER_SERVICE_TEMPLATE="${ASSET_ROOT}/systemd/autosar-can-manager.service"
WATCHDOG_SERVICE_TEMPLATE="${ASSET_ROOT}/systemd/autosar-watchdog.service"
USER_APP_MONITOR_SERVICE_TEMPLATE="${ASSET_ROOT}/systemd/autosar-user-app-monitor.service"

required_templates=(
  "${WRAPPER_TEMPLATE}"
  "${VSOMEIP_ROUTING_WRAPPER_TEMPLATE}"
  "${PLATFORM_WRAPPER_TEMPLATE}"
  "${EXEC_WRAPPER_TEMPLATE}"
  "${TIME_SYNC_WRAPPER_TEMPLATE}"
  "${PERSISTENCY_WRAPPER_TEMPLATE}"
  "${IAM_POLICY_WRAPPER_TEMPLATE}"
  "${CAN_MANAGER_WRAPPER_TEMPLATE}"
  "${WATCHDOG_WRAPPER_TEMPLATE}"
  "${USER_APP_MONITOR_WRAPPER_TEMPLATE}"
  "${ENV_TEMPLATE}"
  "${VSOMEIP_ROUTING_ENV_TEMPLATE}"
  "${PLATFORM_ENV_TEMPLATE}"
  "${EXEC_ENV_TEMPLATE}"
  "${TIME_SYNC_ENV_TEMPLATE}"
  "${PERSISTENCY_ENV_TEMPLATE}"
  "${IAM_POLICY_ENV_TEMPLATE}"
  "${CAN_MANAGER_ENV_TEMPLATE}"
  "${WATCHDOG_ENV_TEMPLATE}"
  "${USER_APP_MONITOR_ENV_TEMPLATE}"
  "${IAM_POLICY_TEMPLATE}"
  "${BRINGUP_TEMPLATE}"
  "${ROUDI_SERVICE_TEMPLATE}"
  "${VSOMEIP_ROUTING_SERVICE_TEMPLATE}"
  "${PLATFORM_SERVICE_TEMPLATE}"
  "${ECU_SERVICE_TEMPLATE}"
  "${EXEC_SERVICE_TEMPLATE}"
  "${TIME_SYNC_SERVICE_TEMPLATE}"
  "${PERSISTENCY_SERVICE_TEMPLATE}"
  "${IAM_POLICY_SERVICE_TEMPLATE}"
  "${CAN_MANAGER_SERVICE_TEMPLATE}"
  "${WATCHDOG_SERVICE_TEMPLATE}"
  "${USER_APP_MONITOR_SERVICE_TEMPLATE}"
)

for template_path in "${required_templates[@]}"; do
  if [[ ! -f "${template_path}" ]]; then
    echo "[ERROR] Missing deployment template file: ${template_path}" >&2
    exit 1
  fi
done

mkdir -p "$(dirname "${WRAPPER_OUTPUT}")"
mkdir -p "$(dirname "${ENV_OUTPUT}")"
mkdir -p "$(dirname "${VSOMEIP_ROUTING_WRAPPER_OUTPUT}")"
mkdir -p "$(dirname "${VSOMEIP_ROUTING_ENV_OUTPUT}")"
mkdir -p "$(dirname "${PLATFORM_WRAPPER_OUTPUT}")"
mkdir -p "$(dirname "${PLATFORM_ENV_OUTPUT}")"
mkdir -p "$(dirname "${EXEC_WRAPPER_OUTPUT}")"
mkdir -p "$(dirname "${EXEC_ENV_OUTPUT}")"
mkdir -p "$(dirname "${TIME_SYNC_WRAPPER_OUTPUT}")"
mkdir -p "$(dirname "${TIME_SYNC_ENV_OUTPUT}")"
mkdir -p "$(dirname "${PERSISTENCY_WRAPPER_OUTPUT}")"
mkdir -p "$(dirname "${PERSISTENCY_ENV_OUTPUT}")"
mkdir -p "$(dirname "${IAM_POLICY_WRAPPER_OUTPUT}")"
mkdir -p "$(dirname "${IAM_POLICY_ENV_OUTPUT}")"
mkdir -p "$(dirname "${IAM_POLICY_OUTPUT}")"
mkdir -p "$(dirname "${CAN_MANAGER_WRAPPER_OUTPUT}")"
mkdir -p "$(dirname "${CAN_MANAGER_ENV_OUTPUT}")"
mkdir -p "$(dirname "${WATCHDOG_WRAPPER_OUTPUT}")"
mkdir -p "$(dirname "${WATCHDOG_ENV_OUTPUT}")"
mkdir -p "$(dirname "${USER_APP_MONITOR_WRAPPER_OUTPUT}")"
mkdir -p "$(dirname "${USER_APP_MONITOR_ENV_OUTPUT}")"
mkdir -p "${BRINGUP_DIR}"
mkdir -p "${SYSTEMD_DIR}"

render_template() {
  local template_path="$1"
  local output_path="$2"
  local mode="$3"

  sed \
    -e "s|__AUTOSAR_AP_PREFIX__|${AUTOSAR_AP_PREFIX}|g" \
    -e "s|__USER_APPS_BUILD_DIR__|${USER_APP_BUILD_DIR}|g" \
    "${template_path}" > "${output_path}"
  chmod "${mode}" "${output_path}"
}

render_template "${WRAPPER_TEMPLATE}" "${WRAPPER_OUTPUT}" 755
render_template "${VSOMEIP_ROUTING_WRAPPER_TEMPLATE}" "${VSOMEIP_ROUTING_WRAPPER_OUTPUT}" 755
render_template "${PLATFORM_WRAPPER_TEMPLATE}" "${PLATFORM_WRAPPER_OUTPUT}" 755
render_template "${EXEC_WRAPPER_TEMPLATE}" "${EXEC_WRAPPER_OUTPUT}" 755
render_template "${TIME_SYNC_WRAPPER_TEMPLATE}" "${TIME_SYNC_WRAPPER_OUTPUT}" 755
render_template "${PERSISTENCY_WRAPPER_TEMPLATE}" "${PERSISTENCY_WRAPPER_OUTPUT}" 755
render_template "${IAM_POLICY_WRAPPER_TEMPLATE}" "${IAM_POLICY_WRAPPER_OUTPUT}" 755
render_template "${CAN_MANAGER_WRAPPER_TEMPLATE}" "${CAN_MANAGER_WRAPPER_OUTPUT}" 755
render_template "${WATCHDOG_WRAPPER_TEMPLATE}" "${WATCHDOG_WRAPPER_OUTPUT}" 755
render_template "${USER_APP_MONITOR_WRAPPER_TEMPLATE}" "${USER_APP_MONITOR_WRAPPER_OUTPUT}" 755

render_optional_env() {
  local template_path="$1"
  local output_path="$2"
  local label="$3"

  if [[ -f "${output_path}" && "${FORCE_OVERWRITE}" != "ON" ]]; then
    echo "[INFO] Keep existing ${label}: ${output_path} (use --force to overwrite)"
  else
    render_template "${template_path}" "${output_path}" 644
  fi
}

render_optional_env "${ENV_TEMPLATE}" "${ENV_OUTPUT}" "env file"
render_optional_env "${VSOMEIP_ROUTING_ENV_TEMPLATE}" "${VSOMEIP_ROUTING_ENV_OUTPUT}" "vSomeIP routing env file"
render_optional_env "${PLATFORM_ENV_TEMPLATE}" "${PLATFORM_ENV_OUTPUT}" "platform env file"
render_optional_env "${EXEC_ENV_TEMPLATE}" "${EXEC_ENV_OUTPUT}" "execution-manager env file"
render_optional_env "${TIME_SYNC_ENV_TEMPLATE}" "${TIME_SYNC_ENV_OUTPUT}" "time-sync env file"
render_optional_env "${PERSISTENCY_ENV_TEMPLATE}" "${PERSISTENCY_ENV_OUTPUT}" "persistency-guard env file"
render_optional_env "${IAM_POLICY_ENV_TEMPLATE}" "${IAM_POLICY_ENV_OUTPUT}" "IAM policy env file"
render_optional_env "${CAN_MANAGER_ENV_TEMPLATE}" "${CAN_MANAGER_ENV_OUTPUT}" "CAN manager env file"
render_optional_env "${WATCHDOG_ENV_TEMPLATE}" "${WATCHDOG_ENV_OUTPUT}" "watchdog env file"
render_optional_env "${USER_APP_MONITOR_ENV_TEMPLATE}" "${USER_APP_MONITOR_ENV_OUTPUT}" "user app monitor env file"

if [[ -f "${IAM_POLICY_OUTPUT}" && "${FORCE_OVERWRITE}" != "ON" ]]; then
  echo "[INFO] Keep existing IAM policy file: ${IAM_POLICY_OUTPUT} (use --force to overwrite)"
else
  render_template "${IAM_POLICY_TEMPLATE}" "${IAM_POLICY_OUTPUT}" 644
fi

if [[ -f "${BRINGUP_OUTPUT}" && "${FORCE_OVERWRITE}" != "ON" ]]; then
  echo "[INFO] Keep existing bringup script: ${BRINGUP_OUTPUT} (use --force to overwrite)"
else
  render_template "${BRINGUP_TEMPLATE}" "${BRINGUP_OUTPUT}" 755
fi

cat > "${STARTUP_OUTPUT}" <<EOF_STARTUP
#!/usr/bin/env bash
set -euo pipefail
exec "${BRINGUP_OUTPUT}" "\$@"
EOF_STARTUP
chmod 755 "${STARTUP_OUTPUT}"

install -m 644 "${ROUDI_SERVICE_TEMPLATE}" "${SYSTEMD_DIR}/autosar-iox-roudi.service"
install -m 644 "${VSOMEIP_ROUTING_SERVICE_TEMPLATE}" "${SYSTEMD_DIR}/autosar-vsomeip-routing.service"
install -m 644 "${PLATFORM_SERVICE_TEMPLATE}" "${SYSTEMD_DIR}/autosar-platform-app.service"
install -m 644 "${ECU_SERVICE_TEMPLATE}" "${SYSTEMD_DIR}/autosar-ecu-full-stack.service"
install -m 644 "${EXEC_SERVICE_TEMPLATE}" "${SYSTEMD_DIR}/autosar-exec-manager.service"
install -m 644 "${TIME_SYNC_SERVICE_TEMPLATE}" "${SYSTEMD_DIR}/autosar-time-sync.service"
install -m 644 "${PERSISTENCY_SERVICE_TEMPLATE}" "${SYSTEMD_DIR}/autosar-persistency-guard.service"
install -m 644 "${IAM_POLICY_SERVICE_TEMPLATE}" "${SYSTEMD_DIR}/autosar-iam-policy.service"
install -m 644 "${CAN_MANAGER_SERVICE_TEMPLATE}" "${SYSTEMD_DIR}/autosar-can-manager.service"
install -m 644 "${WATCHDOG_SERVICE_TEMPLATE}" "${SYSTEMD_DIR}/autosar-watchdog.service"
install -m 644 "${USER_APP_MONITOR_SERVICE_TEMPLATE}" "${SYSTEMD_DIR}/autosar-user-app-monitor.service"

enable_if_binary_exists() {
  local unit_name="$1"
  local binary_path="$2"

  if [[ -x "${binary_path}" ]]; then
    systemctl enable "${unit_name}"
  else
    echo "[WARN] Skip enable ${unit_name}: binary not found at ${binary_path}"
  fi
}

if [[ "${RUN_SYSTEMCTL}" == "ON" ]]; then
  systemctl daemon-reload

  if [[ "${ENABLE_UNITS}" == "ON" ]]; then
    systemctl enable autosar-iox-roudi.service
    enable_if_binary_exists autosar-vsomeip-routing.service "${AUTOSAR_AP_PREFIX}/bin/autosar_vsomeip_routing_manager"
    enable_if_binary_exists autosar-time-sync.service "${AUTOSAR_AP_PREFIX}/bin/autosar_time_sync_daemon"
    enable_if_binary_exists autosar-persistency-guard.service "${AUTOSAR_AP_PREFIX}/bin/autosar_persistency_guard"
    enable_if_binary_exists autosar-iam-policy.service "${AUTOSAR_AP_PREFIX}/bin/autosar_iam_policy_loader"
    enable_if_binary_exists autosar-can-manager.service "${AUTOSAR_AP_PREFIX}/bin/autosar_can_interface_manager"
    enable_if_binary_exists autosar-watchdog.service "${AUTOSAR_AP_PREFIX}/bin/autosar_watchdog_supervisor"
    enable_if_binary_exists autosar-user-app-monitor.service "${AUTOSAR_AP_PREFIX}/bin/autosar_user_app_monitor"

    if [[ -x "${AUTOSAR_AP_PREFIX}/bin/adaptive_autosar" ]]; then
      systemctl enable autosar-platform-app.service
      systemctl enable autosar-exec-manager.service
    else
      echo "[WARN] Skip enable autosar-platform-app.service/autosar-exec-manager.service: adaptive_autosar not found under ${AUTOSAR_AP_PREFIX}/bin"
    fi

    systemctl enable autosar-ecu-full-stack.service
  fi
else
  echo "[INFO] --skip-systemctl specified. Skipping systemctl daemon-reload/enable."
fi

echo "[OK] Raspberry Pi ECU services installed."
echo "[INFO] Wrapper: ${WRAPPER_OUTPUT}"
echo "[INFO] vSomeIP routing wrapper: ${VSOMEIP_ROUTING_WRAPPER_OUTPUT}"
echo "[INFO] Platform wrapper: ${PLATFORM_WRAPPER_OUTPUT}"
echo "[INFO] Exec-manager wrapper: ${EXEC_WRAPPER_OUTPUT}"
echo "[INFO] Time-sync wrapper: ${TIME_SYNC_WRAPPER_OUTPUT}"
echo "[INFO] Persistency wrapper: ${PERSISTENCY_WRAPPER_OUTPUT}"
echo "[INFO] IAM policy wrapper: ${IAM_POLICY_WRAPPER_OUTPUT}"
echo "[INFO] CAN manager wrapper: ${CAN_MANAGER_WRAPPER_OUTPUT}"
echo "[INFO] Watchdog wrapper: ${WATCHDOG_WRAPPER_OUTPUT}"
echo "[INFO] User app monitor wrapper: ${USER_APP_MONITOR_WRAPPER_OUTPUT}"

echo "[INFO] Env file: ${ENV_OUTPUT}"
echo "[INFO] vSomeIP routing env file: ${VSOMEIP_ROUTING_ENV_OUTPUT}"
echo "[INFO] Platform env file: ${PLATFORM_ENV_OUTPUT}"
echo "[INFO] Exec-manager env file: ${EXEC_ENV_OUTPUT}"
echo "[INFO] Time-sync env file: ${TIME_SYNC_ENV_OUTPUT}"
echo "[INFO] Persistency env file: ${PERSISTENCY_ENV_OUTPUT}"
echo "[INFO] IAM policy env file: ${IAM_POLICY_ENV_OUTPUT}"
echo "[INFO] CAN manager env file: ${CAN_MANAGER_ENV_OUTPUT}"
echo "[INFO] Watchdog env file: ${WATCHDOG_ENV_OUTPUT}"
echo "[INFO] User app monitor env file: ${USER_APP_MONITOR_ENV_OUTPUT}"
echo "[INFO] IAM policy file: ${IAM_POLICY_OUTPUT}"

echo "[INFO] Bringup script: ${BRINGUP_OUTPUT}"
echo "[INFO] Startup alias script: ${STARTUP_OUTPUT}"

echo "[INFO] Services:"
echo "       ${SYSTEMD_DIR}/autosar-iox-roudi.service"
echo "       ${SYSTEMD_DIR}/autosar-vsomeip-routing.service"
echo "       ${SYSTEMD_DIR}/autosar-time-sync.service"
echo "       ${SYSTEMD_DIR}/autosar-persistency-guard.service"
echo "       ${SYSTEMD_DIR}/autosar-iam-policy.service"
echo "       ${SYSTEMD_DIR}/autosar-can-manager.service"
echo "       ${SYSTEMD_DIR}/autosar-platform-app.service"
echo "       ${SYSTEMD_DIR}/autosar-exec-manager.service"
echo "       ${SYSTEMD_DIR}/autosar-watchdog.service"
echo "       ${SYSTEMD_DIR}/autosar-user-app-monitor.service"
echo "       ${SYSTEMD_DIR}/autosar-ecu-full-stack.service"

echo "[INFO] Next commands:"
echo "       sudo systemctl start autosar-iox-roudi.service"
echo "       sudo systemctl start autosar-vsomeip-routing.service"
echo "       sudo systemctl start autosar-time-sync.service"
echo "       sudo systemctl start autosar-persistency-guard.service"
echo "       sudo systemctl start autosar-iam-policy.service"
echo "       sudo systemctl start autosar-can-manager.service"
echo "       sudo systemctl start autosar-platform-app.service"
echo "       sudo systemctl start autosar-exec-manager.service"
echo "       sudo systemctl start autosar-watchdog.service"
echo "       sudo systemctl start autosar-user-app-monitor.service"
echo "       sudo systemctl start autosar-ecu-full-stack.service"
echo "       sudo systemctl status autosar-vsomeip-routing.service --no-pager"
echo "       sudo systemctl status autosar-platform-app.service --no-pager"
echo "       sudo systemctl status autosar-exec-manager.service --no-pager"
echo "       sudo systemctl status autosar-watchdog.service --no-pager"
echo "       sudo systemctl status autosar-user-app-monitor.service --no-pager"

echo "[INFO] If you launch apps through bringup.sh, you can disable the fixed ECU template service:"
echo "       sudo systemctl disable --now autosar-ecu-full-stack.service"
