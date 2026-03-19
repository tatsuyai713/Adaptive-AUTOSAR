#!/bin/sh
# =============================================================================
# rc.autosar.sh — QNX boot-time auto-start for AUTOSAR Adaptive Platform
#
# This script is a fallback for QNX targets that do not have SLM (Service
# Launch Manager) available.  It is designed to be called from the QNX
# startup sequence:
#
#   Option A — IFS build file:
#     [+script] .script = {
#         ...
#         /autosar/rc.autosar.sh &
#     }
#
#   Option B — /etc/rc.d integration:
#     ln -s /autosar/rc.autosar.sh /etc/rc.d/rc.autosar
#     # Ensure /etc/rc.d/rc.local sources /etc/rc.d/rc.autosar
#
#   Option C — Manual invocation:
#     /autosar/rc.autosar.sh &
#
# Stop all daemons:
#   /autosar/rc.autosar.sh --stop
#
# =============================================================================
set -u

AUTOSAR_ROOT="${AUTOSAR_ROOT:-/autosar}"

# ---------------------------------------------------------------------------
# Environment
# ---------------------------------------------------------------------------
export LD_LIBRARY_PATH="${AUTOSAR_ROOT}/lib:${LD_LIBRARY_PATH:-}"
export VSOMEIP_CONFIGURATION="${AUTOSAR_ROOT}/etc/vsomeip_routing.json"
export VSOMEIP_APPLICATION_NAME="autosar_vsomeip_routing_manager"
export AUTOSAR_AP_PERSISTENCY_ROOT="${AUTOSAR_ROOT}/var/persistency"

export AUTOSAR_RUN_DIR="${AUTOSAR_ROOT}/run/autosar"
export AUTOSAR_PHM_HEALTH_DIR="${AUTOSAR_ROOT}/run/autosar/phm/health"
export AUTOSAR_USER_APP_REGISTRY_FILE="${AUTOSAR_ROOT}/run/autosar/user_apps_registry.csv"
export AUTOSAR_USER_APP_STATUS_DIR="${AUTOSAR_ROOT}/run/autosar/user_apps"
export AUTOSAR_USER_APP_MONITOR_STATUS_FILE="${AUTOSAR_ROOT}/run/autosar/user_app_monitor.status"

export AUTOSAR_DLT_STATUS_FILE="${AUTOSAR_ROOT}/run/autosar/dlt_daemon.status"
export AUTOSAR_WATCHDOG_STATUS_FILE="${AUTOSAR_ROOT}/run/autosar/watchdog.status"
export AUTOSAR_NM_STATUS_FILE="${AUTOSAR_ROOT}/run/autosar/network_manager.status"
export AUTOSAR_NM_TRIGGER_DIR="${AUTOSAR_ROOT}/run/autosar/nm_triggers"
export AUTOSAR_TIMESYNC_STATUS_FILE="${AUTOSAR_ROOT}/run/autosar/time_sync.status"
export AUTOSAR_NTP_STATUS_FILE="${AUTOSAR_ROOT}/run/autosar/ntp_time_provider.status"
export AUTOSAR_PTP_STATUS_FILE="${AUTOSAR_ROOT}/run/autosar/ptp_time_provider.status"
export AUTOSAR_SM_STATUS_FILE="${AUTOSAR_ROOT}/run/autosar/sm_state.status"
export AUTOSAR_IAM_STATUS_FILE="${AUTOSAR_ROOT}/run/autosar/iam_policy_loader.status"
export AUTOSAR_PERSISTENCY_STATUS_FILE="${AUTOSAR_ROOT}/run/autosar/persistency_guard.status"
export AUTOSAR_UCM_STATUS_FILE="${AUTOSAR_ROOT}/run/autosar/ucm_daemon.status"
export AUTOSAR_UCM_STAGING_DIR="${AUTOSAR_ROOT}/var/ucm/staging"
export AUTOSAR_UCM_PROCESSED_DIR="${AUTOSAR_ROOT}/var/ucm/processed"
export AUTOSAR_CAN_MANAGER_STATUS_FILE="${AUTOSAR_ROOT}/run/autosar/can_manager.status"
export AUTOSAR_CAN_TRIGGER_DIR="${AUTOSAR_ROOT}/run/autosar/can_triggers"
export AUTOSAR_PHM_STATUS_FILE="${AUTOSAR_ROOT}/run/autosar/phm.status"
export AUTOSAR_DIAG_STATUS_FILE="${AUTOSAR_ROOT}/run/autosar/diag_server.status"

PID_DIR="${AUTOSAR_ROOT}/run/pids"

# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------
log() { echo "[AUTOSAR-RC] $*"; }

