# 14: Raspberry Pi 最小構成 ECU — ライブラリビルドからユーザーアプリ管理まで

## 概要

このチュートリアルでは、Adaptive AUTOSAR プロジェクトの **ライブラリビルド確認** から、
**Raspberry Pi への最小構成デプロイ** 、および **ユーザーアプリの登録・実行・監視** までを
一貫して解説します。

### 学習目標

1. AUTOSAR AP ライブラリ群が正しくビルドできることを確認する
2. Raspberry Pi ECU に必要な機能を理解し、最小限に絞る
3. ユーザーアプリを登録し、自動起動・ヘルスモニタリング・自動再起動を構成する
4. 全体のワークフローを理解する

### 構成図

```
┌─────────────────────────── Raspberry Pi ECU ───────────────────────────┐
│                                                                        │
│  [Tier 0: 通信基盤]                                                    │
│    iceoryx RouDi ──── ゼロコピー共有メモリ                              │
│    vSomeIP Routing ── SOME/IP サービスディスカバリ                       │
│                                                                        │
│  [Tier 1: プラットフォーム基盤サービス] (必要なもののみ有効化)            │
│    Time Sync ──── 時刻同期                                              │
│    Persistency ── データ永続化                                          │
│    IAM Policy ─── アクセス制御                                          │
│    SM State ───── 状態管理                                              │
│    CAN Manager ── CAN バス管理 (CAN 使用時のみ)                         │
│                                                                        │
│  [Tier 2: プラットフォームアプリ]                                        │
│    adaptive_autosar ── EM/SM/PHM/Diag/Vehicle 統合バイナリ              │
│                                                                        │
│  [Tier 3: 実行管理・監視]                                               │
│    Exec Manager ──── bringup.sh を実行しユーザーアプリを起動             │
│    User App Monitor ─ PID/ハートビート/PHM 監視 + 自動再起動            │
│    Watchdog ──────── ハードウェア WDT キック                             │
│                                                                        │
│  [ユーザーアプリ]                                                        │
│    bringup.sh で登録 → launch_app / launch_app_with_heartbeat           │
│    /launch_app_managed で起動                                            │
│      ├── my_sensor_app (例)                                             │
│      ├── my_actuator_app (例)                                           │
│      └── ...                                                            │
└────────────────────────────────────────────────────────────────────────┘
```

---

## Part 1: ライブラリビルド確認

### 1.1 前提条件

| 項目 | 要件 |
|---|---|
| OS | Ubuntu 22.04+ / Debian 12+ (x86_64 or aarch64) |
| コンパイラ | GCC 11+ (C++14 対応) |
| CMake | 3.14 以上 |
| Python | 3.8 以上 |
| OpenSSL | libssl-dev (3.0 対応済み) |
| ミドルウェア | vsomeip, iceoryx, CycloneDDS (後述のスクリプトで一括インストール可) |

### 1.2 ミドルウェア一括インストール

ミドルウェアがまだインストールされていない場合:

```bash
sudo ./scripts/install_middleware_stack.sh --install-base-deps
```

これにより以下がインストールされます:

| ミドルウェア | インストール先 | 用途 |
|---|---|---|
| iceoryx | `/opt/iceoryx` | ゼロコピー共有メモリ IPC |
| vSomeIP | `/opt/vsomeip` | SOME/IP 通信 |
| CycloneDDS | `/opt/cyclonedds` | DDS 通信 |

### 1.3 ライブラリのビルド

```bash
mkdir -p build-check && cd build-check

cmake .. \
  -DCMAKE_BUILD_TYPE=Release \
  -Dbuild_tests=OFF \
  -DARA_COM_USE_VSOMEIP=ON \
  -DARA_COM_USE_ICEORYX=ON \
  -DARA_COM_USE_CYCLONEDDS=ON \
  -DVSOMEIP_PREFIX=/opt/vsomeip \
  -DICEORYX_PREFIX=/opt/iceoryx \
  -DCYCLONEDDS_PREFIX=/opt/cyclonedds \
  -DAUTOSAR_AP_BUILD_PLATFORM_APP=ON

make -j$(nproc)
```

