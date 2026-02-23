# 11: Manifest-Profile Switching E2E

## Target

- Build/install script:
  - `scripts/build_and_install_autosar_ap.sh`
- Switchable user-app build/run script:
  - `scripts/build_switchable_pubsub_sample.sh`
  - (script name keeps `sample` for backward compatibility)
- User-app source:
  - `user_apps/src/apps/communication/switchable_pubsub/src/switchable_pub.cpp`
  - `user_apps/src/apps/communication/switchable_pubsub/src/switchable_sub.cpp`

## Purpose

- Verify backend switching with one binary pair by changing only `ARA_COM_BINDING_MANIFEST`.
- Run the same publisher/subscriber binaries in DDS, iceoryx, and vSomeIP profiles.
- Optional: override profile selection at runtime with `ARA_COM_EVENT_BINDING`
  (`dds|iceoryx|vsomeip|auto`) for debugging.

## Prerequisites

```bash
export AUTOSAR_AP_PREFIX=/tmp/autosar_ap
export CYCLONEDDS_PREFIX=/opt/cyclonedds
export VSOMEIP_PREFIX=/opt/vsomeip
```

Installed middleware/runtime:

- CycloneDDS under `${CYCLONEDDS_PREFIX}`
- vSomeIP under `${VSOMEIP_PREFIX}`
- iceoryx under `/opt/iceoryx`

## 1) Build and Install Runtime

```bash
./scripts/build_and_install_autosar_ap.sh \
  --prefix "${AUTOSAR_AP_PREFIX}" \
  --build-dir build-install-manifest-e2e \
  --build-type Release \
  --source-dir "$PWD" \
  --with-platform-app
```

## 2) Build Switchable User App and Run Smoke E2E

This single command runs:

- DDS-profile smoke
- iceoryx-profile smoke
- vSomeIP-profile smoke

```bash
./scripts/build_switchable_pubsub_sample.sh \
  --prefix "${AUTOSAR_AP_PREFIX}" \
  --cyclonedds-prefix "${CYCLONEDDS_PREFIX}" \
  --build-dir build-switchable-pubsub-sample-e2e \
  --run-smoke
```

Expected key output:

- `[OK] DDS-profile smoke passed (...)`
- `[OK] iceoryx-profile smoke passed (...)`
- `[OK] vSomeIP-profile smoke passed (...)`
- `[OK] Switchable pub/sub smoke checks passed`

## 3) Manual Run (Optional)

### 3-1. Runtime environment

```bash
export LD_LIBRARY_PATH=${AUTOSAR_AP_PREFIX}/lib:${AUTOSAR_AP_PREFIX}/lib64:${CYCLONEDDS_PREFIX}/lib:/opt/vsomeip/lib:/opt/iceoryx/lib:${LD_LIBRARY_PATH:-}
```

### 3-2. DDS profile

Terminal 1:

```bash
unset ARA_COM_EVENT_BINDING
unset ARA_COM_PREFER_SOMEIP
unset VSOMEIP_CONFIGURATION
export ARA_COM_BINDING_MANIFEST=$PWD/build-switchable-pubsub-sample-e2e/generated/switchable_manifest_dds.yaml
./build-switchable-pubsub-sample-e2e/autosar_switchable_pubsub_sub
```

Terminal 2:

```bash
unset ARA_COM_EVENT_BINDING
unset ARA_COM_PREFER_SOMEIP
unset VSOMEIP_CONFIGURATION
export ARA_COM_BINDING_MANIFEST=$PWD/build-switchable-pubsub-sample-e2e/generated/switchable_manifest_dds.yaml
./build-switchable-pubsub-sample-e2e/autosar_switchable_pubsub_pub
```

### 3-3. iceoryx profile

Terminal 1:

```bash
unset ARA_COM_EVENT_BINDING
unset ARA_COM_PREFER_SOMEIP
unset VSOMEIP_CONFIGURATION
export ARA_COM_BINDING_MANIFEST=$PWD/build-switchable-pubsub-sample-e2e/generated/switchable_manifest_iceoryx.yaml
iox-roudi
```

Terminal 2:

```bash
unset ARA_COM_EVENT_BINDING
unset ARA_COM_PREFER_SOMEIP
unset VSOMEIP_CONFIGURATION
export ARA_COM_BINDING_MANIFEST=$PWD/build-switchable-pubsub-sample-e2e/generated/switchable_manifest_iceoryx.yaml
./build-switchable-pubsub-sample-e2e/autosar_switchable_pubsub_sub
```

Terminal 3:

```bash
unset ARA_COM_EVENT_BINDING
unset ARA_COM_PREFER_SOMEIP
unset VSOMEIP_CONFIGURATION
export ARA_COM_BINDING_MANIFEST=$PWD/build-switchable-pubsub-sample-e2e/generated/switchable_manifest_iceoryx.yaml
./build-switchable-pubsub-sample-e2e/autosar_switchable_pubsub_pub
```

### 3-4. vSomeIP profile

Terminal 1:

```bash
unset ARA_COM_EVENT_BINDING
unset ARA_COM_PREFER_SOMEIP
export ARA_COM_BINDING_MANIFEST=$PWD/build-switchable-pubsub-sample-e2e/generated/switchable_manifest_vsomeip.yaml
export VSOMEIP_CONFIGURATION=${AUTOSAR_AP_PREFIX}/configuration/vsomeip-rpi.json
${AUTOSAR_AP_PREFIX}/bin/autosar_vsomeip_routing_manager
```

Terminal 2:

```bash
unset ARA_COM_EVENT_BINDING
unset ARA_COM_PREFER_SOMEIP
export ARA_COM_BINDING_MANIFEST=$PWD/build-switchable-pubsub-sample-e2e/generated/switchable_manifest_vsomeip.yaml
export VSOMEIP_CONFIGURATION=${AUTOSAR_AP_PREFIX}/configuration/vsomeip-rpi.json
./build-switchable-pubsub-sample-e2e/autosar_switchable_pubsub_sub
```

Terminal 3:

```bash
unset ARA_COM_EVENT_BINDING
unset ARA_COM_PREFER_SOMEIP
export ARA_COM_BINDING_MANIFEST=$PWD/build-switchable-pubsub-sample-e2e/generated/switchable_manifest_vsomeip.yaml
export VSOMEIP_CONFIGURATION=${AUTOSAR_AP_PREFIX}/configuration/vsomeip-rpi.json
./build-switchable-pubsub-sample-e2e/autosar_switchable_pubsub_pub
```

## Verification Checklist

- Same binaries are used in all modes:
  - `autosar_switchable_pubsub_pub`
  - `autosar_switchable_pubsub_sub`
- Only the manifest path changes between DDS/iceoryx/vSomeIP.
- Subscriber logs include `I heard seq=` in all modes.

## Troubleshooting

Manifest path check:

```bash
ls build-switchable-pubsub-sample-e2e/generated/switchable_manifest_dds.yaml
ls build-switchable-pubsub-sample-e2e/generated/switchable_manifest_iceoryx.yaml
ls build-switchable-pubsub-sample-e2e/generated/switchable_manifest_vsomeip.yaml
```

Routing manager config check:

```bash
ls "${AUTOSAR_AP_PREFIX}/configuration/vsomeip-rpi.json"
echo "$VSOMEIP_CONFIGURATION"
```

If smoke fails, inspect logs:

```bash
ls build-switchable-pubsub-sample-e2e/switchable_*.log
```
