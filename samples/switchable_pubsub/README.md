# Switchable Pub/Sub Sample (DDS <-> vSomeIP)

This sample demonstrates one Pub/Sub application pair that switches transport
at runtime by AUTOSAR-side manifest profile selection only.

- same binaries: `autosar_switchable_pubsub_pub`, `autosar_switchable_pubsub_sub`
- switch key: `ARA_COM_BINDING_MANIFEST=<dds-profile|vsomeip-profile>`
- app source uses generated Proxy/Skeleton API only (standard `ara::com` usage pattern)
- mapping source: auto-generated from `src/pubsub_usage_scan.cpp`

## Build (sample-only)

```bash
./scripts/build_switchable_pubsub_sample.sh
```

Generated artifacts are placed under:

- `build-switchable-pubsub-sample/generated/switchable_topic_mapping_dds.yaml`
- `build-switchable-pubsub-sample/generated/switchable_topic_mapping_vsomeip.yaml`
- `build-switchable-pubsub-sample/generated/switchable_manifest_dds.yaml`
- `build-switchable-pubsub-sample/generated/switchable_manifest_vsomeip.yaml`
- `build-switchable-pubsub-sample/generated/switchable_manifest_dds.arxml`
- `build-switchable-pubsub-sample/generated/switchable_manifest_vsomeip.arxml`
- `build-switchable-pubsub-sample/generated/switchable_proxy_skeleton.hpp`
- `build-switchable-pubsub-sample/generated/switchable_binding.hpp`

## Optional smoke test

```bash
./scripts/build_switchable_pubsub_sample.sh --run-smoke
```

This runs DDS profile first, then vSomeIP profile, and validates message
reception in both modes.

Transport selection in this sample is manifest-profile based.
Set `ARA_COM_BINDING_MANIFEST` to one of the generated profile manifests.
`ARA_COM_EVENT_BINDING` is not used in this sample path.

## Manual run

Set runtime libraries:

```bash
export LD_LIBRARY_PATH=/opt/autosar_ap/lib:/opt/cyclonedds/lib:/opt/vsomeip/lib:${LD_LIBRARY_PATH}
```

DDS profile:

```bash
unset ARA_COM_EVENT_BINDING
export ARA_COM_BINDING_MANIFEST=$PWD/build-switchable-pubsub-sample/generated/switchable_manifest_dds.yaml
./build-switchable-pubsub-sample/autosar_switchable_pubsub_sub &
./build-switchable-pubsub-sample/autosar_switchable_pubsub_pub
```

vSomeIP profile:

```bash
unset ARA_COM_EVENT_BINDING
export ARA_COM_BINDING_MANIFEST=$PWD/build-switchable-pubsub-sample/generated/switchable_manifest_vsomeip.yaml
export VSOMEIP_CONFIGURATION=/opt/autosar_ap/configuration/vsomeip-rpi.json
/opt/autosar_ap/bin/autosar_vsomeip_routing_manager &
./build-switchable-pubsub-sample/autosar_switchable_pubsub_sub &
./build-switchable-pubsub-sample/autosar_switchable_pubsub_pub
```
