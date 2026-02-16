/// @file src/ara/sm/machine_state_client.cpp
/// @brief Implementation for machine state client.
/// @details This file is part of the Adaptive AUTOSAR educational implementation.

#include "./machine_state_client.h"

namespace ara
{
    namespace sm
    {
        MachineStateClient::MachineStateClient()
            : mState{MachineState::kStartup}
        {
        }

        core::Result<MachineState> MachineStateClient::GetMachineState() const
        {
            std::lock_guard<std::mutex> _lock{mMutex};
            return core::Result<MachineState>::FromValue(mState);
        }

        core::Result<void> MachineStateClient::SetNotifier(
            StateChangeNotifier notifier)
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

        void MachineStateClient::ClearNotifier() noexcept
        {
            std::lock_guard<std::mutex> _lock{mMutex};
            mNotifier = nullptr;
        }

        core::Result<void> MachineStateClient::SetMachineState(MachineState state)
        {
            StateChangeNotifier _notifierCopy;
            bool _stateChanged{false};

            {
                std::lock_guard<std::mutex> _lock{mMutex};
                if (mState == state)
                {
                    return core::Result<void>::FromError(
                        MakeErrorCode(SmErrc::kAlreadyInState));
                }
                mState = state;
                _stateChanged = true;
                _notifierCopy = mNotifier;
            }

            if (_stateChanged && _notifierCopy)
            {
                _notifierCopy(state);
            }

            return core::Result<void>::FromValue();
        }
    }
}
