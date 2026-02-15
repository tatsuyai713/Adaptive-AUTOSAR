#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdint>
#include <iostream>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

#include <ara/core/initialization.h>
#include <ara/log/logging_framework.h>

#include "types.h"

namespace
{
    // Global run flag toggled by SIGINT/SIGTERM.
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

    std::uint32_t ParsePollMs(int argc, char *argv[], std::uint32_t fallback)
    {
        const std::string prefix{"--poll-ms="};
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
#if !defined(ARA_COM_USE_VSOMEIP) || (ARA_COM_USE_VSOMEIP != 1)
    (void)argc;
    (void)argv;
    std::cout << "[TemplateSomeIpConsumer] ARA_COM_USE_VSOMEIP is disabled. "
                 "Rebuild runtime with SOME/IP backend enabled."
              << std::endl;
    return 0;
#else
    RegisterSignalHandlers();
    const std::uint32_t pollMs{ParsePollMs(argc, argv, 20U)};

    // 1) Initialize runtime.
    auto initResult{ara::core::Initialize()};
    if (!initResult.HasValue())
    {
        std::cerr << "[TemplateSomeIpConsumer] Initialize failed: "
                  << initResult.Error().Message() << std::endl;
        return 1;
    }

    // 2) Create logger.
    auto logging{std::unique_ptr<ara::log::LoggingFramework>{
        ara::log::LoggingFramework::Create(
            "UTSC",
            ara::log::LogMode::kConsole,
            ara::log::LogLevel::kInfo,
            "User app SOME/IP consumer template")}};

    const auto &logger{logging->CreateLogger(
        "UTSC",
        "Template SOME/IP consumer",
        ara::log::LogLevel::kInfo)};

    // 3) Start asynchronous service discovery.
    std::mutex handleMutex;
    std::vector<user_apps::com_template::VehicleSignalProxy::HandleType> handles;

    auto specifier{user_apps::com_template::CreateInstanceSpecifierOrThrow(
        "AdaptiveAutosar/UserApps/SomeIpConsumerTemplate")};

    auto findResult{user_apps::com_template::VehicleSignalProxy::StartFindService(
        [&handleMutex, &handles](
            ara::com::ServiceHandleContainer<
                user_apps::com_template::VehicleSignalProxy::HandleType> found)
        {
            // Keep the latest handle set in a shared container.
            std::lock_guard<std::mutex> lock(handleMutex);
            handles = std::move(found);
        },
        std::move(specifier))};

    if (!findResult.HasValue())
    {
        std::cerr << "[TemplateSomeIpConsumer] StartFindService failed: "
                  << findResult.Error().Message() << std::endl;
        (void)ara::core::Deinitialize();
        return 1;
    }

    std::unique_ptr<user_apps::com_template::VehicleSignalProxy> proxy;
    std::uint32_t receiveCount{0U};

    {
        auto stream{logger.WithLevel(ara::log::LogLevel::kInfo)};
        stream << "Consumer started. poll-ms=" << pollMs
               << ", press Ctrl+C to stop.";
        logging->Log(logger, ara::log::LogLevel::kInfo, stream);
    }

    while (gRunning.load())
    {
        // 4) Attach proxy once at least one service instance is available.
        if (!proxy)
        {
            user_apps::com_template::VehicleSignalProxy::HandleType selected{
                user_apps::com_template::kServiceId,
                user_apps::com_template::kInstanceId};
            bool hasHandle{false};

            {
                std::lock_guard<std::mutex> lock(handleMutex);
                if (!handles.empty())
                {
                    selected = handles.front();
                    hasHandle = true;
                }
            }

            if (hasHandle)
            {
                proxy.reset(new user_apps::com_template::VehicleSignalProxy{
                    selected});

                // 5) Subscribe with a fixed queue depth.
                proxy->StatusEvent.Subscribe(32U);

                // 6) Register state-change handler.
                proxy->StatusEvent.SetSubscriptionStateChangeHandler(
                    [&logging, &logger](ara::com::SubscriptionState state)
                    {
                        auto stream{logger.WithLevel(ara::log::LogLevel::kInfo)};
                        stream << "Subscription state changed to "
                               << static_cast<std::uint32_t>(state);
                        logging->Log(logger, ara::log::LogLevel::kInfo, stream);
                    });

                // 7) Register receive handler and drain sample queue inside it.
                proxy->StatusEvent.SetReceiveHandler(
                    [&proxy, &logging, &logger, &receiveCount]()
                    {
                        auto readResult{proxy->StatusEvent.GetNewSamples(
                            [&logging, &logger, &receiveCount](
                                ara::com::SamplePtr<
                                    user_apps::com_template::VehicleSignalFrame> sample)
                            {
                                ++receiveCount;
                                if ((receiveCount % 10U) == 0U)
                                {
                                    auto stream{logger.WithLevel(ara::log::LogLevel::kInfo)};
                                    stream << "Received seq=" << sample->SequenceCounter
                                           << " speed=" << static_cast<std::uint32_t>(sample->SpeedKph)
                                           << " rpm=" << static_cast<std::uint32_t>(sample->EngineRpm)
                                           << " gear="
                                           << static_cast<std::uint32_t>(sample->Gear);
                                    logging->Log(logger, ara::log::LogLevel::kInfo, stream);
                                }
                            },
                            16U)};

                        if (!readResult.HasValue())
                        {
                            auto stream{logger.WithLevel(ara::log::LogLevel::kWarn)};
                            stream << "GetNewSamples failed: "
                                   << readResult.Error().Message();
                            logging->Log(logger, ara::log::LogLevel::kWarn, stream);
                        }
                    });
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(pollMs));
    }

    // 8) Cleanup proxy state and discovery.
    if (proxy)
    {
        proxy->StatusEvent.UnsetReceiveHandler();
        proxy->StatusEvent.UnsetSubscriptionStateChangeHandler();
        proxy->StatusEvent.Unsubscribe();
        proxy.reset();
    }

    user_apps::com_template::VehicleSignalProxy::StopFindService(
        findResult.Value());

    (void)ara::core::Deinitialize();
    return 0;
#endif
}
