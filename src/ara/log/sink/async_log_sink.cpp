/// @file src/ara/log/sink/async_log_sink.cpp
/// @brief Implementation for AsyncLogSink.
/// @details This file is part of the Adaptive AUTOSAR educational implementation.

#include "./async_log_sink.h"

namespace ara
{
    namespace log
    {
        namespace sink
        {
            // -----------------------------------------------------------------------
            // Constructor / Destructor
            // -----------------------------------------------------------------------

            AsyncLogSink::AsyncLogSink(
                LogSink *innerSink,
                std::string appId,
                std::string appDescription,
                std::size_t capacity)
                : LogSink{std::move(appId), std::move(appDescription)}
                , mInnerSink{innerSink}
                , mCapacity{(capacity > 0U) ? capacity : 1U}
                , mBuffer(mCapacity)
            {
                mFlushThread = std::thread{[this] { flushLoop(); }};
            }

            AsyncLogSink::~AsyncLogSink() noexcept
            {
                mRunning.store(false, std::memory_order_relaxed);
                mNotEmpty.notify_all();
                if (mFlushThread.joinable())
                {
                    mFlushThread.join();
                }
            }

            // -----------------------------------------------------------------------
            // Log (producer side)
            // -----------------------------------------------------------------------

            void AsyncLogSink::Log(const LogStream &logStream) const
            {
                const std::string message{logStream.ToString()};

                {
                    std::unique_lock<std::mutex> lock{mMutex};

                    const bool full{mSize == mCapacity};
                    if (full)
                    {
                        // Overwrite oldest entry (lossy drop)
                        ++mDropCount;
                        mTail = (mTail + 1U) % mCapacity;
                        --mSize;
                    }

                    mBuffer[mHead].valid = true;
                    mBuffer[mHead].message = message;
                    mHead = (mHead + 1U) % mCapacity;
                    ++mSize;
                }
                mNotEmpty.notify_one();
            }

            // -----------------------------------------------------------------------
            // Flush thread (consumer side)
            // -----------------------------------------------------------------------

            void AsyncLogSink::flushLoop()
            {
                while (mRunning.load(std::memory_order_relaxed) || [this]
                {
                    std::unique_lock<std::mutex> lock{mMutex};
                    return mSize > 0U;
                }())
                {
                    std::string message;
                    bool hasEntry{false};

                    {
                        std::unique_lock<std::mutex> lock{mMutex};
                        mNotEmpty.wait(lock, [this]
                        {
                            return mSize > 0U ||
                                   !mRunning.load(std::memory_order_relaxed);
                        });

                        if (mSize > 0U)
                        {
                            message = std::move(mBuffer[mTail].message);
                            mBuffer[mTail].valid = false;
                            mTail = (mTail + 1U) % mCapacity;
                            --mSize;
                            hasEntry = true;
                        }

                        if (mSize == 0U)
                        {
                            mDrained.notify_all();
                        }
                    }

                    if (hasEntry && mInnerSink != nullptr)
                    {
                        // Re-create a LogStream from the stored string and forward it.
                        LogStream wrapper;
                        wrapper << message;
                        mInnerSink->Log(wrapper);
                    }
                }
            }

            // -----------------------------------------------------------------------
            // Diagnostics
            // -----------------------------------------------------------------------

            std::uint64_t AsyncLogSink::GetDropCount() const noexcept
            {
                return mDropCount.load(std::memory_order_relaxed);
            }

            std::size_t AsyncLogSink::GetPendingCount() const noexcept
            {
                std::unique_lock<std::mutex> lock{mMutex};
                return mSize;
            }

            void AsyncLogSink::Flush()
            {
                std::unique_lock<std::mutex> lock{mMutex};
                if (mSize == 0U)
                {
                    return;
                }
                // Wake the flush thread in case it is sleeping.
                // notify_all is called while holding the lock; the flush thread
                // will re-acquire after mDrained.wait() releases it below.
                mNotEmpty.notify_all();
                // Block until the background thread has drained the buffer.
                mDrained.wait(lock, [this] { return mSize == 0U; });
            }

        } // namespace sink
    } // namespace log
} // namespace ara
