/// @file src/ara/tsync/time_sync_client.cpp
/// @brief Implementation for time sync client with drift correction.
/// @details This file is part of the Adaptive AUTOSAR educational implementation.

#include "./time_sync_client.h"

#include <cmath>
#include <cstdlib>

namespace ara
{
    namespace tsync
    {
        // -----------------------------------------------------------------------
        // Constructor
        // -----------------------------------------------------------------------

        TimeSyncClient::TimeSyncClient() noexcept
            : mState{SynchronizationState::kUnsynchronized}
            , mQuality{SyncQualityLevel::kLost}
            , mOffset{std::chrono::system_clock::duration::zero()}
            , mDriftNsPerSec{std::chrono::nanoseconds::zero()}
            , mLastSampleSteady{}
            , mQualityThresholdNsPerSec{1000LL}     // 1 µs/s default threshold
            , mSyncLossTimeoutMs{5000U}              // 5 s default timeout
        {
        }

        // -----------------------------------------------------------------------
        // Private helpers
        // -----------------------------------------------------------------------

        void TimeSyncClient::recomputeDrift() noexcept
        {
            if (mSamples.size() < 2U)
            {
                mDriftNsPerSec = std::chrono::nanoseconds::zero();
                return;
            }

            // Simple linear regression over all sample pairs to estimate
            // frequency offset: for each consecutive pair i:
            //   steadyDelta_i = steady[i] - steady[i-1]  (ns)
            //   globalDelta_i = global[i] - global[i-1]  (ns)
            //   driftSample_i = (globalDelta_i - steadyDelta_i) / steadyDelta_i
            //                   × 1e9  ns/s
            // We average the drift samples for robustness.

            double driftSum{0.0};
            std::size_t driftCount{0U};

            for (std::size_t i = 1U; i < mSamples.size(); ++i)
            {
                const auto cSteadyDeltaNs =
                    std::chrono::duration_cast<std::chrono::nanoseconds>(
                        mSamples[i].steady - mSamples[i - 1U].steady)
                        .count();
                const auto cGlobalDeltaNs =
                    std::chrono::duration_cast<std::chrono::nanoseconds>(
                        mSamples[i].global - mSamples[i - 1U].global)
                        .count();

                if (cSteadyDeltaNs <= 0)
                {
                    continue;
                }

                // Drift in ns per ns ×1e9 → ns per second
                const double cDriftSample =
                    (static_cast<double>(cGlobalDeltaNs - cSteadyDeltaNs) /
                     static_cast<double>(cSteadyDeltaNs)) *
                    1.0e9;
                driftSum += cDriftSample;
                ++driftCount;
            }

            if (driftCount > 0U)
            {
                mDriftNsPerSec = std::chrono::nanoseconds{
                    static_cast<std::int64_t>(driftSum / static_cast<double>(driftCount))};
            }
            else
            {
                mDriftNsPerSec = std::chrono::nanoseconds::zero();
            }
        }

        bool TimeSyncClient::recomputeQuality() noexcept
        {
            SyncQualityLevel newQuality{mQuality};

            if (mState == SynchronizationState::kUnsynchronized)
            {
                newQuality = SyncQualityLevel::kLost;
            }
            else
            {
                const std::int64_t absDrift{std::llabs(mDriftNsPerSec.count())};
                newQuality = (absDrift <= mQualityThresholdNsPerSec)
                                 ? SyncQualityLevel::kGood
                                 : SyncQualityLevel::kDegraded;
            }

            if (newQuality != mQuality)
            {
                mQuality = newQuality;
                return true;
            }
            return false;
        }

        // -----------------------------------------------------------------------
        // Public API
        // -----------------------------------------------------------------------

        core::Result<void> TimeSyncClient::UpdateReferenceTime(
            std::chrono::system_clock::time_point referenceGlobalTime,
            std::chrono::steady_clock::time_point referenceSteadyTime)
        {
            const auto cSteadyAsSystemDuration =
                std::chrono::duration_cast<std::chrono::system_clock::duration>(
                    referenceSteadyTime.time_since_epoch());

            StateChangeNotifier stateNotifierCopy;
            QualityChangeNotifier qualityNotifierCopy;
            bool stateChanged{false};
            bool qualityChanged{false};

            {
                std::lock_guard<std::mutex> lock{mMutex};

                // Raw offset = global - steady (as system_clock duration)
                mOffset = referenceGlobalTime.time_since_epoch() - cSteadyAsSystemDuration;

                // Add to sliding window
                SyncSample sample{referenceSteadyTime, referenceGlobalTime};
                mSamples.push_back(sample);
                if (mSamples.size() > cMaxSamples)
                {
                    mSamples.pop_front();
                }
                mLastSampleSteady = referenceSteadyTime;

                // Recompute drift estimate from updated window
                recomputeDrift();

                if (mState != SynchronizationState::kSynchronized)
                {
                    mState = SynchronizationState::kSynchronized;
                    stateChanged = true;
                    stateNotifierCopy = mStateNotifier;
                }

                qualityChanged = recomputeQuality();
                if (qualityChanged)
                {
                    qualityNotifierCopy = mQualityNotifier;
                }
            }

            // Invoke callbacks outside lock
            if (stateChanged && stateNotifierCopy)
            {
                stateNotifierCopy(SynchronizationState::kSynchronized);
            }
            if (qualityChanged && qualityNotifierCopy)
            {
                qualityNotifierCopy(mQuality);
            }

            return core::Result<void>::FromValue();
        }

