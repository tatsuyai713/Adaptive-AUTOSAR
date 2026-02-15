#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdint>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>

#include <ara/core/initialization.h>
#include <ara/core/instance_specifier.h>
#include <ara/log/logging_framework.h>

#include "../../../features/communication/pubsub/pubsub_autosar_portable_api.h"
#include "../../../features/communication/pubsub/pubsub_common.h"

namespace
{
    // Process-wide run flag toggled by SIGINT/SIGTERM.
    std::atomic_bool gRunning{true};

    void HandleSignal(int)
    {
        gRunning.store(false);
    }

    void RegisterSignalHandlers()
    {
        std::signal(SIGINT, HandleSignal);
        std::signal(SIGTERM, HandleSignal);
    }

    std::uint32_t ParsePeriodMs(int argc, char *argv[], std::uint32_t fallbackValue)
    {
        std::string value;
        if (sample::ara_com_pubsub::TryReadArgument(argc, argv, "--period-ms", value))
        {
            return sample::ara_com_pubsub::ParsePositiveUintOrDefault(value, fallbackValue);
        }

        return fallbackValue;
    }

    ara::core::InstanceSpecifier CreateSpecifierOrThrow(const std::string &path)
    {
        auto specifierResult{ara::core::InstanceSpecifier::Create(path)};
        if (!specifierResult.HasValue())
        {
            throw std::runtime_error(specifierResult.Error().Message());
        }

        return specifierResult.Value();
    }
}

int main(int argc, char *argv[])
{
#if !defined(ARA_COM_USE_VSOMEIP) || (ARA_COM_USE_VSOMEIP != 1)
    (void)argc;
    (void)argv;
    std::cout << "[TplEcuSomeIpSource] SOME/IP backend is disabled. "
                 "Rebuild runtime with ARA_COM_USE_VSOMEIP=ON."
              << std::endl;
    return 0;
#else
    RegisterSignalHandlers();
    const std::uint32_t periodMs{ParsePeriodMs(argc, argv, 100U)};

    // 1) Initialize ara::core runtime.
    auto initResult{ara::core::Initialize()};
    if (!initResult.HasValue())
    {
        std::cerr << "[TplEcuSomeIpSource] Initialize failed: "
                  << initResult.Error().Message() << std::endl;
        return 1;
    }

    // 2) Setup logging for operational visibility.
    auto logging{std::unique_ptr<ara::log::LoggingFramework>{
        ara::log::LoggingFramework::Create(
            "UESP",
            ara::log::LogMode::kConsole,
            ara::log::LogLevel::kInfo,
            "User template: ECU SOME/IP source")}};
    const auto &logger{
        logging->CreateLogger(
            "UESP",
            "ECU SOME/IP source template",
            ara::log::LogLevel::kInfo)};

    // 3) Build provider with portable SOME/IP backend profile.
    sample::ara_com_pubsub::portable::BackendProfile profile;
    profile.EventBinding = sample::ara_com_pubsub::portable::EventBackend::kSomeIp;
    profile.ZeroCopyBinding = sample::ara_com_pubsub::portable::ZeroCopyBackend::kNone;

    sample::ara_com_pubsub::portable::VehicleStatusProvider provider{
        CreateSpecifierOrThrow(sample::ara_com_pubsub::cProviderInstanceSpecifier),
        std::move(profile)};

    auto offerServiceResult{provider.OfferService()};
    if (!offerServiceResult.HasValue())
    {
        std::cerr << "[TplEcuSomeIpSource] OfferService failed: "
                  << offerServiceResult.Error().Message() << std::endl;
        (void)ara::core::Deinitialize();
        return 1;
    }

    auto subscriptionHandlerResult{
        provider.SetSubscriptionStateHandler(
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
        std::cerr << "[TplEcuSomeIpSource] SetSubscriptionStateHandler failed: "
                  << subscriptionHandlerResult.Error().Message() << std::endl;
        provider.StopOfferService();
        (void)ara::core::Deinitialize();
        return 1;
    }

    auto offerEventResult{provider.OfferEvent()};
    if (!offerEventResult.HasValue())
    {
        std::cerr << "[TplEcuSomeIpSource] OfferEvent failed: "
                  << offerEventResult.Error().Message() << std::endl;
        provider.UnsetSubscriptionStateHandler();
        provider.StopOfferService();
        (void)ara::core::Deinitialize();
        return 1;
    }

    {
        auto stream{logger.WithLevel(ara::log::LogLevel::kInfo)};
        stream << "Started ECU SOME/IP source template. period-ms=" << periodMs
               << ", press Ctrl+C to stop.";
        logging->Log(logger, ara::log::LogLevel::kInfo, stream);
    }

    // 4) Publish vehicle-status frames over SOME/IP periodically.
    std::uint32_t sequence{0U};
    while (gRunning.load())
    {
        ++sequence;

        sample::ara_com_pubsub::VehicleStatusFrame frame{};
        frame.SequenceCounter = sequence;
        frame.SpeedCentiKph = 5000U + (sequence % 3000U);
        frame.EngineRpm = 900U + (sequence % 3500U);
        frame.SteeringAngleCentiDeg = static_cast<std::uint16_t>(sequence % 1200U);
        frame.Gear = static_cast<std::uint8_t>((sequence % 6U) + 1U);
        frame.StatusFlags = static_cast<std::uint8_t>(sequence % 2U);

        const auto payload{sample::ara_com_pubsub::SerializeFrame(frame)};
        auto notifyResult{provider.NotifyEvent(payload, true)};
        if (!notifyResult.HasValue())
        {
            auto stream{logger.WithLevel(ara::log::LogLevel::kWarn)};
            stream << "NotifyEvent failed: " << notifyResult.Error().Message();
            logging->Log(logger, ara::log::LogLevel::kWarn, stream);
        }

        if ((sequence % 10U) == 0U)
        {
            auto stream{logger.WithLevel(ara::log::LogLevel::kInfo)};
            stream << "Published SOME/IP source frame seq=" << sequence
                   << " speed_centi_kph=" << frame.SpeedCentiKph
                   << " rpm=" << frame.EngineRpm;
            logging->Log(logger, ara::log::LogLevel::kInfo, stream);
        }

        std::this_thread::sleep_for(std::chrono::milliseconds{periodMs});
    }

    provider.StopOfferEvent();
    provider.UnsetSubscriptionStateHandler();
    provider.StopOfferService();
    (void)ara::core::Deinitialize();
    return 0;
#endif
}
