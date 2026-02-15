# ARXML Generator (YAML -> ARXML)

このディレクトリには、**既存ファイルを編集せず**に ARXML を生成するためのツールを追加しています。

## 1. 何ができるか

`generate_arxml.py` は YAML から AUTOSAR 形式の ARXML を生成します。

主な機能:
- 複数 YAML 入力のマージ (`--input` を複数指定)
- 通信要素の生成
  - `COMMUNICATION-CLUSTER`
  - `ETHERNET-COMMUNICATION-CONNECTOR`
  - `PROVIDED-SOMEIP-SERVICE-INSTANCE`
  - `REQUIRED-SOMEIP-SERVICE-INSTANCE`
- DDS要素の生成
  - `DDS-BINDING`
  - `DDS-PROVIDED-TOPIC`
  - `DDS-REQUIRED-TOPIC`
- 拡張要素の生成 (`--allow-extensions` 指定時のみ)
  - `EXT-ZEROCOPY-BINDING` (ZeroCopy実装メタデータ)
- 16進数/10進数 ID (`0x1234` など) の解釈
- IPv4/port バリデーション
- 変数展開 (`variables` + 環境変数 `${...}`)
- 未知キー検出 (`--strict`)
- `custom_elements` による拡張タグ挿入

重要:
- デフォルトでは **AUTOSAR標準要素 + DDS設定** を許可します。
  - `zerocopy` / `custom_elements` は `--allow-extensions` 指定時のみ有効です。
- `generate_ara_com_binding.py` が現在コード生成で参照するのは SOME/IP 要素です。
- DDSは標準設定セクションとして出力し、ZeroCopyは実装依存拡張として扱います。

## 2. ファイル構成

- `tools/arxml_generator/generate_arxml.py` : CLI ジェネレータ本体
- `tools/arxml_generator/generate_ara_com_binding.py` : ARXML から ara::com 向け C++ 定数ヘッダを生成
- `tools/arxml_generator/arxml_generator_gui.py` : 簡易 GUI (Tkinter)
- `tools/arxml_generator/examples/pubsub_vsomeip.yaml` : Pub/Sub 基本サンプル
- `tools/arxml_generator/examples/pubsub_multi_transport.yaml` : SOME/IP + DDS + ZeroCopy 拡張サンプル
- `tools/arxml_generator/examples/multi_service_gateway.yaml` : 高機能サンプル
- `tools/arxml_generator/examples/overlay_override.yaml` : マージ用サンプル
- `tools/arxml_generator/YAML_MANUAL.ja.md` : YAML 記述マニュアル

## 3. 使い方 (CLI)

### 3.1 単一 YAML から生成

```bash
python3 tools/arxml_generator/generate_arxml.py \
  --input tools/arxml_generator/examples/pubsub_vsomeip.yaml \
  --output /tmp/pubsub_generated.arxml \
  --print-summary
```

SOME/IP + DDS + ZeroCopy拡張込みの例:

```bash
python3 tools/arxml_generator/generate_arxml.py \
  --input tools/arxml_generator/examples/pubsub_multi_transport.yaml \
  --output /tmp/pubsub_multi_transport.arxml \
  --allow-extensions \
  --print-summary
```

### 3.2 複数 YAML をマージして生成

```bash
python3 tools/arxml_generator/generate_arxml.py \
  --input tools/arxml_generator/examples/pubsub_vsomeip.yaml \
  --input tools/arxml_generator/examples/overlay_override.yaml \
  --output /tmp/merged_generated.arxml \
  --overwrite \
  --print-summary
```

### 3.3 バリデーションのみ

```bash
python3 tools/arxml_generator/generate_arxml.py \
  --input tools/arxml_generator/examples/multi_service_gateway.yaml \
  --validate-only \
  --strict \
  --allow-extensions \
  --print-summary
```

### 3.4 ARXML -> C++ バインディングコード生成

`generate_ara_com_binding.py` は ARXML から、ビルドで使用する
`SERVICE-ID`/`INSTANCE-ID`/`EVENT-ID` などの C++ ヘッダを自動生成します。

```bash
python3 tools/arxml_generator/generate_ara_com_binding.py \
  --input configuration/pubsub_sample_manifest.arxml \
  --output /tmp/vehicle_status_manifest_binding.h \
  --namespace sample::vehicle_status::generated \
  --provided-service-short-name VehicleStatusProviderInstance \
  --provided-event-group-short-name VehicleStatusEventGroup
```

### 3.5 CMake への組み込み (自動生成 -> 自動ビルド)

