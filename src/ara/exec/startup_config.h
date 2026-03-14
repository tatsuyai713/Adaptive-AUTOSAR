/// @file src/ara/exec/startup_config.h
/// @brief Startup configuration and state mapping types per AUTOSAR AP SWS_EM.
/// @details Provides StartupConfig, ProcessToMachineStateMapping, and
///          SchedulingPolicy for configuring application process startup.

#ifndef ARA_EXEC_STARTUP_CONFIG_H
#define ARA_EXEC_STARTUP_CONFIG_H

#include <cstdint>
#include <string>
#include <vector>

namespace ara
{
    namespace exec
    {
        /// @brief Scheduling policy for a process (SWS_EM_02050).
        enum class SchedulingPolicy : std::uint8_t
        {
            kDefault = 0,   ///< OS-default scheduling
            kFifo = 1,      ///< SCHED_FIFO real-time scheduling
            kRoundRobin = 2 ///< SCHED_RR real-time scheduling
        };

        /// @brief Startup configuration for an application process (SWS_EM_02051).
        struct StartupConfig
        {
            std::string processName;              ///< Process name identifier
            std::string executablePath;            ///< Path to the executable binary
            SchedulingPolicy schedulingPolicy{SchedulingPolicy::kDefault};
            std::uint32_t priority{0};             ///< Scheduling priority
            std::uint32_t startupTimeoutMs{5000};  ///< Max time for startup (ms)
            std::string resourceGroup;             ///< Resource group identifier
            std::vector<std::string> dependencies; ///< Processes that must start first
            std::vector<std::string> arguments;    ///< Command-line arguments
        };

        /// @brief Mapping of processes to machine/function-group states (SWS_EM_02052).
        struct ProcessToMachineStateMapping
        {
            std::string processName;       ///< Process name identifier
            std::string functionGroup;     ///< Function group name
            std::string activeState;       ///< State in which the process should run
            bool startAutomatically{true}; ///< Start when the state becomes active
        };

    } // namespace exec
} // namespace ara

#endif // ARA_EXEC_STARTUP_CONFIG_H
