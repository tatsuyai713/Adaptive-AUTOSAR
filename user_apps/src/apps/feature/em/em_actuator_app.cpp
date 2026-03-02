/// @file user_apps/src/apps/feature/em/em_actuator_app.cpp
/// @brief Managed actuator application for ExecutionManager orchestration demo.
///
/// This app is designed to be launched by the EM daemon via ExecutionManager.
/// It simulates an actuator that cycles through target positions.
///
/// Behaviour:
///  - Logs kRunning on startup
///  - Logs actuator position commands every EM_ACTUATOR_INTERVAL_MS milliseconds
///  - Exits cleanly on SIGTERM (reports kTerminating)
///  - Returns exit code 0 on graceful shutdown
///
/// Environment variables:
///  EM_ACTUATOR_INTERVAL_MS  [default: 2000]  Command interval in ms
///  EM_ACTUATOR_INSTANCE_ID  [default: "em_actuator_app"]  App instance ID

#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <sstream>
#include <thread>

#include <ara/core/initialization.h>
#include <ara/exec/signal_handler.h>
#include <ara/log/logging_framework.h>

namespace
{
    std::uint32_t ReadEnvUint32(const char *name, std::uint32_t fallback) noexcept
    {
        const char *raw{std::getenv(name)};
        if (raw == nullptr || *raw == '\0')
        {
            return fallback;
        }
        const long val{std::strtol(raw, nullptr, 10)};
        return (val > 0) ? static_cast<std::uint32_t>(val) : fallback;
    }

    std::string ReadEnvString(const char *name, const std::string &fallback)
    {
        const char *raw{std::getenv(name)};
        return (raw != nullptr && *raw != '\0') ? std::string{raw} : fallback;
    }

    // Simulate actuator positions: 0 → 45 → 90 → 45 → 0 → ... degrees.
    const float cPositionTable[] = {0.0F, 45.0F, 90.0F, 45.0F};
    constexpr std::size_t cPositionTableSize =
        sizeof(cPositionTable) / sizeof(cPositionTable[0]);

    float simulatePosition(std::uint64_t cycle) noexcept
    {
        return cPositionTable[cycle % cPositionTableSize];
    }
} // namespace

int main()
{
    auto initResult{ara::core::Initialize()};
    if (!initResult.HasValue())
    {
        std::cerr << "[em_actuator_app] Initialize failed: "
                  << initResult.Error().Message() << std::endl;
        return 1;
    }

    const std::uint32_t cIntervalMs{ReadEnvUint32("EM_ACTUATOR_INTERVAL_MS", 2000U)};
    const std::string cInstanceId{
        ReadEnvString("EM_ACTUATOR_INSTANCE_ID", "em_actuator_app")};

    auto logging{std::unique_ptr<ara::log::LoggingFramework>{
        ara::log::LoggingFramework::Create(
            "EMAA",
            ara::log::LogMode::kConsole,
            ara::log::LogLevel::kInfo,
            "EM managed actuator app")}};

    const auto &logger{logging->CreateLogger(
        "EMAA", "EM actuator app", ara::log::LogLevel::kInfo)};

    auto logText = [&](ara::log::LogLevel lvl, const std::string &msg)
    {
        auto stream{logger.WithLevel(lvl)};
        stream << msg;
        logging->Log(logger, lvl, stream);
    };

    ara::exec::SignalHandler::Register();

    {
        std::ostringstream oss;
        oss << "[" << cInstanceId << "] kRunning — interval_ms=" << cIntervalMs;
        logText(ara::log::LogLevel::kInfo, oss.str());
    }

    std::uint64_t cycle{0U};
    while (!ara::exec::SignalHandler::IsTerminationRequested())
    {
        ++cycle;

        const float position{simulatePosition(cycle)};
        std::ostringstream oss;
        oss << "[" << cInstanceId << "] cycle=" << cycle
            << "  target_position=" << position << " deg  status=OK";
        logText(ara::log::LogLevel::kInfo, oss.str());

        std::this_thread::sleep_for(std::chrono::milliseconds(cIntervalMs));
    }

    {
        std::ostringstream oss;
        oss << "[" << cInstanceId << "] kTerminating — total cycles=" << cycle;
        logText(ara::log::LogLevel::kInfo, oss.str());
    }

    (void)ara::core::Deinitialize();
    return 0;
}
