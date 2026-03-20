# 30: プラットフォームサービスアーキテクチャ

## 概要

このチュートリアルでは、Adaptive AUTOSAR ランタイムを構成する
**17 のプラットフォームデーモンバイナリ** と **20 の systemd サービス** を解説します。
Raspberry Pi ECU への配備やプロダクション向けユーザーアプリの開発に先立ち、
このアーキテクチャの理解が不可欠です。

## 4 階層サービスアーキテクチャ

```
┌──────────────────────── Raspberry Pi ECU ────────────────────────┐
│                                                                   │
│  [Tier 0 — 通信基盤]                                              │
│    iceoryx RouDi ─────── ゼロコピー共有メモリ                      │
│    vSomeIP Routing ───── SOME/IP サービスディスカバリ               │
│                                                                   │
│  [Tier 1 — プラットフォーム基盤サービス]                            │
│    Time Sync ────── 時刻同期デーモン                               │
│    NTP Provider ─── NTP クロックソース                              │
│    PTP Provider ─── PTP クロックソース                              │
│    Persistency ──── データ永続化ガード                              │
│    IAM Policy ───── ID・アクセス制御                                │
│    SM State ─────── 状態管理                                       │
│    CAN Manager ──── CAN バスインターフェース管理                    │
│    DLT Daemon ───── 診断ログ・トレース                              │
│    Network Mgr ──── ネットワーク管理                                │
│    Crypto Provider ─ HSM 鍵管理・暗号処理                           │
│                                                                   │
│  [Tier 2 — プラットフォームアプリケーション]                         │
│    adaptive_autosar ── EM/SM/PHM/Diag/Vehicle 統合バイナリ          │
│    Diag Server ─────── UDS/DoIP 診断エンドポイント                  │
│    PHM Daemon ──────── プラットフォームヘルス管理オーケストレータ     │
│    UCM Daemon ──────── 更新・構成管理                               │
│                                                                   │
│  [Tier 3 — 実行管理・監視]                                         │
│    Exec Manager ──── bringup.sh 経由でユーザーアプリを起動            │
│    User App Monitor ─ PID / ハートビート / PHM 監視                  │
│    Watchdog ──────── ハードウェア WDT キック                         │
│                                                                   │
│  [ユーザーアプリケーション]                                          │
│    raspi_ecu_app, ecu_full_stack, カスタムアプリ …                   │
└───────────────────────────────────────────────────────────────────┘
```

## デーモン一覧

| # | バイナリ | ソース | 説明 |
|---|---------|--------|------|
| 1 | `adaptive_autosar` | `main.cpp` | オールインワンプラットフォームバイナリ (EM, SM, PHM, Diag, Vehicle) |
| 2 | `autosar_vsomeip_routing_manager` | `main_vsomeip_routing_manager.cpp` | vSomeIP ルーティングデーモン |
| 3 | `autosar_time_sync_daemon` | `main_time_sync_daemon.cpp` | 時刻同期 |
| 4 | `autosar_ntp_time_provider` | `main_ntp_time_provider.cpp` | NTP クロックソース |
| 5 | `autosar_ptp_time_provider` | `main_ptp_time_provider.cpp` | PTP クロックソース |
| 6 | `autosar_persistency_guard` | `main_persistency_guard.cpp` | 永続データ保護 |
| 7 | `autosar_iam_policy_loader` | `main_iam_policy_loader.cpp` | IAM ポリシー適用 |
| 8 | `autosar_sm_state_daemon` | `main_sm_state_daemon.cpp` | マシン状態管理 |
| 9 | `autosar_can_interface_manager` | `main_can_interface_manager.cpp` | CAN バス管理 |
| 10 | `autosar_dlt_daemon` | `main_dlt_daemon.cpp` | 診断ログ |
| 11 | `autosar_network_manager` | `main_network_manager.cpp` | ネットワーク管理 |
| 12 | `autosar_crypto_provider` | `main_crypto_provider.cpp` | HSM・暗号処理 |
| 13 | `autosar_diag_server` | `main_diag_server.cpp` | UDS/DoIP 診断サーバー |
| 14 | `autosar_phm_daemon` | `main_phm_daemon.cpp` | プラットフォームヘルスオーケストレータ |
| 15 | `autosar_ucm_daemon` | `main_ucm_daemon.cpp` | 更新・構成管理 |
| 16 | `autosar_user_app_monitor` | `main_user_app_monitor.cpp` | ユーザーアプリ PID / ハートビート監視 |
| 17 | `autosar_watchdog_supervisor` | `main_watchdog_supervisor.cpp` | ハードウェアウォッチドッグキック |

## systemd サービスマップ

20 の systemd サービスがインストールされます。
サービスファイルはすべて `deployment/rpi_ecu/systemd/` にあります。

