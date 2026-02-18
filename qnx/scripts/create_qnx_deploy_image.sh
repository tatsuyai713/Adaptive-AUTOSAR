#!/usr/bin/env bash
set -euo pipefail

# =============================================================================
# create_qnx_deploy_image.sh
#
# Gathers AUTOSAR AP platform daemons, user apps, and all required shared
# libraries into a deployment staging directory, then produces:
#
#   1. <output>/<name>.tar.gz   — portable tarball (extract on target)
#   2. <output>/<name>.ifs      — QNX IFS image (mount via devb-ram + mkifs)
#
# Usage:
#   ./qnx/scripts/create_qnx_deploy_image.sh [options]
#
# Options:
#   --arch     <aarch64le>   Target architecture (default: aarch64le)
#   --out-root <dir>         Base dir where middleware was installed
#                            (default: /opt/qnx)
#   --user-apps-build <dir>  User apps build directory
#   --image-name <name>      Base name for output image files
#                            (default: autosar_ap-aarch64le)
#   --output-dir <dir>       Directory to write images into
#                            (default: out/qnx/deploy)
#   --no-ifs                 Skip IFS image creation (mkifs required)
#   --no-tar                 Skip tar.gz creation
#   --target-ip <ip>         QNX target IP (written into vsomeip config)
# =============================================================================

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "${SCRIPT_DIR}/common.sh"

ARCH="$(qnx_default_arch)"
OUT_ROOT="$(qnx_default_out_root)"
USER_APPS_BUILD_DIR=""
IMAGE_NAME=""
OUTPUT_DIR="${REPO_ROOT}/out/qnx/deploy"
CREATE_IFS="ON"
CREATE_TAR="ON"
TARGET_IP="192.168.0.100"

while [[ $# -gt 0 ]]; do
  case "$1" in
    --arch)           ARCH="$2"; shift 2 ;;
    --out-root)       OUT_ROOT="$2"; shift 2 ;;
    --user-apps-build) USER_APPS_BUILD_DIR="$2"; shift 2 ;;
    --image-name)     IMAGE_NAME="$2"; shift 2 ;;
    --output-dir)     OUTPUT_DIR="$2"; shift 2 ;;
    --no-ifs)         CREATE_IFS="OFF"; shift ;;
    --no-tar)         CREATE_TAR="OFF"; shift ;;
    --target-ip)      TARGET_IP="$2"; shift 2 ;;
    *) qnx_die "Unknown argument: $1" ;;
  esac
done

[[ -z "${IMAGE_NAME}" ]] && IMAGE_NAME="autosar_ap-${ARCH}"

AUTOSAR_AP_BIN="${OUT_ROOT}/autosar_ap/${ARCH}/bin"
VSOMEIP_LIB="${OUT_ROOT}/vsomeip/lib"
CYCLONEDDS_LIB="${OUT_ROOT}/cyclonedds/lib"
ICEORYX_LIB="${OUT_ROOT}/iceoryx/lib"

if [[ -z "${USER_APPS_BUILD_DIR}" ]]; then
  USER_APPS_BUILD_DIR="${AUTOSAR_QNX_WORK_ROOT:-${REPO_ROOT}/out/qnx/work}/build/user_apps-${ARCH}"
fi

STAGING="${OUTPUT_DIR}/.staging/${IMAGE_NAME}"
QNX_DEPLOY_DIR="${QNX_ROOT}/deploy"

qnx_info "Creating QNX deployment image: ${IMAGE_NAME}"
qnx_info "  arch         = ${ARCH}"
qnx_info "  out_root     = ${OUT_ROOT}"
qnx_info "  staging      = ${STAGING}"
qnx_info "  output_dir   = ${OUTPUT_DIR}"
qnx_info "  target_ip    = ${TARGET_IP}"

# ---------------------------------------------------------------------------
# 1. Prepare staging directory
# ---------------------------------------------------------------------------
rm -rf "${STAGING}"
mkdir -p "${STAGING}/bin" "${STAGING}/lib" "${STAGING}/etc" \
         "${STAGING}/run/autosar/phm/health" \
         "${STAGING}/run/autosar/user_apps" \
         "${STAGING}/run/autosar/nm_triggers" \
         "${STAGING}/run/autosar/can_triggers" \
         "${STAGING}/log" \
         "${STAGING}/var/persistency" \
         "${STAGING}/var/ucm/staging" \
         "${STAGING}/var/ucm/processed"

