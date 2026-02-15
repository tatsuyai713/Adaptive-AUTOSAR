#include "./process_watchdog.h"

namespace ara
{
    namespace exec
    {
        namespace helper
        {
            ProcessWatchdog::ProcessWatchdog(
                const std::string &processName,
                std::chrono::milliseconds timeout,
                ExpiryCallback callback)
                : mProcessName{processName},
                  mTimeout{timeout},
                  mExpiryCallback{std::move(callback)},
                  mRunning{false},
                  mExpired{false}
            {
            }

            ProcessWatchdog::~ProcessWatchdog() noexcept
            {
                Stop();
            }

            void ProcessWatchdog::Start()
            {
                bool expected = false;
                if (!mRunning.compare_exchange_strong(expected, true))
                {
                    return; // Already running
                }

                mExpired.store(false);
                {
                    std::lock_guard<std::mutex> lock{mMutex};
                    mLastAlive = std::chrono::steady_clock::now();
                }

                mWatchThread = std::thread(&ProcessWatchdog::WatchLoop, this);
            }

            void ProcessWatchdog::Stop()
            {
                bool expected = true;
                if (!mRunning.compare_exchange_strong(expected, false))
                {
                    return; // Not running
                }

                mCondition.notify_all();

                if (mWatchThread.joinable())
                {
                    mWatchThread.join();
                }
            }

            void ProcessWatchdog::ReportAlive()
            {
                if (!mRunning.load())
                {
                    return;
                }

                {
                    std::lock_guard<std::mutex> lock{mMutex};
                    mLastAlive = std::chrono::steady_clock::now();
                }
                mCondition.notify_all();
            }

            bool ProcessWatchdog::IsRunning() const noexcept
            {
                return mRunning.load();
            }

            bool ProcessWatchdog::IsExpired() const noexcept
            {
                return mExpired.load();
            }

            std::string ProcessWatchdog::GetProcessName() const
            {
                return mProcessName;
            }

            std::chrono::milliseconds ProcessWatchdog::GetTimeout() const noexcept
            {
                return mTimeout;
            }

            void ProcessWatchdog::WatchLoop()
            {
                while (mRunning.load())
                {
                    std::unique_lock<std::mutex> lock{mMutex};
                    auto deadline = mLastAlive + mTimeout;

                    mCondition.wait_until(lock, deadline, [this]
                                          { return !mRunning.load(); });

                    if (!mRunning.load())
                    {
                        break;
                    }

                    auto now = std::chrono::steady_clock::now();
                    if (now - mLastAlive >= mTimeout)
                    {
                        mExpired.store(true);

                        ExpiryCallback callback = mExpiryCallback;
                        std::string name = mProcessName;
                        lock.unlock();

                        if (callback)
                        {
                            callback(name);
                        }

                        break;
                    }
                }
            }
        }
    }
}
