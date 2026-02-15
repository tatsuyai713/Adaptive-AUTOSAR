#ifndef SAMPLE_ECU_SHOWCASE_COMMON_H
#define SAMPLE_ECU_SHOWCASE_COMMON_H

#include <cstdint>
#include <memory>
#include <string>

#include "ara/per/persistency.h"
#include "ara/phm/health_channel.h"
#include "../communication/pubsub/pubsub_autosar_portable_api.h"
#include "../communication/pubsub/pubsub_common.h"
#include "../communication/can/can_frame_receiver.h"
#include "../communication/vehicle_status/vehicle_status_types.h"

namespace sample
{
    namespace ecu_showcase
    {
        // Read `--name=value` style command-line argument.
        // Returns true only when the key exists and extracts the value part.
        bool TryReadArgument(
            int argc,
            char *argv[],
            const std::string &name,
            std::string &value);

        // Parse unsigned integer values with fallback on invalid input.
        std::uint32_t ParseU32(
            const std::string &text,
            std::uint32_t fallback) noexcept;

        // Parse unsigned 64-bit values with fallback on invalid input.
        std::uint64_t ParseU64(
            const std::string &text,
            std::uint64_t fallback) noexcept;

        // Parse common boolean representations:
        // true: 1/true/on, false: 0/false/off
        bool ParseBool(
            const std::string &text,
            bool fallback) noexcept;

        // Thin wrapper around ara::phm::HealthChannel used by samples.
        // Samples can report status without repeating setup boilerplate.
        class HealthReporter final
        {
        private:
            std::unique_ptr<ara::phm::HealthChannel> mChannel;

        public:
            explicit HealthReporter(const std::string &instanceSpecifier) noexcept;

            void ReportOk() noexcept;
            void ReportFailed() noexcept;
            void ReportDeactivated() noexcept;
        };

        // Thin wrapper around ara::per key-value storage for sample counters.
        // If storage cannot be opened, methods become no-op with fallback values.
        class PersistentCounterStore final
        {
        private:
            ara::per::SharedHandle<ara::per::KeyValueStorage> mStorage;

        public:
            explicit PersistentCounterStore(const std::string &instanceSpecifier) noexcept;

            bool IsAvailable() const noexcept;

            std::uint64_t Load(
                const std::string &key,
                std::uint64_t fallbackValue) const noexcept;

            void Save(
                const std::string &key,
                std::uint64_t value) noexcept;

            void Sync() noexcept;
        };

        // Factory for CAN receiver backend:
        // - "socketcan": real Linux CAN interface
        // - "mock": generated test frames
        std::unique_ptr<sample::ara_com_socketcan_gateway::CanFrameReceiver>
        CreateCanReceiver(
            const std::string &backendName,
            const std::string &ifname,
            std::uint32_t mockIntervalMs);

        // Build transport profile for DDS output.
        sample::ara_com_pubsub::portable::BackendProfile BuildDdsProfile(
            std::uint32_t domainId,
            const std::string &topicName);

        // Build transport profile for SOME/IP input.
        sample::ara_com_pubsub::portable::BackendProfile BuildSomeIpProfile();

        // Convert standard sample frame type to portable pub/sub frame type.
        sample::ara_com_pubsub::VehicleStatusFrame ToPortableFrame(
            const sample::vehicle_status::VehicleStatusFrame &frame) noexcept;
    }
}

#endif
