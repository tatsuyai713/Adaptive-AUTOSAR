#include <algorithm>
#include <atomic>
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
#include <ara/exec/execution_client.h>
#include <ara/exec/extension/non_standard.h>
#include <ara/exec/signal_handler.h>
#include <ara/log/logging_framework.h>

#ifdef ARA_COM_USE_VSOMEIP
#include <ara/com/someip/rpc/socket_rpc_client.h>
#endif

namespace
{
    struct RuntimeConfig
    {
        std::uint32_t cycleMs{200U};
        std::uint32_t statusEveryCycles{10U};
        std::uint32_t watchdogTimeoutMs{1200U};
        std::uint32_t watchdogStartupGraceMs{500U};
        std::uint32_t watchdogCooldownMs{1000U};
        bool watchdogKeepRunningOnExpiry{true};
        bool watchdogAutoReset{false};
        std::uint64_t faultStallCycle{0U};
        std::uint32_t faultStallMs{2400U};
        bool enableEmReport{false};
        std::int64_t emReportTimeoutSec{2};
        std::string emInstanceSpecifier{
            "AdaptiveAutosar/UserApps/ExecRuntimeMonitor"};
#ifdef ARA_COM_USE_VSOMEIP
        std::string rpcIpAddress{"127.0.0.1"};
        std::uint16_t rpcPort{8080U};
        std::uint8_t rpcProtocolVersion{1U};
#endif
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
        long value{std::strtol(raw, &end, 10)};
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
            { return static_cast<char>(std::tolower(c)); });

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
        cfg.watchdogTimeoutMs = static_cast<std::uint32_t>(
            ReadEnvInteger("USER_EXEC_WATCHDOG_TIMEOUT_MS", 1200L, 100L, 60000L));
        cfg.watchdogStartupGraceMs = static_cast<std::uint32_t>(
            ReadEnvInteger("USER_EXEC_WATCHDOG_STARTUP_GRACE_MS", 500L, 0L, 60000L));
        cfg.watchdogCooldownMs = static_cast<std::uint32_t>(
            ReadEnvInteger("USER_EXEC_WATCHDOG_COOLDOWN_MS", 1000L, 0L, 60000L));
        cfg.watchdogKeepRunningOnExpiry =
            ReadEnvBool("USER_EXEC_WATCHDOG_KEEP_RUNNING", true);
        cfg.watchdogAutoReset =
            ReadEnvBool("USER_EXEC_WATCHDOG_AUTO_RESET", false);
        cfg.faultStallCycle = static_cast<std::uint64_t>(
            ReadEnvInteger("USER_EXEC_FAULT_STALL_CYCLE", 0L, 0L, 10000000L));

        const long defaultStallMs{
            static_cast<long>(cfg.watchdogTimeoutMs) * 2L};
        cfg.faultStallMs = static_cast<std::uint32_t>(
            ReadEnvInteger(
                "USER_EXEC_FAULT_STALL_MS",
                defaultStallMs,
                0L,
                120000L));

        cfg.enableEmReport = ReadEnvBool("USER_EXEC_ENABLE_EM_REPORT", false);
        cfg.emReportTimeoutSec =
            ReadEnvInteger("USER_EXEC_EM_REPORT_TIMEOUT_SEC", 2L, 1L, 30L);

        const char *cInstance{std::getenv("USER_EXEC_INSTANCE_SPECIFIER")};
        if (cInstance != nullptr && *cInstance != '\0')
        {
            cfg.emInstanceSpecifier = cInstance;
        }

#ifdef ARA_COM_USE_VSOMEIP
        const char *cRpcIp{std::getenv("USER_EXEC_RPC_IP")};
        if (cRpcIp != nullptr && *cRpcIp != '\0')
        {
            cfg.rpcIpAddress = cRpcIp;
        }

        cfg.rpcPort = static_cast<std::uint16_t>(
            ReadEnvInteger("USER_EXEC_RPC_PORT", 8080L, 1L, 65535L));
        cfg.rpcProtocolVersion = static_cast<std::uint8_t>(
            ReadEnvInteger("USER_EXEC_RPC_PROTOCOL_VERSION", 1L, 1L, 255L));
#endif

