# 51: Raspberry Pi Minimal ECU — Library Build to User App Management

## Overview

This tutorial covers the full workflow from **library build verification** to
**Raspberry Pi minimal deployment** and **user app registration / execution / monitoring**.

### Learning Goals

1. Verify that AUTOSAR AP libraries build correctly
2. Identify the minimal set of services needed for a Raspberry Pi ECU
3. Register user apps with auto-start, health monitoring, and auto-restart
4. Understand the end-to-end workflow

### Architecture

```
┌─────────────────────────── Raspberry Pi ECU ───────────────────────────┐
│                                                                        │
│  [Tier 0: Communication Infrastructure]                                │
│    iceoryx RouDi ──── zero-copy shared memory                          │
│    vSomeIP Routing ── SOME/IP service discovery                        │
│                                                                        │
│  [Tier 1: Platform Foundation] (enable only what's needed)             │
│    Time Sync ──── clock synchronization                                │
│    Persistency ── data persistence                                     │
│    IAM Policy ─── access control                                       │
│    SM State ───── state management                                     │
│    CAN Manager ── CAN bus (CAN only)                                   │
│    Crypto Provider ── HSM key management                               │
│                                                                        │
│  [Tier 2: Platform Applications]                                       │
│    adaptive_autosar ── EM/SM/PHM/Diag/Vehicle integration binary       │
│    Diag Server ─────── UDS/DoIP diagnostic endpoint                    │
│    PHM Daemon ──────── platform health orchestrator                    │
│                                                                        │
│  [Tier 3: Execution Management & Monitoring]                           │
│    Exec Manager ──── runs bringup.sh to launch user apps               │
│    User App Monitor ─ PID / heartbeat / PHM monitoring + auto-restart  │
│    Watchdog ──────── hardware WDT kick                                 │
│                                                                        │
│  [User Applications]                                                    │
│    bringup.sh registered → launch_app / launch_app_with_heartbeat      │
│    / launch_app_managed                                                 │
│      ├── my_sensor_app (example)                                        │
│      ├── my_actuator_app (example)                                      │
│      └── ...                                                            │
└────────────────────────────────────────────────────────────────────────┘
```

---

## Part 1: Library Build Verification

### 1.1 Prerequisites

| Item | Requirement |
|------|-------------|
| OS | Ubuntu 22.04+ / Debian 12+ (x86_64 or aarch64) |
| Compiler | GCC 11+ (C++14 support) |
| CMake | 3.14 or later |
| Python | 3.8 or later |
| OpenSSL | libssl-dev (3.0 compatible) |
| Middleware | vsomeip, iceoryx, CycloneDDS |

### 1.2 Middleware Installation

```bash
sudo ./scripts/install_middleware_stack.sh --install-base-deps
```

Installs:

| Middleware | Install Path | Purpose |
|------------|-------------|---------|
| iceoryx | `/opt/iceoryx` | Zero-copy shared-memory IPC |
| vSomeIP | `/opt/vsomeip` | SOME/IP communication |
| CycloneDDS | `/opt/cyclonedds` | DDS communication |

### 1.3 Build Libraries

```bash
mkdir -p build-check && cd build-check

cmake .. \
  -DCMAKE_BUILD_TYPE=Release \
  -Dbuild_tests=OFF \
  -DARA_COM_USE_VSOMEIP=ON \
  -DARA_COM_USE_ICEORYX=ON \
  -DARA_COM_USE_CYCLONEDDS=ON \
  -DVSOMEIP_PREFIX=/opt/vsomeip \
  -DICEORYX_PREFIX=/opt/iceoryx \
  -DCYCLONEDDS_PREFIX=/opt/cyclonedds \
  -DAUTOSAR_AP_BUILD_PLATFORM_APP=ON

make -j$(nproc)
```

### 1.4 Build Artifacts

**Static Libraries (14)**

| Library | AUTOSAR FC | Description |
|---------|-----------|-------------|
| `libara_core.a` | Core Types | Result, Optional, ErrorCode |
| `libara_log.a` | Logging | DLT logging framework |
| `libara_com.a` | Communication | SOME/IP, DDS, ZeroCopy, SecOC |
| `libara_exec.a` | Execution Management | Process management, Function Groups |
| `libara_sm.a` | State Management | Machine state handlers |
| `libara_diag.a` | Diagnostics | UDS services, DTC, monitors |
| `libara_phm.a` | Platform Health | Supervision, health channels |
| `libara_per.a` | Persistency | KV store, file storage |
| `libara_crypto.a` | Cryptography | AES, RSA, ECDSA, HKDF, X.509 |
| `libara_iam.a` | Identity & Access | Access control, policy engine |
| `libara_tsync.a` | Time Synchronization | NTP/PTP |
| `libara_ucm.a` | Update & Config | OTA update management |
| `libara_nm.a` | Network Management | NM PDU, coordinator |
| `libarxml.a` | ARXML Parser | Manifest parsing (pugixml) |

