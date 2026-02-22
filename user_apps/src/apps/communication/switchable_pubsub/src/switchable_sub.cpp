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

std::uint32_t parse_poll_ms(int argc, char *argv[], std::uint32_t fallback)
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
        std::cerr << "[switchable_sub] Initialize failed: "
                  << init_result.Error().Message() << std::endl;
        return 1;
    }

    auto logging{std::unique_ptr<ara::log::LoggingFramework>{
        ara::log::LoggingFramework::Create(
            "SWSB",
            ara::log::LogMode::kConsole,
            ara::log::LogLevel::kInfo,
            "Switchable Pub/Sub subscriber")}};
    const auto &logger{logging->CreateLogger(
        "SWSB",
        "switchable subscriber",
        ara::log::LogLevel::kInfo)};

    const std::uint32_t poll_ms{parse_poll_ms(argc, argv, 20U)};
    const auto &topic_binding = sample::switchable_generated::GetTopicBinding(kTopicBindingIndex);

    auto info_stream{logger.WithLevel(ara::log::LogLevel::kInfo)};
    info_stream << "Starting subscriber. ros-topic="
                << (topic_binding.ros_topic != nullptr ? topic_binding.ros_topic : "")
                << " poll-ms=" << poll_ms;
    logging->Log(logger, ara::log::LogLevel::kInfo, info_stream);

    using SampleType = sample::transport::VehicleStatusFrame;
    sample::switchable_generated::TopicEventProxy<SampleType> proxy{topic_binding};
    proxy.Event.Subscribe(128U);

    while (g_running.load())
    {
        auto read_result{proxy.Event.GetNewSamples(
            [&logging, &logger](ara::com::SamplePtr<SampleType> sample)
            {
                if (!sample)
                {
                    return;
                }

                auto stream{logger.WithLevel(ara::log::LogLevel::kInfo)};
                stream << "I heard seq=" << sample->SequenceCounter()
                       << " speed=" << sample->SpeedCentiKph()
                       << " rpm=" << sample->EngineRpm()
                       << " gear=" << static_cast<std::uint32_t>(sample->Gear());
                logging->Log(logger, ara::log::LogLevel::kInfo, stream);
            },
            32U)};

        if (!read_result.HasValue())
        {
            auto stream{logger.WithLevel(ara::log::LogLevel::kWarn)};
            stream << "GetNewSamples failed.";
            logging->Log(logger, ara::log::LogLevel::kWarn, stream);
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(poll_ms));
    }

    proxy.Event.UnsetReceiveHandler();
    proxy.Event.Unsubscribe();

    (void)ara::core::Deinitialize();
    return 0;
}
