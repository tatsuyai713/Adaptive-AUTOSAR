# 31: Diagnostic Server (UDS/DoIP)

## Target

`autosar_diag_server` — source: `src/main_diag_server.cpp`

## Purpose

The diagnostic server provides a dedicated **UDS over DoIP** endpoint,
listening on a configurable TCP port and processing diagnostic requests
through `ara::diag::DiagnosticManager`.

It is deployed as a stand-alone systemd service (`autosar-diag-server.service`)
so that the diagnostic interface remains available even during user-app restarts.

## Supported UDS Services

| SID | Service | Description |
|-----|---------|-------------|
| 0x10 | DiagnosticSessionControl | Session management |
| 0x11 | ECUReset | Hard/soft reset |
| 0x14 | ClearDTC | Clear diagnostic trouble codes |
| 0x19 | ReadDTC | Read DTC information |
| 0x22 | ReadDataByID | Data identifier read |
| 0x27 | SecurityAccess | Seed/key authentication |
| 0x2E | WriteDataByID | Data identifier write |
| 0x2F | IOControlByID | Input/output control |
| 0x31 | RoutineControl | Start/stop/query routines |
| 0x34 | RequestDownload | Download session init |
| 0x35 | RequestUpload | Upload session init |
| 0x36 | TransferData | Block transfer |
| 0x37 | RequestTransferExit | Transfer complete |
| 0x3E | TesterPresent | Keep-alive |
| 0x85 | ControlDTCSetting | Enable/disable DTC recording |

## Architecture

```
  DoIP Tester (Ubuntu)              Raspberry Pi ECU
  ┌──────────────┐           ┌──────────────────────┐
  │ doip_diag_   │  TCP:13400│  autosar_diag_server │
  │ tester.py    │ ─────────►│  ┌────────────────┐  │
  │              │           │  │DiagnosticManager│  │
  │              │◄───────── │  │ (15 UDS SIDs)  │  │
  └──────────────┘           │  └────────────────┘  │
                             │  Status → /run/autosar│
                             │  /diag_server.status  │
                             └──────────────────────┘
```

## Environment Variables

| Variable | Default | Description |
|----------|---------|-------------|
| `AUTOSAR_DIAG_LISTEN_ADDR` | `0.0.0.0` | Bind address |
| `AUTOSAR_DIAG_LISTEN_PORT` | `13400` | DoIP TCP port |
| `AUTOSAR_DIAG_P2_SERVER_MS` | `50` | P2 response timing (ms) |
| `AUTOSAR_DIAG_P2STAR_SERVER_MS` | `5000` | P2* extended timing (ms) |
| `AUTOSAR_DIAG_STATUS_PERIOD_MS` | `2000` | Status write interval (ms) |
| `AUTOSAR_DIAG_STATUS_FILE` | `/run/autosar/diag_server.status` | Status output path |

## Run (Standalone)

```bash
# Build the runtime (if not already built)
cd /home/user/Adaptive-AUTOSAR
cmake -B build-local-check -DCMAKE_INSTALL_PREFIX=/opt/autosar-ap .
cmake --build build-local-check -j$(nproc)

# Run directly
AUTOSAR_DIAG_LISTEN_PORT=13400 \
  ./build-local-check/bin/autosar_diag_server
```

## Status File

The daemon writes its status to `/run/autosar/diag_server.status`:

```
listening=1
total_requests=42
total_connections=3
active_connections=1
positive_responses=38
negative_responses=4
updated_epoch_ms=1717171717000
```

Monitor live:

```bash
watch -n1 cat /run/autosar/diag_server.status
```

## systemd Service

```bash
# Enable and start
sudo systemctl enable --now autosar-diag-server.service

# Check status
sudo systemctl status autosar-diag-server.service --no-pager

# Edit configuration
sudo nano /etc/default/autosar-diag-server
sudo systemctl restart autosar-diag-server.service
```

Service ordering:
- **After**: `network-online.target`, `autosar-platform-app.service`
- **Before**: `autosar-exec-manager.service`
- **Restart**: `on-failure` (delay 2 s)

## Testing with DoIP Tester

Use the host-side DoIP tester tool:

```bash
# On Ubuntu host
cd tools/host_tools/doip_diag_tester
python3 doip_diag_tester.py --target <rpi-ip> --port 13400 --sid 0x22 --did 0xF190
```

See `tools/host_tools/doip_diag_tester/README.md` for full usage.

## Next Steps

| Tutorial | Topic |
|----------|-------|
| [30_platform_service_architecture](30_platform_service_architecture.md) | Full platform overview |
| [32_phm_daemon](32_phm_daemon.md) | Health management daemon |
| [50_rpi_ecu_deployment](50_rpi_ecu_deployment.md) | Deploy to RPi |
