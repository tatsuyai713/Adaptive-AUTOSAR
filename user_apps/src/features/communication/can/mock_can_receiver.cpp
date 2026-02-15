#include "./mock_can_receiver.h"

#include <algorithm>
#include <thread>

namespace
{
    // Canonical sample CAN IDs used by the decoder.
    constexpr std::uint32_t cPowertrainCanId{0x100U};
    constexpr std::uint32_t cChassisCanId{0x101U};
}

namespace sample
{
    namespace ara_com_socketcan_gateway
    {
        MockCanReceiver::MockCanReceiver(std::chrono::milliseconds period) noexcept
            : mSequence{0U},
              mPeriod{period},
              mNextFrameTime{std::chrono::steady_clock::now()}
        {
        }

        ara::core::Result<void> MockCanReceiver::Open()
        {
            // Reset sequence/timer state when backend is opened.
            mSequence = 0U;
            mNextFrameTime = std::chrono::steady_clock::now();
            return ara::core::Result<void>::FromValue();
        }

        void MockCanReceiver::Close() noexcept
        {
        }

        ara::core::Result<bool> MockCanReceiver::Receive(
            CanFrame &frame,
            std::chrono::milliseconds timeout)
        {
            // Respect caller timeout contract even in mock mode.
            auto now{std::chrono::steady_clock::now()};
            if (now < mNextFrameTime)
            {
                auto waitDuration{
                    std::chrono::duration_cast<std::chrono::milliseconds>(
                        mNextFrameTime - now)};

                if (timeout <= std::chrono::milliseconds{0})
                {
                    return ara::core::Result<bool>::FromValue(false);
                }

                if (waitDuration > timeout)
                {
                    std::this_thread::sleep_for(timeout);
                    return ara::core::Result<bool>::FromValue(false);
                }

                std::this_thread::sleep_for(waitDuration);
            }

            frame = CanFrame{};
            frame.Timestamp = std::chrono::steady_clock::now();

            if ((mSequence % 2U) == 0U)
            {
                // Even sequence: emit powertrain frame.
                const std::uint16_t speed{
                    static_cast<std::uint16_t>(4500U + (mSequence % 700U))};
                const std::uint16_t rpm{
                    static_cast<std::uint16_t>(1000U + ((mSequence * 13U) % 5000U))};
                const std::uint8_t gear{
                    static_cast<std::uint8_t>((mSequence % 6U) + 1U)};

                frame.Id = cPowertrainCanId;
                frame.Dlc = 6U;
                frame.Data[0] = static_cast<std::uint8_t>(speed & 0xFFU);
                frame.Data[1] = static_cast<std::uint8_t>((speed >> 8U) & 0xFFU);
                frame.Data[2] = static_cast<std::uint8_t>(rpm & 0xFFU);
                frame.Data[3] = static_cast<std::uint8_t>((rpm >> 8U) & 0xFFU);
                frame.Data[4] = gear;
                frame.Data[5] = (mSequence % 10U) == 0U ? 0x02U : 0x01U;
            }
            else
            {
                // Odd sequence: emit chassis frame.
                const std::int16_t steering{
                    static_cast<std::int16_t>((static_cast<std::int32_t>(mSequence % 400U) - 200) * 10)};
                const std::uint16_t steeringRaw{static_cast<std::uint16_t>(steering)};

                frame.Id = cChassisCanId;
                frame.Dlc = 2U;
                frame.Data[0] = static_cast<std::uint8_t>(steeringRaw & 0xFFU);
                frame.Data[1] = static_cast<std::uint8_t>((steeringRaw >> 8U) & 0xFFU);
            }

            // Schedule next frame generation and return one frame to caller.
            ++mSequence;
            mNextFrameTime = frame.Timestamp + mPeriod;
            return ara::core::Result<bool>::FromValue(true);
        }

        const char *MockCanReceiver::BackendName() const noexcept
        {
            return "mock";
        }
    }
}
