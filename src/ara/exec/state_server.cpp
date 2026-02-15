#include "./state_server.h"
#include <stdexcept>

namespace ara
{
    namespace exec
    {
        StateServer::StateServer(
            com::someip::rpc::RpcServer *rpcServer,
            std::set<std::pair<std::string, std::string>> &&functionGroupStates,
            std::map<std::string, std::string> &&initialStates) : mRpcServer{rpcServer},
                                                                  mFunctionGroupStates{std::move(functionGroupStates)},
                                                                  mCurrentStates{std::move(initialStates)},
                                                                  mInitialized{false}
        {
            for (auto currentState : mCurrentStates)
            {
                auto _itr{mFunctionGroupStates.find(currentState)};
                if (_itr == mFunctionGroupStates.end())
                {
                    const std::string cMessage{
                        "State: " + currentState.second +
                        " of function group: " + currentState.first +
                        " is not defined."};

                    throw std::invalid_argument(cMessage);
                }
            }

            for (auto functionGroupState : mFunctionGroupStates)
            {
                auto _itr{mCurrentStates.find(functionGroupState.first)};
                if (_itr == mCurrentStates.end())
                {
                    const std::string cMessage{
                        "Function group: " + functionGroupState.first +
                        " does not have initial state."};

                    throw std::logic_error(cMessage);
                }
            }

            auto _setStateHandler{
                std::bind(
                    &StateServer::handleSetState,
                    this, std::placeholders::_1, std::placeholders::_2)};
            mRpcServer->SetHandler(
                cServiceId, cSetStateId, _setStateHandler);

            auto _stateTransitionHandler{
                std::bind(
                    &StateServer::handleStateTransition,
                    this, std::placeholders::_1, std::placeholders::_2)};
            mRpcServer->SetHandler(
                cServiceId, cStateTransitionId, _stateTransitionHandler);
        }

        void StateServer::injectErrorCode(
            std::vector<uint8_t> &payload, ExecErrc errorCode)
        {
            const auto cErrorCodeInt{static_cast<uint32_t>(errorCode)};
            com::helper::Inject(payload, cErrorCodeInt);
        }

        void StateServer::notify(const std::string &functionGroup)
        {
            std::function<void()> _callback;
            {
                const std::lock_guard<std::mutex> _lock{mMutex};
                auto _itr{mNotifiers.find(functionGroup)};
                if (_itr != mNotifiers.end())
                {
                    _callback = _itr->second;
                }
            }

            if (_callback)
            {
                _callback();
            }
        }

