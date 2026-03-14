/// @file src/ara/com/internal/iceoryx_method_binding.h
/// @brief iceoryx-backed ProxyMethodBinding and SkeletonMethodBinding.
/// @details Two iceoryx channels per method provide a request-reply RPC
///          channel over a publish-subscribe transport.
///
///          Channel naming (hex IDs, 4 digits zero-padded):
///            Request: "svc_XXXX" / "inst_XXXX" / "req_XXXX"
///            Reply:   "svc_XXXX" / "inst_XXXX" / "rep_XXXX"
///
///          Payload framing:
///            Request payload: [4-byte session_id LE] + [serialized args]
///            Reply   payload: [4-byte session_id LE] + [4-byte is_error LE]
///                             + [serialized return value (empty on error)]
///
///          The proxy publishes requests and subscribes to replies; a
///          background poll thread dispatches response handlers keyed by
///          session ID.  The skeleton subscribes to requests and publishes
///          replies; a background service thread calls the registered handler.
///
///          This file is part of the Adaptive AUTOSAR educational implementation.

#ifndef ARA_COM_INTERNAL_ICEORYX_METHOD_BINDING_H
#define ARA_COM_INTERNAL_ICEORYX_METHOD_BINDING_H

#include <atomic>
#include <mutex>
#include <thread>
#include <unordered_map>
#include "./method_binding.h"
#include "../com_error_domain.h"
#include "../zerocopy/zero_copy.h"

#if ARA_COM_USE_ICEORYX

namespace ara
{
    namespace com
    {
        namespace internal
        {
            /// @brief iceoryx-based proxy-side method binding.
            ///        Publishes serialized requests on a request channel and
            ///        receives serialized replies on a reply channel.
            class IceoryxProxyMethodBinding final : public ProxyMethodBinding
            {
            private:
                MethodBindingConfig mConfig;
                std::mutex mMutex;
                std::atomic<std::uint32_t> mNextSessionId{1U};
                std::unordered_map<std::uint32_t, RawResponseHandler> mPending;

                std::unique_ptr<zerocopy::ZeroCopyPublisher> mReqPublisher;
                std::unique_ptr<zerocopy::ZeroCopySubscriber> mRepSubscriber;

                std::atomic<bool> mRunning{false};
                std::thread mPollThread;

                void pollLoop() noexcept;

            public:
                static zerocopy::ChannelDescriptor makeRequestChannel(
                    const MethodBindingConfig &cfg) noexcept;
                static zerocopy::ChannelDescriptor makeReplyChannel(
                    const MethodBindingConfig &cfg) noexcept;

                explicit IceoryxProxyMethodBinding(
                    MethodBindingConfig config) noexcept;
                ~IceoryxProxyMethodBinding() noexcept override;

                IceoryxProxyMethodBinding(
                    const IceoryxProxyMethodBinding &) = delete;
                IceoryxProxyMethodBinding &operator=(
                    const IceoryxProxyMethodBinding &) = delete;

                void Call(
                    const std::vector<std::uint8_t> &requestPayload,
                    RawResponseHandler responseHandler) override;
            };

            /// @brief iceoryx-based skeleton-side method binding.
            ///        Subscribes to requests, calls the registered handler,
            ///        and publishes the reply.
            class IceoryxSkeletonMethodBinding final : public SkeletonMethodBinding
            {
            private:
                MethodBindingConfig mConfig;
                mutable std::mutex mMutex;
                RawRequestHandler mHandler;

                std::unique_ptr<zerocopy::ZeroCopySubscriber> mReqSubscriber;
                std::unique_ptr<zerocopy::ZeroCopyPublisher> mRepPublisher;

                std::atomic<bool> mRunning{false};
                std::thread mServiceThread;

                void serviceLoop() noexcept;

            public:
                explicit IceoryxSkeletonMethodBinding(
                    MethodBindingConfig config) noexcept;
                ~IceoryxSkeletonMethodBinding() noexcept override;

                IceoryxSkeletonMethodBinding(
                    const IceoryxSkeletonMethodBinding &) = delete;
                IceoryxSkeletonMethodBinding &operator=(
                    const IceoryxSkeletonMethodBinding &) = delete;

                core::Result<void> Register(RawRequestHandler handler) override;
                void Unregister() override;
            };
        }
    }
}

#endif // ARA_COM_USE_ICEORYX

#endif // ARA_COM_INTERNAL_ICEORYX_METHOD_BINDING_H
