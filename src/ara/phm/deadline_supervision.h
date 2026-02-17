/// @file src/ara/phm/deadline_supervision.h
/// @brief ara::phm::DeadlineSupervision — Deadline Supervision for PHM.
/// @details Monitors that a checkpoint is reached within a required time
///          window after a reference checkpoint (Start checkpoint).
///
///          Usage pattern:
///          1. Application calls ReportStart() to start the deadline window.
///          2. Application must call ReportEnd() within [minDeadline, maxDeadline] ms.
///          3. If ReportEnd() is not called within maxDeadline, status → kFailed.
///          4. If ReportEnd() is called before minDeadline, status → kFailed.
///          5. If consecutive failures ≥ failedThreshold, status → kExpired.
///
///          Reference: AUTOSAR_SWS_PlatformHealthManagement §7.4.4
///
///          This file is part of the Adaptive AUTOSAR educational implementation.

#ifndef ARA_PHM_DEADLINE_SUPERVISION_H
#define ARA_PHM_DEADLINE_SUPERVISION_H

#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>
#include <mutex>
#include <thread>
#include "../core/result.h"
#include "./phm_error_domain.h"

namespace ara
{
    namespace phm
    {
        /// @brief Deadline Supervision status (SWS_PHM §7.4.4).
        enum class DeadlineSupervisionStatus : uint8_t
        {
            kDeactivated = 0, ///< Supervision not started
            kOk = 1,          ///< Checkpoint reached within deadline window
            kFailed = 2,      ///< Deadline violated (missed or too early)
            kExpired = 3      ///< Consecutive failures exceeded threshold
        };

        /// @brief Configuration for deadline supervision.
        struct DeadlineSupervisionConfig
        {
            /// @brief Minimum time (ms) between Start and End checkpoints.
            /// @details End before minDeadline → kFailed.
            uint32_t minDeadlineMs{0};

            /// @brief Maximum time (ms) between Start and End checkpoints.
            /// @details End after maxDeadline → kFailed.
            uint32_t maxDeadlineMs{1000};

            /// @brief Consecutive failures before kExpired.
            uint32_t failedThreshold{3};

            /// @brief Consecutive passes to recover from kFailed → kOk.
            uint32_t passedThreshold{1};
        };

        /// @brief Deadline Supervision monitor (SWS_PHM §7.4.4).
        /// @details Thread-safe deadline monitor. Uses a background watcher thread
        ///          to detect expired deadlines.
        ///
        ///          Usage:
        ///          @code
        ///          DeadlineSupervisionConfig cfg;
        ///          cfg.minDeadlineMs = 10;
        ///          cfg.maxDeadlineMs = 500;
        ///          cfg.failedThreshold = 3;
        ///
        ///          DeadlineSupervision supervision(cfg);
        ///          supervision.SetStatusCallback([](DeadlineSupervisionStatus s) {
        ///              if (s == DeadlineSupervisionStatus::kExpired) restart();
        ///          });
        ///          supervision.Start();
        ///
        ///          // In the entity:
        ///          supervision.ReportStart();   // begin deadline window
        ///          doWork();                    // must complete within maxDeadline
        ///          supervision.ReportEnd();     // end deadline window
        ///          @endcode
        class DeadlineSupervision
        {
        public:
            using StatusCallback = std::function<void(DeadlineSupervisionStatus)>;

            explicit DeadlineSupervision(const DeadlineSupervisionConfig &config);
            ~DeadlineSupervision();

            DeadlineSupervision(const DeadlineSupervision &) = delete;
            DeadlineSupervision &operator=(const DeadlineSupervision &) = delete;

            /// @brief Start the supervision monitor.
            void Start();

            /// @brief Stop the supervision monitor.
            void Stop();

            /// @brief Report the start of a supervised operation.
            /// @details Opens the deadline window. Must be followed by ReportEnd().
            void ReportStart();

            /// @brief Report the end of a supervised operation.
            /// @details Closes the deadline window. The elapsed time since ReportStart()
            ///          must be in [minDeadlineMs, maxDeadlineMs].
            void ReportEnd();

            /// @brief Get the current supervision status.
            DeadlineSupervisionStatus GetStatus() const noexcept;

            /// @brief Register a callback for status changes.
            void SetStatusCallback(StatusCallback callback);

        private:
            DeadlineSupervisionConfig mConfig;
            mutable std::mutex mMutex;

            DeadlineSupervisionStatus mStatus{DeadlineSupervisionStatus::kDeactivated};
            StatusCallback mStatusCallback;

            bool mWindowOpen{false};
            std::chrono::steady_clock::time_point mStartTime;

            uint32_t mFailedCount{0};
            uint32_t mPassedCount{0};

            std::thread mWatcherThread;
            std::atomic<bool> mRunning{false};

            void watcherLoop();
            void recordResult(bool passed);
            void updateStatus(DeadlineSupervisionStatus newStatus);
        };

    } // namespace phm
} // namespace ara

#endif // ARA_PHM_DEADLINE_SUPERVISION_H
