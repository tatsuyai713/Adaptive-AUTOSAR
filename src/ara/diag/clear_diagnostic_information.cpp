/// @file src/ara/diag/clear_diagnostic_information.cpp
/// @brief Implementation for ara::diag::ClearDiagnosticInformation.
/// @details This file is part of the Adaptive AUTOSAR educational implementation.

#include "./clear_diagnostic_information.h"

namespace ara
{
    namespace diag
    {
        ClearDiagnosticInformation::ClearDiagnosticInformation(
            const ara::core::InstanceSpecifier &specifier,
            ReentrancyType /*reentrancyType*/)
            : routing::RoutableUdsService{specifier, cSid}
        {
        }

        void ClearDiagnosticInformation::SetClearCallback(ClearCallback callback)
        {
            std::lock_guard<std::mutex> lock(mMutex);
            mCallback = std::move(callback);
        }

        std::future<OperationOutput> ClearDiagnosticInformation::HandleMessage(
            const std::vector<uint8_t> &requestData,
            MetaInfo & /*metaInfo*/,
            CancellationHandler && /*cancellationHandler*/)
        {
            std::promise<OperationOutput> promise;
            OperationOutput out;

            // Request: [0x14, groupOfDTC_high, groupOfDTC_mid, groupOfDTC_low]
            if (requestData.size() < 4)
            {
                GenerateNegativeResponse(out, cIncorrectMessageLength);
                promise.set_value(out);
                return promise.get_future();
            }

            const uint32_t groupOfDtc =
                (static_cast<uint32_t>(requestData[1]) << 16) |
                (static_cast<uint32_t>(requestData[2]) << 8)  |
                 static_cast<uint32_t>(requestData[3]);

            ClearCallback cb;
            {
                std::lock_guard<std::mutex> lock(mMutex);
                cb = mCallback;
            }

            bool cleared = true;
            if (cb)
            {
                cleared = cb(groupOfDtc);
            }

            if (!cleared)
            {
                // DTC group not found or clear failed → NRC 0x31
                GenerateNegativeResponse(out, cNrcRequestOutOfRange);
                promise.set_value(out);
                return promise.get_future();
            }

            // Positive response: [0x54] (no additional data per ISO 14229-1)
            out.responseData.push_back(
                static_cast<uint8_t>(cSid + cPositiveResponseSidIncrement)); // 0x54

            promise.set_value(out);
            return promise.get_future();
        }

    } // namespace diag
} // namespace ara
