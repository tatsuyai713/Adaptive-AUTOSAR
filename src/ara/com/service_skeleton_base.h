/// @file src/ara/com/service_skeleton_base.h
/// @brief Declarations for service skeleton base.
/// @details This file is part of the Adaptive AUTOSAR educational implementation.

#ifndef ARA_COM_SERVICE_SKELETON_BASE_H
#define ARA_COM_SERVICE_SKELETON_BASE_H

#ifndef ARA_COM_HAS_GENERATED_EVENT_BINDING_HELPERS
#define ARA_COM_HAS_GENERATED_EVENT_BINDING_HELPERS 1
#endif

#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <utility>
#include <vector>
#include "./types.h"
#include "../core/instance_specifier.h"
#include "../core/result.h"

namespace ara
{
    namespace com
    {
        namespace internal
        {
            class SkeletonEventBinding;
        }

        /// @brief Base class for standard AUTOSAR AP skeleton classes.
        ///        Generated skeleton classes inherit from this and add typed
        ///        SkeletonEvent<T>, SkeletonField<T> members and virtual methods.
        class ServiceSkeletonBase
        {
        private:
            core::InstanceSpecifier mInstanceSpecifier;
            std::uint16_t mServiceId;
            std::uint16_t mInstanceId;
            std::uint8_t mMajorVersion;
            std::uint32_t mMinorVersion;
            MethodCallProcessingMode mProcessingMode;
            bool mOffered{false};
            std::vector<std::uint16_t> mRegisteredEventGroups;
            mutable std::mutex mSubscriptionMutex;

        protected:
            /// @brief Constructor
            /// @param specifier Instance specifier identifying this skeleton
            /// @param serviceId Transport-level service identifier
            /// @param instanceId Transport-level instance identifier
            /// @param mode Method call processing mode
            ServiceSkeletonBase(
                core::InstanceSpecifier specifier,
                std::uint16_t serviceId,
                std::uint16_t instanceId,
                std::uint8_t majorVersion = 1U,
                std::uint32_t minorVersion = 0U,
                MethodCallProcessingMode mode =
                    MethodCallProcessingMode::kEvent) noexcept;

            /// @brief Returns configured service identifier.
            std::uint16_t GetServiceId() const noexcept;
            /// @brief Returns configured instance identifier.
            std::uint16_t GetInstanceId() const noexcept;

            /// @brief Create a SOME/IP skeleton-event binding for generated code.
            /// @details Keeps transport-specific binding details out of app-level code.
            std::unique_ptr<internal::SkeletonEventBinding>
            CreateSomeIpSkeletonEventBinding(
                std::uint16_t eventId,
                std::uint16_t eventGroupId,
                std::uint8_t majorVersion = 0U) const;

        public:
            /// @brief Callback type to validate/deny event subscription changes.
            using EventSubscriptionStateHandler =
                std::function<bool(std::uint16_t, bool)>;

            ServiceSkeletonBase(const ServiceSkeletonBase &) = delete;
            ServiceSkeletonBase &operator=(const ServiceSkeletonBase &) = delete;

            /// @brief Virtual destructor.
            virtual ~ServiceSkeletonBase() noexcept;

            /// @brief Start offering the service
            /// @returns `Result<void>` indicating offer success/failure.
            core::Result<void> OfferService();

            /// @brief Stop offering the service
            void StopOfferService();

            /// @brief Register subscription state handler for a specific event-group.
            /// @param eventGroupId Event-group identifier
            /// @param handler Callback: (clientId, subscribed) -> accept/reject
            /// @returns Error if service is not offered, handler is invalid, or duplicated registration
            core::Result<void> SetEventSubscriptionStateHandler(
                std::uint16_t eventGroupId,
                EventSubscriptionStateHandler handler);

            /// @brief Unregister subscription state handler for a specific event-group.
            /// @param eventGroupId Event-group identifier
            void UnsetEventSubscriptionStateHandler(
                std::uint16_t eventGroupId);

            /// @brief Check if the service is currently offered
            /// @returns `true` when service offer state is active.
            bool IsOffered() const noexcept;

            /// @brief Get the instance specifier
            /// @returns Instance specifier associated with this skeleton.
            const core::InstanceSpecifier &GetInstanceSpecifier() const noexcept;
        };
    }
}

#endif
