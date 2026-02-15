# Raspberry Pi ECU Deployment Profile

This profile turns a Raspberry Pi Linux machine into a prototype AUTOSAR-style ECU
using this repository runtime and user apps.

## What This Profile Covers

- Build/install runtime to `/opt/autosar_ap`
- Build `user_apps` against installed runtime only
- Configure SocketCAN on Linux
- Install `systemd` units for:
  - iceoryx RouDi
  - ECU full-stack app (`CAN + SOME/IP` receive, `DDS` publish)
- Verify transport and ECU paths with a single script

## Prerequisites

- Linux on Raspberry Pi (64-bit recommended)
- C++14 toolchain, CMake, Python3 + PyYAML
- Middleware installed under:
  - `/opt/vsomeip`
  - `/opt/iceoryx`
  - `/opt/cyclonedds`
- `systemd` available

## 1) Build and install runtime + user apps

```bash
sudo ./scripts/build_and_install_rpi_ecu_profile.sh \
  --prefix /opt/autosar_ap \
  --runtime-build-dir build-rpi-autosar-ap \
  --user-app-build-dir /opt/autosar_ap/user_apps_build
```

## 2) Configure CAN interface

Physical CAN:

```bash
sudo ./scripts/setup_socketcan_interface.sh --ifname can0 --bitrate 500000
```

Virtual CAN (for bring-up only):

```bash
sudo ./scripts/setup_socketcan_interface.sh --ifname vcan0 --vcan
```

## 3) Install systemd services

```bash
sudo ./scripts/install_rpi_ecu_services.sh \
  --prefix /opt/autosar_ap \
  --user-app-build-dir /opt/autosar_ap/user_apps_build \
  --enable
```

Edit runtime options in:

- `/etc/default/autosar-ecu-full-stack`

## 4) Start services

```bash
sudo systemctl start autosar-iox-roudi.service
sudo systemctl start autosar-ecu-full-stack.service
sudo systemctl status autosar-ecu-full-stack.service --no-pager
```

## 5) Validate readiness and communication paths

```bash
./scripts/verify_rpi_ecu_profile.sh \
  --prefix /opt/autosar_ap \
  --user-app-build-dir /opt/autosar_ap/user_apps_build \
  --can-backend mock
```

For real CAN input:

```bash
./scripts/verify_rpi_ecu_profile.sh \
  --prefix /opt/autosar_ap \
  --user-app-build-dir /opt/autosar_ap/user_apps_build \
  --can-backend socketcan \
  --can-if can0
```

## Operational Notes

- This profile targets Linux-based prototype ECU operation.
- It is not a certified commercial AUTOSAR stack.
- Safety/security hardening (ASIL/SOTIF, secure boot chain, full update campaign,
  production diagnostics, watchdog integration policy) remains product work.
