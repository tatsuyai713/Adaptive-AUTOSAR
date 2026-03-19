# プラットフォームデーモン リファレンス

本プロジェクトでビルドされる **17 個のプラットフォームデーモン** の詳細リファレンスです。
各デーモンのソースファイル、機能、環境変数、依存関係を記載しています。

---

## 目次

- [アーキテクチャ概要](#アーキテクチャ概要)
- [Tier 0: ミドルウェア基盤](#tier-0-ミドルウェア基盤)
  - [autosar_vsomeip_routing_manager](#autosar_vsomeip_routing_manager)
- [Tier 1: コアプラットフォームサービス](#tier-1-コアプラットフォームサービス)
  - [autosar_dlt_daemon](#autosar_dlt_daemon)
  - [autosar_time_sync_daemon](#autosar_time_sync_daemon)
  - [autosar_persistency_guard](#autosar_persistency_guard)
  - [autosar_iam_policy_loader](#autosar_iam_policy_loader)
  - [autosar_network_manager](#autosar_network_manager)
  - [autosar_can_interface_manager](#autosar_can_interface_manager)
- [Tier 2: プラットフォーム監視・専門サービス](#tier-2-プラットフォーム監視専門サービス)
  - [autosar_sm_state_daemon](#autosar_sm_state_daemon)
  - [autosar_watchdog_supervisor](#autosar_watchdog_supervisor)
  - [autosar_phm_daemon](#autosar_phm_daemon)
  - [autosar_diag_server](#autosar_diag_server)
  - [autosar_crypto_provider](#autosar_crypto_provider)
  - [autosar_ucm_daemon](#autosar_ucm_daemon)
  - [autosar_ntp_time_provider](#autosar_ntp_time_provider)
  - [autosar_ptp_time_provider](#autosar_ptp_time_provider)
- [Tier 3: メインプラットフォームアプリ](#tier-3-メインプラットフォームアプリ)
  - [adaptive_autosar](#adaptive_autosar)
- [Tier 4: ユーザーアプリ管理](#tier-4-ユーザーアプリ管理)
  - [autosar_user_app_monitor](#autosar_user_app_monitor)
- [src/application/ ディレクトリについて](#srcapplication-ディレクトリについて)

---

## アーキテクチャ概要

```
┌──────────────────────────────────────────────────────────────────┐
│                    AUTOSAR Adaptive Platform                      │
│                                                                   │
│  Tier 0   [ vsomeip_routing_manager ]                             │
│                      ↓                                            │
│  Tier 1   [ dlt ] [ time_sync ] [ persistency ] [ iam ] [ nm ]   │
│           [ can_interface_manager ]                               │
│                      ↓                                            │
│  Tier 2   [ sm_state ] [ watchdog ] [ phm ] [ diag ] [ crypto ]  │
│           [ ucm ] [ ntp_time ] [ ptp_time ]                       │
│                      ↓                                            │
│  Tier 3   [ adaptive_autosar ]  ← メインプラットフォームプロセス     │
│                      ↓                                            │
│  Tier 4   [ user_app_monitor ] + bringup.sh                      │
│                      ↓                                            │
│           [ ユーザーアプリケーション群 ]                              │
└──────────────────────────────────────────────────────────────────┘
```

---

## Tier 0: ミドルウェア基盤

### autosar_vsomeip_routing_manager

SOME/IP 通信の中央ルーティングデーモン。全てのSOME/IPアプリケーション間の
メッセージルーティング、サービスディスカバリの中継を行います。

| 項目 | 値 |
|------|-----|
| **ソースファイル** | `src/main_vsomeip_routing_manager.cpp` |
| **systemd サービス** | `autosar-vsomeip-routing.service` |
| **ビルド条件** | `ARA_COM_USE_VSOMEIP=ON` |
| **依存** | iceoryx RouDi (共有メモリ IPC 使用時) |

**機能:**
- vsomeip アプリケーション (`AUTOSAR_VSOMEIP_ROUTING_APP`) を作成し、
  ルーティングマネージャとして起動
- `VSOMEIP_ROUTING` 環境変数を自身のアプリ名に設定し、
  他の vsomeip アプリケーションが自動的にこのプロセスにルーティングを委譲
- SOME/IP Service Discovery (SD) のマルチキャスト応答を中継

**環境変数:**

| 変数 | デフォルト | 説明 |
|------|----------|------|
| `AUTOSAR_VSOMEIP_ROUTING_APP` | `autosar_vsomeip_routing_manager` | vsomeip アプリケーション名 |
| `VSOMEIP_CONFIGURATION` | (JSON パス) | vsomeip 設定ファイルパス |

---

## Tier 1: コアプラットフォームサービス

### autosar_dlt_daemon

AUTOSAR Diagnostic Log and Trace (DLT) プロトコルに準拠したログ収集デーモン。

| 項目 | 値 |
|------|-----|
| **ソースファイル** | `src/main_dlt_daemon.cpp` |
| **systemd サービス** | `autosar-dlt.service` |

**機能:**
- UDP でDLT ログメッセージを受信 (デフォルト ポート 3490)
- ローテーション付きログファイルへ出力
- リモートホストへのログ転送 (オプション)
- アプリ ID / コンテキスト ID 付きの構造化ログ管理

**環境変数:**

| 変数 | デフォルト | 説明 |
|------|----------|------|
| `AUTOSAR_DLT_LISTEN_ADDR` | `0.0.0.0` | UDP リッスンアドレス |
| `AUTOSAR_DLT_LISTEN_PORT` | `3490` | UDP リッスンポート |
| `AUTOSAR_DLT_LOG_FILE` | `/var/log/autosar/dlt.log` | ログファイル出力先 |
| `AUTOSAR_DLT_MAX_FILE_SIZE_KB` | `10240` | ローテーション閾値 (KB) |
| `AUTOSAR_DLT_MAX_ROTATED_FILES` | `5` | 保持するローテーションファイル数 |
| `AUTOSAR_DLT_FORWARD_ENABLED` | `false` | リモート転送を有効化 |
| `AUTOSAR_DLT_FORWARD_HOST` | (空) | 転送先ホスト |
| `AUTOSAR_DLT_FORWARD_PORT` | `3490` | 転送先ポート |
| `AUTOSAR_DLT_STATUS_PERIOD_MS` | `2000` | ステータス書込み間隔 |
| `AUTOSAR_DLT_STATUS_FILE` | `/run/autosar/dlt.status` | ステータスファイル |

---

### autosar_time_sync_daemon

システムクロックとの時刻同期を管理するデーモン。

| 項目 | 値 |
|------|-----|
| **ソースファイル** | `src/main_time_sync_daemon.cpp` |
| **systemd サービス** | `autosar-time-sync.service` |

**機能:**
- `ara::tsync::TimeSyncClient` を生成し、定期的に `UpdateReferenceTime()` を呼出し
- システムクロック (`CLOCK_REALTIME`) とモノトニッククロック (`CLOCK_MONOTONIC`) の
  オフセットを計算
- ステータスファイルに同期状態とオフセット値を記録

**環境変数:**

| 変数 | デフォルト | 説明 |
|------|----------|------|
| `AUTOSAR_TIMESYNC_PERIOD_MS` | `1000` | 同期間隔 (ms) |
| `AUTOSAR_TIMESYNC_STATUS_FILE` | `/run/autosar/time_sync.status` | ステータスファイル |

---

### autosar_persistency_guard

永続ストレージ (`ara::per::KeyValueStorage`) の整合性を保護するデーモン。

| 項目 | 値 |
|------|-----|
| **ソースファイル** | `src/main_persistency_guard.cpp` |
| **systemd サービス** | `autosar-persistency-guard.service` |

**機能:**
- 設定された複数の KVS ストレージスペシファイアを監視
- 定期的にハートビートキー (`__persistency_guard_last_sync_epoch_ms`) を書き込み
- `SyncToStorage()` を呼び出してディスクへの永続化を保証
- 電源断やクラッシュ時のデータ損失を最小化

**環境変数:**

| 変数 | デフォルト | 説明 |
|------|----------|------|
| `AUTOSAR_PERSISTENCY_SPECIFIERS` | `PlatformState,DiagnosticState,ExecutionState` | 監視対象 KVS スペシファイア (カンマ区切り) |
| `AUTOSAR_PERSISTENCY_SYNC_PERIOD_MS` | `5000` | 同期間隔 (ms) |
| `AUTOSAR_PERSISTENCY_STATUS_FILE` | `/run/autosar/persistency_guard.status` | ステータスファイル |

---

### autosar_iam_policy_loader

Identity and Access Management (IAM) ポリシーを CSV ファイルからロードするデーモン。

| 項目 | 値 |
|------|-----|
| **ソースファイル** | `src/main_iam_policy_loader.cpp` |
| **systemd サービス** | `autosar-iam-policy.service` |

**機能:**
- CSV 形式のアクセス制御ポリシーファイルを読み込み
- `ara::iam::AccessControl` にルールを登録
- 定期的にファイルの変更を検知してリロード
- ポリシー形式: `subject,resource,action,allow/deny`

**環境変数:**

| 変数 | デフォルト | 説明 |
|------|----------|------|
| `AUTOSAR_IAM_POLICY_FILE` | `/etc/autosar/iam_policy.csv` | ポリシーファイルパス |
| `AUTOSAR_IAM_RELOAD_PERIOD_MS` | `3000` | リロード間隔 (ms) |
| `AUTOSAR_IAM_STATUS_FILE` | `/run/autosar/iam_policy.status` | ステータスファイル |

---

### autosar_network_manager

AUTOSAR NM (Network Management) ステートマシンを実装するデーモン。

| 項目 | 値 |
|------|-----|
| **ソースファイル** | `src/main_network_manager.cpp` |
| **systemd サービス** | `autosar-network-manager.service` |

**機能:**
- `ara::nm::NetworkManager` を生成し、カンマ区切りの NM チャネルを登録
- NM ステートマシンを駆動:
  `BusSleep → PrepBusSleep → ReadySleep → NormalOperation → RepeatMessage`
- トリガーディレクトリ内のファイル作成でウェイクアップリクエストを検出
- 各チャネルの NM 状態をステータスファイルに記録

**環境変数:**

| 変数 | デフォルト | 説明 |
|------|----------|------|
| `AUTOSAR_NM_CHANNELS` | `can0` | NM チャネル名 (カンマ区切り) |
| `AUTOSAR_NM_TIMEOUT_MS` | `1000` | ノードタイムアウト (ms) |
| `AUTOSAR_NM_REPEAT_MSG_TIME_MS` | `200` | リピートメッセージ時間 (ms) |
| `AUTOSAR_NM_WAIT_BUS_SLEEP_MS` | `3000` | バススリープ待機時間 (ms) |
| `AUTOSAR_NM_TRIGGER_DIR` | `/run/autosar/nm_triggers` | ウェイクアップトリガーディレクトリ |
| `AUTOSAR_NM_PERIOD_MS` | `100` | ティック間隔 (ms) |
| `AUTOSAR_NM_STATUS_FILE` | `/run/autosar/nm.status` | ステータスファイル |

---

### autosar_can_interface_manager

CAN バスインターフェースの監視と自動復旧を行うデーモン。

| 項目 | 値 |
|------|-----|
| **ソースファイル** | `src/main_can_interface_manager.cpp` |
| **systemd サービス** | `autosar-can-manager.service` |

**機能:**
- `/sys/class/net/<ifname>/operstate` を定期的にポーリングして CAN インターフェースの状態を監視
- インターフェースがダウンした場合、設定されたビットレートで自動再設定:
  `ip link set <ifname> type can bitrate <N>` → `ip link set <ifname> up`
- CAN bus-off エラーからの自動復旧

**環境変数:**

| 変数 | デフォルト | 説明 |
|------|----------|------|
| `AUTOSAR_CAN_IFNAME` | `can0` | 監視対象 CAN インターフェース名 |
| `AUTOSAR_CAN_BITRATE` | `500000` | 再設定時ビットレート (bps) |
| `AUTOSAR_CAN_RECONFIGURE_ON_DOWN` | `true` | ダウン時自動再設定 |
| `AUTOSAR_CAN_MONITOR_PERIOD_MS` | `2000` | 監視間隔 (ms) |
| `AUTOSAR_CAN_MANAGER_STATUS_FILE` | `/run/autosar/can_manager.status` | ステータスファイル |

---

## Tier 2: プラットフォーム監視・専門サービス

### autosar_sm_state_daemon

AUTOSAR State Management (SM) のマシン状態とネットワーク通信モードを管理するデーモン。

| 項目 | 値 |
|------|-----|
| **ソースファイル** | `src/main_sm_state_daemon.cpp` |
| **systemd サービス** | `autosar-sm-state.service` |

**機能:**
- `ara::sm::MachineStateClient` で `kRunning` 状態に遷移
- `ara::sm::NetworkHandle` で通信モード `kFull` をリクエスト
- `ara::sm::StateTransitionHandler` でリモート状態変更要求を受付
- シャットダウン時に `kShutdown` へ遷移、通信モードを `kNone` に変更

**環境変数:**

| 変数 | デフォルト | 説明 |
|------|----------|------|
| `AUTOSAR_SM_NETWORK_INSTANCE` | `CanSM_CAN0` | NetworkHandle インスタンス識別子 |
| `AUTOSAR_SM_PERIOD_MS` | `1000` | ステータス書込み間隔 (ms) |
| `AUTOSAR_SM_STATUS_FILE` | `/run/autosar/sm_state.status` | ステータスファイル |

---

### autosar_watchdog_supervisor

ハードウェアウォッチドッグタイマーのキック管理を行うデーモン。

| 項目 | 値 |
|------|-----|
| **ソースファイル** | `src/main_watchdog_supervisor.cpp` |
| **systemd サービス** | `autosar-watchdog.service` |

**機能:**
- Linux `/dev/watchdog` デバイスをオープンし、定期的にキープアライブ書込み
- ウォッチドッグデバイスが利用不可の場合、ソフトウェアモードにフォールバック
- シャットダウン時にマジックバイト `'V'` を書き込みクリーンクローズ
- ハートビートファイルへのタイムスタンプ記録

**環境変数:**

| 変数 | デフォルト | 説明 |
|------|----------|------|
| `AUTOSAR_WATCHDOG_DEVICE` | `/dev/watchdog` | ウォッチドッグデバイスパス |
| `AUTOSAR_WATCHDOG_INTERVAL_MS` | `1000` | キープアライブ間隔 (ms) |
| `AUTOSAR_WATCHDOG_ALLOW_SOFT_MODE` | `false` | ソフトモードフォールバック許可 |
| `AUTOSAR_WATCHDOG_HEARTBEAT_FILE` | `/run/autosar/watchdog_heartbeat` | ハートビートファイル |
| `AUTOSAR_WATCHDOG_STATUS_FILE` | `/run/autosar/watchdog.status` | ステータスファイル |

---

### autosar_phm_daemon

Platform Health Management (PHM) のオーケストレータデーモン。

| 項目 | 値 |
|------|-----|
| **ソースファイル** | `src/main_phm_daemon.cpp` |
| **systemd サービス** | `autosar-phm-daemon.service` |

**機能:**
- `ara::phm::PhmOrchestrator` を生成し、設定されたエンティティ群を監視対象に登録
- 定期的に `EvaluatePlatformHealth()` を呼び出して全体の健全性を評価
- 各エンティティの状態 (Ok / Failed / Expired / Deactivated) を個別に追跡
- 障害率に基づいてプラットフォーム全体の健全性レベルを判定:
  - **Normal**: 障害率 < `DEGRADED_THRESHOLD`
  - **Degraded**: 障害率 < `CRITICAL_THRESHOLD`
  - **Critical**: 障害率 >= `CRITICAL_THRESHOLD`

**環境変数:**

| 変数 | デフォルト | 説明 |
|------|----------|------|
| `AUTOSAR_PHM_PERIOD_MS` | `1000` | 評価間隔 (ms) |
| `AUTOSAR_PHM_DEGRADED_THRESHOLD` | `0.0` | Degraded 判定の障害率閾値 |
| `AUTOSAR_PHM_CRITICAL_THRESHOLD` | `0.5` | Critical 判定の障害率閾値 |
| `AUTOSAR_PHM_ENTITIES` | `platform_app,vsomeip_routing,...` | 監視対象エンティティ (カンマ区切り) |
| `AUTOSAR_PHM_STATUS_FILE` | `/run/autosar/phm.status` | ステータスファイル |

---

### autosar_diag_server

UDS (Unified Diagnostic Services) / DoIP (Diagnostics over IP) 準拠の診断サーバデーモン。

| 項目 | 値 |
|------|-----|
| **ソースファイル** | `src/main_diag_server.cpp` |
| **systemd サービス** | `autosar-diag-server.service` |

**機能:**
- TCP ソケットで DoIP 診断リクエストを待受 (デフォルト ポート 13400)
- 15 種類の標準 UDS サービスを `ara::diag::DiagnosticManager` に登録して処理
- リクエスト数、接続数、Positive/Negative レスポンス数を追跡
- P2 / P2* サーバタイミング制御

**対応 UDS サービス:**
`0x10` DiagnosticSessionControl, `0x11` ECUReset, `0x14` ClearDTC,
`0x19` ReadDTCInformation, `0x22` ReadDataByIdentifier, `0x23` ReadMemoryByAddress,
`0x27` SecurityAccess, `0x2E` WriteDataByIdentifier, `0x2F` IOControl,
`0x31` RoutineControl, `0x34` RequestDownload, `0x35` RequestUpload,
`0x36` TransferData, `0x37` RequestTransferExit, `0x3E` TesterPresent

**環境変数:**

| 変数 | デフォルト | 説明 |
|------|----------|------|
| `AUTOSAR_DIAG_LISTEN_ADDR` | `0.0.0.0` | TCP リッスンアドレス |
| `AUTOSAR_DIAG_LISTEN_PORT` | `13400` | TCP リッスンポート (DoIP) |
| `AUTOSAR_DIAG_P2_SERVER_MS` | `50` | P2 サーバタイミング (ms) |
| `AUTOSAR_DIAG_P2STAR_SERVER_MS` | `5000` | P2* サーバタイミング (ms) |
| `AUTOSAR_DIAG_STATUS_PERIOD_MS` | `2000` | ステータス書込み間隔 (ms) |
| `AUTOSAR_DIAG_STATUS_FILE` | `/run/autosar/diag_server.status` | ステータスファイル |

---

### autosar_crypto_provider

暗号サービスプロバイダデーモン。OpenSSL ベースの HSM バックエンドを提供。

| 項目 | 値 |
|------|-----|
| **ソースファイル** | `src/main_crypto_provider.cpp` |
| **systemd サービス** | `autosar-crypto-provider.service` |

**機能:**
- `ara::crypto::HsmProvider` を初期化 (AES-128-CBC + HMAC-SHA256)
- `platform_aes` / `platform_hmac` プラットフォームキーを事前生成
- 定期的な暗号セルフテスト (鍵生成 → 暗号化 → 復号 → 検証)
- オプションの定期キーローテーション

**環境変数:**

| 変数 | デフォルト | 説明 |
|------|----------|------|
| `AUTOSAR_CRYPTO_TOKEN` | `AutosarEcuHsm` | HSM トークンラベル |
| `AUTOSAR_CRYPTO_PERIOD_MS` | `5000` | ステータス書込み間隔 (ms) |
| `AUTOSAR_CRYPTO_SELF_TEST` | `true` | セルフテスト有効化 |
| `AUTOSAR_CRYPTO_SELF_TEST_INTERVAL_MS` | `60000` | セルフテスト間隔 (ms) |
| `AUTOSAR_CRYPTO_KEY_ROTATION` | `false` | キーローテーション有効化 |
| `AUTOSAR_CRYPTO_ROTATION_INTERVAL_MS` | `3600000` | ローテーション間隔 (1 時間) |
| `AUTOSAR_CRYPTO_KEY_STORAGE_PATH` | `/var/lib/autosar/crypto/keys` | 鍵ストレージパス |
| `AUTOSAR_CRYPTO_STATUS_FILE` | `/run/autosar/crypto_provider.status` | ステータスファイル |

---

### autosar_ucm_daemon

Update and Configuration Management (UCM) デーモン。ソフトウェアパッケージの
ステージング → 検証 → 活性化パイプラインを実行。

| 項目 | 値 |
|------|-----|
| **ソースファイル** | `src/main_ucm_daemon.cpp` |
| **systemd サービス** | `autosar-ucm.service` |

**機能:**
- ステージングディレクトリを定期スキャンし、`*.manifest` ファイルを検出
- マニフェスト形式: パッケージ名、バージョン、ターゲットクラスタ、ペイロードファイル、SHA-256 ダイジェスト
- UCM パイプライン実行:
  `PrepareUpdate → StageSoftwarePackage → VerifyStagedSoftwarePackage → ActivateSoftwarePackage`
- 処理済みマニフェストを `.done` または `.failed` にリネーム

**環境変数:**

| 変数 | デフォルト | 説明 |
|------|----------|------|
| `AUTOSAR_UCM_STAGING_DIR` | `/var/lib/autosar/ucm/staging` | マニフェストスキャンディレクトリ |
| `AUTOSAR_UCM_PROCESSED_DIR` | `/var/lib/autosar/ucm/processed` | 処理済みファイル移動先 |
| `AUTOSAR_UCM_AUTO_ACTIVATE` | `true` | 検証後自動活性化 |
| `AUTOSAR_UCM_SCAN_PERIOD_MS` | `2000` | スキャン間隔 (ms) |
| `AUTOSAR_UCM_STATUS_FILE` | `/run/autosar/ucm.status` | ステータスファイル |

---

### autosar_ntp_time_provider

NTP (Network Time Protocol) ベースの時刻プロバイダデーモン。

| 項目 | 値 |
|------|-----|
| **ソースファイル** | `src/main_ntp_time_provider.cpp` |
| **systemd サービス** | `autosar-ntp-time-provider.service` |

**機能:**
- `ara::tsync::NtpTimeBaseProvider` を使用して NTP 時刻を取得
- chrony / ntpd / 自動検出をサポート
- `TimeSyncClient` に取得した時刻を定期的にフィード

**環境変数:**

| 変数 | デフォルト | 説明 |
|------|----------|------|
| `AUTOSAR_NTP_DAEMON` | `auto` | NTP デーモン種別: `chrony`, `ntpd`, `auto` |
| `AUTOSAR_NTP_PERIOD_MS` | `1000` | ポーリング間隔 (ms) |
| `AUTOSAR_NTP_STATUS_FILE` | `/run/autosar/ntp.status` | ステータスファイル |

---

### autosar_ptp_time_provider

PTP (Precision Time Protocol) ベースの時刻プロバイダデーモン。

| 項目 | 値 |
|------|-----|
| **ソースファイル** | `src/main_ptp_time_provider.cpp` |
| **systemd サービス** | `autosar-ptp-time-provider.service` |

**機能:**
- `ara::tsync::PtpTimeBaseProvider` を使用して PTP ハードウェアクロックデバイスから時刻を取得
- `/dev/ptp0` (デフォルト) の PTP クロックを読み取り
- QNX 環境では `/tmp/autosar/` にステータスを出力

**環境変数:**

| 変数 | デフォルト | 説明 |
|------|----------|------|
| `AUTOSAR_PTP_DEVICE` | `/dev/ptp0` | PTP クロックデバイスパス |
| `AUTOSAR_PTP_PERIOD_MS` | `500` | ポーリング間隔 (ms) |
| `AUTOSAR_PTP_STATUS_FILE` | `/run/autosar/ptp.status` | ステータスファイル |

---

## Tier 3: メインプラットフォームアプリ

### adaptive_autosar

AUTOSAR Adaptive Platform のメインプロセス。Execution Manager (EM)、
State Management (SM)、Platform Health Management (PHM)、Diagnostic Manager、
ExtendedVehicle を統合した中核プロセスです。

| 項目 | 値 |
|------|-----|
| **ソースファイル** | `src/main.cpp` + `src/application/` ディレクトリ全体 |
| **systemd サービス** | `autosar-platform-app.service` |
| **ビルド条件** | `AUTOSAR_AP_BUILD_PLATFORM_APP=ON` |
| **依存** | vsomeip routing, time_sync, persistency, iam |

**機能:**
- 4 つの ARXML マニフェストを読み込み:
  1. `execution_manifest.arxml` — EM 設定 (RPC エンドポイント、Function Group 定義)
  2. `extended_vehicle_manifest.arxml` — ExtendedVehicle 設定
  3. `diagnostic_manager_manifest.arxml` — 診断マネージャ設定
  4. `health_monitoring_manifest.arxml` — PHM 監視設定
- SOME/IP RPC サーバ (`SocketRpcServer`) を起動
- `ExecutionServer` / `StateServer` を生成
- MachineFG (`Off` → `StartUp`) の Function Group 状態遷移を管理
- `StartUp` 状態で PHM / DiagnosticManager / ExtendedVehicle を初期化
- DoIP サーバ / SOME/IP SD サーバの起動
- ソケットポーリングのイベントループを別スレッドで駆動

**コマンドライン引数:**

```bash
adaptive_autosar <exec_manifest> <ev_manifest> <dm_manifest> <phm_manifest>
```

引数を省略した場合、`../../configuration/*.arxml` からの相対パスをデフォルト使用。

**使用例:**

```bash
# 明示的なマニフェスト指定
/opt/autosar-ap/bin/adaptive_autosar \
  /opt/autosar-ap/configuration/execution_manifest.arxml \
  /opt/autosar-ap/configuration/extended_vehicle_manifest.arxml \
  /opt/autosar-ap/configuration/diagnostic_manager_manifest.arxml \
  /opt/autosar-ap/configuration/health_monitoring_manifest.arxml

# TTY 接続時は Enter キーで終了、非 TTY 時は SIGTERM/SIGINT で終了
```

---

## Tier 4: ユーザーアプリ管理

### autosar_user_app_monitor

ユーザーアプリケーションの健全性監視と自動復旧を行うデーモン。
17 デーモンの中で最も複雑なロジックを持ちます。

| 項目 | 値 |
|------|-----|
| **ソースファイル** | `src/main_user_app_monitor.cpp` |
| **systemd サービス** | `autosar-user-app-monitor.service` |
| **依存** | adaptive_autosar (platform-app), exec-manager (bringup.sh) |

**機能:**
- CSV レジストリファイルからユーザーアプリ一覧を読み込み
- 3 種類のヘルスチェック:
  1. **プロセス生存チェック**: `kill(pid, 0)` + `/proc/<pid>/stat` ゾンビ検出
  2. **ハートビート鮮度チェック**: ハートビートファイルの `stat()` mtime が
     タイムアウト内に更新されているか
  3. **PHM ヘルスステータスチェック**: インスタンスごとの `.status` ファイルを読み取り
- 異常検出時のアクション:
  - プロセス終了 (`SIGTERM` → `SIGKILL`)
  - 自動再起動 (`fork/exec`)
  - 再起動レート制限 (スライディングウィンドウ方式)
  - 再起動バックオフ

**環境変数:**

| 変数 | デフォルト | 説明 |
|------|----------|------|
| `AUTOSAR_USER_APP_REGISTRY_FILE` | `/run/autosar/user_apps_registry.csv` | アプリレジストリ CSV |
| `AUTOSAR_USER_APP_MONITOR_PERIOD_MS` | `1000` | チェック間隔 (ms) |
| `AUTOSAR_USER_APP_MONITOR_ENFORCE_HEALTH` | `true` | 健全性強制 (kill+restart) |
| `AUTOSAR_USER_APP_MONITOR_RESTART_ON_FAILURE` | `true` | 異常時自動再起動 |
| `AUTOSAR_USER_APP_MONITOR_RESTART_BACKOFF_MS` | `5000` | 再起動バックオフ (ms) |
| `AUTOSAR_PHM_HEALTH_DIR` | `/run/autosar` | PHM ステータスディレクトリ |
| `AUTOSAR_USER_APP_MONITOR_STATUS_FILE` | `/run/autosar/user_app_monitor.status` | ステータスファイル |

---

## src/application/ ディレクトリについて

`src/application/` ディレクトリは `adaptive_autosar` バイナリ (= `src/main.cpp`)
**専用のアプリケーションコード**です。AUTOSAR Adaptive Platform のメインプラットフォームプロセスを
構成する不可欠な要素であり、以下の理由から削除はできません:

- `ExecutionManagement` — ARXML からの EM/SM 統合、RPC サーバ、Function Group 管理
- `StateManagement` — MachineFG 状態遷移、StateClient
- `PlatformHealthManagement` — FIFO チェックポイント、AliveSupervision、DeadlineSupervision
- `DiagnosticManager` — OBD-II エミュレータ、SOME/IP SD、DoIP ブリッジ
- `ExtendedVehicle` — DoIP/SOME/IP サービスオファリング、PHM チェックポイント報告

このディレクトリは **ユーザーアプリではなくプラットフォームアプリ**です。
ユーザーアプリは `user_apps/src/apps/` に配置されます。
