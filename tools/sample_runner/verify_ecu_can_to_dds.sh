#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"
BUILD_DIR="${1:-build-cyclonedds}"

cd "${REPO_ROOT}"

CAN_BIN="${BUILD_DIR}/ara_ecu_can_to_dds_gateway_sample"

if [[ ! -x "${CAN_BIN}" ]]; then
  echo "[ERROR] Missing executable: ${CAN_BIN}" >&2
  exit 1
fi

export LD_LIBRARY_PATH="/opt/vsomeip/lib:/opt/cyclonedds/lib:/opt/iceoryx/lib:${LD_LIBRARY_PATH:-}"

CAN_LOG="/tmp/adaptive_autosar_verify_can_to_dds.log"
rm -f "${CAN_LOG}"

echo "[INFO] Starting CAN->DDS gateway sample (mock CAN)..."
timeout 6s "${CAN_BIN}" \
  --can-backend=mock \
  --log-every=5 \
  --storage-sync-every=10 >"${CAN_LOG}" 2>&1 || true

publish_count="$(grep -c "Published " "${CAN_LOG}" || true)"

echo "[RESULT] Publish log count: ${publish_count}"
if [[ "${publish_count}" -le 0 ]]; then
  echo "[ERROR] CAN->DDS verification failed. Check log: ${CAN_LOG}" >&2
  exit 1
fi

echo "[OK] CAN receive -> DDS send path verified."
