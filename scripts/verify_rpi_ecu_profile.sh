#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

INSTALL_PREFIX="/opt/autosar_ap"
USER_APP_BUILD_DIR="/opt/autosar_ap/user_apps_build"
CAN_BACKEND="mock"
CAN_IFNAME="can0"
SMOKE_SECONDS=8
CHECK_ONLY="OFF"
RUN_SOMEIP="ON"
RUN_DDS="ON"
RUN_ZEROCOPY="ON"
RUN_ECU="ON"
STRICT_MODE="OFF"

while [[ $# -gt 0 ]]; do
  case "$1" in
    --prefix)
      INSTALL_PREFIX="$2"
      shift 2
      ;;
    --user-app-build-dir)
      USER_APP_BUILD_DIR="$2"
      shift 2
      ;;
    --can-backend)
      CAN_BACKEND="$2"
      shift 2
      ;;
    --can-if)
      CAN_IFNAME="$2"
      shift 2
      ;;
    --smoke-seconds)
      SMOKE_SECONDS="$2"
      shift 2
      ;;
    --check-only)
      CHECK_ONLY="ON"
      shift
      ;;
    --skip-someip)
      RUN_SOMEIP="OFF"
      shift
      ;;
    --skip-dds)
      RUN_DDS="OFF"
      shift
      ;;
    --skip-zerocopy)
      RUN_ZEROCOPY="OFF"
      shift
      ;;
    --skip-ecu)
      RUN_ECU="OFF"
      shift
      ;;
    --strict)
      STRICT_MODE="ON"
      shift
      ;;
    *)
      echo "[ERROR] Unknown argument: $1" >&2
      exit 1
      ;;
  esac
done

declare -i PASS_COUNT=0
declare -i FAIL_COUNT=0
declare -i SKIP_COUNT=0
declare -a RESULT_LINES=()

record_result() {
  local name="$1"
  local status="$2"
  local detail="$3"

  RESULT_LINES+=("${name}|${status}|${detail}")
  case "${status}" in
    PASS)
      PASS_COUNT=$((PASS_COUNT + 1))
      ;;
    FAIL)
      FAIL_COUNT=$((FAIL_COUNT + 1))
      ;;
    SKIP)
      SKIP_COUNT=$((SKIP_COUNT + 1))
      ;;
  esac
}

print_summary() {
  echo
  echo "[SUMMARY] Raspberry Pi ECU profile verification"
  printf "%-34s %-8s %s\n" "Check" "Status" "Detail"
  printf "%-34s %-8s %s\n" "-----" "------" "------"
  for line in "${RESULT_LINES[@]}"; do
    IFS="|" read -r name status detail <<<"${line}"
    printf "%-34s %-8s %s\n" "${name}" "${status}" "${detail}"
  done
  echo
  echo "[SUMMARY] pass=${PASS_COUNT} fail=${FAIL_COUNT} skip=${SKIP_COUNT}"
}

resolve_binary() {
  local binary_name="$1"
  find "${USER_APP_BUILD_DIR}" -type f -name "${binary_name}" -perm -111 | head -n 1
}

safe_kill_and_wait() {
  local pid="$1"
  if [[ -n "${pid}" ]] && kill -0 "${pid}" >/dev/null 2>&1; then
    kill "${pid}" >/dev/null 2>&1 || true
  fi
  if [[ -n "${pid}" ]]; then
    wait "${pid}" 2>/dev/null || true
  fi
}

extract_max_counter() {
  local pattern="$1"
  local log_path="$2"
  local max_value="0"
  local found
  found="$(grep -oE "${pattern}=[0-9]+" "${log_path}" 2>/dev/null | cut -d "=" -f 2 | sort -n | tail -n 1 || true)"
  if [[ -n "${found}" ]]; then
    max_value="${found}"
  fi
  echo "${max_value}"
}

build_ld_library_path() {
  local lib_path=""
  local candidate
  for candidate in \
    "${INSTALL_PREFIX}/lib" \
    "${INSTALL_PREFIX}/lib64" \
    "/opt/vsomeip/lib" \
    "/opt/cyclonedds/lib" \
    "/opt/iceoryx/lib"; do
    if [[ -d "${candidate}" ]]; then
      if [[ -n "${lib_path}" ]]; then
        lib_path="${lib_path}:"
      fi
      lib_path="${lib_path}${candidate}"
    fi
  done
  echo "${lib_path}"
}