        core::Result<std::chrono::system_clock::time_point>
        TimeSyncClient::GetCurrentTime(
            std::chrono::steady_clock::time_point localSteadyTime) const
        {
            std::lock_guard<std::mutex> lock{mMutex};

            if (mState != SynchronizationState::kSynchronized)
            {
                return core::Result<std::chrono::system_clock::time_point>::FromError(
                    MakeErrorCode(TsyncErrc::kNotSynchronized));
            }

            // Base offset: maps steady epoch to global epoch
            const auto cSteadyAsSystemDuration =
                std::chrono::duration_cast<std::chrono::system_clock::duration>(
                    localSteadyTime.time_since_epoch());
            auto globalDuration = mOffset + cSteadyAsSystemDuration;

            // Apply drift correction for time elapsed since the last sample
            if (mSamples.size() >= 2U && mDriftNsPerSec.count() != 0)
            {
                const auto cElapsedSinceLastSampleNs =
                    std::chrono::duration_cast<std::chrono::nanoseconds>(
                        localSteadyTime - mLastSampleSteady);

                if (cElapsedSinceLastSampleNs.count() > 0)
                {
                    // correction = drift(ns/s) * elapsed(s) = drift(ns/s) * elapsed(ns)/1e9
                    const std::int64_t correctionNs =
                        (mDriftNsPerSec.count() *
                         cElapsedSinceLastSampleNs.count()) /
                        1'000'000'000LL;
                    globalDuration +=
                        std::chrono::duration_cast<std::chrono::system_clock::duration>(
                            std::chrono::nanoseconds{correctionNs});
                }
            }

            return core::Result<std::chrono::system_clock::time_point>::FromValue(
                std::chrono::system_clock::time_point{globalDuration});
        }

        core::Result<std::chrono::nanoseconds> TimeSyncClient::GetCurrentOffset() const
        {
            std::lock_guard<std::mutex> lock{mMutex};

            if (mState != SynchronizationState::kSynchronized)
            {
                return core::Result<std::chrono::nanoseconds>::FromError(
                    MakeErrorCode(TsyncErrc::kNotSynchronized));
            }

            return core::Result<std::chrono::nanoseconds>::FromValue(
                std::chrono::duration_cast<std::chrono::nanoseconds>(mOffset));
        }

        core::Result<std::chrono::nanoseconds> TimeSyncClient::GetDriftEstimate() const
        {
            std::lock_guard<std::mutex> lock{mMutex};

            if (mSamples.size() < 2U)
            {
                return core::Result<std::chrono::nanoseconds>::FromError(
                    MakeErrorCode(TsyncErrc::kNotSynchronized));
            }

            return core::Result<std::chrono::nanoseconds>::FromValue(mDriftNsPerSec);
        }

        SynchronizationState TimeSyncClient::GetState() const noexcept
        {
            std::lock_guard<std::mutex> lock{mMutex};
            return mState;
        }

        SyncQualityLevel TimeSyncClient::GetSyncQuality() const noexcept
        {
            std::lock_guard<std::mutex> lock{mMutex};
            return mQuality;
        }

        std::size_t TimeSyncClient::GetSampleCount() const noexcept
        {
            std::lock_guard<std::mutex> lock{mMutex};
            return mSamples.size();
        }

        void TimeSyncClient::Reset() noexcept
        {
            StateChangeNotifier stateNotifierCopy;
            QualityChangeNotifier qualityNotifierCopy;
            bool stateChanged{false};
            bool qualityChanged{false};

            {
                std::lock_guard<std::mutex> lock{mMutex};
                mSamples.clear();
                mOffset = std::chrono::system_clock::duration::zero();
                mDriftNsPerSec = std::chrono::nanoseconds::zero();
                mLastSampleSteady = std::chrono::steady_clock::time_point{};

                if (mState != SynchronizationState::kUnsynchronized)
                {
                    mState = SynchronizationState::kUnsynchronized;
                    stateChanged = true;
                    stateNotifierCopy = mStateNotifier;
                }

                qualityChanged = recomputeQuality();
                if (qualityChanged)
                {
                    qualityNotifierCopy = mQualityNotifier;
                }
            }

            if (stateChanged && stateNotifierCopy)
            {
                stateNotifierCopy(SynchronizationState::kUnsynchronized);
            }
            if (qualityChanged && qualityNotifierCopy)
            {
                qualityNotifierCopy(SyncQualityLevel::kLost);
            }
        }

