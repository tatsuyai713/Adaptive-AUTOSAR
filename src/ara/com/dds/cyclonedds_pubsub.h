/// @file src/ara/com/dds/cyclonedds_pubsub.h
/// @brief Declarations for cyclonedds pubsub.
/// @details This file is part of the Adaptive AUTOSAR educational implementation.

#ifndef ARA_COM_DDS_CYCLONEDDS_PUBSUB_H
#define ARA_COM_DDS_CYCLONEDDS_PUBSUB_H

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include "../com_error_domain.h"
#include "../../core/result.h"

#if defined(ARA_COM_USE_CYCLONEDDS) && (ARA_COM_USE_CYCLONEDDS == 1)
#include <ddscxx/dds/dds.hpp>
#endif

namespace ara
{
    namespace com
    {
        namespace dds
        {
            /// @brief DDS QoS profile selection for publisher and subscriber.
            ///
            /// Profiles correspond to common AUTOSAR AP communication patterns:
            /// - kReliable:       Reliable + Volatile + KeepLast(16).
            ///                    Default for standard event/method communication.
            /// - kBestEffort:     BestEffort + Volatile + KeepLast(1).
            ///                    For high-rate sensor streams where occasional loss is
            ///                    acceptable.
            /// - kTransientLocal: Reliable + TransientLocal + KeepLast(1).
            ///                    For field-like topics where late-joining subscribers
            ///                    receive the last known value on subscription.
            enum class DdsQosProfile : std::uint32_t
            {
                kReliable = 0,      ///< Reliable, Volatile, KeepLast(16)  [default]
                kBestEffort = 1,    ///< BestEffort, Volatile, KeepLast(1)
                kTransientLocal = 2 ///< Reliable, TransientLocal, KeepLast(1)
            };
            /// @brief DDS publisher wrapper backed by Cyclone DDS C++ API.
            template <typename SampleType>
            class CyclonePublisher final
            {
            private:
                std::string mTopicName;

#if defined(ARA_COM_USE_CYCLONEDDS) && (ARA_COM_USE_CYCLONEDDS == 1)
                /// @brief Runtime DDS entities required for publication.
                struct Binding final
                {
                    ::dds::domain::DomainParticipant Participant;
                    ::dds::pub::Publisher Publisher;
                    ::dds::topic::Topic<SampleType> Topic;
                    ::dds::pub::DataWriter<SampleType> Writer;

                    Binding(
                        const std::string &topicName,
                        std::uint32_t domainId,
                        DdsQosProfile profile) : Participant{domainId},
                                                 Publisher{Participant},
                                                 Topic{Participant, topicName},
                                                 Writer{
                                                     Publisher,
                                                     Topic,
                                                     CreateWriterQos(Publisher, profile)}
                    {
                    }

                private:
                    static ::dds::pub::qos::DataWriterQos CreateWriterQos(
                        const ::dds::pub::Publisher &publisher,
                        DdsQosProfile profile)
                    {
                        auto _qos{publisher.default_datawriter_qos()};
                        switch (profile)
                        {
                        case DdsQosProfile::kBestEffort:
                            _qos << ::dds::core::policy::Reliability::BestEffort()
                                 << ::dds::core::policy::Durability::Volatile()
                                 << ::dds::core::policy::History::KeepLast(1);
                            break;
                        case DdsQosProfile::kTransientLocal:
                            _qos << ::dds::core::policy::Reliability::Reliable()
                                 << ::dds::core::policy::Durability::TransientLocal()
                                 << ::dds::core::policy::History::KeepLast(1);
                            break;
                        case DdsQosProfile::kReliable:
                        default:
                            _qos << ::dds::core::policy::Reliability::Reliable()
                                 << ::dds::core::policy::Durability::Volatile()
                                 << ::dds::core::policy::History::KeepLast(16);
                            break;
                        }
                        return _qos;
                    }
                };

                std::unique_ptr<Binding> mBinding;
#endif

                DdsQosProfile mProfile{DdsQosProfile::kReliable};

            public:
                /// @brief Construct with default QoS profile (kReliable).
                explicit CyclonePublisher(
                    std::string topicName,
                    std::uint32_t domainId = 0U) noexcept
                    : CyclonePublisher{std::move(topicName), domainId, DdsQosProfile::kReliable}
                {
                }

                /// @brief Construct with an explicit QoS profile.
                explicit CyclonePublisher(
                    std::string topicName,
                    std::uint32_t domainId,
                    DdsQosProfile profile) noexcept : mTopicName{std::move(topicName)}, mProfile{profile}
                {
#if defined(ARA_COM_USE_CYCLONEDDS) && (ARA_COM_USE_CYCLONEDDS == 1)
                    if (mTopicName.empty())
                    {
                        return;
                    }

                    try
                    {
                        mBinding.reset(new Binding{
                            mTopicName,
                            static_cast<std::uint32_t>(domainId),
                            profile});
                    }
                    catch (const ::dds::core::Exception &)
                    {
                        mBinding.reset();
                    }
#else
                    (void)domainId;
                    (void)profile;
#endif
                }

