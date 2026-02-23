# 12: `ara::exec` Runtime Monitoring (Standard API)

## Target

- `autosar_user_tpl_exec_runtime_monitor`
- Source: `user_apps/src/apps/feature/runtime/exec_runtime_monitor_app.cpp`

## Purpose

- Handle graceful termination (`SIGTERM`/`SIGINT`) via `ara::exec::SignalHandler`.
- Perform runtime alive-timeout supervision in the app loop.
- Report health status via `ara::phm::HealthChannel`.

## Notes

- This tutorial uses standard AUTOSAR AP cluster APIs (`ara::exec`, `ara::phm`).
- It does not depend on `ara::exec::extension::*`.

## Run 1: Standalone Monitoring

Build user apps first:

```bash
./scripts/build_user_apps_from_opt.sh --prefix /opt/autosar_ap
```

Run with fault injection:

```bash
USER_EXEC_ALIVE_TIMEOUT_MS=800 \
USER_EXEC_FAULT_STALL_CYCLE=20 \
USER_EXEC_FAULT_STALL_MS=1800 \
./build-user-apps-opt/src/apps/feature/runtime/autosar_user_tpl_exec_runtime_monitor
```

What to verify:

1. `Fault injection: stall ...` appears.
2. `Alive timeout detected...` appears.
3. `Ctrl+C` performs graceful shutdown and prints `Shutdown complete`.

## Run 2: Stop-on-Expired Policy

```bash
USER_EXEC_ALIVE_TIMEOUT_MS=800 \
USER_EXEC_FAULT_STALL_CYCLE=10 \
USER_EXEC_FAULT_STALL_MS=1800 \
USER_EXEC_STOP_ON_EXPIRED=1 \
./build-user-apps-opt/src/apps/feature/runtime/autosar_user_tpl_exec_runtime_monitor
```

What to verify:

1. Alive-timeout error logs appear.
2. Main loop exits by policy (`stop-on-expired`).

## Main Environment Variables

| Environment Variable | Default | Description |
| --- | --- | --- |
| `USER_EXEC_CYCLE_MS` | `200` | Main loop cycle [ms] |
| `USER_EXEC_STATUS_EVERY` | `10` | Status log period (in cycles) |
| `USER_EXEC_ALIVE_TIMEOUT_MS` | `1200` | Alive timeout threshold [ms] |
| `USER_EXEC_ALIVE_STARTUP_GRACE_MS` | `500` | Initial grace window before timeout checks [ms] |
| `USER_EXEC_ALIVE_COOLDOWN_MS` | `1000` | Cooldown for repeated timeout logs [ms] |
| `USER_EXEC_STOP_ON_EXPIRED` | `0` | Exit main loop when timeout is detected |
| `USER_EXEC_FAULT_STALL_CYCLE` | `0` | Fault injection cycle (`0` disables) |
| `USER_EXEC_FAULT_STALL_MS` | `alive_timeout*2` | Stall duration during fault injection [ms] |
| `USER_EXEC_HEALTH_INSTANCE_SPECIFIER` | `AdaptiveAutosar/UserApps/ExecRuntimeMonitor` | InstanceSpecifier used by `HealthChannel` |
| `USER_EXEC_WATCHDOG_TIMEOUT_MS` | `1200` | Legacy compatibility fallback for timeout |
| `USER_EXEC_WATCHDOG_STARTUP_GRACE_MS` | `500` | Legacy compatibility fallback for startup grace |
| `USER_EXEC_WATCHDOG_COOLDOWN_MS` | `1000` | Legacy compatibility fallback for cooldown |

## Customization Points

1. Tune timeout/grace/cooldown for your runtime profile.
2. Choose supervision policy (`continue` or `stop-on-expired`).
3. Align `USER_EXEC_HEALTH_INSTANCE_SPECIFIER` with deployment manifests.
