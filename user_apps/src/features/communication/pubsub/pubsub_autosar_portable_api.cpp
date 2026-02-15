#include <algorithm>
#include <atomic>
#include <chrono>
#include <cctype>
#include <cstdint>
#include <deque>
#include <iomanip>
#include <mutex>
#include <sstream>
#include <thread>
#include <utility>
#include <vector>
#include "ara/com/com_error_domain.h"
#if defined(ARA_COM_USE_CYCLONEDDS) && (ARA_COM_USE_CYCLONEDDS == 1)
#include "ara/com/dds/dds_pubsub.h"
#include "VehicleStatusFrame.hpp"
#endif
#if defined(ARA_COM_USE_VSOMEIP) && (ARA_COM_USE_VSOMEIP == 1)
#include "../vehicle_status/vehicle_status_proxy.h"
#include "../vehicle_status/vehicle_status_skeleton.h"
#endif
#include "./pubsub_autosar_portable_api.h"
#include "./pubsub_common.h"

namespace
{
    // Default DDS topic used when caller does not provide one.
    constexpr const char *cDefaultDdsTopicName{
        "adaptive_autosar/sample/ara_com_pubsub/VehicleStatusFrame"};

    // Helper to create ara::core::Result errors in ara::com domain.
    template <typename T>
    ara::core::Result<T> MakeComError(
        ara::com::ComErrc errorCode)
    {
        return ara::core::Result<T>::FromError(
            ara::com::MakeErrorCode(errorCode));
    }

    std::string ToLowerAscii(std::string value)
    {
        // Backend parsing is case-insensitive for user-facing CLI options.
        std::transform(
            value.begin(),
            value.end(),
            value.begin(),
            [](unsigned char ch)
            {
                return static_cast<char>(std::tolower(ch));
            });
        return value;
    }

    sample::ara_com_pubsub::portable::BackendProfile NormalizeProfile(
        sample::ara_com_pubsub::portable::BackendProfile profile)
    {
        // Keep profile valid even when topic is omitted.
        if (profile.DdsTopicName.empty())
        {
            profile.DdsTopicName = cDefaultDdsTopicName;
        }

        return profile;
    }

    std::string ToChannelToken(
        const char *prefix,
        std::uint16_t id)
    {
        std::ostringstream stream;
        stream << prefix
               << "_0x"
               << std::uppercase
               << std::hex
               << std::setw(4)
               << std::setfill('0')
               << static_cast<unsigned>(id);
        return stream.str();
    }

    ara::com::zerocopy::ChannelDescriptor BuildChannelDescriptor(
        std::uint16_t serviceId,
        std::uint16_t instanceId,
        std::uint16_t eventId)
    {
        // Deterministic channel name derived from service/instance/event IDs.
        return ara::com::zerocopy::ChannelDescriptor{
            ToChannelToken("svc", serviceId),
            ToChannelToken("inst", instanceId),
            ToChannelToken("evt", eventId)};
    }

#if defined(ARA_COM_USE_VSOMEIP) && (ARA_COM_USE_VSOMEIP == 1)
    // Conversion helpers between portable frame type and SOME/IP sample type.
    sample::vehicle_status::VehicleStatusFrame ToStandardFrame(
        const sample::ara_com_pubsub::VehicleStatusFrame &frame) noexcept
    {
        sample::vehicle_status::VehicleStatusFrame converted{};
        converted.SequenceCounter = frame.SequenceCounter;
        converted.SpeedCentiKph = frame.SpeedCentiKph;
        converted.EngineRpm = frame.EngineRpm;
        converted.SteeringAngleCentiDeg = frame.SteeringAngleCentiDeg;
        converted.Gear = frame.Gear;
        converted.StatusFlags = frame.StatusFlags;
        return converted;
    }

    sample::ara_com_pubsub::VehicleStatusFrame ToPortableFrame(
        const sample::vehicle_status::VehicleStatusFrame &frame) noexcept
    {
        sample::ara_com_pubsub::VehicleStatusFrame converted{};
        converted.SequenceCounter = frame.SequenceCounter;
        converted.SpeedCentiKph = frame.SpeedCentiKph;
        converted.EngineRpm = frame.EngineRpm;
        converted.SteeringAngleCentiDeg = frame.SteeringAngleCentiDeg;
        converted.Gear = frame.Gear;
        converted.StatusFlags = frame.StatusFlags;
        return converted;
    }

    class VehicleStatusSkeletonImpl final
    {
    private:
        sample::vehicle_status::VehicleStatusServiceSkeleton mSkeleton;
        bool mEventOffered{false};
        bool mSubscriptionHandlerRegistered{false};
        std::size_t mSubscriberCount{0U};
        std::mutex mMutex;

    public:
        explicit VehicleStatusSkeletonImpl(
            ara::core::InstanceSpecifier specifier)
            : mSkeleton{std::move(specifier)}
        {
        }

        ara::core::Result<void> OfferService()
        {
            // Delegate service offer lifecycle to generated skeleton.
            return mSkeleton.OfferService();
        }

        void StopOfferService()
        {
            UnsetEventSubscriptionStateHandler(
                sample::ara_com_pubsub::cEventGroupId);

            if (mEventOffered)
            {
                mSkeleton.StatusEvent.StopOffer();
                mEventOffered = false;
            }

            mSkeleton.StopOfferService();

            std::lock_guard<std::mutex> lock(mMutex);
            mSubscriberCount = 0U;
        }

        ara::core::Result<void> OfferEvent(
            std::uint16_t eventId,
            std::uint16_t eventGroupId)
        {
            if (eventId != sample::ara_com_pubsub::cEventId ||
                eventGroupId != sample::ara_com_pubsub::cEventGroupId)
            {
                return MakeComError<void>(
                    ara::com::ComErrc::kFieldValueIsNotValid);
            }

            if (mEventOffered)
            {
                return MakeComError<void>(
                    ara::com::ComErrc::kFieldValueIsNotValid);
            }

            auto result{mSkeleton.StatusEvent.Offer()};
            if (result.HasValue())
            {
                mEventOffered = true;
            }

            return result;
        }

        void StopOfferEvent(std::uint16_t eventId)
        {
            if (eventId != sample::ara_com_pubsub::cEventId)
            {
                return;
            }

            if (mEventOffered)
            {
                mSkeleton.StatusEvent.StopOffer();
                mEventOffered = false;
            }
        }

        ara::core::Result<void> NotifyEvent(
            std::uint16_t eventId,
            const std::vector<std::uint8_t> &payload,
            bool force) const
        {
            (void)force;

            if (eventId != sample::ara_com_pubsub::cEventId)
            {
                return MakeComError<void>(
                    ara::com::ComErrc::kFieldValueIsNotValid);
            }

            sample::ara_com_pubsub::VehicleStatusFrame localFrame{};
            if (!sample::ara_com_pubsub::DeserializeFrame(payload, localFrame))
            {
                return MakeComError<void>(
                    ara::com::ComErrc::kFieldValueIsNotValid);
            }

            const auto frame{ToStandardFrame(localFrame)};
            const_cast<sample::vehicle_status::VehicleStatusServiceSkeleton &>(
                mSkeleton)
                .StatusEvent.Send(frame);
            return ara::core::Result<void>::FromValue();
        }

        ara::core::Result<void> SetEventSubscriptionStateHandler(
            std::uint16_t eventGroupId,
            sample::ara_com_pubsub::portable::SubscriptionStateHandler handler)
        {
            if (eventGroupId != sample::ara_com_pubsub::cEventGroupId)
            {
                return MakeComError<void>(
                    ara::com::ComErrc::kFieldValueIsNotValid);
            }

            if (!handler)
            {
                return MakeComError<void>(
                    ara::com::ComErrc::kSetHandlerNotSet);
            }

            if (!mSkeleton.IsOffered())
            {
                return MakeComError<void>(
                    ara::com::ComErrc::kServiceNotOffered);
            }

            if (mSubscriptionHandlerRegistered)
            {
                return MakeComError<void>(
                    ara::com::ComErrc::kFieldValueIsNotValid);
            }

            auto result{
                mSkeleton.SetEventSubscriptionStateHandler(
                    eventGroupId,
                    [this, handler = std::move(handler)](
                        std::uint16_t clientId,
                        bool subscribed)
                    {
                        const bool accepted{
                            handler(clientId, subscribed)};

                        if (accepted)
                        {
                            std::lock_guard<std::mutex> lock(mMutex);
                            if (subscribed)
                            {
                                ++mSubscriberCount;
                            }
                            else if (mSubscriberCount > 0U)
                            {
                                --mSubscriberCount;
                            }
                        }

                        return accepted;
                    })};

            if (!result.HasValue())
            {
                return result;
            }

            mSubscriptionHandlerRegistered = true;
            return ara::core::Result<void>::FromValue();
        }

        void UnsetEventSubscriptionStateHandler(std::uint16_t eventGroupId)
        {
            if (eventGroupId != sample::ara_com_pubsub::cEventGroupId ||
                !mSubscriptionHandlerRegistered)
            {
                return;
            }

            mSkeleton.UnsetEventSubscriptionStateHandler(eventGroupId);

            mSubscriptionHandlerRegistered = false;
            std::lock_guard<std::mutex> lock(mMutex);
            mSubscriberCount = 0U;
        }

