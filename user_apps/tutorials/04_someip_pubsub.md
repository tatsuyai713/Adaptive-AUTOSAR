# 04: SOME/IP Pub/Sub

## Target

- `autosar_user_com_someip_provider_template`
- `autosar_user_com_someip_consumer_template`
- Source:
  - `user_apps/src/apps/communication/someip/provider_app.cpp`
  - `user_apps/src/apps/communication/someip/consumer_app.cpp`

## Purpose

- Implement Provider and Consumer separately using the `ara::com` API.
- Understand the basic flow of Offer / Find / Subscribe / Receive.

## Prerequisites

```bash
export AUTOSAR_AP_PREFIX=/opt/autosar_ap
export VSOMEIP_CONFIGURATION=${AUTOSAR_AP_PREFIX}/configuration/vsomeip-pubsub-sample.json
export LD_LIBRARY_PATH=${AUTOSAR_AP_PREFIX}/lib:${AUTOSAR_AP_PREFIX}/lib64:/opt/vsomeip/lib:${LD_LIBRARY_PATH:-}
```

## Run

Terminal 1:

```bash
./build-user-apps-opt/src/apps/communication/someip/autosar_user_com_someip_provider_template
```

Terminal 2:

```bash
./build-user-apps-opt/src/apps/communication/someip/autosar_user_com_someip_consumer_template
```

## Customization Points

1. Change the data type in `user_apps/src/apps/communication/someip/types.h` for your application.
2. Connect the receive handler to your business logic.
3. Add recovery logic when `SubscriptionState` changes.
