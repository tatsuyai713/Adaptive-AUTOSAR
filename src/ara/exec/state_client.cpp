/// @file src/ara/exec/state_client.cpp
/// @brief Implementation for state client.
/// @details This file is part of the Adaptive AUTOSAR educational implementation.

#include "./state_client.h"
#include <chrono>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <string>
#include <thread>
#if defined(__linux__) || defined(__QNX__)
#include <dirent.h>
#endif
#if defined(__QNX__)
#include <fcntl.h>
#include <sys/procfs.h>
#include <devctl.h>
#include <unistd.h>
#include <cstring>
#endif

namespace ara
{
    namespace exec
    {
        const ExecErrorDomain StateClient::cErrorDomain;

        StateClient::StateClient(
            std::function<void(const ExecutionErrorEvent &)> undefinedStateCallback,
            com::someip::rpc::RpcClient *rpcClient) : mRpcClient{rpcClient}
        {
            if (mRpcClient == nullptr)
            {
                throw std::invalid_argument("RPC client cannot be null.");
            }

            // Wrap the user callback to also track error events internally
            mUndefinedStateCallback = [this, undefinedStateCallback](const ExecutionErrorEvent &event)
            {
                if (event.functionGroup)
                {
                    const std::string _key{event.functionGroup->GetInstance().ToString()};
                    const std::lock_guard<std::mutex> _lock{mErrorsMutex};
                    mExecutionErrors[_key] = event;
                }

                if (undefinedStateCallback)
                {
                    undefinedStateCallback(event);
                }
            };
            auto _setStateHandler{
                std::bind(
                    &StateClient::setStateHandler,
                    this, std::placeholders::_1)};
            mRpcClient->SetHandler(
                cServiceId, cSetStateId, _setStateHandler);

            auto _stateTransitionHandler{
                std::bind(
                    &StateClient::stateTransitionHandler,
                    this, std::placeholders::_1)};
            mRpcClient->SetHandler(
                cServiceId, cStateTransition, _stateTransitionHandler);
        }

        void StateClient::setPromiseException(
            std::promise<void> &promise, ExecErrc executionErrorCode)
        {
            auto _errorValue{static_cast<core::ErrorDomain::CodeType>(executionErrorCode)};
            core::ErrorCode _errorCode{_errorValue, cErrorDomain};
            ExecException _exception{_errorCode};
            auto _exceptionPtr = std::make_exception_ptr(_exception);
            promise.set_exception(_exceptionPtr);
        }

        void StateClient::genericHandler(
            std::promise<void> &promise,
            const com::someip::rpc::SomeIpRpcMessage &message)
        {
            const std::vector<uint8_t> &cRpcPayload{message.RpcPayload()};

            if (cRpcPayload.empty())
            {
                promise.set_value();
            }
            else
            {
                try
                {
                    size_t _offset{0};
                    uint32_t _executionErrorCodeInt{
                        com::helper::ExtractInteger(cRpcPayload, _offset)};
                    auto _executionErrorCode{
                        static_cast<ExecErrc>(_executionErrorCodeInt)};

                    setPromiseException(promise, _executionErrorCode);
                }
                catch (std::out_of_range)
                {
                    const ExecErrc cCorruptedResponseCode{
                        ExecErrc::kCommunicationError};

                    setPromiseException(promise, cCorruptedResponseCode);
                }
            }
        }

        void StateClient::setStateHandler(
            const com::someip::rpc::SomeIpRpcMessage &message)
        {
            const std::lock_guard<std::recursive_mutex> _lock{
                mSetStateRequest.mutex};
            if (!mSetStateRequest.hasPendingRequest ||
                message.SessionId() != mSetStateRequest.pendingSessionId)
            {
                return;
            }

            mSetStateRequest.hasPendingRequest = false;
            genericHandler(mSetStateRequest.promise, message);
        }

        void StateClient::stateTransitionHandler(
            const com::someip::rpc::SomeIpRpcMessage &message)
        {
            const std::lock_guard<std::recursive_mutex> _lock{
                mStateTransitionRequest.mutex};
            if (!mStateTransitionRequest.hasPendingRequest ||
                message.SessionId() != mStateTransitionRequest.pendingSessionId)
            {
                return;
            }

            mStateTransitionRequest.hasPendingRequest = false;
            genericHandler(mStateTransitionRequest.promise, message);
        }

