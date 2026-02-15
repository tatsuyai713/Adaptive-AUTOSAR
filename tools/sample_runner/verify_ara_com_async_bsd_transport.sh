#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"
BUILD_DIR="${1:-build-cyclonedds}"

cd "${REPO_ROOT}"

UNIT_TEST_BIN="${BUILD_DIR}/ara_unit_test"

if [[ ! -x "${UNIT_TEST_BIN}" ]]; then
  echo "[ERROR] Missing executable: ${UNIT_TEST_BIN}" >&2
  echo "        build with -Dbuild_tests=ON to enable this verification." >&2
  exit 1
fi

export LD_LIBRARY_PATH="/opt/vsomeip/lib:/opt/cyclonedds/lib:/opt/iceoryx/lib:${LD_LIBRARY_PATH:-}"

TEST_LOG="/tmp/adaptive_autosar_verify_async_bsd.log"
rm -f "${TEST_LOG}"

echo "[INFO] Running ara::com Async-BSD transport smoke test..."
timeout 20s "${UNIT_TEST_BIN}" \
  --gtest_filter=NetworkAbstractionTest.PollerConstructor >"${TEST_LOG}" 2>&1 || true

pass_count="$(grep -c "\[       OK \] NetworkAbstractionTest.PollerConstructor" "${TEST_LOG}" || true)"
summary_count="$(grep -c "\[  PASSED  \]" "${TEST_LOG}" || true)"

echo "[RESULT] Async-BSD poller test pass logs: ${pass_count}"
echo "[RESULT] GTest summary lines         : ${summary_count}"

if [[ "${pass_count}" -le 0 || "${summary_count}" -le 0 ]]; then
  echo "[ERROR] Async-BSD transport verification failed. Check log: ${TEST_LOG}" >&2
  exit 1
fi

echo "[OK] Async-BSD transport abstraction is available for ara::com build/runtime."