**Platform Binaries (17)**

| Binary | Description |
|--------|-------------|
| `adaptive_autosar` | Platform integration (EM/SM/PHM/Diag/Vehicle) |
| `autosar_vsomeip_routing_manager` | vSomeIP routing |
| `autosar_time_sync_daemon` | Time synchronization |
| `autosar_persistency_guard` | Persistence guard |
| `autosar_iam_policy_loader` | IAM policy loader |
| `autosar_can_interface_manager` | CAN bus manager |
| `autosar_watchdog_supervisor` | Watchdog supervisor |
| `autosar_user_app_monitor` | User app monitor |
| `autosar_sm_state_daemon` | State management |
| `autosar_ntp_time_provider` | NTP time provider |
| `autosar_ptp_time_provider` | PTP time provider |
| `autosar_network_manager` | Network manager |
| `autosar_ucm_daemon` | UCM daemon |
| `autosar_dlt_daemon` | DLT log daemon |
| `autosar_diag_server` | UDS/DoIP diagnostic server |
| `autosar_phm_daemon` | Platform health orchestrator |
| `autosar_crypto_provider` | HSM & crypto provider |

---

## Part 2: ECU Feature Review — Identifying the Minimal Profile

### 2.1 Full Service Inventory

The RPi ECU profile includes 20 systemd services.
Not all are always required.

#### Required (Tier 0 — Communication Infrastructure)

| Service | Reason |
|---------|--------|
| `autosar-iox-roudi` | Zero-copy IPC runtime. Hard dependency of platform-app and exec-manager |
| `autosar-vsomeip-routing` | SOME/IP routing. Required for service discovery |

#### Required (Tier 2–3 — Platform & Execution Management)

| Service | Reason |
|---------|--------|
| `autosar-platform-app` | EM/SM/PHM/Diag/Vehicle integration process |
| `autosar-exec-manager` | Runs bringup.sh to launch user apps |

#### Recommended (Tier 1 — Platform Foundation)

| Service | Recommendation | When to Skip |
|---------|---------------|--------------|
| `autosar-time-sync` | Essential for time-sensitive apps | No time precision needed |
| `autosar-persistency-guard` | Periodic KV store sync | No persistence needed |
| `autosar-iam-policy` | IAM access control | No security required |
| `autosar-user-app-monitor` | PID/heartbeat/PHM monitoring | Manual monitoring suffices |
| `autosar-watchdog` | HW watchdog kick | No WDT hardware |
| `autosar-sm-state` | State machine management | No state transitions |
| `autosar-diag-server` | UDS/DoIP diagnostic endpoint | No diagnostics needed |
| `autosar-phm-daemon` | Platform health orchestration | No health monitoring |
| `autosar-crypto-provider` | HSM key management | No crypto services |

#### Optional (Use-Case Specific)

| Service | Purpose | Disable If |
|---------|---------|------------|
| `autosar-can-manager` | CAN bus | No CAN hardware |
| `autosar-network-manager` | NM state machine | No NM needed |
| `autosar-ntp-time-provider` | NTP source | time-sync alone suffices |
| `autosar-ptp-time-provider` | PTP/gPTP source | No PTP |
| `autosar-dlt` | DLT log aggregation | Console logging suffices |
| `autosar-ucm` | OTA updates | No updates needed |
| `autosar-ecu-full-stack` | Legacy demo | Always disable (use bringup.sh) |

### 2.2 Minimal Profile

**Minimum 4 services** to launch and manage user apps:

```
autosar-iox-roudi          ← zero-copy IPC backbone
autosar-vsomeip-routing    ← SOME/IP routing
autosar-platform-app       ← platform integration process
autosar-exec-manager       ← user app launcher (bringup.sh)
```

**Recommended additions** for production-like monitoring:

```
autosar-user-app-monitor   ← PID/heartbeat monitoring + auto-restart
autosar-watchdog           ← HW watchdog
autosar-persistency-guard  ← persistent data protection
autosar-diag-server        ← UDS/DoIP diagnostics
autosar-phm-daemon         ← platform health orchestration
autosar-crypto-provider    ← HSM key management
```

---

## Part 3: Build and Install (Raspberry Pi)

### 3.1 One-Command Build & Install

