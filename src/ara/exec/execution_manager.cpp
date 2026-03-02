/// @file src/ara/exec/execution_manager.cpp
/// @brief ExecutionManager implementation.
/// @details This file is part of the Adaptive AUTOSAR educational implementation.

#include "./execution_manager.h"

#include <csignal>
#include <cstring>
#include <stdexcept>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

namespace ara
{
    namespace exec
    {
        // -----------------------------------------------------------------------
        // Constructor / Destructor
        // -----------------------------------------------------------------------

        ExecutionManager::ExecutionManager(
            ExecutionServer &executionServer,
            StateServer &stateServer)
            : mExecutionServer{executionServer}
            , mStateServer{stateServer}
        {
        }

        ExecutionManager::~ExecutionManager() noexcept
        {
            Stop();
        }

        // -----------------------------------------------------------------------
        // Public API
        // -----------------------------------------------------------------------

        core::Result<void> ExecutionManager::RegisterProcess(ProcessDescriptor descriptor)
        {
            if (descriptor.name.empty())
            {
                return core::Result<void>::FromError(
                    MakeErrorCode(ExecErrc::kInvalidArguments));
            }

            std::lock_guard<std::mutex> _lock{mMutex};

            if (mProcesses.count(descriptor.name) != 0U)
            {
                return core::Result<void>::FromError(
                    MakeErrorCode(ExecErrc::kInvalidArguments));
            }

            ManagedProcess _mp;
            _mp.descriptor = std::move(descriptor);
            mProcesses.emplace(_mp.descriptor.name, std::move(_mp));
            return core::Result<void>::FromValue();
        }

        core::Result<void> ExecutionManager::UnregisterProcess(
            const std::string &processName)
        {
            std::lock_guard<std::mutex> _lock{mMutex};

            auto _it{mProcesses.find(processName)};
            if (_it == mProcesses.end())
            {
                return core::Result<void>::FromError(
                    MakeErrorCode(ExecErrc::kFailed));
            }

            if (_it->second.managedState == ManagedProcessState::kRunning ||
                _it->second.managedState == ManagedProcessState::kStarting ||
                _it->second.managedState == ManagedProcessState::kTerminating)
            {
                return core::Result<void>::FromError(
                    MakeErrorCode(ExecErrc::kFailed));
            }

            mProcesses.erase(_it);
            return core::Result<void>::FromValue();
        }

        core::Result<void> ExecutionManager::ActivateFunctionGroup(
            const std::string &functionGroup,
            const std::string &state)
        {
            if (functionGroup.empty() || state.empty())
            {
                return core::Result<void>::FromError(
                    MakeErrorCode(ExecErrc::kInvalidArguments));
            }

            std::lock_guard<std::mutex> _lock{mMutex};

            // Determine previous state (if any)
            const std::string cPreviousState = [&]() -> std::string
            {
                auto _fg{mFunctionGroupStates.find(functionGroup)};
                return (_fg != mFunctionGroupStates.end()) ? _fg->second : std::string{};
            }();

            // Stop processes that belong to functionGroup but are NOT in the new state
            for (auto &_pair : mProcesses)
            {
                ManagedProcess &_mp{_pair.second};
                if (_mp.descriptor.functionGroup != functionGroup)
                {
                    continue;
                }
                if (_mp.descriptor.activeState == state)
                {
                    continue; // Should be (or remain) running
                }
                // Terminate processes that belong to old state
                if (_mp.managedState == ManagedProcessState::kRunning ||
                    _mp.managedState == ManagedProcessState::kStarting)
                {
                    terminateProcess(_mp);
                }
            }

            // Start processes that belong to the new state
            for (auto &_pair : mProcesses)
            {
                ManagedProcess &_mp{_pair.second};
                if (_mp.descriptor.functionGroup != functionGroup ||
                    _mp.descriptor.activeState != state)
                {
                    continue;
                }
                if (_mp.managedState == ManagedProcessState::kNotRunning ||
                    _mp.managedState == ManagedProcessState::kTerminated ||
                    _mp.managedState == ManagedProcessState::kFailed)
                {
                    const int _pid{launchProcess(_mp)};
                    if (_pid > 0)
                    {
                        _mp.pid = _pid;
                        _mp.managedState = ManagedProcessState::kStarting;
                        notifyStateChange(_mp.descriptor.name,
                                          ManagedProcessState::kStarting);
                    }
                    else
                    {
                        _mp.managedState = ManagedProcessState::kFailed;
                        notifyStateChange(_mp.descriptor.name,
                                          ManagedProcessState::kFailed);
                    }
                }
            }

            mFunctionGroupStates[functionGroup] = state;
            return core::Result<void>::FromValue();
        }

