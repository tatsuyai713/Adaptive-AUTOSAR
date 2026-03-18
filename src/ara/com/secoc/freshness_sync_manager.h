/// @file src/ara/com/secoc/freshness_sync_manager.h
/// @brief Cross-ECU freshness counter synchronization protocol.
/// @details Implements SWS_SecOC_00027/00028 — multi-ECU freshness sync
///          with replay acceptance windows and persistence support.

#ifndef ARA_COM_SECOC_FRESHNESS_SYNC_MANAGER_H
#define ARA_COM_SECOC_FRESHNESS_SYNC_MANAGER_H

#include <chrono>
#include <cstdint>
#include <functional>
#include <map>
#include <mutex>
#include <string>
#include <vector>
#include "../../core/result.h"
#include "./secoc_error_domain.h"

namespace ara
{
    namespace com
    {
        namespace secoc
        {
            using PduId = uint16_t;

            /// @brief Synchronization state for cross-ECU freshness protocol.
            enum class FreshnessSyncState : uint8_t
            {
                kUnsynchronized = 0,
                kSyncing = 1,
                kSynchronized = 2,
                kSyncLost = 3
            };

            /// @brief Configuration for the replay acceptance window.
            struct ReplayWindowConfig
            {
                /// @brief Forward acceptance tolerance (number of counter steps).
                uint64_t ForwardTolerance{16};

                /// @brief Backward acceptance tolerance (number of counter steps).
                uint64_t BackwardTolerance{0};

                /// @brief Maximum age for a freshness value in milliseconds.
                uint64_t MaxAgeMs{5000};
            };

            /// @brief Result of a freshness counter verification.
            enum class FreshnessVerifyResult : uint8_t
            {
                kAccepted = 0,
                kRejectedTooOld = 1,
                kRejectedTooNew = 2,
                kRejectedExpired = 3,
                kRejectedUnknownPdu = 4
            };

            /// @brief A freshness sync request message (sender → receiver).
            struct FreshnessSyncRequest
            {
                PduId Pdu;
                uint64_t CounterValue;
                uint64_t TimestampMs;
                std::string SourceEcuId;
            };

            /// @brief A freshness sync response message (receiver → sender).
            struct FreshnessSyncResponse
            {
                PduId Pdu;
                uint64_t AcknowledgedCounter;
                FreshnessSyncState SyncState;
                std::string ResponderEcuId;
            };

            /// @brief Per-PDU freshness synchronization entry.
            struct FreshnessSyncEntry
            {
                uint64_t LocalCounter{0};
                uint64_t RemoteCounter{0};
                /// @brief True once the first counter has been accepted.
                /// @details The initial RemoteCounter=0 is an uninitialized sentinel;
                ///          replay protection only activates after the first acceptance.
                bool HasAny{false};
                FreshnessSyncState State{FreshnessSyncState::kUnsynchronized};
                ReplayWindowConfig Window;
                std::chrono::steady_clock::time_point LastSyncTime;
            };

            /// @brief Callback for outbound sync messages.
            using SyncMessageSender = std::function<void(
                const FreshnessSyncRequest &request)>;

            /// @brief Cross-ECU freshness counter synchronization manager.
            /// @details Manages per-PDU freshness counters with multi-ECU sync,
            ///          replay acceptance windows, and persistence support.
            class FreshnessSyncManager
            {
            public:
                /// @brief Construct with a local ECU identifier.
                explicit FreshnessSyncManager(std::string localEcuId);
                ~FreshnessSyncManager() = default;

                FreshnessSyncManager(const FreshnessSyncManager &) = delete;
                FreshnessSyncManager &operator=(const FreshnessSyncManager &) = delete;

                /// @brief Register a PDU for freshness synchronization.
                core::Result<void> RegisterPdu(PduId pdu,
                                               ReplayWindowConfig window);

                /// @brief Get the current local counter value for a PDU.
                core::Result<uint64_t> GetLocalCounter(PduId pdu) const;

                /// @brief Increment and return the next freshness counter.
                core::Result<uint64_t> IncrementCounter(PduId pdu);

                /// @brief Verify a received freshness value against the window.
                /// @details Advances RemoteCounter on acceptance to prevent replay.
                FreshnessVerifyResult VerifyFreshness(
                    PduId pdu, uint64_t receivedCounter);

                /// @brief Process an incoming sync response from a remote ECU.
                core::Result<void> ProcessSyncResponse(
                    const FreshnessSyncResponse &response);

                /// @brief Initiate a sync request for a PDU.
                core::Result<FreshnessSyncRequest> CreateSyncRequest(
                    PduId pdu) const;

                /// @brief Set the callback for sending sync messages.
                void SetSyncMessageSender(SyncMessageSender sender);

                /// @brief Build and immediately dispatch a sync request for a PDU.
                /// @details Creates a FreshnessSyncRequest and passes it to the registered
                ///          SyncMessageSender callback if one is set.
                /// @returns The request on success, or an error if the PDU is unknown or
                ///          no sender has been registered.
                core::Result<FreshnessSyncRequest> SendSyncRequest(PduId pdu);

                /// @brief Get the current sync state for a PDU.
                FreshnessSyncState GetSyncState(PduId pdu) const;

                /// @brief Get the local ECU identifier.
                const std::string &GetLocalEcuId() const noexcept;

                /// @brief Persist all counter states to a byte vector.
                std::vector<uint8_t> PersistState() const;

                /// @brief Restore counter states from a byte vector.
                core::Result<void> RestoreState(
                    const std::vector<uint8_t> &data);

                /// @brief Number of registered PDUs.
                size_t RegisteredPduCount() const;

            private:
                std::string mLocalEcuId;
                mutable std::mutex mMutex;
                std::map<PduId, FreshnessSyncEntry> mEntries;
                SyncMessageSender mSender;

                uint64_t NowMs() const;
            };
        }
    }
}

#endif