RUN_ID="$(date +%s)-$$"
BASE_LOG_DIR="/tmp/adaptive_autosar_rpi_verify_${RUN_ID}"
mkdir -p "${BASE_LOG_DIR}"

VSO_CONFIG=""
for candidate in \
  "${INSTALL_PREFIX}/configuration/vsomeip-pubsub-sample.json" \
  "${REPO_ROOT}/configuration/vsomeip-pubsub-sample.json"; do
  if [[ -f "${candidate}" ]]; then
    VSO_CONFIG="${candidate}"
    break
  fi
done

LIB_PATH="$(build_ld_library_path)"
if [[ -n "${LIB_PATH}" ]]; then
  export LD_LIBRARY_PATH="${LIB_PATH}:${LD_LIBRARY_PATH:-}"
fi

echo "[INFO] Verify Raspberry Pi ECU profile"
echo "       install_prefix=${INSTALL_PREFIX}"
echo "       user_app_build_dir=${USER_APP_BUILD_DIR}"
echo "       can_backend=${CAN_BACKEND} can_if=${CAN_IFNAME}"
echo "       logs=${BASE_LOG_DIR}"

# Prerequisite checks
if [[ -f "${INSTALL_PREFIX}/lib/cmake/AdaptiveAutosarAP/AdaptiveAutosarAPConfig.cmake" || \
      -f "${INSTALL_PREFIX}/lib64/cmake/AdaptiveAutosarAP/AdaptiveAutosarAPConfig.cmake" ]]; then
  record_result "Runtime install" "PASS" "AdaptiveAutosarAPConfig.cmake found"
else
  record_result "Runtime install" "FAIL" "AdaptiveAutosarAPConfig.cmake not found under ${INSTALL_PREFIX}"
fi

if [[ -d "${USER_APP_BUILD_DIR}" ]]; then
  record_result "User-app build dir" "PASS" "${USER_APP_BUILD_DIR} exists"
else
  record_result "User-app build dir" "FAIL" "${USER_APP_BUILD_DIR} does not exist"
fi

SOMEIP_PROVIDER_BIN="$(resolve_binary autosar_user_com_someip_provider_template || true)"
SOMEIP_CONSUMER_BIN="$(resolve_binary autosar_user_com_someip_consumer_template || true)"
DDS_PUB_BIN="$(resolve_binary autosar_user_com_dds_pub_template || true)"
DDS_SUB_BIN="$(resolve_binary autosar_user_com_dds_sub_template || true)"
ZC_PUB_BIN="$(resolve_binary autosar_user_com_zerocopy_pub_template || true)"
ZC_SUB_BIN="$(resolve_binary autosar_user_com_zerocopy_sub_template || true)"
ECU_BIN="$(resolve_binary autosar_user_tpl_ecu_full_stack || true)"
ECU_SOMEIP_SOURCE_BIN="$(resolve_binary autosar_user_tpl_ecu_someip_source || true)"

can_run_someip_transport="ON"
can_run_dds_transport="ON"
can_run_zerocopy_transport="ON"
can_run_ecu_flow="ON"
can_run_ecu_someip_flow="ON"

if [[ "${RUN_SOMEIP}" == "ON" ]]; then
  if [[ -n "${SOMEIP_PROVIDER_BIN}" && -n "${SOMEIP_CONSUMER_BIN}" ]]; then
    record_result "SOME/IP binaries" "PASS" "provider/consumer templates found"
  else
    record_result "SOME/IP binaries" "FAIL" "autosar_user_com_someip_* binaries missing"
    can_run_someip_transport="OFF"
  fi
  if [[ -n "${VSO_CONFIG}" ]]; then
    record_result "SOME/IP config" "PASS" "${VSO_CONFIG}"
  else
    record_result "SOME/IP config" "FAIL" "vsomeip-pubsub-sample.json not found"
    can_run_someip_transport="OFF"
    can_run_ecu_someip_flow="OFF"
  fi
fi