### 1.4 ビルド成果物の確認

ビルドが成功すると、以下のライブラリとバイナリが生成されます。

**静的ライブラリ (14 個)**

| ライブラリ | AUTOSAR FC | 説明 |
|---|---|---|
| `libara_core.a` | Core Types | 基本型、Result、Optional、ErrorCode |
| `libara_log.a` | Logging | DLT 対応ロギングフレームワーク |
| `libara_com.a` | Communication | SOME/IP, DDS, ZeroCopy pub/sub, SecOC |
| `libara_exec.a` | Execution Management | プロセス管理、Function Group、信号処理 |
| `libara_sm.a` | State Management | マシン状態、診断状態ハンドラ |
| `libara_diag.a` | Diagnostics | UDS サービス、DTC、モニター |
| `libara_phm.a` | Platform Health Mgmt | 監視エンティティ、ヘルスチャネル |
| `libara_per.a` | Persistency | KV ストア、ファイルストレージ |
| `libara_crypto.a` | Cryptography | AES, RSA, ECDSA, HKDF, X.509 |
| `libara_iam.a` | Identity & Access | アクセス制御、ポリシーエンジン |
| `libara_tsync.a` | Time Synchronization | NTP/PTP 時刻同期 |
| `libara_ucm.a` | Update & Config Mgmt | OTA アップデート管理 |
| `libara_nm.a` | Network Management | NM PDU、コーディネーター |
| `libarxml.a` | ARXML Parser | マニフェスト解析 (pugixml ベース) |

**プラットフォームバイナリ (14 個)**

| バイナリ | 説明 |
|---|---|
| `adaptive_autosar` | プラットフォーム統合アプリ (EM/SM/PHM/Diag/Vehicle) |
| `autosar_vsomeip_routing_manager` | vSomeIP ルーティングマネージャー |
| `autosar_time_sync_daemon` | 時刻同期デーモン |
| `autosar_persistency_guard` | 永続化ガードデーモン |
| `autosar_iam_policy_loader` | IAM ポリシーローダー |
| `autosar_can_interface_manager` | CAN インターフェースマネージャー |
| `autosar_watchdog_supervisor` | ウォッチドッグスーパーバイザー |
| `autosar_user_app_monitor` | ユーザーアプリモニター |
| `autosar_sm_state_daemon` | 状態管理デーモン |
| `autosar_ntp_time_provider` | NTP 時刻プロバイダー |
| `autosar_ptp_time_provider` | PTP 時刻プロバイダー |
| `autosar_network_manager` | ネットワークマネージャー |
| `autosar_ucm_daemon` | UCM デーモン |
| `autosar_dlt_daemon` | DLT ログデーモン |

> **ビルド時の注意:** OpenSSL 3.0 環境では HKDF API が EVP_KDF ベースに変更されています。
> このプロジェクトは OpenSSL 1.x / 3.0 の両方に対応しています。

---

## Part 2: ECU 機能の見直し — 最小構成の特定

### 2.1 全サービス一覧と必要性の評価

Raspberry Pi ECU プロファイルには 17 の systemd サービスが含まれます。
すべてが常に必要なわけではありません。

#### 必須 (Tier 0 — 通信基盤)

| サービス | 理由 |
|---|---|
| `autosar-iox-roudi` | ゼロコピー IPC ランタイム。`platform-app` と `exec-manager` が **hard Requires** で依存 |
| `autosar-vsomeip-routing` | SOME/IP ルーティング。サービスディスカバリに必須 |

#### 必須 (Tier 2–3 — プラットフォームと実行管理)

| サービス | 理由 |
|---|---|
| `autosar-platform-app` | EM/SM/PHM/Diag/Vehicle の統合プロセス。ECU の頭脳 |
| `autosar-exec-manager` | `bringup.sh` を実行してユーザーアプリを起動するブリッジ |

