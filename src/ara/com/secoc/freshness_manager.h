/// @file src/ara/com/secoc/freshness_manager.h
/// @brief SecOC Freshness Value Manager — monotonic counter for replay protection.
/// @details The Freshness Manager maintains per-PDU monotonic counters to prevent
///          replay attacks. It provides freshness values that are included in the
///          MAC computation for each secured PDU.
///
///          This is a software-based implementation. Production use may require
///          a hardware-backed counter (e.g., TPM, secure element).
///
///          Reference: AUTOSAR_SWS_SecureOnboardCommunication §7.4
///
///          This file is part of the Adaptive AUTOSAR educational implementation.

#ifndef ARA_COM_SECOC_FRESHNESS_MANAGER_H
#define ARA_COM_SECOC_FRESHNESS_MANAGER_H

#include <cstdint>
#include <map>
#include <mutex>
#include <vector>
#include "../../core/result.h"
#include "./secoc_error_domain.h"

namespace ara
{
    namespace com
    {
        namespace secoc
        {
            /// @brief PDU identifier type for freshness counter indexing.
            using PduId = uint16_t;

            /// @brief Freshness value represented as a byte sequence (little-endian).
            /// @details The freshness value is a monotonically increasing counter.
            ///          Its byte-width is configurable per PDU.
            using FreshnessValue = std::vector<uint8_t>;

            /// @brief Configuration for a freshness counter entry.
            struct FreshnessConfig
            {
                /// @brief Number of bytes used to represent the freshness counter.
                /// @details Typically 4 or 8 bytes for automotive use.
                uint8_t counterWidth{4};

                /// @brief Maximum freshness value before overflow.
                /// @details 0 means wrap-around (counter restarts at 0 after max).
                uint64_t maxCounter{0};  // 0 = no wrap (treat as unlimited)
            };

            /// @brief SecOC Freshness Value Manager (SWS_SecOC_00014).
            /// @details Thread-safe monotonic counter manager for SecOC PDUs.
            ///          Each PDU has an independent counter that increments on every
            ///          authenticated transmission or verified reception.
            ///
            ///          Usage:
            ///          @code
            ///          FreshnessManager fm;
            ///          fm.RegisterPdu(0x100, {4, 0});  // 4-byte counter, no wrap
            ///
            ///          // Sender side
            ///          auto fv = fm.GetFreshnessValue(0x100);
            ///          // ... use fv in MAC computation ...
            ///          fm.IncrementCounter(0x100);
            ///
            ///          // Receiver side
            ///          fm.VerifyAndUpdate(0x100, received_fv);
            ///          @endcode
            class FreshnessManager
            {
            public:
                FreshnessManager() = default;
                ~FreshnessManager() = default;

                // Not copyable or movable (owns mutex)
                FreshnessManager(const FreshnessManager &) = delete;
                FreshnessManager &operator=(const FreshnessManager &) = delete;

                /// @brief Register a PDU with a freshness counter.
                /// @param pduId Unique identifier for the PDU.
                /// @param config Counter configuration (width, max value).
                /// @returns True if registration succeeded; false if already registered.
                bool RegisterPdu(PduId pduId, const FreshnessConfig &config);

                /// @brief Unregister a PDU's freshness counter.
                /// @param pduId PDU identifier to remove.
                void UnregisterPdu(PduId pduId);

                /// @brief Get the current freshness value for a PDU (for MAC computation).
                /// @param pduId PDU identifier.
                /// @returns Current freshness as byte vector, or error if not registered.
                ara::core::Result<FreshnessValue> GetFreshnessValue(PduId pduId) const;

                /// @brief Increment the freshness counter after a successful transmission.
                /// @param pduId PDU identifier.
                /// @returns Ok or error (overflow, not registered).
                ara::core::Result<void> IncrementCounter(PduId pduId);

                /// @brief Verify received freshness and update counter if valid.
                /// @param pduId PDU identifier.
                /// @param receivedFreshness Freshness bytes from received PDU.
                /// @returns Ok if freshness is valid (monotonically greater than current).
                ///          kFreshnessCounterFailed if received value is not strictly greater.
                ///
                /// @note Receiver-side only. Uses a reception window to tolerate
                ///       moderate out-of-order delivery.
                ara::core::Result<void> VerifyAndUpdate(
                    PduId pduId,
                    const FreshnessValue &receivedFreshness);

                /// @brief Get the current counter value as a 64-bit integer.
                /// @param pduId PDU identifier.
                /// @returns Counter value or error.
                ara::core::Result<uint64_t> GetCounterValue(PduId pduId) const;

                /// @brief Reset a counter to zero (e.g., after ECU power-on).
                /// @param pduId PDU identifier.
                ara::core::Result<void> ResetCounter(PduId pduId);

            private:
                struct Entry
                {
                    FreshnessConfig config;
                    uint64_t counter{0};
                };

                mutable std::mutex mMutex;
                std::map<PduId, Entry> mEntries;

                /// @brief Convert 64-bit counter to byte vector (little-endian).
                static FreshnessValue counterToBytes(uint64_t counter, uint8_t width);

                /// @brief Convert freshness byte vector to 64-bit counter (little-endian).
                static uint64_t bytesToCounter(const FreshnessValue &fv);
            };

        } // namespace secoc
    }     // namespace com
} // namespace ara

#endif // ARA_COM_SECOC_FRESHNESS_MANAGER_H
