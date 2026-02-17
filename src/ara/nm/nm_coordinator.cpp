/// @file src/ara/nm/nm_coordinator.cpp
/// @brief Implementation for multi-channel NM coordinator.
/// @details This file is part of the Adaptive AUTOSAR educational implementation.

#include "./nm_coordinator.h"

namespace ara
{
    namespace nm
    {
        NmCoordinator::NmCoordinator(
            NetworkManager &nm,
            CoordinatorPolicy policy)
            : mNm{nm},
              mPolicy{policy}
        {
        }

        core::Result<void> NmCoordinator::RequestCoordinatedSleep()
        {
            std::lock_guard<std::mutex> _lock{mMutex};

            auto _channels = mNm.GetChannelNames();
            if (_channels.empty())
            {
                return core::Result<void>::FromError(
                    MakeErrorCode(NmErrc::kCoordinatorError));
            }

            // Release network on all channels
            for (const auto &_ch : _channels)
            {
                mNm.NetworkRelease(_ch);
            }

            mSleepRequested = true;
            mSleepReadyNotified = false;
            return core::Result<void>::FromValue();
        }

        core::Result<void> NmCoordinator::RequestCoordinatedWakeup()
        {
            std::lock_guard<std::mutex> _lock{mMutex};

            auto _channels = mNm.GetChannelNames();
            if (_channels.empty())
            {
                return core::Result<void>::FromError(
                    MakeErrorCode(NmErrc::kCoordinatorError));
            }

            // Request network on all channels
            for (const auto &_ch : _channels)
            {
                mNm.NetworkRequest(_ch);
            }

            mSleepRequested = false;
            mSleepReadyNotified = false;
            return core::Result<void>::FromValue();
        }

        CoordinatorStatus NmCoordinator::GetStatus() const
        {
            std::lock_guard<std::mutex> _lock{mMutex};

            auto _channels = mNm.GetChannelNames();
            std::uint32_t _total = static_cast<std::uint32_t>(_channels.size());
            std::uint32_t _sleepReady{0U};

            for (const auto &_ch : _channels)
            {
                auto _status = mNm.GetChannelStatus(_ch);
                if (_status.HasValue() &&
                    _status.Value().State == NmState::kBusSleep)
                {
                    ++_sleepReady;
                }
            }

            bool _ready{false};
            if (mPolicy == CoordinatorPolicy::kAllChannelsSleep)
            {
                _ready = (_sleepReady == _total) && (_total > 0U);
            }
            else
            {
                _ready = (_sleepReady > _total / 2U) && (_total > 0U);
            }

            CoordinatorStatus _result;
            _result.CoordinatedSleepReady = _ready;
            _result.ActiveChannelCount = _total;
            _result.SleepReadyChannelCount = _sleepReady;
            return _result;
        }

        void NmCoordinator::Tick(std::uint64_t nowEpochMs)
        {
            // Tick the underlying NM state machine first
            mNm.Tick(nowEpochMs);

            std::lock_guard<std::mutex> _lock{mMutex};

            if (!mSleepRequested)
            {
                return;
            }

            auto _status = GetStatus();
            if (_status.CoordinatedSleepReady && !mSleepReadyNotified)
            {
                mSleepReadyNotified = true;
                if (mSleepReadyCallback)
                {
                    mSleepReadyCallback();
                }
            }
        }

        void NmCoordinator::SetSleepReadyCallback(
            std::function<void()> callback)
        {
            std::lock_guard<std::mutex> _lock{mMutex};
            mSleepReadyCallback = std::move(callback);
        }
    }
}
