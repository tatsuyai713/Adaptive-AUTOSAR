# Adaptive-AUTOSAR
![CI](https://github.com/tatsuyai713/Adaptive-AUTOSAR/actions/workflows/cmake.yml/badge.svg)

Linux-oriented educational implementation of Adaptive AUTOSAR style APIs.

Language:
- English: `README.md`
- Japanese: `README.ja.md`

## Overview

This repository provides a working implementation of the AUTOSAR Adaptive Platform APIs for Linux-based environments. It can be used in two ways:

1. **Linux simulation environment** — develop and test AUTOSAR Adaptive applications on a desktop Linux or Docker without real ECU hardware
2. **ECU deployment** — deploy to a Raspberry Pi (or QNX target) as a prototype ECU with systemd service management, SocketCAN, time synchronization, and more

### What you get

- **15 `ara::*` modules** fully implemented (R24-11 baseline, 100% SWS API coverage)
- **3 communication transports**: SOME/IP (vSomeIP), DDS (CycloneDDS), zero-copy IPC (iceoryx) — switchable at runtime
- **E2E protection**: Profiles 01–07 and 11, with state machine and per-sample status
- **SecOC**: Freshness management, HMAC-based MAC, key rotation
- **Diagnostics**: Full UDS service stack (0x10–0x3E), OBD-II, DoIP host tester
- **ARXML toolchain**: YAML → ARXML → C++ proxy/skeleton/binding header generation
- **User app templates**: 13 ready-to-use application templates with step-by-step tutorials
- **Vendor asset porting**: Rebuild Vector/ETAS/EB-developed C++ assets at source level

### Scope and version

- ARXML schema baseline: `http://autosar.org/schema/r4.0` (`autosar_00050.xsd`)
- This is an educational implementation. It does not claim official AUTOSAR conformance certification.
- Non-standard additions are provided as extensions under `ara::<domain>::extension` and do not replace standard API signatures.

---

## Prerequisites

- C++14 compiler
- CMake >= 3.14
- Python3 + `PyYAML`
- OpenSSL development package (`libcrypto`)

Middleware (built automatically by bundled scripts):

| Library | Default prefix | Notes |
|---|---|---|
| iceoryx v2.0.6 | `/opt/iceoryx` | ACL-tolerant patch for container environments |
| CycloneDDS 0.10.5 | `/opt/cyclonedds` | SHM auto-enabled via iceoryx |
| vsomeip 3.4.10 + CDR | `/opt/vsomeip` | CDR lib extracted from CycloneDDS-CXX; no DDS runtime needed |

---

## Quick Start: Linux Simulation Environment

The following steps let you build, install, and run AUTOSAR AP applications on any Linux machine or Docker container.

### 1. Install dependencies and middleware

```bash
# One-command install (system packages + all middleware)
sudo ./scripts/install_middleware_stack.sh --install-base-deps
```

Or step by step:

```bash
sudo ./scripts/install_dependency.sh
sudo ./scripts/install_middleware_stack.sh
```

### 2. Build and install the AUTOSAR AP runtime

```bash
# Quick install to /tmp (no root required)
./scripts/build_and_install_autosar_ap.sh \
  --prefix /tmp/autosar_ap \
  --build-dir build-install-autosar-ap
```

For a production-like layout:

```bash
sudo ./scripts/build_and_install_autosar_ap.sh --prefix /opt/autosar_ap
```

If middleware is not yet installed, the build script can handle it:

```bash
sudo ./scripts/build_and_install_autosar_ap.sh \
  --prefix /opt/autosar_ap \
  --install-middleware \
  --install-base-deps
```

### 3. Build user applications

```bash
./scripts/build_user_apps_from_opt.sh \
  --prefix /tmp/autosar_ap \
  --source-dir /tmp/autosar_ap/user_apps \
  --build-dir build-user-apps-opt
```

### 4. Run smoke test applications

```bash
./scripts/build_user_apps_from_opt.sh \
  --prefix /tmp/autosar_ap \
  --source-dir /tmp/autosar_ap/user_apps \
  --build-dir build-user-apps-opt-run \
  --run
```

This executes `autosar_user_minimal_runtime` and `autosar_user_per_phm_demo`.

### 5. Try switchable Pub/Sub transports (DDS / iceoryx / SOME/IP)

Build and smoke-test all three transport profiles:

```bash
./scripts/build_switchable_pubsub_sample.sh --run-smoke
```

To manually switch transports with the same binary:

```bash
# DDS profile
export ARA_COM_BINDING_MANIFEST=$PWD/build-switchable-pubsub-sample/generated/switchable_manifest_dds.yaml
./build-switchable-pubsub-sample/autosar_switchable_pubsub_sub &
./build-switchable-pubsub-sample/autosar_switchable_pubsub_pub

# iceoryx profile
export ARA_COM_BINDING_MANIFEST=$PWD/build-switchable-pubsub-sample/generated/switchable_manifest_iceoryx.yaml
iox-roudi &
./build-switchable-pubsub-sample/autosar_switchable_pubsub_sub &
./build-switchable-pubsub-sample/autosar_switchable_pubsub_pub

# SOME/IP profile
export ARA_COM_BINDING_MANIFEST=$PWD/build-switchable-pubsub-sample/generated/switchable_manifest_vsomeip.yaml
export VSOMEIP_CONFIGURATION=/opt/autosar_ap/configuration/vsomeip-rpi.json
/opt/autosar_ap/bin/autosar_vsomeip_routing_manager &
./build-switchable-pubsub-sample/autosar_switchable_pubsub_sub &
./build-switchable-pubsub-sample/autosar_switchable_pubsub_pub
```

---

## ECU Deployment: Raspberry Pi

The repository includes a full deployment profile for running a Raspberry Pi as a prototype ECU with systemd service management.

### What gets deployed

The ECU profile installs 14 systemd services that form the AUTOSAR AP runtime:

| Service | Role |
|---|---|
| `autosar-platform-app` | Platform-side built-in process stack |
| `autosar-exec-manager` | User app launcher (`bringup.sh`) |
| `autosar-vsomeip-routing` | SOME/IP routing manager |
| `autosar-can-manager` | SocketCAN interface manager |
| `autosar-time-sync` | Time synchronization support |
| `autosar-ntp-time-provider` | NTP provider (chrony/ntpd auto-detect) |
| `autosar-ptp-time-provider` | PTP/gPTP provider (ptp4l PHC) |
| `autosar-persistency-guard` | Persistency sync guard |
| `autosar-iam-policy` | IAM policy loader |
| `autosar-user-app-monitor` | User app heartbeat/health monitor with restart recovery |
| `autosar-watchdog` | Watchdog supervisor |
| `autosar-sm-state` | Machine state / network mode manager |

User apps are launched via `/etc/autosar/bringup.sh`.

### Deployment steps

```bash
# 1. Build runtime + user apps + install middleware
sudo ./scripts/build_and_install_rpi_ecu_profile.sh \
  --prefix /opt/autosar_ap \
  --runtime-build-dir build-rpi-autosar-ap \
  --user-app-build-dir /opt/autosar_ap/user_apps_build \
  --install-middleware

# 2. Setup SocketCAN interface
sudo ./scripts/setup_socketcan_interface.sh --ifname can0 --bitrate 500000

# 3. Install and enable systemd services
sudo ./scripts/install_rpi_ecu_services.sh \
  --prefix /opt/autosar_ap \
  --user-app-build-dir /opt/autosar_ap/user_apps_build \
  --enable

# 4. Verify the deployment
./scripts/verify_rpi_ecu_profile.sh \
  --prefix /opt/autosar_ap \
  --user-app-build-dir /opt/autosar_ap/user_apps_build \
  --can-backend mock \
  --require-platform-binary
```

Detailed guides:
- [`deployment/rpi_ecu/README.md`](deployment/rpi_ecu/README.md)
- [`deployment/rpi_ecu/RASPI_SETUP_MANUAL.ja.md`](deployment/rpi_ecu/RASPI_SETUP_MANUAL.ja.md) (Japanese, includes gPTP setup)

---

## QNX 8.0 Cross-Build

For QNX SDP 8.0 targets, cross-compilation scripts are provided for all middleware and the AUTOSAR AP runtime.

```bash
source ~/qnx800/qnxsdp-env.sh
sudo ./scripts/install_middleware_stack_qnx.sh --arch aarch64le --enable-shm
```

See [`qnx/README.md`](qnx/README.md) for the full cross-build flow.

---

## User App Templates

Templates are installed to `<prefix>/user_apps` and cover basic, communication, and full-stack use cases.

| Category | Template | Description |
|---|---|---|
| Basic | `autosar_user_minimal_runtime` | Minimal runtime init/deinit |
| Basic | `autosar_user_exec_signal_template` | Signal handling |
| Basic | `autosar_user_per_phm_demo` | Persistency + health monitoring |
| Communication | `autosar_user_com_someip_provider_template` | SOME/IP publisher |
| Communication | `autosar_user_com_someip_consumer_template` | SOME/IP subscriber |
| Communication | `autosar_user_com_zerocopy_pub_template` | iceoryx zero-copy publisher |
| Communication | `autosar_user_com_zerocopy_sub_template` | iceoryx zero-copy subscriber |
| Communication | `autosar_user_com_dds_pub_template` | DDS publisher |
| Communication | `autosar_user_com_dds_sub_template` | DDS subscriber |
| Feature | `autosar_user_tpl_runtime_lifecycle` | Runtime lifecycle management |
| Feature | `autosar_user_tpl_can_socketcan_receiver` | SocketCAN message decoding |
| Feature | `autosar_user_tpl_ecu_full_stack` | Full ECU stack integration |
| Feature | `autosar_user_tpl_ecu_someip_source` | SOME/IP source ECU |

Tutorials: [`user_apps/tutorials/`](user_apps/tutorials/)

### Porting vendor assets

This repository can rebuild Vector/ETAS/EB-developed C++ app assets at source level. See [`user_apps/tutorials/10_vendor_autosar_asset_porting.ja.md`](user_apps/tutorials/10_vendor_autosar_asset_porting.ja.md).

---

## ARXML and Code Generation

### YAML → ARXML

```bash
python3 tools/arxml_generator/generate_arxml.py \
  --input tools/arxml_generator/examples/pubsub_vsomeip.yaml \
  --output /tmp/pubsub.generated.arxml \
  --overwrite --print-summary
```

### ARXML → ara::com binding header

```bash
python3 tools/arxml_generator/generate_ara_com_binding.py \
  --input /tmp/pubsub.generated.arxml \
  --output /tmp/vehicle_status_manifest_binding.h \
  --namespace sample::vehicle_status::generated \
  --provided-service-short-name VehicleStatusProviderInstance \
  --provided-event-group-short-name VehicleStatusEventGroup
```

Guides: [`tools/arxml_generator/README.md`](tools/arxml_generator/README.md) | [`tools/arxml_generator/YAML_MANUAL.ja.md`](tools/arxml_generator/YAML_MANUAL.ja.md)

---

## Host-Side Diagnostic Tool

A DoIP/UDS diagnostic tester that runs on a host PC (not on the ECU).

```bash
cmake -S tools/host_tools/doip_diag_tester -B build-host-doip-tester
cmake --build build-host-doip-tester -j"$(nproc)"
```

Docs: [`tools/host_tools/doip_diag_tester/README.md`](tools/host_tools/doip_diag_tester/README.md)

---

## Implemented Feature Matrix

Release baseline: `R24-11`. All 15 core `ara::*` modules have reached **100% SWS API coverage**.

Hardware-dependent features (HSM, biometric sensors, CAN-TP/FlexRay, TPM secure boot, etc.) are provided as software abstractions with mock/simulation backends.

| Module | Coverage | Key Capabilities |
| --- | :---: | --- |
| `ara::core` | 100% | `Result`, `Optional`, `Future/Promise`, `ErrorCode/ErrorDomain`, `InstanceSpecifier`, `String/StringView` (C++14 polyfill), `Variant`, `Span`, `Byte`, monadic chaining (`AndThen/OrElse/Map/MapError`), exception hierarchy, `AbortHandler` |
| `ara::log` | 100% | `Logger` with severity filtering, `LogStream` operators, 3 sink types (console/file/network), DLT backend, `AsyncLogSink` (ring-buffer), `FormatterPlugin` (JSON/custom) |
| `ara::com` Common | 100% | Proxy/Skeleton Event/Method/Field wrappers, `FindService`/`StartFindService`, serialization framework (CDR/trivial-copy/string/vector/map), fire-and-forget methods, `CommunicationGroup`, `RawDataStream`, `InstanceIdentifier`, `ServiceVersion`, QoS profiles, `EventCacheUpdatePolicy`, `FilterConfig`, `Transformer` chain, `SampleInfo`/`E2ESampleStatus` |
| `ara::com` SOME/IP | 100% | SD client/server, pub/sub, RPC, SOME/IP-TP segmentation/reassembly, Magic Cookie, `SessionHandler`, IPv6 endpoint, `SdNetworkConfig`, `ConfigurationOption` |
| `ara::com` DDS | 100% | CycloneDDS pub/sub, method bindings (request/reply topics), QoS config (reliable/best-effort/transient-local), `DdsParticipantFactory`, content filters, extended QoS (ownership/partition/resource-limits) |
| `ara::com` Zero-Copy | 100% | iceoryx pub/sub, method bindings, zero-copy loan/publish, `QueueOverflowPolicy`, `ZeroCopyServiceDiscovery`, `PortIntrospection` |
| `ara::com` E2E | 100% | Profiles 01–07 and 11, event/method decorators, `E2EStateMachine` (windowed, enable/disable), per-sample status propagation |
| `ara::com` SecOC | 100% | `FreshnessManager` (monotonic counter, persistence), `SecOcPdu` (HMAC MAC), `SecOcKeyManager`, batch protect/verify, `FreshnessSyncManager` (cross-ECU sync) |
| `ara::exec` | 100% | `ExecutionClient/Server`, `StateClient/Server`, `DeterministicClient`, `FunctionGroup`, `ProcessWatchdog`, `ExecutionManager` (fork/exec lifecycle), `ManifestLoader` (ARXML), `ApplicationClient`, QNX portability |
| `ara::diag` | 100% | `Monitor` (debouncing), UDS services (0x10/0x11/0x14/0x22/0x27/0x28/0x2E/0x2F/0x31/0x34/0x35/0x36/0x37/0x3E/0x85), OBD-II (Mode 01/09), `EventMemory`, `DiagnosticManager` (P2/P2* timing) |
| `ara::phm` | 100% | `SupervisedEntity`, `HealthChannel`, `RecoveryAction`, `AliveSupervision`, `DeadlineSupervision`, `LogicalSupervision`, `PhmOrchestrator` |
| `ara::per` | 100% | `KeyValueStorage`, `FileStorage`, `ReadAccessor`/`ReadWriteAccessor`, recover/reset, batch operations, change observers, `RedundantStorage`, `UpdatePersistency` (UCM migration) |
| `ara::sm` | 100% | `MachineStateClient` (shutdown/restart), `NetworkHandle`, `FunctionGroupStateMachine` (guard/timeout/history), `PowerModeManager`, `DiagnosticStateHandler`, `UpdateRequestHandler` |
| `ara::crypto` | 100% | SHA-1/256/384/512, HMAC, AES-CBC/GCM/CTR, ChaCha20, PBKDF2/HKDF, RSA 2048/4096, ECDSA P-256/P-384, ECDH, X.509 chain verification, CRL revocation, streaming contexts, `KeySlot`/`KeyStorageProvider`, `CryptoServiceProvider` |
| `ara::nm` | 100% | Multi-channel NM state machine (5 states), `NmCoordinator`, `NmPdu` serialization, `CanTpNmAdapter`/`FlexRayNmAdapter` |
| `ara::iam` | 100% | RBAC (`RoleManager`), ABAC (`AbacPolicyEngine`), `Grant`/`GrantManager`, `PolicySigner` (ECDSA), `PasswordStore`, `IdentityManager`, `AuditTrail`, `CapabilityManager` |
| `ara::ucm` | 100% | Update session lifecycle, Transfer API, SHA-256 verification, `CampaignManager` (multi-package), `UpdateHistory`, `DependencyChecker`, `SecureBootManager` |
| `ara::tsync` | 100% | `TimeSyncClient` (drift correction, quality levels), `PtpTimeBaseProvider` (ptp4l/gPTP), `NtpTimeBaseProvider` (chrony/ntpd), `TimeSyncServer`, `RateCorrector` |
| ARXML Tooling | — | YAML → ARXML, ARXML → ara::com binding header generation |
| RPi ECU Profile | — | systemd deployment, 14 resident daemons, SocketCAN, time sync, health monitoring |

### AUTOSAR AP release profile

Default: `R24-11`. Configurable via CMake:

```bash
cmake -S . -B build -DAUTOSAR_AP_RELEASE_PROFILE=R24-11
```

Queryable at runtime via `ara::core::ApReleaseInfo`.

---

## Documentation

Full index: [`docs/README.md`](docs/README.md) | [`docs/README.ja.md`](docs/README.ja.md)

| Topic | Document |
| --- | --- |
| Raspberry Pi ECU deployment | [`deployment/rpi_ecu/README.md`](deployment/rpi_ecu/README.md) |
| Raspberry Pi gPTP setup (Japanese) | [`deployment/rpi_ecu/RASPI_SETUP_MANUAL.ja.md`](deployment/rpi_ecu/RASPI_SETUP_MANUAL.ja.md) |
| QNX 8.0 cross-compilation | [`qnx/README.md`](qnx/README.md) |
| User app build system | [`user_apps/README.md`](user_apps/README.md) |
| Tutorials (step-by-step) | [`user_apps/tutorials/README.md`](user_apps/tutorials/README.md) |
| ARXML generator guide | [`tools/arxml_generator/README.md`](tools/arxml_generator/README.md) |
| Host DoIP/UDS tester | [`tools/host_tools/doip_diag_tester/README.md`](tools/host_tools/doip_diag_tester/README.md) |
| API reference (online) | <https://tatsuyai713.github.io/Adaptive-AUTOSAR/> |
| API reference (local) | `./scripts/generate_doxygen_docs.sh` |
| Contributing | [`CONTRIBUTING.md`](CONTRIBUTING.md) |

## CI/CD

Workflow: [`.github/workflows/cmake.yml`](.github/workflows/cmake.yml)

The CI pipeline verifies: middleware build/install → full build with tests → `ctest` → ARXML generation → split install workflow → external user app build/run → Doxygen generation → GitHub Pages publish.
