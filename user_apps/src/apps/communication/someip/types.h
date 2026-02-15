#ifndef USER_APPS_COM_SOMEIP_TEMPLATE_TYPES_H
#define USER_APPS_COM_SOMEIP_TEMPLATE_TYPES_H

#include <cstdint>
#include <stdexcept>

#include <ara/com/event.h>
#include <ara/com/internal/binding_factory.h>
#include <ara/com/service_handle_type.h>
#include <ara/com/service_proxy_base.h>
#include <ara/com/service_skeleton_base.h>
#include <ara/com/types.h>
#include <ara/core/instance_specifier.h>
#include <ara/core/result.h>

namespace user_apps
{
    namespace com_template
    {
        // Service/Event IDs used by this template. Replace these IDs with your
        // production ARXML-generated values when creating a real ECU service.
        constexpr std::uint16_t kServiceId{0x5555U};
        constexpr std::uint16_t kInstanceId{0x0001U};
        constexpr std::uint16_t kEventId{0x8100U};
        constexpr std::uint16_t kEventGroupId{0x0001U};
        constexpr std::uint8_t kMajorVersion{1U};

        // This payload is intentionally simple and trivially copyable, so it can
        // use the default ara::com serializer.
        struct VehicleSignalFrame
        {
            std::uint32_t SequenceCounter;
            std::uint16_t SpeedKph;
            std::uint16_t EngineRpm;
            std::uint8_t Gear;
            std::uint8_t StatusFlags;
        };

        // Helper for creating a valid AUTOSAR instance specifier once.
        inline ara::core::InstanceSpecifier CreateInstanceSpecifierOrThrow(
            const char *path)
        {
            auto specifierResult{ara::core::InstanceSpecifier::Create(path)};
            if (!specifierResult.HasValue())
            {
                throw std::runtime_error(specifierResult.Error().Message());
            }

            return specifierResult.Value();
        }

        // Minimal skeleton template that exposes one typed event.
        class VehicleSignalSkeleton : public ara::com::ServiceSkeletonBase
        {
        public:
            ara::com::SkeletonEvent<VehicleSignalFrame> StatusEvent;

            explicit VehicleSignalSkeleton(
                ara::core::InstanceSpecifier specifier)
                : ara::com::ServiceSkeletonBase{
                      std::move(specifier),
                      kServiceId,
                      kInstanceId,
                      kMajorVersion,
                      0U,
                      ara::com::MethodCallProcessingMode::kEvent},
                  StatusEvent{
                      ara::com::internal::BindingFactory::CreateSkeletonEventBinding(
                          ara::com::internal::TransportBinding::kVsomeip,
                          ara::com::internal::EventBindingConfig{
                              kServiceId,
                              kInstanceId,
                              kEventId,
                              kEventGroupId,
                              kMajorVersion})}
            {
            }
        };

        // Minimal proxy template that discovers one service instance and subscribes
        // to one typed event.
        class VehicleSignalProxy : public ara::com::ServiceProxyBase
        {
        public:
            using HandleType = ara::com::ServiceHandleType;

            ara::com::ProxyEvent<VehicleSignalFrame> StatusEvent;

            explicit VehicleSignalProxy(HandleType handle)
                : ara::com::ServiceProxyBase{handle},
                  StatusEvent{
                      ara::com::internal::BindingFactory::CreateProxyEventBinding(
                          ara::com::internal::TransportBinding::kVsomeip,
                          ara::com::internal::EventBindingConfig{
                              handle.GetServiceId(),
                              handle.GetInstanceId(),
                              kEventId,
                              kEventGroupId,
                              kMajorVersion})}
            {
            }

            static ara::core::Result<
                ara::com::ServiceHandleContainer<HandleType>>
            FindService(ara::core::InstanceSpecifier specifier)
            {
                (void)specifier;
                return ara::com::ServiceProxyBase::FindService(
                    kServiceId,
                    kInstanceId);
            }

            static ara::core::Result<ara::com::FindServiceHandle>
            StartFindService(
                ara::com::FindServiceHandler<HandleType> handler,
                ara::core::InstanceSpecifier specifier)
            {
                (void)specifier;
                return ara::com::ServiceProxyBase::StartFindService(
                    std::move(handler),
                    kServiceId,
                    kInstanceId);
            }

            static void StopFindService(ara::com::FindServiceHandle)
            {
                ara::com::ServiceProxyBase::StopFindService();
            }
        };
    }
}

#endif