# ---------------------------------------------------------------------------
# 2. Copy platform daemons
# ---------------------------------------------------------------------------
if [[ ! -d "${AUTOSAR_AP_BIN}" ]]; then
  qnx_die "Platform daemons not found: ${AUTOSAR_AP_BIN}"
fi
qnx_info "Copying platform daemons from ${AUTOSAR_AP_BIN}"
cp "${AUTOSAR_AP_BIN}/"* "${STAGING}/bin/"

# ---------------------------------------------------------------------------
# 3. Copy user apps (all executables from build tree, skip libs/cmake files)
# ---------------------------------------------------------------------------
if [[ -d "${USER_APPS_BUILD_DIR}" ]]; then
  qnx_info "Copying user apps from ${USER_APPS_BUILD_DIR}"
  find "${USER_APPS_BUILD_DIR}/src" -maxdepth 4 -type f -perm /u+x \
    ! -name "*.cmake" ! -name "*.a" ! -name "*.so*" \
    -exec cp {} "${STAGING}/bin/" \;
else
  qnx_warn "User apps build dir not found (skip): ${USER_APPS_BUILD_DIR}"
fi

# ---------------------------------------------------------------------------
# 4. Collect shared libraries (preserve symlinks)
# ---------------------------------------------------------------------------
_copy_libs() {
  local src_dir="$1"
  [[ -d "${src_dir}" ]] || return 0
  # Copy real files first
  find "${src_dir}" -maxdepth 1 -type f -name "*.so*" \
    -exec cp -P {} "${STAGING}/lib/" \;
  # Copy symlinks
  find "${src_dir}" -maxdepth 1 -type l -name "*.so*" \
    -exec cp -P {} "${STAGING}/lib/" \;
}

qnx_info "Collecting shared libraries"
_copy_libs "${VSOMEIP_LIB}"
_copy_libs "${CYCLONEDDS_LIB}"
_copy_libs "${ICEORYX_LIB}"

# Remove any static archives or cmake files that may have ended up in lib
find "${STAGING}/lib" -name "*.a" -delete 2>/dev/null || true

# ---------------------------------------------------------------------------
# 5. Generate vsomeip config for QNX target
# ---------------------------------------------------------------------------
qnx_info "Generating vsomeip configuration (target IP: ${TARGET_IP})"
cat > "${STAGING}/etc/vsomeip_routing.json" <<JSON
{
    "unicast" : "${TARGET_IP}",
    "netmask" : "255.255.255.0",
    "logging" : {
        "level" : "debug",
        "console" : "true",
        "file"    : { "enable" : "false" }
    },
    "routing" : {
        "host" : {
            "name"    : "autosar_vsomeip_routing_manager",
            "unicast" : "${TARGET_IP}"
        }
    },
    "service-discovery" : {
        "enable"                    : "true",
        "multicast"                 : "224.0.0.251",
        "port"                      : "30490",
        "protocol"                  : "udp",
        "initial_delay_min"         : "10",
        "initial_delay_max"         : "100",
        "repetitions_base_delay"    : "200",
        "repetitions_max"           : "3",
        "ttl"                       : "3",
        "cyclic_offer_delay"        : "2000",
        "request_response_delay"    : "1500"
    }
}
JSON

cat > "${STAGING}/etc/vsomeip_client.json" <<JSON
{
    "unicast"                  : "${TARGET_IP}",
    "netmask"                  : "255.255.255.0",
    "logging" : {
        "level"   : "info",
        "console" : "true"
    },
    "routing"                  : "autosar_vsomeip_routing_manager",
    "service-discovery" : {
        "enable"   : "true",
        "multicast": "224.0.0.251",
        "port"     : "30490",
        "protocol" : "udp"
    }
}
JSON

# ---------------------------------------------------------------------------
# 6. Generate environment and startup scripts
# ---------------------------------------------------------------------------
qnx_info "Generating startup scripts"

