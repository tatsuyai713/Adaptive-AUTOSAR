# user_apps チュートリアル一覧

このディレクトリは、`user_apps` の各機能を学ぶためのステップバイステップ手順書です。
すべて AUTOSAR AP ランタイムが `/opt/autosar_ap` にインストール済みであることを前提とします。

## 共通ビルドコマンド

```bash
./scripts/build_user_apps_from_opt.sh --prefix /opt/autosar_ap
```

ビルド後の実行ファイルは `build-user-apps-opt/src/apps/...` 以下に生成されます。

## バックエンド選択方針（Manifest-first）

新規アプリ設計では、マニフェストプロファイル切替を主軸にしてください。

- 単一バイナリを維持し、`ARA_COM_BINDING_MANIFEST` でバックエンドを切替える
- 標準フローは `user_apps/src/apps/communication/switchable_pubsub/README.md` を参照
- このディレクトリの `04`/`05`/`06` は transport 個別テンプレートの参照用

## 目次

0. **Pub/Sub 通信 完全チュートリアル** ← **まずここから始めてください**
   [`00_pubsub_e2e_tutorial.ja.md`](00_pubsub_e2e_tutorial.ja.md)
   YAML 定義 → ARXML 生成 → C++ コード生成 → ビルド → 実行 の全フロー解説
1. Manifest プロファイルでのバックエンド切替（**推奨**）
   [`11_manifest_profile_switching_e2e.ja.md`](11_manifest_profile_switching_e2e.ja.md)

2. ランタイム初期化と終了
   [`01_runtime_lifecycle.ja.md`](01_runtime_lifecycle.ja.md)
3. 実行制御シグナル処理
   [`02_exec_signal.ja.md`](02_exec_signal.ja.md)
4. Persistency と PHM
   [`03_per_phm.ja.md`](03_per_phm.ja.md)
5. SOME/IP Pub/Sub（transport 焦点、標準 `ara::com` API テンプレート）
   [`04_someip_pubsub.ja.md`](04_someip_pubsub.ja.md)
6. ZeroCopy Pub/Sub（transport 焦点テンプレート）
   [`05_zerocopy_pubsub.ja.md`](05_zerocopy_pubsub.ja.md)
7. DDS Pub/Sub（transport 焦点テンプレート）
   [`06_dds_pubsub.ja.md`](06_dds_pubsub.ja.md)
8. SocketCAN 受信とデコード
   [`07_socketcan_decode.ja.md`](07_socketcan_decode.ja.md)
9. ECU フルスタック統合
   [`08_ecu_full_stack.ja.md`](08_ecu_full_stack.ja.md)
10. Raspberry Pi ECU 配備
   [`09_rpi_ecu_deployment.ja.md`](09_rpi_ecu_deployment.ja.md)
11. Vector/ETAS/EB 資産の移植
    [`10_vendor_autosar_asset_porting.ja.md`](10_vendor_autosar_asset_porting.ja.md)
12. `ara::exec` 実行状態報告と監視
    [`12_exec_runtime_monitoring.ja.md`](12_exec_runtime_monitoring.ja.md)
13. Ubuntu 側 DoIP/DIAG テスター（host_tools に移動）
    [`../../tools/host_tools/doip_diag_tester/README.ja.md`](../../tools/host_tools/doip_diag_tester/README.ja.md)

## 関連ツール

| ツール | 説明 |
| ------ | ---- |
| [`tools/arxml_generator/pubsub_designer_gui.py`](../../tools/arxml_generator/pubsub_designer_gui.py) | **Pub/Sub Service Designer GUI** — フォーム入力でサービスを設計し YAML/ARXML/C++ を一括生成 |
| [`tools/arxml_generator/arxml_generator_gui.py`](../../tools/arxml_generator/arxml_generator_gui.py) | YAML → ARXML 変換 GUI（従来ツール） |
| [`tools/arxml_generator/generate_arxml.py`](../../tools/arxml_generator/generate_arxml.py) | YAML → ARXML CLI ジェネレータ |
| [`tools/arxml_generator/generate_ara_com_binding.py`](../../tools/arxml_generator/generate_ara_com_binding.py) | ARXML → C++ バインディング定数ヘッダ生成 |
| [`tools/arxml_generator/examples/my_first_service.yaml`](../../tools/arxml_generator/examples/my_first_service.yaml) | 初心者向け YAML テンプレート（コメント付き） |
