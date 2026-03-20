/// @file src/ara/com/internal/dds_method_binding.cpp
/// @brief Implementation of CycloneDDS method binding.
/// @details Uses the CycloneDDS C API with manually defined topic type
///          descriptors for raw-bytes request/reply pairs.
///
///          This file is part of the Adaptive AUTOSAR educational implementation.

#include <algorithm>
#include <cstring>
#include "./dds_method_binding.h"

#if defined(ARA_COM_USE_CYCLONEDDS) && (ARA_COM_USE_CYCLONEDDS == 1)

namespace ara
{
    namespace com
    {
        namespace internal
        {
            // ── AraComDdsRawRequest topic descriptor ──────────────────────────
            //
            // struct AraComDdsRawRequest {
            //     unsigned long session_id;   // @key
            //     unsigned long size;
            //     octet         data[4096];
            // };
            //
            // Ops (CycloneDDS dds_opcodes.h encoding):
            //   [0] ADR | TYPE_4BY | FLAG_KEY   [1] offsetof(session_id)
            //   [2] ADR | TYPE_4BY              [3] offsetof(size)
            //   [4] ADR | TYPE_ARR | SUBTYPE_1BY  [5] offsetof(data)  [6] 4096
            //   [7] RTS

            static const std::uint32_t AraComDdsRawRequest_ops[] = {
                static_cast<std::uint32_t>(DDS_OP_ADR) |
                    static_cast<std::uint32_t>(DDS_OP_TYPE_4BY) |
                    static_cast<std::uint32_t>(DDS_OP_FLAG_KEY),
                static_cast<std::uint32_t>(offsetof(AraComDdsRawRequest, session_id)),
                static_cast<std::uint32_t>(DDS_OP_ADR) |
                    static_cast<std::uint32_t>(DDS_OP_TYPE_4BY),
                static_cast<std::uint32_t>(offsetof(AraComDdsRawRequest, size)),
                static_cast<std::uint32_t>(DDS_OP_ADR) |
                    static_cast<std::uint32_t>(DDS_OP_TYPE_ARR) |
                    static_cast<std::uint32_t>(DDS_OP_SUBTYPE_1BY),
                static_cast<std::uint32_t>(offsetof(AraComDdsRawRequest, data)),
                ARA_COM_DDS_MAX_METHOD_PAYLOAD_SIZE,
                static_cast<std::uint32_t>(DDS_OP_RTS)};

            static const dds_key_descriptor_t AraComDdsRawRequest_keys[] = {
                {"session_id", 0U, 1U}};

            const dds_topic_descriptor_t AraComDdsRawRequest_desc = {
                sizeof(AraComDdsRawRequest),
                alignof(AraComDdsRawRequest),
                DDS_TOPIC_FIXED_SIZE |
                    DDS_TOPIC_FIXED_KEY |
                    DDS_TOPIC_FIXED_KEY_XCDR2,
                1U,
                "ara::com::DdsRawRequest",
                AraComDdsRawRequest_keys,
                8U,
                AraComDdsRawRequest_ops,
                nullptr};

            // ── AraComDdsRawReply topic descriptor ────────────────────────────
            //
            // struct AraComDdsRawReply {
            //     unsigned long session_id;   // @key
            //     unsigned long is_error;
            //     unsigned long size;
            //     octet         data[4096];
            // };
            //
            // Ops:
            //   [0] ADR | TYPE_4BY | FLAG_KEY   [1] offsetof(session_id)
            //   [2] ADR | TYPE_4BY              [3] offsetof(is_error)
            //   [4] ADR | TYPE_4BY              [5] offsetof(size)
            //   [6] ADR | TYPE_ARR | SUBTYPE_1BY  [7] offsetof(data)  [8] 4096
            //   [9] RTS

