# 08: ECU フルスタック統合

## 対象ターゲット

- `autosar_user_tpl_ecu_full_stack`
- `autosar_user_tpl_ecu_someip_source` (SOME/IP 入力検証用)
- ソース: `user_apps/src/apps/feature/ecu/ecu_full_stack_app.cpp`
  - `user_apps/src/apps/feature/ecu/ecu_someip_source_app.cpp`

## 目的

- CAN 受信、SOME/IP 受信、DDS 送信を 1 プロセスで統合する。
- `ara::exec`、`ara::phm`、`ara::per` を組み合わせた実装骨格を作る。

## 実行前準備

```bash
export AUTOSAR_AP_PREFIX=/opt/autosar_ap
export VSOMEIP_CONFIGURATION=${AUTOSAR_AP_PREFIX}/configuration/vsomeip-pubsub-sample.json
export LD_LIBRARY_PATH=${AUTOSAR_AP_PREFIX}/lib:${AUTOSAR_AP_PREFIX}/lib64:/opt/vsomeip/lib:/opt/cyclonedds/lib:/opt/iceoryx/lib:${LD_LIBRARY_PATH:-}
```

## 実行

Mock CAN + SOME/IP 有効:

```bash
./build-user-apps-opt/src/apps/feature/ecu/autosar_user_tpl_ecu_full_stack \
  --can-backend=mock \
  --enable-can=true \
  --enable-someip=true \
  --publish-period-ms=50 \
  --log-every=20
```

SOME/IP 入力のみを使う場合 (別ターミナルでソース起動):

```bash
./build-user-apps-opt/src/apps/feature/ecu/autosar_user_tpl_ecu_someip_source --period-ms=100
./build-user-apps-opt/src/apps/feature/ecu/autosar_user_tpl_ecu_full_stack \
  --enable-can=false \
  --enable-someip=true \
  --require-someip=true
```

CAN のみ:

```bash
./build-user-apps-opt/src/apps/feature/ecu/autosar_user_tpl_ecu_full_stack \
  --can-backend=socketcan \
  --ifname=can0 \
  --enable-someip=false
```

## 機能ごとの編集箇所

1. 入力統合ロジック: `TryBuildOutputFrame(...)`
2. CAN デコード設定: `ParseRuntimeConfig(...)` と `VehicleStatusCanDecoder::Config`
3. DDS 出力設定: `BuildDdsProfile(...)` に渡す Domain/Topic
4. 健全性報告: `HealthReporter` の `ReportOk/ReportFailed/ReportDeactivated`
5. 永続化: `PersistentCounterStore` のキー設計と `Sync` タイミング

## 実装拡張の順序

1. まず CAN のみで起動確認する。
2. 次に SOME/IP 入力を有効化して統合結果を確認する。
3. 最後に ZeroCopy ローカル配信を有効化して性能確認する。
