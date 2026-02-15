#ifndef SAMPLE_ARA_COM_SOCKETCAN_GATEWAY_MOCK_CAN_RECEIVER_H
#define SAMPLE_ARA_COM_SOCKETCAN_GATEWAY_MOCK_CAN_RECEIVER_H

#include <chrono>
#include "./can_frame_receiver.h"

namespace sample
{
    namespace ara_com_socketcan_gateway
    {
        // Mock CAN backend used for local development and CI.
        // It generates deterministic powertrain/chassis frames periodically.
        class MockCanReceiver final : public CanFrameReceiver
        {
        private:
            // Monotonic sequence used to generate evolving frame values.
            std::uint32_t mSequence;
            // Generation period between synthetic CAN frames.
            std::chrono::milliseconds mPeriod;
            // Next frame release timestamp.
            std::chrono::steady_clock::time_point mNextFrameTime;

        public:
            explicit MockCanReceiver(
                std::chrono::milliseconds period = std::chrono::milliseconds{20}) noexcept;

            ara::core::Result<void> Open() override;
            void Close() noexcept override;
            ara::core::Result<bool> Receive(
                CanFrame &frame,
                std::chrono::milliseconds timeout) override;
            const char *BackendName() const noexcept override;
        };
    }
}

#endif
