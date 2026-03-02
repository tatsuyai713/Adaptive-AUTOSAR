# 13: ExecutionManager でユーザーアプリを管理・実行する

## 概要

このチュートリアルでは、`ara::exec::ExecutionManager` を使って複数のユーザーアプリを
**プロセスとして起動・監視・停止** する方法をステップバイステップで説明します。

AUTOSAR AP では、アプリケーションは独立した OS プロセスとして動作し、
ExecutionManager (EM) がそれらのライフサイクルを管理します。
本チュートリアルでは以下の 3 種類のバイナリを構築します:

| バイナリ名 | 役割 |
|---|---|
| `autosar_user_em_sensor_app` | 温度センサーをシミュレーション（EM が起動するマネージドプロセス） |
| `autosar_user_em_actuator_app` | アクチュエータをシミュレーション（EM が起動するマネージドプロセス） |
| `autosar_user_em_daemon` | ExecutionManager を持つオーケストレーションデーモン（親プロセス） |

```
[em_daemon_app]                [em_sensor_app]   [em_actuator_app]
 ExecutionManager ─fork/exec──►  (child)            (child)
 ExecutionServer   ◄──waitpid── exit(0)             exit(0)
 StateServer
```

---

## 対象ファイル

| ファイル | 説明 |
|---|---|
| `user_apps/src/apps/feature/em/em_daemon_app.cpp` | EM デーモン（このチュートリアルの主役） |
| `user_apps/src/apps/feature/em/em_sensor_app.cpp` | マネージドセンサーアプリ |
| `user_apps/src/apps/feature/em/em_actuator_app.cpp` | マネージドアクチュエーターアプリ |
| `user_apps/src/apps/feature/em/CMakeLists.txt` | ビルド定義 |

---

## ユーザーアプリの登録・実行モデル

### AUTOSAR AP の Process Manifest 概念

AUTOSAR AP では、アプリケーションの起動設定を **Process Manifest** (ARXML) に記述します。
本実装では `ProcessDescriptor` 構造体がマニフェストの代わりを果たします。

```cpp
ara::exec::ProcessDescriptor desc;
desc.name          = "SensorApp";          // プロセス名（一意）
desc.executable    = "/opt/autosar_ap/bin/autosar_user_em_sensor_app";
desc.functionGroup = "MachineFG";          // 属する Function Group 名
desc.activeState   = "Running";            // この FG 状態で起動する
desc.startupGrace  = std::chrono::milliseconds{3000};      // 起動猶予時間
desc.terminationTimeout = std::chrono::milliseconds{5000}; // 終了タイムアウト
```

### Function Group と State

Function Group (FG) はアプリの動作モードを表すグループです。

```
MachineFG
  ├── "Off"      ← 全プロセス停止
  └── "Running"  ← SensorApp / ActuatorApp が起動
```

`ActivateFunctionGroup("MachineFG", "Running")` を呼ぶと、
`activeState == "Running"` に登録されたプロセスが自動起動します。

### プロセス状態遷移

```
kNotRunning
    │  ActivateFunctionGroup()
    ▼
kStarting ──── ExecutionState::kRunning 報告受信 ──► kRunning
    │                                                    │
    │  TerminateFunctionGroup() / Stop()                 │
    ▼                                                    ▼
kTerminating ────────────────────────────────────► kTerminated
```

---

## Step 1: ビルド

### ホスト (Ubuntu / macOS) でのビルド

```bash
# 依存関係インストール済み前提でビルド
./scripts/build_user_apps_from_opt.sh --prefix /opt/autosar_ap

# ビルド成果物の確認
ls build-user-apps-opt/src/apps/feature/em/
# autosar_user_em_sensor_app
# autosar_user_em_actuator_app
# autosar_user_em_daemon
```

### Raspberry Pi (aarch64) クロスコンパイル

```bash
# aarch64 クロスコンパイラをインストール
sudo apt-get install -y gcc-aarch64-linux-gnu g++-aarch64-linux-gnu

# クロスコンパイル（--cross-compile オプション付き）
./scripts/build_user_apps_from_opt.sh \
  --prefix /opt/autosar_ap \
  --cross-compile aarch64-linux-gnu \
  --build-dir build-user-apps-rpi

ls build-user-apps-rpi/src/apps/feature/em/
# autosar_user_em_sensor_app   (aarch64 ELF)
# autosar_user_em_actuator_app (aarch64 ELF)
# autosar_user_em_daemon       (aarch64 ELF)
```