DEPLOY_DIR_ON_TARGET="/autosar"

cat > "${STAGING}/env.sh" <<'ENVSH'
#!/bin/sh
# AUTOSAR AP environment for QNX - source this before starting daemons
AUTOSAR_ROOT="$(cd "$(dirname "$0")" && pwd)"
export LD_LIBRARY_PATH="${AUTOSAR_ROOT}/lib:${LD_LIBRARY_PATH:-}"
export VSOMEIP_CONFIGURATION="${AUTOSAR_ROOT}/etc/vsomeip_routing.json"
export VSOMEIP_APPLICATION_NAME="autosar_vsomeip_routing_manager"
export AUTOSAR_AP_PERSISTENCY_ROOT="${AUTOSAR_ROOT}/var/persistency"
export TMPDIR="${AUTOSAR_ROOT}/run"

# QNX-compatible paths for AUTOSAR runtime directories
export AUTOSAR_RUN_DIR="${AUTOSAR_ROOT}/run/autosar"
export AUTOSAR_PHM_HEALTH_DIR="${AUTOSAR_ROOT}/run/autosar/phm/health"
export AUTOSAR_USER_APP_REGISTRY_FILE="${AUTOSAR_ROOT}/run/autosar/user_apps_registry.csv"
export AUTOSAR_USER_APP_STATUS_DIR="${AUTOSAR_ROOT}/run/autosar/user_apps"
export AUTOSAR_USER_APP_MONITOR_STATUS_FILE="${AUTOSAR_ROOT}/run/autosar/user_app_monitor.status"

# Status files for platform daemons
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
ENVSH

cat > "${STAGING}/startup.sh" <<'STARTUP'
#!/bin/sh
# AUTOSAR AP startup script for QNX target
# Usage: ./startup.sh [--routing-only | --all]

AUTOSAR_ROOT="$(cd "$(dirname "$0")" && pwd)"
. "${AUTOSAR_ROOT}/env.sh"

MODE="${1:-all}"

log() { echo "[AUTOSAR] $*"; }
die() { echo "[AUTOSAR][ERROR] $*" >&2; exit 1; }

start_daemon() {
    local name="$1"
    local bin="${AUTOSAR_ROOT}/bin/${name}"
    [ -x "${bin}" ] || { log "WARNING: ${name} not found, skip"; return; }
    log "Starting ${name}"
    "${bin}" &
    echo $! >> "${AUTOSAR_ROOT}/run/${name}.pid"
}

stop_all() {
    log "Stopping all AUTOSAR AP processes"
    for pidfile in "${AUTOSAR_ROOT}/run/"*.pid; do
        [ -f "${pidfile}" ] || continue
        pid="$(cat "${pidfile}")"
        kill "${pid}" 2>/dev/null || true
        rm -f "${pidfile}"
    done
}

# Create runtime directories required by AUTOSAR AP daemons
mkdir -p "${AUTOSAR_ROOT}/run/autosar/phm/health"
mkdir -p "${AUTOSAR_ROOT}/run/autosar/user_apps"
mkdir -p "${AUTOSAR_ROOT}/run/autosar/nm_triggers"
mkdir -p "${AUTOSAR_ROOT}/run/autosar/can_triggers"
mkdir -p "${AUTOSAR_ROOT}/var/ucm/staging"
mkdir -p "${AUTOSAR_ROOT}/var/ucm/processed"
mkdir -p "${AUTOSAR_ROOT}/log"

case "${MODE}" in
  --stop)
    stop_all
    ;;
  --routing-only)
    log "Starting vsomeip routing manager only"
    VSOMEIP_CONFIGURATION="${AUTOSAR_ROOT}/etc/vsomeip_routing.json"
    start_daemon autosar_vsomeip_routing_manager
    ;;
  --all | *)
    log "Starting AUTOSAR AP platform (all daemons)"
    # 1. vsomeip routing manager (must be first)
    VSOMEIP_CONFIGURATION="${AUTOSAR_ROOT}/etc/vsomeip_routing.json" \
      start_daemon autosar_vsomeip_routing_manager
    sleep 1
    # 2. Platform services
    start_daemon autosar_dlt_daemon
    start_daemon autosar_watchdog_supervisor
    start_daemon autosar_network_manager
    start_daemon autosar_time_sync_daemon
    start_daemon autosar_sm_state_daemon
    start_daemon autosar_iam_policy_loader
    start_daemon autosar_persistency_guard
    start_daemon autosar_ucm_daemon
    start_daemon autosar_user_app_monitor
    # 3. Main adaptive platform binary
    start_daemon adaptive_autosar
    ;;