        ara::core::Result<ara::com::zerocopy::ZeroCopyPublisher>
        CreateZeroCopyPublisher(
            std::uint16_t eventId,
            std::string runtimeName,
            std::uint64_t historyCapacity) const
        {
            if (!mSkeleton.IsOffered())
            {
                return MakeComError<ara::com::zerocopy::ZeroCopyPublisher>(
                    ara::com::ComErrc::kServiceNotOffered);
            }

            if (eventId != sample::ara_com_pubsub::cEventId)
            {
                return MakeComError<ara::com::zerocopy::ZeroCopyPublisher>(
                    ara::com::ComErrc::kFieldValueIsNotValid);
            }

            ara::com::zerocopy::ZeroCopyPublisher publisher{
                BuildChannelDescriptor(
                    sample::ara_com_pubsub::cServiceId,
                    sample::ara_com_pubsub::cInstanceId,
                    eventId),
                std::move(runtimeName),
                historyCapacity};

            if (!publisher.IsBindingActive())
            {
                return MakeComError<ara::com::zerocopy::ZeroCopyPublisher>(
                    ara::com::ComErrc::kNetworkBindingFailure);
            }

            return ara::core::Result<ara::com::zerocopy::ZeroCopyPublisher>::FromValue(
                std::move(publisher));
        }
    };

    class VehicleStatusProxyImpl final
    {
    private:
        sample::vehicle_status::VehicleStatusServiceProxy mProxy;
        std::size_t mSampleQueueLimit{16U};
        bool mQueueOverflowViolation{false};
        bool mSubscribed{false};
        sample::ara_com_pubsub::portable::EventReceiveHandler mReceiveHandler;
        sample::ara_com_pubsub::portable::SubscriptionStatusHandler mSubscriptionStatusHandler;
        mutable std::mutex mMutex;

        static ara::core::Result<void> ValidateEventIds(
            std::uint16_t eventId,
            std::uint16_t eventGroupId)
        {
            if (eventId != sample::ara_com_pubsub::cEventId ||
                eventGroupId != sample::ara_com_pubsub::cEventGroupId)
            {
                return MakeComError<void>(
                    ara::com::ComErrc::kFieldValueIsNotValid);
            }

            return ara::core::Result<void>::FromValue();
        }

        void OnSubscriptionStateChanged(ara::com::SubscriptionState state)
        {
            sample::ara_com_pubsub::portable::SubscriptionStatusHandler statusHandler;
            {
                std::lock_guard<std::mutex> lock(mMutex);
                statusHandler = mSubscriptionStatusHandler;
            }

            if (!statusHandler)
            {
                return;
            }

            const std::uint16_t statusCode{
                static_cast<std::uint16_t>(
                    state == ara::com::SubscriptionState::kSubscribed ? 0U : 1U)};
            statusHandler(statusCode);
        }

        void OnReceiveSignal()
        {
            // Pull all currently queued SOME/IP samples into portable callbacks.
            sample::ara_com_pubsub::portable::EventReceiveHandler receiveHandler;
            {
                std::lock_guard<std::mutex> lock(mMutex);
                if (mProxy.StatusEvent.GetFreeSampleCount() == 0U)
                {
                    mQueueOverflowViolation = true;
                }
                receiveHandler = mReceiveHandler;
            }

            if (!receiveHandler)
            {
                return;
            }

            (void)mProxy.StatusEvent.GetNewSamples(
                [&receiveHandler](ara::com::SamplePtr<sample::vehicle_status::VehicleStatusFrame> sample)
                {
                    const auto payload{
                        sample::ara_com_pubsub::SerializeFrame(
                            ToPortableFrame(*sample))};
                    receiveHandler(payload);
                });
        }

        static ara::core::Result<void> ValidateEventId(std::uint16_t eventId)
        {
            if (eventId != sample::ara_com_pubsub::cEventId)
            {
                return MakeComError<void>(
                    ara::com::ComErrc::kFieldValueIsNotValid);
            }

            return ara::core::Result<void>::FromValue();
        }

    public:
        enum class SubscriptionState : std::uint8_t
        {
            kNotSubscribed = 0U,
            kSubscriptionPending = 1U,
            kSubscribed = 2U
        };

        explicit VehicleStatusProxyImpl(
            ara::com::ServiceHandleType handle)
            : mProxy{std::move(handle)}
        {
        }

        static ara::core::Result<void> StartFindService(
            std::function<void(std::vector<ara::com::ServiceHandleType>)> handler,
            std::uint16_t serviceId,
            std::uint16_t instanceId)
        {
            auto specifierResult{
                ara::core::InstanceSpecifier::Create(
                    sample::ara_com_pubsub::cConsumerInstanceSpecifier)};
            if (!specifierResult.HasValue())
            {
                return ara::core::Result<void>::FromError(
                    specifierResult.Error());
            }

            auto startResult{
                sample::vehicle_status::VehicleStatusServiceProxy::StartFindService(
                    [handler = std::move(handler), serviceId, instanceId](
                        ara::com::ServiceHandleContainer<ara::com::ServiceHandleType> handles)
                    {
                        if (!handler)
                        {
                            return;
                        }

                        std::vector<ara::com::ServiceHandleType> filtered;
                        for (const auto &handle : handles)
                        {
                            if (handle.GetServiceId() != serviceId)
                            {
                                continue;
                            }

                            if (instanceId != 0xFFFFU &&
                                handle.GetInstanceId() != instanceId)
                            {
                                continue;
                            }

                            filtered.push_back(handle);
                        }

                        handler(std::move(filtered));
                    },
                    std::move(specifierResult).Value())};

            if (!startResult.HasValue())
            {
                return ara::core::Result<void>::FromError(
                    startResult.Error());
            }

            return ara::core::Result<void>::FromValue();
        }

        static void StopFindService()
        {
            sample::vehicle_status::VehicleStatusServiceProxy::StopFindService(
                ara::com::FindServiceHandle{0U});
        }

        ara::core::Result<void> SubscribeEvent(
            std::uint16_t eventId,
            std::uint16_t eventGroupId,
            sample::ara_com_pubsub::portable::EventReceiveHandler handler,
            std::uint8_t majorVersion)
        {
            auto validationResult{
                ValidateEventIds(eventId, eventGroupId)};
            if (!validationResult.HasValue())
            {
                return validationResult;
            }

            if (!handler)
            {
                return MakeComError<void>(
                    ara::com::ComErrc::kSetHandlerNotSet);
            }

            auto subscribeResult{
                SubscribeEvent(eventId, eventGroupId, majorVersion)};
            if (!subscribeResult.HasValue())
            {
                return subscribeResult;
            }

            return SetReceiveHandler(eventId, std::move(handler));
        }

        ara::core::Result<void> SubscribeEvent(
            std::uint16_t eventId,
            std::uint16_t eventGroupId,
            std::uint8_t majorVersion)
        {
            (void)majorVersion;

            auto validationResult{
                ValidateEventIds(eventId, eventGroupId)};
            if (!validationResult.HasValue())
            {
                return validationResult;
            }

            {
                std::lock_guard<std::mutex> lock(mMutex);
                if (mSubscribed)
                {
                    return MakeComError<void>(
                        ara::com::ComErrc::kFieldValueIsNotValid);
                }

                mQueueOverflowViolation = false;
            }

            mProxy.StatusEvent.Subscribe(mSampleQueueLimit);
            mProxy.StatusEvent.SetReceiveHandler(
                [this]()
                {
                    OnReceiveSignal();
                });
            mProxy.StatusEvent.SetSubscriptionStateChangeHandler(
                [this](ara::com::SubscriptionState state)
                {
                    OnSubscriptionStateChanged(state);
                });

            {
                std::lock_guard<std::mutex> lock(mMutex);
                mSubscribed = true;
            }

            return ara::core::Result<void>::FromValue();
        }

        void UnsubscribeEvent(
            std::uint16_t eventId,
            std::uint16_t eventGroupId)
        {
            auto validationResult{
                ValidateEventIds(eventId, eventGroupId)};
            if (!validationResult.HasValue())
            {
                return;
            }

            mProxy.StatusEvent.UnsetReceiveHandler();
            mProxy.StatusEvent.UnsetSubscriptionStateChangeHandler();
            mProxy.StatusEvent.Unsubscribe();

            std::lock_guard<std::mutex> lock(mMutex);
            mSubscribed = false;
            mQueueOverflowViolation = false;
            mReceiveHandler = sample::ara_com_pubsub::portable::EventReceiveHandler{};
            mSubscriptionStatusHandler = sample::ara_com_pubsub::portable::SubscriptionStatusHandler{};
        }

        ara::core::Result<void> SetReceiveHandler(
            std::uint16_t eventId,
            sample::ara_com_pubsub::portable::EventReceiveHandler handler)
        {
            auto validationResult{
                ValidateEventId(eventId)};
            if (!validationResult.HasValue())
            {
                return validationResult;
            }

            if (!handler)
            {
                return MakeComError<void>(
                    ara::com::ComErrc::kSetHandlerNotSet);
            }

            {
                std::lock_guard<std::mutex> lock(mMutex);
                if (!mSubscribed)
                {
                    return MakeComError<void>(
                        ara::com::ComErrc::kServiceNotAvailable);
                }

                mReceiveHandler = std::move(handler);
            }

            return ara::core::Result<void>::FromValue();
        }

        void UnsetReceiveHandler(std::uint16_t eventId)
        {
            auto validationResult{
                ValidateEventId(eventId)};
            if (!validationResult.HasValue())
            {
                return;
            }

            std::lock_guard<std::mutex> lock(mMutex);
            mReceiveHandler = sample::ara_com_pubsub::portable::EventReceiveHandler{};
        }

