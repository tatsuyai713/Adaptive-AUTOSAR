# 03: Persistency and PHM

## Target

- `autosar_user_per_phm_demo`
- Source: `user_apps/src/apps/basic/per_phm_app.cpp`

## Purpose

- Handle persistent data such as counters with `ara::per`.
- Report normal / failed / deactivated states with `ara::phm`.

## Run

```bash
./build-user-apps-opt/src/apps/basic/autosar_user_per_phm_demo
```

## Customization Points

1. Split key names to match your ECU function (e.g., `rx.count`, `tx.count`).
2. Concretize the conditions under which `kFailed` is reported.
3. Design the timing for calling `SyncToStorage()` after important value updates.
