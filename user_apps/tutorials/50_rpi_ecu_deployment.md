# 50: Raspberry Pi ECU Deployment

## Target

Deploy 17 platform daemons and user applications on a Raspberry Pi as a
prototype Adaptive AUTOSAR ECU, managed by 20 systemd services.

## Purpose

- Install the full AUTOSAR AP runtime to `/opt/autosar-ap`
- Deploy 17 platform daemons as systemd services
- Run user apps (production ECU app, full-stack demo, or custom) persistently
- Verify CAN / SOME/IP / DDS / ZeroCopy together

## Prerequisites

See [30_platform_service_architecture](30_platform_service_architecture.md) for the
full daemon and service inventory.

## 1) Build and Install

```bash
sudo ./scripts/build_and_install_rpi_ecu_profile.sh \
  --prefix /opt/autosar-ap \
  --runtime-build-dir build-rpi-autosar-ap \
  --user-app-build-dir /opt/autosar-ap/user_apps_build \
  --install-middleware
```

This builds and installs:
- 14 static libraries (`libara_*.a`)
- 17 platform daemon binaries
- User app templates
- Deployment configuration files

## 2) Set Up CAN Interface

Physical CAN:

```bash
sudo ./scripts/setup_socketcan_interface.sh --ifname can0 --bitrate 500000
```

Virtual CAN (for testing without hardware):

```bash
sudo ./scripts/setup_socketcan_interface.sh --ifname vcan0 --vcan
```

Skip this step if not using CAN.

## 3) Install systemd Services

```bash
sudo ./scripts/install_rpi_ecu_services.sh \
  --prefix /opt/autosar-ap \
  --user-app-build-dir /opt/autosar-ap/user_apps_build \
  --enable
```

This installs **20 systemd services** with wrapper scripts, env files,
bringup.sh, and IAM policy.

Configuration files in `/etc/default/`:

| File | Key Variables |
|------|---------------|
| `autosar-ecu-full-stack` | `AUTOSAR_ECU_CAN_BACKEND`, `AUTOSAR_ECU_CAN_IF`, `AUTOSAR_ECU_DDS_DOMAIN_ID` |
| `autosar-diag-server` | `AUTOSAR_DIAG_LISTEN_PORT`, `AUTOSAR_DIAG_P2_SERVER_MS` |
| `autosar-phm-daemon` | `AUTOSAR_PHM_PERIOD_MS`, `AUTOSAR_PHM_DEGRADED_THRESHOLD` |
| `autosar-crypto-provider` | `AUTOSAR_CRYPTO_TOKEN`, `AUTOSAR_CRYPTO_KEY_ROTATION` |

## 4) Start Services

### Full Profile (All 20 Services)

```bash
# Tier 0: Communication infrastructure
sudo systemctl start autosar-iox-roudi.service
sudo systemctl start autosar-vsomeip-routing.service

# Tier 1: Platform foundation
sudo systemctl start autosar-time-sync.service
sudo systemctl start autosar-ntp-time-provider.service
sudo systemctl start autosar-ptp-time-provider.service
sudo systemctl start autosar-persistency-guard.service
sudo systemctl start autosar-iam-policy.service
sudo systemctl start autosar-sm-state.service
sudo systemctl start autosar-can-manager.service
sudo systemctl start autosar-dlt.service
sudo systemctl start autosar-network-manager.service
sudo systemctl start autosar-crypto-provider.service

# Tier 2: Platform applications
sudo systemctl start autosar-platform-app.service
sudo systemctl start autosar-diag-server.service
sudo systemctl start autosar-phm-daemon.service
sudo systemctl start autosar-ucm.service

# Tier 3: Execution management & monitoring
sudo systemctl start autosar-exec-manager.service
sudo systemctl start autosar-user-app-monitor.service
sudo systemctl start autosar-watchdog.service

# User app (choose one)
sudo systemctl start autosar-ecu-full-stack.service
```

### Minimal Profile

```bash
sudo systemctl start autosar-iox-roudi.service
sudo systemctl start autosar-vsomeip-routing.service
sudo systemctl start autosar-platform-app.service
sudo systemctl start autosar-exec-manager.service
```

### Check Status

```bash
# All AUTOSAR services at a glance
systemctl list-units 'autosar-*' --no-pager

# Individual service check
sudo systemctl status autosar-diag-server.service --no-pager

# All daemon status files
for f in /run/autosar/*.status; do
  echo "=== $(basename "$f") ==="
  cat "$f"
  echo
done
```

## 5) Verify

Full integration check with mock CAN:

```bash
./scripts/verify_rpi_ecu_profile.sh \
  --prefix /opt/autosar-ap \
  --user-app-build-dir /opt/autosar-ap/user_apps_build \
  --can-backend mock \
  --require-platform-binary
```

Full integration check with SocketCAN:

```bash
./scripts/verify_rpi_ecu_profile.sh \
  --prefix /opt/autosar-ap \
  --user-app-build-dir /opt/autosar-ap/user_apps_build \
  --can-backend socketcan \
  --can-if can0 \
  --require-platform-binary
```

## 6) What is Being Verified

| Layer | Check |
|-------|-------|
| SOME/IP | Provider/consumer template + routing manager |
| DDS | Pub/sub template communication |
| ZeroCopy | iceoryx RouDi + pub/sub template |
| ECU | CAN receive → DDS send |
| ECU | SOME/IP receive → DDS send |
| Diagnostics | `autosar_diag_server` listening on DoIP port |
| PHM | `autosar_phm_daemon` health aggregation |
| Crypto | `autosar_crypto_provider` self-test passes |
| Status Files | All `/run/autosar/*.status` files being updated |

## 7) Service Management

### Disable Unnecessary Services

```bash
sudo systemctl disable --now autosar-can-manager.service    # No CAN
sudo systemctl disable --now autosar-ntp-time-provider.service
sudo systemctl disable --now autosar-ptp-time-provider.service
sudo systemctl disable --now autosar-dlt.service
sudo systemctl disable --now autosar-ucm.service
```

### Update Configuration

```bash
# Edit env file
sudo nano /etc/default/autosar-diag-server

# Restart affected service
sudo systemctl restart autosar-diag-server.service
```

### View Logs

```bash
# Combined platform logs
journalctl -u 'autosar-*' --no-pager -n 50

# Specific service
journalctl -u autosar-phm-daemon.service --no-pager -f
```

## Notes

- For the minimal ECU profile and user app management, see
  [51_rpi_minimal_ecu_user_app_management](51_rpi_minimal_ecu_user_app_management.md).
- For platform service details, see
  [30_platform_service_architecture](30_platform_service_architecture.md).
- Safety and security requirements for production ECUs (ASIL/SOTIF, Secure Boot,
  key management, update campaign management, etc.) require additional
  system integration work.
