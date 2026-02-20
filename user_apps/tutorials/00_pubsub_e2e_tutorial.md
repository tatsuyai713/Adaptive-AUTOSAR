# Adaptive AUTOSAR — Complete Pub/Sub Tutorial
## YAML Definition → ARXML Generation → C++ Codegen → Build → Run

> **Audience**: Developers new to Adaptive AUTOSAR who want to get Pub/Sub working quickly.
> **Estimated time**: 45–60 minutes.
> **Prerequisite**: AUTOSAR AP runtime installed at `/opt/autosar_ap`.

---

## Overall Flow

```
Your communication design
        │
        ▼
┌──────────────────────────────────┐
│  Step 1: Define service in YAML  │  ← describe the topology as text
└──────────────┬───────────────────┘
               │  python3 generate_arxml.py
               ▼
┌──────────────────────────────────┐
│  Step 2: Generate ARXML          │  ← AUTOSAR-standard XML manifest
└──────────────┬───────────────────┘
               │  python3 generate_ara_com_binding.py
               ▼
┌──────────────────────────────────┐
│  Step 3: Generate C++ binding    │  ← ServiceID / EventID constants
└──────────────┬───────────────────┘
               │  write or auto-generate
               ▼
┌──────────────────────────────────┐
│  Step 4: Write user application  │  ← Publisher / Subscriber code
└──────────────┬───────────────────┘
               │  cmake --build
               ▼
┌──────────────────────────────────┐
│  Step 5: Build & Run             │
└──────────────────────────────────┘
```

**Three ways to follow this tutorial:**

| Method | Best for | Workflow |
| ------ | -------- | -------- |
| **A: GUI tool** (recommended) | AUTOSAR beginners | Fill out a form, click Generate |
| **B: Template YAML** | CLI users | Edit the template YAML, run scripts |
| **C: Custom YAML** | Advanced users | Write YAML from scratch |

---

## Prerequisites

```bash
# Verify AUTOSAR AP is installed
ls /opt/autosar_ap/lib/

# Verify vSomeIP is installed
ls /opt/vsomeip/lib/

# Verify Python 3 and PyYAML
python3 -c "import yaml; print('PyYAML OK')"
```

If PyYAML is missing:
```bash
pip3 install pyyaml
```

---

## Method A: One-Click Generation with the GUI (Recommended)

### A-1. Launch the GUI

```bash
# Run from the repository root
python3 tools/arxml_generator/pubsub_designer_gui.py
```

### A-2. Design your service

Fill in the **Basic** tab:

| Field | Example | Notes |
| ----- | ------- | ----- |
| Service name | `VehicleSpeed` | Used as the C++ class name prefix |
| Service ID | `0x1234` | Click "Auto-generate ID" to derive from the name |
| Instance ID | `1` | Use 1 when running a single instance |
| Version | Major `1`, Minor `0` | |

Fill in the **Network** tab:

| Field | Example |
| ----- | ------- |
| Provider IP | `127.0.0.1` (loopback test) |
| Provider port | `30509` |
| Consumer IP | `127.0.0.1` |
| Consumer port | `30510` |
| Multicast IP | `239.255.0.1` |

Add an event in the **Events** tab (click **Add**):

| Field | Example | Notes |
| ----- | ------- | ----- |
| Event name | `SpeedEvent` | Becomes a C++ member name |
| Event ID | `0x8001` | Use 0x8000+ for events |
| Group ID | `1` | Groups related events |
| Payload type name | `SpeedData` | Name of the C++ struct |
| Payload fields | `std::uint16_t speedKph;` | C++ struct field definitions |

### A-3. Review the previews

Click **Refresh Preview** to see the generated output in the right panel:

- **YAML** — generated YAML definition
- **ARXML** — generated ARXML manifest
- **C++ types.h** — Skeleton and Proxy class definitions
- **C++ binding.h** — service/event ID constants
- **Provider App** — sample publisher source code
- **Consumer App** — sample subscriber source code
- **Build Guide** — ready-to-run cmake commands

### A-4. Generate all files

Set the output directory in the **Output** tab, then click **Generate All & Save**.

