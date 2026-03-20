# 22: プロダクション ECU アプリケーション

## 対象

`autosar_user_raspi_ecu` — ソース: `user_apps/src/apps/basic/raspi_ecu_app.cpp`

## 目的

このチュートリアルでは、プラットフォーム機能を専用デーモンに委譲する
**プロダクションスタイル** の ECU アプリケーションを解説します。

モノリシックな `raspi_mock_ecu_app`（診断・PHM・暗号ロジックを単一バイナリに内包）
とは異なり、`raspi_ecu_app` は以下のみを担当する **薄い車両アプリ** です:

1. **ライフサイクル管理** (`ara::exec`)
2. **PHM デーモンへのヘルス報告** (`ara::phm`)
3. **永続カウンタ/設定** (`ara::per`)
4. **CAN / vCAN からの車両センサーデータ取得**
5. **DLT ログとステータスファイルによるテレメトリ公開**
6. **マシン状態遷移への応答** (`ara::sm`)

プラットフォームサービス（診断サーバー、PHM オーケストレータ、暗号プロバイダ、
UCM、NM、時刻同期）はすべて別の systemd デーモンとして動作します。

## アーキテクチャ比較

```
  ┌─ raspi_mock_ecu_app (モノリシック) ─┐   ┌─ raspi_ecu_app (プロダクション) ─┐
  │  Diag Server (内蔵)                │   │  薄い車両ロジックのみ             │
  │  PHM Orchestrator (内蔵)           │   │  → ara::exec ライフサイクル        │
  │  Crypto Self-Test (内蔵)           │   │  → ara::phm ヘルス報告            │
  │  Vehicle Logic                     │   │  → ara::per 永続化               │
  │  CAN / Telemetry                   │   │  → ara::sm 状態遷移応答           │
  │  Persistence                       │   │  → センサー読取 + テレメトリ       │
  └────────────────────────────────────┘   └──────────────────────────────────┘
                                                       ▲  ▲  ▲
                                                       │  │  │
                                autosar_diag_server ───┘  │  │
                                autosar_phm_daemon ───────┘  │
                                autosar_crypto_provider ──────┘
```

## 使用する AUTOSAR API

| API | 用途 |
|-----|------|
| `ara::core::Initialize()` / `Deinitialize()` | ランタイムライフサイクル |
| `ara::exec::ApplicationClient` | 実行状態報告、リカバリ要求 |
| `ara::exec::SignalHandler` | SIGTERM / SIGINT ハンドリング |
| `ara::phm::HealthChannel` | ヘルスステータス報告 (kNormal / kFailed) |
| `ara::per::OpenKeyValueStorage()` | 永続ブート & サイクルカウンタ |
| `ara::sm::MachineStateClient` | マシン状態遷移への応答 |
| `ara::log::LoggingFramework` | 構造化 DLT ログ |
| `ara::core::InstanceSpecifier` | サービスポート識別子 |

## マシン状態

アプリは `ara::sm::MachineStateClient` 経由で以下のマシン状態に応答します:

| 状態 | アクション |
|------|-----------|
| `kStartup` | センサー初期化、永続ストレージ開放 |
| `kRunning` | 通常のセンサー → テレメトリループ |
| `kShutdown` | 永続データフラッシュ、クリーン終了 |
| `kRestart` | フラッシュしてリスタート要求 |
| `kSuspend` | テレメトリループ一時停止 |
| `kDiagnostic` | 診断セッション用にサイクルレート低減 |
| `kUpdate` | 一時停止して UCM 完了待機 |

## 環境変数

| 変数 | デフォルト | 説明 |
|------|----------|------|
| `AUTOSAR_ECU_PERIOD_MS` | `100` | メインループサイクル (ms) |
| `AUTOSAR_ECU_LOG_EVERY` | `50` | N サイクルごとにログ出力 |
| `AUTOSAR_ECU_PERSIST_EVERY` | `100` | N サイクルごとに永続化 |
| `AUTOSAR_ECU_STATUS_FILE` | `/run/autosar/ecu_app.status` | ステータス出力 |
| `AUTOSAR_ECU_CAN_IFNAME` | `vcan0` | CAN インターフェース名 |

## ビルド

```bash
# インストール済みランタイムからユーザーアプリをビルド
./scripts/build_user_apps_from_opt.sh --prefix /opt/autosar-ap

# バイナリ確認
ls build-user-apps-opt/src/apps/basic/autosar_user_raspi_ecu
```

## 実行

```bash
# 仮想 CAN をセットアップ（必要な場合）
sudo ./scripts/setup_socketcan_interface.sh --ifname vcan0 --vcan

# プロダクション ECU アプリを実行
AUTOSAR_ECU_PERIOD_MS=200 \
AUTOSAR_ECU_CAN_IFNAME=vcan0 \
  ./build-user-apps-opt/src/apps/basic/autosar_user_raspi_ecu
```

## ステータスファイル

アプリは `/run/autosar/ecu_app.status` に書き出します:

```
cycle=1500
boot_count=7
machine_state=kRunning
health_reports=1500
speed_centi_kph=8350
engine_rpm=3200
steering_centi_deg=-450
gear=3
coolant_temp_c=92
battery_voltage=138
updated_epoch_ms=1717171717000
```

## 永続データ

ブートカウントとサイクルカウントは再起動後も保持されます:

```cpp
ara::per::OpenKeyValueStorage(spec);
// キー: "raspi_ecu.boot_count", "raspi_ecu.cycle_count"
```

## systemd サービスとしてのデプロイ

RPi 配備時、このアプリは 17 のプラットフォームデーモンと並行して動作します。
完全な配備手順はチュートリアル [50_rpi_ecu_deployment](50_rpi_ecu_deployment.ja.md)
を参照してください。

## 次のステップ

| チュートリアル | トピック |
|---------------|---------|
| [21_ecu_full_stack](21_ecu_full_stack.ja.md) | フルスタック統合 (CAN+SOME/IP→DDS) |
| [30_platform_service_architecture](30_platform_service_architecture.ja.md) | プラットフォームデーモン概要 |
| [50_rpi_ecu_deployment](50_rpi_ecu_deployment.ja.md) | RPi 配備 |
