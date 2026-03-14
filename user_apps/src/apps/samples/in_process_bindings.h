/// @file user_apps/src/apps/samples/in_process_bindings.h
/// @brief Lightweight in-process method and event bindings for samples.
/// @details Connects ProxyXxx and SkeletonXxx within the same process without
///          requiring any real transport (SOME/IP, DDS, iceoryx).
///          These bindings are intentionally minimal — they are used only to
///          demonstrate the ara::com typed API layer in standalone samples.

#ifndef SAMPLES_IN_PROCESS_BINDINGS_H
#define SAMPLES_IN_PROCESS_BINDINGS_H

#include <cstdlib>
#include <deque>
#include <functional>
#include <memory>
#include <vector>
#include "ara/com/internal/event_binding.h"
#include "ara/com/internal/method_binding.h"
#include "ara/com/com_error_domain.h"
#include "ara/core/result.h"

namespace sample
{
    // =========================================================================
    // In-process Method bindings
    // =========================================================================

    /// @brief Shared state that links a proxy method binding to a skeleton method binding.
    struct MethodChannel
    {
        ara::com::internal::SkeletonMethodBinding::RawRequestHandler handler;
    };

    /// @brief Proxy-side in-process method binding.
    ///        Forwards Call() directly to the skeleton handler synchronously.
    class InProcessProxyMethodBinding final
        : public ara::com::internal::ProxyMethodBinding
    {
    private:
        std::shared_ptr<MethodChannel> mCh;

    public:
        explicit InProcessProxyMethodBinding(
            std::shared_ptr<MethodChannel> ch) noexcept
            : mCh{std::move(ch)}
        {
        }

        void Call(
            const std::vector<std::uint8_t> &request,
            RawResponseHandler response) override
        {
            if (mCh->handler)
            {
                response(mCh->handler(request));
            }
            else
            {
                response(
                    ara::core::Result<std::vector<std::uint8_t>>::FromError(
                        ara::com::MakeErrorCode(
                            ara::com::ComErrc::kServiceNotAvailable)));
            }
        }
    };

    /// @brief Skeleton-side in-process method binding.
    ///        Stores the registered handler in the shared MethodChannel.
    class InProcessSkeletonMethodBinding final
        : public ara::com::internal::SkeletonMethodBinding
    {
    private:
        std::shared_ptr<MethodChannel> mCh;

    public:
        explicit InProcessSkeletonMethodBinding(
            std::shared_ptr<MethodChannel> ch) noexcept
            : mCh{std::move(ch)}
        {
        }

        ara::core::Result<void> Register(
            RawRequestHandler handler) override
        {
            mCh->handler = std::move(handler);
            return ara::core::Result<void>::FromValue();
        }

        void Unregister() override
        {
            mCh->handler = nullptr;
        }
    };

    /// @brief Create a connected (proxy, skeleton) method binding pair.
    inline std::pair<
        std::unique_ptr<InProcessProxyMethodBinding>,
        std::unique_ptr<InProcessSkeletonMethodBinding>>
    MakeMethodPair()
    {
        auto ch = std::make_shared<MethodChannel>();
        return {
            std::make_unique<InProcessProxyMethodBinding>(ch),
            std::make_unique<InProcessSkeletonMethodBinding>(ch)};
    }

    // =========================================================================
    // In-process Event bindings
    // =========================================================================

    /// @brief Shared state that links a skeleton event binding to a proxy event binding.
    struct EventChannel
    {
        /// Callback set by the proxy when it subscribes.
        std::function<void(const std::vector<std::uint8_t> &)> onSample;
    };

