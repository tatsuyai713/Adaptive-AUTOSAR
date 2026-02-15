#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <memory>
#include <thread>

#include <ara/core/initialization.h>
#include <ara/log/logging_framework.h>

#include <ara/com/zerocopy/zero_copy_binding.h>

namespace
{
    std::atomic<bool> gRunning{true};

    struct ZeroCopyFrame
    {
        std::uint32_t SequenceCounter;
        std::uint32_t SpeedCentiKph;
        std::uint32_t EngineRpm;
    };

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
#if !defined(ARA_COM_USE_ICEORYX) || (ARA_COM_USE_ICEORYX != 1)
    (void)argc;
    (void)argv;
    std::cout << "[TemplateZeroCopySub] ARA_COM_USE_ICEORYX is disabled. "
                 "Rebuild runtime with zero-copy backend enabled."
              << std::endl;
    return 0;
#else
    RegisterSignalHandlers();
    const std::uint32_t pollMs{ParsePollMs(argc, argv, 20U)};

    // 1) Initialize runtime.
    auto initResult{ara::core::Initialize()};
    if (!initResult.HasValue())
    {
        std::cerr << "[TemplateZeroCopySub] Initialize failed: "
                  << initResult.Error().Message() << std::endl;
        return 1;
    }

    // 2) Setup logging.
    auto logging{std::unique_ptr<ara::log::LoggingFramework>{
        ara::log::LoggingFramework::Create(
            "UTZS",
            ara::log::LogMode::kConsole,
            ara::log::LogLevel::kInfo,
            "User app zero-copy subscriber template")}};
    const auto &logger{logging->CreateLogger(
        "UTZS",
        "Template zero-copy subscriber",
        ara::log::LogLevel::kInfo)};

    // 3) Use the same channel descriptor as the publisher.
    const ara::com::zerocopy::ChannelDescriptor channel{
        "user_apps",
        "templates",
        "vehicle_signal"};

    // 4) Create subscriber. RouDi must be running before this app starts.
    ara::com::zerocopy::ZeroCopySubscriber subscriber{
        channel,
        "user_apps_zerocopy_sub",
        64U,
        0U};

    if (!subscriber.IsBindingActive())
    {
        std::cerr << "[TemplateZeroCopySub] ZeroCopySubscriber binding is not active. "
                     "Check RouDi startup."
                  << std::endl;
        (void)ara::core::Deinitialize();
        return 1;
    }

    std::uint32_t receiveCount{0U};
    while (gRunning.load())
    {
        ara::com::zerocopy::ReceivedSample sample;

        // 5) Poll one sample without copying payload.
        auto takeResult{subscriber.TryTake(sample)};
        if (!takeResult.HasValue())
        {
            auto stream{logger.WithLevel(ara::log::LogLevel::kWarn)};
            stream << "TryTake failed: " << takeResult.Error().Message();
            logging->Log(logger, ara::log::LogLevel::kWarn, stream);
            std::this_thread::sleep_for(std::chrono::milliseconds(pollMs));
            continue;
        }

        if (!takeResult.Value())
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(pollMs));
            continue;
        }

        // 6) Deserialize fixed-size payload from shared memory region.
        if (sample.Size() >= sizeof(ZeroCopyFrame))
        {
            ZeroCopyFrame frame{};
            std::memcpy(&frame, sample.Data(), sizeof(frame));

            ++receiveCount;
            if ((receiveCount % 10U) == 0U)
            {
                auto stream{logger.WithLevel(ara::log::LogLevel::kInfo)};
                stream << "Received zero-copy frame seq=" << frame.SequenceCounter
                       << " speed_centi_kph=" << frame.SpeedCentiKph
                       << " rpm=" << frame.EngineRpm;
                logging->Log(logger, ara::log::LogLevel::kInfo, stream);
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(pollMs));
    }

    (void)ara::core::Deinitialize();
    return 0;
#endif
}
