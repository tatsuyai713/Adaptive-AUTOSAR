# 02: Execution Control and Signal Handling

## Target

- `autosar_user_exec_signal_template`
- Source: `user_apps/src/apps/basic/exec_signal_app.cpp`

## Purpose

- Understand safe shutdown using `ara::exec::SignalHandler`.
- Create a structure that monitors an exit flag on `SIGINT`/`SIGTERM`.

## Run

```bash
./build-user-apps-opt/src/apps/basic/autosar_user_exec_signal_template
```

Stop the application with `Ctrl+C` and confirm that the shutdown sequence executes.

## Customization Points

1. Add monitoring or communication processing to the periodic main-loop body.
2. Add any required cleanup actions on stop (file sync, communication teardown, etc.).
