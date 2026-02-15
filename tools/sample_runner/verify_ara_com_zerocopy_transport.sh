#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"
BUILD_DIR="${1:-build-cyclonedds}"

cd "${REPO_ROOT}"

PUB_BIN="${BUILD_DIR}/ara_com_pubsub_iceoryx_pub_sample"
SUB_BIN="${BUILD_DIR}/ara_com_pubsub_iceoryx_sub_sample"
ROUDI_BIN="/opt/iceoryx/bin/iox-roudi"
RUNTIME_PUB="adaptive_autosar_ara_com_pub_verify"
RUNTIME_SUB="adaptive_autosar_ara_com_sub_verify"

if [[ ! -x "${PUB_BIN}" ]]; then
  echo "[ERROR] Missing executable: ${PUB_BIN}" >&2
  exit 1
fi

if [[ ! -x "${SUB_BIN}" ]]; then
  echo "[ERROR] Missing executable: ${SUB_BIN}" >&2
  exit 1
fi

if [[ ! -x "${ROUDI_BIN}" ]]; then
  echo "[ERROR] Missing executable: ${ROUDI_BIN}" >&2
  exit 1
fi

export VSOMEIP_CONFIGURATION="${REPO_ROOT}/configuration/vsomeip-pubsub-sample.json"
export LD_LIBRARY_PATH="/opt/vsomeip/lib:/opt/cyclonedds/lib:/opt/iceoryx/lib:${LD_LIBRARY_PATH:-}"

ROUDI_LOG="/tmp/adaptive_autosar_verify_zerocopy_roudi.log"
PUB_LOG="/tmp/adaptive_autosar_verify_zerocopy_pub.log"
SUB_LOG="/tmp/adaptive_autosar_verify_zerocopy_sub.log"
rm -f "${ROUDI_LOG}" "${PUB_LOG}" "${SUB_LOG}" /tmp/vsomeip-*

ROUDI_PID=""
if pgrep -f "iox-roudi" >/dev/null 2>&1; then
  echo "[INFO] Reusing existing iceoryx RouDi process."
else
  echo "[INFO] Starting iceoryx RouDi..."
  (
    timeout 14s "${ROUDI_BIN}" >"${ROUDI_LOG}" 2>&1
  ) &
  ROUDI_PID=$!
  sleep 1

  if ! kill -0 "${ROUDI_PID}" >/dev/null 2>&1; then
    if grep -q "MEPOO__SEGMENT_COULD_NOT_APPLY_POSIX_RIGHTS_TO_SHARED_MEMORY" "${ROUDI_LOG}" 2>/dev/null; then
      echo "[WARN] RouDi could not start due shared-memory ACL limitations in this environment."
      echo "[WARN] Skip zerocopy transport verification."
      exit 0
    fi
  fi
fi

sleep 1

echo "[INFO] Starting SOME/IP + zerocopy provider..."
(
  export ADAPTIVE_AUTOSAR_VSOMEIP_SERVER_APP=adaptive_autosar_pubsub_provider
  timeout 14s "${PUB_BIN}" --runtime-name="${RUNTIME_PUB}" --period-ms=120 >"${PUB_LOG}" 2>&1
) &
PUB_PID=$!

sleep 1

echo "[INFO] Starting SOME/IP + zerocopy consumer..."
(
  export ADAPTIVE_AUTOSAR_VSOMEIP_CLIENT_APP=adaptive_autosar_pubsub_consumer
  timeout 10s "${SUB_BIN}" --runtime-name="${RUNTIME_SUB}" --poll-ms=20 >"${SUB_LOG}" 2>&1
) &
SUB_PID=$!

sleep 8

kill "${SUB_PID}" 2>/dev/null || true
kill "${PUB_PID}" 2>/dev/null || true
if [[ -n "${ROUDI_PID}" ]]; then
  kill "${ROUDI_PID}" 2>/dev/null || true
fi
wait "${SUB_PID}" 2>/dev/null || true
wait "${PUB_PID}" 2>/dev/null || true
if [[ -n "${ROUDI_PID}" ]]; then
  wait "${ROUDI_PID}" 2>/dev/null || true
fi

host_ok=0
proxy_ok=0
available_ok=0
connected_ok=0
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

if grep -q "zerocopy subscriber has been connected." "${SUB_LOG}"; then
  connected_ok=1
fi

received_count="$(grep -c "zerocopy received frame:" "${SUB_LOG}" || true)"

echo "[RESULT] Host routing manager       : ${host_ok}"
echo "[RESULT] Proxy routing manager      : ${proxy_ok}"
echo "[RESULT] Service available          : ${available_ok}"
echo "[RESULT] Zerocopy subscriber linked : ${connected_ok}"
echo "[RESULT] Zerocopy received logs     : ${received_count}"

if [[ "${host_ok}" -ne 1 || "${proxy_ok}" -ne 1 || "${available_ok}" -ne 1 || "${connected_ok}" -ne 1 || "${received_count}" -le 0 ]]; then
  echo "[ERROR] ara::com zerocopy verification failed. Check logs:" >&2
  echo "  ${ROUDI_LOG}" >&2
  echo "  ${PUB_LOG}" >&2
  echo "  ${SUB_LOG}" >&2
  exit 1
fi

echo "[OK] ara::com zerocopy path verified (SOME/IP control + iceoryx data path)."
