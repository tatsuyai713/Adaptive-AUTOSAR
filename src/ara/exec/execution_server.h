#ifndef EXECUTION_SERVER_H
#define EXECUTION_SERVER_H

#include <functional>
#include <map>
#include <mutex>
#include <string>
#include "./execution_client.h"
#include "../core/result.h"
#include "../com/someip/rpc/rpc_server.h"

namespace ara
{
    namespace exec
    {
        /// @brief A class to handle the execution client requests at the EM side
        /// @see ExecutionClient
        /// @note The class is not part of the ARA standard.
        class ExecutionServer
        {
        public:
            using ExecutionStateChangeHandler =
                std::function<void(const std::string &, ExecutionState)>;

        private:
            const uint16_t cService{1};
            const uint16_t cMethodId{1};

            mutable std::mutex mMutex;
            com::someip::rpc::RpcServer *const mRpcServer;
            std::map<std::string, ExecutionState> mExecutionStates;
            ExecutionStateChangeHandler mStateChangeHandler;

            static void injectErrorCode(
                std::vector<uint8_t> &payload, ExecErrc errorCode);

            bool handleExecutionStateReport(
                const std::vector<uint8_t> &rpcRequestPayload,
                std::vector<uint8_t> &rpcResponsePayload);

        public:
            /// @brief Constructor
            /// @param rpcServer RPC server abstraction layer
            explicit ExecutionServer(
                com::someip::rpc::RpcServer *rpcServer);

            /// @brief Set callback invoked when an execution state changes
            /// @param handler Callback receiving instance id and new state
            /// @returns Invalid argument error if handler is empty
            core::Result<void> SetStateChangeHandler(
                ExecutionStateChangeHandler handler);

            /// @brief Unset previously configured state change callback
            void UnsetStateChangeHandler();

            /// @brief Try to get the execution state of an instance specfier
            /// @param[in] id The meta-model ID of the instance specifier
            /// @param[out] state Instance specifier execution state
            /// @return True if the execution state is reported; otherwise false
            /// @remark The function is thread-safe.
            bool TryGetExecutionState(
                std::string id, ExecutionState &state);

            /// @brief Get a copy of all reported execution states.
            /// @returns Map of instance id to execution state.
            std::map<std::string, ExecutionState> GetExecutionStatesSnapshot() const;
        };
    }
}

#endif
