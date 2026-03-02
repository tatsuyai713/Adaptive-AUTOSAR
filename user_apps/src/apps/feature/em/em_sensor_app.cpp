/// @file user_apps/src/apps/feature/em/em_sensor_app.cpp
/// @brief Managed sensor application for ExecutionManager orchestration demo.
///
/// This app is designed to be launched by the EM daemon via ExecutionManager.
/// It simulates a temperature sensor that publishes readings periodically.
///
/// Behaviour:
///  - Logs kRunning on startup
///  - Emits sensor readings every EM_SENSOR_INTERVAL_MS milliseconds
///  - Exits cleanly on SIGTERM (reports kTerminating)
///  - Returns exit code 0 on graceful shutdown
///
/// Environment variables:
///  EM_SENSOR_INTERVAL_MS  [default: 1000]  Sampling interval in ms
///  EM_SENSOR_INSTANCE_ID  [default: "em_sensor_app"]  App instance ID for logs

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

    // Simulated sensor: produces a pseudo-random temperature in [20, 30] °C.
    float simulateTemperature(std::uint64_t cycle) noexcept
    {
        return 20.0F + static_cast<float>(cycle % 10U) * 1.0F;
    }
} // namespace

int main()
{
    auto initResult{ara::core::Initialize()};
    if (!initResult.HasValue())
    {
        std::cerr << "[em_sensor_app] Initialize failed: "
                  << initResult.Error().Message() << std::endl;
        return 1;
    }

    const std::uint32_t cIntervalMs{ReadEnvUint32("EM_SENSOR_INTERVAL_MS", 1000U)};
    const std::string cInstanceId{
        ReadEnvString("EM_SENSOR_INSTANCE_ID", "em_sensor_app")};

    auto logging{std::unique_ptr<ara::log::LoggingFramework>{
        ara::log::LoggingFramework::Create(
            "EMSA",
            ara::log::LogMode::kConsole,
            ara::log::LogLevel::kInfo,
            "EM managed sensor app")}};

    const auto &logger{logging->CreateLogger(
        "EMSA", "EM sensor app", ara::log::LogLevel::kInfo)};

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

        const float temperature{simulateTemperature(cycle)};
        std::ostringstream oss;
        oss << "[" << cInstanceId << "] cycle=" << cycle
            << "  temperature=" << temperature << " C";
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