```bash
sudo ./scripts/build_and_install_rpi_ecu_profile.sh \
  --prefix /opt/autosar-ap \
  --runtime-build-dir build-rpi-autosar-ap \
  --user-app-build-dir /opt/autosar-ap/user_apps_build \
  --install-middleware
```

### 3.2 CAN Interface Setup

```bash
# Physical CAN
sudo ./scripts/setup_socketcan_interface.sh --ifname can0 --bitrate 500000

# Virtual CAN (testing)
sudo ./scripts/setup_socketcan_interface.sh --ifname vcan0 --vcan
```

### 3.3 Install systemd Services

```bash
sudo ./scripts/install_rpi_ecu_services.sh \
  --prefix /opt/autosar-ap \
  --user-app-build-dir /opt/autosar-ap/user_apps_build \
  --enable
```

### 3.4 Disable Unnecessary Services (Minimal Profile)

```bash
# Disable services not needed
sudo systemctl disable --now autosar-can-manager.service
sudo systemctl disable --now autosar-network-manager.service
sudo systemctl disable --now autosar-ntp-time-provider.service
sudo systemctl disable --now autosar-ptp-time-provider.service
sudo systemctl disable --now autosar-dlt.service
sudo systemctl disable --now autosar-ucm.service
sudo systemctl disable --now autosar-ecu-full-stack.service
```

---

## Part 4: User App Registration, Execution, and Management

### 4.1 Architecture

```
autosar-exec-manager.service
    │
    └─ exec /etc/autosar/bringup.sh
         │
         ├─ launch_app "my_app" /path/to/my_app
         │     └─ Register PID/name in /run/autosar/user_apps_registry.csv
         │
         ├─ launch_app_with_heartbeat "my_app2" "$HB_FILE" "5000" /path/to/my_app2
         │     └─ Register PID + heartbeat file + timeout
         │
         └─ launch_app_managed "my_app3" "Instance/Spec" "$HB" "5000" "5" "60000" /path/to/my_app3
               └─ Register PID + instance specifier + restart policy

autosar-user-app-monitor.service
    │
    └─ Monitor loop (1s interval)
         ├─ PID alive check (kill -0)
         ├─ Zombie detection (waitpid WNOHANG)
         ├─ Heartbeat file freshness check
         ├─ PHM HealthChannel status check
         └─ On anomaly → restart (with backoff per restart_limit/restart_window_ms)
```

### 4.2 Customize bringup.sh

Edit `/etc/autosar/bringup.sh` to register apps.

#### Helper Functions

| Function | Purpose | Arguments |
|----------|---------|-----------|
| `launch_app` | Basic launch | `"name" command [args...]` |
| `launch_app_with_heartbeat` | With heartbeat | `"name" "hb_file" "timeout_ms" command [args...]` |
| `launch_app_managed` | Full management | `"name" "instance_spec" "hb_file" "timeout_ms" "restart_limit" "restart_window_ms" command [args...]` |

#### Example: Basic Launch

```bash
MY_APP="${AUTOSAR_USER_APPS_BUILD_DIR}/src/apps/basic/autosar_user_minimal_runtime"
launch_app "minimal_runtime" "${MY_APP}"
```

#### Example: Heartbeat Monitoring

```bash
HEALTH_APP="${AUTOSAR_USER_APPS_BUILD_DIR}/src/apps/basic/autosar_user_exec_signal_template"
HB_FILE="${AUTOSAR_USER_APP_STATUS_DIR}/$(sanitize_name "exec_signal_app").heartbeat"
launch_app_with_heartbeat "exec_signal_app" "${HB_FILE}" "5000" "${HEALTH_APP}"
```

#### Example: Full Management with Restart Policy

```bash
PHM_APP="${AUTOSAR_USER_APPS_BUILD_DIR}/src/apps/basic/autosar_user_per_phm_demo"
launch_app_managed \
  "per_phm_demo" \
  "AdaptiveAutosar/UserApps/PerPhmDemo" \
  "" "0" "5" "60000" \
  "${PHM_APP}"
```

| Parameter | Value | Meaning |
|-----------|-------|---------|
| Name | `"per_phm_demo"` | Unique registry name |
| Instance specifier | `"AdaptiveAutosar/UserApps/PerPhmDemo"` | `ara::core::InstanceSpecifier` |
| HB file | `""` | Empty = no heartbeat |
| HB timeout | `"0"` | 0ms = check disabled |
| Restart limit | `"5"` | Max 5 restarts per window |
| Restart window | `"60000"` | 60-second window |

### 4.3 Start Services

Minimal startup order:

