/// @file src/ara/tsync/time_sync_client.cpp
/// @brief Implementation for time sync client with drift correction.
/// @details This file is part of the Adaptive AUTOSAR educational implementation.

#include "./time_sync_client.h"

#include <cmath>
#include <cstdlib>
#include <ctime>

#if defined(__linux__) || defined(__QNX__)
#include <sys/timex.h>
#endif

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
            , mPrevOffsetNs{std::chrono::nanoseconds::zero()}
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
            TimeLeapCallback leapCallbackCopy;
            std::chrono::nanoseconds leapAmount{0};
            bool stateChanged{false};
            bool qualityChanged{false};
            bool leapDetected{false};

            {
                std::lock_guard<std::mutex> lock{mMutex};

                // Compute new raw offset = global - steady
                const auto newOffset = referenceGlobalTime.time_since_epoch() -
                                       cSteadyAsSystemDuration;
                const auto newOffsetNs =
                    std::chrono::duration_cast<std::chrono::nanoseconds>(newOffset);

                // Detect time leap: compare new offset to previous.
                // A jump larger than cLeapThresholdNs is treated as a leap.
                if (mState == SynchronizationState::kSynchronized &&
                    mLeapCallback)
                {
                    leapAmount = newOffsetNs - mPrevOffsetNs;
                    const auto absLeap =
                        leapAmount.count() < 0 ? -leapAmount.count()
                                               : leapAmount.count();
                    if (absLeap >= cLeapThresholdNs)
                    {
                        leapDetected = true;
                        leapCallbackCopy = mLeapCallback;
                    }
                }

                mOffset = newOffset;
                mPrevOffsetNs = newOffsetNs;

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
                    // Record first offset as baseline (no leap on first sync)
                    mPrevOffsetNs = newOffsetNs;
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
            if (leapDetected && leapCallbackCopy)
            {
                leapCallbackCopy(leapAmount);
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
            mLeapCallback = std::move(callback);
        }

        void TimeSyncClient::ClearTimeLeapCallback() noexcept
        {
            std::lock_guard<std::mutex> lock{mMutex};
            mLeapCallback = nullptr;
        }

        core::Result<LeapSecondInfo> TimeSyncClient::GetLeapSecondInfo() const
        {
            // Query the kernel NTP state for leap second information.
            // POSIX adjtimex() / ntp_adjtime() (Linux + QNX) provide:
            //   timex::status : STA_INS (insert) / STA_DEL (delete) leap bits
            //   timex::tai    : current TAI-UTC offset in seconds
            LeapSecondInfo info{};

#if defined(__linux__) || defined(__QNX__)
            struct timex tx{};
            // adjtimex is POSIX; ntp_adjtime is the same on Linux/QNX.
            int state = ::adjtimex(&tx);

            // STA_INS (0x0010): next leap second will be inserted.
            // STA_DEL (0x0020): next leap second will be deleted.
            static constexpr int cStaIns{0x0010};
            static constexpr int cStaDel{0x0020};

            if (tx.tai > 0)
            {
                info.currentLeapSeconds = tx.tai;
            }

            if ((tx.status & cStaIns) || (tx.status & cStaDel))
            {
                info.futureLeapSecondPending = true;
                info.futureLeapSecondPositive = (tx.status & cStaIns) != 0;

                // The leap second occurs at the end of the current UTC month.
                // Compute the start of the next month as the effective time.
                const auto now =
                    std::chrono::system_clock::now();
                const std::time_t nowT =
                    std::chrono::system_clock::to_time_t(now);
                std::tm utcTm{};
#if defined(_POSIX_C_SOURCE) || defined(__linux__) || defined(__QNX__)
                ::gmtime_r(&nowT, &utcTm);
#else
                utcTm = *std::gmtime(&nowT);
#endif
                // Advance to the first second of next month.
                utcTm.tm_sec = 0;
                utcTm.tm_min = 0;
                utcTm.tm_hour = 0;
                utcTm.tm_mday = 1;
                ++utcTm.tm_mon;
                if (utcTm.tm_mon >= 12)
                {
                    utcTm.tm_mon = 0;
                    ++utcTm.tm_year;
                }
                const std::time_t leapT = ::timegm(&utcTm);
                info.futureLeapSecondTime =
                    std::chrono::system_clock::from_time_t(leapT);
            }

            (void)state; // return value indicates current leap state, not an error
#endif

            return core::Result<LeapSecondInfo>::FromValue(info);
        }

    } // namespace tsync
} // namespace ara
