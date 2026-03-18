# Adaptive-AUTOSAR
![CI](https://github.com/tatsuyai713/Adaptive-AUTOSAR/actions/workflows/cmake.yml/badge.svg)

Linux-oriented educational implementation of Adaptive AUTOSAR style APIs.

Language:
- English: `README.md`
- Japanese: `README.ja.md`

## Scope and Version
- ARXML schema baseline in this repository: `http://autosar.org/schema/r4.0` (`autosar_00050.xsd`).
- This repository is an educational implementation of Adaptive AUTOSAR style APIs and behavior subsets.
- This repository does not claim official AUTOSAR conformance certification.

## Quick Start (Build -> Install -> User App Build/Run)
The following commands were validated in Docker on **2026-02-15**.

### Prerequisites
- C++14 compiler
- CMake >= 3.14
- Python3 + `PyYAML`
- OpenSSL development package (`libcrypto`)
- Installed middleware (default paths used by this repo):
  - vSomeIP: `/opt/vsomeip`
  - iceoryx: `/opt/iceoryx`
  - Cyclone DDS (+ idlc): `/opt/cyclonedds`

### 0) Install dependencies and middleware (Linux / Raspberry Pi)
Use the bundled scripts in this repository:

```bash
sudo ./scripts/install_dependency.sh
sudo ./scripts/install_middleware_stack.sh
```

Or run everything from one command:

```bash
sudo ./scripts/install_middleware_stack.sh --install-base-deps
```

The middleware stack installs the following in order:

| Library | Default prefix | Notes |
|---|---|---|
| iceoryx v2.0.6 | `/opt/iceoryx` | ACL-tolerant patch applied for container environments |
| CycloneDDS 0.10.5 | `/opt/cyclonedds` | SHM auto-enabled via iceoryx; generates `cyclonedds-lwrcl.xml` |
| vsomeip 3.4.10 + CDR | `/opt/vsomeip` | CDR lib (`liblwrcl_cdr.a`) extracted from CycloneDDS-CXX; no DDS runtime needed at runtime |

Individual script options (examples):

```bash
# CycloneDDS with SHM explicitly enabled (iceoryx installed automatically if absent)
sudo ./scripts/install_cyclonedds.sh --enable-shm

# vsomeip with CDR only (no full CycloneDDS runtime required at runtime)
sudo ./scripts/install_vsomeip.sh --prefix /opt/vsomeip

# Skip middleware auto-installs (all already present)
sudo ./scripts/install_middleware_stack.sh --skip-system-deps
```

### QNX 8.0 cross-build — middleware install (scripts/)

For QNX SDP 8.0, the `scripts/` folder contains standalone middleware install scripts
that mirror the Linux scripts but cross-compile for QNX using `qcc`/`q++`.

**Prerequisites**: QNX SDP 8.0 installed and `qnxsdp-env.sh` sourceable.

```bash
# Source QNX environment (adjust path as needed)
source ~/qnx800/qnxsdp-env.sh

# Install all middleware for QNX aarch64le in one command
sudo ./scripts/install_middleware_stack_qnx.sh --arch aarch64le --enable-shm

# Or individually
sudo ./scripts/install_iceoryx_qnx.sh install   --arch aarch64le
sudo ./scripts/install_cyclonedds_qnx.sh install --arch aarch64le --enable-shm
sudo ./scripts/install_vsomeip_qnx.sh install   --arch aarch64le --boost-prefix /opt/qnx/third_party
```

QNX middleware install locations (defaults):

| Library | Default prefix |
|---|---|
| iceoryx | `/opt/qnx/iceoryx` |
| CycloneDDS | `/opt/qnx/cyclonedds` |
| vsomeip + CDR | `/opt/qnx/vsomeip` |
| Boost (for vsomeip) | `/opt/qnx/third_party` |

The `install_middleware_stack_qnx.sh` script also builds Boost automatically
when vsomeip is enabled and Boost is not yet present.

For the full QNX AUTOSAR AP runtime cross-build flow, see:
- `qnx/README.md`
- `qnx/env/qnx800.env.example`

### 1) Build and install AUTOSAR AP runtime libraries
Use `/tmp` first (no root privilege required):

```bash
./scripts/build_and_install_autosar_ap.sh \
  --prefix /tmp/autosar_ap \
  --build-dir build-install-autosar-ap
```

If you want the production-like layout:

```bash
sudo ./scripts/build_and_install_autosar_ap.sh --prefix /opt/autosar_ap
```

If middleware is not installed yet, build script can install it first:

```bash
sudo ./scripts/build_and_install_autosar_ap.sh \
  --prefix /opt/autosar_ap \
  --install-middleware \
  --install-base-deps
```

`build_and_install_autosar_ap.sh` builds the platform runtime binary
(`adaptive_autosar`) by default. Use `--without-platform-app` only when you
intentionally want a library-only install.

#### AUTOSAR AP release profile (default: `R24-11`)

This repository tracks the `R24-11` release profile as the default baseline
across `ara::*` modules.

When configuring directly with CMake, you can pin the target AP release profile:

```bash
cmake -S . -B build \
  -DAUTOSAR_AP_RELEASE_PROFILE=R24-11
```

The selected profile is exported via compile definitions of installed
`AdaptiveAutosarAP::*` targets and can be queried in:
- `ara/core/ap_release_info.h` (`ara::core::ApReleaseInfo`) for all modules
- `ara/com/ap_release_info.h` (`ara::com::ApReleaseInfo`) for `ara::com`

Backward-compatible alias:
- `-DARA_COM_AUTOSAR_AP_RELEASE=...` is still accepted and mapped to
  `AUTOSAR_AP_RELEASE_PROFILE`.

### 2) Build user apps against installed runtime only

```bash
./scripts/build_user_apps_from_opt.sh \
  --prefix /tmp/autosar_ap \
  --source-dir /tmp/autosar_ap/user_apps \
  --build-dir build-user-apps-opt
```

### 3) Run default smoke apps via script

```bash
./scripts/build_user_apps_from_opt.sh \
  --prefix /tmp/autosar_ap \
  --source-dir /tmp/autosar_ap/user_apps \
  --build-dir build-user-apps-opt-run \
  --run
```

This executes:
- `autosar_user_minimal_runtime`
- `autosar_user_per_phm_demo`

### 4) Run additional app templates manually (optional)
Example:

```bash
./build-user-apps-opt/src/apps/feature/runtime/autosar_user_tpl_runtime_lifecycle
./build-user-apps-opt/src/apps/feature/can/autosar_user_tpl_can_socketcan_receiver --can-backend=mock
```

### 5) Build only switchable DDS/iceoryx/vSomeIP Pub/Sub user app (independent)

This user app is built independently from the main runtime build and verifies the full generation chain during build:

- app source scan -> topic mapping YAML + manifest YAML (`autosar-generate-comm-manifest`)
- manifest YAML -> ARXML (`tools/arxml_generator/generate_arxml.py`)
- mapping YAML -> proxy/skeleton header (`autosar-generate-proxy-skeleton`)
- ARXML -> binding constants header (`tools/arxml_generator/generate_ara_com_binding.py`)
- app location: `user_apps/src/apps/communication/switchable_pubsub`

```bash
./scripts/build_switchable_pubsub_sample.sh
```

Run smoke checks for DDS, iceoryx, and vSomeIP profiles in one command:

```bash
./scripts/build_switchable_pubsub_sample.sh --run-smoke
```

Runtime transport selection for this user-app path is profile-based.
Set `ARA_COM_BINDING_MANIFEST` to one of the generated profile manifests.
`ARA_COM_EVENT_BINDING` can optionally override the profile at runtime
(`dds|iceoryx|vsomeip|auto`).

Manual profile switch with same binaries:

```bash
# CycloneDDS profile
unset ARA_COM_EVENT_BINDING
export ARA_COM_BINDING_MANIFEST=$PWD/build-switchable-pubsub-sample/generated/switchable_manifest_dds.yaml
./build-switchable-pubsub-sample/autosar_switchable_pubsub_sub &
./build-switchable-pubsub-sample/autosar_switchable_pubsub_pub

# iceoryx profile
unset ARA_COM_EVENT_BINDING
export ARA_COM_BINDING_MANIFEST=$PWD/build-switchable-pubsub-sample/generated/switchable_manifest_iceoryx.yaml
iox-roudi &
./build-switchable-pubsub-sample/autosar_switchable_pubsub_sub &
./build-switchable-pubsub-sample/autosar_switchable_pubsub_pub

# vSomeIP profile
unset ARA_COM_EVENT_BINDING
export ARA_COM_BINDING_MANIFEST=$PWD/build-switchable-pubsub-sample/generated/switchable_manifest_vsomeip.yaml
export VSOMEIP_CONFIGURATION=/opt/autosar_ap/configuration/vsomeip-rpi.json
/opt/autosar_ap/bin/autosar_vsomeip_routing_manager &
./build-switchable-pubsub-sample/autosar_switchable_pubsub_sub &
./build-switchable-pubsub-sample/autosar_switchable_pubsub_pub
```

### Raspberry Pi ECU profile (Linux + systemd)
The repository includes a deployment profile to run a Raspberry Pi machine as a
prototype ECU.

```bash
sudo ./scripts/build_and_install_rpi_ecu_profile.sh \
  --prefix /opt/autosar_ap \
  --runtime-build-dir build-rpi-autosar-ap \
  --user-app-build-dir /opt/autosar_ap/user_apps_build \
  --install-middleware

sudo ./scripts/setup_socketcan_interface.sh --ifname can0 --bitrate 500000
sudo ./scripts/install_rpi_ecu_services.sh --prefix /opt/autosar_ap --user-app-build-dir /opt/autosar_ap/user_apps_build --enable

./scripts/verify_rpi_ecu_profile.sh \
  --prefix /opt/autosar_ap \
  --user-app-build-dir /opt/autosar_ap/user_apps_build \
  --can-backend mock \
  --require-platform-binary
```

Detailed runbook:
- `deployment/rpi_ecu/README.md`
- `deployment/rpi_ecu/RASPI_SETUP_MANUAL.ja.md` (Japanese comprehensive setup manual with gPTP detail)

User-app bringup entry points:
- `/etc/autosar/bringup.sh` (edit this to launch your own applications)
- `autosar-vsomeip-routing.service` (keeps SOME/IP routing manager resident)
- `autosar-time-sync.service` (resident time synchronization support daemon)
- `autosar-persistency-guard.service` (resident persistency sync guard daemon)
- `autosar-iam-policy.service` (resident IAM policy loader daemon)
- `autosar-can-manager.service` (resident SocketCAN interface manager daemon)
- `autosar-platform-app.service` (starts platform-side built-in process stack)
- `autosar-exec-manager.service` (runs `bringup.sh` after platform service)
- `autosar-user-app-monitor.service` (tracks registered user apps, heartbeat freshness, and `ara::phm::HealthChannel` status; triggers restart recovery with startup-grace/backoff/deactivated-stop controls)
- `autosar-watchdog.service` (resident watchdog supervisor daemon)
- `autosar-sm-state.service` (resident SM machine state / network mode management daemon)
- `autosar-ntp-time-provider.service` (resident NTP time synchronization provider daemon — chrony/ntpd auto-detect)
- `autosar-ptp-time-provider.service` (resident PTP/gPTP time synchronization provider daemon — ptp4l PHC integration)
- runtime registry file: `/run/autosar/user_apps_registry.csv`
- runtime monitor status file: `/run/autosar/user_app_monitor.status`
- runtime PHM health files: `/run/autosar/phm/health/*.status`

### Porting Vector/ETAS/EB-oriented app assets
This repository can be used to rebuild vendor-developed C++ app assets at source
level and validate communication with other units.

Porting tutorial:
- `user_apps/tutorials/10_vendor_autosar_asset_porting.ja.md`

## Implemented Feature Matrix (Against AUTOSAR AP Scope)
Release baseline for this matrix: `R24-11`.

Status meanings:
- `Implemented (subset)`: Implemented in this repo, but not full AUTOSAR feature-complete.
- `Not implemented`: Not provided in this repo currently.
- **Coverage %** is an approximate ratio of implemented standard APIs vs. the full AUTOSAR AP SWS specification for each functional cluster. It is calculated based on the number of major standard API classes/methods implemented, and is provided as a rough guide only.

Conformance policy in this repository:
- APIs corresponding to AUTOSAR AP standard interfaces are kept in AUTOSAR-style signatures.
- Platform-side and user-app sample code are implemented to use standard namespaces first (`ara::<domain>::*`), and avoid extension aliases by default.
- Non-standard additions are provided as extensions (primarily under `ara::<domain>::extension`) and are not used to replace standard API signatures.
- Extension entry headers:
  - `src/ara/com/extension/non_standard.h`
  - `src/ara/diag/extension/non_standard.h`
  - `src/ara/exec/extension/non_standard.h`
  - `src/ara/phm/extension/non_standard.h`

| AUTOSAR AP area | Coverage | Status | Available in this repo | Missing / Notes |
| --- | :---: | --- | --- | --- |
| `ara::core` | 100 % | Fully implemented | `Result`, `Optional` (`nullopt`, `make_optional`), `Future/Promise`, `ErrorCode/ErrorDomain` (virtual `ThrowAsException`, `SupportDataType`), `InstanceSpecifier`, runtime init/deinit (`InitOptions`, `SetAbortHandler`), initialization-state query API, `String`/`StringView` (full C++14 polyfill: `find`/`rfind`/`substr`/`compare`/`remove_prefix`/`remove_suffix`/`starts_with`/`ends_with`/`at`/`front`/`back`/`copy`/`find_first_of`/`find_last_of`/`find_first_not_of`/`find_last_not_of`/ordering operators), `Vector`, `Map`/`UnorderedMap`, `Array`, `Span`, `SteadyClock`, `Byte` (SWS_CORE_04400), `Variant` (C++14-compatible type-safe union, `HoldsAlternative`/`Get`/`GetIf`/`Visit`/`Emplace`/`Swap`), `CoreErrorDomain` (SWS_CORE_10400, domain ID 0x8000000000000014), exception hierarchy (`Exception`/`CoreException`/`InvalidArgumentException`/`InvalidMetaModelShortnameException`/`InvalidMetaModelPathException`), `Result::AndThen`/`OrElse`/`Map`/`MapError` (monadic chaining for both `Result<T,E>` and `Result<void,E>`), `ScaleLinearAndRound` (SWS_CORE_04110), `AbortHandler` (OS-level signal handler/abort handler with callback registration, SIGABRT/SIGSEGV/SIGFPE/SIGILL/SIGTERM/SIGINT/SIGBUS) | — |
| `ara::log` | 100 % | Fully implemented | `Logger` (all severity levels + `WithLevel` log-level filtering), `LogStream` (bool/int8/uint8/int16/uint16/int32/uint32/int64/uint64/float/double/string/char*/ErrorCode/InstanceSpecifier/vector/`Span<const Byte>`/`StringView` operators), `LoggingFramework`, `CreateLogger`/`SetDefaultLogLevel` free functions, `InitLogging`/`RegisterAppDescription` (SWS_LOG_00100 application-level log initialization), `LogRaw` (raw byte logging), `Arg<T>` template factory (SWS_LOG_00030), 3 sink types (console/file/network), runtime log-level override/query, DLT backend (`LogMode::kDlt`) via UDP/dlt-viewer integration (DltLogSink), `LogHex(value)`/`LogHex(ptr, len)` (hex-dump formatter), `LogBin(value)` (binary formatter), `AsyncLogSink` (non-blocking ring-buffer sink with lossy drop policy, background flush thread, drop/pending count query), `FormatterPlugin` (pluggable log formatter interface, `DefaultFormatter`, `JsonFormatter`, `FormatterRegistry` singleton) | — |
| `ara::com` common API | 100 % | Fully implemented | `ProxyEvent`/`ProxyMethod`/`ProxyField` proxy-side wrappers, `SkeletonEvent`/`SkeletonMethod`/`SkeletonField` skeleton-side wrappers (SWS_CM_00404: `SkeletonMethod<R(Args...)>` typed handler registration via `SetHandler`/`UnsetHandler`, multi-arg sequential deserialization via `Serializer<T>::DeserializeAt`), `SkeletonField::RegisterGetHandler` (SWS_CM_00112) and `RegisterSetHandler` (SWS_CM_00113) with optional get/set `SkeletonMethodBinding`, `ServiceProxyBase`/`ServiceSkeletonBase`, Subscribe/Unsubscribe, receive/state handlers, `SamplePtr`, serialization framework (`Serializer<T>` for trivially-copyable, CDR, `std::string`, `std::vector<T>`, `std::map<K,V>`, raw `vector<uint8_t>`, guarded CycloneDDS CDR include), concurrent `StartFindService`/handle-aware `StopFindService`, `InstanceSpecifier` overloads for `FindService`/`StartFindService`, capability-aware Field behavior, `BindingFactory::CreateProxyMethodBinding`/`CreateSkeletonMethodBinding` for DDS, iceoryx, **and vsomeip** transports, `ProxyFireAndForgetMethod`/`SkeletonFireAndForgetMethod` (SWS_CM_00196: fire-and-forget typed method wrappers, no response path), `CommunicationGroupServer<T,R>`/`CommunicationGroupClient<T,R>` (broadcast/unicast messaging with `AddClient`/`RemoveClient`, `Broadcast`/`Message`/`ListClients`/`Response` fully implemented), `SkeletonEventBinding::SetInitialValue` (field initial-value delivery across all transports), `ServiceSkeletonBase::ProcessNextMethodCall` (SWS_CM_00199: kPoll dispatch queue with `EnqueuePollCall`/`HasPendingMethodCalls`), `kEventSingleThread` serialization (dispatch mutex via `GetDispatchMutex()` wired through `SkeletonMethod`/`SkeletonFireAndForgetMethod`), `SubscriptionStateChangeHandler` lifecycle notifications across all transports, `ProxyTrigger`/`SkeletonTrigger` (SWS_CM_00500: trigger-based one-way notification — subscribe/fire/multi-subscriber), `RawDataStreamServer`/`RawDataStreamClient` (SWS_CM_00600: byte-level streaming with backpressure/suspend flow control, `StreamSessionId`-based session management — `AcceptSession`/`CloseSession`/`IsSessionActive`/`GetActiveSessionCount` on server, auto-session on client `Connect`/`Disconnect`), `InstanceIdentifier`/`InstanceIdentifierContainer` (SWS_CM_00151/SWS_CM_00152: transport-level instance ID — string/numeric constructors, hash, Resolve from `InstanceSpecifier`, container alias), `ServiceVersion`/`VersionCheckPolicy` (SWS_CM_00150: version compatibility with kExact/kMajorOnly/kMinorBackward/kNoCheck, `IsCompatibleWith`), `NetworkBindingBase`/`LocalIpcBinding` (SWS_CM_00610: abstract binding interface with connection state management), `EventQosProfile`/`MethodQosProfile` (SWS_CM_00920: QoS configuration — reliability/history/durability/deadline/priority), `ProcessNextMethodCall(timeout)` timeout overload (SWS_CM_00200), `GetServiceVersion()` on skeleton, `CheckServiceVersion()` on proxy, `ara::com::runtime::Initialize`/`Deinitialize`/`IsInitialized`/`GetApplicationName` (SWS_CM_00001/SWS_CM_00002: CM runtime lifecycle), `EventCacheUpdatePolicy` (SWS_CM_00701: kLastN/kNewestN), `FilterConfig` (SWS_CM_00702: value/rate-based event filtering — None/Threshold/Range/OneEveryN), `SizedEventReceiveHandler` (SWS_CM_00318: receive handler with sample count), `ProxyEvent::Reinit` (SWS_CM_00340: rebind after service loss), `ProxyEvent::Subscribe` QoS overload (SWS_CM_00920), `SkeletonEvent::SetSubscriptionStateChangeHandler` (SWS_CM_00350: per-client subscribe/unsubscribe notification), `ServiceHandleType::GetServiceInterfaceId` (SWS_CM_00312), `FindService(InstanceIdentifier)` overload (SWS_CM_00124), `ComErrc::kAlreadyOffered`/`kMaxServicesExceeded`/`kWrongMethodCallProcessingMode`/`kCouldNotExecute`/`kErroneousFileHandle` (SWS_CM_11350-11354), `OfferService` already-offered error propagation, `Transformer`/`IdentityTransformer`/`TransformerChain`/`TransformerFactory`/`DefaultTransformerFactory` (SWS_CM_00710-00714: pluggable serialization/protection chain), `E2EErrorDomain`/`E2EErrc`/`MakeErrorCode` (SWS_E2E_00401: E2E protection error domain), `SampleInfo`/`E2ESampleStatus` (SWS_CM_00321: per-sample metadata with timestamp/source/E2E status/sequence number) | — |
| `ara::com` SOME/IP | 100 % | Fully implemented | SOME/IP SD client/server, pub/sub, RPC client/server over vSomeIP backend, message-type coverage for `RequestNoReturn` and SOME/IP-TP (`TpRequest`/`TpResponse`/`TpError`) at common header layer, TP header serialize/deserialize (offset + more-segments flag), `SegmentPayload` segmenter + `TpReassembler` reassembly (with configurable timeout via `cDefaultTpTimeout`, `IsTimedOut()` guard, `SegmentCount()` query, `Reset()` cleanup), `BindingFactory` vsomeip method binding support (`CreateProxyMethodBinding`/`CreateSkeletonMethodBinding` for `kVsomeip`), `SubscriptionStateChangeHandler` invocation (pending→subscribed→unsubscribed transitions, async ACK via `register_subscription_status_handler`), configurable `instanceId` for `SocketRpcClient`/`SocketRpcServer`, field initial-value delivery (`SetInitialValue` + `notify(force=true)`), `ProcessNextMethodCall` kPoll-mode dispatch queue, `Ipv6Address` (128-bit address with `::` abbreviation parsing, `Inject`/`Extract`/`ToString`, equality operators), `Ipv6EndpointOption` (SD IPv6 unicast/multicast/SD endpoint option with `Deserialize`, multicast prefix 0xFF validation), `SdNetworkConfig` (configurable SD multicast address/port/TTL/maxEntriesPerMessage/reboot detection, `ValidateSdNetworkConfig()`), `ConfigurationOption` (DNS-SD Configuration Option OptionType 0x01, key=value string serialization/deserialization per PRS_SOMEIPSD_00550), Magic Cookie support (`GenerateClientMagicCookie`/`GenerateServerMagicCookie`, `IsMagicCookie`/`FindMagicCookie` for TCP stream resynchronization per PRS_SOMEIP_00075), `TpReassemblyManager` (multi-stream concurrent TP reassembly keyed by {MessageId, ClientId}, thread-safe with `FeedSegment`/`CleanupTimedOut`/`ActiveStreamCount`), `SessionHandler` (session-ID handling policy kActive/kInactive per PRS_SOMEIP_00030-00035, atomic next-session with wrap-around 0xFFFF→1, reboot flag), fire-and-forget RPC support (`RequestNoReturn` acceptance in `RpcServer::validate()`) | — |
| `ara::com` DDS | 100 % | Fully implemented | DDS pub/sub via Cyclone DDS wrappers (`ara::com::dds`), `DdsQosProfile` (kReliable/kBestEffort/kTransientLocal) for publisher and subscriber, `DdsProxyEventBinding`/`DdsSkeletonEventBinding` (raw-bytes CycloneDDS C-API topic with manually-crafted `dds_topic_descriptor_t`, WaitSet+polling background thread, `BindingFactory::kCycloneDds`), `DdsProxyMethodBinding`/`DdsSkeletonMethodBinding` (two DDS topics per method — request `ara_com_req_*`/reply `ara_com_rep_*` — atomic session-ID correlation, proxy background poll thread, skeleton lazy-init service thread), `SubscriptionStateChangeHandler` invocation (kSubscriptionPending→kSubscribed→kNotSubscribed transitions), `SetInitialValue` (initial-value delivery on Offer via DDS history), immediate service discovery for DDS data-centric model, `DdsQosConfig` (`DdsQosProfile` struct with `DdsReliability`/`DdsHistoryKind`/`DdsLivelinessKind` enums, configurable `Deadline`/`DomainId`, `DefaultReliableQos()`/`BestEffortQos()` factory functions), per-binding `DdsDomainId` in `EventBindingConfig`/`MethodBindingConfig`, `DdsParticipantFactory` (thread-safe singleton shared DDS participant per domain with reference counting, `AcquireParticipant`/`ReleaseParticipant`), `DdsStatusCondition` (14 status kinds — `kInconsistentTopic` through `kSubscriptionMatched` — with compound flag operators, `DdsStatusListener` aggregate with 9 callback slots, `DdsSampleRejectedStatus`/`DdsLivelinessChangedStatus`/etc. structs), `DdsContentFilter` (SQL/Custom filter expressions with parameter validation, `DdsOwnershipKind` kShared/kExclusive), `DdsExtendedQos` (Ownership, OwnershipStrength, `DdsPartitionConfig`, TransportPriority, TimeBasedFilter, LatencyBudget, ResourceLimits MaxSamples/MaxInstances/MaxSamplesPerInstance) | — |
| `ara::com` zero-copy | 100 % | Fully implemented | Zero-copy pub/sub via iceoryx wrappers (`ara::com::zerocopy`), `IceoryxProxyEventBinding`/`IceoryxSkeletonEventBinding` (background `WaitForData` polling thread drains `ZeroCopySubscriber` samples, zero-copy `Loan`/`Publish` path for `SendAllocated`, outstanding loans tracked via `unordered_map<void*, LoanedSample>`, `BindingFactory::kIceoryx`), `IceoryxProxyMethodBinding`/`IceoryxSkeletonMethodBinding` (two iceoryx channels per method — request `req_XXXX`/reply `rep_XXXX` — session-ID framed payload `[4B session_id][4B is_error][bytes]`, proxy background poll thread, skeleton lazy-init service thread, configurable method-call timeout via `cDefaultMethodTimeout`/`PendingCall::Deadline`, timed-out pending call cleanup in poll loop), `SubscriptionStateChangeHandler` invocation (kSubscriptionPending→kSubscribed→kNotSubscribed transitions), `SetInitialValue` (initial-value delivery on Offer via shared memory), immediate service discovery for iceoryx shared-memory model, `QueueOverflowPolicy` (`kDropOldest`/`kRejectNew` enum for event sample queue backpressure, per-binding dropped-sample counter), `ZeroCopyTransportConfig` (configurable `SubscriberConfig` queue capacity/history request/runtime name, `PublisherConfig` history capacity/offer-on-create, `MaxChunkPayloadSize`, `PollTimeout`), `ZeroCopyServiceDiscovery` (CAPRO-style service discovery with `OfferService`/`StopOfferService`/`ServiceFound`/`ServiceLost`, `ServiceAvailabilityHandler` callback, thread-safe offered/found service registries), `PortIntrospection` (abstract interface for publisher/subscriber port info, `MemoryPoolInfo` with `FreeChunks()`/`UsageRatio()`, `SharedMemoryInfo` with `FreeBytes()`, `IntrospectionSnapshot` aggregate, `PortConnectionState` 5-state enum) | — |
| `ara::com` E2E | 100 % | Fully implemented | E2E Profile11 (`TryProtect`/`Check`/`TryForward`) and event decorators, E2E Profile 01 (CRC-8 SAE-J1850, configurable DataID), E2E Profile 02 (CRC-8H2F, higher Hamming distance), E2E Profile 03 (CRC-16/CCITT, 0x1021 MSB-first poly, 4-byte header, 4-bit counter 0-15, SWS_E2E_00104), E2E Profile 04 (CRC-32/AUTOSAR, 0xF4ACFB13 reflected, 6-byte header, strongest protection), E2E Profile 05 (CRC-16/ARC, 0x8005 reflected poly, up to 4096 bytes, LE 2-byte CRC header), E2E Profile 06 (CRC-32 standard, 0xEDB88320 reflected poly, 6-byte header, 8-bit counter 0-255, SWS_E2E_00105), E2E Profile 07 (CRC-64/ECMA-182, 0xC96C5795D7870F42 reflected poly, 16-byte header: CRC64(8)+Counter(2)+DataID(4)+Reserved(2), 16-bit counter 0x0000-0xFFFE), `E2ESkeletonMethodBindingDecorator`/`E2EProxyMethodBindingDecorator` (method-level E2E protection — intercepts request/response to apply CRC check + header strip), `E2EStateMachine` (SWS_E2E_00345 windowed state machine with kValid/kNoData/kInit/kInvalid/kRepeat, configurable window size and OK/Error thresholds), `E2EErrorDomain`/`E2EErrc` (SWS_E2E_00401: error domain with kRepeated/kWrongSequence/kError/kNotAvailable/kNoNewData/kStateMachineError), `E2ESampleStatus` per-sample E2E check result (SWS_CM_01001) | — |
| `ara::exec` | 100 % | Fully implemented | `ExecutionClient`/`ExecutionServer`, `StateServer`, `StateClient` (function-group state set/transition/error query via SOME/IP RPC, `GetProcessState`, `SetStateWithTimeout`), `DeterministicClient` (`WaitForActivation`/`WaitForNextActivation` SWS_EM_02001, `kWait` activation return, `SetCpuAffinity` SWS_EM_02040, `SetCycleTime` SWS_EM_02045), `FunctionGroup`/`FunctionGroupState`, `SignalHandler`, `WorkerThread`, enhanced `ProcessWatchdog` (startup grace / continuous expiry / callback cooldown / expiry count), execution-state change callback, `ExecutionManager` (central EM orchestrator: process manifest registry, `RegisterProcess`/`UnregisterProcess`, `ActivateFunctionGroup`/`TerminateFunctionGroup`, fork/exec process lifecycle, background monitor thread, state sync from `ExecutionServer`, `ProcessStateChangeHandler` callback), `StartupConfig`/`ProcessToMachineStateMapping`/`SchedulingPolicy` (SWS_EM_02050-02052), `GetProcessName`/`GetFunctionGroup` accessors, `ManifestLoader` (ARXML execution manifest loader), `ClusterMonitor` (cluster-level health monitor) | — |
| `ara::diag` | 100 % | Fully implemented | `Monitor` (with debouncing, FDC query, current status query), `Event` (with `GetEventId` accessor), `Conversation`, `DTCInformation`, `Condition`, `OperationCycle`, `GenericUDSService`, `SecurityAccess`, `GenericRoutine`, `DataTransfer` (Upload/Download), routing framework, `DiagnosticSessionManager` (S3 timer, UDS session lifecycle), `OBD-II Service` (Mode 01/09, 12 PIDs), `DataIdentifierService` (UDS 0x22/0x2E, multi-DID read, per-DID callbacks), `CommunicationControl` (UDS 0x28, Tx/Rx enable/disable per comm type), `ControlDtcSetting` (UDS 0x85, on/off DTC update, suppressPosRspBit), `ClearDiagnosticInformation` (UDS 0x14, group-of-DTC clear, 0xFFFFFF=clearAll), `EcuResetRequest` (UDS 0x11, soft/hard/keyOffOn reset), `InputOutputControl` (UDS 0x2F, ReturnToEcu/ResetToDefault/FreezeCurrentState/ShortTermAdjustment, status read-back), `EventMemory` (SWS_Diag_00500 snapshot records, extended data records, aging counter, displacement policy, overflow detection), `DiagnosticManager` (central UDS orchestrator with P2/P2* timing) | — |
| `ara::phm` | 100 % | Fully implemented | `SupervisedEntity` (`Enable`/`Disable` lifecycle control, `GetSupervisionStatus` SWS_PHM_01142, `GetLocalSupervisionStatus`, `Create` factory), `HealthChannel` (with `Offer`/`StopOffer` lifecycle, `SetHealthStatusCallback`, `GetHealthChannelExternalStatus`), `RecoveryAction`, `FunctionGroupStateRecoveryAction`, `PlatformResetRecoveryAction`, `CheckpointCommunicator`, supervision helpers, `AliveSupervision` (periodic checkpoint monitoring, min/max margin, failed/passed threshold, kOk/kFailed/kExpired states), `DeadlineSupervision` (min/max deadline window, failed/passed threshold, kDeactivated/kOk/kFailed/kExpired states), `LogicalSupervision` (checkpoint ordering graph, adjacency-validated transitions, failed threshold, allowReset), `GlobalSupervisionStatus`/`LocalSupervisionStatus`/`HealthChannelExternalStatus`/`RecoveryActionType` enums (SWS_PHM_00110-00370), `PhmOrchestrator` (PHM orchestration daemon) | — |
| `ara::per` | 100 % | Fully implemented | `KeyValueStorage` (`GetCurrentStorageSize`, `GetStorageQuota`), `FileStorage` (`OpenFileWriteOnly`, `RecoverFile`/`RecoverAllFiles`, `GetCurrentFileStorageSize`, `GetFileInfo` SWS_PER_00420, `GetCurrentFileStorageQuota`, `EstimatedFreeSpace` SWS_PER_00450), `ReadAccessor` (with `Seek`/`GetCurrentPosition`), `ReadWriteAccessor` (with `Seek`/`GetCurrentPosition`, `Peek`), `SharedHandle`/`UniqueHandle` wrappers, `RecoverKeyValueStorage`/`ResetKeyValueStorage`, `RecoverFileStorage`/`ResetFileStorage`, `ResetPersistency` (global factory reset), `UpdatePersistency` (UCM schema-migration hook, SWS_PER_00456, schema metadata update + backup snapshot generation), batch operations (`SetValues`/`GetValues`/`SetStringValues`/`GetStringValues`/`RemoveKeys`), per-key and global change observer callbacks, `FileInfo` struct, `GetCurrentPersistencyVersion` (SWS_PER_00452), `RegisterDataTypeFaultHandler`, `RedundantStorage` (dual-partition redundant KV storage with failover) | — |
| `ara::sm` | 100 % | Fully implemented | `TriggerIn`/`TriggerOut`/`TriggerInOut`, `Notifier`, `SmErrorDomain`, `MachineStateClient` (lifecycle state management, `RequestShutdown`/`RequestRestart`, `RequestGracefulShutdown` SWS_SM_00851), `NetworkHandle` (communication mode control, `GetNetworkState`), `StateTransitionHandler` (function-group transition callbacks), `DiagnosticStateHandler` (SM/Diag session bridge: per-session entry/exit callbacks, S3-timeout coupling, programming session state isolation), `FunctionGroupStateMachine` (state graph with guard conditions, timeout-based auto-transitions via `Tick()`, transition history ring buffer), `PowerModeManager` (SWS_SM_00101 power mode management with callback), `MachineStateResult` enum (SWS_SM_00100), `RecoveryHandler` (RecoveryAction enum, recovery callback), `UpdateRequestHandler` (update lifecycle: PrepareUpdate/VerifyUpdate/RollbackUpdate, state tracking), resident SM state daemon, `NetworkInterfaceController` (per-interface OS network control) | — |
| ARXML tooling | — | Implemented (subset) | YAML -> ARXML, ARXML -> ara::com binding header generator | Supports repository scope and extensions, not full ARXML universe |
| `ara::crypto` | 100 % | Fully implemented | Error domain, `ComputeDigest` (SHA-1/256/384/512), `ComputeHmac`, `AesEncrypt`/`AesDecrypt` (AES-CBC, PKCS#7), `AesGcmEncrypt`/`AesGcmDecrypt` (AES-128/256-GCM authenticated encryption with AAD, 16-byte auth tag), `AesCtrEncrypt`/`AesCtrDecrypt` (AES-128/256-CTR stream cipher, no padding), `ChaCha20Encrypt`/`ChaCha20Decrypt` (ChaCha20 IETF, RFC 8439, 32-byte key, 96-bit nonce), `DeriveKeyPbkdf2` (PBKDF2-HMAC, SHA-1/256/384/512, RFC 6070), `DeriveKeyHkdf` (HKDF extract-and-expand, RFC 5869), `GenerateSymmetricKey`, `GenerateRandomBytes`, `KeySlot`/`KeyStorageProvider` (slot-based key management with filesystem persistence), `GenerateRsaKeyPair`/`RsaSign`/`RsaVerify`/`RsaEncrypt`/`RsaDecrypt` (RSA 2048/4096, PKCS#1 v1.5 sign, OAEP encrypt), `GenerateEcKeyPair`/`EcdsaSign`/`EcdsaVerify` (ECDSA P-256/P-384), `ParseX509Pem`/`ParseX509Der`/`VerifyX509Chain` (X.509 certificate parsing and chain verification), `CheckCertificateRevocation` (SWS_CRYPT_25100 CRL-based revocation check), `HashFunctionCtx` (SWS_CRYPT_21100 streaming EVP_MD_CTX), `MessageAuthenticationCodeCtx` (SWS_CRYPT_22100 streaming HMAC, `FinishTruncated` SWS_CRYPT_22250), `EcdhDeriveSecret`/`ExportPublicKeyPem`/`ImportPublicKeyPem` (SWS_CRYPT_24100 ECDH key agreement + key import/export), `SymmetricBlockCipherCtx` (SWS_CRYPT_23700 streaming AES-CBC via EVP_CIPHER_CTX), `SignerPrivateCtx` (SWS_CRYPT_23800 streaming RSA/ECDSA signing via EVP_DigestSign), `VerifierPublicCtx` (SWS_CRYPT_23900 streaming RSA/ECDSA verification via EVP_DigestVerify), `HsmProvider` (HSM integration abstraction) | — |
| `ara::nm` | 100 % | Fully implemented | `NetworkManager` (multi-channel NM state machine: BusSleep/PrepBusSleep/ReadySleep/NormalOperation/RepeatMessage, `HandleRemoteSleepIndication` SWS_NM_00002, `HandleRepeatMessageRequest` SWS_NM_00005, `IsClusterSleepReady` SWS_NM_00009), `NetworkRequest`/`NetworkRelease`, `NmMessageIndication`, `Tick`-based state transitions, channel status query, state-change callback, partial-networking flag, `NmCoordinator` (coordinated multi-channel bus sleep/wake, AllChannels/Majority policy, sleep-ready callback), `NmTransportAdapter` (abstract NM PDU transport interface), `NmPdu` (NM PDU serialization/deserialization, control-bit vector, partial-networking filter-mask support), `CanTpNmAdapter`/`FlexRayNmAdapter` (transport adapter drivers) | — |
| `ara::com::secoc` | 100 % | Fully implemented | `FreshnessManager` (per-PDU monotonic counter, replay protection, VerifyAndUpdate, `SaveToFile`/`LoadFromFile` NVM-like counter persistence, `SetOverflowWarningCallback` SWS_SecOC_00204, `IsNearOverflow`), `SecOcPdu` (configurable HMAC-based MAC, truncated freshness/MAC transmission, Protect/Verify), `SecOcKeyManager` (PDU-to-key-slot binding via `ara::crypto::KeyStorageProvider`, lazy load + cache, `RefreshKey` for key rotation), `SecOcErrorDomain`, `SecOcPduCollective` (SWS_SecOC_00203 multi-PDU batch protect/verify), `VerificationStatusIndication`/`VerificationResult` (SWS_SecOC_00200-201), `SecOcOverrideStatus` (SWS_SecOC_00202 bypass modes), `FreshnessSyncManager` (cross-ECU freshness counter synchronization) | — |
| `ara::iam` | 100 % | Fully implemented | In-memory IAM policy engine (subject/resource/action, wildcard rules), error domain, `Grant`/`GrantManager` (time-bounded permission tokens, issue/revoke/query/purge, file persistence), `PolicyVersionManager` (policy snapshot/rollback, versioned history), `RoleManager` (RBAC: role definitions with single-parent hierarchy, subject-role assignment, transitive `HasRole`/`GetRolesForSubject`, `IsAllowedViaRole` integration with `AccessControl`, `SaveToFile`/`LoadFromFile`), `AbacPolicyEngine` (attribute-based access control: subject/resource/action wildcard + per-attribute conditions with kEquals/kNotEquals/kContains/kExists/kNotExists ops, first-match semantics, audit callback, file persistence, `EvaluateWithTime` SWS_IAM_00310 TBAC schedule matching), `PolicySigner` (ECDSA P-256/SHA-256 policy signing and verification via `ara::crypto`), `PasswordStore` (SWS_IAM_00100 salted-hash credential management), `RequestContext` (SWS_IAM_00101 authentication context), `IdentityManager` (SWS_IAM_00102 PID-to-identity mapping, `AuthenticateProcess`), `AuditTrail` (timestamped audit record logging, query, clear), `CapabilityManager` (capability-based access control: grant/revoke/query per subject), `BiometricInterface` (biometric sensor HAL) | — |
| `ara::ucm` | 100 % | Fully implemented | UCM error domain, update-session manager (`Prepare`/`Stage`/`Verify`/`Activate`/`Rollback`/`Cancel`, `ConfirmRollback` SWS_UCM_00041, `ValidateChunkCrc` SWS_UCM_00151), Transfer API (`TransferStart`/`TransferData`/`TransferExit`), SHA-256 payload verification, state/progress callbacks, per-cluster active-version tracking with downgrade rejection, `CampaignManager` (multi-package update orchestration, per-package state tracking, automatic state recalculation, campaign rollback, `SignCampaignManifest`/`VerifyCampaignManifest` SWS_UCM_00045), `UpdateHistory` (persistent update log, per-cluster history query, file persistence), `DependencyChecker` (cluster dependency graph, version-range validation, `CheckDependencies`/`CheckAllDependencies`, file persistence), `SwClusterInfoType`/`SwClusterStateType` (SWS_UCM_00022-00023), `SwPackageInfoType`/`SwPackageActionType` (SWS_UCM_00024-00025), `PackageManagerStatusType` (SWS_UCM_00026), `TransferIdType` (SWS_UCM_00027), `SecureBootManager` (secure boot/TPM), `InstallerDaemon` (background installer) | — |
| `ara::tsync` | 100 % | Fully implemented | `TimeSyncClient` (reference update, synchronized time conversion, offset/state query, state-change notification callback, drift correction via linear-regression sliding window, `SyncQualityLevel` kGood/kDegraded/kLost, sync-loss timeout detection, `GetTimeBaseStatus`, `SetTimeLeapCallback`, `GetLeapSecondInfo`), `SynchronizedTimeBaseProvider` abstract interface, `PtpTimeBaseProvider` (ptp4l/gPTP PHC integration via `/dev/ptpN`), `NtpTimeBaseProvider` (chrony/ntpd auto-detect integration), `TimeSyncServer` (background provider polling, multi-consumer distribution, availability callback, provider-loss reset, `SetCorrectionCallback`), error domain, `UserTimeBaseProvider` (SWS_TimeSync_00101 user-supplied time source), `TimeBaseStatusType` (SWS_TimeSync_00100 sync status flags), `OffsetTimeBase` (SWS_TimeSync_00102), `LeapSecondInfo` struct (SWS_TimeSync_00103), resident PTP and NTP provider daemons, `RateCorrector` (PID-style clock rate correction) | — |
| Raspberry Pi ECU profile | — | Implemented (subset) | Build/install wrapper, SocketCAN setup script, systemd deployment templates, integrated readiness/transport verification script, resident daemons (`vsomeip-routing`, `time-sync`, `persistency-guard`, `iam-policy`, `can-manager`, `user-app-monitor`, `watchdog`, `sm-state`, `ntp-time-provider`, `ptp-time-provider`) | Prototype ECU operation on Linux is covered; production safety/security hardening remains system-level integration work |

### Coverage Notes

All 15 core `ara::*` modules have reached **100 %** SWS coverage. Hardware-dependent features (HSM, biometric sensors, CAN-TP/FlexRay buses, TPM secure boot, etc.) are provided as software abstractions with mock/simulation backends so that the full API surface is exercised and tested without requiring physical ECU hardware.

## User App Templates (Installed as `/opt|/tmp/autosar_ap/user_apps`)
- Basic:
  - `autosar_user_minimal_runtime`
  - `autosar_user_exec_signal_template`
  - `autosar_user_per_phm_demo`
- Communication:
  - `autosar_user_com_someip_provider_template`
  - `autosar_user_com_someip_consumer_template`
  - `autosar_user_com_zerocopy_pub_template`
  - `autosar_user_com_zerocopy_sub_template`
  - `autosar_user_com_dds_pub_template`
  - `autosar_user_com_dds_sub_template`
- Feature:
  - `autosar_user_tpl_runtime_lifecycle`
  - `autosar_user_tpl_can_socketcan_receiver`
  - `autosar_user_tpl_ecu_full_stack`
  - `autosar_user_tpl_ecu_someip_source`

See also:
- `user_apps/README.md`
- `user_apps/tutorials/README.ja.md`
- `deployment/rpi_ecu/README.md`
- `user_apps/tutorials/10_vendor_autosar_asset_porting.ja.md` (porting Vector/ETAS/EB-oriented app assets)
- `tools/host_tools/doip_diag_tester/README.ja.md` (Ubuntu-side DoIP/DIAG host tester for Raspberry Pi ECU)

## Host-Side Diagnostic Tool (Not ECU On-Target)
- Binary: `autosar_host_doip_diag_tester`
- Source: `tools/host_tools/doip_diag_tester/doip_diag_tester_app.cpp`
- Docs:
  - `tools/host_tools/doip_diag_tester/README.md`
  - `tools/host_tools/doip_diag_tester/README.ja.md`
- Build:
```bash
cmake -S tools/host_tools/doip_diag_tester -B build-host-doip-tester
cmake --build build-host-doip-tester -j"$(nproc)"
```

## ARXML and Code Generation
### YAML -> ARXML

```bash
python3 tools/arxml_generator/generate_arxml.py \
  --input tools/arxml_generator/examples/pubsub_vsomeip.yaml \
  --output /tmp/pubsub.generated.arxml \
  --overwrite \
  --print-summary
```

### ARXML -> ara::com binding header

```bash
python3 tools/arxml_generator/generate_ara_com_binding.py \
  --input /tmp/pubsub.generated.arxml \
  --output /tmp/vehicle_status_manifest_binding.h \
  --namespace sample::vehicle_status::generated \
  --provided-service-short-name VehicleStatusProviderInstance \
  --provided-event-group-short-name VehicleStatusEventGroup
```

Detailed manuals:
- `tools/arxml_generator/README.md`
- `tools/arxml_generator/YAML_MANUAL.ja.md`

## GitHub Actions Coverage
Workflow: `.github/workflows/cmake.yml`

Current CI verifies:
1. Middleware dependency build/install (vSomeIP, iceoryx, Cyclone DDS)
2. Full configure/build (`build_tests=ON`, all backends ON, samples OFF)
3. Unit tests (`ctest`) with required runtime library paths
4. ARXML generation and ARXML-based binding generation checks
5. Split install workflow (`build_and_install_autosar_ap.sh`)
6. External user app build and run against installed package (`build_user_apps_from_opt.sh --run`)
7. Doxygen API documentation generation (`./scripts/generate_doxygen_docs.sh`) and artifact upload (`doxygen-html`)
8. GitHub Pages publish for generated docs on push to `main`/`master`

## Documentation

Full documentation index: [`docs/README.md`](docs/README.md) | [`docs/README.ja.md`](docs/README.ja.md) (Japanese)

### By topic

| Topic | Document |
| --- | --- |
| Project overview (this file) | `README.md` / `README.ja.md` |
| **Deployment** | |
| Raspberry Pi ECU deployment | `deployment/rpi_ecu/README.md` |
| Raspberry Pi gPTP setup (Japanese) | `deployment/rpi_ecu/RASPI_SETUP_MANUAL.ja.md` |
| QNX 8.0 cross-compilation | `qnx/README.md` |
| QNX middleware install scripts | `scripts/install_middleware_stack_qnx.sh` |
| **User apps & tutorials** | |
| User app build system | `user_apps/README.md` |
| Tutorial index | `user_apps/tutorials/README.md` |
| Tutorial index (Japanese) | `user_apps/tutorials/README.ja.md` |
| **Tools** | |
| ARXML generator guide | `tools/arxml_generator/README.md` |
| ARXML YAML spec (Japanese) | `tools/arxml_generator/YAML_MANUAL.ja.md` |
| Host DoIP/UDS tester | `tools/host_tools/doip_diag_tester/README.md` |
| **API reference** | |
| Online (GitHub Pages) | <https://tatsuyai713.github.io/Adaptive-AUTOSAR/> |
| Local generation | `./scripts/generate_doxygen_docs.sh` |
| **Other** | |
| Project Wiki | <https://github.com/langroodi/Adaptive-AUTOSAR/wiki> |
| Contributing guide | `CONTRIBUTING.md` |

### Project layout

See [`docs/README.md`](docs/README.md#project-layout) for the annotated directory tree.
