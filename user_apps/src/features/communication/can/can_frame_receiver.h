#ifndef SAMPLE_ARA_COM_SOCKETCAN_GATEWAY_CAN_FRAME_RECEIVER_H
#define SAMPLE_ARA_COM_SOCKETCAN_GATEWAY_CAN_FRAME_RECEIVER_H

#include <array>
#include <chrono>
#include <cstdint>
#include "ara/core/result.h"

namespace sample
{
    namespace ara_com_socketcan_gateway
    {
        struct CanFrame
        {
            std::uint32_t Id{0U};
            bool IsExtended{false};
            bool IsRemote{false};
            std::uint8_t Dlc{0U};
            std::array<std::uint8_t, 8U> Data{{0U}};
            std::chrono::steady_clock::time_point Timestamp{};
        };

        /// @brief Abstract ingress API for CAN frames.
        ///
        /// Application logic can be written against this interface. Linux uses
        /// SocketCAN implementation, while other AUTOSAR AP stacks can provide
        /// their own adapter (for example Vector/Elektrobit specific drivers).
        class CanFrameReceiver
        {
        public:
            virtual ~CanFrameReceiver() noexcept = default;

            virtual ara::core::Result<void> Open() = 0;
            virtual void Close() noexcept = 0;
            virtual ara::core::Result<bool> Receive(
                CanFrame &frame,
                std::chrono::milliseconds timeout) = 0;
            virtual const char *BackendName() const noexcept = 0;
        };
    }
}

#endif
