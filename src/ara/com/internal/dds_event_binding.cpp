/// @file src/ara/com/internal/dds_event_binding.cpp
/// @brief Implementation of CycloneDDS event binding.
/// @details Uses the CycloneDDS C API (dds/dds.h) with a manually defined
///          topic type descriptor for raw byte payloads.  No IDL code
///          generation is required.
///
///          This file is part of the Adaptive AUTOSAR educational implementation.

#include <algorithm>
#include <cstring>
#include <iomanip>
#include <sstream>
#include "./dds_event_binding.h"

#if defined(ARA_COM_USE_CYCLONEDDS) && (ARA_COM_USE_CYCLONEDDS == 1)

namespace ara
{
    namespace com
    {
        namespace internal
        {
            // ── AraComDdsRawEvent topic descriptor ────────────────────────
            //
            // Manually defined without IDL codegen.  Equivalent to:
            //   struct AraComDdsRawEvent {
            //       unsigned long size;           // @key
            //       octet         data[4096];
            //   };
            //
            // Op list (CycloneDDS dds_opcodes.h DDS_OP_* encoding):
            //   [0] DDS_OP_ADR | DDS_OP_TYPE_4BY | DDS_OP_FLAG_KEY
            //       [1] offsetof(AraComDdsRawEvent, size)
            //   [2] DDS_OP_ADR | DDS_OP_TYPE_ARR | DDS_OP_SUBTYPE_1BY
            //       [3] offsetof(AraComDdsRawEvent, data)
            //       [4] ARA_COM_DDS_MAX_PAYLOAD_SIZE
            //   [5] DDS_OP_RTS

            static const std::uint32_t AraComDdsRawEvent_ops[] = {
                static_cast<std::uint32_t>(DDS_OP_ADR) |
                    static_cast<std::uint32_t>(DDS_OP_TYPE_4BY) |
                    static_cast<std::uint32_t>(DDS_OP_FLAG_KEY),
                static_cast<std::uint32_t>(offsetof(AraComDdsRawEvent, size)),
                static_cast<std::uint32_t>(DDS_OP_ADR) |
                    static_cast<std::uint32_t>(DDS_OP_TYPE_ARR) |
                    static_cast<std::uint32_t>(DDS_OP_SUBTYPE_1BY),
                static_cast<std::uint32_t>(offsetof(AraComDdsRawEvent, data)),
                ARA_COM_DDS_MAX_PAYLOAD_SIZE,
                static_cast<std::uint32_t>(DDS_OP_RTS)};

            static const dds_key_descriptor_t AraComDdsRawEvent_keys[] = {
                {"size", 0U, 1U}};

            const dds_topic_descriptor_t AraComDdsRawEvent_desc = {
                sizeof(AraComDdsRawEvent),        // m_size
                alignof(AraComDdsRawEvent),        // m_align
                DDS_TOPIC_FIXED_SIZE |
                    DDS_TOPIC_FIXED_KEY |
                    DDS_TOPIC_FIXED_KEY_XCDR2,     // m_flagset
                1U,                                // m_nkeys
                "ara::com::DdsRawEvent",           // m_typename
                AraComDdsRawEvent_keys,             // m_keys
                6U,                                // m_nops
                AraComDdsRawEvent_ops,              // m_ops
                nullptr                            // m_meta (no XTypes XML)
            };

            // ── Shared helpers ─────────────────────────────────────────────

            namespace
            {
                std::string idToHex(std::uint16_t id)
                {
                    std::ostringstream oss;
                    oss << std::hex << std::setfill('0') << std::setw(4)
                        << static_cast<unsigned>(id);
                    return oss.str();
                }

                /// @brief Release DDS entities in reverse creation order.
                void deleteSafe(dds_entity_t &entity) noexcept
                {
                    if (entity > 0)
                    {
                        dds_delete(entity);
                        entity = 0;
                    }
                }
            } // namespace

            // ── DdsProxyEventBinding ───────────────────────────────────────

            std::string DdsProxyEventBinding::makeTopicName(
                const EventBindingConfig &cfg) noexcept
            {
                return "ara_com_svc" + idToHex(cfg.ServiceId) +
                       "_inst" + idToHex(cfg.InstanceId) +
                       "_evt" + idToHex(cfg.EventId);
            }

            DdsProxyEventBinding::DdsProxyEventBinding(
                EventBindingConfig config) noexcept
                : mConfig{config}
            {
            }

