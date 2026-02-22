# 11: Manifest プロファイル切替 E2E

## 対象

- ビルド/インストールスクリプト:
  - `scripts/build_and_install_autosar_ap.sh`
- switchable ユーザーアプリのビルド/実行スクリプト:
  - `scripts/build_switchable_pubsub_sample.sh`
  - （後方互換のためスクリプト名に `sample` を残しています）
- ユーザーアプリソース:
  - `user_apps/src/apps/communication/switchable_pubsub/src/switchable_pub.cpp`
  - `user_apps/src/apps/communication/switchable_pubsub/src/switchable_sub.cpp`

## 目的

- `ARA_COM_BINDING_MANIFEST` の切替だけで、同一バイナリのバックエンド切替を検証する。
- 同じ Publisher/Subscriber バイナリを DDS/iceoryx/vSomeIP の各プロファイルで動かす。

## 実行前準備

```bash
export AUTOSAR_AP_PREFIX=/tmp/autosar_ap
export CYCLONEDDS_PREFIX=/opt/cyclonedds
export VSOMEIP_PREFIX=/opt/vsomeip
```

ミドルウェア/ランタイム配置:

- CycloneDDS: `${CYCLONEDDS_PREFIX}`
- vSomeIP: `${VSOMEIP_PREFIX}`
- iceoryx: `/opt/iceoryx`

## 1) ランタイムをビルドしてインストール

```bash
./scripts/build_and_install_autosar_ap.sh \
  --prefix "${AUTOSAR_AP_PREFIX}" \
  --build-dir build-install-manifest-e2e \
  --build-type Release \
  --source-dir "$PWD" \
  --with-platform-app
```

## 2) switchable ユーザーアプリをビルドして E2E smoke を実行

この 1 コマンドで以下を連続実行します:

- DDS プロファイル smoke
- iceoryx プロファイル smoke
- vSomeIP プロファイル smoke

```bash
./scripts/build_switchable_pubsub_sample.sh \
  --prefix "${AUTOSAR_AP_PREFIX}" \
  --cyclonedds-prefix "${CYCLONEDDS_PREFIX}" \
  --build-dir build-switchable-pubsub-sample-e2e \
  --run-smoke
```

期待される主な出力:

- `[OK] DDS-profile smoke passed (...)`
- `[OK] iceoryx-profile smoke passed (...)`
- `[OK] vSomeIP-profile smoke passed (...)`
- `[OK] Switchable pub/sub smoke checks passed`

## 3) 手動実行（任意）

### 3-1. 実行環境を設定

```bash
export LD_LIBRARY_PATH=${AUTOSAR_AP_PREFIX}/lib:${AUTOSAR_AP_PREFIX}/lib64:${CYCLONEDDS_PREFIX}/lib:/opt/vsomeip/lib:/opt/iceoryx/lib:${LD_LIBRARY_PATH:-}
```

### 3-2. DDS プロファイル

端末 1:

```bash
unset ARA_COM_EVENT_BINDING
unset ARA_COM_PREFER_SOMEIP
unset VSOMEIP_CONFIGURATION
export ARA_COM_BINDING_MANIFEST=$PWD/build-switchable-pubsub-sample-e2e/generated/switchable_manifest_dds.yaml
./build-switchable-pubsub-sample-e2e/autosar_switchable_pubsub_sub
```

端末 2:

```bash
unset ARA_COM_EVENT_BINDING
unset ARA_COM_PREFER_SOMEIP
unset VSOMEIP_CONFIGURATION
export ARA_COM_BINDING_MANIFEST=$PWD/build-switchable-pubsub-sample-e2e/generated/switchable_manifest_dds.yaml
./build-switchable-pubsub-sample-e2e/autosar_switchable_pubsub_pub
```

### 3-3. iceoryx プロファイル

端末 1:

```bash
unset ARA_COM_EVENT_BINDING
unset ARA_COM_PREFER_SOMEIP
unset VSOMEIP_CONFIGURATION
export ARA_COM_BINDING_MANIFEST=$PWD/build-switchable-pubsub-sample-e2e/generated/switchable_manifest_iceoryx.yaml
iox-roudi
```

端末 2:

```bash
unset ARA_COM_EVENT_BINDING
unset ARA_COM_PREFER_SOMEIP
unset VSOMEIP_CONFIGURATION
export ARA_COM_BINDING_MANIFEST=$PWD/build-switchable-pubsub-sample-e2e/generated/switchable_manifest_iceoryx.yaml
./build-switchable-pubsub-sample-e2e/autosar_switchable_pubsub_sub
```

端末 3:

```bash
unset ARA_COM_EVENT_BINDING
unset ARA_COM_PREFER_SOMEIP
unset VSOMEIP_CONFIGURATION
export ARA_COM_BINDING_MANIFEST=$PWD/build-switchable-pubsub-sample-e2e/generated/switchable_manifest_iceoryx.yaml
./build-switchable-pubsub-sample-e2e/autosar_switchable_pubsub_pub
```

### 3-4. vSomeIP プロファイル

端末 1:

```bash
unset ARA_COM_EVENT_BINDING
unset ARA_COM_PREFER_SOMEIP
export ARA_COM_BINDING_MANIFEST=$PWD/build-switchable-pubsub-sample-e2e/generated/switchable_manifest_vsomeip.yaml
export VSOMEIP_CONFIGURATION=${AUTOSAR_AP_PREFIX}/configuration/vsomeip-rpi.json
${AUTOSAR_AP_PREFIX}/bin/autosar_vsomeip_routing_manager
```

端末 2:

```bash
unset ARA_COM_EVENT_BINDING
unset ARA_COM_PREFER_SOMEIP
export ARA_COM_BINDING_MANIFEST=$PWD/build-switchable-pubsub-sample-e2e/generated/switchable_manifest_vsomeip.yaml
export VSOMEIP_CONFIGURATION=${AUTOSAR_AP_PREFIX}/configuration/vsomeip-rpi.json
./build-switchable-pubsub-sample-e2e/autosar_switchable_pubsub_sub
```

端末 3:

```bash
unset ARA_COM_EVENT_BINDING
unset ARA_COM_PREFER_SOMEIP
export ARA_COM_BINDING_MANIFEST=$PWD/build-switchable-pubsub-sample-e2e/generated/switchable_manifest_vsomeip.yaml
export VSOMEIP_CONFIGURATION=${AUTOSAR_AP_PREFIX}/configuration/vsomeip-rpi.json
./build-switchable-pubsub-sample-e2e/autosar_switchable_pubsub_pub
```

## 動作確認チェックリスト

- 全モードで同じバイナリを使っている:
  - `autosar_switchable_pubsub_pub`
  - `autosar_switchable_pubsub_sub`
- DDS/iceoryx/vSomeIP の差分は manifest パスのみ。
- 全モードで Subscriber ログに `I heard seq=` が出る。

## トラブルシューティング

manifest パス確認:

```bash
ls build-switchable-pubsub-sample-e2e/generated/switchable_manifest_dds.yaml
ls build-switchable-pubsub-sample-e2e/generated/switchable_manifest_iceoryx.yaml
ls build-switchable-pubsub-sample-e2e/generated/switchable_manifest_vsomeip.yaml
```

ルーティング設定確認:

```bash
ls "${AUTOSAR_AP_PREFIX}/configuration/vsomeip-rpi.json"
echo "$VSOMEIP_CONFIGURATION"
```

smoke 失敗時はログを確認:

```bash
ls build-switchable-pubsub-sample-e2e/switchable_*.log
```
