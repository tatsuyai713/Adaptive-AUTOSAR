#!/usr/bin/env bash
set -euo pipefail

# =============================================================================
# deploy_to_qnx_target.sh
#
# Transfers the AUTOSAR AP deployment image to a QNX target via SSH/SCP,
# then sets it up on the target (extract tar.gz or mount IFS image).
#
# Usage:
#   ./qnx/scripts/deploy_to_qnx_target.sh [options]
#
# Options:
#   --host   <ip|hostname>   QNX target IP or hostname  [required]
#   --port   <port>          SSH port (default: 22)
#   --user   <user>          SSH user (default: root)
#   --image  <path>          Image file to transfer.
#                            If omitted, looks for out/qnx/deploy/*.tar.gz
#   --image-type tar|ifs     Override auto-detection (default: auto)
#   --remote-dir <path>      Mount/install path on target
#                            (default: /autosar)
#   --target-ip <ip>         IP to write into vsomeip config on target
#                            (defaults to --host value)
#   --no-transfer            Skip SCP, assume image already on target
#   --dry-run                Print commands without executing
#   --arch   <aarch64le>     Architecture tag (default: aarch64le)
# =============================================================================

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "${SCRIPT_DIR}/common.sh"

SSH_HOST=""
SSH_PORT="22"
SSH_USER="root"
IMAGE_FILE=""
IMAGE_TYPE="auto"
REMOTE_DIR="/autosar"
TARGET_IP=""
SKIP_TRANSFER="OFF"
DRY_RUN="OFF"
ARCH="$(qnx_default_arch)"

while [[ $# -gt 0 ]]; do
  case "$1" in
    --host)         SSH_HOST="$2"; shift 2 ;;
    --port)         SSH_PORT="$2"; shift 2 ;;
    --user)         SSH_USER="$2"; shift 2 ;;
    --image)        IMAGE_FILE="$2"; shift 2 ;;
    --image-type)   IMAGE_TYPE="$2"; shift 2 ;;
    --remote-dir)   REMOTE_DIR="$2"; shift 2 ;;
    --target-ip)    TARGET_IP="$2"; shift 2 ;;
    --no-transfer)  SKIP_TRANSFER="ON"; shift ;;
    --dry-run)      DRY_RUN="ON"; shift ;;
    --arch)         ARCH="$2"; shift 2 ;;
    *) qnx_die "Unknown argument: $1" ;;
  esac
done

[[ -z "${SSH_HOST}" ]] && qnx_die "Required: --host <ip>"
[[ -z "${TARGET_IP}" ]] && TARGET_IP="${SSH_HOST}"

# ---------------------------------------------------------------------------
# Auto-detect image if not specified
# ---------------------------------------------------------------------------
if [[ -z "${IMAGE_FILE}" ]]; then
  DEPLOY_DIR="${REPO_ROOT}/out/qnx/deploy"
  # Prefer tar.gz; fall back to IFS
  if ls "${DEPLOY_DIR}/autosar_ap-${ARCH}.tar.gz" &>/dev/null; then
    IMAGE_FILE="${DEPLOY_DIR}/autosar_ap-${ARCH}.tar.gz"
  elif ls "${DEPLOY_DIR}/autosar_ap-${ARCH}.ifs" &>/dev/null; then
    IMAGE_FILE="${DEPLOY_DIR}/autosar_ap-${ARCH}.ifs"
  else
    qnx_die "No image found in ${DEPLOY_DIR}. Run create_qnx_deploy_image.sh first."
  fi
fi

[[ -f "${IMAGE_FILE}" ]] || qnx_die "Image file not found: ${IMAGE_FILE}"

# ---------------------------------------------------------------------------
# Auto-detect image type
# ---------------------------------------------------------------------------
if [[ "${IMAGE_TYPE}" == "auto" ]]; then
  case "${IMAGE_FILE}" in
    *.tar.gz|*.tgz) IMAGE_TYPE="tar" ;;
    *.ifs)          IMAGE_TYPE="ifs" ;;
    *) qnx_die "Cannot detect image type from filename: ${IMAGE_FILE}. Use --image-type." ;;
  esac
fi