if [[ "${RUN_DDS}" == "ON" ]]; then
  if [[ -n "${DDS_PUB_BIN}" && -n "${DDS_SUB_BIN}" ]]; then
    record_result "DDS binaries" "PASS" "pub/sub templates found"
  else
    record_result "DDS binaries" "FAIL" "autosar_user_com_dds_* binaries missing"
    can_run_dds_transport="OFF"
  fi
fi

if [[ "${RUN_ZEROCOPY}" == "ON" ]]; then
  if [[ -n "${ZC_PUB_BIN}" && -n "${ZC_SUB_BIN}" ]]; then
    record_result "ZeroCopy binaries" "PASS" "pub/sub templates found"
  else
    record_result "ZeroCopy binaries" "FAIL" "autosar_user_com_zerocopy_* binaries missing"
    can_run_zerocopy_transport="OFF"
  fi
  if [[ -x "/opt/iceoryx/bin/iox-roudi" ]]; then
    record_result "ZeroCopy runtime" "PASS" "/opt/iceoryx/bin/iox-roudi found"
  else
    record_result "ZeroCopy runtime" "FAIL" "/opt/iceoryx/bin/iox-roudi not found"
    can_run_zerocopy_transport="OFF"
  fi
fi

if [[ "${RUN_ECU}" == "ON" ]]; then
  if [[ -n "${ECU_BIN}" ]]; then
    record_result "ECU full-stack binary" "PASS" "autosar_user_tpl_ecu_full_stack found"
  else
    record_result "ECU full-stack binary" "FAIL" "autosar_user_tpl_ecu_full_stack missing"
    can_run_ecu_flow="OFF"
    can_run_ecu_someip_flow="OFF"
  fi

  if [[ "${CAN_BACKEND}" == "socketcan" ]]; then
    if ip link show "${CAN_IFNAME}" >/dev/null 2>&1; then
      record_result "SocketCAN interface" "PASS" "${CAN_IFNAME} exists"
    else
      record_result "SocketCAN interface" "FAIL" "${CAN_IFNAME} not found"
      can_run_ecu_flow="OFF"
    fi
  else
    record_result "SocketCAN interface" "SKIP" "mock backend selected"
  fi

  if [[ "${RUN_SOMEIP}" == "ON" ]]; then
    if [[ -n "${ECU_SOMEIP_SOURCE_BIN}" ]]; then
      record_result "ECU SOME/IP source binary" "PASS" "autosar_user_tpl_ecu_someip_source found"
    else
      record_result "ECU SOME/IP source binary" "FAIL" "autosar_user_tpl_ecu_someip_source missing"
      can_run_ecu_someip_flow="OFF"
    fi
  fi
fi

if [[ "${CHECK_ONLY}" == "ON" ]]; then
  record_result "Smoke tests" "SKIP" "--check-only specified"
  print_summary
  if [[ "${FAIL_COUNT}" -gt 0 ]]; then
    exit 1
  fi
  exit 0
fi

