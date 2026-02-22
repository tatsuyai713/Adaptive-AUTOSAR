# 06: DDS Pub/Sub

## Target

- `autosar_user_com_dds_pub_template`
- `autosar_user_com_dds_sub_template`
- Source:
  - `user_apps/src/apps/communication/dds/pub_app.cpp`
  - `user_apps/src/apps/communication/dds/sub_app.cpp`
- IDL:
  - `user_apps/idl/UserAppsStatus.idl`

## Purpose

- Understand how to use the DDS backend from the `ara::com` side.
- Understand type generation from IDL (`idlc`) and application integration.

## Positioning (Manifest-first roadmap)

- This tutorial is a transport-specific DDS template.
- It validates the DDS path directly (`ara::com::dds` + generated IDL types), not manifest-profile switching.
- For same-binary backend switching by manifest profile, follow `user_apps/src/apps/communication/switchable_pubsub/README.md`.

## Prerequisites

```bash
export AUTOSAR_AP_PREFIX=/opt/autosar_ap
export LD_LIBRARY_PATH=${AUTOSAR_AP_PREFIX}/lib:${AUTOSAR_AP_PREFIX}/lib64:/opt/cyclonedds/lib:${LD_LIBRARY_PATH:-}
```

## Run

Terminal 1:

```bash
./build-user-apps-opt/src/apps/communication/dds/autosar_user_com_dds_pub_template
```

Terminal 2:

```bash
./build-user-apps-opt/src/apps/communication/dds/autosar_user_com_dds_sub_template
```

## Customization Points

1. Add required fields to `UserAppsStatus.idl`.
2. Update publish/subscribe processing to match the generated types.
3. Fix or configure the Domain ID and Topic name for your environment.