            DdsProxyEventBinding::~DdsProxyEventBinding() noexcept
            {
                if (mState != SubscriptionState::kNotSubscribed)
                {
                    Unsubscribe();
                }
            }

            core::Result<void> DdsProxyEventBinding::Subscribe(
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
                }

                // Create DDS entities
                const std::uint32_t domainId =
                    static_cast<std::uint32_t>(mConfig.ServiceId) & 0x00FFU;

                mParticipant = dds_create_participant(
                    static_cast<dds_domainid_t>(domainId), nullptr, nullptr);
                if (mParticipant < 0)
                {
                    return core::Result<void>::FromError(
                        MakeErrorCode(ComErrc::kNetworkBindingFailure));
                }

                const std::string topicName = makeTopicName(mConfig);
                mTopic = dds_create_topic(
                    mParticipant,
                    &AraComDdsRawEvent_desc,
                    topicName.c_str(),
                    nullptr, nullptr);
                if (mTopic < 0)
                {
                    deleteSafe(mParticipant);
                    return core::Result<void>::FromError(
                        MakeErrorCode(ComErrc::kNetworkBindingFailure));
                }

                mSubscriber = dds_create_subscriber(mParticipant, nullptr, nullptr);
                if (mSubscriber < 0)
                {
                    deleteSafe(mTopic);
                    deleteSafe(mParticipant);
                    return core::Result<void>::FromError(
                        MakeErrorCode(ComErrc::kNetworkBindingFailure));
                }

                // Reliable QoS reader
                dds_qos_t *qos = dds_create_qos();
                dds_qset_reliability(qos, DDS_RELIABILITY_RELIABLE,
                                     DDS_SECS(1));
                dds_qset_history(qos, DDS_HISTORY_KEEP_LAST,
                                 static_cast<int32_t>(mMaxSampleCount));
                mReader = dds_create_reader(mSubscriber, mTopic, qos, nullptr);
                dds_delete_qos(qos);

                if (mReader < 0)
                {
                    deleteSafe(mSubscriber);
                    deleteSafe(mTopic);
                    deleteSafe(mParticipant);
                    return core::Result<void>::FromError(
                        MakeErrorCode(ComErrc::kNetworkBindingFailure));
                }

                // WaitSet + ReadCondition for the polling thread
                mReadCond = dds_create_readcondition(
                    mReader, DDS_ANY_SAMPLE_STATE |
                                 DDS_ANY_VIEW_STATE |
                                 DDS_ANY_INSTANCE_STATE);
                if (mReadCond < 0)
                {
                    deleteSafe(mReader);
                    deleteSafe(mSubscriber);
                    deleteSafe(mTopic);
                    deleteSafe(mParticipant);
                    return core::Result<void>::FromError(
                        MakeErrorCode(ComErrc::kNetworkBindingFailure));
                }

                mWaitSet = dds_create_waitset(mParticipant);
                if (mWaitSet < 0)
                {
                    deleteSafe(mReadCond);
                    deleteSafe(mReader);
                    deleteSafe(mSubscriber);
                    deleteSafe(mTopic);
                    deleteSafe(mParticipant);
                    return core::Result<void>::FromError(
                        MakeErrorCode(ComErrc::kNetworkBindingFailure));
                }

                dds_waitset_attach(mWaitSet, mReadCond,
                                   static_cast<dds_attach_t>(mReader));

                {
                    std::lock_guard<std::mutex> lock(mMutex);
                    mState = SubscriptionState::kSubscribed;
                }

                mRunning.store(true, std::memory_order_relaxed);
                mPollThread = std::thread([this] { pollLoop(); });

                return core::Result<void>::FromValue();
            }

            void DdsProxyEventBinding::Unsubscribe()
            {
                mRunning.store(false, std::memory_order_relaxed);
                if (mPollThread.joinable())
                {
                    mPollThread.join();
                }

                deleteSafe(mWaitSet);
                deleteSafe(mReadCond);
                deleteSafe(mReader);
                deleteSafe(mSubscriber);
                deleteSafe(mTopic);
                deleteSafe(mParticipant);

                std::lock_guard<std::mutex> lock(mMutex);
                mState = SubscriptionState::kNotSubscribed;
                mSampleQueue.clear();
                mReceiveHandler = nullptr;
            }

