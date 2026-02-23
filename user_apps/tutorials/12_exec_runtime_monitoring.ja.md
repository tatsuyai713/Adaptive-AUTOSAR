# 12: `ara::exec` 実行監視（標準API）

## 対象ターゲット

- `autosar_user_tpl_exec_runtime_monitor`
- ソース: `user_apps/src/apps/feature/runtime/exec_runtime_monitor_app.cpp`

## 目的

- `ara::exec::SignalHandler` で終了要求 (`SIGTERM`/`SIGINT`) を扱う。
- アプリループで Alive タイムアウト監視を行う。
- `ara::phm::HealthChannel` でヘルス状態を報告する。

## 注記

- このチュートリアルは標準 AUTOSAR AP クラスタAPI（`ara::exec` / `ara::phm`）を使用します。
- `ara::exec::extension::*` には依存しません。

## 実行 1: 単体監視モード

まず通常の user_apps ビルドを行います。

```bash
./scripts/build_user_apps_from_opt.sh --prefix /opt/autosar_ap
```

故障注入ありで起動します。

```bash
USER_EXEC_ALIVE_TIMEOUT_MS=800 \
USER_EXEC_FAULT_STALL_CYCLE=20 \
USER_EXEC_FAULT_STALL_MS=1800 \
./build-user-apps-opt/src/apps/feature/runtime/autosar_user_tpl_exec_runtime_monitor
```

確認ポイント:

1. `Fault injection: stall ...` のログが出る。
2. `Alive timeout detected...` のログが出る。
3. `Ctrl+C` で安全停止し、`Shutdown complete` が出る。

## 実行 2: Expired時停止ポリシー

```bash
USER_EXEC_ALIVE_TIMEOUT_MS=800 \
USER_EXEC_FAULT_STALL_CYCLE=10 \
USER_EXEC_FAULT_STALL_MS=1800 \
USER_EXEC_STOP_ON_EXPIRED=1 \
./build-user-apps-opt/src/apps/feature/runtime/autosar_user_tpl_exec_runtime_monitor
```

確認ポイント:

1. Alive タイムアウトエラーログが出る。
2. ポリシーによりメインループが終了する。

## 主な環境変数

| 環境変数 | 既定値 | 説明 |
| --- | --- | --- |
| `USER_EXEC_CYCLE_MS` | `200` | メインループ周期 [ms] |
| `USER_EXEC_STATUS_EVERY` | `10` | ステータスログ出力周期（サイクル数） |
| `USER_EXEC_ALIVE_TIMEOUT_MS` | `1200` | Alive タイムアウト閾値 [ms] |
| `USER_EXEC_ALIVE_STARTUP_GRACE_MS` | `500` | タイムアウト判定開始までの猶予時間 [ms] |
| `USER_EXEC_ALIVE_COOLDOWN_MS` | `1000` | 連続タイムアウトログのクールダウン [ms] |
| `USER_EXEC_STOP_ON_EXPIRED` | `0` | タイムアウト検出時にメインループを終了するか |
| `USER_EXEC_FAULT_STALL_CYCLE` | `0` | 故障注入するサイクル（`0` で無効） |
| `USER_EXEC_FAULT_STALL_MS` | `alive_timeout*2` | 故障注入時の停止時間 [ms] |
| `USER_EXEC_HEALTH_INSTANCE_SPECIFIER` | `AdaptiveAutosar/UserApps/ExecRuntimeMonitor` | `HealthChannel` の InstanceSpecifier |
| `USER_EXEC_WATCHDOG_TIMEOUT_MS` | `1200` | 互換用: タイムアウト値のフォールバック |
| `USER_EXEC_WATCHDOG_STARTUP_GRACE_MS` | `500` | 互換用: 起動猶予値のフォールバック |
| `USER_EXEC_WATCHDOG_COOLDOWN_MS` | `1000` | 互換用: クールダウン値のフォールバック |

## 変更ポイント

1. 監視対象に合わせて timeout/grace/cooldown を調整する。
2. 監視違反時の方針（継続/停止）を定義する。
3. `USER_EXEC_HEALTH_INSTANCE_SPECIFIER` をマニフェスト定義と一致させる。
