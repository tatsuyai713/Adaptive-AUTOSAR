# Switchable Pub/Sub User App (DDS <-> iceoryx <-> vSomeIP)

This user app demonstrates one Pub/Sub application pair that switches transport
at runtime by AUTOSAR-side manifest profile selection only.

- same binaries: `autosar_switchable_pubsub_pub`, `autosar_switchable_pubsub_sub`
- switch key: `ARA_COM_BINDING_MANIFEST=<dds-profile|iceoryx-profile|vsomeip-profile>`
- app source uses generated Proxy/Skeleton API only (standard `ara::com` usage pattern)
- mapping source: auto-generated from `src/pubsub_usage_scan.cpp`

## Build (standalone user app)

Note: script name `build_switchable_pubsub_sample.sh` is kept for backward compatibility.

```bash
./scripts/build_switchable_pubsub_sample.sh
```

Generated artifacts are placed under:

- `build-switchable-pubsub-sample/generated/switchable_topic_mapping_dds.yaml`
- `build-switchable-pubsub-sample/generated/switchable_topic_mapping_vsomeip.yaml`
- `build-switchable-pubsub-sample/generated/switchable_topic_mapping_iceoryx.yaml`
- `build-switchable-pubsub-sample/generated/switchable_manifest_dds.yaml`
- `build-switchable-pubsub-sample/generated/switchable_manifest_vsomeip.yaml`
- `build-switchable-pubsub-sample/generated/switchable_manifest_iceoryx.yaml`
- `build-switchable-pubsub-sample/generated/switchable_manifest_dds.arxml`
- `build-switchable-pubsub-sample/generated/switchable_manifest_vsomeip.arxml`
- `build-switchable-pubsub-sample/generated/switchable_manifest_iceoryx.arxml`
- `build-switchable-pubsub-sample/generated/switchable_proxy_skeleton.hpp`
- `build-switchable-pubsub-sample/generated/switchable_binding.hpp`

## Optional smoke test

```bash
./scripts/build_switchable_pubsub_sample.sh --run-smoke
```

This runs DDS profile, then iceoryx profile, then vSomeIP profile, and validates
message reception in all modes.

Transport selection in this user-app path is manifest-profile based.
Set `ARA_COM_BINDING_MANIFEST` to one of the generated profile manifests.
`ARA_COM_EVENT_BINDING` may be used as a runtime override
(`dds|iceoryx|vsomeip|auto`) when needed.

## Manual run

Set runtime libraries:

```bash
export LD_LIBRARY_PATH=/opt/autosar_ap/lib:/opt/cyclonedds/lib:/opt/vsomeip/lib:/opt/iceoryx/lib:${LD_LIBRARY_PATH}
```

DDS profile:

```bash
unset ARA_COM_EVENT_BINDING
export ARA_COM_BINDING_MANIFEST=$PWD/build-switchable-pubsub-sample/generated/switchable_manifest_dds.yaml
./build-switchable-pubsub-sample/autosar_switchable_pubsub_sub &
./build-switchable-pubsub-sample/autosar_switchable_pubsub_pub
```

iceoryx profile:

```bash
unset ARA_COM_EVENT_BINDING
export ARA_COM_BINDING_MANIFEST=$PWD/build-switchable-pubsub-sample/generated/switchable_manifest_iceoryx.yaml
iox-roudi &
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
