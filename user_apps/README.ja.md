# User Apps（AUTOSAR AP 外部コンシューマプロジェクト）

`user_apps` は、インストール済みの AUTOSAR AP 成果物（例: `/opt/autosar_ap`）に対して
ビルドされる外部コンシューマプロジェクトです。

このディレクトリは責務ごとに構成されています:
- `src/apps`: 機能別に分類された実行可能なユーザーアプリケーション
- `src/features`: 高度なアプリが使用する再利用可能なフィーチャーモジュール

## ビルド

リポジトリルートから:

```bash
./scripts/build_user_apps_from_opt.sh --prefix /opt/autosar_ap
```

ビルドされた実行ファイルは以下のようなサブディレクトリに生成されます:
- `build-user-apps-opt/src/apps/basic/`
- `build-user-apps-opt/src/apps/communication/*/`
- `build-user-apps-opt/src/apps/feature/*/`

クイック検索:

```bash
find build-user-apps-opt -type f -name "autosar_user_*" -perm -111 | sort
```

独自の外部ソースツリーからビルドする場合:

```bash
./scripts/build_user_apps_from_opt.sh \
  --prefix /opt/autosar_ap \
  --source-dir /path/to/your_user_apps
```

### フォルダー分割 CMake

`user_apps` はフォルダーレベルの CMake ファイルに分割されています:

- ルート共通設定: `user_apps/CMakeLists.txt`
- アプリグループディスパッチャー: `user_apps/src/apps/CMakeLists.txt`
- フォルダー別ターゲットファイル:
  - `user_apps/src/apps/basic/CMakeLists.txt`
  - `user_apps/src/apps/communication/*/CMakeLists.txt`
  - `user_apps/src/apps/feature/*/CMakeLists.txt`

CMake オプションで選択したグループのみビルドできます:

- `-DUSER_APPS_BUILD_APPS_BASIC=ON|OFF`
- `-DUSER_APPS_BUILD_APPS_COMMUNICATION=ON|OFF`
- `-DUSER_APPS_BUILD_APPS_FEATURE=ON|OFF`

例（フィーチャーアプリのみビルド）:

```bash
AP_CMAKE_DIR=/opt/autosar_ap/lib/cmake/AdaptiveAutosarAP
if [ ! -f "${AP_CMAKE_DIR}/AdaptiveAutosarAPConfig.cmake" ]; then
  AP_CMAKE_DIR=/opt/autosar_ap/lib64/cmake/AdaptiveAutosarAP
fi

cmake -S user_apps -B build-user-apps-feature-only \
  -DAUTOSAR_AP_PREFIX=/opt/autosar_ap \
  -DAdaptiveAutosarAP_DIR="${AP_CMAKE_DIR}" \
  -DUSER_APPS_BUILD_APPS_BASIC=OFF \
  -DUSER_APPS_BUILD_APPS_COMMUNICATION=OFF \
  -DUSER_APPS_BUILD_APPS_FEATURE=ON
cmake --build build-user-apps-feature-only -j"$(nproc)"
```

## アプリターゲット

### 基本アプリ（`user_apps/src/apps/basic/`）
- `autosar_user_minimal_runtime`
- `autosar_user_exec_signal_template`
- `autosar_user_per_phm_demo`

### 通信アプリ（`user_apps/src/apps/communication/`）
- `autosar_user_com_someip_provider_template`
- `autosar_user_com_someip_consumer_template`
- `autosar_user_com_zerocopy_pub_template`
- `autosar_user_com_zerocopy_sub_template`
- `autosar_user_com_dds_pub_template`
- `autosar_user_com_dds_sub_template`

### フィーチャーアプリ（`user_apps/src/apps/feature/`）
- `autosar_user_tpl_runtime_lifecycle`
- `autosar_user_tpl_exec_runtime_monitor`
- `autosar_user_tpl_can_socketcan_receiver`
- `autosar_user_tpl_ecu_full_stack`
- `autosar_user_tpl_ecu_someip_source`

## ソース構成

- アプリエントリポイント:
  - `user_apps/src/apps/basic/`
  - `user_apps/src/apps/communication/`
  - `user_apps/src/apps/feature/`
- 共有フィーチャーモジュール:
  - `user_apps/src/features/communication/vehicle_status/`
  - `user_apps/src/features/communication/pubsub/`
  - `user_apps/src/features/communication/can/`
  - `user_apps/src/features/ecu/`

## フィーチャーチュートリアル

すべてのチュートリアルは `user_apps/tutorials/` にあります。

- インデックス: `user_apps/tutorials/README.ja.md`
- ランタイムライフサイクル: `user_apps/tutorials/01_runtime_lifecycle.ja.md`
- 実行シグナルハンドリング: `user_apps/tutorials/02_exec_signal.ja.md`
- `ara::exec` 実行状態報告と監視: `user_apps/tutorials/12_exec_runtime_monitoring.ja.md`
- Persistency + PHM: `user_apps/tutorials/03_per_phm.ja.md`
- SOME/IP pub/sub: `user_apps/tutorials/04_someip_pubsub.ja.md`
- ゼロコピー pub/sub: `user_apps/tutorials/05_zerocopy_pubsub.ja.md`
- DDS pub/sub: `user_apps/tutorials/06_dds_pubsub.ja.md`
- SocketCAN 受信/デコード: `user_apps/tutorials/07_socketcan_decode.ja.md`
- ECU フルフロー: `user_apps/tutorials/08_ecu_full_stack.ja.md`
- Raspberry Pi ECU デプロイメント: `user_apps/tutorials/09_rpi_ecu_deployment.ja.md`
- Vector/ETAS/EB 資産の移植: `user_apps/tutorials/10_vendor_autosar_asset_porting.ja.md`
- Ubuntu DoIP/DIAG ホストテスター: `tools/host_tools/doip_diag_tester/README.ja.md`

## クイックスタート: 独自アプリの追加

1. フィーチャーアプリのソースを新しいアプリとしてコピー。

```bash
cp user_apps/src/apps/feature/ecu/ecu_full_stack_app.cpp \
  user_apps/src/my_new_ecu_app.cpp
```

2. ソースを対応するアプリグループ（例: `user_apps/src/apps/feature/ecu/`）に配置し、
   そのフォルダーの `CMakeLists.txt`（ルートファイルではない）にターゲットを登録する。
   インポートされた AUTOSAR AP ターゲット（`AdaptiveAutosarAP::ara_core`、
   `AdaptiveAutosarAP::ara_com` など）にリンクすること。

3. `scripts/build_user_apps_from_opt.sh` で再ビルド。

4. ビルドディレクトリから新しいバイナリを実行。
