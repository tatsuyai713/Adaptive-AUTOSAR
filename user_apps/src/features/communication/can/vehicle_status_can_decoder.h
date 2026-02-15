#ifndef SAMPLE_ARA_COM_SOCKETCAN_GATEWAY_VEHICLE_STATUS_CAN_DECODER_H
#define SAMPLE_ARA_COM_SOCKETCAN_GATEWAY_VEHICLE_STATUS_CAN_DECODER_H

#include <cstdint>
#include "./can_frame_receiver.h"
#include "../vehicle_status/vehicle_status_types.h"

namespace sample
{
    namespace ara_com_socketcan_gateway
    {
        class VehicleStatusCanDecoder
        {
        public:
            struct Config
            {
                std::uint32_t PowertrainCanId{0x100U};
                std::uint32_t ChassisCanId{0x101U};
                bool RequireBothFramesBeforePublish{true};
            };

        private:
            Config mConfig;
            sample::vehicle_status::VehicleStatusFrame mLatestFrame;
            std::uint32_t mSequenceCounter;
            bool mPowertrainSeen;
            bool mChassisSeen;

        public:
            VehicleStatusCanDecoder() noexcept;
            explicit VehicleStatusCanDecoder(Config config) noexcept;

            /// @brief Update decoder state with one CAN frame.
            /// @param[in] canFrame Input frame from CAN ingress adapter
            /// @param[out] outputFrame A new AUTOSAR event payload when returning true
            /// @returns True when outputFrame contains a publish-ready sample
            bool TryDecode(
                const CanFrame &canFrame,
                sample::vehicle_status::VehicleStatusFrame &outputFrame) noexcept;
        };
    }
}

#endif