#### 推奨 (Tier 1 — プラットフォーム基盤)

| サービス | 推奨理由 | 省略可能な場合 |
|---|---|---|
| `autosar-time-sync` | 時刻同期が必要なアプリに不可欠 | 時刻精度不要の場合 |
| `autosar-persistency-guard` | KV ストア永続化の定期同期 | 永続化不要の場合 |
| `autosar-iam-policy` | IAM アクセス制御ポリシー適用 | セキュリティ不要の場合 |
| `autosar-user-app-monitor` | ユーザーアプリの PID/ハートビート/PHM 監視 | 手動監視で足りる場合 |
| `autosar-watchdog` | HW ウォッチドッグキック | WDT 不使用の場合 |
| `autosar-sm-state` | 状態管理ステートマシン | 状態遷移不要の場合 |

#### オプション (用途限定)

| サービス | 用途 | デフォルトで無効化推奨 |
|---|---|---|
| `autosar-can-manager` | CAN バス使用時のみ | CAN 不使用なら不要 |
| `autosar-network-manager` | NM ステートマシン | NM 不要なら省略 |
| `autosar-ntp-time-provider` | NTP 時刻ソース | time-sync のみで十分な場合 |
| `autosar-ptp-time-provider` | PTP/gPTP 時刻ソース | PTP 不使用なら不要 |
| `autosar-dlt` | DLT ログ集約 | コンソールログで十分な場合 |
| `autosar-ucm` | OTA ソフトウェアアップデート | アップデート不要な場合 |
| `autosar-ecu-full-stack` | CAN→SOME/IP→DDS フルスタックデモ | **常に無効化**。`bringup.sh` に置き換え |

### 2.2 最小構成プロファイル

**最小限の 4 サービス** でユーザーアプリを起動・管理できます:

```
autosar-iox-roudi          ← ゼロコピー IPC 基盤
autosar-vsomeip-routing    ← SOME/IP ルーティング
autosar-platform-app       ← プラットフォーム統合プロセス
autosar-exec-manager       ← ユーザーアプリ起動 (bringup.sh)
```

**推奨追加サービス** (ユーザーアプリ管理を強化):

```
autosar-user-app-monitor   ← PID/ハートビート監視 + 自動再起動
autosar-watchdog            ← HW ウォッチドッグ
autosar-persistency-guard   ← 永続データ保護
```

---

## Part 3: ビルドとインストール (Raspberry Pi)

### 3.1 ワンコマンドビルド＆インストール

```bash
sudo ./scripts/build_and_install_rpi_ecu_profile.sh \
  --prefix /opt/autosar_ap \
  --runtime-build-dir build-rpi-autosar-ap \
  --user-app-build-dir /opt/autosar_ap/user_apps_build \
  --install-middleware
```

このスクリプトが行うこと:

1. ミドルウェア（iceoryx / vSomeIP / CycloneDDS）のインストール（`--install-middleware` 指定時）
2. AUTOSAR AP ランタイムのビルドと `/opt/autosar_ap` へのインストール
3. `user_apps` テンプレートのビルド
4. デプロイメント設定ファイルのコピー

### 3.2 CAN インターフェースの準備

**実 CAN ハードウェア使用時:**

```bash
sudo ./scripts/setup_socketcan_interface.sh --ifname can0 --bitrate 500000
```

**検証用仮想 CAN (vcan):**

```bash
sudo ./scripts/setup_socketcan_interface.sh --ifname vcan0 --vcan
```

> CAN を使用しない場合はこの手順をスキップしてください。

### 3.3 systemd サービスのインストール

```bash
sudo ./scripts/install_rpi_ecu_services.sh \
  --prefix /opt/autosar_ap \
  --user-app-build-dir /opt/autosar_ap/user_apps_build \
  --enable
```

このスクリプトが行うこと:

