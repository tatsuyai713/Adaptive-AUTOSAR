# Tutorials

Step-by-step guides for developing applications with the Adaptive-AUTOSAR runtime.
All tutorial source files are in Japanese (`.ja.md`).

Language: [日本語](README.ja.md)

---

## Prerequisites

Before starting the tutorials, complete the Quick Start in the [root README](../../README.md):

1. Install dependencies and middleware (`scripts/install_middleware_stack.sh`)
2. Build and install the AUTOSAR AP runtime (`scripts/build_and_install_autosar_ap.sh`)
3. Build user apps against the installed runtime (`scripts/build_user_apps_from_opt.sh`)

---

## Tutorial Index

| # | Topic | Source app template | File |
| --- | --- | --- | --- |
| 01 | Runtime init/deinit lifecycle | `autosar_user_minimal_runtime` | [01_runtime_lifecycle.ja.md](01_runtime_lifecycle.ja.md) |
| 02 | Execution management and signal handling | `autosar_user_exec_signal_template` | [02_exec_signal.ja.md](02_exec_signal.ja.md) |
| 03 | Persistency (KVS/file) and health monitoring (PHM) | `autosar_user_per_phm_demo` | [03_per_phm.ja.md](03_per_phm.ja.md) |
| 04 | SOME/IP pub/sub communication | `autosar_user_com_someip_provider_template` / `_consumer_template` | [04_someip_pubsub.ja.md](04_someip_pubsub.ja.md) |
| 05 | Zero-copy IPC with iceoryx | `autosar_user_com_zerocopy_pub_template` / `_sub_template` | [05_zerocopy_pubsub.ja.md](05_zerocopy_pubsub.ja.md) |
| 06 | DDS pub/sub with CycloneDDS | `autosar_user_com_dds_pub_template` / `_sub_template` | [06_dds_pubsub.ja.md](06_dds_pubsub.ja.md) |
| 07 | SocketCAN message receive and decode | `autosar_user_tpl_can_socketcan_receiver` | [07_socketcan_decode.ja.md](07_socketcan_decode.ja.md) |
| 08 | Full ECU stack implementation | `autosar_user_tpl_ecu_full_stack` | [08_ecu_full_stack.ja.md](08_ecu_full_stack.ja.md) |
| 09 | Raspberry Pi ECU deployment (systemd) | — | [09_rpi_ecu_deployment.ja.md](09_rpi_ecu_deployment.ja.md) |
| 10 | Porting Vector/ETAS/EB app assets | — | [10_vendor_autosar_asset_porting.ja.md](10_vendor_autosar_asset_porting.ja.md) |

---

## Learning Path

### Beginner — Core runtime

1. [01 Runtime lifecycle](01_runtime_lifecycle.ja.md) — Start here. Learn how to initialize and shut down the AUTOSAR AP runtime.
2. [02 Execution and signals](02_exec_signal.ja.md) — Add signal handling and execution-state reporting.
3. [03 Persistency and PHM](03_per_phm.ja.md) — Store persistent data and report health checkpoints.

### Intermediate — Communication

4. [04 SOME/IP pub/sub](04_someip_pubsub.ja.md) — Publish and subscribe to events over SOME/IP using vSomeIP.
5. [05 Zero-copy IPC](05_zerocopy_pubsub.ja.md) — High-throughput shared-memory transport with iceoryx.
6. [06 DDS pub/sub](06_dds_pubsub.ja.md) — Pub/sub over DDS with CycloneDDS.

### Advanced — Full ECU

7. [07 SocketCAN](07_socketcan_decode.ja.md) — Receive and decode CAN frames from a SocketCAN interface.
8. [08 Full ECU stack](08_ecu_full_stack.ja.md) — Combine all features into a complete ECU application.
9. [09 Raspberry Pi deployment](09_rpi_ecu_deployment.ja.md) — Deploy and manage the ECU with systemd on a Raspberry Pi.
10. [10 Vendor asset porting](10_vendor_autosar_asset_porting.ja.md) — Recompile Vector/ETAS/EB-developed C++ assets against this runtime.

---

## App Templates

All tutorial apps are available as pre-built binaries after running `build_user_apps_from_opt.sh`.
Source code lives under `user_apps/src/apps/`.

| Category | Binary name | Source directory |
| --- | --- | --- |
| Basic | `autosar_user_minimal_runtime` | `src/apps/basic/` |
| Basic | `autosar_user_exec_signal_template` | `src/apps/basic/` |
| Basic | `autosar_user_per_phm_demo` | `src/apps/basic/` |
| Communication | `autosar_user_com_someip_provider_template` | `src/apps/communication/someip/` |
| Communication | `autosar_user_com_someip_consumer_template` | `src/apps/communication/someip/` |
| Communication | `autosar_user_com_zerocopy_pub_template` | `src/apps/communication/zerocopy/` |
| Communication | `autosar_user_com_zerocopy_sub_template` | `src/apps/communication/zerocopy/` |
| Communication | `autosar_user_com_dds_pub_template` | `src/apps/communication/dds/` |
| Communication | `autosar_user_com_dds_sub_template` | `src/apps/communication/dds/` |
| Feature | `autosar_user_tpl_runtime_lifecycle` | `src/apps/feature/runtime/` |
| Feature | `autosar_user_tpl_can_socketcan_receiver` | `src/apps/feature/can/` |
| Feature | `autosar_user_tpl_ecu_full_stack` | `src/apps/feature/ecu/` |
| Feature | `autosar_user_tpl_ecu_someip_source` | `src/apps/feature/ecu/` |

---

## Related Documentation

- [user_apps/README.md](../README.md) — User app build system, CMake options, how to add your own app
- [deployment/rpi_ecu/README.md](../../deployment/rpi_ecu/README.md) — Raspberry Pi ECU deployment details
- [tools/host_tools/doip_diag_tester/README.md](../../tools/host_tools/doip_diag_tester/README.md) — Host-side DoIP/UDS diagnostic tester (for use with tutorial 08/09)
- [docs/README.md](../../docs/README.md) — Full project documentation index
