/// @file src/ara/sm/network_handle.cpp
/// @brief Implementation for network handle.
/// @details This file is part of the Adaptive AUTOSAR educational implementation.

#include "./network_handle.h"

namespace ara
{
    namespace sm
    {
        NetworkHandle::NetworkHandle(const core::InstanceSpecifier &instance)
            : mInstance{instance},
              mCurrentMode{ComMode::kNone}
        {
        }

        core::Result<void> NetworkHandle::RequestComMode(ComMode mode)
        {
            ComModeNotifier _notifierCopy;
            bool _modeChanged{false};

            {
                std::lock_guard<std::mutex> _lock{mMutex};
                if (mCurrentMode == mode)
                {
                    return core::Result<void>::FromError(
                        MakeErrorCode(SmErrc::kAlreadyInState));
                }
                mCurrentMode = mode;
                _modeChanged = true;
                _notifierCopy = mNotifier;
            }

            if (_modeChanged && _notifierCopy)
            {
                _notifierCopy(mode);
            }

            return core::Result<void>::FromValue();
        }

        core::Result<ComMode> NetworkHandle::GetCurrentComMode() const
        {
            std::lock_guard<std::mutex> _lock{mMutex};
            return core::Result<ComMode>::FromValue(mCurrentMode);
        }

        core::Result<void> NetworkHandle::SetNotifier(ComModeNotifier notifier)
        {
            if (!notifier)
            {
                return core::Result<void>::FromError(
                    MakeErrorCode(SmErrc::kInvalidArgument));
            }

            std::lock_guard<std::mutex> _lock{mMutex};
            mNotifier = std::move(notifier);
            return core::Result<void>::FromValue();
        }

        void NetworkHandle::ClearNotifier() noexcept
        {
            std::lock_guard<std::mutex> _lock{mMutex};
            mNotifier = nullptr;
        }

        const core::InstanceSpecifier &NetworkHandle::GetInstance() const noexcept
        {
            return mInstance;
        }
    }
}