IMAGE_SIZE="$(du -sh "${IMAGE_FILE}" | cut -f1)"
REMOTE_IMAGE_PATH="/tmp/$(basename "${IMAGE_FILE}")"

qnx_info "====== QNX Target Deployment ======"
qnx_info "  target       : ${SSH_USER}@${SSH_HOST}:${SSH_PORT}"
qnx_info "  image        : ${IMAGE_FILE} (${IMAGE_SIZE}, type=${IMAGE_TYPE})"
qnx_info "  remote_dir   : ${REMOTE_DIR}"
qnx_info "  target_ip    : ${TARGET_IP}"

# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------
SSH_OPTS=(-o StrictHostKeyChecking=no -o ConnectTimeout=10 -p "${SSH_PORT}")

_ssh() {
  if [[ "${DRY_RUN}" == "ON" ]]; then
    echo "[DRY-RUN] ssh ${SSH_USER}@${SSH_HOST}: $*"
    return 0
  fi
  ssh "${SSH_OPTS[@]}" "${SSH_USER}@${SSH_HOST}" "$@"
}

_scp() {
  if [[ "${DRY_RUN}" == "ON" ]]; then
    echo "[DRY-RUN] scp $*"
    return 0
  fi
  scp -P "${SSH_PORT}" "${SSH_OPTS[@]}" "$@"
}

# ---------------------------------------------------------------------------
# Check connectivity
# ---------------------------------------------------------------------------
qnx_info "Checking SSH connectivity to ${SSH_HOST}..."
if [[ "${DRY_RUN}" != "ON" ]]; then
  if ! ssh "${SSH_OPTS[@]}" "${SSH_USER}@${SSH_HOST}" "uname -a" &>/dev/null; then
    qnx_die "Cannot connect to ${SSH_HOST}. Check host, port, credentials."
  fi
  qnx_info "  Connected OK"
fi

# ---------------------------------------------------------------------------
# Transfer image
# ---------------------------------------------------------------------------
if [[ "${SKIP_TRANSFER}" != "ON" ]]; then
  qnx_info "Transferring image to target: ${REMOTE_IMAGE_PATH}"
  _scp "${IMAGE_FILE}" "${SSH_USER}@${SSH_HOST}:${REMOTE_IMAGE_PATH}"
  qnx_info "  Transfer complete"
fi

