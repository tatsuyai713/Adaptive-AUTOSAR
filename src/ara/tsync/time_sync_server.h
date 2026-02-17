/// @file src/ara/tsync/time_sync_server.h
/// @brief ara::tsync::TimeSyncServer — server-side time base distributor.
/// @details The TimeSyncServer manages one or more time base sources
///          (SynchronizedTimeBaseProvider implementations) and periodically
///          publishes synchronized time to registered consumers (TimeSyncClient
///          instances). This mirrors the AUTOSAR AP SWS_TS §7.4 Time Base
///          Provider/Server role.
///
///          Responsibilities:
///          1. Poll the configured SynchronizedTimeBaseProvider at a configurable
///             interval (default 100 ms).
///          2. On successful poll, propagate the new time reference to all
///             registered TimeSyncClient consumers.
///          3. Notify registered status listeners when the provider becomes
///             available or unavailable.
///          4. Provide the authoritative GetCurrentTime() for the local node.
///
///          Usage:
///          @code
///          PtpTimeBaseProvider ptpProvider("/dev/ptp0");
///          TimeSyncServer server(ptpProvider);
///          server.SetPollIntervalMs(50);
///          server.Start();
///
///          // Register a consumer:
///          TimeSyncClient consumer;
///          server.RegisterConsumer(consumer);
///
///          // Later:
///          server.Stop();
///          @endcode
///
///          Reference: AUTOSAR SWS_TimeSynchronization §7.4
///          This file is part of the Adaptive AUTOSAR educational implementation.

#ifndef ARA_TSYNC_TIME_SYNC_SERVER_H
#define ARA_TSYNC_TIME_SYNC_SERVER_H

#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>
#include <mutex>
#include <thread>
#include <vector>
#include "../core/result.h"
#include "./synchronized_time_base_provider.h"
#include "./time_sync_client.h"
#include "./tsync_error_domain.h"

namespace ara
{
    namespace tsync
    {
        /// @brief Configuration for the TimeSyncServer.
        struct TimeSyncServerConfig
        {
            /// @brief Provider polling interval in milliseconds.
            uint32_t pollIntervalMs{100};

            /// @brief Maximum consecutive provider failures before declaring
            ///        the time base as unavailable (resets all consumers).
            uint32_t maxFailureCount{5};
        };

        /// @brief Server-side time base distributor (SWS_TS §7.4).
        /// @details Runs a background thread that periodically polls the configured
        ///          SynchronizedTimeBaseProvider and propagates time references to
        ///          all registered TimeSyncClient consumers.
        class TimeSyncServer
        {
        public:
            /// @brief Callback type for provider availability changes.
            /// @param available True when provider becomes available, false when lost.
            using AvailabilityCallback = std::function<void(bool available)>;

            /// @brief Construct with a time base provider and optional config.
            /// @param provider  Reference to the time source implementation.
            /// @param config    Server configuration (optional).
            explicit TimeSyncServer(
                SynchronizedTimeBaseProvider &provider,
                const TimeSyncServerConfig &config = {}) noexcept;

            ~TimeSyncServer();

            // Non-copyable (owns mutex and thread)
            TimeSyncServer(const TimeSyncServer &) = delete;
            TimeSyncServer &operator=(const TimeSyncServer &) = delete;

            /// @brief Start the background polling thread.
            /// @returns Void Result; error if already running.
            core::Result<void> Start();

            /// @brief Stop the background polling thread.
            void Stop();

            /// @brief Register a TimeSyncClient as a consumer of this time base.
            /// @param consumer Reference to the client to keep synchronized.
            ///        The caller must ensure the client lifetime exceeds the server.
            core::Result<void> RegisterConsumer(TimeSyncClient &consumer);

            /// @brief Unregister a previously registered consumer.
            void UnregisterConsumer(TimeSyncClient &consumer);

            /// @brief Register a callback for provider availability changes.
            void SetAvailabilityCallback(AvailabilityCallback cb);

            /// @brief Get the current synchronized time (convenience accessor).
            /// @returns System clock time_point if synchronized, error otherwise.
            core::Result<std::chrono::system_clock::time_point> GetCurrentTime() const;

            /// @brief Check whether the provider is currently available.
            bool IsProviderAvailable() const noexcept;

            /// @brief Get the provider poll interval (milliseconds).
            uint32_t GetPollIntervalMs() const noexcept;

            /// @brief Set the provider poll interval (milliseconds, min 10).
            void SetPollIntervalMs(uint32_t ms) noexcept;

        private:
            SynchronizedTimeBaseProvider &mProvider;
            TimeSyncServerConfig mConfig;

            mutable std::mutex mMutex;
            std::vector<TimeSyncClient *> mConsumers;
            AvailabilityCallback mAvailabilityCallback;

            TimeSyncClient mInternalClient; ///< Internal client for GetCurrentTime()

            std::thread mPollThread;
            std::atomic<bool> mRunning{false};
            std::atomic<bool> mProviderAvailable{false};

            uint32_t mConsecutiveFailures{0};

            void pollLoop();
            void distributeToConsumers(
                std::chrono::system_clock::time_point globalTime,
                std::chrono::steady_clock::time_point steadyTime);
            void handleProviderLoss();
            void notifyAvailability(bool available);
        };

    } // namespace tsync
} // namespace ara

#endif // ARA_TSYNC_TIME_SYNC_SERVER_H
