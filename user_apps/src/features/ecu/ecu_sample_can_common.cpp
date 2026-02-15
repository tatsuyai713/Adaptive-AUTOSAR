#include <chrono>

#include "./ecu_sample_common.h"
#include "../communication/can/mock_can_receiver.h"
#include "../communication/can/socketcan_receiver.h"

namespace sample
{
    namespace ecu_showcase
    {
        std::unique_ptr<sample::ara_com_socketcan_gateway::CanFrameReceiver>
        CreateCanReceiver(
            const std::string &backendName,
            const std::string &ifname,
            std::uint32_t mockIntervalMs)
        {
            // Real Linux SocketCAN backend (ex: can0, vcan0).
            if (backendName == "socketcan")
            {
                return std::unique_ptr<sample::ara_com_socketcan_gateway::CanFrameReceiver>{
                    new sample::ara_com_socketcan_gateway::SocketCanReceiver{ifname}};
            }

            // Mock backend for local development without CAN hardware.
            if (backendName == "mock")
            {
                return std::unique_ptr<sample::ara_com_socketcan_gateway::CanFrameReceiver>{
                    new sample::ara_com_socketcan_gateway::MockCanReceiver{
                        std::chrono::milliseconds{mockIntervalMs}}};
            }

            // Unknown backend name.
            return nullptr;
        }
    }
}
