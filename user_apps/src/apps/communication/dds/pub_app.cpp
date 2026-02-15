#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdint>
#include <iostream>
#include <memory>
#include <thread>

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

    std::uint32_t ParsePeriodMs(int argc, char *argv[], std::uint32_t fallback)
    {
        const std::string prefix{"--period-ms="};
        for (int i = 1; i < argc; ++i)
        {
            const std::string arg{argv[i]};
            if (arg.find(prefix) != 0U)
            {
                continue;
            }

            try
            {
                return static_cast<std::uint32_t>(
                    std::stoul(arg.substr(prefix.size())));
            }
            catch (const std::exception &)
            {
                return fallback;
            }
        }

        return fallback;
    }
} // namespace

int main(int argc, char *argv[])
{
#if !defined(ARA_COM_USE_CYCLONEDDS) || (ARA_COM_USE_CYCLONEDDS != 1)
    (void)argc;
    (void)argv;
    std::cout << "[TemplateDdsPub] ARA_COM_USE_CYCLONEDDS is disabled. "
                 "Rebuild runtime with DDS backend enabled."
              << std::endl;
    return 0;
#elif !defined(USER_APPS_DDS_TYPE_AVAILABLE) || (USER_APPS_DDS_TYPE_AVAILABLE != 1)
    (void)argc;
    (void)argv;
    std::cout << "[TemplateDdsPub] DDS type code is not generated. "
                 "Ensure idlc is available and reconfigure user_apps."
              << std::endl;
    return 0;
#else
    RegisterSignalHandlers();
    const std::uint32_t periodMs{ParsePeriodMs(argc, argv, 100U)};

    // 1) Initialize runtime.
    auto initResult{ara::core::Initialize()};
    if (!initResult.HasValue())
    {
        std::cerr << "[TemplateDdsPub] Initialize failed: "
                  << initResult.Error().Message() << std::endl;
        return 1;
    }

    // 2) Setup logging.
    auto logging{std::unique_ptr<ara::log::LoggingFramework>{
        ara::log::LoggingFramework::Create(
            "UTDP",
            ara::log::LogMode::kConsole,
            ara::log::LogLevel::kInfo,
            "User app DDS publisher template")}};
    const auto &logger{logging->CreateLogger(
        "UTDP",
        "Template DDS publisher",
        ara::log::LogLevel::kInfo)};

    const std::string topic{"adaptive_autosar/user_apps/apps/UserAppsStatus"};
    const std::uint32_t domainId{0U};

    // 3) Create DDS publisher with generated IDL type.
    ara::com::dds::DdsPublisher<user_apps::templates::UserAppsStatus> publisher{
        topic,
        domainId};

    if (!publisher.IsBindingActive())
    {
        std::cerr << "[TemplateDdsPub] DdsPublisher binding is not active." << std::endl;
        (void)ara::core::Deinitialize();
        return 1;
    }

    std::uint32_t sequence{0U};
    while (gRunning.load())
    {
        ++sequence;

        user_apps::templates::UserAppsStatus sample{};
        sample.SequenceCounter(sequence);
        sample.SpeedCentiKph(6000U + (sequence % 1000U));
        sample.EngineRpm(900U + (sequence % 3200U));
        sample.Gear(static_cast<std::uint8_t>((sequence % 6U) + 1U));
        sample.StatusFlags(static_cast<std::uint8_t>(sequence % 2U));

        // 4) Publish one DDS sample.
        auto writeResult{publisher.Write(sample)};
        if (!writeResult.HasValue())
        {
            auto stream{logger.WithLevel(ara::log::LogLevel::kWarn)};
            stream << "DDS write failed: " << writeResult.Error().Message();
            logging->Log(logger, ara::log::LogLevel::kWarn, stream);
        }

        if ((sequence % 10U) == 0U)
        {
            auto stream{logger.WithLevel(ara::log::LogLevel::kInfo)};
            stream << "Published DDS sample seq=" << sequence;
            logging->Log(logger, ara::log::LogLevel::kInfo, stream);
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(periodMs));
    }

    (void)ara::core::Deinitialize();
    return 0;
#endif
}
