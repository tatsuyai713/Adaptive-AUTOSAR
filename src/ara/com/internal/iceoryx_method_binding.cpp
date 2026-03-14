/// @file src/ara/com/internal/iceoryx_method_binding.cpp
/// @brief Implementation of iceoryx method binding.
/// @details This file is part of the Adaptive AUTOSAR educational implementation.

#include <algorithm>
#include <iomanip>
#include <sstream>
#include "./iceoryx_method_binding.h"

#if ARA_COM_USE_ICEORYX

namespace ara
{
    namespace com
    {
        namespace internal
        {
            namespace
            {
                std::string idToHex(std::uint16_t id)
                {
                    std::ostringstream oss;
                    oss << std::hex << std::setfill('0') << std::setw(4)
                        << static_cast<unsigned>(id);
                    return oss.str();
                }

                /// @brief Encode a 32-bit value as 4 little-endian bytes.
                void encodeU32LE(std::uint32_t value, std::vector<std::uint8_t> &buf)
                {
                    buf.push_back(static_cast<std::uint8_t>(value & 0xFFU));
                    buf.push_back(static_cast<std::uint8_t>((value >> 8U) & 0xFFU));
                    buf.push_back(static_cast<std::uint8_t>((value >> 16U) & 0xFFU));
                    buf.push_back(static_cast<std::uint8_t>((value >> 24U) & 0xFFU));
                }

                /// @brief Decode a 32-bit value from 4 little-endian bytes.
                /// @returns false if there are not enough bytes.
                bool decodeU32LE(
                    const std::uint8_t *data, std::size_t size,
                    std::size_t offset, std::uint32_t &value)
                {
                    if (offset + 4U > size)
                    {
                        return false;
                    }
                    value = static_cast<std::uint32_t>(data[offset]) |
                            (static_cast<std::uint32_t>(data[offset + 1U]) << 8U) |
                            (static_cast<std::uint32_t>(data[offset + 2U]) << 16U) |
                            (static_cast<std::uint32_t>(data[offset + 3U]) << 24U);
                    return true;
                }
            } // namespace

            // ── IceoryxProxyMethodBinding ─────────────────────────────────────

            zerocopy::ChannelDescriptor
            IceoryxProxyMethodBinding::makeRequestChannel(
                const MethodBindingConfig &cfg) noexcept
            {
                return zerocopy::ChannelDescriptor{
                    "svc_" + idToHex(cfg.ServiceId),
                    "inst_" + idToHex(cfg.InstanceId),
                    "req_" + idToHex(cfg.MethodId)};
            }

            zerocopy::ChannelDescriptor
            IceoryxProxyMethodBinding::makeReplyChannel(
                const MethodBindingConfig &cfg) noexcept
            {
                return zerocopy::ChannelDescriptor{
                    "svc_" + idToHex(cfg.ServiceId),
                    "inst_" + idToHex(cfg.InstanceId),
                    "rep_" + idToHex(cfg.MethodId)};
            }

            IceoryxProxyMethodBinding::IceoryxProxyMethodBinding(
                MethodBindingConfig config) noexcept
                : mConfig{config}
            {
                mReqPublisher.reset(new zerocopy::ZeroCopyPublisher{
                    makeRequestChannel(mConfig), "ara_com_proxy_method"});

                mRepSubscriber.reset(new zerocopy::ZeroCopySubscriber{
                    makeReplyChannel(mConfig), "ara_com_proxy_method", 64U});

                mRunning.store(true, std::memory_order_relaxed);
                mPollThread = std::thread([this] { pollLoop(); });
            }

            IceoryxProxyMethodBinding::~IceoryxProxyMethodBinding() noexcept
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

                mRepSubscriber.reset();
                mReqPublisher.reset();
            }

