# ARXML ジェネレータ（YAML → ARXML）

既存ファイルを変更せずに AUTOSAR ARXML マニフェストと C++ バインディングコードを
生成するためのツール群です。

## 1. できること

`generate_arxml.py` は YAML 定義から AUTOSAR 形式の ARXML を生成します。

主な機能:

- 複数 YAML のマージ（`--input` を複数指定）
- 通信要素の生成:
  - `COMMUNICATION-CLUSTER`
  - `ETHERNET-COMMUNICATION-CONNECTOR`
  - `PROVIDED-SOMEIP-SERVICE-INSTANCE`
  - `REQUIRED-SOMEIP-SERVICE-INSTANCE`
- DDS 要素の生成:
  - `DDS-BINDING`、`DDS-PROVIDED-TOPIC`、`DDS-REQUIRED-TOPIC`
- 拡張要素の生成（`--allow-extensions` 指定時のみ）:
  - `EXT-ZEROCOPY-BINDING`（ゼロコピー実装メタデータ）
- 16 進数 / 10 進数 ID の解釈（`0x1234` など）
- IPv4 アドレスとポート番号のバリデーション
- 変数展開（`variables:` セクション + `${ENV_VAR}`）
- 未知キーの検出（`--strict`）
- `custom_elements` による任意タグの挿入

> デフォルトでは **AUTOSAR 標準要素 + DDS** のみ許可されます。
> `zerocopy` / `custom_elements` は `--allow-extensions` が必要です。

## 2. ファイル構成

| ファイル | 役割 |
| -------- | ---- |
| `generate_arxml.py` | CLI ジェネレータ: YAML → ARXML |
| `generate_ara_com_binding.py` | コード生成: ARXML → C++ ara::com 定数ヘッダ |
| `pubsub_designer_gui.py` | **サービスデザイナー GUI**（初心者向け・推奨） |
| `arxml_generator_gui.py` | YAML → ARXML 変換 GUI（従来ツール） |
| `examples/my_first_service.yaml` | 初心者向け YAML テンプレート（コメント付き） |
| `examples/pubsub_vsomeip.yaml` | 基本的な Pub/Sub SOME/IP サンプル |
| `examples/pubsub_multi_transport.yaml` | SOME/IP + DDS + ZeroCopy サンプル |
| `examples/multi_service_gateway.yaml` | マルチサービスサンプル |
| `examples/overlay_override.yaml` | YAML マージサンプル |
| `YAML_MANUAL.ja.md` | YAML 記述仕様詳細リファレンス |

## 3. CLI の使い方

### 3.1 単一 YAML から生成する

```bash
python3 tools/arxml_generator/generate_arxml.py \
  --input  tools/arxml_generator/examples/pubsub_vsomeip.yaml \
  --output /tmp/pubsub_generated.arxml \
  --print-summary
```

SOME/IP + DDS + ZeroCopy 拡張を含む場合:

```bash
python3 tools/arxml_generator/generate_arxml.py \
  --input  tools/arxml_generator/examples/pubsub_multi_transport.yaml \
  --output /tmp/pubsub_multi_transport.arxml \
  --allow-extensions \
  --print-summary
```

### 3.2 複数 YAML をマージして生成する

```bash
python3 tools/arxml_generator/generate_arxml.py \
  --input  tools/arxml_generator/examples/pubsub_vsomeip.yaml \
  --input  tools/arxml_generator/examples/overlay_override.yaml \
  --output /tmp/merged_generated.arxml \
  --overwrite \
  --print-summary
```

### 3.3 バリデーションのみ（ファイル出力なし）

```bash
python3 tools/arxml_generator/generate_arxml.py \
  --input  tools/arxml_generator/examples/multi_service_gateway.yaml \
  --validate-only \
  --strict \
  --allow-extensions \
  --print-summary
```

### 3.4 ARXML → C++ バインディングヘッダを生成する

`generate_ara_com_binding.py` は ARXML マニフェストを読み込み、
`SERVICE-ID`、`INSTANCE-ID`、`EVENT-ID` などを `constexpr` 定数として
C++ ヘッダに出力します。

```bash
python3 tools/arxml_generator/generate_ara_com_binding.py \
  --input   configuration/pubsub_sample_manifest.arxml \
  --output  /tmp/vehicle_status_manifest_binding.h \
  --namespace sample::vehicle_status::generated \
  --provided-service-short-name VehicleStatusProviderInstance \
  --provided-event-group-short-name VehicleStatusEventGroup
```

### 3.5 CMake への統合（自動生成 → 自動ビルド）

`ARA_COM_ENABLE_ARXML_CODEGEN=ON` のとき、`cmake --build` が自動的に:

1. `configuration/pubsub_sample_manifest.arxml` を読み込み
2. `generate_ara_com_binding.py` を実行して
3. `build/generated/ara_com_generated/vehicle_status_manifest_binding.h` を生成し
4. サンプルターゲットのインクルードパスに追加します

主な CMake キャッシュ変数:

- `ARA_COM_ENABLE_ARXML_CODEGEN` — `ON`/`OFF`
- `ARA_COM_CODEGEN_INPUT_ARXML` — 入力 ARXML のパス
- `ARA_COM_CODEGEN_PROVIDED_SERVICE_SHORT_NAME` — 対象 `PROVIDED-SOMEIP-SERVICE-INSTANCE`
- `ARA_COM_CODEGEN_PROVIDED_EVENT_GROUP_SHORT_NAME` — 対象 `SOMEIP-PROVIDED-EVENT-GROUP`

### 3.6 エンドツーエンド: YAML → ARXML → C++ コード生成 → ビルド

```bash
python3 tools/arxml_generator/generate_arxml.py \
  --input  tools/arxml_generator/examples/pubsub_vsomeip.yaml \
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

生成されるヘッダ:

- `build/generated/ara_com_generated/vehicle_status_manifest_binding.h`

## 4. GUI の使い方

### 4.1 Pub/Sub Service Designer GUI（推奨）

フォーム入力でサービスを設計し、YAML / ARXML / C++ コードをリアルタイムプレビュー、
ワンクリックで全ファイルを生成します:

```bash
python3 tools/arxml_generator/pubsub_designer_gui.py
```

機能一覧:

- サービス名・ID（サービス名から自動生成可能）・バージョンの設定
- ネットワーク設定（IP / ポート）のフォーム入力
- イベントの追加 / 編集 / 削除（ペイロード構造体フィールドも定義可能）
- リアルタイムプレビュー: YAML、ARXML、C++ `types.h`、C++ `binding.h`、Provider App、Consumer App
- **Generate All & Save** — 選択ディレクトリに全ファイルを出力

詳細: [`user_apps/tutorials/00_pubsub_e2e_tutorial.ja.md`](../../user_apps/tutorials/00_pubsub_e2e_tutorial.ja.md)

### 4.2 YAML → ARXML 変換 GUI（従来ツール）

```bash
python3 tools/arxml_generator/arxml_generator_gui.py
```

機能一覧:

- 入力 YAML ファイルの追加 / 削除
- 出力 ARXML パスの指定
- インデント、strict、allow-extensions、validate-only オプションの設定
- 生成サマリの表示

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

`custom_elements` を使う場合は `--allow-extensions` が必要です。

## 6. 依存関係

- Python 3
- PyYAML

```bash
python3 -m pip install pyyaml
```

## 7. YAML 記述仕様

詳細な YAML 記述ルールは `tools/arxml_generator/YAML_MANUAL.ja.md` を参照してください。
