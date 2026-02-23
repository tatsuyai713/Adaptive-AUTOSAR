# User App Entry Points

This directory contains executable application entry points grouped by function.

## Directory Layout

- `basic/`
  - runtime lifecycle basics
  - signal handling basics
  - PER + PHM basics
- `communication/`
  - `someip/`
  - `zerocopy/`
  - `dds/`
- `feature/`
  - `runtime/`: lifecycle-focused feature app
    - includes an `ara::exec` state-reporting + watchdog monitoring sample
  - `can/`: SocketCAN receive/decode feature app
  - `ecu/`: full ECU integration feature app

## Notes

- Reusable support modules are under `user_apps/src/features/`.
- All app entry files are fully commented for onboarding and reuse.
- Host-side DoIP/DIAG tester is separated under `tools/host_tools/doip_diag_tester/`.
