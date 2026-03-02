/// @file user_apps/src/apps/feature/em/em_daemon_app.cpp
/// @brief ExecutionManager orchestration daemon demo.
///
/// This daemon demonstrates how to use ara::exec::ExecutionManager to:
///  - Register multiple managed application processes
///  - Map them to Function Groups and states
///  - Activate / terminate Function Groups (starting / stopping processes)
///  - Monitor process state changes via a callback
///  - Perform a graceful shutdown on SIGTERM
///
/// ### Architecture
///
///   [em_daemon_app]             [em_sensor_app]   [em_actuator_app]
///    ExecutionManager ─fork/exec──►  (child)            (child)
///    ExecutionServer   ◄──waitpid── exit(0)             exit(0)
///    StateServer
///
/// The managed child processes are launched as real OS processes via
/// fork()/exec().  They handle SIGTERM to exit cleanly.
///
/// ### Environment variables
///
///  EM_SENSOR_BIN     Path to em_sensor_app binary  [auto-detected]
///  EM_ACTUATOR_BIN   Path to em_actuator_app binary [auto-detected]
///  EM_RUN_SECONDS    Seconds to run before auto-shutdown (0 = run until SIGTERM)
///                    [default: 10]
///
/// ### Usage
///
///   # Run for 10 seconds then stop automatically:
///   ./em_daemon_app
///
///   # Run until Ctrl+C:
///   EM_RUN_SECONDS=0 ./em_daemon_app
///
///   # Specify custom binary paths:
///   EM_SENSOR_BIN=/opt/autosar_ap/bin/autosar_user_em_sensor_app \
///   EM_ACTUATOR_BIN=/opt/autosar_ap/bin/autosar_user_em_actuator_app \
///   EM_RUN_SECONDS=30 ./em_daemon_app

#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include <ara/com/someip/rpc/rpc_server.h>
#include <ara/core/initialization.h>
#include <ara/exec/execution_manager.h>
#include <ara/exec/execution_server.h>
#include <ara/exec/signal_handler.h>
#include <ara/exec/state_server.h>
#include <ara/log/logging_framework.h>

// ---------------------------------------------------------------------------
// Minimal in-process RPC server (no network transport needed for this demo)
// ---------------------------------------------------------------------------
namespace
{
    /// @brief RpcServer subclass that performs pure in-process dispatch.
    ///        No network sockets are opened. Suitable for demos that don't
    ///        require managed apps to report state via SOME/IP.
    class LocalRpcServer final : public ara::com::someip::rpc::RpcServer
    {
    public:
        LocalRpcServer() : RpcServer(1U, 1U) {}
    };

    // -----------------------------------------------------------------------
    // Helpers
    // -----------------------------------------------------------------------

    std::string ReadEnvString(const char *name, const std::string &fallback)
    {
        const char *raw{std::getenv(name)};
        return (raw != nullptr && *raw != '\0') ? std::string{raw} : fallback;
    }

    std::uint32_t ReadEnvUint32(const char *name, std::uint32_t fallback) noexcept
    {
        const char *raw{std::getenv(name)};
        if (raw == nullptr || *raw == '\0')
        {
            return fallback;
        }
        const long val{std::strtol(raw, nullptr, 10)};
        return (val >= 0) ? static_cast<std::uint32_t>(val) : fallback;
    }

    // Auto-detect binary path relative to current binary location.
    // Looks for <dir>/<name>, then /opt/autosar_ap/bin/<name>.
    std::string FindBinary(const std::string &name, const std::string &envVar)
    {
        // 1) Honour environment override
        const char *fromEnv{std::getenv(envVar.c_str())};
        if (fromEnv != nullptr && *fromEnv != '\0')
        {
            return std::string{fromEnv};
        }

        // 2) Try installed location
        const std::string installed{"/opt/autosar_ap/bin/" + name};
        if (::access(installed.c_str(), X_OK) == 0)
        {
            return installed;
        }

        // 3) Try sibling binary in same directory as argv[0] — not available
        //    here, so fall back to just the name (resolved via PATH).
        return name;
    }

