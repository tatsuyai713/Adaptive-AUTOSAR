/// @file src/ara/com/internal/vsomeip_event_binding.h
/// @brief Declarations for vsomeip event binding.
/// @details This file is part of the Adaptive AUTOSAR educational implementation.

#ifndef ARA_COM_INTERNAL_VSOMEIP_EVENT_BINDING_H
#define ARA_COM_INTERNAL_VSOMEIP_EVENT_BINDING_H

#include <deque>
#include <mutex>
#include <set>
#include <string>
#include "./event_binding.h"
#include "../com_error_domain.h"
#include "../someip/vsomeip_application.h"

namespace ara
{
    namespace com
    {
        namespace internal
        {
            /// @brief vsomeip-based proxy-side event binding.
            ///        Extracts the subscribe/message-handler/sample-queue logic from ServiceProxy.
            class VsomeipProxyEventBinding final : public ProxyEventBinding
            {
            private:
                EventBindingConfig mConfig;
                mutable std::mutex mMutex;
                SubscriptionState mState{SubscriptionState::kNotSubscribed};
                std::deque<std::vector<std::uint8_t>> mSampleQueue;
                std::size_t mMaxSampleCount{16U};
                std::function<void()> mReceiveHandler;
                SubscriptionStateChangeHandler mStateChangeHandler;

            public:
                explicit VsomeipProxyEventBinding(
                    EventBindingConfig config) noexcept;

                ~VsomeipProxyEventBinding() noexcept override;

                VsomeipProxyEventBinding(const VsomeipProxyEventBinding &) = delete;
                VsomeipProxyEventBinding &operator=(const VsomeipProxyEventBinding &) = delete;

                core::Result<void> Subscribe(
                    std::size_t maxSampleCount) override;
                void Unsubscribe() override;
                SubscriptionState GetSubscriptionState() const noexcept override;
                core::Result<std::size_t> GetNewSamples(
                    RawReceiveHandler handler,
                    std::size_t maxNumberOfSamples) override;
                void SetReceiveHandler(
                    std::function<void()> handler) override;
                void UnsetReceiveHandler() override;
                std::size_t GetFreeSampleCount() const noexcept override;
                void SetSubscriptionStateChangeHandler(
                    SubscriptionStateChangeHandler handler) override;
                void UnsetSubscriptionStateChangeHandler() override;
            };

            /// @brief vsomeip-based skeleton-side event binding.
            ///        Extracts OfferEvent/NotifyEvent logic from ServiceSkeleton.
            class VsomeipSkeletonEventBinding final : public SkeletonEventBinding
            {
            private:
                EventBindingConfig mConfig;
                bool mOffered{false};

            public:
                explicit VsomeipSkeletonEventBinding(
                    EventBindingConfig config) noexcept;

                ~VsomeipSkeletonEventBinding() noexcept override;

                VsomeipSkeletonEventBinding(const VsomeipSkeletonEventBinding &) = delete;
                VsomeipSkeletonEventBinding &operator=(const VsomeipSkeletonEventBinding &) = delete;

                core::Result<void> Offer() override;
                void StopOffer() override;
                core::Result<void> Send(
                    const std::vector<std::uint8_t> &payload) override;
                core::Result<void *> Allocate(std::size_t size) override;
                core::Result<void> SendAllocated(
                    void *data, std::size_t size) override;
            };
        }
    }
}

#endif
