# Host DoIP/DIAG Tester (Ubuntu/Linux)

This tool is a host-side diagnostic tester for validating a Raspberry Pi ECU.
It is intentionally separated from `user_apps` because it is not an ECU on-target app.

## Source

- `tools/host_tools/doip_diag_tester/doip_diag_tester_app.cpp`

## Build

```bash
cmake -S tools/host_tools/doip_diag_tester -B build-host-doip-tester
cmake --build build-host-doip-tester -j"$(nproc)"
```

## Binary

- `build-host-doip-tester/autosar_host_doip_diag_tester`

## Run Example

```bash
./build-host-doip-tester/autosar_host_doip_diag_tester \
  --mode=diag-read-did \
  --host=192.168.10.20 \
  --did=0xF50D
```

For full usage and scenarios, see `tools/host_tools/doip_diag_tester/README.ja.md`.
