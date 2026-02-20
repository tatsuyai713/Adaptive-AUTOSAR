# ARXML Generator (YAML → ARXML)

Tools for generating AUTOSAR ARXML manifests and C++ binding code
without modifying any existing repository files.

## 1. What it does

`generate_arxml.py` generates AUTOSAR-format ARXML from YAML definitions.

Key features:

- Merge multiple YAML inputs (`--input` repeated)
- Generate communication elements:
  - `COMMUNICATION-CLUSTER`
  - `ETHERNET-COMMUNICATION-CONNECTOR`
  - `PROVIDED-SOMEIP-SERVICE-INSTANCE`
  - `REQUIRED-SOMEIP-SERVICE-INSTANCE`
- Generate DDS elements:
  - `DDS-BINDING`, `DDS-PROVIDED-TOPIC`, `DDS-REQUIRED-TOPIC`
- Generate extension elements (only with `--allow-extensions`):
  - `EXT-ZEROCOPY-BINDING` (zero-copy implementation metadata)
- Parse hex/decimal IDs (`0x1234`, etc.)
- Validate IPv4 addresses and port ranges
- Variable substitution (`variables:` section + `${ENV_VAR}`)
- Unknown-key detection (`--strict`)
- Arbitrary tag insertion via `custom_elements`

> By default only **AUTOSAR-standard elements + DDS** are allowed.
> `zerocopy` / `custom_elements` require `--allow-extensions`.

## 2. File Layout

| File | Purpose |
| ---- | ------- |
| `generate_arxml.py` | CLI generator: YAML → ARXML |
| `generate_ara_com_binding.py` | Codegen: ARXML → C++ ara::com constants header |
| `pubsub_designer_gui.py` | **Service Designer GUI** (recommended for beginners) |
| `arxml_generator_gui.py` | Simple YAML-to-ARXML conversion GUI |
| `examples/my_first_service.yaml` | Beginner-friendly YAML template |
| `examples/pubsub_vsomeip.yaml` | Basic Pub/Sub SOME/IP example |
| `examples/pubsub_multi_transport.yaml` | SOME/IP + DDS + ZeroCopy example |
| `examples/multi_service_gateway.yaml` | Multi-service example |
| `examples/overlay_override.yaml` | YAML merge example |
| `YAML_MANUAL.ja.md` | Detailed YAML syntax reference (Japanese) |

## 3. CLI Usage

### 3.1 Generate from a single YAML

```bash
python3 tools/arxml_generator/generate_arxml.py \
  --input  tools/arxml_generator/examples/pubsub_vsomeip.yaml \
  --output /tmp/pubsub_generated.arxml \
  --print-summary
```

With SOME/IP + DDS + ZeroCopy extensions:

```bash
python3 tools/arxml_generator/generate_arxml.py \
  --input  tools/arxml_generator/examples/pubsub_multi_transport.yaml \
  --output /tmp/pubsub_multi_transport.arxml \
  --allow-extensions \
  --print-summary
```

### 3.2 Merge multiple YAML files

```bash
python3 tools/arxml_generator/generate_arxml.py \
  --input  tools/arxml_generator/examples/pubsub_vsomeip.yaml \
  --input  tools/arxml_generator/examples/overlay_override.yaml \
  --output /tmp/merged_generated.arxml \
  --overwrite \
  --print-summary
```

### 3.3 Validate only (no output written)

```bash
python3 tools/arxml_generator/generate_arxml.py \
  --input  tools/arxml_generator/examples/multi_service_gateway.yaml \
  --validate-only \
  --strict \
  --allow-extensions \
  --print-summary
```

### 3.4 Generate C++ binding header from ARXML

`generate_ara_com_binding.py` reads an ARXML manifest and emits a C++ header
containing `SERVICE-ID`, `INSTANCE-ID`, `EVENT-ID`, etc. as `constexpr` constants.

```bash
python3 tools/arxml_generator/generate_ara_com_binding.py \
  --input   configuration/pubsub_sample_manifest.arxml \
  --output  /tmp/vehicle_status_manifest_binding.h \
  --namespace sample::vehicle_status::generated \
  --provided-service-short-name VehicleStatusProviderInstance \
  --provided-event-group-short-name VehicleStatusEventGroup
```

### 3.5 CMake integration (auto-generate → auto-build)

When `ARA_COM_ENABLE_ARXML_CODEGEN=ON`, `cmake --build` automatically:

1. Reads `configuration/pubsub_sample_manifest.arxml`
2. Runs `generate_ara_com_binding.py`
3. Writes `build/generated/ara_com_generated/vehicle_status_manifest_binding.h`
4. Adds the generated header to the sample target's include path

Key CMake cache variables:

- `ARA_COM_ENABLE_ARXML_CODEGEN` — `ON`/`OFF`
- `ARA_COM_CODEGEN_INPUT_ARXML` — path to input ARXML
- `ARA_COM_CODEGEN_PROVIDED_SERVICE_SHORT_NAME` — target `PROVIDED-SOMEIP-SERVICE-INSTANCE`
- `ARA_COM_CODEGEN_PROVIDED_EVENT_GROUP_SHORT_NAME` — target `SOMEIP-PROVIDED-EVENT-GROUP`

### 3.6 End-to-End: YAML → ARXML → C++ codegen → build

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

Generated header:

- `build/generated/ara_com_generated/vehicle_status_manifest_binding.h`

## 4. GUI Usage

### 4.1 Pub/Sub Service Designer GUI (recommended)

Design a service from scratch using a form, with live YAML / ARXML / C++ previews
and one-click generation of all build artifacts:

```bash
python3 tools/arxml_generator/pubsub_designer_gui.py
```

Features:

- Set service name, ID (auto-generate from name), version
- Configure network settings (IP / ports) via form fields
- Manage events (add / edit / remove); define payload struct fields
- Real-time preview: YAML, ARXML, C++ `types.h`, C++ `binding.h`, Provider App, Consumer App
- **Generate All & Save** — writes every file to the selected output directory

See also: [`user_apps/tutorials/00_pubsub_e2e_tutorial.md`](../../user_apps/tutorials/00_pubsub_e2e_tutorial.md)

### 4.2 YAML → ARXML conversion GUI (legacy)

```bash
python3 tools/arxml_generator/arxml_generator_gui.py
```

Features:

- Add / remove input YAML files
- Specify output ARXML path
- Set indent, strict, allow-extensions, validate-only options
- Display generation summary

## 5. Minimal YAML Structure

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

`custom_elements` requires `--allow-extensions`.

## 6. Dependencies

- Python 3
- PyYAML

```bash
python3 -m pip install pyyaml
```

## 7. YAML Reference

Detailed YAML syntax rules: `tools/arxml_generator/YAML_MANUAL.ja.md`
