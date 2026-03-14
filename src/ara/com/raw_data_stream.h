/// @file src/ara/com/raw_data_stream.h
/// @brief Raw data streaming interface per AUTOSAR AP SWS_CM_00600.
/// @details Provides byte-level streaming for large data transfers that
///          don't fit the Event/Method/Field pattern. Supports chunked
///          send/receive with flow control and configurable buffer sizes.

#ifndef ARA_COM_RAW_DATA_STREAM_H
#define ARA_COM_RAW_DATA_STREAM_H

#include <atomic>
#include <cstdint>
#include <deque>
#include <functional>
#include <limits>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>
#include "./com_error_domain.h"
#include "../core/result.h"

namespace ara
{
    namespace com
    {
        /// @brief State of a raw data stream connection.
        enum class StreamState : std::uint8_t
        {
            kClosed = 0U,     ///< Stream is not connected.
            kOpen = 1U,       ///< Stream is connected and ready.
            kSuspended = 2U,  ///< Stream is temporarily suspended (flow control).
            kError = 3U       ///< Stream encountered an error.
        };

        /// @brief Unique session identifier for a raw data stream session
        ///        per SWS_CM_00610.
        using StreamSessionId = std::uint32_t;

        /// @brief Skeleton-side raw data stream server per SWS_CM_00601.
        ///        Receives data chunks from clients.
        class RawDataStreamServer
        {
        public:
            /// @brief Callback for received data chunks.
            using DataHandler =
                std::function<void(const std::uint8_t *, std::size_t)>;

            /// @brief Callback for stream state changes.
            using StateHandler = std::function<void(StreamState)>;

        private:
            StreamState mState{StreamState::kClosed};
            DataHandler mDataHandler;
            StateHandler mStateHandler;
            std::size_t mMaxBufferSize;
            std::deque<std::vector<std::uint8_t>> mReceiveBuffer;
            mutable std::mutex mMutex;
            std::size_t mTotalBytesReceived{0U};

        public:
            /// @brief Constructs a raw data stream server.
            /// @param maxBufferSize Maximum bytes to buffer before backpressure.
            explicit RawDataStreamServer(
                std::size_t maxBufferSize = 1024U * 1024U) noexcept
                : mMaxBufferSize{maxBufferSize}
            {
            }

            RawDataStreamServer(const RawDataStreamServer &) = delete;
            RawDataStreamServer &operator=(const RawDataStreamServer &) = delete;

            /// @brief Open the stream to accept incoming data.
            /// @returns Result indicating success.
            core::Result<void> Open()
            {
                std::lock_guard<std::mutex> lock(mMutex);
                mState = StreamState::kOpen;
                mTotalBytesReceived = 0U;
                mReceiveBuffer.clear();
                NotifyStateChange();
                return core::Result<void>::FromValue();
            }

            /// @brief Close the stream.
            void Close()
            {
                std::lock_guard<std::mutex> lock(mMutex);
                mState = StreamState::kClosed;
                mReceiveBuffer.clear();
                NotifyStateChange();
            }

            /// @brief Get current stream state.
            StreamState GetState() const noexcept
            {
                std::lock_guard<std::mutex> lock(mMutex);
                return mState;
            }

            /// @brief Set handler for incoming data chunks.
            /// @param handler Callback receiving raw byte pointer and size.
            void SetDataHandler(DataHandler handler)
            {
                std::lock_guard<std::mutex> lock(mMutex);
                mDataHandler = std::move(handler);
            }

            /// @brief Set handler for stream state changes.
            /// @param handler Callback receiving new stream state.
            void SetStateHandler(StateHandler handler)
            {
                std::lock_guard<std::mutex> lock(mMutex);
                mStateHandler = std::move(handler);
            }

