#include <chrono>
#include <cstdint>
#include <iostream>
#include <memory>
#include <thread>

#include <ara/core/initialization.h>
#include <ara/exec/signal_handler.h>
#include <ara/log/logging_framework.h>

int main()
{
    // 1) Initialize runtime. All ara::* APIs should be used after this step.
    auto initResult{ara::core::Initialize()};
    if (!initResult.HasValue())
    {
        std::cerr << "[TemplateExecSignal] Initialize failed: "
                  << initResult.Error().Message() << std::endl;
        return 1;
    }

    // 2) Create logger to observe lifecycle events.
    auto logging{std::unique_ptr<ara::log::LoggingFramework>{
        ara::log::LoggingFramework::Create(
            "UTES",
            ara::log::LogMode::kConsole,
            ara::log::LogLevel::kInfo,
            "User app execution/signal template")}};

    const auto &logger{logging->CreateLogger(
        "UTES",
        "Template exec signal app",
        ara::log::LogLevel::kInfo)};

    // 3) Register standardized SIGTERM/SIGINT handling helper.
    ara::exec::SignalHandler::Register();

    {
        auto stream{logger.WithLevel(ara::log::LogLevel::kInfo)};
        stream << "Signal template running. Send SIGINT/SIGTERM to stop.";
        logging->Log(logger, ara::log::LogLevel::kInfo, stream);
    }

    // 4) Run a simple cycle until termination is requested.
    std::uint32_t heartbeat{0U};
    while (!ara::exec::SignalHandler::IsTerminationRequested())
    {
        ++heartbeat;
        if ((heartbeat % 10U) == 0U)
        {
            auto stream{logger.WithLevel(ara::log::LogLevel::kInfo)};
            stream << "Heartbeat=" << heartbeat;
            logging->Log(logger, ara::log::LogLevel::kInfo, stream);
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(100U));
    }

    // 5) Deinitialize runtime before process exit.
    (void)ara::core::Deinitialize();
    return 0;
}