            static const std::uint32_t AraComDdsRawReply_ops[] = {
                static_cast<std::uint32_t>(DDS_OP_ADR) |
                    static_cast<std::uint32_t>(DDS_OP_TYPE_4BY) |
                    static_cast<std::uint32_t>(DDS_OP_FLAG_KEY),
                static_cast<std::uint32_t>(offsetof(AraComDdsRawReply, session_id)),
                static_cast<std::uint32_t>(DDS_OP_ADR) |
                    static_cast<std::uint32_t>(DDS_OP_TYPE_4BY),
                static_cast<std::uint32_t>(offsetof(AraComDdsRawReply, is_error)),
                static_cast<std::uint32_t>(DDS_OP_ADR) |
                    static_cast<std::uint32_t>(DDS_OP_TYPE_4BY),
                static_cast<std::uint32_t>(offsetof(AraComDdsRawReply, size)),
                static_cast<std::uint32_t>(DDS_OP_ADR) |
                    static_cast<std::uint32_t>(DDS_OP_TYPE_ARR) |
                    static_cast<std::uint32_t>(DDS_OP_SUBTYPE_1BY),
                static_cast<std::uint32_t>(offsetof(AraComDdsRawReply, data)),
                ARA_COM_DDS_MAX_METHOD_PAYLOAD_SIZE,
                static_cast<std::uint32_t>(DDS_OP_RTS)};

            static const dds_key_descriptor_t AraComDdsRawReply_keys[] = {
                {"session_id", 0U, 1U}};

            const dds_topic_descriptor_t AraComDdsRawReply_desc = {
                sizeof(AraComDdsRawReply),
                alignof(AraComDdsRawReply),
                DDS_TOPIC_FIXED_SIZE |
                    DDS_TOPIC_FIXED_KEY |
                    DDS_TOPIC_FIXED_KEY_XCDR2,
                1U,
                "ara::com::DdsRawReply",
                AraComDdsRawReply_keys,
                10U,
                AraComDdsRawReply_ops,
                nullptr};

            // ── Shared helpers ─────────────────────────────────────────────────

            namespace
            {
                std::string idToHex(std::uint16_t id)
                {
                    static constexpr char hex[] = "0123456789abcdef";
                    std::string result(4, '0');
                    result[0] = hex[(id >> 12) & 0xF];
                    result[1] = hex[(id >> 8) & 0xF];
                    result[2] = hex[(id >> 4) & 0xF];
                    result[3] = hex[id & 0xF];
                    return result;
                }

                void deleteSafe(dds_entity_t &entity) noexcept
                {
                    if (entity > 0)
                    {
                        dds_delete(entity);
                        entity = 0;
                    }
                }
            } // namespace

            // ── DdsProxyMethodBinding ──────────────────────────────────────────

            std::string DdsProxyMethodBinding::makeRequestTopicName(
                const MethodBindingConfig &cfg) noexcept
            {
                return "ara_com_req_svc" + idToHex(cfg.ServiceId) +
                       "_inst" + idToHex(cfg.InstanceId) +
                       "_mth" + idToHex(cfg.MethodId);
            }

            std::string DdsProxyMethodBinding::makeReplyTopicName(
                const MethodBindingConfig &cfg) noexcept
            {
                return "ara_com_rep_svc" + idToHex(cfg.ServiceId) +
                       "_inst" + idToHex(cfg.InstanceId) +
                       "_mth" + idToHex(cfg.MethodId);
            }

            bool DdsProxyMethodBinding::initDds() noexcept
            {
                const std::uint32_t domainId =
                    static_cast<std::uint32_t>(mConfig.ServiceId) & 0x00FFU;

                mParticipant = dds_create_participant(
                    static_cast<dds_domainid_t>(domainId), nullptr, nullptr);
                if (mParticipant < 0)
                {
                    return false;
                }

                const std::string reqTopicName = makeRequestTopicName(mConfig);
                mReqTopic = dds_create_topic(
                    mParticipant,
                    &AraComDdsRawRequest_desc,
                    reqTopicName.c_str(),
                    nullptr, nullptr);
                if (mReqTopic < 0)
                {
                    shutdownDds();
                    return false;
                }

                const std::string repTopicName = makeReplyTopicName(mConfig);
                mRepTopic = dds_create_topic(
                    mParticipant,
                    &AraComDdsRawReply_desc,
                    repTopicName.c_str(),
                    nullptr, nullptr);
                if (mRepTopic < 0)
                {
                    shutdownDds();
                    return false;
                }

                mPublisher = dds_create_publisher(mParticipant, nullptr, nullptr);
                if (mPublisher < 0)
                {
                    shutdownDds();
                    return false;
                }

                dds_qos_t *wqos = dds_create_qos();
                dds_qset_reliability(wqos, DDS_RELIABILITY_RELIABLE, DDS_SECS(5));
                dds_qset_history(wqos, DDS_HISTORY_KEEP_LAST, 16);
                mWriter = dds_create_writer(mPublisher, mReqTopic, wqos, nullptr);
                dds_delete_qos(wqos);
                if (mWriter < 0)
                {
                    shutdownDds();
                    return false;
                }

                mSubscriber = dds_create_subscriber(mParticipant, nullptr, nullptr);
                if (mSubscriber < 0)
                {
                    shutdownDds();
                    return false;
                }

                dds_qos_t *rqos = dds_create_qos();
                dds_qset_reliability(rqos, DDS_RELIABILITY_RELIABLE, DDS_SECS(5));
                dds_qset_history(rqos, DDS_HISTORY_KEEP_LAST, 16);
                mReader = dds_create_reader(mSubscriber, mRepTopic, rqos, nullptr);
                dds_delete_qos(rqos);
                if (mReader < 0)
                {
                    shutdownDds();
                    return false;
                }

                mReadCond = dds_create_readcondition(
                    mReader,
                    DDS_ANY_SAMPLE_STATE | DDS_ANY_VIEW_STATE | DDS_ANY_INSTANCE_STATE);
                if (mReadCond < 0)
                {
                    shutdownDds();
                    return false;
                }

                mWaitSet = dds_create_waitset(mParticipant);
                if (mWaitSet < 0)
                {
                    shutdownDds();
                    return false;
                }

                dds_waitset_attach(mWaitSet, mReadCond,
                                   static_cast<dds_attach_t>(mReader));

                return true;
            }

