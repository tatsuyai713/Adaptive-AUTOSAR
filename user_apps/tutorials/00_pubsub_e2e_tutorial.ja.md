# Adaptive AUTOSAR — Pub/Sub 通信 完全チュートリアル
## YAML 定義 → ARXML 生成 → C++ コード生成 → ビルド → 実行

> **対象読者**: Adaptive AUTOSAR が初めてで、まず Pub/Sub 通信を動かしたい方
> **所要時間**: 約 45〜60 分
> **前提**: AUTOSAR AP ランタイムが `/opt/autosar_ap` にインストール済みであること

---

## 全体フロー

```
あなたの通信設計
        │
        ▼
┌──────────────────────────────────┐
│  Step 1: YAML でサービスを定義   │  ← 通信トポロジをテキストで記述
└──────────────┬───────────────────┘
               │  python3 generate_arxml.py
               ▼
┌──────────────────────────────────┐
│  Step 2: ARXML を生成            │  ← AUTOSAR 標準 XML マニフェスト
└──────────────┬───────────────────┘
               │  python3 generate_ara_com_binding.py
               ▼
┌──────────────────────────────────┐
│  Step 3: C++ バインディング生成  │  ← ServiceID / EventID 定数ヘッダ
└──────────────┬───────────────────┘
               │  手書き or pubsub_designer_gui.py で自動生成
               ▼
┌──────────────────────────────────┐
│  Step 4: ユーザーアプリを作成    │  ← Publisher / Subscriber コード
└──────────────┬───────────────────┘
               │  cmake --build
               ▼
┌──────────────────────────────────┐
│  Step 5: ビルド & 実行           │
└──────────────────────────────────┘
```

**3 つの方法で進められます:**

| 方法 | 向いている人 | 操作 |
| ---- | ---------- | ---- |
| **A: GUI ツール**（推奨） | AUTOSAR 初心者 | フォーム入力だけで全ファイルを一括生成 |
| **B: テンプレート YAML** | CLI 派 | テンプレートを編集してスクリプト実行 |
| **C: カスタム YAML** | 上級者 | YAML を完全手書きで制御 |

---

## 前提確認

```bash
# AUTOSAR AP がインストールされているか確認
ls /opt/autosar_ap/lib/

# vSomeIP がインストールされているか確認
ls /opt/vsomeip/lib/

# Python3 と PyYAML が使えるか確認
python3 -c "import yaml; print('PyYAML OK')"
```

PyYAML がない場合:
```bash
pip3 install pyyaml
```

---

## 方法 A: GUI ツールで一括生成（推奨）

### A-1. GUI を起動する

```bash
# リポジトリルートから実行
python3 tools/arxml_generator/pubsub_designer_gui.py
```

### A-2. サービスを設計する

**Basic タブ** に入力:

| 項目 | 例 | 説明 |
| ---- | -- | ---- |
| Service name | `VehicleSpeed` | C++ クラス名のプレフィックスになります |
| Service ID | `0x1234` | "Auto-generate ID" ボタンでサービス名から自動生成可能 |
| Instance ID | `1` | インスタンスが 1 つの場合は通常 1 |
| Version | Major `1`, Minor `0` | |

**Network タブ** に入力:

| 項目 | 例 |
| ---- | -- |
| Provider IP | `127.0.0.1`（ループバックテスト） |
| Provider port | `30509` |
| Consumer IP | `127.0.0.1` |
| Consumer port | `30510` |
| Multicast IP | `239.255.0.1` |

**Events タブ** で「Add」をクリックしてイベントを追加:

| 項目 | 例 | 説明 |
| ---- | -- | ---- |
| Event name | `SpeedEvent` | C++ メンバ名になります |
| Event ID | `0x8001` | 0x8000 以上を推奨 |
| Group ID | `1` | 関連イベントをグループ化 |
| Payload type name | `SpeedData` | C++ 構造体の名前 |
| Payload fields | `std::uint16_t speedKph;` | C++ フィールド定義 |

### A-3. プレビューを確認する

「Refresh Preview」をクリックすると右パネルに表示されます:

- **YAML タブ** — 生成される YAML 定義
- **ARXML タブ** — 生成される ARXML マニフェスト
- **C++ types.h タブ** — Skeleton / Proxy クラス定義
- **C++ binding.h タブ** — サービス / イベント ID 定数
- **Provider App タブ** — Publisher サンプルコード
- **Consumer App タブ** — Subscriber サンプルコード
- **Build Guide タブ** — cmake コマンド集

