/// @file src/main_sm_state_daemon.cpp
/// @brief Resident daemon that manages AUTOSAR SM machine lifecycle state
///        and network communication modes.

#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <fstream>
#include <string>
#include <thread>

#include <sys/stat.h>
#include <sys/types.h>

#include "./ara/sm/machine_state_client.h"
#include "./ara/sm/network_handle.h"
#include "./ara/sm/state_transition_handler.h"
#include "./ara/core/instance_specifier.h"

namespace
{
    std::atomic_bool gRunning{true};

    void RequestStop(int) noexcept
    {
        gRunning = false;
    }

    std::string GetEnvOrDefault(const char *key, std::string fallback)
    {
        const char *value{std::getenv(key)};
        if (value != nullptr)
        {
            return value;
        }

        return fallback;
    }

    std::uint32_t GetEnvU32(const char *key, std::uint32_t fallback)
    {
        const char *value{std::getenv(key)};
        if (value == nullptr)
        {
            return fallback;
        }

        try
        {
            const std::uint64_t parsed{std::stoull(value)};
            if (parsed == 0U || parsed > 600000U)
            {
                return fallback;
            }
            return static_cast<std::uint32_t>(parsed);
        }
        catch (...)
        {
            return fallback;
        }
    }

    void EnsureRunDirectory()
    {
        ::mkdir("/run/autosar", 0755);
    }

    const char *MachineStateToString(ara::sm::MachineState state)
    {
        switch (state)
        {
        case ara::sm::MachineState::kStartup:
            return "Startup";
        case ara::sm::MachineState::kRunning:
            return "Running";
        case ara::sm::MachineState::kShutdown:
            return "Shutdown";
        case ara::sm::MachineState::kRestart:
            return "Restart";
        case ara::sm::MachineState::kSuspend:
            return "Suspend";
        default:
            return "Unknown";
        }
    }

    const char *ComModeToString(ara::sm::ComMode mode)
    {
        switch (mode)
        {
        case ara::sm::ComMode::kFull:
            return "Full";
        case ara::sm::ComMode::kSilent:
            return "Silent";
        case ara::sm::ComMode::kNone:
            return "None";
        default:
            return "Unknown";
        }
    }

    void WriteStatusFile(
        const std::string &statusFile,
        const ara::sm::MachineStateClient &machineState,
        const ara::sm::NetworkHandle &networkHandle,
        const ara::sm::StateTransitionHandler &transitionHandler)
    {
        std::ofstream stream(statusFile);
        if (!stream.is_open())
        {
            return;
        }

        const auto machineResult{machineState.GetMachineState()};
        if (machineResult.HasValue())
        {
            stream << "machine_state="
                   << MachineStateToString(machineResult.Value()) << "\n";
        }
        else
        {
            stream << "machine_state=unavailable\n";
        }

        const auto comResult{networkHandle.GetCurrentComMode()};
        if (comResult.HasValue())
        {
            stream << "com_mode="
                   << ComModeToString(comResult.Value()) << "\n";
        }
        else
        {
            stream << "com_mode=unavailable\n";
        }

        stream << "transition_handler_active="
               << (transitionHandler.HasHandler("MachineState") ? "true"
                                                                : "false")
               << "\n";

        const auto nowMs{
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch())
                .count()};
        stream << "updated_epoch_ms=" << nowMs << "\n";
    }
}

int main()
{
    std::signal(SIGINT, RequestStop);
    std::signal(SIGTERM, RequestStop);

    const std::uint32_t periodMs{
        GetEnvU32("AUTOSAR_SM_PERIOD_MS", 1000U)};
    const std::string statusFile{
        GetEnvOrDefault(
            "AUTOSAR_SM_STATUS_FILE",
            "/run/autosar/sm_state.status")};
    const std::string networkInstance{
        GetEnvOrDefault(
            "AUTOSAR_SM_NETWORK_INSTANCE",
            "AdaptiveAutosar/SM/DefaultNetwork")};

    EnsureRunDirectory();

    ara::sm::MachineStateClient machineState;
    ara::core::InstanceSpecifier netSpec{networkInstance};
    ara::sm::NetworkHandle networkHandle{netSpec};
    ara::sm::StateTransitionHandler transitionHandler;

    // Transition to Running state.
    machineState.SetMachineState(ara::sm::MachineState::kRunning);

    // Request full communication mode.
    networkHandle.RequestComMode(ara::sm::ComMode::kFull);

    // Register a default transition handler for MachineState function group.
    transitionHandler.Register(
        "MachineState",
        [](const std::string &, const std::string &,
           const std::string &, ara::sm::TransitionPhase) {
            // Default handler: no-op for status tracking.
        });

    while (gRunning.load())
    {
        WriteStatusFile(statusFile, machineState, networkHandle,
                        transitionHandler);

        const std::uint32_t sleepStepMs{100U};
        std::uint32_t sleptMs{0U};
        while (gRunning.load() && sleptMs < periodMs)
        {
            std::this_thread::sleep_for(
                std::chrono::milliseconds(sleepStepMs));
            sleptMs += sleepStepMs;
        }
    }

    // Graceful shutdown: transition machine state.
    machineState.SetMachineState(ara::sm::MachineState::kShutdown);
    networkHandle.RequestComMode(ara::sm::ComMode::kNone);

    WriteStatusFile(statusFile, machineState, networkHandle,
                    transitionHandler);

    return 0;
}
