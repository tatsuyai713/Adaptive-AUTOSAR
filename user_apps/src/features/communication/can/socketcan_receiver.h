#ifndef SAMPLE_ARA_COM_SOCKETCAN_GATEWAY_SOCKETCAN_RECEIVER_H
#define SAMPLE_ARA_COM_SOCKETCAN_GATEWAY_SOCKETCAN_RECEIVER_H

#include <string>
#include "./can_frame_receiver.h"

namespace sample
{
    namespace ara_com_socketcan_gateway
    {
        // Linux SocketCAN receiver implementation used by gateway samples.
        // It adapts raw CAN frames into the generic CanFrameReceiver interface.
        class SocketCanReceiver final : public CanFrameReceiver
        {
        private:
            std::string mInterfaceName;
            int mSocketFd;

        public:
            explicit SocketCanReceiver(std::string interfaceName) noexcept;

            SocketCanReceiver(const SocketCanReceiver &) = delete;
            SocketCanReceiver &operator=(const SocketCanReceiver &) = delete;

            SocketCanReceiver(SocketCanReceiver &&other) noexcept;
            SocketCanReceiver &operator=(SocketCanReceiver &&other) noexcept;

            ~SocketCanReceiver() noexcept override;

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