        core::Result<void> ExecutionManager::TerminateFunctionGroup(
            const std::string &functionGroup)
        {
            if (functionGroup.empty())
            {
                return core::Result<void>::FromError(
                    MakeErrorCode(ExecErrc::kInvalidArguments));
            }

            std::lock_guard<std::mutex> _lock{mMutex};

            bool _found{false};
            for (auto &_pair : mProcesses)
            {
                ManagedProcess &_mp{_pair.second};
                if (_mp.descriptor.functionGroup != functionGroup)
                {
                    continue;
                }
                _found = true;
                if (_mp.managedState == ManagedProcessState::kRunning ||
                    _mp.managedState == ManagedProcessState::kStarting)
                {
                    terminateProcess(_mp);
                }
            }

            if (!_found)
            {
                return core::Result<void>::FromError(
                    MakeErrorCode(ExecErrc::kFailed));
            }

            mFunctionGroupStates.erase(functionGroup);
            return core::Result<void>::FromValue();
        }

        core::Result<void> ExecutionManager::Start()
        {
            if (mRunning.exchange(true))
            {
                return core::Result<void>::FromError(
                    MakeErrorCode(ExecErrc::kAlreadyInState));
            }

            mMonitorThread = std::thread{[this]
                                         { monitorLoop(); }};
            return core::Result<void>::FromValue();
        }

        void ExecutionManager::Stop()
        {
            if (!mRunning.exchange(false))
            {
                return; // Not running
            }

            // Terminate all running processes
            {
                std::lock_guard<std::mutex> _lock{mMutex};
                for (auto &_pair : mProcesses)
                {
                    ManagedProcess &_mp{_pair.second};
                    if (_mp.managedState == ManagedProcessState::kRunning ||
                        _mp.managedState == ManagedProcessState::kStarting)
                    {
                        terminateProcess(_mp);
                    }
                }
            }

            if (mMonitorThread.joinable())
            {
                mMonitorThread.join();
            }
        }

        core::Result<ProcessStatus> ExecutionManager::GetProcessStatus(
            const std::string &processName) const
        {
            std::lock_guard<std::mutex> _lock{mMutex};

            auto _it{mProcesses.find(processName)};
            if (_it == mProcesses.end())
            {
                return core::Result<ProcessStatus>::FromError(
                    MakeErrorCode(ExecErrc::kFailed));
            }

            const ManagedProcess &_mp{_it->second};
            ProcessStatus _status;
            _status.descriptor = _mp.descriptor;
            _status.managedState = _mp.managedState;
            _status.pid = _mp.pid;
            _status.reportedState = _mp.reportedState;
            return core::Result<ProcessStatus>::FromValue(_status);
        }

        std::vector<ProcessStatus> ExecutionManager::GetAllProcessStatuses() const
        {
            std::lock_guard<std::mutex> _lock{mMutex};

            std::vector<ProcessStatus> _statuses;
            _statuses.reserve(mProcesses.size());
            for (const auto &_pair : mProcesses)
            {
                const ManagedProcess &_mp{_pair.second};
                ProcessStatus _status;
                _status.descriptor = _mp.descriptor;
                _status.managedState = _mp.managedState;
                _status.pid = _mp.pid;
                _status.reportedState = _mp.reportedState;
                _statuses.push_back(std::move(_status));
            }
            return _statuses;
        }

        void ExecutionManager::SetProcessStateChangeHandler(
            ProcessStateChangeHandler handler)
        {
            std::lock_guard<std::mutex> _lock{mMutex};
            mStateChangeHandler = std::move(handler);
        }

        bool ExecutionManager::IsRunning() const noexcept
        {
            return mRunning.load();
        }

        // -----------------------------------------------------------------------
        // Private helpers
        // -----------------------------------------------------------------------

        int ExecutionManager::launchProcess(ManagedProcess &proc)
        {
            const pid_t _pid{::fork()};
            if (_pid < 0)
            {
                // fork() failed
                return -1;
            }

            if (_pid == 0)
            {
                // Child process
                const std::string &cExe{proc.descriptor.executable};
                // Build argv: argv[0] = exe path, then arguments, then nullptr
                std::vector<const char *> _argv;
                _argv.push_back(cExe.c_str());
                for (const auto &_arg : proc.descriptor.arguments)
                {
                    _argv.push_back(_arg.c_str());
                }
                _argv.push_back(nullptr);

                // Replace child image
                // NOLINTNEXTLINE: execv is the portable Linux way
                ::execv(cExe.c_str(), const_cast<char *const *>(_argv.data()));

                // execv() only returns on error
                ::_exit(1);
            }

            // Parent: return child PID
            return static_cast<int>(_pid);
        }

