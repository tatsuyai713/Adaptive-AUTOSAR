# 30: Platform Service Architecture

## Overview

This tutorial explains the **17 platform daemon binaries** and **20 systemd services**
that form the Adaptive AUTOSAR runtime.
Understanding this architecture is essential before deploying to a Raspberry Pi ECU
or developing production user applications.

## 4-Tier Service Architecture

```
┌──────────────────────── Raspberry Pi ECU ────────────────────────┐
│                                                                   │
│  [Tier 0 — Communication Infrastructure]                          │
│    iceoryx RouDi ─────── zero-copy shared-memory                  │
│    vSomeIP Routing ───── SOME/IP service discovery                │
│                                                                   │
│  [Tier 1 — Platform Foundation Services]                          │
│    Time Sync ────── time synchronization daemon                   │
│    NTP Provider ─── NTP clock source                              │
│    PTP Provider ─── PTP clock source                              │
│    Persistency ──── data persistence guard                        │
│    IAM Policy ───── identity & access control                     │
│    SM State ─────── state management                              │
│    CAN Manager ──── CAN bus interface management                  │
│    DLT Daemon ───── diagnostic log & trace                        │
│    Network Mgr ──── network management                            │
│    Crypto Provider ─ HSM key management & crypto ops              │
│                                                                   │
│  [Tier 2 — Platform Applications]                                 │
│    adaptive_autosar ── EM/SM/PHM/Diag/Vehicle integration binary  │
│    Diag Server ─────── UDS/DoIP diagnostic endpoint               │
│    PHM Daemon ──────── platform health management orchestrator    │
│    UCM Daemon ──────── update & configuration manager             │
│                                                                   │
│  [Tier 3 — Execution Management & Monitoring]                     │
│    Exec Manager ──── launches user apps via bringup.sh            │
│    User App Monitor ─ PID / heartbeat / PHM monitoring            │
│    Watchdog ──────── hardware WDT kick                            │
│                                                                   │
│  [User Applications]                                              │
│    raspi_ecu_app, ecu_full_stack, custom apps …                   │
└───────────────────────────────────────────────────────────────────┘
```

## Daemon Inventory

| # | Binary | Source | Description |
|---|--------|--------|-------------|
| 1 | `adaptive_autosar` | `main.cpp` | All-in-one platform binary (EM, SM, PHM, Diag, Vehicle) |
| 2 | `autosar_vsomeip_routing_manager` | `main_vsomeip_routing_manager.cpp` | vSomeIP routing daemon |
| 3 | `autosar_time_sync_daemon` | `main_time_sync_daemon.cpp` | Time synchronization |
| 4 | `autosar_ntp_time_provider` | `main_ntp_time_provider.cpp` | NTP clock source |
| 5 | `autosar_ptp_time_provider` | `main_ptp_time_provider.cpp` | PTP clock source |
| 6 | `autosar_persistency_guard` | `main_persistency_guard.cpp` | Persistent data protection |
| 7 | `autosar_iam_policy_loader` | `main_iam_policy_loader.cpp` | IAM policy enforcement |
| 8 | `autosar_sm_state_daemon` | `main_sm_state_daemon.cpp` | Machine state management |
| 9 | `autosar_can_interface_manager` | `main_can_interface_manager.cpp` | CAN bus management |
| 10 | `autosar_dlt_daemon` | `main_dlt_daemon.cpp` | Diagnostic logging |
| 11 | `autosar_network_manager` | `main_network_manager.cpp` | Network management |
| 12 | `autosar_crypto_provider` | `main_crypto_provider.cpp` | HSM & cryptographic operations |
| 13 | `autosar_diag_server` | `main_diag_server.cpp` | UDS/DoIP diagnostic server |
| 14 | `autosar_phm_daemon` | `main_phm_daemon.cpp` | Platform health orchestrator |
| 15 | `autosar_ucm_daemon` | `main_ucm_daemon.cpp` | Update & configuration manager |
| 16 | `autosar_user_app_monitor` | `main_user_app_monitor.cpp` | User app PID/heartbeat monitor |
| 17 | `autosar_watchdog_supervisor` | `main_watchdog_supervisor.cpp` | Hardware watchdog kick |

## systemd Service Map

20 systemd services are installed.
All service files are in `deployment/rpi_ecu/systemd/`.

