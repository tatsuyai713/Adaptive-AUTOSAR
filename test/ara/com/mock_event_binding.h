#ifndef ARA_COM_TEST_MOCK_EVENT_BINDING_H
#define ARA_COM_TEST_MOCK_EVENT_BINDING_H

#include <cstdint>
#include <deque>
#include <functional>
#include <mutex>
#include <vector>
#include "../../../src/ara/com/internal/event_binding.h"
#include "../../../src/ara/com/internal/method_binding.h"
#include "../../../src/ara/com/com_error_domain.h"

namespace ara
{
    namespace com
    {
        namespace test
        {
            /// @brief Mock proxy-side event binding for unit testing.
            ///        Does not require vsomeip/iceoryx/DDS.
            class MockProxyEventBinding : public internal::ProxyEventBinding
            {
            private:
                SubscriptionState mState{SubscriptionState::kNotSubscribed};
                std::deque<std::vector<std::uint8_t>> mSampleQueue;
                std::size_t mMaxSampleCount{16U};
                std::function<void()> mReceiveHandler;
                SubscriptionStateChangeHandler mStateChangeHandler;

            public:
                core::Result<void> Subscribe(std::size_t maxSampleCount) override
                {
                    mMaxSampleCount = maxSampleCount;
                    mSampleQueue.clear();
                    mState = SubscriptionState::kSubscribed;
                    return core::Result<void>::FromValue();
                }

                void Unsubscribe() override
                {
                    mState = SubscriptionState::kNotSubscribed;
                    mSampleQueue.clear();
                    mReceiveHandler = nullptr;
                }

                SubscriptionState GetSubscriptionState() const noexcept override
                {
                    return mState;
                }

                core::Result<std::size_t> GetNewSamples(
                    RawReceiveHandler handler,
                    std::size_t maxNumberOfSamples) override
                {
                    if (!handler)
                    {
                        return core::Result<std::size_t>::FromError(
                            MakeErrorCode(ComErrc::kSetHandlerNotSet));
                    }

                    if (mState != SubscriptionState::kSubscribed)
                    {
                        return core::Result<std::size_t>::FromError(
                            MakeErrorCode(ComErrc::kServiceNotAvailable));
                    }

                    std::size_t count = 0U;
                    while (!mSampleQueue.empty() && count < maxNumberOfSamples)
                    {
                        auto &sample = mSampleQueue.front();
                        handler(sample.data(), sample.size());
                        mSampleQueue.pop_front();
                        ++count;
                    }
                    return core::Result<std::size_t>::FromValue(count);
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
                    if (mSampleQueue.size() >= mMaxSampleCount)
                    {
                        return 0U;
                    }
                    return mMaxSampleCount - mSampleQueue.size();
                }

                void SetSubscriptionStateChangeHandler(
                    SubscriptionStateChangeHandler handler) override
                {
                    mStateChangeHandler = std::move(handler);
                }

                void UnsetSubscriptionStateChangeHandler() override
                {
                    mStateChangeHandler = nullptr;
                }

                // ── Test helpers ──

                void InjectSample(const std::vector<std::uint8_t> &data)
                {
                    mSampleQueue.push_back(data);
                    if (mReceiveHandler)
                    {
                        mReceiveHandler();
                    }
                }
            };

            /// @brief Mock skeleton-side event binding for unit testing.
            class MockSkeletonEventBinding : public internal::SkeletonEventBinding
            {
            private:
                bool mOffered{false};

            public:
                std::vector<std::vector<std::uint8_t>> SentPayloads;
                std::vector<std::pair<void *, std::size_t>> AllocatedSends;

                core::Result<void> Offer() override
                {
                    mOffered = true;
                    return core::Result<void>::FromValue();
                }

                void StopOffer() override
                {
                    mOffered = false;
                }

                core::Result<void> Send(
                    const std::vector<std::uint8_t> &payload) override
                {
                    if (!mOffered)
                    {
                        return core::Result<void>::FromError(
                            MakeErrorCode(ComErrc::kServiceNotOffered));
                    }
                    SentPayloads.push_back(payload);
                    return core::Result<void>::FromValue();
                }

                core::Result<void *> Allocate(std::size_t size) override
                {
                    void *ptr = std::malloc(size);
                    if (!ptr)
                    {
                        return core::Result<void *>::FromError(
                            MakeErrorCode(ComErrc::kSampleAllocationFailure));
                    }
                    return core::Result<void *>::FromValue(ptr);
                }

                core::Result<void> SendAllocated(
                    void *data, std::size_t size) override
                {
                    if (!mOffered)
                    {
                        std::free(data);
                        return core::Result<void>::FromError(
                            MakeErrorCode(ComErrc::kServiceNotOffered));
                    }
                    // Copy the data before freeing
                    std::vector<std::uint8_t> payload(
                        static_cast<std::uint8_t *>(data),
                        static_cast<std::uint8_t *>(data) + size);
                    std::free(data);
                    SentPayloads.push_back(std::move(payload));
                    return core::Result<void>::FromValue();
                }

                bool IsOffered() const noexcept { return mOffered; }
            };

            /// @brief Mock proxy-side method binding for unit testing.
            class MockProxyMethodBinding : public internal::ProxyMethodBinding
            {
            public:
                std::vector<std::uint8_t> LastRequest;
                std::vector<std::uint8_t> ResponseToReturn;
                bool ShouldFail{false};

                void Call(
                    const std::vector<std::uint8_t> &requestPayload,
                    RawResponseHandler responseHandler) override
                {
                    LastRequest = requestPayload;
                    if (ShouldFail)
                    {
                        responseHandler(
                            core::Result<std::vector<std::uint8_t>>::FromError(
                                MakeErrorCode(ComErrc::kNetworkBindingFailure)));
                    }
                    else
                    {
                        responseHandler(
                            core::Result<std::vector<std::uint8_t>>::FromValue(
                                ResponseToReturn));
                    }
                }
            };
        }
    }
}

#endif
