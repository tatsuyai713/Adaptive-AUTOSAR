#include "./vehicle_status_can_decoder.h"

namespace
{
    // CAN payload in this sample is little-endian for 16-bit fields.
    std::uint16_t ReadLittleEndianU16(
        const std::array<std::uint8_t, 8U> &data,
        std::size_t offset) noexcept
    {
        const std::uint16_t low{data[offset]};
        const std::uint16_t high{data[offset + 1U]};
        return static_cast<std::uint16_t>(low | (high << 8U));
    }
}

namespace sample
{
    namespace ara_com_socketcan_gateway
    {
        VehicleStatusCanDecoder::VehicleStatusCanDecoder() noexcept
            : VehicleStatusCanDecoder{Config{}}
        {
        }

        VehicleStatusCanDecoder::VehicleStatusCanDecoder(Config config) noexcept
            : mConfig{config},
              mLatestFrame{},
              mSequenceCounter{0U},
              mPowertrainSeen{false},
              mChassisSeen{false}
        {
        }

        bool VehicleStatusCanDecoder::TryDecode(
            const CanFrame &canFrame,
            sample::vehicle_status::VehicleStatusFrame &outputFrame) noexcept
        {
            // Ignore Remote Transmission Request frames (no payload data).
            if (canFrame.IsRemote)
            {
                return false;
            }

            bool recognizedFrame{false};

            if (canFrame.Id == mConfig.PowertrainCanId)
            {
                // Powertrain frame carries speed/rpm/gear/status flags.
                if (canFrame.Dlc < 6U)
                {
                    return false;
                }

                mLatestFrame.SpeedCentiKph = ReadLittleEndianU16(canFrame.Data, 0U);
                mLatestFrame.EngineRpm = ReadLittleEndianU16(canFrame.Data, 2U);
                mLatestFrame.Gear = canFrame.Data[4U];
                mLatestFrame.StatusFlags = canFrame.Data[5U];
                mPowertrainSeen = true;
                recognizedFrame = true;
            }
            else if (canFrame.Id == mConfig.ChassisCanId)
            {
                // Chassis frame carries steering angle.
                if (canFrame.Dlc < 2U)
                {
                    return false;
                }

                mLatestFrame.SteeringAngleCentiDeg =
                    ReadLittleEndianU16(canFrame.Data, 0U);
                mChassisSeen = true;
                recognizedFrame = true;
            }

            if (!recognizedFrame)
            {
                return false;
            }

            if (mConfig.RequireBothFramesBeforePublish &&
                !(mPowertrainSeen && mChassisSeen))
            {
                // Wait for complete signal set before emitting first sample.
                return false;
            }

            // Emit one publish-ready AUTOSAR payload frame.
            mLatestFrame.SequenceCounter = mSequenceCounter++;
            outputFrame = mLatestFrame;
            return true;
        }
    }
}