        ara::core::Result<std::size_t> GetNewSamples(
            std::uint16_t eventId,
            std::size_t maxSamples,
            sample::ara_com_pubsub::portable::EventReceiveHandler handler)
        {
            auto validationResult{
                ValidateEventId(eventId)};
            if (!validationResult.HasValue())
            {
                return ara::core::Result<std::size_t>::FromError(
                    validationResult.Error());
            }

            if (!handler)
            {
                return MakeComError<std::size_t>(
                    ara::com::ComErrc::kSetHandlerNotSet);
            }

            if (maxSamples == 0U)
            {
                return MakeComError<std::size_t>(
                    ara::com::ComErrc::kFieldValueIsNotValid);
            }

            {
                std::lock_guard<std::mutex> lock(mMutex);
                if (!mSubscribed)
                {
                    return MakeComError<std::size_t>(
                        ara::com::ComErrc::kServiceNotAvailable);
                }
            }

            return mProxy.StatusEvent.GetNewSamples(
                [&handler](ara::com::SamplePtr<sample::vehicle_status::VehicleStatusFrame> sample)
                {
                    const auto payload{
                        sample::ara_com_pubsub::SerializeFrame(
                            ToPortableFrame(*sample))};
                    handler(payload);
                },
                maxSamples);
        }

        ara::core::Result<void> SetSampleQueueLimit(
            std::uint16_t eventId,
            std::size_t maxSamples)
        {
            auto validationResult{
                ValidateEventId(eventId)};
            if (!validationResult.HasValue())
            {
                return validationResult;
            }

            if (maxSamples == 0U)
            {
                return MakeComError<void>(
                    ara::com::ComErrc::kFieldValueIsNotValid);
            }

            {
                std::lock_guard<std::mutex> lock(mMutex);
                if (mSubscribed)
                {
                    return MakeComError<void>(
                        ara::com::ComErrc::kFieldValueIsNotValid);
                }

                mSampleQueueLimit = maxSamples;
            }

            return ara::core::Result<void>::FromValue();
        }

        SubscriptionState GetSubscriptionState(
            std::uint16_t eventId) const noexcept
        {
            auto validationResult{
                ValidateEventId(eventId)};
            if (!validationResult.HasValue())
            {
                return SubscriptionState::kNotSubscribed;
            }

            switch (mProxy.StatusEvent.GetSubscriptionState())
            {
            case ara::com::SubscriptionState::kSubscriptionPending:
                return SubscriptionState::kSubscriptionPending;
            case ara::com::SubscriptionState::kSubscribed:
                return SubscriptionState::kSubscribed;
            case ara::com::SubscriptionState::kNotSubscribed:
            default:
                return SubscriptionState::kNotSubscribed;
            }
        }

        bool HasSampleQueueOverflowViolation(
            std::uint16_t eventId) const noexcept
        {
            auto validationResult{
                ValidateEventId(eventId)};
            if (!validationResult.HasValue())
            {
                return false;
            }

            std::lock_guard<std::mutex> lock(mMutex);
            return mQueueOverflowViolation;
        }

        void ClearSampleQueueOverflowViolation(
            std::uint16_t eventId) noexcept
        {
            auto validationResult{
                ValidateEventId(eventId)};
            if (!validationResult.HasValue())
            {
                return;
            }

            std::lock_guard<std::mutex> lock(mMutex);
            mQueueOverflowViolation = false;
        }

        ara::core::Result<void> SetSubscriptionStatusHandler(
            std::uint16_t eventId,
            std::uint16_t eventGroupId,
            sample::ara_com_pubsub::portable::SubscriptionStatusHandler handler)
        {
            auto validationResult{
                ValidateEventIds(eventId, eventGroupId)};
            if (!validationResult.HasValue())
            {
                return validationResult;
            }

            if (!handler)
            {
                return MakeComError<void>(
                    ara::com::ComErrc::kSetHandlerNotSet);
            }

            {
                std::lock_guard<std::mutex> lock(mMutex);
                if (!mSubscribed)
                {
                    return MakeComError<void>(
                        ara::com::ComErrc::kServiceNotAvailable);
                }

                mSubscriptionStatusHandler = std::move(handler);
            }

            return ara::core::Result<void>::FromValue();
        }

        void UnsetSubscriptionStatusHandler(
            std::uint16_t eventId,
            std::uint16_t eventGroupId)
        {
            auto validationResult{
                ValidateEventIds(eventId, eventGroupId)};
            if (!validationResult.HasValue())
            {
                return;
            }

            std::lock_guard<std::mutex> lock(mMutex);
            mSubscriptionStatusHandler = sample::ara_com_pubsub::portable::SubscriptionStatusHandler{};
        }

        ara::core::Result<ara::com::zerocopy::ZeroCopySubscriber> CreateZeroCopySubscriber(
            std::uint16_t eventId,
            std::string runtimeName,
            std::uint64_t queueCapacity,
            std::uint64_t historyRequest) const
        {
            auto validationResult{
                ValidateEventId(eventId)};
            if (!validationResult.HasValue())
            {
                return MakeComError<ara::com::zerocopy::ZeroCopySubscriber>(
                    ara::com::ComErrc::kFieldValueIsNotValid);
            }

            if (GetSubscriptionState(eventId) == SubscriptionState::kNotSubscribed)
            {
                return MakeComError<ara::com::zerocopy::ZeroCopySubscriber>(
                    ara::com::ComErrc::kServiceNotAvailable);
            }

            const auto &handle{mProxy.GetHandle()};
            ara::com::zerocopy::ZeroCopySubscriber subscriber{
                BuildChannelDescriptor(
                    handle.GetServiceId(),
                    handle.GetInstanceId(),
                    eventId),
                std::move(runtimeName),
                queueCapacity,
                historyRequest};

            if (!subscriber.IsBindingActive())
            {
                return MakeComError<ara::com::zerocopy::ZeroCopySubscriber>(
                    ara::com::ComErrc::kNetworkBindingFailure);
            }

            return ara::core::Result<ara::com::zerocopy::ZeroCopySubscriber>::FromValue(
                std::move(subscriber));
        }
    };

    ara::com::ServiceHandleType ToAraComHandle(
        const sample::ara_com_pubsub::portable::VehicleStatusServiceHandle &handle)
    {
        return ara::com::ServiceHandleType{
            handle.ServiceId,
            handle.InstanceId};
    }
#endif

#if defined(ARA_COM_USE_CYCLONEDDS) && (ARA_COM_USE_CYCLONEDDS == 1)
    // DDS mapping type generated from IDL.
    using DdsVehicleStatusFrame =
        adaptive_autosar::sample::ara_com_pubsub::VehicleStatusFrame;

    sample::ara_com_pubsub::VehicleStatusFrame ToLocalFrame(
        const DdsVehicleStatusFrame &frame)
    {
        sample::ara_com_pubsub::VehicleStatusFrame localFrame;
        localFrame.SequenceCounter = frame.SequenceCounter();
        localFrame.SpeedCentiKph = frame.SpeedCentiKph();
        localFrame.EngineRpm = frame.EngineRpm();
        localFrame.SteeringAngleCentiDeg = frame.SteeringAngleCentiDeg();
        localFrame.Gear = frame.Gear();
        localFrame.StatusFlags = frame.StatusFlags();
        return localFrame;
    }

    DdsVehicleStatusFrame ToDdsFrame(
        const sample::ara_com_pubsub::VehicleStatusFrame &frame)
    {
        DdsVehicleStatusFrame ddsFrame;
        ddsFrame.SequenceCounter(frame.SequenceCounter);
        ddsFrame.SpeedCentiKph(frame.SpeedCentiKph);
        ddsFrame.EngineRpm(frame.EngineRpm);
        ddsFrame.SteeringAngleCentiDeg(frame.SteeringAngleCentiDeg);
        ddsFrame.Gear(frame.Gear);
        ddsFrame.StatusFlags(frame.StatusFlags);
        return ddsFrame;
    }

    struct DdsProviderState
    {
        explicit DdsProviderState(
            std::string topicName,
            std::uint32_t domainId) : Publisher{
                                           std::move(topicName),
                                           domainId}
        {
        }

        ara::com::dds::DdsPublisher<DdsVehicleStatusFrame> Publisher;
        sample::ara_com_pubsub::portable::SubscriptionStateHandler SubscriptionHandler;
        bool ServiceOffered{false};
        bool EventOffered{false};
        std::int32_t LastMatchedSubscribers{0};
        mutable std::mutex Mutex;
    };

    struct DdsConsumerState
    {
        explicit DdsConsumerState(
            std::string topicName,
            std::uint32_t domainId) : Subscriber{
                                           std::move(topicName),
                                           domainId}
        {
            PollingThread = std::thread(
                [this]()
                {
                    PollLoop();
                });
        }

        ~DdsConsumerState() noexcept
        {
            Running.store(false);
            if (PollingThread.joinable())
            {
                PollingThread.join();
            }
        }

