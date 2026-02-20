# 09: Raspberry Pi ECU Deployment

## Target

- Deploy a Raspberry Pi Linux machine as a prototype ECU
- Applications:
  - `autosar_user_tpl_ecu_full_stack`
  - `autosar_user_tpl_ecu_someip_source` (for SOME/IP input verification)

## Purpose

- Install runtime and user apps to `/opt/autosar_ap`
- Run ECU applications persistently with `systemd`
- Verify CAN / SOME/IP / DDS / ZeroCopy together

## 1) Build and Install

```bash
sudo ./scripts/build_and_install_rpi_ecu_profile.sh \
  --prefix /opt/autosar_ap \
  --runtime-build-dir build-rpi-autosar-ap \
  --user-app-build-dir /opt/autosar_ap/user_apps_build
```

## 2) Set Up CAN Interface

Physical CAN:

```bash
sudo ./scripts/setup_socketcan_interface.sh --ifname can0 --bitrate 500000
```

Virtual CAN (for testing without hardware):

```bash
sudo ./scripts/setup_socketcan_interface.sh --ifname vcan0 --vcan
```

## 3) Install systemd Services

```bash
sudo ./scripts/install_rpi_ecu_services.sh \
  --prefix /opt/autosar_ap \
  --user-app-build-dir /opt/autosar_ap/user_apps_build \
  --enable
```

Configuration file:

- `/etc/default/autosar-ecu-full-stack`

Edit as needed:

- `AUTOSAR_ECU_CAN_BACKEND` (`socketcan` or `mock`)
- `AUTOSAR_ECU_CAN_IF` (e.g., `can0`)
- `AUTOSAR_ECU_ENABLE_SOMEIP`
- `AUTOSAR_ECU_DDS_DOMAIN_ID`
- `AUTOSAR_ECU_DDS_TOPIC`

## 4) Start Services

```bash
sudo systemctl start autosar-iox-roudi.service
sudo systemctl start autosar-ecu-full-stack.service
sudo systemctl status autosar-ecu-full-stack.service --no-pager
```

## 5) Verify

Full integration check with mock CAN:

```bash
./scripts/verify_rpi_ecu_profile.sh \
  --prefix /opt/autosar_ap \
  --user-app-build-dir /opt/autosar_ap/user_apps_build \
  --can-backend mock \
  --require-platform-binary
```

Full integration check with SocketCAN:

```bash
./scripts/verify_rpi_ecu_profile.sh \
  --prefix /opt/autosar_ap \
  --user-app-build-dir /opt/autosar_ap/user_apps_build \
  --can-backend socketcan \
  --can-if can0 \
  --require-platform-binary
```

## 6) What is Being Verified

- SOME/IP: provider/consumer template communication + routing manager startup check
- DDS: pub/sub template communication
- ZeroCopy: iceoryx RouDi + pub/sub template communication
- ECU:
  - CAN receive (mock/socketcan) → DDS send
  - SOME/IP receive → DDS send

## Notes

- Safety and security requirements for production ECUs (ASIL/SOTIF, Secure Boot, key management, update campaign management, etc.) require additional system integration work.
