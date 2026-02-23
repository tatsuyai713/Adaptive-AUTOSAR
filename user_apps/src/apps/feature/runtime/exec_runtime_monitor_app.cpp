#include <algorithm>
#include <cerrno>
#include <chrono>
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <thread>

#include <ara/core/initialization.h>
#include <ara/core/instance_specifier.h>
#include <ara/exec/signal_handler.h>
#include <ara/log/logging_framework.h>
#include <ara/phm/health_channel.h>

namespace
{
    struct RuntimeConfig
    {
        std::uint32_t cycleMs{200U};
        std::uint32_t statusEveryCycles{10U};

        std::uint32_t aliveTimeoutMs{1200U};
        std::uint32_t aliveStartupGraceMs{500U};
        std::uint32_t aliveCooldownMs{1000U};
        bool stopOnExpired{false};

        std::uint64_t faultStallCycle{0U};
        std::uint32_t faultStallMs{2400U};

        std::string healthInstanceSpecifier{
            "AdaptiveAutosar/UserApps/ExecRuntimeMonitor"};
    };

    long ReadEnvInteger(
        const char *name,
        long fallback,
        long minimum,
        long maximum) noexcept
    {
        const char *raw{std::getenv(name)};
        if (raw == nullptr || *raw == '\0')
        {
            return fallback;
        }

        errno = 0;
        char *end{nullptr};
        const long value{std::strtol(raw, &end, 10)};
        if (errno != 0 || end == raw || (end != nullptr && *end != '\0'))
        {
            return fallback;
        }

        if (value < minimum)
        {
            return minimum;
        }

        if (value > maximum)
        {
            return maximum;
        }

        return value;
    }

    bool ReadEnvBool(const char *name, bool fallback) noexcept
    {
        const char *raw{std::getenv(name)};
        if (raw == nullptr || *raw == '\0')
        {
            return fallback;
        }

        std::string value{raw};
        std::transform(
            value.begin(),
            value.end(),
            value.begin(),
            [](unsigned char c)
            {
                return static_cast<char>(std::tolower(c));
            });

        if (value == "1" || value == "true" || value == "on" || value == "yes")
        {
            return true;
        }

        if (value == "0" || value == "false" || value == "off" || value == "no")
        {
            return false;
        }

        return fallback;
    }

    RuntimeConfig LoadRuntimeConfig() noexcept
    {
        RuntimeConfig cfg;
        cfg.cycleMs = static_cast<std::uint32_t>(
            ReadEnvInteger("USER_EXEC_CYCLE_MS", 200L, 10L, 5000L));
        cfg.statusEveryCycles = static_cast<std::uint32_t>(
            ReadEnvInteger("USER_EXEC_STATUS_EVERY", 10L, 1L, 1000L));

        // Keep legacy watchdog variables as fallbacks for compatibility.
        const long legacyTimeoutMs{
            ReadEnvInteger("USER_EXEC_WATCHDOG_TIMEOUT_MS", 1200L, 100L, 60000L)};
        const long legacyStartupGraceMs{
            ReadEnvInteger("USER_EXEC_WATCHDOG_STARTUP_GRACE_MS", 500L, 0L, 60000L)};
        const long legacyCooldownMs{
            ReadEnvInteger("USER_EXEC_WATCHDOG_COOLDOWN_MS", 1000L, 0L, 60000L)};

        cfg.aliveTimeoutMs = static_cast<std::uint32_t>(
            ReadEnvInteger("USER_EXEC_ALIVE_TIMEOUT_MS", legacyTimeoutMs, 100L, 60000L));
        cfg.aliveStartupGraceMs = static_cast<std::uint32_t>(
            ReadEnvInteger(
                "USER_EXEC_ALIVE_STARTUP_GRACE_MS",
                legacyStartupGraceMs,
                0L,
                60000L));
        cfg.aliveCooldownMs = static_cast<std::uint32_t>(
            ReadEnvInteger("USER_EXEC_ALIVE_COOLDOWN_MS", legacyCooldownMs, 0L, 60000L));
        cfg.stopOnExpired = ReadEnvBool("USER_EXEC_STOP_ON_EXPIRED", false);

        cfg.faultStallCycle = static_cast<std::uint64_t>(
            ReadEnvInteger("USER_EXEC_FAULT_STALL_CYCLE", 0L, 0L, 10000000L));

        const long defaultStallMs{
            static_cast<long>(cfg.aliveTimeoutMs) * 2L};
        cfg.faultStallMs = static_cast<std::uint32_t>(
            ReadEnvInteger(
                "USER_EXEC_FAULT_STALL_MS",
                defaultStallMs,
                0L,
                120000L));

        const char *instance{std::getenv("USER_EXEC_HEALTH_INSTANCE_SPECIFIER")};
        if (instance != nullptr && *instance != '\0')
        {
            cfg.healthInstanceSpecifier = instance;
        }

        return cfg;
    }
} // namespace

