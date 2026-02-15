/// @file src/ara/com/internal/vsomeip_event_binding.cpp
/// @brief Implementation for vsomeip event binding.
/// @details This file is part of the Adaptive AUTOSAR educational implementation.

#include <algorithm>
#include <set>
#include <utility>
#include "./vsomeip_event_binding.h"

namespace ara
{
    namespace com
    {
        namespace internal
        {
            // ── VsomeipProxyEventBinding ──────────────────────────

            VsomeipProxyEventBinding::VsomeipProxyEventBinding(
                EventBindingConfig config) noexcept
                : mConfig{config}
            {
            }

            VsomeipProxyEventBinding::~VsomeipProxyEventBinding() noexcept
            {
                if (mState != SubscriptionState::kNotSubscribed)
                {
                    Unsubscribe();
                }
            }

            core::Result<void> VsomeipProxyEventBinding::Subscribe(
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
                    mState = SubscriptionState::kSubscriptionPending;
                }

                auto app = someip::VsomeipApplication::GetClientApplication();
                app->request_service(
                    static_cast<vsomeip::service_t>(mConfig.ServiceId),
                    static_cast<vsomeip::instance_t>(mConfig.InstanceId));

                std::set<vsomeip::eventgroup_t> eventGroups{
                    static_cast<vsomeip::eventgroup_t>(mConfig.EventGroupId)};
                app->request_event(
                    static_cast<vsomeip::service_t>(mConfig.ServiceId),
                    static_cast<vsomeip::instance_t>(mConfig.InstanceId),
                    static_cast<vsomeip::event_t>(mConfig.EventId),
                    eventGroups);

                app->register_message_handler(
                    static_cast<vsomeip::service_t>(mConfig.ServiceId),
                    static_cast<vsomeip::instance_t>(mConfig.InstanceId),
                    static_cast<vsomeip::method_t>(mConfig.EventId),
                    [this](const std::shared_ptr<vsomeip::message> &message)
                    {
                        std::vector<std::uint8_t> payloadBytes;
                        if (message)
                        {
                            auto payload = message->get_payload();
                            if (payload)
                            {
                                const auto len =
                                    static_cast<std::size_t>(payload->get_length());
                                payloadBytes.assign(
                                    payload->get_data(),
                                    payload->get_data() + len);
                            }
                        }

                        std::function<void()> notifyHandler;
                        {
                            std::lock_guard<std::mutex> lock(mMutex);
                            if (mState == SubscriptionState::kNotSubscribed)
                            {
                                return;
                            }

                            if (mReceiveHandler)
                            {
                                // Queue the sample first, then notify
                                if (mSampleQueue.size() >= mMaxSampleCount)
                                {
                                    mSampleQueue.pop_front();
                                }
                                mSampleQueue.push_back(std::move(payloadBytes));
                                notifyHandler = mReceiveHandler;
                            }
                            else
                            {
                                if (mSampleQueue.size() >= mMaxSampleCount)
                                {
                                    mSampleQueue.pop_front();
                                }
                                mSampleQueue.push_back(std::move(payloadBytes));
                            }
                        }

                        if (notifyHandler)
                        {
                            notifyHandler();
                        }
                    });

                app->subscribe(
                    static_cast<vsomeip::service_t>(mConfig.ServiceId),
                    static_cast<vsomeip::instance_t>(mConfig.InstanceId),
                    static_cast<vsomeip::eventgroup_t>(mConfig.EventGroupId),
                    static_cast<vsomeip::major_version_t>(mConfig.MajorVersion),
                    static_cast<vsomeip::event_t>(mConfig.EventId));

                {
                    std::lock_guard<std::mutex> lock(mMutex);
                    mState = SubscriptionState::kSubscribed;
                }

                return core::Result<void>::FromValue();
            }

            void VsomeipProxyEventBinding::Unsubscribe()
            {
                {
                    std::lock_guard<std::mutex> lock(mMutex);
                    if (mState == SubscriptionState::kNotSubscribed)
                    {
                        return;
                    }
                    mState = SubscriptionState::kNotSubscribed;
                    mSampleQueue.clear();
                    mReceiveHandler = nullptr;
                }

                auto app = someip::VsomeipApplication::GetClientApplication();
                app->unsubscribe(
                    static_cast<vsomeip::service_t>(mConfig.ServiceId),
                    static_cast<vsomeip::instance_t>(mConfig.InstanceId),
                    static_cast<vsomeip::eventgroup_t>(mConfig.EventGroupId),
                    static_cast<vsomeip::event_t>(mConfig.EventId));
                app->release_event(
                    static_cast<vsomeip::service_t>(mConfig.ServiceId),
                    static_cast<vsomeip::instance_t>(mConfig.InstanceId),
                    static_cast<vsomeip::event_t>(mConfig.EventId));
                app->unregister_message_handler(
                    static_cast<vsomeip::service_t>(mConfig.ServiceId),
                    static_cast<vsomeip::instance_t>(mConfig.InstanceId),
                    static_cast<vsomeip::method_t>(mConfig.EventId));
            }

