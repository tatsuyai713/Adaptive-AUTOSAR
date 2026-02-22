#include <atomic>
#include <chrono>
#include <csignal>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <memory>
#include <thread>

#include <ara/core/initialization.h>
#include <ara/log/logging_framework.h>

#include "VehicleStatusFrame.hpp"
#include "switchable_proxy_skeleton.hpp"

namespace
{
std::atomic<bool> g_running{true};
constexpr std::size_t kTopicBindingIndex{0U};

void handle_signal(int)
{
    g_running.store(false);
}

void register_signals()
{
    std::signal(SIGINT, handle_signal);
    std::signal(SIGTERM, handle_signal);
}

std::uint32_t parse_period_ms(int argc, char *argv[], std::uint32_t fallback)
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
            return static_cast<std::uint32_t>(std::stoul(arg.substr(prefix.size())));
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
    register_signals();

    auto init_result{ara::core::Initialize()};
    if (!init_result.HasValue())
    {
        std::cerr << "[switchable_pub] Initialize failed: "
                  << init_result.Error().Message() << std::endl;
        return 1;
    }

    auto logging{std::unique_ptr<ara::log::LoggingFramework>{
        ara::log::LoggingFramework::Create(
            "SWPB",
            ara::log::LogMode::kConsole,
            ara::log::LogLevel::kInfo,
            "Switchable Pub/Sub publisher")}};
    const auto &logger{logging->CreateLogger(
        "SWPB",
        "switchable publisher",
        ara::log::LogLevel::kInfo)};

    const std::uint32_t period_ms{parse_period_ms(argc, argv, 200U)};
    const auto &topic_binding = sample::switchable_generated::GetTopicBinding(kTopicBindingIndex);

    auto info_stream{logger.WithLevel(ara::log::LogLevel::kInfo)};
    info_stream << "Starting publisher. ros-topic="
                << (topic_binding.ros_topic != nullptr ? topic_binding.ros_topic : "")
                << " period-ms=" << period_ms;
    logging->Log(logger, ara::log::LogLevel::kInfo, info_stream);

    using SampleType = sample::transport::VehicleStatusFrame;
    sample::switchable_generated::TopicEventSkeleton<SampleType> skeleton{topic_binding};

    auto offer_service_result{skeleton.OfferService()};
    if (!offer_service_result.HasValue())
    {
        std::cerr << "[switchable_pub] OfferService failed." << std::endl;
        (void)ara::core::Deinitialize();
        return 1;
    }

    auto offer_event_result{skeleton.Event.Offer()};
    if (!offer_event_result.HasValue())
    {
        std::cerr << "[switchable_pub] Event offer failed." << std::endl;
        skeleton.StopOfferService();
        (void)ara::core::Deinitialize();
        return 1;
    }

    std::uint32_t sequence{0U};
    while (g_running.load())
    {
        ++sequence;

        SampleType sample{};
        sample.SequenceCounter(sequence);
        sample.SpeedCentiKph(6000U + (sequence % 200U));
        sample.EngineRpm(900U + (sequence % 2500U));
        sample.SteeringAngleCentiDeg(static_cast<std::uint16_t>(sequence % 720U));
        sample.Gear(static_cast<std::uint8_t>((sequence % 6U) + 1U));
        sample.StatusFlags(static_cast<std::uint8_t>(sequence % 2U));

        skeleton.Event.Send(sample);

        auto stream{logger.WithLevel(ara::log::LogLevel::kInfo)};
        stream << "Publishing seq=" << sample.SequenceCounter()
               << " speed=" << sample.SpeedCentiKph()
               << " rpm=" << sample.EngineRpm();
        logging->Log(logger, ara::log::LogLevel::kInfo, stream);

        std::this_thread::sleep_for(std::chrono::milliseconds(period_ms));
    }

    skeleton.Event.StopOffer();
    skeleton.StopOfferService();

    (void)ara::core::Deinitialize();
    return 0;
}