    /// @brief Convert ManagedProcessState to a display string.
    const char *StateToString(ara::exec::ManagedProcessState s) noexcept
    {
        using S = ara::exec::ManagedProcessState;
        switch (s)
        {
        case S::kNotRunning:  return "NotRunning";
        case S::kStarting:    return "Starting";
        case S::kRunning:     return "Running";
        case S::kTerminating: return "Terminating";
        case S::kTerminated:  return "Terminated";
        case S::kFailed:      return "Failed";
        default:              return "Unknown";
        }
    }

} // namespace

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------

int main()
{
    // 1) Initialize AUTOSAR Adaptive Runtime --------------------------------
    auto initResult{ara::core::Initialize()};
    if (!initResult.HasValue())
    {
        std::cerr << "[em_daemon] Initialize failed: "
                  << initResult.Error().Message() << std::endl;
        return 1;
    }

    // 2) Set up logging -----------------------------------------------------
    auto logging{std::unique_ptr<ara::log::LoggingFramework>{
        ara::log::LoggingFramework::Create(
            "EMDA",
            ara::log::LogMode::kConsole,
            ara::log::LogLevel::kInfo,
            "EM orchestration daemon")}};

    const auto &logger{logging->CreateLogger(
        "EMDA", "EM daemon", ara::log::LogLevel::kInfo)};

    auto logInfo = [&](const std::string &msg)
    {
        auto stream{logger.WithLevel(ara::log::LogLevel::kInfo)};
        stream << msg;
        logging->Log(logger, ara::log::LogLevel::kInfo, stream);
    };
    auto logWarn = [&](const std::string &msg)
    {
        auto stream{logger.WithLevel(ara::log::LogLevel::kWarn)};
        stream << msg;
        logging->Log(logger, ara::log::LogLevel::kWarn, stream);
    };

    // 3) Read configuration -------------------------------------------------
    const std::string cSensorBin{
        FindBinary("autosar_user_em_sensor_app", "EM_SENSOR_BIN")};
    const std::string cActuatorBin{
        FindBinary("autosar_user_em_actuator_app", "EM_ACTUATOR_BIN")};
    const std::uint32_t cRunSeconds{ReadEnvUint32("EM_RUN_SECONDS", 10U)};

    {
        std::ostringstream oss;
        oss << "Config:"
            << "  sensor_bin=" << cSensorBin
            << "  actuator_bin=" << cActuatorBin
            << "  run_seconds=" << cRunSeconds
                << " (0=until SIGTERM)";
        logInfo(oss.str());
    }

    // 4) Build in-process RPC servers (no network sockets) ------------------
    LocalRpcServer execRpc;   // Used by ExecutionServer
    LocalRpcServer stateRpc;  // Used by StateServer

    // 5) Declare Function Group states --------------------------------------
    //    MachineFG has two states: "Off" and "Running".
    //    Both managed apps should be active in "Running".
    std::set<std::pair<std::string, std::string>> fgStates{
        {"MachineFG", "Off"},
        {"MachineFG", "Running"}};

    std::map<std::string, std::string> initStates{
        {"MachineFG", "Off"}};

    // 6) Create EM-side servers ---------------------------------------------
    ara::exec::ExecutionServer execServer{&execRpc};
    ara::exec::StateServer stateServer{&stateRpc,
                                       std::move(fgStates),
                                       std::move(initStates)};

    // 7) Create ExecutionManager --------------------------------------------
    ara::exec::ExecutionManager em{execServer, stateServer};

    // 8) Register managed processes ----------------------------------------
    {
        ara::exec::ProcessDescriptor sensorDesc;
        sensorDesc.name          = "SensorApp";
        sensorDesc.executable    = cSensorBin;
        sensorDesc.functionGroup = "MachineFG";
        sensorDesc.activeState   = "Running";
        sensorDesc.startupGrace  = std::chrono::milliseconds{3000};
        sensorDesc.terminationTimeout = std::chrono::milliseconds{5000};

        auto result{em.RegisterProcess(sensorDesc)};
        if (!result.HasValue())
        {
            logWarn("Failed to register SensorApp: " +
                    std::string{result.Error().Message()});
        }
        else
        {
            logInfo("Registered SensorApp -> MachineFG/Running");
        }
    }

    {
        ara::exec::ProcessDescriptor actuatorDesc;
        actuatorDesc.name          = "ActuatorApp";
        actuatorDesc.executable    = cActuatorBin;
        actuatorDesc.functionGroup = "MachineFG";
        actuatorDesc.activeState   = "Running";
        actuatorDesc.startupGrace  = std::chrono::milliseconds{3000};
        actuatorDesc.terminationTimeout = std::chrono::milliseconds{5000};

        auto result{em.RegisterProcess(actuatorDesc)};
        if (!result.HasValue())
        {
            logWarn("Failed to register ActuatorApp: " +
                    std::string{result.Error().Message()});
        }
        else
        {
            logInfo("Registered ActuatorApp -> MachineFG/Running");
        }
    }

    // 9) Attach process-state-change callback -------------------------------
    em.SetProcessStateChangeHandler(
        [&](const std::string &name, ara::exec::ManagedProcessState state)
        {
            std::ostringstream oss;
            oss << "Process state changed: " << name
                << " -> " << StateToString(state);
            logInfo(oss.str());
        });

    // 10) Start the EM background monitor thread ----------------------------
    auto startResult{em.Start()};
    if (!startResult.HasValue())
    {
        logWarn("ExecutionManager::Start failed: " +
                std::string{startResult.Error().Message()});
        (void)ara::core::Deinitialize();
        return 1;
    }
    logInfo("ExecutionManager started.");

    // 11) Register signal handler for graceful shutdown ---------------------
    ara::exec::SignalHandler::Register();

    // 12) Activate MachineFG → "Running" (launches sensor + actuator) -------
    logInfo("Activating MachineFG -> Running ...");
    auto activateResult{em.ActivateFunctionGroup("MachineFG", "Running")};
    if (!activateResult.HasValue())
    {
        logWarn("ActivateFunctionGroup failed: " +
                std::string{activateResult.Error().Message()});
    }

    // 13) Monitoring loop ---------------------------------------------------
    logInfo("EM daemon running. Press Ctrl+C or send SIGTERM to stop.");

    const auto cStartTime{std::chrono::steady_clock::now()};
    const auto cRunDuration{
        cRunSeconds > 0U
            ? std::chrono::seconds{cRunSeconds}
            : std::chrono::seconds{3600 * 24}  // effectively forever
    };

    std::uint32_t statusTick{0U};
    while (!ara::exec::SignalHandler::IsTerminationRequested())
    {
        std::this_thread::sleep_for(std::chrono::milliseconds{500});
        ++statusTick;

        // Print process status every 5 seconds
        if ((statusTick % 10U) == 0U)
        {
            const auto statuses{em.GetAllProcessStatuses()};
            for (const auto &s : statuses)
            {
                std::ostringstream oss;
                oss << "  STATUS  " << s.descriptor.name
                    << "  fg=" << s.descriptor.functionGroup
                    << "/" << s.descriptor.activeState
                    << "  pid=" << s.pid
                    << "  managed=" << StateToString(s.managedState);
                logInfo(oss.str());
            }
        }

        // Auto-shutdown after EM_RUN_SECONDS
        const auto elapsed{std::chrono::steady_clock::now() - cStartTime};
        if (elapsed >= cRunDuration)
        {
            logInfo("Auto-shutdown: run_seconds limit reached.");
            break;
        }
    }

    // 14) Graceful shutdown -------------------------------------------------
    logInfo("Terminating MachineFG -> Off ...");
    auto termResult{em.TerminateFunctionGroup("MachineFG")};
    if (!termResult.HasValue())
    {
        logWarn("TerminateFunctionGroup failed: " +
                std::string{termResult.Error().Message()});
    }

    // Brief wait for processes to be confirmed terminated
    std::this_thread::sleep_for(std::chrono::milliseconds{1000});

    // Print final statuses
    for (const auto &s : em.GetAllProcessStatuses())
    {
        std::ostringstream oss;
        oss << "  FINAL  " << s.descriptor.name
            << "  managed=" << StateToString(s.managedState)
            << "  pid=" << s.pid;
        logInfo(oss.str());
    }

    em.Stop();
    logInfo("ExecutionManager stopped. Shutdown complete.");

    // 15) Deinitialize runtime ----------------------------------------------
    (void)ara::core::Deinitialize();
    return 0;
}
