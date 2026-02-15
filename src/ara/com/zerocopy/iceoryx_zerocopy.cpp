#include <cstring>
#include <limits>
#include <mutex>
#include <utility>
#include "./zero_copy.h"
#include "../com_error_domain.h"

#if defined(ARA_COM_USE_ICEORYX) && (ARA_COM_USE_ICEORYX == 1)
#include <iceoryx_posh/capro/service_description.hpp>
#include <iceoryx_posh/iceoryx_posh_types.hpp>
#include <iceoryx_posh/mepoo/chunk_header.hpp>
#include <iceoryx_posh/popo/publisher_options.hpp>
#include <iceoryx_posh/popo/subscriber_options.hpp>
#include <iceoryx_posh/popo/untyped_publisher.hpp>
#include <iceoryx_posh/popo/untyped_subscriber.hpp>
#include <iceoryx_posh/runtime/posh_runtime.hpp>
#endif

namespace ara
{
    namespace com
    {
        namespace zerocopy
        {
            namespace
            {
                bool IsValidChannel(const ChannelDescriptor &channel) noexcept
                {
                    return !channel.Service.empty() &&
                           !channel.Instance.empty() &&
                           !channel.Event.empty();
                }

                core::Result<void> MakeComErrorResult(ComErrc code) noexcept
                {
                    return core::Result<void>::FromError(MakeErrorCode(code));
                }

                core::Result<bool> MakeComBoolErrorResult(ComErrc code) noexcept
                {
                    return core::Result<bool>::FromError(MakeErrorCode(code));
                }

#if defined(ARA_COM_USE_ICEORYX) && (ARA_COM_USE_ICEORYX == 1)
                std::mutex sRuntimeMutex;
                bool sRuntimeInitialized{false};

                void EnsureRuntimeInitialized(const std::string &runtimeName)
                {
                    std::lock_guard<std::mutex> _runtimeLock(sRuntimeMutex);
                    if (!sRuntimeInitialized)
                    {
                        const std::string cRuntimeName{
                            runtimeName.empty() ? "adaptive_autosar_ara_com"
                                                : runtimeName};

                        const iox::RuntimeName_t cIoxRuntimeName{
                            iox::cxx::TruncateToCapacity,
                            cRuntimeName.c_str()};

                        iox::runtime::PoshRuntime::initRuntime(cIoxRuntimeName);
                        sRuntimeInitialized = true;
                    }
                }

                iox::capro::ServiceDescription
                ToIoxServiceDescription(const ChannelDescriptor &channel) noexcept
                {
                    const iox::capro::IdString_t cService{
                        iox::cxx::TruncateToCapacity,
                        channel.Service.c_str()};
                    const iox::capro::IdString_t cInstance{
                        iox::cxx::TruncateToCapacity,
                        channel.Instance.c_str()};
                    const iox::capro::IdString_t cEvent{
                        iox::cxx::TruncateToCapacity,
                        channel.Event.c_str()};

                    return iox::capro::ServiceDescription{
                        cService,
                        cInstance,
                        cEvent};
                }
#endif
            }

            class LoanedSample::Impl
            {
            public:
#if defined(ARA_COM_USE_ICEORYX) && (ARA_COM_USE_ICEORYX == 1)
                std::shared_ptr<iox::popo::UntypedPublisher> Publisher;
                void *Payload;
                std::size_t PayloadSize;

                Impl(
                    std::shared_ptr<iox::popo::UntypedPublisher> publisher,
                    void *payload,
                    std::size_t payloadSize) noexcept : Publisher{std::move(publisher)},
                                                        Payload{payload},
                                                        PayloadSize{payloadSize}
                {
                }
#else
                Impl() noexcept
                {
                }
#endif
            };

            LoanedSample::LoanedSample() noexcept = default;

            LoanedSample::LoanedSample(std::unique_ptr<Impl> impl) noexcept
                : mImpl{std::move(impl)}
            {
            }

            LoanedSample::LoanedSample(LoanedSample &&other) noexcept = default;

            LoanedSample &LoanedSample::operator=(LoanedSample &&other) noexcept = default;

