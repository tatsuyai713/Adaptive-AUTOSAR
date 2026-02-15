#include <cstdint>
#include <iostream>

#include <ara/core/initialization.h>
#include <ara/core/instance_specifier.h>
#include <ara/per/persistency.h>
#include <ara/phm/health_channel.h>

int main()
{
    // 1) Initialize runtime first.
    auto initResult = ara::core::Initialize();
    if (!initResult.HasValue())
    {
        std::cerr << "[UserPerPhm] Initialize failed: "
                  << initResult.Error().Message() << std::endl;
        return 1;
    }

    // 2) Create an instance specifier used by PER/PHM APIs.
    auto instanceResult = ara::core::InstanceSpecifier::Create(
        "AdaptiveAutosar/UserApps/PerPhmDemo");
    if (!instanceResult.HasValue())
    {
        std::cerr << "[UserPerPhm] Invalid instance specifier: "
                  << instanceResult.Error().Message() << std::endl;
        (void)ara::core::Deinitialize();
        return 1;
    }

    // 3) Report health state transitions through PHM channel.
    ara::phm::HealthChannel health{instanceResult.Value()};
    (void)health.ReportHealthStatus(ara::phm::HealthStatus::kOk);

    // 4) Open key-value storage and update a persistent run counter.
    auto storageResult = ara::per::OpenKeyValueStorage(instanceResult.Value());
    if (storageResult.HasValue())
    {
        auto storage = storageResult.Value();

        // Read previous value. Missing key is treated as first launch.
        auto counterResult = storage->GetValue<std::uint64_t>("run_count");
        std::uint64_t runCount = counterResult.HasValue() ? counterResult.Value() : 0U;
        ++runCount;

        // Persist new value and force sync to storage.
        (void)storage->SetValue<std::uint64_t>("run_count", runCount);
        (void)storage->SyncToStorage();

        std::cout << "[UserPerPhm] run_count=" << runCount << std::endl;
    }
    else
    {
        // Storage may be unavailable in minimal environments; report and continue.
        std::cout << "[UserPerPhm] storage unavailable: "
                  << storageResult.Error().Message() << std::endl;
    }

    // 5) Report deactivation and deinitialize runtime.
    (void)health.ReportHealthStatus(ara::phm::HealthStatus::kDeactivated);
    (void)ara::core::Deinitialize();
    return 0;
}
