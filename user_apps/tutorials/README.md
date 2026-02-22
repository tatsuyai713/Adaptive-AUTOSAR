# user_apps Tutorials

This directory contains step-by-step guides for each `user_apps` feature.
All tutorials assume the runtime is installed at `/opt/autosar_ap`.

## Common Build Command

```bash
./scripts/build_user_apps_from_opt.sh --prefix /opt/autosar_ap
```

Built executables are placed under `build-user-apps-opt/src/apps/...`.

## Transport Selection Policy (Manifest-first)

For new app design, use manifest-profile switching as the primary approach:

- Keep one app binary and switch backend with `ARA_COM_BINDING_MANIFEST`.
- Use `user_apps/src/apps/communication/switchable_pubsub/README.md` as the canonical workflow.
- Use tutorials `04`/`05`/`06` in this directory as backend-specific reference templates.

## Table of Contents

0. **Complete Pub/Sub Tutorial** ← **Start here**
   [`00_pubsub_e2e_tutorial.md`](00_pubsub_e2e_tutorial.md)
   Full walkthrough: YAML → ARXML → C++ codegen → build → run
1. Manifest-profile backend switching (**recommended**)
   [`11_manifest_profile_switching_e2e.md`](11_manifest_profile_switching_e2e.md)

2. Runtime initialization and shutdown
   [`01_runtime_lifecycle.ja.md`](01_runtime_lifecycle.ja.md)
3. Execution control signal handling
   [`02_exec_signal.ja.md`](02_exec_signal.ja.md)
4. Persistency and PHM
   [`03_per_phm.ja.md`](03_per_phm.ja.md)
5. SOME/IP Pub/Sub (transport-specific template)
   [`04_someip_pubsub.ja.md`](04_someip_pubsub.ja.md)
6. ZeroCopy Pub/Sub (transport-specific template)
   [`05_zerocopy_pubsub.ja.md`](05_zerocopy_pubsub.ja.md)
7. DDS Pub/Sub (transport-specific template)
   [`06_dds_pubsub.ja.md`](06_dds_pubsub.ja.md)
8. SocketCAN receive and decode
   [`07_socketcan_decode.ja.md`](07_socketcan_decode.ja.md)
9. ECU full-stack integration
   [`08_ecu_full_stack.ja.md`](08_ecu_full_stack.ja.md)
10. Raspberry Pi ECU deployment
   [`09_rpi_ecu_deployment.ja.md`](09_rpi_ecu_deployment.ja.md)
11. Porting Vector/ETAS/EB assets
    [`10_vendor_autosar_asset_porting.ja.md`](10_vendor_autosar_asset_porting.ja.md)
12. DoIP/DIAG tester (Ubuntu host) — moved to host_tools
    [`../../tools/host_tools/doip_diag_tester/README.ja.md`](../../tools/host_tools/doip_diag_tester/README.ja.md)

## Related Tools

| Tool | Description |
| ---- | ----------- |
| [`tools/arxml_generator/pubsub_designer_gui.py`](../../tools/arxml_generator/pubsub_designer_gui.py) | **Pub/Sub Service Designer GUI** — design a service via form, generate YAML/ARXML/C++ in one click |
| [`tools/arxml_generator/arxml_generator_gui.py`](../../tools/arxml_generator/arxml_generator_gui.py) | YAML → ARXML conversion GUI (legacy tool) |
| [`tools/arxml_generator/generate_arxml.py`](../../tools/arxml_generator/generate_arxml.py) | YAML → ARXML CLI generator |
| [`tools/arxml_generator/generate_ara_com_binding.py`](../../tools/arxml_generator/generate_ara_com_binding.py) | ARXML → C++ binding constants header |
| [`tools/arxml_generator/examples/my_first_service.yaml`](../../tools/arxml_generator/examples/my_first_service.yaml) | Beginner-friendly YAML template |