                bool IsBindingActive() const noexcept
                {
#if defined(ARA_COM_USE_CYCLONEDDS) && (ARA_COM_USE_CYCLONEDDS == 1)
                    return static_cast<bool>(mBinding);
#else
                    return false;
#endif
                }

                /// @brief Return the QoS profile used by this publisher.
                DdsQosProfile GetQosProfile() const noexcept
                {
                    return mProfile;
                }

                core::Result<void> Write(const SampleType &sample) noexcept
                {
#if defined(ARA_COM_USE_CYCLONEDDS) && (ARA_COM_USE_CYCLONEDDS == 1)
                    if (!IsBindingActive())
                    {
                        return core::Result<void>::FromError(
                            MakeErrorCode(ComErrc::kNetworkBindingFailure));
                    }

                    try
                    {
                        mBinding->Writer.write(sample);
                        return core::Result<void>::FromValue();
                    }
                    catch (const ::dds::core::Exception &)
                    {
                        return core::Result<void>::FromError(
                            MakeErrorCode(ComErrc::kCommunicationStackError));
                    }
#else
                    (void)sample;
                    return core::Result<void>::FromError(
                        MakeErrorCode(ComErrc::kCommunicationStackError));
#endif
                }

                core::Result<std::int32_t> GetMatchedSubscriptionCount() const noexcept
                {
#if defined(ARA_COM_USE_CYCLONEDDS) && (ARA_COM_USE_CYCLONEDDS == 1)
                    if (!IsBindingActive())
                    {
                        return core::Result<std::int32_t>::FromError(
                            MakeErrorCode(ComErrc::kNetworkBindingFailure));
                    }

                    try
                    {
                        const auto cStatus{
                            mBinding->Writer.publication_matched_status()};
                        return core::Result<std::int32_t>::FromValue(
                            cStatus.current_count());
                    }
                    catch (const ::dds::core::Exception &)
                    {
                        return core::Result<std::int32_t>::FromError(
                            MakeErrorCode(ComErrc::kCommunicationStackError));
                    }
#else
                    return core::Result<std::int32_t>::FromError(
                        MakeErrorCode(ComErrc::kCommunicationStackError));
#endif
                }
            };

            /// @brief DDS subscriber wrapper backed by Cyclone DDS C++ API.
            template <typename SampleType>
            class CycloneSubscriber final
            {
            private:
                std::string mTopicName;

#if defined(ARA_COM_USE_CYCLONEDDS) && (ARA_COM_USE_CYCLONEDDS == 1)
                /// @brief Runtime DDS entities required for subscription.
                struct Binding final
                {
                    ::dds::domain::DomainParticipant Participant;
                    ::dds::sub::Subscriber Subscriber;
                    ::dds::topic::Topic<SampleType> Topic;
                    ::dds::sub::DataReader<SampleType> Reader;
                    ::dds::sub::cond::ReadCondition ReadCondition;
                    ::dds::core::cond::WaitSet WaitSet;

                    Binding(
                        const std::string &topicName,
                        std::uint32_t domainId,
                        DdsQosProfile profile) : Participant{domainId},
                                                 Subscriber{Participant},
                                                 Topic{Participant, topicName},
                                                 Reader{
                                                     Subscriber,
                                                     Topic,
                                                     CreateReaderQos(Subscriber, profile)},
                                                 ReadCondition{
                                                     Reader,
                                                     ::dds::sub::status::DataState::new_data()}
                    {
                        WaitSet.attach_condition(ReadCondition);
                    }

                private:
                    static ::dds::sub::qos::DataReaderQos CreateReaderQos(
                        const ::dds::sub::Subscriber &subscriber,
                        DdsQosProfile profile)
                    {
                        auto _qos{subscriber.default_datareader_qos()};
                        switch (profile)
                        {
                        case DdsQosProfile::kBestEffort:
                            _qos << ::dds::core::policy::Reliability::BestEffort()
                                 << ::dds::core::policy::Durability::Volatile()
                                 << ::dds::core::policy::History::KeepLast(1);
                            break;
                        case DdsQosProfile::kTransientLocal:
                            _qos << ::dds::core::policy::Reliability::Reliable()
                                 << ::dds::core::policy::Durability::TransientLocal()
                                 << ::dds::core::policy::History::KeepLast(1);
                            break;
                        case DdsQosProfile::kReliable:
                        default:
                            _qos << ::dds::core::policy::Reliability::Reliable()
                                 << ::dds::core::policy::Durability::Volatile()
                                 << ::dds::core::policy::History::KeepLast(64);
                            break;
                        }
                        return _qos;
                    }
                };

                std::unique_ptr<Binding> mBinding;
#endif

                DdsQosProfile mProfile{DdsQosProfile::kReliable};