esac

log "Done. PIDs stored in ${AUTOSAR_ROOT}/run/"
STARTUP

chmod +x "${STAGING}/startup.sh" "${STAGING}/env.sh"

# ---------------------------------------------------------------------------
# 7. Copy any existing deploy support files
# ---------------------------------------------------------------------------
if [[ -d "${QNX_DEPLOY_DIR}" ]]; then
  cp -r "${QNX_DEPLOY_DIR}/"* "${STAGING}/" 2>/dev/null || true
fi

# ---------------------------------------------------------------------------
# 8. Create tar.gz image
# ---------------------------------------------------------------------------
if [[ "${CREATE_TAR}" == "ON" ]]; then
  TAR_OUT="${OUTPUT_DIR}/${IMAGE_NAME}.tar.gz"
  mkdir -p "${OUTPUT_DIR}"
  qnx_info "Creating tar.gz: ${TAR_OUT}"
  tar -czf "${TAR_OUT}" -C "$(dirname "${STAGING}")" "$(basename "${STAGING}")"
  qnx_info "  size: $(du -sh "${TAR_OUT}" | cut -f1)"
fi

# ---------------------------------------------------------------------------
# 9. Create QNX IFS image (requires mkifs from QNX SDP)
# ---------------------------------------------------------------------------
if [[ "${CREATE_IFS}" == "ON" ]]; then
  MKIFS="$(command -v mkifs 2>/dev/null || echo "${QNX_HOST}/usr/bin/mkifs")"
  if [[ ! -x "${MKIFS}" ]]; then
    qnx_warn "mkifs not found — skipping IFS image (set QNX_HOST or use --no-ifs)"
  else
    IFS_BUILD="${OUTPUT_DIR}/${IMAGE_NAME}.build"
    IFS_OUT="${OUTPUT_DIR}/${IMAGE_NAME}.ifs"
    mkdir -p "${OUTPUT_DIR}"
    qnx_info "Generating mkifs build file: ${IFS_BUILD}"

    {
      echo "[image=${IFS_OUT}]"
      echo "[uid=0,gid=0]"
      # All files relative to staging, preserving directory structure
      find "${STAGING}" -type f | sort | while read -r f; do
        rel="${f#${STAGING}/}"
        perm="644"
        [[ -x "${f}" ]] && perm="755"
        echo "[perm=0${perm}] /${IMAGE_NAME}/${rel}=${f}"
      done
      # Recreate symlinks for shared libraries
      find "${STAGING}/lib" -type l | while read -r lnk; do
        rel="${lnk#${STAGING}/}"
        target="$(readlink "${lnk}")"
        echo "[type=link] /${IMAGE_NAME}/${rel}=${target}"
      done
    } > "${IFS_BUILD}"

    qnx_info "Building IFS image: ${IFS_OUT}"
    "${MKIFS}" "${IFS_BUILD}"
    qnx_info "  size: $(du -sh "${IFS_OUT}" | cut -f1)"
  fi
fi

# ---------------------------------------------------------------------------
# Summary
# ---------------------------------------------------------------------------
qnx_info "====== Deployment image created ======"
qnx_info "  Staging dir : ${STAGING}"
[[ "${CREATE_TAR}" == "ON" ]] && \
  qnx_info "  tar.gz      : ${OUTPUT_DIR}/${IMAGE_NAME}.tar.gz"
[[ "${CREATE_IFS}" == "ON" ]] && [[ -f "${OUTPUT_DIR}/${IMAGE_NAME}.ifs" ]] && \
  qnx_info "  IFS image   : ${OUTPUT_DIR}/${IMAGE_NAME}.ifs"
qnx_info ""
qnx_info "To deploy: ./qnx/scripts/deploy_to_qnx_target.sh --host <IP>"