            void IceoryxProxyMethodBinding::Call(
                const std::vector<std::uint8_t> &requestPayload,
                RawResponseHandler responseHandler)
            {
                if (!mReqPublisher)
                {
                    responseHandler(
                        core::Result<std::vector<std::uint8_t>>::FromError(
                            MakeErrorCode(ComErrc::kNetworkBindingFailure)));
                    return;
                }

                const std::uint32_t sessionId =
                    mNextSessionId.fetch_add(1U, std::memory_order_relaxed);

                {
                    std::lock_guard<std::mutex> lock(mMutex);
                    mPending.emplace(sessionId, std::move(responseHandler));
                }

                // Framing: [4-byte session_id LE] + [request bytes]
                std::vector<std::uint8_t> frame;
                frame.reserve(4U + requestPayload.size());
                encodeU32LE(sessionId, frame);
                frame.insert(frame.end(), requestPayload.begin(), requestPayload.end());

                auto result = mReqPublisher->PublishCopy(frame);
                if (!result.HasValue())
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

            void IceoryxProxyMethodBinding::pollLoop() noexcept
            {
                static constexpr std::chrono::milliseconds cPollTimeout{50};

                while (mRunning.load(std::memory_order_relaxed))
                {
                    if (!mRepSubscriber || !mRepSubscriber->IsBindingActive())
                    {
                        std::this_thread::sleep_for(cPollTimeout);
                        continue;
                    }

                    if (!mRepSubscriber->WaitForData(cPollTimeout))
                    {
                        continue;
                    }

                    while (true)
                    {
                        zerocopy::ReceivedSample sample;
                        const auto taken = mRepSubscriber->TryTake(sample);
                        if (!taken.HasValue() || !taken.Value())
                        {
                            break;
                        }

                        const auto *data = sample.Data();
                        const auto size = sample.Size();

                        // Framing: [4-byte session_id] [4-byte is_error] [reply bytes]
                        std::uint32_t sessionId = 0U;
                        std::uint32_t isError = 0U;
                        if (!decodeU32LE(data, size, 0U, sessionId) ||
                            !decodeU32LE(data, size, 4U, isError))
                        {
                            continue;
                        }

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

                        if (!handler)
                        {
                            continue;
                        }

                        if (isError != 0U)
                        {
                            handler(core::Result<std::vector<std::uint8_t>>::FromError(
                                MakeErrorCode(ComErrc::kCommunicationStackError)));
                        }
                        else
                        {
                            const std::size_t headerSize = 8U; // session_id + is_error
                            const std::vector<std::uint8_t> replyPayload(
                                data + headerSize,
                                data + size);
                            handler(core::Result<std::vector<std::uint8_t>>::FromValue(
                                replyPayload));
                        }
                    }
                }
            }

            // ── IceoryxSkeletonMethodBinding ──────────────────────────────────

            IceoryxSkeletonMethodBinding::IceoryxSkeletonMethodBinding(
                MethodBindingConfig config) noexcept
                : mConfig{config}
            {
            }

            IceoryxSkeletonMethodBinding::~IceoryxSkeletonMethodBinding() noexcept
            {
                Unregister();
            }

            core::Result<void> IceoryxSkeletonMethodBinding::Register(
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

                if (!mReqSubscriber)
                {
                    mReqSubscriber.reset(new zerocopy::ZeroCopySubscriber{
                        IceoryxProxyMethodBinding::makeRequestChannel(mConfig),
                        "ara_com_skeleton_method",
                        64U});
                }

                if (!mRepPublisher)
                {
                    mRepPublisher.reset(new zerocopy::ZeroCopyPublisher{
                        IceoryxProxyMethodBinding::makeReplyChannel(mConfig),
                        "ara_com_skeleton_method"});
                }

                if (!mServiceThread.joinable())
                {
                    mRunning.store(true, std::memory_order_relaxed);
                    mServiceThread = std::thread([this] { serviceLoop(); });
                }

                return core::Result<void>::FromValue();
            }

            void IceoryxSkeletonMethodBinding::Unregister()
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

                mRepPublisher.reset();
                mReqSubscriber.reset();
            }

            void IceoryxSkeletonMethodBinding::serviceLoop() noexcept
            {
                static constexpr std::chrono::milliseconds cPollTimeout{50};

                while (mRunning.load(std::memory_order_relaxed))
                {
                    if (!mReqSubscriber || !mReqSubscriber->IsBindingActive())
                    {
                        std::this_thread::sleep_for(cPollTimeout);
                        continue;
                    }

                    if (!mReqSubscriber->WaitForData(cPollTimeout))
                    {
                        continue;
                    }

                    while (true)
                    {
                        zerocopy::ReceivedSample sample;
                        const auto taken = mReqSubscriber->TryTake(sample);
                        if (!taken.HasValue() || !taken.Value())
                        {
                            break;
                        }

                        const auto *data = sample.Data();
                        const auto size = sample.Size();

                        // Framing: [4-byte session_id] [request bytes]
                        std::uint32_t sessionId = 0U;
                        if (!decodeU32LE(data, size, 0U, sessionId))
                        {
                            continue;
                        }

                        const std::size_t headerSize = 4U;
                        const std::vector<std::uint8_t> reqPayload(
                            data + headerSize, data + size);

                        RawRequestHandler handler;
                        {
                            std::lock_guard<std::mutex> lock(mMutex);
                            handler = mHandler;
                        }

                        // Build reply frame: [4-byte session_id] [4-byte is_error] [reply bytes]
                        std::vector<std::uint8_t> replyFrame;
                        encodeU32LE(sessionId, replyFrame);

                        if (handler)
                        {
                            auto result = handler(reqPayload);
                            if (result.HasValue())
                            {
                                encodeU32LE(0U, replyFrame); // is_error = 0
                                const auto &respPayload = result.Value();
                                replyFrame.insert(replyFrame.end(),
                                                  respPayload.begin(),
                                                  respPayload.end());
                            }
                            else
                            {
                                encodeU32LE(1U, replyFrame); // is_error = 1
                            }
                        }
                        else
                        {
                            encodeU32LE(1U, replyFrame); // is_error = 1 (no handler)
                        }

                        if (mRepPublisher)
                        {
                            mRepPublisher->PublishCopy(replyFrame);
                        }
                    }
                }
            }
        }
    }
}

#endif // ARA_COM_USE_ICEORYX