            public:
                /// @brief Construct with default QoS profile (kReliable).
                explicit CycloneSubscriber(
                    std::string topicName,
                    std::uint32_t domainId = 0U) noexcept
                    : CycloneSubscriber{std::move(topicName), domainId, DdsQosProfile::kReliable}
                {
                }

                /// @brief Construct with an explicit QoS profile.
                explicit CycloneSubscriber(
                    std::string topicName,
                    std::uint32_t domainId,
                    DdsQosProfile profile) noexcept : mTopicName{std::move(topicName)}, mProfile{profile}
                {
#if defined(ARA_COM_USE_CYCLONEDDS) && (ARA_COM_USE_CYCLONEDDS == 1)
                    if (mTopicName.empty())
                    {
                        return;
                    }

                    try
                    {
                        mBinding.reset(new Binding{
                            mTopicName,
                            static_cast<std::uint32_t>(domainId),
                            profile});
                    }
                    catch (const ::dds::core::Exception &)
                    {
                        mBinding.reset();
                    }
#else
                    (void)domainId;
                    (void)profile;
#endif
                }

                bool IsBindingActive() const noexcept
                {
#if defined(ARA_COM_USE_CYCLONEDDS) && (ARA_COM_USE_CYCLONEDDS == 1)
                    return static_cast<bool>(mBinding);
#else
                    return false;
#endif
                }

                /// @brief Return the QoS profile used by this subscriber.
                DdsQosProfile GetQosProfile() const noexcept
                {
                    return mProfile;
                }

                core::Result<std::size_t> Take(
                    std::size_t maxSamples,
                    std::function<void(const SampleType &)> handler) noexcept
                {
                    if (maxSamples == 0U)
                    {
                        return core::Result<std::size_t>::FromError(
                            MakeErrorCode(ComErrc::kFieldValueIsNotValid));
                    }
                    if (!handler)
                    {
                        return core::Result<std::size_t>::FromError(
                            MakeErrorCode(ComErrc::kSetHandlerNotSet));
                    }

#if defined(ARA_COM_USE_CYCLONEDDS) && (ARA_COM_USE_CYCLONEDDS == 1)
                    if (!IsBindingActive())
                    {
                        return core::Result<std::size_t>::FromError(
                            MakeErrorCode(ComErrc::kNetworkBindingFailure));
                    }

                    try
                    {
                        std::size_t _consumed{0U};
                        auto _samples{mBinding->Reader.select().max_samples(static_cast<uint32_t>(maxSamples)).take()};
                        for (const auto &cSample : _samples)
                        {
                            if (!cSample.info().valid())
                            {
                                continue;
                            }

                            handler(cSample.data());
                            ++_consumed;
                        }

                        return core::Result<std::size_t>::FromValue(_consumed);
                    }
                    catch (const ::dds::core::Exception &)
                    {
                        return core::Result<std::size_t>::FromError(
                            MakeErrorCode(ComErrc::kCommunicationStackError));
                    }
#else
                    return core::Result<std::size_t>::FromError(
                        MakeErrorCode(ComErrc::kCommunicationStackError));
#endif
                }

                core::Result<std::int32_t> GetMatchedPublicationCount() const noexcept
                {
#if defined(ARA_COM_USE_CYCLONEDDS) && (ARA_COM_USE_CYCLONEDDS == 1)
                    if (!IsBindingActive())
                    {
                        return core::Result<std::int32_t>::FromError(
                            MakeErrorCode(ComErrc::kNetworkBindingFailure));
                    }

                    try
                    {
                        const auto cStatus{
                            mBinding->Reader.subscription_matched_status()};
                        return core::Result<std::int32_t>::FromValue(
                            cStatus.current_count());
                    }
                    catch (const ::dds::core::Exception &)
                    {
                        return core::Result<std::int32_t>::FromError(
                            MakeErrorCode(ComErrc::kCommunicationStackError));
                    }
#else
                    return core::Result<std::int32_t>::FromError(
                        MakeErrorCode(ComErrc::kCommunicationStackError));
#endif
                }

                /// @brief Block until new data is available or the timeout expires.
                /// @details Uses the DDS WaitSet mechanism — no busy-wait or sleep.
                /// @param timeout Maximum time to wait.
                /// @returns true when data is available, false on timeout or error.
                bool WaitForData(std::chrono::milliseconds timeout) noexcept
                {
#if defined(ARA_COM_USE_CYCLONEDDS) && (ARA_COM_USE_CYCLONEDDS == 1)
                    if (!IsBindingActive())
                    {
                        return false;
                    }

                    try
                    {
                        const auto cDuration{
                            ::dds::core::Duration::from_millisecs(
                                static_cast<int64_t>(timeout.count()))};
                        mBinding->WaitSet.wait(cDuration);
                        return true;
                    }
                    catch (const ::dds::core::TimeoutError &)
                    {
                        return false;
                    }
                    catch (const ::dds::core::Exception &)
                    {
                        return false;
                    }
#else
                    (void)timeout;
                    return false;
#endif
                }
            };
        }
    }
}

#endif