            LoanedSample::~LoanedSample() noexcept
            {
#if defined(ARA_COM_USE_ICEORYX) && (ARA_COM_USE_ICEORYX == 1)
                if (mImpl && mImpl->Publisher && mImpl->Payload)
                {
                    mImpl->Publisher->release(mImpl->Payload);
                    mImpl->Payload = nullptr;
                }
#endif
            }

            bool LoanedSample::IsValid() const noexcept
            {
#if defined(ARA_COM_USE_ICEORYX) && (ARA_COM_USE_ICEORYX == 1)
                return mImpl && mImpl->Publisher && mImpl->Payload;
#else
                return false;
#endif
            }

            std::uint8_t *LoanedSample::Data() noexcept
            {
#if defined(ARA_COM_USE_ICEORYX) && (ARA_COM_USE_ICEORYX == 1)
                return IsValid() ? static_cast<std::uint8_t *>(mImpl->Payload)
                                 : nullptr;
#else
                return nullptr;
#endif
            }

            const std::uint8_t *LoanedSample::Data() const noexcept
            {
#if defined(ARA_COM_USE_ICEORYX) && (ARA_COM_USE_ICEORYX == 1)
                return IsValid() ? static_cast<const std::uint8_t *>(mImpl->Payload)
                                 : nullptr;
#else
                return nullptr;
#endif
            }

            std::size_t LoanedSample::Size() const noexcept
            {
#if defined(ARA_COM_USE_ICEORYX) && (ARA_COM_USE_ICEORYX == 1)
                return mImpl ? mImpl->PayloadSize : 0U;
#else
                return 0U;
#endif
            }

            class ReceivedSample::Impl
            {
            public:
#if defined(ARA_COM_USE_ICEORYX) && (ARA_COM_USE_ICEORYX == 1)
                std::shared_ptr<iox::popo::UntypedSubscriber> Subscriber;
                const void *Payload;
                std::size_t PayloadSize;

                Impl(
                    std::shared_ptr<iox::popo::UntypedSubscriber> subscriber,
                    const void *payload,
                    std::size_t payloadSize) noexcept : Subscriber{std::move(subscriber)},
                                                        Payload{payload},
                                                        PayloadSize{payloadSize}
                {
                }
#else
                Impl() noexcept
                {
                }
#endif
            };

            ReceivedSample::ReceivedSample() noexcept = default;

            ReceivedSample::ReceivedSample(std::unique_ptr<Impl> impl) noexcept
                : mImpl{std::move(impl)}
            {
            }

            ReceivedSample::ReceivedSample(ReceivedSample &&other) noexcept = default;

            ReceivedSample &ReceivedSample::operator=(ReceivedSample &&other) noexcept = default;

            ReceivedSample::~ReceivedSample() noexcept
            {
#if defined(ARA_COM_USE_ICEORYX) && (ARA_COM_USE_ICEORYX == 1)
                if (mImpl && mImpl->Subscriber && mImpl->Payload)
                {
                    mImpl->Subscriber->release(mImpl->Payload);
                    mImpl->Payload = nullptr;
                }
#endif
            }

            bool ReceivedSample::IsValid() const noexcept
            {
#if defined(ARA_COM_USE_ICEORYX) && (ARA_COM_USE_ICEORYX == 1)
                return mImpl && mImpl->Subscriber && mImpl->Payload;
#else
                return false;
#endif
            }

            const std::uint8_t *ReceivedSample::Data() const noexcept
            {
#if defined(ARA_COM_USE_ICEORYX) && (ARA_COM_USE_ICEORYX == 1)
                return IsValid() ? static_cast<const std::uint8_t *>(mImpl->Payload)
                                 : nullptr;
#else
                return nullptr;
#endif
            }

            std::size_t ReceivedSample::Size() const noexcept
            {
#if defined(ARA_COM_USE_ICEORYX) && (ARA_COM_USE_ICEORYX == 1)
                return mImpl ? mImpl->PayloadSize : 0U;
#else
                return 0U;
#endif
            }

            class ZeroCopyPublisher::Impl
            {
            public:
#if defined(ARA_COM_USE_ICEORYX) && (ARA_COM_USE_ICEORYX == 1)
                std::shared_ptr<iox::popo::UntypedPublisher> Publisher;