        void PollLoop()
        {
            // Poll DDS reader in a background thread and fan-out to handlers/queue.
            while (Running.load())
            {
                bool shouldPoll{false};
                {
                    std::lock_guard<std::mutex> lock(Mutex);
                    shouldPoll =
                        (State == sample::ara_com_pubsub::portable::VehicleStatusConsumer::SubscriptionState::kSubscribed);
                }

                if (shouldPoll && Subscriber.IsBindingActive())
                {
                    (void)Subscriber.Take(
                        32U,
                        [this](const DdsVehicleStatusFrame &sample)
                        {
                            const auto payload{
                                sample::ara_com_pubsub::SerializeFrame(
                                    ToLocalFrame(sample))};

                            sample::ara_com_pubsub::portable::EventReceiveHandler receiveHandler;
                            {
                                std::lock_guard<std::mutex> lock(Mutex);
                                receiveHandler = ReceiveHandler;
                                if (!receiveHandler)
                                {
                                    const auto queueLimit{
                                        std::max<std::size_t>(1U, MaxSampleQueue)};
                                    if (SampleQueue.size() >= queueLimit)
                                    {
                                        SampleQueue.pop_front();
                                        MaxSampleQueueExceeded = true;
                                    }

                                    SampleQueue.push_back(payload);
                                    return;
                                }
                            }

                            receiveHandler(payload);
                        });

                    sample::ara_com_pubsub::portable::SubscriptionStatusHandler statusHandler;
                    std::uint16_t statusCode{0U};
                    bool statusChanged{false};
                    {
                        auto matchedCountResult{
                            Subscriber.GetMatchedPublicationCount()};
                        if (matchedCountResult.HasValue())
                        {
                            std::lock_guard<std::mutex> lock(Mutex);
                            const std::int32_t matchedPublishers{
                                matchedCountResult.Value()};
                            if (matchedPublishers != LastMatchedPublishers)
                            {
                                LastMatchedPublishers = matchedPublishers;
                                statusHandler = SubscriptionStatus;
                                statusCode = matchedPublishers > 0 ? 0U : 1U;
                                statusChanged = true;
                            }
                        }
                    }

                    if (statusChanged && statusHandler)
                    {
                        statusHandler(statusCode);
                    }
                }

                std::this_thread::sleep_for(std::chrono::milliseconds(20));
            }
        }

        ara::com::dds::DdsSubscriber<DdsVehicleStatusFrame> Subscriber;
        sample::ara_com_pubsub::portable::EventReceiveHandler ReceiveHandler;
        sample::ara_com_pubsub::portable::SubscriptionStatusHandler SubscriptionStatus;
        sample::ara_com_pubsub::portable::VehicleStatusConsumer::SubscriptionState State{
            sample::ara_com_pubsub::portable::VehicleStatusConsumer::SubscriptionState::kNotSubscribed};
        std::deque<std::vector<std::uint8_t>> SampleQueue;
        std::size_t MaxSampleQueue{16U};
        bool MaxSampleQueueExceeded{false};
        std::int32_t LastMatchedPublishers{0};
        std::atomic_bool Running{true};
        std::thread PollingThread;
        mutable std::mutex Mutex;
    };
#endif

    struct PortableFindServiceContext
    {
        // Keep one active find-service operation to match sample API behavior.
        bool Active{false};
        sample::ara_com_pubsub::portable::EventBackend Backend{
            sample::ara_com_pubsub::portable::EventBackend::kSomeIp};
        std::mutex Mutex;
    };

    PortableFindServiceContext &GetPortableFindServiceContext()
    {
        static PortableFindServiceContext context;
        return context;
    }
}

namespace sample
{
    namespace ara_com_pubsub
    {
        namespace portable
        {
            const char *ToString(EventBackend backend) noexcept
            {
                switch (backend)
                {
                case EventBackend::kDds:
                    return "dds";
                case EventBackend::kSomeIp:
                default:
                    return "someip";
                }
            }

            const char *ToString(ZeroCopyBackend backend) noexcept
            {
                switch (backend)
                {
                case ZeroCopyBackend::kZeroCopy:
                    return "zerocopy";
                case ZeroCopyBackend::kNone:
                default:
                    return "none";
                }
            }

            ara::core::Result<EventBackend> ParseEventBackend(
                std::string backendName)
            {
                // Accept neutral and backend-specific aliases.
                backendName = ToLowerAscii(std::move(backendName));
                if (backendName == "someip" || backendName == "vsomeip")
                {
                    return ara::core::Result<EventBackend>::FromValue(
                        EventBackend::kSomeIp);
                }
                if (backendName == "cyclonedds" ||
                    backendName == "dds" ||
                    backendName == "cyclone-dds")
                {
                    return ara::core::Result<EventBackend>::FromValue(
                        EventBackend::kDds);
                }

                return MakeComError<EventBackend>(
                    ara::com::ComErrc::kFieldValueIsNotValid);
            }

            ara::core::Result<ZeroCopyBackend> ParseZeroCopyBackend(
                std::string backendName)
            {
                // Keep legacy aliases for compatibility with existing scripts.
                backendName = ToLowerAscii(std::move(backendName));
                if (backendName == "none" ||
                    backendName == "off" ||
                    backendName == "disabled")
                {
                    return ara::core::Result<ZeroCopyBackend>::FromValue(
                        ZeroCopyBackend::kNone);
                }
                if (backendName == "iceoryx" ||
                    backendName == "zerocopy" ||
                    backendName == "on" ||
                    backendName == "enabled")
                {
                    return ara::core::Result<ZeroCopyBackend>::FromValue(
                        ZeroCopyBackend::kZeroCopy);
                }

                return MakeComError<ZeroCopyBackend>(
                    ara::com::ComErrc::kFieldValueIsNotValid);
            }

            class VehicleStatusProvider::Impl
            {
            public:
                explicit Impl(
                    ara::core::InstanceSpecifier specifier,
                    BackendProfile profile) : Profile{NormalizeProfile(std::move(profile))}
                {
                    // Create middleware-specific state behind transport-neutral API.
                    switch (Profile.EventBinding)
                    {
                    case EventBackend::kSomeIp:
#if defined(ARA_COM_USE_VSOMEIP) && (ARA_COM_USE_VSOMEIP == 1)
                        SomeIpSkeleton.reset(
                            new VehicleStatusSkeletonImpl{std::move(specifier)});
#else
                        (void)specifier;
#endif
                        break;
                    case EventBackend::kDds:
#if defined(ARA_COM_USE_CYCLONEDDS) && (ARA_COM_USE_CYCLONEDDS == 1)
                        DdsState.reset(new DdsProviderState{
                            Profile.DdsTopicName,
                            Profile.DdsDomainId});
#else
                        (void)specifier;
#endif
                        break;
                    }
                }

                BackendProfile Profile;

#if defined(ARA_COM_USE_VSOMEIP) && (ARA_COM_USE_VSOMEIP == 1)
                std::unique_ptr<VehicleStatusSkeletonImpl> SomeIpSkeleton;
#endif
#if defined(ARA_COM_USE_CYCLONEDDS) && (ARA_COM_USE_CYCLONEDDS == 1)
                std::unique_ptr<DdsProviderState> DdsState;
#endif
            };

            VehicleStatusProvider::VehicleStatusProvider(
                ara::core::InstanceSpecifier specifier,
                BackendProfile profile) : mImpl{new Impl{
                                                     std::move(specifier),
                                                     std::move(profile)}}
            {
            }

            VehicleStatusProvider::~VehicleStatusProvider() noexcept = default;
            VehicleStatusProvider::VehicleStatusProvider(VehicleStatusProvider &&other) noexcept = default;
            VehicleStatusProvider &VehicleStatusProvider::operator=(VehicleStatusProvider &&other) noexcept = default;

            EventBackend VehicleStatusProvider::GetEventBackend() const noexcept
            {
                return mImpl->Profile.EventBinding;
            }

            ZeroCopyBackend VehicleStatusProvider::GetZeroCopyBackend() const noexcept
            {
                return mImpl->Profile.ZeroCopyBinding;
            }

            ara::core::Result<void> VehicleStatusProvider::OfferService()
            {
                // Offer service in backend-specific way with unified error mapping.
                switch (mImpl->Profile.EventBinding)
                {
                case EventBackend::kSomeIp:
#if defined(ARA_COM_USE_VSOMEIP) && (ARA_COM_USE_VSOMEIP == 1)
                    if (!mImpl->SomeIpSkeleton)
                    {
                        return MakeComError<void>(
                            ara::com::ComErrc::kNetworkBindingFailure);
                    }
                    return mImpl->SomeIpSkeleton->OfferService();
#else
                    return MakeComError<void>(
                        ara::com::ComErrc::kNetworkBindingFailure);
#endif
                case EventBackend::kDds:
#if defined(ARA_COM_USE_CYCLONEDDS) && (ARA_COM_USE_CYCLONEDDS == 1)
                    if (!mImpl->DdsState ||
                        !mImpl->DdsState->Publisher.IsBindingActive())
                    {
                        return MakeComError<void>(
                            ara::com::ComErrc::kNetworkBindingFailure);
                    }

                    {
                        std::lock_guard<std::mutex> lock(mImpl->DdsState->Mutex);
                        mImpl->DdsState->ServiceOffered = true;
                    }
                    return ara::core::Result<void>::FromValue();
#else
                    return MakeComError<void>(
                        ara::com::ComErrc::kNetworkBindingFailure);
#endif
                }

                return MakeComError<void>(
                    ara::com::ComErrc::kNetworkBindingFailure);
            }

