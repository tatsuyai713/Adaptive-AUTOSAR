/// @file src/ara/com/dds/cyclonedds_pubsub.h
/// @brief Declarations for cyclonedds pubsub.
/// @details This file is part of the Adaptive AUTOSAR educational implementation.

#ifndef ARA_COM_DDS_CYCLONEDDS_PUBSUB_H
#define ARA_COM_DDS_CYCLONEDDS_PUBSUB_H

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
                        std::uint32_t domainId) : Participant{domainId},
                                                  Publisher{Participant},
                                                  Topic{Participant, topicName},
                                                  Writer{
                                                      Publisher,
                                                      Topic,
                                                      CreateWriterQos(Publisher)}
                    {
                    }

                private:
                    static ::dds::pub::qos::DataWriterQos CreateWriterQos(
                        const ::dds::pub::Publisher &publisher)
                    {
                        auto _qos{
                            publisher.default_datawriter_qos() <<
                            ::dds::core::policy::Reliability::Reliable() <<
                            ::dds::core::policy::Durability::Volatile() <<
                            ::dds::core::policy::History::KeepLast(16)};
                        return _qos;
                    }
                };

                std::unique_ptr<Binding> mBinding;
#endif

            public:
                explicit CyclonePublisher(
                    std::string topicName,
                    std::uint32_t domainId = 0U) noexcept : mTopicName{std::move(topicName)}
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
                            static_cast<std::uint32_t>(domainId)});
                    }
                    catch (const ::dds::core::Exception &)
                    {
                        mBinding.reset();
                    }
#else
                    (void)domainId;
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

                    Binding(
                        const std::string &topicName,
                        std::uint32_t domainId) : Participant{domainId},
                                                  Subscriber{Participant},
                                                  Topic{Participant, topicName},
                                                  Reader{
                                                      Subscriber,
                                                      Topic,
                                                      CreateReaderQos(Subscriber)}
                    {
                    }

                private:
                    static ::dds::sub::qos::DataReaderQos CreateReaderQos(
                        const ::dds::sub::Subscriber &subscriber)
                    {
                        auto _qos{
                            subscriber.default_datareader_qos() <<
                            ::dds::core::policy::Reliability::Reliable() <<
                            ::dds::core::policy::Durability::Volatile() <<
                            ::dds::core::policy::History::KeepLast(64)};
                        return _qos;
                    }
                };

                std::unique_ptr<Binding> mBinding;
#endif

            public:
                explicit CycloneSubscriber(
                    std::string topicName,
                    std::uint32_t domainId = 0U) noexcept : mTopicName{std::move(topicName)}
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
                            static_cast<std::uint32_t>(domainId)});
                    }
                    catch (const ::dds::core::Exception &)
                    {
                        mBinding.reset();
                    }
#else
                    (void)domainId;
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
            };
        }
    }
}

#endif
