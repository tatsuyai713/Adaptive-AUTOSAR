/// @file src/ara/log/sink/async_log_sink.h
/// @brief Asynchronous log sink with bounded ring buffer.
/// @details This file is part of the Adaptive AUTOSAR educational implementation.
///
/// `AsyncLogSink` wraps any other `LogSink` and decouples the calling thread
/// from the I/O operation. Log entries are pushed into a fixed-size ring
/// buffer; a background flush thread drains the buffer and calls the inner
/// sink. When the buffer is full, the oldest entry is overwritten (lossy)
/// so that callers are never blocked.

#ifndef ASYNC_LOG_SINK_H
#define ASYNC_LOG_SINK_H

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

#include "./log_sink.h"

namespace ara
{
    namespace log
    {
        namespace sink
        {
            /// @brief Asynchronous (non-blocking) log sink backed by a ring buffer.
            ///
            /// ### Thread safety
            /// `Log()` is safe to call from multiple threads simultaneously.
            ///
            /// ### Drop policy
            /// When the ring buffer is full, the oldest unread slot is silently
            /// overwritten. The overwrite count can be retrieved via
            /// `GetDropCount()`.
            ///
            /// ### Shutdown
            /// The background flush thread stops and drains the remaining queue
            /// when the `AsyncLogSink` is destroyed.
            class AsyncLogSink final : public LogSink
            {
            public:
                /// @brief Construct an async sink wrapping an inner sink.
                ///
                /// @param innerSink      Underlying sink (must outlive this object).
                /// @param appId          Application ID (passed to LogSink base).
                /// @param appDescription Application description.
                /// @param capacity       Ring buffer capacity (number of log entries).
                AsyncLogSink(LogSink *innerSink,
                             std::string appId,
                             std::string appDescription,
                             std::size_t capacity = 256U);

                ~AsyncLogSink() noexcept override;

                /// @brief Push a log entry into the ring buffer (non-blocking).
                void Log(const LogStream &logStream) const override;

                /// @brief Number of entries overwritten due to buffer overflow.
                std::uint64_t GetDropCount() const noexcept;

                /// @brief Number of entries currently waiting in the ring buffer.
                std::size_t GetPendingCount() const noexcept;

                /// @brief Flush all pending entries synchronously.
                ///
                /// Blocks until the background thread has drained the buffer.
                void Flush();

            private:
                LogSink *mInnerSink;
                const std::size_t mCapacity;

                // Ring buffer stored as vector of optional log messages.
                // We copy the LogStream payload string for simplicity.
                struct Entry
                {
                    bool valid{false};
                    std::string message;
                };

                mutable std::vector<Entry> mBuffer;
                mutable std::size_t mHead{0U}; ///< Next write position.
                mutable std::size_t mTail{0U}; ///< Next read position.
                mutable std::size_t mSize{0U}; ///< Current number of entries.

                mutable std::mutex mMutex;
                mutable std::condition_variable mNotEmpty;

                mutable std::atomic<std::uint64_t> mDropCount{0U};

                std::atomic<bool> mRunning{true};
                std::thread mFlushThread;

                void flushLoop();
            };

        } // namespace sink
    } // namespace log
} // namespace ara

#endif
