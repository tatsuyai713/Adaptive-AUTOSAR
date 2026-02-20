# 01: Runtime Initialization and Shutdown

## Target

- `autosar_user_tpl_runtime_lifecycle`
- Source: `user_apps/src/apps/feature/runtime/runtime_lifecycle_app.cpp`

## Purpose

- Understand the minimal setup for `ara::core::Initialize()` and `ara::core::Deinitialize()`.
- Create a template for logger initialization and main-loop structure.

## Run

```bash
./build-user-apps-opt/src/apps/feature/runtime/autosar_user_tpl_runtime_lifecycle
```

## Customization Points

1. Replace the body of the `for` loop with your own business logic.
2. Change the logger context IDs (`UTRL`/`RTLF`) to match your application.
3. Add a `try-catch` around `main()` if exception handling is required.
