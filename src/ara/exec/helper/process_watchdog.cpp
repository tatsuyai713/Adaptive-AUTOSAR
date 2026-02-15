#include "./process_watchdog.h"

namespace ara
{
    namespace exec
    {
        namespace helper
        {
            namespace
            {
                ProcessWatchdog::WatchdogOptions SanitizeOptions(
                    ProcessWatchdog::WatchdogOptions options) noexcept
                {
                    if (options.startupGrace.count() < 0)
                    {
                        options.startupGrace = std::chrono::milliseconds(0);
                    }
                    if (options.expiryCallbackCooldown.count() < 0)
                    {
                        options.expiryCallbackCooldown = std::chrono::milliseconds(0);
                    }
                    return options;
                }
            }

            ProcessWatchdog::ProcessWatchdog(
                const std::string &processName,
                std::chrono::milliseconds timeout,
                ExpiryCallback callback,
                WatchdogOptions options)
                : mProcessName{processName},
                  mTimeout{timeout},
                  mExpiryCallback{std::move(callback)},
                  mOptions{SanitizeOptions(options)},
                  mRunning{false},
                  mExpired{false},
                  mExpiryCount{0U},
                  mWatchLoopActive{false},
                  mHasExpiryCallbackTimestamp{false}
            {
            }

            ProcessWatchdog::ProcessWatchdog(
                const std::string &processName,
                std::chrono::milliseconds timeout)
                : ProcessWatchdog(
                      processName,
                      timeout,
                      nullptr,
                      WatchdogOptions{})
            {
            }

            ProcessWatchdog::ProcessWatchdog(
                const std::string &processName,
                std::chrono::milliseconds timeout,
                ExpiryCallback callback)
                : ProcessWatchdog(
                      processName,
                      timeout,
                      std::move(callback),
                      WatchdogOptions{})
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
                mExpiryCount.store(0U);
                {
                    std::lock_guard<std::mutex> lock{mMutex};
                    const auto now{std::chrono::steady_clock::now()};
                    mLastAlive = now + mOptions.startupGrace;
                    mLastExpiryCallback = now;
                    mHasExpiryCallbackTimestamp = false;
                }

                mWatchLoopActive.store(true);
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

            void ProcessWatchdog::Reset()
            {
                mExpired.store(false);

                const auto now{std::chrono::steady_clock::now()};
                {
                    std::lock_guard<std::mutex> lock{mMutex};
                    mLastAlive = now + mOptions.startupGrace;
                    mLastExpiryCallback = now;
                    mHasExpiryCallbackTimestamp = false;
                }
                mCondition.notify_all();

                if (mRunning.load() && !mWatchLoopActive.load())
                {
                    if (mWatchThread.joinable())
                    {
                        mWatchThread.join();
                    }

                    mWatchLoopActive.store(true);
                    mWatchThread = std::thread(&ProcessWatchdog::WatchLoop, this);
                }
            }

            bool ProcessWatchdog::IsRunning() const noexcept
            {
                return mRunning.load();
            }

            bool ProcessWatchdog::IsExpired() const noexcept
            {
                return mExpired.load();
            }

            std::uint64_t ProcessWatchdog::GetExpiryCount() const noexcept
            {
                return mExpiryCount.load();
            }

            std::string ProcessWatchdog::GetProcessName() const
            {
                return mProcessName;
            }

            std::chrono::milliseconds ProcessWatchdog::GetTimeout() const noexcept
            {
                return mTimeout;
            }

            ProcessWatchdog::WatchdogOptions ProcessWatchdog::GetOptions() const
            {
                std::lock_guard<std::mutex> lock{mMutex};
                return mOptions;
            }

            void ProcessWatchdog::SetOptions(WatchdogOptions options)
            {
                const WatchdogOptions sanitized{SanitizeOptions(options)};
                {
                    std::lock_guard<std::mutex> lock{mMutex};
                    mOptions = sanitized;
                }
                mCondition.notify_all();
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
                        mExpiryCount.fetch_add(1U);

                        ExpiryCallback callback = mExpiryCallback;
                        std::string name = mProcessName;
                        WatchdogOptions options = mOptions;
                        bool invokeCallback{true};

                        if (options.expiryCallbackCooldown.count() > 0)
                        {
                            if (mHasExpiryCallbackTimestamp &&
                                (now - mLastExpiryCallback) <
                                    options.expiryCallbackCooldown)
                            {
                                invokeCallback = false;
                            }
                        }

                        if (invokeCallback)
                        {
                            mLastExpiryCallback = now;
                            mHasExpiryCallbackTimestamp = true;
                        }

                        if (options.keepRunningOnExpiry)
                        {
                            mLastAlive = now;
                        }
                        lock.unlock();

                        if (invokeCallback && callback)
                        {
                            callback(name);
                        }

                        if (!options.keepRunningOnExpiry)
                        {
                            break;
                        }
                    }
                }

                mWatchLoopActive.store(false);
            }
        }
    }
}