            void VehicleStatusProvider::StopOfferService()
            {
                switch (mImpl->Profile.EventBinding)
                {
                case EventBackend::kSomeIp:
#if defined(ARA_COM_USE_VSOMEIP) && (ARA_COM_USE_VSOMEIP == 1)
                    if (mImpl->SomeIpSkeleton)
                    {
                        mImpl->SomeIpSkeleton->StopOfferService();
                    }
#endif
                    break;
                case EventBackend::kDds:
#if defined(ARA_COM_USE_CYCLONEDDS) && (ARA_COM_USE_CYCLONEDDS == 1)
                    if (mImpl->DdsState)
                    {
                        std::lock_guard<std::mutex> lock(mImpl->DdsState->Mutex);
                        mImpl->DdsState->ServiceOffered = false;
                        mImpl->DdsState->EventOffered = false;
                        mImpl->DdsState->LastMatchedSubscribers = 0;
                    }
#endif
                    break;
                }
            }

            ara::core::Result<void> VehicleStatusProvider::OfferEvent()
            {
                switch (mImpl->Profile.EventBinding)
                {
                case EventBackend::kSomeIp:
#if defined(ARA_COM_USE_VSOMEIP) && (ARA_COM_USE_VSOMEIP == 1)
                    if (!mImpl->SomeIpSkeleton)
                    {
                        return MakeComError<void>(
                            ara::com::ComErrc::kNetworkBindingFailure);
                    }

                    return mImpl->SomeIpSkeleton->OfferEvent(
                        cEventId,
                        cEventGroupId);
#else
                    return MakeComError<void>(
                        ara::com::ComErrc::kNetworkBindingFailure);
#endif
                case EventBackend::kDds:
#if defined(ARA_COM_USE_CYCLONEDDS) && (ARA_COM_USE_CYCLONEDDS == 1)
                    if (!mImpl->DdsState)
                    {
                        return MakeComError<void>(
                            ara::com::ComErrc::kNetworkBindingFailure);
                    }

                    std::lock_guard<std::mutex> lock(mImpl->DdsState->Mutex);
                    if (!mImpl->DdsState->ServiceOffered)
                    {
                        return MakeComError<void>(
                            ara::com::ComErrc::kServiceNotOffered);
                    }

                    mImpl->DdsState->EventOffered = true;
                    return ara::core::Result<void>::FromValue();
#else
                    return MakeComError<void>(
                        ara::com::ComErrc::kNetworkBindingFailure);
#endif
                }

                return MakeComError<void>(
                    ara::com::ComErrc::kNetworkBindingFailure);
            }

            void VehicleStatusProvider::StopOfferEvent()
            {
                switch (mImpl->Profile.EventBinding)
                {
                case EventBackend::kSomeIp:
#if defined(ARA_COM_USE_VSOMEIP) && (ARA_COM_USE_VSOMEIP == 1)
                    if (mImpl->SomeIpSkeleton)
                    {
                        mImpl->SomeIpSkeleton->StopOfferEvent(cEventId);
                    }
#endif
                    break;
                case EventBackend::kDds:
#if defined(ARA_COM_USE_CYCLONEDDS) && (ARA_COM_USE_CYCLONEDDS == 1)
                    if (mImpl->DdsState)
                    {
                        std::lock_guard<std::mutex> lock(mImpl->DdsState->Mutex);
                        mImpl->DdsState->EventOffered = false;
                    }
#endif
                    break;
                }
            }

            ara::core::Result<void> VehicleStatusProvider::NotifyEvent(
                const std::vector<std::uint8_t> &payload,
                bool force) const
            {
                // Publish one event sample through currently selected event backend.
                switch (mImpl->Profile.EventBinding)
                {
                case EventBackend::kSomeIp:
#if defined(ARA_COM_USE_VSOMEIP) && (ARA_COM_USE_VSOMEIP == 1)
                    if (!mImpl->SomeIpSkeleton)
                    {
                        return MakeComError<void>(
                            ara::com::ComErrc::kNetworkBindingFailure);
                    }

                    return mImpl->SomeIpSkeleton->NotifyEvent(
                        cEventId,
                        payload,
                        force);
#else
                    (void)payload;
                    (void)force;
                    return MakeComError<void>(
                        ara::com::ComErrc::kNetworkBindingFailure);
#endif
                case EventBackend::kDds:
#if defined(ARA_COM_USE_CYCLONEDDS) && (ARA_COM_USE_CYCLONEDDS == 1)
                    if (!mImpl->DdsState)
                    {
                        return MakeComError<void>(
                            ara::com::ComErrc::kNetworkBindingFailure);
                    }

                    SubscriptionStateHandler subscriptionHandler;
                    {
                        std::lock_guard<std::mutex> lock(mImpl->DdsState->Mutex);
                        if (!mImpl->DdsState->ServiceOffered ||
                            !mImpl->DdsState->EventOffered)
                        {
                            return MakeComError<void>(
                                ara::com::ComErrc::kServiceNotOffered);
                        }

                        subscriptionHandler = mImpl->DdsState->SubscriptionHandler;
                    }

                    VehicleStatusFrame frame;
                    if (!DeserializeFrame(payload, frame))
                    {
                        return MakeComError<void>(
                            ara::com::ComErrc::kFieldValueIsNotValid);
                    }

                    auto writeResult{
                        mImpl->DdsState->Publisher.Write(
                            ToDdsFrame(frame))};
                    if (!writeResult.HasValue())
                    {
                        return writeResult;
                    }

                    if (subscriptionHandler)
                    {
                        auto matchedCountResult{
                            mImpl->DdsState->Publisher.GetMatchedSubscriptionCount()};
                        if (matchedCountResult.HasValue())
                        {
                            const std::int32_t matchedSubscribers{
                                matchedCountResult.Value()};

                            bool stateChanged{false};
                            bool isSubscribed{false};
                            {
                                std::lock_guard<std::mutex> lock(mImpl->DdsState->Mutex);
                                const std::int32_t previousMatchedSubscribers{
                                    mImpl->DdsState->LastMatchedSubscribers};
                                mImpl->DdsState->LastMatchedSubscribers = matchedSubscribers;

                                stateChanged =
                                    (previousMatchedSubscribers <= 0 && matchedSubscribers > 0) ||
                                    (previousMatchedSubscribers > 0 && matchedSubscribers <= 0);
                                isSubscribed = matchedSubscribers > 0;
                            }

                            if (stateChanged)
                            {
                                subscriptionHandler(0U, isSubscribed);
                            }
                        }
                    }

                    return ara::core::Result<void>::FromValue();
#else
                    (void)payload;
                    (void)force;
                    return MakeComError<void>(
                        ara::com::ComErrc::kNetworkBindingFailure);
#endif
                }

                return MakeComError<void>(
                    ara::com::ComErrc::kNetworkBindingFailure);
            }

            ara::core::Result<void> VehicleStatusProvider::SetSubscriptionStateHandler(
                SubscriptionStateHandler handler)
            {
                if (!handler)
                {
                    return MakeComError<void>(
                        ara::com::ComErrc::kSetHandlerNotSet);
                }

                switch (mImpl->Profile.EventBinding)
                {
                case EventBackend::kSomeIp:
#if defined(ARA_COM_USE_VSOMEIP) && (ARA_COM_USE_VSOMEIP == 1)
                    if (!mImpl->SomeIpSkeleton)
                    {
                        return MakeComError<void>(
                            ara::com::ComErrc::kNetworkBindingFailure);
                    }

                    return mImpl->SomeIpSkeleton->SetEventSubscriptionStateHandler(
                        cEventGroupId,
                        std::move(handler));
#else
                    return MakeComError<void>(
                        ara::com::ComErrc::kNetworkBindingFailure);
#endif
                case EventBackend::kDds:
#if defined(ARA_COM_USE_CYCLONEDDS) && (ARA_COM_USE_CYCLONEDDS == 1)
                    if (!mImpl->DdsState)
                    {
                        return MakeComError<void>(
                            ara::com::ComErrc::kNetworkBindingFailure);
                    }

                    std::lock_guard<std::mutex> lock(mImpl->DdsState->Mutex);
                    if (!mImpl->DdsState->ServiceOffered)
                    {
                        return MakeComError<void>(
                            ara::com::ComErrc::kServiceNotOffered);
                    }

                    mImpl->DdsState->SubscriptionHandler = std::move(handler);
                    return ara::core::Result<void>::FromValue();
#else
                    return MakeComError<void>(
                        ara::com::ComErrc::kNetworkBindingFailure);
#endif
                }

                return MakeComError<void>(
                    ara::com::ComErrc::kNetworkBindingFailure);
            }

            void VehicleStatusProvider::UnsetSubscriptionStateHandler()
            {
                switch (mImpl->Profile.EventBinding)
                {
                case EventBackend::kSomeIp:
#if defined(ARA_COM_USE_VSOMEIP) && (ARA_COM_USE_VSOMEIP == 1)
                    if (mImpl->SomeIpSkeleton)
                    {
                        mImpl->SomeIpSkeleton->UnsetEventSubscriptionStateHandler(
                            cEventGroupId);
                    }
#endif
                    break;
                case EventBackend::kDds:
#if defined(ARA_COM_USE_CYCLONEDDS) && (ARA_COM_USE_CYCLONEDDS == 1)
                    if (mImpl->DdsState)
                    {
                        std::lock_guard<std::mutex> lock(mImpl->DdsState->Mutex);
                        mImpl->DdsState->SubscriptionHandler = SubscriptionStateHandler{};
                    }
#endif
                    break;
                }
            }

