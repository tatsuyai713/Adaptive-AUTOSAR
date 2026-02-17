/// @file src/ara/diag/input_output_control.cpp
/// @brief Implementation for ara::diag::InputOutputControl.
/// @details This file is part of the Adaptive AUTOSAR educational implementation.

#include "./input_output_control.h"

namespace ara
{
    namespace diag
    {
        InputOutputControl::InputOutputControl(
            const ara::core::InstanceSpecifier &specifier,
            ReentrancyType /*reentrancyType*/)
            : routing::RoutableUdsService{specifier, cSid}
        {
        }

        void InputOutputControl::RegisterControlHandler(uint16_t did, ControlHandler handler)
        {
            std::lock_guard<std::mutex> lock(mMutex);
            mControlHandlers[did] = std::move(handler);
        }

        void InputOutputControl::RegisterReadHandler(uint16_t did, ReadHandler handler)
        {
            std::lock_guard<std::mutex> lock(mMutex);
            mReadHandlers[did] = std::move(handler);
        }

        void InputOutputControl::UnregisterHandlers(uint16_t did)
        {
            std::lock_guard<std::mutex> lock(mMutex);
            mControlHandlers.erase(did);
            mReadHandlers.erase(did);
        }

        std::future<OperationOutput> InputOutputControl::HandleMessage(
            const std::vector<uint8_t> &requestData,
            MetaInfo & /*metaInfo*/,
            CancellationHandler && /*cancellationHandler*/)
        {
            std::promise<OperationOutput> promise;
            OperationOutput out;

            // Request: [0x2F, DID_H, DID_L, controlOption, (controlOptionRecord...)]
            if (requestData.size() < 4)
            {
                GenerateNegativeResponse(out, cIncorrectMessageLength);
                promise.set_value(out);
                return promise.get_future();
            }

            const uint16_t did =
                (static_cast<uint16_t>(requestData[1]) << 8) |
                 static_cast<uint16_t>(requestData[2]);
            const uint8_t controlOptByte = requestData[3];

            if (controlOptByte > 0x03U)
            {
                GenerateNegativeResponse(out, cNrcRequestOutOfRange);
                promise.set_value(out);
                return promise.get_future();
            }

            const auto controlOpt = static_cast<ControlOption>(controlOptByte);

            // Data bytes after the control option byte
            const std::vector<uint8_t> controlData(
                requestData.begin() + 4, requestData.end());

            ControlHandler ctrlHandler;
            ReadHandler readHandler;
            {
                std::lock_guard<std::mutex> lock(mMutex);
                auto ctrlIt = mControlHandlers.find(did);
                if (ctrlIt == mControlHandlers.end())
                {
                    out.responseData.clear();
                    GenerateNegativeResponse(out, cNrcRequestOutOfRange);
                    promise.set_value(out);
                    return promise.get_future();
                }
                ctrlHandler = ctrlIt->second;

                auto readIt = mReadHandlers.find(did);
                if (readIt != mReadHandlers.end())
                {
                    readHandler = readIt->second;
                }
            }

            // Invoke control handler (outside lock)
            const bool accepted = ctrlHandler(controlOpt, controlData);
            if (!accepted)
            {
                GenerateNegativeResponse(out, cNrcConditionsNotCorrect);
                promise.set_value(out);
                return promise.get_future();
            }

            // Positive response: [0x6F, DID_H, DID_L, controlOption, (statusRecord)]
            out.responseData.push_back(
                static_cast<uint8_t>(cSid + cPositiveResponseSidIncrement)); // 0x6F
            out.responseData.push_back(static_cast<uint8_t>(did >> 8));
            out.responseData.push_back(static_cast<uint8_t>(did & 0xFF));
            out.responseData.push_back(controlOptByte);

            // Append current signal value (controlStatusRecord) if read handler registered
            if (readHandler)
            {
                const std::vector<uint8_t> status = readHandler();
                out.responseData.insert(out.responseData.end(),
                                        status.begin(), status.end());
            }

            promise.set_value(out);
            return promise.get_future();
        }

    } // namespace diag
} // namespace ara
