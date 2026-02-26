#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdint>
#include <iostream>
#include <memory>

#include <ara/core/initialization.h>
#include <ara/log/logging_framework.h>

#if defined(ARA_COM_USE_CYCLONEDDS) && (ARA_COM_USE_CYCLONEDDS == 1)
#include <ara/com/dds/dds_pubsub.h>
#endif

#if defined(USER_APPS_DDS_TYPE_AVAILABLE) && (USER_APPS_DDS_TYPE_AVAILABLE == 1)
#include "UserAppsStatus.hpp"
#endif

namespace
{
    std::atomic<bool> gRunning{true};

    void HandleSignal(int)
    {
        gRunning.store(false);
    }

    void RegisterSignalHandlers()
    {
        std::signal(SIGINT, HandleSignal);
        std::signal(SIGTERM, HandleSignal);
    }
} // namespace

int main(int argc, char *argv[])
{
#if !defined(ARA_COM_USE_CYCLONEDDS) || (ARA_COM_USE_CYCLONEDDS != 1)
    (void)argc;
    (void)argv;
    std::cout << "[TemplateDdsSub] ARA_COM_USE_CYCLONEDDS is disabled. "
                 "Rebuild runtime with DDS backend enabled."
              << std::endl;
    return 0;
#elif !defined(USER_APPS_DDS_TYPE_AVAILABLE) || (USER_APPS_DDS_TYPE_AVAILABLE != 1)
    (void)argc;
    (void)argv;
    std::cout << "[TemplateDdsSub] DDS type code is not generated. "
                 "Ensure idlc is available and reconfigure user_apps."
              << std::endl;
    return 0;
#else
    (void)argc;
    (void)argv;
    RegisterSignalHandlers();

    // 1) Initialize runtime.
    auto initResult{ara::core::Initialize()};
    if (!initResult.HasValue())
    {
        std::cerr << "[TemplateDdsSub] Initialize failed: "
                  << initResult.Error().Message() << std::endl;
        return 1;
    }

    // 2) Setup logging.
    auto logging{std::unique_ptr<ara::log::LoggingFramework>{
        ara::log::LoggingFramework::Create(
            "UTDS",
            ara::log::LogMode::kConsole,
            ara::log::LogLevel::kInfo,
            "User app DDS subscriber template")}};
    const auto &logger{logging->CreateLogger(
        "UTDS",
        "Template DDS subscriber",
        ara::log::LogLevel::kInfo)};

    const std::string topic{"adaptive_autosar/user_apps/apps/UserAppsStatus"};
    const std::uint32_t domainId{0U};

    // 3) Create DDS subscriber for generated IDL type.
    ara::com::dds::DdsSubscriber<user_apps::templates::UserAppsStatus> subscriber{
        topic,
        domainId};

    if (!subscriber.IsBindingActive())
    {
        std::cerr << "[TemplateDdsSub] DdsSubscriber binding is not active." << std::endl;
        (void)ara::core::Deinitialize();
        return 1;
    }

    std::uint32_t receiveCount{0U};
    while (gRunning.load())
    {
        // Wait for new DDS data (up to 500 ms) — event-driven, no busy-wait.
        subscriber.WaitForData(std::chrono::milliseconds{500U});

        // Take up to N DDS samples each time data arrives.
        auto takeResult{subscriber.Take(
            32U,
            [&logging, &logger, &receiveCount](
                const user_apps::templates::UserAppsStatus &sample)
            {
                ++receiveCount;
                if ((receiveCount % 10U) == 0U)
                {
                    auto stream{logger.WithLevel(ara::log::LogLevel::kInfo)};
                    stream << "Received DDS sample seq=" << sample.SequenceCounter()
                           << " speed_centi_kph=" << sample.SpeedCentiKph()
                           << " rpm=" << sample.EngineRpm();
                    logging->Log(logger, ara::log::LogLevel::kInfo, stream);
                }
            })};

        if (!takeResult.HasValue())
        {
            auto stream{logger.WithLevel(ara::log::LogLevel::kWarn)};
            stream << "DDS take failed: " << takeResult.Error().Message();
            logging->Log(logger, ara::log::LogLevel::kWarn, stream);
        }
    }

    (void)ara::core::Deinitialize();
    return 0;
#endif
}
