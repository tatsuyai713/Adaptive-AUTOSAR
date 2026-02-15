/// @file src/ara/exec/execution_client.h
/// @brief Declarations for execution client.
/// @details This file is part of the Adaptive AUTOSAR educational implementation.

#ifndef EXECUTION_CLIENT_H
#define EXECUTION_CLIENT_H

#include <chrono>
#include <cstdint>
#include <future>
#include <map>
#include <memory>
#include <mutex>
#include "../core/instance_specifier.h"
#include "../core/result.h"
#include "../com/someip/rpc/rpc_client.h"
#include "./exec_exception.h"

namespace ara
{
    namespace exec
    {
        /// @brief Adaptive application internal state
        enum class ExecutionState : uint8_t
        {
            kRunning = 0,     ///< Application process performs normally
            kTerminating = 1, ///< Application process is about to terminate
            kIdle = 2         ///< Application process is idle (not actively processing)
        };

        /// @brief Class that enables an adaptive application to interact with Execution Management
        class ExecutionClient final
        {
        private:
            /// @brief Tracks one in-flight RPC report request.
            struct PendingRequest
            {
                std::promise<void> promise;
                std::future<void> future;

                PendingRequest() : future(promise.get_future())
                {
                }
            };

            static const ExecErrorDomain cErrorDomain;
            const uint16_t cServiceId{1};
            const uint16_t cMethodId{1};
            const uint16_t cClientId{2};

            const core::InstanceSpecifier mInstanceSpecifier;
            com::someip::rpc::RpcClient *const mRpcClient;
            const std::chrono::seconds mTimeout;
            mutable std::mutex mMutex;
            mutable std::map<uint16_t, std::shared_ptr<PendingRequest>> mPendingRequests;
            mutable uint16_t mNextSessionId{1};

            uint16_t reserveSessionId() const noexcept;

            ExecException generateException(ExecErrc executionErrorCode) const;

            void reportExecutionStateHandler(
                const com::someip::rpc::SomeIpRpcMessage &message);

        public:
            /// @brief Constructor
            /// @param instanceSpecifier Adaptive application instance shortname-path
            /// @param rpcClient RPC client abstraction layer
            /// @param timeout RPC request timeout in seconds
            /// @throws std::invalid_argument Throws when timeout is not positive
            ExecutionClient(
                core::InstanceSpecifier instanceSpecifier,
                com::someip::rpc::RpcClient *rpcClient,
                int64_t timeout = 30);

            ~ExecutionClient();

            /// @brief Report the application internal state to Execution Management
            /// @param state Application current internal state
            /// @returns Void Result if the state reporting was successful, otherwise a Result containing the occurred error
            ara::core::Result<void> ReportExecutionState(
                ExecutionState state) const;
        };
    }
}

#endif
