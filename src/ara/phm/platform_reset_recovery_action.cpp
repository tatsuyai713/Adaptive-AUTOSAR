/// @file src/ara/phm/platform_reset_recovery_action.cpp
/// @brief Implementation for PlatformResetRecoveryAction.
/// @details This file is part of the Adaptive AUTOSAR educational implementation.

#include "./platform_reset_recovery_action.h"

#include <stdexcept>
#include <utility>

namespace ara
{
    namespace phm
    {
        PlatformResetRecoveryAction::PlatformResetRecoveryAction(
            const core::InstanceSpecifier &instance,
            ResetCallback resetCallback)
            : RecoveryAction{instance},
              mResetCallback{std::move(resetCallback)}
        {
            if (!mResetCallback)
            {
                throw std::invalid_argument(
                    "Reset callback must not be null.");
            }
        }

        void PlatformResetRecoveryAction::RecoveryHandler(
            const exec::ExecutionErrorEvent &executionError,
            TypeOfSupervision supervision)
        {
            if (IsOffered() && mResetCallback)
            {
                mResetCallback();
            }
        }
    }
}