1. 16 個のラッパースクリプトを `/usr/local/bin/` にインストール
2. 16 個の環境設定ファイルを `/etc/default/` にインストール
3. IAM ポリシーを `/etc/autosar/iam_policy.csv` にインストール
4. `bringup.sh` を `/etc/autosar/bringup.sh` にインストール
5. 14 個の systemd ユニットファイルを `/etc/systemd/system/` にインストール
6. `systemctl daemon-reload` を実行
7. 各バイナリが存在するサービスのみ有効化

### 3.4 不要なサービスの無効化 (最小構成)

CAN や DDS が不要な場合など、明示的に無効化します:

```bash
# CAN 不使用の場合
sudo systemctl disable --now autosar-can-manager.service
sudo systemctl disable --now autosar-network-manager.service

# NTP/PTP 不使用の場合
sudo systemctl disable --now autosar-ntp-time-provider.service
sudo systemctl disable --now autosar-ptp-time-provider.service

# DLT 不使用の場合
sudo systemctl disable --now autosar-dlt.service

# UCM (OTA) 不使用の場合
sudo systemctl disable --now autosar-ucm.service

# デモ用フルスタックサービスを無効化（bringup.sh に置き換え）
sudo systemctl disable --now autosar-ecu-full-stack.service
```

---

## Part 4: ユーザーアプリの登録・実行・管理

### 4.1 アーキテクチャ

```
autosar-exec-manager.service
    │
    └─ exec /etc/autosar/bringup.sh
         │
         ├─ launch_app "my_app" /opt/autosar_ap/user_apps_build/bin/my_app
         │     └─ PID/名前を /run/autosar/user_apps_registry.csv に登録
         │
         ├─ launch_app_with_heartbeat "my_app2" "$HB_FILE" "5000" /path/to/my_app2
         │     └─ PID + ハートビートファイル + タイムアウトを登録
         │
         └─ launch_app_managed "my_app3" "Instance/Spec" "$HB" "5000" "5" "60000" /path/to/my_app3
               └─ PID + インスタンス指定子 + 再起動ポリシーを登録

autosar-user-app-monitor.service
    │
    └─ 監視ループ (1秒間隔)
         ├─ PID 生存確認 (kill -0)
         ├─ ゾンビ検出 (waitpid WNOHANG)
         ├─ ハートビートファイルの鮮度チェック
         ├─ PHM HealthChannel ステータスファイル確認
         └─ 異常検出時 → 再起動 (restart_limit / restart_window_ms に基づくバックオフ付き)
```

### 4.2 bringup.sh のカスタマイズ

`/etc/autosar/bringup.sh` を編集してアプリを登録します。

#### ヘルパー関数

| 関数 | 用途 | 引数 |
|---|---|---|
| `launch_app` | 基本起動 | `"名前" コマンド [引数...]` |
| `launch_app_with_heartbeat` | ハートビート付き | `"名前" "HBファイル" "タイムアウトms" コマンド [引数...]` |
| `launch_app_managed` | 完全管理 | `"名前" "インスタンス指定子" "HBファイル" "タイムアウトms" "再起動上限" "再起動ウィンドウms" コマンド [引数...]` |

#### 例: 基本的なアプリ起動

```bash
# /etc/autosar/bringup.sh に追加

MY_APP="${AUTOSAR_USER_APPS_BUILD_DIR}/src/apps/basic/autosar_user_minimal_runtime"
launch_app "minimal_runtime" "${MY_APP}"
```

#### 例: ハートビート監視付き

```bash
HEALTH_APP="${AUTOSAR_USER_APPS_BUILD_DIR}/src/apps/basic/autosar_user_exec_signal_template"
HB_FILE="${AUTOSAR_USER_APP_STATUS_DIR}/$(sanitize_name "exec_signal_app").heartbeat"

launch_app_with_heartbeat "exec_signal_app" "${HB_FILE}" "5000" "${HEALTH_APP}"
```

