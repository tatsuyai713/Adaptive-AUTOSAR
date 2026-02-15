# User Apps (AUTOSAR AP External Consumer Project)

`user_apps` is an external consumer project built against installed AUTOSAR AP
artifacts (for example `/opt/autosar_ap`).

This directory is organized by responsibility:
- `src/apps`: executable user applications grouped by function
- `src/features`: reusable feature modules used by advanced apps

## Build

From repository root:

```bash
./scripts/build_user_apps_from_opt.sh --prefix /opt/autosar_ap
```

Built executables are generated under subdirectories such as:
- `build-user-apps-opt/src/apps/basic/`
- `build-user-apps-opt/src/apps/communication/*/`
- `build-user-apps-opt/src/apps/feature/*/`

Quick lookup:

```bash
find build-user-apps-opt -type f -name "autosar_user_*" -perm -111 | sort
```

From your own external source tree:

```bash
./scripts/build_user_apps_from_opt.sh \
  --prefix /opt/autosar_ap \
  --source-dir /path/to/your_user_apps
```

### Folder-separated CMake

`user_apps` is split into folder-level CMake files:

- Root common config: `user_apps/CMakeLists.txt`
- App group dispatcher: `user_apps/src/apps/CMakeLists.txt`
- Per-folder target files:
  - `user_apps/src/apps/basic/CMakeLists.txt`
  - `user_apps/src/apps/communication/*/CMakeLists.txt`
  - `user_apps/src/apps/feature/*/CMakeLists.txt`

You can build only selected groups via CMake options:

- `-DUSER_APPS_BUILD_APPS_BASIC=ON|OFF`
- `-DUSER_APPS_BUILD_APPS_COMMUNICATION=ON|OFF`
- `-DUSER_APPS_BUILD_APPS_FEATURE=ON|OFF`

Example (build only feature apps):

```bash
AP_CMAKE_DIR=/opt/autosar_ap/lib/cmake/AdaptiveAutosarAP
if [ ! -f "${AP_CMAKE_DIR}/AdaptiveAutosarAPConfig.cmake" ]; then
  AP_CMAKE_DIR=/opt/autosar_ap/lib64/cmake/AdaptiveAutosarAP
fi

cmake -S user_apps -B build-user-apps-feature-only \
  -DAUTOSAR_AP_PREFIX=/opt/autosar_ap \
  -DAdaptiveAutosarAP_DIR="${AP_CMAKE_DIR}" \
  -DUSER_APPS_BUILD_APPS_BASIC=OFF \
  -DUSER_APPS_BUILD_APPS_COMMUNICATION=OFF \
  -DUSER_APPS_BUILD_APPS_FEATURE=ON
cmake --build build-user-apps-feature-only -j"$(nproc)"
```

## App Targets

### Basic apps (`user_apps/src/apps/basic/`)
- `autosar_user_minimal_runtime`
- `autosar_user_exec_signal_template`
- `autosar_user_per_phm_demo`

### Communication apps (`user_apps/src/apps/communication/`)
- `autosar_user_com_someip_provider_template`
- `autosar_user_com_someip_consumer_template`
- `autosar_user_com_zerocopy_pub_template`
- `autosar_user_com_zerocopy_sub_template`
- `autosar_user_com_dds_pub_template`
- `autosar_user_com_dds_sub_template`

### Feature apps (`user_apps/src/apps/feature/`)
- `autosar_user_tpl_runtime_lifecycle`
- `autosar_user_tpl_can_socketcan_receiver`
- `autosar_user_tpl_ecu_full_stack`
- `autosar_user_tpl_ecu_someip_source`

## Source Layout

- App entry points:
  - `user_apps/src/apps/basic/`
  - `user_apps/src/apps/communication/`
  - `user_apps/src/apps/feature/`
- Shared feature modules:
  - `user_apps/src/features/communication/vehicle_status/`
  - `user_apps/src/features/communication/pubsub/`
  - `user_apps/src/features/communication/can/`
  - `user_apps/src/features/ecu/`

## Feature Tutorials

All tutorials are under `user_apps/tutorials/`.

- Index: `user_apps/tutorials/README.ja.md`
- Runtime lifecycle: `user_apps/tutorials/01_runtime_lifecycle.ja.md`
- Execution signal handling: `user_apps/tutorials/02_exec_signal.ja.md`
- Persistency + PHM: `user_apps/tutorials/03_per_phm.ja.md`
- SOME/IP pub/sub: `user_apps/tutorials/04_someip_pubsub.ja.md`
- Zero-copy pub/sub: `user_apps/tutorials/05_zerocopy_pubsub.ja.md`
- DDS pub/sub: `user_apps/tutorials/06_dds_pubsub.ja.md`
- SocketCAN receive/decode: `user_apps/tutorials/07_socketcan_decode.ja.md`
- Full ECU flow: `user_apps/tutorials/08_ecu_full_stack.ja.md`
- Raspberry Pi ECU deployment: `user_apps/tutorials/09_rpi_ecu_deployment.ja.md`
- Porting from Vector/ETAS/EB assets: `user_apps/tutorials/10_vendor_autosar_asset_porting.ja.md`
- Ubuntu DoIP/DIAG host tester for Raspberry Pi ECU: `tools/host_tools/doip_diag_tester/README.ja.md`

## Quick Start: Add Your Own App

1. Copy a feature app source as your new app.

```bash
cp user_apps/src/apps/feature/ecu/ecu_full_stack_app.cpp \
  user_apps/src/my_new_ecu_app.cpp
```

2. Place the source under the matching app group (for example
   `user_apps/src/apps/feature/ecu/`) and register the target in that folder's
   `CMakeLists.txt` (not in the root file) while linking imported AUTOSAR AP
   targets (`AdaptiveAutosarAP::ara_core`, `AdaptiveAutosarAP::ara_com`, etc).

3. Rebuild with `scripts/build_user_apps_from_opt.sh`.

4. Run your new binary from the build directory.
