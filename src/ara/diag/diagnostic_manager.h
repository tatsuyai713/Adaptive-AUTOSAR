/// @file src/ara/diag/diagnostic_manager.h
/// @brief Central diagnostic orchestrator for coordinating UDS services.
/// @details This file is part of the Adaptive AUTOSAR educational implementation.

#ifndef DIAG_DIAGNOSTIC_MANAGER_H
#define DIAG_DIAGNOSTIC_MANAGER_H

#include <chrono>
#include <cstdint>
#include <functional>
#include <map>
#include <mutex>
#include <string>
#include <vector>
#include "../core/result.h"
#include "./conversation.h"
#include "./event.h"
#include "./diag_error_domain.h"

namespace ara
{
    namespace diag
    {
        /// @brief P2/P2* server timing parameters per ISO 14229.
        struct ResponseTiming
        {
            /// @brief P2server max time (default session), in ms.
            uint16_t P2ServerMs{50};
            /// @brief P2*server max time (non-default session), in ms.
            uint16_t P2StarServerMs{5000};
        };

        /// @brief Statistics for a registered UDS service.
        struct ServiceStats
        {
            uint8_t ServiceId{0};
            uint32_t RequestCount{0};
            uint32_t SuccessCount{0};
            uint32_t NrcCount{0};
            std::chrono::steady_clock::time_point LastRequestTime;
        };

        /// @brief A single pending request tracked by the manager.
        struct PendingRequest
        {
            uint32_t RequestId{0};
            uint8_t ServiceId{0};
            uint16_t SubFunction{0};
            std::chrono::steady_clock::time_point ArrivalTime;
            bool ResponsePending{false};
        };

        /// @brief Callback invoked when a response timeout is imminent.
        using ResponsePendingCallback =
            std::function<void(uint32_t requestId, uint8_t serviceId)>;

        /// @brief Central orchestrator managing UDS service requests,
        ///        conversations, events, and server timing.
        class DiagnosticManager
        {
        public:
            DiagnosticManager();
            ~DiagnosticManager() = default;

            /// @brief Set the P2/P2* server timing parameters.
            void SetResponseTiming(const ResponseTiming &timing);

            /// @brief Get the current P2/P2* server timing parameters.
            ResponseTiming GetResponseTiming() const;

            /// @brief Register a UDS service for tracking.
            core::Result<void> RegisterService(uint8_t serviceId);

            /// @brief Unregister a UDS service.
            core::Result<void> UnregisterService(uint8_t serviceId);

            /// @brief Submit a new diagnostic request for processing.
            /// @returns A request ID for tracking.
            core::Result<uint32_t> SubmitRequest(
                uint8_t serviceId,
                uint16_t subFunction,
                const std::vector<uint8_t> &payload);

            /// @brief Complete a pending request with a positive response.
            core::Result<void> CompleteRequest(uint32_t requestId);

            /// @brief Reject a pending request with a NRC.
            core::Result<void> RejectRequest(
                uint32_t requestId, uint8_t nrc);

            /// @brief Check pending requests against P2/P2* timing and send
            ///        response-pending (NRC 0x78) events as needed.
            /// @returns Number of requests that triggered response-pending.
            uint32_t CheckTimingConstraints();

            /// @brief Set callback for response-pending notifications.
            void SetResponsePendingCallback(ResponsePendingCallback cb);

            /// @brief Get statistics for a particular service.
            core::Result<ServiceStats> GetServiceStats(
                uint8_t serviceId) const;

            /// @brief Get all pending requests.
            std::vector<PendingRequest> GetPendingRequests() const;

            /// @brief Get total number of requests processed.
            uint32_t GetTotalRequestCount() const;

        private:
            mutable std::mutex mMutex;
            ResponseTiming mTiming;
            std::map<uint8_t, ServiceStats> mServices;
            std::map<uint32_t, PendingRequest> mPending;
            uint32_t mNextRequestId{1};
            uint32_t mTotalRequests{0};
            ResponsePendingCallback mRpCallback;
        };
    }
}

#endif