            void DdsProxyMethodBinding::shutdownDds() noexcept
            {
                deleteSafe(mWaitSet);
                deleteSafe(mReadCond);
                deleteSafe(mReader);
                deleteSafe(mSubscriber);
                deleteSafe(mWriter);
                deleteSafe(mPublisher);
                deleteSafe(mRepTopic);
                deleteSafe(mReqTopic);
                deleteSafe(mParticipant);
            }

            DdsProxyMethodBinding::DdsProxyMethodBinding(
                MethodBindingConfig config) noexcept
                : mConfig{config}
            {
                mInitialized = initDds();
                if (mInitialized)
                {
                    mRunning.store(true, std::memory_order_relaxed);
                    mPollThread = std::thread([this] { pollLoop(); });
                }
            }

            DdsProxyMethodBinding::~DdsProxyMethodBinding() noexcept
            {
                mRunning.store(false, std::memory_order_relaxed);
                if (mPollThread.joinable())
                {
                    mPollThread.join();
                }

                // Fail any pending calls
                std::unordered_map<std::uint32_t, RawResponseHandler> pending;
                {
                    std::lock_guard<std::mutex> lock(mMutex);
                    pending.swap(mPending);
                }
                for (auto &kv : pending)
                {
                    kv.second(core::Result<std::vector<std::uint8_t>>::FromError(
                        MakeErrorCode(ComErrc::kNetworkBindingFailure)));
                }

                shutdownDds();
            }

            void DdsProxyMethodBinding::Call(
                const std::vector<std::uint8_t> &requestPayload,
                RawResponseHandler responseHandler)
            {
                if (!mInitialized || mWriter <= 0)
                {
                    responseHandler(
                        core::Result<std::vector<std::uint8_t>>::FromError(
                            MakeErrorCode(ComErrc::kNetworkBindingFailure)));
                    return;
                }

                if (requestPayload.size() > ARA_COM_DDS_MAX_METHOD_PAYLOAD_SIZE)
                {
                    responseHandler(
                        core::Result<std::vector<std::uint8_t>>::FromError(
                            MakeErrorCode(ComErrc::kCommunicationStackError)));
                    return;
                }

                const std::uint32_t sessionId =
                    mNextSessionId.fetch_add(1U, std::memory_order_relaxed);

                {
                    std::lock_guard<std::mutex> lock(mMutex);
                    mPending.emplace(sessionId, std::move(responseHandler));
                }

                AraComDdsRawRequest req;
                req.session_id = sessionId;
                req.size = static_cast<std::uint32_t>(requestPayload.size());
                std::memcpy(req.data, requestPayload.data(), requestPayload.size());
                if (requestPayload.size() < ARA_COM_DDS_MAX_METHOD_PAYLOAD_SIZE)
                {
                    std::memset(req.data + requestPayload.size(), 0,
                                ARA_COM_DDS_MAX_METHOD_PAYLOAD_SIZE - requestPayload.size());
                }

                const dds_return_t rc = dds_write(mWriter, &req);
                if (rc != DDS_RETCODE_OK)
                {
                    RawResponseHandler handler;
                    {
                        std::lock_guard<std::mutex> lock(mMutex);
                        auto it = mPending.find(sessionId);
                        if (it != mPending.end())
                        {
                            handler = std::move(it->second);
                            mPending.erase(it);
                        }
                    }
                    if (handler)
                    {
                        handler(core::Result<std::vector<std::uint8_t>>::FromError(
                            MakeErrorCode(ComErrc::kCommunicationStackError)));
                    }
                }
            }

