# 08: ECU Full-Stack Integration

## Target

- `autosar_user_tpl_ecu_full_stack`
- `autosar_user_tpl_ecu_someip_source` (for SOME/IP input validation)
- Source: `user_apps/src/apps/feature/ecu/ecu_full_stack_app.cpp`
  - `user_apps/src/apps/feature/ecu/ecu_someip_source_app.cpp`

## Purpose

- Integrate CAN receive, SOME/IP receive, and DDS send in a single process.
- Create an implementation skeleton combining `ara::exec`, `ara::phm`, and `ara::per`.

## Prerequisites

```bash
export AUTOSAR_AP_PREFIX=/opt/autosar_ap
export VSOMEIP_CONFIGURATION=${AUTOSAR_AP_PREFIX}/configuration/vsomeip-pubsub-sample.json
export LD_LIBRARY_PATH=${AUTOSAR_AP_PREFIX}/lib:${AUTOSAR_AP_PREFIX}/lib64:/opt/vsomeip/lib:/opt/cyclonedds/lib:/opt/iceoryx/lib:${LD_LIBRARY_PATH:-}
```

## Run

Mock CAN + SOME/IP enabled:

```bash
./build-user-apps-opt/src/apps/feature/ecu/autosar_user_tpl_ecu_full_stack \
  --can-backend=mock \
  --enable-can=true \
  --enable-someip=true \
  --publish-period-ms=50 \
  --log-every=20
```

SOME/IP input only (start source in a separate terminal):

```bash
./build-user-apps-opt/src/apps/feature/ecu/autosar_user_tpl_ecu_someip_source --period-ms=100
./build-user-apps-opt/src/apps/feature/ecu/autosar_user_tpl_ecu_full_stack \
  --enable-can=false \
  --enable-someip=true \
  --require-someip=true
```

CAN only:

```bash
./build-user-apps-opt/src/apps/feature/ecu/autosar_user_tpl_ecu_full_stack \
  --can-backend=socketcan \
  --ifname=can0 \
  --enable-someip=false
```

## Key Edit Locations by Feature

1. Input integration logic: `TryBuildOutputFrame(...)`
2. CAN decode configuration: `ParseRuntimeConfig(...)` and `VehicleStatusCanDecoder::Config`
3. DDS output configuration: Domain/Topic passed to `BuildDdsProfile(...)`
4. Health reporting: `HealthReporter` — `ReportOk` / `ReportFailed` / `ReportDeactivated`
5. Persistence: Key design of `PersistentCounterStore` and `Sync` timing

## Recommended Implementation Order

1. First verify with CAN only.
2. Then enable SOME/IP input and verify the integrated result.
3. Finally enable ZeroCopy local delivery and verify performance.