            SubscriptionState
            VsomeipProxyEventBinding::GetSubscriptionState() const noexcept
            {
                std::lock_guard<std::mutex> lock(mMutex);
                return mState;
            }

            core::Result<std::size_t> VsomeipProxyEventBinding::GetNewSamples(
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

            void VsomeipProxyEventBinding::SetReceiveHandler(
                std::function<void()> handler)
            {
                std::lock_guard<std::mutex> lock(mMutex);
                mReceiveHandler = std::move(handler);
            }

            void VsomeipProxyEventBinding::UnsetReceiveHandler()
            {
                std::lock_guard<std::mutex> lock(mMutex);
                mReceiveHandler = nullptr;
            }

            std::size_t
            VsomeipProxyEventBinding::GetFreeSampleCount() const noexcept
            {
                std::lock_guard<std::mutex> lock(mMutex);
                if (mSampleQueue.size() >= mMaxSampleCount)
                {
                    return 0U;
                }
                return mMaxSampleCount - mSampleQueue.size();
            }

            void VsomeipProxyEventBinding::SetSubscriptionStateChangeHandler(
                SubscriptionStateChangeHandler handler)
            {
                std::lock_guard<std::mutex> lock(mMutex);
                mStateChangeHandler = std::move(handler);
            }

            void VsomeipProxyEventBinding::UnsetSubscriptionStateChangeHandler()
            {
                std::lock_guard<std::mutex> lock(mMutex);
                mStateChangeHandler = nullptr;
            }

            // ── VsomeipSkeletonEventBinding ──────────────────────

            VsomeipSkeletonEventBinding::VsomeipSkeletonEventBinding(
                EventBindingConfig config) noexcept
                : mConfig{config}
            {
            }

            VsomeipSkeletonEventBinding::~VsomeipSkeletonEventBinding() noexcept
            {
                if (mOffered)
                {
                    StopOffer();
                }
            }

            core::Result<void> VsomeipSkeletonEventBinding::Offer()
            {
                if (mOffered)
                {
                    return core::Result<void>::FromError(
                        MakeErrorCode(ComErrc::kFieldValueIsNotValid));
                }

                auto app = someip::VsomeipApplication::GetServerApplication();
                std::set<vsomeip::eventgroup_t> eventGroups{
                    static_cast<vsomeip::eventgroup_t>(mConfig.EventGroupId)};
                app->offer_event(
                    static_cast<vsomeip::service_t>(mConfig.ServiceId),
                    static_cast<vsomeip::instance_t>(mConfig.InstanceId),
                    static_cast<vsomeip::event_t>(mConfig.EventId),
                    eventGroups);
                mOffered = true;

                return core::Result<void>::FromValue();
            }

            void VsomeipSkeletonEventBinding::StopOffer()
            {
                if (!mOffered)
                {
                    return;
                }

                auto app = someip::VsomeipApplication::GetServerApplication();
                app->stop_offer_event(
                    static_cast<vsomeip::service_t>(mConfig.ServiceId),
                    static_cast<vsomeip::instance_t>(mConfig.InstanceId),
                    static_cast<vsomeip::event_t>(mConfig.EventId));
                mOffered = false;
            }

            core::Result<void> VsomeipSkeletonEventBinding::Send(
                const std::vector<std::uint8_t> &payload)
            {
                if (!mOffered)
                {
                    return core::Result<void>::FromError(
                        MakeErrorCode(ComErrc::kServiceNotOffered));
                }

                auto app = someip::VsomeipApplication::GetServerApplication();
                auto vsPayload = vsomeip::runtime::get()->create_payload();
                std::vector<vsomeip::byte_t> payloadBytes{
                    payload.begin(), payload.end()};
                vsPayload->set_data(payloadBytes);

                app->notify(
                    static_cast<vsomeip::service_t>(mConfig.ServiceId),
                    static_cast<vsomeip::instance_t>(mConfig.InstanceId),
                    static_cast<vsomeip::event_t>(mConfig.EventId),
                    vsPayload,
                    true);

                return core::Result<void>::FromValue();
            }

            core::Result<void *> VsomeipSkeletonEventBinding::Allocate(
                std::size_t size)
            {
                // vsomeip does not support zero-copy allocation.
                // Return heap memory; SendAllocated will serialize and send.
                void *ptr = std::malloc(size);
                if (!ptr)
                {
                    return core::Result<void *>::FromError(
                        MakeErrorCode(ComErrc::kSampleAllocationFailure));
                }
                return core::Result<void *>::FromValue(ptr);
            }

            core::Result<void> VsomeipSkeletonEventBinding::SendAllocated(
                void *data, std::size_t size)
            {
                if (!data)
                {
                    return core::Result<void>::FromError(
                        MakeErrorCode(ComErrc::kFieldValueIsNotValid));
                }

                std::vector<std::uint8_t> payload(
                    static_cast<std::uint8_t *>(data),
                    static_cast<std::uint8_t *>(data) + size);
                std::free(data);

                return Send(payload);
            }
        }
    }
}