            ara::core::Result<ara::com::zerocopy::ZeroCopyPublisher>
            VehicleStatusProvider::CreateZeroCopyPublisher(
                std::string runtimeName,
                std::uint64_t historyCapacity) const
            {
                if (mImpl->Profile.ZeroCopyBinding != ZeroCopyBackend::kZeroCopy)
                {
                    return MakeComError<ara::com::zerocopy::ZeroCopyPublisher>(
                        ara::com::ComErrc::kCommunicationStackError);
                }

                switch (mImpl->Profile.EventBinding)
                {
                case EventBackend::kSomeIp:
#if defined(ARA_COM_USE_VSOMEIP) && (ARA_COM_USE_VSOMEIP == 1)
                    if (!mImpl->SomeIpSkeleton)
                    {
                        return MakeComError<ara::com::zerocopy::ZeroCopyPublisher>(
                            ara::com::ComErrc::kNetworkBindingFailure);
                    }

                    return mImpl->SomeIpSkeleton->CreateZeroCopyPublisher(
                        cEventId,
                        std::move(runtimeName),
                        historyCapacity);
#else
                    (void)runtimeName;
                    (void)historyCapacity;
                    return MakeComError<ara::com::zerocopy::ZeroCopyPublisher>(
                        ara::com::ComErrc::kNetworkBindingFailure);
#endif
                case EventBackend::kDds:
                    (void)runtimeName;
                    (void)historyCapacity;
                    return MakeComError<ara::com::zerocopy::ZeroCopyPublisher>(
                        ara::com::ComErrc::kCommunicationStackError);
                }

                return MakeComError<ara::com::zerocopy::ZeroCopyPublisher>(
                    ara::com::ComErrc::kCommunicationStackError);
            }

            class VehicleStatusConsumer::Impl
            {
            public:
                explicit Impl(
                    VehicleStatusServiceHandle handle,
                    BackendProfile profile) : Profile{NormalizeProfile(std::move(profile))},
                                              Handle{handle}
                {
                    // Handle can force backend selection when discovered externally.
                    if (Handle.EventBinding != Profile.EventBinding)
                    {
                        Profile.EventBinding = Handle.EventBinding;
                    }
                    else
                    {
                        Handle.EventBinding = Profile.EventBinding;
                    }

                    switch (Profile.EventBinding)
                    {
                    case EventBackend::kSomeIp:
#if defined(ARA_COM_USE_VSOMEIP) && (ARA_COM_USE_VSOMEIP == 1)
                        SomeIpProxy.reset(
                            new VehicleStatusProxyImpl{
                                ToAraComHandle(Handle)});
#endif
                        break;
                    case EventBackend::kDds:
#if defined(ARA_COM_USE_CYCLONEDDS) && (ARA_COM_USE_CYCLONEDDS == 1)
                        DdsState.reset(new DdsConsumerState{
                            Profile.DdsTopicName,
                            Profile.DdsDomainId});
#endif
                        break;
                    }
                }

                BackendProfile Profile;
                VehicleStatusServiceHandle Handle;

#if defined(ARA_COM_USE_VSOMEIP) && (ARA_COM_USE_VSOMEIP == 1)
                std::unique_ptr<VehicleStatusProxyImpl> SomeIpProxy;
#endif
#if defined(ARA_COM_USE_CYCLONEDDS) && (ARA_COM_USE_CYCLONEDDS == 1)
                std::unique_ptr<DdsConsumerState> DdsState;
#endif
            };

            ara::core::Result<void> VehicleStatusConsumer::StartFindService(
                FindServiceHandler handler,
                BackendProfile profile)
            {
                // Start backend-specific discovery and normalize callback payloads.
                if (!handler)
                {
                    return MakeComError<void>(
                        ara::com::ComErrc::kSetHandlerNotSet);
                }

                profile = NormalizeProfile(std::move(profile));

                auto &context{GetPortableFindServiceContext()};
                {
                    std::lock_guard<std::mutex> lock(context.Mutex);
                    if (context.Active)
                    {
                        return MakeComError<void>(
                            ara::com::ComErrc::kFieldValueIsNotValid);
                    }

                    context.Active = true;
                    context.Backend = profile.EventBinding;
                }

                switch (profile.EventBinding)
                {
                case EventBackend::kSomeIp:
#if defined(ARA_COM_USE_VSOMEIP) && (ARA_COM_USE_VSOMEIP == 1)
                {
                    auto startResult{
                        VehicleStatusProxyImpl::StartFindService(
                            [handler = std::move(handler)](
                                std::vector<ara::com::ServiceHandleType> handles)
                            {
                                if (!handler)
                                {
                                    return;
                                }

                                std::vector<VehicleStatusServiceHandle> portableHandles;
                                portableHandles.reserve(handles.size());
                                for (const auto &handle : handles)
                                {
                                    portableHandles.push_back(
                                        VehicleStatusServiceHandle{
                                            handle.GetServiceId(),
                                            handle.GetInstanceId(),
                                            EventBackend::kSomeIp});
                                }

                                handler(std::move(portableHandles));
                            },
                            cServiceId,
                            cInstanceId)};

                    if (!startResult.HasValue())
                    {
                        std::lock_guard<std::mutex> lock(context.Mutex);
                        context.Active = false;
                    }

                    return startResult;
                }
#else
                    {
                        std::lock_guard<std::mutex> lock(context.Mutex);
                        context.Active = false;
                    }
                    return MakeComError<void>(
                        ara::com::ComErrc::kNetworkBindingFailure);
#endif
                case EventBackend::kDds:
#if defined(ARA_COM_USE_CYCLONEDDS) && (ARA_COM_USE_CYCLONEDDS == 1)
                    handler(
                        std::vector<VehicleStatusServiceHandle>{
                            VehicleStatusServiceHandle{
                                cServiceId,
                                cInstanceId,
                                EventBackend::kDds}});
                    return ara::core::Result<void>::FromValue();
#else
                    {
                        std::lock_guard<std::mutex> lock(context.Mutex);
                        context.Active = false;
                    }
                    return MakeComError<void>(
                        ara::com::ComErrc::kNetworkBindingFailure);
#endif
                }

                {
                    std::lock_guard<std::mutex> lock(context.Mutex);
                    context.Active = false;
                }
                return MakeComError<void>(
                    ara::com::ComErrc::kNetworkBindingFailure);
            }

            void VehicleStatusConsumer::StopFindService()
            {
                EventBackend backend{EventBackend::kSomeIp};
                bool active{false};

                auto &context{GetPortableFindServiceContext()};
                {
                    std::lock_guard<std::mutex> lock(context.Mutex);
                    if (!context.Active)
                    {
                        return;
                    }

                    backend = context.Backend;
                    context.Active = false;
                    active = true;
                }

                if (!active)
                {
                    return;
                }

                if (backend == EventBackend::kSomeIp)
                {
#if defined(ARA_COM_USE_VSOMEIP) && (ARA_COM_USE_VSOMEIP == 1)
                    VehicleStatusProxyImpl::StopFindService();
#endif
                }
            }

            VehicleStatusConsumer::VehicleStatusConsumer(
                VehicleStatusServiceHandle handle,
                BackendProfile profile) : mImpl{new Impl{
                                                     handle,
                                                     std::move(profile)}}
            {
            }

            VehicleStatusConsumer::~VehicleStatusConsumer() noexcept = default;
            VehicleStatusConsumer::VehicleStatusConsumer(VehicleStatusConsumer &&other) noexcept = default;
            VehicleStatusConsumer &VehicleStatusConsumer::operator=(VehicleStatusConsumer &&other) noexcept = default;

            EventBackend VehicleStatusConsumer::GetEventBackend() const noexcept
            {
                return mImpl->Profile.EventBinding;
            }

            ZeroCopyBackend VehicleStatusConsumer::GetZeroCopyBackend() const noexcept
            {
                return mImpl->Profile.ZeroCopyBinding;
            }

            ara::core::Result<void> VehicleStatusConsumer::Subscribe(
                EventReceiveHandler handler,
                std::uint8_t majorVersion)
            {
                // Subscribe and immediately attach receive callback.
                if (!handler)
                {
                    return MakeComError<void>(
                        ara::com::ComErrc::kSetHandlerNotSet);
                }

                switch (mImpl->Profile.EventBinding)
                {
                case EventBackend::kSomeIp:
#if defined(ARA_COM_USE_VSOMEIP) && (ARA_COM_USE_VSOMEIP == 1)
                    if (!mImpl->SomeIpProxy)
                    {
                        return MakeComError<void>(
                            ara::com::ComErrc::kNetworkBindingFailure);
                    }

                    return mImpl->SomeIpProxy->SubscribeEvent(
                        cEventId,
                        cEventGroupId,
                        std::move(handler),
                        majorVersion);
#else
                    (void)majorVersion;
                    return MakeComError<void>(
                        ara::com::ComErrc::kNetworkBindingFailure);
#endif
                case EventBackend::kDds:
#if defined(ARA_COM_USE_CYCLONEDDS) && (ARA_COM_USE_CYCLONEDDS == 1)
                    (void)majorVersion;
                    if (!mImpl->DdsState ||
                        !mImpl->DdsState->Subscriber.IsBindingActive())
                    {
                        return MakeComError<void>(
                            ara::com::ComErrc::kNetworkBindingFailure);
                    }

                    {
                        std::lock_guard<std::mutex> lock(mImpl->DdsState->Mutex);
                        if (mImpl->DdsState->State != SubscriptionState::kNotSubscribed)
                        {
                            return MakeComError<void>(
                                ara::com::ComErrc::kFieldValueIsNotValid);
                        }

                        mImpl->DdsState->ReceiveHandler = std::move(handler);
                        mImpl->DdsState->State = SubscriptionState::kSubscribed;
                        mImpl->DdsState->SampleQueue.clear();
                        mImpl->DdsState->MaxSampleQueueExceeded = false;
                    }

                    return ara::core::Result<void>::FromValue();
#else
                    (void)majorVersion;
                    return MakeComError<void>(
                        ara::com::ComErrc::kNetworkBindingFailure);
#endif
                }

                return MakeComError<void>(
                    ara::com::ComErrc::kNetworkBindingFailure);
            }