このリポジトリでは `ARA_COM_ENABLE_ARXML_CODEGEN=ON` のとき、
`cmake --build` 時に以下を自動実行します。

1. `configuration/pubsub_sample_manifest.arxml` を入力に
2. `tools/arxml_generator/generate_ara_com_binding.py` を実行し
3. `build/generated/ara_com_generated/vehicle_status_manifest_binding.h` を生成
4. 生成ヘッダをサンプルターゲットの include に自動追加

主な CMake キャッシュ変数:
- `ARA_COM_ENABLE_ARXML_CODEGEN` : `ON/OFF`
- `ARA_COM_CODEGEN_INPUT_ARXML` : 入力 ARXML パス
- `ARA_COM_CODEGEN_PROVIDED_SERVICE_SHORT_NAME` : 対象 `PROVIDED-SOMEIP-SERVICE-INSTANCE`
- `ARA_COM_CODEGEN_PROVIDED_EVENT_GROUP_SHORT_NAME` : 対象 `SOMEIP-PROVIDED-EVENT-GROUP`

### 3.6 End-to-End: YAML -> ARXML -> C++ codegen -> build

以下で、サンプルYAMLから生成したARXMLをそのままビルドに使えます。

```bash
python3 tools/arxml_generator/generate_arxml.py \
  --input tools/arxml_generator/examples/pubsub_vsomeip.yaml \
  --output configuration/pubsub_sample_manifest.generated.arxml \
  --overwrite \
  --print-summary

cmake -S . -B build \
  -DARA_COM_ENABLE_ARXML_CODEGEN=ON \
  -DARA_COM_CODEGEN_INPUT_ARXML=configuration/pubsub_sample_manifest.generated.arxml \
  -DARA_COM_CODEGEN_PROVIDED_SERVICE_SHORT_NAME=VehicleStatusProviderInstance \
  -DARA_COM_CODEGEN_PROVIDED_EVENT_GROUP_SHORT_NAME=VehicleStatusEventGroup

cmake --build build -j$(nproc)
```

ビルド時に生成されるヘッダ:
- `build/generated/ara_com_generated/vehicle_status_manifest_binding.h`

## 4. 使い方 (GUI)

```bash
python3 tools/arxml_generator/arxml_generator_gui.py
```

GUI でできること:
- 入力 YAML の追加/削除
- 出力 ARXML パス指定
- インデント、strict、allow-extensions、validate-only の指定
- 実行結果サマリ表示

## 5. YAML の基本構造

```yaml
variables:
  LOOPBACK_IP: "127.0.0.1"

autosar:
  schema_namespace: "http://autosar.org/schema/r4.0"
  schema_location: "http://autosar.org/schema/r4.0 autosar_00050.xsd"
  packages:
    - short_name: MyManifest
      communication_cluster:
        short_name: LocalCluster
        protocol_version: 1
        ethernet_physical_channels:
          - short_name: LoopbackChannel
            network_endpoints:
              - short_name: AppEndpoint
                ipv4_address: "${LOOPBACK_IP}"

      ethernet_communication_connectors:
        - short_name: AppConnector
          ap_application_endpoints:
            - short_name: AppUdp
              transport: udp
              port: 30500

      someip:
        provided_service_instances:
          - short_name: ProviderInstance
            service_interface_id: 0x1234
            major_version: 1
            minor_version: 0
            service_instance_id: 1
            event_groups:
              - short_name: EventGroup
                event_group_id: 1
                event_id: 0x8001
                multicast_udp_port: 30500
                ipv4_multicast_ip_address: "239.255.0.1"
            sd_server:
              initial_delay_min: 20
              initial_delay_max: 200

        required_service_instances:
          - short_name: ConsumerRequirement
            service_interface_id: 0x1234
            major_version: 1
            minor_version: 0
            required_event_groups:
              - short_name: EventGroupRequirement
                event_group_id: 1
                event_id: 0x8001
            sd_client:
              initial_delay_min: 20
              initial_delay_max: 200

      custom_elements:
        - tag: SOME-OTHER-AUTOSAR-ELEMENT
          short_name: OptionalExtension
          children:
            - tag: VALUE
              text: 1
```

上記の `custom_elements` を使う場合は `--allow-extensions` が必要です。

## 6. 依存関係

- Python 3
- PyYAML (`yaml`)

PyYAML がない場合:
```bash
python3 -m pip install pyyaml
```

## 7. YAML 記述仕様

詳細な YAML 記述ルールは以下を参照してください:
- `tools/arxml_generator/YAML_MANUAL.ja.md`
