#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <thread>

#include <ara/core/initialization.h>
#include <ara/log/logging_framework.h>

#include "types.h"

namespace
{
    // Global run flag toggled by SIGINT/SIGTERM.
    std::atomic<bool> gRunning{true};

    // Basic signal handler for stopping the publishing loop cleanly.
    void HandleSignal(int)
    {
        gRunning.store(false);
    }

    // Register process-level handlers. This keeps the template easy to run
    // manually from a terminal.
    void RegisterSignalHandlers()
    {
        std::signal(SIGINT, HandleSignal);
        std::signal(SIGTERM, HandleSignal);
    }

    // Simple argument parser for unsigned integer values in the form:
    //   --period-ms=100
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
#if !defined(ARA_COM_USE_VSOMEIP) || (ARA_COM_USE_VSOMEIP != 1)
    // This binary can still be compiled even when SOME/IP is disabled in the
    // installed AUTOSAR AP package.
    (void)argc;
    (void)argv;
    std::cout << "[TemplateSomeIpProvider] ARA_COM_USE_VSOMEIP is disabled. "
                 "Rebuild runtime with SOME/IP backend enabled."
              << std::endl;
    return 0;
#else
    RegisterSignalHandlers();

    // Tune event publishing cycle from CLI.
    const std::uint32_t periodMs{ParsePeriodMs(argc, argv, 200U)};

    // 1) Initialize ara::core runtime once per process.
    auto initResult{ara::core::Initialize()};
    if (!initResult.HasValue())
    {
        std::cerr << "[TemplateSomeIpProvider] Initialize failed: "
                  << initResult.Error().Message() << std::endl;
        return 1;
    }

    // 2) Prepare logging for operational traces.
    auto logging{std::unique_ptr<ara::log::LoggingFramework>{
        ara::log::LoggingFramework::Create(
            "UTSP",
            ara::log::LogMode::kConsole,
            ara::log::LogLevel::kInfo,
            "User app SOME/IP provider template")}};

    const auto &logger{logging->CreateLogger(
        "UTSP",
        "Template SOME/IP provider",
        ara::log::LogLevel::kInfo)};

    // 3) Build a skeleton object for one service instance.
    auto specifier{user_apps::com_template::CreateInstanceSpecifierOrThrow(
        "AdaptiveAutosar/UserApps/SomeIpProviderTemplate")};
    user_apps::com_template::VehicleSignalSkeleton skeleton{
        std::move(specifier)};

    // 4) Offer the service so consumers can discover it.
    auto offerServiceResult{skeleton.OfferService()};
    if (!offerServiceResult.HasValue())
    {
        std::cerr << "[TemplateSomeIpProvider] OfferService failed: "
                  << offerServiceResult.Error().Message() << std::endl;
        (void)ara::core::Deinitialize();
        return 1;
    }

    // 5) Register subscription callback to inspect client subscribe/unsubscribe.
    auto subscriptionHandlerResult{
        skeleton.SetEventSubscriptionStateHandler(
            user_apps::com_template::kEventGroupId,
            [&logging, &logger](std::uint16_t clientId, bool subscribed)
            {
                auto stream{logger.WithLevel(ara::log::LogLevel::kInfo)};
                stream << "Client 0x" << static_cast<std::uint32_t>(clientId)
                       << (subscribed ? " subscribed" : " unsubscribed");
                logging->Log(logger, ara::log::LogLevel::kInfo, stream);
                return true;
            })};

    if (!subscriptionHandlerResult.HasValue())
    {
        std::cerr << "[TemplateSomeIpProvider] SetEventSubscriptionStateHandler failed: "
                  << subscriptionHandlerResult.Error().Message() << std::endl;
        skeleton.StopOfferService();
        (void)ara::core::Deinitialize();
        return 1;
    }

    // 6) Offer event channel.
    auto offerEventResult{skeleton.StatusEvent.Offer()};
    if (!offerEventResult.HasValue())
    {
        std::cerr << "[TemplateSomeIpProvider] Event offer failed: "
                  << offerEventResult.Error().Message() << std::endl;
        skeleton.UnsetEventSubscriptionStateHandler(
            user_apps::com_template::kEventGroupId);
        skeleton.StopOfferService();
        (void)ara::core::Deinitialize();
        return 1;
    }

    {
        auto stream{logger.WithLevel(ara::log::LogLevel::kInfo)};
        stream << "Provider started. period-ms=" << periodMs
               << ", press Ctrl+C to stop.";
        logging->Log(logger, ara::log::LogLevel::kInfo, stream);
    }

    // 7) Publish periodic event payloads.
    std::uint32_t sequence{0U};
    while (gRunning.load())
    {
        ++sequence;

        user_apps::com_template::VehicleSignalFrame frame{};
        frame.SequenceCounter = sequence;
        frame.SpeedKph = static_cast<std::uint16_t>(40U + (sequence % 120U));
        frame.EngineRpm = static_cast<std::uint16_t>(900U + (sequence % 2500U));
        frame.Gear = static_cast<std::uint8_t>((sequence % 6U) + 1U);
        frame.StatusFlags = static_cast<std::uint8_t>(sequence % 2U);

        // Typed send via ara::com::SkeletonEvent<T>.
        skeleton.StatusEvent.Send(frame);

        if ((sequence % 10U) == 0U)
        {
            auto stream{logger.WithLevel(ara::log::LogLevel::kInfo)};
            stream << "Published frame seq=" << frame.SequenceCounter
                   << " speed=" << static_cast<std::uint32_t>(frame.SpeedKph)
                   << " rpm=" << static_cast<std::uint32_t>(frame.EngineRpm);
            logging->Log(logger, ara::log::LogLevel::kInfo, stream);
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(periodMs));
    }

    // 8) Stop offer in reverse order.
    skeleton.StatusEvent.StopOffer();
    skeleton.UnsetEventSubscriptionStateHandler(
        user_apps::com_template::kEventGroupId);
    skeleton.StopOfferService();

    (void)ara::core::Deinitialize();
    return 0;
#endif
}