int main()
{
    auto initResult{ara::core::Initialize()};
    if (!initResult.HasValue())
    {
        std::cerr << "[UserExecMonitor] Initialize failed: "
                  << initResult.Error().Message() << std::endl;
        return 1;
    }

    auto logging{std::unique_ptr<ara::log::LoggingFramework>{
        ara::log::LoggingFramework::Create(
            "UEMN",
            ara::log::LogMode::kConsole,
            ara::log::LogLevel::kInfo,
            "User app execution monitoring demo")}};

    const auto &logger{logging->CreateLogger(
        "UEMN",
        "User exec monitor app",
        ara::log::LogLevel::kInfo)};

    auto logText = [&](ara::log::LogLevel level, const std::string &text)
    {
        auto stream{logger.WithLevel(level)};
        stream << text;
        logging->Log(logger, level, stream);
    };

    const RuntimeConfig cfg{LoadRuntimeConfig()};
    ara::exec::SignalHandler::Register();

    std::ostringstream cfgSummary;
    cfgSummary
        << "Started. cycle_ms=" << cfg.cycleMs
        << ", alive_timeout_ms=" << cfg.aliveTimeoutMs
        << ", startup_grace_ms=" << cfg.aliveStartupGraceMs
        << ", cooldown_ms=" << cfg.aliveCooldownMs
        << ", stop_on_expired=" << (cfg.stopOnExpired ? "true" : "false")
        << ", fault_stall_cycle=" << cfg.faultStallCycle;
    logText(ara::log::LogLevel::kInfo, cfgSummary.str());

    std::unique_ptr<ara::phm::HealthChannel> healthChannel;
    auto healthSpecifierResult{
        ara::core::InstanceSpecifier::Create(cfg.healthInstanceSpecifier)};
    if (!healthSpecifierResult.HasValue())
    {
        std::ostringstream stream;
        stream << "PHM health reporting disabled: invalid instance specifier '"
               << cfg.healthInstanceSpecifier
               << "' (" << healthSpecifierResult.Error().Message() << ")";
        logText(ara::log::LogLevel::kWarn, stream.str());
    }
    else
    {
        healthChannel.reset(new ara::phm::HealthChannel{
            healthSpecifierResult.Value()});
        auto offerResult{healthChannel->Offer()};
        if (!offerResult.HasValue())
        {
            std::ostringstream stream;
            stream << "PHM health channel offer failed: "
                   << offerResult.Error().Message();
            logText(ara::log::LogLevel::kWarn, stream.str());
            healthChannel.reset();
        }
        else
        {
            (void)healthChannel->ReportHealthStatus(ara::phm::HealthStatus::kOk);
        }
    }

    const auto timeout{std::chrono::milliseconds(cfg.aliveTimeoutMs)};
    const auto startupGrace{std::chrono::milliseconds(cfg.aliveStartupGraceMs)};
    const auto cooldown{std::chrono::milliseconds(cfg.aliveCooldownMs)};

    const auto monitorStart{std::chrono::steady_clock::now()};
    const auto startupDeadline{monitorStart + startupGrace};
    auto lastCheckpoint{monitorStart};
    auto lastExpiryLog{std::chrono::steady_clock::time_point::min()};

    bool isExpired{false};
    std::uint64_t expiredCount{0U};
    std::uint64_t cycle{0U};

    while (!ara::exec::SignalHandler::IsTerminationRequested())
    {
        ++cycle;

        if (cfg.faultStallCycle > 0U && cycle == cfg.faultStallCycle)
        {
            std::ostringstream stream;
            stream << "Fault injection: stall for "
                   << cfg.faultStallMs
                   << " ms (cycle="
                   << cycle
                   << ").";
            logText(ara::log::LogLevel::kWarn, stream.str());
            std::this_thread::sleep_for(
                std::chrono::milliseconds(cfg.faultStallMs));
        }

        const auto now{std::chrono::steady_clock::now()};
        const auto gap{
            std::chrono::duration_cast<std::chrono::milliseconds>(
                now - lastCheckpoint)};
        const bool afterStartupGrace{now >= startupDeadline};
        const bool expiredNow{afterStartupGrace && gap > timeout};

        if (expiredNow)
        {
            const bool shouldLog{
                lastExpiryLog == std::chrono::steady_clock::time_point::min() ||
                (now - lastExpiryLog) >= cooldown};
            if (shouldLog)
            {
                lastExpiryLog = now;
                ++expiredCount;

                std::ostringstream stream;
                stream << "Alive timeout detected. gap_ms=" << gap.count()
                       << ", timeout_ms=" << cfg.aliveTimeoutMs
                       << ", count=" << expiredCount;
                logText(ara::log::LogLevel::kError, stream.str());

                if (healthChannel)
                {
                    (void)healthChannel->ReportHealthStatus(
                        ara::phm::HealthStatus::kExpired);
                }
            }

            isExpired = true;
            if (cfg.stopOnExpired)
            {
                logText(
                    ara::log::LogLevel::kError,
                    "Policy stop-on-expired is enabled. Terminating main loop.");
                break;
            }
        }
        else if (isExpired)
        {
            isExpired = false;
            logText(
                ara::log::LogLevel::kInfo,
                "Alive timeout monitor recovered to normal.");
            if (healthChannel)
            {
                (void)healthChannel->ReportHealthStatus(
                    ara::phm::HealthStatus::kOk);
            }
        }

        if ((cycle % cfg.statusEveryCycles) == 0U)
        {
            std::ostringstream stream;
            stream << "Heartbeat cycle=" << cycle
                   << ", alive_state=" << (isExpired ? "expired" : "ok")
                   << ", alive_gap_ms=" << gap.count()
                   << ", alive_expired_count=" << expiredCount;
            logText(ara::log::LogLevel::kInfo, stream.str());
        }

        lastCheckpoint = std::chrono::steady_clock::now();
        std::this_thread::sleep_for(std::chrono::milliseconds(cfg.cycleMs));
    }

    if (healthChannel)
    {
        (void)healthChannel->ReportHealthStatus(
            ara::phm::HealthStatus::kDeactivated);
        healthChannel->StopOffer();
    }

    {
        std::ostringstream stream;
        stream << "Shutdown complete. final_alive_expired_count="
               << expiredCount;
        logText(ara::log::LogLevel::kInfo, stream.str());
    }

    (void)ara::core::Deinitialize();
    return 0;
}
