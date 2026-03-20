# 40: ExecutionManager Orchestration for User Apps

## Overview

This tutorial demonstrates using `ara::exec::ExecutionManager` to
**launch, monitor, and stop** multiple user applications as OS processes.

In AUTOSAR AP, applications run as independent OS processes.
The ExecutionManager (EM) manages their lifecycle.
This tutorial builds three binaries:

| Binary | Role |
|--------|------|
| `autosar_user_em_sensor_app` | Simulated temperature sensor (managed child process) |
| `autosar_user_em_actuator_app` | Simulated actuator (managed child process) |
| `autosar_user_em_daemon` | Orchestration daemon with ExecutionManager (parent process) |

```
[em_daemon_app]                [em_sensor_app]   [em_actuator_app]
 ExecutionManager ─fork/exec──►  (child)            (child)
 ExecutionServer   ◄──waitpid── exit(0)             exit(0)
 StateServer
```

---

## Target Files

| File | Description |
|------|-------------|
| `user_apps/src/apps/feature/em/em_daemon_app.cpp` | EM daemon (main focus) |
| `user_apps/src/apps/feature/em/em_sensor_app.cpp` | Managed sensor app |
| `user_apps/src/apps/feature/em/em_actuator_app.cpp` | Managed actuator app |
| `user_apps/src/apps/feature/em/CMakeLists.txt` | Build definitions |

---

## User App Registration and Execution Model

### AUTOSAR AP Process Manifest Concept

In AUTOSAR AP, application startup configuration is described in a **Process Manifest** (ARXML).
In this implementation, the `ProcessDescriptor` struct serves as the manifest equivalent.

```cpp
ara::exec::ProcessDescriptor desc;
desc.name          = "SensorApp";          // Unique process name
desc.executable    = "/opt/autosar-ap/bin/autosar_user_em_sensor_app";
desc.functionGroup = "MachineFG";          // Function Group name
desc.activeState   = "Running";            // Start in this FG state
desc.startupGrace  = std::chrono::milliseconds{3000};      // Startup grace period
desc.terminationTimeout = std::chrono::milliseconds{5000}; // Termination timeout
```

### Function Groups and States

A Function Group (FG) represents an application operating mode.

```
MachineFG
  ├── "Off"      ← All processes stopped
  └── "Running"  ← SensorApp / ActuatorApp start
```

Calling `ActivateFunctionGroup("MachineFG", "Running")` automatically starts
all processes registered with `activeState == "Running"`.

### Process State Transitions

```
kNotRunning
    │  ActivateFunctionGroup()
    ▼
kStarting ──── ExecutionState::kRunning reported ──► kRunning
    │                                                    │
    │  TerminateFunctionGroup() / Stop()                 │
    ▼                                                    ▼
kTerminating ────────────────────────────────────► kTerminated
```

---

## Step 1: Build

### Host (Ubuntu / macOS)

```bash
./scripts/build_user_apps_from_opt.sh --prefix /opt/autosar-ap

ls build-user-apps-opt/src/apps/feature/em/
# autosar_user_em_sensor_app
# autosar_user_em_actuator_app
# autosar_user_em_daemon
```

### Cross-Compile for Raspberry Pi (aarch64)

```bash
sudo apt-get install -y gcc-aarch64-linux-gnu g++-aarch64-linux-gnu

./scripts/build_user_apps_from_opt.sh \
  --prefix /opt/autosar-ap \
  --cross-compile aarch64-linux-gnu \
  --build-dir build-user-apps-rpi
```

---

## Step 2: Run on Host

Run a 10-second demo on the host:

```bash
cd build-user-apps-opt/src/apps/feature/em
./autosar_user_em_daemon
```

Expected output:

