# Adaptive-AUTOSAR
![CI](https://github.com/tatsuyai713/Adaptive-AUTOSAR/actions/workflows/cmake.yml/badge.svg)

Linux-oriented educational implementation of Adaptive AUTOSAR style APIs.

Language:
- English: `README.md`
- Japanese: `README.ja.md`

## Scope and Version
- ARXML schema baseline in this repository: `http://autosar.org/schema/r4.0` (`autosar_00050.xsd`).
- This repository is an educational implementation of Adaptive AUTOSAR style APIs and behavior subsets.
- This repository does not claim official AUTOSAR conformance certification.

## Quick Start (Build -> Install -> User App Build/Run)
The following commands were validated in Docker on **2026-02-15**.

### Prerequisites
- C++14 compiler
- CMake >= 3.14
- Python3 + `PyYAML`
- OpenSSL development package (`libcrypto`)
- Installed middleware (default paths used by this repo):
  - vSomeIP: `/opt/vsomeip`
  - iceoryx: `/opt/iceoryx`
  - Cyclone DDS (+ idlc): `/opt/cyclonedds`

### 0) Install dependencies and middleware (Linux / Raspberry Pi)
Use the bundled scripts in this repository:

```bash
sudo ./scripts/install_dependency.sh
sudo ./scripts/install_middleware_stack.sh
```

Or run everything from one command:

```bash
sudo ./scripts/install_middleware_stack.sh --install-base-deps
```

### QNX800 cross-build
For QNX SDP 8.0 cross-compilation flow, see:
- `qnx/README.md`
- `qnx/env/qnx800.env.example`

### 1) Build and install AUTOSAR AP runtime libraries
Use `/tmp` first (no root privilege required):

```bash
./scripts/build_and_install_autosar_ap.sh \
  --prefix /tmp/autosar_ap \
  --build-dir build-install-autosar-ap
```

If you want the production-like layout:

```bash
sudo ./scripts/build_and_install_autosar_ap.sh --prefix /opt/autosar_ap
```

If middleware is not installed yet, build script can install it first:

```bash
sudo ./scripts/build_and_install_autosar_ap.sh \
  --prefix /opt/autosar_ap \
  --install-middleware \
  --install-base-deps
```

`build_and_install_autosar_ap.sh` builds the platform runtime binary
(`adaptive_autosar`) by default. Use `--without-platform-app` only when you
intentionally want a library-only install.

### 2) Build user apps against installed runtime only

```bash
./scripts/build_user_apps_from_opt.sh \
  --prefix /tmp/autosar_ap \
  --source-dir /tmp/autosar_ap/user_apps \
  --build-dir build-user-apps-opt
```

### 3) Run default smoke apps via script

```bash
./scripts/build_user_apps_from_opt.sh \
  --prefix /tmp/autosar_ap \
  --source-dir /tmp/autosar_ap/user_apps \
  --build-dir build-user-apps-opt-run \
  --run
```

This executes:
- `autosar_user_minimal_runtime`
- `autosar_user_per_phm_demo`

### 4) Run additional app templates manually (optional)
Example:

```bash
./build-user-apps-opt/src/apps/feature/runtime/autosar_user_tpl_runtime_lifecycle
./build-user-apps-opt/src/apps/feature/can/autosar_user_tpl_can_socketcan_receiver --can-backend=mock
```

### Raspberry Pi ECU profile (Linux + systemd)
The repository includes a deployment profile to run a Raspberry Pi machine as a
prototype ECU.

```bash
sudo ./scripts/build_and_install_rpi_ecu_profile.sh \
  --prefix /opt/autosar_ap \
  --runtime-build-dir build-rpi-autosar-ap \
  --user-app-build-dir /opt/autosar_ap/user_apps_build \
  --install-middleware

sudo ./scripts/setup_socketcan_interface.sh --ifname can0 --bitrate 500000
sudo ./scripts/install_rpi_ecu_services.sh --prefix /opt/autosar_ap --user-app-build-dir /opt/autosar_ap/user_apps_build --enable

./scripts/verify_rpi_ecu_profile.sh \
  --prefix /opt/autosar_ap \
  --user-app-build-dir /opt/autosar_ap/user_apps_build \
  --can-backend mock \
  --require-platform-binary
```

Detailed runbook:
- `deployment/rpi_ecu/README.md`

User-app bringup entry points:
- `/etc/autosar/bringup.sh` (edit this to launch your own applications)
- `autosar-vsomeip-routing.service` (keeps SOME/IP routing manager resident)
- `autosar-time-sync.service` (resident time synchronization support daemon)
- `autosar-persistency-guard.service` (resident persistency sync guard daemon)
- `autosar-iam-policy.service` (resident IAM policy loader daemon)
- `autosar-can-manager.service` (resident SocketCAN interface manager daemon)
- `autosar-platform-app.service` (starts platform-side built-in process stack)
- `autosar-exec-manager.service` (runs `bringup.sh` after platform service)
- `autosar-user-app-monitor.service` (tracks registered user apps, heartbeat freshness, and `ara::phm::HealthChannel` status; triggers restart recovery with startup-grace/backoff/deactivated-stop controls)
- `autosar-watchdog.service` (resident watchdog supervisor daemon)
- `autosar-sm-state.service` (resident SM machine state / network mode management daemon)
- `autosar-ntp-time-provider.service` (resident NTP time synchronization provider daemon — chrony/ntpd auto-detect)
- `autosar-ptp-time-provider.service` (resident PTP/gPTP time synchronization provider daemon — ptp4l PHC integration)
- runtime registry file: `/run/autosar/user_apps_registry.csv`
- runtime monitor status file: `/run/autosar/user_app_monitor.status`
- runtime PHM health files: `/run/autosar/phm/health/*.status`

### Porting Vector/ETAS/EB-oriented app assets
This repository can be used to rebuild vendor-developed C++ app assets at source
level and validate communication with other units.

Porting tutorial:
- `user_apps/tutorials/10_vendor_autosar_asset_porting.ja.md`

## Implemented Feature Matrix (Against AUTOSAR AP Scope)
Status meanings:
- `Implemented (subset)`: Implemented in this repo, but not full AUTOSAR feature-complete.
- `Not implemented`: Not provided in this repo currently.
- **Coverage %** is an approximate ratio of implemented standard APIs vs. the full AUTOSAR AP SWS specification for each functional cluster. It is calculated based on the number of major standard API classes/methods implemented, and is provided as a rough guide only.

Conformance policy in this repository:
- APIs corresponding to AUTOSAR AP standard interfaces are kept in AUTOSAR-style signatures.
- Platform-side and user-app sample code are implemented to use standard namespaces first (`ara::<domain>::*`), and avoid extension aliases by default.
- Non-standard additions are provided as extensions (primarily under `ara::<domain>::extension`) and are not used to replace standard API signatures.
- Extension entry headers:
  - `src/ara/com/extension/non_standard.h`
  - `src/ara/diag/extension/non_standard.h`
  - `src/ara/exec/extension/non_standard.h`
  - `src/ara/phm/extension/non_standard.h`

| AUTOSAR AP area | Coverage | Status | Available in this repo | Missing / Notes |
| --- | :---: | --- | --- | --- |
| `ara::core` | ~50 % | Implemented (subset) | `Result`, `Optional`, `Future/Promise`, `ErrorCode/ErrorDomain`, `InstanceSpecifier`, runtime init/deinit, initialization-state query API | `Variant`, `String`/`StringView`, container wrappers (`Vector`/`Map`/`Array`/`Span`), `SteadyClock`, `CoreErrorDomain`, Exception class hierarchy |
| `ara::log` | ~75 % | Implemented (subset) | `Logger` (all severity levels + `WithLevel` log-level filtering), `LogStream`, `LoggingFramework`, 3 sink types (console/file/network), runtime log-level override/query | DLT backend integration, `LogHex`/`LogBin` formatters |
| `ara::com` common API | ~60 % | Implemented (subset) | `Event`/`Method`/`Field` wrappers, `ServiceProxyBase`/`ServiceSkeletonBase`, Subscribe/Unsubscribe, receive/state handlers, `SamplePtr`, serialization framework, concurrent `StartFindService`/handle-aware `StopFindService`, capability-aware Field behavior | Generated API stubs, communication groups, full SWS corner cases |
| `ara::com` SOME/IP | ~60 % | Implemented (subset) | SOME/IP SD client/server, pub/sub, RPC client/server over vSomeIP backend | Not all SOME/IP/AP options and edge behavior are covered |
| `ara::com` DDS | ~40 % | Implemented (subset) | DDS pub/sub via Cyclone DDS wrappers (`ara::com::dds`) | DDS QoS/profile coverage is partial |
| `ara::com` zero-copy | ~40 % | Implemented (subset) | Zero-copy pub/sub via iceoryx wrappers (`ara::com::zerocopy`) | Backend-mapped implementation, not full AUTOSAR transport standardization scope |
| `ara::com` E2E | ~30 % | Implemented (subset) | E2E Profile11 (`TryProtect`/`Check`/`TryForward`) and event decorators | Other E2E profiles (P01/P02/P04/P05/P07) are not implemented |
| `ara::exec` | ~55 % | Implemented (subset) | `ExecutionClient`/`ExecutionServer`, `StateServer`, `DeterministicClient`, `FunctionGroup`/`FunctionGroupState`, `SignalHandler`, `WorkerThread`, enhanced `ProcessWatchdog` (startup grace / continuous expiry / callback cooldown / expiry count), execution-state change callback | Full EM/Process orchestration, `StateClient` standard interface |
| `ara::diag` | ~70 % | Implemented (subset) | `Monitor` (with debouncing, FDC query, current status query), `Event`, `Conversation`, `DTCInformation`, `Condition`, `OperationCycle`, `GenericUDSService`, `SecurityAccess`, `GenericRoutine`, `DataTransfer` (Upload/Download), routing framework with counter/timer-based debouncing | Full Diagnostic Manager orchestration, all UDS sub-functions |
| `ara::phm` | ~50 % | Implemented (subset) | `SupervisedEntity`, `HealthChannel` (with `Offer`/`StopOffer` lifecycle), `RecoveryAction`, `CheckpointCommunicator`, supervision helpers | `AliveSupervision`/`DeadlineSupervision`/`LogicalSupervision` detailed SWS APIs |
| `ara::per` | ~65 % | Implemented (subset) | `KeyValueStorage`, `FileStorage`, `ReadAccessor` (with `Seek`/`GetCurrentPosition`), `ReadWriteAccessor` (with `Seek`/`GetCurrentPosition`) | `SharedHandle`/`UniqueHandle` standard wrappers, redundancy/recovery policies |
| `ara::sm` | ~55 % | Implemented (subset) | `TriggerIn`/`TriggerOut`/`TriggerInOut`, `Notifier`, `SmErrorDomain`, `MachineStateClient` (lifecycle state management), `NetworkHandle` (communication mode control), `StateTransitionHandler` (function-group transition callbacks), resident SM state daemon | Full EM-coordinated state-manager orchestration, diagnostic state coupling |
| ARXML tooling | — | Implemented (subset) | YAML -> ARXML, ARXML -> ara::com binding header generator | Supports repository scope and extensions, not full ARXML universe |
| `ara::crypto` | ~15 % | Implemented (subset) | Error domain, SHA-256 digest, HMAC, AES encryption, key generation, random-byte API | Key management/PKI stack, `CryptoProvider`, X.509, SecOC |
| `ara::iam` | ~20 % | Implemented (subset) | In-memory IAM policy engine (subject/resource/action, wildcard rules), error domain | Policy persistence, platform IAM integration, grant management |
| `ara::ucm` | ~40 % | Implemented (subset) | UCM error domain, update-session manager (`Prepare`/`Stage`/`Verify`/`Activate`/`Rollback`/`Cancel`), Transfer API (`TransferStart`/`TransferData`/`TransferExit`), SHA-256 payload verification, state/progress callbacks, per-cluster active-version tracking with downgrade rejection | Campaign management, installer daemon, secure boot integration |
| `ara::tsync` | ~50 % | Implemented (subset) | `TimeSyncClient` (reference update, synchronized time conversion, offset/state query, state-change notification callback), `SynchronizedTimeBaseProvider` abstract interface, `PtpTimeBaseProvider` (ptp4l/gPTP PHC integration via `/dev/ptpN`), `NtpTimeBaseProvider` (chrony/ntpd auto-detect integration), error domain, resident PTP and NTP provider daemons | `TimeSyncServer`, rate correction, full TSP SWS profile coverage |
| Raspberry Pi ECU profile | — | Implemented (subset) | Build/install wrapper, SocketCAN setup script, systemd deployment templates, integrated readiness/transport verification script, resident daemons (`vsomeip-routing`, `time-sync`, `persistency-guard`, `iam-policy`, `can-manager`, `user-app-monitor`, `watchdog`, `sm-state`, `ntp-time-provider`, `ptp-time-provider`) | Prototype ECU operation on Linux is covered; production safety/security hardening remains system-level integration work |

## User App Templates (Installed as `/opt|/tmp/autosar_ap/user_apps`)
- Basic:
  - `autosar_user_minimal_runtime`
  - `autosar_user_exec_signal_template`
  - `autosar_user_per_phm_demo`
- Communication:
  - `autosar_user_com_someip_provider_template`
  - `autosar_user_com_someip_consumer_template`
  - `autosar_user_com_zerocopy_pub_template`
  - `autosar_user_com_zerocopy_sub_template`
  - `autosar_user_com_dds_pub_template`
  - `autosar_user_com_dds_sub_template`
- Feature:
  - `autosar_user_tpl_runtime_lifecycle`
  - `autosar_user_tpl_can_socketcan_receiver`
  - `autosar_user_tpl_ecu_full_stack`
  - `autosar_user_tpl_ecu_someip_source`

See also:
- `user_apps/README.md`
- `user_apps/tutorials/README.ja.md`
- `deployment/rpi_ecu/README.md`
- `user_apps/tutorials/10_vendor_autosar_asset_porting.ja.md` (porting Vector/ETAS/EB-oriented app assets)
- `tools/host_tools/doip_diag_tester/README.ja.md` (Ubuntu-side DoIP/DIAG host tester for Raspberry Pi ECU)

## Host-Side Diagnostic Tool (Not ECU On-Target)
- Binary: `autosar_host_doip_diag_tester`
- Source: `tools/host_tools/doip_diag_tester/doip_diag_tester_app.cpp`
- Docs:
  - `tools/host_tools/doip_diag_tester/README.md`
  - `tools/host_tools/doip_diag_tester/README.ja.md`
- Build:
```bash
cmake -S tools/host_tools/doip_diag_tester -B build-host-doip-tester
cmake --build build-host-doip-tester -j"$(nproc)"
```

## ARXML and Code Generation
### YAML -> ARXML

```bash
python3 tools/arxml_generator/generate_arxml.py \
  --input tools/arxml_generator/examples/pubsub_vsomeip.yaml \
  --output /tmp/pubsub.generated.arxml \
  --overwrite \
  --print-summary
```

### ARXML -> ara::com binding header

```bash
python3 tools/arxml_generator/generate_ara_com_binding.py \
  --input /tmp/pubsub.generated.arxml \
  --output /tmp/vehicle_status_manifest_binding.h \
  --namespace sample::vehicle_status::generated \
  --provided-service-short-name VehicleStatusProviderInstance \
  --provided-event-group-short-name VehicleStatusEventGroup
```

Detailed manuals:
- `tools/arxml_generator/README.md`
- `tools/arxml_generator/YAML_MANUAL.ja.md`

## GitHub Actions Coverage
Workflow: `.github/workflows/cmake.yml`

Current CI verifies:
1. Middleware dependency build/install (vSomeIP, iceoryx, Cyclone DDS)
2. Full configure/build (`build_tests=ON`, all backends ON, samples OFF)
3. Unit tests (`ctest`) with required runtime library paths
4. ARXML generation and ARXML-based binding generation checks
5. Split install workflow (`build_and_install_autosar_ap.sh`)
6. External user app build and run against installed package (`build_user_apps_from_opt.sh --run`)
7. Doxygen API documentation generation (`./scripts/generate_doxygen_docs.sh`) and artifact upload (`doxygen-html`)
8. GitHub Pages publish for generated docs on push to `main`/`master`

## Documentation
- GitHub Pages API docs: `https://tatsuyai713.github.io/Adaptive-AUTOSAR/`
- Local/API docs generation: `./scripts/generate_doxygen_docs.sh` -> `docs/index.html` (`docs/api/index.html`)
- Project Wiki: https://github.com/langroodi/Adaptive-AUTOSAR/wiki
- Contributing guide: `CONTRIBUTING.md`
