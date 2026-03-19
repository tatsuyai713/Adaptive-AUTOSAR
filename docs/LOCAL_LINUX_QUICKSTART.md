# ローカル Linux デプロイメントガイド

手元の Linux PC / VM / Docker 上に AUTOSAR Adaptive Platform を**実際にデプロイ**し、
プラットフォームデーモン群をサービスとして稼働させ、ユーザーアプリを
Execution Manager の管理下で実行するためのガイドです。

---

## 目次

1. [概要](#1-概要)
2. [前提条件](#2-前提条件)
3. [ミドルウェアのインストール](#3-ミドルウェアのインストール)
4. [AUTOSAR AP ランタイムのビルドとインストール](#4-autosar-ap-ランタイムのビルドとインストール)
5. [ユーザーアプリのビルド](#5-ユーザーアプリのビルド)
6. [systemd サービスのインストール](#6-systemd-サービスのインストール)
7. [プラットフォームの起動](#7-プラットフォームの起動)
8. [ユーザーアプリのデプロイと実行管理](#8-ユーザーアプリのデプロイと実行管理)
9. [動作確認](#9-動作確認)
10. [systemd なしでの手動デプロイ](#10-systemd-なしでの手動デプロイ)
11. [ユーザーアプリ開発ガイド](#11-ユーザーアプリ開発ガイド)
12. [トラブルシューティング](#12-トラブルシューティング)

---

## 1. 概要

### デプロイ構成

```
/opt/autosar_ap/                     ← インストール先 (AUTOSAR_AP_PREFIX)
├── bin/                             ← 17 個のプラットフォームデーモン
│   ├── adaptive_autosar             ← メインプラットフォームアプリ (EM/SM 統合)
│   ├── autosar_vsomeip_routing_manager
│   ├── autosar_phm_daemon
│   ├── autosar_diag_server
│   ├── autosar_dlt_daemon
│   └── ... (計 17 バイナリ)
├── lib/                             ← 共有ライブラリ (ara_core, ara_exec, ...)
├── include/                         ← ヘッダファイル
├── configuration/                   ← ARXML マニフェスト + vsomeip JSON
│   ├── execution_manifest.arxml
│   ├── extended_vehicle_manifest.arxml
│   ├── diagnostic_manager_manifest.arxml
│   ├── health_monitoring_manifest.arxml
│   ├── vsomeip-rpi.json
│   └── vsomeip-local.json
├── user_apps/                       ← ユーザーアプリ ソーステンプレート
└── deployment/rpi_ecu/              ← サービスファイルテンプレート

/opt/autosar_ap/user_apps_build/     ← ユーザーアプリビルド出力先
/usr/local/bin/autosar-*-wrapper.sh  ← systemd 用ラッパースクリプト
/etc/default/autosar-*               ← サービス環境設定ファイル
/etc/autosar/bringup.sh              ← ユーザーアプリ起動スクリプト
/etc/systemd/system/autosar-*.service← systemd ユニットファイル (20 個)

/run/autosar/                        ← ランタイム状態ディレクトリ
├── phm/health/                      ← PHM ヘルスチェック用
├── user_apps/                       ← ユーザーアプリステータス
├── user_apps_registry.csv           ← 起動済みアプリ一覧
├── *.status                         ← 各デーモンステータスファイル
└── nm_triggers/, can_triggers/      ← トリガファイル
```

### プロセス起動順序

```
Tier 0: iceoryx RouDi          ← 共有メモリ IPC ミドルウェア
         ↓
Tier 1: vsomeip routing        ← SOME/IP ルーティングマネージャ
         ↓
Tier 2: core services (並列)   ← DLT, 時刻同期, 永続化, IAM, NM, CAN
         ↓
Tier 3: platform services      ← SM, ウォッチドッグ, PHM, 診断, 暗号, UCM, NTP, PTP
         ↓
Tier 4: adaptive_autosar       ← メインプラットフォームアプリ (EM 統合)
         ↓
Tier 5: bringup.sh             ← ユーザーアプリ起動 + user_app_monitor
```

---

## 2. 前提条件

### 動作環境

- Ubuntu 22.04 LTS / 24.04 LTS (推奨)
- Debian 12 以降
- x86_64 または aarch64

### 必須パッケージ

```bash
sudo apt update
sudo apt install -y \
  build-essential \
  cmake \
  git \
  libssl-dev \
  pkg-config \
  libacl1-dev \
  netcat-openbsd

# 確認
cmake --version    # 3.14 以上
g++ --version      # C++14 対応
openssl version
```

---

## 3. ミドルウェアのインストール

ミドルウェアなしでもプラットフォームデーモン群は動作しますが、
ara::com の SOME/IP / DDS 通信を使用する場合はインストールが必要です。

### 3.1 一括インストール (推奨)

```bash
cd ~/Adaptive-AUTOSAR

# 基本依存パッケージ + ミドルウェア 3 種をインストール (約 30-60 分)
sudo ./scripts/install_middleware_stack.sh --install-base-deps
```

インストールされるミドルウェア:

| ミドルウェア | バージョン | インストール先 |
|-------------|----------|--------------|
| iceoryx | v2.0.6 | `/opt/iceoryx` |
| CycloneDDS | 最新 | `/opt/cyclonedds` |
| vsomeip | v3.4.10 | `/opt/vsomeip` |

### 3.2 ミドルウェアなしで進める場合

ミドルウェアをインストールしない場合は、後述のビルドステップで
`--without-vsomeip --without-iceoryx --without-cyclonedds` を指定します。
この場合、vsomeip routing manager は生成されませんが、
他の 16 デーモンは正常に動作します。

---

## 4. AUTOSAR AP ランタイムのビルドとインストール

### 4.1 ワンステップビルド (推奨)

プロファイルスクリプトで、ビルド → インストール → ユーザーアプリビルド → デプロイ資材コピーを一括実行:

```bash
cd ~/Adaptive-AUTOSAR

# ミドルウェア込みの場合
sudo ./scripts/build_and_install_rpi_ecu_profile.sh \
  --prefix /opt/autosar_ap \
  --install-middleware \
  --install-base-deps

# ミドルウェアがインストール済みの場合
sudo ./scripts/build_and_install_rpi_ecu_profile.sh \
  --prefix /opt/autosar_ap

# ミドルウェアなしの場合
sudo ./scripts/build_and_install_rpi_ecu_profile.sh \
  --prefix /opt/autosar_ap \
  --without-vsomeip \
  --without-iceoryx \
  --without-cyclonedds
```

### 4.2 手動ビルド (カスタムが必要な場合)

```bash
# ステップ 1: AUTOSAR AP ランタイムをビルド・インストール
sudo ./scripts/build_and_install_autosar_ap.sh \
  --source-dir "$(pwd)" \
  --prefix /opt/autosar_ap \
  --build-type Release \
  --with-platform-app

# ステップ 2: ユーザーアプリをビルド
sudo ./scripts/build_user_apps_from_opt.sh \
  --prefix /opt/autosar_ap \
  --source-dir /opt/autosar_ap/user_apps \
  --build-dir /opt/autosar_ap/user_apps_build

# ステップ 3: デプロイ資材をコピー
sudo cp -a deployment/rpi_ecu /opt/autosar_ap/deployment/
sudo cp -a configuration/. /opt/autosar_ap/configuration/
```

### 4.3 インストール結果の確認

```bash
# バイナリの確認 (17 個)
ls /opt/autosar_ap/bin/
# adaptive_autosar
# autosar_can_interface_manager
# autosar_crypto_provider
# autosar_diag_server
# autosar_dlt_daemon
# autosar_iam_policy_loader
# autosar_network_manager
# autosar_ntp_time_provider
# autosar_phm_daemon
# autosar_persistency_guard
# autosar_ptp_time_provider
# autosar_sm_state_daemon
# autosar_time_sync_daemon
# autosar_ucm_daemon
# autosar_user_app_monitor
# autosar_vsomeip_routing_manager
# autosar_watchdog_supervisor

# ライブラリの確認
ls /opt/autosar_ap/lib/libara_*.so

# ARXML マニフェストの確認
ls /opt/autosar_ap/configuration/*.arxml
```

---

## 5. ユーザーアプリのビルド

プロファイルスクリプトでビルド済みの場合はスキップできます。

```bash
# インストール済みの AUTOSAR AP ランタイムに対してビルド
sudo ./scripts/build_user_apps_from_opt.sh \
  --prefix /opt/autosar_ap \
  --source-dir /opt/autosar_ap/user_apps \
  --build-dir /opt/autosar_ap/user_apps_build

# ビルド結果の確認 — ECU アプリが生成されていること
ls /opt/autosar_ap/user_apps_build/src/apps/basic/
# autosar_user_raspi_ecu_app
# autosar_user_exec_signal_template
# autosar_user_per_phm_template
```

---

## 6. systemd サービスのインストール

### 6.1 サービスのインストール

```bash
# root 権限が必要
sudo ./scripts/install_rpi_ecu_services.sh \
  --prefix /opt/autosar_ap \
  --user-app-build-dir /opt/autosar_ap/user_apps_build \
  --enable
```

このスクリプトが行うこと:

1. `deployment/rpi_ecu/bin/*.sh.in` → `/usr/local/bin/autosar-*-wrapper.sh` にレンダリング
2. `deployment/rpi_ecu/env/*.env.in` → `/etc/default/autosar-*` にレンダリング
3. `deployment/rpi_ecu/bringup/bringup.sh.in` → `/etc/autosar/bringup.sh` にレンダリング
4. `deployment/rpi_ecu/config/iam_policy.csv.in` → `/etc/autosar/iam_policy.csv` にレンダリング
5. `deployment/rpi_ecu/systemd/*.service` → `/etc/systemd/system/` にコピー (20 ファイル)
6. `systemctl daemon-reload` と条件付き `systemctl enable`

テンプレート変数:
- `__AUTOSAR_AP_PREFIX__` → `/opt/autosar_ap`
- `__USER_APPS_BUILD_DIR__` → `/opt/autosar_ap/user_apps_build`

### 6.2 インストールされる 20 サービス

| サービス名 | 説明 | 依存関係 |
|-----------|------|---------|
| `autosar-iox-roudi` | iceoryx RouDi | network.target |
| `autosar-vsomeip-routing` | SOME/IP ルーティング | iox-roudi |
| `autosar-dlt` | DLT ログ | network-online |
| `autosar-time-sync` | 時刻同期 | network-online |
| `autosar-persistency-guard` | 永続ストレージ | network-online |
| `autosar-iam-policy` | IAM ポリシー | network-online |
| `autosar-can-manager` | CAN 管理 | network-online |
| `autosar-network-manager` | NM 管理 | network-online |
| `autosar-sm-state` | 状態管理 | network-online |
| `autosar-phm-daemon` | PHM | network-online |
| `autosar-diag-server` | 診断サーバ | network-online |
| `autosar-crypto-provider` | 暗号 | network-online |
| `autosar-ucm` | UCM 更新管理 | network-online |
| `autosar-ntp-time-provider` | NTP | network-online |
| `autosar-ptp-time-provider` | PTP | network-online |
| `autosar-watchdog` | ウォッチドッグ | platform-app, exec-manager |
| `autosar-platform-app` | メイン EM/SM | iox-roudi, vsomeip-routing, time-sync, persistency, iam |
| `autosar-exec-manager` | bringup.sh 実行 | iox-roudi, platform-app |
| `autosar-user-app-monitor` | ユーザーアプリ監視 | platform-app, exec-manager |
| `autosar-ecu-full-stack` | ECU サンプル | iox-roudi, vsomeip-routing, platform-app |

### 6.3 サービス環境設定のカスタマイズ

各デーモンの動作は `/etc/default/autosar-*` の環境変数で調整できます:

```bash
# 例: 診断サーバのポートを変更
sudo vi /etc/default/autosar-diag-server
# AUTOSAR_DIAG_LISTEN_PORT=13400
# AUTOSAR_DIAG_LISTEN_ADDR=0.0.0.0

# 例: PHM の監視間隔を変更
sudo vi /etc/default/autosar-phm-daemon
# AUTOSAR_PHM_PERIOD_MS=1000
# AUTOSAR_PHM_DEGRADED_THRESHOLD=0.0
# AUTOSAR_PHM_CRITICAL_THRESHOLD=0.5
```

---

## 7. プラットフォームの起動

### 7.1 依存順序に従ったサービス起動

```bash
# Tier 0: iceoryx RouDi (ミドルウェアを使う場合)
sudo systemctl start autosar-iox-roudi.service

# Tier 1: SOME/IP ルーティング
sudo systemctl start autosar-vsomeip-routing.service

# Tier 2: コアプラットフォームサービス (並列起動可能)
sudo systemctl start autosar-dlt.service
sudo systemctl start autosar-time-sync.service
sudo systemctl start autosar-persistency-guard.service
sudo systemctl start autosar-iam-policy.service
sudo systemctl start autosar-can-manager.service
sudo systemctl start autosar-network-manager.service

# Tier 3: プラットフォーム監視・専門サービス
sudo systemctl start autosar-sm-state.service
sudo systemctl start autosar-phm-daemon.service
sudo systemctl start autosar-diag-server.service
sudo systemctl start autosar-crypto-provider.service
sudo systemctl start autosar-ucm.service
sudo systemctl start autosar-ntp-time-provider.service
sudo systemctl start autosar-ptp-time-provider.service

# Tier 4: メインプラットフォームアプリ (Execution Manager 統合)
sudo systemctl start autosar-platform-app.service

# Tier 5: ユーザーアプリ起動 + 監視
sudo systemctl start autosar-exec-manager.service
sudo systemctl start autosar-watchdog.service
sudo systemctl start autosar-user-app-monitor.service
```

### 7.2 状態確認

```bash
# 全 AUTOSAR サービスの状態を一括確認
systemctl list-units 'autosar-*' --no-pager

# 個別サービスの詳細確認
sudo systemctl status autosar-platform-app.service --no-pager
sudo systemctl status autosar-exec-manager.service --no-pager

# ログの確認
journalctl -u autosar-platform-app.service --no-pager -n 50
journalctl -u autosar-phm-daemon.service --no-pager -n 20

# ランタイム状態ファイルの確認
cat /run/autosar/phm.status
cat /run/autosar/diag_server.status
cat /run/autosar/sm_state.status
```

### 7.3 停止

```bash
# 全 AUTOSAR サービスを停止
sudo systemctl stop autosar-ecu-full-stack.service
sudo systemctl stop autosar-exec-manager.service
sudo systemctl stop autosar-user-app-monitor.service
sudo systemctl stop autosar-watchdog.service
sudo systemctl stop autosar-platform-app.service
sudo systemctl stop autosar-phm-daemon.service
sudo systemctl stop autosar-diag-server.service
sudo systemctl stop autosar-crypto-provider.service
sudo systemctl stop autosar-ucm.service
sudo systemctl stop autosar-sm-state.service
sudo systemctl stop autosar-ntp-time-provider.service
sudo systemctl stop autosar-ptp-time-provider.service
sudo systemctl stop autosar-can-manager.service
sudo systemctl stop autosar-network-manager.service
sudo systemctl stop autosar-persistency-guard.service
sudo systemctl stop autosar-iam-policy.service
sudo systemctl stop autosar-time-sync.service
sudo systemctl stop autosar-dlt.service
sudo systemctl stop autosar-vsomeip-routing.service
sudo systemctl stop autosar-iox-roudi.service
```

---

## 8. ユーザーアプリのデプロイと実行管理

### 8.1 bringup.sh の仕組み

`autosar-exec-manager.service` は `/etc/autosar/bringup.sh` を実行します。
このスクリプトがユーザーアプリの起動と登録を行います。

bringup.sh には 3 つのアプリ起動関数が用意されています:

| 関数 | 説明 |
|------|------|
| `launch_app` | 基本起動。デフォルトの再起動ポリシーを適用 |
| `launch_app_with_heartbeat` | ヘルスチェック付き起動。ハートビートファイルで生存確認 |
| `launch_app_managed` | 完全管理モード。インスタンス指定子・再起動ポリシーを明示的に設定 |

### 8.2 ユーザーアプリの登録

`/etc/autosar/bringup.sh` を編集してアプリを追加します:

```bash
sudo vi /etc/autosar/bringup.sh
```

ファイル末尾の「USER APPLICATION LAUNCH SECTION」にアプリ起動コマンドを追加:

```bash
# =========================================================================
# USER APPLICATION LAUNCH SECTION
# =========================================================================

# --- 例 1: シンプルな ECU アプリ ---
MY_APP="${AUTOSAR_USER_APPS_BUILD_DIR}/src/apps/basic/autosar_user_raspi_ecu_app"
launch_app "raspi_ecu" \
  "${MY_APP}" --period-ms=100

# --- 例 2: ヘルスチェック付きアプリ ---
HEALTH_APP="${AUTOSAR_USER_APPS_BUILD_DIR}/src/apps/basic/autosar_user_exec_signal_template"
HB_FILE="${AUTOSAR_USER_APP_STATUS_DIR}/$(sanitize_name "exec_signal_template").heartbeat"
launch_app_with_heartbeat "exec_signal_template" "${HB_FILE}" "5000" \
  "${HEALTH_APP}"

# --- 例 3: 完全管理モード ---
PHM_APP="${AUTOSAR_USER_APPS_BUILD_DIR}/src/apps/basic/autosar_user_per_phm_template"
launch_app_managed \
  "per_phm_template" \
  "AdaptiveAutosar/UserApps/PerPhmDemo" \
  "" \
  "0" \
  "5" \
  "60000" \
  "${PHM_APP}"
```

### 8.3 アプリ登録の詳細

`launch_app_managed` の引数:

```
launch_app_managed \
  <アプリ名>            ← 識別名 (ログ・PID ファイル・レジストリに使用)
  <インスタンス指定子>    ← AUTOSAR InstanceSpecifier (例: "AdaptiveAutosar/UserApps/MyApp")
  <ハートビートファイル>  ← 空文字列 "" で無効、パス指定で有効
  <HB タイムアウト ms>   ← ハートビート更新の最大許容間隔
  <再起動上限>           ← restart_window_ms 内の最大再起動回数
  <再起動ウィンドウ ms>   ← 再起動回数をカウントする時間窓
  <バイナリ> [引数...]   ← 実行するコマンド
```

### 8.4 アプリ登録レジストリ

起動されたアプリは `/run/autosar/user_apps_registry.csv` に記録されます:

```csv
# name,pid,heartbeat_file,heartbeat_timeout_ms,instance_specifier,restart_limit,restart_window_ms,restart_command
raspi_ecu,12345,,0,AdaptiveAutosar/UserApps/raspi_ecu,5,60000,/opt/autosar_ap/user_apps_build/src/apps/basic/autosar_user_raspi_ecu_app --period-ms=100
```

`autosar_user_app_monitor` デーモンがこのレジストリを監視し、以下を行います:
- PID の生存チェック
- ハートビートファイルのタイムスタンプ鮮度チェック
- 異常終了時の自動再起動 (再起動ポリシーに従う)

### 8.5 変更の反映

bringup.sh を編集したら、exec-manager サービスを再起動します:

```bash
sudo systemctl restart autosar-exec-manager.service

# アプリが起動していることを確認
cat /run/autosar/user_apps_registry.csv
```

---

## 9. 動作確認

### 9.1 プラットフォーム健全性の確認

```bash
# PHM ステータス
cat /run/autosar/phm.status
# 出力例:
#   platform_health=Normal
#   entity_count=13
#   failed_entity_count=0
#   updated_epoch_ms=1742382346000
```

### 9.2 診断サーバへの UDS リクエスト

```bash
# DiagnosticSessionControl → Extended Session (SID=0x10, SubFunc=0x03)
echo -ne '\x10\x03' | nc -w 1 localhost 13400 | xxd
# 正常応答: 50 03 (PositiveResponse = SID + 0x40)

# 診断サーバステータス
cat /run/autosar/diag_server.status
# 出力例:
#   listening=true
#   total_requests=1
#   positive_responses=1
```

### 9.3 ユーザーアプリの実行確認

```bash
# レジストリに登録されたアプリの確認
cat /run/autosar/user_apps_registry.csv

# アプリプロセスの確認
ps aux | grep autosar_user

# ハートビートファイルの確認 (ヘルスチェック有効の場合)
ls -la /run/autosar/user_apps/*.heartbeat
cat /run/autosar/user_apps/exec_signal_template.heartbeat
```

### 9.4 ウォッチドッグの確認

```bash
cat /run/autosar/watchdog.status
```

### 9.5 ネットワーク管理の確認

```bash
cat /run/autosar/network_manager.status
```

---

## 10. systemd なしでの手動デプロイ

Docker コンテナや systemd のない環境では、手動でデーモンを起動できます。

### 10.1 ランタイムディレクトリの作成

```bash
export AUTOSAR_AP_PREFIX=/opt/autosar_ap
export AUTOSAR_RUN_DIR=/run/autosar

sudo mkdir -p ${AUTOSAR_RUN_DIR}/phm/health
sudo mkdir -p ${AUTOSAR_RUN_DIR}/user_apps
sudo mkdir -p ${AUTOSAR_RUN_DIR}/nm_triggers
sudo mkdir -p ${AUTOSAR_RUN_DIR}/can_triggers
sudo mkdir -p /var/autosar/persistency
sudo mkdir -p /var/ucm/staging
sudo mkdir -p /var/ucm/processed
sudo mkdir -p /var/log/autosar
```

### 10.2 環境変数の設定

```bash
export LD_LIBRARY_PATH="${AUTOSAR_AP_PREFIX}/lib:${LD_LIBRARY_PATH:-}"
export VSOMEIP_CONFIGURATION="${AUTOSAR_AP_PREFIX}/configuration/vsomeip-local.json"

# ステータスファイル
export AUTOSAR_PHM_STATUS_FILE="${AUTOSAR_RUN_DIR}/phm.status"
export AUTOSAR_DIAG_STATUS_FILE="${AUTOSAR_RUN_DIR}/diag_server.status"
export AUTOSAR_DLT_STATUS_FILE="${AUTOSAR_RUN_DIR}/dlt_daemon.status"
export AUTOSAR_WATCHDOG_STATUS_FILE="${AUTOSAR_RUN_DIR}/watchdog.status"
export AUTOSAR_NM_STATUS_FILE="${AUTOSAR_RUN_DIR}/network_manager.status"
export AUTOSAR_TIMESYNC_STATUS_FILE="${AUTOSAR_RUN_DIR}/time_sync.status"
export AUTOSAR_SM_STATUS_FILE="${AUTOSAR_RUN_DIR}/sm_state.status"
export AUTOSAR_IAM_STATUS_FILE="${AUTOSAR_RUN_DIR}/iam_policy_loader.status"
export AUTOSAR_PERSISTENCY_STATUS_FILE="${AUTOSAR_RUN_DIR}/persistency_guard.status"
export AUTOSAR_UCM_STATUS_FILE="${AUTOSAR_RUN_DIR}/ucm_daemon.status"
export AUTOSAR_NTP_STATUS_FILE="${AUTOSAR_RUN_DIR}/ntp_time_provider.status"
export AUTOSAR_PTP_STATUS_FILE="${AUTOSAR_RUN_DIR}/ptp_time_provider.status"
export AUTOSAR_CAN_MANAGER_STATUS_FILE="${AUTOSAR_RUN_DIR}/can_manager.status"
export AUTOSAR_PHM_HEALTH_DIR="${AUTOSAR_RUN_DIR}/phm/health"
export AUTOSAR_USER_APP_REGISTRY_FILE="${AUTOSAR_RUN_DIR}/user_apps_registry.csv"
export AUTOSAR_USER_APP_STATUS_DIR="${AUTOSAR_RUN_DIR}/user_apps"
export AUTOSAR_USER_APP_MONITOR_STATUS_FILE="${AUTOSAR_RUN_DIR}/user_app_monitor.status"
export AUTOSAR_NM_TRIGGER_DIR="${AUTOSAR_RUN_DIR}/nm_triggers"
export AUTOSAR_CAN_TRIGGER_DIR="${AUTOSAR_RUN_DIR}/can_triggers"
export AUTOSAR_AP_PERSISTENCY_ROOT="/var/autosar/persistency"
export AUTOSAR_UCM_STAGING_DIR="/var/ucm/staging"
export AUTOSAR_UCM_PROCESSED_DIR="/var/ucm/processed"
export AUTOSAR_IAM_POLICY_FILE="/dev/null"
```

### 10.3 デーモンの手動起動

```bash
BIN="${AUTOSAR_AP_PREFIX}/bin"
CFG="${AUTOSAR_AP_PREFIX}/configuration"
PIDS=""

# --- Tier 1: コアサービス (バックグラウンド起動) ---
${BIN}/autosar_dlt_daemon &
PIDS="$PIDS $!"

${BIN}/autosar_time_sync_daemon &
PIDS="$PIDS $!"

${BIN}/autosar_persistency_guard &
PIDS="$PIDS $!"

${BIN}/autosar_iam_policy_loader &
PIDS="$PIDS $!"

${BIN}/autosar_network_manager &
PIDS="$PIDS $!"

${BIN}/autosar_can_interface_manager &
PIDS="$PIDS $!"

sleep 1

# --- Tier 2: プラットフォーム監視 ---
${BIN}/autosar_sm_state_daemon &
PIDS="$PIDS $!"

${BIN}/autosar_phm_daemon &
PIDS="$PIDS $!"

${BIN}/autosar_diag_server &
PIDS="$PIDS $!"

${BIN}/autosar_crypto_provider &
PIDS="$PIDS $!"

${BIN}/autosar_ucm_daemon &
PIDS="$PIDS $!"

${BIN}/autosar_watchdog_supervisor &
PIDS="$PIDS $!"

${BIN}/autosar_ntp_time_provider &
PIDS="$PIDS $!"

${BIN}/autosar_ptp_time_provider &
PIDS="$PIDS $!"

sleep 1

# --- Tier 3: メインプラットフォームアプリ ---
VSOMEIP_CONFIGURATION="${CFG}/vsomeip-local.json" \
  ${BIN}/adaptive_autosar \
    "${CFG}/execution_manifest.arxml" \
    "${CFG}/extended_vehicle_manifest.arxml" \
    "${CFG}/diagnostic_manager_manifest.arxml" \
    "${CFG}/health_monitoring_manifest.arxml" &
PIDS="$PIDS $!"

sleep 2

# --- Tier 4: ユーザーアプリ監視 ---
${BIN}/autosar_user_app_monitor &
PIDS="$PIDS $!"

echo "AUTOSAR AP platform started. PIDs: ${PIDS}"
echo "Press Ctrl+C to stop."

# 停止処理
trap "echo 'Stopping...'; kill ${PIDS} 2>/dev/null; wait; echo 'Stopped.'" EXIT INT TERM
wait
```

### 10.4 ユーザーアプリの手動起動

プラットフォームデーモンが起動した状態で、ユーザーアプリを起動:

```bash
# bringup.sh を直接実行
export AUTOSAR_USER_APPS_BUILD_DIR=/opt/autosar_ap/user_apps_build
/etc/autosar/bringup.sh &

# または個別にアプリを直接実行
${AUTOSAR_AP_PREFIX}/user_apps_build/src/apps/basic/autosar_user_raspi_ecu_app \
  --period-ms=100 &
```

---

## 11. ユーザーアプリ開発ガイド

### 11.1 テンプレートの利用

`/opt/autosar_ap/user_apps/` にソーステンプレートがインストールされています:

```bash
ls /opt/autosar_ap/user_apps/src/apps/basic/
# raspi_ecu_app.cpp              ← 本番 ECU アプリテンプレート
# exec_signal_template_app.cpp   ← シグナルハンドリングテンプレート
# per_phm_template_app.cpp       ← 永続化 + PHM テンプレート
```

### 11.2 新しいアプリの追加

```cpp
// /opt/autosar_ap/user_apps/src/apps/basic/my_vehicle_app.cpp

#include "ara/core/initialization.h"
#include "ara/exec/signal_handler.h"
#include "ara/exec/application_client.h"
#include "ara/phm/health_channel.h"
#include "ara/log/logger.h"

int main()
{
    auto init = ara::core::Initialize();
    if (!init.HasValue()) return 1;

    ara::exec::SignalHandler::Register();

    // Execution Manager に状態レポート
    auto appSpec = ara::core::InstanceSpecifier::Create(
        "MyApps/VehicleApp/App").Value();
    ara::exec::ApplicationClient client(appSpec);
    client.ReportApplicationHealth();

    // PHM ヘルスチャネル
    auto phmSpec = ara::core::InstanceSpecifier::Create(
        "MyApps/VehicleApp/Health").Value();
    ara::phm::HealthChannel health(phmSpec);

    // ロガー
    auto logSpec = ara::core::InstanceSpecifier::Create(
        "MyApps/VehicleApp/Log").Value();
    ara::log::Logger logger(logSpec);

    logger.LogInfo() << "VehicleApp started";

    while (!ara::exec::SignalHandler::IsTerminationRequested())
    {
        client.ReportApplicationHealth();
        (void)health.ReportHealthStatus(ara::phm::HealthStatus::kOk);

        // アプリケーションロジック
        // ...

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    logger.LogInfo() << "VehicleApp terminating";
    (void)ara::core::Deinitialize();
    return 0;
}
```

CMakeLists.txt に追加:

```cmake
# /opt/autosar_ap/user_apps/src/apps/basic/CMakeLists.txt に追記
add_user_template_target(
  my_vehicle_app
  my_vehicle_app.cpp
  AdaptiveAutosarAP::ara_core
  AdaptiveAutosarAP::ara_exec
  AdaptiveAutosarAP::ara_phm
  AdaptiveAutosarAP::ara_log
)
```

リビルド:

```bash
sudo ./scripts/build_user_apps_from_opt.sh \
  --prefix /opt/autosar_ap \
  --source-dir /opt/autosar_ap/user_apps \
  --build-dir /opt/autosar_ap/user_apps_build
```

### 11.3 bringup.sh に登録

```bash
sudo vi /etc/autosar/bringup.sh

# 追加:
MY_VEHICLE_APP="${AUTOSAR_USER_APPS_BUILD_DIR}/src/apps/basic/my_vehicle_app"
launch_app_with_heartbeat "my_vehicle_app" \
  "${AUTOSAR_USER_APP_STATUS_DIR}/$(sanitize_name "my_vehicle_app").heartbeat" \
  5000 \
  "${MY_VEHICLE_APP}"
```

```bash
# サービスを再起動して反映
sudo systemctl restart autosar-exec-manager.service

# 確認
cat /run/autosar/user_apps_registry.csv
journalctl -u autosar-exec-manager.service --no-pager -n 20
```

---

## 12. トラブルシューティング

### ビルドエラー: OpenSSL が見つからない

```bash
sudo apt install -y libssl-dev
```

### ミドルウェアインストールで ACL エラー

Docker コンテナ内では ACL が使えない場合があります。iceoryx のインストールスクリプトが自動で対処しますが、問題が続く場合:

```bash
sudo apt install -y libacl1-dev
```

### adaptive_autosar が "Invalid ARXML document file!" で終了する

ARXML ファイルのパスが正しくありません。4 つのマニフェストファイルを引数で渡してください:

```bash
/opt/autosar_ap/bin/adaptive_autosar \
  /opt/autosar_ap/configuration/execution_manifest.arxml \
  /opt/autosar_ap/configuration/extended_vehicle_manifest.arxml \
  /opt/autosar_ap/configuration/diagnostic_manager_manifest.arxml \
  /opt/autosar_ap/configuration/health_monitoring_manifest.arxml
```

### サービスが起動しない

```bash
# ジャーナルでエラーを確認
journalctl -u autosar-<service-name>.service --no-pager -n 50

# ラッパースクリプトの内容を確認
cat /usr/local/bin/autosar-<service-name>-wrapper.sh

# バイナリが存在するか確認
ls -la /opt/autosar_ap/bin/<binary-name>

# ライブラリが見つかるか確認
ldd /opt/autosar_ap/bin/<binary-name>
```

### ライブラリが見つからない (ld.so エラー)

```bash
# LD_LIBRARY_PATH を確認
echo $LD_LIBRARY_PATH

# 永続的に設定
echo "/opt/autosar_ap/lib" | sudo tee /etc/ld.so.conf.d/autosar-ap.conf
sudo ldconfig
```

### ユーザーアプリが再起動を繰り返す

`user_app_monitor` が異常終了を検出して再起動しています。
再起動ポリシーを確認:

```bash
# レジストリで restart_limit と restart_window_ms を確認
cat /run/autosar/user_apps_registry.csv

# アプリのログを確認
journalctl -u autosar-exec-manager.service --no-pager -n 100
```

### ポート 13400 が使用中

```bash
# 使用中のプロセスを確認
sudo lsof -i :13400

# 別のポートを使用
sudo vi /etc/default/autosar-diag-server
# AUTOSAR_DIAG_LISTEN_PORT=13401
sudo systemctl restart autosar-diag-server.service
```
