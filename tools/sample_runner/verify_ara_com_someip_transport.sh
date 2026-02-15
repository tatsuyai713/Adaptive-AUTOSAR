#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"
BUILD_DIR="${1:-build-cyclonedds}"

cd "${REPO_ROOT}"

PUB_BIN="${BUILD_DIR}/ara_com_pubsub_vsomeip_pub_sample"
SUB_BIN="${BUILD_DIR}/ara_com_pubsub_vsomeip_sub_sample"

if [[ ! -x "${PUB_BIN}" ]]; then
  echo "[ERROR] Missing executable: ${PUB_BIN}" >&2
  exit 1
fi

if [[ ! -x "${SUB_BIN}" ]]; then
  echo "[ERROR] Missing executable: ${SUB_BIN}" >&2
  exit 1
fi

export VSOMEIP_CONFIGURATION="${REPO_ROOT}/configuration/vsomeip-pubsub-sample.json"
export LD_LIBRARY_PATH="/opt/vsomeip/lib:/opt/cyclonedds/lib:/opt/iceoryx/lib:${LD_LIBRARY_PATH:-}"

PUB_LOG="/tmp/adaptive_autosar_verify_someip_pub.log"
SUB_LOG="/tmp/adaptive_autosar_verify_someip_sub.log"
rm -f "${PUB_LOG}" "${SUB_LOG}" /tmp/vsomeip-*

echo "[INFO] Starting SOME/IP provider (routing host)..."
(
  export ADAPTIVE_AUTOSAR_VSOMEIP_SERVER_APP=adaptive_autosar_pubsub_provider
  timeout 14s "${PUB_BIN}" --period-ms=120 >"${PUB_LOG}" 2>&1
) &
PUB_PID=$!

sleep 1

echo "[INFO] Starting SOME/IP consumer (proxy client)..."
(
  export ADAPTIVE_AUTOSAR_VSOMEIP_CLIENT_APP=adaptive_autosar_pubsub_consumer
  timeout 10s "${SUB_BIN}" --poll-ms=20 >"${SUB_LOG}" 2>&1
) &
SUB_PID=$!

sleep 8

kill "${SUB_PID}" 2>/dev/null || true
kill "${PUB_PID}" 2>/dev/null || true
wait "${SUB_PID}" 2>/dev/null || true
wait "${PUB_PID}" 2>/dev/null || true

host_ok=0
proxy_ok=0
available_ok=0
received_count=0

if grep -q "Instantiating routing manager \[Host\]" "${PUB_LOG}"; then
  host_ok=1
fi

if grep -q "Instantiating routing manager \[Proxy\]" "${SUB_LOG}"; then
  proxy_ok=1
fi

if grep -q "ON_AVAILABLE" "${SUB_LOG}"; then
  available_ok=1
fi

received_count="$(grep -c "SOME/IP received frame:" "${SUB_LOG}" || true)"

echo "[RESULT] Host routing manager : ${host_ok}"
echo "[RESULT] Proxy routing manager: ${proxy_ok}"
echo "[RESULT] Service available    : ${available_ok}"
echo "[RESULT] Received frame logs  : ${received_count}"

if [[ "${host_ok}" -ne 1 || "${proxy_ok}" -ne 1 || "${available_ok}" -ne 1 || "${received_count}" -le 0 ]]; then
  echo "[ERROR] ara::com SOME/IP verification failed. Check logs:" >&2
  echo "  ${PUB_LOG}" >&2
  echo "  ${SUB_LOG}" >&2
  exit 1
fi

echo "[OK] ara::com SOME/IP path verified (internally using Async-BSD-Socket-Lib for SOME/IP SD transport)."