            ara::core::Result<void> VehicleStatusConsumer::Subscribe(
                std::uint8_t majorVersion)
            {
                switch (mImpl->Profile.EventBinding)
                {
                case EventBackend::kSomeIp:
#if defined(ARA_COM_USE_VSOMEIP) && (ARA_COM_USE_VSOMEIP == 1)
                    if (!mImpl->SomeIpProxy)
                    {
                        return MakeComError<void>(
                            ara::com::ComErrc::kNetworkBindingFailure);
                    }

                    return mImpl->SomeIpProxy->SubscribeEvent(
                        cEventId,
                        cEventGroupId,
                        majorVersion);
#else
                    (void)majorVersion;
                    return MakeComError<void>(
                        ara::com::ComErrc::kNetworkBindingFailure);
#endif
                case EventBackend::kDds:
#if defined(ARA_COM_USE_CYCLONEDDS) && (ARA_COM_USE_CYCLONEDDS == 1)
                    (void)majorVersion;
                    if (!mImpl->DdsState ||
                        !mImpl->DdsState->Subscriber.IsBindingActive())
                    {
                        return MakeComError<void>(
                            ara::com::ComErrc::kNetworkBindingFailure);
                    }

                    {
                        std::lock_guard<std::mutex> lock(mImpl->DdsState->Mutex);
                        if (mImpl->DdsState->State != SubscriptionState::kNotSubscribed)
                        {
                            return MakeComError<void>(
                                ara::com::ComErrc::kFieldValueIsNotValid);
                        }

                        mImpl->DdsState->State = SubscriptionState::kSubscribed;
                        mImpl->DdsState->SampleQueue.clear();
                        mImpl->DdsState->MaxSampleQueueExceeded = false;
                    }

                    return ara::core::Result<void>::FromValue();
#else
                    (void)majorVersion;
                    return MakeComError<void>(
                        ara::com::ComErrc::kNetworkBindingFailure);
#endif
                }

                return MakeComError<void>(
                    ara::com::ComErrc::kNetworkBindingFailure);
            }

            void VehicleStatusConsumer::Unsubscribe()
            {
                switch (mImpl->Profile.EventBinding)
                {
                case EventBackend::kSomeIp:
#if defined(ARA_COM_USE_VSOMEIP) && (ARA_COM_USE_VSOMEIP == 1)
                    if (mImpl->SomeIpProxy)
                    {
                        mImpl->SomeIpProxy->UnsubscribeEvent(
                            cEventId,
                            cEventGroupId);
                    }
#endif
                    break;
                case EventBackend::kDds:
#if defined(ARA_COM_USE_CYCLONEDDS) && (ARA_COM_USE_CYCLONEDDS == 1)
                    if (mImpl->DdsState)
                    {
                        std::lock_guard<std::mutex> lock(mImpl->DdsState->Mutex);
                        mImpl->DdsState->State = SubscriptionState::kNotSubscribed;
                        mImpl->DdsState->ReceiveHandler = EventReceiveHandler{};
                        mImpl->DdsState->SampleQueue.clear();
                        mImpl->DdsState->MaxSampleQueueExceeded = false;
                        mImpl->DdsState->LastMatchedPublishers = 0;
                    }
#endif
                    break;
                }
            }

            ara::core::Result<void> VehicleStatusConsumer::SetReceiveHandler(
                EventReceiveHandler handler)
            {
                if (!handler)
                {
                    return MakeComError<void>(
                        ara::com::ComErrc::kSetHandlerNotSet);
                }

                switch (mImpl->Profile.EventBinding)
                {
                case EventBackend::kSomeIp:
#if defined(ARA_COM_USE_VSOMEIP) && (ARA_COM_USE_VSOMEIP == 1)
                    if (!mImpl->SomeIpProxy)
                    {
                        return MakeComError<void>(
                            ara::com::ComErrc::kNetworkBindingFailure);
                    }

                    return mImpl->SomeIpProxy->SetReceiveHandler(
                        cEventId,
                        std::move(handler));
#else
                    return MakeComError<void>(
                        ara::com::ComErrc::kNetworkBindingFailure);
#endif
                case EventBackend::kDds:
#if defined(ARA_COM_USE_CYCLONEDDS) && (ARA_COM_USE_CYCLONEDDS == 1)
                    if (!mImpl->DdsState)
                    {
                        return MakeComError<void>(
                            ara::com::ComErrc::kNetworkBindingFailure);
                    }

                    {
                        std::lock_guard<std::mutex> lock(mImpl->DdsState->Mutex);
                        if (mImpl->DdsState->State != SubscriptionState::kSubscribed)
                        {
                            return MakeComError<void>(
                                ara::com::ComErrc::kServiceNotAvailable);
                        }

                        mImpl->DdsState->ReceiveHandler = std::move(handler);
                    }

                    return ara::core::Result<void>::FromValue();
#else
                    return MakeComError<void>(
                        ara::com::ComErrc::kNetworkBindingFailure);
#endif
                }

                return MakeComError<void>(
                    ara::com::ComErrc::kNetworkBindingFailure);
            }

            void VehicleStatusConsumer::UnsetReceiveHandler()
            {
                switch (mImpl->Profile.EventBinding)
                {
                case EventBackend::kSomeIp:
#if defined(ARA_COM_USE_VSOMEIP) && (ARA_COM_USE_VSOMEIP == 1)
                    if (mImpl->SomeIpProxy)
                    {
                        mImpl->SomeIpProxy->UnsetReceiveHandler(cEventId);
                    }
#endif
                    break;
                case EventBackend::kDds:
#if defined(ARA_COM_USE_CYCLONEDDS) && (ARA_COM_USE_CYCLONEDDS == 1)
                    if (mImpl->DdsState)
                    {
                        std::lock_guard<std::mutex> lock(mImpl->DdsState->Mutex);
                        mImpl->DdsState->ReceiveHandler = EventReceiveHandler{};
                    }
#endif
                    break;
                }
            }

            ara::core::Result<std::size_t> VehicleStatusConsumer::GetNewSamples(
                std::size_t maxSamples,
                EventReceiveHandler handler)
            {
                // Pull mode API used when app does not rely on SetReceiveHandler callbacks.
                if (!handler)
                {
                    return MakeComError<std::size_t>(
                        ara::com::ComErrc::kSetHandlerNotSet);
                }
                if (maxSamples == 0U)
                {
                    return MakeComError<std::size_t>(
                        ara::com::ComErrc::kFieldValueIsNotValid);
                }

                switch (mImpl->Profile.EventBinding)
                {
                case EventBackend::kSomeIp:
#if defined(ARA_COM_USE_VSOMEIP) && (ARA_COM_USE_VSOMEIP == 1)
                    if (!mImpl->SomeIpProxy)
                    {
                        return MakeComError<std::size_t>(
                            ara::com::ComErrc::kNetworkBindingFailure);
                    }

                    return mImpl->SomeIpProxy->GetNewSamples(
                        cEventId,
                        maxSamples,
                        std::move(handler));
#else
                    return MakeComError<std::size_t>(
                        ara::com::ComErrc::kNetworkBindingFailure);
#endif
                case EventBackend::kDds:
#if defined(ARA_COM_USE_CYCLONEDDS) && (ARA_COM_USE_CYCLONEDDS == 1)
                    if (!mImpl->DdsState)
                    {
                        return MakeComError<std::size_t>(
                            ara::com::ComErrc::kNetworkBindingFailure);
                    }

                    std::vector<std::vector<std::uint8_t>> samples;
                    {
                        std::lock_guard<std::mutex> lock(mImpl->DdsState->Mutex);
                        if (mImpl->DdsState->State != SubscriptionState::kSubscribed)
                        {
                            return MakeComError<std::size_t>(
                                ara::com::ComErrc::kServiceNotAvailable);
                        }

                        const auto sampleCount{
                            std::min(maxSamples, mImpl->DdsState->SampleQueue.size())};
                        samples.reserve(sampleCount);
                        for (std::size_t sampleIndex{0U}; sampleIndex < sampleCount; ++sampleIndex)
                        {
                            samples.push_back(
                                std::move(mImpl->DdsState->SampleQueue.front()));
                            mImpl->DdsState->SampleQueue.pop_front();
                        }
                    }

                    for (const auto &sample : samples)
                    {
                        handler(sample);
                    }

                    return ara::core::Result<std::size_t>::FromValue(samples.size());
#else
                    return MakeComError<std::size_t>(
                        ara::com::ComErrc::kNetworkBindingFailure);
#endif
                }

                return MakeComError<std::size_t>(
                    ara::com::ComErrc::kNetworkBindingFailure);
            }

