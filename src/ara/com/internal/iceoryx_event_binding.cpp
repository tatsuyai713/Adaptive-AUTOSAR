/// @file src/ara/com/internal/iceoryx_event_binding.cpp
/// @brief Implementation of iceoryx event binding.
/// @details This file is part of the Adaptive AUTOSAR educational implementation.

#include <algorithm>
#include <iomanip>
#include <sstream>
#include "./iceoryx_event_binding.h"

#if ARA_COM_USE_ICEORYX

namespace ara
{
    namespace com
    {
        namespace internal
        {
            namespace
            {
                /// @brief Convert a 16-bit service/instance/event identifier to a
                ///        4-character zero-padded hex string.
                std::string idToHex(std::uint16_t id)
                {
                    std::ostringstream oss;
                    oss << std::hex << std::setfill('0') << std::setw(4)
                        << static_cast<unsigned>(id);
                    return oss.str();
                }
            } // namespace

            // ── IceoryxProxyEventBinding ───────────────────────────────────

            zerocopy::ChannelDescriptor
            IceoryxProxyEventBinding::makeChannel(
                const EventBindingConfig &cfg) noexcept
            {
                return zerocopy::ChannelDescriptor{
                    "svc_" + idToHex(cfg.ServiceId),
                    "inst_" + idToHex(cfg.InstanceId),
                    "evt_" + idToHex(cfg.EventId)};
            }

            IceoryxProxyEventBinding::IceoryxProxyEventBinding(
                EventBindingConfig config) noexcept
                : mConfig{config}
            {
            }

            IceoryxProxyEventBinding::~IceoryxProxyEventBinding() noexcept
            {
                if (mState != SubscriptionState::kNotSubscribed)
                {
                    Unsubscribe();
                }
            }

            core::Result<void> IceoryxProxyEventBinding::Subscribe(
                std::size_t maxSampleCount)
            {
                {
                    std::lock_guard<std::mutex> lock(mMutex);
                    if (mState != SubscriptionState::kNotSubscribed)
                    {
                        return core::Result<void>::FromError(
                            MakeErrorCode(ComErrc::kFieldValueIsNotValid));
                    }
                    mMaxSampleCount = std::max<std::size_t>(1U, maxSampleCount);
                    mSampleQueue.clear();
                    mState = SubscriptionState::kSubscribed;
                }

                mSubscriber.reset(new zerocopy::ZeroCopySubscriber{
                    makeChannel(mConfig),
                    "ara_com_proxy",
                    static_cast<std::uint64_t>(mMaxSampleCount)});

                mRunning.store(true, std::memory_order_relaxed);
                mPollThread = std::thread([this] { pollLoop(); });

                return core::Result<void>::FromValue();
            }

            void IceoryxProxyEventBinding::Unsubscribe()
            {
                mRunning.store(false, std::memory_order_relaxed);
                if (mPollThread.joinable())
                {
                    mPollThread.join();
                }

                {
                    std::lock_guard<std::mutex> lock(mMutex);
                    mState = SubscriptionState::kNotSubscribed;
                    mSampleQueue.clear();
                    mReceiveHandler = nullptr;
                }

                mSubscriber.reset();
            }

            SubscriptionState
            IceoryxProxyEventBinding::GetSubscriptionState() const noexcept
            {
                std::lock_guard<std::mutex> lock(mMutex);
                return mState;
            }

            core::Result<std::size_t> IceoryxProxyEventBinding::GetNewSamples(
                RawReceiveHandler handler,
                std::size_t maxNumberOfSamples)
            {
                if (!handler)
                {
                    return core::Result<std::size_t>::FromError(
                        MakeErrorCode(ComErrc::kSetHandlerNotSet));
                }

                std::vector<std::vector<std::uint8_t>> samples;
                {
                    std::lock_guard<std::mutex> lock(mMutex);
                    if (mState != SubscriptionState::kSubscribed)
                    {
                        return core::Result<std::size_t>::FromError(
                            MakeErrorCode(ComErrc::kServiceNotAvailable));
                    }

                    const std::size_t count =
                        std::min(maxNumberOfSamples, mSampleQueue.size());
                    samples.reserve(count);
                    for (std::size_t i = 0U; i < count; ++i)
                    {
                        samples.push_back(std::move(mSampleQueue.front()));
                        mSampleQueue.pop_front();
                    }
                }

                for (const auto &sample : samples)
                {
                    handler(sample.data(), sample.size());
                }

                return core::Result<std::size_t>::FromValue(samples.size());
            }

            void IceoryxProxyEventBinding::SetReceiveHandler(
                std::function<void()> handler)
            {
                std::lock_guard<std::mutex> lock(mMutex);
                mReceiveHandler = std::move(handler);
            }

            void IceoryxProxyEventBinding::UnsetReceiveHandler()
            {
                std::lock_guard<std::mutex> lock(mMutex);
                mReceiveHandler = nullptr;
            }

            std::size_t
            IceoryxProxyEventBinding::GetFreeSampleCount() const noexcept
            {
                std::lock_guard<std::mutex> lock(mMutex);
                if (mSampleQueue.size() >= mMaxSampleCount)
                {
                    return 0U;
                }
                return mMaxSampleCount - mSampleQueue.size();
            }

