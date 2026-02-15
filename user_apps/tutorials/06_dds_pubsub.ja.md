# 06: DDS Pub/Sub

## 対象ターゲット

- `autosar_user_com_dds_pub_template`
- `autosar_user_com_dds_sub_template`
- ソース:
  - `user_apps/src/apps/communication/dds/pub_app.cpp`
  - `user_apps/src/apps/communication/dds/sub_app.cpp`
- IDL:
  - `user_apps/idl/UserAppsStatus.idl`

## 目的

- DDS バックエンドを ara::com 側から利用する流れを理解する。
- IDL からの型生成（`idlc`）とアプリ連携を理解する。

## 実行前準備

```bash
export AUTOSAR_AP_PREFIX=/opt/autosar_ap
export LD_LIBRARY_PATH=${AUTOSAR_AP_PREFIX}/lib:${AUTOSAR_AP_PREFIX}/lib64:/opt/cyclonedds/lib:${LD_LIBRARY_PATH:-}
```

## 実行

端末1:

```bash
./build-user-apps-opt/src/apps/communication/dds/autosar_user_com_dds_pub_template
```

端末2:

```bash
./build-user-apps-opt/src/apps/communication/dds/autosar_user_com_dds_sub_template
```

## 変更ポイント

1. `UserAppsStatus.idl` に必要フィールドを追加する。
2. 生成型に合わせて publish/subscribe 処理を更新する。
3. Domain ID と Topic 名を環境に合わせて固定または設定化する。