        void StateClient::advanceSessionId(uint16_t &sessionId) noexcept
        {
            const uint16_t cSessionIdMin{1};
            constexpr uint16_t cSessionIdMax{
                std::numeric_limits<uint16_t>::max()};
            sessionId =
                (sessionId == cSessionIdMax) ? cSessionIdMin : sessionId + 1;
        }

        std::shared_future<void> StateClient::getFuture(
            RequestState &request,
            uint16_t methodId,
            const std::vector<uint8_t> &rpcPayload)
        {
            try
            {
                std::shared_future<void> _result{request.promise.get_future()};
                request.pendingSessionId = request.nextSessionId;
                request.hasPendingRequest = true;
                mRpcClient->Send(cServiceId, methodId, cClientId, rpcPayload);
                advanceSessionId(request.nextSessionId);

                return _result;
            }
            catch (std::future_error)
            {
                const ExecErrc cAlreadyRequestedCode{ExecErrc::kFailed};
                std::promise<void> _promise;
                setPromiseException(_promise, cAlreadyRequestedCode);
                std::shared_future<void> _result{_promise.get_future()};

                return _result;
            }
        }

        std::shared_future<void> StateClient::SetState(
            const FunctionGroupState &state)
        {
            const std::lock_guard<std::recursive_mutex> _lock{
                mSetStateRequest.mutex};

            if (mSetStateRequest.future.valid())
            {
                try
                {
                    const ExecErrc cExecErrc{ExecErrc::kCancelled};
                    setPromiseException(mSetStateRequest.promise, cExecErrc);
                }
                catch (std::future_error)
                {
                    // Catch the exception if the promise is already set
                }

                mSetStateRequest.promise = std::promise<void>();
                mSetStateRequest.hasPendingRequest = false;
            }

            std::vector<uint8_t> _rpcPayload;
            state.Serialize(_rpcPayload);
            mSetStateRequest.future =
                getFuture(mSetStateRequest, cSetStateId, _rpcPayload);

            return mSetStateRequest.future;
        }

        std::shared_future<void> StateClient::GetInitialMachineStateTransitionResult()
        {
            const std::lock_guard<std::recursive_mutex> _lock{
                mStateTransitionRequest.mutex};

            if (mStateTransitionRequest.future.valid())
            {
                try
                {
                    const ExecErrc cExecErrc{ExecErrc::kCancelled};
                    setPromiseException(mStateTransitionRequest.promise, cExecErrc);
                }
                catch (std::future_error)
                {
                    // Catch the exception if the promise is already set
                }

                mStateTransitionRequest.promise = std::promise<void>();
                mStateTransitionRequest.hasPendingRequest = false;
            }

            const std::vector<uint8_t> cRpcPayload;
            mStateTransitionRequest.future =
                getFuture(mStateTransitionRequest, cStateTransition, cRpcPayload);

            return mStateTransitionRequest.future;
        }

        core::Result<FunctionGroupState> StateClient::GetState(
            const FunctionGroup &functionGroup) const noexcept
        {
            // Attempt to create a FunctionGroupState with the default/current state
            // In a full implementation, this would query EM via RPC for the actual state.
            const std::string cDefaultState{"Running"};
            auto _stateResult{FunctionGroupState::Create(functionGroup, cDefaultState)};
            if (_stateResult.HasValue())
            {
                return _stateResult;
            }

            const ExecErrc cExecErrc{ExecErrc::kFailed};
            auto _errorValue{static_cast<core::ErrorDomain::CodeType>(cExecErrc)};
            core::ErrorCode _errorCode{_errorValue, cErrorDomain};
            core::Result<FunctionGroupState> _result{_errorCode};
            return _result;
        }

        core::Result<ExecutionErrorEvent> StateClient::GetExecutionError(
            const FunctionGroup &functionGroup) noexcept
        {
            const std::string _key{functionGroup.GetInstance().ToString()};

            {
                const std::lock_guard<std::mutex> _lock{mErrorsMutex};
                auto _itr{mExecutionErrors.find(_key)};
                if (_itr != mExecutionErrors.end())
                {
                    core::Result<ExecutionErrorEvent> _result{_itr->second};
                    return _result;
                }
            }

            // No execution error tracked for this function group
            const ExecErrc cExecErrc{ExecErrc::kFailed};
            auto _errorValue{static_cast<core::ErrorDomain::CodeType>(cExecErrc)};
            core::ErrorCode _errorCode{_errorValue, cErrorDomain};
            core::Result<ExecutionErrorEvent> _result{_errorCode};

            return _result;
        }

