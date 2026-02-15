#include "./time_sync_client.h"

namespace ara
{
    namespace tsync
    {
        TimeSyncClient::TimeSyncClient() noexcept
            : mState{SynchronizationState::kUnsynchronized},
              mOffset{std::chrono::system_clock::duration::zero()}
        {
        }

        core::Result<void> TimeSyncClient::UpdateReferenceTime(
            std::chrono::system_clock::time_point referenceGlobalTime,
            std::chrono::steady_clock::time_point referenceSteadyTime)
        {
            const auto _steadyAsSystemDuration =
                std::chrono::duration_cast<std::chrono::system_clock::duration>(
                    referenceSteadyTime.time_since_epoch());

            std::lock_guard<std::mutex> _lock{mMutex};
            mOffset = referenceGlobalTime.time_since_epoch() - _steadyAsSystemDuration;
            mState = SynchronizationState::kSynchronized;

            return core::Result<void>::FromValue();
        }

        core::Result<std::chrono::system_clock::time_point> TimeSyncClient::GetCurrentTime(
            std::chrono::steady_clock::time_point localSteadyTime) const
        {
            std::lock_guard<std::mutex> _lock{mMutex};
            if (mState != SynchronizationState::kSynchronized)
            {
                return core::Result<std::chrono::system_clock::time_point>::FromError(
                    MakeErrorCode(TsyncErrc::kNotSynchronized));
            }

            const auto _steadyAsSystemDuration =
                std::chrono::duration_cast<std::chrono::system_clock::duration>(
                    localSteadyTime.time_since_epoch());
            const auto _synchronizedDuration = mOffset + _steadyAsSystemDuration;

            return core::Result<std::chrono::system_clock::time_point>::FromValue(
                std::chrono::system_clock::time_point{_synchronizedDuration});
        }

        core::Result<std::chrono::nanoseconds> TimeSyncClient::GetCurrentOffset() const
        {
            std::lock_guard<std::mutex> _lock{mMutex};
            if (mState != SynchronizationState::kSynchronized)
            {
                return core::Result<std::chrono::nanoseconds>::FromError(
                    MakeErrorCode(TsyncErrc::kNotSynchronized));
            }

            const auto _offsetNanoseconds =
                std::chrono::duration_cast<std::chrono::nanoseconds>(mOffset);
            return core::Result<std::chrono::nanoseconds>::FromValue(
                _offsetNanoseconds);
        }

        SynchronizationState TimeSyncClient::GetState() const noexcept
        {
            std::lock_guard<std::mutex> _lock{mMutex};
            return mState;
        }

        void TimeSyncClient::Reset() noexcept
        {
            std::lock_guard<std::mutex> _lock{mMutex};
            mState = SynchronizationState::kUnsynchronized;
            mOffset = std::chrono::system_clock::duration::zero();
        }
    }
}