```bash
# Tier 0: Communication
sudo systemctl start autosar-iox-roudi.service
sudo systemctl start autosar-vsomeip-routing.service

# Tier 1: Platform foundation (as needed)
sudo systemctl start autosar-time-sync.service
sudo systemctl start autosar-persistency-guard.service
sudo systemctl start autosar-iam-policy.service
sudo systemctl start autosar-crypto-provider.service

# Tier 2: Platform applications
sudo systemctl start autosar-platform-app.service
sudo systemctl start autosar-diag-server.service
sudo systemctl start autosar-phm-daemon.service

# Tier 3: Execution management
sudo systemctl start autosar-exec-manager.service
sudo systemctl start autosar-user-app-monitor.service
sudo systemctl start autosar-watchdog.service
```

### 4.4 Check Status

```bash
# Service status
for svc in autosar-iox-roudi autosar-vsomeip-routing autosar-platform-app autosar-exec-manager; do
  echo -n "$svc: "
  systemctl is-active $svc.service
done

# User app registry
cat /run/autosar/user_apps_registry.csv

# Monitor status
cat /run/autosar/user_app_monitor.status
```

### 4.5 User App Monitor Configuration

Edit `/etc/default/autosar-user-app-monitor`:

| Variable | Default | Description |
|----------|---------|-------------|
| `AUTOSAR_USER_APP_MONITOR_PERIOD_MS` | `1000` | Polling interval (ms) |
| `AUTOSAR_USER_APP_MONITOR_HEARTBEAT_GRACE_MS` | `500` | Heartbeat tolerance (ms) |
| `AUTOSAR_USER_APP_MONITOR_STARTUP_GRACE_MS` | `3000` | Post-startup grace (ms) |
| `AUTOSAR_USER_APP_MONITOR_RESTART_BACKOFF_MS` | `1000` | Min wait between restarts (ms) |
| `AUTOSAR_USER_APP_MONITOR_ENFORCE_HEALTH` | `true` | Restart on PHM anomaly |
| `AUTOSAR_USER_APP_MONITOR_RESTART_ON_FAILURE` | `true` | Restart on exit |

---

## Part 5: Verification

### Automated

```bash
./scripts/verify_rpi_ecu_profile.sh \
  --prefix /opt/autosar-ap \
  --user-app-build-dir /opt/autosar-ap/user_apps_build \
  --can-backend mock \
  --require-platform-binary
```

### Manual Checklist

```bash
# Platform services active?
for svc in autosar-iox-roudi autosar-vsomeip-routing autosar-platform-app autosar-exec-manager; do
  echo -n "$svc: "; systemctl is-active $svc.service
done

# User apps registered?
wc -l /run/autosar/user_apps_registry.csv

# Logs
journalctl -u autosar-exec-manager.service --no-pager -n 20
journalctl -u autosar-user-app-monitor.service --no-pager -n 20
```

---

## Part 6: Practical Example — Custom App Creation & Deployment

### 6.1 Create a Minimal App

```cpp
// user_apps/src/apps/basic/my_custom_app.cpp
#include <chrono>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <thread>

#include "ara/core/initialization.h"
#include "ara/exec/execution_client.h"

static volatile sig_atomic_t gRunning = 1;
static void signalHandler(int) { gRunning = 0; }

int main()
{
    ara::core::Initialize();
    std::signal(SIGTERM, signalHandler);
    std::signal(SIGINT, signalHandler);

    ara::core::InstanceSpecifier specifier("AdaptiveAutosar/UserApps/MyCustomApp");
    ara::exec::ExecutionClient execClient(specifier);
    execClient.ReportExecutionState(ara::exec::ExecutionState::kRunning);

    const char *hbPath = std::getenv("MY_APP_HEARTBEAT_FILE");
    std::printf("[MyApp] Started. PID=%d\n", static_cast<int>(getpid()));

    while (gRunning)
    {
        std::printf("[MyApp] Working...\n");
        if (hbPath)
        {
            std::ofstream hb(hbPath);
            hb << std::chrono::duration_cast<std::chrono::seconds>(
                      std::chrono::system_clock::now().time_since_epoch()).count();
        }
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    std::printf("[MyApp] Shutting down.\n");
    ara::core::Deinitialize();
    return 0;
}
```

### 6.2 Add to CMakeLists.txt

```cmake
add_user_template_target(
  autosar_user_my_custom_app
  my_custom_app.cpp
  AdaptiveAutosarAP::ara_core
  AdaptiveAutosarAP::ara_exec
)
```

### 6.3 Build

```bash
./scripts/build_user_apps_from_opt.sh --prefix /opt/autosar-ap
```