---

## Step 2: ホストで動作確認

まずホスト上で 10 秒間のデモ実行を行い、動作を確認します。

```bash
cd build-user-apps-opt/src/apps/feature/em

# デフォルト（10 秒で自動終了）
./autosar_user_em_daemon
```

期待される出力例:

```
[EMDA] Config:  sensor_bin=autosar_user_em_sensor_app  actuator_bin=autosar_user_em_actuator_app  run_seconds=10 (0=until SIGTERM)
[EMDA] Registered SensorApp -> MachineFG/Running
[EMDA] Registered ActuatorApp -> MachineFG/Running
[EMDA] ExecutionManager started.
[EMDA] Activating MachineFG -> Running ...
[EMDA] EM daemon running. Press Ctrl+C or send SIGTERM to stop.
[EMDA] Process state changed: SensorApp -> Starting
[EMDA] Process state changed: ActuatorApp -> Starting
[EMSA] [em_sensor_app] kRunning — interval_ms=1000
[EMAA] [em_actuator_app] kRunning — interval_ms=2000
[EMSA] [em_sensor_app] cycle=1  temperature=21.0 C
[EMAA] [em_actuator_app] cycle=1  target_position=45 deg  status=OK
...
[EMDA] Auto-shutdown: run_seconds limit reached.
[EMDA] Terminating MachineFG -> Off ...
[EMSA] [em_sensor_app] kTerminating — total cycles=9
[EMAA] [em_actuator_app] kTerminating — total cycles=4
[EMDA] FINAL SensorApp    managed=Terminated  pid=-1
[EMDA] FINAL ActuatorApp  managed=Terminated  pid=-1
[EMDA] ExecutionManager stopped. Shutdown complete.
```

---

## Step 3: 環境変数でカスタマイズ

### バイナリパスを明示指定

```bash
EM_SENSOR_BIN=/opt/autosar_ap/bin/autosar_user_em_sensor_app \
EM_ACTUATOR_BIN=/opt/autosar_ap/bin/autosar_user_em_actuator_app \
./autosar_user_em_daemon
```

### 無制限実行（SIGTERM まで継続）

```bash
EM_RUN_SECONDS=0 ./autosar_user_em_daemon &
# ... 確認後に停止
kill $!
```

### サンプリング周期を変更

センサーアプリは `EM_SENSOR_INTERVAL_MS`、アクチュエーターアプリは `EM_ACTUATOR_INTERVAL_MS` で制御します。
ただし EM が子プロセスを起動するとき、これらの環境変数を子プロセスに**引き継ぐには**、
デーモン側の環境に設定しておく必要があります。

```bash
EM_SENSOR_INTERVAL_MS=500 \
EM_ACTUATOR_INTERVAL_MS=1000 \
EM_RUN_SECONDS=15 \
./autosar_user_em_daemon
```

### 主な環境変数一覧

#### `autosar_user_em_daemon`

| 環境変数 | 既定値 | 説明 |
|---|---|---|
| `EM_SENSOR_BIN` | 自動検出 | センサーアプリのパス |
| `EM_ACTUATOR_BIN` | 自動検出 | アクチュエーターアプリのパス |
| `EM_RUN_SECONDS` | `10` | 自動終了までの秒数（`0` で SIGTERM まで継続） |

#### `autosar_user_em_sensor_app`

| 環境変数 | 既定値 | 説明 |
|---|---|---|
| `EM_SENSOR_INTERVAL_MS` | `1000` | センサー読み取り周期 [ms] |
| `EM_SENSOR_INSTANCE_ID` | `em_sensor_app` | ログ識別子 |

#### `autosar_user_em_actuator_app`

| 環境変数 | 既定値 | 説明 |
|---|---|---|
| `EM_ACTUATOR_INTERVAL_MS` | `2000` | アクチュエーターコマンド周期 [ms] |
| `EM_ACTUATOR_INSTANCE_ID` | `em_actuator_app` | ログ識別子 |

---

## Step 4: Raspberry Pi へ配備

### 4-1) バイナリを転送

