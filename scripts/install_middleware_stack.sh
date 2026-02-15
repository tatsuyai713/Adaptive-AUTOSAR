#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

USE_VSOMEIP="ON"
USE_ICEORYX="ON"
USE_CYCLONEDDS="ON"
INSTALL_BASE_DEPS="OFF"
SKIP_MIDDLEWARE_SYSTEM_DEPS="OFF"
FORCE_REINSTALL="OFF"

VSOMEIP_PREFIX="/opt/vsomeip"
ICEORYX_PREFIX="/opt/iceoryx"
CYCLONEDDS_PREFIX="/opt/cyclonedds"

while [[ $# -gt 0 ]]; do
  case "$1" in
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
    --install-base-deps)
      INSTALL_BASE_DEPS="ON"
      shift
      ;;
    --skip-system-deps)
      SKIP_MIDDLEWARE_SYSTEM_DEPS="ON"
      shift
      ;;
    --force)
      FORCE_REINSTALL="ON"
      shift
      ;;
    --vsomeip-prefix)
      VSOMEIP_PREFIX="$2"
      shift 2
      ;;
    --iceoryx-prefix)
      ICEORYX_PREFIX="$2"
      shift 2
      ;;
    --cyclonedds-prefix)
      CYCLONEDDS_PREFIX="$2"
      shift 2
      ;;
    *)
      echo "[ERROR] Unknown argument: $1" >&2
      exit 1
      ;;
  esac
done

echo "[INFO] Install middleware stack"
echo "       vsomeip=${USE_VSOMEIP} (${VSOMEIP_PREFIX})"
echo "       iceoryx=${USE_ICEORYX} (${ICEORYX_PREFIX})"
echo "       cyclonedds=${USE_CYCLONEDDS} (${CYCLONEDDS_PREFIX})"
echo "       install_base_deps=${INSTALL_BASE_DEPS}"

if [[ "${INSTALL_BASE_DEPS}" == "ON" ]]; then
  "${SCRIPT_DIR}/install_dependemcy.sh"
fi

common_args=()
if [[ "${SKIP_MIDDLEWARE_SYSTEM_DEPS}" == "ON" ]]; then
  common_args+=(--skip-system-deps)
fi
if [[ "${FORCE_REINSTALL}" == "ON" ]]; then
  common_args+=(--force)
fi

# Build order: iceoryx -> cyclonedds -> vsomeip
if [[ "${USE_ICEORYX}" == "ON" ]]; then
  "${SCRIPT_DIR}/install_iceoryx.sh" \
    --prefix "${ICEORYX_PREFIX}" \
    "${common_args[@]}"
fi

if [[ "${USE_CYCLONEDDS}" == "ON" ]]; then
  cyclonedds_args=(
    --prefix "${CYCLONEDDS_PREFIX}"
    --iceoryx-prefix "${ICEORYX_PREFIX}"
  )
  cyclonedds_args+=("${common_args[@]}")
  if [[ "${USE_ICEORYX}" == "OFF" ]]; then
    cyclonedds_args+=(--disable-shm)
  fi
  "${SCRIPT_DIR}/install_cyclonedds.sh" "${cyclonedds_args[@]}"
fi

if [[ "${USE_VSOMEIP}" == "ON" ]]; then
  "${SCRIPT_DIR}/install_vsomeip.sh" \
    --prefix "${VSOMEIP_PREFIX}" \
    "${common_args[@]}"
fi

echo "[OK] Middleware stack installation complete."
