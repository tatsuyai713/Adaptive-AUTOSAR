# User App Feature Modules

This directory contains reusable implementation modules shared by advanced apps.

## Directory Layout

- `communication/vehicle_status/`
  - shared SOME/IP-oriented service types/proxy/skeleton wrappers
- `communication/pubsub/`
  - transport-neutral ara::com pub/sub helper layer
  - common payload serialization/deserialization
  - DDS IDL for feature-level pub/sub support
- `communication/can/`
  - SocketCAN receiver
  - mock CAN receiver
  - vehicle-status CAN decoder
- `ecu/`
  - ECU utility helpers (argument parsing, PHM/PER wrappers, backend factories)

## Notes

- These modules are not standalone executables.
- App entry points that use them are under `user_apps/src/apps/feature/`.
