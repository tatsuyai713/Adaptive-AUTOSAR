#ifndef SAMPLE_VEHICLE_STATUS_SKELETON_H
#define SAMPLE_VEHICLE_STATUS_SKELETON_H

#include <memory>

#include "ara/com/event.h"
#include "ara/com/internal/binding_factory.h"
#include "ara/com/service_skeleton_base.h"
#include "ara/com/types.h"
#include "ara/core/instance_specifier.h"
#include "ara/core/result.h"
#include "./vehicle_status_types.h"

namespace sample
{
    namespace vehicle_status
    {
        /// @brief Standard AUTOSAR AP skeleton for the VehicleStatus service.
        ///
        /// This class demonstrates what a code generator would produce from an
        /// ARXML service interface definition. Application code using this skeleton
        /// is portable to commercial AUTOSAR AP stacks (Vector, Bosch, Elektrobit).
        class VehicleStatusServiceSkeleton : public ara::com::ServiceSkeletonBase
        {
        private:
            // In commercial AUTOSAR stacks this part is generated and vendor-specific.
            static std::unique_ptr<ara::com::internal::SkeletonEventBinding>
            CreateStatusEventBinding()
            {
                return ara::com::internal::BindingFactory::CreateSkeletonEventBinding(
                    ara::com::internal::TransportBinding::kVsomeip,
                    ara::com::internal::EventBindingConfig{
                        cServiceId,
                        cInstanceId,
                        cStatusEventId,
                        cStatusEventGroupId,
                        cMajorVersion});
            }

        public:
            /// @brief Typed event: VehicleStatusFrame notification
            ara::com::SkeletonEvent<VehicleStatusFrame> StatusEvent;

            /// @brief Construct skeleton with instance specifier
            /// @param specifier Instance specifier for this service instance
            /// @param mode Method call processing mode (default: kEvent)
            explicit VehicleStatusServiceSkeleton(
                ara::core::InstanceSpecifier specifier,
                ara::com::MethodCallProcessingMode mode =
                    ara::com::MethodCallProcessingMode::kEvent)
                : ServiceSkeletonBase{
                      specifier,
                      cServiceId,
                      cInstanceId,
                      cMajorVersion,
                      0U,
                      mode},
                  StatusEvent{CreateStatusEventBinding()}
            {
            }

            VehicleStatusServiceSkeleton(const VehicleStatusServiceSkeleton &) = delete;
            VehicleStatusServiceSkeleton &operator=(const VehicleStatusServiceSkeleton &) = delete;
            VehicleStatusServiceSkeleton(VehicleStatusServiceSkeleton &&) = default;
            VehicleStatusServiceSkeleton &operator=(VehicleStatusServiceSkeleton &&) = default;

            ~VehicleStatusServiceSkeleton() noexcept override = default;
        };
    }
}

#endif