                explicit Impl(
                    std::shared_ptr<iox::popo::UntypedPublisher> publisher) noexcept : Publisher{
                                                                                             std::move(publisher)}
                {
                }
#else
                Impl() noexcept
                {
                }
#endif
            };

            ZeroCopyPublisher::ZeroCopyPublisher(
                ChannelDescriptor channel,
                std::string runtimeName,
                std::uint64_t historyCapacity) : mImpl{nullptr}
            {
                if (!IsValidChannel(channel))
                {
                    return;
                }

#if defined(ARA_COM_USE_ICEORYX) && (ARA_COM_USE_ICEORYX == 1)
                EnsureRuntimeInitialized(runtimeName);

                iox::popo::PublisherOptions _options;
                _options.historyCapacity = historyCapacity;

                auto _publisher{
                    std::make_shared<iox::popo::UntypedPublisher>(
                        ToIoxServiceDescription(channel),
                        _options)};
                mImpl.reset(new Impl{std::move(_publisher)});
#else
                (void)runtimeName;
                (void)historyCapacity;
#endif
            }

            ZeroCopyPublisher::~ZeroCopyPublisher() noexcept = default;

            ZeroCopyPublisher::ZeroCopyPublisher(ZeroCopyPublisher &&other) noexcept = default;

            ZeroCopyPublisher &ZeroCopyPublisher::operator=(ZeroCopyPublisher &&other) noexcept = default;

            bool ZeroCopyPublisher::IsBindingActive() const noexcept
            {
#if defined(ARA_COM_USE_ICEORYX) && (ARA_COM_USE_ICEORYX == 1)
                return mImpl && mImpl->Publisher;
#else
                return false;
#endif
            }

            core::Result<void> ZeroCopyPublisher::Loan(
                std::size_t payloadSize,
                LoanedSample &sample,
                std::size_t payloadAlignment) noexcept
            {
                sample = LoanedSample{};
                if (payloadSize == 0U ||
                    payloadAlignment == 0U ||
                    payloadSize > std::numeric_limits<uint32_t>::max() ||
                    payloadAlignment > std::numeric_limits<uint32_t>::max())
                {
                    return MakeComErrorResult(ComErrc::kIllegalUseOfAllocate);
                }

#if defined(ARA_COM_USE_ICEORYX) && (ARA_COM_USE_ICEORYX == 1)
                if (!IsBindingActive())
                {
                    return MakeComErrorResult(ComErrc::kNetworkBindingFailure);
                }

                auto _loanedPayload{
                    mImpl->Publisher->loan(
                        static_cast<uint32_t>(payloadSize),
                        static_cast<uint32_t>(payloadAlignment))};

                if (_loanedPayload.has_error())
                {
                    return MakeComErrorResult(ComErrc::kSampleAllocationFailure);
                }

                sample = LoanedSample{
                    std::unique_ptr<LoanedSample::Impl>{
                        new LoanedSample::Impl{
                            mImpl->Publisher,
                            _loanedPayload.value(),
                            payloadSize}}};

                return core::Result<void>::FromValue();
#else
                (void)payloadSize;
                (void)payloadAlignment;
                return MakeComErrorResult(ComErrc::kCommunicationStackError);
#endif
            }

            core::Result<void> ZeroCopyPublisher::Publish(LoanedSample &&sample) noexcept
            {
#if defined(ARA_COM_USE_ICEORYX) && (ARA_COM_USE_ICEORYX == 1)
                if (!sample.IsValid())
                {
                    return MakeComErrorResult(ComErrc::kIllegalUseOfAllocate);
                }

                sample.mImpl->Publisher->publish(sample.mImpl->Payload);
                sample.mImpl->Payload = nullptr;
                sample.mImpl->PayloadSize = 0U;
                sample.mImpl->Publisher.reset();
                return core::Result<void>::FromValue();
#else
                (void)sample;
                return MakeComErrorResult(ComErrc::kCommunicationStackError);
#endif
            }

            core::Result<void> ZeroCopyPublisher::PublishCopy(
                const std::vector<std::uint8_t> &payload) noexcept
            {
                LoanedSample _sample;
                auto _loanResult{Loan(payload.size(), _sample)};
                if (!_loanResult.HasValue())
                {
                    return core::Result<void>::FromError(_loanResult.Error());
                }

                std::memcpy(_sample.Data(), payload.data(), payload.size());
                return Publish(std::move(_sample));
            }