        core::Result<exec::ExecutionState> StateClient::GetProcessState(
            const std::string &processName) const
        {
#if defined(__linux__) || defined(__QNX__)
            // Scan /proc entries to find a running process whose comm name matches.
            DIR *procDir = ::opendir("/proc");
            if (procDir != nullptr)
            {
                bool found{false};
                struct dirent *entry;
                while ((entry = ::readdir(procDir)) != nullptr)
                {
                    // PID directories consist only of digits.
                    bool isPid{entry->d_name[0] != '\0'};
                    for (const char *ch = entry->d_name; *ch && isPid; ++ch)
                    {
                        if (*ch < '0' || *ch > '9')
                        {
                            isPid = false;
                        }
                    }
                    if (!isPid)
                    {
                        continue;
                    }

#if defined(__QNX__)
                    // QNX: /proc/<pid>/as exists; read process name via
                    // devctl(DCMD_PROC_INFO) on the address-space file.
                    const std::string asPath{
                        std::string("/proc/") + entry->d_name + "/as"};
                    const int asFd = ::open(asPath.c_str(), O_RDONLY);
                    if (asFd < 0)
                    {
                        continue;
                    }
                    struct _proc_name
                    {
                        short _zero;
                        short _pathlen;
                        char  _name[256];
                    } pname;
                    std::memset(&pname, 0, sizeof(pname));
                    pname._pathlen = sizeof(pname._name);
                    if (::devctl(asFd, DCMD_PROC_MAPDEBUG_BASE, &pname,
                                 sizeof(pname), nullptr) == EOK)
                    {
                        std::string comm{pname._name};
                        // Strip leading path to get basename
                        auto pos = comm.rfind('/');
                        if (pos != std::string::npos)
                        {
                            comm = comm.substr(pos + 1);
                        }
                        if (comm == processName)
                        {
                            found = true;
                        }
                    }
                    ::close(asFd);
                    if (found)
                    {
                        break;
                    }
#else
                    // Linux: /proc/<pid>/comm contains the process name.
                    const std::string commPath{
                        std::string("/proc/") + entry->d_name + "/comm"};
                    std::ifstream commFile{commPath};
                    if (!commFile.is_open())
                    {
                        continue;
                    }

                    std::string comm;
                    std::getline(commFile, comm);

                    if (comm == processName)
                    {
                        found = true;
                        break;
                    }
#endif
                }
                ::closedir(procDir);

                const exec::ExecutionState state =
                    found ? exec::ExecutionState::kRunning
                          : exec::ExecutionState::kTerminating;
                return core::Result<exec::ExecutionState>::FromValue(state);
            }
#endif
            // Fallback: assume running (EM would track the actual state).
            return core::Result<exec::ExecutionState>::FromValue(
                exec::ExecutionState::kRunning);
        }

        std::shared_future<void> StateClient::SetStateWithTimeout(
            const FunctionGroupState &state,
            std::chrono::milliseconds timeout)
        {
            auto innerFuture = SetState(state);

            // Wrap the inner future in a monitored future that enforces the timeout.
            auto sharedPromise = std::make_shared<std::promise<void>>();
            std::shared_future<void> result{sharedPromise->get_future()};

            std::thread([innerFuture, sharedPromise, timeout]() mutable
            {
                const auto status = innerFuture.wait_for(timeout);
                try
                {
                    if (status == std::future_status::ready)
                    {
                        innerFuture.get(); // propagate any stored exception
                        sharedPromise->set_value();
                    }
                    else
                    {
                        // Timeout expired - report as cancelled.
                        const auto errVal =
                            static_cast<core::ErrorDomain::CodeType>(
                                ExecErrc::kCancelled);
                        core::ErrorCode errCode{errVal, cErrorDomain};
                        ExecException exc{errCode};
                        sharedPromise->set_exception(
                            std::make_exception_ptr(exc));
                    }
                }
                catch (...)
                {
                    try
                    {
                        sharedPromise->set_exception(
                            std::current_exception());
                    }
                    catch (std::future_error &) {}
                }
            }).detach();

            return result;
        }
    }
}