            SubscriptionState
            DdsProxyEventBinding::GetSubscriptionState() const noexcept
            {
                std::lock_guard<std::mutex> lock(mMutex);
                return mState;
            }

            core::Result<std::size_t> DdsProxyEventBinding::GetNewSamples(
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

            void DdsProxyEventBinding::SetReceiveHandler(
                std::function<void()> handler)
            {
                std::lock_guard<std::mutex> lock(mMutex);
                mReceiveHandler = std::move(handler);
            }

            void DdsProxyEventBinding::UnsetReceiveHandler()
            {
                std::lock_guard<std::mutex> lock(mMutex);
                mReceiveHandler = nullptr;
            }

            std::size_t DdsProxyEventBinding::GetFreeSampleCount()
                const noexcept
            {
                std::lock_guard<std::mutex> lock(mMutex);
                if (mSampleQueue.size() >= mMaxSampleCount)
                {
                    return 0U;
                }
                return mMaxSampleCount - mSampleQueue.size();
            }

            void DdsProxyEventBinding::SetSubscriptionStateChangeHandler(
                SubscriptionStateChangeHandler handler)
            {
                std::lock_guard<std::mutex> lock(mMutex);
                mStateChangeHandler = std::move(handler);
            }

            void DdsProxyEventBinding::UnsetSubscriptionStateChangeHandler()
            {
                std::lock_guard<std::mutex> lock(mMutex);
                mStateChangeHandler = nullptr;
            }

            void DdsProxyEventBinding::pollLoop() noexcept
            {
                static constexpr dds_duration_t cWaitTimeout = DDS_MSECS(50);

                while (mRunning.load(std::memory_order_relaxed))
                {
                    if (mWaitSet <= 0)
                    {
                        break;
                    }

                    dds_attach_t triggered = 0;
                    const int32_t waitRet =
                        dds_waitset_wait(mWaitSet, &triggered, 1, cWaitTimeout);
                    if (waitRet <= 0)
                    {
                        continue; // timeout or error
                    }

                    // Drain all available samples
                    static constexpr std::size_t cMaxTake = 16U;
                    AraComDdsRawEvent msgs[cMaxTake];
                    std::memset(msgs, 0, sizeof(msgs));
                    void *ptrBuf[cMaxTake];
                    dds_sample_info_t si[cMaxTake];
                    for (std::size_t i = 0U; i < cMaxTake; ++i)
                    {
                        ptrBuf[i] = &msgs[i];
                    }

                    const int32_t n = dds_take(
                        mReader, ptrBuf, si,
                        static_cast<size_t>(cMaxTake),
                        static_cast<uint32_t>(cMaxTake));

                    for (int32_t i = 0; i < n; ++i)
                    {
                        if (!si[i].valid_data)
                        {
                            continue;
                        }

                        const auto &msg = msgs[i];
                        const std::uint32_t payloadSize =
                            std::min(msg.size, ARA_COM_DDS_MAX_PAYLOAD_SIZE);

                        std::function<void()> notify;
                        {
                            std::lock_guard<std::mutex> lock(mMutex);
                            if (mSampleQueue.size() >= mMaxSampleCount)
                            {
                                mSampleQueue.pop_front();
                            }
                            mSampleQueue.emplace_back(
                                msg.data, msg.data + payloadSize);
                            notify = mReceiveHandler;
                        }

                        if (notify)
                        {
                            notify();
                        }
                    }
                }
            }

            // ── DdsSkeletonEventBinding ────────────────────────────────────

            DdsSkeletonEventBinding::DdsSkeletonEventBinding(
                EventBindingConfig config) noexcept
                : mConfig{config}
            {
            }

            DdsSkeletonEventBinding::~DdsSkeletonEventBinding() noexcept
            {
                if (mOffered)
                {
                    StopOffer();
                }
            }

            core::Result<void> DdsSkeletonEventBinding::Offer()
            {
                if (mOffered)
                {
                    return core::Result<void>::FromError(
                        MakeErrorCode(ComErrc::kFieldValueIsNotValid));
                }

                const std::uint32_t domainId =
                    static_cast<std::uint32_t>(mConfig.ServiceId) & 0x00FFU;

                mParticipant = dds_create_participant(
                    static_cast<dds_domainid_t>(domainId), nullptr, nullptr);
                if (mParticipant < 0)
                {
                    return core::Result<void>::FromError(
                        MakeErrorCode(ComErrc::kNetworkBindingFailure));
                }

                const std::string topicName =
                    DdsProxyEventBinding::makeTopicName(mConfig);
                mTopic = dds_create_topic(
                    mParticipant,
                    &AraComDdsRawEvent_desc,
                    topicName.c_str(),
                    nullptr, nullptr);
                if (mTopic < 0)
                {
                    deleteSafe(mParticipant);
                    return core::Result<void>::FromError(
                        MakeErrorCode(ComErrc::kNetworkBindingFailure));
                }

                mPublisher = dds_create_publisher(mParticipant, nullptr, nullptr);
                if (mPublisher < 0)
                {
                    deleteSafe(mTopic);
                    deleteSafe(mParticipant);
                    return core::Result<void>::FromError(
                        MakeErrorCode(ComErrc::kNetworkBindingFailure));
                }

                dds_qos_t *qos = dds_create_qos();
                dds_qset_reliability(qos, DDS_RELIABILITY_RELIABLE, DDS_SECS(1));
                dds_qset_history(qos, DDS_HISTORY_KEEP_LAST, 16);
                mWriter = dds_create_writer(mPublisher, mTopic, qos, nullptr);
                dds_delete_qos(qos);

                if (mWriter < 0)
                {
                    deleteSafe(mPublisher);
                    deleteSafe(mTopic);
                    deleteSafe(mParticipant);
                    return core::Result<void>::FromError(
                        MakeErrorCode(ComErrc::kNetworkBindingFailure));
                }

                mOffered = true;
                return core::Result<void>::FromValue();
            }

            void DdsSkeletonEventBinding::StopOffer()
            {
                if (!mOffered)
                {
                    return;
                }

                deleteSafe(mWriter);
                deleteSafe(mPublisher);
                deleteSafe(mTopic);
                deleteSafe(mParticipant);
                mOffered = false;
            }

            core::Result<void> DdsSkeletonEventBinding::Send(
                const std::vector<std::uint8_t> &payload)
            {
                if (!mOffered || mWriter <= 0)
                {
                    return core::Result<void>::FromError(
                        MakeErrorCode(ComErrc::kServiceNotOffered));
                }

                if (payload.size() > ARA_COM_DDS_MAX_PAYLOAD_SIZE)
                {
                    return core::Result<void>::FromError(
                        MakeErrorCode(ComErrc::kCommunicationStackError));
                }

                AraComDdsRawEvent msg;
                std::memset(&msg, 0, sizeof(msg));
                msg.size = static_cast<std::uint32_t>(payload.size());
                std::memcpy(msg.data, payload.data(), payload.size());

                const dds_return_t rc = dds_write(mWriter, &msg);
                if (rc != DDS_RETCODE_OK)
                {
                    return core::Result<void>::FromError(
                        MakeErrorCode(ComErrc::kCommunicationStackError));
                }

                return core::Result<void>::FromValue();
            }

            core::Result<void *> DdsSkeletonEventBinding::Allocate(
                std::size_t size)
            {
                if (!mOffered || mWriter <= 0)
                {
                    return core::Result<void *>::FromError(
                        MakeErrorCode(ComErrc::kServiceNotOffered));
                }

                if (size > ARA_COM_DDS_MAX_PAYLOAD_SIZE)
                {
                    return core::Result<void *>::FromError(
                        MakeErrorCode(ComErrc::kCommunicationStackError));
                }

                // No true zero-copy in DDS C API; allocate a heap buffer that
                // SendAllocated() will copy into the DDS message.
                void *ptr = std::malloc(size);
                if (!ptr)
                {
                    return core::Result<void *>::FromError(
                        MakeErrorCode(ComErrc::kSampleAllocationFailure));
                }

                return core::Result<void *>::FromValue(ptr);
            }

            core::Result<void> DdsSkeletonEventBinding::SendAllocated(
                void *data, std::size_t size)
            {
                if (!data)
                {
                    return core::Result<void>::FromError(
                        MakeErrorCode(ComErrc::kFieldValueIsNotValid));
                }

                const std::vector<std::uint8_t> payload(
                    static_cast<const std::uint8_t *>(data),
                    static_cast<const std::uint8_t *>(data) + size);
                std::free(data);

                return Send(payload);
            }
        }
    }
}

#endif // ARA_COM_USE_CYCLONEDDS
