/// @file src/ara/com/service_proxy_base.h
/// @brief Declarations for service proxy base.
/// @details This file is part of the Adaptive AUTOSAR educational implementation.

#ifndef ARA_COM_SERVICE_PROXY_BASE_H
#define ARA_COM_SERVICE_PROXY_BASE_H

#include <cstdint>
#include <functional>
#include <vector>
#include "./types.h"
#include "./service_version.h"
#include "./instance_identifier.h"
#include "./service_handle_type.h"
#include "../core/instance_specifier.h"
#include "../core/result.h"

namespace ara
{
    namespace com
    {
        /// @brief Base class for standard AUTOSAR AP proxy classes.
        ///        Generated proxy classes inherit from this and add typed
        ///        Event<T>, Method<R(Args...)>, Field<T> members.
        class ServiceProxyBase
        {
        private:
            ServiceHandleType mHandle;

        protected:
            /// @brief Constructor from a service handle
            /// @param handle Handle identifying the service instance
            explicit ServiceProxyBase(ServiceHandleType handle) noexcept;

        public:
            ServiceProxyBase(const ServiceProxyBase &) = delete;
            ServiceProxyBase &operator=(const ServiceProxyBase &) = delete;

            /// @brief Move constructor.
            /// @param other Source object to move from.
            ServiceProxyBase(ServiceProxyBase &&other) noexcept;
            /// @brief Move assignment.
            /// @param other Source object to move from.
            /// @returns Reference to `*this`.
            ServiceProxyBase &operator=(ServiceProxyBase &&other) noexcept;

            /// @brief Virtual destructor.
            virtual ~ServiceProxyBase() noexcept;

            /// @brief Get the handle for this proxy
            /// @returns Bound service handle.
            const ServiceHandleType &GetHandle() const noexcept;

            /// @brief One-shot service discovery
            /// @param serviceId Service ID to find
            /// @param instanceId Specific instance ID (0xFFFF for any)
            /// @returns Container of matching service handles
            static core::Result<ServiceHandleContainer<ServiceHandleType>>
            FindService(
                std::uint16_t serviceId,
                std::uint16_t instanceId = 0xFFFF);

            /// @brief One-shot service discovery via InstanceSpecifier (SWS_CM_00122).
            /// @param specifier InstanceSpecifier that resolves to a port prototype
            /// @returns Container of matching service handles
            static core::Result<ServiceHandleContainer<ServiceHandleType>>
            FindService(const core::InstanceSpecifier &specifier);

            /// @brief One-shot service discovery via InstanceIdentifier (SWS_CM_00124).
            /// @param identifier Transport-level instance identifier
            /// @returns Container of matching service handles
            static core::Result<ServiceHandleContainer<ServiceHandleType>>
            FindService(const InstanceIdentifier &identifier);

            /// @brief Start continuous service discovery
            /// @param handler Callback invoked when availability changes
            /// @param serviceId Service ID to find
            /// @param instanceId Specific instance ID (0xFFFF for any)
            /// @returns Result containing a FindServiceHandle
            static core::Result<FindServiceHandle> StartFindService(
                std::function<void(ServiceHandleContainer<ServiceHandleType>)> handler,
                std::uint16_t serviceId,
                std::uint16_t instanceId = 0xFFFF);

            /// @brief Start continuous service discovery via InstanceSpecifier (SWS_CM_00123).
            /// @param handler Callback invoked when availability changes
            /// @param specifier InstanceSpecifier that resolves to a port prototype
            /// @returns Result containing a FindServiceHandle
            static core::Result<FindServiceHandle> StartFindService(
                std::function<void(ServiceHandleContainer<ServiceHandleType>)> handler,
                const core::InstanceSpecifier &specifier);

            /// @brief Stop continuous service discovery for a specific search handle
            /// @param handle Handle returned by StartFindService
            /// @returns Error if no active search exists or handle mismatches
            static core::Result<void> StopFindService(FindServiceHandle handle);

            /// @brief Stop continuous service discovery
            /// @note Legacy helper that stops the currently active search, if any.
            static void StopFindService();

            /// @brief Check version compatibility with a service handle.
            /// @param requiredVersion The version this proxy requires.
            /// @param offeredVersion The version offered by the skeleton.
            /// @param policy Compatibility policy to use.
            /// @returns True if versions are compatible.
            static bool CheckServiceVersion(
                const ServiceVersion &requiredVersion,
                const ServiceVersion &offeredVersion,
                VersionCheckPolicy policy =
                    VersionCheckPolicy::kMinorBackward) noexcept;
        };
    }
}

#endif
