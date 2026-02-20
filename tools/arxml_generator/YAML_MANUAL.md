# YAML Writing Manual (ARXML Generator)

This document is a practical reference for the YAML specification used with
`tools/arxml_generator/generate_arxml.py`.

## 1. Key Assumptions

- Default behavior **allows AUTOSAR-standard elements + DDS sections**.
  - YAML containing `zerocopy` / `custom_elements` will error without `--allow-extensions`.
- The generator **directly produces** the following as standard elements:
  - `COMMUNICATION-CLUSTER`
  - `ETHERNET-COMMUNICATION-CONNECTOR`
  - `PROVIDED-SOMEIP-SERVICE-INSTANCE`
  - `REQUIRED-SOMEIP-SERVICE-INSTANCE`
- `dds` is output as a standard ARXML section; `zerocopy` is output as an **extension element (`EXT-*`)**.
- `generate_ara_com_binding.py` currently references SOME/IP ID groups.
  - DDS/ZeroCopy information is retained for future auto-generation and configuration sharing.

## 2. Root Structure

```yaml
variables: {}
autosar:
  schema_namespace: "http://autosar.org/schema/r4.0"
  schema_location: "http://autosar.org/schema/r4.0 autosar_00050.xsd"
  packages: []
```

- `variables` is optional.
- `autosar.packages` is required.
- `autosar.package` (singular) is also accepted and is internally converted to a list.

## 3. Variable Substitution

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

- `${LOCAL_IP}` is expanded using `variables` and environment variables.

## 4. Keys Available in a Package

- `short_name` (required)
- `communication_cluster` / `communication_clusters`
- `ethernet_communication_connector` / `ethernet_communication_connectors`
- `someip`
- `provided_someip_service_instances` (compatibility key for placing directly outside `someip`)
- `required_someip_service_instances` (same)
- `dds` (standard section)
- `zerocopy` (extension element)
- `custom_elements`

## 5. Communication Cluster

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

- `network_endpoints[].ipv4_address` is validated as an IPv4 address.

## 6. Ethernet Connector

```yaml
ethernet_communication_connectors:
  - short_name: ProviderConnector
    ap_application_endpoints:
      - short_name: ProviderUdp
        transport: udp
        port: 30509
```

- `transport` is `udp` or `tcp`.
- `port` is `1..65535`.

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

Service ID / Event ID format:
- Both `4660` (decimal) and `0x1234` (hex) are accepted.

## 8. DDS (Standard Section)

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

Main generated ARXML tags:
- `DDS-BINDING`
- `DDS-PROVIDED-TOPIC`
- `DDS-REQUIRED-TOPIC`

Notes:
- `publish_topics` is an alias for `provided_topics`
- `subscribe_topics` is an alias for `required_topics`

## 9. ZeroCopy (Extension Section)

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

You must specify `--allow-extensions` to use this section.

Main generated ARXML extension tags:
- `EXT-ZEROCOPY-BINDING`
- `EXT-ZEROCOPY-SERVICE-CHANNEL`

Aliases:
- `service_instances` is an alias for `service_channels`

## 10. custom_elements (Arbitrary Extensions)

Use `custom_elements` to embed non-standard or proprietary tags:

```yaml
custom_elements:
  - tag: DO-IP-INSTANTIATION
    short_name: DoIpGateway
    children:
      - tag: LOGICAL-ADDRESS
        text: "0x0E00"
```

`custom_elements` also requires `--allow-extensions`.

## 11. Strict Mode

```bash
python3 tools/arxml_generator/generate_arxml.py \
  --input your.yaml \
  --validate-only \
  --strict \
  --print-summary
```

- With `--strict`, unknown keys cause an error exit.
- Without `--strict`, unknown keys are shown as warnings in the summary.
- Without `--allow-extensions`, `zerocopy` / `custom_elements` are rejected.

To validate YAML with extension sections:

```bash
python3 tools/arxml_generator/generate_arxml.py \
  --input your_with_extensions.yaml \
  --validate-only \
  --strict \
  --allow-extensions \
  --print-summary
```

## 12. Merge Behavior (multiple `--input` arguments)

- `variables`: later inputs overwrite earlier ones
- `autosar.schema_namespace` / `schema_location`: later inputs overwrite
- `autosar.packages`: concatenated in input order

## 13. Ready-to-Use Examples

- Basic SOME/IP:
  - `tools/arxml_generator/examples/pubsub_vsomeip.yaml`
- SOME/IP + DDS + ZeroCopy extensions:
  - `tools/arxml_generator/examples/pubsub_multi_transport.yaml`
- Advanced multi-package:
  - `tools/arxml_generator/examples/multi_service_gateway.yaml`
