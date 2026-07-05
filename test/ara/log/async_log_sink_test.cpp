#include <gtest/gtest.h>
#include <memory>
#include <string>
#include <thread>
#include <condition_variable>
#include "../../../src/ara/log/sink/async_log_sink.h"
#include "../../../src/ara/log/sink/console_log_sink.h"
#include "../../../src/ara/log/log_stream.h"

namespace ara
{
    namespace log
    {
        namespace sink
        {
            // A simple counting sink for testing
            class CountingSink : public LogSink
            {
            public:
                CountingSink()
                    : LogSink{"CNT", "CountingSink"}
                {
                }

                void Log(const LogStream &logStream) const override
                {
                    ++mCount;
                    (void)logStream;
                }

                mutable std::atomic<int> mCount{0};
            };

            class BlockingSink : public LogSink
            {
            public:
                BlockingSink() : LogSink{"BLK", "BlockingSink"}
                {
                }

                void Log(const LogStream &logStream) const override
                {
                    {
                        std::lock_guard<std::mutex> lock{mMutex};
                        mEntered = true;
                    }
                    mEnteredCv.notify_all();

                    std::unique_lock<std::mutex> lock{mMutex};
                    mReleaseCv.wait(lock, [this] { return mReleased; });
                    ++mCount;
                    (void)logStream;
                }

                void WaitUntilEntered() const
                {
                    std::unique_lock<std::mutex> lock{mMutex};
                    mEnteredCv.wait(lock, [this] { return mEntered; });
                }

                void Release() const
                {
                    {
                        std::lock_guard<std::mutex> lock{mMutex};
                        mReleased = true;
                    }
                    mReleaseCv.notify_all();
                }

                mutable std::mutex mMutex;
                mutable std::condition_variable mEnteredCv;
                mutable std::condition_variable mReleaseCv;
                mutable bool mEntered{false};
                mutable bool mReleased{false};
                mutable std::atomic<int> mCount{0};
            };

            TEST(AsyncLogSinkTest, ConstructAndDestroy)
            {
                CountingSink _inner;
                {
                    AsyncLogSink _async{&_inner, "TST", "Test", 16U};
                    // Should construct and destroy cleanly
                }
                // After destruction, no crash
            }

            TEST(AsyncLogSinkTest, LogEntriesAreFlushed)
            {
                CountingSink _inner;
                AsyncLogSink _async{&_inner, "TST", "Test", 64U};

                LogStream _stream;
                _stream << "hello";
                _async.Log(_stream);

                LogStream _stream2;
                _stream2 << "world";
                _async.Log(_stream2);

                _async.Flush();

                EXPECT_EQ(_inner.mCount.load(), 2);
            }

            TEST(AsyncLogSinkTest, DropCountOnOverflow)
            {
                CountingSink _inner;
                // Very small buffer: capacity = 2
                AsyncLogSink _async{&_inner, "TST", "Test", 2U};

                // Flood with messages (some may be dropped)
                for (int i = 0; i < 100; ++i)
                {
                    LogStream _stream;
                    _stream << "msg " << i;
                    _async.Log(_stream);
                }

                _async.Flush();

                // At least some entries should have been logged
                EXPECT_GT(_inner.mCount.load(), 0);

                // With capacity=2 and 100 messages, drops should have occurred
                // (may vary due to timing)
                auto totalHandled =
                    static_cast<std::uint64_t>(_inner.mCount.load()) +
                    _async.GetDropCount();
                EXPECT_EQ(totalHandled, 100U);
            }

            TEST(AsyncLogSinkTest, PendingCountIsZeroAfterFlush)
            {
                CountingSink _inner;
                AsyncLogSink _async{&_inner, "TST", "Test", 64U};

                LogStream _stream;
                _stream << "test";
                _async.Log(_stream);

                _async.Flush();

                EXPECT_EQ(_async.GetPendingCount(), 0U);
            }

            TEST(AsyncLogSinkTest, FlushWaitsForInnerSinkCompletion)
            {
                BlockingSink _inner;
                AsyncLogSink _async{&_inner, "TST", "Test", 64U};

                LogStream _stream;
                _stream << "blocked";
                _async.Log(_stream);

                _inner.WaitUntilEntered();

                std::atomic<bool> _flushReturned{false};
                std::thread _flushThread{[&_async, &_flushReturned] {
                    _async.Flush();
                    _flushReturned.store(true);
                }};

                std::this_thread::sleep_for(std::chrono::milliseconds(20));
                EXPECT_FALSE(_flushReturned.load());

                _inner.Release();
                _flushThread.join();
                EXPECT_TRUE(_flushReturned.load());
                EXPECT_EQ(_inner.mCount.load(), 1);
            }
        }
    }
}
