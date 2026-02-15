#ifndef PROCESS_WATCHDOG_H
#define PROCESS_WATCHDOG_H

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
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
            /// @note Helper extension used by this repository runtime; not an AUTOSAR AP standard class.
            class ProcessWatchdog
            {
            public:
                using ExpiryCallback =
                    std::function<void(const std::string &processName)>;

                /// @brief Runtime options for watchdog behavior.
                struct WatchdogOptions
                {
                    /// @brief Initial grace period added before first timeout check after start/reset.
                    std::chrono::milliseconds startupGrace{0};
                    /// @brief Minimum interval between expiry callbacks in continuous mode.
                    std::chrono::milliseconds expiryCallbackCooldown{0};
                    /// @brief Keep monitoring and continue reporting expiries instead of stopping at first expiry.
                    bool keepRunningOnExpiry{false};
                };

                /// @brief Constructor
                /// @param processName Name of the process to monitor
                /// @param timeout Maximum time between alive reports
                ProcessWatchdog(
                    const std::string &processName,
                    std::chrono::milliseconds timeout);

                /// @brief Constructor
                /// @param processName Name of the process to monitor
                /// @param timeout Maximum time between alive reports
                /// @param callback Optional callback invoked on expiry
                ProcessWatchdog(
                    const std::string &processName,
                    std::chrono::milliseconds timeout,
                    ExpiryCallback callback);

                /// @brief Constructor
                /// @param processName Name of the process to monitor
                /// @param timeout Maximum time between alive reports
                /// @param callback Optional callback invoked on expiry
                /// @param options Watchdog behavior options
                ProcessWatchdog(
                    const std::string &processName,
                    std::chrono::milliseconds timeout,
                    ExpiryCallback callback,
                    WatchdogOptions options);

                ~ProcessWatchdog() noexcept;

                ProcessWatchdog(const ProcessWatchdog &) = delete;
                ProcessWatchdog &operator=(const ProcessWatchdog &) = delete;

                /// @brief Start the watchdog monitoring thread
                void Start();

                /// @brief Stop the watchdog monitoring thread
                void Stop();

                /// @brief Report that the process is alive, resetting the timer
                void ReportAlive();

                /// @brief Reset the watchdog timer and expiry state.
                /// Applies startup grace before the next timeout check.
                void Reset();

                /// @brief Check if the watchdog thread is running
                bool IsRunning() const noexcept;

                /// @brief Check if the watchdog has expired
                bool IsExpired() const noexcept;

                /// @brief Number of expiry detections observed since the last Start.
                std::uint64_t GetExpiryCount() const noexcept;

                /// @brief Get the monitored process name
                std::string GetProcessName() const;

                /// @brief Get the configured timeout
                std::chrono::milliseconds GetTimeout() const noexcept;

                /// @brief Get configured watchdog options.
                WatchdogOptions GetOptions() const;

                /// @brief Update watchdog options at runtime.
                /// @note If the watchdog is running, the new options are applied on next cycle.
                void SetOptions(WatchdogOptions options);

            private:
                std::string mProcessName;
                std::chrono::milliseconds mTimeout;
                ExpiryCallback mExpiryCallback;
                WatchdogOptions mOptions;

                std::atomic<bool> mRunning;
                std::atomic<bool> mExpired;
                std::atomic<std::uint64_t> mExpiryCount;
                mutable std::mutex mMutex;
                std::condition_variable mCondition;
                std::thread mWatchThread;
                std::atomic<bool> mWatchLoopActive;
                std::chrono::steady_clock::time_point mLastAlive;
                std::chrono::steady_clock::time_point mLastExpiryCallback;
                bool mHasExpiryCallbackTimestamp;

                void WatchLoop();
            };
        }
    }
}

#endif
