/// @file src/ara/com/internal/iceoryx_event_binding.h
/// @brief iceoryx-backed ProxyEventBinding and SkeletonEventBinding.
/// @details Wraps ZeroCopyPublisher / ZeroCopySubscriber in the abstract
///          ProxyEventBinding / SkeletonEventBinding interface.
///          The ProxyEventBinding uses a background thread that calls
///          ZeroCopySubscriber::WaitForData() and then drains samples
///          into an internal deque, notifying the receive handler on arrival.
///          The SkeletonEventBinding publishes via PublishCopy() (copy path)
///          or via Loan() + Publish() (zero-copy path).
///
///          This file is part of the Adaptive AUTOSAR educational implementation.

#ifndef ARA_COM_INTERNAL_ICEORYX_EVENT_BINDING_H
#define ARA_COM_INTERNAL_ICEORYX_EVENT_BINDING_H

#include <atomic>
#include <deque>
#include <mutex>
#include <thread>
#include <unordered_map>
#include "./event_binding.h"
#include "../com_error_domain.h"
#include "../zerocopy/zero_copy.h"

#if ARA_COM_USE_ICEORYX

namespace ara
{
    namespace com
    {
        namespace internal
        {
            /// @brief iceoryx-based proxy-side event binding.
            ///        A background polling thread drains ZeroCopySubscriber
            ///        samples into an internal queue and fires the receive handler.
            class IceoryxProxyEventBinding final : public ProxyEventBinding
            {
            private:
                EventBindingConfig mConfig;
                mutable std::mutex mMutex;
                SubscriptionState mState{SubscriptionState::kNotSubscribed};
                std::deque<std::vector<std::uint8_t>> mSampleQueue;
                std::size_t mMaxSampleCount{16U};
                std::function<void()> mReceiveHandler;
                SubscriptionStateChangeHandler mStateChangeHandler;

                std::unique_ptr<zerocopy::ZeroCopySubscriber> mSubscriber;
                std::atomic<bool> mRunning{false};
                std::thread mPollThread;

                void pollLoop() noexcept;

            public:
                static zerocopy::ChannelDescriptor makeChannel(
                    const EventBindingConfig &cfg) noexcept;

                explicit IceoryxProxyEventBinding(
                    EventBindingConfig config) noexcept;
                ~IceoryxProxyEventBinding() noexcept override;

                IceoryxProxyEventBinding(const IceoryxProxyEventBinding &) = delete;
                IceoryxProxyEventBinding &operator=(
                    const IceoryxProxyEventBinding &) = delete;

                core::Result<void> Subscribe(std::size_t maxSampleCount) override;
                void Unsubscribe() override;
                SubscriptionState GetSubscriptionState() const noexcept override;
                core::Result<std::size_t> GetNewSamples(
                    RawReceiveHandler handler,
                    std::size_t maxNumberOfSamples) override;
                void SetReceiveHandler(std::function<void()> handler) override;
                void UnsetReceiveHandler() override;
                std::size_t GetFreeSampleCount() const noexcept override;
                void SetSubscriptionStateChangeHandler(
                    SubscriptionStateChangeHandler handler) override;
                void UnsetSubscriptionStateChangeHandler() override;
            };

            /// @brief iceoryx-based skeleton-side event binding.
            ///        Uses ZeroCopyPublisher::PublishCopy() for the standard
            ///        Send() path and Loan()/Publish() for the zero-copy path.
            class IceoryxSkeletonEventBinding final : public SkeletonEventBinding
            {
            private:
                EventBindingConfig mConfig;
                std::unique_ptr<zerocopy::ZeroCopyPublisher> mPublisher;
                bool mOffered{false};
                std::vector<std::uint8_t> mInitialValue;
                bool mHasInitialValue{false};

                /// @brief Outstanding Loan() allocations keyed by raw pointer.
                std::mutex mLoanMutex;
                std::unordered_map<void *, zerocopy::LoanedSample> mActiveLoans;

            public:
                explicit IceoryxSkeletonEventBinding(
                    EventBindingConfig config) noexcept;
                ~IceoryxSkeletonEventBinding() noexcept override;

                IceoryxSkeletonEventBinding(const IceoryxSkeletonEventBinding &) = delete;
                IceoryxSkeletonEventBinding &operator=(
                    const IceoryxSkeletonEventBinding &) = delete;

                core::Result<void> Offer() override;
                void StopOffer() override;
                core::Result<void> Send(
                    const std::vector<std::uint8_t> &payload) override;
                core::Result<void *> Allocate(std::size_t size) override;
                core::Result<void> SendAllocated(
                    void *data, std::size_t size) override;
                void SetInitialValue(
                    const std::vector<std::uint8_t> &payload) override;
            };
        }
    }
}

#endif // ARA_COM_USE_ICEORYX

#endif // ARA_COM_INTERNAL_ICEORYX_EVENT_BINDING_H