```bash
# ビルドしたバイナリを RPi へコピー（SSH 経由）
scp build-user-apps-rpi/src/apps/feature/em/autosar_user_em_sensor_app \
    build-user-apps-rpi/src/apps/feature/em/autosar_user_em_actuator_app \
    build-user-apps-rpi/src/apps/feature/em/autosar_user_em_daemon \
    pi@raspberrypi.local:/opt/autosar_ap/bin/
```

### 4-2) RPi 上でテスト実行

```bash
ssh pi@raspberrypi.local

# 実行権限を付与
chmod +x /opt/autosar_ap/bin/autosar_user_em_*

# デフォルト 10 秒実行
/opt/autosar_ap/bin/autosar_user_em_daemon

# または RPi ローカルでパスを指定して実行
EM_SENSOR_BIN=/opt/autosar_ap/bin/autosar_user_em_sensor_app \
EM_ACTUATOR_BIN=/opt/autosar_ap/bin/autosar_user_em_actuator_app \
EM_RUN_SECONDS=30 \
/opt/autosar_ap/bin/autosar_user_em_daemon
```

### 4-3) systemd サービスとして登録

#### サービスファイルの作成

```bash
sudo tee /etc/systemd/system/autosar-em-demo.service > /dev/null << 'EOF'
[Unit]
Description=AUTOSAR AP ExecutionManager Demo
After=network.target

[Service]
Type=simple
User=pi
Environment=EM_SENSOR_BIN=/opt/autosar_ap/bin/autosar_user_em_sensor_app
Environment=EM_ACTUATOR_BIN=/opt/autosar_ap/bin/autosar_user_em_actuator_app
Environment=EM_RUN_SECONDS=0
ExecStart=/opt/autosar_ap/bin/autosar_user_em_daemon
Restart=on-failure
RestartSec=5s
StandardOutput=journal
StandardError=journal

[Install]
WantedBy=multi-user.target
EOF
```

#### サービスの有効化と起動

```bash
sudo systemctl daemon-reload
sudo systemctl enable autosar-em-demo.service
sudo systemctl start autosar-em-demo.service

# ステータス確認
sudo systemctl status autosar-em-demo.service --no-pager

# ログ確認
journalctl -u autosar-em-demo.service -f
```

#### サービスの停止

```bash
sudo systemctl stop autosar-em-demo.service
```

SIGTERM が em_daemon_app に送られ、EM が子プロセスを SIGTERM → SIGKILL で終了させます。

---

## Step 5: 自分のアプリを ExecutionManager に登録する

自分で作成したアプリを EM で管理するには、以下の手順に従います。

### 5-1) マネージドアプリの実装パターン

マネージドアプリ（EM から fork/exec される子プロセス）は以下のパターンに従います:

```cpp
#include <ara/core/initialization.h>
#include <ara/exec/signal_handler.h>
#include <ara/log/logging_framework.h>

int main()
{
    // 1) ランタイム初期化
    auto initResult{ara::core::Initialize()};
    if (!initResult.HasValue()) { return 1; }

    // 2) ロギング設定
    auto logging{/* ... */};

    // 3) SIGTERM ハンドラを登録
    ara::exec::SignalHandler::Register();

    // 4) 起動完了をログ
    logger.info("kRunning");

    // 5) メインループ（SIGTERM で終了）
    while (!ara::exec::SignalHandler::IsTerminationRequested())
    {
        // アプリロジック
        std::this_thread::sleep_for(std::chrono::milliseconds{100});
    }

    // 6) 終了処理
    logger.info("kTerminating");
    (void)ara::core::Deinitialize();
    return 0;  // ← 必ず 0 を返す（EM が正常終了として扱う）
}
```

**重要なポイント:**

- `ara::exec::SignalHandler::Register()` を必ず呼ぶ（SIGTERM を正しく捕捉するため）
- メインループは `SignalHandler::IsTerminationRequested()` で終了判定する
- 正常終了は `return 0`（EM が `kTerminated` と判断する）
- 異常終了（非ゼロ return）は EM が `kFailed` と記録する

### 5-2) ProcessDescriptor を登録する

デーモン側（EM を持つアプリ）で `ProcessDescriptor` を使って登録します:

