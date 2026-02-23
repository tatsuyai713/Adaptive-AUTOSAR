#ifndef SAMPLE_VEHICLE_STATUS_PROXY_H
#define SAMPLE_VEHICLE_STATUS_PROXY_H

#include "ara/com/service_proxy_base.h"
#include "ara/com/event.h"
#include "ara/com/types.h"
#include "ara/core/instance_specifier.h"
#include "ara/core/result.h"
#include "./vehicle_status_types.h"

// Compatibility fallback for pre-helper runtime packages.
#if !defined(ARA_COM_HAS_GENERATED_EVENT_BINDING_HELPERS)
#include "ara/com/internal/binding_factory.h"
#endif

namespace sample
{
    namespace vehicle_status
    {
        /// @brief Standard AUTOSAR AP proxy for the VehicleStatus service.
        ///
        /// This class demonstrates what a code generator would produce from an
        /// ARXML service interface definition. Application code using this proxy
        /// is portable to commercial AUTOSAR AP stacks (Vector, Bosch, Elektrobit).
        ///
        /// Usage:
        /// @code
        /// auto handles = VehicleStatusServiceProxy::FindService(specifier);
        /// VehicleStatusServiceProxy proxy(handles.Value().front());
        /// proxy.StatusEvent.Subscribe(10);
        /// proxy.StatusEvent.SetReceiveHandler([&proxy]() {
        ///     proxy.StatusEvent.GetNewSamples(
        ///         [](ara::com::SamplePtr<VehicleStatusFrame> sample) {
        ///             std::cout << sample->SpeedCentiKph << std::endl;
        ///         });
        /// });
        /// @endcode
        class VehicleStatusServiceProxy : public ara::com::ServiceProxyBase
        {
        public:
            /// @brief Handle type alias (per AUTOSAR AP, each proxy defines its HandleType)
            using HandleType = ara::com::ServiceHandleType;

            /// @brief Typed event: VehicleStatusFrame notification
            ara::com::ProxyEvent<VehicleStatusFrame> StatusEvent;

            /// @brief Construct proxy from a discovered service handle
            explicit VehicleStatusServiceProxy(HandleType handle)
                : ServiceProxyBase{handle},
#if defined(ARA_COM_HAS_GENERATED_EVENT_BINDING_HELPERS)
                  StatusEvent{
                      CreateSomeIpProxyEventBinding(
                          cStatusEventId,
                          cStatusEventGroupId,
                          cMajorVersion)}
#else
                  // Legacy fallback path (installed runtime without helper macro).
                  StatusEvent{
                      ara::com::internal::BindingFactory::CreateProxyEventBinding(
                          ara::com::internal::TransportBinding::kVsomeip,
                          ara::com::internal::EventBindingConfig{
                              handle.GetServiceId(),
                              handle.GetInstanceId(),
                              cStatusEventId,
                              cStatusEventGroupId,
                              cMajorVersion})}
#endif
            {
            }

            VehicleStatusServiceProxy(const VehicleStatusServiceProxy &) = delete;
            VehicleStatusServiceProxy &operator=(const VehicleStatusServiceProxy &) = delete;
            VehicleStatusServiceProxy(VehicleStatusServiceProxy &&) = default;
            VehicleStatusServiceProxy &operator=(VehicleStatusServiceProxy &&) = default;

            /// @brief One-shot service discovery
            /// @param specifier Instance specifier identifying the service
            /// @returns Container of matching service handles
            static ara::core::Result<
                ara::com::ServiceHandleContainer<HandleType>>
            FindService(
                ara::core::InstanceSpecifier specifier)
            {
                (void)specifier;
                return ServiceProxyBase::FindService(
                    cServiceId, cInstanceId);
            }

            /// @brief Start continuous service discovery
            /// @param handler Callback invoked when availability changes
            /// @param specifier Instance specifier identifying the service
            /// @returns FindServiceHandle for stopping the search
            static ara::core::Result<ara::com::FindServiceHandle>
            StartFindService(
                ara::com::FindServiceHandler<HandleType> handler,
                ara::core::InstanceSpecifier specifier)
            {
                (void)specifier;
                return ServiceProxyBase::StartFindService(
                    std::move(handler),
                    cServiceId,
                    cInstanceId);
            }

            /// @brief Stop continuous service discovery
            static void StopFindService(ara::com::FindServiceHandle handle)
            {
                (void)ServiceProxyBase::StopFindService(handle);
            }
        };
    }
}

#endif
