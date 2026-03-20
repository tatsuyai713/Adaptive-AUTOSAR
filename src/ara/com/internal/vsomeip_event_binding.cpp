/// @file src/ara/com/internal/vsomeip_event_binding.cpp
/// @brief Implementation for vsomeip event binding.
/// @details This file is part of the Adaptive AUTOSAR educational implementation.

#include <algorithm>
#include <set>
#include <utility>
#include "./vsomeip_event_binding.h"

#if ARA_COM_USE_VSOMEIP

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
                SubscriptionStateChangeHandler pendingNotify;
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
                    if (mStateChangeHandler)
                    {
                        pendingNotify = mStateChangeHandler;
                    }
                }

                // Notify transition to kSubscriptionPending outside lock
                if (pendingNotify)
                {
                    pendingNotify(SubscriptionState::kSubscriptionPending);
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

                            if (mSampleQueue.size() >= mMaxSampleCount)
                            {
                                mSampleQueue.pop_front();
                            }
                            mSampleQueue.push_back(std::move(payloadBytes));
                            if (mReceiveHandler)
                            {
                                notifyHandler = mReceiveHandler;
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

                // Register subscription status handler for async ACK from remote
                app->register_subscription_status_handler(
                    static_cast<vsomeip::service_t>(mConfig.ServiceId),
                    static_cast<vsomeip::instance_t>(mConfig.InstanceId),
                    static_cast<vsomeip::eventgroup_t>(mConfig.EventGroupId),
                    static_cast<vsomeip::event_t>(mConfig.EventId),
                    [this](
                        const vsomeip::service_t,
                        const vsomeip::instance_t,
                        const vsomeip::eventgroup_t,
                        const vsomeip::event_t,
                        const uint16_t status)
                    {
                        SubscriptionStateChangeHandler notify;
                        SubscriptionState newState;
                        {
                            std::lock_guard<std::mutex> lock(mMutex);
                            if (mState == SubscriptionState::kNotSubscribed)
                            {
                                return;
                            }
                            // status == 0 means subscribed OK, non-zero means rejected
                            newState = (status == 0U)
                                           ? SubscriptionState::kSubscribed
                                           : SubscriptionState::kNotSubscribed;
                            if (mState != newState)
                            {
                                mState = newState;
                                if (mStateChangeHandler)
                                {
                                    notify = mStateChangeHandler;
                                }
                            }
                        }
                        if (notify)
                        {
                            notify(newState);
                        }
                    });

                // Set kSubscribed as a fallback for local routing (no async ACK)
                {
                    SubscriptionStateChangeHandler subscribedNotify;
                    std::lock_guard<std::mutex> lock(mMutex);
                    if (mState == SubscriptionState::kSubscriptionPending)
                    {
                        mState = SubscriptionState::kSubscribed;
                        if (mStateChangeHandler)
                        {
                            subscribedNotify = mStateChangeHandler;
                        }
                    }
                    if (subscribedNotify)
                    {
                        // Notify outside lock scope via the captured handler
                        // — but we are inside the lock here; schedule below.
                        pendingNotify = subscribedNotify;
                    }
                    else
                    {
                        pendingNotify = nullptr;
                    }
                }

                if (pendingNotify)
                {
                    pendingNotify(SubscriptionState::kSubscribed);
                }

                return core::Result<void>::FromValue();
            }

            void VsomeipProxyEventBinding::Unsubscribe()
            {
                SubscriptionStateChangeHandler notify;
                {
                    std::lock_guard<std::mutex> lock(mMutex);
                    if (mState == SubscriptionState::kNotSubscribed)
                    {
                        return;
                    }
                    mState = SubscriptionState::kNotSubscribed;
                    mSampleQueue.clear();
                    mReceiveHandler = nullptr;
                    if (mStateChangeHandler)
                    {
                        notify = mStateChangeHandler;
                    }
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

                if (notify)
                {
                    notify(SubscriptionState::kNotSubscribed);
                }
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

                std::size_t delivered = 0U;
                while (delivered < maxNumberOfSamples)
                {
                    std::vector<std::uint8_t> sample;
                    {
                        std::lock_guard<std::mutex> lock(mMutex);
                        if (mState != SubscriptionState::kSubscribed)
                        {
                            if (delivered == 0U)
                            {
                                return core::Result<std::size_t>::FromError(
                                    MakeErrorCode(ComErrc::kServiceNotAvailable));
                            }
                            break;
                        }
                        if (mSampleQueue.empty())
                        {
                            break;
                        }
                        sample = std::move(mSampleQueue.front());
                        mSampleQueue.pop_front();
                    }
                    handler(sample.data(), sample.size());
                    ++delivered;
                }
                return core::Result<std::size_t>::FromValue(delivered);
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

                // If an initial value is set, push it to vsomeip so that
                // every new subscriber immediately receives the current field value.
                if (mHasInitialValue)
                {
                    SendInitialNotification();
                }

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
                vsPayload->set_data(
                    reinterpret_cast<const vsomeip::byte_t *>(payload.data()),
                    static_cast<vsomeip::length_t>(payload.size()));

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

                auto vsPayload = vsomeip::runtime::get()->create_payload();
                vsPayload->set_data(
                    static_cast<const vsomeip::byte_t *>(data),
                    static_cast<vsomeip::length_t>(size));
                std::free(data);

                if (!mOffered)
                {
                    return core::Result<void>::FromError(
                        MakeErrorCode(ComErrc::kServiceNotOffered));
                }

                auto app = someip::VsomeipApplication::GetServerApplication();
                app->notify(
                    static_cast<vsomeip::service_t>(mConfig.ServiceId),
                    static_cast<vsomeip::instance_t>(mConfig.InstanceId),
                    static_cast<vsomeip::event_t>(mConfig.EventId),
                    vsPayload,
                    true);

                return core::Result<void>::FromValue();
            }

            void VsomeipSkeletonEventBinding::SetInitialValue(
                const std::vector<std::uint8_t> &payload)
            {
                mInitialValue = payload;
                mHasInitialValue = true;

                // If already offered, push the value immediately so that
                // late subscribers receive it via vsomeip's internal cache.
                if (mOffered)
                {
                    SendInitialNotification();
                }
            }

            void VsomeipSkeletonEventBinding::SendInitialNotification()
            {
                auto app = someip::VsomeipApplication::GetServerApplication();
                auto vsPayload = vsomeip::runtime::get()->create_payload();
                vsPayload->set_data(
                    reinterpret_cast<const vsomeip::byte_t *>(mInitialValue.data()),
                    static_cast<vsomeip::length_t>(mInitialValue.size()));

                // force=true ensures the notification is cached and delivered
                // to any subscriber immediately upon subscription.
                app->notify(
                    static_cast<vsomeip::service_t>(mConfig.ServiceId),
                    static_cast<vsomeip::instance_t>(mConfig.InstanceId),
                    static_cast<vsomeip::event_t>(mConfig.EventId),
                    vsPayload,
                    true);
            }
        }
    }
}

#endif // ARA_COM_USE_VSOMEIP