### A-4. すべて生成する

「Output」タブで出力ディレクトリを設定し、「Generate All & Save」をクリックします。

生成されるファイル:
```
/tmp/pubsub_gen/
├── vehiclespeed_service.yaml        ← YAML 定義
├── vehiclespeed_manifest.arxml      ← ARXML マニフェスト
├── types.h                          ← Skeleton / Proxy クラス
├── vehiclespeed_binding.h           ← C++ 定数ヘッダ
├── vehiclespeed_provider_app.cpp    ← Publisher サンプル
├── vehiclespeed_consumer_app.cpp    ← Subscriber サンプル
└── BUILD_GUIDE.sh                   ← ビルドコマンド参照
```

> **[Step 4](#step-4-ユーザーアプリへの組み込み) に進んでください。**

---

## 方法 B: テンプレート YAML を使う

### B-1. テンプレートをコピーして編集する

```bash
cp tools/arxml_generator/examples/my_first_service.yaml \
   my_vehiclespeed_service.yaml

# エディタで開く
nano my_vehiclespeed_service.yaml
```

**最低限変更すべき箇所（★ マーク）:**

```yaml
variables:
  PROVIDER_IP: "127.0.0.1"   # 実機間テストなら実際の IP アドレスに変更

autosar:
  packages:
    - short_name: MyFirstServiceManifest   # サービスに合わせてリネーム

      someip:
        provided_service_instances:
          - short_name: VehicleSpeedProviderInstance
            service_interface:
              id: 0xAB01      # ★ プロジェクト内で一意の値に変更
            event_groups:
              - event_id: 0x8001   # ★ イベント ID（0x8000 以上）

        required_service_instances:
          - service_interface_id: 0xAB01   # ★ provided の id と一致させる
```

### B-2. ARXML を生成する

```bash
python3 tools/arxml_generator/generate_arxml.py \
  --input  my_vehiclespeed_service.yaml \
  --output my_vehiclespeed_manifest.arxml \
  --overwrite \
  --print-summary
```

期待される出力:
```
[arxml-generator] generation summary
  output: my_vehiclespeed_manifest.arxml
  packages: 1
  communication clusters: 1
  ethernet connectors: 2
  provided SOME/IP instances: 1
  required SOME/IP instances: 1
  warnings: 0
```

### B-3. C++ バインディングヘッダを生成する

```bash
python3 tools/arxml_generator/generate_ara_com_binding.py \
  --input   my_vehiclespeed_manifest.arxml \
  --output  my_vehiclespeed_binding.h \
  --namespace  vehiclespeed::generated \
  --provided-service-short-name  VehicleSpeedProviderInstance \
  --provided-event-group-short-name  SensorDataEventGroup
```

生成される `my_vehiclespeed_binding.h`:
```cpp
// Auto-generated from ARXML. Do not edit manually.
namespace vehiclespeed {
namespace generated {
constexpr std::uint16_t kServiceId{0xAB01U};
constexpr std::uint16_t kInstanceId{0x0001U};
constexpr std::uint16_t kStatusEventId{0x8001U};
constexpr std::uint16_t kStatusEventGroupId{0x0001U};
constexpr std::uint8_t kMajorVersion{0x01U};
constexpr std::uint8_t kMinorVersion{0x00U};
}
}
```

---

## Step 4: ユーザーアプリへの組み込み

### 4-1. ディレクトリ構成を作る

```bash
mkdir -p user_apps/src/apps/my_vehiclespeed
```

作成するファイル:
```
user_apps/src/apps/my_vehiclespeed/
├── CMakeLists.txt              ← ビルド定義
├── types.h                     ← Skeleton / Proxy クラス
├── vehiclespeed_binding.h      ← 生成済みバインディング定数
├── provider_app.cpp            ← Publisher アプリ
└── consumer_app.cpp            ← Subscriber アプリ
```

### 4-2. types.h を準備する

GUI で生成した `types.h` をコピーするか、既存テンプレートをカスタマイズします:

```bash
# GUI で生成した場合
cp /tmp/pubsub_gen/types.h user_apps/src/apps/my_vehiclespeed/

# または既存テンプレートをコピー
cp user_apps/src/apps/communication/someip/types.h \
   user_apps/src/apps/my_vehiclespeed/types.h
```

**types.h のカスタマイズポイント:**

```cpp
namespace vehiclespeed::generated {

// ① ARXML の値と一致させる
constexpr std::uint16_t kServiceId{0xAB01U};
constexpr std::uint16_t kInstanceId{0x0001U};
constexpr std::uint16_t kEventId{0x8001U};
constexpr std::uint16_t kEventGroupId{0x0001U};
constexpr std::uint8_t  kMajorVersion{0x01U};

// ② ペイロード構造体を定義する
struct SpeedData {
    std::uint32_t sequence;
    std::uint16_t speedKph;
    std::uint8_t  gear;
};

// ③ Skeleton / Proxy クラス（自動生成済みをそのまま使用 or カスタマイズ）
class VehicleSpeedSkeleton : public ara::com::ServiceSkeletonBase {
public:
    ara::com::SkeletonEvent<SpeedData> SpeedEvent;
    // ...
};

class VehicleSpeedProxy : public ara::com::ServiceProxyBase {
public:
    ara::com::ProxyEvent<SpeedData> SpeedEvent;
    // ...
};

} // namespace
```

### 4-3. Publisher アプリを作成する

```cpp
// provider_app.cpp — 速度データを送信
#include <atomic>
#include <chrono>
#include <csignal>
#include <iostream>
#include <thread>

#include <ara/core/initialization.h>
#include <ara/log/logging_framework.h>
#include "types.h"

namespace {
    std::atomic<bool> gRunning{true};
    void HandleSignal(int) { gRunning.store(false); }
}

int main() {
    std::signal(SIGINT, HandleSignal);
    std::signal(SIGTERM, HandleSignal);

    // ① ランタイム初期化
    if (!ara::core::Initialize().HasValue()) {
        std::cerr << "Initialize failed\n";
        return 1;
    }

    // ② ロガー作成
    auto logging = std::unique_ptr<ara::log::LoggingFramework>{
        ara::log::LoggingFramework::Create(
            "VSPD", ara::log::LogMode::kConsole,
            ara::log::LogLevel::kInfo, "VehicleSpeed Publisher")};
    auto& logger = logging->CreateLogger("VSPD", "Publisher", ara::log::LogLevel::kInfo);

    // ③ Skeleton（サービス提供者）を生成
    using namespace vehiclespeed::generated;
    auto specifier = CreateInstanceSpecifierOrThrow(
        "AdaptiveAutosar/UserApps/VehicleSpeedProvider");
    VehicleSpeedSkeleton skeleton{std::move(specifier)};

    // ④ サービスを提供開始
    if (!skeleton.OfferService().HasValue()) {
        std::cerr << "OfferService failed\n";
        ara::core::Deinitialize();
        return 1;
    }
    skeleton.SpeedEvent.Offer();

    std::cout << "[VehicleSpeedProvider] 配信開始。Ctrl+C で停止\n";

    // ⑤ 定期的にデータを送信
    std::uint32_t seq = 0;
    while (gRunning.load()) {
        ++seq;
        SpeedData data{};
        data.sequence = seq;
        data.speedKph = static_cast<std::uint16_t>(40 + (seq % 120));
        data.gear     = static_cast<std::uint8_t>((seq / 10) % 6 + 1);

        skeleton.SpeedEvent.Send(data);

        if (seq % 10 == 0) {
            auto s = logger.WithLevel(ara::log::LogLevel::kInfo);
            s << "送信 seq=" << seq
              << " speed=" << static_cast<int>(data.speedKph)
              << " gear="  << static_cast<int>(data.gear);
            logging->Log(logger, ara::log::LogLevel::kInfo, s);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

    // ⑥ クリーンアップ
    skeleton.SpeedEvent.StopOffer();
    skeleton.StopOfferService();
    ara::core::Deinitialize();
    return 0;
}
```

### 4-4. Subscriber アプリを作成する

```cpp
// consumer_app.cpp — 速度データを受信
#include <atomic>
#include <chrono>
#include <csignal>
#include <iostream>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

#include <ara/core/initialization.h>
#include <ara/log/logging_framework.h>
#include "types.h"

namespace {
    std::atomic<bool> gRunning{true};
    void HandleSignal(int) { gRunning.store(false); }
}

int main() {
    std::signal(SIGINT, HandleSignal);
    std::signal(SIGTERM, HandleSignal);

    if (!ara::core::Initialize().HasValue()) {
        std::cerr << "Initialize failed\n";
        return 1;
    }

    auto logging = std::unique_ptr<ara::log::LoggingFramework>{
        ara::log::LoggingFramework::Create(
            "VSUB", ara::log::LogMode::kConsole,
            ara::log::LogLevel::kInfo, "VehicleSpeed Subscriber")};
    auto& logger = logging->CreateLogger("VSUB", "Subscriber", ara::log::LogLevel::kInfo);

    using namespace vehiclespeed::generated;

    // ① サービス探索（非同期）
    std::mutex handleMutex;
    std::vector<VehicleSpeedProxy::HandleType> handles;

    auto specifier = CreateInstanceSpecifierOrThrow(
        "AdaptiveAutosar/UserApps/VehicleSpeedConsumer");

    auto findResult = VehicleSpeedProxy::StartFindService(
        [&handleMutex, &handles](VehicleSpeedProxy::HandleType found) {
            std::lock_guard<std::mutex> lock(handleMutex);
            handles = {found};
        },
        std::move(specifier));

    if (!findResult.HasValue()) {
        std::cerr << "StartFindService failed\n";
        ara::core::Deinitialize();
        return 1;
    }

    std::cout << "[VehicleSpeedConsumer] 受信待機中。Ctrl+C で停止\n";

    std::unique_ptr<VehicleSpeedProxy> proxy;
    std::uint32_t recvCount = 0;

    while (gRunning.load()) {
        // ② ハンドルが得られたらプロキシを生成してサブスクライブ
        if (!proxy) {
            std::lock_guard<std::mutex> lock(handleMutex);
            if (!handles.empty()) {
                proxy = std::make_unique<VehicleSpeedProxy>(handles.front());

                // ③ イベント購読（キュー深さ 32）
                proxy->SpeedEvent.Subscribe(32U);

                // ④ 受信ハンドラを登録
                proxy->SpeedEvent.SetReceiveHandler(
                    [&proxy, &logging, &logger, &recvCount]() {
                        proxy->SpeedEvent.GetNewSamples(
                            [&](ara::com::SamplePtr<SpeedData> sample) {
                                ++recvCount;
                                auto s = logger.WithLevel(ara::log::LogLevel::kInfo);
                                s << "受信 #" << recvCount
                                  << " seq="   << sample->sequence
                                  << " speed=" << static_cast<int>(sample->speedKph)
                                  << " gear="  << static_cast<int>(sample->gear);
                                logging->Log(logger, ara::log::LogLevel::kInfo, s);
                            }, 16U);
                    });
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }

    // ⑤ クリーンアップ
    if (proxy) {
        proxy->SpeedEvent.UnsetReceiveHandler();
        proxy->SpeedEvent.Unsubscribe();
    }
    VehicleSpeedProxy::StopFindService(findResult.Value());
    ara::core::Deinitialize();
    return 0;
}
```

### 4-5. CMakeLists.txt を作成する

```cmake
# user_apps/src/apps/my_vehiclespeed/CMakeLists.txt

add_user_template_target(
  vehiclespeed_provider
  provider_app.cpp
  AdaptiveAutosarAP::ara_core
  AdaptiveAutosarAP::ara_com
  AdaptiveAutosarAP::ara_log
)
target_include_directories(vehiclespeed_provider
  PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")

add_user_template_target(
  vehiclespeed_consumer
  consumer_app.cpp
  AdaptiveAutosarAP::ara_core
  AdaptiveAutosarAP::ara_com
  AdaptiveAutosarAP::ara_log
)
target_include_directories(vehiclespeed_consumer
  PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")

foreach(target vehiclespeed_provider vehiclespeed_consumer)
  if(USER_APPS_VSOMEIP_INCLUDE_DIR)
    target_include_directories(${target} SYSTEM PRIVATE "${USER_APPS_VSOMEIP_INCLUDE_DIR}")
  endif()
endforeach()
```

`user_apps/src/apps/CMakeLists.txt` に追記:
```cmake
add_subdirectory(my_vehiclespeed)
```

---

## Step 5: ビルド

### 5-1. インストール済みランタイムからビルドする（推奨）

```bash
./scripts/build_user_apps_from_opt.sh --prefix /opt/autosar_ap
```

成功時の出力:
```
[100%] Built target vehiclespeed_provider
[100%] Built target vehiclespeed_consumer
```

実行ファイルの場所:
```
build-user-apps-opt/src/apps/my_vehiclespeed/
├── vehiclespeed_provider
└── vehiclespeed_consumer
```

### 5-2. cmake を直接使う場合

```bash
cmake -S user_apps -B build-user-apps \
  -DAUTOSAR_AP_INSTALL_PREFIX=/opt/autosar_ap \
  -DCMAKE_BUILD_TYPE=Release

cmake --build build-user-apps -j$(nproc) \
  --target vehiclespeed_provider vehiclespeed_consumer
```

### 5-3. CMake への ARXML コード生成の統合（オプション）

`cmake --build` 時に自動で ARXML → C++ ヘッダ生成を行う場合:

```cmake
if(ARA_COM_ENABLE_ARXML_CODEGEN)
  set(MY_ARXML  "${CMAKE_SOURCE_DIR}/my_vehiclespeed_manifest.arxml")
  set(MY_BIND_H "${CMAKE_BINARY_DIR}/generated/vehiclespeed_binding.h")

  add_custom_command(
    OUTPUT  "${MY_BIND_H}"
    COMMAND Python3::Interpreter
            "${CMAKE_SOURCE_DIR}/tools/arxml_generator/generate_ara_com_binding.py"
            --input  "${MY_ARXML}"
            --output "${MY_BIND_H}"
            --namespace vehiclespeed::generated
            --provided-service-short-name VehicleSpeedProviderInstance
            --provided-event-group-short-name SensorDataEventGroup
    DEPENDS "${MY_ARXML}"
    COMMENT "Generating vehiclespeed binding header from ARXML"
  )

  add_custom_target(vehiclespeed_codegen ALL DEPENDS "${MY_BIND_H}")
  add_dependencies(vehiclespeed_provider vehiclespeed_codegen)
  target_include_directories(vehiclespeed_provider
    PRIVATE "${CMAKE_BINARY_DIR}/generated")
endif()
```

---

## Step 6: 実行と確認

### 6-1. 環境変数を設定する

```bash
export AUTOSAR_AP_PREFIX=/opt/autosar_ap
export VSOMEIP_CONFIGURATION=${AUTOSAR_AP_PREFIX}/configuration/vsomeip-pubsub-sample.json
export LD_LIBRARY_PATH=${AUTOSAR_AP_PREFIX}/lib:${AUTOSAR_AP_PREFIX}/lib64:/opt/vsomeip/lib:${LD_LIBRARY_PATH:-}
```

### 6-2. Publisher を起動する（端末 1）

```bash
./build-user-apps-opt/src/apps/my_vehiclespeed/vehiclespeed_provider
```

期待される出力:
```
[VehicleSpeedProvider] 配信開始。Ctrl+C で停止
[VehicleSpeedProvider] 送信 seq=10 speed=50 gear=2
[VehicleSpeedProvider] 送信 seq=20 speed=60 gear=3
```

### 6-3. Subscriber を起動する（端末 2）

```bash
./build-user-apps-opt/src/apps/my_vehiclespeed/vehiclespeed_consumer
```

期待される出力:
```
[VehicleSpeedConsumer] 受信待機中。Ctrl+C で停止
[VehicleSpeedConsumer] 受信 #1 seq=5 speed=44 gear=1
[VehicleSpeedConsumer] 受信 #2 seq=6 speed=45 gear=1
```

### 6-4. 動作確認チェックリスト

- [ ] Publisher が「送信」ログを出力している
- [ ] Subscriber が「受信」ログを出力している
- [ ] 両端末の `seq=` 番号が対応している
- [ ] Ctrl+C で両プロセスが正常終了する

---

## YAML パラメータリファレンス

### サービス ID の決め方

```
範囲               推奨用途
0x0000             予約済み（使用不可）
0x0001–0x7FFF      標準 SOME/IP サービス
0x8000–0xFFFD      実装依存・実験的サービス
0xFFFE–0xFFFF      予約済み（使用不可）

プロジェクト内で重複しないように管理してください。
pubsub_designer_gui.py の "Auto-generate ID" で
サービス名から決定論的な ID を生成できます。
```

### イベント ID の決め方

```
0x0001–0x7FFF  メソッド ID（RPC 用）
0x8000–0xFFFE  イベント ID ← Pub/Sub はこの範囲を使用
0xFFFF         予約済み
```

### ポート番号の選び方

```
1024–49151   登録済みポート（他サービスとの競合に注意）
49152–65535  動的 / プライベートポート（比較的安全）

参考:
30490  SOME/IP SD（変更しないこと）
30509  Provider ポート例
30510  Consumer ポート例
```

### SD 遅延の調整

```yaml
sd_server:
  initial_delay_min: 20    # ms — Provider がアドバタイズを始めるまでの最小遅延
  initial_delay_max: 200   # ms — Provider がアドバタイズを始めるまでの最大遅延

sd_client:
  initial_delay_min: 20    # ms — Consumer がサービスを探し始めるまでの最小遅延
  initial_delay_max: 200   # ms — Consumer がサービスを探し始めるまでの最大遅延
```

- 素早く接続したい場合: `min=0, max=50`
- 多数 ECU に対して負荷分散したい場合: `min=0, max=2000`

---

## よくある問題と対処法

### Subscriber に何も届かない

**原因 1: vSomeIP 設定ファイルが見つからない**
```bash
echo $VSOMEIP_CONFIGURATION
ls "$VSOMEIP_CONFIGURATION"
grep -i service "$VSOMEIP_CONFIGURATION"
```

**原因 2: ARXML と types.h のサービス ID が一致していない**
```bash
grep "SERVICE-INTERFACE-ID" my_vehiclespeed_manifest.arxml
grep "kServiceId"           user_apps/src/apps/my_vehiclespeed/types.h
# 両方の値が一致していること
```

**原因 3: ファイアウォールでポートがブロックされている**
```bash
sudo ufw allow 30509/udp
sudo ufw allow 30510/udp
```

### "OfferService failed" が出る

ランタイムが起動していないか、`LD_LIBRARY_PATH` が正しくない場合:
```bash
ps aux | grep autosar_platform_app
echo $LD_LIBRARY_PATH
```

### ARXML 生成エラー: "service_interface_id is required"

```yaml
# 以下のどちらかの書き方が必要です:
service_interface_id: 0x1234    # フラット形式
# または
service_interface:
  id: 0x1234                    # ネスト形式
```

### 複数のサービスを同時に動かしたい

サービスごとに異なる ID とポートを使います:
```yaml
# service_a.yaml
service_interface.id: 0x1001
provider_port: 30509

# service_b.yaml
service_interface.id: 0x1002
provider_port: 30511
```

---

## 次のステップ

| チュートリアル | 内容 |
| ------------- | ---- |
| `05_zerocopy_pubsub.ja.md` | iceoryx を使った共有メモリゼロコピー通信 |
| `06_dds_pubsub.ja.md` | Cyclone DDS を使った分散通信 |
| `07_socketcan_decode.ja.md` | CAN バスデータの受信とデコード |
| `08_ecu_full_stack.ja.md` | ECU フルスタック統合 |
| `09_rpi_ecu_deployment.ja.md` | Raspberry Pi への配備 |

---

## クイックリファレンス

```bash
# GUI デザイナー起動
python3 tools/arxml_generator/pubsub_designer_gui.py

# YAML → ARXML 生成
python3 tools/arxml_generator/generate_arxml.py \
  --input my_service.yaml --output my_service.arxml --overwrite

# ARXML → C++ バインディングヘッダ生成
python3 tools/arxml_generator/generate_ara_com_binding.py \
  --input my_service.arxml --output binding.h \
  --namespace my::ns

# YAML バリデーションのみ（ファイル出力なし）
python3 tools/arxml_generator/generate_arxml.py \
  --input my_service.yaml --validate-only --strict --print-summary

# user_apps ビルド
./scripts/build_user_apps_from_opt.sh --prefix /opt/autosar_ap

# 実行環境変数設定
export AUTOSAR_AP_PREFIX=/opt/autosar_ap
export VSOMEIP_CONFIGURATION=$AUTOSAR_AP_PREFIX/configuration/vsomeip-pubsub-sample.json
export LD_LIBRARY_PATH=$AUTOSAR_AP_PREFIX/lib:/opt/vsomeip/lib:${LD_LIBRARY_PATH:-}
```
