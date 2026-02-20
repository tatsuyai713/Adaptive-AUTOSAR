# 05: ZeroCopy Pub/Sub

## Target

- `autosar_user_com_zerocopy_pub_template`
- `autosar_user_com_zerocopy_sub_template`
- Source:
  - `user_apps/src/apps/communication/zerocopy/pub_app.cpp`
  - `user_apps/src/apps/communication/zerocopy/sub_app.cpp`

## Purpose

- Use shared-memory-based communication with the `ara::com::zerocopy` API.
- Understand the basics of Loan / Publish and TryTake.

## Prerequisites

```bash
export AUTOSAR_AP_PREFIX=/opt/autosar_ap
export LD_LIBRARY_PATH=${AUTOSAR_AP_PREFIX}/lib:${AUTOSAR_AP_PREFIX}/lib64:/opt/iceoryx/lib:${LD_LIBRARY_PATH:-}
```

Start RouDi if not already running.

## Run

Terminal 1:

```bash
./build-user-apps-opt/src/apps/communication/zerocopy/autosar_user_com_zerocopy_pub_template
```

Terminal 2:

```bash
./build-user-apps-opt/src/apps/communication/zerocopy/autosar_user_com_zerocopy_sub_template
```

## Customization Points

1. Adjust the channel identifiers (service/instance/event) to match your application values.
2. Calculate the loan size based on actual data size rather than a fixed length.
3. Add behavior for receive timeouts (retry, monitoring counter, etc.).