            ara::core::Result<void> VehicleStatusConsumer::SetSampleQueueLimit(
                std::size_t maxSamples)
            {
                if (maxSamples == 0U)
                {
                    return MakeComError<void>(
                        ara::com::ComErrc::kFieldValueIsNotValid);
                }

                switch (mImpl->Profile.EventBinding)
                {
                case EventBackend::kSomeIp:
#if defined(ARA_COM_USE_VSOMEIP) && (ARA_COM_USE_VSOMEIP == 1)
                    if (!mImpl->SomeIpProxy)
                    {
                        return MakeComError<void>(
                            ara::com::ComErrc::kNetworkBindingFailure);
                    }

                    return mImpl->SomeIpProxy->SetSampleQueueLimit(
                        cEventId,
                        maxSamples);
#else
                    return MakeComError<void>(
                        ara::com::ComErrc::kNetworkBindingFailure);
#endif
                case EventBackend::kDds:
#if defined(ARA_COM_USE_CYCLONEDDS) && (ARA_COM_USE_CYCLONEDDS == 1)
                    if (!mImpl->DdsState)
                    {
                        return MakeComError<void>(
                            ara::com::ComErrc::kNetworkBindingFailure);
                    }

                    std::lock_guard<std::mutex> lock(mImpl->DdsState->Mutex);
                    mImpl->DdsState->MaxSampleQueue = maxSamples;
                    while (mImpl->DdsState->SampleQueue.size() >
                           mImpl->DdsState->MaxSampleQueue)
                    {
                        mImpl->DdsState->SampleQueue.pop_front();
                        mImpl->DdsState->MaxSampleQueueExceeded = true;
                    }

                    return ara::core::Result<void>::FromValue();
#else
                    return MakeComError<void>(
                        ara::com::ComErrc::kNetworkBindingFailure);
#endif
                }

                return MakeComError<void>(
                    ara::com::ComErrc::kNetworkBindingFailure);
            }

            VehicleStatusConsumer::SubscriptionState
            VehicleStatusConsumer::GetSubscriptionState() const noexcept
            {
                switch (mImpl->Profile.EventBinding)
                {
                case EventBackend::kSomeIp:
#if defined(ARA_COM_USE_VSOMEIP) && (ARA_COM_USE_VSOMEIP == 1)
                    if (mImpl->SomeIpProxy)
                    {
                        const auto state{
                            mImpl->SomeIpProxy->GetSubscriptionState(cEventId)};
                        switch (state)
                        {
                        case VehicleStatusProxyImpl::SubscriptionState::kSubscriptionPending:
                            return SubscriptionState::kSubscriptionPending;
                        case VehicleStatusProxyImpl::SubscriptionState::kSubscribed:
                            return SubscriptionState::kSubscribed;
                        case VehicleStatusProxyImpl::SubscriptionState::kNotSubscribed:
                        default:
                            return SubscriptionState::kNotSubscribed;
                        }
                    }
#endif
                    return SubscriptionState::kNotSubscribed;
                case EventBackend::kDds:
#if defined(ARA_COM_USE_CYCLONEDDS) && (ARA_COM_USE_CYCLONEDDS == 1)
                    if (mImpl->DdsState)
                    {
                        std::lock_guard<std::mutex> lock(mImpl->DdsState->Mutex);
                        return mImpl->DdsState->State;
                    }
#endif
                    return SubscriptionState::kNotSubscribed;
                }

                return SubscriptionState::kNotSubscribed;
            }

            bool VehicleStatusConsumer::HasSampleQueueOverflowViolation() const noexcept
            {
                switch (mImpl->Profile.EventBinding)
                {
                case EventBackend::kSomeIp:
#if defined(ARA_COM_USE_VSOMEIP) && (ARA_COM_USE_VSOMEIP == 1)
                    if (mImpl->SomeIpProxy)
                    {
                        return mImpl->SomeIpProxy->HasSampleQueueOverflowViolation(
                            cEventId);
                    }
#endif
                    return false;
                case EventBackend::kDds:
#if defined(ARA_COM_USE_CYCLONEDDS) && (ARA_COM_USE_CYCLONEDDS == 1)
                    if (mImpl->DdsState)
                    {
                        std::lock_guard<std::mutex> lock(mImpl->DdsState->Mutex);
                        return mImpl->DdsState->MaxSampleQueueExceeded;
                    }
#endif
                    return false;
                }

                return false;
            }

            void VehicleStatusConsumer::ClearSampleQueueOverflowViolation() noexcept
            {
                switch (mImpl->Profile.EventBinding)
                {
                case EventBackend::kSomeIp:
#if defined(ARA_COM_USE_VSOMEIP) && (ARA_COM_USE_VSOMEIP == 1)
                    if (mImpl->SomeIpProxy)
                    {
                        mImpl->SomeIpProxy->ClearSampleQueueOverflowViolation(
                            cEventId);
                    }
#endif
                    break;
                case EventBackend::kDds:
#if defined(ARA_COM_USE_CYCLONEDDS) && (ARA_COM_USE_CYCLONEDDS == 1)
                    if (mImpl->DdsState)
                    {
                        std::lock_guard<std::mutex> lock(mImpl->DdsState->Mutex);
                        mImpl->DdsState->MaxSampleQueueExceeded = false;
                    }
#endif
                    break;
                }
            }

            ara::core::Result<void> VehicleStatusConsumer::SetSubscriptionStatusHandler(
                SubscriptionStatusHandler handler)
            {
                if (!handler)
                {
                    return MakeComError<void>(
                        ara::com::ComErrc::kSetHandlerNotSet);
                }

                switch (mImpl->Profile.EventBinding)
                {
                case EventBackend::kSomeIp:
#if defined(ARA_COM_USE_VSOMEIP) && (ARA_COM_USE_VSOMEIP == 1)
                    if (!mImpl->SomeIpProxy)
                    {
                        return MakeComError<void>(
                            ara::com::ComErrc::kNetworkBindingFailure);
                    }

                    return mImpl->SomeIpProxy->SetSubscriptionStatusHandler(
                        cEventId,
                        cEventGroupId,
                        std::move(handler));
#else
                    return MakeComError<void>(
                        ara::com::ComErrc::kNetworkBindingFailure);
#endif
                case EventBackend::kDds:
#if defined(ARA_COM_USE_CYCLONEDDS) && (ARA_COM_USE_CYCLONEDDS == 1)
                    if (!mImpl->DdsState)
                    {
                        return MakeComError<void>(
                            ara::com::ComErrc::kNetworkBindingFailure);
                    }

                    {
                        std::lock_guard<std::mutex> lock(mImpl->DdsState->Mutex);
                        if (mImpl->DdsState->State != SubscriptionState::kSubscribed)
                        {
                            return MakeComError<void>(
                                ara::com::ComErrc::kServiceNotAvailable);
                        }

                        mImpl->DdsState->SubscriptionStatus = std::move(handler);
                    }

                    return ara::core::Result<void>::FromValue();
#else
                    return MakeComError<void>(
                        ara::com::ComErrc::kNetworkBindingFailure);
#endif
                }

                return MakeComError<void>(
                    ara::com::ComErrc::kNetworkBindingFailure);
            }

            void VehicleStatusConsumer::UnsetSubscriptionStatusHandler()
            {
                switch (mImpl->Profile.EventBinding)
                {
                case EventBackend::kSomeIp:
#if defined(ARA_COM_USE_VSOMEIP) && (ARA_COM_USE_VSOMEIP == 1)
                    if (mImpl->SomeIpProxy)
                    {
                        mImpl->SomeIpProxy->UnsetSubscriptionStatusHandler(
                            cEventId,
                            cEventGroupId);
                    }
#endif
                    break;
                case EventBackend::kDds:
#if defined(ARA_COM_USE_CYCLONEDDS) && (ARA_COM_USE_CYCLONEDDS == 1)
                    if (mImpl->DdsState)
                    {
                        std::lock_guard<std::mutex> lock(mImpl->DdsState->Mutex);
                        mImpl->DdsState->SubscriptionStatus = SubscriptionStatusHandler{};
                    }
#endif
                    break;
                }
            }

            ara::core::Result<ara::com::zerocopy::ZeroCopySubscriber>
            VehicleStatusConsumer::CreateZeroCopySubscriber(
                std::string runtimeName,
                std::uint64_t queueCapacity,
                std::uint64_t historyRequest) const
            {
                if (mImpl->Profile.ZeroCopyBinding != ZeroCopyBackend::kZeroCopy)
                {
                    return MakeComError<ara::com::zerocopy::ZeroCopySubscriber>(
                        ara::com::ComErrc::kCommunicationStackError);
                }

                switch (mImpl->Profile.EventBinding)
                {
                case EventBackend::kSomeIp:
#if defined(ARA_COM_USE_VSOMEIP) && (ARA_COM_USE_VSOMEIP == 1)
                    if (!mImpl->SomeIpProxy)
                    {
                        return MakeComError<ara::com::zerocopy::ZeroCopySubscriber>(
                            ara::com::ComErrc::kNetworkBindingFailure);
                    }

                    return mImpl->SomeIpProxy->CreateZeroCopySubscriber(
                        cEventId,
                        std::move(runtimeName),
                        queueCapacity,
                        historyRequest);
#else
                    (void)runtimeName;
                    (void)queueCapacity;
                    (void)historyRequest;
                    return MakeComError<ara::com::zerocopy::ZeroCopySubscriber>(
                        ara::com::ComErrc::kNetworkBindingFailure);
#endif
                case EventBackend::kDds:
                    (void)runtimeName;
                    (void)queueCapacity;
                    (void)historyRequest;
                    return MakeComError<ara::com::zerocopy::ZeroCopySubscriber>(
                        ara::com::ComErrc::kCommunicationStackError);
                }

                return MakeComError<ara::com::zerocopy::ZeroCopySubscriber>(
                    ara::com::ComErrc::kCommunicationStackError);
            }
        }
    }
}