Files written to the output directory:
```
/tmp/pubsub_gen/
├── vehiclespeed_service.yaml        ← YAML definition
├── vehiclespeed_manifest.arxml      ← ARXML manifest
├── types.h                          ← Skeleton / Proxy classes
├── vehiclespeed_binding.h           ← C++ ID constants
├── vehiclespeed_provider_app.cpp    ← Publisher sample
├── vehiclespeed_consumer_app.cpp    ← Subscriber sample
└── BUILD_GUIDE.sh                   ← Build command reference
```

> **Continue to [Step 4](#step-4-integrate-into-a-user-application).**

---

## Method B: Template YAML

### B-1. Copy and edit the template

```bash
cp tools/arxml_generator/examples/my_first_service.yaml \
   my_vehiclespeed_service.yaml

# Open in your editor
nano my_vehiclespeed_service.yaml
```

**Minimum changes required (marked ★):**

```yaml
variables:
  PROVIDER_IP: "127.0.0.1"   # change to actual ECU IP for cross-machine tests
  CONSUMER_IP: "127.0.0.1"

autosar:
  packages:
    - short_name: MyFirstServiceManifest   # rename to match your service

      someip:
        provided_service_instances:
          - short_name: VehicleSpeedProviderInstance
            service_interface:
              id: 0xAB01      # ★ unique service ID for your project
            event_groups:
              - event_id: 0x8001   # ★ event ID (0x8000+)

        required_service_instances:
          - service_interface_id: 0xAB01   # ★ must equal provided id
```

### B-2. Generate ARXML

```bash
python3 tools/arxml_generator/generate_arxml.py \
  --input  my_vehiclespeed_service.yaml \
  --output my_vehiclespeed_manifest.arxml \
  --overwrite \
  --print-summary
```

Expected output:
```
[arxml-generator] generation summary
  output: my_vehiclespeed_manifest.arxml
  packages: 1
  communication clusters: 1
  ethernet connectors: 2
  provided SOME/IP instances: 1
  required SOME/IP instances: 1
  warnings: 0
```

### B-3. Generate C++ binding header

```bash
python3 tools/arxml_generator/generate_ara_com_binding.py \
  --input   my_vehiclespeed_manifest.arxml \
  --output  my_vehiclespeed_binding.h \
  --namespace  vehiclespeed::generated \
  --provided-service-short-name  VehicleSpeedProviderInstance \
  --provided-event-group-short-name  SensorDataEventGroup
```

Generated `my_vehiclespeed_binding.h`:
```cpp
#ifndef GEN_MY_VEHICLESPEED_BINDING_H_INCLUDED
#define GEN_MY_VEHICLESPEED_BINDING_H_INCLUDED

#include <cstdint>

// Auto-generated from ARXML. Do not edit manually.
namespace vehiclespeed {
namespace generated {
constexpr std::uint16_t kServiceId{0xAB01U};
constexpr std::uint16_t kInstanceId{0x0001U};
constexpr std::uint16_t kStatusEventId{0x8001U};
constexpr std::uint16_t kStatusEventGroupId{0x0001U};
constexpr std::uint8_t kMajorVersion{0x01U};
constexpr std::uint8_t kMinorVersion{0x00U};
}
}
#endif
```

---

## Step 4: Integrate into a User Application

### 4-1. Create the directory structure

```bash
mkdir -p user_apps/src/apps/my_vehiclespeed
```

Files to create:
```
user_apps/src/apps/my_vehiclespeed/
├── CMakeLists.txt              ← build definition
├── types.h                     ← Skeleton / Proxy classes
├── vehiclespeed_binding.h      ← generated binding constants
├── provider_app.cpp            ← Publisher application
└── consumer_app.cpp            ← Subscriber application
```

### 4-2. Set up types.h

Either copy the GUI-generated `types.h` or customize the existing template:

```bash
# If generated by the GUI
cp /tmp/pubsub_gen/types.h user_apps/src/apps/my_vehiclespeed/

# Or copy the existing SOME/IP template
cp user_apps/src/apps/communication/someip/types.h \
   user_apps/src/apps/my_vehiclespeed/types.h
```

**Key customization points in `types.h`:**

```cpp
namespace vehiclespeed::generated {

// ① Match these IDs to your ARXML values
constexpr std::uint16_t kServiceId{0xAB01U};
constexpr std::uint16_t kInstanceId{0x0001U};
constexpr std::uint16_t kEventId{0x8001U};
constexpr std::uint16_t kEventGroupId{0x0001U};
constexpr std::uint8_t  kMajorVersion{0x01U};

// ② Define your payload struct
struct SpeedData {
    std::uint32_t sequence;
    std::uint16_t speedKph;
    std::uint8_t  gear;
};

// ③ Skeleton and Proxy classes (auto-generated or hand-written)
class VehicleSpeedSkeleton : public ara::com::ServiceSkeletonBase {
public:
    ara::com::SkeletonEvent<SpeedData> SpeedEvent;
    // ...
};

class VehicleSpeedProxy : public ara::com::ServiceProxyBase {
public:
    ara::com::ProxyEvent<SpeedData> SpeedEvent;
    // ...
};

} // namespace
```

### 4-3. Publisher application

```cpp
// provider_app.cpp — sends vehicle speed data
#include <atomic>
#include <chrono>
#include <csignal>
#include <iostream>
#include <thread>

#include <ara/core/initialization.h>
#include <ara/log/logging_framework.h>
#include "types.h"

namespace {
    std::atomic<bool> gRunning{true};
    void HandleSignal(int) { gRunning.store(false); }
}

int main() {
    std::signal(SIGINT, HandleSignal);
    std::signal(SIGTERM, HandleSignal);

    // 1) Initialize runtime
    if (!ara::core::Initialize().HasValue()) {
        std::cerr << "Initialize failed\n";
        return 1;
    }

    // 2) Create logger
    auto logging = std::unique_ptr<ara::log::LoggingFramework>{
        ara::log::LoggingFramework::Create(
            "VSPD", ara::log::LogMode::kConsole,
            ara::log::LogLevel::kInfo, "VehicleSpeed Publisher")};
    auto& logger = logging->CreateLogger("VSPD", "Publisher", ara::log::LogLevel::kInfo);

    // 3) Build Skeleton (service provider)
    using namespace vehiclespeed::generated;
    auto specifier = CreateInstanceSpecifierOrThrow(
        "AdaptiveAutosar/UserApps/VehicleSpeedProvider");
    VehicleSpeedSkeleton skeleton{std::move(specifier)};

    // 4) Offer the service
    if (!skeleton.OfferService().HasValue()) {
        std::cerr << "OfferService failed\n";
        ara::core::Deinitialize();
        return 1;
    }
    skeleton.SpeedEvent.Offer();

    std::cout << "[VehicleSpeedProvider] Publishing. Press Ctrl+C to stop.\n";

    // 5) Publish data periodically
    std::uint32_t seq = 0;
    while (gRunning.load()) {
        ++seq;

        SpeedData data{};
        data.sequence = seq;
        data.speedKph = static_cast<std::uint16_t>(40 + (seq % 120));
        data.gear     = static_cast<std::uint8_t>((seq / 10) % 6 + 1);

        skeleton.SpeedEvent.Send(data);

        if (seq % 10 == 0) {
            auto s = logger.WithLevel(ara::log::LogLevel::kInfo);
            s << "Published seq=" << seq
              << " speed=" << static_cast<int>(data.speedKph)
              << " gear="  << static_cast<int>(data.gear);
            logging->Log(logger, ara::log::LogLevel::kInfo, s);
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

    // 6) Cleanup
    skeleton.SpeedEvent.StopOffer();
    skeleton.StopOfferService();
    ara::core::Deinitialize();
    return 0;
}
```

### 4-4. Subscriber application

```cpp
// consumer_app.cpp — receives vehicle speed data
#include <atomic>
#include <chrono>
#include <csignal>
#include <iostream>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

#include <ara/core/initialization.h>
#include <ara/log/logging_framework.h>
#include "types.h"

namespace {
    std::atomic<bool> gRunning{true};
    void HandleSignal(int) { gRunning.store(false); }
}

int main() {
    std::signal(SIGINT, HandleSignal);
    std::signal(SIGTERM, HandleSignal);

    if (!ara::core::Initialize().HasValue()) {
        std::cerr << "Initialize failed\n";
        return 1;
    }

    auto logging = std::unique_ptr<ara::log::LoggingFramework>{
        ara::log::LoggingFramework::Create(
            "VSUB", ara::log::LogMode::kConsole,
            ara::log::LogLevel::kInfo, "VehicleSpeed Subscriber")};
    auto& logger = logging->CreateLogger("VSUB", "Subscriber", ara::log::LogLevel::kInfo);

    using namespace vehiclespeed::generated;

    // 1) Start async service discovery
    std::mutex handleMutex;
    std::vector<VehicleSpeedProxy::HandleType> handles;

    auto specifier = CreateInstanceSpecifierOrThrow(
        "AdaptiveAutosar/UserApps/VehicleSpeedConsumer");

    auto findResult = VehicleSpeedProxy::StartFindService(
        [&handleMutex, &handles](VehicleSpeedProxy::HandleType found) {
            std::lock_guard<std::mutex> lock(handleMutex);
            handles = {found};
        },
        std::move(specifier));

    if (!findResult.HasValue()) {
        std::cerr << "StartFindService failed\n";
        ara::core::Deinitialize();
        return 1;
    }

    std::cout << "[VehicleSpeedConsumer] Waiting for service. Ctrl+C to stop.\n";

    std::unique_ptr<VehicleSpeedProxy> proxy;
    std::uint32_t recvCount = 0;

    while (gRunning.load()) {
        // 2) Create proxy and subscribe once a handle is available
        if (!proxy) {
            std::lock_guard<std::mutex> lock(handleMutex);
            if (!handles.empty()) {
                proxy = std::make_unique<VehicleSpeedProxy>(handles.front());

                // 3) Subscribe with queue depth 32
                proxy->SpeedEvent.Subscribe(32U);

                // 4) Register receive handler
                proxy->SpeedEvent.SetReceiveHandler(
                    [&proxy, &logging, &logger, &recvCount]() {
                        proxy->SpeedEvent.GetNewSamples(
                            [&](ara::com::SamplePtr<SpeedData> sample) {
                                ++recvCount;
                                auto s = logger.WithLevel(ara::log::LogLevel::kInfo);
                                s << "Received #" << recvCount
                                  << " seq="   << sample->sequence
                                  << " speed=" << static_cast<int>(sample->speedKph)
                                  << " gear="  << static_cast<int>(sample->gear);
                                logging->Log(logger, ara::log::LogLevel::kInfo, s);
                            }, 16U);
                    });
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }

    // 5) Cleanup
    if (proxy) {
        proxy->SpeedEvent.UnsetReceiveHandler();
        proxy->SpeedEvent.Unsubscribe();
    }
    VehicleSpeedProxy::StopFindService(findResult.Value());
    ara::core::Deinitialize();
    return 0;
}
```

### 4-5. CMakeLists.txt

```cmake
# user_apps/src/apps/my_vehiclespeed/CMakeLists.txt

add_user_template_target(
  vehiclespeed_provider
  provider_app.cpp
  AdaptiveAutosarAP::ara_core
  AdaptiveAutosarAP::ara_com
  AdaptiveAutosarAP::ara_log
)
target_include_directories(vehiclespeed_provider
  PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")

add_user_template_target(
  vehiclespeed_consumer
  consumer_app.cpp
  AdaptiveAutosarAP::ara_core
  AdaptiveAutosarAP::ara_com
  AdaptiveAutosarAP::ara_log
)
target_include_directories(vehiclespeed_consumer
  PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")

foreach(target vehiclespeed_provider vehiclespeed_consumer)
  if(USER_APPS_VSOMEIP_INCLUDE_DIR)
    target_include_directories(${target} SYSTEM PRIVATE "${USER_APPS_VSOMEIP_INCLUDE_DIR}")
  endif()
endforeach()
```

Append to `user_apps/src/apps/CMakeLists.txt`:
```cmake
add_subdirectory(my_vehiclespeed)
```

---

## Step 5: Build

### 5-1. Build from installed runtime (recommended)

```bash
./scripts/build_user_apps_from_opt.sh --prefix /opt/autosar_ap
```

Expected output:
```
[100%] Built target vehiclespeed_provider
[100%] Built target vehiclespeed_consumer
```

Executables:
```
build-user-apps-opt/src/apps/my_vehiclespeed/
├── vehiclespeed_provider
└── vehiclespeed_consumer
```

### 5-2. Build with cmake directly

```bash
cmake -S user_apps -B build-user-apps \
  -DAUTOSAR_AP_INSTALL_PREFIX=/opt/autosar_ap \
  -DCMAKE_BUILD_TYPE=Release

cmake --build build-user-apps -j$(nproc) \
  --target vehiclespeed_provider vehiclespeed_consumer
```

### 5-3. Integrate ARXML codegen into CMake (optional)

Add to the main `CMakeLists.txt` to auto-run binding generation at build time:

```cmake
if(ARA_COM_ENABLE_ARXML_CODEGEN)
  set(MY_ARXML  "${CMAKE_SOURCE_DIR}/my_vehiclespeed_manifest.arxml")
  set(MY_BIND_H "${CMAKE_BINARY_DIR}/generated/vehiclespeed_binding.h")

  add_custom_command(
    OUTPUT  "${MY_BIND_H}"
    COMMAND Python3::Interpreter
            "${CMAKE_SOURCE_DIR}/tools/arxml_generator/generate_ara_com_binding.py"
            --input  "${MY_ARXML}"
            --output "${MY_BIND_H}"
            --namespace vehiclespeed::generated
            --provided-service-short-name VehicleSpeedProviderInstance
            --provided-event-group-short-name SensorDataEventGroup
    DEPENDS "${MY_ARXML}"
    COMMENT "Generating vehiclespeed binding header"
  )

  add_custom_target(vehiclespeed_codegen ALL DEPENDS "${MY_BIND_H}")
  add_dependencies(vehiclespeed_provider vehiclespeed_codegen)
  target_include_directories(vehiclespeed_provider
    PRIVATE "${CMAKE_BINARY_DIR}/generated")
endif()
```

---

## Step 6: Run and Verify

### 6-1. Set environment variables

```bash
export AUTOSAR_AP_PREFIX=/opt/autosar_ap
export VSOMEIP_CONFIGURATION=${AUTOSAR_AP_PREFIX}/configuration/vsomeip-pubsub-sample.json
export LD_LIBRARY_PATH=${AUTOSAR_AP_PREFIX}/lib:${AUTOSAR_AP_PREFIX}/lib64:/opt/vsomeip/lib:${LD_LIBRARY_PATH:-}
```

### 6-2. Start the publisher (Terminal 1)

```bash
./build-user-apps-opt/src/apps/my_vehiclespeed/vehiclespeed_provider
```

Expected output:
```
[VehicleSpeedProvider] Publishing. Press Ctrl+C to stop.
[VehicleSpeedProvider] Published seq=10 speed=50 gear=2
[VehicleSpeedProvider] Published seq=20 speed=60 gear=3
```

### 6-3. Start the subscriber (Terminal 2)

```bash
./build-user-apps-opt/src/apps/my_vehiclespeed/vehiclespeed_consumer
```

Expected output:
```
[VehicleSpeedConsumer] Waiting for service. Ctrl+C to stop.
[VehicleSpeedConsumer] Received #1 seq=5 speed=44 gear=1
[VehicleSpeedConsumer] Received #2 seq=6 speed=45 gear=1
```

### 6-4. Verification checklist

- [ ] Publisher prints `Published` lines
- [ ] Subscriber prints `Received` lines
- [ ] `seq=` values in both terminals correspond
- [ ] Ctrl+C gracefully terminates both processes

---

## YAML Parameter Reference

### Choosing a service ID

```
Range          Recommended usage
0x0000         Reserved — do not use
0x0001–0x7FFF  Standard SOME/IP services
0x8000–0xFFFD  Implementation-defined / experimental
0xFFFE–0xFFFF  Reserved — do not use

Keep a project-level ID registry to avoid conflicts.
Use pubsub_designer_gui.py "Auto-generate ID" to derive a
deterministic ID from the service name.
```

### Choosing an event ID

```
0x0001–0x7FFF  Method IDs (used for RPC)
0x8000–0xFFFE  Event IDs ← use this range for Pub/Sub
0xFFFF         Reserved
```

### Choosing port numbers

```
1024–49151   Registered ports (check for conflicts with existing services)
49152–65535  Dynamic / private ports (generally safe to use)

Common example:
30490  SOME/IP SD (Service Discovery) — do NOT change
30509  Provider port  (example)
30510  Consumer port  (example)
```

### Tuning SD delays

```yaml
sd_server:
  initial_delay_min: 20    # ms — min delay before provider advertises
  initial_delay_max: 200   # ms — max delay before provider advertises

sd_client:
  initial_delay_min: 20    # ms — min delay before consumer searches
  initial_delay_max: 200   # ms — max delay before consumer searches
```

- Faster connection: `min=0, max=50`
- Load spreading across many ECUs: `min=0, max=2000`

---

## Troubleshooting

### Subscriber receives nothing

**Cause 1: vSomeIP configuration file not found**
```bash
echo $VSOMEIP_CONFIGURATION
ls "$VSOMEIP_CONFIGURATION"

# Check that the service ID in the JSON matches your ARXML
grep -i service "$VSOMEIP_CONFIGURATION"
```

**Cause 2: Service ID mismatch between ARXML and types.h**
```bash
grep "SERVICE-INTERFACE-ID" my_vehiclespeed_manifest.arxml
grep "kServiceId" user_apps/src/apps/my_vehiclespeed/types.h
# Both values must match
```

**Cause 3: Port blocked by firewall**
```bash
sudo ufw allow 30509/udp
sudo ufw allow 30510/udp
```

### Build error: "undefined reference to kServiceId"

The binding header is not included or the namespace differs:

```cpp
// Correct
#include "vehiclespeed_binding.h"
using namespace vehiclespeed::generated;
// or
auto id = vehiclespeed::generated::kServiceId;
```

### "OfferService failed"

The runtime may not be running. Check:
```bash
ps aux | grep autosar_platform_app
echo $LD_LIBRARY_PATH
```

### ARXML generation error: "service_interface_id is required"

```yaml
# These are equivalent — use either form:
service_interface_id: 0x1234    # flat form
# or
service_interface:
  id: 0x1234                    # nested form
```

### Running multiple services simultaneously

Use distinct IDs and ports per service:

```yaml
# service_a.yaml
service_interface.id: 0x1001
provider_port: 30509

# service_b.yaml
service_interface.id: 0x1002
provider_port: 30511
```

---

## Next Steps

| Tutorial | Topic |
| -------- | ----- |
| `05_zerocopy_pubsub.ja.md` | Shared-memory zero-copy with iceoryx |
| `06_dds_pubsub.ja.md` | Distributed communication with Cyclone DDS |
| `07_socketcan_decode.ja.md` | CAN bus reception and decoding |
| `08_ecu_full_stack.ja.md` | Full ECU stack integration |
| `09_rpi_ecu_deployment.ja.md` | Deploying to Raspberry Pi |

---

## Quick Reference

```bash
# Launch GUI designer
python3 tools/arxml_generator/pubsub_designer_gui.py

# YAML → ARXML
python3 tools/arxml_generator/generate_arxml.py \
  --input my_service.yaml --output my_service.arxml --overwrite

# ARXML → C++ binding header
python3 tools/arxml_generator/generate_ara_com_binding.py \
  --input my_service.arxml --output binding.h \
  --namespace my::ns

# Validate YAML only (no output)
python3 tools/arxml_generator/generate_arxml.py \
  --input my_service.yaml --validate-only --strict --print-summary

# Build user apps
./scripts/build_user_apps_from_opt.sh --prefix /opt/autosar_ap

# Set runtime environment
export AUTOSAR_AP_PREFIX=/opt/autosar_ap
export VSOMEIP_CONFIGURATION=$AUTOSAR_AP_PREFIX/configuration/vsomeip-pubsub-sample.json
export LD_LIBRARY_PATH=$AUTOSAR_AP_PREFIX/lib:/opt/vsomeip/lib:${LD_LIBRARY_PATH:-}
```