            /// @brief Receive data into the stream (called by transport).
            /// @param data Pointer to received bytes.
            /// @param size Number of bytes.
            /// @returns Error if stream is not open or buffer is full.
            core::Result<void> OnDataReceived(
                const std::uint8_t *data, std::size_t size)
            {
                std::lock_guard<std::mutex> lock(mMutex);
                if (mState != StreamState::kOpen)
                {
                    return core::Result<void>::FromError(
                        MakeErrorCode(ComErrc::kServiceNotAvailable));
                }

                std::size_t currentSize = 0U;
                for (const auto &chunk : mReceiveBuffer)
                {
                    currentSize += chunk.size();
                }

                if (currentSize + size > mMaxBufferSize)
                {
                    mState = StreamState::kSuspended;
                    NotifyStateChange();
                    return core::Result<void>::FromError(
                        MakeErrorCode(ComErrc::kMaxSamplesExceeded));
                }

                mReceiveBuffer.emplace_back(data, data + size);
                mTotalBytesReceived += size;

                if (mDataHandler)
                {
                    mDataHandler(data, size);
                }

                return core::Result<void>::FromValue();
            }

            /// @brief Read buffered data, consuming it from the internal queue.
            /// @param maxBytes Maximum bytes to consume.
            /// @returns Consumed data bytes.
            core::Result<std::vector<std::uint8_t>> Read(
                std::size_t maxBytes = std::numeric_limits<std::size_t>::max())
            {
                std::lock_guard<std::mutex> lock(mMutex);
                std::vector<std::uint8_t> result;

                while (!mReceiveBuffer.empty() &&
                       result.size() < maxBytes)
                {
                    auto &front = mReceiveBuffer.front();
                    std::size_t remaining = maxBytes - result.size();

                    if (front.size() <= remaining)
                    {
                        result.insert(result.end(), front.begin(), front.end());
                        mReceiveBuffer.pop_front();
                    }
                    else
                    {
                        result.insert(result.end(),
                                      front.begin(),
                                      front.begin() +
                                          static_cast<std::ptrdiff_t>(remaining));
                        front.erase(front.begin(),
                                    front.begin() +
                                        static_cast<std::ptrdiff_t>(remaining));
                    }
                }

                if (mState == StreamState::kSuspended && mReceiveBuffer.empty())
                {
                    mState = StreamState::kOpen;
                    NotifyStateChange();
                }

                return core::Result<std::vector<std::uint8_t>>::FromValue(
                    std::move(result));
            }

            /// @brief Get total bytes received since the stream was opened.
            std::size_t GetTotalBytesReceived() const noexcept
            {
                std::lock_guard<std::mutex> lock(mMutex);
                return mTotalBytesReceived;
            }

            /// @brief Get number of bytes currently buffered.
            std::size_t GetBufferedSize() const noexcept
            {
                std::lock_guard<std::mutex> lock(mMutex);
                std::size_t total = 0U;
                for (const auto &chunk : mReceiveBuffer)
                {
                    total += chunk.size();
                }
                return total;
            }

            /// @brief Accept a new client session (SWS_CM_00610).
            /// @returns Session ID assigned to the new client.
            core::Result<StreamSessionId> AcceptSession()
            {
                std::lock_guard<std::mutex> lock(mMutex);
                if (mState != StreamState::kOpen)
                {
                    return core::Result<StreamSessionId>::FromError(
                        MakeErrorCode(ComErrc::kServiceNotAvailable));
                }
                StreamSessionId id = mNextSessionId++;
                mActiveSessions[id] = true;
                return core::Result<StreamSessionId>::FromValue(id);
            }

            /// @brief Close a client session (SWS_CM_00611).
            /// @param sessionId Session to close.
            void CloseSession(StreamSessionId sessionId)
            {
                std::lock_guard<std::mutex> lock(mMutex);
                mActiveSessions.erase(sessionId);
            }

            /// @brief Check if a session is active.
            /// @param sessionId Session to check.
            /// @returns true if the session is active.
            bool IsSessionActive(StreamSessionId sessionId) const
            {
                std::lock_guard<std::mutex> lock(mMutex);
                auto it = mActiveSessions.find(sessionId);
                return it != mActiveSessions.end() && it->second;
            }

