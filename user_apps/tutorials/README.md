# user_apps Tutorials

This directory contains step-by-step guides for each `user_apps` feature.
All tutorials assume the runtime is installed at `/opt/autosar-ap`.

## Common Build Command

```bash
./scripts/build_user_apps_from_opt.sh --prefix /opt/autosar-ap
```

Built executables are placed under `build-user-apps-opt/src/apps/...`.

## Transport Selection Policy (Manifest-first)

For new app design, use manifest-profile switching as the primary approach:

- Keep one app binary and switch backend with `ARA_COM_BINDING_MANIFEST`.
- Use `user_apps/src/apps/communication/switchable_pubsub/README.md` as the canonical workflow.
- Use tutorials `10`/`11`/`12` in this directory as backend-specific reference templates.

## Table of Contents

### Fundamentals (00–04)

| # | Title | File |
|---|-------|------|
| 00 | **Complete Pub/Sub Tutorial** ← **Start here** | [`00_pubsub_e2e_tutorial.md`](00_pubsub_e2e_tutorial.md) |
| 01 | Runtime initialization and shutdown | [`01_runtime_lifecycle.md`](01_runtime_lifecycle.md) |
| 02 | Execution control signal handling | [`02_exec_signal.md`](02_exec_signal.md) |
| 03 | Persistency and PHM | [`03_per_phm.md`](03_per_phm.md) |
| 04 | `ara::exec` runtime reporting and monitoring | [`04_exec_runtime_monitoring.md`](04_exec_runtime_monitoring.md) |

### Communication (10–13)

| # | Title | File |
|---|-------|------|
| 10 | SOME/IP Pub/Sub (transport-focused template) | [`10_someip_pubsub.md`](10_someip_pubsub.md) |
| 11 | ZeroCopy Pub/Sub (transport-focused template) | [`11_zerocopy_pubsub.md`](11_zerocopy_pubsub.md) |
| 12 | DDS Pub/Sub (transport-focused template) | [`12_dds_pubsub.md`](12_dds_pubsub.md) |
| 13 | Manifest-profile backend switching (**recommended**) | [`13_manifest_profile_switching_e2e.md`](13_manifest_profile_switching_e2e.md) |

### Vehicle Integration (20–22)

| # | Title | File |
|---|-------|------|
| 20 | SocketCAN receive and decode | [`20_socketcan_decode.md`](20_socketcan_decode.md) |
| 21 | ECU full-stack integration | [`21_ecu_full_stack.md`](21_ecu_full_stack.md) |
| 22 | Production ECU application | [`22_production_ecu_app.md`](22_production_ecu_app.md) |

### Platform Services (30–33)

| # | Title | File |
|---|-------|------|
| 30 | Platform service architecture (17 daemons, 20 services) | [`30_platform_service_architecture.md`](30_platform_service_architecture.md) |
| 31 | Diagnostic server (UDS/DoIP) | [`31_diag_server.md`](31_diag_server.md) |
| 32 | Platform Health Management daemon | [`32_phm_daemon.md`](32_phm_daemon.md) |
| 33 | Crypto provider (HSM, key rotation) | [`33_crypto_provider.md`](33_crypto_provider.md) |

### Execution Management (40)

| # | Title | File |
|---|-------|------|
| 40 | ExecutionManager orchestration for user-app management | [`40_execution_manager_orchestration.md`](40_execution_manager_orchestration.md) |

### Deployment (50–52)

| # | Title | File |
|---|-------|------|
| 50 | Raspberry Pi ECU deployment (full 20-service profile) | [`50_rpi_ecu_deployment.md`](50_rpi_ecu_deployment.md) |
| 51 | RPi minimal ECU — library build to user app management | [`51_rpi_minimal_ecu_user_app_management.md`](51_rpi_minimal_ecu_user_app_management.md) |
| 52 | Porting Vector/ETAS/EB assets | [`52_vendor_autosar_asset_porting.md`](52_vendor_autosar_asset_porting.md) |

### Host Tools

| Tool | Description |
|------|-------------|
| DoIP/DIAG tester (Ubuntu host) | [`../../tools/host_tools/doip_diag_tester/README.md`](../../tools/host_tools/doip_diag_tester/README.md) |

## Related Tools

| Tool | Description |
| ---- | ----------- |
| [`tools/arxml_generator/pubsub_designer_gui.py`](../../tools/arxml_generator/pubsub_designer_gui.py) | **Pub/Sub Service Designer GUI** — design a service via form, generate YAML/ARXML/C++ in one click |
| [`tools/arxml_generator/arxml_generator_gui.py`](../../tools/arxml_generator/arxml_generator_gui.py) | YAML → ARXML conversion GUI (legacy tool) |
| [`tools/arxml_generator/generate_arxml.py`](../../tools/arxml_generator/generate_arxml.py) | YAML → ARXML CLI generator |
| [`tools/arxml_generator/generate_ara_com_binding.py`](../../tools/arxml_generator/generate_ara_com_binding.py) | ARXML → C++ binding constants header |
| [`tools/arxml_generator/examples/my_first_service.yaml`](../../tools/arxml_generator/examples/my_first_service.yaml) | Beginner-friendly YAML template |
