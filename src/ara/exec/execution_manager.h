/// @file src/ara/exec/execution_manager.h
/// @brief ExecutionManager — central process/function-group orchestrator.
/// @details Bridges ExecutionServer (process state reports) and StateServer
///          (function group state transitions). Manages the lifecycle of
///          registered Adaptive Application processes on the host.
///
///          Typical usage (EM daemon startup):
///          @code
///          ara::exec::ExecutionManager em(execServer, stateServer);
///          em.RegisterProcess({
///              "MyApp",
///              "/opt/autosar-ap/bin/my_app",
///              "MachineFG", "Running",
///              {}, 3s, 5s
///          });
///          em.ActivateFunctionGroup("MachineFG", "Running");
///          // ...
///          em.TerminateFunctionGroup("MachineFG");
///          @endcode
///
///          This file is part of the Adaptive AUTOSAR educational implementation.

#ifndef ARA_EXEC_EXECUTION_MANAGER_H
#define ARA_EXEC_EXECUTION_MANAGER_H

#include <atomic>
#include <chrono>
#include <functional>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <vector>
#include "../core/result.h"
#include "./execution_client.h"
#include "./execution_server.h"
#include "./state_server.h"

namespace ara
{
    namespace exec
    {
        /// @brief State of a process as tracked by the ExecutionManager.
        enum class ManagedProcessState : std::uint8_t
        {
            kNotRunning = 0,  ///< Process has not been started.
            kStarting = 1,    ///< Process launched; awaiting kRunning report.
            kRunning = 2,     ///< Process confirmed running via ExecutionClient.
            kTerminating = 3, ///< SIGTERM sent; awaiting kTerminating report.
            kTerminated = 4,  ///< Process has exited cleanly.
            kFailed = 5       ///< Process terminated unexpectedly.
        };

        /// @brief Manifest entry describing one managed process.
        struct ProcessDescriptor
        {
            /// @brief Unique process name (used as ExecutionClient instance ID).
            std::string name;
            /// @brief Absolute path to the executable.
            std::string executable;
            /// @brief Name of the FunctionGroup this process belongs to.
            std::string functionGroup;
            /// @brief FunctionGroup state in which this process should be active.
            std::string activeState;
            /// @brief Command-line arguments forwarded to the process.
            std::vector<std::string> arguments;
            /// @brief Grace period before startup is considered failed.
            std::chrono::milliseconds startupGrace{3000};
            /// @brief Time to wait for kTerminating after SIGTERM before SIGKILL.
            std::chrono::milliseconds terminationTimeout{5000};
        };

        /// @brief Runtime status record for one managed process.
        struct ProcessStatus
        {
            ProcessDescriptor descriptor;
            ManagedProcessState managedState;
            int pid; ///< OS process ID, or -1 if not running.
            ExecutionState reportedState; ///< Last state reported by ExecutionClient.
        };

        /// @brief Central orchestrator for Adaptive Application process lifecycle.
        ///
        /// Thread-safe. Operates as the EM-side counterpart to
        /// ExecutionClient/StateClient. On Linux, processes are launched via
        /// fork()/exec(). A background monitor thread reaps exited children.
        class ExecutionManager
        {
        public:
            /// @brief Callback type invoked on process state change.
            using ProcessStateChangeHandler =
                std::function<void(const std::string &processName,
                                   ManagedProcessState state)>;

            /// @brief Construct the ExecutionManager.
            /// @param executionServer EM-side server that receives kRunning/kTerminating reports.
            /// @param stateServer     EM-side server that handles function group transitions.
            ExecutionManager(
                ExecutionServer &executionServer,
                StateServer &stateServer);

            ~ExecutionManager() noexcept;

            ExecutionManager(const ExecutionManager &) = delete;
            ExecutionManager &operator=(const ExecutionManager &) = delete;

            /// @brief Register a process descriptor.
            /// @param descriptor Manifest entry for the process.
            /// @returns Ok, or kInvalidArguments if the name is empty / already registered.
            core::Result<void> RegisterProcess(ProcessDescriptor descriptor);

            /// @brief Unregister a process.
            /// @param processName Process name as given in ProcessDescriptor::name.
            /// @returns Ok, or kFailed if the process is currently running.
            core::Result<void> UnregisterProcess(const std::string &processName);

            /// @brief Activate a function group to the given state.
            /// @details Starts all processes whose (functionGroup, activeState) matches,
            ///          and stops processes that are active in the *previous* state.
            /// @param functionGroup Function group name.
            /// @param state         Target state name.
            /// @returns Ok, or kFailed / kInvalidArguments on error.
            core::Result<void> ActivateFunctionGroup(
                const std::string &functionGroup,
                const std::string &state);

            /// @brief Terminate all processes belonging to a function group.
            /// @param functionGroup Function group name.
            /// @returns Ok, or kFailed if no such function group is known.
            core::Result<void> TerminateFunctionGroup(
                const std::string &functionGroup);

            /// @brief Start all registered processes and enter the initial state.
            /// @returns Ok, or kFailed if the manager is already started.
            core::Result<void> Start();

            /// @brief Gracefully stop all running processes and join the monitor thread.
            void Stop();

            /// @brief Get the status of a single process.
            /// @param processName Process name.
            /// @returns ProcessStatus, or kFailed if the name is unknown.
            core::Result<ProcessStatus> GetProcessStatus(
                const std::string &processName) const;

            /// @brief Get a snapshot of all process statuses.
            std::vector<ProcessStatus> GetAllProcessStatuses() const;

            /// @brief Set an optional callback invoked on every managed-state change.
            /// @param handler Callback or nullptr to clear.
            void SetProcessStateChangeHandler(ProcessStateChangeHandler handler);

            /// @brief Query whether the manager is running.
            bool IsRunning() const noexcept;

        private:
            mutable std::mutex mMutex;
            ExecutionServer &mExecutionServer;
            StateServer &mStateServer;

            struct ManagedProcess
            {
                ProcessDescriptor descriptor;
                ManagedProcessState managedState{ManagedProcessState::kNotRunning};
                int pid{-1};
                ExecutionState reportedState{ExecutionState::kIdle};
            };

            std::map<std::string, ManagedProcess> mProcesses;
            /// @brief Active state per function group (name → state name).
            std::map<std::string, std::string> mFunctionGroupStates;

            std::atomic<bool> mRunning{false};
            std::thread mMonitorThread;
            ProcessStateChangeHandler mStateChangeHandler;

            /// @brief Launch a single process via fork()/exec().
            /// @returns OS pid on success, or -1 on error.
            int launchProcess(ManagedProcess &proc);

            /// @brief Send SIGTERM to a process and wait for it to exit.
            void terminateProcess(ManagedProcess &proc);

            /// @brief Background monitor: reap exited children, update states.
            void monitorLoop();

            /// @brief Notify handler under lock.
            void notifyStateChange(
                const std::string &name,
                ManagedProcessState state);

            /// @brief Synchronise reported ExecutionState into managedState.
            void syncExecutionStates();
        };

    } // namespace exec
} // namespace ara

#endif // ARA_EXEC_EXECUTION_MANAGER_H