run_someip_transport_smoke() {
  local pub_log="${BASE_LOG_DIR}/someip_provider.log"
  local sub_log="${BASE_LOG_DIR}/someip_consumer.log"
  local pub_pid=""
  local sub_pid=""

  rm -f /tmp/vsomeip-*
  export VSOMEIP_CONFIGURATION="${VSO_CONFIG}"

  (
    export ADAPTIVE_AUTOSAR_VSOMEIP_SERVER_APP="adaptive_autosar_pubsub_provider"
    timeout "$((SMOKE_SECONDS + 14))s" "${SOMEIP_PROVIDER_BIN}" --period-ms=80 >"${pub_log}" 2>&1
  ) &
  pub_pid=$!

  sleep 1

  (
    export ADAPTIVE_AUTOSAR_VSOMEIP_CLIENT_APP="adaptive_autosar_pubsub_consumer"
    timeout "$((SMOKE_SECONDS + 12))s" "${SOMEIP_CONSUMER_BIN}" --poll-ms=20 >"${sub_log}" 2>&1
  ) &
  sub_pid=$!

  wait "${sub_pid}" 2>/dev/null || true
  wait "${pub_pid}" 2>/dev/null || true

  if grep -q "ARA_COM_USE_VSOMEIP is disabled" "${pub_log}" "${sub_log}" 2>/dev/null; then
    record_result "SOME/IP transport smoke" "SKIP" "SOME/IP backend disabled in build"
    return
  fi

  local host_ok=0
  local proxy_ok=0
  local published_count=0
  local received_count=0

  if grep -Eq "routing manager \\[Host\\]|Instantiating routing manager \\[Host\\]" "${pub_log}"; then
    host_ok=1
  fi
  if grep -Eq "routing manager \\[Proxy\\]|Instantiating routing manager \\[Proxy\\]" "${sub_log}"; then
    proxy_ok=1
  fi
  published_count="$(grep -c "Published frame seq=" "${pub_log}" || true)"
  received_count="$(grep -c "Received seq=" "${sub_log}" || true)"

  if [[ "${host_ok}" -eq 1 && "${proxy_ok}" -eq 1 && "${published_count}" -gt 0 && "${received_count}" -gt 0 ]]; then
    record_result "SOME/IP transport smoke" "PASS" "host/proxy routing active, published=${published_count}, received=${received_count}"
  else
    record_result "SOME/IP transport smoke" "FAIL" "published=${published_count}, received=${received_count}, host=${host_ok}, proxy=${proxy_ok}, logs=${pub_log},${sub_log}"
  fi
}

run_dds_transport_smoke() {
  local pub_log="${BASE_LOG_DIR}/dds_pub.log"
  local sub_log="${BASE_LOG_DIR}/dds_sub.log"
  local pub_pid=""
  local sub_pid=""

  (
    timeout "$((SMOKE_SECONDS + 12))s" "${DDS_SUB_BIN}" --poll-ms=20 >"${sub_log}" 2>&1
  ) &
  sub_pid=$!

  sleep 1

  (
    timeout "$((SMOKE_SECONDS + 8))s" "${DDS_PUB_BIN}" --period-ms=80 >"${pub_log}" 2>&1
  ) &
  pub_pid=$!

  sleep "${SMOKE_SECONDS}"

  safe_kill_and_wait "${pub_pid}"
  safe_kill_and_wait "${sub_pid}"

  if grep -q "ARA_COM_USE_CYCLONEDDS is disabled" "${pub_log}" "${sub_log}" 2>/dev/null; then
    record_result "DDS transport smoke" "SKIP" "DDS backend disabled in build"
    return
  fi
  if grep -q "DDS type code is not generated" "${pub_log}" "${sub_log}" 2>/dev/null; then
    record_result "DDS transport smoke" "SKIP" "DDS IDL type code is not generated"
    return
  fi

  local published_count=0
  local received_count=0
  published_count="$(grep -c "Published DDS sample seq=" "${pub_log}" || true)"
  received_count="$(grep -c "Received DDS sample seq=" "${sub_log}" || true)"

  if [[ "${published_count}" -gt 0 && "${received_count}" -gt 0 ]]; then
    record_result "DDS transport smoke" "PASS" "published=${published_count}, received=${received_count}"
  else
    record_result "DDS transport smoke" "FAIL" "published=${published_count}, received=${received_count}, logs=${pub_log},${sub_log}"
  fi
}