        return cfg;
    }
}

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
        << ", watchdog_timeout_ms=" << cfg.watchdogTimeoutMs
        << ", watchdog_keep_running="
        << (cfg.watchdogKeepRunningOnExpiry ? "true" : "false")
        << ", fault_stall_cycle=" << cfg.faultStallCycle
        << ", em_report=" << (cfg.enableEmReport ? "on" : "off");
    logText(ara::log::LogLevel::kInfo, cfgSummary.str());

    std::atomic_bool watchdogExpired{false};
    ara::exec::extension::ProcessWatchdog::WatchdogOptions options;
    options.startupGrace = std::chrono::milliseconds(cfg.watchdogStartupGraceMs);
    options.expiryCallbackCooldown = std::chrono::milliseconds(cfg.watchdogCooldownMs);
    options.keepRunningOnExpiry = cfg.watchdogKeepRunningOnExpiry;

    ara::exec::extension::ProcessWatchdog watchdog(
        "autosar_user_tpl_exec_runtime_monitor",
        std::chrono::milliseconds(cfg.watchdogTimeoutMs),
        [&watchdogExpired](const std::string &processName)
        {
            std::cerr << "[UserExecMonitor] Watchdog timeout detected for "
                      << processName << std::endl;
            watchdogExpired.store(true);
        },
        options);
    watchdog.Start();

    std::unique_ptr<ara::exec::ExecutionClient> executionClient;
#ifdef ARA_COM_USE_VSOMEIP
    std::unique_ptr<ara::com::someip::rpc::SocketRpcClient> rpcClient;
#endif

    if (cfg.enableEmReport)
    {
        auto instanceResult{
            ara::core::InstanceSpecifier::Create(cfg.emInstanceSpecifier)};
        if (!instanceResult.HasValue())
        {
            std::ostringstream stream;
            stream << "EM report disabled: invalid instance specifier '"
                   << cfg.emInstanceSpecifier
                   << "' (" << instanceResult.Error().Message() << ")";
            logText(ara::log::LogLevel::kWarn, stream.str());
        }
        else
        {
#ifdef ARA_COM_USE_VSOMEIP
            try
            {
                rpcClient.reset(new ara::com::someip::rpc::SocketRpcClient(
                    nullptr,
                    cfg.rpcIpAddress,
                    cfg.rpcPort,
                    cfg.rpcProtocolVersion));

                executionClient.reset(new ara::exec::ExecutionClient(
                    instanceResult.Value(),
                    rpcClient.get(),
                    cfg.emReportTimeoutSec));

                auto runningResult{
                    executionClient->ReportExecutionState(
                        ara::exec::ExecutionState::kRunning)};
                if (runningResult.HasValue())
                {
                    logText(
                        ara::log::LogLevel::kInfo,
                        "EM report: kRunning sent successfully.");
                }
                else
                {
                    std::ostringstream stream;
                    stream << "EM report (kRunning) failed: "
                           << runningResult.Error().Message();
                    logText(ara::log::LogLevel::kWarn, stream.str());
                }
            }
            catch (const std::exception &ex)
            {
                std::ostringstream stream;
                stream << "EM reporter setup failed: " << ex.what();
                logText(ara::log::LogLevel::kWarn, stream.str());
            }
#else
            logText(
                ara::log::LogLevel::kWarn,
                "EM report requested, but this runtime was built without vSomeIP support.");
#endif
        }
    }

    std::uint64_t cycle{0U};
    std::uint64_t previousExpiryCount{0U};

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

        watchdog.ReportAlive();

        if (watchdogExpired.exchange(false))
        {
            std::ostringstream stream;
            stream << "Monitor callback observed watchdog expiry. count="
                   << watchdog.GetExpiryCount();
            logText(ara::log::LogLevel::kError, stream.str());

            if (cfg.watchdogAutoReset)
            {
                watchdog.Reset();
                logText(
                    ara::log::LogLevel::kWarn,
                    "Watchdog reset executed by monitor policy.");
            }
        }

        if ((cycle % cfg.statusEveryCycles) == 0U)
        {
            const auto expiryCount{watchdog.GetExpiryCount()};
            if (expiryCount != previousExpiryCount)
            {
                previousExpiryCount = expiryCount;
            }

            std::ostringstream stream;
            stream << "Heartbeat cycle=" << cycle
                   << ", watchdog_expired="
                   << (watchdog.IsExpired() ? "true" : "false")
                   << ", watchdog_expiry_count="
                   << expiryCount;
            logText(ara::log::LogLevel::kInfo, stream.str());
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(cfg.cycleMs));
    }

    if (executionClient)
    {
        auto terminatingResult{
            executionClient->ReportExecutionState(
                ara::exec::ExecutionState::kTerminating)};
        if (terminatingResult.HasValue())
        {
            logText(
                ara::log::LogLevel::kInfo,
                "EM report: kTerminating sent successfully.");
        }
        else
        {
            std::ostringstream stream;
            stream << "EM report (kTerminating) failed: "
                   << terminatingResult.Error().Message();
            logText(ara::log::LogLevel::kWarn, stream.str());
        }
    }

    watchdog.Stop();
    {
        std::ostringstream stream;
        stream << "Shutdown complete. final_watchdog_expiry_count="
               << watchdog.GetExpiryCount();
        logText(ara::log::LogLevel::kInfo, stream.str());
    }

    (void)ara::core::Deinitialize();
    return 0;
}
