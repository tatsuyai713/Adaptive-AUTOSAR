#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
SAMPLE_DIR="${REPO_ROOT}/samples/switchable_pubsub"

AUTOSAR_AP_PREFIX="/opt/autosar_ap"
CYCLONEDDS_PREFIX="/opt/cyclonedds"
BUILD_DIR="${REPO_ROOT}/build-switchable-pubsub-sample"
RUN_SMOKE="OFF"
CLEAN="OFF"

while [[ $# -gt 0 ]]; do
  case "$1" in
    --prefix)
      AUTOSAR_AP_PREFIX="$2"
      shift 2
      ;;
    --cyclonedds-prefix)
      CYCLONEDDS_PREFIX="$2"
      shift 2
      ;;
    --build-dir)
      BUILD_DIR="$2"
      shift 2
      ;;
    --run-smoke)
      RUN_SMOKE="ON"
      shift
      ;;
    --clean)
      CLEAN="ON"
      shift
      ;;
    *)
      echo "[ERROR] Unknown argument: $1" >&2
      exit 1
      ;;
  esac
done

if [[ "${CLEAN}" == "ON" ]]; then
  rm -rf "${BUILD_DIR}"
fi

if [[ ! -f "${SAMPLE_DIR}/CMakeLists.txt" ]]; then
  echo "[ERROR] sample CMakeLists not found: ${SAMPLE_DIR}/CMakeLists.txt" >&2
  exit 1
fi

AP_CMAKE_DIR=""
for candidate_libdir in lib lib64; do
  candidate="${AUTOSAR_AP_PREFIX}/${candidate_libdir}/cmake/AdaptiveAutosarAP/AdaptiveAutosarAPConfig.cmake"
  if [[ -f "${candidate}" ]]; then
    AP_CMAKE_DIR="${AUTOSAR_AP_PREFIX}/${candidate_libdir}/cmake/AdaptiveAutosarAP"
    break
  fi
done

if [[ -z "${AP_CMAKE_DIR}" ]]; then
  echo "[ERROR] AdaptiveAutosarAPConfig.cmake not found under ${AUTOSAR_AP_PREFIX}/{lib,lib64}/cmake/AdaptiveAutosarAP" >&2
  exit 1
fi

mkdir -p "${BUILD_DIR}"

echo "[INFO] Configure switchable pub/sub sample"
echo "       sample_dir=${SAMPLE_DIR}"
echo "       build_dir=${BUILD_DIR}"
echo "       autosar_prefix=${AUTOSAR_AP_PREFIX}"
echo "       cyclonedds_prefix=${CYCLONEDDS_PREFIX}"

cmake -S "${SAMPLE_DIR}" -B "${BUILD_DIR}" \
  -DCMAKE_BUILD_TYPE=Release \
  -DAUTOSAR_AP_PREFIX="${AUTOSAR_AP_PREFIX}" \
  -DCYCLONEDDS_PREFIX="${CYCLONEDDS_PREFIX}" \
  -DAdaptiveAutosarAP_DIR="${AP_CMAKE_DIR}"

cmake --build "${BUILD_DIR}" -j"$(nproc)"

echo "[OK] Build completed"
echo "[INFO] Generated artifacts:"
for artifact in \
  "${BUILD_DIR}/generated/switchable_topic_mapping_dds.yaml" \
  "${BUILD_DIR}/generated/switchable_topic_mapping_vsomeip.yaml" \
  "${BUILD_DIR}/generated/switchable_manifest_dds.yaml" \
  "${BUILD_DIR}/generated/switchable_manifest_vsomeip.yaml" \
  "${BUILD_DIR}/generated/switchable_manifest_dds.arxml" \
  "${BUILD_DIR}/generated/switchable_manifest_vsomeip.arxml" \
  "${BUILD_DIR}/generated/switchable_proxy_skeleton.hpp" \
  "${BUILD_DIR}/generated/switchable_binding.hpp"; do
  if [[ -f "${artifact}" ]]; then
    echo "       ${artifact}"
  else
    echo "[ERROR] missing generated artifact: ${artifact}" >&2
    exit 1
  fi
done

echo "[INFO] Built binaries:"
for binary in \
  "${BUILD_DIR}/autosar_switchable_pubsub_pub" \
  "${BUILD_DIR}/autosar_switchable_pubsub_sub"; do
  if [[ -x "${binary}" ]]; then
    echo "       ${binary}"
  else
    echo "[ERROR] missing executable: ${binary}" >&2
    exit 1
  fi
done

if [[ "${RUN_SMOKE}" != "ON" ]]; then
  exit 0
fi

echo "[INFO] Running smoke checks (DDS-profile then vSomeIP-profile)"

TIMEOUT_BIN=""
if command -v timeout >/dev/null 2>&1; then
  TIMEOUT_BIN="timeout"
elif command -v gtimeout >/dev/null 2>&1; then
  TIMEOUT_BIN="gtimeout"
else
  echo "[ERROR] timeout command not found. Install GNU coreutils (timeout/gtimeout)." >&2
  exit 1
fi

TIMEOUT_KILL_AFTER_ARGS=()
if "${TIMEOUT_BIN}" --help 2>&1 | grep -q -- "--kill-after"; then
  # Force-kill stuck processes after TERM grace period to avoid indefinite CI hangs.
  TIMEOUT_KILL_AFTER_ARGS=(--kill-after=3s)
fi

run_with_timeout() {
  local duration="$1"
  shift
  "${TIMEOUT_BIN}" "${TIMEOUT_KILL_AFTER_ARGS[@]}" "${duration}" "$@"
}

