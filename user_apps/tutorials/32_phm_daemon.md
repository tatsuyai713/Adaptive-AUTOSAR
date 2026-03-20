# 32: Platform Health Management Daemon

## Target

`autosar_phm_daemon` — source: `src/main_phm_daemon.cpp`

## Purpose

The PHM daemon provides a **platform-wide health orchestrator** that aggregates
supervision results from all registered entities and evaluates overall
platform health (Normal / Degraded / Critical).

Deployed as `autosar-phm-daemon.service`, it runs independently of the
monitored applications and writes periodic health summaries.

## Architecture

```
  ┌─────────────────────────────────────────────┐
  │              autosar_phm_daemon              │
  │                                             │
  │  PhmOrchestrator                            │
  │  ├─ Entity: vsomeip_routing  → kOk          │
  │  ├─ Entity: time_sync        → kOk          │
  │  ├─ Entity: persistency      → kOk          │
  │  ├─ Entity: diag_server      → kFailed      │
  │  ├─ Entity: crypto_provider  → kOk          │
  │  ├─ …                                       │
  │  └─ Platform Health: kDegraded              │
  │                                             │
  │  → /run/autosar/phm.status                  │
  └─────────────────────────────────────────────┘
         ▲                        ▲
         │  ara::phm API          │  ara::phm API
  ┌──────┴──────┐          ┌──────┴──────┐
  │ User App    │          │ Platform    │
  │ (heartbeat) │          │ Daemons     │
  └─────────────┘          └─────────────┘
```

## Health States

### Platform Health

| State | Meaning |
|-------|---------|
| `kNormal` | All entities OK or within degraded threshold |
| `kDegraded` | Failed entity ratio exceeds degraded threshold |
| `kCritical` | Failed entity ratio exceeds critical threshold |

### Entity Supervision Status

| Status | Meaning |
|--------|---------|
| `kOk` | Alive, deadlines met |
| `kFailed` | Supervision failure detected |
| `kExpired` | Alive timeout expired |
| `kDeactivated` | Monitoring disabled |

## Environment Variables

| Variable | Default | Description |
|----------|---------|-------------|
| `AUTOSAR_PHM_PERIOD_MS` | `1000` | Evaluation cycle (ms) |
| `AUTOSAR_PHM_DEGRADED_THRESHOLD` | `0.0` | Failed-entity ratio → kDegraded |
| `AUTOSAR_PHM_CRITICAL_THRESHOLD` | `0.5` | Failed-entity ratio → kCritical |
| `AUTOSAR_PHM_ENTITIES` | *(13 platform services)* | Comma-separated entity names |
| `AUTOSAR_PHM_STATUS_FILE` | `/run/autosar/phm.status` | Status output path |

### Default Monitored Entities

```
vsomeip_routing,time_sync,persistency,iam_policy,sm_state,
can_manager,dlt,network_manager,crypto_provider,diag_server,
ucm,user_app_monitor,watchdog
```

## Run (Standalone)

```bash
# Run with defaults
./build-local-check/bin/autosar_phm_daemon

# Custom thresholds
AUTOSAR_PHM_DEGRADED_THRESHOLD=0.1 \
AUTOSAR_PHM_CRITICAL_THRESHOLD=0.3 \
  ./build-local-check/bin/autosar_phm_daemon
```

## Status File

The daemon writes to `/run/autosar/phm.status`:

```
platform_health=kNormal
entity_count=13
entity.vsomeip_routing.status=kOk
entity.vsomeip_routing.failures=0
entity.time_sync.status=kOk
entity.time_sync.failures=0
entity.diag_server.status=kFailed
entity.diag_server.failures=2
failed_entity_count=1
updated_epoch_ms=1717171717000
```

Monitor live:

```bash
watch -n1 cat /run/autosar/phm.status
```

## systemd Service

```bash
sudo systemctl enable --now autosar-phm-daemon.service
sudo systemctl status autosar-phm-daemon.service --no-pager
```

Service ordering:
- **After**: `autosar-platform-app.service`
- **Before**: `autosar-exec-manager.service`
- **Restart**: `on-failure` (delay 2 s)

## Integration with User Apps

User applications report health to the PHM daemon via `ara::phm::HealthChannel`:

```cpp
#include "ara/phm/health_channel.h"

ara::core::InstanceSpecifier spec("/app/health");
ara::phm::HealthChannel hc(spec);

// Report healthy
hc.ReportHealthStatus(ara::phm::HealthStatus::kNormal);

// Report failure
hc.ReportHealthStatus(ara::phm::HealthStatus::kFailed);
```

See tutorial [03_per_phm](03_per_phm.md) for a user-app-side example.

## Next Steps

| Tutorial | Topic |
|----------|-------|
| [03_per_phm](03_per_phm.md) | User-app health reporting |
| [04_exec_runtime_monitoring](04_exec_runtime_monitoring.md) | Alive-timeout supervision |
| [33_crypto_provider](33_crypto_provider.md) | Crypto provider daemon |
