# 12: `ara::exec` Runtime Reporting and Monitoring

## Target

- `autosar_user_tpl_exec_runtime_monitor`
- Source: `user_apps/src/apps/feature/runtime/exec_runtime_monitor_app.cpp`

## Purpose

- Handle graceful termination (`SIGTERM`/`SIGINT`) via `ara::exec::SignalHandler`.
- Supervise process liveness via `ara::exec::extension::ProcessWatchdog`.
- (When vSomeIP is enabled) report `kRunning`/`kTerminating` to EM via `ara::exec::ExecutionClient`.

## Run 1: Standalone Monitoring Mode (No EM dependency)

Build user apps first:

```bash
./scripts/build_user_apps_from_opt.sh --prefix /opt/autosar_ap
```

Run with watchdog fault injection:

```bash
USER_EXEC_WATCHDOG_TIMEOUT_MS=800 \
USER_EXEC_FAULT_STALL_CYCLE=20 \
USER_EXEC_FAULT_STALL_MS=1800 \
./build-user-apps-opt/src/apps/feature/runtime/autosar_user_tpl_exec_runtime_monitor
```

What to verify:

1. `Fault injection: stall ...` appears.
2. Watchdog expiry detection logs appear.
3. `Ctrl+C` performs graceful shutdown and prints `Shutdown complete`.

## Run 2: EM State Reporting Mode

Run this in an environment where an EM-side server (`ExecutionServer`) is already running:

```bash
USER_EXEC_ENABLE_EM_REPORT=1 \
USER_EXEC_INSTANCE_SPECIFIER=AdaptiveAutosar/UserApps/ExecRuntimeMonitor \
USER_EXEC_EM_REPORT_TIMEOUT_SEC=2 \
./build-user-apps-opt/src/apps/feature/runtime/autosar_user_tpl_exec_runtime_monitor
```

What to verify:

1. Startup logs include `EM report: kRunning ...`.
2. Shutdown logs include `EM report: kTerminating ...`.

Notes:

- If EM is unavailable, the app logs `EM report ... failed` and continues.
- EM reporting mode requires a vSomeIP-enabled runtime build.

## Main Environment Variables

| Environment Variable | Default | Description |
| --- | --- | --- |
| `USER_EXEC_CYCLE_MS` | `200` | Main loop cycle [ms] |
| `USER_EXEC_STATUS_EVERY` | `10` | Status log period (in cycles) |
| `USER_EXEC_WATCHDOG_TIMEOUT_MS` | `1200` | Watchdog timeout [ms] |
| `USER_EXEC_WATCHDOG_STARTUP_GRACE_MS` | `500` | Startup grace period [ms] |
| `USER_EXEC_WATCHDOG_COOLDOWN_MS` | `1000` | Expiry callback cooldown [ms] |
| `USER_EXEC_WATCHDOG_KEEP_RUNNING` | `1` | Continue monitoring after expiry |
| `USER_EXEC_WATCHDOG_AUTO_RESET` | `0` | Call `Reset()` on detected expiry |
| `USER_EXEC_FAULT_STALL_CYCLE` | `0` | Fault injection cycle (`0` disables) |
| `USER_EXEC_FAULT_STALL_MS` | `timeout*2` | Stall duration during fault injection [ms] |
| `USER_EXEC_ENABLE_EM_REPORT` | `0` | Enable EM state reporting |
| `USER_EXEC_INSTANCE_SPECIFIER` | `AdaptiveAutosar/UserApps/ExecRuntimeMonitor` | InstanceSpecifier for EM report |
| `USER_EXEC_EM_REPORT_TIMEOUT_SEC` | `2` | EM report timeout [sec] |
| `USER_EXEC_RPC_IP` | `127.0.0.1` | (vSomeIP) RPC destination IP |
| `USER_EXEC_RPC_PORT` | `8080` | (vSomeIP) RPC destination port |
| `USER_EXEC_RPC_PROTOCOL_VERSION` | `1` | SOME/IP protocol version |

## Customization Points

1. Tune watchdog timeout/grace based on your workload timing.
2. Choose a supervision policy on violation (log only, `Reset()`, or stop process).
3. In production EM integration, align instance specifier and network settings with your manifests.
