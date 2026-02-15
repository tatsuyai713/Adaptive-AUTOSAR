#ifndef ARA_COM_SERVICE_SKELETON_BASE_H
#define ARA_COM_SERVICE_SKELETON_BASE_H

#include <cstdint>
#include <functional>
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

            std::uint16_t GetServiceId() const noexcept;
            std::uint16_t GetInstanceId() const noexcept;

        public:
            using EventSubscriptionStateHandler =
                std::function<bool(std::uint16_t, bool)>;

            ServiceSkeletonBase(const ServiceSkeletonBase &) = delete;
            ServiceSkeletonBase &operator=(const ServiceSkeletonBase &) = delete;

            virtual ~ServiceSkeletonBase() noexcept;

            /// @brief Start offering the service
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
            bool IsOffered() const noexcept;

            /// @brief Get the instance specifier
            const core::InstanceSpecifier &GetInstanceSpecifier() const noexcept;
        };
    }
}

#endif