    /// @brief Proxy-side in-process event binding.
    ///        On Subscribe(), registers an onSample callback in EventChannel so the
    ///        skeleton can push samples directly into the proxy's receive queue.
    class InProcessProxyEventBinding final
        : public ara::com::internal::ProxyEventBinding
    {
    private:
        std::shared_ptr<EventChannel> mCh;
        std::deque<std::vector<std::uint8_t>> mQueue;
        std::size_t mMaxCount{16U};
        ara::com::SubscriptionState mState{
            ara::com::SubscriptionState::kNotSubscribed};
        std::function<void()> mReceiveHandler;
        ara::com::SubscriptionStateChangeHandler mStateHandler;

    public:
        explicit InProcessProxyEventBinding(
            std::shared_ptr<EventChannel> ch) noexcept
            : mCh{std::move(ch)}
        {
        }

        ara::core::Result<void> Subscribe(std::size_t maxSampleCount) override
        {
            mMaxCount = maxSampleCount;
            mQueue.clear();
            mState = ara::com::SubscriptionState::kSubscribed;
            mCh->onSample = [this](const std::vector<std::uint8_t> &data)
            {
                if (mQueue.size() < mMaxCount)
                {
                    mQueue.push_back(data);
                    if (mReceiveHandler)
                    {
                        mReceiveHandler();
                    }
                }
            };
            return ara::core::Result<void>::FromValue();
        }

        void Unsubscribe() override
        {
            mState = ara::com::SubscriptionState::kNotSubscribed;
            mCh->onSample = nullptr;
            mQueue.clear();
        }

        ara::com::SubscriptionState GetSubscriptionState() const noexcept override
        {
            return mState;
        }

        ara::core::Result<std::size_t> GetNewSamples(
            RawReceiveHandler handler,
            std::size_t maxNumberOfSamples) override
        {
            std::size_t count = 0U;
            while (!mQueue.empty() && count < maxNumberOfSamples)
            {
                const auto &s = mQueue.front();
                handler(s.data(), s.size());
                mQueue.pop_front();
                ++count;
            }
            return ara::core::Result<std::size_t>::FromValue(count);
        }

        void SetReceiveHandler(std::function<void()> handler) override
        {
            mReceiveHandler = std::move(handler);
        }

        void UnsetReceiveHandler() override
        {
            mReceiveHandler = nullptr;
        }

        std::size_t GetFreeSampleCount() const noexcept override
        {
            if (mQueue.size() >= mMaxCount)
            {
                return 0U;
            }
            return mMaxCount - mQueue.size();
        }

        void SetSubscriptionStateChangeHandler(
            ara::com::SubscriptionStateChangeHandler handler) override
        {
            mStateHandler = std::move(handler);
        }

        void UnsetSubscriptionStateChangeHandler() override
        {
            mStateHandler = nullptr;
        }
    };

    /// @brief Skeleton-side in-process event binding.
    ///        Pushes serialized payloads into the EventChannel's onSample callback.
    class InProcessSkeletonEventBinding final
        : public ara::com::internal::SkeletonEventBinding
    {
    private:
        std::shared_ptr<EventChannel> mCh;
        bool mOffered{false};

    public:
        explicit InProcessSkeletonEventBinding(
            std::shared_ptr<EventChannel> ch) noexcept
            : mCh{std::move(ch)}
        {
        }

        ara::core::Result<void> Offer() override
        {
            mOffered = true;
            return ara::core::Result<void>::FromValue();
        }

        void StopOffer() override
        {
            mOffered = false;
        }

        ara::core::Result<void> Send(
            const std::vector<std::uint8_t> &payload) override
        {
            if (!mOffered)
            {
                return ara::core::Result<void>::FromError(
                    ara::com::MakeErrorCode(
                        ara::com::ComErrc::kServiceNotOffered));
            }
            if (mCh->onSample)
            {
                mCh->onSample(payload);
            }
            return ara::core::Result<void>::FromValue();
        }

        ara::core::Result<void *> Allocate(std::size_t size) override
        {
            void *ptr = std::malloc(size);
            if (!ptr)
            {
                return ara::core::Result<void *>::FromError(
                    ara::com::MakeErrorCode(
                        ara::com::ComErrc::kSampleAllocationFailure));
            }
            return ara::core::Result<void *>::FromValue(ptr);
        }

        ara::core::Result<void> SendAllocated(
            void *data, std::size_t size) override
        {
            if (!mOffered)
            {
                std::free(data);
                return ara::core::Result<void>::FromError(
                    ara::com::MakeErrorCode(
                        ara::com::ComErrc::kServiceNotOffered));
            }
            std::vector<std::uint8_t> payload(
                static_cast<std::uint8_t *>(data),
                static_cast<std::uint8_t *>(data) + size);
            std::free(data);
            if (mCh->onSample)
            {
                mCh->onSample(payload);
            }
            return ara::core::Result<void>::FromValue();
        }
    };

    /// @brief Create a connected (proxy, skeleton) event binding pair.
    inline std::pair<
        std::unique_ptr<InProcessProxyEventBinding>,
        std::unique_ptr<InProcessSkeletonEventBinding>>
    MakeEventPair()
    {
        auto ch = std::make_shared<EventChannel>();
        return {
            std::make_unique<InProcessProxyEventBinding>(ch),
            std::make_unique<InProcessSkeletonEventBinding>(ch)};
    }

} // namespace sample

#endif // SAMPLES_IN_PROCESS_BINDINGS_H
