/// @file src/ara/tsync/time_sync_client.cpp
/// @brief Implementation for time sync client.
/// @details This file is part of the Adaptive AUTOSAR educational implementation.

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

            StateChangeNotifier _notifierCopy;
            bool _stateChanged{false};

            {
                std::lock_guard<std::mutex> _lock{mMutex};
                mOffset = referenceGlobalTime.time_since_epoch() - _steadyAsSystemDuration;

                if (mState != SynchronizationState::kSynchronized)
                {
                    mState = SynchronizationState::kSynchronized;
                    _stateChanged = true;
                    _notifierCopy = mStateNotifier;
                }
            }

            if (_stateChanged && _notifierCopy)
            {
                _notifierCopy(SynchronizationState::kSynchronized);
            }

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
            StateChangeNotifier _notifierCopy;
            bool _stateChanged{false};

            {
                std::lock_guard<std::mutex> _lock{mMutex};
                if (mState != SynchronizationState::kUnsynchronized)
                {
                    mState = SynchronizationState::kUnsynchronized;
                    _stateChanged = true;
                    _notifierCopy = mStateNotifier;
                }
                mOffset = std::chrono::system_clock::duration::zero();
            }

            if (_stateChanged && _notifierCopy)
            {
                _notifierCopy(SynchronizationState::kUnsynchronized);
            }
        }

        core::Result<void> TimeSyncClient::SetStateChangeNotifier(
            StateChangeNotifier notifier)
        {
            if (!notifier)
            {
                return core::Result<void>::FromError(
                    MakeErrorCode(TsyncErrc::kInvalidArgument));
            }

            std::lock_guard<std::mutex> _lock{mMutex};
            mStateNotifier = std::move(notifier);
            return core::Result<void>::FromValue();
        }

        void TimeSyncClient::ClearStateChangeNotifier() noexcept
        {
            std::lock_guard<std::mutex> _lock{mMutex};
            mStateNotifier = nullptr;
        }
    }
}
