#include <atomic>
#include <chrono>
#include <condition_variable>
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
    (void)argc;
    (void)argv;
    RegisterSignalHandlers();

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

    // Condition variable to wake the main thread as soon as a service is found.
    std::mutex discoveryMutex;
    std::condition_variable discoveryCV;
    bool discoveryFound{false};

    auto specifier{user_apps::com_template::CreateInstanceSpecifierOrThrow(
        "AdaptiveAutosar/UserApps/SomeIpConsumerTemplate")};

    auto findResult{user_apps::com_template::VehicleSignalProxy::StartFindService(
        [&handleMutex, &handles, &discoveryMutex, &discoveryCV, &discoveryFound](
            ara::com::ServiceHandleContainer<
                user_apps::com_template::VehicleSignalProxy::HandleType> found)
        {
            // Keep the latest handle set in a shared container.
            {
                std::lock_guard<std::mutex> lock(handleMutex);
                handles = std::move(found);
            }
            // Wake the main thread when at least one handle is available.
            if (!handles.empty())
            {
                std::lock_guard<std::mutex> lock(discoveryMutex);
                discoveryFound = true;
                discoveryCV.notify_one();
            }
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
        stream << "Consumer started. Waiting for service discovery, press Ctrl+C to stop.";
        logging->Log(logger, ara::log::LogLevel::kInfo, stream);
    }

    // 4) Wait (event-driven) until a service is available — no polling.
    {
        std::unique_lock<std::mutex> lock(discoveryMutex);
        discoveryCV.wait_for(
            lock,
            std::chrono::seconds{30},
            [&discoveryFound] { return discoveryFound; });
    }

    // 5) Attach proxy once at least one service instance is available.
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

            // 6) Subscribe with a fixed queue depth.
            proxy->StatusEvent.Subscribe(32U);

            // 7) Register state-change handler.
            proxy->StatusEvent.SetSubscriptionStateChangeHandler(
                [&logging, &logger](ara::com::SubscriptionState state)
                {
                    auto stream{logger.WithLevel(ara::log::LogLevel::kInfo)};
                    stream << "Subscription state changed to "
                           << static_cast<std::uint32_t>(state);
                    logging->Log(logger, ara::log::LogLevel::kInfo, stream);
                });

            // 8) Register receive handler — data delivery is fully event-driven.
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

    // 9) Keep the process alive until shutdown signal.
    //    Data is delivered via SetReceiveHandler — no active polling required.
    while (gRunning.load())
    {
        std::this_thread::sleep_for(std::chrono::milliseconds{200U});
    }

    // 10) Cleanup proxy state and discovery.
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