            void DdsProxyMethodBinding::pollLoop() noexcept
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
                        continue;
                    }

                    static constexpr std::size_t cMaxTake = 16U;
                    AraComDdsRawReply msgs[cMaxTake];
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
                        RawResponseHandler handler;
                        {
                            std::lock_guard<std::mutex> lock(mMutex);
                            auto it = mPending.find(msg.session_id);
                            if (it != mPending.end())
                            {
                                handler = std::move(it->second);
                                mPending.erase(it);
                            }
                        }

                        if (!handler)
                        {
                            continue;
                        }

                        if (msg.is_error != 0U)
                        {
                            handler(core::Result<std::vector<std::uint8_t>>::FromError(
                                MakeErrorCode(ComErrc::kCommunicationStackError)));
                        }
                        else
                        {
                            const std::uint32_t payloadSize =
                                std::min(msg.size, ARA_COM_DDS_MAX_METHOD_PAYLOAD_SIZE);
                            handler(core::Result<std::vector<std::uint8_t>>::FromValue(
                                std::vector<std::uint8_t>(
                                    msg.data, msg.data + payloadSize)));
                        }
                    }
                }
            }

            // ── DdsSkeletonMethodBinding ───────────────────────────────────────

            bool DdsSkeletonMethodBinding::initDds() noexcept
            {
                const std::uint32_t domainId =
                    static_cast<std::uint32_t>(mConfig.ServiceId) & 0x00FFU;

                mParticipant = dds_create_participant(
                    static_cast<dds_domainid_t>(domainId), nullptr, nullptr);
                if (mParticipant < 0)
                {
                    return false;
                }

                const std::string reqTopicName =
                    DdsProxyMethodBinding::makeRequestTopicName(mConfig);
                mReqTopic = dds_create_topic(
                    mParticipant,
                    &AraComDdsRawRequest_desc,
                    reqTopicName.c_str(),
                    nullptr, nullptr);
                if (mReqTopic < 0)
                {
                    shutdownDds();
                    return false;
                }

                const std::string repTopicName =
                    DdsProxyMethodBinding::makeReplyTopicName(mConfig);
                mRepTopic = dds_create_topic(
                    mParticipant,
                    &AraComDdsRawReply_desc,
                    repTopicName.c_str(),
                    nullptr, nullptr);
                if (mRepTopic < 0)
                {
                    shutdownDds();
                    return false;
                }

                mPublisher = dds_create_publisher(mParticipant, nullptr, nullptr);
                if (mPublisher < 0)
                {
                    shutdownDds();
                    return false;
                }

                dds_qos_t *wqos = dds_create_qos();
                dds_qset_reliability(wqos, DDS_RELIABILITY_RELIABLE, DDS_SECS(5));
                dds_qset_history(wqos, DDS_HISTORY_KEEP_LAST, 16);
                mWriter = dds_create_writer(mPublisher, mRepTopic, wqos, nullptr);
                dds_delete_qos(wqos);
                if (mWriter < 0)
                {
                    shutdownDds();
                    return false;
                }

                mSubscriber = dds_create_subscriber(mParticipant, nullptr, nullptr);
                if (mSubscriber < 0)
                {
                    shutdownDds();
                    return false;
                }

                dds_qos_t *rqos = dds_create_qos();
                dds_qset_reliability(rqos, DDS_RELIABILITY_RELIABLE, DDS_SECS(5));
                dds_qset_history(rqos, DDS_HISTORY_KEEP_LAST, 16);
                mReader = dds_create_reader(mSubscriber, mReqTopic, rqos, nullptr);
                dds_delete_qos(rqos);
                if (mReader < 0)
                {
                    shutdownDds();
                    return false;
                }

                mReadCond = dds_create_readcondition(
                    mReader,
                    DDS_ANY_SAMPLE_STATE | DDS_ANY_VIEW_STATE | DDS_ANY_INSTANCE_STATE);
                if (mReadCond < 0)
                {
                    shutdownDds();
                    return false;
                }

                mWaitSet = dds_create_waitset(mParticipant);
                if (mWaitSet < 0)
                {
                    shutdownDds();
                    return false;
                }

                dds_waitset_attach(mWaitSet, mReadCond,
                                   static_cast<dds_attach_t>(mReader));

                return true;
            }

            void DdsSkeletonMethodBinding::shutdownDds() noexcept
            {
                deleteSafe(mWaitSet);
                deleteSafe(mReadCond);
                deleteSafe(mReader);
                deleteSafe(mSubscriber);
                deleteSafe(mWriter);
                deleteSafe(mPublisher);
                deleteSafe(mRepTopic);
                deleteSafe(mReqTopic);
                deleteSafe(mParticipant);
            }

            DdsSkeletonMethodBinding::DdsSkeletonMethodBinding(
                MethodBindingConfig config) noexcept
                : mConfig{config}
            {
            }

            DdsSkeletonMethodBinding::~DdsSkeletonMethodBinding() noexcept
            {
                Unregister();
            }

            core::Result<void> DdsSkeletonMethodBinding::Register(
                RawRequestHandler handler)
            {
                if (!handler)
                {
                    return core::Result<void>::FromError(
                        MakeErrorCode(ComErrc::kSetHandlerNotSet));
                }

                {
                    std::lock_guard<std::mutex> lock(mMutex);
                    mHandler = std::move(handler);
                }

                if (!mInitialized)
                {
                    mInitialized = initDds();
                    if (!mInitialized)
                    {
                        std::lock_guard<std::mutex> lock(mMutex);
                        mHandler = nullptr;
                        return core::Result<void>::FromError(
                            MakeErrorCode(ComErrc::kNetworkBindingFailure));
                    }
                }

                if (!mServiceThread.joinable())
                {
                    mRunning.store(true, std::memory_order_relaxed);
                    mServiceThread = std::thread([this] { serviceLoop(); });
                }

                return core::Result<void>::FromValue();
            }

            void DdsSkeletonMethodBinding::Unregister()
            {
                mRunning.store(false, std::memory_order_relaxed);
                if (mServiceThread.joinable())
                {
                    mServiceThread.join();
                }

                {
                    std::lock_guard<std::mutex> lock(mMutex);
                    mHandler = nullptr;
                }

                if (mInitialized)
                {
                    shutdownDds();
                    mInitialized = false;
                }
            }

            void DdsSkeletonMethodBinding::serviceLoop() noexcept
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
                        continue;
                    }

                    static constexpr std::size_t cMaxTake = 16U;
                    AraComDdsRawRequest msgs[cMaxTake];
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
                            std::min(msg.size, ARA_COM_DDS_MAX_METHOD_PAYLOAD_SIZE);
                        const std::vector<std::uint8_t> reqPayload(
                            msg.data, msg.data + payloadSize);

                        RawRequestHandler handler;
                        {
                            std::lock_guard<std::mutex> lock(mMutex);
                            handler = mHandler;
                        }

                        AraComDdsRawReply reply;
                        reply.session_id = msg.session_id;

                        if (handler)
                        {
                            auto result = handler(reqPayload);
                            if (result.HasValue())
                            {
                                reply.is_error = 0U;
                                const auto &respPayload = result.Value();
                                reply.size = static_cast<std::uint32_t>(
                                    std::min(respPayload.size(),
                                             static_cast<std::size_t>(
                                                 ARA_COM_DDS_MAX_METHOD_PAYLOAD_SIZE)));
                                std::memcpy(reply.data,
                                            respPayload.data(), reply.size);
                                if (reply.size < ARA_COM_DDS_MAX_METHOD_PAYLOAD_SIZE)
                                {
                                    std::memset(reply.data + reply.size, 0,
                                                ARA_COM_DDS_MAX_METHOD_PAYLOAD_SIZE - reply.size);
                                }
                            }
                            else
                            {
                                reply.is_error = 1U;
                                reply.size = 0U;
                                std::memset(reply.data, 0, ARA_COM_DDS_MAX_METHOD_PAYLOAD_SIZE);
                            }
                        }
                        else
                        {
                            reply.is_error = 1U;
                            reply.size = 0U;
                            std::memset(reply.data, 0, ARA_COM_DDS_MAX_METHOD_PAYLOAD_SIZE);
                        }

                        if (mWriter > 0)
                        {
                            dds_write(mWriter, &reply);
                        }
                    }
                }
            }
        }
    }
}

#endif // ARA_COM_USE_CYCLONEDDS