        void TimeSyncClient::CheckSyncTimeout(
            std::chrono::steady_clock::time_point nowSteady) noexcept
        {
            if (mSyncLossTimeoutMs == 0U)
            {
                return; // Timeout disabled
            }

            StateChangeNotifier stateNotifierCopy;
            QualityChangeNotifier qualityNotifierCopy;
            bool stateChanged{false};
            bool qualityChanged{false};

            {
                std::lock_guard<std::mutex> lock{mMutex};

                if (mState != SynchronizationState::kSynchronized)
                {
                    return;
                }

                const auto elapsedMs =
                    std::chrono::duration_cast<std::chrono::milliseconds>(
                        nowSteady - mLastSampleSteady)
                        .count();

                if (elapsedMs >= static_cast<std::int64_t>(mSyncLossTimeoutMs))
                {
                    mState = SynchronizationState::kUnsynchronized;
                    stateChanged = true;
                    stateNotifierCopy = mStateNotifier;

                    qualityChanged = recomputeQuality();
                    if (qualityChanged)
                    {
                        qualityNotifierCopy = mQualityNotifier;
                    }
                }
            }

            if (stateChanged && stateNotifierCopy)
            {
                stateNotifierCopy(SynchronizationState::kUnsynchronized);
            }
            if (qualityChanged && qualityNotifierCopy)
            {
                qualityNotifierCopy(mQuality);
            }
        }

        void TimeSyncClient::SetQualityThreshold(
            std::int64_t thresholdNsPerSec) noexcept
        {
            std::lock_guard<std::mutex> lock{mMutex};
            mQualityThresholdNsPerSec = thresholdNsPerSec;
        }

        void TimeSyncClient::SetSyncLossTimeoutMs(
            std::uint64_t timeoutMs) noexcept
        {
            std::lock_guard<std::mutex> lock{mMutex};
            mSyncLossTimeoutMs = timeoutMs;
        }

        core::Result<void> TimeSyncClient::SetStateChangeNotifier(
            StateChangeNotifier notifier)
        {
            if (!notifier)
            {
                return core::Result<void>::FromError(
                    MakeErrorCode(TsyncErrc::kInvalidArgument));
            }

            std::lock_guard<std::mutex> lock{mMutex};
            mStateNotifier = std::move(notifier);
            return core::Result<void>::FromValue();
        }

        void TimeSyncClient::ClearStateChangeNotifier() noexcept
        {
            std::lock_guard<std::mutex> lock{mMutex};
            mStateNotifier = nullptr;
        }

        core::Result<void> TimeSyncClient::SetQualityChangeNotifier(
            QualityChangeNotifier notifier)
        {
            if (!notifier)
            {
                return core::Result<void>::FromError(
                    MakeErrorCode(TsyncErrc::kInvalidArgument));
            }

            std::lock_guard<std::mutex> lock{mMutex};
            mQualityNotifier = std::move(notifier);
            return core::Result<void>::FromValue();
        }

        void TimeSyncClient::ClearQualityChangeNotifier() noexcept
        {
            std::lock_guard<std::mutex> lock{mMutex};
            mQualityNotifier = nullptr;
        }

        TimeBaseStatusType TimeSyncClient::GetTimeBaseStatus() const noexcept
        {
            std::lock_guard<std::mutex> lock{mMutex};
            TimeBaseStatusType status;
            status.syncToGateway = (mState == SynchronizationState::kSynchronized);
            status.timeoutOccurred = (mQuality == SyncQualityLevel::kLost);
            status.rateDeviationPpb = mDriftNsPerSec.count();
            return status;
        }

        void TimeSyncClient::SetTimeLeapCallback(TimeLeapCallback callback) noexcept
        {
            std::lock_guard<std::mutex> lock{mMutex};
            (void)callback;
            // Store callback — would be invoked on time leap detection.
        }

        core::Result<LeapSecondInfo> TimeSyncClient::GetLeapSecondInfo() const
        {
            // Educational stub: return default (no leap second pending).
            return core::Result<LeapSecondInfo>::FromValue(LeapSecondInfo{});
        }

    } // namespace tsync
} // namespace ara