ハートビートの仕組み:
- アプリが定期的に `date +%s > $HB_FILE` (または `ara::phm::HealthChannel` 経由) でタイムスタンプを書き込む
- `autosar-user-app-monitor` が `heartbeat_timeout_ms` 以内に更新されているかチェック
- 期限切れ → アプリ異常とみなし再起動

#### 例: 完全管理エントリ (再起動ポリシー付き)

```bash
PHM_APP="${AUTOSAR_USER_APPS_BUILD_DIR}/src/apps/basic/autosar_user_per_phm_demo"

launch_app_managed \
  "per_phm_demo" \
  "AdaptiveAutosar/UserApps/PerPhmDemo" \
  "" \
  "0" \
  "5" \
  "60000" \
  "${PHM_APP}"
```

パラメータの意味:

| パラメータ | 値 | 説明 |
|---|---|---|
| 名前 | `"per_phm_demo"` | レジストリ上の一意名 |
| インスタンス指定子 | `"AdaptiveAutosar/UserApps/PerPhmDemo"` | `ara::core::InstanceSpecifier` に対応 |
| HB ファイル | `""` | 空 = ハートビート無効 |
| HB タイムアウト | `"0"` | 0ms = チェック無効 |
| 再起動上限 | `"5"` | ウィンドウ内最大 5 回まで再起動 |
| 再起動ウィンドウ | `"60000"` | 60 秒間のウィンドウ |

### 4.3 サービスの起動

最小構成の起動順序:

```bash
# Tier 0: 通信基盤
sudo systemctl start autosar-iox-roudi.service
sudo systemctl start autosar-vsomeip-routing.service

# Tier 1: 推奨プラットフォームサービス (必要なもののみ)
sudo systemctl start autosar-time-sync.service          # 時刻同期
sudo systemctl start autosar-persistency-guard.service   # 永続化
sudo systemctl start autosar-iam-policy.service          # アクセス制御

# Tier 2: プラットフォームアプリ
sudo systemctl start autosar-platform-app.service

# Tier 3: 実行管理・監視
sudo systemctl start autosar-exec-manager.service        # bringup.sh を実行
sudo systemctl start autosar-user-app-monitor.service    # アプリ監視
sudo systemctl start autosar-watchdog.service            # WDT (HW あれば)
```

### 4.4 状態の確認

```bash
# サービス全体の状態確認
sudo systemctl status autosar-platform-app.service --no-pager
sudo systemctl status autosar-exec-manager.service --no-pager
sudo systemctl status autosar-user-app-monitor.service --no-pager

# ユーザーアプリのレジストリ確認
cat /run/autosar/user_apps_registry.csv

# モニターのステータス
cat /run/autosar/user_app_monitor.status

# PHM ヘルスチャネル
ls /run/autosar/phm/health/
```

レジストリ CSV のフォーマット:

```csv
# name,pid,heartbeat_file,heartbeat_timeout_ms,instance_specifier,restart_limit,restart_window_ms,restart_command
minimal_runtime,12345,,0,AdaptiveAutosar/UserApps/minimal_runtime,5,60000,/opt/autosar_ap/user_apps_build/src/apps/basic/autosar_user_minimal_runtime
exec_signal_app,12346,/run/autosar/user_apps/exec_signal_app.heartbeat,5000,AdaptiveAutosar/UserApps/exec_signal_app,5,60000,/opt/autosar_ap/user_apps_build/src/apps/basic/autosar_user_exec_signal_template
```

### 4.5 User App Monitor の設定パラメータ

`/etc/default/autosar-user-app-monitor` で以下を調整できます:

