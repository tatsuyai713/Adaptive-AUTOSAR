#ifndef SERVICE_HANDLE_TYPE_H
#define SERVICE_HANDLE_TYPE_H

#include <stdint.h>
#include "../core/instance_specifier.h"

namespace ara
{
    namespace com
    {
        /// @brief A handle to identify a discovered service instance
        /// @note Per AUTOSAR AP, this is returned by FindService and used to create proxy instances
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

            bool operator==(const ServiceHandleType &other) const noexcept
            {
                return mServiceId == other.mServiceId &&
                       mInstanceId == other.mInstanceId;
            }

            bool operator!=(const ServiceHandleType &other) const noexcept
            {
                return !(*this == other);
            }

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
