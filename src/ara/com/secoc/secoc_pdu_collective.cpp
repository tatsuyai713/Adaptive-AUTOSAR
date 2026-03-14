/// @file src/ara/com/secoc/secoc_pdu_collective.cpp
/// @brief Implementation of SecOcPduCollective.

#include "./secoc_pdu_collective.h"

namespace ara
{
    namespace com
    {
        namespace secoc
        {
            void SecOcPduCollective::AddPdu(PduId id, SecOcPdu *pdu)
            {
                std::lock_guard<std::mutex> lock(mMutex);
                mPdus[id] = pdu;
            }

            core::Result<std::map<PduId, std::vector<std::uint8_t>>>
            SecOcPduCollective::ProtectAll(
                const std::map<PduId, std::vector<std::uint8_t>> &payloads)
            {
                std::lock_guard<std::mutex> lock(mMutex);

                if (mOverrideStatus == SecOcOverrideStatus::kSkipAll)
                {
                    // Bypass: return payloads as-is
                    return core::Result<std::map<PduId, std::vector<std::uint8_t>>>::FromValue(payloads);
                }

                std::map<PduId, std::vector<std::uint8_t>> result;
                for (const auto &pair : payloads)
                {
                    auto it = mPdus.find(pair.first);
                    if (it == mPdus.end() || !it->second)
                        return core::Result<std::map<PduId, std::vector<std::uint8_t>>>::FromError(
                            MakeErrorCode(SecOcErrc::kAuthenticationFailed));

                    auto secured = it->second->Protect(pair.second);
                    if (!secured.HasValue())
                        return core::Result<std::map<PduId, std::vector<std::uint8_t>>>::FromError(
                            secured.Error());
                    result[pair.first] = secured.Value();
                }
                return core::Result<std::map<PduId, std::vector<std::uint8_t>>>::FromValue(
                    std::move(result));
            }

            core::Result<std::map<PduId, std::vector<std::uint8_t>>>
            SecOcPduCollective::VerifyAll(
                const std::map<PduId, std::vector<std::uint8_t>> &securedPdus)
            {
                std::lock_guard<std::mutex> lock(mMutex);

                std::map<PduId, std::vector<std::uint8_t>> result;
                for (const auto &pair : securedPdus)
                {
                    VerificationStatusIndication status;
                    status.pduId = pair.first;

                    if (mOverrideStatus == SecOcOverrideStatus::kSkipAll ||
                        mOverrideStatus == SecOcOverrideStatus::kSkipVerify)
                    {
                        status.result = VerificationResult::kPass;
                        result[pair.first] = pair.second;
                        if (mCallback) mCallback(status);
                        continue;
                    }

                    auto it = mPdus.find(pair.first);
                    if (it == mPdus.end() || !it->second)
                    {
                        status.result = VerificationResult::kFail;
                        if (mCallback) mCallback(status);
                        return core::Result<std::map<PduId, std::vector<std::uint8_t>>>::FromError(
                            MakeErrorCode(SecOcErrc::kAuthenticationFailed));
                    }

                    auto verified = it->second->Verify(pair.second);
                    if (!verified.HasValue())
                    {
                        status.result = VerificationResult::kFail;
                        if (mCallback) mCallback(status);
                        return core::Result<std::map<PduId, std::vector<std::uint8_t>>>::FromError(
                            verified.Error());
                    }

                    status.result = VerificationResult::kPass;
                    result[pair.first] = verified.Value();
                    if (mCallback) mCallback(status);
                }
                return core::Result<std::map<PduId, std::vector<std::uint8_t>>>::FromValue(
                    std::move(result));
            }

            void SecOcPduCollective::SetVerificationStatusCallback(
                VerificationStatusCallback callback)
            {
                std::lock_guard<std::mutex> lock(mMutex);
                mCallback = std::move(callback);
            }

            void SecOcPduCollective::SetOverrideStatus(SecOcOverrideStatus status)
            {
                std::lock_guard<std::mutex> lock(mMutex);
                mOverrideStatus = status;
            }
        }
    }
}