| 環境変数 | 既定値 | 説明 |
|---|---|---|
| `AUTOSAR_USER_APP_MONITOR_PERIOD_MS` | `1000` | 監視ポーリング間隔 (ms) |
| `AUTOSAR_USER_APP_MONITOR_HEARTBEAT_GRACE_MS` | `500` | ハートビート判定の許容誤差 (ms) |
| `AUTOSAR_USER_APP_MONITOR_STARTUP_GRACE_MS` | `3000` | 起動直後の監視猶予時間 (ms) |
| `AUTOSAR_USER_APP_MONITOR_RESTART_BACKOFF_MS` | `1000` | 再起動間の最小待機時間 (ms) |
| `AUTOSAR_USER_APP_MONITOR_ENFORCE_HEALTH` | `true` | PHM 異常時に再起動するか |
| `AUTOSAR_USER_APP_MONITOR_RESTART_ON_FAILURE` | `true` | プロセス終了時に再起動するか |
| `AUTOSAR_USER_APP_MONITOR_ALLOW_DEACTIVATED_AS_HEALTHY` | `false` | PHM 未使用アプリを正常扱いにするか |
| `AUTOSAR_USER_APP_MONITOR_KILL_SIGNAL` | `TERM` | 異常プロセスへの停止シグナル |

### 4.6 アプリの更新と再起動

アプリを更新した場合:

```bash
# 1) アプリを再ビルド
cd /opt/autosar_ap/user_apps_build && make -j$(nproc)

# 2) bringup.sh を編集（必要に応じて）
sudo vi /etc/autosar/bringup.sh

# 3) exec-manager を再起動（全ユーザーアプリが再起動される）
sudo systemctl restart autosar-exec-manager.service
```

---

## Part 5: 検証

### 5.1 自動検証スクリプト

```bash
./scripts/verify_rpi_ecu_profile.sh \
  --prefix /opt/autosar_ap \
  --user-app-build-dir /opt/autosar_ap/user_apps_build \
  --can-backend mock \
  --require-platform-binary
```

このスクリプトは以下を検証します:

| チェック項目 | 内容 |
|---|---|
| バイナリ存在確認 | 全プラットフォームバイナリとテンプレートアプリが存在するか |
| CMake Config | `AdaptiveAutosarAPConfig.cmake` が正しく配置されているか |
| SOME/IP 通信テスト | Provider → Consumer の pub/sub が動作するか |
| DDS 通信テスト | DDS pub → sub が動作するか |
| ZeroCopy 通信テスト | iceoryx ゼロコピー pub/sub が動作するか |
| ECU CAN→DDS | CAN 入力 → DDS 出力のフローが動作するか |

### 5.2 手動確認チェックリスト

```bash
# 1) プラットフォームサービスが全て active か確認
for svc in autosar-iox-roudi autosar-vsomeip-routing autosar-platform-app autosar-exec-manager; do
  echo -n "$svc: "
  systemctl is-active $svc.service
done

# 2) ユーザーアプリが登録されているか確認
wc -l /run/autosar/user_apps_registry.csv

# 3) 監視デーモンが動作しているか確認
systemctl is-active autosar-user-app-monitor.service

# 4) ログ確認
journalctl -u autosar-exec-manager.service --no-pager -n 20
journalctl -u autosar-user-app-monitor.service --no-pager -n 20
```

---

## Part 6: 実践例 — カスタムアプリの作成・登録・デプロイ

### 6.1 最小アプリの作成

`user_apps/src/apps/basic/` に新しいファイルを作成します:

```cpp
// user_apps/src/apps/basic/my_custom_app.cpp
#include <chrono>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <thread>

#include "ara/core/initialization.h"
#include "ara/exec/execution_client.h"

static volatile sig_atomic_t gRunning = 1;

static void signalHandler(int) { gRunning = 0; }

int main(int argc, char *argv[])
{
    // AUTOSAR AP 初期化
    ara::core::Initialize();

    // SIGTERM で graceful shutdown
    std::signal(SIGTERM, signalHandler);
    std::signal(SIGINT, signalHandler);

    // Execution Client で kRunning を報告
    ara::core::InstanceSpecifier specifier("AdaptiveAutosar/UserApps/MyCustomApp");
    ara::exec::ExecutionClient execClient(specifier);
    execClient.ReportExecutionState(ara::exec::ExecutionState::kRunning);

    // ハートビート用ファイル (環境変数から取得)
    const char *hbPath = std::getenv("MY_APP_HEARTBEAT_FILE");

    std::printf("[MyApp] Started. PID=%d\n", static_cast<int>(getpid()));

    while (gRunning)
    {
        // メインロジック
        std::printf("[MyApp] Working...\n");

        // ハートビート更新
        if (hbPath)
        {
            std::ofstream hb(hbPath);
            hb << std::chrono::duration_cast<std::chrono::seconds>(
                      std::chrono::system_clock::now().time_since_epoch())
                      .count();
        }

        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    std::printf("[MyApp] Shutting down.\n");
    ara::core::Deinitialize();
    return 0;
}
```