            void IceoryxProxyEventBinding::SetSubscriptionStateChangeHandler(
                SubscriptionStateChangeHandler handler)
            {
                std::lock_guard<std::mutex> lock(mMutex);
                mStateChangeHandler = std::move(handler);
            }

            void IceoryxProxyEventBinding::UnsetSubscriptionStateChangeHandler()
            {
                std::lock_guard<std::mutex> lock(mMutex);
                mStateChangeHandler = nullptr;
            }

            void IceoryxProxyEventBinding::pollLoop() noexcept
            {
                static constexpr std::chrono::milliseconds cPollTimeout{50};

                while (mRunning.load(std::memory_order_relaxed))
                {
                    if (!mSubscriber || !mSubscriber->IsBindingActive())
                    {
                        std::this_thread::sleep_for(cPollTimeout);
                        continue;
                    }

                    if (!mSubscriber->WaitForData(cPollTimeout))
                    {
                        continue;
                    }

                    while (true)
                    {
                        zerocopy::ReceivedSample sample;
                        const auto result = mSubscriber->TryTake(sample);
                        if (!result.HasValue() || !result.Value())
                        {
                            break;
                        }

                        const auto *data = sample.Data();
                        const auto size = sample.Size();
                        if (data == nullptr || size == 0U)
                        {
                            continue;
                        }

                        std::function<void()> notify;
                        {
                            std::lock_guard<std::mutex> lock(mMutex);
                            if (mSampleQueue.size() >= mMaxSampleCount)
                            {
                                mSampleQueue.pop_front();
                            }
                            mSampleQueue.emplace_back(data, data + size);
                            notify = mReceiveHandler;
                        }

                        if (notify)
                        {
                            notify();
                        }
                    }
                }
            }

            // ── IceoryxSkeletonEventBinding ────────────────────────────────

            IceoryxSkeletonEventBinding::IceoryxSkeletonEventBinding(
                EventBindingConfig config) noexcept
                : mConfig{config}
            {
            }

            IceoryxSkeletonEventBinding::~IceoryxSkeletonEventBinding() noexcept
            {
                if (mOffered)
                {
                    StopOffer();
                }
            }

            core::Result<void> IceoryxSkeletonEventBinding::Offer()
            {
                if (mOffered)
                {
                    return core::Result<void>::FromError(
                        MakeErrorCode(ComErrc::kFieldValueIsNotValid));
                }

                mPublisher.reset(new zerocopy::ZeroCopyPublisher{
                    IceoryxProxyEventBinding::makeChannel(mConfig),
                    "ara_com_skeleton"});

                mOffered = true;
                return core::Result<void>::FromValue();
            }

            void IceoryxSkeletonEventBinding::StopOffer()
            {
                if (!mOffered)
                {
                    return;
                }

                {
                    std::lock_guard<std::mutex> lock(mLoanMutex);
                    mActiveLoans.clear();
                }

                mPublisher.reset();
                mOffered = false;
            }

            core::Result<void> IceoryxSkeletonEventBinding::Send(
                const std::vector<std::uint8_t> &payload)
            {
                if (!mOffered || !mPublisher)
                {
                    return core::Result<void>::FromError(
                        MakeErrorCode(ComErrc::kServiceNotOffered));
                }

                return mPublisher->PublishCopy(payload);
            }

            core::Result<void *> IceoryxSkeletonEventBinding::Allocate(
                std::size_t size)
            {
                if (!mOffered || !mPublisher)
                {
                    return core::Result<void *>::FromError(
                        MakeErrorCode(ComErrc::kServiceNotOffered));
                }

                zerocopy::LoanedSample sample;
                auto loanResult = mPublisher->Loan(size, sample);
                if (!loanResult.HasValue())
                {
                    return core::Result<void *>::FromError(loanResult.Error());
                }

                void *ptr = sample.Data();
                if (ptr == nullptr)
                {
                    return core::Result<void *>::FromError(
                        MakeErrorCode(ComErrc::kSampleAllocationFailure));
                }

                {
                    std::lock_guard<std::mutex> lock(mLoanMutex);
                    mActiveLoans.emplace(ptr, std::move(sample));
                }

                return core::Result<void *>::FromValue(ptr);
            }

            core::Result<void> IceoryxSkeletonEventBinding::SendAllocated(
                void *data, std::size_t /*size*/)
            {
                if (!mOffered || !mPublisher)
                {
                    return core::Result<void>::FromError(
                        MakeErrorCode(ComErrc::kServiceNotOffered));
                }

                zerocopy::LoanedSample sample;
                {
                    std::lock_guard<std::mutex> lock(mLoanMutex);
                    auto it = mActiveLoans.find(data);
                    if (it == mActiveLoans.end())
                    {
                        return core::Result<void>::FromError(
                            MakeErrorCode(ComErrc::kFieldValueIsNotValid));
                    }
                    sample = std::move(it->second);
                    mActiveLoans.erase(it);
                }

                return mPublisher->Publish(std::move(sample));
            }
        }
    }
}

#endif // ARA_COM_USE_ICEORYX
