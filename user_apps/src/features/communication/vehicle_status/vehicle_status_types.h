#ifndef SAMPLE_VEHICLE_STATUS_TYPES_H
#define SAMPLE_VEHICLE_STATUS_TYPES_H

#include <cstdint>

#if defined(__has_include)
#if __has_include("ara_com_generated/vehicle_status_manifest_binding.h")
#include "ara_com_generated/vehicle_status_manifest_binding.h"
#define SAMPLE_VEHICLE_STATUS_HAS_GENERATED_BINDING 1
#endif
#endif

namespace sample
{
    namespace vehicle_status
    {
        /// @brief Sample vehicle status event payload.
        ///        Identical to the pubsub sample's VehicleStatusFrame
        ///        but placed in the standard-API sample namespace.
        struct VehicleStatusFrame
        {
            std::uint32_t SequenceCounter{0U};
            std::uint32_t SpeedCentiKph{0U};
            std::uint32_t EngineRpm{0U};
            std::uint16_t SteeringAngleCentiDeg{0U};
            std::uint8_t Gear{0U};
            std::uint8_t StatusFlags{0U};
        };

#if defined(SAMPLE_VEHICLE_STATUS_HAS_GENERATED_BINDING)
        // Service interface constants generated from ARXML at build time.
        constexpr std::uint16_t cServiceId{generated::kServiceId};
        constexpr std::uint16_t cInstanceId{generated::kInstanceId};
        constexpr std::uint16_t cStatusEventId{generated::kStatusEventId};
        constexpr std::uint16_t cStatusEventGroupId{generated::kStatusEventGroupId};
        constexpr std::uint8_t cMajorVersion{generated::kMajorVersion};
#else
        // Fallback constants when generated binding header is unavailable.
        constexpr std::uint16_t cServiceId{0x1234};
        constexpr std::uint16_t cInstanceId{0x0001};
        constexpr std::uint16_t cStatusEventId{0x8001};
        constexpr std::uint16_t cStatusEventGroupId{0x0001};
        constexpr std::uint8_t cMajorVersion{0x01};
#endif
    }
}

// VehicleStatusFrame is trivially copyable, so the default Serializer<T>
// (memcpy-based) works automatically. No specialization needed.

#endif
