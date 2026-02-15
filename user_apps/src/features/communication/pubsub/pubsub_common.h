#ifndef ARA_COM_PUBSUB_COMMON_H
#define ARA_COM_PUBSUB_COMMON_H

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>
#include "ara/log/logging_framework.h"

namespace sample
{
    namespace ara_com_pubsub
    {
        constexpr std::uint16_t cServiceId{0x1234};
        constexpr std::uint16_t cInstanceId{0x0001};
        constexpr std::uint16_t cEventId{0x8001};
        constexpr std::uint16_t cEventGroupId{0x0001};
        constexpr std::uint8_t cMajorVersion{0x01};
        constexpr std::uint32_t cDdsDomainId{0U};
        constexpr const char *cDdsTopicName{
            "adaptive_autosar/sample/ara_com_pubsub/VehicleStatusFrame"};

        constexpr const char *cProviderInstanceSpecifier{
            "AdaptiveAutosar/Sample/PubSubProvider"};
        constexpr const char *cConsumerInstanceSpecifier{
            "AdaptiveAutosar/Sample/PubSubConsumer"};

        /// @brief Sample event payload used for SOME/IP and zero-copy publication.
        struct VehicleStatusFrame
        {
            std::uint32_t SequenceCounter;
            std::uint32_t SpeedCentiKph;
            std::uint32_t EngineRpm;
            std::uint16_t SteeringAngleCentiDeg;
            std::uint8_t Gear;
            std::uint8_t StatusFlags;
        };

        /// @brief Runtime lifecycle helper for ara::core initialization/deinitialization.
        class AdaptiveRuntime final
        {
        private:
            bool mInitialized;

        public:
            AdaptiveRuntime();
            ~AdaptiveRuntime() noexcept;

            AdaptiveRuntime(const AdaptiveRuntime &) = delete;
            AdaptiveRuntime &operator=(const AdaptiveRuntime &) = delete;
        };

        void LogMessage(
            ara::log::LoggingFramework &framework,
            const ara::log::Logger &logger,
            ara::log::LogLevel level,
            const std::string &message);

        std::vector<std::uint8_t> SerializeFrame(
            const VehicleStatusFrame &frame);

        bool DeserializeFrame(
            const std::uint8_t *payload,
            std::size_t payloadSize,
            VehicleStatusFrame &frame) noexcept;

        bool DeserializeFrame(
            const std::vector<std::uint8_t> &payload,
            VehicleStatusFrame &frame) noexcept;

        std::string BuildFrameSummary(const VehicleStatusFrame &frame);

        bool TryReadArgument(
            int argc,
            char *argv[],
            const std::string &argumentName,
            std::string &value);

        std::uint32_t ParsePositiveUintOrDefault(
            const std::string &input,
            std::uint32_t fallbackValue) noexcept;
    }
}

#endif