run_zerocopy_transport_smoke() {
  local roudi_log="${BASE_LOG_DIR}/zerocopy_roudi.log"
  local pub_log="${BASE_LOG_DIR}/zerocopy_pub.log"
  local sub_log="${BASE_LOG_DIR}/zerocopy_sub.log"
  local roudi_pid=""
  local pub_pid=""
  local sub_pid=""
  local own_roudi="OFF"

  if pgrep -f "iox-roudi" >/dev/null 2>&1; then
    own_roudi="OFF"
  else
    own_roudi="ON"
    (
      timeout "$((SMOKE_SECONDS + 20))s" /opt/iceoryx/bin/iox-roudi >"${roudi_log}" 2>&1
    ) &
    roudi_pid=$!
    sleep 1

    if ! kill -0 "${roudi_pid}" >/dev/null 2>&1; then
      if grep -q "MEPOO__SEGMENT_COULD_NOT_APPLY_POSIX_RIGHTS_TO_SHARED_MEMORY" "${roudi_log}" 2>/dev/null; then
        if [[ "${STRICT_MODE}" == "ON" ]]; then
          record_result "ZeroCopy transport smoke" "FAIL" "RouDi ACL setup failed, strict mode enabled. log=${roudi_log}"
        else
          record_result "ZeroCopy transport smoke" "SKIP" "RouDi ACL setup failed in this environment. log=${roudi_log}"
        fi
        safe_kill_and_wait "${roudi_pid}"
        return
      fi
    fi
  fi

  (
    timeout "$((SMOKE_SECONDS + 12))s" "${ZC_SUB_BIN}" --poll-ms=20 >"${sub_log}" 2>&1
  ) &
  sub_pid=$!
  sleep 1

  (
    timeout "$((SMOKE_SECONDS + 8))s" "${ZC_PUB_BIN}" --period-ms=80 >"${pub_log}" 2>&1
  ) &
  pub_pid=$!

  sleep "${SMOKE_SECONDS}"

  safe_kill_and_wait "${pub_pid}"
  safe_kill_and_wait "${sub_pid}"
  if [[ "${own_roudi}" == "ON" ]]; then
    safe_kill_and_wait "${roudi_pid}"
  fi

  if grep -q "ARA_COM_USE_ICEORYX is disabled" "${pub_log}" "${sub_log}" 2>/dev/null; then
    record_result "ZeroCopy transport smoke" "SKIP" "Zero-copy backend disabled in build"
    return
  fi

  local published_count=0
  local received_count=0
  published_count="$(grep -c "Published zero-copy frame seq=" "${pub_log}" || true)"
  received_count="$(grep -c "Received zero-copy frame seq=" "${sub_log}" || true)"

  if [[ "${published_count}" -gt 0 && "${received_count}" -gt 0 ]]; then
    record_result "ZeroCopy transport smoke" "PASS" "published=${published_count}, received=${received_count}"
  else
    record_result "ZeroCopy transport smoke" "FAIL" "published=${published_count}, received=${received_count}, logs=${pub_log},${sub_log}"
  fi
}

run_ecu_can_to_dds_smoke() {
  local ecu_log="${BASE_LOG_DIR}/ecu_can_to_dds.log"

  timeout "$((SMOKE_SECONDS + 12))s" "${ECU_BIN}" \
    --can-backend="${CAN_BACKEND}" \
    --ifname="${CAN_IFNAME}" \
    --enable-can=true \
    --enable-someip=false \
    --publish-period-ms=50 \
    --log-every=5 \
    --storage-sync-every=10 \
    >"${ecu_log}" 2>&1 || true

  if grep -q "DDS backend is disabled" "${ecu_log}"; then
    record_result "ECU CAN->DDS smoke" "SKIP" "DDS backend disabled in build"
    return
  fi

  local published_count=0
  local dds_tx_max=0
  published_count="$(grep -c "Published seq=" "${ecu_log}" || true)"
  dds_tx_max="$(extract_max_counter "dds_tx_total" "${ecu_log}")"

  if [[ "${published_count}" -gt 0 && "${dds_tx_max}" -gt 0 ]]; then
    record_result "ECU CAN->DDS smoke" "PASS" "published=${published_count}, dds_tx_max=${dds_tx_max}"
  else
    record_result "ECU CAN->DDS smoke" "FAIL" "published=${published_count}, dds_tx_max=${dds_tx_max}, log=${ecu_log}"
  fi
}

