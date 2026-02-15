/// @file src/ara/exec/execution_server.cpp
/// @brief Implementation for execution server.
/// @details This file is part of the Adaptive AUTOSAR educational implementation.

#include "./execution_server.h"

namespace ara
{
    namespace exec
    {
        ExecutionServer::ExecutionServer(
            com::someip::rpc::RpcServer *rpcServer) : mRpcServer{rpcServer}
        {
            auto _handler{
                std::bind(&ExecutionServer::handleExecutionStateReport,
                          this, std::placeholders::_1, std::placeholders::_2)};
            mRpcServer->SetHandler(cService, cMethodId, _handler);
        }

        core::Result<void> ExecutionServer::SetStateChangeHandler(
            ExecutionStateChangeHandler handler)
        {
            if (!handler)
            {
                return core::Result<void>::FromError(
                    MakeErrorCode(ExecErrc::kInvalidArguments));
            }

            const std::lock_guard<std::mutex> _executionStatesLock(mMutex);
            mStateChangeHandler = std::move(handler);
            return core::Result<void>::FromValue();
        }

        void ExecutionServer::UnsetStateChangeHandler()
        {
            const std::lock_guard<std::mutex> _executionStatesLock(mMutex);
            mStateChangeHandler = nullptr;
        }

        void ExecutionServer::injectErrorCode(
            std::vector<uint8_t> &payload, ExecErrc errorCode)
        {
            const auto cErrorCodeInt{static_cast<uint32_t>(errorCode)};
            com::helper::Inject(payload, cErrorCodeInt);
        }

        bool ExecutionServer::handleExecutionStateReport(
            const std::vector<uint8_t> &rpcRequestPayload,
            std::vector<uint8_t> &rpcResponsePayload)
        {
            // RPC request payload format:
            // [Instance spcifier meta-model ID length: static 4 Bytes]
            // [Instance spcifier meta-model ID: dynamic]
            // [Insatnce specifier execution state: static 1 Byte]
            const size_t cMinimumPayloadLength{5};

            // Validate minimum RPC payload length
            if (rpcRequestPayload.size() < cMinimumPayloadLength)
            {
                injectErrorCode(rpcResponsePayload, ExecErrc::kInvalidArguments);
                return false;
            }

            size_t _beginOffset{0};
            uint32_t _length{
                com::helper::ExtractInteger(rpcRequestPayload, _beginOffset)};

            auto _endOffset{static_cast<size_t>(_beginOffset + _length)};
            // Validate the RPC payload length
            if (rpcRequestPayload.size() <= _endOffset)
            {
                injectErrorCode(rpcResponsePayload, ExecErrc::kInvalidArguments);
                return false;
            }

            uint8_t _executionStateByte{rpcRequestPayload.at(_endOffset)};

            // Validate the reported execution state range
            const auto cRunningStateInt{
                static_cast<uint8_t>(ExecutionState::kRunning)};
            const auto cIdleStateInt{
                static_cast<uint8_t>(ExecutionState::kIdle)};
            if (_executionStateByte > cIdleStateInt)
            {
                injectErrorCode(rpcResponsePayload, ExecErrc::kInvalidArguments);
                return false;
            }

            std::string _id(
                rpcRequestPayload.cbegin() + _beginOffset,
                rpcRequestPayload.cbegin() + _endOffset);

            auto _executionState{static_cast<ExecutionState>(_executionStateByte)};
            ExecutionStateChangeHandler _callback;
            bool _stateChanged{false};

            {
                const std::lock_guard<std::mutex> _executionStatesLock(mMutex);
                auto _itr{mExecutionStates.find(_id)};
                if (_itr == mExecutionStates.end() || _itr->second != _executionState)
                {
                    // Insert/update the newly reported state and
                    // react with an empty RPC response payload
                    mExecutionStates[_id] = _executionState;
                    _callback = mStateChangeHandler;
                    _stateChanged = true;
                }
                else
                {
                    injectErrorCode(rpcResponsePayload, ExecErrc::kAlreadyInState);
                    return false;
                }
            }

            rpcResponsePayload.clear();
            if (_stateChanged && _callback)
            {
                _callback(_id, _executionState);
            }

            return true;
        }

        bool ExecutionServer::TryGetExecutionState(
            std::string id, ExecutionState &state)
        {
            auto _result{GetExecutionState(std::move(id))};
            if (_result.HasValue())
            {
                state = _result.Value();
                return true;
            }
            else
            {
                return false;
            }
        }

        core::Result<ExecutionState> ExecutionServer::GetExecutionState(std::string id)
        {
            const std::lock_guard<std::mutex> _executionStatesLock(mMutex);
            auto _itr{mExecutionStates.find(id)};
            if (_itr == mExecutionStates.end())
            {
                return core::Result<ExecutionState>::FromError(
                    MakeErrorCode(ExecErrc::kInvalidArguments));
            }

            return core::Result<ExecutionState>{_itr->second};
        }

        std::map<std::string, ExecutionState>
        ExecutionServer::GetExecutionStatesSnapshot() const
        {
            const std::lock_guard<std::mutex> _executionStatesLock(mMutex);
            return mExecutionStates;
        }
    }
}