| Service | Unit File | Tier | After | Before |
|---------|-----------|------|-------|--------|
| `autosar-iox-roudi` | External iceoryx | 0 | — | all |
| `autosar-vsomeip-routing` | `autosar-vsomeip-routing.service` | 0 | roudi | platform-app |
| `autosar-time-sync` | `autosar-time-sync.service` | 1 | roudi, vsomeip | platform-app |
| `autosar-ntp-time-provider` | `autosar-ntp-time-provider.service` | 1 | time-sync | platform-app |
| `autosar-ptp-time-provider` | `autosar-ptp-time-provider.service` | 1 | time-sync | platform-app |
| `autosar-persistency-guard` | `autosar-persistency-guard.service` | 1 | local-fs | platform-app |
| `autosar-iam-policy` | `autosar-iam-policy.service` | 1 | local-fs | platform-app |
| `autosar-sm-state` | `autosar-sm-state.service` | 1 | vsomeip | platform-app |
| `autosar-can-manager` | `autosar-can-manager.service` | 1 | network | platform-app |
| `autosar-dlt` | `autosar-dlt.service` | 1 | local-fs | platform-app |
| `autosar-network-manager` | `autosar-network-manager.service` | 1 | network | platform-app |
| `autosar-crypto-provider` | `autosar-crypto-provider.service` | 1 | local-fs | platform-app |
| `autosar-platform-app` | `autosar-platform-app.service` | 2 | Tier 1 | exec-manager |
| `autosar-diag-server` | `autosar-diag-server.service` | 2 | network, platform-app | exec-manager |
| `autosar-phm-daemon` | `autosar-phm-daemon.service` | 2 | platform-app | exec-manager |
| `autosar-ucm` | `autosar-ucm.service` | 2 | platform-app | exec-manager |
| `autosar-exec-manager` | `autosar-exec-manager.service` | 3 | Tier 2 | — |
| `autosar-user-app-monitor` | `autosar-user-app-monitor.service` | 3 | exec-manager | — |
| `autosar-watchdog` | `autosar-watchdog.service` | 3 | platform-app | — |
| `autosar-ecu-full-stack` | `autosar-ecu-full-stack.service` | App | roudi, vsomeip | — |

## Service Startup Order

```
                    ┌─ iox-roudi ─────────┐
                    │                     │
                    ├─ vsomeip-routing ───┤
                    │                     │
Tier 1 starts ─────┤  time-sync          │
(parallel)         │  ntp-provider       │
                    │  ptp-provider       │
                    │  persistency-guard  │
                    │  iam-policy         │
                    │  sm-state           │
                    │  can-manager        │
                    │  dlt               │
                    │  network-manager    │
                    │  crypto-provider    │
                    │                     │
Tier 2 starts ─────┤  platform-app       │
                    │  diag-server        │
                    │  phm-daemon         │
                    │  ucm               │
                    │                     │
Tier 3 starts ─────┤  exec-manager       │
                    │  user-app-monitor   │
                    │  watchdog           │
                    └─────────────────────┘
```

## Status Files

Every daemon writes periodic status to `/run/autosar/<name>.status`:

```bash
# Check all running daemon statuses
for f in /run/autosar/*.status; do
  echo "=== $(basename "$f") ==="
  cat "$f"
  echo
done
```

## Environment Configuration

Each service reads environment variables from `/etc/default/autosar-<name>`.
These files are generated from `deployment/rpi_ecu/env/autosar-<name>.env.in`
at install time.

Common pattern:

```bash
# /etc/default/autosar-diag-server  (example)
AUTOSAR_DIAG_LISTEN_ADDR=0.0.0.0
AUTOSAR_DIAG_LISTEN_PORT=13400
AUTOSAR_DIAG_P2_SERVER_MS=50
AUTOSAR_DIAG_STATUS_PERIOD_MS=2000
```

## Minimal Profile

Not all 20 services are required. For a minimal ECU deployment:

| Need | Enable |
|------|--------|
| IPC backbone | `autosar-iox-roudi` (always) |
| SOME/IP | `autosar-vsomeip-routing` |
| Platform core | `autosar-platform-app` |
| Diagnostics | `autosar-diag-server` |
| Health | `autosar-phm-daemon` |
| Persistency | `autosar-persistency-guard` |
| State mgmt | `autosar-sm-state` |

See [51_rpi_minimal_ecu_user_app_management](51_rpi_minimal_ecu_user_app_management.ja.md) for
the complete minimal profile guide.

## Next Steps

| Tutorial | Topic |
|----------|-------|
| [31_diag_server](31_diag_server.md) | Diagnostic server daemon in depth |
| [32_phm_daemon](32_phm_daemon.md) | Platform health management daemon |
| [33_crypto_provider](33_crypto_provider.md) | Cryptography provider daemon |
| [50_rpi_ecu_deployment](50_rpi_ecu_deployment.md) | Deploy all services to RPi |
