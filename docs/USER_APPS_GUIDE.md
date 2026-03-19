# ユーザーアプリケーション ガイド

`user_apps/src/apps/` に含まれる全ユーザーアプリケーションの分類と詳細解説です。
各アプリの機能、使用する ara::* モジュール、ビルド条件、実行方法を記載しています。

---

## 目次

- [分類概要](#分類概要)
- [1. basic — 基本テンプレート](#1-basic--基本テンプレート)
  - [minimal_runtime_app](#minimal_runtime_app)
  - [exec_signal_app](#exec_signal_app)
  - [per_phm_app](#per_phm_app)
  - [raspi_ecu_app](#raspi_ecu_app)
  - [raspi_mock_ecu_app](#raspi_mock_ecu_app)
- [2. feature — 機能別デモ](#2-feature--機能別デモ)
  - [2.1 runtime — ランタイム・ライフサイクル](#21-runtime--ランタイムライフサイクル)
    - [runtime_lifecycle_app](#runtime_lifecycle_app)
    - [exec_runtime_monitor_app](#exec_runtime_monitor_app)
  - [2.2 em — Execution Manager 連携](#22-em--execution-manager-連携)
    - [em_daemon_app](#em_daemon_app)
    - [em_sensor_app](#em_sensor_app)
    - [em_actuator_app](#em_actuator_app)
  - [2.3 ecu — ECU 統合](#23-ecu--ecu-統合)
    - [ecu_someip_source_app](#ecu_someip_source_app)
    - [ecu_full_stack_app](#ecu_full_stack_app)
  - [2.4 can — CAN バス](#24-can--can-バス)
    - [socketcan_receive_decode_app](#socketcan_receive_decode_app)
- [3. communication — 通信テンプレート](#3-communication--通信テンプレート)
  - [3.1 someip — SOME/IP](#31-someip--someip)
    - [provider_app (SOME/IP)](#provider_app-someip)
    - [consumer_app (SOME/IP)](#consumer_app-someip)
  - [3.2 dds — DDS](#32-dds--dds)
    - [pub_app (DDS)](#pub_app-dds)
    - [sub_app (DDS)](#sub_app-dds)
  - [3.3 zerocopy — ゼロコピー共有メモリ](#33-zerocopy--ゼロコピー共有メモリ)
    - [pub_app (Zero-Copy)](#pub_app-zero-copy)
    - [sub_app (Zero-Copy)](#sub_app-zero-copy)
  - [3.4 switchable_pubsub — 切替可能トランスポート](#34-switchable_pubsub--切替可能トランスポート)
    - [switchable_pub](#switchable_pub)
    - [switchable_sub](#switchable_sub)
- [4. samples — ara::com API デモ](#4-samples--aracom-api-デモ)
  - [01_typed_method_rpc](#01_typed_method_rpc)
  - [02_fire_and_forget](#02_fire_and_forget)
  - [03_communication_group](#03_communication_group)
  - [04_field_get_set](#04_field_get_set)
  - [05_event_pubsub](#05_event_pubsub)
- [ビルド方法](#ビルド方法)
- [アプリ選定ガイド](#アプリ選定ガイド)

---

## 分類概要

```
user_apps/src/apps/
├── basic/                      ← 基本テンプレート (ミドルウェア不要)
│   ├── minimal_runtime_app     ← 最小構成デモ
│   ├── exec_signal_app         ← シグナルハンドリング + ライフサイクル
│   ├── per_phm_app             ← 永続化 + PHM ヘルスチェック
│   ├── raspi_ecu_app           ← 本番 ECU アプリ
│   └── raspi_mock_ecu_app      ← 全モジュールモック ECU
│
├── feature/                    ← 機能別デモ
│   ├── runtime/                ← ランタイム監視
│   │   ├── runtime_lifecycle   ← 基本ライフサイクル
│   │   └── exec_runtime_monitor← Alive Supervision モニタ
│   ├── em/                     ← Execution Manager
│   │   ├── em_daemon_app       ← EM オーケストレーションデーモン
│   │   ├── em_sensor_app       ← EM 管理対象: センサー
│   │   └── em_actuator_app     ← EM 管理対象: アクチュエータ
│   ├── ecu/                    ← ECU 統合アプリ
│   │   ├── ecu_someip_source   ← SOME/IP 車両ステータス送信
│   │   └── ecu_full_stack      ← CAN + SOME/IP → DDS フュージョン
│   └── can/                    ← CAN バス
│       └── socketcan_receive_decode ← SocketCAN 受信 + デコード
│
├── communication/              ← 通信テンプレート (ミドルウェア必須)
│   ├── someip/                 ← SOME/IP pub/sub
│   ├── dds/                    ← DDS pub/sub
│   ├── zerocopy/               ← iceoryx ゼロコピー
│   └── switchable_pubsub/      ← 切替可能トランスポート
│
└── samples/                    ← ara::com API デモ (インプロセス)
    ├── 01_typed_method_rpc     ← メソッド RPC
    ├── 02_fire_and_forget      ← ワンウェイ通知
    ├── 03_communication_group  ← グループ通信
    ├── 04_field_get_set        ← フィールド Get/Set
    └── 05_event_pubsub         ← イベント Pub/Sub
```

### 各カテゴリの特性

| カテゴリ | ミドルウェア | 用途 | 難易度 |
|---------|-----------|------|-------|
| **basic** | 不要 | 開発開始の出発点 | 入門 |
| **feature** | 一部必要 | 特定機能の学習・テスト | 中級 |
| **communication** | 必須 | 通信パターンの実装 | 中級-上級 |
| **samples** | 不要 | ara::com API の学習 | 入門-中級 |

---

## 1. basic — 基本テンプレート

ミドルウェア不要で動作する基本的なアプリケーションテンプレートです。
AUTOSAR Adaptive Platform の基本概念を理解するための出発点として最適です。

---

### minimal_runtime_app

**最小構成アプリケーション** — ara::core の初期化/終了処理のみを行う最小のアプリ。

| 項目 | 値 |
|------|-----|
| **ソースファイル** | `user_apps/src/apps/basic/minimal_runtime_app.cpp` |
| **ビルドターゲット** | `autosar_user_minimal_runtime` |
| **使用モジュール** | `ara::core`, `ara::log` |
| **ミドルウェア** | 不要 |

**動作:**
1. `ara::core::Initialize()` で AUTOSAR ランタイムを初期化
2. `LoggingFramework` を生成してログメッセージを 1 つ出力
3. `ara::core::Deinitialize()` で終了

**使用例:**
```bash
./autosar_user_minimal_runtime
```

**学習ポイント:**
- AUTOSAR アプリの最小構成
- `ara::core::Initialize()` / `Deinitialize()` のライフサイクル
- `ara::log::LoggingFramework` の基本使用

---

### exec_signal_app

**シグナルハンドリング + ライフサイクルテンプレート** — SIGINT/SIGTERM を受けて
グレースフルシャットダウンする管理対象アプリの雛形。

| 項目 | 値 |
|------|-----|
| **ソースファイル** | `user_apps/src/apps/basic/exec_signal_app.cpp` |
| **ビルドターゲット** | `autosar_user_exec_signal_template` |
| **使用モジュール** | `ara::core`, `ara::exec` |
| **ミドルウェア** | 不要 |

**動作:**
1. `ara::core::Initialize()` で初期化
2. `ara::exec::SignalHandler::Register()` で SIGINT/SIGTERM ハンドラを登録
3. 100ms 間隔のメインループ
4. シグナル受信時に `IsTerminationRequested()` が `true` を返し、ループを脱出
5. `Deinitialize()` で終了

**使用例:**
```bash
./autosar_user_exec_signal_template
# Ctrl+C で停止
```

**学習ポイント:**
- `ara::exec::SignalHandler` によるグレースフルシャットダウン
- 管理対象アプリの基本的なメインループパターン
- bringup.sh から起動される典型的なアプリ構造

---

### per_phm_app

**永続化 + PHM テンプレート** — KeyValueStorage への読み書きと
HealthChannel による健全性レポートを組み合わせたデモ。

| 項目 | 値 |
|------|-----|
| **ソースファイル** | `user_apps/src/apps/basic/per_phm_app.cpp` |
| **ビルドターゲット** | `autosar_user_per_phm_template` |
| **使用モジュール** | `ara::core`, `ara::per`, `ara::phm` |
| **ミドルウェア** | 不要 |

**動作:**
1. `ara::phm::HealthChannel` で `kOk` をレポート
2. `ara::per::KeyValueStorage` をオープン
3. `run_count` キーを読み込み、インクリメントして書き戻し
4. `SyncToStorage()` でディスクに永続化
5. `kDeactivated` をレポートして終了

**使用例:**
```bash
# 実行するたびに run_count がインクリメントされる
./autosar_user_per_phm_template
./autosar_user_per_phm_template  # run_count = 2
```

**学習ポイント:**
- `ara::per::KeyValueStorage` の Get/Set/Sync パターン
- `ara::phm::HealthChannel` のステータスレポート
- `InstanceSpecifier` の生成と使用

---

### raspi_ecu_app

**本番 ECU アプリケーション** — Raspberry Pi 上で実際の車両 ECU として動作する
本格的なアプリケーション。全プラットフォームデーモンと連携して動作します。

| 項目 | 値 |
|------|-----|
| **ソースファイル** | `user_apps/src/apps/basic/raspi_ecu_app.cpp` |
| **ビルドターゲット** | `autosar_user_raspi_ecu_app` |
| **使用モジュール** | `ara::core`, `ara::exec`, `ara::phm`, `ara::per`, `ara::sm`, `ara::log` |
| **ミドルウェア** | 不要 |

**機能:**
- `ara::exec::ApplicationClient` — Execution Manager にヘルスレポート
- `ara::phm::HealthChannel` — PHM デーモンにハートビート送信
- `ara::per::KeyValueStorage` — `boot_count` / `cycle_count` の永続化
- `ara::sm::MachineStateClient` — マシン状態の監視と反応
- **車両センサーデータシミュレーション**: 速度、RPM、ステアリング角、ギア、冷却水温度、バッテリー電圧
- 定期的なステータスファイル出力

**環境変数:**

| 変数 | デフォルト | 説明 |
|------|----------|------|
| `AUTOSAR_ECU_PERIOD_MS` | `100` | メインループ間隔 |
| `AUTOSAR_ECU_LOG_EVERY` | `10` | ログ出力頻度 (N サイクルごと) |
| `AUTOSAR_ECU_PERSIST_EVERY` | `100` | 永続化頻度 (N サイクルごと) |
| `AUTOSAR_ECU_STATUS_FILE` | (なし) | ステータスファイルパス |
| `AUTOSAR_ECU_CAN_IFNAME` | `can0` | CAN インターフェース名 |

**使用例:**
```bash
./autosar_user_raspi_ecu_app --period-ms=100

# bringup.sh に登録する場合
launch_app "raspi_ecu" "${AUTOSAR_USER_APPS_BUILD_DIR}/src/apps/basic/autosar_user_raspi_ecu_app" --period-ms=100
```

**学習ポイント:**
- 複数の ara::* モジュールの統合パターン
- ApplicationClient によるプラットフォーム連携
- 実際の ECU アプリの設計パターン

---

### raspi_mock_ecu_app

**全モジュールモック ECU** — 全 11 種類の ara::* モジュールを単一アプリで行使する
スタンドアロンデモ。ミドルウェアやプラットフォームデーモンなしで動作します。

| 項目 | 値 |
|------|-----|
| **ソースファイル** | `user_apps/src/apps/basic/raspi_mock_ecu_app.cpp` |
| **ビルドターゲット** | `ara_raspi_mock_ecu_sample` |
| **使用モジュール** | 全 11 モジュール |
| **ミドルウェア** | 不要 |

**行使するモジュール:**

| モジュール | テスト内容 |
|----------|----------|
| `ara::core` | Initialize/Deinitialize, InstanceSpecifier, ErrorCode |
| `ara::log` | LoggingFramework, LogStream |
| `ara::exec` | SignalHandler, ApplicationClient |
| `ara::phm` | HealthChannel (kOk/kExpired/kDeactivated) |
| `ara::per` | KeyValueStorage (read/write/sync) |
| `ara::diag` | DiagnosticManager (UDS session simulation) |
| `ara::sm` | MachineStateClient (state transitions) |
| `ara::tsync` | TimeSyncClient (mock NTP sync) |
| `ara::nm` | NetworkManager (CAN NM lifecycle) |
| `ara::crypto` | HsmProvider (AES-128 encrypt/decrypt, HMAC-SHA256) |
| `ara::ucm` | UpdateManager (staged update with SHA-256 digest) |

**コマンドライン引数:**

| 引数 | デフォルト | 説明 |
|------|----------|------|
| `--max-cycles=N` | `0` (無限) | 最大サイクル数 |
| `--period-ms=N` | `500` | サイクル間隔 |
| `--verbose-crypto` | (無効) | 暗号処理の詳細ログ出力 |

**使用例:**
```bash
# 10 サイクル実行して終了
./ara_raspi_mock_ecu_sample --max-cycles=10 --period-ms=100

# 暗号処理の詳細ログ付き
./ara_raspi_mock_ecu_sample --max-cycles=5 --verbose-crypto
```

**学習ポイント:**
- 全 ara::* モジュールの API 使用例
- スタンドアロン動作 (デーモン・ミドルウェア不要)
- 各モジュールの基本的な使い方の一覧

---

## 2. feature — 機能別デモ

特定の AUTOSAR 機能を掘り下げたデモアプリケーションです。

### 2.1 runtime — ランタイム・ライフサイクル

#### runtime_lifecycle_app

**基本ライフサイクルテンプレート** — 10 サイクルのハートビートループを実行して終了する
最小の管理対象アプリテンプレート。

| 項目 | 値 |
|------|-----|
| **ソースファイル** | `user_apps/src/apps/feature/runtime/runtime_lifecycle_app.cpp` |
| **ビルドターゲット** | `autosar_user_runtime_lifecycle` |
| **使用モジュール** | `ara::core` |
| **ミドルウェア** | 不要 |

**動作:**
- 初期化 → 10 回の 100ms ループ → 終了
- 管理対象アプリの開発開始点として使用

---

#### exec_runtime_monitor_app

**Alive Supervision モニタ** — PHM の Alive Supervision をシミュレートし、
タイムアウト検出やフォールトインジェクション (意図的な停止) をテストできるアプリ。

| 項目 | 値 |
|------|-----|
| **ソースファイル** | `user_apps/src/apps/feature/runtime/exec_runtime_monitor_app.cpp` |
| **ビルドターゲット** | `autosar_user_exec_runtime_monitor` |
| **使用モジュール** | `ara::core`, `ara::exec`, `ara::phm` |
| **ミドルウェア** | 不要 |

**機能:**
- 設定可能な Alive Supervision ウィンドウ
- スタートアップ猶予期間
- フォールトインジェクション: 指定サイクルで意図的に停止し、タイムアウト検出をテスト
- PHM HealthChannel に `kOk` / `kExpired` / `kDeactivated` をレポート

**環境変数:**

| 変数 | デフォルト | 説明 |
|------|----------|------|
| `USER_EXEC_CYCLE_MS` | (設定値) | サイクル間隔 |
| `USER_EXEC_ALIVE_TIMEOUT_MS` | (設定値) | Alive タイムアウト |
| `USER_EXEC_ALIVE_STARTUP_GRACE_MS` | (設定値) | スタートアップ猶予 |
| `USER_EXEC_ALIVE_COOLDOWN_MS` | (設定値) | クールダウン |
| `USER_EXEC_STOP_ON_EXPIRED` | (設定値) | タイムアウト時に停止 |
| `USER_EXEC_FAULT_STALL_CYCLE` | (設定値) | フォールトインジェクション対象サイクル |
| `USER_EXEC_FAULT_STALL_MS` | (設定値) | フォールト時の停止時間 |
| `USER_EXEC_HEALTH_INSTANCE_SPECIFIER` | (設定値) | HealthChannel インスタンス |
| `USER_EXEC_STATUS_EVERY` | (設定値) | ステータスログ頻度 |

---

### 2.2 em — Execution Manager 連携

Execution Manager がユーザーアプリをプロセスとして管理 (fork/exec) する仕組みの
デモンストレーションです。3 つのアプリが連携して動作します。

#### em_daemon_app

**EM オーケストレーションデーモン** — `ara::exec::ExecutionManager` を使って
子プロセス (sensor / actuator) を管理するデーモンアプリ。

| 項目 | 値 |
|------|-----|
| **ソースファイル** | `user_apps/src/apps/feature/em/em_daemon_app.cpp` |
| **ビルドターゲット** | `autosar_user_em_daemon` |
| **使用モジュール** | `ara::core`, `ara::exec` |
| **ミドルウェア** | 不要 |

**機能:**
- ローカル RPC サーバ (`LocalRpcServer`) を使用 (ネットワーク不要)
- MachineFG (`Off` / `Running`) を定義
- `em_sensor_app` と `em_actuator_app` のバイナリパスを自動検出
- `ActivateFunctionGroup("MachineFG", "Running")` で子プロセスを起動
- 5 秒間隔でプロセスの生存を監視
- 設定時間後に自動シャットダウン (`EM_RUN_SECONDS`)

**環境変数:**

| 変数 | デフォルト | 説明 |
|------|----------|------|
| `EM_SENSOR_BIN` | (自動検出) | sensor アプリのパス |
| `EM_ACTUATOR_BIN` | (自動検出) | actuator アプリのパス |
| `EM_RUN_SECONDS` | `0` (無制限) | 自動シャットダウンまでの秒数 |

**使用例:**
```bash
# sensor と actuator を自動検出して起動
./autosar_user_em_daemon

# 30 秒後に自動終了
EM_RUN_SECONDS=30 ./autosar_user_em_daemon
```

---

#### em_sensor_app

**EM 管理対象センサーアプリ** — `em_daemon_app` から fork/exec で起動される
シンプルなセンサーシミュレータ。

| 項目 | 値 |
|------|-----|
| **ソースファイル** | `user_apps/src/apps/feature/em/em_sensor_app.cpp` |
| **ビルドターゲット** | `autosar_user_em_sensor` |
| **使用モジュール** | `ara::core`, `ara::exec` |
| **ミドルウェア** | 不要 |

**動作:**
- 温度データをシミュレーション (20-30°C の範囲)
- `EM_SENSOR_INTERVAL_MS` 間隔で値を更新
- `em_daemon_app` によって起動・停止される

---

#### em_actuator_app

**EM 管理対象アクチュエータアプリ** — `em_daemon_app` から fork/exec で起動される
シンプルなアクチュエータシミュレータ。

| 項目 | 値 |
|------|-----|
| **ソースファイル** | `user_apps/src/apps/feature/em/em_actuator_app.cpp` |
| **ビルドターゲット** | `autosar_user_em_actuator` |
| **使用モジュール** | `ara::core`, `ara::exec` |
| **ミドルウェア** | 不要 |

**動作:**
- アクチュエータポジションを巡回: 0° → 45° → 90° → 45° → 0° → ...
- `EM_ACTUATOR_INTERVAL_MS` 間隔で位置を更新
- `em_daemon_app` によって起動・停止される

---

### 2.3 ecu — ECU 統合

#### ecu_someip_source_app

**SOME/IP 車両ステータスソース** — SOME/IP バックエンドで車両ステータスフレームを
配信する送信側アプリ。

| 項目 | 値 |
|------|-----|
| **ソースファイル** | `user_apps/src/apps/feature/ecu/ecu_someip_source_app.cpp` |
| **ビルドターゲット** | `autosar_user_ecu_someip_source` |
| **使用モジュール** | `ara::core`, `ara::com` (SOME/IP) |
| **ミドルウェア** | vsomeip 必須 (`ARA_COM_USE_VSOMEIP=1`) |

**機能:**
- `VehicleStatusProvider` で SOME/IP バックエンドプロファイルを使用
- `VehicleStatusFrame` (速度、RPM、ステアリング、ギア、ステータスフラグ) を
  設定可能な間隔で配信

---

#### ecu_full_stack_app

**フルスタック ECU リファレンスアプリ** — CAN + SOME/IP → DDS のデータフュージョン、
ゼロコピーローカル通信、PHM ヘルスレポート、永続カウンタを備えた包括的な ECU アプリ。

| 項目 | 値 |
|------|-----|
| **ソースファイル** | `user_apps/src/apps/feature/ecu/ecu_full_stack_app.cpp` |
| **ビルドターゲット** | `autosar_user_ecu_full_stack` |
| **使用モジュール** | `ara::core`, `ara::com` (SOME/IP + DDS), `ara::phm`, `ara::per` |
| **ミドルウェア** | CycloneDDS 必須 (`ARA_COM_USE_CYCLONEDDS=1`) |

**機能:**
- CAN バス入力 (SocketCAN or mock) + SOME/IP 入力のフュージョン
- DDS トピックへの配信
- オプションのゼロコピーローカルパス (iceoryx)
- PHM ヘルスレポート + 永続カウンタ

**コマンドライン引数:**

| 引数 | 説明 |
|------|------|
| `--enable-can` | CAN 入力を有効化 |
| `--enable-someip` | SOME/IP 入力を有効化 |
| `--require-someip` | SOME/IP が必須 |
| `--require-both-sources` | 両方のソースが必須 |
| `--enable-zerocopy-local` | ゼロコピーローカルパスを有効化 |
| `--can-backend socketcan\|mock` | CAN バックエンド選択 |
| `--ifname <name>` | CAN インターフェース名 |
| `--dds-domain-id <N>` | DDS ドメイン ID |
| `--dds-topic <name>` | DDS トピック名 |
| `--publish-period-ms <N>` | 配信間隔 |
| `--source-stale-ms <N>` | ソースのステイル判定時間 |

---

### 2.4 can — CAN バス

#### socketcan_receive_decode_app

**SocketCAN 受信 + デコードテンプレート** — SocketCAN (または mock) からフレームを
受信し、車両ステータスデータにデコードするアプリ。

| 項目 | 値 |
|------|-----|
| **ソースファイル** | `user_apps/src/apps/feature/can/socketcan_receive_decode_app.cpp` |
| **ビルドターゲット** | `autosar_user_socketcan_receive_decode` |
| **使用モジュール** | `ara::core`, `ara::com` (CAN) |
| **ミドルウェア** | 不要 (mock バックエンド使用可) |

**機能:**
- `CanFrameReceiver` で SocketCAN または mock バックエンドからフレーム受信
- `VehicleStatusCanDecoder` で CAN フレームを `VehicleStatusFrame` にデコード
- 設定可能な CAN ID: パワートレイン (0x100)、シャシー (0x101)
- シーケンスカウンタ、速度、RPM、ギアを抽出

**コマンドライン引数:**

| 引数 | 説明 |
|------|------|
| `--powertrain-can-id <N>` | パワートレイン CAN ID (デフォルト: 0x100) |
| `--chassis-can-id <N>` | シャシー CAN ID (デフォルト: 0x101) |

---

## 3. communication — 通信テンプレート

各通信ミドルウェアを使用した Pub/Sub パターンのテンプレートです。
いずれもミドルウェアのインストールが必要です。

### 3.1 someip — SOME/IP

#### provider_app (SOME/IP)

**SOME/IP サービスプロバイダ** — `VehicleSignalSkeleton` でサービスをオファリングし、
型付きイベントを配信するプロバイダアプリ。

| 項目 | 値 |
|------|-----|
| **ソースファイル** | `user_apps/src/apps/communication/someip/provider_app.cpp` |
| **ビルドターゲット** | `autosar_user_someip_provider` |
| **ビルド条件** | `ARA_COM_USE_VSOMEIP=ON` |

**動作:**
1. `VehicleSignalSkeleton` でサービスオファリング
2. サブスクリプションコールバックの登録
3. `VehicleSignalFrame` イベントの定期配信
4. AUTOSAR InstanceSpecifier: `AdaptiveAutosar/UserApps/SomeIpProviderTemplate`

---

#### consumer_app (SOME/IP)

**SOME/IP サービスコンシューマ** — `VehicleSignalProxy::StartFindService()` で
サービスディスカバリを行い、受信したイベントを処理するコンシューマアプリ。

| 項目 | 値 |
|------|-----|
| **ソースファイル** | `user_apps/src/apps/communication/someip/consumer_app.cpp` |
| **ビルドターゲット** | `autosar_user_someip_consumer` |
| **ビルド条件** | `ARA_COM_USE_VSOMEIP=ON` |

**動作:**
1. `StartFindService()` でサービスディスカバリ (最大 30 秒待機)
2. `condition_variable` でサービス検出を待機
3. `StatusEvent` をサブスクライブ
4. `SetReceiveHandler` + `GetNewSamples` でイベント駆動処理

---

### 3.2 dds — DDS

#### pub_app (DDS)

**DDS パブリッシャ** — CycloneDDS を使用して型付きメッセージを配信するテンプレート。

| 項目 | 値 |
|------|-----|
| **ソースファイル** | `user_apps/src/apps/communication/dds/pub_app.cpp` |
| **ビルドターゲット** | `autosar_user_dds_pub` |
| **ビルド条件** | `ARA_COM_USE_CYCLONEDDS=ON`, `USER_APPS_DDS_TYPE_AVAILABLE=1` |

**動作:**
- `ara::com::dds::DdsPublisher<UserAppsStatus>` でパブリッシュ
- トピック: `adaptive_autosar/user_apps/apps/UserAppsStatus`
- DDS ドメイン ID: 0

---

#### sub_app (DDS)

**DDS サブスクライバ** — CycloneDDS からメッセージを受信するテンプレート。

| 項目 | 値 |
|------|-----|
| **ソースファイル** | `user_apps/src/apps/communication/dds/sub_app.cpp` |
| **ビルドターゲット** | `autosar_user_dds_sub` |
| **ビルド条件** | `ARA_COM_USE_CYCLONEDDS=ON`, `USER_APPS_DDS_TYPE_AVAILABLE=1` |

**動作:**
- `ara::com::dds::DdsSubscriber<UserAppsStatus>` でサブスクライブ
- `WaitForData` + `Take` ループでメッセージ受信

---

### 3.3 zerocopy — ゼロコピー共有メモリ

#### pub_app (Zero-Copy)

**ゼロコピーパブリッシャ** — iceoryx 共有メモリを使用したゼロコピー通信のテンプレート。

| 項目 | 値 |
|------|-----|
| **ソースファイル** | `user_apps/src/apps/communication/zerocopy/pub_app.cpp` |
| **ビルドターゲット** | `autosar_user_zerocopy_pub` |
| **ビルド条件** | `ARA_COM_USE_ICEORYX=ON` |

**動作:**
- `ZeroCopyPublisher` でチャネル `{user_apps, templates, vehicle_signal}` を作成
- `Loan()` で共有メモリバッファを確保
- データ書き込み後 `Publish()` でゼロコピー配信

---

#### sub_app (Zero-Copy)

**ゼロコピーサブスクライバ** — iceoryx 共有メモリからデータを受信するテンプレート。

| 項目 | 値 |
|------|-----|
| **ソースファイル** | `user_apps/src/apps/communication/zerocopy/sub_app.cpp` |
| **ビルドターゲット** | `autosar_user_zerocopy_sub` |
| **ビルド条件** | `ARA_COM_USE_ICEORYX=ON` |

**動作:**
- `ZeroCopySubscriber` で対応チャネルをサブスクライブ
- `WaitForData` + `TryTake` で共有メモリからデータを読み取り
- `ZeroCopyFrame` をデシリアライズ

---

### 3.4 switchable_pubsub — 切替可能トランスポート

ビルド時のコンパイルフラグで SOME/IP / DDS / ROS2 からトランスポートバインディングを
選択できる pub/sub テンプレートです。

#### switchable_pub

**切替可能トランスポート パブリッシャ** — コード変更なしでトランスポートを切替可能。

| 項目 | 値 |
|------|-----|
| **ソースファイル** | `user_apps/src/apps/communication/switchable_pubsub/src/switchable_pub.cpp` |
| **ビルドターゲット** | `autosar_user_switchable_pub` |
| **ビルド条件** | 選択したトランスポートに対応するフラグ |

**動作:**
- `TopicEventSkeleton<VehicleStatusFrame>` で配信
- 車両ステータスフレーム (速度、RPM、ステアリング、ギア) をシミュレーション
- アプリ ID: `SWPB`

**コマンドライン引数:**

| 引数 | 説明 |
|------|------|
| `--period-ms <N>` | 配信間隔 |

---

#### switchable_sub

**切替可能トランスポート サブスクライバ** — パブリッシャと対になるサブスクライバ。

| 項目 | 値 |
|------|-----|
| **ソースファイル** | `user_apps/src/apps/communication/switchable_pubsub/src/switchable_sub.cpp` |
| **ビルドターゲット** | `autosar_user_switchable_sub` |

**動作:**
- `TopicEventProxy<VehicleStatusFrame>` でサブスクライブ
- キャパシティ 128 で subscribe、`--poll-ms` 間隔でポーリング
- 受信サンプルのログ出力 (seq, speed, RPM, gear)
- アプリ ID: `SWSB`

---

## 4. samples — ara::com API デモ

`ara::com` の各 API パターンを **インプロセスバインディング** (ネットワーク不要) で
デモンストレーションするサンプルです。`sample::MakeMethodPair()` / `sample::MakeEventPair()`
を使用するため、ミドルウェアなしで動作します。

---

### 01_typed_method_rpc

**型付きメソッド RPC** — `SkeletonMethod<R(Args...)>` と `ProxyMethod<R(Args...)>` の
リクエスト/レスポンスパターンを示すサンプル。

| 項目 | 値 |
|------|-----|
| **ソースファイル** | `user_apps/src/apps/samples/01_typed_method_rpc.cpp` |
| **ビルドターゲット** | `autosar_sample_01_typed_method_rpc` |

**デモ内容:**
- **add**: `uint32_t + uint32_t → uint32_t` 正常レスポンス
- **divide**: ゼロ除算時にエラー (`ComErrc::kGrantEnforcementError`) を返す
- **log**: `void` 戻り値のメソッド
- `SetHandler` / `UnsetHandler` / `Future::GetResult()` の使用法

**AUTOSAR SWS 参照:** SWS_CM_00191, SWS_CM_00192

---

### 02_fire_and_forget

**ワンウェイ通知 (Fire-and-Forget)** — レスポンスを返さないワンウェイメソッドの
パターンを示すサンプル。

| 項目 | 値 |
|------|-----|
| **ソースファイル** | `user_apps/src/apps/samples/02_fire_and_forget.cpp` |
| **ビルドターゲット** | `autosar_sample_02_fire_and_forget` |

**デモ内容:**
- `SkeletonFireAndForgetMethod<Args...>` と `ProxyFireAndForgetMethod<Args...>`
- パラメータ: `eventId (uint32_t)`, `severity (uint8_t)`, `text (string)`
- Proxy 側 `operator()` は `void` を返す (Future なし)
- ハンドラ除去後の呼び出しは無視される

---

### 03_communication_group

**グループ通信** — サーバが複数クライアントにブロードキャスト/ユニキャストし、
クライアントがレスポンスを返すパターンのサンプル。

| 項目 | 値 |
|------|-----|
| **ソースファイル** | `user_apps/src/apps/samples/03_communication_group.cpp` |
| **ビルドターゲット** | `autosar_sample_03_communication_group` |

**デモ内容:**
- `CommunicationGroupServer<T, R>` / `CommunicationGroupClient<T, R>`
- シナリオ: 温度コントローラ (サーバ) → センサークライアント群
- サーバ: `Broadcast()` で全クライアントにコマンド送信
- クライアント: `Response()` で温度値を返す
- `ListClients()` / `RemoveClient()` のクライアント管理

---

### 04_field_get_set

**フィールド Get/Set** — `SkeletonField<T>` と `ProxyField<T>` による
フィールドアクセスパターンのサンプル。

| 項目 | 値 |
|------|-----|
| **ソースファイル** | `user_apps/src/apps/samples/04_field_get_set.cpp` |
| **ビルドターゲット** | `autosar_sample_04_field_get_set` |

**デモ内容:**
- `RegisterGetHandler`: 現在値の取得
- `RegisterSetHandler`: バリデーション付き設定 (0 を拒否、非ゼロを許可)
- `Update()`: プッシュ + サブスクライバへの通知
- `ProxyField::Get()` / `Set()` / `GetNewSamples()` / `Subscribe()` / `Unsubscribe()`

**AUTOSAR SWS 参照:** SWS_CM_00112, SWS_CM_00113

---

### 05_event_pubsub

**イベント Pub/Sub** — `SkeletonEvent<T>` と `ProxyEvent<T>` による
イベントベースの Pub/Sub パターンのサンプル。

| 項目 | 値 |
|------|-----|
| **ソースファイル** | `user_apps/src/apps/samples/05_event_pubsub.cpp` |
| **ビルドターゲット** | `autosar_sample_05_event_pubsub` |

**デモ内容:**
- `Offer()` / `StopOffer()` — サービスのオファリング制御
- コピーパス: `Send(const T&)` でイベント送信
- ゼロコピーパス: `Allocate()` + `Send(SampleAllocateePtr<T>)` で送信
- `Subscribe(maxSamples)` / `SetReceiveHandler()` / `GetNewSamples()`
- `GetFreeSampleCount()` — 残りスロット数の確認
- `SetSubscriptionStateChangeHandler()` — サブスクリプション状態変化の通知
- データ型: `VehicleStatus{speedKph, engineRpm}`

---

## ビルド方法

### インストール済みランタイムに対してビルド (推奨)

```bash
# AUTOSAR AP ランタイムが /opt/autosar-ap にインストール済みの場合
sudo ./scripts/build_user_apps_from_opt.sh \
  --prefix /opt/autosar-ap \
  --source-dir /opt/autosar-ap/user_apps \
  --build-dir /opt/autosar-ap/user_apps_build
```

### ソースツリー内ビルド (開発時)

```bash
# メインビルドに含まれる basic アプリはそのまま実行可能
cmake -S . -B build -Dbuild_tests=ON
cmake --build build -j$(nproc)

ls build/ara_raspi_mock_ecu_sample
ls build/autosar_user_*
```

### 通信アプリのビルド (ミドルウェア有効時)

```bash
cmake -S user_apps -B build-ua \
  -DAUTOSAR_AP_PREFIX=/opt/autosar-ap \
  -DARA_COM_USE_VSOMEIP=ON \
  -DARA_COM_USE_ICEORYX=ON \
  -DARA_COM_USE_CYCLONEDDS=ON
cmake --build build-ua -j$(nproc)
```

---

## アプリ選定ガイド

### 初めて AUTOSAR AP を触る場合

1. `raspi_mock_ecu_app` — 全モジュールの API を一通り体験
2. `minimal_runtime_app` — 最小構成を理解
3. `samples/01_typed_method_rpc` — ara::com のメソッド呼び出しを学ぶ

### ECU アプリを開発したい場合

1. `exec_signal_app` — 管理対象アプリの基本パターンを習得
2. `raspi_ecu_app` — 本番 ECU アプリの構造を参考に
3. `per_phm_app` — 永続化 + PHM の統合方法を学ぶ

### Execution Manager を使いたい場合

1. `em_daemon_app` + `em_sensor_app` + `em_actuator_app` — EM によるプロセス管理
2. `exec_runtime_monitor_app` — Alive Supervision のテスト

### 通信機能を実装したい場合

1. `someip/provider_app` + `consumer_app` — SOME/IP Pub/Sub
2. `dds/pub_app` + `sub_app` — DDS Pub/Sub
3. `zerocopy/pub_app` + `sub_app` — ゼロコピー共有メモリ
4. `switchable_pubsub/` — トランスポート切替可能な実装

### CAN バスと連携したい場合

1. `socketcan_receive_decode_app` — CAN フレーム受信 + デコード
2. `ecu_full_stack_app` — CAN + SOME/IP → DDS フュージョン
