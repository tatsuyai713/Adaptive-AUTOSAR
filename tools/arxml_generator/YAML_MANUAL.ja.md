# YAML 記述マニュアル (ARXML Generator)

このドキュメントは `tools/arxml_generator/generate_arxml.py` 向けの
YAML 記述仕様をまとめた実践マニュアルです。

## 1. 重要な前提

- デフォルト動作は **AUTOSAR標準要素 + DDS セクションを許可** です。
  - `zerocopy` / `custom_elements` を含む YAML は、`--allow-extensions` なしではエラーになります。
- このジェネレータが **標準要素として直接生成** するのは主に以下です。
  - `COMMUNICATION-CLUSTER`
  - `ETHERNET-COMMUNICATION-CONNECTOR`
  - `PROVIDED-SOMEIP-SERVICE-INSTANCE`
  - `REQUIRED-SOMEIP-SERVICE-INSTANCE`
- `dds` は標準セクションとして ARXML に出力し、`zerocopy` は **拡張要素 (`EXT-*`)** として出力します。
- `generate_ara_com_binding.py` が現在参照するのは SOME/IP の ID 群です。
  - DDS/ZeroCopy 情報は、将来の自動生成や設定共有向けに保持する目的です。

## 2. ルート構造

```yaml
variables: {}
autosar:
  schema_namespace: "http://autosar.org/schema/r4.0"
  schema_location: "http://autosar.org/schema/r4.0 autosar_00050.xsd"
  packages: []
```

- `variables` は任意です。
- `autosar.packages` は必須です。
- `autosar.package` でも単数指定できます（内部で list 化）。

## 3. 変数展開

```yaml
variables:
  LOCAL_IP: "127.0.0.1"

autosar:
  packages:
    - short_name: Example
      communication_cluster:
        ethernet_physical_channels:
          - network_endpoints:
              - short_name: App
                ipv4_address: "${LOCAL_IP}"
```

- `${LOCAL_IP}` は `variables` と環境変数を使って展開されます。

## 4. package で使えるキー

- `short_name` (必須)
- `communication_cluster` / `communication_clusters`
- `ethernet_communication_connector` / `ethernet_communication_connectors`
- `someip`
- `provided_someip_service_instances` (someip 外に直置きする互換キー)
- `required_someip_service_instances` (同上)
- `dds` (標準セクション)
- `zerocopy` (拡張要素)
- `custom_elements`

## 5. 通信クラスタ

```yaml
communication_cluster:
  short_name: LocalCluster
  protocol_version: 1
  ethernet_physical_channels:
    - short_name: LoopbackChannel
      network_endpoints:
        - short_name: ProviderEndpoint
          ipv4_address: "127.0.0.1"
```

- `network_endpoints[].ipv4_address` は IPv4 として検証されます。

## 6. Ethernet Connector

```yaml
ethernet_communication_connectors:
  - short_name: ProviderConnector
    ap_application_endpoints:
      - short_name: ProviderUdp
        transport: udp
        port: 30509
```

- `transport` は `udp` or `tcp`。
- `port` は `1..65535`。

## 7. SOME/IP

```yaml
someip:
  provided_service_instances:
    - short_name: VehicleStatusProvider
      service_interface_id: 0x1234
      major_version: 1
      minor_version: 0
      service_instance_id: 1
      event_groups:
        - short_name: VehicleStatusEventGroup
          event_group_id: 1
          event_id: 0x8001
          multicast_udp_port: 30509
          ipv4_multicast_ip_address: "239.255.0.1"
      sd_server:
        initial_delay_min: 20
        initial_delay_max: 200

  required_service_instances:
    - short_name: VehicleStatusConsumer
      service_interface_id: 0x1234
      major_version: 1
      minor_version: 0
      required_event_groups:
        - short_name: VehicleStatusEventGroupRequirement
          event_group_id: 1
          event_id: 0x8001
      sd_client:
        initial_delay_min: 20
        initial_delay_max: 200
```

サービス ID / イベント ID の書式:
- `4660` (10進) と `0x1234` (16進) の両方を受け付けます。

## 8. DDS (標準セクション)

```yaml
dds:
  short_name: VehicleStatusDdsBinding
  domain_id: 0
  provided_topics:
    - short_name: VehicleStatusTx
      topic_name: "adaptive_autosar/sample/ara_com_pubsub/VehicleStatusFrame"
      type_name: "sample::ara_com_pubsub::VehicleStatusFrame"
      qos_profile: "reliable_keep_last_10"
  required_topics:
    - short_name: VehicleStatusRx
      topic_name: "adaptive_autosar/sample/ara_com_pubsub/VehicleStatusFrame"
      type_name: "sample::ara_com_pubsub::VehicleStatusFrame"
      qos_profile: "reliable_keep_last_10"
```

生成される主な ARXML タグ:
- `DDS-BINDING`
- `DDS-PROVIDED-TOPIC`
- `DDS-REQUIRED-TOPIC`

補足:
- `publish_topics` は `provided_topics` の別名
- `subscribe_topics` は `required_topics` の別名

## 9. ZeroCopy (拡張セクション)

```yaml
zerocopy:
  short_name: VehicleStatusZeroCopyBinding
  runtime: iceoryx
  enabled: true
  service_channels:
    - short_name: VehicleStatusEventChannel
      service_id: 0x1234
      instance_id: 1
      event_id: 0x8001
      publisher_history: 16
      subscriber_queue: 16
      max_sample_size: 512
      max_publishers: 2
      max_subscribers: 8
```

このセクションを使う場合は必ず `--allow-extensions` を指定してください。

生成される主な ARXML 拡張タグ:
- `EXT-ZEROCOPY-BINDING`
- `EXT-ZEROCOPY-SERVICE-CHANNEL`

別名:
- `service_instances` は `service_channels` の別名

## 10. custom_elements (任意拡張)

標準外・独自タグを自由に埋め込む場合は `custom_elements` を使います。

```yaml
custom_elements:
  - tag: DO-IP-INSTANTIATION
    short_name: DoIpGateway
    children:
      - tag: LOGICAL-ADDRESS
        text: "0x0E00"
```

`custom_elements` を使う場合も `--allow-extensions` が必要です。

## 11. strict モード

```bash
python3 tools/arxml_generator/generate_arxml.py \
  --input your.yaml \
  --validate-only \
  --strict \
  --print-summary
```

- `--strict` を使うと、未知キーでエラー終了します。
- `--strict` なしでは警告としてサマリに表示します。
- `--allow-extensions` なしでは `zerocopy` / `custom_elements` を拒否します。

拡張セクション付き YAML を検証する場合:

```bash
python3 tools/arxml_generator/generate_arxml.py \
  --input your_with_extensions.yaml \
  --validate-only \
  --strict \
  --allow-extensions \
  --print-summary
```

## 12. マージ仕様 (`--input` 複数指定)

- `variables`: 後勝ちで上書き
- `autosar.schema_namespace` / `schema_location`: 後勝ち
- `autosar.packages`: 入力順に連結

## 13. すぐ使えるサンプル

- SOME/IP 基本:
  - `tools/arxml_generator/examples/pubsub_vsomeip.yaml`
- SOME/IP + DDS + ZeroCopy拡張:
  - `tools/arxml_generator/examples/pubsub_multi_transport.yaml`
- 高機能・複数 package:
  - `tools/arxml_generator/examples/multi_service_gateway.yaml`
