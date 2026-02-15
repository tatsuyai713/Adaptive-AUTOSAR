#include <limits>
#include <sstream>
#include <stdexcept>
#include "ara/core/initialization.h"
#include "./pubsub_common.h"

namespace
{
    // Fixed wire size used by both event transport and zero-copy payload.
    constexpr std::size_t cFramePayloadSize{16U};

    // Serialize 16-bit integer as big-endian to keep payload format stable.
    void AppendUint16(
        std::vector<std::uint8_t> &payload,
        std::uint16_t value)
    {
        payload.push_back(static_cast<std::uint8_t>((value >> 8U) & 0xFFU));
        payload.push_back(static_cast<std::uint8_t>(value & 0xFFU));
    }

    // Serialize 32-bit integer as big-endian to keep payload format stable.
    void AppendUint32(
        std::vector<std::uint8_t> &payload,
        std::uint32_t value)
    {
        payload.push_back(static_cast<std::uint8_t>((value >> 24U) & 0xFFU));
        payload.push_back(static_cast<std::uint8_t>((value >> 16U) & 0xFFU));
        payload.push_back(static_cast<std::uint8_t>((value >> 8U) & 0xFFU));
        payload.push_back(static_cast<std::uint8_t>(value & 0xFFU));
    }

    // Deserialize one 16-bit value and advance parsing offset.
    bool ReadUint16(
        const std::uint8_t *payload,
        std::size_t payloadSize,
        std::size_t &offset,
        std::uint16_t &value) noexcept
    {
        if (offset > payloadSize || (payloadSize - offset) < 2U)
        {
            return false;
        }

        value = static_cast<std::uint16_t>(payload[offset]) << 8U;
        ++offset;
        value |= static_cast<std::uint16_t>(payload[offset]);
        ++offset;
        return true;
    }

    // Deserialize one 32-bit value and advance parsing offset.
    bool ReadUint32(
        const std::uint8_t *payload,
        std::size_t payloadSize,
        std::size_t &offset,
        std::uint32_t &value) noexcept
    {
        if (offset > payloadSize || (payloadSize - offset) < 4U)
        {
            return false;
        }

        value = static_cast<std::uint32_t>(payload[offset]) << 24U;
        ++offset;
        value |= static_cast<std::uint32_t>(payload[offset]) << 16U;
        ++offset;
        value |= static_cast<std::uint32_t>(payload[offset]) << 8U;
        ++offset;
        value |= static_cast<std::uint32_t>(payload[offset]);
        ++offset;
        return true;
    }
}

namespace sample
{
    namespace ara_com_pubsub
    {
        AdaptiveRuntime::AdaptiveRuntime() : mInitialized{false}
        {
            // Initialize ara::core runtime once for this process scope.
            auto _initializeResult{ara::core::Initialize()};
            if (!_initializeResult.HasValue())
            {
                throw std::runtime_error(
                    _initializeResult.Error().Message());
            }

            mInitialized = true;
        }

        AdaptiveRuntime::~AdaptiveRuntime() noexcept
        {
            if (mInitialized)
            {
                // Pair with ara::core::Initialize().
                ara::core::Deinitialize();
            }
        }

        void LogMessage(
            ara::log::LoggingFramework &framework,
            const ara::log::Logger &logger,
            ara::log::LogLevel level,
            const std::string &message)
        {
            // Keep sample logging usage compact and consistent.
            auto _stream{logger.WithLevel(level)};
            _stream << message;
            framework.Log(logger, level, _stream);
        }

        std::vector<std::uint8_t> SerializeFrame(
            const VehicleStatusFrame &frame)
        {
            // Serialize in deterministic field order shared by all samples.
            std::vector<std::uint8_t> _payload;
            _payload.reserve(cFramePayloadSize);

            AppendUint32(_payload, frame.SequenceCounter);
            AppendUint32(_payload, frame.SpeedCentiKph);
            AppendUint32(_payload, frame.EngineRpm);
            AppendUint16(_payload, frame.SteeringAngleCentiDeg);
            _payload.push_back(frame.Gear);
            _payload.push_back(frame.StatusFlags);

            return _payload;
        }

        bool DeserializeFrame(
            const std::uint8_t *payload,
            std::size_t payloadSize,
            VehicleStatusFrame &frame) noexcept
        {
            // Payload must match the exact frame binary layout.
            if (!payload || payloadSize != cFramePayloadSize)
            {
                return false;
            }

            std::size_t _offset{0U};
            if (!ReadUint32(
                    payload,
                    payloadSize,
                    _offset,
                    frame.SequenceCounter))
            {
                return false;
            }
            if (!ReadUint32(
                    payload,
                    payloadSize,
                    _offset,
                    frame.SpeedCentiKph))
            {
                return false;
            }
            if (!ReadUint32(
                    payload,
                    payloadSize,
                    _offset,
                    frame.EngineRpm))
            {
                return false;
            }
            if (!ReadUint16(
                    payload,
                    payloadSize,
                    _offset,
                    frame.SteeringAngleCentiDeg))
            {
                return false;
            }
            if (_offset > payloadSize || (payloadSize - _offset) < 2U)
            {
                return false;
            }
            frame.Gear = payload[_offset++];
            frame.StatusFlags = payload[_offset++];

            return _offset == payloadSize;
        }

        bool DeserializeFrame(
            const std::vector<std::uint8_t> &payload,
            VehicleStatusFrame &frame) noexcept
        {
            return DeserializeFrame(payload.data(), payload.size(), frame);
        }

        std::string BuildFrameSummary(const VehicleStatusFrame &frame)
        {
            // Human-readable summary for logs and troubleshooting.
            std::ostringstream _stream;
            _stream << "seq=" << frame.SequenceCounter
                    << ", speed_centi_kph=" << frame.SpeedCentiKph
                    << ", engine_rpm=" << frame.EngineRpm
                    << ", steering_centi_deg=" << frame.SteeringAngleCentiDeg
                    << ", gear=" << static_cast<std::uint32_t>(frame.Gear)
                    << ", flags=0x" << std::hex
                    << static_cast<std::uint32_t>(frame.StatusFlags);
            return _stream.str();
        }

        bool TryReadArgument(
            int argc,
            char *argv[],
            const std::string &argumentName,
            std::string &value)
        {
            // Supports "--name=value" CLI format.
            const std::string cPrefix{argumentName + "="};
            for (int _index{1}; _index < argc; ++_index)
            {
                const std::string cArgument{argv[_index]};
                if (cArgument.find(cPrefix) == 0U)
                {
                    value = cArgument.substr(cPrefix.size());
                    return true;
                }
            }

            return false;
        }

        std::uint32_t ParsePositiveUintOrDefault(
            const std::string &input,
            std::uint32_t fallbackValue) noexcept
        {
            try
            {
                // Decimal-only numeric parsing for predictable CLI behavior.
                std::size_t _consumed{0U};
                const unsigned long cValue{
                    std::stoul(input, &_consumed, 10)};

                if (_consumed != input.size())
                {
                    return fallbackValue;
                }

                if (cValue > std::numeric_limits<std::uint32_t>::max())
                {
                    return fallbackValue;
                }

                return static_cast<std::uint32_t>(cValue);
            }
            catch (const std::exception &)
            {
                return fallbackValue;
            }
        }
    }
}
