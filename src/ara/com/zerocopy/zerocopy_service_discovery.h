/// @file src/ara/com/zerocopy/zerocopy_service_discovery.h
/// @brief Service discovery for zero-copy (iceoryx) transport.
/// @details Wraps iceoryx CAPRO (CAnnonical PROtocol) service discovery
///          to provide FindService/OfferService semantics compatible with
///          the ara::com service discovery model.
///
///          In the iceoryx transport, service discovery is implicit:
///          publishers automatically "offer" by publishing, and subscribers
///          automatically "find" by subscribing.  This module adds explicit
///          tracking of offered/found services for the ara::com FindService
///          and OfferService APIs.
///
///          This file is part of the Adaptive AUTOSAR educational implementation.

#ifndef ARA_COM_ZEROCOPY_SERVICE_DISCOVERY_H
#define ARA_COM_ZEROCOPY_SERVICE_DISCOVERY_H

#include <cstdint>
#include <functional>
#include <mutex>
#include <string>
#include <vector>
#include "./zero_copy.h"

namespace ara
{
    namespace com
    {
        namespace zerocopy
        {
            /// @brief Service availability state.
            enum class ServiceAvailability : std::uint8_t
            {
                kAvailable = 0U,
                kNotAvailable = 1U
            };

            /// @brief Discovered service endpoint information.
            struct DiscoveredService
            {
                ChannelDescriptor Channel;
                ServiceAvailability State{ServiceAvailability::kNotAvailable};
            };

            /// @brief Callback type for service availability changes.
            using ServiceAvailabilityHandler =
                std::function<void(const DiscoveredService &)>;

            /// @brief Zero-copy service discovery manager.
            ///        Tracks offered and discovered services for the iceoryx
            ///        transport layer.
            class ZeroCopyServiceDiscovery
            {
            private:
                mutable std::mutex mMutex;
                std::vector<DiscoveredService> mOfferedServices;
                std::vector<DiscoveredService> mFoundServices;
                ServiceAvailabilityHandler mHandler;

            public:
                ZeroCopyServiceDiscovery() = default;
                ~ZeroCopyServiceDiscovery() noexcept = default;

                ZeroCopyServiceDiscovery(
                    const ZeroCopyServiceDiscovery &) = delete;
                ZeroCopyServiceDiscovery &operator=(
                    const ZeroCopyServiceDiscovery &) = delete;

                /// @brief Register a service as offered (skeleton side).
                void OfferService(const ChannelDescriptor &channel);

                /// @brief Remove a previously offered service.
                void StopOfferService(const ChannelDescriptor &channel);

                /// @brief Mark a remote service as found/available (proxy side).
                void ServiceFound(const ChannelDescriptor &channel);

                /// @brief Mark a remote service as lost/unavailable.
                void ServiceLost(const ChannelDescriptor &channel);

                /// @brief Set the service availability change handler.
                void SetAvailabilityHandler(
                    ServiceAvailabilityHandler handler);

                /// @brief Get all currently offered services.
                std::vector<DiscoveredService> GetOfferedServices() const;

                /// @brief Get all currently found services.
                std::vector<DiscoveredService> GetFoundServices() const;

                /// @brief Check if a specific service is offered.
                bool IsServiceOffered(
                    const ChannelDescriptor &channel) const;

                /// @brief Check if a specific service is found/available.
                bool IsServiceFound(
                    const ChannelDescriptor &channel) const;
            };
        }
    }
}

#endif
