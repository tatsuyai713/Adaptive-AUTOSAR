/// @file src/ara/diag/control_dtc_setting.cpp
/// @brief Implementation for ara::diag::ControlDtcSetting.
/// @details This file is part of the Adaptive AUTOSAR educational implementation.

#include "./control_dtc_setting.h"

namespace ara
{
    namespace diag
    {
        ControlDtcSetting::ControlDtcSetting(
            const ara::core::InstanceSpecifier &specifier,
            ReentrancyType /*reentrancyType*/)
            : routing::RoutableUdsService{specifier, cSid}
        {
        }

        void ControlDtcSetting::SetSettingCallback(SettingCallback callback)
        {
            std::lock_guard<std::mutex> lock(mMutex);
            mCallback = std::move(callback);
        }

        bool ControlDtcSetting::IsDtcSettingEnabled() const noexcept
        {
            std::lock_guard<std::mutex> lock(mMutex);
            return mDtcSettingEnabled;
        }

        std::future<OperationOutput> ControlDtcSetting::HandleMessage(
            const std::vector<uint8_t> &requestData,
            MetaInfo & /*metaInfo*/,
            CancellationHandler && /*cancellationHandler*/)
        {
            std::promise<OperationOutput> promise;
            OperationOutput out;

            // Request: [0x85, subFunction, (optional DTCSettingControlOptionRecord)]
            if (requestData.size() < 2)
            {
                GenerateNegativeResponse(out, cIncorrectMessageLength);
                promise.set_value(out);
                return promise.get_future();
            }

            const uint8_t subFuncByte = requestData[1] & 0x7FU; // strip suppressPosRspBit
            const bool suppressPosRsp = (requestData[1] & 0x80U) != 0;

            if (subFuncByte != static_cast<uint8_t>(SettingType::kOn) &&
                subFuncByte != static_cast<uint8_t>(SettingType::kOff))
            {
                GenerateNegativeResponse(out, cNrcSubFunctionNotSupported);
                promise.set_value(out);
                return promise.get_future();
            }

            const bool newEnabled = (subFuncByte == static_cast<uint8_t>(SettingType::kOn));

            SettingCallback cb;
            {
                std::lock_guard<std::mutex> lock(mMutex);
                mDtcSettingEnabled = newEnabled;
                cb = mCallback;
            }
            if (cb)
            {
                cb(newEnabled);
            }

            if (!suppressPosRsp)
            {
                out.responseData.push_back(
                    static_cast<uint8_t>(cSid + cPositiveResponseSidIncrement)); // 0xC5
                out.responseData.push_back(subFuncByte);
            }

            promise.set_value(out);
            return promise.get_future();
        }

    } // namespace diag
} // namespace ara
