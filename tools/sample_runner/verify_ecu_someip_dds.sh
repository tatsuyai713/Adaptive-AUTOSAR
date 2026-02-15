#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"
BUILD_DIR="${1:-build-cyclonedds}"

cd "${REPO_ROOT}"

PUB_BIN="${BUILD_DIR}/ara_com_pubsub_vsomeip_pub_sample"
BRIDGE_BIN="${BUILD_DIR}/ara_ecu_someip_to_dds_bridge_sample"

if [[ ! -x "${PUB_BIN}" ]]; then
  echo "[ERROR] Missing executable: ${PUB_BIN}" >&2
  exit 1
fi

if [[ ! -x "${BRIDGE_BIN}" ]]; then
  echo "[ERROR] Missing executable: ${BRIDGE_BIN}" >&2
  exit 1
fi

export VSOMEIP_CONFIGURATION="${REPO_ROOT}/configuration/vsomeip-pubsub-sample.json"
export LD_LIBRARY_PATH="/opt/vsomeip/lib:/opt/cyclonedds/lib:/opt/iceoryx/lib:${LD_LIBRARY_PATH:-}"

PUB_LOG="/tmp/adaptive_autosar_verify_pub.log"
BRIDGE_LOG="/tmp/adaptive_autosar_verify_bridge.log"
rm -f "${PUB_LOG}" "${BRIDGE_LOG}" /tmp/vsomeip-*

echo "[INFO] Starting SOME/IP provider (routing host)..."
(
  export ADAPTIVE_AUTOSAR_VSOMEIP_SERVER_APP=adaptive_autosar_pubsub_provider
  timeout 14s "${PUB_BIN}" --period-ms=120 >"${PUB_LOG}" 2>&1
) &
PUB_PID=$!

sleep 1

echo "[INFO] Starting SOME/IP->DDS bridge (proxy client)..."
(
  export ADAPTIVE_AUTOSAR_VSOMEIP_CLIENT_APP=adaptive_autosar_pubsub_consumer
  timeout 10s "${BRIDGE_BIN}" \
    --batch-size=16 \
    --log-every=5 \
    --service-wait-ms=4000 >"${BRIDGE_LOG}" 2>&1
) &
BRIDGE_PID=$!

sleep 8

kill "${BRIDGE_PID}" 2>/dev/null || true
kill "${PUB_PID}" 2>/dev/null || true
wait "${BRIDGE_PID}" 2>/dev/null || true
wait "${PUB_PID}" 2>/dev/null || true

host_ok=0
proxy_ok=0
available_ok=0
bridge_publish_count=0

if grep -q "Instantiating routing manager \[Host\]" "${PUB_LOG}"; then
  host_ok=1
fi

if grep -q "Instantiating routing manager \[Proxy\]" "${BRIDGE_LOG}"; then
  proxy_ok=1
fi

if grep -q "ON_AVAILABLE" "${BRIDGE_LOG}"; then
  available_ok=1
fi

bridge_publish_count="$(grep -c "Bridge published" "${BRIDGE_LOG}" || true)"

echo "[RESULT] Host routing manager   : ${host_ok}"
echo "[RESULT] Proxy routing manager  : ${proxy_ok}"
echo "[RESULT] Service availability   : ${available_ok}"
echo "[RESULT] Bridge published count : ${bridge_publish_count}"

if [[ "${host_ok}" -ne 1 || "${proxy_ok}" -ne 1 || "${available_ok}" -ne 1 || "${bridge_publish_count}" -le 0 ]]; then
  echo "[ERROR] Verification failed. Check logs:" >&2
  echo "  ${PUB_LOG}" >&2
  echo "  ${BRIDGE_LOG}" >&2
  exit 1
fi

echo "[OK] SOME/IP routing + receive + DDS forward path verified."