```cpp
ara::exec::ProcessDescriptor myAppDesc;
myAppDesc.name          = "MyApp";            // 一意なプロセス名
myAppDesc.executable    = "/opt/autosar_ap/bin/my_app";
myAppDesc.functionGroup = "MachineFG";
myAppDesc.activeState   = "Running";
myAppDesc.startupGrace  = std::chrono::milliseconds{3000};
myAppDesc.terminationTimeout = std::chrono::milliseconds{5000};
// 引数が必要な場合
myAppDesc.arguments     = {"--config=/etc/myapp.conf"};

auto result{em.RegisterProcess(myAppDesc)};
if (!result.HasValue()) {
    // エラー処理
}
```

### 5-3) Function Group と State の設計

アプリが複数の動作モードを持つ場合、複数の FG と State を定義できます:

```cpp
// FG 宣言: MachineFG に 3 つの状態を定義
std::set<std::pair<std::string, std::string>> fgStates{
    {"MachineFG", "Off"},
    {"MachineFG", "Standby"},
    {"MachineFG", "Running"}
};

std::map<std::string, std::string> initStates{
    {"MachineFG", "Off"}
};

ara::exec::StateServer stateServer{&stateRpc, std::move(fgStates), std::move(initStates)};

// アプリを「Standby 状態のみ起動」として登録
ara::exec::ProcessDescriptor standbyApp;
standbyApp.name          = "DiagApp";
standbyApp.functionGroup = "MachineFG";
standbyApp.activeState   = "Standby";  // ← Standby 状態のみ起動

// アプリを「Running 状態のみ起動」として登録
ara::exec::ProcessDescriptor runApp;
runApp.name          = "SensorApp";
runApp.functionGroup = "MachineFG";
runApp.activeState   = "Running";  // ← Running 状態のみ起動
```

状態遷移:

```cpp
em.ActivateFunctionGroup("MachineFG", "Standby");   // DiagApp が起動
em.ActivateFunctionGroup("MachineFG", "Running");   // DiagApp 停止 + SensorApp 起動
em.TerminateFunctionGroup("MachineFG");             // 全停止
```

### 5-4) プロセス状態変化のコールバック

```cpp
em.SetProcessStateChangeHandler(
    [](const std::string &name, ara::exec::ManagedProcessState state)
    {
        // state に応じた処理（ログ、アラーム、フォールバックなど）
        if (state == ara::exec::ManagedProcessState::kFailed)
        {
            // フォールバック処理
        }
    });
```

### 5-5) プロセス状態の確認

```cpp
// 特定プロセスの状態
auto status{em.GetProcessStatus("SensorApp")};
if (status.HasValue()) {
    std::cout << "pid=" << status.Value().pid
              << " state=" << static_cast<int>(status.Value().managedState)
              << std::endl;
}

// 全プロセスの状態
for (const auto &s : em.GetAllProcessStatuses()) {
    std::cout << s.descriptor.name << ": pid=" << s.pid << std::endl;
}
```

---

## Step 6: よくある問題と解決方法

### バイナリが見つからない

```
[EMDA] Process state changed: SensorApp -> Failed
```

**原因:** `executable` に指定したパスが存在しないか実行権限がない。

**解決:**
```bash
# バイナリが存在するか確認
ls -la /opt/autosar_ap/bin/autosar_user_em_sensor_app

# 実行権限を付与
chmod +x /opt/autosar_ap/bin/autosar_user_em_*

# 環境変数でパスを上書き
EM_SENSOR_BIN=$(pwd)/autosar_user_em_sensor_app ./autosar_user_em_daemon
```

### 子プロセスが kStarting のまま kRunning に遷移しない

**原因:** このデモでは子プロセスが `ExecutionServer` に `kRunning` を報告しないため、
`syncExecutionStates()` が状態を昇格させません（RPC 通信なしの in-process デモのため）。

**動作への影響:** プロセスは実際には動作していますが、管理状態は `kStarting` のままです。
`waitpid()` によるプロセス終了監視は正常に機能します。

**完全な SOME/IP RPC を使いたい場合:**
`ara::exec::ExecutionClient` を子プロセスに実装し、
`ExecutionServer` へ `ExecutionState::kRunning` を報告してください。

### SIGTERM で子プロセスが止まらない

**原因:** 子プロセスが `SignalHandler::Register()` を呼んでいないか、
SIGTERM をブロックしている。