```
[EMDA] Config:  sensor_bin=autosar_user_em_sensor_app  actuator_bin=autosar_user_em_actuator_app  run_seconds=10 (0=until SIGTERM)
[EMDA] Registered SensorApp -> MachineFG/Running
[EMDA] Registered ActuatorApp -> MachineFG/Running
[EMDA] ExecutionManager started.
[EMDA] Activating MachineFG -> Running ...
[EMSA] [em_sensor_app] kRunning — interval_ms=1000
[EMAA] [em_actuator_app] kRunning — interval_ms=2000
[EMSA] [em_sensor_app] cycle=1  temperature=21.0 C
...
[EMDA] Auto-shutdown: run_seconds limit reached.
[EMDA] Terminating MachineFG -> Off ...
[EMDA] ExecutionManager stopped. Shutdown complete.
```

---

## Step 3: Environment Variable Customization

### Explicit Binary Paths

```bash
EM_SENSOR_BIN=/opt/autosar-ap/bin/autosar_user_em_sensor_app \
EM_ACTUATOR_BIN=/opt/autosar-ap/bin/autosar_user_em_actuator_app \
./autosar_user_em_daemon
```

### Unlimited Run (Until SIGTERM)

```bash
EM_RUN_SECONDS=0 ./autosar_user_em_daemon &
kill $!
```

### Change Sampling Intervals

```bash
EM_SENSOR_INTERVAL_MS=500 \
EM_ACTUATOR_INTERVAL_MS=1000 \
EM_RUN_SECONDS=15 \
./autosar_user_em_daemon
```

### Environment Variable Reference

#### `autosar_user_em_daemon`

| Variable | Default | Description |
|----------|---------|-------------|
| `EM_SENSOR_BIN` | Auto-detect | Path to sensor app binary |
| `EM_ACTUATOR_BIN` | Auto-detect | Path to actuator app binary |
| `EM_RUN_SECONDS` | `10` | Seconds until auto-shutdown (`0` = run until SIGTERM) |

#### `autosar_user_em_sensor_app`

| Variable | Default | Description |
|----------|---------|-------------|
| `EM_SENSOR_INTERVAL_MS` | `1000` | Sensor read interval (ms) |
| `EM_SENSOR_INSTANCE_ID` | `em_sensor_app` | Log identifier |

#### `autosar_user_em_actuator_app`

| Variable | Default | Description |
|----------|---------|-------------|
| `EM_ACTUATOR_INTERVAL_MS` | `2000` | Actuator command interval (ms) |
| `EM_ACTUATOR_INSTANCE_ID` | `em_actuator_app` | Log identifier |

---

## Step 4: Deploy to Raspberry Pi

### 4-1) Transfer Binaries

```bash
scp build-user-apps-rpi/src/apps/feature/em/autosar_user_em_* \
    pi@raspberrypi.local:/opt/autosar-ap/bin/
```

### 4-2) Test on RPi

```bash
ssh pi@raspberrypi.local
chmod +x /opt/autosar-ap/bin/autosar_user_em_*
/opt/autosar-ap/bin/autosar_user_em_daemon
```

### 4-3) Register as systemd Service

```bash
sudo tee /etc/systemd/system/autosar-em-demo.service > /dev/null << 'EOF'
[Unit]
Description=AUTOSAR AP ExecutionManager Demo
After=network.target

[Service]
Type=simple
User=pi
Environment=EM_SENSOR_BIN=/opt/autosar-ap/bin/autosar_user_em_sensor_app
Environment=EM_ACTUATOR_BIN=/opt/autosar-ap/bin/autosar_user_em_actuator_app
Environment=EM_RUN_SECONDS=0
ExecStart=/opt/autosar-ap/bin/autosar_user_em_daemon
Restart=on-failure
RestartSec=5s
StandardOutput=journal
StandardError=journal

[Install]
WantedBy=multi-user.target
EOF

sudo systemctl daemon-reload
sudo systemctl enable --now autosar-em-demo.service
sudo systemctl status autosar-em-demo.service --no-pager
```

---

## Step 5: Register Your Own App in ExecutionManager

### 5-1) Managed App Pattern