| サービス | ユニットファイル | Tier | After | Before |
|---------|-----------------|------|-------|--------|
| `autosar-iox-roudi` | 外部 iceoryx | 0 | — | 全体 |
| `autosar-vsomeip-routing` | `autosar-vsomeip-routing.service` | 0 | roudi | platform-app |
| `autosar-time-sync` | `autosar-time-sync.service` | 1 | roudi, vsomeip | platform-app |
| `autosar-ntp-time-provider` | `autosar-ntp-time-provider.service` | 1 | time-sync | platform-app |
| `autosar-ptp-time-provider` | `autosar-ptp-time-provider.service` | 1 | time-sync | platform-app |
| `autosar-persistency-guard` | `autosar-persistency-guard.service` | 1 | local-fs | platform-app |
| `autosar-iam-policy` | `autosar-iam-policy.service` | 1 | local-fs | platform-app |
| `autosar-sm-state` | `autosar-sm-state.service` | 1 | vsomeip | platform-app |
| `autosar-can-manager` | `autosar-can-manager.service` | 1 | network | platform-app |
| `autosar-dlt` | `autosar-dlt.service` | 1 | local-fs | platform-app |
| `autosar-network-manager` | `autosar-network-manager.service` | 1 | network | platform-app |
| `autosar-crypto-provider` | `autosar-crypto-provider.service` | 1 | local-fs | platform-app |
| `autosar-platform-app` | `autosar-platform-app.service` | 2 | Tier 1 | exec-manager |
| `autosar-diag-server` | `autosar-diag-server.service` | 2 | network, platform-app | exec-manager |
| `autosar-phm-daemon` | `autosar-phm-daemon.service` | 2 | platform-app | exec-manager |
| `autosar-ucm` | `autosar-ucm.service` | 2 | platform-app | exec-manager |
| `autosar-exec-manager` | `autosar-exec-manager.service` | 3 | Tier 2 | — |
| `autosar-user-app-monitor` | `autosar-user-app-monitor.service` | 3 | exec-manager | — |
| `autosar-watchdog` | `autosar-watchdog.service` | 3 | platform-app | — |
| `autosar-ecu-full-stack` | `autosar-ecu-full-stack.service` | App | roudi, vsomeip | — |

## サービス起動順序

```
                    ┌─ iox-roudi ─────────┐
                    │                     │
                    ├─ vsomeip-routing ───┤
                    │                     │
Tier 1 起動 ───────┤  time-sync          │
(並列)             │  ntp-provider       │
                    │  ptp-provider       │
                    │  persistency-guard  │
                    │  iam-policy         │
                    │  sm-state           │
                    │  can-manager        │
                    │  dlt               │
                    │  network-manager    │
                    │  crypto-provider    │
                    │                     │
Tier 2 起動 ───────┤  platform-app       │
                    │  diag-server        │
                    │  phm-daemon         │
                    │  ucm               │
                    │                     │
Tier 3 起動 ───────┤  exec-manager       │
                    │  user-app-monitor   │
                    │  watchdog           │
                    └─────────────────────┘
```

## ステータスファイル

各デーモンは `/run/autosar/<name>.status` に定期的にステータスを書き出します:

```bash
# 稼働中の全デーモンのステータスを確認
for f in /run/autosar/*.status; do
  echo "=== $(basename "$f") ==="
  cat "$f"
  echo
done
```

## 環境変数による設定

各サービスは `/etc/default/autosar-<name>` から環境変数を読み込みます。
これらのファイルはインストール時に `deployment/rpi_ecu/env/autosar-<name>.env.in`
から生成されます。

共通パターン:

```bash
# /etc/default/autosar-diag-server  (例)
AUTOSAR_DIAG_LISTEN_ADDR=0.0.0.0
AUTOSAR_DIAG_LISTEN_PORT=13400
AUTOSAR_DIAG_P2_SERVER_MS=50
AUTOSAR_DIAG_STATUS_PERIOD_MS=2000
```

## 最小構成プロファイル

20 サービスすべてが必須ではありません。最小限の ECU 配備ではこれだけで十分です:

| 用途 | 有効化するサービス |
|------|------------------|
| IPC 基盤 | `autosar-iox-roudi` (常時) |
| SOME/IP | `autosar-vsomeip-routing` |
| プラットフォームコア | `autosar-platform-app` |
| 診断 | `autosar-diag-server` |
| ヘルス | `autosar-phm-daemon` |
| 永続化 | `autosar-persistency-guard` |
| 状態管理 | `autosar-sm-state` |

完全な最小構成ガイドは [51_rpi_minimal_ecu_user_app_management](51_rpi_minimal_ecu_user_app_management.ja.md)
を参照してください。

## 次のステップ

| チュートリアル | トピック |
|---------------|---------|
| [31_diag_server](31_diag_server.ja.md) | 診断サーバーデーモンの詳細 |
| [32_phm_daemon](32_phm_daemon.ja.md) | プラットフォームヘルス管理デーモン |
| [33_crypto_provider](33_crypto_provider.ja.md) | 暗号プロバイダデーモン |
| [50_rpi_ecu_deployment](50_rpi_ecu_deployment.ja.md) | RPi への全サービス配備 |