### 6.2 CMakeLists.txt への登録

`user_apps/src/apps/basic/CMakeLists.txt` に追加:

```cmake
add_user_template_target(
  autosar_user_my_custom_app
  my_custom_app.cpp
  AdaptiveAutosarAP::ara_core
  AdaptiveAutosarAP::ara_exec
)
```

### 6.3 ビルド

```bash
./scripts/build_user_apps_from_opt.sh --prefix /opt/autosar_ap
ls build-user-apps-opt/src/apps/basic/autosar_user_my_custom_app
```

### 6.4 bringup.sh に登録

```bash
sudo vi /etc/autosar/bringup.sh
```

以下を追加:

```bash
MY_CUSTOM_APP="${AUTOSAR_USER_APPS_BUILD_DIR}/src/apps/basic/autosar_user_my_custom_app"
HB_FILE="${AUTOSAR_USER_APP_STATUS_DIR}/my_custom_app.heartbeat"

# ハートビート付きで起動
export MY_APP_HEARTBEAT_FILE="${HB_FILE}"
launch_app_with_heartbeat "my_custom_app" "${HB_FILE}" "5000" "${MY_CUSTOM_APP}"
```

### 6.5 デプロイと実行

```bash
# exec-manager を再起動してアプリをデプロイ
sudo systemctl restart autosar-exec-manager.service

# 状態確認
sleep 3
cat /run/autosar/user_apps_registry.csv
sudo systemctl status autosar-user-app-monitor.service --no-pager
journalctl -u autosar-exec-manager.service --no-pager -n 20
```

### 6.6 障害回復の確認

```bash
# アプリを意図的に kill
MYPID=$(grep "my_custom_app" /run/autosar/user_apps_registry.csv | cut -d, -f2)
kill "${MYPID}"

# 数秒待つと user-app-monitor が自動再起動する
sleep 5
cat /run/autosar/user_apps_registry.csv  # 新しい PID で再登録されている
journalctl -u autosar-user-app-monitor.service --no-pager -n 10
```

---

## Part 7: サービス依存関係マップ

```
local-fs.target / network-online.target
    │
    ├── autosar-iox-roudi             [Tier 0] 必須 (Restart=always)
    ├── autosar-vsomeip-routing       [Tier 0] 必須 (Restart=on-failure)
    │
    ├── autosar-time-sync             [Tier 1] 推奨
    ├── autosar-persistency-guard     [Tier 1] 推奨
    ├── autosar-iam-policy            [Tier 1] 推奨
    ├── autosar-sm-state              [Tier 1] 推奨
    ├── autosar-can-manager           [Tier 1] CAN使用時のみ
    │    └── autosar-network-manager  [Tier 1] NM使用時のみ
    ├── autosar-dlt                   [Tier 1] オプション
    ├── autosar-ntp-time-provider     [Tier 1] オプション
    └── autosar-ptp-time-provider     [Tier 1] オプション
         │
         ▼
    autosar-platform-app              [Tier 2] 必須 (Wants Tier1)
    autosar-ucm                       [Tier 2] オプション
         │
         ▼
    autosar-exec-manager              [Tier 3] 必須 (Requires: iox-roudi, platform-app)
         │
         ├── autosar-watchdog          [Tier 3] 推奨 (Restart=always)
         └── autosar-user-app-monitor  [Tier 3] 推奨 (Restart=always)
              │
              ▼
         autosar-ecu-full-stack        [Tier 4] デモ用 → bringup.sh で代替
```