run_ecu_someip_to_dds_smoke() {
  local source_log="${BASE_LOG_DIR}/ecu_someip_source.log"
  local ecu_log="${BASE_LOG_DIR}/ecu_someip_to_dds.log"
  local src_pid=""
  local ecu_pid=""

  rm -f /tmp/vsomeip-*
  export VSOMEIP_CONFIGURATION="${VSO_CONFIG}"

  (
    export ADAPTIVE_AUTOSAR_VSOMEIP_SERVER_APP="adaptive_autosar_pubsub_provider"
    timeout "$((SMOKE_SECONDS + 18))s" "${ECU_SOMEIP_SOURCE_BIN}" --period-ms=80 >"${source_log}" 2>&1
  ) &
  src_pid=$!
  sleep 1

  (
    export ADAPTIVE_AUTOSAR_VSOMEIP_CLIENT_APP="adaptive_autosar_pubsub_consumer"
    timeout "$((SMOKE_SECONDS + 14))s" "${ECU_BIN}" \
      --can-backend=mock \
      --enable-can=false \
      --enable-someip=true \
      --require-someip=true \
      --service-wait-ms=6000 \
      --publish-period-ms=50 \
      --log-every=5 \
      --storage-sync-every=10 \
      >"${ecu_log}" 2>&1
  ) &
  ecu_pid=$!

  wait "${ecu_pid}" 2>/dev/null || true
  wait "${src_pid}" 2>/dev/null || true

  if grep -q "SOME/IP backend is disabled" "${source_log}" 2>/dev/null || \
     grep -q "SOME/IP input requested but ARA_COM_USE_VSOMEIP is disabled" "${ecu_log}" 2>/dev/null; then
    record_result "ECU SOME/IP->DDS smoke" "SKIP" "SOME/IP backend disabled in build"
    return
  fi
  if grep -q "DDS backend is disabled" "${ecu_log}" 2>/dev/null; then
    record_result "ECU SOME/IP->DDS smoke" "SKIP" "DDS backend disabled in build"
    return
  fi

  local source_published=0
  local mode_count=0
  local someip_rx_max=0
  local dds_tx_max=0
  source_published="$(grep -c "Published SOME/IP source frame seq=" "${source_log}" || true)"
  mode_count="$(grep -Ec "mode=someip_only|mode=fused" "${ecu_log}" || true)"
  someip_rx_max="$(extract_max_counter "someip_rx_total" "${ecu_log}")"
  dds_tx_max="$(extract_max_counter "dds_tx_total" "${ecu_log}")"

  if [[ "${source_published}" -gt 0 && "${mode_count}" -gt 0 && "${someip_rx_max}" -gt 0 && "${dds_tx_max}" -gt 0 ]]; then
    record_result "ECU SOME/IP->DDS smoke" "PASS" "source_pub=${source_published}, someip_rx_max=${someip_rx_max}, dds_tx_max=${dds_tx_max}"
  else
    record_result "ECU SOME/IP->DDS smoke" "FAIL" "source_pub=${source_published}, mode_logs=${mode_count}, someip_rx_max=${someip_rx_max}, dds_tx_max=${dds_tx_max}, logs=${source_log},${ecu_log}"
  fi
}

if [[ "${RUN_SOMEIP}" == "ON" ]]; then
  if [[ "${can_run_someip_transport}" == "ON" ]]; then
    run_someip_transport_smoke
  else
    record_result "SOME/IP transport smoke" "SKIP" "prerequisites failed"
  fi
fi

if [[ "${RUN_DDS}" == "ON" ]]; then
  if [[ "${can_run_dds_transport}" == "ON" ]]; then
    run_dds_transport_smoke
  else
    record_result "DDS transport smoke" "SKIP" "prerequisites failed"
  fi
fi

if [[ "${RUN_ZEROCOPY}" == "ON" ]]; then
  if [[ "${can_run_zerocopy_transport}" == "ON" ]]; then
    run_zerocopy_transport_smoke
  else
    record_result "ZeroCopy transport smoke" "SKIP" "prerequisites failed"
  fi
fi

if [[ "${RUN_ECU}" == "ON" ]]; then
  if [[ "${can_run_ecu_flow}" == "ON" ]]; then
    run_ecu_can_to_dds_smoke
  else
    record_result "ECU CAN->DDS smoke" "SKIP" "prerequisites failed"
  fi

  if [[ "${RUN_SOMEIP}" == "ON" ]]; then
    if [[ "${can_run_ecu_someip_flow}" == "ON" ]]; then
      run_ecu_someip_to_dds_smoke
    else
      record_result "ECU SOME/IP->DDS smoke" "SKIP" "prerequisites failed"
    fi
  fi
fi

print_summary

if [[ "${FAIL_COUNT}" -gt 0 ]]; then
  echo "[ERROR] Verification failed. Inspect logs: ${BASE_LOG_DIR}" >&2
  exit 1
fi

echo "[OK] Raspberry Pi ECU profile verification passed."
