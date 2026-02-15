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
These scripts are based on the installation flow used in:
`https://github.com/tatsuyai713/lwrcl/tree/main/scripts`

```bash
sudo ./scripts/install_dependemcy.sh
sudo ./scripts/install_middleware_stack.sh
```

Or run everything from one command:

```bash
sudo ./scripts/install_middleware_stack.sh --install-base-deps
```

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
  --can-backend mock
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
- `autosar-watchdog.service` (resident watchdog supervisor daemon)

### Porting Vector/ETAS/EB-oriented app assets
This repository can be used to rebuild vendor-developed C++ app assets at source
level and validate communication with other units.

Porting tutorial:
- `user_apps/tutorials/10_vendor_autosar_asset_porting.ja.md`

## Implemented Feature Matrix (Against AUTOSAR AP Scope)
Status meanings:
- `Implemented (subset)`: Implemented in this repo, but not full AUTOSAR feature-complete.
- `Not implemented`: Not provided in this repo currently.

| AUTOSAR AP area | Status | Available in this repo | Missing / Notes |
| --- | --- | --- | --- |
| `ara::core` | Implemented (subset) | `Result`, `Optional`, `Future/Promise`, `ErrorCode/ErrorDomain`, `InstanceSpecifier`, runtime init/deinit, initialization-state query API | Full standard API surface is not complete |
| `ara::log` | Implemented (subset) | Logging framework, logger, sink abstraction (console/file), runtime log-level override/query API | Limited configuration/features compared to commercial stacks |
| `ara::com` common API | Implemented (subset) | Event/Method/Field wrappers, Proxy/Skeleton base, Subscribe/Unsubscribe, receive/state handlers, sample pointer abstractions, concurrent `StartFindService`/handle-aware `StopFindService`, capability-aware Field behavior | Generated API coverage and SWS corner cases are partial |
| `ara::com` SOME/IP transport | Implemented (subset) | SOME/IP SD, pub/sub, RPC paths over vSomeIP backend | Not all SOME/IP/AP options and edge behavior are covered |
| `ara::com` DDS transport | Implemented (subset) | DDS pub/sub via Cyclone DDS wrappers (`ara::com::dds`) | DDS QoS/profile coverage is partial |
| `ara::com` zero-copy transport | Implemented (subset) | Zero-copy pub/sub via iceoryx wrappers (`ara::com::zerocopy`) | Zero-copy is backend-mapped implementation, not full AUTOSAR transport standardization scope |
| `ara::com` E2E | Implemented (subset) | E2E Profile11 and event decorators | Other E2E profiles are not implemented |
| `ara::exec` | Implemented (subset) | Execution/state client-server helpers, signal handler, worker thread utilities, execution-state change callback API | Full EM/Process orchestration behaviors are partial |
| `ara::diag` | Implemented (subset) | UDS/DoIP-oriented components, routing and debouncing helpers, monitor FDC-threshold action support | Not full Diagnostic Manager feature set |
| `ara::phm` | Implemented (subset) | Health channel, supervision primitives | Full PHM integration scope is partial |
| `ara::per` | Implemented (subset) | Key-value/file storage abstractions and APIs | Production-grade persistence policies are partial |
| `ara::sm` | Implemented (subset) | State/trigger utility abstractions | Not full AUTOSAR SM functional cluster |
| ARXML tooling | Implemented (subset) | YAML -> ARXML, ARXML -> ara::com binding header generator | Supports repository scope and extensions, not full ARXML universe |
| `ara::crypto` | Implemented (subset) | Error domain, SHA-256 digest API, random-byte API | Minimal primitives only (no key management/PKI stack yet) |
| `ara::iam` | Implemented (subset) | In-memory IAM policy engine (subject/resource/action, wildcard rules), error domain | Policy persistence and platform IAM integration are not implemented |
| `ara::ucm` | Implemented (subset) | UCM error domain and update-session manager (`Prepare/Stage/Verify/Activate/Rollback/Cancel`), SHA-256 payload verification, state/progress callbacks, per-cluster active-version tracking with downgrade rejection | Simplified software update model (no installer daemon, campaign management, secure boot integration) |
| Time sync (`ara::tsync`) | Implemented (subset) | Time sync client with reference update, synchronized time conversion, offset/state APIs, error domain | No network time protocol daemon integration yet |
| Raspberry Pi ECU deployment profile | Implemented (subset) | Build/install wrapper, SocketCAN setup script, systemd deployment templates, integrated readiness/transport verification script, resident daemons (`vsomeip-routing`, `time-sync`, `persistency-guard`, `iam-policy`, `can-manager`, `watchdog`) | Prototype ECU operation on Linux is covered; production safety/security hardening remains system-level integration work |

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
