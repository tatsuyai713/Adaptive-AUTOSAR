/// @file src/ara/tsync/time_sync_client.h
/// @brief Declarations for time sync client.
/// @details This file is part of the Adaptive AUTOSAR educational implementation.

#ifndef TIME_SYNC_CLIENT_H
#define TIME_SYNC_CLIENT_H

#include <chrono>
#include <cstdint>
#include <deque>
#include <functional>
#include <mutex>
#include "../core/result.h"
#include "./tsync_error_domain.h"
#include "./time_base_status.h"

namespace ara
{
    namespace tsync
    {
        /// @brief Synchronization state for the local time base.
        enum class SynchronizationState : std::uint32_t
        {
            kUnsynchronized = 0,
            kSynchronized = 1
        };

        /// @brief Synchronization quality level based on estimated clock drift.
        enum class SyncQualityLevel : std::uint32_t
        {
            kGood = 0,     ///< Estimated drift is within the configured threshold.
            kDegraded = 1, ///< Estimated drift exceeds the configured threshold.
            kLost = 2      ///< No samples received within the sync-loss timeout.
        };

        /// @brief Minimal time synchronization client for ECU applications.
        ///
        /// This class stores a relation between local `steady_clock` and a
        /// provided global/system time reference. Applications can request a
        /// synchronized timestamp for "now" or for a specific local steady time.
        ///
        /// ### Drift Correction
        /// When multiple reference samples are provided, the client estimates the
        /// clock frequency offset (drift) via linear regression over a sliding
        /// window of up to `cMaxSamples` samples. The drift-corrected offset is
        /// applied when `GetCurrentTime()` is called with a steady time that is
        /// after the latest reference sample.
        ///
        /// ### Sync Quality
        /// The quality level reflects the estimated drift against a configurable
        /// threshold (`SetQualityThreshold()`). If no new sample arrives within
        /// `SetSyncLossTimeoutMs()` milliseconds the client detects sync loss via
        /// `CheckSyncTimeout()` and reverts to `kUnsynchronized`.
        class TimeSyncClient
        {
        public:
            /// @brief Callback type for synchronization state changes.
            using StateChangeNotifier = std::function<void(SynchronizationState)>;

            /// @brief Callback type for sync quality level changes.
            using QualityChangeNotifier = std::function<void(SyncQualityLevel)>;

            /// @brief Callback type for time leap notifications (SWS_TS_00110).
            using TimeLeapCallback = std::function<void(std::chrono::nanoseconds leapAmount)>;

            /// @brief Maximum number of reference samples kept in the sliding window.
            static constexpr std::size_t cMaxSamples{8U};

        private:
            /// A single (steady, global) reference sample pair.
            struct SyncSample
            {
                std::chrono::steady_clock::time_point steady;
                std::chrono::system_clock::time_point global;
            };

            mutable std::mutex                    mMutex;
            SynchronizationState                  mState;
            SyncQualityLevel                      mQuality;
            std::chrono::system_clock::duration   mOffset;
            /// Drift estimate in nanoseconds per second (positive = local runs fast).
            std::chrono::nanoseconds              mDriftNsPerSec;
            std::deque<SyncSample>                mSamples;
            std::chrono::steady_clock::time_point mLastSampleSteady;
            StateChangeNotifier                   mStateNotifier;
            QualityChangeNotifier                 mQualityNotifier;
            /// Max tolerable |drift| in ns/s before quality degrades.
            std::int64_t                          mQualityThresholdNsPerSec;
            /// Milliseconds without a new sample before sync is considered lost.
            std::uint64_t                         mSyncLossTimeoutMs;
            /// Callback invoked when a time leap is detected (SWS_TS_00110).
            TimeLeapCallback                      mLeapCallback;
            /// Previous raw offset (ns) for leap detection.
            std::chrono::nanoseconds              mPrevOffsetNs;
            /// Minimum absolute jump (ns) treated as a leap rather than normal drift.
            static constexpr std::int64_t         cLeapThresholdNs{100'000'000LL}; // 100 ms

            /// Recompute drift from sample window using linear regression (lock held).
            void recomputeDrift() noexcept;
            /// Recompute quality from current drift (lock held). Returns true if changed.
            bool recomputeQuality() noexcept;

