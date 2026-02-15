#include <chrono>
#include <cstdint>
#include <iostream>
#include <memory>
#include <thread>

#include <ara/core/initialization.h>
#include <ara/log/logging_framework.h>

int main()
{
    // 1) Initialize the AUTOSAR Adaptive runtime before using any ara::* APIs.
    auto initResult{ara::core::Initialize()};
    if (!initResult.HasValue())
    {
        std::cerr << "[TplRuntimeLifecycle] Initialize failed: "
                  << initResult.Error().Message() << std::endl;
        return 1;
    }

    // 2) Create one logging framework instance for this process.
    auto logging{std::unique_ptr<ara::log::LoggingFramework>{
        ara::log::LoggingFramework::Create(
            "UTRL",
            ara::log::LogMode::kConsole,
            ara::log::LogLevel::kInfo,
            "User template: runtime lifecycle")}};

    // 3) Create a logger context for application logs.
    const auto &logger{
        logging->CreateLogger(
            "RTLF",
            "Runtime lifecycle template",
            ara::log::LogLevel::kInfo)};

    // 4) Run a minimal periodic loop to show how app lifecycle looks in practice.
    for (std::uint32_t heartbeat{1U}; heartbeat <= 10U; ++heartbeat)
    {
        auto stream{logger.WithLevel(ara::log::LogLevel::kInfo)};
        stream << "Heartbeat cycle " << heartbeat
               << " / 10 (replace this block with your business logic).";
        logging->Log(logger, ara::log::LogLevel::kInfo, stream);

        std::this_thread::sleep_for(std::chrono::milliseconds{100U});
    }

    // 5) Deinitialize runtime on graceful shutdown.
    (void)ara::core::Deinitialize();
    return 0;
}