create_runtime_dirs() {
    mkdir -p "${AUTOSAR_ROOT}/run/autosar/phm/health"
    mkdir -p "${AUTOSAR_ROOT}/run/autosar/user_apps"
    mkdir -p "${AUTOSAR_ROOT}/run/autosar/nm_triggers"
    mkdir -p "${AUTOSAR_ROOT}/run/autosar/can_triggers"
    mkdir -p "${AUTOSAR_ROOT}/var/ucm/staging"
    mkdir -p "${AUTOSAR_ROOT}/var/ucm/processed"
    mkdir -p "${AUTOSAR_ROOT}/var/persistency"
    mkdir -p "${AUTOSAR_ROOT}/log"
    mkdir -p "${PID_DIR}"
}

start_daemon() {
    local name="$1"
    shift
    local bin="${AUTOSAR_ROOT}/bin/${name}"
    if [ ! -x "${bin}" ]; then
        log "SKIP: ${name} (not found)"
        return
    fi
    log "Starting ${name}"
    "${bin}" "$@" &
    echo $! > "${PID_DIR}/${name}.pid"
}

stop_all() {
    log "Stopping all AUTOSAR AP daemons"
    if [ -d "${PID_DIR}" ]; then
        for pidfile in "${PID_DIR}"/*.pid; do
            [ -f "${pidfile}" ] || continue
            pid="$(cat "${pidfile}")"
            name="$(basename "${pidfile}" .pid)"
            if kill -0 "${pid}" 2>/dev/null; then
                log "  Stopping ${name} (PID ${pid})"
                kill "${pid}" 2>/dev/null
            fi
            rm -f "${pidfile}"
        done
    fi

    # Wait briefly for graceful shutdown
    sleep 2

    # Force-kill any remaining AUTOSAR processes
    for pidfile in "${PID_DIR}"/*.pid; do
        [ -f "${pidfile}" ] || continue
        pid="$(cat "${pidfile}")"
        kill -9 "${pid}" 2>/dev/null || true
        rm -f "${pidfile}"
    done

    log "All daemons stopped"
}

status_all() {
    log "AUTOSAR AP daemon status:"
    if [ -d "${PID_DIR}" ]; then
        for pidfile in "${PID_DIR}"/*.pid; do
            [ -f "${pidfile}" ] || continue
            pid="$(cat "${pidfile}")"
            name="$(basename "${pidfile}" .pid)"
            if kill -0 "${pid}" 2>/dev/null; then
                echo "  [RUNNING] ${name} (PID ${pid})"
            else
                echo "  [STOPPED] ${name} (stale PID ${pid})"
                rm -f "${pidfile}"
            fi
        done
    else
        echo "  No PID directory found"
    fi
}

# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------
MODE="${1:-start}"

case "${MODE}" in
    --stop|stop)
        stop_all
        exit 0
        ;;
    --status|status)
        status_all
        exit 0
        ;;
    --restart|restart)
        stop_all
        sleep 1
        MODE="start"
        ;;
esac

# Start sequence
create_runtime_dirs
log "Starting AUTOSAR Adaptive Platform daemons"
log "  AUTOSAR_ROOT=${AUTOSAR_ROOT}"

# --- Tier 0: vsomeip routing manager (must start first) ---
VSOMEIP_CONFIGURATION="${AUTOSAR_ROOT}/etc/vsomeip_routing.json" \
    start_daemon autosar_vsomeip_routing_manager
sleep 1

# --- Tier 1: Core platform services (parallel) ---
start_daemon autosar_dlt_daemon
start_daemon autosar_time_sync_daemon
start_daemon autosar_persistency_guard
start_daemon autosar_iam_policy_loader
start_daemon autosar_network_manager
start_daemon autosar_can_interface_manager
sleep 1

# --- Tier 2: Platform monitoring and specialized services ---
start_daemon autosar_sm_state_daemon
start_daemon autosar_watchdog_supervisor
start_daemon autosar_phm_daemon
start_daemon autosar_diag_server
start_daemon autosar_crypto_provider
start_daemon autosar_ucm_daemon
start_daemon autosar_ntp_time_provider
start_daemon autosar_ptp_time_provider
sleep 1

# --- Tier 3: Main platform application ---
VSOMEIP_CONFIGURATION="${AUTOSAR_ROOT}/etc/vsomeip_client.json" \
    start_daemon adaptive_autosar
sleep 2

# --- Tier 4: User app monitor + user app bringup ---
start_daemon autosar_user_app_monitor

# Launch user applications via bringup script
if [ -x "${AUTOSAR_ROOT}/bringup_qnx.sh" ]; then
    log "Executing user app bringup: ${AUTOSAR_ROOT}/bringup_qnx.sh"
    VSOMEIP_CONFIGURATION="${AUTOSAR_ROOT}/etc/vsomeip_client.json" \
        "${AUTOSAR_ROOT}/bringup_qnx.sh"
else
    log "No bringup_qnx.sh found (skip user app launch)"
fi

log "AUTOSAR AP startup complete. PID files in ${PID_DIR}/"