            bool ZeroCopyPublisher::HasSubscribers() const noexcept
            {
#if defined(ARA_COM_USE_ICEORYX) && (ARA_COM_USE_ICEORYX == 1)
                return IsBindingActive() && mImpl->Publisher->hasSubscribers();
#else
                return false;
#endif
            }

            class ZeroCopySubscriber::Impl
            {
            public:
#if defined(ARA_COM_USE_ICEORYX) && (ARA_COM_USE_ICEORYX == 1)
                std::shared_ptr<iox::popo::UntypedSubscriber> Subscriber;

                explicit Impl(
                    std::shared_ptr<iox::popo::UntypedSubscriber> subscriber) noexcept : Subscriber{
                                                                                              std::move(subscriber)}
                {
                }
#else
                Impl() noexcept
                {
                }
#endif
            };

            ZeroCopySubscriber::ZeroCopySubscriber(
                ChannelDescriptor channel,
                std::string runtimeName,
                std::uint64_t queueCapacity,
                std::uint64_t historyRequest) : mImpl{nullptr}
            {
                if (!IsValidChannel(channel))
                {
                    return;
                }

#if defined(ARA_COM_USE_ICEORYX) && (ARA_COM_USE_ICEORYX == 1)
                EnsureRuntimeInitialized(runtimeName);

                iox::popo::SubscriberOptions _options;
                _options.queueCapacity = queueCapacity;
                _options.historyRequest = historyRequest;

                auto _subscriber{
                    std::make_shared<iox::popo::UntypedSubscriber>(
                        ToIoxServiceDescription(channel),
                        _options)};
                mImpl.reset(new Impl{std::move(_subscriber)});
#else
                (void)runtimeName;
                (void)queueCapacity;
                (void)historyRequest;
#endif
            }

            ZeroCopySubscriber::~ZeroCopySubscriber() noexcept = default;

            ZeroCopySubscriber::ZeroCopySubscriber(ZeroCopySubscriber &&other) noexcept = default;

            ZeroCopySubscriber &ZeroCopySubscriber::operator=(ZeroCopySubscriber &&other) noexcept = default;

            bool ZeroCopySubscriber::IsBindingActive() const noexcept
            {
#if defined(ARA_COM_USE_ICEORYX) && (ARA_COM_USE_ICEORYX == 1)
                return mImpl && mImpl->Subscriber;
#else
                return false;
#endif
            }

            core::Result<bool> ZeroCopySubscriber::TryTake(ReceivedSample &sample) noexcept
            {
                sample = ReceivedSample{};

#if defined(ARA_COM_USE_ICEORYX) && (ARA_COM_USE_ICEORYX == 1)
                if (!IsBindingActive())
                {
                    return MakeComBoolErrorResult(ComErrc::kNetworkBindingFailure);
                }

                auto _takeResult{mImpl->Subscriber->take()};
                if (_takeResult.has_error())
                {
                    if (_takeResult.get_error() ==
                        iox::popo::ChunkReceiveResult::NO_CHUNK_AVAILABLE)
                    {
                        return core::Result<bool>::FromValue(false);
                    }
                    else
                    {
                        return MakeComBoolErrorResult(ComErrc::kMaxSamplesExceeded);
                    }
                }

                const void *cPayload{_takeResult.value()};
                const iox::mepoo::ChunkHeader *cChunkHeader{
                    iox::mepoo::ChunkHeader::fromUserPayload(cPayload)};
                if (!cChunkHeader)
                {
                    mImpl->Subscriber->release(cPayload);
                    return MakeComBoolErrorResult(ComErrc::kCommunicationStackError);
                }

                sample = ReceivedSample{
                    std::unique_ptr<ReceivedSample::Impl>{
                        new ReceivedSample::Impl{
                            mImpl->Subscriber,
                            cPayload,
                            cChunkHeader->userPayloadSize()}}};
                return core::Result<bool>::FromValue(true);
#else
                return MakeComBoolErrorResult(ComErrc::kCommunicationStackError);
#endif
            }
        }
    }
}
