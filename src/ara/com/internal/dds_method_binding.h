/// @file src/ara/com/internal/dds_method_binding.h
/// @brief CycloneDDS-backed ProxyMethodBinding and SkeletonMethodBinding.
/// @details Two DDS topics per method — request and reply — provide a
///          request-reply RPC channel without IDL code generation.
///          The proxy assigns an atomic session ID to each call, stores the
///          response handler in a pending map, and writes the request to the
///          request topic.  A background thread drains the reply topic and
///          dispatches the matching response handler.
///          The skeleton runs a background thread that drains the request
///          topic, invokes the registered handler, and writes the reply.
///
///          This file is part of the Adaptive AUTOSAR educational implementation.

#ifndef ARA_COM_INTERNAL_DDS_METHOD_BINDING_H
#define ARA_COM_INTERNAL_DDS_METHOD_BINDING_H

#include <atomic>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include "./method_binding.h"
#include "../com_error_domain.h"

#if defined(ARA_COM_USE_CYCLONEDDS) && (ARA_COM_USE_CYCLONEDDS == 1)
#include <dds/dds.h>
#include <dds/ddsc/dds_opcodes.h>
#endif

namespace ara
{
    namespace com
    {
        namespace internal
        {
#if defined(ARA_COM_USE_CYCLONEDDS) && (ARA_COM_USE_CYCLONEDDS == 1)

            /// @brief Maximum serialized payload size for one DDS raw-bytes method
            ///        request or reply.  Payloads larger than this are rejected.
            static constexpr std::uint32_t ARA_COM_DDS_MAX_METHOD_PAYLOAD_SIZE = 4096U;

            /// @brief DDS type for a raw method request.
            ///        - session_id: monotonically increasing call identifier (key)
            ///        - size:       number of valid bytes in data[]
            ///        - data[]:     serialized method arguments
            struct AraComDdsRawRequest
            {
                std::uint32_t session_id;
                std::uint32_t size;
                std::uint8_t  data[ARA_COM_DDS_MAX_METHOD_PAYLOAD_SIZE];
            };

            /// @brief DDS type for a raw method reply.
            ///        - session_id: must match the request session_id (key)
            ///        - is_error:   0 = success, 1 = error
            ///        - size:       number of valid bytes in data[]
            ///        - data[]:     serialized return value (empty on error)
            struct AraComDdsRawReply
            {
                std::uint32_t session_id;
                std::uint32_t is_error;
                std::uint32_t size;
                std::uint8_t  data[ARA_COM_DDS_MAX_METHOD_PAYLOAD_SIZE];
            };

            extern const dds_topic_descriptor_t AraComDdsRawRequest_desc;
            extern const dds_topic_descriptor_t AraComDdsRawReply_desc;

#endif // ARA_COM_USE_CYCLONEDDS

            /// @brief CycloneDDS-based proxy-side method binding.
            ///        Creates a DDS writer for requests and a DDS reader for replies.
            ///        Replies are dispatched to callers via session-ID correlation.
            class DdsProxyMethodBinding final : public ProxyMethodBinding
            {
            private:
                MethodBindingConfig mConfig;
                std::mutex mMutex;
                std::atomic<std::uint32_t> mNextSessionId{1U};
                std::unordered_map<std::uint32_t, RawResponseHandler> mPending;

#if defined(ARA_COM_USE_CYCLONEDDS) && (ARA_COM_USE_CYCLONEDDS == 1)
                dds_entity_t mParticipant{0};
                dds_entity_t mReqTopic{0};
                dds_entity_t mRepTopic{0};
                dds_entity_t mPublisher{0};
                dds_entity_t mWriter{0};
                dds_entity_t mSubscriber{0};
                dds_entity_t mReader{0};
                dds_entity_t mWaitSet{0};
                dds_entity_t mReadCond{0};
#endif

                std::atomic<bool> mRunning{false};
                std::thread mPollThread;
                bool mInitialized{false};

                void pollLoop() noexcept;
                bool initDds() noexcept;
                void shutdownDds() noexcept;

            public:
                /// @brief Build the DDS topic name for requests.
                static std::string makeRequestTopicName(
                    const MethodBindingConfig &cfg) noexcept;

                /// @brief Build the DDS topic name for replies.
                static std::string makeReplyTopicName(
                    const MethodBindingConfig &cfg) noexcept;

                explicit DdsProxyMethodBinding(
                    MethodBindingConfig config) noexcept;
                ~DdsProxyMethodBinding() noexcept override;

                DdsProxyMethodBinding(const DdsProxyMethodBinding &) = delete;
                DdsProxyMethodBinding &operator=(
                    const DdsProxyMethodBinding &) = delete;

                void Call(
                    const std::vector<std::uint8_t> &requestPayload,
                    RawResponseHandler responseHandler) override;
            };

            /// @brief CycloneDDS-based skeleton-side method binding.
            ///        Creates a DDS reader for requests and a DDS writer for replies.
            ///        A background thread services requests and writes replies.
            class DdsSkeletonMethodBinding final : public SkeletonMethodBinding
            {
            private:
                MethodBindingConfig mConfig;
                mutable std::mutex mMutex;
                RawRequestHandler mHandler;

#if defined(ARA_COM_USE_CYCLONEDDS) && (ARA_COM_USE_CYCLONEDDS == 1)
                dds_entity_t mParticipant{0};
                dds_entity_t mReqTopic{0};
                dds_entity_t mRepTopic{0};
                dds_entity_t mPublisher{0};
                dds_entity_t mWriter{0};
                dds_entity_t mSubscriber{0};
                dds_entity_t mReader{0};
                dds_entity_t mWaitSet{0};
                dds_entity_t mReadCond{0};
#endif

                std::atomic<bool> mRunning{false};
                std::thread mServiceThread;
                bool mInitialized{false};

                void serviceLoop() noexcept;
                bool initDds() noexcept;
                void shutdownDds() noexcept;

            public:
                explicit DdsSkeletonMethodBinding(
                    MethodBindingConfig config) noexcept;
                ~DdsSkeletonMethodBinding() noexcept override;

                DdsSkeletonMethodBinding(const DdsSkeletonMethodBinding &) = delete;
                DdsSkeletonMethodBinding &operator=(
                    const DdsSkeletonMethodBinding &) = delete;

                core::Result<void> Register(RawRequestHandler handler) override;
                void Unregister() override;
            };
        }
    }
}

#endif // ARA_COM_INTERNAL_DDS_METHOD_BINDING_H
