/// @file src/ara/com/secoc/secoc_pdu_collective.h
/// @brief SecOC PDU collective and verification events per AUTOSAR AP SWS_SecOC.
/// @details Provides SecOcPduCollective for grouped PDU authentication,
///          VerificationStatusIndication for observer notification, and
///          SecOcOverrideStatus for diagnostic bypass.

#ifndef ARA_COM_SECOC_PDU_COLLECTIVE_H
#define ARA_COM_SECOC_PDU_COLLECTIVE_H

#include <cstdint>
#include <functional>
#include <map>
#include <mutex>
#include <vector>
#include "../../core/result.h"
#include "./secoc_pdu.h"

namespace ara
{
    namespace com
    {
        namespace secoc
        {
            /// @brief Verification result for a single PDU (SWS_SecOC_00200).
            enum class VerificationResult : std::uint8_t
            {
                kPass = 0,              ///< MAC verified successfully
                kFail = 1,              ///< MAC verification failed
                kFreshnessFailure = 2,  ///< Freshness check failed
                kNotVerified = 3        ///< Not yet verified
            };

            /// @brief Verification status indication per PDU (SWS_SecOC_00201).
            struct VerificationStatusIndication
            {
                PduId pduId{0};
                VerificationResult result{VerificationResult::kNotVerified};
                std::uint64_t freshness{0};
            };

            /// @brief Callback type for verification events.
            using VerificationStatusCallback = std::function<
                void(const VerificationStatusIndication &)>;

            /// @brief Override status for diagnostic bypass (SWS_SecOC_00202).
            enum class SecOcOverrideStatus : std::uint8_t
            {
                kNoOverride = 0,    ///< Normal operation
                kSkipVerify = 1,    ///< Skip MAC verification (diag mode)
                kSkipAll = 2        ///< Skip all SecOC processing
            };

            /// @brief Collective PDU processor for grouped authentication (SWS_SecOC_00203).
            /// @details Processes multiple I-PDUs as a group, producing a single MAC
            ///          that covers all PDUs in the collection.
            class SecOcPduCollective
            {
            public:
                SecOcPduCollective() = default;
                ~SecOcPduCollective() noexcept = default;

                /// @brief Add a PDU processor to the collection.
                void AddPdu(PduId id, SecOcPdu *pdu);

                /// @brief Protect all PDUs in the collection.
                /// @param payloads Map of PduId → payload bytes.
                /// @returns Map of PduId → secured PDU bytes, or error.
                core::Result<std::map<PduId, std::vector<std::uint8_t>>> ProtectAll(
                    const std::map<PduId, std::vector<std::uint8_t>> &payloads);

                /// @brief Verify all PDUs in the collection.
                /// @param securedPdus Map of PduId → received secured bytes.
                /// @returns Map of PduId → authenticated payload bytes, or error.
                core::Result<std::map<PduId, std::vector<std::uint8_t>>> VerifyAll(
                    const std::map<PduId, std::vector<std::uint8_t>> &securedPdus);

                /// @brief Register a callback for per-PDU verification events.
                void SetVerificationStatusCallback(VerificationStatusCallback callback);

                /// @brief Set override status (diagnostic bypass).
                void SetOverrideStatus(SecOcOverrideStatus status);

                SecOcOverrideStatus GetOverrideStatus() const noexcept { return mOverrideStatus; }

            private:
                std::map<PduId, SecOcPdu *> mPdus;
                VerificationStatusCallback mCallback;
                SecOcOverrideStatus mOverrideStatus{SecOcOverrideStatus::kNoOverride};
                mutable std::mutex mMutex;
            };

        } // namespace secoc
    }     // namespace com
} // namespace ara

#endif // ARA_COM_SECOC_PDU_COLLECTIVE_H