            /// @brief Get number of active sessions.
            std::size_t GetActiveSessionCount() const
            {
                std::lock_guard<std::mutex> lock(mMutex);
                return mActiveSessions.size();
            }

        private:
            void NotifyStateChange()
            {
                if (mStateHandler)
                {
                    mStateHandler(mState);
                }
            }

            StreamSessionId mNextSessionId{1U};
            std::unordered_map<StreamSessionId, bool> mActiveSessions;
        };

        /// @brief Proxy-side raw data stream client per SWS_CM_00602.
        ///        Sends data chunks to a server.
        class RawDataStreamClient
        {
        public:
            using StateHandler = std::function<void(StreamState)>;

        private:
            StreamState mState{StreamState::kClosed};
            RawDataStreamServer *mServer{nullptr};
            StateHandler mStateHandler;
            mutable std::mutex mMutex;
            std::size_t mTotalBytesSent{0U};
            StreamSessionId mSessionId{0U};

        public:
            RawDataStreamClient() = default;

            RawDataStreamClient(const RawDataStreamClient &) = delete;
            RawDataStreamClient &operator=(const RawDataStreamClient &) = delete;

            /// @brief Connect to a stream server and obtain a session.
            /// @param server Target stream server to send data to.
            /// @returns Result indicating success.
            core::Result<void> Connect(RawDataStreamServer &server)
            {
                std::lock_guard<std::mutex> lock(mMutex);
                mServer = &server;
                auto sessionResult = server.AcceptSession();
                if (sessionResult.HasValue())
                {
                    mSessionId = sessionResult.Value();
                }
                mState = StreamState::kOpen;
                mTotalBytesSent = 0U;
                NotifyStateChange();
                return core::Result<void>::FromValue();
            }

            /// @brief Disconnect from the server and close session.
            void Disconnect()
            {
                std::lock_guard<std::mutex> lock(mMutex);
                if (mServer && mSessionId != 0U)
                {
                    mServer->CloseSession(mSessionId);
                    mSessionId = 0U;
                }
                mServer = nullptr;
                mState = StreamState::kClosed;
                NotifyStateChange();
            }

            /// @brief Get session ID assigned by the server.
            /// @returns Current session ID (0 if not connected).
            StreamSessionId GetSessionId() const noexcept
            {
                std::lock_guard<std::mutex> lock(mMutex);
                return mSessionId;
            }

            /// @brief Get current stream state.
            StreamState GetState() const noexcept
            {
                std::lock_guard<std::mutex> lock(mMutex);
                return mState;
            }

            /// @brief Set handler for stream state changes.
            void SetStateHandler(StateHandler handler)
            {
                std::lock_guard<std::mutex> lock(mMutex);
                mStateHandler = std::move(handler);
            }

            /// @brief Write data to the stream.
            /// @param data Pointer to bytes to send.
            /// @param size Number of bytes.
            /// @returns Result indicating success or backpressure error.
            core::Result<void> Write(const std::uint8_t *data, std::size_t size)
            {
                std::lock_guard<std::mutex> lock(mMutex);
                if (mState != StreamState::kOpen || !mServer)
                {
                    return core::Result<void>::FromError(
                        MakeErrorCode(ComErrc::kServiceNotAvailable));
                }

                auto result = mServer->OnDataReceived(data, size);
                if (result.HasValue())
                {
                    mTotalBytesSent += size;
                }
                else
                {
                    mState = StreamState::kSuspended;
                    NotifyStateChange();
                }
                return result;
            }

            /// @brief Write a vector of bytes to the stream.
            /// @param data Bytes to send.
            /// @returns Result indicating success or error.
            core::Result<void> Write(const std::vector<std::uint8_t> &data)
            {
                return Write(data.data(), data.size());
            }

            /// @brief Get total bytes sent.
            std::size_t GetTotalBytesSent() const noexcept
            {
                std::lock_guard<std::mutex> lock(mMutex);
                return mTotalBytesSent;
            }

        private:
            void NotifyStateChange()
            {
                if (mStateHandler)
                {
                    mStateHandler(mState);
                }
            }
        };
    }
}

#endif
