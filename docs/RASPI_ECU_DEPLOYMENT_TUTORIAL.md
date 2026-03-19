# Raspberry Pi ECU デプロイメント チュートリアル

Raspberry Pi を AUTOSAR Adaptive Platform ベースの実 ECU として動作させるための包括的なガイドです。
Ubuntu (Linux) と QNX 8.0 の両方のケースをカバーします。

---

## 目次

1. [概要](#1-概要)
2. [アーキテクチャ概要](#2-アーキテクチャ概要)
3. [Ubuntu (Linux) でのセットアップ](#3-ubuntu-linux-でのセットアップ)
4. [QNX 8.0 でのセットアップ](#4-qnx-80-でのセットアップ)
5. [Execution Manager 詳細解説](#5-execution-manager-詳細解説)
6. [ユーザーアプリ開発ガイド](#6-ユーザーアプリ開発ガイド)
7. [トラブルシューティング](#7-トラブルシューティング)

---

## 1. 概要

### このチュートリアルで実現すること

Raspberry Pi 上で以下の AUTOSAR Adaptive Platform スタックを実動作させます:

| コンポーネント | 説明 |
|------------|------|
| 17 個のプラットフォームデーモン | 診断 (UDS/DoIP)、PHM、暗号、UCM 等 |
| Execution Manager | ユーザーアプリのプロセス管理・監視・復旧 |
| ユーザーアプリケーション | 車両センサー読み取り、状態管理、ヘルスレポート |

**実際の ECU として動作させます。**各プラットフォーム機能は独立したデーモンプロセスとして実行され、
ユーザーアプリは Execution Manager の管理下でライフサイクル制御されます。

### 対応ハードウェア

| ボード | RAM | 備考 |
|--------|-----|------|
| Raspberry Pi 4 Model B | 4GB 以上推奨 | オンデバイスビルド可能 |
| Raspberry Pi 5 | 8GB 推奨 | ビルド高速 |
| Raspberry Pi 3 Model B+ | 2GB | クロスビルド推奨 |

### プラットフォームデーモン一覧

本プロジェクトでビルドされる 17 個のプラットフォームデーモン:

| バイナリ名 | 機能 | ソースファイル |
|-----------|------|-------------|
| `adaptive_autosar` | メインプラットフォームアプリ (EM/SM 統合) | `src/main.cpp` |
| `autosar_vsomeip_routing_manager` | SOME/IP ルーティングマネージャ | `src/main_vsomeip_routing_manager.cpp` |
| `autosar_time_sync_daemon` | 時刻同期デーモン (NTP/PTP) | `src/main_time_sync_daemon.cpp` |
| `autosar_persistency_guard` | 永続ストレージ管理 | `src/main_persistency_guard.cpp` |
| `autosar_iam_policy_loader` | IAM ポリシーローダ | `src/main_iam_policy_loader.cpp` |
| `autosar_can_interface_manager` | CAN インターフェース管理 | `src/main_can_interface_manager.cpp` |
| `autosar_watchdog_supervisor` | プロセスウォッチドッグ監視 | `src/main_watchdog_supervisor.cpp` |
| `autosar_user_app_monitor` | ユーザーアプリ健全性監視 | `src/main_user_app_monitor.cpp` |
| `autosar_sm_state_daemon` | ステートマシン状態通知 | `src/main_sm_state_daemon.cpp` |
| `autosar_ntp_time_provider` | NTP 時刻プロバイダ | `src/main_ntp_time_provider.cpp` |
| `autosar_ptp_time_provider` | PTP 時刻プロバイダ | `src/main_ptp_time_provider.cpp` |
| `autosar_network_manager` | ネットワーク管理 (NM) | `src/main_network_manager.cpp` |
| `autosar_ucm_daemon` | ソフトウェア更新管理 (UCM) | `src/main_ucm_daemon.cpp` |
| `autosar_dlt_daemon` | 診断ログトレース (DLT) | `src/main_dlt_daemon.cpp` |
| `autosar_diag_server` | 診断サーバ (UDS/DoIP) | `src/main_diag_server.cpp` |
| `autosar_phm_daemon` | プラットフォーム健全性管理 | `src/main_phm_daemon.cpp` |
| `autosar_crypto_provider` | 暗号サービスプロバイダ | `src/main_crypto_provider.cpp` |

すべてのソースファイルが揃っており、追加のソースコード作成は不要です。

---

## 2. アーキテクチャ概要

### プロセス構成

```
┌─────────────────────────────────────────────────────────────┐
│                   Raspberry Pi ECU                          │
│                                                             │
│  ┌─────────────────────────────────────────────────────┐    │
│  │ プラットフォームデーモン層                              │    │
│  │                                                     │    │
│  │  vsomeip_routing  time_sync  persistency_guard      │    │
│  │  iam_policy       can_mgr   watchdog_supervisor     │    │
│  │  sm_state         phm       diag_server             │    │
│  │  crypto_provider  ucm       dlt_daemon              │    │
│  │  network_manager  ntp/ptp_time_provider             │    │
│  │  user_app_monitor                                   │    │
│  └─────────────────────┬───────────────────────────────┘    │
│                        │                                    │
│  ┌─────────────────────┴───────────────────────────────┐    │
│  │ Execution Manager (adaptive_autosar)                │    │
│  │  ┌─────────────┐  ┌──────────────┐  ┌───────────┐  │    │
│  │  │ マニフェスト  │  │ プロセス管理   │  │ FG 状態   │  │    │
│  │  │ ローダ       │  │ (fork/exec)  │  │ マシン    │  │    │
│  │  └─────────────┘  └──────────────┘  └───────────┘  │    │
│  └─────────────────────┬───────────────────────────────┘    │
│                        │                                    │
│  ┌─────────────────────┴───────────────────────────────┐    │
│  │ ユーザーアプリケーション層                              │    │
│  │                                                     │    │
│  │  raspi_ecu_app    sensor_fusion    vehicle_monitor   │    │
│  │  (ユーザーが開発したアプリケーション)                    │    │
│  └─────────────────────────────────────────────────────┘    │
│                                                             │
│  ┌─────────────────────────────────────────────────────┐    │
│  │ ara::* ライブラリ (共有ライブラリ)                      │    │
│  │  ara_core  ara_log  ara_com  ara_exec  ara_diag     │    │
│  │  ara_phm   ara_per  ara_crypto  ara_sm  ara_tsync   │    │
│  │  ara_ucm   ara_nm   ara_iam                         │    │
│  └─────────────────────────────────────────────────────┘    │
└─────────────────────────────────────────────────────────────┘
```

### Execution Manager のプロセス管理フロー

```
1. マニフェスト (ARXML) を読み込み → ManifestLoader
2. エントリを検証 (循環依存チェック、参照検証)
3. スタートアップ優先度 + 依存関係でソート
4. ExecutionManager に ProcessDescriptor を登録
5. ActivateFunctionGroup("MachineFG", "Running") 呼び出し
   └─ 対象プロセスを fork/exec (Linux) または posix_spawn (QNX) で起動
6. モニタスレッドが子プロセスを waitpid で監視
7. 各アプリが ExecutionClient.ReportExecutionState(kRunning) で報告
8. PHM が SupervisedEntity を通じてヘルスチェック
9. 異常検出時に RecoveryAction (再起動 / FG 状態遷移) を実行
```

### ユーザーアプリの登録方法 (2 通り)

| 方式 | 説明 | 推奨用途 |
|------|------|---------|
| **bringup.sh 方式** | シェルスクリプトで `launch_app` / `launch_app_managed` を呼び出し | 開発・プロトタイピング |
| **ARXML マニフェスト方式** | XML で ProcessDescriptor を定義し EM に自動ロードさせる | 本番運用 |

---

## 3. Ubuntu (Linux) でのセットアップ

### 3.1 Ubuntu Server のインストール

Raspberry Pi に Ubuntu Server 24.04 LTS (64-bit) をインストールします。

```bash
# 1. Raspberry Pi Imager で Ubuntu Server 24.04 LTS (64-bit) を書き込み
#    https://www.raspberrypi.com/software/

# 2. 初回起動後 SSH 接続
ssh ubuntu@raspi-ecu.local

# 3. システム更新
sudo apt update && sudo apt full-upgrade -y
sudo reboot
```

### 3.2 依存パッケージのインストール

```bash
sudo apt install -y \
  build-essential \
  cmake \
  git \
  python3 \
  python3-pip \
  python3-yaml \
  libssl-dev \
  libboost-all-dev \
  can-utils \
  iproute2 \
  net-tools \
  ethtool \
  curl \
  wget \
  unzip \
  pkg-config

# RAM 4GB 未満の場合はスワップ拡張
sudo fallocate -l 4G /swapfile
sudo chmod 600 /swapfile
sudo mkswap /swapfile
sudo swapon /swapfile
echo '/swapfile none swap sw 0 0' | sudo tee -a /etc/fstab
```

### 3.3 リポジトリのクローンとビルド

```bash
cd ~
git clone https://github.com/<your-org>/Adaptive-AUTOSAR.git
cd Adaptive-AUTOSAR
```

### 3.4 ミドルウェアのインストール

```bash
# 一括インストール (所要時間: Pi 4 で約 30-90 分)
sudo ./scripts/install_middleware_stack.sh --install-base-deps

# 個別インストールも可能
sudo ./scripts/install_dependency.sh
sudo ./scripts/install_vsomeip.sh
sudo ./scripts/install_iceoryx.sh
sudo ./scripts/install_cyclonedds.sh

# インストール確認
ls /opt/vsomeip/lib/libvsomeip3*
ls /opt/iceoryx/lib/libiceoryx*
ls /opt/cyclonedds/lib/libddsc*
```

### 3.5 AUTOSAR AP のビルドとインストール

```bash
# 統合ビルドスクリプト (推奨)
sudo ./scripts/build_and_install_rpi_ecu_profile.sh \
  --prefix /opt/autosar-ap \
  --runtime-build-dir build-rpi-autosar-ap \
  --user-app-build-dir /opt/autosar-ap/user_apps_build \
  --install-middleware

# ビルド所要時間:
#   Pi 4 (4GB): 約 15-30 分
#   Pi 5 (8GB): 約 8-15 分
```

ビルド結果の確認:

```bash
# プラットフォームデーモンが揃っているか確認
ls /opt/autosar-ap/bin/
# 以下が存在すること:
#   adaptive_autosar
#   autosar_vsomeip_routing_manager
#   autosar_time_sync_daemon
#   autosar_persistency_guard
#   autosar_iam_policy_loader
#   autosar_can_interface_manager
#   autosar_watchdog_supervisor
#   autosar_user_app_monitor
#   autosar_sm_state_daemon
#   autosar_ntp_time_provider
#   autosar_ptp_time_provider
#   autosar_network_manager
#   autosar_ucm_daemon
#   autosar_dlt_daemon
#   autosar_diag_server
#   autosar_phm_daemon
#   autosar_crypto_provider

# ユーザーアプリ
ls /opt/autosar-ap/user_apps_build/src/apps/basic/
# 以下が存在すること:
#   autosar_user_raspi_ecu       ← 本番用 ECU アプリ
#   autosar_user_minimal_runtime
#   autosar_user_exec_signal_template
#   autosar_user_per_phm_demo

# ライブラリ
ls /opt/autosar-ap/lib/
```

### 3.6 ライブラリパスの設定

```bash
echo '/opt/autosar-ap/lib' | sudo tee /etc/ld.so.conf.d/autosar-ap.conf
echo '/opt/vsomeip/lib' | sudo tee -a /etc/ld.so.conf.d/autosar-ap.conf
echo '/opt/iceoryx/lib' | sudo tee -a /etc/ld.so.conf.d/autosar-ap.conf
echo '/opt/cyclonedds/lib' | sudo tee -a /etc/ld.so.conf.d/autosar-ap.conf
sudo ldconfig
```

### 3.7 systemd サービスのインストール

```bash
sudo ./scripts/install_rpi_ecu_services.sh \
  --prefix /opt/autosar-ap \
  --user-app-build-dir /opt/autosar-ap/user_apps_build \
  --enable
```

このスクリプトが行うこと:

1. 20 個の systemd サービスユニットを `/etc/systemd/system/` にインストール
2. 各サービスのラッパースクリプトを `/usr/local/bin/` に生成
3. 環境変数ファイルを `/etc/default/autosar-*` に配置
4. ユーザーアプリブリングアップスクリプトを `/etc/autosar/bringup.sh` に配置
5. `systemctl enable` で自動起動を設定

### 3.8 サービスの起動と確認

```bash
# 1. iceoryx RouDi (最初に起動)
sudo systemctl start autosar-iox-roudi.service
sleep 2

# 2. SOME/IP ルーティングマネージャ
sudo systemctl start autosar-vsomeip-routing.service
sleep 2

# 3. コアサービス群
sudo systemctl start autosar-time-sync.service
sudo systemctl start autosar-persistency-guard.service
sudo systemctl start autosar-iam-policy.service
sudo systemctl start autosar-can-manager.service
sudo systemctl start autosar-network-manager.service

# 4. プラットフォームアプリケーション
sudo systemctl start autosar-platform-app.service
sleep 3

# 5. 監視・補助サービス
sudo systemctl start autosar-watchdog.service
sudo systemctl start autosar-phm-daemon.service
sudo systemctl start autosar-diag-server.service
sudo systemctl start autosar-crypto-provider.service
sudo systemctl start autosar-ucm.service
sudo systemctl start autosar-dlt.service
sudo systemctl start autosar-sm-state.service
sudo systemctl start autosar-ntp-time-provider.service

# 6. ユーザーアプリ監視 + ブリングアップ
sudo systemctl start autosar-user-app-monitor.service
sudo systemctl start autosar-exec-manager.service

# 一括確認
systemctl status 'autosar-*.service' --no-pager | grep -E '(●|Active:)'
```

### 3.9 ユーザーアプリの登録と実行管理

#### 方式 A: bringup.sh によるアプリ登録 (推奨)

`/etc/autosar/bringup.sh` を編集してユーザーアプリを追加します:

```bash
sudo nano /etc/autosar/bringup.sh
```

```bash
#!/usr/bin/env bash
set -euo pipefail

# ... (既存のヘルパー関数は自動生成済み) ...

APP_DIR="${AUTOSAR_USER_APPS_BUILD_DIR:-/opt/autosar-ap/user_apps_build}"

# --- 例 1: 基本的な起動 (PID 監視のみ) ---
launch_app "raspi_ecu" \
    "${APP_DIR}/src/apps/basic/autosar_user_raspi_ecu"

# --- 例 2: ハートビート監視付き起動 ---
launch_app_with_heartbeat \
    "my_sensor_app" \
    "/run/autosar/user_apps/my_sensor_app.heartbeat" \
    5000 \
    "${APP_DIR}/src/apps/feature/ecu/sensor_fusion_app"

# --- 例 3: フル管理モード (インスタンス指定 + リスタートポリシー) ---
launch_app_managed \
    "vehicle_monitor" \
    "AdaptiveAutosar/UserApps/VehicleMonitor" \
    "/run/autosar/user_apps/vehicle_monitor.heartbeat" \
    3000 \
    5 \
    60000 \
    "${APP_DIR}/src/apps/feature/ecu/vehicle_monitor_app"
```

登録後のサービス再起動:

```bash
sudo systemctl restart autosar-exec-manager.service

# 登録状態の確認
cat /run/autosar/user_apps_registry.csv
```

`user_apps_registry.csv` の出力例:

```csv
# name,pid,heartbeat_file,heartbeat_timeout_ms,instance_specifier,restart_limit,restart_window_ms,restart_command
raspi_ecu,12345,,0,AdaptiveAutosar/UserApps/raspi_ecu,5,60000,/opt/autosar-ap/user_apps_build/src/apps/basic/autosar_user_raspi_ecu
my_sensor_app,12346,/run/autosar/user_apps/my_sensor_app.heartbeat,5000,AdaptiveAutosar/UserApps/my_sensor_app,5,60000,/opt/autosar-ap/user_apps_build/src/apps/feature/ecu/sensor_fusion_app
```

#### 方式 B: ARXML マニフェストによるアプリ登録

ARXML ファイルでプロセスを宣言的に定義し、Execution Manager に自動ロードさせます。

サンプルマニフェスト (`deployment/rpi_ecu/config/execution_manifest_example.arxml`):

```xml
<PROCESS-DESIGN>
  <SHORT-NAME>RaspiEcuApp</SHORT-NAME>
  <EXECUTABLE-REF>/opt/autosar-ap/user_apps_build/src/apps/basic/autosar_user_raspi_ecu</EXECUTABLE-REF>
  <FUNCTION-GROUP-REF>MachineFG</FUNCTION-GROUP-REF>
  <STATE-REF>Running</STATE-REF>
  <STARTUP-PRIORITY>10</STARTUP-PRIORITY>
  <STARTUP-GRACE-MS>5000</STARTUP-GRACE-MS>
  <TERMINATION-TIMEOUT-MS>5000</TERMINATION-TIMEOUT-MS>
</PROCESS-DESIGN>
```

マニフェストの各フィールド:

| フィールド | 説明 | デフォルト |
|-----------|------|----------|
| `SHORT-NAME` | プロセスの一意な名前 | (必須) |
| `EXECUTABLE-REF` | バイナリの絶対パス | (必須) |
| `FUNCTION-GROUP-REF` | 所属する Function Group | (必須) |
| `STATE-REF` | この FG 状態でのみ起動される | (必須) |
| `STARTUP-PRIORITY` | 起動優先度 (小さい = 先に起動) | 100 |
| `STARTUP-GRACE-MS` | 起動猶予期間 (ms) | 3000 |
| `TERMINATION-TIMEOUT-MS` | SIGTERM 後の SIGKILL までの待ち時間 | 5000 |
| `CPU-QUOTA-PERCENT` | CPU 使用率制限 (0 = 無制限) | 0 |
| `MEMORY-LIMIT-BYTES` | メモリ制限 (0 = 無制限) | 0 |
| `DEPENDS-ON` | 依存プロセス名 (カンマ区切り) | (なし) |

マニフェストを EM に渡す:

```bash
export AUTOSAR_EM_MANIFEST=/opt/autosar-ap/etc/execution_manifest.arxml
sudo systemctl restart autosar-exec-manager.service
```

### 3.10 動作確認

```bash
# 全サービスのステータス
systemctl status 'autosar-*.service' --no-pager

# ECU アプリのステータスファイル
cat /run/autosar/ecu_app.status

# PHM の健全性ステータス
cat /run/autosar/phm.status

# 診断サーバの接続テスト (DoIP ポート 13400)
echo -ne '\x10\x01' | nc -w 1 localhost 13400 | xxd

# ユーザーアプリ監視ステータス
cat /run/autosar/user_app_monitor.status

# リアルタイムログ
sudo journalctl -u 'autosar-*' -f
```

---

## 4. QNX 8.0 でのセットアップ

### 4.1 前提条件

- **ホストマシン**: Linux (x86_64) — クロスビルドに使用
- **QNX SDP 8.0** がインストール済み
- ターゲット: Raspberry Pi 4/5 または QNX 対応 ARM64 ボード

```bash
# QNX SDK 環境を読み込み
source ~/qnx800/qnxsdp-env.sh

# 環境変数の確認
echo "QNX_HOST=${QNX_HOST}"
echo "QNX_TARGET=${QNX_TARGET}"
```

### 4.2 ミドルウェアのクロスビルド

```bash
cd ~/Adaptive-AUTOSAR

# ミドルウェア格納先を準備
sudo mkdir -p /opt/qnx && sudo chmod 777 /opt/qnx

# 一括クロスビルド
./qnx/scripts/build_libraries_qnx.sh all install

# 個別ビルドの場合
./qnx/scripts/build_libraries_qnx.sh cyclonedds install --disable-shm
./qnx/scripts/build_libraries_qnx.sh vsomeip install

# メモリ不足時は並列数を制限
export AUTOSAR_QNX_JOBS=2
./qnx/scripts/build_libraries_qnx.sh all install
```

### 4.3 AUTOSAR AP のクロスビルド

```bash
# プラットフォームランタイムのクロスビルド
./qnx/scripts/build_autosar_ap_qnx.sh install
# インストール先: /opt/qnx/autosar-ap/aarch64le/

# ユーザーアプリのクロスビルド
./qnx/scripts/build_user_apps_qnx.sh build

# 一括ビルド (上記すべてを順次実行)
./qnx/scripts/build_all_qnx.sh install
```

### 4.4 デプロイイメージの作成

```bash
./qnx/scripts/create_qnx_deploy_image.sh --target-ip 192.168.1.100
```

出力 (`out/qnx/deploy/`):

| ファイル | 説明 |
|---------|------|
| `autosar_ap-aarch64le.tar.gz` | 展開可能なアーカイブ (推奨) |
| `autosar_ap-aarch64le.ifs` | QNX IFS イメージ (`mkifs` が必要) |

デプロイイメージに含まれるもの:

```
autosar_ap-aarch64le/
├── bin/                    ← 17 プラットフォームデーモン + ユーザーアプリ
├── lib/                    ← vsomeip / CycloneDDS / iceoryx 共有ライブラリ
├── etc/
│   ├── vsomeip_routing.json
│   ├── vsomeip_client.json
│   └── slm/
│       └── autosar.xml     ← SLM 自動起動設定
├── startup.sh              ← 手動起動スクリプト
├── rc.autosar.sh           ← rc.d 自動起動スクリプト
├── bringup_qnx.sh          ← ユーザーアプリブリングアップ
├── env.sh                  ← 環境変数設定
├── run/                    ← ランタイムディレクトリ
├── var/                    ← 永続ストレージ / UCM
└── log/                    ← ログ出力先
```

### 4.5 QNX ターゲットへのデプロイ

```bash
# SSH 経由で自動転送・展開
./qnx/scripts/deploy_to_qnx_target.sh --host 192.168.1.100

# オプション
./qnx/scripts/deploy_to_qnx_target.sh \
  --host 192.168.1.100 \
  --user root \
  --remote-dir /autosar \
  --target-ip 192.168.1.100
```

デプロイスクリプトが行うこと:

1. `tar.gz` を SCP でターゲットに転送
2. SSH でターゲットに接続し `/autosar` に展開
3. vsomeip の unicast IP をターゲット IP に書き換え
4. 実行権限を設定

### 4.6 QNX での自動起動設定

QNX には systemd がないため、以下の方法で自動起動を実現します。

#### 方式 A: SLM (Service Launch Manager) — 推奨

QNX 8.0 の SLM は systemd に相当するサービス管理機構です。
依存関係の解決、自動再起動、優先度制御をサポートします。

```bash
# ターゲットに SSH 接続
ssh root@192.168.1.100

# SLM 設定をインストール
mkdir -p /etc/slm/config
cp /autosar/etc/slm/autosar.xml /etc/slm/config/

# SLM を再読み込み (すでに起動していれば)
slay -f slm
slm &

# SLM が起動時に自動起動するようにする
# /etc/rc.d/rc.local に以下を追加:
echo 'slm &' >> /etc/rc.d/rc.local
```

SLM 設定ファイル (`/autosar/etc/slm/autosar.xml`) の構造:

```
Tier 0: autosar-vsomeip-routing     ← 最初に起動 (全サービスが依存)
         │
Tier 1: time-sync, persistency, iam, network-mgr, can-mgr, dlt
         │
Tier 2: sm-state, watchdog, phm, diag, crypto, ucm, ntp/ptp
         │
Tier 3: adaptive_autosar (メインプラットフォーム)
         │
Tier 4: user-app-monitor → user-bringup (ユーザーアプリ起動)
```

SLM コンポーネントの依存関係:

```xml
<SLM:component name="autosar-platform-app">
  <SLM:command launch="/autosar/bin/adaptive_autosar" priority="15" type="simple">
    <SLM:env var="LD_LIBRARY_PATH" value="/autosar/lib"/>
    <SLM:env var="VSOMEIP_CONFIGURATION" value="/autosar/etc/vsomeip_client.json"/>
  </SLM:command>
  <!-- これらのサービスが起動してから起動 -->
  <SLM:dependency component="autosar-vsomeip-routing"/>
  <SLM:dependency component="autosar-time-sync"/>
  <SLM:dependency component="autosar-persistency-guard"/>
  <SLM:dependency component="autosar-iam-policy"/>
  <SLM:restart action="always" delay="3000" max="3"/>
</SLM:component>
```

SLM 設定の各要素:

| 要素 | 説明 |
|------|------|
| `<SLM:command launch="...">` | 実行するバイナリのパス |
| `priority` | 起動優先度 (小さい = 高優先) |
| `type="simple"` | 通常のデーモン (バックグラウンド実行) |
| `type="oneshot"` | 一回実行して終了 (bringup 用) |
| `<SLM:env>` | 環境変数の設定 |
| `<SLM:dependency>` | 依存コンポーネント (先に起動必須) |
| `<SLM:restart action="always">` | 異常終了時に自動再起動 |
| `delay` | 再起動までの待ち時間 (ms) |
| `max` | 最大再起動回数 |

#### 方式 B: rc.d スクリプト — SLM 非搭載環境向け

SLM が利用できない QNX 環境では、`rc.autosar.sh` を使用します。

```bash
# ターゲットに SSH 接続
ssh root@192.168.1.100

# 方法 1: rc.d に登録
cp /autosar/rc.autosar.sh /etc/rc.d/rc.autosar
chmod +x /etc/rc.d/rc.autosar

# /etc/rc.d/rc.local に追加
echo '/etc/rc.d/rc.autosar start &' >> /etc/rc.d/rc.local
```

```bash
# 方法 2: IFS ビルドファイルに組み込み (組み込みシステム向け)
# buildfile (.build) に以下を追加:
#
# [+script] .script = {
#     procnto-smp -v
#     PATH=/proc/boot:/autosar/bin:/usr/bin:/bin
#     LD_LIBRARY_PATH=/autosar/lib:/usr/lib:/lib
#     /autosar/rc.autosar.sh start &
# }
```

rc.autosar.sh の操作コマンド:

```bash
# 手動起動
/autosar/rc.autosar.sh start

# 全停止
/autosar/rc.autosar.sh stop

# ステータス確認
/autosar/rc.autosar.sh status

# 再起動
/autosar/rc.autosar.sh restart
```

#### 方式 C: startup.sh による手動起動

開発時やデバッグ時には手動起動も可能です:

```bash
ssh root@192.168.1.100

# 全デーモン起動
/autosar/startup.sh --all

# vsomeip ルーティングマネージャのみ
/autosar/startup.sh --routing-only

# 停止
/autosar/startup.sh --stop
```

### 4.7 QNX でのユーザーアプリの登録と実行管理

QNX でもユーザーアプリの登録方法は Linux と同様です。

#### bringup_qnx.sh の編集

```bash
ssh root@192.168.1.100
vi /autosar/bringup_qnx.sh
```

```sh
# --- ユーザーアプリの追加 ---

# 基本起動
launch_app "raspi_ecu" \
    "/autosar/bin/autosar_user_raspi_ecu"

# ハートビート監視付き
launch_app_with_heartbeat \
    "sensor_app" \
    "/autosar/run/autosar/user_apps/sensor_app.heartbeat" \
    5000 \
    "/autosar/bin/sensor_fusion_app"

# フル管理モード
launch_app_managed \
    "vehicle_monitor" \
    "AdaptiveAutosar/UserApps/VehicleMonitor" \
    "/autosar/run/autosar/user_apps/vehicle_monitor.heartbeat" \
    3000 \
    5 \
    60000 \
    "/autosar/bin/vehicle_monitor_app"
```

#### 新しいユーザーアプリのデプロイ手順

1. ホストマシンでクロスビルド:
```bash
# ホストマシンで
./qnx/scripts/build_user_apps_qnx.sh build
```

2. ターゲットにバイナリを転送:
```bash
scp out/qnx/work/build/user_apps-aarch64le/src/apps/basic/autosar_user_raspi_ecu \
    root@192.168.1.100:/autosar/bin/
```

3. `bringup_qnx.sh` にアプリを追加 (上記参照)

4. サービスを再起動:
```bash
# SLM 方式の場合
slay -f autosar-user-bringup
# SLM が自動で再起動

# rc.d 方式の場合
/autosar/rc.autosar.sh restart
```

### 4.8 QNX での動作確認

```bash
ssh root@192.168.1.100

# プロセス一覧
pidin | grep autosar

# ステータスファイル確認
cat /autosar/run/autosar/phm.status
cat /autosar/run/autosar/diag_server.status
cat /autosar/run/autosar/user_app_monitor.status

# vsomeip 通信確認
ls /tmp/vsomeip-* 2>/dev/null

# PID ファイル確認 (rc.d 方式)
ls /autosar/run/pids/

# 手動でユーザーアプリを実行
. /autosar/env.sh
VSOMEIP_CONFIGURATION=/autosar/etc/vsomeip_client.json \
    /autosar/bin/autosar_user_raspi_ecu
```

---

## 5. Execution Manager 詳細解説

### 5.1 ProcessDescriptor の構造

Execution Manager が管理するプロセスの定義:

```cpp
struct ProcessDescriptor {
    std::string name;               // プロセスの一意な名前
    std::string executable;         // バイナリの絶対パス
    std::string functionGroup;      // 所属する Function Group
    std::string activeState;        // この FG 状態で起動される
    std::vector<std::string> arguments;  // コマンドライン引数
    std::chrono::milliseconds startupGrace{3000};    // 起動猶予期間
    std::chrono::milliseconds terminationTimeout{5000}; // 終了タイムアウト
};
```

拡張マニフェストエントリ (`ManifestProcessEntry`):

```cpp
struct ManifestProcessEntry {
    ProcessDescriptor Descriptor;
    std::map<std::string, std::string> EnvironmentVariables;  // 環境変数
    uint32_t CpuQuotaPercent;       // CPU 使用率制限 (0 = 無制限)
    uint64_t MemoryLimitBytes;      // メモリ制限 (0 = 無制限)
    std::vector<std::string> Dependencies;  // 依存プロセス名
    uint32_t StartupPriority;       // 起動優先度 (小さい = 先に起動)
};
```

### 5.2 Function Group と状態遷移

Function Group (FG) は関連するプロセスをグループ化し、状態遷移で一括制御します。

```
MachineFG 状態遷移図:

  ┌─────────┐     ActivateFG("Running")     ┌─────────┐
  │ Startup │ ────────────────────────────→ │ Running │
  └─────────┘                                └────┬────┘
       ↑                                          │
       │                                     SetState()
       │              ┌───────────┐               │
       │              │ Diagnostic│ ←─────────────┤
       │              └───────────┘               │
       │              ┌───────────┐               │
       │              │  Update   │ ←─────────────┤
       │              └───────────┘               │
       │                                          │
       │              ┌───────────┐               │
       └──────────── │ Shutdown  │ ←─────────────┘
                      └───────────┘
```

状態遷移時の動作:

1. **Running → Diagnostic**: Running 状態のプロセスを停止し、Diagnostic 状態のプロセスを起動
2. **Running → Update**: Running 状態のプロセスを停止し、Update 状態のプロセスを起動
3. **→ Shutdown**: すべてのプロセスに SIGTERM を送信し、タイムアウト後に SIGKILL

### 5.3 プロセスライフサイクル

```
                    RegisterProcess()
                         │
                         ▼
┌──────────────┐   ActivateFG()   ┌──────────┐
│ kNotRunning  │ ───────────────→ │ kStarting│
└──────────────┘                  └────┬─────┘
       ↑                               │
       │                    ExecutionClient.ReportState(kRunning)
       │                               │
       │                               ▼
       │                         ┌──────────┐
       │                         │ kRunning │ ← ヘルスチェック (PHM)
       │                         └────┬─────┘
       │                              │
       │               SIGTERM / TerminateFG()
       │                              │
       │                              ▼
       │                     ┌──────────────┐
       │                     │ kTerminating │
       │                     └──────┬───────┘
       │                            │
       │              ┌─────────────┴──────────────┐
       │              │                            │
       │              ▼                            ▼
       │    ┌──────────────┐              ┌─────────┐
       └─── │ kTerminated  │              │ kFailed │ → RecoveryAction
            └──────────────┘              └─────────┘
```

### 5.4 プロセス起動の仕組み

**Linux**:
```cpp
pid_t pid = fork();
if (pid == 0) {
    // 子プロセス
    execv(executable.c_str(), argv);
    _exit(127);
}
```

**QNX**:
```cpp
pid_t pid;
posix_spawn(&pid, executable.c_str(), nullptr, nullptr, argv, environ);
```

QNX ではマルチスレッド安全な `posix_spawn()` が優先的に使用されます。

### 5.5 PHM 連携とヘルスチェック

PHMOrchestrator が各プロセスの健全性を集約監視します:

```
プラットフォーム健全性レベル:

  kNormal   → すべてのエンティティが正常
  kDegraded → degradedThreshold を超えるエンティティが障害
  kCritical → criticalThreshold を超えるエンティティが障害
```

監視タイプ:

| タイプ | 説明 | チェック方法 |
|--------|------|------------|
| AliveSupervision | 定期的なハートビート | `ReportCheckpoint()` が一定時間内に呼ばれるか |
| DeadlineSupervision | 2 チェックポイント間の時間制約 | 処理時間が制限内か |
| LogicalSupervision | チェックポイントの順序制約 | 正しい順序で通過したか |

### 5.6 復旧アクション

障害検出時に実行される復旧アクション:

| アクション | 説明 |
|-----------|------|
| `RestartRecoveryAction` | プロセスを再起動 |
| `FunctionGroupStateRecoveryAction` | FG を安全な状態に遷移 |
| `PlatformResetRecoveryAction` | プラットフォーム全体をリセット |

---

## 6. ユーザーアプリ開発ガイド

### 6.1 最小構成のユーザーアプリ

```cpp
#include "ara/core/initialization.h"
#include "ara/exec/signal_handler.h"
#include "ara/log/logging_framework.h"

int main()
{
    // 1. AUTOSAR ランタイム初期化
    auto init = ara::core::Initialize();
    if (!init.HasValue()) return 1;

    ara::exec::SignalHandler::Register();

    // 2. ロギング設定
    auto logging = std::unique_ptr<ara::log::LoggingFramework>(
        ara::log::LoggingFramework::Create(
            "MYAP", ara::log::LogMode::kConsole,
            ara::log::LogLevel::kInfo, "My App"));

    // 3. メインループ
    while (!ara::exec::SignalHandler::IsTerminationRequested())
    {
        // アプリケーションロジック
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    // 4. クリーンアップ
    (void)ara::core::Deinitialize();
    return 0;
}
```

### 6.2 EM 連携 + PHM ヘルスレポート付きアプリ

```cpp
#include "ara/core/initialization.h"
#include "ara/core/instance_specifier.h"
#include "ara/exec/application_client.h"
#include "ara/exec/signal_handler.h"
#include "ara/phm/health_channel.h"
#include "ara/log/logging_framework.h"

int main()
{
    auto init = ara::core::Initialize();
    if (!init.HasValue()) return 1;
    ara::exec::SignalHandler::Register();

    // EM に登録
    auto appSpec = ara::core::InstanceSpecifier::Create(
        "AdaptiveAutosar/UserApps/MyApp/App").Value();
    ara::exec::ApplicationClient appClient(appSpec);
    appClient.ReportApplicationHealth();

    // PHM ヘルスチャネル
    auto phmSpec = ara::core::InstanceSpecifier::Create(
        "AdaptiveAutosar/UserApps/MyApp/Health").Value();
    ara::phm::HealthChannel healthChannel(phmSpec);

    while (!ara::exec::SignalHandler::IsTerminationRequested())
    {
        // ヘルス報告 (定期的に必要)
        appClient.ReportApplicationHealth();
        (void)healthChannel.ReportHealthStatus(
            ara::phm::HealthStatus::kOk);

        // アプリケーションロジック
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    // シャットダウン
    (void)healthChannel.ReportHealthStatus(
        ara::phm::HealthStatus::kDeactivated);
    (void)ara::core::Deinitialize();
    return 0;
}
```

### 6.3 CMakeLists.txt の書き方 (ユーザーアプリ)

```cmake
# user_apps/src/apps/basic/CMakeLists.txt に追加

add_user_template_target(
  my_custom_ecu_app          # ターゲット名
  my_custom_ecu_app.cpp      # ソースファイル
  AdaptiveAutosarAP::ara_core
  AdaptiveAutosarAP::ara_log
  AdaptiveAutosarAP::ara_exec
  AdaptiveAutosarAP::ara_phm
  AdaptiveAutosarAP::ara_per
  AdaptiveAutosarAP::ara_sm
)
```

### 6.4 ユーザーアプリのビルドとデプロイ

#### Linux ネイティブビルド

```bash
cd ~/Adaptive-AUTOSAR

# ユーザーアプリのビルド
cmake -S user_apps -B build-user-apps \
  -DAUTOSAR_AP_PREFIX=/opt/autosar-ap
cmake --build build-user-apps

# bringup.sh にアプリを追加して再起動
sudo systemctl restart autosar-exec-manager.service
```

#### QNX クロスビルド

```bash
cd ~/Adaptive-AUTOSAR
source ~/qnx800/qnxsdp-env.sh

# クロスビルド
./qnx/scripts/build_user_apps_qnx.sh build

# ターゲットに転送
scp out/qnx/work/build/user_apps-aarch64le/src/apps/basic/my_custom_ecu_app \
    root@192.168.1.100:/autosar/bin/

# bringup_qnx.sh にアプリを追加
ssh root@192.168.1.100 vi /autosar/bringup_qnx.sh
```

---

## 7. トラブルシューティング

### 7.1 ビルドエラー

#### メモリ不足

```bash
# 並列ビルド数を減らす
MAKEFLAGS="-j2" cmake --build build

# QNX クロスビルドの場合
export AUTOSAR_QNX_JOBS=2
```

#### ミドルウェアが見つからない

```bash
ls /opt/vsomeip/lib/libvsomeip3*
ls /opt/iceoryx/lib/libiceoryx*
sudo ldconfig
ldconfig -p | grep vsomeip
```

### 7.2 サービス起動エラー (Ubuntu)

```bash
# ログ確認
sudo journalctl -u autosar-platform-app.service --no-pager -n 50

# iceoryx の共有メモリをクリア
sudo rm -rf /dev/shm/iceoryx_mgmt
sudo rm -rf /tmp/roudi_*
sudo systemctl restart autosar-iox-roudi.service

# vsomeip ルーティングマネージャのログ
sudo journalctl -u autosar-vsomeip-routing.service --no-pager -n 50
```

### 7.3 QNX でプロセスが起動しない

```bash
# ライブラリの依存関係確認
ldd /autosar/bin/adaptive_autosar

# 環境変数の確認
env | grep -E '(LD_LIBRARY|VSOMEIP|AUTOSAR)'

# 手動実行してエラーメッセージを確認
. /autosar/env.sh
/autosar/bin/adaptive_autosar

# SLM のログ確認
slog2info | grep -i slm
slog2info | grep -i autosar
```

### 7.4 ユーザーアプリが EM に登録されない

```bash
# レジストリファイルの確認
cat /run/autosar/user_apps_registry.csv  # Linux
cat /autosar/run/autosar/user_apps_registry.csv  # QNX

# bringup スクリプトの手動実行
bash -x /etc/autosar/bringup.sh  # Linux
sh -x /autosar/bringup_qnx.sh   # QNX

# バイナリの実行権限確認
ls -la /opt/autosar-ap/user_apps_build/src/apps/basic/autosar_user_raspi_ecu
chmod +x /opt/autosar-ap/user_apps_build/src/apps/basic/autosar_user_raspi_ecu
```

### 7.5 PHM がアプリを障害と判定する

```bash
# PHM ステータスの確認
cat /run/autosar/phm.status

# ヘルスチャネルファイルの確認
ls -la /run/autosar/phm/health/

# アプリ側でヘルスレポートが定期的に送信されているか確認
# アプリのログで ReportHealthStatus の呼び出しを確認
sudo journalctl -u autosar-exec-manager.service -f | grep -i health
```

### 7.6 CAN インターフェースの問題

```bash
# CAN デバイスの確認
ip link show type can
dmesg | grep -iE "(mcp|can|spi)" | tail -20

# 仮想 CAN (テスト用)
sudo modprobe vcan
sudo ip link add dev vcan0 type vcan
sudo ip link set up vcan0

# CAN フレーム送受信テスト
candump vcan0 &
cansend vcan0 123#DEADBEEF
```

---

## 付録 A: Linux と QNX の自動起動方式の比較

| 項目 | Ubuntu (Linux) | QNX 8.0 |
|------|---------------|---------|
| サービス管理 | systemd | SLM (Service Launch Manager) |
| サービス定義 | `.service` ファイル | XML (`slm-config-autosar.xml`) |
| 依存関係解決 | `After=` / `Requires=` | `<SLM:dependency>` |
| 自動再起動 | `Restart=on-failure` | `<SLM:restart action="always">` |
| 環境変数 | `EnvironmentFile=` | `<SLM:env>` |
| ログ | `journalctl` | `slog2info` |
| 代替手段 | — | `rc.autosar.sh` (rc.d 統合) |

## 付録 B: ファイルパス対応表

| 用途 | Ubuntu (Linux) | QNX 8.0 |
|------|---------------|---------|
| バイナリ | `/opt/autosar-ap/bin/` | `/autosar/bin/` |
| ライブラリ | `/opt/autosar-ap/lib/` | `/autosar/lib/` |
| ランタイムデータ | `/run/autosar/` | `/autosar/run/autosar/` |
| 永続ストレージ | `/var/autosar/persistency/` | `/autosar/var/persistency/` |
| 設定ファイル | `/etc/default/autosar-*` | `/autosar/env.sh` |
| ブリングアップ | `/etc/autosar/bringup.sh` | `/autosar/bringup_qnx.sh` |
| サービス定義 | `/etc/systemd/system/autosar-*.service` | `/etc/slm/config/autosar.xml` |
| ログ | `journalctl -u autosar-*` | `slog2info \| grep autosar` |

## 付録 C: 環境変数リファレンス

| 変数 | 説明 | デフォルト値 |
|------|------|------------|
| `AUTOSAR_AP_PREFIX` | AUTOSAR AP インストール先 | `/opt/autosar-ap` |
| `LD_LIBRARY_PATH` | 共有ライブラリ検索パス | (自動設定) |
| `VSOMEIP_CONFIGURATION` | vsomeip 設定ファイルパス | (サービスごとに設定) |
| `VSOMEIP_APPLICATION_NAME` | vsomeip アプリケーション名 | (サービスごとに設定) |
| `AUTOSAR_ECU_PERIOD_MS` | ECU アプリのメインループ周期 | `100` |
| `AUTOSAR_ECU_LOG_EVERY` | ログ出力間隔 (サイクル数) | `50` |
| `AUTOSAR_ECU_PERSIST_EVERY` | 永続化間隔 (サイクル数) | `100` |
| `AUTOSAR_ECU_CAN_IFNAME` | CAN インターフェース名 | `vcan0` |
| `AUTOSAR_PHM_PERIOD_MS` | PHM 監視周期 | `1000` |
| `AUTOSAR_PHM_DEGRADED_THRESHOLD` | 劣化判定閾値 | `0.0` |
| `AUTOSAR_PHM_CRITICAL_THRESHOLD` | 重大判定閾値 | `0.5` |
| `AUTOSAR_DIAG_LISTEN_PORT` | 診断サーバ待受ポート | `13400` |
| `AUTOSAR_EM_MANIFEST` | EM マニフェストファイルパス | (未設定時は bringup.sh 方式) |