        bool StateServer::handleSetState(
            const std::vector<uint8_t> &rpcRequestPayload,
            std::vector<uint8_t> &rpcResponsePayload)
        {
            // RPC request payload format:
            // [Function group length: static 4 Bytes]
            // [Function group name: dynamic]
            // [Function group state length: static 4 Bytes]
            // [Function group state name: dynamic]
            const size_t cMinimumPayloadLength{8};

            // Validate minimum RPC payload length
            if (rpcRequestPayload.size() < cMinimumPayloadLength)
            {
                injectErrorCode(rpcResponsePayload, ExecErrc::kInvalidArguments);
                return false;
            }

            size_t _beginOffset{0};
            uint32_t _functionGroupLength{
                com::helper::ExtractInteger(rpcRequestPayload, _beginOffset)};

            auto _endOffset{
                static_cast<size_t>(_beginOffset + _functionGroupLength)};
            // Validate the RPC payload length
            if (rpcRequestPayload.size() < _endOffset + sizeof(uint32_t))
            {
                injectErrorCode(rpcResponsePayload, ExecErrc::kInvalidArguments);
                return false;
            }

            std::string _functionGroup(
                rpcRequestPayload.cbegin() + _beginOffset,
                rpcRequestPayload.cbegin() + _endOffset);

            _beginOffset = _endOffset;
            uint32_t _stateLength{
                com::helper::ExtractInteger(rpcRequestPayload, _beginOffset)};

            _endOffset = static_cast<size_t>(_beginOffset + _stateLength);
            // Re-validate the RPC payload length
            if (rpcRequestPayload.size() < _endOffset)
            {
                injectErrorCode(rpcResponsePayload, ExecErrc::kInvalidArguments);
                return false;
            }

            std::string _state(
                rpcRequestPayload.cbegin() + _beginOffset,
                rpcRequestPayload.cbegin() + _endOffset);

            std::pair<std::string, std::string> _pair(_functionGroup, _state);
            auto _functionGroupStatesItr{mFunctionGroupStates.find(_pair)};
            // Validate the function group and the state combination
            if (_functionGroupStatesItr == mFunctionGroupStates.end())
            {
                injectErrorCode(rpcResponsePayload, ExecErrc::kInvalidTransition);
                return false;
            }

            {
                const std::lock_guard<std::mutex> _currentStatesLock{mMutex};

                auto _currentStatesItr{mCurrentStates.find(_functionGroup)};
                if (_currentStatesItr == mCurrentStates.end())
                {
                    injectErrorCode(rpcResponsePayload, ExecErrc::kInvalidTransition);
                    return false;
                }
                else if (_currentStatesItr->second == _state)
                {
                    injectErrorCode(rpcResponsePayload, ExecErrc::kAlreadyInState);
                    return false;
                }

                // Update the newly reported state.
                _currentStatesItr->second = _state;
            }

            // Notify out of lock to prevent callback-induced deadlocks.
            notify(_functionGroup);
            rpcResponsePayload.clear();
            return true;
        }

        bool StateServer::handleStateTransition(
            const std::vector<uint8_t> &rpcRequestPayload,
            std::vector<uint8_t> &rpcResponsePayload)
        {
            // Validate the RPC payload length
            if (!rpcRequestPayload.empty())
            {
                // The RPC payload should be empty.
                injectErrorCode(rpcResponsePayload, ExecErrc::kInvalidArguments);
                return false;
            }

            bool _expected{false};
            if (!mInitialized.compare_exchange_strong(_expected, true))
            {
                // EM re-initialization is not possible.
                injectErrorCode(rpcResponsePayload, ExecErrc::kFailed);
                return false;
            }

            return true;
        }

        bool StateServer::TryGetState(
            std::string functionGroup, std::string &state)
        {
            const std::lock_guard<std::mutex> _currentStatesLock{mMutex};

            auto _itr{mCurrentStates.find(functionGroup)};
            if (_itr != mCurrentStates.end())
            {
                state = _itr->second;
                return true;
            }
            else
            {
                return false;
            }
        }

        void StateServer::SetNotifier(
            std::string functionGroup, std::function<void()> callback)
        {
            std::string _functionGroup(functionGroup);
            auto _result{TrySetNotifier(std::move(functionGroup), std::move(callback))};
            if (!_result.HasValue())
            {
                auto _errorCode{static_cast<ExecErrc>(_result.Error().Value())};
                if (_errorCode == ExecErrc::kInvalidTransition)
                {
                    throw std::out_of_range(
                        "Function group: " + _functionGroup + " does not exist.");
                }

                throw std::invalid_argument("Notifier callback is empty.");
            }
        }

        core::Result<void> StateServer::TrySetNotifier(
            std::string functionGroup, std::function<void()> callback)
        {
            if (!callback)
            {
                return core::Result<void>::FromError(
                    MakeErrorCode(ExecErrc::kInvalidArguments));
            }

            const std::lock_guard<std::mutex> _currentStatesLock{mMutex};
            auto _stateItr{mCurrentStates.find(functionGroup)};
            if (_stateItr == mCurrentStates.end())
            {
                return core::Result<void>::FromError(
                    MakeErrorCode(ExecErrc::kInvalidTransition));
            }

            mNotifiers[std::move(functionGroup)] = std::move(callback);
            return core::Result<void>::FromValue();
        }

        bool StateServer::Initialized() const noexcept
        {
            return mInitialized;
        }
    }
}
