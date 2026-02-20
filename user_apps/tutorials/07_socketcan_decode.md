# 07: SocketCAN Receive and Decode

## Target

- `autosar_user_tpl_can_socketcan_receiver`
- Source: `user_apps/src/apps/feature/can/socketcan_receive_decode_app.cpp`

## Purpose

- Receive Linux SocketCAN frames and decode them into a vehicle status.
- Understand how to switch between `socketcan` and `mock` backends.

## Run

Mock backend:

```bash
./build-user-apps-opt/src/apps/feature/can/autosar_user_tpl_can_socketcan_receiver \
  --can-backend=mock \
  --recv-timeout-ms=20
```

SocketCAN backend:

```bash
./build-user-apps-opt/src/apps/feature/can/autosar_user_tpl_can_socketcan_receiver \
  --can-backend=socketcan \
  --ifname=can0 \
  --powertrain-can-id=0x100 \
  --chassis-can-id=0x101
```

## Customization Points

1. Update CAN ID and signal mappings to match your ECU specification.
2. Adjust `RequireBothFramesBeforeDecode` to match your requirements.
3. Connect decode results to DDS/SOMEIP transmission processing.