**解決:** 子プロセスに `ara::exec::SignalHandler::Register()` が実装されているか確認する。

### `terminationTimeout` 経過後に SIGKILL が送られる

これは仕様通りの動作です。`terminationTimeout` を延ばすか、
子プロセスの終了処理を高速化してください。

---

## Step 7: コードを読み解く（em_daemon_app.cpp の重要部分）

### LocalRpcServer パターン

`ara::com::someip::rpc::RpcServer` のコンストラクタは `protected` であるため、
直接インスタンス化できません。デーモンアプリでは以下のようにサブクラス化しています:

```cpp
class LocalRpcServer final : public ara::com::someip::rpc::RpcServer
{
public:
    LocalRpcServer() : RpcServer(1U, 1U) {}
};
```

これにより、ネットワークソケットなしで in-process dispatch のみを行う
RPC サーバーが作成できます。テストコードの `MockRpcServer` と同じパターンです。

### バイナリ自動検出

```cpp
std::string FindBinary(const std::string &name, const std::string &envVar)
{
    // 1) 環境変数が設定されていればそちらを優先
    const char *fromEnv{std::getenv(envVar.c_str())};
    if (fromEnv != nullptr && *fromEnv != '\0') return std::string{fromEnv};

    // 2) /opt/autosar_ap/bin/ にインストールされていれば使用
    const std::string installed{"/opt/autosar_ap/bin/" + name};
    if (::access(installed.c_str(), X_OK) == 0) return installed;

    // 3) PATH から解決（最終フォールバック）
    return name;
}
```

### 終了フロー

```cpp
// 1) Function Group を "Off" 状態へ（= 全子プロセスに SIGTERM）
em.TerminateFunctionGroup("MachineFG");

// 2) 子プロセスの終了を待機
std::this_thread::sleep_for(std::chrono::milliseconds{1000});

// 3) 最終状態を確認
for (const auto &s : em.GetAllProcessStatuses()) { /* ログ */ }

// 4) EM のモニタースレッドを停止
em.Stop();
```

---

## まとめ

| 概念 | 実装の場所 |
|---|---|
| ProcessDescriptor 登録 | `em.RegisterProcess(desc)` |
| Function Group 活性化 | `em.ActivateFunctionGroup("FG", "State")` |
| Function Group 停止 | `em.TerminateFunctionGroup("FG")` |
| 状態変化コールバック | `em.SetProcessStateChangeHandler(handler)` |
| SIGTERM ハンドリング（子側） | `ara::exec::SignalHandler::Register()` / `IsTerminationRequested()` |
| プロセス起動 | `ExecutionManager::launchProcess()` → `fork()/exec()` |
| プロセス停止 | `SIGTERM` → waitpid タイムアウト → `SIGKILL` |

### 次のステップ

- `ara::exec::ExecutionClient` を子プロセスに実装して `ExecutionState::kRunning` を報告し、
  `kStarting → kRunning` の自動遷移を確認する
- 複数の Function Group を定義して、システムモード遷移（Startup/Normal/Shutdown）を実装する
- `kFailed` 時に再起動ポリシー（`UnregisterProcess` + 再 `RegisterProcess`）を実装する
- `ara::phm::HealthChannel` と組み合わせて Watchdog 監視を追加する

---

## 参照

- [src/ara/exec/execution_manager.h](../../src/ara/exec/execution_manager.h) — ExecutionManager クラス定義
- [src/ara/exec/execution_manager.cpp](../../src/ara/exec/execution_manager.cpp) — fork/exec・waitpid 実装
- [user_apps/src/apps/feature/em/em_daemon_app.cpp](../src/apps/feature/em/em_daemon_app.cpp) — デーモンの全実装
- [user_apps/src/apps/feature/em/em_sensor_app.cpp](../src/apps/feature/em/em_sensor_app.cpp) — センサーアプリの全実装
- [user_apps/src/apps/feature/em/em_actuator_app.cpp](../src/apps/feature/em/em_actuator_app.cpp) — アクチュエーターアプリの全実装
- [12_exec_runtime_monitoring.ja.md](12_exec_runtime_monitoring.ja.md) — SignalHandler と Alive 監視の詳細
- [09_rpi_ecu_deployment.ja.md](09_rpi_ecu_deployment.ja.md) — Raspberry Pi 配備の詳細
