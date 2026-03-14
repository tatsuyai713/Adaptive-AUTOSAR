/// @file src/ara/com/trigger.h
/// @brief Trigger communication pattern per AUTOSAR AP SWS_CM_00500.
/// @details A Trigger is a one-way notification mechanism that carries no payload
///          data. Unlike Events which transport typed sample data, Triggers are
///          used purely for signaling purposes (e.g., "data ready", "wakeup").
///          Unlike fire-and-forget methods, Triggers are multicast and stateless.

#ifndef ARA_COM_TRIGGER_H
#define ARA_COM_TRIGGER_H

#include <algorithm>
#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <vector>
#include "./types.h"
#include "./com_error_domain.h"
#include "../core/result.h"

namespace ara
{
    namespace com
    {
        /// @brief Proxy-side trigger per AUTOSAR AP SWS_CM_00500.
        ///        Receives trigger notifications from a skeleton.
        class ProxyTrigger
        {
        private:
            std::atomic<bool> mSubscribed{false};
            std::function<void()> mHandler;
            mutable std::mutex mMutex;
            std::atomic<uint64_t> mTriggerCount{0U};

        public:
            ProxyTrigger() = default;

            ProxyTrigger(const ProxyTrigger &) = delete;
            ProxyTrigger &operator=(const ProxyTrigger &) = delete;
            ProxyTrigger(ProxyTrigger &&) = default;
            ProxyTrigger &operator=(ProxyTrigger &&) = default;

            /// @brief Subscribe to trigger notifications.
            /// @returns Result indicating success or error.
            core::Result<void> Subscribe()
            {
                mSubscribed.store(true);
                mTriggerCount.store(0U);
                return core::Result<void>::FromValue();
            }

            /// @brief Unsubscribe from trigger notifications.
            void Unsubscribe()
            {
                mSubscribed.store(false);
                std::lock_guard<std::mutex> lock(mMutex);
                mHandler = nullptr;
            }

            /// @brief Get subscription state.
            /// @returns Current subscription state.
            SubscriptionState GetSubscriptionState() const noexcept
            {
                return mSubscribed.load()
                           ? SubscriptionState::kSubscribed
                           : SubscriptionState::kNotSubscribed;
            }

            /// @brief Set callback for trigger arrival.
            /// @param handler Callback invoked each time a trigger fires.
            void SetReceiveHandler(std::function<void()> handler)
            {
                std::lock_guard<std::mutex> lock(mMutex);
                mHandler = std::move(handler);
            }

            /// @brief Remove trigger receive handler.
            void UnsetReceiveHandler()
            {
                std::lock_guard<std::mutex> lock(mMutex);
                mHandler = nullptr;
            }

            /// @brief Get the number of trigger events received since subscription.
            /// @returns Number of trigger notifications.
            uint64_t GetTriggerCount() const noexcept
            {
                return mTriggerCount.load();
            }

            /// @brief Called by the transport binding when a trigger arrives.
            void OnTriggerReceived()
            {
                if (!mSubscribed.load())
                {
                    return;
                }

                mTriggerCount.fetch_add(1U);

                std::lock_guard<std::mutex> lock(mMutex);
                if (mHandler)
                {
                    mHandler();
                }
            }
        };

        /// @brief Skeleton-side trigger per AUTOSAR AP SWS_CM_00501.
        ///        Sends trigger notifications to all subscribed proxies.
        class SkeletonTrigger
        {
        private:
            bool mOffered{false};
            std::vector<ProxyTrigger *> mSubscribers;
            mutable std::mutex mMutex;

        public:
            SkeletonTrigger() = default;

            SkeletonTrigger(const SkeletonTrigger &) = delete;
            SkeletonTrigger &operator=(const SkeletonTrigger &) = delete;
            SkeletonTrigger(SkeletonTrigger &&) = default;
            SkeletonTrigger &operator=(SkeletonTrigger &&) = default;

            /// @brief Offer this trigger for subscription.
            /// @returns Result indicating success or error.
            core::Result<void> Offer()
            {
                std::lock_guard<std::mutex> lock(mMutex);
                mOffered = true;
                return core::Result<void>::FromValue();
            }

            /// @brief Stop offering this trigger.
            void StopOffer()
            {
                std::lock_guard<std::mutex> lock(mMutex);
                mOffered = false;
                mSubscribers.clear();
            }

            /// @brief Register a proxy trigger for local notification delivery.
            /// @param subscriber Proxy trigger to notify when Fire() is called.
            core::Result<void> AddSubscriber(ProxyTrigger &subscriber)
            {
                std::lock_guard<std::mutex> lock(mMutex);
                if (!mOffered)
                {
                    return core::Result<void>::FromError(
                        MakeErrorCode(ComErrc::kServiceNotOffered));
                }
                mSubscribers.push_back(&subscriber);
                return core::Result<void>::FromValue();
            }

            /// @brief Remove a previously registered subscriber.
            /// @param subscriber Proxy trigger to remove.
            void RemoveSubscriber(ProxyTrigger &subscriber)
            {
                std::lock_guard<std::mutex> lock(mMutex);
                mSubscribers.erase(
                    std::remove(mSubscribers.begin(), mSubscribers.end(),
                                &subscriber),
                    mSubscribers.end());
            }

            /// @brief Fire the trigger, notifying all subscribers.
            /// @returns Error if not offered or no subscribers.
            core::Result<void> Fire()
            {
                std::lock_guard<std::mutex> lock(mMutex);
                if (!mOffered)
                {
                    return core::Result<void>::FromError(
                        MakeErrorCode(ComErrc::kServiceNotOffered));
                }

                for (auto *sub : mSubscribers)
                {
                    sub->OnTriggerReceived();
                }

                return core::Result<void>::FromValue();
            }

            /// @brief Check if currently offered.
            bool IsOffered() const noexcept
            {
                std::lock_guard<std::mutex> lock(mMutex);
                return mOffered;
            }

            /// @brief Get the number of active subscribers.
            std::size_t GetSubscriberCount() const noexcept
            {
                std::lock_guard<std::mutex> lock(mMutex);
                return mSubscribers.size();
            }
        };
    }
}

#endif