### 6.4 Register in bringup.sh

```bash
MY_CUSTOM_APP="${AUTOSAR_USER_APPS_BUILD_DIR}/src/apps/basic/autosar_user_my_custom_app"
HB_FILE="${AUTOSAR_USER_APP_STATUS_DIR}/my_custom_app.heartbeat"
export MY_APP_HEARTBEAT_FILE="${HB_FILE}"
launch_app_with_heartbeat "my_custom_app" "${HB_FILE}" "5000" "${MY_CUSTOM_APP}"
```

### 6.5 Deploy and Verify

```bash
sudo systemctl restart autosar-exec-manager.service
sleep 3
cat /run/autosar/user_apps_registry.csv
```

### 6.6 Test Failure Recovery

```bash
MYPID=$(grep "my_custom_app" /run/autosar/user_apps_registry.csv | cut -d, -f2)
kill "${MYPID}"
sleep 5
cat /run/autosar/user_apps_registry.csv  # New PID after auto-restart
```

---

## Part 7: Service Dependency Map

```
local-fs.target / network-online.target
    │
    ├── autosar-iox-roudi             [Tier 0] Required (Restart=always)
    ├── autosar-vsomeip-routing       [Tier 0] Required (Restart=on-failure)
    │
    ├── autosar-time-sync             [Tier 1] Recommended
    ├── autosar-persistency-guard     [Tier 1] Recommended
    ├── autosar-iam-policy            [Tier 1] Recommended
    ├── autosar-sm-state              [Tier 1] Recommended
    ├── autosar-crypto-provider       [Tier 1] Recommended
    ├── autosar-can-manager           [Tier 1] CAN only
    ├── autosar-dlt                   [Tier 1] Optional
    ├── autosar-ntp-time-provider     [Tier 1] Optional
    └── autosar-ptp-time-provider     [Tier 1] Optional
         │
         ▼
    autosar-platform-app              [Tier 2] Required
    autosar-diag-server               [Tier 2] Recommended
    autosar-phm-daemon                [Tier 2] Recommended
    autosar-ucm                       [Tier 2] Optional
         │
         ▼
    autosar-exec-manager              [Tier 3] Required
         │
         ├── autosar-watchdog          [Tier 3] Recommended
         └── autosar-user-app-monitor  [Tier 3] Recommended
```

---

## Part 8: Troubleshooting

### Build Error: OpenSSL HKDF

**Symptom:** `EVP_PKEY_CTX_set_hkdf_md was not declared`

**Fix:** The project auto-switches with `#if OPENSSL_VERSION_NUMBER >= 0x30000000L`.
Check `src/ara/crypto/crypto_provider.cpp` if upgrading from an older codebase.

### Service Won't Start

```bash
journalctl -u autosar-<service-name>.service --no-pager -n 50
cat /etc/default/autosar-<service-name>
```

### User App Restart Loop

Check `restart_limit` / `restart_window_ms`.
Increase `AUTOSAR_USER_APP_MONITOR_STARTUP_GRACE_MS`.

### Heartbeat Timeout

Ensure the app writes its heartbeat file. Increase `heartbeat_timeout_ms`.

---

## Summary

| Step | Command |
|------|---------|
| 1. Install middleware | `sudo ./scripts/install_middleware_stack.sh --install-base-deps` |
| 2. Build & install runtime | `sudo ./scripts/build_and_install_rpi_ecu_profile.sh --prefix /opt/autosar-ap ...` |
| 3. CAN setup (if needed) | `sudo ./scripts/setup_socketcan_interface.sh --ifname vcan0 --vcan` |
| 4. Install services | `sudo ./scripts/install_rpi_ecu_services.sh --prefix /opt/autosar-ap ... --enable` |
| 5. Disable unnecessary | `sudo systemctl disable --now autosar-ecu-full-stack.service` |
| 6. Edit bringup.sh | `sudo vi /etc/autosar/bringup.sh` |
| 7. Start services | `sudo systemctl start autosar-iox-roudi ...` |
| 8. Verify | `./scripts/verify_rpi_ecu_profile.sh --prefix /opt/autosar-ap ... --can-backend mock` |

### Minimal Quick Reference

```bash
# Minimal 4 + 3 service configuration
sudo systemctl start autosar-iox-roudi.service
sudo systemctl start autosar-vsomeip-routing.service
sudo systemctl start autosar-platform-app.service
sudo systemctl start autosar-exec-manager.service
sudo systemctl start autosar-user-app-monitor.service
sudo systemctl start autosar-watchdog.service
sudo systemctl start autosar-persistency-guard.service
```
