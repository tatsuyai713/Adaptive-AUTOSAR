# 05: ZeroCopy Pub/Sub

## 対象ターゲット

- `autosar_user_com_zerocopy_pub_template`
- `autosar_user_com_zerocopy_sub_template`
- ソース:
  - `user_apps/src/apps/communication/zerocopy/pub_app.cpp`
  - `user_apps/src/apps/communication/zerocopy/sub_app.cpp`

## 目的

- `ara::com::zerocopy` API で共有メモリベース通信を使う。
- Loan/Publish と TryTake の基本を理解する。

## 実行前準備

```bash
export AUTOSAR_AP_PREFIX=/opt/autosar_ap
export LD_LIBRARY_PATH=${AUTOSAR_AP_PREFIX}/lib:${AUTOSAR_AP_PREFIX}/lib64:/opt/iceoryx/lib:${LD_LIBRARY_PATH:-}
```

必要に応じて RouDi を起動してください。

## 実行

端末1:

```bash
./build-user-apps-opt/src/apps/communication/zerocopy/autosar_user_com_zerocopy_pub_template
```

端末2:

```bash
./build-user-apps-opt/src/apps/communication/zerocopy/autosar_user_com_zerocopy_sub_template
```

## 変更ポイント

1. チャネル識別子（service/instance/event）を自アプリ値へ合わせる。
2. Loan サイズを固定長ではなくデータサイズに合わせて算出する。
3. 未受信タイムアウト時の挙動（再試行、監視カウンタ）を追加する。