Managed apps (child processes fork/exec'd by EM) follow this pattern:

```cpp
#include <ara/core/initialization.h>
#include <ara/exec/signal_handler.h>
#include <ara/log/logging_framework.h>

int main()
{
    auto initResult{ara::core::Initialize()};
    if (!initResult.HasValue()) { return 1; }

    ara::exec::SignalHandler::Register();

    while (!ara::exec::SignalHandler::IsTerminationRequested())
    {
        // App logic
        std::this_thread::sleep_for(std::chrono::milliseconds{100});
    }

    (void)ara::core::Deinitialize();
    return 0;  // Return 0 — EM interprets as normal termination
}
```

**Key points:**
- Always call `ara::exec::SignalHandler::Register()` for SIGTERM handling
- Check `SignalHandler::IsTerminationRequested()` in the main loop
- Return 0 for normal exit (EM records `kTerminated`)
- Non-zero return causes EM to record `kFailed`

### 5-2) Register ProcessDescriptor

```cpp
ara::exec::ProcessDescriptor myAppDesc;
myAppDesc.name          = "MyApp";
myAppDesc.executable    = "/opt/autosar-ap/bin/my_app";
myAppDesc.functionGroup = "MachineFG";
myAppDesc.activeState   = "Running";
myAppDesc.startupGrace  = std::chrono::milliseconds{3000};
myAppDesc.terminationTimeout = std::chrono::milliseconds{5000};
myAppDesc.arguments     = {"--config=/etc/myapp.conf"};

auto result{em.RegisterProcess(myAppDesc)};
```

### 5-3) Design Function Groups and States

Multiple FG states support different operating modes:

```cpp
// Standby→Running→Off transitions
em.ActivateFunctionGroup("MachineFG", "Standby");   // DiagApp starts
em.ActivateFunctionGroup("MachineFG", "Running");   // DiagApp stops + SensorApp starts
em.TerminateFunctionGroup("MachineFG");             // All stop
```

### 5-4) Process State Change Callbacks

```cpp
em.SetProcessStateChangeHandler(
    [](const std::string &name, ara::exec::ManagedProcessState state)
    {
        if (state == ara::exec::ManagedProcessState::kFailed)
        {
            // Implement fallback logic
        }
    });
```

---

## Step 6: Troubleshooting

### Binary Not Found

```
[EMDA] Process state changed: SensorApp -> Failed
```

**Fix:** Check that the executable path exists and has execute permission.

### Child Process Stuck at kStarting

**Cause:** Demo uses in-process state tracking without RPC reporting.
The process runs correctly; managed state remains `kStarting`.
For full state transitions, implement `ara::exec::ExecutionClient` in child processes.

### SIGTERM Doesn't Stop Child

**Fix:** Ensure child calls `ara::exec::SignalHandler::Register()`.

### SIGKILL After terminationTimeout

This is by design. Increase `terminationTimeout` or speed up shutdown logic.

---

## Summary

| Concept | Implementation |
|---------|---------------|
| ProcessDescriptor registration | `em.RegisterProcess(desc)` |
| Function Group activation | `em.ActivateFunctionGroup("FG", "State")` |
| Function Group termination | `em.TerminateFunctionGroup("FG")` |
| State change callback | `em.SetProcessStateChangeHandler(handler)` |
| SIGTERM handling (child side) | `SignalHandler::Register()` / `IsTerminationRequested()` |
| Process launch | `fork()` / `exec()` |
| Process stop | `SIGTERM` → waitpid timeout → `SIGKILL` |

## References

- [src/ara/exec/execution_manager.h](../../src/ara/exec/execution_manager.h) — ExecutionManager class
- [user_apps/src/apps/feature/em/em_daemon_app.cpp](../src/apps/feature/em/em_daemon_app.cpp) — Daemon implementation
- [04_exec_runtime_monitoring](04_exec_runtime_monitoring.md) — SignalHandler and Alive supervision
- [50_rpi_ecu_deployment](50_rpi_ecu_deployment.md) — RPi deployment
