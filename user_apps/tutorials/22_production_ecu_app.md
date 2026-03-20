# 22: Production ECU Application

## Target

`autosar_user_raspi_ecu` — source: `user_apps/src/apps/basic/raspi_ecu_app.cpp`

## Purpose

This tutorial explains the **production-style** ECU application that delegates
all platform concerns to dedicated daemons.

Unlike the monolithic `raspi_mock_ecu_app` (which embeds diagnostic, PHM,
and crypto logic in a single binary), `raspi_ecu_app` is a **thin vehicle
application** responsible only for:

1. **Lifecycle management** (`ara::exec`)
2. **Health reporting** to the PHM daemon (`ara::phm`)
3. **Persistent counters / config** (`ara::per`)
4. **Vehicle sensor data** from CAN / vCAN
5. **Telemetry publishing** via DLT logging and status file
6. **Machine-state reactions** (`ara::sm`)

Platform services — diagnostic server, PHM orchestrator, crypto provider,
UCM, NM, time-sync — all run as separate systemd daemons.

## Architecture Comparison

```
  ┌─ raspi_mock_ecu_app (monolithic) ─┐    ┌─ raspi_ecu_app (production) ─┐
  │  Diag Server (embedded)           │    │  Thin vehicle logic only    │
  │  PHM Orchestrator (embedded)      │    │  → ara::exec lifecycle      │
  │  Crypto Self-Test (embedded)      │    │  → ara::phm health report   │
  │  Vehicle Logic                    │    │  → ara::per persistence     │
  │  CAN / Telemetry                  │    │  → ara::sm state reactions  │
  │  Persistence                      │    │  → Sensor read + telemetry  │
  └───────────────────────────────────┘    └─────────────────────────────┘
                                                      ▲  ▲  ▲
                                                      │  │  │
                               autosar_diag_server ───┘  │  │
                               autosar_phm_daemon ───────┘  │
                               autosar_crypto_provider ──────┘
```

## AUTOSAR APIs Used

| API | Usage |
|-----|-------|
| `ara::core::Initialize()` / `Deinitialize()` | Runtime lifecycle |
| `ara::exec::ApplicationClient` | Report execution state, request recovery |
| `ara::exec::SignalHandler` | SIGTERM / SIGINT handling |
| `ara::phm::HealthChannel` | Health status reporting (kNormal / kFailed) |
| `ara::per::OpenKeyValueStorage()` | Persistent boot & cycle counters |
| `ara::sm::MachineStateClient` | React to machine-state transitions |
| `ara::log::LoggingFramework` | Structured DLT logging |
| `ara::core::InstanceSpecifier` | Service port identifiers |

## Machine States

The app responds to these machine states via `ara::sm::MachineStateClient`:

| State | Action |
|-------|--------|
| `kStartup` | Initialize sensors, open persistent storage |
| `kRunning` | Normal sensor → telemetry loop |
| `kShutdown` | Flush persistent data, clean exit |
| `kRestart` | Flush and request restart |
| `kSuspend` | Pause telemetry loop |
| `kDiagnostic` | Reduce cycle rate for diagnostic session |
| `kUpdate` | Pause and wait for UCM completion |

## Environment Variables

| Variable | Default | Description |
|----------|---------|-------------|
| `AUTOSAR_ECU_PERIOD_MS` | `100` | Main loop cycle (ms) |
| `AUTOSAR_ECU_LOG_EVERY` | `50` | Log output every N cycles |
| `AUTOSAR_ECU_PERSIST_EVERY` | `100` | Persist every N cycles |
| `AUTOSAR_ECU_STATUS_FILE` | `/run/autosar/ecu_app.status` | Status output |
| `AUTOSAR_ECU_CAN_IFNAME` | `vcan0` | CAN interface name |

## Build

```bash
# Build user apps from installed runtime
./scripts/build_user_apps_from_opt.sh --prefix /opt/autosar-ap

# Binary location
ls build-user-apps-opt/src/apps/basic/autosar_user_raspi_ecu
```

## Run

```bash
# Set up virtual CAN (if needed)
sudo ./scripts/setup_socketcan_interface.sh --ifname vcan0 --vcan

# Run the production ECU app
AUTOSAR_ECU_PERIOD_MS=200 \
AUTOSAR_ECU_CAN_IFNAME=vcan0 \
  ./build-user-apps-opt/src/apps/basic/autosar_user_raspi_ecu
```

## Status File

The app writes to `/run/autosar/ecu_app.status`:

```
cycle=1500
boot_count=7
machine_state=kRunning
health_reports=1500
speed_centi_kph=8350
engine_rpm=3200
steering_centi_deg=-450
gear=3
coolant_temp_c=92
battery_voltage=138
updated_epoch_ms=1717171717000
```

## Persistent Data

Boot count and cycle count are persisted to survive restarts:

```cpp
ara::per::OpenKeyValueStorage(spec);
// Keys: "raspi_ecu.boot_count", "raspi_ecu.cycle_count"
```

## Deployment as systemd Service

For RPi deployment, this app runs alongside the 17 platform daemons.
See tutorial [50_rpi_ecu_deployment](50_rpi_ecu_deployment.md) for the
complete deployment procedure.

## Next Steps

| Tutorial | Topic |
|----------|-------|
| [21_ecu_full_stack](21_ecu_full_stack.md) | Full-stack integration (CAN+SOME/IP→DDS) |
| [30_platform_service_architecture](30_platform_service_architecture.md) | Platform daemon overview |
| [50_rpi_ecu_deployment](50_rpi_ecu_deployment.md) | RPi deployment |
