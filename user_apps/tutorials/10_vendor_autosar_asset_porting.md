# 10: Porting Vector/ETAS/EB Assets to This Platform

## Target and Goal

This tutorial targets assets meeting the following criteria:

- Existing assets: C++ application assets written for Adaptive AUTOSAR implementations by Vector / ETAS / EB, etc.
- Goal:
  - Rebuild and run them on this repository's implementation
  - Confirm `ara::com` communication with other UNITs

Important:

- This is a **source-compatible** porting procedure.
- It is NOT a procedure to run pre-built vendor binaries as-is.

## 0) Constraints to Understand First

### Assets That Port Easily

- Primarily use standard APIs (`ara::core`, `ara::com`, `ara::exec`, `ara::log`, `ara::diag`)
- Do not depend on vendor-specific headers or namespaces
- Explicitly state communication parameters (Service/Instance/Event/DDS topic, etc.)

### Assets That Require Modification

- Directly reference vendor-specific APIs (custom layers calling vSomeIP directly, custom EM APIs, etc.)
- Assume vendor-generated Proxy/Skeleton class names and placement
- Fix runtime configuration to a vendor-proprietary format

## 1) Install the Runtime

First install this implementation:

```bash
sudo ./scripts/build_and_install_autosar_ap.sh --prefix /opt/autosar_ap
```

For Raspberry Pi ECU use:

```bash
sudo ./scripts/build_and_install_rpi_ecu_profile.sh \
  --prefix /opt/autosar_ap \
  --runtime-build-dir build-rpi-autosar-ap \
  --user-app-build-dir /opt/autosar_ap/user_apps_build
```

## 2) Inventory Existing Assets (Portability Assessment)

Check existing sources for the following:

1. No vendor-specific includes
2. Event/method calls through `ara::com` stay within the standard API
3. Service ID / Instance ID / Event ID can be obtained
4. Switchable points for runtime configuration (SOME/IP, DDS, ZeroCopy)

Recommended commands:

```bash
rg -n "vector|etas|elektrobit|vsomeip|iceoryx|cyclonedds" /path/to/your_asset
rg -n "ara::com|ara::core|ara::exec|ara::log" /path/to/your_asset
```

## 3) Create a User App Project

Use existing `user_apps` as a template or prepare an external source tree:

```bash
cp -r /opt/autosar_ap/user_apps /path/to/your_user_apps
```

Build:

```bash
./scripts/build_user_apps_from_opt.sh \
  --prefix /opt/autosar_ap \
  --source-dir /path/to/your_user_apps \
  --build-dir build-your-user-apps
```

## 4) Align CMake to Standard Targets

Link against imported targets from `/opt/autosar_ap`:

```cmake
find_package(AdaptiveAutosarAP REQUIRED CONFIG
  PATHS /opt/autosar_ap/lib/cmake/AdaptiveAutosarAP
  NO_DEFAULT_PATH)

add_executable(my_ecu_app src/my_ecu_app.cpp)
target_link_libraries(my_ecu_app
  PRIVATE
    AdaptiveAutosarAP::ara_core
    AdaptiveAutosarAP::ara_com
    AdaptiveAutosarAP::ara_exec
    AdaptiveAutosarAP::ara_log)
```

## 5) Communication Asset Alignment

### Using SOME/IP

1. Align app name with `configuration/vsomeip-pubsub-sample.json`
2. Set `VSOMEIP_CONFIGURATION` at runtime
3. Match Service/Instance/Event IDs with the peer UNIT

```bash
export VSOMEIP_CONFIGURATION=/opt/autosar_ap/configuration/vsomeip-pubsub-sample.json
```

### Using DDS

1. Align Domain ID and Topic name with the peer UNIT
2. Align IDL type compatibility (this repository uses `idlc`)

### Using ZeroCopy

1. Start `iox-roudi`
2. Use the same channel descriptor for Publisher and Subscriber

## 6) Vendor Dependency Replacement Strategy

As a principle, keep the application layer using only `ara::` APIs:

1. Isolate vendor-specific wrappers to a single location
2. Keep the application body to `ara::com` API calls only
3. Make IDs and topics configurable values
4. Restrict CMake dependencies to `/opt/autosar_ap` only

## 7) Communication Verification with Existing UNITs

### A. Confirm the foundation with standalone communication (between templates)

```bash
./scripts/verify_rpi_ecu_profile.sh \
  --prefix /opt/autosar_ap \
  --user-app-build-dir /opt/autosar_ap/user_apps_build \
  --can-backend mock \
  --require-platform-binary
```

### B. Verify with your ported application

1. Start the Provider-side UNIT
2. Start the ported Consumer side
3. Check receive logs and counters
4. Verify in the reverse direction (Consumer/Provider) as well

Verification criteria:

- Service discovery is established
- Subscribe/ReceiveHandler is being called
- Expected payload can be decoded

## 8) Raspberry Pi ECU Deployment as Persistent Service

Run like an ECU with `systemd`:

```bash
sudo ./scripts/install_rpi_ecu_services.sh \
  --prefix /opt/autosar_ap \
  --user-app-build-dir /opt/autosar_ap/user_apps_build \
  --enable
```

Environment variable configuration:

- `/etc/default/autosar-ecu-full-stack`

## 9) Troubleshooting

### SOME/IP service not found

- Check `VSOMEIP_CONFIGURATION` path
- Check for duplicate app name/Client ID
- Mismatch in Service/Instance/Event IDs

### DDS not receiving

- Domain ID / Topic mismatch
- IDL type differences (field order/types)

### ZeroCopy not working

- `iox-roudi` not started
- Shared memory permission issue (environment-specific)

## 10) Practical Checklist

1. Eliminated vendor-specific API dependencies
2. Can build with `ara::` APIs only
3. Can build referencing only `/opt/autosar_ap`
4. Aligned IDs / topics / versions with peer UNITs
5. Documented the communication verification procedure including startup order
6. Confirmed restart and log retrieval procedures for `systemd` persistent operation

Meeting these 6 criteria makes it straightforward to operate vendor implementation assets on this platform as "development and verification assets that can communicate with other UNITs."
