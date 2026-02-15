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
#if !defined(ARA_COM_USE_ICEORYX) || (ARA_COM_USE_ICEORYX != 1)
    (void)argc;
    (void)argv;
    std::cout << "[TemplateZeroCopyPub] ARA_COM_USE_ICEORYX is disabled. "
                 "Rebuild runtime with zero-copy backend enabled."
              << std::endl;
    return 0;
#else
    RegisterSignalHandlers();
    const std::uint32_t periodMs{ParsePeriodMs(argc, argv, 100U)};

    // 1) Initialize runtime.
    auto initResult{ara::core::Initialize()};
    if (!initResult.HasValue())
    {
        std::cerr << "[TemplateZeroCopyPub] Initialize failed: "
                  << initResult.Error().Message() << std::endl;
        return 1;
    }

    // 2) Setup logging.
    auto logging{std::unique_ptr<ara::log::LoggingFramework>{
        ara::log::LoggingFramework::Create(
            "UTZP",
            ara::log::LogMode::kConsole,
            ara::log::LogLevel::kInfo,
            "User app zero-copy publisher template")}};
    const auto &logger{logging->CreateLogger(
        "UTZP",
        "Template zero-copy publisher",
        ara::log::LogLevel::kInfo)};

    // 3) Define logical channel tokens.
    const ara::com::zerocopy::ChannelDescriptor channel{
        "user_apps",
        "templates",
        "vehicle_signal"};

    // 4) Create publisher. RouDi must be running before this app starts.
    ara::com::zerocopy::ZeroCopyPublisher publisher{
        channel,
        "user_apps_zerocopy_pub",
        8U};

    if (!publisher.IsBindingActive())
    {
        std::cerr << "[TemplateZeroCopyPub] ZeroCopyPublisher binding is not active. "
                     "Check RouDi startup."
                  << std::endl;
        (void)ara::core::Deinitialize();
        return 1;
    }

    std::uint32_t sequence{0U};
    while (gRunning.load())
    {
        ++sequence;

        ZeroCopyFrame frame{};
        frame.SequenceCounter = sequence;
        frame.SpeedCentiKph = 5000U + (sequence % 1200U);
        frame.EngineRpm = 1000U + (sequence % 3000U);

        ara::com::zerocopy::LoanedSample sample;

        // 5) Loan memory chunk from middleware shared memory pool.
        auto loanResult{publisher.Loan(sizeof(frame), sample, alignof(ZeroCopyFrame))};
        if (!loanResult.HasValue())
        {
            auto stream{logger.WithLevel(ara::log::LogLevel::kWarn)};
            stream << "Loan failed: " << loanResult.Error().Message();
            logging->Log(logger, ara::log::LogLevel::kWarn, stream);
            std::this_thread::sleep_for(std::chrono::milliseconds(periodMs));
            continue;
        }

        // 6) Fill the loaned memory with payload and publish it.
        std::memcpy(sample.Data(), &frame, sizeof(frame));

        auto publishResult{publisher.Publish(std::move(sample))};
        if (!publishResult.HasValue())
        {
            auto stream{logger.WithLevel(ara::log::LogLevel::kWarn)};
            stream << "Publish failed: " << publishResult.Error().Message();
            logging->Log(logger, ara::log::LogLevel::kWarn, stream);
        }

        if ((sequence % 10U) == 0U)
        {
            auto stream{logger.WithLevel(ara::log::LogLevel::kInfo)};
            stream << "Published zero-copy frame seq=" << frame.SequenceCounter
                   << " speed_centi_kph=" << frame.SpeedCentiKph
                   << " rpm=" << frame.EngineRpm;
            logging->Log(logger, ara::log::LogLevel::kInfo, stream);
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(periodMs));
    }

    (void)ara::core::Deinitialize();
    return 0;
#endif
}
