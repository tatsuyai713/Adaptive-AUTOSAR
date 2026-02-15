/// @file src/ara/exec/execution_client.cpp
/// @brief Implementation for execution client.
/// @details This file is part of the Adaptive AUTOSAR educational implementation.

#include <utility>
#include <limits>
#include <stdexcept>
#include "./execution_client.h"

namespace ara
{
    namespace exec
    {
        const ExecErrorDomain ExecutionClient::cErrorDomain;

        ExecutionClient::ExecutionClient(
            core::InstanceSpecifier instanceSpecifier,
            com::someip::rpc::RpcClient *rpcClient,
            int64_t timeout) : mInstanceSpecifier{std::move(instanceSpecifier)},
                               mRpcClient{rpcClient},
                               mTimeout{std::chrono::seconds(timeout > 0 ? timeout : 1)}
        {
            if (mRpcClient == nullptr)
            {
                throw std::invalid_argument("RPC client cannot be null.");
            }

            if (timeout <= 0)
            {
                throw std::invalid_argument("Invalid timeout.");
            }

            auto _handler{
                std::bind(
                    &ExecutionClient::reportExecutionStateHandler,
                    this, std::placeholders::_1)};
            mRpcClient->SetHandler(cServiceId, cMethodId, _handler);
        }

        uint16_t ExecutionClient::reserveSessionId() const noexcept
        {
            const uint16_t _sessionId{mNextSessionId};
            constexpr uint16_t cSessionIdMax{std::numeric_limits<uint16_t>::max()};
            if (mNextSessionId == cSessionIdMax)
            {
                mNextSessionId = 1;
            }
            else
            {
                ++mNextSessionId;
            }

            return _sessionId;
        }

        ExecException ExecutionClient::generateException(
            ExecErrc executionErrorCode) const
        {
            auto _code{static_cast<core::ErrorDomain::CodeType>(executionErrorCode)};
            core::ErrorCode _errorCode(_code, cErrorDomain);
            ExecException _result(_errorCode);

            return _result;
        }

        void ExecutionClient::reportExecutionStateHandler(
            const com::someip::rpc::SomeIpRpcMessage &message)
        {
            std::shared_ptr<PendingRequest> _pendingRequest;
            {
                const std::lock_guard<std::mutex> _lock{mMutex};
                auto _itr{mPendingRequests.find(message.SessionId())};
                if (_itr == mPendingRequests.end())
                {
                    // Ignore stale or unexpected responses.
                    return;
                }

                _pendingRequest = _itr->second;
                mPendingRequests.erase(_itr);
            }

            const std::vector<uint8_t> &cRpcPayload{message.RpcPayload()};

            if (cRpcPayload.empty())
            {
                try
                {
                    _pendingRequest->promise.set_value();
                }
                catch (const std::future_error &)
                {
                    // Ignore duplicate or late completion.
                }
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
                    ExecException _exception(generateException(_executionErrorCode));
                    auto _exceptionPtr = std::make_exception_ptr(_exception);

                    _pendingRequest->promise.set_exception(_exceptionPtr);
                }
                catch (const std::out_of_range &)
                {
                    const ExecErrc cCorruptedResponseCode{
                        ExecErrc::kCommunicationError};
                    ExecException _exception(generateException(
                        cCorruptedResponseCode));
                    auto _exceptionPtr = std::make_exception_ptr(_exception);

                    _pendingRequest->promise.set_exception(_exceptionPtr);
                }
                catch (const std::future_error &)
                {
                    // Ignore duplicate or late completion.
                }
            }
        }

        ara::core::Result<void> ExecutionClient::ReportExecutionState(
            ExecutionState state) const
        {
            std::vector<uint8_t> _rpcPayload;
            mInstanceSpecifier.Serialize(_rpcPayload);

            auto _stateByte{static_cast<uint8_t>(state)};
            _rpcPayload.push_back(_stateByte);

            std::shared_ptr<PendingRequest> _pendingRequest;
            uint16_t _sessionId{0};
            try
            {
                {
                    const std::lock_guard<std::mutex> _lock{mMutex};
                    if (!mPendingRequests.empty())
                    {
                        const ExecErrc cAlreadyReporedCode{ExecErrc::kFailed};
                        ExecException _exception(generateException(cAlreadyReporedCode));
                        throw _exception;
                    }

                    _sessionId = reserveSessionId();
                    _pendingRequest = std::make_shared<PendingRequest>();
                    mPendingRequests.emplace(_sessionId, _pendingRequest);
                }

                try
                {
                    mRpcClient->Send(cServiceId, cMethodId, cClientId, _rpcPayload);
                }
                catch (...)
                {
                    const std::lock_guard<std::mutex> _lock{mMutex};
                    mPendingRequests.erase(_sessionId);
                    throw generateException(ExecErrc::kCommunicationError);
                }

                std::future_status _status{
                    _pendingRequest->future.wait_for(mTimeout)};

                if (_status == std::future_status::timeout)
                {
                    const std::lock_guard<std::mutex> _lock{mMutex};
                    mPendingRequests.erase(_sessionId);
                    const ExecErrc cTimeoutCode{ExecErrc::kCommunicationError};
                    ExecException _exception(generateException(cTimeoutCode));

                    throw _exception;
                }
                else
                {
                    _pendingRequest->future.get();
                }

                core::Result<void> _result;
                return _result;
            }
            catch (const ExecException &ex)
            {
                core::Result<void> _result(ex.GetErrorCode());
                return _result;
            }
            catch (const std::exception &)
            {
                core::Result<void> _result{
                    MakeErrorCode(ExecErrc::kGeneralError)};
                return _result;
            }
        }

        ExecutionClient::~ExecutionClient()
        {
            std::map<uint16_t, std::shared_ptr<PendingRequest>> _pendingRequests;
            {
                const std::lock_guard<std::mutex> _lock{mMutex};
                _pendingRequests.swap(mPendingRequests);
            }

            for (auto &_item : _pendingRequests)
            {
                try
                {
                    ExecException _exception(generateException(ExecErrc::kCancelled));
                    auto _exceptionPtr = std::make_exception_ptr(_exception);
                    _item.second->promise.set_exception(_exceptionPtr);
                }
                catch (const std::future_error &)
                {
                    // Ignore requests that are already completed.
                }
                catch (const std::exception &)
                {
                    // Destructors must not throw.
                }
            }
        }
    }
}
