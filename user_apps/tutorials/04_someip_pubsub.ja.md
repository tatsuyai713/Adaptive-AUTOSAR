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

## 実行前準備

```bash
export VSOMEIP_CONFIGURATION=./configuration/vsomeip-pubsub-sample.json
export LD_LIBRARY_PATH=/opt/autosar_ap/lib:/opt/vsomeip/lib:$LD_LIBRARY_PATH
```

## 実行

端末1:

```bash
./build-user-apps-opt/autosar_user_com_someip_provider_template
```

端末2:

```bash
./build-user-apps-opt/autosar_user_com_someip_consumer_template
```

## 変更ポイント

1. 送受信データ型を `user_apps/src/apps/communication/someip/types.h` で自アプリ用に変更する。
2. 受信ハンドラで業務ロジック呼び出しへ接続する。
3. `SubscriptionState` 変化時の復帰処理を追加する。