# ---------------------------------------------------------------------------
# Setup on target — TAR approach (extract)
# ---------------------------------------------------------------------------
if [[ "${IMAGE_TYPE}" == "tar" ]]; then
  qnx_info "Extracting tar image on target -> ${REMOTE_DIR}"
  _ssh bash -c "
    set -e
    # Remove old deployment if exists
    if [ -d '${REMOTE_DIR}' ]; then
      echo '[AUTOSAR] Removing old deployment at ${REMOTE_DIR}'
      rm -rf '${REMOTE_DIR}'
    fi
    mkdir -p '${REMOTE_DIR}'

    # Extract: the tar contains a single top-level directory (autosar_ap-<arch>)
    # We strip the first path component and extract into REMOTE_DIR
    echo '[AUTOSAR] Extracting...'
    cd '$(dirname "${REMOTE_DIR}")'
    tar -xzf '${REMOTE_IMAGE_PATH}' --strip-components=1 \
        -C '${REMOTE_DIR}'

    # Update vsomeip unicast IP to actual target IP
    VSOMEIP_CFG='${REMOTE_DIR}/etc/vsomeip_routing.json'
    if [ -f \"\${VSOMEIP_CFG}\" ]; then
      sed -i 's|\"unicast\" : \"[^\"]*\"|\"unicast\" : \"${TARGET_IP}\"|g' \
          \"\${VSOMEIP_CFG}\"
      sed -i 's|\"unicast\" : \"[^\"]*\"|\"unicast\" : \"${TARGET_IP}\"|g' \
          '${REMOTE_DIR}/etc/vsomeip_client.json' 2>/dev/null || true
    fi

    # Mark executables
    chmod +x '${REMOTE_DIR}/bin/'* 2>/dev/null || true
    chmod +x '${REMOTE_DIR}/startup.sh' '${REMOTE_DIR}/env.sh' 2>/dev/null || true

    # Clean up transferred image
    rm -f '${REMOTE_IMAGE_PATH}'
    echo '[AUTOSAR] Extraction complete'
    ls -la '${REMOTE_DIR}/bin/' | head -20
  "
fi

# ---------------------------------------------------------------------------
# Setup on target — IFS approach (devb-ram + mount)
# ---------------------------------------------------------------------------
if [[ "${IMAGE_TYPE}" == "ifs" ]]; then
  # Calculate RAM device size needed (image size in 512-byte sectors + 20% headroom)
  IMAGE_BYTES="$(stat -c%s "${IMAGE_FILE}" 2>/dev/null || stat -f%z "${IMAGE_FILE}")"
  # Add 20% and round up to 512-byte sector boundary
  RAM_SECTORS=$(( (IMAGE_BYTES * 120 / 100 + 511) / 512 ))

  qnx_info "Mounting IFS image on target (RAM sectors: ${RAM_SECTORS})"
  _ssh bash -c "
    set -e
    IMAGE='${REMOTE_IMAGE_PATH}'
    MOUNT='${REMOTE_DIR}'

    # Unmount previous mount if any
    if mount | grep -q '\${MOUNT}'; then
      echo '[AUTOSAR] Unmounting previous mount at \${MOUNT}'
      umount '\${MOUNT}' 2>/dev/null || true
    fi

    # Kill any previous devb-ram instance for unit 9
    slay -f devb-ram 2>/dev/null || true
    sleep 0.5

    # Create a RAM block device
    echo '[AUTOSAR] Creating RAM block device (${RAM_SECTORS} sectors)'
    devb-ram unit=9 blk_size=512 capacity=${RAM_SECTORS} &
    sleep 1

    [ -b /dev/ram9 ] || { echo 'ERROR: /dev/ram9 not created'; exit 1; }

    # Write IFS image to RAM device
    echo '[AUTOSAR] Writing IFS image to /dev/ram9'
    dd if='\${IMAGE}' of=/dev/ram9 bs=64k

    # Mount as IFS filesystem
    mkdir -p '\${MOUNT}'
    echo '[AUTOSAR] Mounting IFS at \${MOUNT}'
    mount -t ifs /dev/ram9 '\${MOUNT}'

    # Update vsomeip config in place (IFS is read-only, copy to /tmp)
    if [ -d '\${MOUNT}/etc' ]; then
      mkdir -p '\${MOUNT}_etc'
      cp -r '\${MOUNT}/etc' '\${MOUNT}_etc/'
      sed -i 's|\"unicast\" : \"[^\"]*\"|\"unicast\" : \"${TARGET_IP}\"|g' \
          '\${MOUNT}_etc/etc/vsomeip_routing.json' 2>/dev/null || true
      echo '[AUTOSAR] vsomeip config updated (in /tmp)'
      echo '[AUTOSAR] Set: VSOMEIP_CONFIGURATION=\${MOUNT}_etc/etc/vsomeip_routing.json'
    fi

    # Clean up transferred image (save RAM)
    rm -f '\${IMAGE}'
    echo '[AUTOSAR] IFS mounted at \${MOUNT}'
    ls '\${MOUNT}/'
  "
fi

# ---------------------------------------------------------------------------
# Print usage instructions on target
# ---------------------------------------------------------------------------
qnx_info ""
qnx_info "====== Deployment complete ======"
qnx_info ""
qnx_info "To start AUTOSAR AP on the target:"
qnx_info ""
qnx_info "  ssh ${SSH_USER}@${SSH_HOST}"
qnx_info "  # Start all daemons:"
qnx_info "  ${REMOTE_DIR}/startup.sh --all"
qnx_info ""
qnx_info "  # Or start only vsomeip routing manager:"
qnx_info "  ${REMOTE_DIR}/startup.sh --routing-only"
qnx_info ""
qnx_info "  # Stop all:"
qnx_info "  ${REMOTE_DIR}/startup.sh --stop"
qnx_info ""
qnx_info "  # Run a user app manually:"
qnx_info "  . ${REMOTE_DIR}/env.sh"
qnx_info "  VSOMEIP_CONFIGURATION=${REMOTE_DIR}/etc/vsomeip_client.json \\"
qnx_info "    ${REMOTE_DIR}/bin/autosar_user_minimal_runtime"