        void ExecutionManager::terminateProcess(ManagedProcess &proc)
        {
            if (proc.pid <= 0)
            {
                proc.managedState = ManagedProcessState::kTerminated;
                return;
            }

            // Send SIGTERM
            ::kill(static_cast<pid_t>(proc.pid), SIGTERM);
            proc.managedState = ManagedProcessState::kTerminating;
            notifyStateChange(proc.descriptor.name,
                              ManagedProcessState::kTerminating);

            // Wait up to terminationTimeout for the child to exit
            const auto cDeadline{
                std::chrono::steady_clock::now() + proc.descriptor.terminationTimeout};

            while (std::chrono::steady_clock::now() < cDeadline)
            {
                int _status{0};
                const pid_t _ret{::waitpid(static_cast<pid_t>(proc.pid),
                                            &_status, WNOHANG)};
                if (_ret == static_cast<pid_t>(proc.pid))
                {
                    proc.pid = -1;
                    proc.managedState = ManagedProcessState::kTerminated;
                    notifyStateChange(proc.descriptor.name,
                                      ManagedProcessState::kTerminated);
                    return;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            }

            // Timeout: send SIGKILL
            ::kill(static_cast<pid_t>(proc.pid), SIGKILL);
            ::waitpid(static_cast<pid_t>(proc.pid), nullptr, 0);
            proc.pid = -1;
            proc.managedState = ManagedProcessState::kTerminated;
            notifyStateChange(proc.descriptor.name,
                              ManagedProcessState::kTerminated);
        }

        void ExecutionManager::monitorLoop()
        {
            while (mRunning.load())
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(200));

                std::lock_guard<std::mutex> _lock{mMutex};
                syncExecutionStates();

                // Reap any exited children (non-blocking)
                for (auto &_pair : mProcesses)
                {
                    ManagedProcess &_mp{_pair.second};
                    if (_mp.pid <= 0)
                    {
                        continue;
                    }
                    int _status{0};
                    const pid_t _ret{
                        ::waitpid(static_cast<pid_t>(_mp.pid), &_status, WNOHANG)};
                    if (_ret == static_cast<pid_t>(_mp.pid))
                    {
                        _mp.pid = -1;
                        const bool cClean{WIFEXITED(_status) &&
                                          WEXITSTATUS(_status) == 0};
                        if (_mp.managedState == ManagedProcessState::kTerminating ||
                            cClean)
                        {
                            _mp.managedState = ManagedProcessState::kTerminated;
                            notifyStateChange(_mp.descriptor.name,
                                              ManagedProcessState::kTerminated);
                        }
                        else
                        {
                            _mp.managedState = ManagedProcessState::kFailed;
                            notifyStateChange(_mp.descriptor.name,
                                              ManagedProcessState::kFailed);
                        }
                    }
                }
            }
        }

        void ExecutionManager::notifyStateChange(
            const std::string &name,
            ManagedProcessState state)
        {
            if (mStateChangeHandler)
            {
                mStateChangeHandler(name, state);
            }
        }

        void ExecutionManager::syncExecutionStates()
        {
            // Pull reported execution states from ExecutionServer into local records
            const auto cSnapshot{mExecutionServer.GetExecutionStatesSnapshot()};
            for (auto &_pair : mProcesses)
            {
                ManagedProcess &_mp{_pair.second};
                auto _it{cSnapshot.find(_mp.descriptor.name)};
                if (_it == cSnapshot.end())
                {
                    continue;
                }
                const ExecutionState cNewReported{_it->second};
                if (cNewReported == _mp.reportedState)
                {
                    continue;
                }
                _mp.reportedState = cNewReported;

                // Promote kStarting → kRunning when process reports kRunning
                if (_mp.managedState == ManagedProcessState::kStarting &&
                    cNewReported == ExecutionState::kRunning)
                {
                    _mp.managedState = ManagedProcessState::kRunning;
                    notifyStateChange(_mp.descriptor.name,
                                      ManagedProcessState::kRunning);
                }
            }
        }

    } // namespace exec
} // namespace ara
