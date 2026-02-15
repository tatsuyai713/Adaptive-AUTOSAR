# 10: Vector/ETAS/EB 向け資産を本プラットフォームで動かす手順

## 対象とゴール

このチュートリアルは、以下の要件を満たす資産を対象にします。

- 既存資産: Vector / ETAS / EB などの Adaptive AUTOSAR 実装向け C++ アプリ資産
- 目的:
  - 本リポジトリ実装上で再ビルドして実行する
  - 他 UNIT と `ara::com` 通信できることを確認する

重要:

- これは **ソース互換** の移植手順です。
- ベンダ実装でビルド済みバイナリをそのまま動かす手順ではありません。

## 0) 先に押さえる制約

### そのまま流用しやすい資産

- `ara::core / ara::com / ara::exec / ara::log / ara::diag` の標準 API を中心に使っている
- ベンダ固有ヘッダ/名前空間に依存していない
- 通信パラメータ (Service/Instance/Event/DDS topic 等) が明示されている

### 修正が必要な資産

- ベンダ固有 API (`vSomeIP` を直接呼ぶ独自層、独自 EM API 等) を直接参照している
- ベンダ生成の Proxy/Skeleton クラス名・配置を前提にしている
- ランタイム設定をベンダツール専用形式に固定している

## 1) ランタイムをインストール

まず本実装をインストールします。

```bash
sudo ./scripts/build_and_install_autosar_ap.sh --prefix /opt/autosar_ap
```

Raspberry Pi ECU として使う場合:

```bash
sudo ./scripts/build_and_install_rpi_ecu_profile.sh \
  --prefix /opt/autosar_ap \
  --runtime-build-dir build-rpi-autosar-ap \
  --user-app-build-dir /opt/autosar_ap/user_apps_build
```

## 2) 既存資産の棚卸し (移植判定)

次の観点で既存ソースを確認します。

1. ベンダ固有 include を使っていないか
2. `ara::com` のイベント/メソッド呼び出しが標準 API に収まっているか
3. サービス ID / インスタンス ID / イベント ID が取得可能か
4. 実行時設定 (`SOME/IP`, `DDS`, `ZeroCopy`) の切替点があるか

推奨コマンド例:

```bash
rg -n "vector|etas|elektrobit|vsomeip|iceoryx|cyclonedds" /path/to/your_asset
rg -n "ara::com|ara::core|ara::exec|ara::log" /path/to/your_asset
```

## 3) ユーザーアプリ側プロジェクトを作る

既存 `user_apps` を雛形にするか、外部ツリーを用意します。

```bash
cp -r /opt/autosar_ap/user_apps /path/to/your_user_apps
```

ビルド:

```bash
./scripts/build_user_apps_from_opt.sh \
  --prefix /opt/autosar_ap \
  --source-dir /path/to/your_user_apps \
  --build-dir build-your-user-apps
```

## 4) CMake を標準ターゲットに合わせる

`/opt/autosar_ap` の imported targets を使ってリンクします。

最小例:

```cmake
find_package(AdaptiveAutosarAP REQUIRED CONFIG
  PATHS /opt/autosar_ap/lib/cmake/AdaptiveAutosarAP
  NO_DEFAULT_PATH)

add_executable(my_ecu_app src/my_ecu_app.cpp)
target_link_libraries(my_ecu_app
  PRIVATE
    AdaptiveAutosarAP::ara_core
    AdaptiveAutosarAP::ara_com
    AdaptiveAutosarAP::ara_exec
    AdaptiveAutosarAP::ara_log)
```

## 5) 通信資産の合わせ込み

### SOME/IP を使う場合

1. アプリ名を `configuration/vsomeip-pubsub-sample.json` に合わせる
2. 実行時に `VSOMEIP_CONFIGURATION` を設定する
3. Service/Instance/Event IDs を相手 UNIT と一致させる

```bash
export VSOMEIP_CONFIGURATION=/opt/autosar_ap/configuration/vsomeip-pubsub-sample.json
```

### DDS を使う場合

1. Domain ID と Topic 名を相手 UNIT と一致させる
2. IDL 型の互換性を揃える (本リポジトリは `idlc` 利用)

### ZeroCopy を使う場合

1. `iox-roudi` を起動する
2. Publisher/Subscriber で同一チャネル記述子を使う

## 6) ベンダ依存コードの置換方針

原則として、アプリ層は `ara::` のみを見る形に寄せます。

1. ベンダ独自ラッパを 1 箇所に隔離する
2. アプリ本体は `ara::com` API 呼び出しだけにする
3. ID や topic は設定値化する
4. 依存を `CMakeLists.txt` で `/opt/autosar_ap` に限定する

## 7) 既存 UNIT と疎通確認

### A. 単体通信 (テンプレート間) で足場確認

```bash
./scripts/verify_rpi_ecu_profile.sh \
  --prefix /opt/autosar_ap \
  --user-app-build-dir /opt/autosar_ap/user_apps_build \
  --can-backend mock \
  --require-platform-binary
```

### B. あなたの移植アプリで確認

1. Provider 側 UNIT を起動
2. 移植した Consumer 側を起動
3. 受信ログとカウンタを確認
4. 逆方向 (Consumer/Provider) も確認

判定ポイント:

- サービス発見が成立している
- Subscribe/ReceiveHandler が呼ばれている
- 期待した payload が decode できる

## 8) Raspberry Pi ECU 常駐化

`systemd` 化して ECU 風に運用できます。

```bash
sudo ./scripts/install_rpi_ecu_services.sh \
  --prefix /opt/autosar_ap \
  --user-app-build-dir /opt/autosar_ap/user_apps_build \
  --enable
```

環境変数設定:

- `/etc/default/autosar-ecu-full-stack`

## 9) トラブルシュート

### SOME/IP が見つからない

- `VSOMEIP_CONFIGURATION` のパス確認
- アプリ名/Client ID 重複の有無
- Service/Instance/Event IDs の不一致

### DDS 受信しない

- Domain ID / Topic 不一致
- IDL 型差異 (フィールド順/型)

### ZeroCopy が動かない

- `iox-roudi` 未起動
- 共有メモリ権限問題 (環境依存)

## 10) 実務向けチェックリスト

1. ベンダ固有 API 依存を排除した
2. `ara::` API のみでビルド可能
3. `/opt/autosar_ap` のみ参照してビルド可能
4. 相手 UNIT と ID / topic / version を一致させた
5. 起動順を含めた疎通手順を文書化した
6. `systemd` 常駐時の再起動/ログ取得手順を確認した

この 6 点を満たせば、ベンダ実装向け資産を本プラットフォームで「他 UNIT と通信できる開発・検証資産」として運用しやすくなります。
