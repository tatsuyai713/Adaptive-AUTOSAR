#ifndef PROCESS_WATCHDOG_H
#define PROCESS_WATCHDOG_H

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <string>
#include <thread>

namespace ara
{
    namespace exec
    {
        namespace helper
        {
            /// @brief Process liveness watchdog for alive supervision.
            /// Monitors that a process reports alive within a configurable timeout.
            class ProcessWatchdog
            {
            public:
                using ExpiryCallback =
                    std::function<void(const std::string &processName)>;

                /// @brief Constructor
                /// @param processName Name of the process to monitor
                /// @param timeout Maximum time between alive reports
                /// @param callback Optional callback invoked on expiry
                ProcessWatchdog(
                    const std::string &processName,
                    std::chrono::milliseconds timeout,
                    ExpiryCallback callback = nullptr);

                ~ProcessWatchdog() noexcept;

                ProcessWatchdog(const ProcessWatchdog &) = delete;
                ProcessWatchdog &operator=(const ProcessWatchdog &) = delete;

                /// @brief Start the watchdog monitoring thread
                void Start();

                /// @brief Stop the watchdog monitoring thread
                void Stop();

                /// @brief Report that the process is alive, resetting the timer
                void ReportAlive();

                /// @brief Check if the watchdog thread is running
                bool IsRunning() const noexcept;

                /// @brief Check if the watchdog has expired
                bool IsExpired() const noexcept;

                /// @brief Get the monitored process name
                std::string GetProcessName() const;

                /// @brief Get the configured timeout
                std::chrono::milliseconds GetTimeout() const noexcept;

            private:
                std::string mProcessName;
                std::chrono::milliseconds mTimeout;
                ExpiryCallback mExpiryCallback;

                std::atomic<bool> mRunning;
                std::atomic<bool> mExpired;
                mutable std::mutex mMutex;
                std::condition_variable mCondition;
                std::thread mWatchThread;
                std::chrono::steady_clock::time_point mLastAlive;

                void WatchLoop();
            };
        }
    }
}

#endif
