# user_apps チュートリアル一覧

このディレクトリは、`user_apps` の各機能を学ぶためのステップバイステップ手順書です。
すべて AUTOSAR AP ランタイムが `/opt/autosar-ap` にインストール済みであることを前提とします。

## 共通ビルドコマンド

```bash
./scripts/build_user_apps_from_opt.sh --prefix /opt/autosar-ap
```

ビルド後の実行ファイルは `build-user-apps-opt/src/apps/...` 以下に生成されます。

## バックエンド選択方針（Manifest-first）

新規アプリ設計では、マニフェストプロファイル切替を主軸にしてください。

- 単一バイナリを維持し、`ARA_COM_BINDING_MANIFEST` でバックエンドを切替える
- 標準フローは `user_apps/src/apps/communication/switchable_pubsub/README.md` を参照
- このディレクトリの `10`/`11`/`12` は transport 個別テンプレートの参照用

## 目次

### 基礎 (00–04)

| # | タイトル | ファイル |
|---|---------|---------|
| 00 | **Pub/Sub 通信 完全チュートリアル** ← **まずここから** | [`00_pubsub_e2e_tutorial.ja.md`](00_pubsub_e2e_tutorial.ja.md) |
| 01 | ランタイム初期化と終了 | [`01_runtime_lifecycle.ja.md`](01_runtime_lifecycle.ja.md) |
| 02 | 実行制御シグナル処理 | [`02_exec_signal.ja.md`](02_exec_signal.ja.md) |
| 03 | Persistency と PHM | [`03_per_phm.ja.md`](03_per_phm.ja.md) |
| 04 | `ara::exec` 実行状態報告と監視 | [`04_exec_runtime_monitoring.ja.md`](04_exec_runtime_monitoring.ja.md) |

### 通信 (10–13)

| # | タイトル | ファイル |
|---|---------|---------|
| 10 | SOME/IP Pub/Sub（transport 焦点テンプレート） | [`10_someip_pubsub.ja.md`](10_someip_pubsub.ja.md) |
| 11 | ZeroCopy Pub/Sub（transport 焦点テンプレート） | [`11_zerocopy_pubsub.ja.md`](11_zerocopy_pubsub.ja.md) |
| 12 | DDS Pub/Sub（transport 焦点テンプレート） | [`12_dds_pubsub.ja.md`](12_dds_pubsub.ja.md) |
| 13 | Manifest プロファイルでのバックエンド切替（**推奨**） | [`13_manifest_profile_switching_e2e.ja.md`](13_manifest_profile_switching_e2e.ja.md) |

### 車両統合 (20–22)

| # | タイトル | ファイル |
|---|---------|---------|
| 20 | SocketCAN 受信とデコード | [`20_socketcan_decode.ja.md`](20_socketcan_decode.ja.md) |
| 21 | ECU フルスタック統合 | [`21_ecu_full_stack.ja.md`](21_ecu_full_stack.ja.md) |
| 22 | 本番 ECU アプリケーション | [`22_production_ecu_app.ja.md`](22_production_ecu_app.ja.md) |

### プラットフォームサービス (30–33)

| # | タイトル | ファイル |
|---|---------|---------|
| 30 | プラットフォームサービスアーキテクチャ（17 デーモン、20 サービス） | [`30_platform_service_architecture.ja.md`](30_platform_service_architecture.ja.md) |
| 31 | 診断サーバー（UDS/DoIP） | [`31_diag_server.ja.md`](31_diag_server.ja.md) |
| 32 | Platform Health Management デーモン | [`32_phm_daemon.ja.md`](32_phm_daemon.ja.md) |
| 33 | 暗号プロバイダー（HSM、鍵ローテーション） | [`33_crypto_provider.ja.md`](33_crypto_provider.ja.md) |

### 実行管理 (40)

| # | タイトル | ファイル |
|---|---------|---------|
| 40 | ExecutionManager でユーザーアプリを管理・実行する | [`40_execution_manager_orchestration.ja.md`](40_execution_manager_orchestration.ja.md) |

### 配備 (50–52)

| # | タイトル | ファイル |
|---|---------|---------|
| 50 | Raspberry Pi ECU 配備（全 20 サービスプロファイル） | [`50_rpi_ecu_deployment.ja.md`](50_rpi_ecu_deployment.ja.md) |
| 51 | RPi 最小構成 ECU — ライブラリビルドからユーザーアプリ管理まで | [`51_rpi_minimal_ecu_user_app_management.ja.md`](51_rpi_minimal_ecu_user_app_management.ja.md) |
| 52 | Vector/ETAS/EB 資産の移植 | [`52_vendor_autosar_asset_porting.ja.md`](52_vendor_autosar_asset_porting.ja.md) |

### ホストツール

| ツール | 説明 |
|--------|------|
| Ubuntu 側 DoIP/DIAG テスター | [`../../tools/host_tools/doip_diag_tester/README.ja.md`](../../tools/host_tools/doip_diag_tester/README.ja.md) |

## 関連ツール

| ツール | 説明 |
| ------ | ---- |
| [`tools/arxml_generator/pubsub_designer_gui.py`](../../tools/arxml_generator/pubsub_designer_gui.py) | **Pub/Sub Service Designer GUI** — フォーム入力でサービスを設計し YAML/ARXML/C++ を一括生成 |
| [`tools/arxml_generator/arxml_generator_gui.py`](../../tools/arxml_generator/arxml_generator_gui.py) | YAML → ARXML 変換 GUI（従来ツール） |
| [`tools/arxml_generator/generate_arxml.py`](../../tools/arxml_generator/generate_arxml.py) | YAML → ARXML CLI ジェネレータ |
| [`tools/arxml_generator/generate_ara_com_binding.py`](../../tools/arxml_generator/generate_ara_com_binding.py) | ARXML → C++ バインディング定数ヘッダ生成 |
| [`tools/arxml_generator/examples/my_first_service.yaml`](../../tools/arxml_generator/examples/my_first_service.yaml) | 初心者向け YAML テンプレート（コメント付き） |
