/// @file src/ara/phm/alive_supervision.h
/// @brief ara::phm::AliveSupervision — Alive Supervision for PHM.
/// @details Monitors periodic alive checkpoints. A supervised entity must call
///          ReportCheckpoint() within every supervision window. If the call is
///          missing or occurs too frequently, the supervision state transitions
///          to FAILED, triggering a recovery action.
///
///          Supervision states (SWS_PHM §7.4.3):
///          - kDeactivated: supervision not started
///          - kOk: alive checkpoint received within window
///          - kFailed: alive missed or period violated
///          - kExpired: failed state persisted across multiple cycles
///
///          Reference: AUTOSAR_SWS_PlatformHealthManagement §7.4.3
///
///          This file is part of the Adaptive AUTOSAR educational implementation.

#ifndef ARA_PHM_ALIVE_SUPERVISION_H
#define ARA_PHM_ALIVE_SUPERVISION_H

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
        /// @brief Alive Supervision states (SWS_PHM §7.4.3).
        enum class AliveSupervisionStatus : uint8_t
        {
            kDeactivated = 0, ///< Supervision not started or explicitly stopped
            kOk = 1,          ///< Alive checkpoints are received within expected window
            kFailed = 2,      ///< Alive missed or period violated in current window
            kExpired = 3      ///< kFailed persisted for too many consecutive windows
        };

        /// @brief Configuration for alive supervision.
        struct AliveSupervisionConfig
        {
            /// @brief Expected alive period in milliseconds.
            ///        The entity should call ReportCheckpoint() once per period.
            uint32_t alivePeriodMs{1000};

            /// @brief Allowed minimum factor of the alive period.
            ///        Valid: alivePeriodMs * minMargin ≤ actual_period.
            float minMargin{0.5f};

            /// @brief Allowed maximum factor of the alive period.
            ///        Valid: actual_period ≤ alivePeriodMs * maxMargin.
            float maxMargin{2.0f};

            /// @brief Number of consecutive failed windows before kExpired.
            uint32_t failedThreshold{3};

            /// @brief Number of consecutive passed windows to recover from kFailed → kOk.
            uint32_t passedThreshold{1};
        };

        /// @brief Alive Supervision monitor (SWS_PHM §7.4.3).
        /// @details Runs a background timer thread that checks whether the supervised
        ///          entity has called ReportCheckpoint() within the expected window.
        ///
        ///          Usage (from supervised entity context):
        ///          @code
        ///          AliveSupervisionConfig cfg;
        ///          cfg.alivePeriodMs = 500;   // expect call every 500ms
        ///          cfg.failedThreshold = 3;
        ///
        ///          AliveSupervision supervision(cfg);
        ///          supervision.SetStatusCallback([](AliveSupervisionStatus s) {
        ///              if (s == AliveSupervisionStatus::kExpired) { restart(); }
        ///          });
        ///          supervision.Start();
        ///
        ///          // In the entity's periodic task:
        ///          supervision.ReportCheckpoint();
        ///          @endcode
        class AliveSupervision
        {
        public:
            /// @brief Callback invoked when supervision status changes.
            using StatusCallback = std::function<void(AliveSupervisionStatus)>;

            /// @brief Construct with configuration.
            explicit AliveSupervision(const AliveSupervisionConfig &config);

            ~AliveSupervision();

            // Not copyable (owns mutex and thread)
            AliveSupervision(const AliveSupervision &) = delete;
            AliveSupervision &operator=(const AliveSupervision &) = delete;

            /// @brief Start the alive supervision monitor.
            void Start();

            /// @brief Stop the alive supervision monitor.
            void Stop();

            /// @brief Report an alive checkpoint from the supervised entity.
            /// @details Call this periodically (once per alive period).
            ///          Thread-safe; may be called from any thread.
            void ReportCheckpoint();

            /// @brief Get the current supervision status.
            AliveSupervisionStatus GetStatus() const noexcept;

            /// @brief Register a callback for status changes.
            void SetStatusCallback(StatusCallback callback);

        private:
            AliveSupervisionConfig mConfig;
            mutable std::mutex mMutex;

            AliveSupervisionStatus mStatus{AliveSupervisionStatus::kDeactivated};
            StatusCallback mStatusCallback;

            std::chrono::steady_clock::time_point mLastCheckpointTime;
            std::atomic<bool> mCheckpointReported{false};

            uint32_t mFailedCount{0};
            uint32_t mPassedCount{0};

            std::thread mMonitorThread;
            std::atomic<bool> mRunning{false};

            void monitorLoop();
            void updateStatus(AliveSupervisionStatus newStatus);
        };

    } // namespace phm
} // namespace ara

#endif // ARA_PHM_ALIVE_SUPERVISION_H
