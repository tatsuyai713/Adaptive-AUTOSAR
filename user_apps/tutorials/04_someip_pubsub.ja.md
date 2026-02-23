# 04: SOME/IP Pub/Sub

## 対象ターゲット

- `autosar_user_com_someip_provider_template`
- `autosar_user_com_someip_consumer_template`
- ソース:
  - `user_apps/src/apps/communication/someip/provider_app.cpp`
  - `user_apps/src/apps/communication/someip/consumer_app.cpp`

## 目的

- ara::com API で Provider/Consumer を分離して実装する。
- Offer/Find/Subscribe/Receive の基本フローを理解する。

## 位置づけ（Manifest-first 方針）

- このチュートリアルは transport 固定の SOME/IP テンプレートです。
- バックエンド固有のバインディング処理は生成コード相当の Proxy/Skeleton ヘッダ（`user_apps/src/apps/communication/someip/types.h`）に閉じ込め、アプリ本体は標準 `ara::com` API を使います。
- 単一バイナリをマニフェストで切替える本流は `user_apps/src/apps/communication/switchable_pubsub/README.md` を参照してください。

## 実行前準備

```bash
export AUTOSAR_AP_PREFIX=/opt/autosar_ap
export VSOMEIP_CONFIGURATION=${AUTOSAR_AP_PREFIX}/configuration/vsomeip-pubsub-sample.json
export LD_LIBRARY_PATH=${AUTOSAR_AP_PREFIX}/lib:${AUTOSAR_AP_PREFIX}/lib64:/opt/vsomeip/lib:${LD_LIBRARY_PATH:-}
```

## 実行

端末1:

```bash
./build-user-apps-opt/src/apps/communication/someip/autosar_user_com_someip_provider_template
```

端末2:

```bash
./build-user-apps-opt/src/apps/communication/someip/autosar_user_com_someip_consumer_template
```

## 変更ポイント

1. 送受信データ型を `user_apps/src/apps/communication/someip/types.h` で自アプリ用に変更する。
2. 受信ハンドラで業務ロジック呼び出しへ接続する。
3. `SubscriptionState` 変化時の復帰処理を追加する。
