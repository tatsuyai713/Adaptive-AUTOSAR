/// @file src/ara/com/service_skeleton_base.h
/// @brief Declarations for service skeleton base.
/// @details This file is part of the Adaptive AUTOSAR educational implementation.

#ifndef ARA_COM_SERVICE_SKELETON_BASE_H
#define ARA_COM_SERVICE_SKELETON_BASE_H

#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <functional>
#include <mutex>
#include <utility>
#include <vector>
#include "./types.h"
#include "./service_version.h"
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

            /// @brief Queue of deferred method calls for kPoll mode.
            std::deque<std::function<void()>> mPollQueue;
            mutable std::mutex mPollMutex;
            std::condition_variable mPollCV;

            /// @brief Serialization mutex for kEventSingleThread mode.
            ///        Method dispatch lambdas lock this so that at most one
            ///        method handler executes at a time.
            std::recursive_mutex mDispatchMutex;

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

            /// @brief Enqueue a deferred method call for kPoll dispatch.
            ///        Concrete skeletons or method bindings call this when
            ///        the processing mode is kPoll so that the application
            ///        controls when each method call is dispatched via
            ///        ProcessNextMethodCall().
            /// @param task Callable encapsulating the method dispatch
            void EnqueuePollCall(std::function<void()> task);

            /// @brief Returns pointer to the dispatch serialization mutex when
            ///        the processing mode is kEventSingleThread, nullptr otherwise.
            ///        Derived skeletons pass this to SkeletonMethod constructors
            ///        so that concurrent handler invocations are serialized.
            std::recursive_mutex *GetDispatchMutex() noexcept;

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

            /// @brief Process the next pending method call (SWS_CM_00199).
            /// @details When the skeleton is constructed with `kPoll` processing
            ///          mode, this function shall be called by the application
            ///          to dispatch exactly one pending method request.
            ///          With `kEvent` mode this is a no-op and returns success.
            /// @returns A Future that becomes ready when the dispatched method
            ///          call finishes, or immediately if no call was pending.
            core::Result<void> ProcessNextMethodCall();

            /// @brief Process next pending method call with timeout (SWS_CM_00200).
            /// @details Blocks until a method call is available or the timeout
            ///          expires. Returns success if a call was dispatched, or
            ///          success with no action if the timeout elapsed first.
            /// @param timeout Maximum time to wait for a pending method call.
            /// @returns Result indicating success.
            core::Result<void> ProcessNextMethodCall(
                std::chrono::milliseconds timeout);

            /// @brief Get the configured method processing mode.
            MethodCallProcessingMode GetMethodCallProcessingMode() const noexcept;

            /// @brief Check whether there are pending method calls in the poll queue.
            /// @returns `true` if at least one pending call is queued.
            bool HasPendingMethodCalls() const noexcept;

            /// @brief Get the service interface version.
            /// @returns ServiceVersion with major and minor version numbers.
            ServiceVersion GetServiceVersion() const noexcept;
        };
    }
}

#endif