        public:
            TimeSyncClient() noexcept;

            /// @brief Update synchronization relation using one reference sample.
            /// @param referenceGlobalTime  Global/system time of the sample.
            /// @param referenceSteadyTime  Local steady time when the sample was taken.
            core::Result<void> UpdateReferenceTime(
                std::chrono::system_clock::time_point referenceGlobalTime,
                std::chrono::steady_clock::time_point referenceSteadyTime =
                    std::chrono::steady_clock::now());

            /// @brief Resolve synchronized global/system time for a local steady time.
            ///
            /// Applies drift correction when more than one sample is available.
            /// @param localSteadyTime Local steady time to convert.
            /// @returns Synchronized system time or `kNotSynchronized` error.
            core::Result<std::chrono::system_clock::time_point> GetCurrentTime(
                std::chrono::steady_clock::time_point localSteadyTime =
                    std::chrono::steady_clock::now()) const;

            /// @brief Get currently applied raw offset in nanoseconds.
            core::Result<std::chrono::nanoseconds> GetCurrentOffset() const;

            /// @brief Get estimated clock drift in nanoseconds per second.
            /// @returns Drift estimate, or `kNotSynchronized` if less than two samples.
            core::Result<std::chrono::nanoseconds> GetDriftEstimate() const;

            /// @brief Get the rate deviation of the local clock (SWS_TS_00330).
            /// @details Returns the same value as GetDriftEstimate(), expressed as
            ///          nanoseconds per second. Positive means the local clock
            ///          runs faster than the reference.
            /// @returns Rate deviation, or `kNotSynchronized` if unavailable.
            core::Result<std::chrono::nanoseconds> GetRateDeviation() const;

            /// @brief Get synchronization state.
            SynchronizationState GetState() const noexcept;

            /// @brief Get current sync quality level.
            SyncQualityLevel GetSyncQuality() const noexcept;

            /// @brief Get number of reference samples in the sliding window.
            std::size_t GetSampleCount() const noexcept;

            /// @brief Reset state to unsynchronized and clear all samples.
            void Reset() noexcept;

            /// @brief Check for sync loss based on elapsed time since the last sample.
            ///
            /// Call periodically (e.g., from a monitor thread) to detect loss of
            /// synchronization when no new samples arrive.
            /// @param nowSteady  Current steady clock time.
            void CheckSyncTimeout(
                std::chrono::steady_clock::time_point nowSteady =
                    std::chrono::steady_clock::now()) noexcept;

            /// @brief Set the maximum |drift| (ns/s) before quality degrades to kDegraded.
            /// @param thresholdNsPerSec  Threshold in nanoseconds per second. Default: 1000.
            void SetQualityThreshold(std::int64_t thresholdNsPerSec) noexcept;

            /// @brief Set timeout after which synchronization is considered lost.
            /// @param timeoutMs  Milliseconds without a new sample. Default: 5000.
            void SetSyncLossTimeoutMs(std::uint64_t timeoutMs) noexcept;

            /// @brief Register a notifier for synchronization state changes.
            core::Result<void> SetStateChangeNotifier(StateChangeNotifier notifier);

            /// @brief Remove the synchronization state change notifier.
            void ClearStateChangeNotifier() noexcept;

            /// @brief Register a notifier for sync quality level changes.
            core::Result<void> SetQualityChangeNotifier(QualityChangeNotifier notifier);

            /// @brief Remove the sync quality change notifier.
            void ClearQualityChangeNotifier() noexcept;

            /// @brief Get detailed time base status (SWS_TS_00100).
            /// @returns Current TimeBaseStatusType for this client.
            TimeBaseStatusType GetTimeBaseStatus() const noexcept;

            /// @brief Register a callback invoked when a time leap is detected (SWS_TS_00110).
            void SetTimeLeapCallback(TimeLeapCallback callback) noexcept;

            /// @brief Remove the time leap callback.
            void ClearTimeLeapCallback() noexcept;

            /// @brief Get leap second information (SWS_TS_00201).
            /// @returns Current leap second info if available.
            core::Result<LeapSecondInfo> GetLeapSecondInfo() const;
        };
    }
}

#endif