---

## Part 8: トラブルシューティング

### ビルドエラー: OpenSSL HKDF

**症状:** `EVP_PKEY_CTX_set_hkdf_md was not declared`

**原因:** OpenSSL 3.0 で旧 HKDF マクロが削除された。

**解決:** このプロジェクトは `#if OPENSSL_VERSION_NUMBER >= 0x30000000L` で自動切替しますが、
古いコードベースから更新した場合は `src/ara/crypto/crypto_provider.cpp` の HKDF 実装を確認してください。

### サービスが起動しない

```bash
# ログ確認
journalctl -u autosar-<service-name>.service --no-pager -n 50

# バイナリパスの確認
cat /etc/default/autosar-<service-name>

# LD_LIBRARY_PATH の確認
cat /usr/local/bin/autosar-<service-name>-wrapper.sh
```

### ユーザーアプリが再起動ループ

**原因候補:**
- アプリが起動直後にクラッシュしている
- `restart_limit` / `restart_window_ms` のウィンドウ内で上限に達すると再起動が抑制される

**確認:**
```bash
journalctl -u autosar-user-app-monitor.service --no-pager -n 50
cat /run/autosar/user_app_monitor.status
```

**対策:**
- `AUTOSAR_USER_APP_MONITOR_STARTUP_GRACE_MS` を増やす
- `AUTOSAR_USER_APP_MONITOR_RESTART_BACKOFF_MS` を増やす

### ハートビートタイムアウト

**症状:** アプリは動作しているがモニターが異常検出

**対策:**
1. アプリが確実にハートビートファイルを更新しているか確認
2. `heartbeat_timeout_ms` を十分大きくする
3. `AUTOSAR_USER_APP_MONITOR_HEARTBEAT_GRACE_MS` を調整

---

## まとめ

| ステップ | コマンド |
|---|---|
| 1. ミドルウェアインストール | `sudo ./scripts/install_middleware_stack.sh --install-base-deps` |
| 2. ランタイムビルド＆インストール | `sudo ./scripts/build_and_install_rpi_ecu_profile.sh --prefix /opt/autosar_ap ...` |
| 3. CAN 準備 (必要時) | `sudo ./scripts/setup_socketcan_interface.sh --ifname vcan0 --vcan` |
| 4. サービスインストール | `sudo ./scripts/install_rpi_ecu_services.sh --prefix /opt/autosar_ap ... --enable` |
| 5. 不要サービス無効化 | `sudo systemctl disable --now autosar-ecu-full-stack.service` |
| 6. bringup.sh 編集 | `sudo vi /etc/autosar/bringup.sh` (アプリ登録) |
| 7. サービス起動 | `sudo systemctl start autosar-iox-roudi autosar-vsomeip-routing ...` |
| 8. 検証 | `./scripts/verify_rpi_ecu_profile.sh --prefix /opt/autosar_ap ... --can-backend mock` |

### 最小構成クイックリファレンス

```bash
# 最小限の 4 + 2 サービス構成
sudo systemctl start autosar-iox-roudi.service
sudo systemctl start autosar-vsomeip-routing.service
sudo systemctl start autosar-platform-app.service
sudo systemctl start autosar-exec-manager.service
sudo systemctl start autosar-user-app-monitor.service
sudo systemctl start autosar-watchdog.service

# 不要サービスの停止
sudo systemctl disable --now autosar-ecu-full-stack.service
sudo systemctl disable --now autosar-can-manager.service
sudo systemctl disable --now autosar-network-manager.service
sudo systemctl disable --now autosar-ntp-time-provider.service
sudo systemctl disable --now autosar-ptp-time-provider.service
sudo systemctl disable --now autosar-dlt.service
sudo systemctl disable --now autosar-ucm.service
```
