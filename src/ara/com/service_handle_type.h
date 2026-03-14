/// @file src/ara/com/service_handle_type.h
/// @brief Declarations for service handle type.
/// @details This file is part of the Adaptive AUTOSAR educational implementation.

#ifndef SERVICE_HANDLE_TYPE_H
#define SERVICE_HANDLE_TYPE_H

#include <stdint.h>
#include <string>
#include "../core/instance_specifier.h"

namespace ara
{
    namespace com
    {
        /// Forward declaration for GetInstanceIdentifier.
        class InstanceIdentifier;

        /// @brief Immutable identifier for a discovered service instance.
        /// @details
        /// A handle is returned by discovery APIs and then passed to generated
        /// Proxy constructors. It carries transport-level service/instance IDs.
        class ServiceHandleType
        {
        private:
            uint16_t mServiceId;
            uint16_t mInstanceId;

        public:
            /// @brief Constructor
            /// @param serviceId SOME/IP service ID
            /// @param instanceId SOME/IP instance ID
            ServiceHandleType(
                uint16_t serviceId,
                uint16_t instanceId) noexcept
                : mServiceId{serviceId},
                  mInstanceId{instanceId}
            {
            }

            /// @brief Get the service ID
            /// @returns SOME/IP service ID
            uint16_t GetServiceId() const noexcept
            {
                return mServiceId;
            }

            /// @brief Get the instance ID
            /// @returns SOME/IP instance ID
            uint16_t GetInstanceId() const noexcept
            {
                return mInstanceId;
            }

            /// @brief Get the service interface ID as string (SWS_CM_00312).
            /// @returns String representation of service/instance pair.
            std::string GetServiceInterfaceId() const
            {
                return std::to_string(mServiceId) + ":" +
                       std::to_string(mInstanceId);
            }

            /// @brief Equality comparison.
            /// @param other Handle to compare with.
            /// @returns `true` when service ID and instance ID are both equal.
            bool operator==(const ServiceHandleType &other) const noexcept
            {
                return mServiceId == other.mServiceId &&
                       mInstanceId == other.mInstanceId;
            }

            /// @brief Inequality comparison.
            /// @param other Handle to compare with.
            /// @returns `true` when at least one identifier differs.
            bool operator!=(const ServiceHandleType &other) const noexcept
            {
                return !(*this == other);
            }

            /// @brief Strict weak ordering for associative containers.
            /// @param other Handle to compare with.
            /// @returns Ordering by `(serviceId, instanceId)`.
            bool operator<(const ServiceHandleType &other) const noexcept
            {
                if (mServiceId != other.mServiceId)
                {
                    return mServiceId < other.mServiceId;
                }
                return mInstanceId < other.mInstanceId;
            }
        };
    }
}

#endif
