# 12: `ara::exec` による実行状態報告と監視

## 対象ターゲット

- `autosar_user_tpl_exec_runtime_monitor`
- ソース: `user_apps/src/apps/feature/runtime/exec_runtime_monitor_app.cpp`

## 目的

- `ara::exec::SignalHandler` で終了要求 (`SIGTERM`/`SIGINT`) を扱う。
- `ara::exec::extension::ProcessWatchdog` で Alive 監視を行う。
- （vSomeIP 有効時）`ara::exec::ExecutionClient` で EM へ `kRunning`/`kTerminating` を報告する。

## 実行 1: 単体監視モード（EM未接続）

まず通常の user_apps ビルドを行います。

```bash
./scripts/build_user_apps_from_opt.sh --prefix /opt/autosar_ap
```

次に watchdog 故障注入を有効化して起動します。

```bash
USER_EXEC_WATCHDOG_TIMEOUT_MS=800 \
USER_EXEC_FAULT_STALL_CYCLE=20 \
USER_EXEC_FAULT_STALL_MS=1800 \
./build-user-apps-opt/src/apps/feature/runtime/autosar_user_tpl_exec_runtime_monitor
```

確認ポイント:

1. `Fault injection: stall ...` のログが出る。
2. watchdog expiry の検出ログが出る。
3. `Ctrl+C` で安全停止し、`Shutdown complete` が出る。

## 実行 2: EM 状態報告モード

EM サーバ側（`ExecutionServer` を持つプロセス）が起動済みの環境で、以下を実行します。

```bash
USER_EXEC_ENABLE_EM_REPORT=1 \
USER_EXEC_INSTANCE_SPECIFIER=AdaptiveAutosar/UserApps/ExecRuntimeMonitor \
USER_EXEC_EM_REPORT_TIMEOUT_SEC=2 \
./build-user-apps-opt/src/apps/feature/runtime/autosar_user_tpl_exec_runtime_monitor
```

確認ポイント:

1. 起動時に `EM report: kRunning ...` が出る。
2. 終了時に `EM report: kTerminating ...` が出る。

注記:

- EM 側未起動や通信不通の場合、`EM report ... failed` の警告を出して継続します。
- このモードは vSomeIP 有効ビルドを前提とします。

## 主な環境変数

| 環境変数 | 既定値 | 説明 |
| --- | --- | --- |
| `USER_EXEC_CYCLE_MS` | `200` | メインループ周期 [ms] |
| `USER_EXEC_STATUS_EVERY` | `10` | ステータスログ出力周期（サイクル数） |
| `USER_EXEC_WATCHDOG_TIMEOUT_MS` | `1200` | watchdog タイムアウト [ms] |
| `USER_EXEC_WATCHDOG_STARTUP_GRACE_MS` | `500` | 起動直後の猶予 [ms] |
| `USER_EXEC_WATCHDOG_COOLDOWN_MS` | `1000` | expiry コールバックのクールダウン [ms] |
| `USER_EXEC_WATCHDOG_KEEP_RUNNING` | `1` | expiry 後も監視継続するか |
| `USER_EXEC_WATCHDOG_AUTO_RESET` | `0` | expiry 検出時に `Reset()` するか |
| `USER_EXEC_FAULT_STALL_CYCLE` | `0` | 故障注入するサイクル（`0` で無効） |
| `USER_EXEC_FAULT_STALL_MS` | `timeout*2` | 故障注入時の停止時間 [ms] |
| `USER_EXEC_ENABLE_EM_REPORT` | `0` | EM への状態報告を有効化 |
| `USER_EXEC_INSTANCE_SPECIFIER` | `AdaptiveAutosar/UserApps/ExecRuntimeMonitor` | EM 報告の InstanceSpecifier |
| `USER_EXEC_EM_REPORT_TIMEOUT_SEC` | `2` | EM 報告の待ち時間 [sec] |
| `USER_EXEC_RPC_IP` | `127.0.0.1` | （vSomeIP）RPC 接続先 IP |
| `USER_EXEC_RPC_PORT` | `8080` | （vSomeIP）RPC 接続先ポート |
| `USER_EXEC_RPC_PROTOCOL_VERSION` | `1` | SOME/IP protocol version |

## 変更ポイント

1. 監視対象イベントに合わせて watchdog timeout/grace を調整する。
2. 監視違反時のポリシー（ログのみ、`Reset()`、プロセス停止）を決める。
3. 本番の EM 接続では instance specifier とネットワーク設定をマニフェストと一致させる。
