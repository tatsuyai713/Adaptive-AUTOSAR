# user_apps 機能別チュートリアル

このディレクトリは、`user_apps` のテンプレートを機能ごとに学ぶための手順書です。  
すべて `--prefix /opt/autosar_ap` を前提にしています。

## 共通ビルド手順

```bash
./scripts/build_user_apps_from_opt.sh --prefix /opt/autosar_ap
```

ビルド後の実行ファイルは通常 `build-user-apps-opt/` 配下に生成されます。

## 目次

1. ランタイム初期化と終了  
   `01_runtime_lifecycle.ja.md`
2. 実行制御シグナル処理  
   `02_exec_signal.ja.md`
3. Persistency と PHM  
   `03_per_phm.ja.md`
4. SOME/IP Pub/Sub  
   `04_someip_pubsub.ja.md`
5. ZeroCopy Pub/Sub  
   `05_zerocopy_pubsub.ja.md`
6. DDS Pub/Sub  
   `06_dds_pubsub.ja.md`
7. SocketCAN 受信とデコード  
   `07_socketcan_decode.ja.md`
8. ECU フルスタック統合  
   `08_ecu_full_stack.ja.md`
9. Raspberry Pi ECU 配備  
   `09_rpi_ecu_deployment.ja.md`
10. Vector/ETAS/EB 資産の移植  
   `10_vendor_autosar_asset_porting.ja.md`
11. Ubuntu 側 DoIP/DIAG テスター  
   `11_doip_diag_tester.ja.md`