safe_wait_with_deadline() {
  local pid="$1"
  local label="$2"
  local deadline_sec="$3"
  local end_at=$((SECONDS + deadline_sec))

  if [[ -z "${pid}" ]]; then
    return 0
  fi

  while kill -0 "${pid}" >/dev/null 2>&1; do
    if ((SECONDS >= end_at)); then
      echo "[WARN] ${label} exceeded ${deadline_sec}s; sending SIGTERM." >&2
      kill "${pid}" >/dev/null 2>&1 || true
      sleep 1
      if kill -0 "${pid}" >/dev/null 2>&1; then
        echo "[WARN] ${label} did not stop on SIGTERM; sending SIGKILL." >&2
        kill -9 "${pid}" >/dev/null 2>&1 || true
      fi
      break
    fi
    sleep 1
  done

  wait "${pid}" 2>/dev/null || true
}

LD_LIBS=""
for libdir in "${AUTOSAR_AP_PREFIX}/lib" "${AUTOSAR_AP_PREFIX}/lib64" "${CYCLONEDDS_PREFIX}/lib" "/opt/vsomeip/lib" "/opt/iceoryx/lib"; do
  if [[ -d "${libdir}" ]]; then
    if [[ -n "${LD_LIBS}" ]]; then
      LD_LIBS="${LD_LIBS}:"
    fi
    LD_LIBS="${LD_LIBS}${libdir}"
  fi
done
export LD_LIBRARY_PATH="${LD_LIBS}:${LD_LIBRARY_PATH:-}"

PUB_BIN="${BUILD_DIR}/autosar_switchable_pubsub_pub"
SUB_BIN="${BUILD_DIR}/autosar_switchable_pubsub_sub"

# DDS-profile mode
export ARA_COM_BINDING_MANIFEST="${BUILD_DIR}/generated/switchable_manifest_dds.yaml"
unset ARA_COM_EVENT_BINDING
unset ARA_COM_PREFER_SOMEIP
unset VSOMEIP_CONFIGURATION
DDS_PUB_LOG="${BUILD_DIR}/switchable_dds_pub.log"
DDS_SUB_LOG="${BUILD_DIR}/switchable_dds_sub.log"
rm -f "${DDS_PUB_LOG}" "${DDS_SUB_LOG}"
(run_with_timeout 6s "${SUB_BIN}" >"${DDS_SUB_LOG}" 2>&1) &
DDS_SUB_PID=$!
sleep 1
(run_with_timeout 4s "${PUB_BIN}" >"${DDS_PUB_LOG}" 2>&1) || true
safe_wait_with_deadline "${DDS_SUB_PID}" "DDS subscriber smoke process" 12

DDS_HEARD_COUNT=$(grep -c "I heard seq=" "${DDS_SUB_LOG}" || true)
if [[ "${DDS_HEARD_COUNT}" -lt 1 ]]; then
  echo "[ERROR] DDS-profile smoke failed: subscriber did not receive samples." >&2
  exit 1
fi

echo "[OK] DDS-profile smoke passed (received=${DDS_HEARD_COUNT})"

# vSomeIP-profile mode
export ARA_COM_BINDING_MANIFEST="${BUILD_DIR}/generated/switchable_manifest_vsomeip.yaml"
unset ARA_COM_EVENT_BINDING
unset ARA_COM_PREFER_SOMEIP
export VSOMEIP_CONFIGURATION="${AUTOSAR_AP_PREFIX}/configuration/vsomeip-rpi.json"
VSOMEIP_PUB_LOG="${BUILD_DIR}/switchable_vsomeip_pub.log"
VSOMEIP_SUB_LOG="${BUILD_DIR}/switchable_vsomeip_sub.log"
VSOMEIP_RM_LOG="${BUILD_DIR}/switchable_vsomeip_routing_manager.log"
rm -f "${VSOMEIP_PUB_LOG}" "${VSOMEIP_SUB_LOG}" "${VSOMEIP_RM_LOG}"
(run_with_timeout 12s "${AUTOSAR_AP_PREFIX}/bin/autosar_vsomeip_routing_manager" >"${VSOMEIP_RM_LOG}" 2>&1) &
RM_PID=$!
sleep 1
(run_with_timeout 6s "${SUB_BIN}" >"${VSOMEIP_SUB_LOG}" 2>&1) &
VSOMEIP_SUB_PID=$!
sleep 1
(run_with_timeout 4s "${PUB_BIN}" >"${VSOMEIP_PUB_LOG}" 2>&1) || true
safe_wait_with_deadline "${VSOMEIP_SUB_PID}" "vSomeIP subscriber smoke process" 12
safe_wait_with_deadline "${RM_PID}" "vSomeIP routing manager smoke process" 18

VSOMEIP_HEARD_COUNT=$(grep -c "I heard seq=" "${VSOMEIP_SUB_LOG}" || true)
if [[ "${VSOMEIP_HEARD_COUNT}" -lt 1 ]]; then
  echo "[ERROR] vSomeIP-profile smoke failed: subscriber did not receive samples." >&2
  exit 1
fi

if ! grep -Eqi "REGISTER EVENT|SUBSCRIBE|vsomeip|routing" "${VSOMEIP_PUB_LOG}" "${VSOMEIP_SUB_LOG}" "${VSOMEIP_RM_LOG}"; then
  echo "[ERROR] vSomeIP-profile smoke failed: expected routing/vsomeip logs were not found." >&2
  exit 1
fi

echo "[OK] vSomeIP-profile smoke passed (received=${VSOMEIP_HEARD_COUNT})"
echo "[OK] Switchable pub/sub smoke checks passed"
