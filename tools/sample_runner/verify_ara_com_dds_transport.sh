#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"
BUILD_DIR="${1:-build-cyclonedds}"

cd "${REPO_ROOT}"

PUB_BIN="${BUILD_DIR}/ara_com_pubsub_cyclonedds_pub_sample"
SUB_BIN="${BUILD_DIR}/ara_com_pubsub_cyclonedds_sub_sample"

if [[ ! -x "${PUB_BIN}" ]]; then
  echo "[ERROR] Missing executable: ${PUB_BIN}" >&2
  exit 1
fi

if [[ ! -x "${SUB_BIN}" ]]; then
  echo "[ERROR] Missing executable: ${SUB_BIN}" >&2
  exit 1
fi

export LD_LIBRARY_PATH="/opt/cyclonedds/lib:/opt/iceoryx/lib:${LD_LIBRARY_PATH:-}"

PUB_LOG="/tmp/adaptive_autosar_verify_dds_pub.log"
SUB_LOG="/tmp/adaptive_autosar_verify_dds_sub.log"
rm -f "${PUB_LOG}" "${SUB_LOG}"

echo "[INFO] Starting DDS publisher..."
(
  timeout 14s "${PUB_BIN}" --period-ms=120 >"${PUB_LOG}" 2>&1
) &
PUB_PID=$!

sleep 1

echo "[INFO] Starting DDS subscriber..."
(
  timeout 10s "${SUB_BIN}" --poll-ms=20 >"${SUB_LOG}" 2>&1
) &
SUB_PID=$!

sleep 8

kill "${SUB_PID}" 2>/dev/null || true
kill "${PUB_PID}" 2>/dev/null || true
wait "${SUB_PID}" 2>/dev/null || true
wait "${PUB_PID}" 2>/dev/null || true

published_count="$(grep -c "Published frame:" "${PUB_LOG}" || true)"
received_count="$(grep -c "DDS received frame:" "${SUB_LOG}" || true)"

echo "[RESULT] DDS published frame logs: ${published_count}"
echo "[RESULT] DDS received frame logs : ${received_count}"

if [[ "${published_count}" -le 0 || "${received_count}" -le 0 ]]; then
  echo "[ERROR] ara::com DDS verification failed. Check logs:" >&2
  echo "  ${PUB_LOG}" >&2
  echo "  ${SUB_LOG}" >&2
  exit 1
fi

echo "[OK] ara::com DDS path verified."
