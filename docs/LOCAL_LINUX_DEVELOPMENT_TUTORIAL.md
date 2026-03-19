# ローカル Linux 開発環境での AUTOSAR AP 動作チュートリアル

Raspberry Pi を使わず、手元の Linux マシン (Ubuntu / Debian) 上で AUTOSAR Adaptive Platform の
全プラットフォームデーモンとユーザーアプリケーションをビルド・実行・動作確認するためのガイドです。

---

## 目次

1. [概要](#1-概要)
2. [環境構築](#2-環境構築)
3. [ビルド手順](#3-ビルド手順)
4. [動作確認 1: ユニットテスト](#4-動作確認-1-ユニットテスト)
5. [動作確認 2: プラットフォームデーモン単体実行](#5-動作確認-2-プラットフォームデーモン単体実行)
6. [動作確認 3: ARXML マニフェストによる実行管理](#6-動作確認-3-arxml-マニフェストによる実行管理)
7. [動作確認 4: ユーザーアプリの統合テスト](#7-動作確認-4-ユーザーアプリの統合テスト)
8. [動作確認 5: 全デーモン統合実行](#8-動作確認-5-全デーモン統合実行)
9. [ユーザーアプリの開発と登録](#9-ユーザーアプリの開発と登録)
10. [トラブルシューティング](#10-トラブルシューティング)

---

## 1. 概要

### このチュートリアルで実現すること

- 17 個のプラットフォームデーモンをローカル Linux でビルド・実行
- ARXML マニフェストを使った Execution Manager のプロセス管理を検証
- ユーザーアプリケーションを EM の管理下で起動・監視・復旧できることを確認
- systemd への登録なしで、ターミナルから直接動作確認

### 前提環境

| 項目 | 要件 |
|------|------|
| OS | Ubuntu 22.04 / 24.04 LTS, Debian 12 以降 |
| コンパイラ | GCC 11+ または Clang 14+ |
| CMake | 3.14 以上 |
| OpenSSL | libssl-dev |
| RAM | 4GB 以上推奨 |

---

## 2. 環境構築

### 2.1 必須パッケージのインストール

```bash
sudo apt update
sudo apt install -y \
  build-essential \
  cmake \
  git \
  libssl-dev \
  pkg-config \
  python3
```

### 2.2 オプション: ミドルウェアのインストール

SOME/IP、DDS、iceoryx を使った通信テストが不要であれば、ミドルウェアは必須ではありません。
プラットフォームデーモンとユーザーアプリはミドルウェアなしでも動作します。

```bash
# ミドルウェアが必要な場合のみ
cd ~/Adaptive-AUTOSAR
sudo ./scripts/install_middleware_stack.sh --install-base-deps
```

### 2.3 リポジトリのクローン

```bash
cd ~
git clone https://github.com/<your-org>/Adaptive-AUTOSAR.git
cd Adaptive-AUTOSAR
```

---

## 3. ビルド手順

### 3.1 テスト付きフルビルド

```bash
cmake -S . -B build -Dbuild_tests=ON
cmake --build build -j$(nproc)
```

### 3.2 ビルド結果の確認

```bash
# 17 プラットフォームデーモン
ls -1 build/adaptive_autosar \
      build/autosar_vsomeip_routing_manager \
      build/autosar_time_sync_daemon \
      build/autosar_persistency_guard \
      build/autosar_iam_policy_loader \
      build/autosar_can_interface_manager \
      build/autosar_watchdog_supervisor \
      build/autosar_user_app_monitor \
      build/autosar_sm_state_daemon \
      build/autosar_ntp_time_provider \
      build/autosar_ptp_time_provider \
      build/autosar_network_manager \
      build/autosar_ucm_daemon \
      build/autosar_dlt_daemon \
      build/autosar_diag_server \
      build/autosar_phm_daemon \
      build/autosar_crypto_provider

# テストバイナリ
ls -1 build/ara_unit_test build/arxml_unit_test

# Mock ECU サンプル
ls -1 build/ara_raspi_mock_ecu_sample
```

すべてのバイナリが ELF 実行ファイルとして存在することを確認:

```bash
file build/adaptive_autosar
# → ELF 64-bit LSB pie executable, x86-64, ...
```

---

## 4. 動作確認 1: ユニットテスト

### 4.1 全テストの実行

```bash
./build/ara_unit_test
```

期待される出力:

```
[==========] Running 322 tests from XX test suites.
...
[  PASSED  ] 322 tests.
```

### 4.2 Execution Manager 関連テストの抽出実行

```bash
# マニフェストローダのテスト
./build/ara_unit_test --gtest_filter='*ManifestLoader*'

# Execution Manager のテスト
./build/ara_unit_test --gtest_filter='*ExecutionManager*'

# Function Group 状態マシンのテスト
./build/ara_unit_test --gtest_filter='*FunctionGroupStateMachine*'

# StateClient / StateServer のテスト
./build/ara_unit_test --gtest_filter='*StateClient*:*StateServer*'
```

### 4.3 テスト名の一覧確認

```bash
# EM 関連テストの一覧
./build/ara_unit_test --gtest_list_tests | grep -iE '(manifest|execution|function.?group|state)'
```

### 4.4 ARXML パーサのテスト

```bash
./build/arxml_unit_test
```

---

## 5. 動作確認 2: プラットフォームデーモン単体実行

### 5.1 ランタイムディレクトリの準備

各デーモンはステータスファイルを `/run/autosar/` に書き込みます。
ローカルテスト用にディレクトリを用意します。

```bash
# ローカルテスト用ランタイムディレクトリ
export AUTOSAR_LOCAL_ROOT="$(pwd)/run_local"
mkdir -p "${AUTOSAR_LOCAL_ROOT}/autosar/phm/health"
mkdir -p "${AUTOSAR_LOCAL_ROOT}/autosar/user_apps"
mkdir -p "${AUTOSAR_LOCAL_ROOT}/autosar/nm_triggers"
mkdir -p "${AUTOSAR_LOCAL_ROOT}/autosar/can_triggers"
mkdir -p "${AUTOSAR_LOCAL_ROOT}/persistency"
mkdir -p "${AUTOSAR_LOCAL_ROOT}/ucm/staging"
mkdir -p "${AUTOSAR_LOCAL_ROOT}/ucm/processed"
```

### 5.2 PHM デーモンの単体実行

```bash
AUTOSAR_PHM_STATUS_FILE="${AUTOSAR_LOCAL_ROOT}/autosar/phm.status" \
AUTOSAR_PHM_PERIOD_MS=2000 \
AUTOSAR_PHM_ENTITIES="test_app_1,test_app_2" \
    ./build/autosar_phm_daemon &
PHM_PID=$!

# 2-3 秒待ってステータス確認
sleep 3
cat "${AUTOSAR_LOCAL_ROOT}/autosar/phm.status"
# 期待される出力:
#   platform_health=Normal
#   entity_count=2
#   entity.test_app_1.status=Ok
#   ...

# 停止
kill $PHM_PID
```

### 5.3 診断サーバ (DoIP) の単体実行

```bash
AUTOSAR_DIAG_LISTEN_PORT=13400 \
AUTOSAR_DIAG_STATUS_FILE="${AUTOSAR_LOCAL_ROOT}/autosar/diag_server.status" \
    ./build/autosar_diag_server &
DIAG_PID=$!

sleep 2

# ステータス確認
cat "${AUTOSAR_LOCAL_ROOT}/autosar/diag_server.status"
# 期待される出力:
#   listening=true
#   total_requests=0
#   ...

# UDS リクエストを送信 (DiagnosticSessionControl: 0x10 0x01)
echo -ne '\x10\x01' | nc -w 1 127.0.0.1 13400 | xxd
# 期待される出力: 50 00 (positive response: 0x10+0x40=0x50)

# TesterPresent (0x3E 0x00)
echo -ne '\x3e\x00' | nc -w 1 127.0.0.1 13400 | xxd
# 期待される出力: 7e 00

# 再度ステータス確認
cat "${AUTOSAR_LOCAL_ROOT}/autosar/diag_server.status"
# total_requests=2, positive_responses=2

kill $DIAG_PID
```

### 5.4 ウォッチドッグスーパーバイザの単体実行

```bash
AUTOSAR_WATCHDOG_STATUS_FILE="${AUTOSAR_LOCAL_ROOT}/autosar/watchdog.status" \
    ./build/autosar_watchdog_supervisor &
WD_PID=$!

sleep 3
cat "${AUTOSAR_LOCAL_ROOT}/autosar/watchdog.status"

kill $WD_PID
```

### 5.5 暗号プロバイダの単体実行

```bash
./build/autosar_crypto_provider &
CRYPTO_PID=$!

sleep 2
# 暗号プロバイダは OpenSSL ベースの AES/HMAC サービスを提供
# ステータスファイルまたはログ出力で動作確認

kill $CRYPTO_PID
```

### 5.6 その他のデーモン

同様の手法で各デーモンを個別に起動・ステータス確認できます:

```bash
# 時刻同期デーモン
AUTOSAR_TIMESYNC_STATUS_FILE="${AUTOSAR_LOCAL_ROOT}/autosar/time_sync.status" \
    ./build/autosar_time_sync_daemon &

# 永続ストレージガード
AUTOSAR_AP_PERSISTENCY_ROOT="${AUTOSAR_LOCAL_ROOT}/persistency" \
AUTOSAR_PERSISTENCY_STATUS_FILE="${AUTOSAR_LOCAL_ROOT}/autosar/persistency_guard.status" \
    ./build/autosar_persistency_guard &

# ネットワークマネージャ
AUTOSAR_NM_STATUS_FILE="${AUTOSAR_LOCAL_ROOT}/autosar/network_manager.status" \
AUTOSAR_NM_TRIGGER_DIR="${AUTOSAR_LOCAL_ROOT}/autosar/nm_triggers" \
    ./build/autosar_network_manager &

# SM ステートデーモン
AUTOSAR_SM_STATUS_FILE="${AUTOSAR_LOCAL_ROOT}/autosar/sm_state.status" \
    ./build/autosar_sm_state_daemon &

# UCM デーモン
AUTOSAR_UCM_STATUS_FILE="${AUTOSAR_LOCAL_ROOT}/autosar/ucm_daemon.status" \
AUTOSAR_UCM_STAGING_DIR="${AUTOSAR_LOCAL_ROOT}/ucm/staging" \
AUTOSAR_UCM_PROCESSED_DIR="${AUTOSAR_LOCAL_ROOT}/ucm/processed" \
    ./build/autosar_ucm_daemon &

# DLT デーモン
AUTOSAR_DLT_STATUS_FILE="${AUTOSAR_LOCAL_ROOT}/autosar/dlt_daemon.status" \
    ./build/autosar_dlt_daemon &

# IAM ポリシーローダ
AUTOSAR_IAM_STATUS_FILE="${AUTOSAR_LOCAL_ROOT}/autosar/iam_policy_loader.status" \
    ./build/autosar_iam_policy_loader &
```

---

## 6. 動作確認 3: ARXML マニフェストによる実行管理

### 6.1 テスト用マニフェストの作成

Execution Manager がプロセスを ARXML マニフェストから読み込み、
fork/exec で起動・管理できることを検証します。

```bash
mkdir -p "${AUTOSAR_LOCAL_ROOT}/manifests"
```

テスト用のシンプルなアプリケーションを用意:

```bash
cat > "${AUTOSAR_LOCAL_ROOT}/test_app.sh" << 'EOF'
#!/bin/sh
echo "[TestApp] Started (PID=$$)"
trap 'echo "[TestApp] SIGTERM received, shutting down"; exit 0' TERM
while true; do
    sleep 1
done
EOF
chmod +x "${AUTOSAR_LOCAL_ROOT}/test_app.sh"
```

マニフェストファイルを作成:

```bash
cat > "${AUTOSAR_LOCAL_ROOT}/manifests/test_manifest.arxml" << EOF
<?xml version="1.0" encoding="UTF-8"?>
<AUTOSAR xmlns="http://autosar.org/schema/r4.0">
  <AR-PACKAGES>
    <AR-PACKAGE>
      <SHORT-NAME>TestApps</SHORT-NAME>
      <AR-PACKAGES>
        <AR-PACKAGE>
          <SHORT-NAME>Manifest</SHORT-NAME>
          <ELEMENTS>
            <PROCESS-DESIGN>
              <SHORT-NAME>TestApp1</SHORT-NAME>
              <EXECUTABLE-REF>${AUTOSAR_LOCAL_ROOT}/test_app.sh</EXECUTABLE-REF>
              <FUNCTION-GROUP-REF>MachineFG</FUNCTION-GROUP-REF>
              <STATE-REF>Running</STATE-REF>
              <STARTUP-PRIORITY>10</STARTUP-PRIORITY>
              <STARTUP-GRACE-MS>3000</STARTUP-GRACE-MS>
              <TERMINATION-TIMEOUT-MS>5000</TERMINATION-TIMEOUT-MS>
            </PROCESS-DESIGN>
            <PROCESS-DESIGN>
              <SHORT-NAME>TestApp2</SHORT-NAME>
              <EXECUTABLE-REF>${AUTOSAR_LOCAL_ROOT}/test_app.sh</EXECUTABLE-REF>
              <FUNCTION-GROUP-REF>MachineFG</FUNCTION-GROUP-REF>
              <STATE-REF>Running</STATE-REF>
              <STARTUP-PRIORITY>20</STARTUP-PRIORITY>
              <STARTUP-GRACE-MS>3000</STARTUP-GRACE-MS>
              <TERMINATION-TIMEOUT-MS>5000</TERMINATION-TIMEOUT-MS>
              <DEPENDS-ON>TestApp1</DEPENDS-ON>
            </PROCESS-DESIGN>
          </ELEMENTS>
        </AR-PACKAGE>
      </AR-PACKAGES>
    </AR-PACKAGE>
  </AR-PACKAGES>
</AUTOSAR>
EOF
```

### 6.2 マニフェストの検証 (ユニットテスト)

プロジェクトのユニットテストでマニフェストローダの動作を確認:

```bash
./build/ara_unit_test --gtest_filter='*ManifestLoader*'
```

テスト内容:
- ARXML パース成功
- 重複名の検出
- 循環依存の検出 (DFS ベース)
- 不明な依存の検出
- スタートアップ優先度によるトポロジカルソート

### 6.3 マニフェスト手動パース確認

マニフェストローダを直接呼び出すテストプログラムを使って検証:

```bash
./build/ara_unit_test --gtest_filter='*ManifestLoader*LoadFromFile*'
```

期待される結果: ARXML からプロセス定義を正しく読み込み、依存関係順にソートできること。

### 6.4 Execution Manager のプロセス管理テスト

```bash
# EM のプロセスライフサイクルテスト
./build/ara_unit_test --gtest_filter='*ExecutionManager*'
```

テスト内容:
- プロセス登録 (RegisterProcess)
- Function Group の有効化 (ActivateFunctionGroup)
- プロセス状態遷移 (kNotRunning → kStarting → kRunning → kTerminating → kTerminated)
- プロセス終了の検出 (waitpid)
- 重複登録の拒否

### 6.5 Function Group 状態マシンテスト

```bash
./build/ara_unit_test --gtest_filter='*FunctionGroupStateMachine*'
```

テスト内容:
- 状態の追加と遷移
- ガード条件付き遷移
- タイムアウト遷移
- 遷移履歴の記録

### 6.6 統合テスト: ManifestLoader + ExecutionManager (End-to-End)

ARXML マニフェストの読み込みから、ExecutionManager によるプロセスの fork/exec 起動、
Function Group 状態遷移、プロセス終了まで一気通貫で検証する統合テストが用意されています。

```bash
./build/test_em_manifest
```

このテストは以下の 8 ステップを実行します:

1. **ManifestLoader: ファイルからの読み込み** — `/tmp` に一時 ARXML を書き出し、`LoadFromFile()` で 3 プロセス (SleepAppA, SleepAppB, DiagModeApp) を読み込む
2. **バリデーション** — 重複名・循環依存がないことを `ValidateEntries()` で確認
3. **トポロジカルソート** — 依存関係により SleepAppA が SleepAppB より前に来ることを確認
4. **ExecutionManager: マニフェストからの登録** — ソート済みエントリを `RegisterProcess()` で登録
5. **Function Group 有効化** — `ActivateFunctionGroup("TestFG", "Running")` で Running 状態のプロセスのみ起動（DiagModeApp は起動しない）
6. **状態遷移** — Running → Diagnostic に切り替え、DiagModeApp が起動し SleepAppA/B が停止
7. **Function Group 終了** — `TerminateFunctionGroup("TestFG")` で全プロセス停止
8. **状態変更ハンドラ** — `SetProcessStateChangeHandler` が正しく呼び出されたことを確認

期待される出力:
```
=== ARXML Manifest + Execution Manager Integration Test ===

--- Test 1: ManifestLoader - Load from file ---
[PASS] LoadFromFile succeeds
[PASS] 3 entries loaded (got 3)
[PASS] First entry is SleepAppA
...
--- Test 5: ActivateFunctionGroup(TestFG, Running) ---
[PASS] EM.Start() succeeded
[PASS] ActivateFG(TestFG, Running) succeeded
[PASS] 2 processes launched (Running state)
[PASS] 1 process NOT launched (Diagnostic state)
...
=== Summary ===
ALL TESTS PASSED
```

> **注意**: このテストは実際に `/bin/sleep` をサブプロセスとして fork/exec するため、
> プロセスの起動・終了に数秒かかります。

---

## 7. 動作確認 4: ユーザーアプリの統合テスト

### 7.1 Mock ECU サンプルの実行

全 11 モジュールを網羅する統合テスト:

```bash
./build/ara_raspi_mock_ecu_sample --max-cycles=10 --verbose-crypto
```

期待される出力:

```
[MockEcu] === AUTOSAR AP Mock ECU Application ===
[MockEcu] Boot #1 (previous cycles: 0)
[MockEcu] --- Diagnostic services ---
[MockEcu]   DiagnosticSessionControl (0x10): OK
[MockEcu]   ECUReset (0x11): OK
[MockEcu]   ReadDTCInformation (0x19): OK
...
[MockEcu] --- Crypto self-test ---
[MockEcu]   AES-128-CBC encrypt: OK (32 bytes)
[MockEcu]   AES-128-CBC decrypt: OK (matches plaintext)
[MockEcu]   HMAC-SHA256: OK (32 bytes)
...
[MockEcu] --- UCM software update simulation ---
[MockEcu]   PrepareUpdate: OK
[MockEcu]   StageSoftwarePackage: OK
[MockEcu]   VerifyStagedSoftwarePackage: OK
[MockEcu]   ActivateSoftwarePackage: OK
...
[MockEcu] === Graceful shutdown ===
```

確認ポイント:
- ara::core 初期化/終了が成功
- ara::diag UDS サービスが正常に処理される
- ara::crypto AES-CBC 暗号化/復号が一致する
- ara::ucm ソフトウェア更新フローが完了する
- ara::phm ヘルスレポートが送信される
- ara::per 永続カウンタが保存/読み込みされる

### 7.2 ユーザーアプリのビルドと実行 (インストール方式)

本番デプロイ方式をローカルでシミュレートする場合:

```bash
# 1. AUTOSAR AP をローカルにインストール
cmake --install build --prefix /tmp/autosar-ap_local

# 2. ユーザーアプリをビルド
cmake -S user_apps -B build-user-apps \
  -DAUTOSAR_AP_PREFIX=/tmp/autosar-ap_local
cmake --build build-user-apps -j$(nproc)

# 3. 本番用 ECU アプリを実行
./build-user-apps/src/apps/basic/autosar_user_raspi_ecu

# 期待される出力:
#   [RaspiEcu] === Raspberry Pi ECU Application starting ===
#   [RaspiEcu] Registered with EM, instance=...
#   [RaspiEcu] Health channel active, reported Ok to PHM daemon
#   [RaspiEcu] Boot #1, previous cycles: 0
#   [RaspiEcu] === Main loop started ===
#   [cycle=0] sm=Running speed=6000 rpm=2000 gear=3 health_rpt=1
#   ...
# Ctrl+C で停止:
#   [RaspiEcu] === Graceful shutdown ===
```

---

## 8. 動作確認 5: 全デーモン統合実行

### 8.1 統合起動スクリプト (ローカル用)

systemd なしで全デーモンを順次起動するスクリプト:

```bash
cat > "${AUTOSAR_LOCAL_ROOT}/start_all_local.sh" << 'SCRIPT'
#!/bin/bash
set -u

BUILD_DIR="${1:-./build}"
RUN_DIR="$(pwd)/run_local"
PID_DIR="${RUN_DIR}/pids"

mkdir -p "${RUN_DIR}/autosar/phm/health"
mkdir -p "${RUN_DIR}/autosar/user_apps"
mkdir -p "${RUN_DIR}/autosar/nm_triggers"
mkdir -p "${RUN_DIR}/autosar/can_triggers"
mkdir -p "${RUN_DIR}/persistency"
mkdir -p "${RUN_DIR}/ucm/staging"
mkdir -p "${RUN_DIR}/ucm/processed"
mkdir -p "${PID_DIR}"

export LD_LIBRARY_PATH="${BUILD_DIR}:${LD_LIBRARY_PATH:-}"

log() { echo "[LOCAL] $*"; }

start() {
    local name="$1"
    shift
    local bin="${BUILD_DIR}/${name}"
    [ -x "${bin}" ] || { log "SKIP: ${name}"; return; }
    log "Starting ${name}"
    "$@" "${bin}" &
    echo $! > "${PID_DIR}/${name}.pid"
}

stop_all() {
    log "Stopping all daemons"
    for f in "${PID_DIR}"/*.pid; do
        [ -f "$f" ] || continue
        kill "$(cat "$f")" 2>/dev/null
        rm -f "$f"
    done
    sleep 1
    log "Stopped"
}

trap stop_all EXIT INT TERM

# --- Tier 1: Core services ---
log "=== Starting AUTOSAR AP platform (local mode) ==="

start autosar_time_sync_daemon \
    env AUTOSAR_TIMESYNC_STATUS_FILE="${RUN_DIR}/autosar/time_sync.status"

start autosar_persistency_guard \
    env AUTOSAR_AP_PERSISTENCY_ROOT="${RUN_DIR}/persistency" \
        AUTOSAR_PERSISTENCY_STATUS_FILE="${RUN_DIR}/autosar/persistency_guard.status"

start autosar_iam_policy_loader \
    env AUTOSAR_IAM_STATUS_FILE="${RUN_DIR}/autosar/iam_policy_loader.status"

start autosar_network_manager \
    env AUTOSAR_NM_STATUS_FILE="${RUN_DIR}/autosar/network_manager.status" \
        AUTOSAR_NM_TRIGGER_DIR="${RUN_DIR}/autosar/nm_triggers"

start autosar_dlt_daemon \
    env AUTOSAR_DLT_STATUS_FILE="${RUN_DIR}/autosar/dlt_daemon.status"

sleep 1

# --- Tier 2: Monitoring and specialized services ---
start autosar_sm_state_daemon \
    env AUTOSAR_SM_STATUS_FILE="${RUN_DIR}/autosar/sm_state.status"

start autosar_watchdog_supervisor \
    env AUTOSAR_WATCHDOG_STATUS_FILE="${RUN_DIR}/autosar/watchdog.status"

start autosar_phm_daemon \
    env AUTOSAR_PHM_STATUS_FILE="${RUN_DIR}/autosar/phm.status" \
        AUTOSAR_PHM_ENTITIES="platform_app,ecu_app"

start autosar_diag_server \
    env AUTOSAR_DIAG_STATUS_FILE="${RUN_DIR}/autosar/diag_server.status" \
        AUTOSAR_DIAG_LISTEN_PORT=13400

start autosar_crypto_provider env

start autosar_ucm_daemon \
    env AUTOSAR_UCM_STATUS_FILE="${RUN_DIR}/autosar/ucm_daemon.status" \
        AUTOSAR_UCM_STAGING_DIR="${RUN_DIR}/ucm/staging" \
        AUTOSAR_UCM_PROCESSED_DIR="${RUN_DIR}/ucm/processed"

start autosar_ntp_time_provider \
    env AUTOSAR_NTP_STATUS_FILE="${RUN_DIR}/autosar/ntp_time_provider.status"

start autosar_can_interface_manager \
    env AUTOSAR_CAN_MANAGER_STATUS_FILE="${RUN_DIR}/autosar/can_manager.status" \
        AUTOSAR_CAN_TRIGGER_DIR="${RUN_DIR}/autosar/can_triggers"

sleep 1

# --- Tier 3: Main platform ---
start adaptive_autosar env

sleep 2

# --- Tier 4: User app monitor ---
start autosar_user_app_monitor \
    env AUTOSAR_USER_APP_REGISTRY_FILE="${RUN_DIR}/autosar/user_apps_registry.csv" \
        AUTOSAR_USER_APP_STATUS_DIR="${RUN_DIR}/autosar/user_apps" \
        AUTOSAR_USER_APP_MONITOR_STATUS_FILE="${RUN_DIR}/autosar/user_app_monitor.status"

log "=== All daemons started ==="
log "PID files: ${PID_DIR}/"
log "Status files: ${RUN_DIR}/autosar/"
log ""
log "Press Ctrl+C to stop all daemons"

# Keep alive
while true; do sleep 3600; done
SCRIPT
chmod +x "${AUTOSAR_LOCAL_ROOT}/start_all_local.sh"
```

### 8.2 統合起動

```bash
./run_local/start_all_local.sh ./build
```

### 8.3 統合動作確認

別のターミナルで以下を実行:

```bash
# 全ステータスファイルを一括確認
for f in run_local/autosar/*.status; do
    echo "=== $(basename "$f") ==="
    cat "$f"
    echo ""
done

# PHM のヘルス状態
cat run_local/autosar/phm.status
# → platform_health=Normal

# 診断サーバへの UDS リクエスト
echo -ne '\x10\x01' | nc -w 1 127.0.0.1 13400 | xxd
# → 期待: 50 00 (positive response)

echo -ne '\x3e\x00' | nc -w 1 127.0.0.1 13400 | xxd
# → 期待: 7e 00 (TesterPresent response)

echo -ne '\x22\xf1\x90' | nc -w 1 127.0.0.1 13400 | xxd
# → 期待: 62 00 (ReadDataByIdentifier response)

# 診断サーバのステータス
cat run_local/autosar/diag_server.status
# → total_requests=3, positive_responses=3

# ウォッチドッグのステータス
cat run_local/autosar/watchdog.status

# NM のステータス
cat run_local/autosar/network_manager.status

# プロセス一覧
ps aux | grep autosar

# PID ファイル一覧
ls run_local/pids/
```

### 8.4 Ctrl+C で全停止

起動したターミナルで Ctrl+C を押すと、全デーモンに SIGTERM が送信されグレースフル停止します。

---

## 9. ユーザーアプリの開発と登録

### 9.1 新しいユーザーアプリの作成

#### ソースファイル

```cpp
// user_apps/src/apps/basic/my_ecu_app.cpp
#include "ara/core/initialization.h"
#include "ara/core/instance_specifier.h"
#include "ara/exec/application_client.h"
#include "ara/exec/signal_handler.h"
#include "ara/phm/health_channel.h"
#include "ara/log/logging_framework.h"

#include <chrono>
#include <iostream>
#include <thread>

int main()
{
    auto init = ara::core::Initialize();
    if (!init.HasValue()) {
        std::cerr << "Initialize failed\n";
        return 1;
    }
    ara::exec::SignalHandler::Register();

    // ロギング
    auto logging = std::unique_ptr<ara::log::LoggingFramework>(
        ara::log::LoggingFramework::Create(
            "MYAP", ara::log::LogMode::kConsole,
            ara::log::LogLevel::kInfo, "My ECU App"));

    auto &logger = logging->CreateLogger(
        "MAIN", "Main task", ara::log::LogLevel::kInfo);

    // EM 登録
    auto appSpec = ara::core::InstanceSpecifier::Create(
        "AdaptiveAutosar/UserApps/MyApp/App").Value();
    ara::exec::ApplicationClient appClient(appSpec);
    appClient.ReportApplicationHealth();

    // PHM ヘルスチャネル
    auto phmSpec = ara::core::InstanceSpecifier::Create(
        "AdaptiveAutosar/UserApps/MyApp/Health").Value();
    ara::phm::HealthChannel health(phmSpec);

    std::uint32_t cycle = 0;
    while (!ara::exec::SignalHandler::IsTerminationRequested())
    {
        appClient.ReportApplicationHealth();
        (void)health.ReportHealthStatus(ara::phm::HealthStatus::kOk);

        if (cycle % 50 == 0) {
            auto stream = logger.WithLevel(ara::log::LogLevel::kInfo);
            stream << "[cycle=" << cycle << "] running";
            logging->Log(logger, ara::log::LogLevel::kInfo, stream);
        }

        ++cycle;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    (void)health.ReportHealthStatus(ara::phm::HealthStatus::kDeactivated);
    (void)ara::core::Deinitialize();
    return 0;
}
```

#### CMakeLists.txt への追加

```cmake
# user_apps/src/apps/basic/CMakeLists.txt に追加:
add_user_template_target(
  my_ecu_app
  my_ecu_app.cpp
  AdaptiveAutosarAP::ara_core
  AdaptiveAutosarAP::ara_log
  AdaptiveAutosarAP::ara_exec
  AdaptiveAutosarAP::ara_phm
)
```

#### ビルドと実行

```bash
# インストール方式
cmake --install build --prefix /tmp/autosar-ap_local
cmake -S user_apps -B build-user-apps -DAUTOSAR_AP_PREFIX=/tmp/autosar-ap_local
cmake --build build-user-apps -j$(nproc)
./build-user-apps/src/apps/basic/my_ecu_app
```

### 9.2 ARXML マニフェストへの登録

```xml
<PROCESS-DESIGN>
  <SHORT-NAME>MyEcuApp</SHORT-NAME>
  <EXECUTABLE-REF>/tmp/autosar-ap_local/user_apps_build/src/apps/basic/my_ecu_app</EXECUTABLE-REF>
  <FUNCTION-GROUP-REF>MachineFG</FUNCTION-GROUP-REF>
  <STATE-REF>Running</STATE-REF>
  <STARTUP-PRIORITY>10</STARTUP-PRIORITY>
  <STARTUP-GRACE-MS>5000</STARTUP-GRACE-MS>
  <TERMINATION-TIMEOUT-MS>5000</TERMINATION-TIMEOUT-MS>
</PROCESS-DESIGN>
```

### 9.3 bringup.sh への登録 (Linux systemd 方式の場合)

```bash
# /etc/autosar/bringup.sh に追加:
launch_app "my_ecu_app" \
    "${AUTOSAR_USER_APPS_BUILD_DIR}/src/apps/basic/my_ecu_app"
```

---

## 10. トラブルシューティング

### 10.1 ビルドエラー

#### OpenSSL が見つからない

```bash
sudo apt install -y libssl-dev
# cmake を再実行
cmake -S . -B build -Dbuild_tests=ON
```

#### メモリ不足

```bash
# 並列数を減らす
cmake --build build -j2
```

### 10.2 デーモンが即座に終了する

```bash
# 直接実行してエラーメッセージを確認
./build/autosar_phm_daemon
# 通常は SIGINT/SIGTERM までブロックするはず

# ライブラリの依存関係確認
ldd ./build/autosar_phm_daemon
```

### 10.3 診断サーバに接続できない

```bash
# ポートが使用中か確認
ss -tlnp | grep 13400

# ポート番号を変更して起動
AUTOSAR_DIAG_LISTEN_PORT=23400 ./build/autosar_diag_server &
echo -ne '\x10\x01' | nc -w 1 127.0.0.1 23400 | xxd
```

### 10.4 ステータスファイルが作成されない

```bash
# ディレクトリの存在と権限を確認
ls -la /run/autosar/
# /run/autosar/ が存在しない場合:
sudo mkdir -p /run/autosar
sudo chmod 777 /run/autosar

# またはローカルパスを使う (環境変数で上書き)
AUTOSAR_PHM_STATUS_FILE=./phm.status ./build/autosar_phm_daemon &
```

### 10.5 テストが失敗する

```bash
# 個別テストを詳細出力で実行
./build/ara_unit_test --gtest_filter='*FailingTestName*' --gtest_print_time=1

# 全テスト結果をファイルに保存
./build/ara_unit_test --gtest_output=xml:test_results.xml
```

---

## 付録: デーモンのステータスファイル一覧

| デーモン | デフォルトステータスファイル | 主要フィールド |
|---------|----------------------|-------------|
| `autosar_phm_daemon` | `/run/autosar/phm.status` | `platform_health`, `entity_count`, `failed_entity_count` |
| `autosar_diag_server` | `/run/autosar/diag_server.status` | `listening`, `total_requests`, `positive_responses` |
| `autosar_watchdog_supervisor` | `/run/autosar/watchdog.status` | (実装依存) |
| `autosar_time_sync_daemon` | `/run/autosar/time_sync.status` | (実装依存) |
| `autosar_persistency_guard` | `/run/autosar/persistency_guard.status` | (実装依存) |
| `autosar_iam_policy_loader` | `/run/autosar/iam_policy_loader.status` | (実装依存) |
| `autosar_network_manager` | `/run/autosar/network_manager.status` | (実装依存) |
| `autosar_ucm_daemon` | `/run/autosar/ucm_daemon.status` | (実装依存) |
| `autosar_dlt_daemon` | `/run/autosar/dlt_daemon.status` | (実装依存) |
| `autosar_sm_state_daemon` | `/run/autosar/sm_state.status` | (実装依存) |
| `autosar_user_app_monitor` | `/run/autosar/user_app_monitor.status` | (実装依存) |

## 付録: 環境変数によるデーモン設定

各デーモンの挙動は環境変数で制御できます。
詳細は [RASPI_ECU_DEPLOYMENT_TUTORIAL.md](RASPI_ECU_DEPLOYMENT_TUTORIAL.md) の付録 C を参照してください。
