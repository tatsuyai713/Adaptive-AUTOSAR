#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"
BUILD_DIR="${1:-build-cyclonedds}"

cd "${REPO_ROOT}"

PUB_BIN="${BUILD_DIR}/ara_com_pubsub_vsomeip_pub_sample"
REF_BIN="${BUILD_DIR}/ara_ecu_reference_gateway_sample"

if [[ ! -x "${PUB_BIN}" ]]; then
  echo "[ERROR] Missing executable: ${PUB_BIN}" >&2
  exit 1
fi

if [[ ! -x "${REF_BIN}" ]]; then
  echo "[ERROR] Missing executable: ${REF_BIN}" >&2
  exit 1
fi

export VSOMEIP_CONFIGURATION="${REPO_ROOT}/configuration/vsomeip-pubsub-sample.json"
export LD_LIBRARY_PATH="/opt/vsomeip/lib:/opt/cyclonedds/lib:/opt/iceoryx/lib:${LD_LIBRARY_PATH:-}"

PUB_LOG="/tmp/adaptive_autosar_verify_reference_pub.log"
REF_LOG="/tmp/adaptive_autosar_verify_reference_gateway.log"
rm -f "${PUB_LOG}" "${REF_LOG}" /tmp/vsomeip-*

echo "[INFO] Starting SOME/IP provider (routing host)..."
(
  export ADAPTIVE_AUTOSAR_VSOMEIP_SERVER_APP=adaptive_autosar_pubsub_provider
  timeout 16s "${PUB_BIN}" --period-ms=120 >"${PUB_LOG}" 2>&1
) &
PUB_PID=$!

sleep 1

echo "[INFO] Starting ECU reference gateway (CAN + SOME/IP -> DDS)..."
(
  export ADAPTIVE_AUTOSAR_VSOMEIP_CLIENT_APP=adaptive_autosar_pubsub_consumer
  timeout 10s "${REF_BIN}" \
    --can-backend=mock \
    --publish-period-ms=100 \
    --source-stale-ms=500 \
    --service-wait-ms=4000 \
    --log-every=5 \
    --storage-sync-every=10 >"${REF_LOG}" 2>&1
) &
REF_PID=$!

sleep 8

kill "${REF_PID}" 2>/dev/null || true
kill "${PUB_PID}" 2>/dev/null || true
wait "${REF_PID}" 2>/dev/null || true
wait "${PUB_PID}" 2>/dev/null || true

host_ok=0
proxy_ok=0
available_ok=0
fused_publish_count=0
combined_mode_count=0

if grep -q "Instantiating routing manager \[Host\]" "${PUB_LOG}"; then
  host_ok=1
fi

if grep -q "Instantiating routing manager \[Proxy\]" "${REF_LOG}"; then
  proxy_ok=1
fi

if grep -q "ON_AVAILABLE" "${REF_LOG}"; then
  available_ok=1
fi

fused_publish_count="$(grep -c "Fused publish count=" "${REF_LOG}" || true)"
combined_mode_count="$(grep -c "mode=3" "${REF_LOG}" || true)"

echo "[RESULT] Host routing manager : ${host_ok}"
echo "[RESULT] Proxy routing manager: ${proxy_ok}"
echo "[RESULT] Service available    : ${available_ok}"
echo "[RESULT] Fused publish count  : ${fused_publish_count}"
echo "[RESULT] Combined mode count  : ${combined_mode_count}"

if [[ "${host_ok}" -ne 1 || "${proxy_ok}" -ne 1 || "${available_ok}" -ne 1 || "${fused_publish_count}" -le 0 || "${combined_mode_count}" -le 0 ]]; then
  echo "[ERROR] Reference gateway verification failed. Check logs:" >&2
  echo "  ${PUB_LOG}" >&2
  echo "  ${REF_LOG}" >&2
  exit 1
fi

echo "[OK] ECU reference gateway path verified (CAN + SOME/IP receive -> DDS send)."
