/// @file src/application/platform/execution_management.h
/// @brief Declarations for execution management.
/// @details This file is part of the Adaptive AUTOSAR educational implementation.

#ifndef EXECUTION_MANAGEMENT_H
#define EXECUTION_MANAGEMENT_H

#include <set>
#include "../../ara/exec/state_server.h"
#include "../helper/fifo_checkpoint_communicator.h"
#include "../extended_vehicle.h"
#include "./state_management.h"
#include "./platform_health_management.h"
#include "./diagnostic_manager.h"

/// @brief AUTOSAR application namespace
namespace application
{
    /// @brief AUTOSAR platform application namespace
    namespace platform
    {
        /// @brief Execution managment modelled process
        class ExecutionManagement : public ara::exec::helper::ModelledProcess
        {
        private:
            static const std::string cAppId;
            static const std::string cFifoPath;
            const std::string cMachineFunctionGroup{"MachineFG"};

            helper::FifoCheckpointCommunicator mCommunicator;
            StateManagement mStateManagement;
            PlatformHealthManagement mPlatformHealthManager;
            ExtendedVehicle mExtendedVehicle;
            DiagnosticManager mDiagnosticManager;
            ara::exec::StateServer *mStateServer;

            /// @brief Reads RPC server transport parameters from ARXML.
            /// @param configFilepath Configuration file path.
            /// @returns RPC configuration structure.
            static helper::RpcConfiguration getRpcConfiguration(
                const std::string &configFilepath);

            /// @brief Collects all declared states of one function group.
            /// @param functionGroupShortName Function group short name.
            /// @param functionGroupContent XML fragment for function group.
            /// @param functionGroupStates Output set of `(group, state)` pairs.
            static void fillFunctionGroupStates(
                std::string functionGroupShortName,
                const std::string &functionGroupContent,
                std::set<std::pair<std::string, std::string>> &functionGroupStates);

            /// @brief Extracts initial state for one function group.
            /// @param functionGroupShortName Function group short name.
            /// @param functionGroupContent XML fragment for function group.
            /// @param initialStates Output map of initial states.
            static void fillInitialStates(
                std::string functionGroupShortName,
                const std::string &functionGroupContent,
                std::map<std::string, std::string> &initialStates);

            /// @brief Parses all function groups and their state tables from ARXML.
            /// @param configFilepath Configuration file path.
            /// @param functionGroupStates Output set of `(group, state)` pairs.
            /// @param initialStates Output map of initial states.
            static void fillStates(
                const std::string &configFilepath,
                std::set<std::pair<std::string, std::string>> &functionGroupStates,
                std::map<std::string, std::string> &initialStates);

            /// @brief Starts subordinate platform processes on startup state transition.
            /// @param arguments Runtime argument map.
            void onStateChange(
                const std::map<std::string, std::string> &arguments);

        protected:
            /// @brief Main execution loop of execution management process.
            /// @param cancellationToken Cooperative stop token.
            /// @param arguments Runtime argument map.
            /// @returns Process exit code.
            int Main(
                const std::atomic_bool *cancellationToken,
                const std::map<std::string, std::string> &arguments) override;

        public:
            /// @brief Constructor
            /// @param poller Global poller for network communication
            explicit ExecutionManagement(AsyncBsdSocketLib::Poller *poller);

            /// @brief Destructor.
            ~ExecutionManagement() override;
        };
    }
}

#endif
