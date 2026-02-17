/// @file src/ara/diag/communication_control.cpp
/// @brief Implementation for ara::diag::CommunicationControl.
/// @details This file is part of the Adaptive AUTOSAR educational implementation.

#include "./communication_control.h"

namespace ara
{
    namespace diag
    {
        CommunicationControl::CommunicationControl(
            const ara::core::InstanceSpecifier &specifier,
            ReentrancyType /*reentrancyType*/)
            : routing::RoutableUdsService{specifier, cSid}
        {
        }

        void CommunicationControl::SetControlCallback(ControlCallback callback)
        {
            std::lock_guard<std::mutex> lock(mMutex);
            mCallback = std::move(callback);
        }

        CommunicationControl::SubFunction
        CommunicationControl::GetCurrentSubFunction() const noexcept
        {
            std::lock_guard<std::mutex> lock(mMutex);
            return mCurrentSubFunc;
        }

        CommunicationControl::CommType
        CommunicationControl::GetCurrentCommType() const noexcept
        {
            std::lock_guard<std::mutex> lock(mMutex);
            return mCurrentCommType;
        }

        std::future<OperationOutput> CommunicationControl::HandleMessage(
            const std::vector<uint8_t> &requestData,
            MetaInfo & /*metaInfo*/,
            CancellationHandler && /*cancellationHandler*/)
        {
            std::promise<OperationOutput> promise;
            OperationOutput out;

            // Request: [0x28, subFunction, communicationType]
            if (requestData.size() < 3)
            {
                GenerateNegativeResponse(out, cIncorrectMessageLength);
                promise.set_value(out);
                return promise.get_future();
            }

            const uint8_t subFuncByte = requestData[1] & 0x7FU; // strip suppressPosRspBit
            const uint8_t commTypeByte = requestData[2];

            // Validate sub-function
            if (subFuncByte > 0x03U)
            {
                GenerateNegativeResponse(out, cNrcSubFunctionNotSupported);
                promise.set_value(out);
                return promise.get_future();
            }

            // Validate communication type
            if (commTypeByte == 0x00U || commTypeByte > 0x03U)
            {
                GenerateNegativeResponse(out, cNrcRequestOutOfRange);
                promise.set_value(out);
                return promise.get_future();
            }

            const auto sf = static_cast<SubFunction>(subFuncByte);
            const auto ct = static_cast<CommType>(commTypeByte);

            // Apply and notify
            ControlCallback cb;
            {
                std::lock_guard<std::mutex> lock(mMutex);
                mCurrentSubFunc = sf;
                mCurrentCommType = ct;
                cb = mCallback;
            }
            if (cb)
            {
                cb(sf, ct);
            }

            // Suppress positive response if suppressPosRspBit is set
            const bool suppressPosRsp = (requestData[1] & 0x80U) != 0;
            if (!suppressPosRsp)
            {
                out.responseData.push_back(
                    static_cast<uint8_t>(cSid + cPositiveResponseSidIncrement)); // 0x68
                out.responseData.push_back(subFuncByte);
            }

            promise.set_value(out);
            return promise.get_future();
        }

    } // namespace diag
} // namespace ara
