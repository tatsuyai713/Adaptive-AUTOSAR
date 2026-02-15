#!/usr/bin/env bash
set -euo pipefail

IFNAME="can0"
BITRATE="500000"
RESTART_MS="100"
ENABLE_FD="OFF"
DBITRATE="2000000"
USE_VCAN="OFF"

while [[ $# -gt 0 ]]; do
  case "$1" in
    --ifname)
      IFNAME="$2"
      shift 2
      ;;
    --bitrate)
      BITRATE="$2"
      shift 2
      ;;
    --restart-ms)
      RESTART_MS="$2"
      shift 2
      ;;
    --fd)
      ENABLE_FD="ON"
      shift
      ;;
    --dbitrate)
      DBITRATE="$2"
      shift 2
      ;;
    --vcan)
      USE_VCAN="ON"
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

if [[ "${USE_VCAN}" == "ON" ]]; then
  modprobe vcan
  ip link add dev "${IFNAME}" type vcan 2>/dev/null || true
  ip link set "${IFNAME}" up
  echo "[OK] Virtual CAN interface ready: ${IFNAME}"
  exit 0
fi

modprobe can
modprobe can_raw
modprobe can_dev

ip link set "${IFNAME}" down 2>/dev/null || true

if [[ "${ENABLE_FD}" == "ON" ]]; then
  ip link set "${IFNAME}" type can bitrate "${BITRATE}" dbitrate "${DBITRATE}" fd on restart-ms "${RESTART_MS}"
else
  ip link set "${IFNAME}" type can bitrate "${BITRATE}" restart-ms "${RESTART_MS}"
fi

ip link set "${IFNAME}" up

echo "[OK] SocketCAN interface configured."
echo "     ifname=${IFNAME} bitrate=${BITRATE} restart-ms=${RESTART_MS} fd=${ENABLE_FD}"
